#include "Database.h"
#include "models/Account.h"
#include "models/AssetEntry.h"
#include "models/Category.h"
#include "models/CategoryRule.h"
#include "models/Transaction.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLatin1String>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVector>
#include <QDebug>
#include <QtLogging>
#include <algorithm>
#include <array>
#include <optional>

namespace {

// SQLite's own message. QSqlError::text() joins the database and driver texts
// with a space ("file is not a database Unable to fetch row").
QString sqlText(const QSqlError& error)
{
    const QString text = error.databaseText().isEmpty() ? error.text() : error.databaseText();
    return text.trimmed();
}

// `text` as a sentence, for a message that adds another sentence after it.
QString sentence(const QString& text)
{
    const QString trimmed = text.trimmed();
    return trimmed.endsWith(QLatin1Char('.')) ? trimmed : trimmed + QLatin1Char('.');
}

} // namespace

// ── Singleton ─────────────────────────────────────────────────────────────────

Database::Database()
    : m_snapshots(kConnection)
    , m_assetEntries(kConnection)
    , m_categoriesTable(kConnection)
    , m_subcategoriesTable(kConnection)
    , m_accountsTable(kConnection)
    , m_accountHashesTable(kConnection)
    , m_accountCategoriesTable(kConnection)
    , m_accountTypesTable(kConnection)
    , m_transactionsTable(kConnection)
    , m_balanceCheckpointsTable(kConnection)
    , m_categoryRulesTable(kConnection)
{}

Database& Database::instance()
{
    static Database database;
    return database;
}

// ── Open / init ───────────────────────────────────────────────────────────────

bool Database::open(const QString& path)
{
    m_upgradeReport.clear();
    m_openError = OpenError::None;
    if (openFile(path)) {
        return true;
    }
    // Nothing may write to a file that this version cannot use (newer
    // schema, failed upgrade): the connection is closed, so isOpen() is false
    // and every write fails with "Database not open".
    close();
    return false;
}

