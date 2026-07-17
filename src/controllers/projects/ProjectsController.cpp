#include "ProjectsController.h"
#include "utils/Formatter.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtMinMax>
#include <QtNumeric>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)
#include <cmath>

namespace {

QVariantMap kpi(const QString& label, const QString& value,
                const QString& sub = {}, const QString& color = "#cdd6f4")
{
    return { { "label", label }, { "value", value },
             { "sub", sub }, { "color", color } };
}

// Standard annuity formula: monthly payment for principal P, annual rate r
// (fraction), n monthly payments. r == 0 → simple division.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — private helper, both
// call sites below pass local variables already named principal/rate matching
// these params, so a swap isn't a realistic mistake in practice.
double annuityMonthly(double principal, double annualRate, int months)
{
    if (months <= 0) { return 0.0; }
    const double monthlyRate = annualRate / 12.0;
    if (monthlyRate <= 0.0) { return principal / months; }
    return principal * monthlyRate / (1.0 - std::pow(1.0 + monthlyRate, -months));
}

// MOCK seed projects (user can add real ones via addProject(), not persisted).
constexpr double kHouseDepositTarget = 40000;
constexpr double kHouseDepositEarmarked = 12500;
constexpr double kHouseDepositMonthly = 600;
constexpr int    kHouseDepositTargetYears = 3;
constexpr double kCarTarget = 15000;
constexpr int    kCarTargetMonths = 15;
constexpr double kCarEarmarked = 6200;
constexpr double kCarMonthly = 300;
constexpr double kChartMaxHeadroom = 1.15; // 15% headroom above the target/projection for the chart axis

} // namespace

ProjectsController::ProjectsController(QObject* parent)
    : QObject(parent)
{
    const QDate today = QDate::currentDate();

    // MOCK: seed projects (user can add real ones via addProject(), not persisted)
    m_projects = {
        { "House deposit", kHouseDepositTarget, today.addYears(kHouseDepositTargetYears),
          kHouseDepositEarmarked, kHouseDepositMonthly },
        { "New car",       kCarTarget, today.addMonths(kCarTargetMonths), kCarEarmarked, kCarMonthly },
    };
    for (const Project& project : m_projects) {
        m_projectNames.append(project.name);
    }

    rebuildCards();
    recomputeProject();
}

void ProjectsController::rebuildCards()
{
    const QDate today = QDate::currentDate();
    m_projectCards.clear();

    for (const Project& project : m_projects) {
        const int monthsToTarget =
            qMax(1, ((project.targetDate.year() - today.year()) * 12)
                        + project.targetDate.month() - today.month());
        const double projectedAtTarget = project.earmarked + (project.monthly * monthsToTarget);
        const double gap = qMax(0.0, project.target - projectedAtTarget);

        const int monthsToComplete =
            project.monthly > 0 ? static_cast<int>(std::ceil((project.target - project.earmarked) / project.monthly)) : 0;
        const QDate completion = QDate(today.year(), today.month(), 1)
                                     .addMonths(qMax(monthsToComplete, 0));

        m_projectCards.append(QVariantMap{
            { "name",           project.name },
            { "targetText",     Fmt::money(project.target) },
            { "earmarkedText",  Fmt::money(project.earmarked) },
            { "monthlyText",    Fmt::money(project.monthly) + "/mo" },
            { "completionText", completion.toString("MMM yyyy") },
            { "targetDateText", project.targetDate.toString("MMM yyyy") },
            { "progressPct",    qBound(0.0, project.earmarked / project.target, 1.0) },
            { "gapText",        gap > 0 ? Fmt::money(gap) : QStringLiteral("—") },
            { "onTrack",        gap <= 0 },
        });
    }

    emit projectsChanged();
}

void ProjectsController::addProject(const QString& name, double target,
                                    int monthsToTarget, double earmarked,
                                    double monthly)
{
    if (name.trimmed().isEmpty() || target <= 0 || monthsToTarget < 1) {
        return;
    }

    const QDate today = QDate::currentDate();
    m_projects.append(Project{
        name.trimmed(), target,
        QDate(today.year(), today.month(), 1).addMonths(monthsToTarget),
        qMax(0.0, earmarked), qMax(0.0, monthly),
    });
    m_projectNames.append(name.trimmed());

    rebuildCards();

    // Select the new project so the detail view shows it.
    m_currentProject = static_cast<int>(m_projects.size()) - 1;
    recomputeProject();
}

