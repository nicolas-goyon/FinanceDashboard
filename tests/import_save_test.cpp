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
//   #133 the start check takes the last saved row in booking order (the
//        confirmation date), not in operation-date order
//   #33  the reconciliation compares whole cents: a gap of one cent is a
//        gap, and the adjustment row is that cent; an amount larger than
//        Money::kMaxAmount is refused
//   #47  a hash typed in upper case matches, and a save adds no second
//        link; a save to another account moves the hash, and the next
//        import suggests that account; a failed save leaves the hash where
//        it was
//   #52  the predictor trains and scores in a worker thread: the rows show at
//        once, a save waits, a category picked or a name changed during the
//        job stays the choice of the user, and the result of an older job
//        (another file, reset()) counts for nothing
//   #50  an 8-character BIC and the same BIC on 11 characters give one hash;
//        an account linked with the hash from before #50 is found, and
//        the save replaces that hash; the new hash is tried first; the old
//        hash of another account is shown, replaced by a save or by "Link
//        this account", and kept when the save fails

#include "SnapshotImportController.h"
#include "db/Database.h"
#include "import/SumeriaCcpCsvExtractionTemplate.h"
#include "models/ImportLog.h"
#include "models/ImportedStatement.h"
#include "models/Account.h"
#include "models/CategoryRule.h"
#include "models/Transaction.h"
#include "utils/AccountHashing.h"
#include "utils/Formatter.h"
#include "utils/StatusMessage.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QFile>
#include <QFuture>
#include <QMessageAuthenticationCode>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QStringEncoder>
#include <QSignalSpy>
#include <QTest>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <limits>
#include <optional>

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

// An April statement with one new merchant, twice: no row of the history
// has its label.
QByteArray newMerchantTwiceStatement()
{
    QByteArray csv;
    csv += "IBAN,";
    csv += kIban;
    csv += "\nBIC,";
    csv += kBic;
    csv += "\nPeriod,From 04/01/2026 to 04/30/2026\n";
    csv += "Account balance on 04/01/2026,440.00 \xE2\x82\xAC\n";
    csv += "Account balance on 04/30/2026,410.00 \xE2\x82\xAC\n";
    csv += "Confirmation date/ value,Operation date,Description and identifier,Debit (-),Credit (+),Balance\n";
    csv += "04/02,04/02,\"Card payment 1234: WIDGET EMPORIUM\nInternal ref.: B1\",-10.00,,430.00\n";
    csv += "04/09,04/09,\"Card payment 1234: WIDGET EMPORIUM\nInternal ref.: B2\",-20.00,,410.00\n";
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
    void oneCentGapIsAGap();
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
    void tooLargeAmountIsRefused();
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
    void startCheckUsesTheBookingOrder();
    void pickedAccountTakesTheHash();
    void failedSaveKeepsTheHashWhereItWas();
    void bicOnEightOrElevenCharactersGivesOneHash();
    void oldHashOnAnotherAccountIsReplaced();
    void predictionsRunInAWorkerThread();
    void olderPredictionJobCountsForNothing();
    void nameGivenBeforeTheSuggestionsIsScored();
    void newJobCancelsTheOlderOne();
    void cancelledJobGivesNoResult();

private:
    // Runs the template alone on `csv`: the statement as it reads it, and its log.
    bool readStatement(const QByteArray& csv, Data::ImportedStatement& out,
                       QVector<Data::ImportLogEntry>& log);
    // Parses `csv`, then waits for the suggestions of the worker thread (#52).
    void parse(SnapshotImportController& controller, const QByteArray& csv);
    // The same, without the wait.
    void parseWithoutWaiting(SnapshotImportController& controller, const QByteArray& csv);
    // Every job of the worker thread ends, and its signals reach the
    // controller.
    static void waitForTheWorker();
    [[nodiscard]] static QString rowCategory(const SnapshotImportController& controller, int row);
    // A first import of the March statement, with another category for each
    // merchant: Groceries, Transport, Food.
    void saveMarchWithThreeCategories();
    // Parses and saves `csv`, then returns the saved rows, oldest first.
    QVector<Data::Transaction> parseAndSave(const QByteArray& csv);
    [[nodiscard]] int transactionCount() const;
    [[nodiscard]] int checkpointCount() const;
    [[nodiscard]] bool hashLinked() const;
    // The accounts as AccountController gives them to QML (for
    // matchAccountIndex()): their ids and hashes.
    [[nodiscard]] static QVariantList accountsForMatch();
    [[nodiscard]] static int indexOfAccount(const QVariantList& accounts, int accountId);
    [[nodiscard]] static int hashRowCount();
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
    parseWithoutWaiting(controller, csv);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.predicting(), 10000);
}

