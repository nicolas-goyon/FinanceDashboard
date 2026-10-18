// import_save_test — the statement-import save path, end to end.
//
// Runs the real SnapshotImportController and the real Database on a temporary
// SQLite file: parse a Sumeria CSV statement, change rows the way the review
// screen does, then save. Each test starts from an empty database.
//
//   #16  rows with an empty category, subcategory or date are saved
//        (a null QString used to be bound as SQL NULL in NOT NULL columns)
//   #17  a failed save writes nothing, and a retry stores each row once
//   #18  a re-import adds nothing, an overlapping statement adds only its new rows;
//        two identical rows of one statement are two transactions; a row that
//        breaks a NOT NULL column fails the save instead of being skipped
//   #29  the parser reads the statement this file writes

#include "SnapshotImportController.h"
#include "db/Database.h"
#include "models/Account.h"
#include "models/CategoryRule.h"
#include "models/Transaction.h"
#include "utils/AccountHashing.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantMap>

#include <limits>

namespace {

constexpr const char* kTemplate = "sumeria_ccp_csv_extraction";
constexpr const char* kIban = "FR7630006000011234567890189";
constexpr const char* kBic = "PSSTFRPP";

// A March statement: 500.00 at the start, three card payments, 440.00 at the
// end. `endBalance` lets a test declare a closing balance that does not match.
QByteArray marchStatement(const char* endBalance = "440.00")
{
    QByteArray csv;
    csv += "IBAN,";
    csv += kIban;
    csv += "\nBIC,";
    csv += kBic;
    csv += "\nPeriod,From 03/01/2026 to 03/31/2026\n";
    csv += "Account balance on 03/01/2026,500.00 \xE2\x82\xAC\n";
    csv += "Account balance on 03/31/2026,";
    csv += endBalance;
    csv += " \xE2\x82\xAC\n";
    csv += "Confirmation date/ value,Operation date,Description and identifier,Debit (-),Credit (+),Balance\n";
    csv += "03/02,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,480.00\n";
    csv += "03/05,03/05,\"Card payment 1234: SNCF\nInternal ref.: A2\",-30.00,,450.00\n";
    csv += "03/09,03/09,\"Card payment 1234: BOULANGERIE\nInternal ref.: A3\",-10.00,,440.00\n";
    return csv;
}

// An April-style statement that overlaps the March one: it starts after the
// first March row, repeats the last two March rows exactly, and adds one.
QByteArray overlappingStatement()
{
    QByteArray csv;
    csv += "IBAN,";
    csv += kIban;
    csv += "\nBIC,";
    csv += kBic;
    csv += "\nPeriod,From 03/04/2026 to 03/31/2026\n";
    csv += "Account balance on 03/04/2026,480.00 \xE2\x82\xAC\n";
    csv += "Account balance on 03/31/2026,40.00 \xE2\x82\xAC\n";
    csv += "Confirmation date/ value,Operation date,Description and identifier,Debit (-),Credit (+),Balance\n";
    csv += "03/05,03/05,\"Card payment 1234: SNCF\nInternal ref.: A2\",-30.00,,450.00\n";
    csv += "03/09,03/09,\"Card payment 1234: BOULANGERIE\nInternal ref.: A3\",-10.00,,440.00\n";
    csv += "03/15,03/15,\"Transfer: RENT MARCH\nInternal ref.: A4\",-400.00,,40.00\n";
    return csv;
}

// A statement with the same row `copies` times: same dates, amount and
// description, and an empty balance cell, so nothing in the file tells the
// rows apart.
QByteArray repeatedRowStatement(int copies)
{
    QByteArray csv;
    csv += "IBAN,";
    csv += kIban;
    csv += "\nBIC,";
    csv += kBic;
    csv += "\nPeriod,From 03/01/2026 to 03/31/2026\n";
    csv += "Account balance on 03/01/2026,500.00 \xE2\x82\xAC\n";
    csv += "Account balance on 03/31/2026," + QByteArray::number(500 - 2 * copies) + ".00 \xE2\x82\xAC\n";
    csv += "Confirmation date/ value,Operation date,Description and identifier,Debit (-),Credit (+),Balance\n";
    for (int i = 0; i < copies; ++i) {
        csv += "03/02,03/02,Coffee machine,-2.00,,\n";
    }
    return csv;
}

} // namespace

class ImportSaveTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void parserReadsTheStatement();
    void firstImportWithoutCategories();
    void categoryWithoutSubcategory();
    void reconciliationAdjustmentRow();
    void categoryOnlyRule();
    void clearedDateOnUpdate();
    void failedSaveWritesNothing();
    void retryAfterFailureStoresEachRowOnce();
    void reimportAddsNothing();
    void overlappingStatementAddsOnlyNewRows();
    void identicalRowsAreTwoTransactions();
    void oneMoreIdenticalRowIsAdded();
    void nullAmountFailsTheSave();

private:
    void parse(SnapshotImportController& controller, const QByteArray& csv);
    [[nodiscard]] int transactionCount() const;
    [[nodiscard]] int checkpointCount() const;
    [[nodiscard]] bool hashLinked() const;
    void failInsertsNamed(const QString& name);
    static void categorizeAll(SnapshotImportController& controller);

    QTemporaryDir m_dir;
    int m_account = -1;
};

void ImportSaveTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    QVERIFY2(Database::instance().open(m_dir.filePath(QStringLiteral("test.db"))),
             qPrintable(Database::instance().lastError()));
}

void ImportSaveTest::init()
{
    Database& db = Database::instance();
    QVERIFY2(db.resetAll(), qPrintable(db.lastError()));
    m_account = db.addAccount(QStringLiteral("Checking"), QStringLiteral("CCP"), QStringLiteral("Day-to-day"),
                              QStringLiteral("EUR"), QStringLiteral(""));
    QVERIFY(m_account >= 0);
    QVERIFY(db.addCategory(QStringLiteral("Groceries"), QStringLiteral("#a6e3a1")) >= 0);
}

void ImportSaveTest::parse(SnapshotImportController& controller, const QByteArray& csv)
{
    const QString path = m_dir.filePath(QStringLiteral("statement.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(csv), qint64(csv.size()));
    file.close();
    controller.parseFile(QUrl::fromLocalFile(path), QString::fromLatin1(kTemplate));
    QVERIFY(controller.hasParsed());
    QVERIFY(!controller.lastParseFailed());
}

int ImportSaveTest::transactionCount() const
{
    return Database::instance().transactionCountForAccount(m_account);
}

int ImportSaveTest::checkpointCount() const
{
    return static_cast<int>(Database::instance().loadBalanceCheckpoints(m_account).size());
}

bool ImportSaveTest::hashLinked() const
{
    const QString hash = AccountHashing::compute(QString::fromLatin1(kIban), QString::fromLatin1(kBic));
    return !hash.isEmpty() && Database::instance().accountHasHash(m_account, hash);
}

// Makes the INSERT of a transaction with this name fail, as a full disk or a
// locked file would. resetAll() in init() drops the table, and its triggers.
void ImportSaveTest::failInsertsNamed(const QString& name)
{
    QSqlQuery query(Database::instance().sqlDatabase());
    QVERIFY2(query.exec(QStringLiteral("CREATE TRIGGER fd_test_fail BEFORE INSERT ON transactions"
                                       " WHEN NEW.name = '%1'"
                                       " BEGIN SELECT RAISE(ABORT, 'forced by the test'); END;").arg(name)),
             qPrintable(query.lastError().text()));
}

// Every row gets a category and a subcategory, so that only the forced
// failure can stop the save.
void ImportSaveTest::categorizeAll(SnapshotImportController& controller)
{
    for (int row = 0; row < controller.transactionRows().size(); ++row) {
        controller.setRowCategory(row, QStringLiteral("Groceries"));
        controller.setRowSubcategory(row, QStringLiteral("Supermarket"));
    }
}

void ImportSaveTest::parserReadsTheStatement()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    const QVariantList rows = controller.transactionRows();
    QCOMPARE(rows.size(), qsizetype(3));
    const QVariantMap first = rows.at(0).toMap();
    QVERIFY(first.value(QStringLiteral("description")).toString().contains(QStringLiteral("CARREFOUR")));
    const QVariantMap reconciliation = controller.checkReconciliation(m_account);
    QVERIFY(reconciliation.value(QStringLiteral("endOk")).toBool());
}

void ImportSaveTest::firstImportWithoutCategories()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    controller.save(m_account, QStringLiteral("Checking"));
    QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
    QCOMPARE(transactionCount(), 3);
}

