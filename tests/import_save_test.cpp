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
//        a later export with other dates and balances matches by bank
//        reference; two identical rows of one statement are two transactions;
//        a row that breaks a NOT NULL column fails the save instead of being
//        skipped
//   #29  the parser reads the statement this file writes
//   #30  a row date gets the year that puts it in the statement period
//   #31  a 29 February row is read
//   #32  grouped and comma amounts are read; "nan", "inf" and garbled
//        grouping are refused
//   #43  a "Prénom Nom" line gives the holder name
//   #44  a start balance that cannot be read leaves the end balance as it is
//   #44  ...and the reconciliation does not count the unread balance as 0
//   #45  a Windows-1252 file is read; a UTF-8 byte order mark is skipped; a
//        UTF-8 file with a few bad bytes stays UTF-8; a UTF-16 file with a
//        byte order mark is read
//   #46  a "ref" in the free text stays in the name; the reference block is
//        the run of reference lines at the end; a name is never empty or null
//   #39  a failed save returns false, says it in red (until the next
//        message), and leaves the report as it was; reset() clears the report
//        of the last save
//   #53  "Auto-filled" counts only the rows whose category the predictor
//        filled
//   #54  a status message has a level, and a new message stays for the whole
//        delay

#include "SnapshotImportController.h"
#include "db/Database.h"
#include "import/SumeriaCcpCsvExtractionTemplate.h"
#include "models/ImportLog.h"
#include "models/ImportedStatement.h"
#include "models/Account.h"
#include "models/CategoryRule.h"
#include "models/Transaction.h"
#include "utils/AccountHashing.h"
#include "utils/StatusMessage.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QStringEncoder>
#include <QTest>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
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

// The March statement as a later export can show it: other confirmation
// dates and other running balances, but the same bank references and amounts.
QByteArray marchStatementLaterExport()
{
    QByteArray csv;
    csv += "IBAN,";
    csv += kIban;
    csv += "\nBIC,";
    csv += kBic;
    csv += "\nPeriod,From 03/01/2026 to 03/31/2026\n";
    csv += "Account balance on 03/01/2026,500.00 \xE2\x82\xAC\n";
    csv += "Account balance on 03/31/2026,440.00 \xE2\x82\xAC\n";
    csv += "Confirmation date/ value,Operation date,Description and identifier,Debit (-),Credit (+),Balance\n";
    csv += "03/03,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,481.00\n";
    csv += "03/06,03/05,\"Card payment 1234: SNCF\nInternal ref.: A2\",-30.00,,451.00\n";
    csv += "03/10,03/09,\"Card payment 1234: BOULANGERIE\nInternal ref.: A3\",-10.00,,441.00\n";
    return csv;
}

