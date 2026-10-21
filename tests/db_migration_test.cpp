// db_migration_test — schema versioning and the migration steps (#20, #18).
//
// Builds databases with a second QSqlDatabase connection, the way an older
// version of the app left them, then opens them with the real Database:
//
//   - a new file gets the latest schema, without a backup or a report;
//   - an old file (user_version 0, accounts with iban / bic / number, the
//     same statement rows saved twice) is upgraded: a backup copy first, the
//     legacy columns dropped (and their values gone from both files), the
//     duplicates removed (the copy with a category stays, then the one with
//     a subcategory, then the oldest; a bank reference and an amount make the
//     identity of a row), fingerprints and the UNIQUE index added, a report;
//   - an upgraded file is not upgraded again;
//   - a file from a newer version of the app is refused, and closed;
//   - a file that is not a database is refused with SQLite's message;
//   - a failed upgrade leaves the file as it was (one SQL transaction for
//     every step), closes it, and leaves no backup copy;
//   - the backup copy is taken before any change to the file;
//   - the accounts rebuild keeps the AUTOINCREMENT counter;
//   - resetAll() keeps the latest schema;
//   - step 3 adds the settings table: an old file with accounts records that
//     the account defaults were written, a new file records nothing, and
//     resetAll() records both (#37);
//   - a file from v0.2.1 (schema version 2) gets only step 3: with data, its
//     defaults count as written; emptied by a reset, it records nothing (#37);
//   - step 4 rounds the stored amounts to the cent, keeps the fingerprints
//     and says how many rows changed; the backup copy keeps the old values
//     (#33).

#include "db/Database.h"
#include "db/tables/TransactionsTable.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <array>
#include <memory>

namespace {

constexpr const char* kFixture = "fixture";

// The transactions table as createTable() makes it (the base schema).
constexpr const char* kTransactionsV0 =
    "CREATE TABLE transactions ("
    "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  account_id        INTEGER NOT NULL,"
    "  confirmation_date TEXT    NOT NULL DEFAULT '',"
    "  operation_date    TEXT    NOT NULL DEFAULT '',"
    "  name              TEXT    NOT NULL DEFAULT '',"
    "  description       TEXT    NOT NULL DEFAULT '',"
    "  debit             REAL    NOT NULL DEFAULT 0,"
    "  credit            REAL    NOT NULL DEFAULT 0,"
    "  balance_after     REAL    NOT NULL DEFAULT 0,"
    "  category          TEXT    NOT NULL DEFAULT '',"
    "  subcategory       TEXT    NOT NULL DEFAULT '',"
    "  source_template   TEXT    NOT NULL DEFAULT ''"
    ");";

// The accounts table with the columns that v0.2.0 decided not to store.
constexpr const char* kAccountsLegacy =
    "CREATE TABLE accounts ("
    "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name     TEXT    NOT NULL UNIQUE,"
    "  type     TEXT    NOT NULL DEFAULT '',"
    "  category TEXT    NOT NULL DEFAULT '',"
    "  currency TEXT    NOT NULL DEFAULT 'EUR',"
    "  notes    TEXT    NOT NULL DEFAULT '',"
    "  iban     TEXT    NOT NULL DEFAULT '',"
    "  bic      TEXT    NOT NULL DEFAULT '',"
    "  number   TEXT    NOT NULL DEFAULT ''"
    ");";

// A transactions table from an old version: no `name` column yet.
constexpr const char* kTransactionsWithoutName =
    "CREATE TABLE transactions ("
    "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  account_id        INTEGER NOT NULL,"
    "  confirmation_date TEXT    NOT NULL DEFAULT '',"
    "  operation_date    TEXT    NOT NULL DEFAULT '',"
    "  description       TEXT    NOT NULL DEFAULT '',"
    "  debit             REAL    NOT NULL DEFAULT 0,"
    "  credit            REAL    NOT NULL DEFAULT 0,"
    "  balance_after     REAL    NOT NULL DEFAULT 0,"
    "  category          TEXT    NOT NULL DEFAULT '',"
    "  subcategory       TEXT    NOT NULL DEFAULT '',"
    "  source_template   TEXT    NOT NULL DEFAULT ''"
    ");";

// A damaged transactions table: no `description`, so that migration step 2
// fails after step 1 has rebuilt the accounts table.
constexpr const char* kTransactionsWithoutDescription =
    "CREATE TABLE transactions ("
    "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  account_id        INTEGER NOT NULL,"
    "  confirmation_date TEXT    NOT NULL DEFAULT '',"
    "  operation_date    TEXT    NOT NULL DEFAULT '',"
    "  debit             REAL    NOT NULL DEFAULT 0,"
    "  credit            REAL    NOT NULL DEFAULT 0,"
    "  balance_after     REAL    NOT NULL DEFAULT 0"
    ");";

} // namespace

class DbMigrationTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void newDatabaseGetsTheLatestSchema();
    void oldDatabaseIsUpgraded();
    void upgradedDatabaseIsNotUpgradedAgain();
    void newerSchemaIsRefused();
    void corruptFileIsRefused();
    void failedUpgradeChangesNothing();
    void backupIsTakenBeforeAnyChange();
    void accountCounterIsKept();
    void resetKeepsTheLatestSchema();
    void version2DatabaseIsUpgraded();
    void version3AmountsAreRoundedToTheCent();

private:
    // Runs SQL on `path` through its own connection, then closes it.
    static void execOn(const QString& path, const QStringList& statements);
    static int countOn(const QString& path, const QString& sql);
    void createOldDatabase(const QString& path);
    [[nodiscard]] QStringList backups() const;
    [[nodiscard]] static int count(const QString& sql);
    [[nodiscard]] static QStringList columns(const QString& table);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void DbMigrationTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void DbMigrationTest::cleanup()
{
    Database::instance().close();
    m_dir.reset();
}

void DbMigrationTest::execOn(const QString& path, const QStringList& statements)
{
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kFixture));
        db.setDatabaseName(path);
        QVERIFY2(db.open(), qPrintable(db.lastError().text()));
        QSqlQuery query(db);
        for (const QString& sql : statements) {
            QVERIFY2(query.exec(sql), qPrintable(sql + QStringLiteral(" — ") + query.lastError().text()));
        }
        query.finish();
        db.close();
    }
    QSqlDatabase::removeDatabase(QLatin1String(kFixture));
}

int DbMigrationTest::countOn(const QString& path, const QString& sql)
{
    int result = -1;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kFixture));
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(sql) && query.next()) {
                result = query.value(0).toInt();
            }
            query.finish();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(QLatin1String(kFixture));
    return result;
}