bool Database::openFile(const QString& path)
{
    // Read before the open: QSqlDatabase::open() creates the file.
    const QFileInfo fileBefore(path);
    const bool existedBefore = fileBefore.exists() && fileBefore.size() > 0;

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                      QLatin1String(kConnection));
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        return failOpen(OpenError::CannotOpen, QStringLiteral("Cannot open: %1").arg(sqlText(m_db.lastError())));
    }

    // The first read: a file that is not a database fails here, with
    // SQLite's own message ("file is not a database").
    int from = 0;
    {
        QSqlQuery version(m_db);
        if (!version.exec(QStringLiteral("PRAGMA user_version;")) || !version.next()) {
            return failOpen(OpenError::CannotOpen,
                            QStringLiteral("Cannot read the database: %1").arg(sqlText(version.lastError())));
        }
        from = version.value(0).toInt();
    }
    // A file from a newer version of the app: refuse it before any change.
    if (from > kSchemaVersion) {
        return failOpen(OpenError::NewerSchema,
                        QStringLiteral("This database uses schema version %1. This version of the app "
                                       "knows schema version %2 at most: use a newer version of the app.")
                            .arg(from)
                            .arg(kSchemaVersion));
    }

    // An existing file that this version upgrades: a copy first, before any
    // change to the file.
    const bool upgrading = existedBefore && from < kSchemaVersion;
    QString backupPath;
    bool copyCleared = false;
    if (upgrading) {
        if (!backUp(path, backupPath)) {
            return false;
        }
        // Maintainer decision (2026-10-18): the copy keeps no IBAN, BIC or
        // account number either. The app does not use them since v0.2.0.
        const QStringList numbers = accountNumberColumns();
        if (!numbers.isEmpty()) {
            if (!clearAccountNumbersInCopy(backupPath, numbers)) {
                const QString detail = m_lastError;
                QFile::remove(backupPath);
                return failOpen(OpenError::UpgradeFailed,
                                QStringLiteral("Could not remove the account numbers from the copy of the "
                                               "database, so it was not upgraded: %1 The file was not changed.")
                                    .arg(sentence(detail)));
            }
            copyCleared = true;
        }
    }

    // The account numbers that step 1 removes: with secure_delete, SQLite
    // writes zeros over the freed pages inside the upgrade transaction, so the
    // old values are gone even when the VACUUM below does not run (a crash
    // after the commit, a full disk).
    if (copyCleared) {
        QSqlQuery secureDelete(m_db);
        secureDelete.exec(QStringLiteral("PRAGMA secure_delete = ON;"));
        secureDelete.finish();
    }

    // The base tables and every migration step in one SQL transaction: if a
    // step fails, the rollback leaves the file as it was.
    UpgradeStats stats;
    m_lastError.clear();
    const bool done = m_db.transaction() && createBaseTables() && upgrade(from, stats) && m_db.commit();
    if (!done) {
        const QString detail = m_lastError.isEmpty() ? sqlText(m_db.lastError()) : m_lastError;
        m_db.rollback();
        if (!upgrading) {
            return failOpen(OpenError::CannotOpen, detail);
        }
        // The file is unchanged, so this copy is not needed: without this, each
        // failed start would leave one more copy.
        QFile::remove(backupPath);
        return failOpen(OpenError::UpgradeFailed,
                        QStringLiteral("Could not upgrade the database from schema version %1 to %2: %3 "
                                       "The file was not changed.")
                            .arg(from)
                            .arg(kSchemaVersion)
                            .arg(sentence(detail)));
    }

    // VACUUM writes the file again without free pages (the zeroed ones of
    // step 1 too); secure_delete goes back to SQLite's default.
    if (stats.droppedAccountNumbers) {
        QSqlQuery vacuum(m_db);
        if (!vacuum.exec(QStringLiteral("VACUUM;"))) {
            qWarning() << "[Database] VACUUM after the upgrade failed:" << sqlText(vacuum.lastError());
        }
        vacuum.finish();
    }
    if (copyCleared) {
        QSqlQuery secureDelete(m_db);
        secureDelete.exec(QStringLiteral("PRAGMA secure_delete = OFF;"));
        secureDelete.finish();
    }

    // WAL mode: better read concurrency, faster writes. After the upgrade:
    // journal_mode cannot change inside a transaction.
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    // The PRAGMA returns a row; finish() releases the statement, else the
    // next commit() fails with "SQL statements in progress".
    pragma.finish();
    if (stats.droppedAccountNumbers) {
        // Write the WAL into the file and empty it: the old pages go too.
        pragma.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE);"));
        pragma.finish();
    }

    if (upgrading) {
        m_upgradeReport = QStringLiteral("The database was upgraded from schema version %1 to %2.\n\n"
                                         "A copy from before the upgrade is in:\n%3")
                              .arg(from)
                              .arg(kSchemaVersion)
                              .arg(QDir::toNativeSeparators(backupPath));
        if (copyCleared) {
            m_upgradeReport += QStringLiteral("\n\nThe old IBAN, BIC and account numbers were removed, from "
                                              "the database and from the copy.");
        }
        if (stats.removedDuplicates > 0) {
            m_upgradeReport += QStringLiteral("\n\n%1 duplicate transaction(s) were removed: the same "
                                              "statement rows had been saved more than once. Of each row, "
                                              "the copy with a category stays (then the one with a "
                                              "subcategory, then the oldest one). If a statement really had "
                                              "the same row twice, import it again: the second row comes back.")
                                   .arg(stats.removedDuplicates);
        }
        if (stats.removedWithOtherValues > 0) {
            m_upgradeReport += QStringLiteral("\n\n%1 of the removed copies had another category, "
                                              "subcategory, label, date or balance. The copy from before "
                                              "the upgrade keeps them, and the application log lists them.")
                                   .arg(stats.removedWithOtherValues);
        }
        if (stats.reusedReferences > 0) {
            m_upgradeReport += QStringLiteral("\n\nWarning: %1 of the removed copies have the bank reference "
                                              "and the amount of the copy that stays, but another operation "
                                              "date. If they are different transactions, the bank reuses its "
                                              "references, and the copy from before the upgrade keeps them. "
                                              "See:\n%2")
                                   .arg(stats.reusedReferences)
                                   .arg(QLatin1String(TransactionsTable::kReusedReferenceIssueUrl));
        }
        qInfo().noquote() << "[Database]" << m_upgradeReport;
    }
    return true;
}

bool Database::failOpen(OpenError error, const QString& message)
{
    m_openError = error;
    m_lastError = message;
    qWarning().noquote() << "[Database]" << message;
    return false;
}

void Database::close()
{
    if (m_db.isValid()) {
        m_db.close();
    }
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(QLatin1String(kConnection));
}