// A statement for the parser tests: the IBAN, BIC and `period` lines, the two
// balance lines, the column header, then `rows`.
QByteArray statement(const QByteArray& period, const QByteArray& start, const QByteArray& end,
                     const QByteArray& rows)
{
    QByteArray csv;
    csv += "IBAN,";
    csv += kIban;
    csv += "\nBIC,";
    csv += kBic;
    csv += "\n";
    if (!period.isEmpty()) {
        csv += "Period," + period + "\n";
    }
    csv += "Account balance on " + start + "\n";
    csv += "Account balance on " + end + "\n";
    csv += "Confirmation date/ value,Operation date,Description and identifier,Debit (-),Credit (+),Balance\n";
    csv += rows;
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
    void laterExportMatchesByBankReference();
    void bankReferenceIsTheLastExactLabel();
    void senderTextIsNotTheBankReference();
    void reusedReferenceIsReported();
    void identicalRowsAreTwoTransactions();
    void oneMoreIdenticalRowIsAdded();
    void nullAmountFailsTheSave();
    void leapDayRowIsRead();
    void rowsAcrossNewYearGetTheirYear();
    void decemberRowInJanuaryStatement();
    void missingPeriodFailsTheParse();
    void groupedAndCommaAmountsAreRead();
    void nonFiniteAmountsAreRefused();
    void malformedAmountsAreRefused();
    void longPeriodGetsTheLaterYear();
    void dateOutsideThePeriodIsLogged();
    void holderNameWithAnAccentIsRead();
    void unreadableStartBalanceKeepsTheEndBalance();
    void windows1252FileIsRead();
    void utf8ByteOrderMarkIsSkipped();
    void utf8FileWithABadByteStaysUtf8();
    void utf16FileIsRead();
    void refInTheFreeTextStaysInTheName();
    void nameFallsBackToTheFirstLine();
    void failedSaveSaysItAndKeepsTheReport();
    void saveWithoutAccountIsAWarning();
    void resetClearsTheLastReport();
    void manualPicksAreNotAutoFilled();
    void newStatusMessageStaysForTheWholeDelay();

private:
    // Runs the template alone on `csv`: the statement as it reads it, and its log.
    bool readStatement(const QByteArray& csv, Data::ImportedStatement& out,
                       QVector<Data::ImportLogEntry>& log);
    void parse(SnapshotImportController& controller, const QByteArray& csv);
    // Parses and saves `csv`, then returns the saved rows, oldest first.
    QVector<Data::Transaction> parseAndSave(const QByteArray& csv);
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

QVector<Data::Transaction> ImportSaveTest::parseAndSave(const QByteArray& csv)
{
    SnapshotImportController controller;
    parse(controller, csv);
    controller.save(m_account, QStringLiteral("Checking"));
    QVector<Data::Transaction> rows = Database::instance().transactionsForAccount(m_account);
    std::sort(rows.begin(), rows.end(), [](const Data::Transaction& a, const Data::Transaction& b) {
        return a.id < b.id;
    });
    return rows;
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

void ImportSaveTest::laterExportMatchesByBankReference()
{
    {
        SnapshotImportController controller;
        parse(controller, marchStatement());
        controller.save(m_account, QStringLiteral("Checking"));
        QCOMPARE(transactionCount(), 3);
    }
    // Other dates and balances, same references and amounts: nothing new.
    SnapshotImportController controller;
    parse(controller, marchStatementLaterExport());
    QCOMPARE(controller.checkReconciliation(m_account).value(QStringLiteral("alreadySavedCount")).toInt(), 3);
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 3);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("alreadySavedCount")).toInt(), 3);
}

void ImportSaveTest::bankReferenceIsTheLastExactLabel()
{
    QCOMPARE(TransactionsTable::bankReference(QStringLiteral("Card payment 1234: SHOP Internal ref.: 06c6-0f6b")),
             QStringLiteral("06c6-0f6b"));
    // The sender's text comes first; the bank's block is last.
    QCOMPARE(TransactionsTable::bankReference(
                 QStringLiteral("Transfer Rent Internal ref.: APT12 External ref.: 4861 Internal ref.: C8EE57")),
             QStringLiteral("C8EE57"));
    // Another case, no dot or no colon: not the bank's label.
    QVERIFY(TransactionsTable::bankReference(QStringLiteral("Rent internal ref: APT12")).isEmpty());
    QVERIFY(TransactionsTable::bankReference(QStringLiteral("Rent Internal ref: APT12")).isEmpty());
    // Punctuation after the id is not part of it.
    QCOMPARE(TransactionsTable::bankReference(QStringLiteral("Payment \"Internal ref.: ABC123\", thanks")),
             QStringLiteral("ABC123"));
}

