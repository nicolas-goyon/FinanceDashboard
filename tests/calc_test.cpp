// calc_test — the calculations behind the texts and charts of the pages.
//
//   #41   Projects: no monthly contribution gives "Never", a project with no
//         gap has its own save-vs-borrow text, and "Still Needed" is never
//         negative
//   #42   Cash Flow: the two sides of the money flow have the same total, also
//         when the spending is more than the income
//   #210  Cash Flow on real data: no internal transfer, the top 5 categories
//   #56   Cash Flow: the 3-month average starts at the third month;
//         Investments: the deposit of the month is not performance
//   #48   Fmt::money(): rounded to cents before the sign, "—" for NaN and
//         infinity, and the currency of the amount
//   #207  TransferMatcher: a debit and a credit of the same amount on two
//         accounts of the net worth, 3 days apart or less, are one transfer
//   #208  NetWorthCalc: the value of an investment account between two
//         valuations, and savings + gains = change of the net worth
//   #211  SavingsCalc: the ceilings of the regulated accounts, the rows of
//         interest
//   #33   Money: whole cents (half away from zero), the amounts that the app
//         can store, and cents that survive a round trip through a double

#include "InvestmentsController.h"
#include "ProjectsController.h"
#include "calc/CashFlowCalc.h"
#include "calc/NetWorthCalc.h"
#include "calc/SavingsCalc.h"
#include "calc/TransferMatcher.h"
#include "utils/Formatter.h"
#include "utils/Money.h"

#include <QDate>
#include <QDateTime>
#include <QStringList>
#include <QTest>
#include <QVariantMap>

#include <array>
#include <cmath>
#include <limits>

namespace {

QString kpiValue(const ProjectsController& controller, const QString& label)
{
    for (const QVariant& kpi : controller.kpis()) {
        const QVariantMap map = kpi.toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            return map.value(QStringLiteral("value")).toString();
        }
    }
    return {};
}

// `text` with each '_' made a narrow no-break space (U+202F), the separator
// of Fmt::money().
QString nb(const char* text)
{
    return QString::fromUtf8(text).replace(QLatin1Char('_'), QChar(0x202F));
}

double total(const QVariantList& side)
{
    double sum = 0.0;
    for (const QVariant& part : side) {
        sum += part.toMap().value(QStringLiteral("value")).toDouble();
    }
    return sum;
}

QStringList names(const QVariantList& side)
{
    QStringList result;
    for (const QVariant& part : side) {
        result.append(part.toMap().value(QStringLiteral("name")).toString());
    }
    return result;
}

QVariantMap card(const ProjectsController& controller, const QString& name)
{
    for (const QVariant& projectCard : controller.projectCards()) {
        const QVariantMap map = projectCard.toMap();
        if (map.value(QStringLiteral("name")).toString() == name) {
            return map;
        }
    }
    return {};
}

} // namespace

class CalcTest : public QObject
{
    Q_OBJECT

private slots:
    void projectWithNoMonthlyContributionIsNeverDone();
    void projectWithNoGapSaysSo();
    void projectAboveItsTargetNeedsNothing();
    void projectWithATinyContributionStaysInRange();
    void moneyFlowTotalsMatchWithAnOverspend();
    void moneyFlowSavesTheLeftover();
    void rollingAverageStartsAtTheThirdMonth();
    void cashFlowLeavesTheTransfersOut();
    void monthlyPerformanceLeavesTheDepositOut();
    void moneyIsRoundedBeforeItsSign();
    void moneyOfNotANumberIsADash();
    void moneyUsesTheGivenCurrency();
    void centsAreRoundedHalfAwayFromZero();
    void sumOfCentsIsExact();
    void onlyValidAmountsAreRounded();
    void centsSurviveARoundTrip();
    void transferIsADebitAndACreditOfTheNetWorth();
    void transferPairsTakeTheSmallestGap();
    void valueBetweenTwoValuations();
    void savingsPlusGainsIsTheChangeOfTheNetWorth();
    void savingsCeilingsAndInterest();
    void accountGroupsInEnglishAndFrench();
};