bool Database::createBaseTables()
{
    // Each table class creates its own schema, and logs the SQLite error.
    const auto created = [this](bool ok, const char* table) {
        if (!ok) {
            m_lastError = QStringLiteral("Cannot create the table %1 (the application log has the SQLite "
                                         "error).").arg(QLatin1String(table));
        }
        return ok;
    };
    return created(m_snapshots.createTable(), "snapshots") &&
           created(m_assetEntries.createTable(), "asset_entries") &&
           created(m_categoriesTable.createTable(), "categories") &&
           created(m_subcategoriesTable.createTable(), "subcategories") &&
           created(m_accountCategoriesTable.createTable(), "account_categories") &&
           created(m_accountTypesTable.createTable(), "account_types") &&
           created(m_accountsTable.createTable(), "accounts") &&
           created(m_accountHashesTable.createTable(), "account_hashes") &&
           created(m_transactionsTable.createTable(), "transactions") &&
           created(m_balanceCheckpointsTable.createTable(), "account_balance_checkpoints") &&
           created(m_categoryRulesTable.createTable(), "category_rules");
}

// A copy of the file before the first change. VACUUM INTO writes a
// consistent copy that includes what is still in the WAL file; it must run
// outside a transaction, and the target must not exist yet.
bool Database::backUp(const QString& path, QString& backupPath)
{
    const QFileInfo info(path);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    backupPath = info.dir().filePath(
        QStringLiteral("%1.backup-%2.%3").arg(info.completeBaseName(), stamp, info.suffix()));
    for (int n = 2; QFileInfo::exists(backupPath); ++n) {
        backupPath = info.dir().filePath(
            QStringLiteral("%1.backup-%2-%3.%4").arg(info.completeBaseName(), stamp).arg(n).arg(info.suffix()));
    }
    QSqlQuery backup(m_db);
    backup.prepare(QStringLiteral("VACUUM INTO ?;"));
    backup.addBindValue(QDir::toNativeSeparators(backupPath));
    if (!backup.exec()) {
        const QString detail = sqlText(backup.lastError());
        backup.finish();
        QFile::remove(backupPath); // a partial copy, if any
        return failOpen(OpenError::UpgradeFailed,
                        QStringLiteral("Could not save a copy of the database before the upgrade, so it "
                                       "was not upgraded: %1 The file was not changed.")
                            .arg(sentence(detail)));
    }
    return true;
}

QStringList Database::accountNumberColumns() const
{
    const QStringList columns = columnsOf(QStringLiteral("accounts"));
    QStringList result;
    for (const QString& legacy : {QStringLiteral("iban"), QStringLiteral("bic"), QStringLiteral("number")}) {
        if (columns.contains(legacy, Qt::CaseInsensitive)) {
            result.append(legacy);
        }
    }
    return result;
}

// Its own connection to the copy. secure_delete writes zeros over the old
// values, and VACUUM writes the file again without free pages, so the old
// bytes are not in the file any more. The copy uses a rollback journal: one
// file, no -wal next to it.
bool Database::clearAccountNumbersInCopy(const QString& backupPath, const QStringList& columns)
{
    static const QLatin1String kCopyConnection("finance_dashboard_copy");
    QStringList assignments;
    for (const QString& column : columns) {
        assignments.append(QStringLiteral("%1 = ''").arg(column));
    }
    bool ok = false;
    {
        QSqlDatabase copy = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kCopyConnection);
        copy.setDatabaseName(backupPath);
        if (copy.open()) {
            const QStringList statements = {
                QStringLiteral("PRAGMA journal_mode = DELETE;"),
                QStringLiteral("PRAGMA secure_delete = ON;"),
                QStringLiteral("UPDATE accounts SET %1;").arg(assignments.join(QStringLiteral(", "))),
                QStringLiteral("VACUUM;"),
            };
            ok = true;
            for (const QString& sql : statements) {
                QSqlQuery query(copy);
                if (!query.exec(sql)) {
                    m_lastError = sqlText(query.lastError());
                    ok = false;
                    break;
                }
                query.finish();
            }
            copy.close();
        } else {
            m_lastError = sqlText(copy.lastError());
        }
    }
    QSqlDatabase::removeDatabase(kCopyConnection);
    return ok;
}