void ImportSaveTest::senderTextIsNotTheBankReference()
{
    // Two rent transfers of the same amount: the sender wrote a label that
    // looks like a reference, the bank's own references differ.
    const QVector<Data::Transaction> rows = parseAndSave(statement(
        "From 03/01/2026 to 03/31/2026", "03/01/2026,2000.00 \xE2\x82\xAC", "03/31/2026,100.00 \xE2\x82\xAC",
        "03/05,03/05,\"Transfer to LANDLORD\nRent internal ref: APT12\nInternal ref.: B1\",-950.00,,1050.00\n"
        "03/06,03/06,\"Transfer to LANDLORD\nRent Internal ref.: APT12\nInternal ref.: B2\",-950.00,,100.00\n"));
    QCOMPARE(rows.size(), qsizetype(2));
}

void ImportSaveTest::reusedReferenceIsReported()
{
    {
        SnapshotImportController controller;
        parse(controller, marchStatement());
        controller.save(m_account, QStringLiteral("Checking"));
        QCOMPARE(transactionCount(), 3);
    }
    // April: a debit with the reference and the amount of a March row (A1,
    // 20.00), on another operation date.
    SnapshotImportController controller;
    parse(controller, statement(
        "From 04/01/2026 to 04/30/2026", "04/01/2026,440.00 \xE2\x82\xAC", "04/30/2026,420.00 \xE2\x82\xAC",
        "04/02,04/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,420.00\n"));
    QCOMPARE(controller.checkReconciliation(m_account).value(QStringLiteral("reusedReferenceCount")).toInt(), 1);
    controller.save(m_account, QStringLiteral("Checking"));
    QCOMPARE(transactionCount(), 3); // skipped, as decided
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("alreadySavedCount")).toInt(), 1);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("reusedReferenceCount")).toInt(), 1);
    QCOMPARE(controller.lastImportReport().value(QStringLiteral("reusedReferenceUrl")).toString(),
             QLatin1String(TransactionsTable::kReusedReferenceIssueUrl));
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

void ImportSaveTest::leapDayRowIsRead()
{
    const QVector<Data::Transaction> rows = parseAndSave(statement(
        "From 02/01/2024 to 02/29/2024", "02/01/2024,100.00 \xE2\x82\xAC", "02/29/2024,90.00 \xE2\x82\xAC",
        "02/29,02/28,\"Card payment 1234: SHOP\nInternal ref.: L1\",-10.00,,90.00\n"));
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).confirmationDate, QDate(2024, 2, 29));
    QCOMPARE(rows.at(0).operationDate, QDate(2024, 2, 28));
}

void ImportSaveTest::rowsAcrossNewYearGetTheirYear()
{
    const QVector<Data::Transaction> rows = parseAndSave(statement(
        "From 12/15/2025 to 01/14/2026", "12/15/2025,100.00 \xE2\x82\xAC", "01/14/2026,80.00 \xE2\x82\xAC",
        "12/20,12/20,\"Card payment 1234: GIFTS\nInternal ref.: Y1\",-10.00,,90.00\n"
        "01/05,01/05,\"Card payment 1234: SALES\nInternal ref.: Y2\",-10.00,,80.00\n"));
    QCOMPARE(rows.size(), qsizetype(2));
    QCOMPARE(rows.at(0).operationDate, QDate(2025, 12, 20));
    QCOMPARE(rows.at(1).operationDate, QDate(2026, 1, 5)); // not 2025-01-05
}

void ImportSaveTest::decemberRowInJanuaryStatement()
{
    // Card payment on 31 December, booked on 2 January.
    const QVector<Data::Transaction> rows = parseAndSave(statement(
        "From 01/01/2026 to 01/31/2026", "01/01/2026,100.00 \xE2\x82\xAC", "01/31/2026,90.00 \xE2\x82\xAC",
        "01/02,12/31,\"Card payment 1234: PARTY\nInternal ref.: D1\",-10.00,,90.00\n"));
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).confirmationDate, QDate(2026, 1, 2));
    QCOMPARE(rows.at(0).operationDate, QDate(2025, 12, 31)); // not 2026-12-31
}