void ImportSaveTest::parseWithoutWaiting(SnapshotImportController& controller, const QByteArray& csv)
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

void ImportSaveTest::waitForTheWorker()
{
    QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
    QCoreApplication::processEvents();
}

QString ImportSaveTest::rowCategory(const SnapshotImportController& controller, int row)
{
    return controller.transactionRows().at(row).toMap().value(QStringLiteral("category")).toString();
}

void ImportSaveTest::saveMarchWithThreeCategories()
{
    QVERIFY(Database::instance().addCategory(QStringLiteral("Transport"), QStringLiteral("#89b4fa")) >= 0);
    QVERIFY(Database::instance().addCategory(QStringLiteral("Food"), QStringLiteral("#f9e2af")) >= 0);
    SnapshotImportController controller;
    parse(controller, marchStatement());
    controller.setRowCategory(0, QStringLiteral("Groceries")); // CARREFOUR
    controller.setRowCategory(1, QStringLiteral("Transport")); // SNCF
    controller.setRowCategory(2, QStringLiteral("Food"));      // BOULANGERIE
    QVERIFY(controller.save(m_account, QStringLiteral("Checking")));
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

QVariantList ImportSaveTest::accountsForMatch()
{
    QVariantList accounts;
    for (const Data::Account& account : Database::instance().loadAccounts()) {
        QVariantList hashes;
        for (const Data::AccountHash& hash : account.hashes) {
            hashes.append(QVariantMap{{QStringLiteral("hash"), hash.hash}});
        }
        accounts.append(QVariantMap{{QStringLiteral("id"), account.id}, {QStringLiteral("hashes"), hashes}});
    }
    return accounts;
}

int ImportSaveTest::indexOfAccount(const QVariantList& accounts, int accountId)
{
    for (int i = 0; i < accounts.size(); ++i) {
        if (accounts.at(i).toMap().value(QStringLiteral("id")).toInt() == accountId) {
            return i;
        }
    }
    return -1;
}

int ImportSaveTest::hashRowCount()
{
    QSqlQuery query(Database::instance().sqlDatabase());
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM account_hashes;")) && query.next() ? query.value(0).toInt()
                                                                                             : -1;
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

void ImportSaveTest::oneCentGapIsAGap()
{
    // The example of #33: 0.00 at the start, 0.10 and 0.20 in, 0.31 declared
    // at the end. In doubles, the gap is 0.00999999999999995, under the old
    // tolerance of 0.01: "End balance OK".
    const QByteArray rows =
        "03/02,03/02,\"Transfer from: ALICE\nInternal ref.: C1\",,0.10,0.10\n"
        "03/03,03/03,\"Transfer from: BOB\nInternal ref.: C2\",,0.20,0.30\n";
    {
        SnapshotImportController controller;
        parse(controller, statement("From 03/01/2026 to 03/31/2026", "03/01/2026,0.00 \xE2\x82\xAC",
                                    "03/31/2026,0.31 \xE2\x82\xAC", rows));
        const QVariantMap check = controller.checkReconciliation(m_account);
        QVERIFY(!check.value(QStringLiteral("endOk")).toBool());
        QCOMPARE(check.value(QStringLiteral("endDiffCents")).toLongLong(), qint64(1));
        QCOMPARE(check.value(QStringLiteral("endDiffText")).toString(), Fmt::money(0.01, true));
        QCOMPARE(check.value(QStringLiteral("endComputedText")).toString(), Fmt::money(0.30));

        // The adjustment row is that cent, and closes the gap.
        controller.addReconciliationAdjustment(m_account);
        QVERIFY(controller.checkReconciliation(m_account).value(QStringLiteral("endOk")).toBool());
        controller.save(m_account, QStringLiteral("Checking"));
        QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
        const QVector<Data::Transaction> saved = Database::instance().transactionsForAccount(m_account);
        QCOMPARE(saved.size(), qsizetype(3));
        const auto adjustment = std::find_if(saved.cbegin(), saved.cend(), [](const Data::Transaction& tx) {
            return tx.category == QStringLiteral("Reconciliation Adjustment");
        });
        QVERIFY(adjustment != saved.cend());
        QVERIFY(adjustment->credit == 0.01); // exact: the row is rounded to the cent
        QVERIFY(adjustment->debit == 0.0);
    }

    // The same rows with 0.30 at the end: no gap.
    SnapshotImportController controller;
    parse(controller, statement("From 03/01/2026 to 03/31/2026", "03/01/2026,0.00 \xE2\x82\xAC",
                                "03/31/2026,0.30 \xE2\x82\xAC", rows));
    const QVariantMap check = controller.checkReconciliation(m_account);
    QVERIFY(check.value(QStringLiteral("endOk")).toBool());
    QCOMPARE(check.value(QStringLiteral("endDiffCents")).toLongLong(), qint64(0));
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
    // Since #33, Database refuses a NaN amount before any write.
    Data::Transaction row;
    row.operationDate = QDate(2026, 3, 2);
    row.confirmationDate = row.operationDate;
    row.description = QStringLiteral("Card payment: CARREFOUR");
    row.debit = std::numeric_limits<double>::quiet_NaN();
    int inserted = -1;
    QVERIFY(!Database::instance().saveImport(m_account, {row}, QString(), {}, &inserted));
    QCOMPARE(transactionCount(), 0);
    // The table refuses it too: a NaN amount binds as NULL, and INSERT OR
    // IGNORE skipped such a row without an error; only a fingerprint
    // conflict may be skipped (#18).
    QVERIFY(!Database::instance().transactionsTable().insertBatch(m_account, {row}, &inserted));
    QCOMPARE(inserted, 0);
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

void ImportSaveTest::tooLargeAmountIsRefused()
{
    // Larger than Money::kMaxAmount (10^13): the app cannot store its cents.
    SnapshotImportController controller;
    parse(controller, statement(
        "From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC", "03/31/2026,480.00 \xE2\x82\xAC",
        "03/02,03/02,\"Card payment 1234: CARREFOUR\nInternal ref.: A1\",-20.00,,480.00\n"
        "03/03,03/03,\"Card payment 1234: HUGE\nInternal ref.: N1\",-20000000000000.00,,480.00\n"
        "03/04,03/04,\"Card payment 1234: LIMIT\nInternal ref.: N2\",,10000000000000.00,480.00\n"));
    // The row at the limit stays; the larger one is skipped, and the log
    // tells why.
    QCOMPARE(controller.transactionRows().size(), qsizetype(2));
    Data::ImportedStatement statementRead;
    QVector<Data::ImportLogEntry> log;
    QVERIFY(readStatement(statement("From 03/01/2026 to 03/31/2026", "03/01/2026,500.00 \xE2\x82\xAC",
                                    "03/31/2026,480.00 \xE2\x82\xAC",
                                    "03/03,03/03,\"Card payment 1234: HUGE\nInternal ref.: N1\",-20000000000000.00,,480.00\n"),
                          statementRead, log));
    const bool told = std::any_of(log.cbegin(), log.cend(), [](const Data::ImportLogEntry& entry) {
        return entry.message.contains(QStringLiteral("larger than 10^13"));
    });
    QVERIFY(told);
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

void ImportSaveTest::startCheckUsesTheBookingOrder()
{
    // The account has a row booked on 02/20 (balance 1 000) and a card
    // payment made on 02/27 but booked on 03/02 (balance 850).
    Data::Transaction salary;
    salary.operationDate = QDate(2026, 2, 20);
    salary.confirmationDate = QDate(2026, 2, 20);
    salary.name = salary.description = QStringLiteral("Salary");
    salary.credit = 1000.0;
    salary.balanceAfter = 1000.0;
    Data::Transaction card;
    card.operationDate = QDate(2026, 2, 27);
    card.confirmationDate = QDate(2026, 3, 2);
    card.name = card.description = QStringLiteral("Card payment");
    card.debit = 150.0;
    card.balanceAfter = 850.0;
    QVERIFY(Database::instance().insertTransactions(m_account, {salary, card}));

    // A statement that starts on 02/28 with 1 000: the card payment is not
    // booked yet. Before the fix, check 1 took the card payment (the latest
    // operation date), expected 850 and showed a gap of +150.
    SnapshotImportController controller;
    parse(controller, statement("From 02/28/2026 to 03/31/2026", "02/28/2026,1000.00 \xE2\x82\xAC",
                                "03/31/2026,1010.00 \xE2\x82\xAC",
                                "03/05,03/05,\"Transfer: GIFT\nInternal ref.: B1\",,10.00,1010.00\n"));
    const QVariantMap check = controller.checkReconciliation(m_account);
    QVERIFY(check.value(QStringLiteral("hasStartReference")).toBool());
    QVERIFY2(check.value(QStringLiteral("startOk")).toBool(),
             qPrintable(check.value(QStringLiteral("startDiffText")).toString()));
    QCOMPARE(check.value(QStringLiteral("startSourceDateText")).toString(), Fmt::dateOrDash(QDate(2026, 2, 20)));

    // A statement that starts after the booking of the card payment: that row.
    SnapshotImportController later;
    parse(later, statement("From 03/03/2026 to 03/31/2026", "03/03/2026,850.00 \xE2\x82\xAC",
                           "03/31/2026,860.00 \xE2\x82\xAC",
                           "03/05,03/05,\"Transfer: GIFT\nInternal ref.: B1\",,10.00,860.00\n"));
    const QVariantMap laterCheck = later.checkReconciliation(m_account);
    QVERIFY(laterCheck.value(QStringLiteral("startOk")).toBool());
    QCOMPARE(laterCheck.value(QStringLiteral("startSourceDateText")).toString(), Fmt::dateOrDash(QDate(2026, 3, 2)));
}

void ImportSaveTest::pickedAccountTakesTheHash()
{
    Database& db = Database::instance();
    const int savings = db.addAccount(QStringLiteral("Savings"), QStringLiteral("CCP"), QStringLiteral(""),
                                      QStringLiteral("EUR"), QStringLiteral(""));
    QVERIFY(savings >= 0);
    // The hash of the statement, typed in upper case on Checking. Before
    // #47, the save did not find it and added a second row.
    const QString hash = AccountHashing::compute(QString::fromLatin1(kIban), QString::fromLatin1(kBic));
    QVERIFY2(db.addAccountHash(m_account, hash.toUpper(), QStringLiteral("manual")) >= 0, qPrintable(db.lastError()));
    {
        SnapshotImportController controller;
        parse(controller, marchStatement());
        const QVariantList accounts = accountsForMatch();
        QCOMPARE(controller.matchAccountIndex(accounts), indexOfAccount(accounts, m_account));
        controller.save(m_account, QStringLiteral("Checking"));
        QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
    }
    QCOMPARE(hashRowCount(), 1);

    // The user picks Savings for the next statement: the hash moves.
    {
        SnapshotImportController controller;
        parse(controller, overlappingStatement());
        controller.save(savings, QStringLiteral("Savings"));
        QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
    }
    QCOMPARE(hashRowCount(), 1);
    QCOMPARE(db.accountOfHash(hash), savings);
    QVERIFY(!db.accountHasHash(m_account, hash));

    // The next import suggests Savings.
    SnapshotImportController controller;
    parse(controller, marchStatement());
    const QVariantList accounts = accountsForMatch();
    QCOMPARE(controller.matchAccountIndex(accounts), indexOfAccount(accounts, savings));
}

void ImportSaveTest::failedSaveKeepsTheHashWhereItWas()
{
    // The move of the hash is in the SQL transaction of the save.
    Database& db = Database::instance();
    const int savings = db.addAccount(QStringLiteral("Savings"), QStringLiteral("CCP"), QStringLiteral(""),
                                      QStringLiteral("EUR"), QStringLiteral(""));
    QVERIFY(savings >= 0);
    const QString hash = AccountHashing::compute(QString::fromLatin1(kIban), QString::fromLatin1(kBic));
    QVERIFY2(db.linkAccountHash(m_account, hash, QStringLiteral("auto")), qPrintable(db.lastError()));

    SnapshotImportController controller;
    parse(controller, marchStatement());
    // The last writes of the save (the balance checkpoints) fail.
    QSqlQuery query(db.sqlDatabase());
    QVERIFY2(query.exec(QStringLiteral("CREATE TRIGGER fd_test_fail_checkpoint BEFORE INSERT ON account_balance_checkpoints"
                                       " BEGIN SELECT RAISE(ABORT, 'forced by the test'); END;")),
             qPrintable(query.lastError().text()));
    query.finish();
    QVERIFY(!controller.save(savings, QStringLiteral("Savings")));
    QCOMPARE(db.accountOfHash(hash), m_account);
    QCOMPARE(db.transactionCountForAccount(savings), 0);
    QCOMPARE(hashRowCount(), 1);
}

void ImportSaveTest::bicOnEightOrElevenCharactersGivesOneHash()
{
    const QString iban = QString::fromLatin1(kIban);
    // The two forms of the BIC of a primary office give one hash.
    const QString hash = AccountHashing::compute(iban, QStringLiteral("PSSTFRPP"));
    QCOMPARE(AccountHashing::compute(iban, QStringLiteral("PSSTFRPPXXX")), hash);
    QCOMPARE(AccountHashing::compute(iban, QStringLiteral(" psst frppxxx ")), hash);
    // Another branch is another key, and has no form from before #50.
    QVERIFY(AccountHashing::compute(iban, QStringLiteral("PSSTFRPPLIL")) != hash);
    QVERIFY(AccountHashing::legacyCompute(iban, QStringLiteral("PSSTFRPPLIL")).isEmpty());
    QVERIFY(AccountHashing::legacyCompute(QString(), QStringLiteral("PSSTFRPP")).isEmpty());
    // Before #50, the key was the normalized BIC, without "XXX".
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(QByteArrayLiteral("PSSTFRPP"));
    mac.addData(QByteArray(kIban));
    const QString legacy = QString::fromLatin1(mac.result().toHex());
    QCOMPARE(AccountHashing::legacyCompute(iban, QStringLiteral("PSSTFRPP")), legacy);
    QCOMPARE(AccountHashing::legacyCompute(iban, QStringLiteral("PSSTFRPPXXX")), legacy);
    QVERIFY(legacy != hash);

    // An account linked by v0.2.1 (the statement has the 8-character BIC) is
    // found, and the save replaces its hash.
    Database& db = Database::instance();
    QVERIFY2(db.linkAccountHash(m_account, legacy, QStringLiteral("auto")), qPrintable(db.lastError()));
    {
        SnapshotImportController controller;
        parse(controller, marchStatement());
        const QVariantList accounts = accountsForMatch();
        QCOMPARE(controller.matchAccountIndex(accounts), indexOfAccount(accounts, m_account));
        controller.save(m_account, QStringLiteral("Checking"));
        QVERIFY2(controller.statusText().startsWith(QStringLiteral("✓")), qPrintable(controller.statusText()));
    }
    QCOMPARE(hashRowCount(), 1);
    QVERIFY(db.accountHasHash(m_account, hash));
    QVERIFY(!db.accountHasHash(m_account, legacy));

    // A statement with the BIC on 11 characters finds the same account.
    QByteArray csv = marchStatement();
    QVERIFY(csv.contains("BIC,PSSTFRPP\n"));
    csv.replace("BIC,PSSTFRPP\n", "BIC,PSSTFRPPXXX\n");
    SnapshotImportController controller;
    parse(controller, csv);
    const QVariantList accounts = accountsForMatch();
    QCOMPARE(controller.matchAccountIndex(accounts), indexOfAccount(accounts, m_account));
    QCOMPARE(controller.statementInfo().value(QStringLiteral("accountHashRaw")).toString(), hash);
}

void ImportSaveTest::oldHashOnAnotherAccountIsReplaced()
{
    Database& db = Database::instance();
    const int savings = db.addAccount(QStringLiteral("Savings"), QStringLiteral("CCP"), QStringLiteral(""),
                                      QStringLiteral("EUR"), QStringLiteral(""));
    QVERIFY(savings >= 0);
    const QString iban = QString::fromLatin1(kIban);
    const QString hash = AccountHashing::compute(iban, QString::fromLatin1(kBic));
    const QString legacy = AccountHashing::legacyCompute(iban, QString::fromLatin1(kBic));
    QVERIFY(!legacy.isEmpty());

    // The old hash on Checking, the new one on Savings: the new one wins.
    QVERIFY2(db.linkAccountHash(m_account, legacy, QStringLiteral("auto")), qPrintable(db.lastError()));
    QVERIFY2(db.linkAccountHash(savings, hash, QStringLiteral("auto")), qPrintable(db.lastError()));
    {
        SnapshotImportController controller;
        parse(controller, marchStatement());
        const QVariantList accounts = accountsForMatch();
        QCOMPARE(controller.matchAccountIndex(accounts), indexOfAccount(accounts, savings));
        // The review screen tells that Checking has the old hash.
        QCOMPARE(controller.legacyHashAccountIndex(accounts), indexOfAccount(accounts, m_account));
        QCOMPARE(controller.statementInfo().value(QStringLiteral("legacyHashRaw")).toString(), legacy);

        // A failed save keeps the old hash on Checking.
        QSqlQuery query(db.sqlDatabase());
        QVERIFY(query.exec(QStringLiteral("CREATE TRIGGER fd_test_fail_checkpoint BEFORE INSERT ON"
                                          " account_balance_checkpoints BEGIN SELECT RAISE(ABORT, 'forced'); END;")));
        QVERIFY(!controller.save(savings, QStringLiteral("Savings")));
        QCOMPARE(db.accountOfHash(legacy), m_account);
        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER fd_test_fail_checkpoint;")));
        query.finish();

        // The save to Savings removes it: one hash, the new one, on Savings.
        QVERIFY(controller.save(savings, QStringLiteral("Savings")));
    }
    QCOMPARE(db.accountOfHash(legacy), -1);
    QCOMPARE(db.accountOfHash(hash), savings);
    QCOMPARE(hashRowCount(), 1);

    // "Link this account" replaces the old hash too.
    QVERIFY(db.linkAccountHash(m_account, legacy, QStringLiteral("auto")));
    QVERIFY2(db.linkAccountHash(m_account, hash, QStringLiteral("auto"), legacy), qPrintable(db.lastError()));
    QCOMPARE(db.accountOfHash(legacy), -1);
    QCOMPARE(db.accountOfHash(hash), m_account);
    QCOMPARE(hashRowCount(), 1);
}

void ImportSaveTest::predictionsRunInAWorkerThread()
{
    saveMarchWithThreeCategories();

    SnapshotImportController controller;
    QSignalSpy predicting(&controller, &SnapshotImportController::predictingChanged);
    parseWithoutWaiting(controller, marchStatementLaterExport());
    // The rows show at once; their suggestions come from the worker thread.
    QVERIFY(controller.predicting());
    QCOMPARE(controller.transactionRows().size(), qsizetype(3));
    QVERIFY(rowCategory(controller, 0).isEmpty());

    // During the job: a category picked by hand, and a name changed.
    controller.setRowCategory(1, QStringLiteral("Food"));
    controller.setRowName(2, QStringLiteral("ZZQQ WIDGET EMPORIUM"));
    // A save waits for the suggestions.
    QVERIFY(!controller.save(m_account, QStringLiteral("Checking")));
    QVERIFY2(controller.statusText().contains(QStringLiteral("not ready yet")), qPrintable(controller.statusText()));
    QCOMPARE(transactionCount(), 3);

    QTRY_VERIFY_WITH_TIMEOUT(!controller.predicting(), 10000);
    QCOMPARE(predicting.count(), 2); // true, then false
    QCOMPARE(rowCategory(controller, 0), QStringLiteral("Groceries"));
    QCOMPARE(rowCategory(controller, 1), QStringLiteral("Food"));   // the choice of the user stays
    QVERIFY(rowCategory(controller, 2).isEmpty());                    // scored with its new name: unknown
    QVERIFY2(controller.save(m_account, QStringLiteral("Checking")), qPrintable(controller.statusText()));

    // "+ rule" scores the other rows again in the worker thread: the second
    // row of a new merchant gets the category of the rule, and only the rule
    // gives it.
    SnapshotImportController next;
    parse(next, newMerchantTwiceStatement());
    QVERIFY(rowCategory(next, 1).isEmpty());
    next.setRowCategory(0, QStringLiteral("Food"));
    QVERIFY2(next.rememberRowAsRule(0), qPrintable(next.statusText()));
    QVERIFY(next.predicting());
    QVERIFY(rowCategory(next, 1).isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!next.predicting(), 10000);
    QCOMPARE(rowCategory(next, 1), QStringLiteral("Food"));
}

void ImportSaveTest::olderPredictionJobCountsForNothing()
{
    saveMarchWithThreeCategories();

    // Another file during a job: only the job of the second file writes its
    // suggestions (SNCF, BOULANGERIE, then a new merchant).
    SnapshotImportController controller;
    QSignalSpy applied(&controller, &SnapshotImportController::predictionsAboutToBeApplied);
    parseWithoutWaiting(controller, marchStatement());
    parseWithoutWaiting(controller, overlappingStatement());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.predicting(), 10000);
    waitForTheWorker();
    QCOMPARE(applied.count(), 1);
    QCOMPARE(rowCategory(controller, 0), QStringLiteral("Transport"));
    QCOMPARE(rowCategory(controller, 1), QStringLiteral("Food"));
    QVERIFY(rowCategory(controller, 2).isEmpty());

    // reset(), or a file that does not parse, while the job runs, or when
    // the job is done and its signal waits in the event queue: the job
    // changes nothing.
    for (const bool withReset : {true, false}) {
        for (const bool jobDone : {false, true}) {
            parseWithoutWaiting(controller, marchStatement());
            QVERIFY(controller.predicting());
            if (jobDone) {
                QVERIFY(QThreadPool::globalInstance()->waitForDone(10000));
            }
            if (withReset) {
                controller.reset();
            } else {
                controller.parseFile(QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("missing.csv"))),
                                     QString::fromLatin1(kTemplate));
                QVERIFY(controller.lastParseFailed());
            }
            QVERIFY(!controller.predicting());
            QSignalSpy changed(&controller, &SnapshotImportController::statementChanged);
            waitForTheWorker();
            QCOMPARE(changed.count(), 0);
            QCOMPARE(applied.count(), 1);
            QVERIFY(controller.transactionRows().isEmpty());
        }
    }
}

void ImportSaveTest::nameGivenBeforeTheSuggestionsIsScored()
{
    saveMarchWithThreeCategories();

    // The page gives the text of a NAME field with the focus to setRowName()
    // when predictionsAboutToBeApplied() comes, before the rows get their
    // suggestions: the row is scored with its new name, by the new predictor.
    SnapshotImportController controller;
    QString categoryBefore = QStringLiteral("not called");
    connect(&controller, &SnapshotImportController::predictionsAboutToBeApplied, &controller,
            [&controller, &categoryBefore]() {
                categoryBefore = rowCategory(controller, 0);
                controller.setRowName(2, QStringLiteral("CARREFOUR"));
            });
    parseWithoutWaiting(controller, marchStatement());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.predicting(), 10000);
    QVERIFY(categoryBefore.isEmpty()); // the rows had no suggestion yet
    QCOMPARE(rowCategory(controller, 0), QStringLiteral("Groceries"));
    QCOMPARE(rowCategory(controller, 2), QStringLiteral("Groceries")); // BOULANGERIE is CARREFOUR now
}