// ── Schema migrations ─────────────────────────────────────────────────────────
//
// createTable() in each table class makes the base schema (version 0, the
// schema before versioning), and must not change: a database at any version
// starts from it. Every later change is one step here, so a new database and
// an old one go through the same steps and end with the same schema. Add a
// step: write migrateToN() with its own SQL (not a createTable() that changed),
// add its case below, raise kSchemaVersion, and add a test in
// tests/db_migration_test.cpp.

int Database::schemaVersion() const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA user_version;")) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

QStringList Database::columnsOf(const QString& table) const
{
    QStringList columns;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("PRAGMA table_info(%1);").arg(table))) {
        while (query.next()) {
            columns.append(query.value(1).toString()); // cid, name, type, notnull, dflt_value, pk
        }
    }
    return columns;
}

bool Database::upgrade(int from, UpgradeStats& stats)
{
    m_lastError.clear();
    for (int step = from + 1; step <= kSchemaVersion; ++step) {
        bool ok = false;
        switch (step) {
        case 1: ok = migrateTo1(stats); break;
        case 2: ok = migrateTo2(stats); break;
        default: m_lastError = QStringLiteral("there is no migration step %1.").arg(step); break;
        }
        if (!ok) {
            m_lastError = QStringLiteral("step %1 failed: %2").arg(step).arg(m_lastError);
            return false;
        }
        qInfo() << "[Database] migration step" << step << "done";
    }
    if (from == kSchemaVersion) {
        return true;
    }
    QSqlQuery setVersion(m_db);
    const bool ok = setVersion.exec(QStringLiteral("PRAGMA user_version = %1;").arg(kSchemaVersion));
    if (!ok) {
        m_lastError = sqlText(setVersion.lastError());
    }
    setVersion.finish();
    return ok;
}

// Version 1 — an accounts table from before the "no IBAN / BIC / account
// number" decision (version.md, v0.2.0) can still have those columns and their
// plain-text values. Rebuild the table without them (SQLite's procedure for a
// schema change that ALTER TABLE cannot always do: new table, copy, drop,
// rename). No-op when the columns are not there.
bool Database::migrateTo1(UpgradeStats& stats)
{
    if (accountNumberColumns().isEmpty()) {
        return true;
    }
    const QStringList columns = columnsOf(QStringLiteral("accounts"));

    const QStringList keep = {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("type"),
                              QStringLiteral("category"), QStringLiteral("currency"), QStringLiteral("notes")};
    QStringList copied;
    for (const QString& column : keep) {
        if (columns.contains(column, Qt::CaseInsensitive)) {
            copied.append(column);
        }
    }
    // AUTOINCREMENT: the new table takes over the counter of the old one
    // (RENAME moved its sqlite_sequence row), so that the id of a deleted
    // account is never given to a new one.
    QSqlQuery query(m_db);
    const bool ok =
        query.exec(QStringLiteral("ALTER TABLE accounts RENAME TO accounts_before_v1;")) &&
        m_accountsTable.createTable() &&
        query.exec(QStringLiteral("INSERT INTO accounts (%1) SELECT %1 FROM accounts_before_v1;")
                       .arg(copied.join(QStringLiteral(", ")))) &&
        query.exec(QStringLiteral(
            "UPDATE sqlite_sequence SET seq = MAX(seq, COALESCE("
            " (SELECT seq FROM sqlite_sequence WHERE name = 'accounts_before_v1'), 0))"
            " WHERE name = 'accounts';")) &&
        query.exec(QStringLiteral(
            "INSERT INTO sqlite_sequence (name, seq)"
            " SELECT 'accounts', seq FROM sqlite_sequence WHERE name = 'accounts_before_v1'"
            " AND NOT EXISTS (SELECT 1 FROM sqlite_sequence WHERE name = 'accounts');")) &&
        query.exec(QStringLiteral("DROP TABLE accounts_before_v1;"));
    if (!ok) {
        m_lastError = sqlText(query.lastError());
    }
    stats.droppedAccountNumbers = ok;
    return ok;
}

