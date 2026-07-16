#include "ProjectsController.h"
#include "utils/Formatter.h"

#include <QDateTime>
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
double annuityMonthly(double principal, double annualRate, int months)
{
    if (months <= 0) return 0.0;
    const double i = annualRate / 12.0;
    if (i <= 0.0) return principal / months;
    return principal * i / (1.0 - std::pow(1.0 + i, -months));
}

} // namespace

ProjectsController::ProjectsController(QObject* parent)
    : QObject(parent)
{
    const QDate today = QDate::currentDate();

    // MOCK: seed projects (user can add real ones via addProject(), not persisted)
    m_projects = {
        { "House deposit", 40000, today.addYears(3), 12500, 600 },
        { "New car",       15000, today.addMonths(15), 6200, 300 },
    };
    for (const Project& p : m_projects)
        m_projectNames.append(p.name);

    rebuildCards();
    recomputeProject();
}

void ProjectsController::rebuildCards()
{
    const QDate today = QDate::currentDate();
    m_projectCards.clear();

    for (const Project& p : m_projects) {
        const int monthsToTarget =
            qMax(1, int((p.targetDate.year() - today.year()) * 12
                        + p.targetDate.month() - today.month()));
        const double projectedAtTarget = p.earmarked + p.monthly * monthsToTarget;
        const double gap = qMax(0.0, p.target - projectedAtTarget);

        const int monthsToComplete =
            p.monthly > 0 ? int(std::ceil((p.target - p.earmarked) / p.monthly)) : 0;
        const QDate completion = QDate(today.year(), today.month(), 1)
                                     .addMonths(qMax(monthsToComplete, 0));

        m_projectCards.append(QVariantMap{
            { "name",           p.name },
            { "targetText",     Fmt::money(p.target) },
            { "earmarkedText",  Fmt::money(p.earmarked) },
            { "monthlyText",    Fmt::money(p.monthly) + "/mo" },
            { "completionText", completion.toString("MMM yyyy") },
            { "targetDateText", p.targetDate.toString("MMM yyyy") },
            { "progressPct",    qBound(0.0, p.earmarked / p.target, 1.0) },
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
    if (name.trimmed().isEmpty() || target <= 0 || monthsToTarget < 1)
        return;

    const QDate today = QDate::currentDate();
    m_projects.append(Project{
        name.trimmed(), target,
        QDate(today.year(), today.month(), 1).addMonths(monthsToTarget),
        qMax(0.0, earmarked), qMax(0.0, monthly),
    });
    m_projectNames.append(name.trimmed());

    rebuildCards();

    // Select the new project so the detail view shows it.
    m_currentProject = m_projects.size() - 1;
    recomputeProject();
}

void ProjectsController::setCurrentProject(int index)
{
    if (index < 0 || index >= m_projects.size() || index == m_currentProject)
        return;
    m_currentProject = index;
    recomputeProject();
}

void ProjectsController::setLoanRatePct(double pct)
{
    if (pct < 0 || qFuzzyCompare(pct, m_loanRatePct)) return;
    m_loanRatePct = pct;
    recomputeLoan();
}

void ProjectsController::recomputeProject()
{
    const Project& p = m_projects[m_currentProject];
    const QDate today = QDate::currentDate();

    // ── Projection: current earmarked + monthly contribution until target date
    m_projTimestamps.clear();
    m_projValues.clear();

    const int monthsToTarget =
        qMax(1, int((p.targetDate.year() - today.year()) * 12
                    + p.targetDate.month() - today.month()));

    double amount = p.earmarked;
    double projectedAtTarget = p.earmarked + p.monthly * monthsToTarget;
    for (int m = 0; m <= monthsToTarget; ++m) {
        const QDate month = QDate(today.year(), today.month(), 1).addMonths(m);
        m_projTimestamps.append(double(month.startOfDay().toMSecsSinceEpoch()));
        m_projValues.append(amount);
        amount += p.monthly;
    }

    m_targetAmount = p.target;
    m_chartMax   = qMax(p.target, projectedAtTarget) * 1.15;
    m_minTimeMs  = m_projTimestamps.first().toDouble();
    m_maxTimeMs  = m_projTimestamps.last().toDouble();
    m_fundingGap = qMax(0.0, p.target - projectedAtTarget);

    // Projected completion date at current rate (ignoring the target date)
    const int monthsToComplete =
        p.monthly > 0 ? int(std::ceil((p.target - p.earmarked) / p.monthly)) : 0;
    const QDate completion = QDate(today.year(), today.month(), 1)
                                 .addMonths(qMax(monthsToComplete, 0));

    m_kpis = {
        kpi("Still Needed", Fmt::money(p.target - p.earmarked),
            "of " + Fmt::money(p.target)),
        kpi("Projected Completion", completion.toString("MMMM yyyy"),
            QString("at %1 / month").arg(Fmt::money(p.monthly))),
        kpi("Gap at Target Date",
            m_fundingGap > 0 ? Fmt::money(m_fundingGap) : "None",
            p.targetDate.toString("MMMM yyyy"),
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
    for (int years : { 10, 15, 20 }) {
        const int n = years * 12;
        const double monthly = annuityMonthly(principal, rate, n);
        const double totalCost = monthly * n;
        m_loanScenarios.append(QVariantMap{
            { "label", QString("%1 years @ %2 %").arg(years).arg(m_loanRatePct, 0, 'f', 2) },
            { "monthly",       Fmt::money(monthly) },
            { "totalInterest", Fmt::money(totalCost - principal) },
            { "totalCost",     Fmt::money(totalCost) },
        });
    }

    // "Save longer vs borrow now" — months needed to save the gap vs the
    // interest cost of the middle (15-year) scenario.
    const Project& p = m_projects[m_currentProject];
    const int monthsToSaveGap =
        p.monthly > 0 ? int(std::ceil(principal / p.monthly)) : 0;
    const double interest15y =
        annuityMonthly(principal, rate, 180) * 180 - principal;

    m_saveVsBorrowText =
        QString("Saving the %1 gap at %2/month takes ~%3 more months. "
                "Borrowing it today over 15 years costs %4 in interest.")
            .arg(Fmt::money(principal))
            .arg(Fmt::money(p.monthly))
            .arg(monthsToSaveGap)
            .arg(Fmt::money(interest15y));

    emit loanChanged();
}
