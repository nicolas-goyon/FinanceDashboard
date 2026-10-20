// db_copy_test — "Copy my prod database" of the Test app (#130).
//
// Database::replaceWithCopyOf() and DatabaseManagementController::
// copyProdDatabase(), with a prod and a Test database in a temporary folder,
// laid out as the two apps keep them (prod/finance_dashboard.db and
// test/finance_dashboard.db):
//
//   - the Test database becomes a copy of the prod database, the Test data
//     from before is kept in a file, and the prod file does not change;
//   - the copy of an older prod database is upgraded, with its backup copy and
//     a report, and the prod file keeps its version;
//   - the copy has what the prod app committed while it runs (rows that are
//     only in its WAL file), and what it committed before it stopped without
//     a checkpoint: then the prod file and its WAL do not change either (a
//     read-write connection would write the WAL into the file);
//   - a prod database with a newer schema, a file that is not a database and
//     a missing file are refused before any change;
//   - a copy that does not open (its upgrade fails) is kept, and the Test data
//     comes back;
//   - a Test database that another connection uses (the Test app open twice)
//     is not replaced;
//   - the controller reports the copy and emits databaseReset(), or reports
//     the failure and does not.

#include "DatabaseManagementController.h"
#include "db/Database.h"
#include "models/Category.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

constexpr const char* kFixture = "fixture";
constexpr const char* kProdApp = "prod_app";
constexpr const char* kOtherTestApp = "other_test_app";

} // namespace

class DbCopyTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void copyReplacesTheTestDatabase();
    void olderProdDatabaseIsUpgradedInTheCopy();
    void copyHasWhatTheRunningProdAppCommitted();
    void prodFileWithACommittedWalDoesNotChange();
    void newerProdDatabaseIsRefused();
    void fileThatIsNotADatabaseIsRefused();
    void missingProdDatabaseIsRefused();
    void copyThatDoesNotOpenIsKept();
    void testDatabaseInUseIsNotReplaced();
    void controllerReportsTheCopy();

private:
    // Runs SQL on `path` through its own connection, then closes it.
    static void execOn(const QString& path, const QStringList& statements);
    static int intOn(const QString& path, const QString& sql);
    static QByteArray sha256(const QString& path);
    // The category names of the open database.
    static QStringList categories();
    // The files of `dir` whose name starts with `prefix`.
    static QStringList filesOf(const QString& dir, const QString& prefix);
    // The Test database is open with its own data, and nothing else changed.
    void verifyTestDataStays();

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_prodDir;
    QString m_testDir;
    QString m_prod;
    QString m_test;
};

void DbCopyTest::execOn(const QString& path, const QStringList& statements)
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

int DbCopyTest::intOn(const QString& path, const QString& sql)
{
    int value = -1;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kFixture));
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(sql) && query.next()) {
                value = query.value(0).toInt();
            }
            query.finish();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(QLatin1String(kFixture));
    return value;
}

QByteArray DbCopyTest::sha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

QStringList DbCopyTest::categories()
{
    QStringList names;
    for (const Data::Category& category : Database::instance().loadCategories()) {
        names.append(category.name);
    }
    return names;
}

QStringList DbCopyTest::filesOf(const QString& dir, const QString& prefix)
{
    return QDir(dir).entryList({prefix + QLatin1Char('*')}, QDir::Files, QDir::Name);
}

void DbCopyTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_prodDir = m_dir->filePath(QStringLiteral("prod"));
    m_testDir = m_dir->filePath(QStringLiteral("test"));
    QVERIFY(QDir().mkpath(m_prodDir));
    QVERIFY(QDir().mkpath(m_testDir));
    m_prod = QDir(m_prodDir).filePath(QStringLiteral("finance_dashboard.db"));
    m_test = QDir(m_testDir).filePath(QStringLiteral("finance_dashboard.db"));

    // The prod database, made and closed by the real Database, as the prod
    // app leaves it.
    Database& db = Database::instance();
    QVERIFY2(db.open(m_prod), qPrintable(db.lastError()));
    QVERIFY(db.addCategory(QStringLiteral("Prod category"), QStringLiteral("#a6e3a1")) >= 0);
    db.close();

    // The Test database, open, with its own data.
    QVERIFY2(db.open(m_test), qPrintable(db.lastError()));
    QVERIFY(db.addCategory(QStringLiteral("Test category"), QStringLiteral("#89b4fa")) >= 0);
}