// Version 2 — one fingerprint per transaction (TransactionsTable), unique
// per account, so that a statement row is saved only once. Existing rows get
// their fingerprint from their stored values. The duplicates that re-imports
// saved before this version are removed (open() saved a copy first).
// Maintainer decision (2026-10-18): of each row, the copy with a category
// stays, then the one with a subcategory, then the oldest one. The removed
// copies that differ from the one that stays are counted and logged.
bool Database::migrateTo2(UpgradeStats& stats)
{
    QSqlQuery query(m_db);
    if (!columnsOf(QStringLiteral("transactions")).contains(QStringLiteral("fingerprint"), Qt::CaseInsensitive) &&
        !query.exec(QStringLiteral("ALTER TABLE transactions ADD COLUMN fingerprint TEXT NOT NULL DEFAULT '';"))) {
        m_lastError = sqlText(query.lastError());
        return false;
    }

    // Read every row first, then write: no UPDATE while the SELECT is open.
    struct Row { int id; QString fingerprint; };
    QVector<Row> rows;
    {
        QSqlQuery select(m_db);
        if (!select.exec(QStringLiteral("SELECT id, operation_date, confirmation_date, debit, credit,"
                                        " balance_after, description FROM transactions;"))) {
            m_lastError = sqlText(select.lastError());
            return false;
        }
        while (select.next()) {
            rows.append({select.value(0).toInt(),
                         TransactionsTable::fingerprint(select.value(1).toString(), select.value(2).toString(),
                                                        select.value(3).toDouble(), select.value(4).toDouble(),
                                                        select.value(5).toDouble(), select.value(6).toString())});
        }
    }
    QSqlQuery update(m_db);
    update.prepare(QStringLiteral("UPDATE transactions SET fingerprint = ? WHERE id = ?;"));
    for (const Row& row : rows) {
        update.addBindValue(row.fingerprint);
        update.addBindValue(row.id);
        if (!update.exec()) {
            m_lastError = sqlText(update.lastError());
            return false;
        }
    }

    // rank 1: the copy that stays.
    static const QString kRanked = QStringLiteral(
        "WITH ranked AS (SELECT *, ROW_NUMBER() OVER (PARTITION BY account_id, fingerprint"
        " ORDER BY (category <> '') DESC, (subcategory <> '') DESC, id) AS copy_rank FROM transactions) ");

    {
        QSqlQuery differ(m_db);
        if (!differ.exec(kRanked + QStringLiteral(
                "SELECT r.id, k.id, r.operation_date, r.description, r.category, r.subcategory, r.name,"
                " r.confirmation_date, r.balance_after, k.operation_date"
                " FROM ranked r JOIN ranked k ON k.account_id = r.account_id AND k.fingerprint = r.fingerprint"
                " AND k.copy_rank = 1"
                " WHERE r.copy_rank > 1 AND (r.category IS NOT k.category OR r.subcategory IS NOT k.subcategory"
                " OR r.name IS NOT k.name OR r.operation_date IS NOT k.operation_date"
                " OR r.confirmation_date IS NOT k.confirmation_date OR r.balance_after IS NOT k.balance_after);"))) {
            m_lastError = sqlText(differ.lastError());
            return false;
        }
        while (differ.next()) {
            ++stats.removedWithOtherValues;
            // Same reference and amount, another operation date: see
            // TransactionsTable::kReusedReferenceIssueUrl.
            if (!TransactionsTable::bankReference(differ.value(3).toString()).isEmpty() &&
                differ.value(2).toString() != differ.value(9).toString()) {
                ++stats.reusedReferences;
            }
            qInfo().noquote() << QStringLiteral("[Database] duplicate transaction %1 removed (transaction %2 "
                                                "stays): %3 \"%4\", category \"%5\", subcategory \"%6\", "
                                                "label \"%7\", confirmation date %8, balance after %9")
                                     .arg(differ.value(0).toString(), differ.value(1).toString(),
                                          differ.value(2).toString(), differ.value(3).toString(),
                                          differ.value(4).toString(), differ.value(5).toString(),
                                          differ.value(6).toString(), differ.value(7).toString(),
                                          differ.value(8).toString());
        }
    }

    if (!query.exec(kRanked + QStringLiteral(
            "DELETE FROM transactions WHERE id IN (SELECT id FROM ranked WHERE copy_rank > 1);"))) {
        m_lastError = sqlText(query.lastError());
        return false;
    }
    stats.removedDuplicates = query.numRowsAffected();

    if (!query.exec(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_transactions_fingerprint"
                                   " ON transactions(account_id, fingerprint);"))) {
        m_lastError = sqlText(query.lastError());
        return false;
    }
    return true;
}

bool    Database::isOpen()     const { return m_db.isOpen(); }
QString Database::lastError()  const { return m_lastError; }

// ── Save ──────────────────────────────────────────────────────────────────────