namespace {
Data::Transaction row(int id, int account, const QDate& date, double debit, double credit, int transferId = 0)
{
    Data::Transaction transaction;
    transaction.id = id;
    transaction.accountId = account;
    transaction.operationDate = date;
    transaction.debit = debit;
    transaction.credit = credit;
    transaction.transferId = transferId;
    return transaction;
}
} // namespace

void CalcTest::projectWithNoMonthlyContributionIsNeverDone()
{
    ProjectsController controller;
    // addProject() selects the new project.
    controller.addProject(QStringLiteral("Bike"), 1000.0, 6, 200.0, 0.0);

    QCOMPARE(kpiValue(controller, QStringLiteral("Projected Completion")), QStringLiteral("Never"));
    QCOMPARE(card(controller, QStringLiteral("Bike")).value(QStringLiteral("completionText")).toString(),
             QStringLiteral("Never"));
    const QString text = controller.saveVsBorrowText();
    QVERIFY2(text.startsWith(QStringLiteral("With no monthly contribution, saving does not close the ")
                             + Fmt::money(800.0) + QStringLiteral(" gap.")),
             qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("~0 more months")), qPrintable(text));
}

void CalcTest::projectWithNoGapSaysSo()
{
    ProjectsController controller;
    // 1 000 € in 10 months at 200 €/month: done after 5 months, no gap.
    controller.addProject(QStringLiteral("Bike"), 1000.0, 10, 0.0, 200.0);

    QCOMPARE(kpiValue(controller, QStringLiteral("Gap at Target Date")), QStringLiteral("None"));
    const QDate today = QDate::currentDate();
    const QString done = QDate(today.year(), today.month(), 1).addMonths(5).toString(QStringLiteral("MMMM yyyy"));
    QCOMPARE(kpiValue(controller, QStringLiteral("Projected Completion")), done);
    const QString text = controller.saveVsBorrowText();
    QCOMPARE(text, QStringLiteral("There is no gap. At %1/month, the target is reached in %2. "
                                  "The loan scenarios borrow a sample amount of %3.")
                       .arg(Fmt::money(200.0), done, Fmt::money(10000.0)));
}

void CalcTest::projectAboveItsTargetNeedsNothing()
{
    ProjectsController controller;
    controller.addProject(QStringLiteral("Bike"), 1000.0, 6, 3000.0, 100.0);

    QCOMPARE(kpiValue(controller, QStringLiteral("Still Needed")), Fmt::money(0.0));
    QCOMPARE(kpiValue(controller, QStringLiteral("Projected Completion")), QStringLiteral("Reached"));
    QCOMPARE(card(controller, QStringLiteral("Bike")).value(QStringLiteral("completionText")).toString(),
             QStringLiteral("Reached"));
    QVERIFY2(controller.saveVsBorrowText().startsWith(
                 QStringLiteral("There is no gap. The earmarked amount already covers the target.")),
             qPrintable(controller.saveVsBorrowText()));
}