void ImportSaveTest::newJobCancelsTheOlderOne()
{
    saveMarchWithThreeCategories();

    // A refresh, another file, reset() and the destructor cancel the job of
    // the worker thread: it stops at its next check (cancelledJobGivesNoResult).
    // The test reads the private watcher (friend class).
    SnapshotImportController controller;
    parseWithoutWaiting(controller, marchStatement());
    const QFuture<SnapshotImportController::PredictionJob> first = controller.m_predictionWatcher.future();
    QVERIFY(!first.isCanceled());
    controller.refreshPredictions();
    QVERIFY(first.isCanceled());

    const QFuture<SnapshotImportController::PredictionJob> second = controller.m_predictionWatcher.future();
    QVERIFY(!second.isCanceled());
    parseWithoutWaiting(controller, overlappingStatement());
    QVERIFY(second.isCanceled());

    const QFuture<SnapshotImportController::PredictionJob> third = controller.m_predictionWatcher.future();
    QVERIFY(!third.isCanceled());
    controller.reset();
    QVERIFY(third.isCanceled());

    QFuture<SnapshotImportController::PredictionJob> fourth;
    {
        SnapshotImportController other;
        parseWithoutWaiting(other, marchStatement());
        fourth = other.m_predictionWatcher.future();
        QVERIFY(!fourth.isCanceled());
    }
    QVERIFY(fourth.isCanceled());
    waitForTheWorker();
}