bool Database::saveSnapshot(const Data::FinancialSnapshot& snapshot)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // 1. Ensure the date row exists
    if (!m_snapshots.upsert(snapshot.date())) {
        m_db.rollback();
        m_lastError = "Could not upsert snapshot date.";
        return false;
    }

    // 2. Resolve the row id
    const auto snapshotId = m_snapshots.idForDate(snapshot.date());
    if (!snapshotId) {
        m_db.rollback();
        m_lastError = "Could not resolve snapshot id.";
        return false;
    }

    // 3. Full replace: delete old entries, insert new ones
    if (!m_assetEntries.deleteForSnapshot(*snapshotId)                              ||
        !m_assetEntries.insertBatch(*snapshotId, "stock", snapshot.stocks())       ||
        !m_assetEntries.insertBatch(*snapshotId, "coin",  snapshot.coins())        ||
        !m_assetEntries.insertBatch(*snapshotId, "metal", snapshot.metals())       ||
        !m_assetEntries.insertBatch(*snapshotId, "etf",   snapshot.etfs()))
    {
        m_db.rollback();
        m_lastError = "Failed to write asset entries.";
        return false;
    }

    m_db.commit();
    return true;
}

// ── Load ──────────────────────────────────────────────────────────────────────

std::optional<Data::FinancialSnapshot>
Database::loadSnapshot(const QDate& date) const
{
    if (!isOpen()) { return std::nullopt; }

    const auto snapshotId = m_snapshots.idForDate(date);
    if (!snapshotId) { return std::nullopt; }

    return m_assetEntries.loadForSnapshot(*snapshotId, date);
}

std::optional<Data::FinancialSnapshot>
Database::loadLatestSnapshot() const
{
    if (!isOpen()) { return std::nullopt; }

    const auto latest = m_snapshots.latest();
    if (!latest) { return std::nullopt; }

    return m_assetEntries.loadForSnapshot(latest->first, latest->second);
}

QVector<QDate> Database::listDates() const
{
    return m_snapshots.allDates();
}

// ── Categories ──────────────────────────────────────────────────────────────────

QVector<Data::Category> Database::loadCategories() const
{
    QVector<Data::Category> cats = m_categoriesTable.all();
    for (Data::Category& cat : cats) {
        cat.subcategories = m_subcategoriesTable.allForCategory(cat.id);
    }
    return cats;
}

int Database::addCategory(const QString& name, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_categoriesTable.insert(name, color);
}

bool Database::renameCategory(int categoryId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_categoriesTable.rename(categoryId, newName);
}

bool Database::setCategoryColor(int categoryId, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_categoriesTable.setColor(categoryId, color);
}

bool Database::removeCategory(int categoryId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // Manual cascade: subcategories first, then the category row itself —
    // same "delete-then-insert" transaction style as saveSnapshot().
    if (!m_subcategoriesTable.removeForCategory(categoryId) || !m_categoriesTable.remove(categoryId)) {
        m_db.rollback();
        m_lastError = "Failed to remove category.";
        return false;
    }

    m_db.commit();
    return true;
}

int Database::addSubcategory(int categoryId, const QString& name, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_subcategoriesTable.insert(categoryId, name, color);
}

bool Database::renameSubcategory(int subcategoryId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.rename(subcategoryId, newName);
}

bool Database::setSubcategoryColor(int subcategoryId, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.setColor(subcategoryId, color);
}

bool Database::removeSubcategory(int subcategoryId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.remove(subcategoryId);
}

// ── Accounts ──────────────────────────────────────────────────────────────────

QVector<Data::Account> Database::loadAccounts() const
{
    QVector<Data::Account> accs = m_accountsTable.all();
    for (Data::Account& acc : accs) {
        acc.hashes = m_accountHashesTable.allForAccount(acc.id);
    }
    return accs;
}

int Database::addAccount(const QString& name, const QString& type, const QString& category,
                          const QString& currency, const QString& notes)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountsTable.insert(name, type, category, currency, notes);
}

bool Database::updateAccount(int accountId, const QString& name, const QString& type, const QString& category,
                              const QString& currency, const QString& notes)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountsTable.update(accountId, name, type, category, currency, notes);
}

bool Database::removeAccount(int accountId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // Manual cascade: hashes, transactions, and balance checkpoints first,
    // then the account row itself — same "delete-then-insert" transaction
    // style as removeCategory()/saveSnapshot().
    if (!m_accountHashesTable.removeForAccount(accountId)      ||
        !m_transactionsTable.removeForAccount(accountId)       ||
        !m_balanceCheckpointsTable.removeForAccount(accountId) ||
        !m_accountsTable.remove(accountId)) {
        m_db.rollback();
        m_lastError = "Failed to remove account.";
        return false;
    }

    m_db.commit();
    return true;
}