void ImportSaveTest::categoryWithoutSubcategory()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    for (int row = 0; row < 3; ++row) {
        controller.setRowCategory(row, QStringLiteral("Groceries"));
    }
    controller.save(m_account, QStringLiteral("Checking"));
    QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
    QCOMPARE(transactionCount(), 3);
    for (const Data::Transaction& transaction : Database::instance().transactionsForAccount(m_account)) {
        QCOMPARE(transaction.category, QStringLiteral("Groceries"));
        QVERIFY(transaction.subcategory.isEmpty());
    }
}

void ImportSaveTest::reconciliationAdjustmentRow()
{
    SnapshotImportController controller;
    parse(controller, marchStatement("430.00"));
    QVERIFY(!controller.checkReconciliation(m_account).value(QStringLiteral("endOk")).toBool());
    controller.addReconciliationAdjustment(m_account);
    QCOMPARE(controller.transactionRows().size(), qsizetype(4));
    controller.save(m_account, QStringLiteral("Checking"));
    QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
    QCOMPARE(transactionCount(), 4);
}

void ImportSaveTest::categoryOnlyRule()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    controller.setRowCategory(0, QStringLiteral("Groceries"));
    QVERIFY2(controller.rememberRowAsRule(0), qPrintable(controller.statusText()));
    const QVector<Data::CategoryRule> rules = Database::instance().loadCategoryRules();
    QCOMPARE(rules.size(), qsizetype(1));
    QCOMPARE(rules.first().category, QStringLiteral("Groceries"));
    QVERIFY(rules.first().subcategory.isEmpty());
}

void ImportSaveTest::clearedDateOnUpdate()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 3);

    Data::Transaction edited = Database::instance().transactionsForAccount(m_account).first();
    edited.confirmationDate = QDate();
    QVERIFY2(Database::instance().updateTransaction(edited), qPrintable(Database::instance().lastError()));

    bool found = false;
    for (const Data::Transaction& transaction : Database::instance().transactionsForAccount(m_account)) {
        if (transaction.id == edited.id) {
            found = true;
            QVERIFY(!transaction.confirmationDate.isValid());
        }
    }
    QVERIFY(found);
}

void ImportSaveTest::failedSaveWritesNothing()
{
    failInsertsNamed(QStringLiteral("BOOM"));
    SnapshotImportController controller;
    parse(controller, marchStatement());
    categorizeAll(controller);
    controller.setRowName(1, QStringLiteral("BOOM")); // the second of three rows
    controller.save(m_account, QStringLiteral("Checking"));

    QVERIFY2(controller.statusText().startsWith(QStringLiteral("⚠")), qPrintable(controller.statusText()));
    QCOMPARE(transactionCount(), 0);  // not the first row alone
    QCOMPARE(checkpointCount(), 0);   // no statement balance either
    QVERIFY(!hashLinked());           // and no account link
}