void DbMigrationTest::createOldDatabase(const QString& path)
{
    execOn(path, {
        QString::fromLatin1(kAccountsLegacy),
        QStringLiteral("INSERT INTO accounts (name, type, category, currency, notes, iban, bic, number) VALUES"
                       " ('Checking', 'CCP', 'Day-to-day', 'EUR', 'main', 'FR7630006000011234567890189',"
                       "  'PSSTFRPP', '00012345678'),"
                       " ('Savings', 'Livret A', 'Savings', 'EUR', '', '', '', '');"),
        QString::fromLatin1(kTransactionsV0),
        // Account 1, ids 1–4: row A twice (a re-import; the first copy has
        // the category), row B, and B' (same as B but another balance, so not
        // a duplicate). Account 2, id 5: the content of row A, which is not a
        // duplicate in another account. Account 1 again, ids 6–10: row C twice
        // (only the second copy has a category), row D twice with the same
        // bank reference and amount but another confirmation date and balance
        // (a later export), and an exact copy of row B.
        QStringLiteral("INSERT INTO transactions (account_id, confirmation_date, operation_date, name,"
                       " description, debit, credit, balance_after, category, subcategory) VALUES"
                       " (1, '2026-03-02', '2026-03-02', 'CARREFOUR', 'Card payment: CARREFOUR ref A1', 20.0, 0, 480.0, 'Groceries', ''),"
                       " (1, '2026-03-02', '2026-03-02', 'CARREFOUR', 'Card payment: CARREFOUR ref A1', 20.0, 0, 480.0, '', ''),"
                       " (1, '2026-03-05', '2026-03-05', 'SNCF', 'Card payment: SNCF ref A2', 30.0, 0, 450.0, '', ''),"
                       " (1, '2026-03-05', '2026-03-05', 'SNCF', 'Card payment: SNCF ref A2', 30.0, 0, 451.0, '', ''),"
                       " (2, '2026-03-02', '2026-03-02', 'CARREFOUR', 'Card payment: CARREFOUR ref A1', 20.0, 0, 480.0, '', ''),"
                       " (1, '2026-03-07', '2026-03-07', 'BAKERY', 'Card payment: BAKERY ref A5', 5.0, 0, 445.0, '', ''),"
                       " (1, '2026-03-07', '2026-03-07', 'BAKERY', 'Card payment: BAKERY ref A5', 5.0, 0, 445.0, 'Food', 'Bakery'),"
                       " (1, '2026-03-10', '2026-03-09', 'CINEMA', 'Card payment 1234: CINEMA Internal ref.: R9', 12.0, 0, 433.0, '', ''),"
                       " (1, '2026-03-11', '2026-03-09', 'CINEMA', 'Card payment 1234: CINEMA Internal ref.: R9', 12.0, 0, 421.0, '', ''),"
                       " (1, '2026-03-05', '2026-03-05', 'SNCF', 'Card payment: SNCF ref A2', 30.0, 0, 450.0, '', ''),"
                       // Ids 11–13: three monthly rent transfers. The sender's
                       // label looks like a reference; each has its own bank
                       // reference at the end, so none is a duplicate.
                       " (1, '2026-01-05', '2026-01-05', 'LANDLORD', 'Transfer to LANDLORD Rent internal ref: APT12 Internal ref.: K1', 950.0, 0, 2050.0, '', ''),"
                       " (1, '2026-02-05', '2026-02-05', 'LANDLORD', 'Transfer to LANDLORD Rent internal ref: APT12 Internal ref.: K2', 950.0, 0, 1100.0, '', ''),"
                       " (1, '2026-03-05', '2026-03-05', 'LANDLORD', 'Transfer to LANDLORD Rent internal ref: APT12 Internal ref.: K3', 950.0, 0, 150.0, '', ''),"
                       // Ids 14–15: the same bank reference and amount on two
                       // operation dates (a reused reference, issue #126).
                       " (1, '2026-02-10', '2026-02-10', 'POWER', 'Direct debit: POWER Internal ref.: M1', 60.0, 0, 1040.0, '', ''),"
                       " (1, '2026-03-10', '2026-03-10', 'POWER', 'Direct debit: POWER Internal ref.: M1', 60.0, 0, 90.0, '', '');"),
    });
}

QStringList DbMigrationTest::backups() const
{
    return QDir(m_dir->path()).entryList({QStringLiteral("*.backup-*")}, QDir::Files);
}

int DbMigrationTest::count(const QString& sql)
{
    QSqlQuery query(Database::instance().sqlDatabase());
    if (!query.exec(sql) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

QStringList DbMigrationTest::columns(const QString& table)
{
    QStringList result;
    QSqlQuery query(Database::instance().sqlDatabase());
    if (query.exec(QStringLiteral("PRAGMA table_info(%1);").arg(table))) {
        while (query.next()) {
            result.append(query.value(1).toString());
        }
    }
    return result;
}

void DbMigrationTest::newDatabaseGetsTheLatestSchema()
{
    Database& db = Database::instance();
    QVERIFY2(db.open(m_dir->filePath(QStringLiteral("new.db"))), qPrintable(db.lastError()));
    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);
    QVERIFY(db.upgradeReport().isEmpty());
    QVERIFY(backups().isEmpty());
    QVERIFY(columns(QStringLiteral("transactions")).contains(QStringLiteral("fingerprint")));
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'index'"
                                  " AND name = 'idx_transactions_fingerprint';")), 1);
    // Step 3: the settings table, with no record. The first run writes the
    // defaults.
    QVERIFY(columns(QStringLiteral("settings")).contains(QStringLiteral("value")));
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM settings;")), 0);
    QVERIFY(!db.defaultsWritten(Database::Defaults::Categories));
    QVERIFY(!db.defaultsWritten(Database::Defaults::Accounts));
}