int Database::addAccountHash(int accountId, const QString& hash, const QString& source)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountHashesTable.insert(accountId, hash, source);
}

bool Database::removeAccountHash(int hashId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountHashesTable.remove(hashId);
}

bool Database::accountHasHash(int accountId, const QString& hash) const
{
    if (!isOpen()) { return false; }
    return m_accountHashesTable.existsForAccount(accountId, hash);
}

// ── Account categories ────────────────────────────────────────────────────────

QVector<Data::AccountCategory> Database::loadAccountCategories() const
{
    return m_accountCategoriesTable.all();
}

int Database::addAccountCategory(const QString& name, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountCategoriesTable.insert(name, color);
}

bool Database::renameAccountCategory(int accountCategoryId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountCategoriesTable.rename(accountCategoryId, newName);
}

bool Database::setAccountCategoryColor(int accountCategoryId, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountCategoriesTable.setColor(accountCategoryId, color);
}

bool Database::removeAccountCategory(int accountCategoryId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountCategoriesTable.remove(accountCategoryId);
}

// ── Account types ─────────────────────────────────────────────────────────────

QVector<Data::AccountType> Database::loadAccountTypes() const
{
    return m_accountTypesTable.all();
}

int Database::addAccountType(const QString& name)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountTypesTable.insert(name);
}

bool Database::renameAccountType(int typeId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountTypesTable.rename(typeId, newName);
}

bool Database::removeAccountType(int typeId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountTypesTable.remove(typeId);
}

// ── Transactions ──────────────────────────────────────────────────────────────

bool Database::insertTransactions(int accountId, const QVector<Data::Transaction>& txs)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_transactionsTable.insertBatch(accountId, txs);
}

bool Database::saveImport(int accountId, const QVector<Data::Transaction>& txs, const QString& accountHash,
                          const QVector<Data::AccountBalanceCheckpoint>& checkpoints, int* inserted)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    if (!m_db.transaction()) {
        m_lastError = QStringLiteral("Cannot start a transaction: %1").arg(m_db.lastError().text());
        qWarning() << "[Database] saveImport:" << m_lastError;
        return false;
    }
    const auto fail = [this](const QString& what) {
        m_db.rollback();
        m_lastError = what;
        qWarning() << "[Database] saveImport:" << what << "— rolled back, nothing saved";
        return false;
    };

    if (!m_transactionsTable.insertBatch(accountId, txs, inserted)) {
        return fail(QStringLiteral("Could not save the transactions."));
    }
    if (!accountHash.isEmpty() && !m_accountHashesTable.existsForAccount(accountId, accountHash) &&
        m_accountHashesTable.insert(accountId, accountHash, QStringLiteral("auto")) < 0) {
        return fail(QStringLiteral("Could not link the account fingerprint."));
    }
    for (const Data::AccountBalanceCheckpoint& checkpoint : checkpoints) {
        if (m_balanceCheckpointsTable.insert(accountId, checkpoint.date, checkpoint.balance, checkpoint.note) < 0) {
            return fail(QStringLiteral("Could not save a balance checkpoint."));
        }
    }
    if (!m_db.commit()) {
        return fail(QStringLiteral("Could not commit the import: %1").arg(m_db.lastError().text()));
    }
    return true;
}

QHash<QString, QDate> Database::transactionFingerprints(int accountId) const
{
    if (!isOpen()) { return {}; }
    return m_transactionsTable.operationDatesByFingerprint(accountId);
}

QVector<Data::Transaction> Database::transactionsForAccount(int accountId) const
{
    if (!isOpen()) { return {}; }
    return m_transactionsTable.allForAccount(accountId);
}

int Database::transactionCountForAccount(int accountId) const
{
    if (!isOpen()) { return 0; }
    return m_transactionsTable.countForAccount(accountId);
}

bool Database::updateTransaction(const Data::Transaction& tx)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_transactionsTable.updateById(tx);
}

bool Database::clearTransactionsForAccount(int accountId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_transactionsTable.removeForAccount(accountId);
}

