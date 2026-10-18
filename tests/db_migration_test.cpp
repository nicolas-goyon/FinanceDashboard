// db_migration_test — schema versioning and the migration steps (#20, #18).
//
// Builds databases with a second QSqlDatabase connection, the way an older
// version of the app left them, then opens them with the real Database:
//
//   - a new file gets the latest schema, without a backup or a report;
//   - an old file (user_version 0, accounts with iban / bic / number, the
//     same statement rows saved twice) is upgraded: a backup copy first, the
//     legacy columns dropped, the exact duplicates removed (the lowest id
//     stays), fingerprints and the UNIQUE index added, and a report;
//   - an upgraded file is not upgraded again;
//   - a file from a newer version of the app is refused, and closed;
//   - a file that is not a database is refused with SQLite's message;
//   - a failed upgrade leaves the file as it was (one SQL transaction for
//     every step), closes it, and leaves no backup copy;
//   - the backup copy is taken before any change to the file;
//   - the accounts rebuild keeps the AUTOINCREMENT counter;
//   - resetAll() keeps the latest schema.

#include "db/Database.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

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
        // Account 1: row A twice (a re-import), row B, and B' (same as B but
        // another balance, so not a duplicate). Account 2: the same content
        // as row A, which is not a duplicate in another account.
        QStringLiteral("INSERT INTO transactions (account_id, confirmation_date, operation_date, name,"
                       " description, debit, credit, balance_after, category) VALUES"
                       " (1, '2026-03-02', '2026-03-02', 'CARREFOUR', 'Card payment: CARREFOUR ref A1', 20.0, 0, 480.0, 'Groceries'),"
                       " (1, '2026-03-02', '2026-03-02', 'CARREFOUR', 'Card payment: CARREFOUR ref A1', 20.0, 0, 480.0, ''),"
                       " (1, '2026-03-05', '2026-03-05', 'SNCF', 'Card payment: SNCF ref A2', 30.0, 0, 450.0, ''),"
                       " (1, '2026-03-05', '2026-03-05', 'SNCF', 'Card payment: SNCF ref A2', 30.0, 0, 451.0, ''),"
                       " (2, '2026-03-02', '2026-03-02', 'CARREFOUR', 'Card payment: CARREFOUR ref A1', 20.0, 0, 480.0, '');"),
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

    // Transactions: the one exact duplicate removed, the first copy kept.
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE account_id = 1;")), 3);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE account_id = 2;")), 1);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE id = 1 AND category = 'Groceries';")), 1);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE id = 2;")), 0);
    QCOMPARE(count(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE fingerprint = '';")), 0);

    QVERIFY2(db.upgradeReport().contains(QStringLiteral("from schema version 0 to %1").arg(Database::kSchemaVersion)),
             qPrintable(db.upgradeReport()));
    QVERIFY2(db.upgradeReport().contains(QStringLiteral("1 duplicate transaction")), qPrintable(db.upgradeReport()));

    // The backup is the database as it was before the upgrade.
    const QStringList copies = backups();
    QCOMPARE(copies.size(), qsizetype(1));
    QVERIFY2(db.upgradeReport().contains(copies.first()), qPrintable(db.upgradeReport()));
    const QString backup = m_dir->filePath(copies.first());
    QCOMPARE(countOn(backup, QStringLiteral("PRAGMA user_version;")), 0);
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM transactions;")), 5);
    QCOMPARE(countOn(backup, QStringLiteral("SELECT COUNT(*) FROM accounts WHERE iban != '';")), 1);
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
}

QTEST_GUILESS_MAIN(DbMigrationTest)
#include "db_migration_test.moc"