void CalcTest::projectWithATinyContributionStaysInRange()
{
    ProjectsController controller;
    const qsizetype before = controller.projectNames().size();

    // Below half a cent a month: no contribution. Before, the month count
    // overflowed int (undefined behaviour) and gave a negative date.
    controller.addProject(QStringLiteral("Tiny"), 1000000.0, 6, 0.0, 0.0001);
    QCOMPARE(kpiValue(controller, QStringLiteral("Projected Completion")), QStringLiteral("Never"));
    QVERIFY2(controller.saveVsBorrowText().startsWith(QStringLiteral("With no monthly contribution")),
             qPrintable(controller.saveVsBorrowText()));

    // 1 € a month for a gap of about 1 000 000 €: more than 100 years.
    controller.addProject(QStringLiteral("Slow"), 1000000.0, 6, 0.0, 1.0);
    QCOMPARE(kpiValue(controller, QStringLiteral("Projected Completion")), QStringLiteral("Never"));
    QVERIFY2(controller.saveVsBorrowText().contains(QStringLiteral("takes more than 100 years")),
             qPrintable(controller.saveVsBorrowText()));

    // Not a number, an infinity, or a target date after 100 years: refused.
    // addProject() returns false, so the dialog stays open and tells why (#75).
    QVERIFY(!controller.addProject(QStringLiteral("Infinite"), std::numeric_limits<double>::infinity(), 6, 0.0, 10.0));
    QVERIFY(!controller.addProject(QStringLiteral("Far"), 1000.0, 1201, 0.0, 10.0));
    QVERIFY(!controller.addProject(QStringLiteral("NaN"), 1000.0, 6, std::numeric_limits<double>::quiet_NaN(), 10.0));
    QVERIFY(!controller.addProject(QStringLiteral("Zero"), 0.0, 6, 0.0, 10.0));
    QVERIFY(!controller.addProject(QStringLiteral("  "), 1000.0, 6, 0.0, 10.0));
    QCOMPARE(controller.projectNames().size(), before + 2);
    QVERIFY(controller.addProject(QStringLiteral("Ok"), 1000.0, 6, 0.0, 10.0));
}

namespace {
QVector<CashFlowCalc::Part> parts(const QVector<double>& values)
{
    const QStringList kNames = {QStringLiteral("Rent"), QStringLiteral("Groceries"), QStringLiteral("Transport"),
                                QStringLiteral("Utilities"), QStringLiteral("Leisure"), QStringLiteral("Other")};
    QVector<CashFlowCalc::Part> result;
    for (qsizetype i = 0; i < values.size(); ++i) {
        result.append(CashFlowCalc::Part{kNames.at(i), values.at(i), QStringLiteral("#ffffff")});
    }
    return result;
}
} // namespace

void CalcTest::moneyFlowTotalsMatchWithAnOverspend()
{
    // 2 300 € spent with an income of 2 000 €: 300 € come from the savings.
    const CashFlowCalc::FlowSides sides = CashFlowCalc::moneyFlow(2000.0, parts({950, 450, 150, 180, 320, 250}));

    QCOMPARE(total(sides.inputs), 2300.0);
    QCOMPARE(total(sides.outputs), 2300.0);
    QCOMPARE(names(sides.inputs), QStringList({QStringLiteral("Income"), QStringLiteral("Overspend")}));
    QCOMPARE(sides.inputs.last().toMap().value(QStringLiteral("value")).toDouble(), 300.0);
    QVERIFY(!names(sides.outputs).contains(QStringLiteral("Savings")));
}

void CalcTest::moneyFlowSavesTheLeftover()
{
    const CashFlowCalc::FlowSides sides = CashFlowCalc::moneyFlow(3000.0, parts({950, 450, 150, 180, 320, 250}));

    QCOMPARE(total(sides.inputs), 3000.0);
    QCOMPARE(total(sides.outputs), 3000.0);
    QCOMPARE(names(sides.inputs), QStringList({QStringLiteral("Income")}));
    QCOMPARE(names(sides.outputs).last(), QStringLiteral("Savings"));
    QCOMPARE(sides.outputs.last().toMap().value(QStringLiteral("value")).toDouble(), 700.0);

    // A negative value is not drawn, and not counted.
    const CashFlowCalc::FlowSides refund = CashFlowCalc::moneyFlow(1000.0, parts({950, -100, 0, 0, 0, 0}));
    QCOMPARE(total(refund.inputs), 1000.0);
    QCOMPARE(total(refund.outputs), 1000.0);
    QCOMPARE(names(refund.outputs), QStringList({QStringLiteral("Rent"), QStringLiteral("Savings")}));
}

