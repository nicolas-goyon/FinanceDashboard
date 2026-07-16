#include "InvestmentsController.h"
#include "utils/Formatter.h"

#include <QDate>
#include <QDateTime>

InvestmentsController::InvestmentsController(QObject* parent)
    : QObject(parent)
{
    // MOCK: 12 months of portfolio value + assumed monthly deposit
    const double portfolio[12] = { 10100, 10600, 10200, 11100, 11700, 11300,
                                   12200, 12900, 12500, 13600, 14300, 15200 };
    const double monthlyDeposit = 250.0;
    const double depositsStart  = 9250.0;

    const QDate today = QDate::currentDate();
    const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-11);

    double deposits = depositsStart;
    double prev = 0;

    for (int m = 0; m < 12; ++m) {
        deposits += monthlyDeposit;
        const QDate month = firstMonth.addMonths(m);

        m_timestamps.append(double(month.startOfDay().toMSecsSinceEpoch()));
        m_portfolioValues.append(portfolio[m]);
        m_depositValues.append(deposits);
        m_monthLabels.append(month.toString("MMM"));
        m_valueMax = qMax(m_valueMax, portfolio[m]);

        if (m > 0) {
            const double pct = (portfolio[m] - prev) / prev * 100.0;
            m_perfGains.append(pct >= 0 ? pct : 0.0);
            m_perfLosses.append(pct < 0 ? pct : 0.0);
            m_perfMax = qMax(m_perfMax, pct);
            m_perfMin = qMin(m_perfMin, pct);
        } else {
            m_perfGains.append(0.0);
            m_perfLosses.append(0.0);
        }
        prev = portfolio[m];
    }
    m_valueMax *= 1.12;
    m_perfMax = m_perfMax * 1.3 + 0.5;
    m_perfMin = m_perfMin * 1.3 - 0.5;
    m_minTimeMs = m_timestamps.first().toDouble();
    m_maxTimeMs = m_timestamps.last().toDouble();

    // ── KPIs ──────────────────────────────────────────────────────────────────
    const double invested = deposits;
    const double pnl      = portfolio[11] - invested;
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
        QVariantMap{ { "label", "ETF World" },  { "value", 55.0 }, { "color", "#89b4fa" } },
        QVariantMap{ { "label", "Tech stocks" },{ "value", 25.0 }, { "color", "#cba6f7" } },
        QVariantMap{ { "label", "Crypto" },     { "value", 10.0 }, { "color", "#fab387" } },
        QVariantMap{ { "label", "Cash" },       { "value", 10.0 }, { "color", "#a6e3a1" } },
    };
}
