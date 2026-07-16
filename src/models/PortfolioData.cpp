#include "PortfolioData.h"

#include <QJsonArray>
#include <QJsonValue>

namespace Data {

// ── DataPoint ─────────────────────────────────────────────────────────────────

DataPoint DataPoint::fromJson(const QJsonObject& obj)
{
    DataPoint dp;
    dp.time  = obj.value(QStringLiteral("time")).toString();
    dp.value = obj.value(QStringLiteral("value")).toDouble();
    return dp;
}

// ── PortfolioSummary ──────────────────────────────────────────────────────────

PortfolioSummary PortfolioSummary::fromJson(const QJsonObject& obj)
{
    PortfolioSummary s;

    // Parse the time-series array
    const QJsonArray arr = obj.value(QStringLiteral("data")).toArray();
    s.data.reserve(arr.size());
    for (const QJsonValue& entry : arr)
        s.data << DataPoint::fromJson(entry.toObject());

    s.timeframe               = obj.value(QStringLiteral("timeframe")).toString();
    s.overallPortfolioAmount  = obj.value(QStringLiteral("overall_portfolio_amount")).toDouble();
    s.performance             = obj.value(QStringLiteral("performance")).toDouble();
    s.gains                   = obj.value(QStringLiteral("gains")).toDouble();
    s.dividends               = obj.value(QStringLiteral("dividends")).toDouble();
    s.buyAmount               = obj.value(QStringLiteral("buy_amount")).toDouble();
    s.buyCount                = obj.value(QStringLiteral("buy_count")).toInt();
    s.sellAmount              = obj.value(QStringLiteral("sell_amount")).toDouble();
    s.sellCount               = obj.value(QStringLiteral("sell_count")).toInt();
    s.bestAsset               = obj.value(QStringLiteral("best_asset")).toString();

    return s;
}

} // namespace Data