void CalcTest::rollingAverageStartsAtTheThirdMonth()
{
    // 100, 200, 300, … of spending in the 12 months.
    const QDate today(2026, 11, 4);
    QVector<Data::Transaction> rows;
    for (int m = 0; m < CashFlowCalc::kMonths; ++m) {
        rows.append(row(m + 1, 1, QDate(2025, 12, 10).addMonths(m), 100.0 * (m + 1), 0.0));
    }
    const CashFlowCalc::Result result = CashFlowCalc::compute(rows, today);
    QCOMPARE(result.months.first(), QDate(2025, 12, 1));
    QCOMPARE(result.months.last(), QDate(2026, 11, 1));
    QCOMPARE(result.spendingCents.first(), qint64(10000));
    QCOMPARE(result.rolling.size(), qsizetype(10));
    QCOMPARE(result.rolling.first(), 200.0); // (100 + 200 + 300) / 3
    QCOMPARE(result.rolling.last(), 1100.0); // (1000 + 1100 + 1200) / 3
}

void CalcTest::cashFlowLeavesTheTransfersOut()
{
    // #210: a transfer to the Livret A is not spending; a row older than the
    // 12 months is not counted; the 6th category and after go to "Other".
    const QDate today(2026, 11, 4);
    Data::Transaction salary = row(1, 1, QDate(2026, 11, 1), 0.0, 2500.0);
    salary.category = QStringLiteral("Income");
    Data::Transaction toLivret = row(2, 1, QDate(2026, 11, 2), 1000.0, 0.0, 2);
    QVector<Data::Transaction> rows = {salary, toLivret, row(3, 1, QDate(2025, 11, 30), 999.0, 0.0)};
    const QStringList categories = {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
                                    QStringLiteral("D"), QStringLiteral("E"), QStringLiteral("F"),
                                    QStringLiteral("G")};
    for (qsizetype i = 0; i < categories.size(); ++i) {
        Data::Transaction spent = row(int(10 + i), 1, QDate(2026, 11, 3), 70.0 - (10.0 * double(i)), 0.0);
        spent.category = categories.at(i);
        rows.append(spent);
    }
    const CashFlowCalc::Result result = CashFlowCalc::compute(rows, today);
    QCOMPARE(result.incomeCents.last(), qint64(250000));
    QCOMPARE(result.spendingCents.last(), qint64(28000)); // 70 + 60 + … + 10, no transfer
    QCOMPARE(result.spendingCents.first(), qint64(0));    // 30/11/2025 is before the 12 months
    QCOMPARE(result.stackedNames, QStringList({QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
                                               QStringLiteral("D"), QStringLiteral("E"), QStringLiteral("Other")}));
    QCOMPARE(result.stackedCents.at(5).last(), qint64(3000)); // F (20) + G (10)
    QCOMPARE(result.biggest.last().size(), qsizetype(CashFlowCalc::kBiggestRows));
    QCOMPARE(result.biggest.last().first().category, QStringLiteral("A"));
}

void CalcTest::monthlyPerformanceLeavesTheDepositOut()
{
    InvestmentsController controller;
    // Month 2 of the mock data: 10 100 € → 10 600 €, with a 250 € deposit.
    const double gain = controller.perfGains().at(1).toDouble();
    QVERIFY2(qAbs(gain - ((10600.0 - 10100.0 - 250.0) / 10100.0 * 100.0)) < 1e-9, qPrintable(QString::number(gain)));
    // Month 3: 10 600 € → 10 200 €, a loss on top of the deposit.
    const double loss = controller.perfLosses().at(2).toDouble();
    QVERIFY2(qAbs(loss - ((10200.0 - 10600.0 - 250.0) / 10600.0 * 100.0)) < 1e-9, qPrintable(QString::number(loss)));
}

