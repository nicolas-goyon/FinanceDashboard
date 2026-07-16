#include "PortfolioController.h"
#include "data/DebugData.h"
#include "utils/Formatter.h"

#include <cmath>

PortfolioController::PortfolioController(QObject* parent)
    : QObject(parent)
{
    // MOCK: sample holdings from data/DebugData.h
    const auto holdings = Debug::sampleHoldings();

    double totalMV = 0.0;

    for (const auto& h : holdings) {
        const double marketValue = h.shares * h.currentPrice;
        const double cost        = h.shares * h.avgCost;
        const double pnl         = marketValue - cost;
        const double pnlPct      = (pnl / cost) * 100.0;
        const bool   positive    = pnl >= 0;
        totalMV += marketValue;

        m_rows.append(QVariantMap{
            { "ticker",      h.ticker },
            { "name",        h.name },
            { "shares",      QString::number(h.shares, 'f', 2) },
            { "avgCost",     Fmt::money(h.avgCost) },
            { "price",       Fmt::money(h.currentPrice) },
            { "marketValue", Fmt::money(marketValue) },
            { "pnl",         Fmt::money(pnl, /*showPlus=*/true) },
            { "pnlPct",      QString("%1%2%").arg(positive ? "+" : "-")
                                             .arg(std::abs(pnlPct), 0, 'f', 2) },
            { "positive",    positive },
        });
    }

    m_footerText = QString("Total Market Value:  %1   |   Positions: %2")
                       .arg(Fmt::money(totalMV))
                       .arg(holdings.size());
}