void DbCopyTest::cleanup()
{
    // Close the files before QTemporaryDir removes them (Windows keeps an
    // open file).
    Database::instance().close();
    QSqlDatabase::removeDatabase(QLatin1String(kProdApp));
    QSqlDatabase::removeDatabase(QLatin1String(kOtherTestApp));
    m_dir.reset();
}

void DbCopyTest::verifyTestDataStays()
{
    QVERIFY(Database::instance().isOpen());
    QCOMPARE(categories(), QStringList{QStringLiteral("Test category")});
    QVERIFY(Database::instance().upgradeReport().isEmpty());
    // No copy, no kept file, no backup: only the Test database (and its WAL).
    const QStringList files = filesOf(m_testDir, QStringLiteral("finance_dashboard."));
    for (const QString& file : files) {
        QVERIFY2(file.startsWith(QLatin1String("finance_dashboard.db")), qPrintable(file));
    }
}

void DbCopyTest::copyReplacesTheTestDatabase()
{
    Database& db = Database::instance();
    const QByteArray prodBefore = sha256(m_prod);

    QString kept;
    QVERIFY2(db.replaceWithCopyOf(m_prod, kept), qPrintable(db.lastError()));

    // The Test app uses the copy, at the same path.
    QVERIFY(db.isOpen());
    QCOMPARE(db.sqlDatabase().databaseName(), m_test);
    QCOMPARE(categories(), QStringList{QStringLiteral("Prod category")});
    QVERIFY(db.upgradeReport().isEmpty()); // same schema version: no upgrade
    // The copy can be written to.
    QVERIFY(db.addCategory(QStringLiteral("Written in the copy"), QStringLiteral("#f9e2af")) >= 0);

    // The Test data from before is kept, next to the Test database.
    QCOMPARE(QFileInfo(kept).absolutePath(), QFileInfo(m_test).absolutePath());
    QVERIFY2(QFileInfo(kept).fileName().startsWith(QLatin1String("finance_dashboard.before-copy-")), qPrintable(kept));
    QCOMPARE(intOn(kept, QStringLiteral("SELECT count(*) FROM categories WHERE name = 'Test category';")), 1);
    QVERIFY(filesOf(m_testDir, QStringLiteral("finance_dashboard.copy")).isEmpty());

    // The prod database does not change, and gets no other file than its own
    // (SQLite can leave its -wal and -shm files).
    QCOMPARE(sha256(m_prod), prodBefore);
    for (const QString& file : filesOf(m_prodDir, QString())) {
        QVERIFY2(file.startsWith(QLatin1String("finance_dashboard.db")), qPrintable(file));
    }
    QCOMPARE(intOn(m_prod, QStringLiteral("SELECT count(*) FROM categories WHERE name = 'Written in the copy';")), 0);
}