void DbMigrationTest::oldDatabaseIsUpgraded()
{
    const QString path = m_dir->filePath(QStringLiteral("old.db"));
    createOldDatabase(path);
    QVERIFY(!QTest::currentTestFailed());

    Database& db = Database::instance();
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);

    // Accounts: legacy columns gone, rows and ids kept.
    const QStringList accountColumns = columns(QStringLiteral("accounts"));
    QVERIFY(!accountColumns.contains(QStringLiteral("iban")));
    QVERIFY(!accountColumns.contains(QStringLiteral("bic")));
    QVERIFY(!accountColumns.contains(QStringLiteral("number")));
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM accounts;")), 2);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM accounts WHERE id = 1 AND name = 'Checking'"
                                  " AND type = 'CCP' AND currency = 'EUR' AND notes = 'main';")), 1);

    // Transactions: 5 duplicates removed (ids 2, 6, 9, 10, 15). Of each row,
    // the copy with a category stays (ids 1 and 7), else the oldest (ids 3,
    // 8, 14). The three rent transfers stay.
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE account_id = 1;")), 9);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE account_id = 2;")), 1);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE id IN (1, 3, 4, 5, 7, 8, 11, 12, 13, 14);")), 10);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE id = 1 AND category = 'Groceries';")), 1);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE id = 7 AND category = 'Food'"
                                  " AND subcategory = 'Bakery';")), 1);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE fingerprint = '';")), 0);

    const QString report = db.upgradeReport();
    QVERIFY2(report.contains(QStringLiteral("from schema version 0 to %1").arg(Database::kSchemaVersion)),
             qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("5 duplicate transaction")), qPrintable(report));
    // Ids 2, 6, 9 and 15 differ from the copy that stays; id 10 is an exact
    // copy. Id 15 has the reference of id 14 with another operation date.
    QVERIFY2(report.contains(QStringLiteral("4 of the removed copies had another")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Warning: 1 of the removed copies")), qPrintable(report));
    QVERIFY2(report.contains(QLatin1String(TransactionsTable::kReusedReferenceIssueUrl)), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("IBAN, BIC and account numbers were removed")), qPrintable(report));
    // Step 3: the file has accounts, so their defaults count as written; it has
    // no category, so the default categories come on the next start.
    QVERIFY(db.defaultsWritten(Database::Defaults::Accounts));
    QVERIFY(!db.defaultsWritten(Database::Defaults::Categories));

    // The backup is the database as it was before the upgrade.
    const QStringList copies = backups();
    QCOMPARE(copies.size(), qsizetype(1));
    QVERIFY2(db.upgradeReport().contains(copies.first()), qPrintable(db.upgradeReport()));
    const QString backup = m_dir->filePath(copies.first());
    QCOMPARE(countOn(backup, QStringLiteral("PRAGMA user_version;")), 0);
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM transactions;")), 15);
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM accounts;")), 2);
    // No account number in the copy, and none left in either file.
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM accounts"
                                            " WHERE iban != '' OR bic != '' OR number != '';")), 0);
    for (const QString& file : {backup, path, path + QStringLiteral("-wal")}) {
        QFile bytes(file);
        if (!bytes.exists()) {
            continue;
        }
        QVERIFY(bytes.open(QIODevice::ReadOnly));
        const QByteArray content = bytes.readAll();
        QVERIFY2(!content.contains("FR7630006000011234567890189"), qPrintable(file));
        QVERIFY2(!content.contains("PSSTFRPP"), qPrintable(file));
        QVERIFY2(!content.contains("00012345678"), qPrintable(file));
    }
}

void DbMigrationTest::upgradedDatabaseIsNotUpgradedAgain()
{
    const QString path = m_dir->filePath(QStringLiteral("old.db"));
    createOldDatabase(path);
    QVERIFY(!QTest::currentTestFailed());

    Database& db = Database::instance();
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    db.close();
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);
    QVERIFY(db.upgradeReport().isEmpty());
    QCOMPARE(backups().size(), qsizetype(1));
}

void DbMigrationTest::newerSchemaIsRefused()
{
    const QString path = m_dir->filePath(QStringLiteral("newer.db"));
    execOn(path, {QString::fromLatin1(kTransactionsV0), QStringLiteral("PRAGMA user_version = 99;")});
    QVERIFY(!QTest::currentTestFailed());

    Database& db = Database::instance();
    QVERIFY(!db.open(path));
    QVERIFY2(db.lastError().contains(QStringLiteral("schema version 99")), qPrintable(db.lastError()));
    QCOMPARE(db.openError(), Database::OpenError::NewerSchema);
    // Closed: nothing can write to the file.
    QVERIFY(!db.isOpen());
    QCOMPARE(db.addAccount(QStringLiteral("Checking"), QStringLiteral("CCP"), QString(), QStringLiteral("EUR"),
                           QString()),
             -1);
    // Left as it was: same version, no table added.
    QCOMPARE(countOn(path, QStringLiteral("PRAGMA user_version;")), 99);
    QCOMPARE(countOn(path, QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table'"
                                          " AND name != 'sqlite_sequence';")),
             1);
    QVERIFY(backups().isEmpty());
}