void ImportSaveTest::missingPeriodFailsTheParse()
{
    const QString path = m_dir.filePath(QStringLiteral("no-period.csv"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(statement("", "03/01/2026,500.00 \xE2\x82\xAC", "03/31/2026,480.00 \xE2\x82\xAC",
                         "03/02,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,480.00\n"));
    file.close();
    SnapshotImportController controller;
    controller.parseFile(QUrl::fromLocalFile(path), QString::fromLatin1(kTemplate));
    QVERIFY(controller.lastParseFailed());
    QVERIFY(controller.errorCount() >= 1);
}

void ImportSaveTest::groupedAndCommaAmountsAreRead()
{
    // 2 000.00 (narrow no-break space) − 1 300.00 + 1 234,56 − 1,234 = 700.56
    SnapshotImportController controller;
    parse(controller, statement(
        "From 03/01/2026 to 03/31/2026", "03/01/2026,2\xE2\x80\xAF" "000.00 \xE2\x82\xAC",
        "03/31/2026,700.56 \xE2\x82\xAC",
        "03/02,03/02,\"Transfer: RENT\nInternal ref.: G1\",-1 300.00,,700.00\n"
        "03/03,03/03,\"Payment received: SALARY\nInternal ref.: G2\",,\"1 234,56 \xE2\x82\xAC\",1934.56\n"
        "03/04,03/04,\"Card payment 1234: LAPTOP\nInternal ref.: G3\",\"-1,234\",,700.56\n"));
    QCOMPARE(controller.transactionRows().size(), qsizetype(3));
    QVERIFY(controller.checkReconciliation(m_account).value(QStringLiteral("endOk")).toBool());
    controller.save(m_account, QStringLiteral("Checking"));
    QVector<Data::Transaction> rows = Database::instance().transactionsForAccount(m_account);
    std::sort(rows.begin(), rows.end(), [](const Data::Transaction& a, const Data::Transaction& b) {
        return a.id < b.id;
    });
    QCOMPARE(rows.size(), qsizetype(3));
    QCOMPARE(rows.at(0).debit, 1300.0);
    QCOMPARE(rows.at(1).credit, 1234.56);
    QCOMPARE(rows.at(2).debit, 1234.0);
}

void ImportSaveTest::nonFiniteAmountsAreRefused()
{
    SnapshotImportController controller;
    parse(controller, statement(
        "From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC", "03/31/2026,480.00 \xE2\x82\xAC",
        "03/02,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,480.00\n"
        "03/03,03/03,\"Card payment 1234: ODD\nInternal ref.: N1\",nan,,480.00\n"
        "03/04,03/04,\"Card payment 1234: ODDER\nInternal ref.: N2\",-inf,,480.00\n"));
    QCOMPARE(controller.transactionRows().size(), qsizetype(1)); // both rows skipped
    QVERIFY(controller.warningCount() >= 2);
}

void ImportSaveTest::malformedAmountsAreRefused()
{
    SnapshotImportController controller;
    parse(controller, statement(
        "From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC", "03/31/2026,480.00 \xE2\x82\xAC",
        "03/02,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,480.00\n"
        "03/03,03/03,\"Card payment 1234: ODD\nInternal ref.: N1\",-1.2.3,,480.00\n"
        "03/04,03/04,\"Card payment 1234: ODDER\nInternal ref.: N2\",\"-1,,5\",,480.00\n"
        "03/05,03/05,\"Card payment 1234: ODDEST\nInternal ref.: N3\",\"-0,123\",,480.00\n"));
    QCOMPARE(controller.transactionRows().size(), qsizetype(1)); // the three garbled rows are skipped
    QVERIFY(controller.warningCount() >= 3);
}

void ImportSaveTest::longPeriodGetsTheLaterYear()
{
    // A whole-year statement: 12/15 can be 2024-12-15 (in the 31 days before
    // the start) or 2025-12-15 (in the period). The period wins, with a
    // warning about the long period.
    SnapshotImportController controller;
    parse(controller, statement(
        "From 01/01/2025 to 12/31/2025", "01/01/2025,100.00 \xE2\x82\xAC", "12/31/2025,90.00 \xE2\x82\xAC",
        "12/15,12/15,\"Card payment 1234: GIFTS\nInternal ref.: W1\",-10.00,,90.00\n"));
    bool warned = false;
    for (const QVariant& entry : controller.logEntries()) {
        warned = warned || entry.toMap().value(QStringLiteral("message")).toString().contains(QStringLiteral("11 months"));
    }
    QVERIFY(warned);
    controller.save(m_account, QStringLiteral("Checking"));
    const QVector<Data::Transaction> rows = Database::instance().transactionsForAccount(m_account);
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).operationDate, QDate(2025, 12, 15)); // not 2024-12-15
}

