#include "NetWorthController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <QtMinMax>

#include <array>

namespace {

QVariantMap kpi(const QString& label, const QString& value,
                const QString& sub = {}, const QString& color = "#cdd6f4")
{
    return { { "label", label }, { "value", value },
             { "sub", sub }, { "color", color } };
}

constexpr int kMonthsShown = 12;
constexpr double kMaxTotalHeadroom = 1.12;    // 12% headroom above the max total for the chart axis
constexpr double kNetChangeAxisScale = 1.2;   // scales the net-change axis bounds past the actual min/max
constexpr double kNetChangeAxisPadding = 1;   // extra flat padding on the net-change axis bounds
constexpr int kPriorMonthIndex = kMonthsShown - 2; // "a month ago" for the MoM comparison label

} // namespace

NetWorthController::NetWorthController(QObject* parent)
    : QObject(parent)
{
    // MOCK: 12 months ending with the current month, per-type balances (checking/savings/trading)
    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);

    // Per-type balances per month (checking / savings / trading)
    const std::array<double, 12> checking = { 3400, 3150, 3900, 3600, 4100, 3800,
                                              4300, 3950, 4500, 4200, 4650, 4400 };
    const std::array<double, 12> savings  = { 18200, 18700, 19200, 19750, 20300, 20900,
                                              21500, 22150, 22800, 23500, 24250, 25000 };
    const std::array<double, 12> trading  = { 10100, 10600, 10200, 11100, 11700, 11300,
                                              12200, 12900, 12500, 13600, 14300, 15200 };

    double prevTotal = 0;
    QVector<double> totals;

    for (int i = 0; i < kMonthsShown; ++i) {
        const QDate month = firstMonth.addMonths(i);
        const double checkingBalance = checking.at(i);
        const double savingsBalance = checkingBalance + savings.at(i);
        const double tradingBalance = savingsBalance + trading.at(i);

        m_timestamps.append(static_cast<double>(month.startOfDay().toMSecsSinceEpoch()));
        m_checkingBound.append(checkingBalance);
        m_savingsBound.append(savingsBalance);
        m_tradingBound.append(tradingBalance);
        m_monthLabels.append(month.toString("MMM yy"));
        totals.append(tradingBalance);

        if (i > 0) {
            const double diff = tradingBalance - prevTotal;
            m_netGains.append(diff >= 0 ? diff : 0.0);
            m_netLosses.append(diff < 0 ? diff : 0.0);
            m_netChangeMax = qMax(m_netChangeMax, diff);
            m_netChangeMin = qMin(m_netChangeMin, diff);
        } else {
            m_netGains.append(0.0);
            m_netLosses.append(0.0);
        }
        prevTotal = tradingBalance;

        m_maxTotal = qMax(m_maxTotal, tradingBalance);
    }
    m_maxTotal *= kMaxTotalHeadroom;
    m_netChangeMax = (m_netChangeMax * kNetChangeAxisScale) + kNetChangeAxisPadding;
    m_netChangeMin = (m_netChangeMin * kNetChangeAxisScale) - kNetChangeAxisPadding;
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    // ── KPIs ──────────────────────────────────────────────────────────────────
    // MOCK: "Savings Rate" and "Emergency Fund" below are hardcoded display values
    const double net    = totals.at(11);
    const double momAbs = totals.at(11) - totals.at(10);
    const double momPct = momAbs / totals.at(10) * 100.0;
    const bool momUp    = momAbs >= 0;

    m_kpis = {
        kpi("Total Net Worth", Fmt::money(net)),
        kpi("Month over Month",
            QString("%1  (%2%3%)").arg(Fmt::money(momAbs, true))
                                  .arg(momUp ? "+" : "")
                                  .arg(momPct, 0, 'f', 1),
            "vs " + firstMonth.addMonths(kPriorMonthIndex).toString("MMMM"),
            momUp ? "#a6e3a1" : "#f38ba8"),
        kpi("Savings Rate", "28.4 %", "this month", "#a6e3a1"),
        kpi("Emergency Fund", "4.7 months", "of essential expenses"),
    };

    // MOCK: allocation donut (current month, derived from the mock balances above)
    m_allocation = {
        QVariantMap{ { "label", "Checking" }, { "value", checking.at(kMonthsShown - 1) }, { "color", "#89b4fa" } },
        QVariantMap{ { "label", "Savings"  }, { "value", savings.at(kMonthsShown - 1)  }, { "color", "#a6e3a1" } },
        QVariantMap{ { "label", "Trading"  }, { "value", trading.at(kMonthsShown - 1)  }, { "color", "#cba6f7" } },
    };
}
