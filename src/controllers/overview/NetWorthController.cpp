#include "NetWorthController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>

namespace {

QVariantMap kpi(const QString& label, const QString& value,
                const QString& sub = {}, const QString& color = "#cdd6f4")
{
    return { { "label", label }, { "value", value },
             { "sub", sub }, { "color", color } };
}

} // namespace

NetWorthController::NetWorthController(QObject* parent)
    : QObject(parent)
{
    // MOCK: 12 months ending with the current month, per-type balances (checking/savings/trading)
    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);

    // Per-type balances per month (checking / savings / trading)
    const double checking[12] = { 3400, 3150, 3900, 3600, 4100, 3800,
                                  4300, 3950, 4500, 4200, 4650, 4400 };
    const double savings[12]  = { 18200, 18700, 19200, 19750, 20300, 20900,
                                  21500, 22150, 22800, 23500, 24250, 25000 };
    const double trading[12]  = { 10100, 10600, 10200, 11100, 11700, 11300,
                                  12200, 12900, 12500, 13600, 14300, 15200 };

    double prevTotal = 0;
    QVector<double> totals;

    for (int i = 0; i < 12; ++i) {
        const QDate m = firstMonth.addMonths(i);
        const double b1 = checking[i];
        const double b2 = b1 + savings[i];
        const double b3 = b2 + trading[i];

        m_timestamps.append(double(m.startOfDay().toMSecsSinceEpoch()));
        m_checkingBound.append(b1);
        m_savingsBound.append(b2);
        m_tradingBound.append(b3);
        m_monthLabels.append(m.toString("MMM yy"));
        totals.append(b3);

        if (i > 0) {
            const double diff = b3 - prevTotal;
            m_netGains.append(diff >= 0 ? diff : 0.0);
            m_netLosses.append(diff < 0 ? diff : 0.0);
            m_netChangeMax = qMax(m_netChangeMax, diff);
            m_netChangeMin = qMin(m_netChangeMin, diff);
        } else {
            m_netGains.append(0.0);
            m_netLosses.append(0.0);
        }
        prevTotal = b3;

        m_maxTotal = qMax(m_maxTotal, b3);
    }
    m_maxTotal *= 1.12;
    m_netChangeMax = m_netChangeMax * 1.2 + 1;
    m_netChangeMin = m_netChangeMin * 1.2 - 1;
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    // ── KPIs ──────────────────────────────────────────────────────────────────
    // MOCK: "Savings Rate" and "Emergency Fund" below are hardcoded display values
    const double net    = totals[11];
    const double momAbs = totals[11] - totals[10];
    const double momPct = momAbs / totals[10] * 100.0;
    const bool momUp    = momAbs >= 0;

    m_kpis = {
        kpi("Total Net Worth", Fmt::money(net)),
        kpi("Month over Month",
            QString("%1  (%2%3%)").arg(Fmt::money(momAbs, true))
                                  .arg(momUp ? "+" : "")
                                  .arg(momPct, 0, 'f', 1),
            "vs " + firstMonth.addMonths(10).toString("MMMM"),
            momUp ? "#a6e3a1" : "#f38ba8"),
        kpi("Savings Rate", "28.4 %", "this month", "#a6e3a1"),
        kpi("Emergency Fund", "4.7 months", "of essential expenses"),
    };

    // MOCK: allocation donut (current month, derived from the mock balances above)
    m_allocation = {
        QVariantMap{ { "label", "Checking" }, { "value", checking[11] }, { "color", "#89b4fa" } },
        QVariantMap{ { "label", "Savings"  }, { "value", savings[11]  }, { "color", "#a6e3a1" } },
        QVariantMap{ { "label", "Trading"  }, { "value", trading[11]  }, { "color", "#cba6f7" } },
    };
}