void DbCopyTest::olderProdDatabaseIsUpgradedInTheCopy()
{
    // A prod database as v0.2.1 left it: schema version 2, without the
    // settings table (db_migration_test, version2DatabaseIsUpgraded).
    execOn(m_prod, {QStringLiteral("DROP TABLE settings;"), QStringLiteral("PRAGMA user_version = 2;")});
    QVERIFY(!QTest::currentTestFailed());
    const QByteArray prodBefore = sha256(m_prod);

    Database& db = Database::instance();
    QString kept;
    QVERIFY2(db.replaceWithCopyOf(m_prod, kept), qPrintable(db.lastError()));

    QCOMPARE(db.schemaVersion(), Database::kSchemaVersion);
    QCOMPARE(categories(), QStringList{QStringLiteral("Prod category")});
    const QString report = db.upgradeReport();
    QVERIFY2(report.contains(QStringLiteral("from schema version 2 to %1").arg(Database::kSchemaVersion)),
             qPrintable(report));
    // Step 3 recorded the defaults of the copy as written: they do not come
    // on top of the prod data.
    QVERIFY(db.defaultsWritten(Database::Defaults::Categories));
    // The backup copy of the upgrade is in the Test folder.
    QCOMPARE(filesOf(m_testDir, QStringLiteral("finance_dashboard.backup-")).size(), 1);
    QVERIFY(QFileInfo::exists(kept));

    // The prod database keeps its version.
    QCOMPARE(sha256(m_prod), prodBefore);
    QCOMPARE(intOn(m_prod, QStringLiteral("PRAGMA user_version;")), 2);
}

void DbCopyTest::copyHasWhatTheRunningProdAppCommitted()
{
    // The prod app runs: its connection is open, and its last rows are only in
    // the WAL file (no checkpoint).
    QSqlDatabase prodApp = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kProdApp));
    prodApp.setDatabaseName(m_prod);
    QVERIFY2(prodApp.open(), qPrintable(prodApp.lastError().text()));
    {
        QSqlQuery query(prodApp);
        QVERIFY(query.exec(QStringLiteral("PRAGMA wal_autocheckpoint = 0;")));
        query.finish();
        QVERIFY2(query.exec(QStringLiteral("INSERT INTO categories (name, color) VALUES ('Committed while it runs', '#cba6f7');")),
                 qPrintable(query.lastError().text()));
    }
    QVERIFY(QFileInfo(m_prod + QStringLiteral("-wal")).size() > 0);

    Database& db = Database::instance();
    QString kept;
    QVERIFY2(db.replaceWithCopyOf(m_prod, kept), qPrintable(db.lastError()));
    QCOMPARE(categories(), (QStringList{QStringLiteral("Prod category"), QStringLiteral("Committed while it runs")}));

    // The prod app continues to work.
    {
        QSqlQuery query(prodApp);
        QVERIFY2(query.exec(QStringLiteral("INSERT INTO categories (name, color) VALUES ('After the copy', '#f5c2e7');")),
                 qPrintable(query.lastError().text()));
    }
    prodApp.close();
    prodApp = QSqlDatabase();
    QCOMPARE(intOn(m_prod, QStringLiteral("SELECT count(*) FROM categories;")), 3);
}

void DbCopyTest::prodFileWithACommittedWalDoesNotChange()
{
    // The prod app stopped without its last checkpoint (it was killed, or it
    // crashed): its last rows are only in finance_dashboard.db-wal, and no
    // connection is open. The files are copied while a connection holds them,
    // as the prod app left them.
    const QString live = m_dir->filePath(QStringLiteral("live.db"));
    QVERIFY(QFile::copy(m_prod, live));
    {
        QSqlDatabase prodApp = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kProdApp));
        prodApp.setDatabaseName(live);
        QVERIFY2(prodApp.open(), qPrintable(prodApp.lastError().text()));
        QSqlQuery query(prodApp);
        QVERIFY(query.exec(QStringLiteral("PRAGMA wal_autocheckpoint = 0;")));
        query.finish();
        QVERIFY2(query.exec(QStringLiteral("INSERT INTO categories (name, color) VALUES ('Only in the WAL', '#cba6f7');")),
                 qPrintable(query.lastError().text()));
        query.finish();
        QVERIFY(QFile::remove(m_prod));
        QVERIFY(QFile::copy(live, m_prod));
        QVERIFY(QFile::copy(live + QStringLiteral("-wal"), m_prod + QStringLiteral("-wal")));
        prodApp.close();
    }
    QSqlDatabase::removeDatabase(QLatin1String(kProdApp));
    QVERIFY(QFileInfo(m_prod + QStringLiteral("-wal")).size() > 0);
    const QByteArray prodBefore = sha256(m_prod);
    const QByteArray walBefore = sha256(m_prod + QStringLiteral("-wal"));

    Database& db = Database::instance();
    QString kept;
    QVERIFY2(db.replaceWithCopyOf(m_prod, kept), qPrintable(db.lastError()));
    QCOMPARE(categories(), (QStringList{QStringLiteral("Prod category"), QStringLiteral("Only in the WAL")}));
    // Read-only: the WAL is not written into the prod file.
    QCOMPARE(sha256(m_prod), prodBefore);
    QCOMPARE(sha256(m_prod + QStringLiteral("-wal")), walBefore);
}