void ImportSaveTest::cancelledJobGivesNoResult()
{
    saveMarchWithThreeCategories();

    // The work of a job, as the worker thread runs it: isCanceled() is called
    // after the training and after each row.
    const QVector<Data::Transaction> categorized = Database::instance().allCategorizedTransactions();
    const QVector<Data::CategoryRule> rules = Database::instance().loadCategoryRules();
    QVector<SnapshotImportController::RowInput> inputs;
    for (const QString& name : {QStringLiteral("Card payment 1234: CARREFOUR"), QStringLiteral("Card payment 1234: SNCF"),
                                QStringLiteral("Card payment 1234: BOULANGERIE")}) {
        inputs.append(SnapshotImportController::RowInput{name, QString(), -10.0, QDate(2026, 4, 2)});
    }

    int checks = 0;
    std::optional<SnapshotImportController::PredictionJob> job = SnapshotImportController::runPredictionJob(
        7, categorized, rules, inputs, [&checks]() { ++checks; return false; });
    QVERIFY(job.has_value());
    QCOMPARE(checks, 1 + static_cast<int>(inputs.size()));
    QCOMPARE(job->generation, 7);
    QCOMPARE(job->predictions.size(), inputs.size());
    QCOMPARE(job->predictions.at(1).category, QStringLiteral("Transport"));

    // Cancelled during the training: no row is scored, no result.
    checks = 0;
    job = SnapshotImportController::runPredictionJob(8, categorized, rules, inputs, [&checks]() {
        ++checks;
        return true;
    });
    QVERIFY(!job.has_value());
    QCOMPARE(checks, 1);

    // Cancelled while the first row is scored: the job stops after it.
    checks = 0;
    job = SnapshotImportController::runPredictionJob(9, categorized, rules, inputs, [&checks]() {
        return ++checks == 2;
    });
    QVERIFY(!job.has_value());
    QCOMPARE(checks, 2);
}

QTEST_GUILESS_MAIN(ImportSaveTest)
#include "import_save_test.moc"
