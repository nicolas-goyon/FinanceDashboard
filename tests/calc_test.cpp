// calc_test — the calculations behind the texts and charts of the pages.
//
//   #41   Projects: no monthly contribution gives "Never", a project with no
//         gap has its own save-vs-borrow text, and "Still Needed" is never
//         negative
//   #42   Cash Flow: the two sides of the money flow have the same total, also
//         when the spending is more than the income
//   #56   Cash Flow: the 3-month average starts at the third month;
//         Investments: the deposit of the month is not performance
//   #48   Fmt::money(): rounded to cents before the sign, "—" for NaN and
//         infinity, and the currency of the amount
//   #33   Money: whole cents (half away from zero), the amounts that the app
//         can store, and cents that survive a round trip through a double

#include "CashFlowController.h"
#include "InvestmentsController.h"
#include "ProjectsController.h"
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
    void monthlyPerformanceLeavesTheDepositOut();
    void moneyIsRoundedBeforeItsSign();
    void moneyOfNotANumberIsADash();
    void moneyUsesTheGivenCurrency();
    void centsAreRoundedHalfAwayFromZero();
    void sumOfCentsIsExact();
    void onlyValidAmountsAreRounded();
    void centsSurviveARoundTrip();
};

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

void CalcTest::moneyFlowTotalsMatchWithAnOverspend()
{
    // 2 300 € spent with an income of 2 000 €: 300 € come from the savings.
    const std::array<double, CashFlowController::kCategories> spending = {950, 450, 150, 180, 320, 250};
    const CashFlowController::FlowSides sides = CashFlowController::moneyFlow(2000.0, spending);

    QCOMPARE(total(sides.inputs), 2300.0);
    QCOMPARE(total(sides.outputs), 2300.0);
    QCOMPARE(names(sides.inputs), QStringList({QStringLiteral("Salary"), QStringLiteral("Overspend")}));
    QCOMPARE(sides.inputs.last().toMap().value(QStringLiteral("value")).toDouble(), 300.0);
    QVERIFY(!names(sides.outputs).contains(QStringLiteral("Savings")));
}

void CalcTest::moneyFlowSavesTheLeftover()
{
    const std::array<double, CashFlowController::kCategories> spending = {950, 450, 150, 180, 320, 250};
    const CashFlowController::FlowSides sides = CashFlowController::moneyFlow(3000.0, spending);

    QCOMPARE(total(sides.inputs), 3000.0);
    QCOMPARE(total(sides.outputs), 3000.0);
    QCOMPARE(names(sides.inputs), QStringList({QStringLiteral("Salary")}));
    QCOMPARE(names(sides.outputs).last(), QStringLiteral("Savings"));
    QCOMPARE(sides.outputs.last().toMap().value(QStringLiteral("value")).toDouble(), 700.0);

    // A refund (a negative value) is not drawn, and not counted.
    const std::array<double, CashFlowController::kCategories> withRefund = {950, -100, 0, 0, 0, 0};
    const CashFlowController::FlowSides refund = CashFlowController::moneyFlow(1000.0, withRefund);
    QCOMPARE(total(refund.inputs), 1000.0);
    QCOMPARE(total(refund.outputs), 1000.0);
    QCOMPARE(names(refund.outputs), QStringList({QStringLiteral("Rent"), QStringLiteral("Savings")}));
}

void CalcTest::rollingAverageStartsAtTheThirdMonth()
{
    CashFlowController controller;
    const QVariantList timestamps = controller.rollingTimestamps();
    const QVariantList values = controller.rollingValues();
    const QVariantList expenses = controller.expenseValues();

    QCOMPARE(expenses.size(), qsizetype(12));
    QCOMPARE(timestamps.size(), qsizetype(10));
    QCOMPARE(values.size(), qsizetype(10));
    // The first point is the third month, with the mean of the first three.
    const QDate first = QDateTime::fromMSecsSinceEpoch(qint64(controller.minTimeMs())).date();
    QCOMPARE(QDateTime::fromMSecsSinceEpoch(qint64(timestamps.first().toDouble())).date(), first.addMonths(2));
    const double mean = (expenses.at(0).toDouble() + expenses.at(1).toDouble() + expenses.at(2).toDouble()) / 3.0;
    QCOMPARE(values.first().toDouble(), mean);
    QCOMPARE(QDateTime::fromMSecsSinceEpoch(qint64(timestamps.last().toDouble())).date(),
             QDateTime::fromMSecsSinceEpoch(qint64(controller.maxTimeMs())).date());
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

QTEST_GUILESS_MAIN(CalcTest)
#include "calc_test.moc"