void ImportSaveTest::dateOutsideThePeriodIsLogged()
{
    // March statement: the operation date 01/20 is more than 31 days before
    // the start, so the confirmation date is used, with a warning.
    SnapshotImportController controller;
    parse(controller, statement(
        "From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC", "03/31/2026,480.00 \xE2\x82\xAC",
        "03/02,01/20,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,480.00\n"));
    bool warned = false;
    for (const QVariant& entry : controller.logEntries()) {
        warned = warned || entry.toMap().value(QStringLiteral("message")).toString().startsWith(
                               QStringLiteral("The operation date of this row"));
    }
    QVERIFY(warned);
    controller.save(m_account, QStringLiteral("Checking"));
    const QVector<Data::Transaction> rows = Database::instance().transactionsForAccount(m_account);
    QCOMPARE(rows.size(), qsizetype(1));
    QCOMPARE(rows.at(0).operationDate, QDate(2026, 3, 2));
}

bool ImportSaveTest::readStatement(const QByteArray& csv, Data::ImportedStatement& out,
                                   QVector<Data::ImportLogEntry>& log)
{
    const QString path = m_dir.filePath(QStringLiteral("template.csv"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(csv) != csv.size()) {
        return false;
    }
    file.close();
    return SumeriaCcpCsvExtractionTemplate().parse(path, out, log);
}

void ImportSaveTest::holderNameWithAnAccentIsRead()
{
    Data::ImportedStatement st;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement("Pr\xC3\xA9nom Nom,H\xC3\xA9l\xC3\xA8ne Martin\n" + marchStatement(), st, log));
    QCOMPARE(st.accountHolderName, QStringLiteral("Hélène Martin"));
}

void ImportSaveTest::unreadableStartBalanceKeepsTheEndBalance()
{
    Data::ImportedStatement st;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement(statement("From 03/01/2026 to 03/31/2026", "03/01/2026,five hundred",
                                    "03/31/2026,440.00 \xE2\x82\xAC",
                                    "03/02,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,480.00\n"),
                          st, log));
    QVERIFY(!st.balanceStartDate.isValid());
    QCOMPARE(st.balanceStart, 0.0);
    QCOMPARE(st.balanceEndDate, QDate(2026, 3, 31));
    QCOMPARE(st.balanceEnd, 440.0);
    const bool warned = std::any_of(log.cbegin(), log.cend(), [](const Data::ImportLogEntry& entry) {
        return entry.level == Data::LogLevel::Warning && entry.message.contains(QStringLiteral("start balance"));
    });
    QVERIFY(warned);

    // The reconciliation does not use 0 for the unread start: before the fix,
    // it found a gap of 500 and offered an adjustment of +500.
    SnapshotImportController controller;
    parse(controller, statement("From 03/01/2026 to 03/31/2026", "03/01/2026,five hundred",
                                "03/31/2026,440.00 \xE2\x82\xAC",
                                "03/02,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-60.00,,440.00\n"));
    const QVariantMap check = controller.checkReconciliation(m_account);
    QVERIFY(!check.value(QStringLiteral("hasDeclaredStart")).toBool());
    QVERIFY(!check.value(QStringLiteral("endAvailable")).toBool());
    QVERIFY(check.value(QStringLiteral("endOk")).toBool());
    controller.addReconciliationAdjustment(m_account);
    QCOMPARE(controller.transactionRows().size(), qsizetype(1));   // no adjustment row
    QCOMPARE(controller.statementInfo().value(QStringLiteral("balanceStartText")).toString(), QStringLiteral("—"));
}

