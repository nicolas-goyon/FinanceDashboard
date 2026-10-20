// calc_test — the calculations behind the texts and charts of the pages.
//
//   #41   Projects: no monthly contribution gives "Never", a project with no
//         gap has its own save-vs-borrow text, and "Still Needed" is never
//         negative

#include "ProjectsController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QTest>
#include <QVariantMap>

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
    controller.addProject(QStringLiteral("Infinite"), std::numeric_limits<double>::infinity(), 6, 0.0, 10.0);
    controller.addProject(QStringLiteral("Far"), 1000.0, 1201, 0.0, 10.0);
    controller.addProject(QStringLiteral("NaN"), 1000.0, 6, std::numeric_limits<double>::quiet_NaN(), 10.0);
    QCOMPARE(controller.projectNames().size(), before + 2);
}

QTEST_GUILESS_MAIN(CalcTest)
#include "calc_test.moc"