void DbMigrationTest::corruptFileIsRefused()
{
    const QString path = m_dir->filePath(QStringLiteral("corrupt.db"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(8192, 'x'));
    file.close();

    Database& db = Database::instance();
    QVERIFY(!db.open(path));
    QCOMPARE(db.openError(), Database::OpenError::CannotOpen);
    QVERIFY2(db.lastError().contains(QStringLiteral("not a database")), qPrintable(db.lastError()));
    QVERIFY(!db.isOpen());
    QVERIFY(backups().isEmpty());
}

void DbMigrationTest::failedUpgradeChangesNothing()
{
    const QString path = m_dir->filePath(QStringLiteral("damaged.db"));
    execOn(path, {
        QString::fromLatin1(kAccountsLegacy),
        QStringLiteral("INSERT INTO accounts (name, iban) VALUES ('Checking', 'FR7630006000011234567890189');"),
        QString::fromLatin1(kTransactionsWithoutDescription),
        QStringLiteral("INSERT INTO transactions (account_id, debit) VALUES (1, 20.0);"),
    });
    QVERIFY(!QTest::currentTestFailed());

    Database& db = Database::instance();
    QVERIFY(!db.open(path));
    QCOMPARE(db.openError(), Database::OpenError::UpgradeFailed);
    QVERIFY2(db.lastError().contains(QStringLiteral("step 2")), qPrintable(db.lastError()));
    QVERIFY2(db.lastError().contains(QStringLiteral("not changed")), qPrintable(db.lastError()));
    QVERIFY(!db.isOpen());

    // Step 1 (the accounts rebuild) was rolled back with step 2, and so were
    // the base tables and the `name` column.
    QCOMPARE(countOn(path, QStringLiteral("PRAGMA user_version;")), 0);
    QCOMPARE(countOn(path, QStringLiteral("SELECT COUNT(*) FROM pragma_table_info('accounts') WHERE name = 'iban';")), 1);
    QCOMPARE(countOn(path, QStringLiteral("SELECT COUNT(*) FROM accounts WHERE iban != '';")), 1);
    QCOMPARE(countOn(path, QStringLiteral("SELECT COUNT(*) FROM pragma_table_info('transactions')"
                                          " WHERE name IN ('name', 'fingerprint');")),
             0);
    QCOMPARE(countOn(path, QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE name = 'category_rules';")), 0);
    QCOMPARE(countOn(path, QStringLiteral("SELECT COUNT(*) FROM transactions;")), 1);
    // The file is unchanged, so the copy of this start was removed: a start
    // that fails again does not add one more.
    QVERIFY(backups().isEmpty());
    QVERIFY(!db.open(path));
    QVERIFY(backups().isEmpty());
}

void DbMigrationTest::backupIsTakenBeforeAnyChange()
{
    const QString path = m_dir->filePath(QStringLiteral("older.db"));
    execOn(path, {
        QString::fromLatin1(kAccountsLegacy),
        QStringLiteral("INSERT INTO accounts (name) VALUES ('Checking');"),
        QString::fromLatin1(kTransactionsWithoutName),
        QStringLiteral("INSERT INTO transactions (account_id, operation_date, description, debit)"
                       " VALUES (1, '2026-03-02', 'Card payment: CARREFOUR', 20.0);"),
    });
    QVERIFY(!QTest::currentTestFailed());

    Database& db = Database::instance();
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QVERIFY(columns(QStringLiteral("transactions")).contains(QStringLiteral("name")));

    const QStringList copies = backups();
    QCOMPARE(copies.size(), qsizetype(1));
    const QString backup = m_dir->filePath(copies.first());
    // As the file was: no `name` column, no table that createTable() adds.
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM pragma_table_info('transactions') WHERE name = 'name';")), 0);
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE name = 'category_rules';")), 0);
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM transactions;")), 1);
}

void DbMigrationTest::accountCounterIsKept()
{
    const QString path = m_dir->filePath(QStringLiteral("deleted.db"));
    execOn(path, {
        QString::fromLatin1(kAccountsLegacy),
        QStringLiteral("INSERT INTO accounts (name) VALUES ('Checking'), ('Savings'), ('Closed');"),
        QStringLiteral("DELETE FROM accounts WHERE name = 'Closed';"), // id 3: counter 3, highest id 2
        QString::fromLatin1(kTransactionsV0),
    });
    QVERIFY(!QTest::currentTestFailed());

    Database& db = Database::instance();
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QCOMPARE(count(QStringLiteral("SELECT seq FROM sqlite_sequence WHERE name = 'accounts';")), 3);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM sqlite_sequence WHERE name = 'accounts_before_v1';")), 0);
    // The id of the deleted account is not given again.
    QCOMPARE(db.addAccount(QStringLiteral("New"), QStringLiteral("CCP"), QStringLiteral(""), QStringLiteral("EUR"),
                           QStringLiteral("")),
             4);
}

void DbMigrationTest::resetKeepsTheLatestSchema()
{
    Database& db = Database::instance();
    QVERIFY2(db.open(m_dir->filePath(QStringLiteral("reset.db"))), qPrintable(db.lastError()));
    QVERIFY2(db.resetAll(), qPrintable(db.lastError()));
    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);
    QVERIFY(columns(QStringLiteral("transactions")).contains(QStringLiteral("fingerprint")));
    // After a reset, no default data comes back (#37).
    QVERIFY(db.defaultsWritten(Database::Defaults::Categories));
    QVERIFY(db.defaultsWritten(Database::Defaults::Accounts));
}

void DbMigrationTest::version2DatabaseIsUpgraded()
{
    // Two files as v0.2.1 left them: schema version 2, that is the latest
    // schema without the settings table. One has data, and "Reset Database"
    // of v0.2.1 emptied the other one.
    Database& db = Database::instance();
    const QString used = m_dir->filePath(QStringLiteral("used-v2.db"));
    const QString emptied = m_dir->filePath(QStringLiteral("emptied-v2.db"));
    for (const QString& path : {used, emptied}) {
        QVERIFY2(db.open(path), qPrintable(db.lastError()));
        db.close();
        execOn(path, {QStringLiteral("DROP TABLE settings;"), QStringLiteral("PRAGMA user_version = 2;")});
    }
    execOn(used, {
        QStringLiteral("INSERT INTO categories (name, color) VALUES ('Groceries', '#a6e3a1');"),
        QStringLiteral("INSERT INTO account_types (name) VALUES ('CCP');"),
    });
    QVERIFY(!QTest::currentTestFailed());

    QVERIFY2(db.open(used), qPrintable(db.lastError()));
    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);
    QVERIFY2(db.upgradeReport().contains(QStringLiteral("from schema version 2 to %1").arg(Database::kSchemaVersion)),
             qPrintable(db.upgradeReport()));
    QCOMPARE(backups().size(), qsizetype(1));
    // It has data: its defaults count as written, and its data stays.
    QVERIFY(db.defaultsWritten(Database::Defaults::Categories));
    QVERIFY(db.defaultsWritten(Database::Defaults::Accounts));
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM categories;")), 1);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM account_types;")), 1);
    db.close();

    // Empty: step 3 cannot tell it from a new file, so it records nothing.
    // The defaults come one more time, at the first start after the upgrade
    // (the changelog says it).
    QVERIFY2(db.open(emptied), qPrintable(db.lastError()));
    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);
    QVERIFY(!db.defaultsWritten(Database::Defaults::Categories));
    QVERIFY(!db.defaultsWritten(Database::Defaults::Accounts));
}

