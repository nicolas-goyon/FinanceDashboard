#include "SavingsController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QtMinMax>

#include <array>

namespace {
constexpr double kBalanceAxisHeadroom = 1.15;   // 15% headroom above the max balance for the chart axis
constexpr double kInterestAxisHeadroom = 1.25;  // 25% headroom above the max interest for the chart axis
constexpr double kMockAddedThisYear = 4300.0;   // MOCK: hardcoded "Added This Year" KPI display value
constexpr double kLivretACeiling = 22950.0;
constexpr double kLddsCeiling = 12000.0;
constexpr double kAssuranceVieCeiling = 20000.0;
} // namespace

SavingsController::SavingsController(QObject* parent)
    : QObject(parent)
{
    // MOCK: monthly balances (Livret A / LDDS / Assurance Vie)
    const std::array<std::array<double, kMonths>, kAccounts> balances = {{
        { 15200, 15600, 16000, 16450, 16900, 17300, 17750, 18150, 18550, 18900, 19200, 19500 },
        {  8100,  8300,  8500,  8650,  8800,  9000,  9200,  9350,  9500,  9700,  9850, 10000 },
        { 12100, 12250, 12150, 12400, 12600, 12550, 12800, 13000, 13150, 13400, 13650, 13900 },
    }};
    // MOCK: interest / gains per year (2023–2025)
    const std::array<std::array<double, kYears>, kAccounts> interest = {{
        { 380, 465, 510 },
        { 190, 240, 265 },
        { 310, 420, 505 },
    }};

    for (int accIdx = 0; accIdx < kAccounts; ++accIdx) {
        for (int monthIdx = 0; monthIdx < kMonths; ++monthIdx) {
            m_balances.at(accIdx).at(monthIdx) = balances.at(accIdx).at(monthIdx);
            m_balanceMax = qMax(m_balanceMax, balances.at(accIdx).at(monthIdx));
        }
        for (int yearIdx = 0; yearIdx < kYears; ++yearIdx) {
            m_interest.at(accIdx).at(yearIdx) = interest.at(accIdx).at(yearIdx);
            m_interestMax = qMax(m_interestMax, interest.at(accIdx).at(yearIdx));
        }
    }
    m_balanceMax  *= kBalanceAxisHeadroom;
    m_interestMax *= kInterestAxisHeadroom;

    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);
    for (int monthIdx = 0; monthIdx < kMonths; ++monthIdx) {
        m_timestamps.append(static_cast<double>(firstMonth.addMonths(monthIdx)
                                       .startOfDay().toMSecsSinceEpoch()));
    }
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    const int startYear = today.year() - 3;
    m_yearLabels = { QString::number(startYear), QString::number(startYear + 1),
                     QString::number(startYear + 2) };

    // ── KPIs ──────────────────────────────────────────────────────────────────
    // MOCK: "Blended Yield" and "Added This Year" are hardcoded display values
    const double total = balances.at(0).at(kMonths - 1) + balances.at(1).at(kMonths - 1) + balances.at(2).at(kMonths - 1);
    m_kpis = {
        QVariantMap{ { "label", "Total Savings" }, { "value", Fmt::money(total) },
                     { "sub", "" }, { "color", "#cdd6f4" } },
        QVariantMap{ { "label", "Blended Yield" }, { "value", "2.9 %" },
                     { "sub", "across all accounts" }, { "color", "#a6e3a1" } },
        QVariantMap{ { "label", "Added This Year" }, { "value", Fmt::money(kMockAddedThisYear, true) },
                     { "sub", "deposits, excl. interest" }, { "color", "#a6e3a1" } },
    };

    // ── Ceilings / goals ──────────────────────────────────────────────────────
    auto goal = [](const QString& label, double current, double target,
                   const QString& color) {
        return QVariantMap{
            { "label", label },
            { "currentText", Fmt::money(current) },
            { "targetText",  Fmt::money(target) },
            { "pct", qMin(current / target, 1.0) },
            { "color", color },
        };
    };
    m_goals = {
        goal("Livret A — ceiling",       balances.at(0).at(kMonths - 1), kLivretACeiling, "#89b4fa"),
        goal("LDDS — ceiling",           balances.at(1).at(kMonths - 1), kLddsCeiling, "#a6e3a1"),
        goal("Assurance Vie — personal", balances.at(2).at(kMonths - 1), kAssuranceVieCeiling, "#f9e2af"),
    };
}

QVariantList SavingsController::accountValues(int index) const
{
    QVariantList result;
    if (index < 0 || index >= kAccounts) { return result; }
    for (int monthIdx = 0; monthIdx < kMonths; ++monthIdx) {
        result.append(m_balances.at(index).at(monthIdx));
    }
    return result;
}

QVariantList SavingsController::interestValues(int index) const
{
    QVariantList result;
    if (index < 0 || index >= kAccounts) { return result; }
    for (int yearIdx = 0; yearIdx < kYears; ++yearIdx) {
        result.append(m_interest.at(index).at(yearIdx));
    }
    return result;
}