std::optional<Data::Transaction> Database::latestTransactionOnOrBefore(int accountId, const QDate& date) const
{
    if (!isOpen()) { return std::nullopt; }
    return m_transactionsTable.latestOnOrBefore(accountId, date);
}

QVector<Data::Transaction> Database::allCategorizedTransactions() const
{
    if (!isOpen()) { return {}; }
    return m_transactionsTable.allCategorized();
}

// ── Category rules ────────────────────────────────────────────────────────────

QVector<Data::CategoryRule> Database::loadCategoryRules() const
{
    if (!isOpen()) { return {}; }
    return m_categoryRulesTable.all();
}

int Database::addCategoryRule(const QString& pattern, const QString& category, const QString& subcategory)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    if (pattern.isEmpty() || category.isEmpty()) {
        m_lastError = "A category rule needs both a pattern and a category.";
        return -1;
    }

    // Upsert rather than insert: the table has UNIQUE(pattern), and the
    // realistic sequence is "teach a rule, then later correct it". A second
    // insert would just fail with a constraint violation, and stacking two
    // rules for the same merchant would leave the predictor picking between
    // contradictory instructions — see CategoryRulesTable's class comment.
    const Data::CategoryRule existing = m_categoryRulesTable.byPattern(pattern);
    if (existing.id >= 0) {
        if (existing.category == category && existing.subcategory == subcategory) {
            return existing.id; // already says exactly this — nothing to write
        }
        if (!m_categoryRulesTable.updateTarget(existing.id, category, subcategory)) {
            m_lastError = "Failed to update category rule.";
            return -1;
        }
        return existing.id;
    }

    Data::CategoryRule rule;
    rule.pattern = pattern;
    rule.category = category;
    rule.subcategory = subcategory;
    return m_categoryRulesTable.insert(rule);
}

bool Database::removeCategoryRule(int ruleId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_categoryRulesTable.remove(ruleId);
}

// ── Balance checkpoints ───────────────────────────────────────────────────────

QVector<Data::AccountBalanceCheckpoint> Database::loadBalanceCheckpoints(int accountId) const
{
    if (!isOpen()) { return {}; }
    return m_balanceCheckpointsTable.allForAccount(accountId);
}

int Database::addBalanceCheckpoint(int accountId, const QDate& date, double balance, const QString& note)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_balanceCheckpointsTable.insert(accountId, date, balance, note);
}

bool Database::removeBalanceCheckpoint(int checkpointId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_balanceCheckpointsTable.remove(checkpointId);
}

std::optional<Data::AccountBalanceCheckpoint> Database::latestCheckpointOnOrBefore(int accountId, const QDate& date) const
{
    if (!isOpen()) { return std::nullopt; }
    return m_balanceCheckpointsTable.latestOnOrBefore(accountId, date);
}

// ── Whole-database reset ──────────────────────────────────────────────────────

bool Database::resetAll()
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // Drop every table this app owns (children before parents doesn't
    // actually matter here — no FK constraints are declared anywhere in
    // this schema — but listed in roughly the same order tables were
    // introduced for readability).
    static const std::array<const char*, 11> kTables = {
        "asset_entries", "snapshots",
        "subcategories", "categories",
        "account_hashes", "transactions", "account_balance_checkpoints", "accounts",
        "account_categories", "account_types",
        "category_rules",
    };

    QSqlQuery query(m_db);
    for (const char* table : kTables) {
        if (!query.exec(QStringLiteral("DROP TABLE IF EXISTS %1;").arg(QLatin1String(table)))) {
            m_db.rollback();
            m_lastError = QStringLiteral("Failed to drop table %1: %2")
                              .arg(QLatin1String(table), query.lastError().text());
            return false;
        }
    }

    // Recreate every table empty (each createTable() is CREATE TABLE IF NOT
    // EXISTS, so this is safe to call again right after dropping them all),
    // then bring them to the latest schema, in the same transaction: if a
    // step fails, the rollback keeps the data as it was.
    UpgradeStats stats;
    m_lastError.clear();
    if (!createBaseTables() || !upgrade(0, stats)) {
        const QString detail = m_lastError;
        m_db.rollback();
        m_lastError = QStringLiteral("Failed to recreate the tables after the reset: %1").arg(detail);
        return false;
    }

    if (!m_db.commit()) {
        m_lastError = QStringLiteral("Failed to commit the reset: %1").arg(m_db.lastError().text());
        m_db.rollback();
        return false;
    }
    return true;
}