void DbMigrationTest::version3AmountsAreRoundedToTheCent()
{
    // A file as the builds of v0.2.2 before #33 left it: schema version 3,
    // amounts with more than two decimals (a typed amount, a reconciliation
    // adjustment, a sum), and the fingerprints of the unrounded amounts.
    Database& db = Database::instance();
    const QString path = m_dir->filePath(QStringLiteral("v3.db"));
    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    db.close();
    const QString typed = TransactionsTable::fingerprint(QStringLiteral("2026-03-02"), QStringLiteral("2026-03-02"),
                                                         12.344, 0.0, 487.656, QStringLiteral("Typed amount"));
    const QString adjustment = TransactionsTable::fingerprint(QStringLiteral("2026-03-31"), QStringLiteral("2026-03-31"),
                                                              0.0, 0.00999999999999995, 0.31,
                                                              QStringLiteral("Auto-added"));
    const QString exact = TransactionsTable::fingerprint(QStringLiteral("2026-03-05"), QStringLiteral("2026-03-05"),
                                                         5.0, 0.0, 482.66, QStringLiteral("Exact amount"));
    execOn(path, {
        QStringLiteral("INSERT INTO accounts (name, type, category, currency, notes)"
                       " VALUES ('Checking', 'CCP', '', 'EUR', '');"),
        QStringLiteral("INSERT INTO transactions (account_id, confirmation_date, operation_date, name, description,"
                       " debit, credit, balance_after, fingerprint) VALUES"
                       " (1, '2026-03-02', '2026-03-02', 'SHOP', 'Typed amount', 12.344, 0, 487.656, '%1'),"
                       " (1, '2026-03-31', '2026-03-31', 'ADJUSTMENT', 'Auto-added', 0, 0.00999999999999995, 0.31, '%2'),"
                       " (1, '2026-03-05', '2026-03-05', 'BAKERY', 'Exact amount', 5, 0, 482.66, '%3');")
            .arg(typed, adjustment, exact),
        QStringLiteral("INSERT INTO account_balance_checkpoints (account_id, date, balance, note) VALUES"
                       " (1, '2026-03-01', 500.0000001, ''), (1, '2026-03-31', 0.31, '');"),
        QStringLiteral("PRAGMA user_version = 3;"),
    });
    QVERIFY(!QTest::currentTestFailed());

    QVERIFY2(db.open(path), qPrintable(db.lastError()));
    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);
    // Two transactions and one checkpoint changed.
    QVERIFY2(db.upgradeReport().contains(QStringLiteral("3 row(s) had an amount with more than two decimals")),
             qPrintable(db.upgradeReport()));

    // Exact comparisons: the stored doubles are the ones of whole cents.
    QSqlQuery query(db.sqlDatabase());
    QVERIFY(query.exec(QStringLiteral("SELECT debit, credit, balance_after, fingerprint FROM transactions ORDER BY id;")));
    const std::array<std::array<double, 3>, 3> expected = {{{12.34, 0.0, 487.66}, {0.0, 0.01, 0.31}, {5.0, 0.0, 482.66}}};
    const QStringList fingerprints = {typed, adjustment, exact};
    for (qsizetype row = 0; row < 3; ++row) {
        QVERIFY(query.next());
        const auto& amounts = expected.at(static_cast<size_t>(row));
        for (int column = 0; column < 3; ++column) {
            QVERIFY2(query.value(column).toDouble() == amounts.at(static_cast<size_t>(column)),
                     qPrintable(QStringLiteral("row %1, column %2: %3").arg(row).arg(column)
                                    .arg(query.value(column).toDouble(), 0, 'g', 17)));
        }
        // The fingerprint stays, and it is also the one of the rounded amounts.
        QCOMPARE(query.value(3).toString(), fingerprints.at(row));
    }
    QCOMPARE(TransactionsTable::fingerprint(QStringLiteral("2026-03-02"), QStringLiteral("2026-03-02"), 12.34, 0.0,
                                            487.66, QStringLiteral("Typed amount")),
             typed);
    QVERIFY(query.exec(QStringLiteral("SELECT balance FROM account_balance_checkpoints ORDER BY id;")));
    QVERIFY(query.next());
    QVERIFY(query.value(0).toDouble() == 500.0);
    QVERIFY(query.next());
    QVERIFY(query.value(0).toDouble() == 0.31);
    query.finish();
    db.close();

    // The copy from before the upgrade keeps the old values.
    QCOMPARE(backups().size(), qsizetype(1));
    const QString backup = m_dir->filePath(backups().first());
    QCOMPARE(countOn(backup, QStringLiteral("SELECT ROUND(debit * 1000) FROM transactions WHERE id = 1;")), 12344);
    QCOMPARE(countOn(backup, QStringLiteral("PRAGMA user_version;")), 3);
}

QTEST_GUILESS_MAIN(DbMigrationTest)
#include "db_migration_test.moc"
