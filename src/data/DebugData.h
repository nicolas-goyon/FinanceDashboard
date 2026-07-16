#pragma once

#include <QString>
#include <QVector>

namespace Debug {

// ── Portfolio holding ─────────────────────────────────────────────────────────
struct Holding {
    QString ticker;
    QString name;
    double  shares;
    double  avgCost;      // USD per share
    double  currentPrice; // USD per share
};

// ── Account summary ───────────────────────────────────────────────────────────
struct AccountSummary {
    double totalValue;
    double totalCost;
    double dayChange;
    double dayChangePct;
    double unrealizedPnL;
    double realizedPnL;
};

// ── Daily performance point ───────────────────────────────────────────────────
struct DailyPoint {
    QString date;   // "YYYY-MM-DD"
    double  value;  // portfolio value in USD
};

// ── Static debug data ─────────────────────────────────────────────────────────
// MOCK: fixed sample holdings, used by legacy PortfolioController
inline QVector<Holding> sampleHoldings()
{
    return {
        { "AAPL",  "Apple Inc.",            15.0,  142.30, 189.45 },
        { "MSFT",  "Microsoft Corp.",       10.0,  255.10, 415.20 },
        { "GOOGL", "Alphabet Inc.",          5.0, 2100.00, 2875.60 },
        { "AMZN",  "Amazon.com Inc.",        8.0,  135.50, 182.10 },
        { "NVDA",  "NVIDIA Corp.",          12.0,  220.75, 875.30 },
        { "TSLA",  "Tesla Inc.",             6.0,  195.00, 248.70 },
        { "BRK.B", "Berkshire Hathaway B",  20.0,  310.40, 358.90 },
    };
}

// MOCK: fixed sample account summary, used by legacy OverviewController
inline AccountSummary sampleSummary()
{
    return {
        /* totalValue    */ 98'432.50,
        /* totalCost     */ 72'185.00,
        /* dayChange     */ +1'243.80,
        /* dayChangePct  */ +1.28,
        /* unrealizedPnL */ +2'6247.50,
        /* realizedPnL   */ +4'812.30,
    };
}

// MOCK: fixed daily performance series — not currently consumed by any
// registered controller, kept for future use/tests
inline QVector<DailyPoint> samplePerformance()
{
    return {
        { "2024-01-02", 72000.00 },
        { "2024-02-01", 74500.00 },
        { "2024-03-01", 76200.00 },
        { "2024-04-01", 73800.00 },
        { "2024-05-01", 79100.00 },
        { "2024-06-03", 82450.00 },
        { "2024-07-01", 85300.00 },
        { "2024-08-01", 88000.00 },
        { "2024-09-02", 84600.00 },
        { "2024-10-01", 91200.00 },
        { "2024-11-01", 94800.00 },
        { "2024-12-02", 98432.50 },
    };
}

} // namespace Debug