void ImportSaveTest::windows1252FileIsRead()
{
    // "é" is 0xE9 and "€" is 0x80 in Windows-1252: neither is valid UTF-8.
    QByteArray csv = "Pr\xE9nom Nom,H\xE9l\xE8ne Martin\n" + marchStatement();
    csv.replace("\xE2\x82\xAC", "\x80");
    Data::ImportedStatement st;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement(csv, st, log));
    QCOMPARE(st.accountHolderName, QStringLiteral("Hélène Martin"));
    QCOMPARE(st.balanceStart, 500.0);
    QCOMPARE(st.balanceEnd, 440.0);
    QCOMPARE(st.transactions.size(), qsizetype(3));
    const bool told = std::any_of(log.cbegin(), log.cend(), [](const Data::ImportLogEntry& entry) {
        return entry.message.contains(QStringLiteral("Windows-1252"));
    });
    QVERIFY(told);
}

void ImportSaveTest::utf8ByteOrderMarkIsSkipped()
{
    Data::ImportedStatement st;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement("\xEF\xBB\xBF" + marchStatement(), st, log));
    QCOMPARE(st.iban, QString::fromLatin1(kIban));   // the first label is still "IBAN"
    QCOMPARE(st.balanceEnd, 440.0);
    const bool told = std::any_of(log.cbegin(), log.cend(), [](const Data::ImportLogEntry& entry) {
        return entry.message.contains(QStringLiteral("Windows-1252"));
    });
    QVERIFY(!told);
}

void ImportSaveTest::utf8FileWithABadByteStaysUtf8()
{
    // One stray Windows-1252 byte in a UTF-8 file: the file stays UTF-8 (the
    // "€" of the balances are valid UTF-8), with or without a byte order mark.
    for (const QByteArray& bom : {QByteArray(), QByteArray("\xEF\xBB\xBF")}) {
        Data::ImportedStatement st;
        QVector<Data::ImportLogEntry> log;
        QVERIFY(readStatement(bom + "Pr\xC3\xA9nom Nom,Ren\xE9 Martin\n" + marchStatement(), st, log));
        QCOMPARE(st.iban, QString::fromLatin1(kIban));
        QCOMPARE(st.balanceStart, 500.0);
        QCOMPARE(st.balanceEnd, 440.0);
        QCOMPARE(st.accountHolderName, QStringLiteral("Ren� Martin"));
        const bool told = std::any_of(log.cbegin(), log.cend(), [](const Data::ImportLogEntry& entry) {
            return entry.level == Data::LogLevel::Warning && entry.message.contains(QStringLiteral("not valid UTF-8"));
        });
        QVERIFY(told);
    }
}

void ImportSaveTest::utf16FileIsRead()
{
    QStringEncoder utf16(QStringConverter::Utf16LE);
    const QByteArray csv = QByteArray("\xFF\xFE") + QByteArray(utf16.encode(QString::fromUtf8(marchStatement())));
    Data::ImportedStatement st;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement(csv, st, log));
    QCOMPARE(st.iban, QString::fromLatin1(kIban));
    QCOMPARE(st.balanceEnd, 440.0);
    QCOMPARE(st.transactions.size(), qsizetype(3));
}