void CalcTest::moneyIsRoundedBeforeItsSign()
{
    QCOMPARE(Fmt::money(-1e-12, true), nb("+0,00_€"));
    QCOMPARE(Fmt::money(-0.004), nb("0,00_€"));
    QCOMPARE(Fmt::money(-0.006), nb("-0,01_€"));
    QCOMPARE(Fmt::money(19.99), nb("19,99_€"));
    QCOMPARE(Fmt::money(0.999), nb("1,00_€"));
    QCOMPARE(Fmt::money(-1234567.891), nb("-1_234_567,89_€"));
    QCOMPARE(Fmt::money(80.5, true), nb("+80,50_€"));
}

void CalcTest::moneyOfNotANumberIsADash()
{
    QCOMPARE(Fmt::money(std::numeric_limits<double>::quiet_NaN()), QStringLiteral("—"));
    QCOMPARE(Fmt::money(std::numeric_limits<double>::infinity()), QStringLiteral("—"));
    QCOMPARE(Fmt::money(-std::numeric_limits<double>::infinity(), true), QStringLiteral("—"));
    QCOMPARE(Fmt::money(1e300), QStringLiteral("—"));
}

void CalcTest::moneyUsesTheGivenCurrency()
{
    QCOMPARE(Fmt::money(80.5, Fmt::Currency::USD), nb("80,50_$"));
    QCOMPARE(Fmt::money(-80.5, Fmt::Currency::USD, true), nb("-80,50_$"));
    QCOMPARE(Fmt::money(80.5, Fmt::Currency::EUR), nb("80,50_€"));
    QCOMPARE(Fmt::currencyFromCode(QStringLiteral("USD")), Fmt::Currency::USD);
    QCOMPARE(Fmt::currencyFromCode(QStringLiteral("usd")), Fmt::Currency::USD);
    QCOMPARE(Fmt::currencyFromCode(QStringLiteral("EUR")), Fmt::Currency::EUR);
    QCOMPARE(Fmt::currencyFromCode(QString()), Fmt::Currency::EUR);
    QCOMPARE(Fmt::currencySymbol(Fmt::Currency::USD), QStringLiteral("$"));
}

void CalcTest::centsAreRoundedHalfAwayFromZero()
{
    QCOMPARE(Money::toCents(19.99), qint64(1999));
    QCOMPARE(Money::toCents(-86.4), qint64(-8640));
    QCOMPARE(Money::toCents(1.006), qint64(101));
    QCOMPARE(Money::toCents(-2.004), qint64(-200));
    QCOMPARE(Money::toCents(0.125), qint64(13));   // 0.125 is exact in a double
    QCOMPARE(Money::toCents(-0.125), qint64(-13));
    QCOMPARE(Money::toCents(0.00999999999999995), qint64(1));
    QVERIFY(Money::fromCents(-8640) == -86.4);
    // The same cents as Fmt::money().
    QCOMPARE(Fmt::money(Money::roundToCents(0.125)), nb("0,13_€"));
}

void CalcTest::sumOfCentsIsExact()
{
    // The example of #33: 0.10 + 0.20 is not 0.30 in a double, so a check
    // with a tolerance of 0.01 took a gap of one cent for no gap.
    const double sum = 0.10 + 0.20;
    QVERIFY(sum != 0.30);
    QVERIFY(std::abs(0.31 - sum) < 0.01);
    const qint64 gap = Money::toCents(0.31) - (Money::toCents(0.10) + Money::toCents(0.20));
    QCOMPARE(gap, qint64(1));
    QCOMPARE(Money::toCents(0.30) - (Money::toCents(0.10) + Money::toCents(0.20)), qint64(0));
    QVERIFY(Money::roundToCents(sum) == 0.30); // exact, not a fuzzy QCOMPARE
}

