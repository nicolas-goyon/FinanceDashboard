#include "InvestmentsController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QtMinMax>

#include <array>

namespace {
constexpr int kMonthsShown = 12;
constexpr double kValueAxisHeadroom = 1.12;  // 12% headroom above the max value for the chart axis
constexpr double kPerfAxisScale = 1.3;       // scales the perf-% axis bounds past the actual min/max
constexpr double kPerfAxisPadding = 0.5;     // extra flat padding on the perf-% axis bounds
constexpr double kAllocEtfPct = 55.0;
constexpr double kAllocTechStocksPct = 25.0;
constexpr double kAllocCryptoPct = 10.0;
constexpr double kAllocCashPct = 10.0;
} // namespace

InvestmentsController::InvestmentsController(QObject* parent)
    : QObject(parent)
{
    // MOCK: 12 months of portfolio value + assumed monthly deposit
    const std::array<double, kMonthsShown> portfolio = { 10100, 10600, 10200, 11100, 11700, 11300,
                                               12200, 12900, 12500, 13600, 14300, 15200 };
    const double monthlyDeposit = 250.0;
    const double depositsStart  = 9250.0;

    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);

    double deposits = depositsStart;
    double prev = 0;

    for (int monthIdx = 0; monthIdx < kMonthsShown; ++monthIdx) {
        deposits += monthlyDeposit;
        const QDate month = firstMonth.addMonths(monthIdx);

        m_timestamps.append(static_cast<double>(month.startOfDay().toMSecsSinceEpoch()));
        m_portfolioValues.append(portfolio.at(monthIdx));
        m_depositValues.append(deposits);
        m_monthLabels.append(month.toString("MMM"));
        m_valueMax = qMax(m_valueMax, portfolio.at(monthIdx));

        if (monthIdx > 0) {
            // The deposit of the month is not a gain: performance is the change
            // of the value without it (#56).
            const double pct = (portfolio.at(monthIdx) - prev - monthlyDeposit) / prev * 100.0;
            m_perfGains.append(pct >= 0 ? pct : 0.0);
            m_perfLosses.append(pct < 0 ? pct : 0.0);
            m_perfMax = qMax(m_perfMax, pct);
            m_perfMin = qMin(m_perfMin, pct);
        } else {
            m_perfGains.append(0.0);
            m_perfLosses.append(0.0);
        }
        prev = portfolio.at(monthIdx);
    }
    m_valueMax *= kValueAxisHeadroom;
    m_perfMax = (m_perfMax * kPerfAxisScale) + kPerfAxisPadding;
    m_perfMin = (m_perfMin * kPerfAxisScale) - kPerfAxisPadding;
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    // ── KPIs ──────────────────────────────────────────────────────────────────
    const double invested = deposits;
    const double pnl      = portfolio.at(11) - invested;
    const double retPct   = pnl / invested * 100.0;

    m_kpis = {
        QVariantMap{ { "label", "Total Invested" }, { "value", Fmt::money(invested) },
                     { "sub", "cumulative deposits" }, { "color", "#cdd6f4" } },
        QVariantMap{ { "label", "Unrealized P&L" }, { "value", Fmt::money(pnl, true) },
                     { "sub", "value minus deposits" },
                     { "color", pnl >= 0 ? "#a6e3a1" : "#f38ba8" } },
        QVariantMap{ { "label", "Simple Return" },
                     { "value", QString("%1%2 %").arg(retPct >= 0 ? "+" : "")
                                                 .arg(retPct, 0, 'f', 1) },
                     { "sub", "on deposited capital" },
                     { "color", retPct >= 0 ? "#a6e3a1" : "#f38ba8" } },
    };

    // MOCK: allocation donut (static breakdown)
    m_allocation = {
        QVariantMap{ { "label", "ETF World" },  { "value", kAllocEtfPct }, { "color", "#89b4fa" } },
        QVariantMap{ { "label", "Tech stocks" },{ "value", kAllocTechStocksPct }, { "color", "#cba6f7" } },
        QVariantMap{ { "label", "Crypto" },     { "value", kAllocCryptoPct }, { "color", "#fab387" } },
        QVariantMap{ { "label", "Cash" },       { "value", kAllocCashPct }, { "color", "#a6e3a1" } },
    };
}