void ImportSaveTest::refInTheFreeTextStaysInTheName()
{
    Data::ImportedStatement st;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement(statement("From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC",
                                    "03/31/2026,0.00 \xE2\x82\xAC",
                                    "03/02,03/02,\"Rent for March, ref: APT12\nInternal ref.: R1\",-500.00,,0.00\n"),
                          st, log));
    QCOMPARE(st.transactions.size(), qsizetype(1));
    QCOMPARE(st.transactions.at(0).name, QStringLiteral("Rent for March, ref: APT12"));

    // The reference block is the run of reference lines at the end: a "ref:"
    // line followed by free text stays, and a label of several words ends
    // the name too.
    Data::ImportedStatement block;
    QVERIFY(readStatement(statement("From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC",
                                    "03/31/2026,0.00 \xE2\x82\xAC",
                                    "03/02,03/02,\"Transfer ref: rent March\nMarch 2026\nEnd-to-end ref.: E2E1\n"
                                    "Internal ref.: R2\",-500.00,,0.00\n"),
                          block, log));
    QCOMPARE(block.transactions.size(), qsizetype(1));
    QCOMPARE(block.transactions.at(0).name, QStringLiteral("Transfer ref: rent March March 2026"));
}

void ImportSaveTest::nameFallsBackToTheFirstLine()
{
    Data::ImportedStatement st;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement(statement("From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC",
                                    "03/31/2026,470.00 \xE2\x82\xAC",
                                    "03/02,03/02,\"Transfer ref: rent March\nInternal ref.: F1\",-20.00,,480.00\n"
                                    "03/03,03/03,\"Internal ref.: F2\",-10.00,,470.00\n"),
                          st, log));
    QCOMPARE(st.transactions.size(), qsizetype(2));
    QCOMPARE(st.transactions.at(0).name, QStringLiteral("Transfer ref: rent March"));
    QCOMPARE(st.transactions.at(1).name, QStringLiteral("Internal ref.: F2"));

    // An empty description gives an empty name, not a null one.
    Data::ImportedStatement empty;
    QVERIFY(readStatement(statement("From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC",
                                    "03/31/2026,495.00 \xE2\x82\xAC", "03/04,03/04,,-5.00,,495.00\n"),
                          empty, log));
    QCOMPARE(empty.transactions.size(), qsizetype(1));
    QVERIFY(empty.transactions.at(0).name.isEmpty());
    QVERIFY(!empty.transactions.at(0).name.isNull());

    // The save stores the reference-only row and the empty one with their
    // names (#16: a null name fails it).
    SnapshotImportController controller;
    parse(controller, statement("From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC",
                                "03/31/2026,485.00 \xE2\x82\xAC",
                                "03/03,03/03,\"Internal ref.: F2\",-10.00,,490.00\n"
                                "03/04,03/04,,-5.00,,485.00\n"));
    controller.save(m_account, QStringLiteral("Checking"));
    QVector<Data::Transaction> rows = Database::instance().transactionsForAccount(m_account);
    std::sort(rows.begin(), rows.end(), [](const Data::Transaction& a, const Data::Transaction& b) {
        return a.id < b.id;
    });
    QCOMPARE(rows.size(), qsizetype(2));
    QCOMPARE(rows.at(0).name, QStringLiteral("Internal ref.: F2"));
    QCOMPARE(rows.at(1).name, QStringLiteral(""));
}

void ImportSaveTest::failedSaveSaysItAndKeepsTheReport()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    QVERIFY(controller.save(m_account, QStringLiteral("Checking")));
    const QVariantMap report = controller.lastImportReport();
    QVERIFY(!report.isEmpty());

    // A second statement on the same controller, whose save fails: the report
    // stays the one of the save that worked.
    failInsertsNamed(QStringLiteral("BOOM"));
    parse(controller, overlappingStatement());
    controller.setRowName(2, QStringLiteral("BOOM"));   // the one new row
    QVERIFY(!controller.save(m_account, QStringLiteral("Checking")));
    QCOMPARE(controller.statusLevel(), QStringLiteral("danger"));
    QCOMPARE(controller.lastImportReport(), report);
}