void DbCopyTest::newerProdDatabaseIsRefused()
{
    const int newer = Database::kSchemaVersion + 1;
    execOn(m_prod, {QStringLiteral("PRAGMA user_version = %1;").arg(newer)});
    QVERIFY(!QTest::currentTestFailed());
    const QByteArray prodBefore = sha256(m_prod);

    Database& db = Database::instance();
    QString kept;
    QVERIFY(!db.replaceWithCopyOf(m_prod, kept));
    QVERIFY2(db.lastError().contains(QStringLiteral("schema version %1").arg(newer)), qPrintable(db.lastError()));
    QVERIFY2(db.lastError().contains(QStringLiteral("Nothing was changed")), qPrintable(db.lastError()));
    QVERIFY(kept.isEmpty());
    verifyTestDataStays();
    QCOMPARE(sha256(m_prod), prodBefore);
}

void DbCopyTest::fileThatIsNotADatabaseIsRefused()
{
    const QString junk = QDir(m_prodDir).filePath(QStringLiteral("junk.db"));
    {
        QFile file(junk);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(8192, 'x'));
    }

    Database& db = Database::instance();
    QString kept;
    QVERIFY(!db.replaceWithCopyOf(junk, kept));
    QVERIFY2(db.lastError().contains(QStringLiteral("not a database")), qPrintable(db.lastError()));
    QVERIFY(kept.isEmpty());
    verifyTestDataStays();
}

void DbCopyTest::missingProdDatabaseIsRefused()
{
    const QString missing = QDir(m_prodDir).filePath(QStringLiteral("missing.db"));

    Database& db = Database::instance();
    QString kept;
    QVERIFY(!db.replaceWithCopyOf(missing, kept));
    QVERIFY2(db.lastError().contains(QStringLiteral("There is no database")), qPrintable(db.lastError()));
    QVERIFY(kept.isEmpty());
    QVERIFY(!QFileInfo::exists(missing)); // the copy does not create it
    verifyTestDataStays();

    // The open database itself is refused too.
    QVERIFY(!db.replaceWithCopyOf(m_test, kept));
    QVERIFY2(db.lastError().contains(QStringLiteral("is the open database")), qPrintable(db.lastError()));
    verifyTestDataStays();
}

void DbCopyTest::copyThatDoesNotOpenIsKept()
{
    // A schema version 2 file whose step 3 fails: its settings table has
    // other columns, so the upgrade of the copy fails.
    execOn(m_prod, {
        QStringLiteral("DROP TABLE settings;"),
        QStringLiteral("CREATE TABLE settings (other TEXT);"),
        QStringLiteral("PRAGMA user_version = 2;"),
    });
    QVERIFY(!QTest::currentTestFailed());
    const QByteArray prodBefore = sha256(m_prod);

    Database& db = Database::instance();
    QString kept;
    QVERIFY(!db.replaceWithCopyOf(m_prod, kept));
    QVERIFY2(db.lastError().contains(QStringLiteral("does not open")), qPrintable(db.lastError()));
    QVERIFY2(db.lastError().contains(QStringLiteral("The database from before is back")), qPrintable(db.lastError()));
    QVERIFY(kept.isEmpty());

    // The Test data is back and open; the copy is kept, unchanged (version 2).
    QVERIFY(db.isOpen());
    QCOMPARE(categories(), QStringList{QStringLiteral("Test category")});
    const QStringList failed = filesOf(m_testDir, QStringLiteral("finance_dashboard.copy-failed-"));
    QCOMPARE(failed.size(), 1);
    QVERIFY2(db.lastError().contains(failed.first()), qPrintable(db.lastError()));
    QCOMPARE(intOn(QDir(m_testDir).filePath(failed.first()), QStringLiteral("PRAGMA user_version;")), 2);
    // No backup copy of the failed upgrade, and no kept file.
    QVERIFY(filesOf(m_testDir, QStringLiteral("finance_dashboard.backup-")).isEmpty());
    QVERIFY(filesOf(m_testDir, QStringLiteral("finance_dashboard.before-copy-")).isEmpty());
    QCOMPARE(sha256(m_prod), prodBefore);
}