void CalcTest::onlyValidAmountsAreRounded()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    QVERIFY(Money::isValid(0.0));
    QVERIFY(Money::isValid(Money::kMaxAmount));
    QVERIFY(Money::isValid(-Money::kMaxAmount));
    QVERIFY(!Money::isValid(nan));
    QVERIFY(!Money::isValid(infinity));
    QVERIFY(!Money::isValid(2e13));
    // An amount that is not valid stays as it is, so that Database refuses it.
    QVERIFY(std::isnan(Money::roundToCents(nan)));
    QVERIFY(Money::roundToCents(2e13 + 0.004) == 2e13 + 0.004);
    QVERIFY(Money::roundToCents(-infinity) == -infinity);
    // toCents() has a result for every value: 0 for NaN, the limit for larger.
    QCOMPARE(Money::toCents(nan), qint64(0));
    QCOMPARE(Money::toCents(infinity), qint64(1'000'000'000'000'000));
    QCOMPARE(Money::toCents(-1e300), qint64(-1'000'000'000'000'000));
}

void CalcTest::centsSurviveARoundTrip()
{
    // Up to kMaxAmount, an amount rounded to the cent gives back its cents:
    // the migration keeps the fingerprints (TransactionsTable::fingerprint()).
    const std::array<qint64, 8> cents = {
        1, -1, 30, 1999, 123'456'789, -987'654'321'012, 999'999'999'999'999, -1'000'000'000'000'000,
    };
    for (const qint64 value : cents) {
        const double amount = Money::fromCents(value);
        QCOMPARE(Money::toCents(amount), value);
        QCOMPARE(Money::toCents(Money::roundToCents(amount)), value);
        QVERIFY(Money::roundToCents(amount) == amount);
    }
    for (qint64 value = -100'000; value <= 100'000; value += 7) {
        QCOMPARE(Money::toCents(Money::fromCents(value)), value);
    }
}

void CalcTest::transferIsADebitAndACreditOfTheNetWorth()
{
    // Account 1: checking, 2: Livret A, 3: a joint account outside the net worth.
    const QSet<int> scope = {1, 2};
    const QDate day(2026, 3, 10);
    const QVector<Data::Transaction> rows = {
        row(1, 1, day, 500.0, 0.0),                // checking → Livret A, two days later
        row(2, 2, day.addDays(2), 0.0, 500.0),
        row(3, 1, day, 80.0, 0.0),                 // to the joint account: spending
        row(4, 3, day, 0.0, 80.0),
        row(5, 1, day, 40.0, 0.0),                 // 4 days apart: not suggested
        row(6, 2, day.addDays(4), 0.0, 40.0),
        row(7, 1, day, 25.0, 0.0),                 // same account: not a transfer
        row(8, 1, day, 0.0, 25.0),
        row(9, 1, day, 300.0, 0.0, Data::kNotATransfer), // the user said no
        row(10, 2, day, 0.0, 300.0),
        row(11, 1, day, 12.34, 0.0),               // amounts in cents: 12.34 is not 12.35
        row(12, 2, day, 0.0, 12.35),
    };
    const QVector<TransferMatcher::Pair> pairs = TransferMatcher::find(rows, scope);
    QCOMPARE(pairs.size(), qsizetype(1));
    QCOMPARE(pairs.first().debitId, 1);
    QCOMPARE(pairs.first().creditId, 2);
    QCOMPARE(pairs.first().days, 2);
    // A wider window finds the 4-day pair too.
    QCOMPARE(TransferMatcher::find(rows, scope, 4).size(), qsizetype(2));
}

void CalcTest::transferPairsTakeTheSmallestGap()
{
    // Two debits of 100 and two credits of 100: each row is in one pair, and
    // the closest dates go together.
    const QSet<int> scope = {1, 2};
    const QDate day(2026, 5, 1);
    const QVector<Data::Transaction> rows = {
        row(1, 1, day, 100.0, 0.0),
        row(2, 1, day.addDays(3), 100.0, 0.0),
        row(3, 2, day.addDays(1), 0.0, 100.0),
        row(4, 2, day.addDays(3), 0.0, 100.0),
    };
    const QVector<TransferMatcher::Pair> pairs = TransferMatcher::find(rows, scope);
    QCOMPARE(pairs.size(), qsizetype(2));
    QCOMPARE(pairs.at(0).debitId, 2); // same day
    QCOMPARE(pairs.at(0).creditId, 4);
    QCOMPARE(pairs.at(1).debitId, 1);
    QCOMPARE(pairs.at(1).creditId, 3);
}

void CalcTest::valueBetweenTwoValuations()
{
    // An assurance vie: 10 000 on 01/31, a deposit of 500 on 02/10, then a
    // valuation of 11 200 on 03/31.
    NetWorthCalc::AccountHistory account;
    account.accountId = 1;
    account.checkpoints = {
        Data::AccountBalanceCheckpoint{1, 1, QDate(2026, 1, 31), 10000.0, QString()},
        Data::AccountBalanceCheckpoint{2, 1, QDate(2026, 3, 31), 11200.0, QString()},
    };
    Data::Transaction deposit = row(1, 1, QDate(2026, 2, 10), 0.0, 500.0);
    deposit.balanceAfter = 500.0; // the cash line of the statement: not the value
    account.rows = {deposit};
    QCOMPARE(NetWorthCalc::valueCentsAt(account, QDate(2026, 1, 15)), qint64(0)); // before anything
    QCOMPARE(NetWorthCalc::valueCentsAt(account, QDate(2026, 2, 9)), qint64(1000000));
    QCOMPARE(NetWorthCalc::valueCentsAt(account, QDate(2026, 2, 15)), qint64(1050000));
    QCOMPARE(NetWorthCalc::valueCentsAt(account, QDate(2026, 3, 31)), qint64(1120000));
    // February and March: 500 deposited, so 700 of gains.
    const NetWorthCalc::Period period = NetWorthCalc::period({account}, QDate(2026, 2, 1), QDate(2026, 3, 31));
    QCOMPARE(period.changeCents(), qint64(120000));
    QCOMPARE(period.flows.savingsCents(), qint64(50000));
    QCOMPARE(period.gainsCents(), qint64(70000));

    // A checking account with no checkpoint: the balance before its first
    // row comes from that row.
    NetWorthCalc::AccountHistory checking;
    checking.accountId = 2;
    Data::Transaction first = row(2, 2, QDate(2026, 1, 5), 100.0, 0.0);
    first.balanceAfter = 1000.0;
    checking.rows = {row(3, 2, QDate(2026, 1, 20), 0.0, 2000.0), first};
    QCOMPARE(NetWorthCalc::valueCentsAt(checking, QDate(2026, 1, 4)), qint64(0));
    QCOMPARE(NetWorthCalc::valueCentsAt(checking, QDate(2026, 1, 5)), qint64(100000));
    QCOMPARE(NetWorthCalc::valueCentsAt(checking, QDate(2026, 1, 31)), qint64(300000));
}

void CalcTest::savingsPlusGainsIsTheChangeOfTheNetWorth()
{
    // A year of fictitious data: a salary, rent, a transfer to the Livret A
    // (an internal transfer, both sides), a payment to a joint account outside
    // the net worth (spending), interest in a row, and a valuation of an
    // investment account.
    NetWorthCalc::AccountHistory checking;
    checking.accountId = 1;
    checking.checkpoints = {Data::AccountBalanceCheckpoint{1, 1, QDate(2025, 12, 31), 1000.0, QString()}};
    checking.rows = {
        row(1, 1, QDate(2026, 1, 28), 0.0, 2500.0),        // salary
        row(2, 1, QDate(2026, 2, 1), 900.0, 0.0),          // rent
        row(3, 1, QDate(2026, 2, 3), 1000.0, 0.0, 3),      // → Livret A
        row(5, 1, QDate(2026, 2, 5), 150.0, 0.0),          // → joint account
    };
    NetWorthCalc::AccountHistory livret;
    livret.accountId = 2;
    livret.checkpoints = {Data::AccountBalanceCheckpoint{2, 2, QDate(2025, 12, 31), 5000.0, QString()}};
    livret.rows = {
        row(4, 2, QDate(2026, 2, 4), 0.0, 1000.0, 3),      // ← checking
        row(6, 2, QDate(2026, 12, 31), 0.0, 150.0),        // interest
    };
    NetWorthCalc::AccountHistory pea;
    pea.accountId = 3;
    pea.checkpoints = {
        Data::AccountBalanceCheckpoint{3, 3, QDate(2025, 12, 31), 8000.0, QString()},
        Data::AccountBalanceCheckpoint{4, 3, QDate(2026, 12, 31), 8600.0, QString()},
    };
    const QVector<NetWorthCalc::AccountHistory> scope = {checking, livret, pea};
    const NetWorthCalc::Period year = NetWorthCalc::period(scope, QDate(2026, 1, 1), QDate(2026, 12, 31));
    QCOMPARE(year.flows.incomeCents, qint64(265000));   // salary + interest
    QCOMPARE(year.flows.spendingCents, qint64(105000)); // rent + joint account, not the transfer
    QCOMPARE(year.flows.savingsCents(), qint64(160000));
    QCOMPARE(year.startValueCents, qint64(1400000));
    QCOMPARE(year.endValueCents, qint64(1620000));
    QCOMPARE(year.gainsCents(), qint64(60000));         // the PEA
    QCOMPARE(year.flows.savingsCents() + year.gainsCents(), year.changeCents());
}

void CalcTest::savingsCeilingsAndInterest()
{
    QCOMPARE(SavingsCalc::ceilingCents(QStringLiteral("Livret A")), qint64(2295000));
    QCOMPARE(SavingsCalc::ceilingCents(QStringLiteral("ldds")), qint64(1200000));
    QCOMPARE(SavingsCalc::ceilingCents(QStringLiteral("CCP")), qint64(0));

    Data::Transaction interest = row(1, 1, QDate(2026, 12, 31), 0.0, 150.0);
    interest.name = QStringLiteral("INTERETS 2026");
    QVERIFY(SavingsCalc::isInterest(interest));
    interest.name = QStringLiteral("Int\u00e9r\u00eats cr\u00e9diteurs");
    QVERIFY(SavingsCalc::isInterest(interest));
    interest.name = QStringLiteral("TRANSFER");
    interest.category = QStringLiteral("Interest");
    QVERIFY(SavingsCalc::isInterest(interest));
    // A debit is not interest; neither is a word that only contains it.
    Data::Transaction fee = row(2, 1, QDate(2026, 12, 31), 10.0, 0.0);
    fee.name = QStringLiteral("INTERETS DEBITEURS");
    QVERIFY(!SavingsCalc::isInterest(fee));
    Data::Transaction other = row(3, 1, QDate(2026, 12, 31), 0.0, 10.0);
    other.name = QStringLiteral("DISINTERESTED");
    QVERIFY(!SavingsCalc::isInterest(other));
}

void CalcTest::accountGroupsInEnglishAndFrench()
{
    // A database made in French has the default account categories in
    // French: the savings and investment groups still find them.
    Data::Account account;
    account.type = QStringLiteral("Other");
    account.category = QStringLiteral("\u00c9pargne");
    QVERIFY(Data::isSavings(account));
    account.category = QStringLiteral("Savings");
    QVERIFY(Data::isSavings(account));
    account.category = QStringLiteral("Investissements");
    QVERIFY(Data::isInvestment(account));
    QVERIFY(!Data::isSavings(account));
    account.category = QStringLiteral("investments");
    QVERIFY(Data::isInvestment(account));
    account.category = QStringLiteral("Courant");
    QVERIFY(!Data::isSavings(account));
    QVERIFY(!Data::isInvestment(account));
}

QTEST_GUILESS_MAIN(CalcTest)
#include "calc_test.moc"