void ImportSaveTest::saveWithoutAccountIsAWarning()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    QVERIFY(!controller.save(-1, QStringLiteral("(no account selected)")));
    QCOMPARE(controller.statusLevel(), QStringLiteral("warning"));
    QCOMPARE(transactionCount(), 0);
}

void ImportSaveTest::resetClearsTheLastReport()
{
    SnapshotImportController controller;
    parse(controller, marchStatement());
    QVERIFY(controller.save(m_account, QStringLiteral("Checking")));
    QCOMPARE(controller.statusLevel(), QStringLiteral("success"));
    QVERIFY(!controller.lastImportReport().isEmpty());

    // "Import another file", then a save that fails: no report of the first import.
    controller.reset();
    QVERIFY(controller.lastImportReport().isEmpty());
    failInsertsNamed(QStringLiteral("BOOM"));
    parse(controller, overlappingStatement());
    controller.setRowName(2, QStringLiteral("BOOM"));
    QVERIFY(!controller.save(m_account, QStringLiteral("Checking")));
    QVERIFY(controller.lastImportReport().isEmpty());
}

void ImportSaveTest::manualPicksAreNotAutoFilled()
{
    {
        // An empty database: nothing to learn from, so no prediction.
        SnapshotImportController controller;
        parse(controller, marchStatement());
        QCOMPARE(controller.predictionInfo().value(QStringLiteral("filledCount")).toInt(), 0);
        categorizeAll(controller);   // three picks by hand
        QCOMPARE(controller.predictionInfo().value(QStringLiteral("filledCount")).toInt(), 0);
        QVERIFY(controller.save(m_account, QStringLiteral("Checking")));
    }

    // The same merchants again: the predictor fills the three rows.
    SnapshotImportController controller;
    parse(controller, marchStatementLaterExport());
    QCOMPARE(controller.predictionInfo().value(QStringLiteral("filledCount")).toInt(), 3);
    // A subcategory picked by hand keeps the predicted category: still counted.
    controller.setRowSubcategory(0, QStringLiteral("Market"));
    QCOMPARE(controller.predictionInfo().value(QStringLiteral("filledCount")).toInt(), 3);
    // A category picked by hand (another one than the predicted Groceries) is
    // not auto-filled any more.
    controller.setRowCategory(1, QStringLiteral("Transport"));
    QCOMPARE(controller.predictionInfo().value(QStringLiteral("filledCount")).toInt(), 2);
}

void ImportSaveTest::newStatusMessageStaysForTheWholeDelay()
{
    QObject owner;
    int changes = 0;
    StatusMessage status(&owner, 1000, [&changes]() { ++changes; });
    status.show(StatusMessage::Level::Warning, QStringLiteral("first"));
    QCOMPARE(status.variant(), QStringLiteral("warning"));
    QTest::qWait(600);
    status.show(StatusMessage::Level::Success, QStringLiteral("second"));
    QCOMPARE(status.variant(), QStringLiteral("success"));
    // 1200 ms after the first message, 600 ms after the second one: the
    // timer of the first message must not clear the second one.
    QTest::qWait(600);
    QCOMPARE(status.text(), QStringLiteral("second"));
    QTRY_COMPARE_WITH_TIMEOUT(status.text(), QString(), 3000);
    QCOMPARE(changes, 3);   // two messages, then one clear

    // A failure stays until the next message (#39).
    status.show(StatusMessage::Level::Danger, QStringLiteral("failure"));
    QCOMPARE(status.variant(), QStringLiteral("danger"));
    QTest::qWait(1500);
    QCOMPARE(status.text(), QStringLiteral("failure"));
    status.show(StatusMessage::Level::Success, QStringLiteral("next"));
    QCOMPARE(status.text(), QStringLiteral("next"));
}

QTEST_GUILESS_MAIN(ImportSaveTest)
#include "import_save_test.moc"
