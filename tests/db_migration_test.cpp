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
//   - a file from a newer version of the app is refused;
//   - resetAll() keeps the latest schema.

#include "db/Database.h"

#include <QDir>
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
    QCOMPARE(countOn(path, QStringLiteral("PRAGMA user_version;")), 99); // left as it was
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