void ProjectsController::setCurrentProject(int index)
{
    if (index < 0 || index >= m_projects.size() || index == m_currentProject) {
        return;
    }
    m_currentProject = index;
    recomputeProject();
}

void ProjectsController::setLoanRatePct(double pct)
{
    if (pct < 0 || qFuzzyCompare(pct, m_loanRatePct)) { return; }
    m_loanRatePct = pct;
    recomputeLoan();
}

void ProjectsController::recomputeProject()
{
    const Project& project = m_projects.at(m_currentProject);
    const QDate today = QDate::currentDate();

    // ── Projection: current earmarked + monthly contribution until target date
    m_projTimestamps.clear();
    m_projValues.clear();

    const int monthsToTarget =
        qMax(1, ((project.targetDate.year() - today.year()) * 12)
                    + project.targetDate.month() - today.month());

    double amount = project.earmarked;
    const double projectedAtTarget = project.earmarked + (project.monthly * monthsToTarget);
    for (int monthIdx = 0; monthIdx <= monthsToTarget; ++monthIdx) {
        const QDate month = QDate(today.year(), today.month(), 1).addMonths(monthIdx);
        m_projTimestamps.append(static_cast<double>(month.startOfDay().toMSecsSinceEpoch()));
        m_projValues.append(amount);
        amount += project.monthly;
    }

    m_targetAmount = project.target;
    m_chartMax   = qMax(project.target, projectedAtTarget) * kChartMaxHeadroom;
    m_minTimeMs  = m_projTimestamps.first().toDouble();
    m_maxTimeMs  = m_projTimestamps.last().toDouble();
    m_fundingGap = qMax(0.0, project.target - projectedAtTarget);

    // Projected completion date at current rate (ignoring the target date)
    const int monthsToComplete =
        project.monthly > 0 ? static_cast<int>(std::ceil((project.target - project.earmarked) / project.monthly)) : 0;
    const QDate completion = QDate(today.year(), today.month(), 1)
                                 .addMonths(qMax(monthsToComplete, 0));

    m_kpis = {
        kpi("Still Needed", Fmt::money(project.target - project.earmarked),
            "of " + Fmt::money(project.target)),
        kpi("Projected Completion", completion.toString("MMMM yyyy"),
            QString("at %1 / month").arg(Fmt::money(project.monthly))),
        kpi("Gap at Target Date",
            m_fundingGap > 0 ? Fmt::money(m_fundingGap) : "None",
            project.targetDate.toString("MMMM yyyy"),
            m_fundingGap > 0 ? "#f38ba8" : "#a6e3a1"),
    };

    emit projectChanged();
    recomputeLoan();
}

void ProjectsController::recomputeLoan()
{
    // Simulate borrowing the funding gap (or a nominal 10 k€ if there is none,
    // so the simulator stays usable).
    const double principal = m_fundingGap > 0 ? m_fundingGap : 10000.0;
    const double rate = m_loanRatePct / 100.0;

    m_loanScenarios.clear();
    for (const int years : { 10, 15, 20 }) {
        const int numPayments = years * 12;
        const double monthly = annuityMonthly(principal, rate, numPayments);
        const double totalCost = monthly * numPayments;
        m_loanScenarios.append(QVariantMap{
            { "label", QString("%1 years @ %2 %").arg(years).arg(m_loanRatePct, 0, 'f', 2) },
            { "monthly",       Fmt::money(monthly) },
            { "totalInterest", Fmt::money(totalCost - principal) },
            { "totalCost",     Fmt::money(totalCost) },
        });
    }

    // "Save longer vs borrow now" — months needed to save the gap vs the
    // interest cost of the middle (15-year) scenario.
    const Project& project = m_projects.at(m_currentProject);
    const int monthsToSaveGap =
        project.monthly > 0 ? static_cast<int>(std::ceil(principal / project.monthly)) : 0;
    const double interest15y =
        (annuityMonthly(principal, rate, 180) * 180) - principal;

    m_saveVsBorrowText =
        QString("Saving the %1 gap at %2/month takes ~%3 more months. "
                "Borrowing it today over 15 years costs %4 in interest.")
            .arg(Fmt::money(principal))
            .arg(Fmt::money(project.monthly))
            .arg(monthsToSaveGap)
            .arg(Fmt::money(interest15y));

    emit loanChanged();
}