void ImportSaveTest::retryAfterFailureStoresEachRowOnce()
{
    failInsertsNamed(QStringLiteral("BOOM"));
    SnapshotImportController controller;
    parse(controller, marchStatement());
    categorizeAll(controller);
    controller.setRowName(1, QStringLiteral("BOOM"));
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 0);

    // The user corrects the row and saves again.
    controller.setRowName(1, QStringLiteral("SNCF"));
    controller.save(m_account, QStringLiteral("Checking"));
    QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
    QCOMPARE(transactionCount(), 3);
    QCOMPARE(checkpointCount(), 2);   // statement start and end
    QVERIFY(hashLinked());
}

void ImportSaveTest::reimportAddsNothing()
{
    {
        SnapshotImportController controller;
        parse(controller, marchStatement());
        controller.save(m_account, QStringLiteral("Checking"));
        QCOMPARE(transactionCount(), 3);
    }
    SnapshotImportController controller;
    parse(controller, marchStatement());
    QCOMPARE(controller.checkReconciliation(m_account).value(QStringLiteral("alreadySavedCount")).toInt(), 3);
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 3);
    QVERIFY2(controller.statusText().contains(QStringLiteral("already saved")), qPrintable(controller.statusText()));
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("alreadySavedCount")).toInt(), 3);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("transactionCount")).toInt(), 0);
}

void ImportSaveTest::overlappingStatementAddsOnlyNewRows()
{
    {
        SnapshotImportController controller;
        parse(controller, marchStatement());
        controller.save(m_account, QStringLiteral("Checking"));
        QCOMPARE(transactionCount(), 3);
    }
    SnapshotImportController controller;
    parse(controller, overlappingStatement());
    QCOMPARE(controller.transactionRows().size(), qsizetype(3));
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 4);  // 3 from March + the one new row
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("alreadySavedCount")).toInt(), 2);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("transactionCount")).toInt(), 1);
}

void ImportSaveTest::identicalRowsAreTwoTransactions()
{
    {
        SnapshotImportController controller;
        parse(controller, repeatedRowStatement(2));
        QCOMPARE(controller.transactionRows().size(), qsizetype(2));
        QCOMPARE(controller.checkReconciliation(m_account).value(QStringLiteral("alreadySavedCount")).toInt(), 0);
        controller.save(m_account, QStringLiteral("Checking"));
        QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓  Saved 2 of 2")),
                 qPrintable(controller.statusText()));
        QCOMPARE(transactionCount(), 2);
    }
    // A re-import matches each row one to one.
    SnapshotImportController controller;
    parse(controller, repeatedRowStatement(2));
    QCOMPARE(controller.checkReconciliation(m_account).value(QStringLiteral("alreadySavedCount")).toInt(), 2);
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 2);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("alreadySavedCount")).toInt(), 2);
}

void ImportSaveTest::oneMoreIdenticalRowIsAdded()
{
    {
        SnapshotImportController controller;
        parse(controller, repeatedRowStatement(1));
        controller.save(m_account, QStringLiteral("Checking"));
        QCOMPARE(transactionCount(), 1);
    }
    // A later export of the same period has the row twice: one is new.
    SnapshotImportController controller;
    parse(controller, repeatedRowStatement(2));
    QCOMPARE(controller.checkReconciliation(m_account).value(QStringLiteral("alreadySavedCount")).toInt(), 1);
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 2);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("alreadySavedCount")).toInt(), 1);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("transactionCount")).toInt(), 1);
}

void ImportSaveTest::nullAmountFailsTheSave()
{
    // A NaN amount binds as NULL. INSERT OR IGNORE skipped such a row without
    // an error; only a fingerprint conflict may be skipped.
    Data::Transaction row;
    row.operationDate = QDate(2026, 3, 2);
    row.confirmationDate = row.operationDate;
    row.description = QStringLiteral("Card payment: CARREFOUR");
    row.debit = std::numeric_limits<double>::quiet_NaN();
    int inserted = -1;
    QVERIFY(!Database::instance().saveImport(m_account, {row}, QString(), {}, &inserted));
    QCOMPARE(transactionCount(), 0);
}

QTEST_GUILESS_MAIN(ImportSaveTest)
#include "import_save_test.moc"