void DbCopyTest::testDatabaseInUseIsNotReplaced()
{
    // A second connection to the Test database: the Test app is open twice.
    // When the first one closes, the -wal file stays.
    QSqlDatabase other = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kOtherTestApp));
    other.setDatabaseName(m_test);
    QVERIFY2(other.open(), qPrintable(other.lastError().text()));
    {
        QSqlQuery query(other);
        QVERIFY(query.exec(QStringLiteral("SELECT count(*) FROM categories;")));
        QVERIFY(query.next());
    }

    Database& db = Database::instance();
    QString kept;
    QVERIFY(!db.replaceWithCopyOf(m_prod, kept));
    QVERIFY2(db.lastError().contains(QStringLiteral("Another connection uses the database")), qPrintable(db.lastError()));
    QVERIFY(kept.isEmpty());
    other.close();
    other = QSqlDatabase();
    verifyTestDataStays();
}

void DbCopyTest::controllerReportsTheCopy()
{
    DatabaseManagementController controller;
    QSignalSpy reset(&controller, &DatabaseManagementController::databaseReset);
    QSignalSpy report(&controller, &DatabaseManagementController::copyReportChanged);

    // The prod app: no prod database path, no action.
    QVERIFY(!controller.prodDatabaseFound());
    QVERIFY(controller.prodDatabaseText().isEmpty());
    controller.copyProdDatabase();
    QCOMPARE(report.count(), 0);
    QCOMPARE(reset.count(), 0);

    // The Test app.
    controller.setProdDatabasePath(m_prod);
    QVERIFY(controller.prodDatabaseFound());
    QVERIFY2(controller.prodDatabaseText().contains(QDir::toNativeSeparators(m_prod)), qPrintable(controller.prodDatabaseText()));
    QVERIFY2(controller.prodDatabaseText().contains(QStringLiteral("last change on")), qPrintable(controller.prodDatabaseText()));
    controller.copyProdDatabase();
    QCOMPARE(reset.count(), 1);
    QCOMPARE(report.count(), 1);
    QVERIFY(!controller.copyFailed());
    QVERIFY2(controller.copyReport().contains(QStringLiteral("finance_dashboard.before-copy-")), qPrintable(controller.copyReport()));
    QCOMPARE(controller.statusLevel(), QStringLiteral("success"));
    QCOMPARE(categories(), QStringList{QStringLiteral("Prod category")});

    // No prod database: the copy fails, and the pages do not reload.
    controller.setProdDatabasePath(QDir(m_prodDir).filePath(QStringLiteral("missing.db")));
    QVERIFY(!controller.prodDatabaseFound());
    QVERIFY2(controller.prodDatabaseText().contains(QStringLiteral("There is no prod database")), qPrintable(controller.prodDatabaseText()));
    controller.copyProdDatabase();
    QCOMPARE(reset.count(), 1);
    QCOMPARE(report.count(), 2);
    QVERIFY(controller.copyFailed());
    QVERIFY2(controller.copyReport().contains(QStringLiteral("There is no database")), qPrintable(controller.copyReport()));
    QCOMPARE(controller.statusLevel(), QStringLiteral("danger"));
}

QTEST_GUILESS_MAIN(DbCopyTest)
#include "db_copy_test.moc"
