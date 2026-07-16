#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>

// ── Data::DataPoint / Data::PortfolioSummary ──────────────────────────────────
//
// Structs that map to the portfolio JSON schema.
// Each implements  static T fromJson(const QJsonObject&)
// so they work directly with JsonReader::read<T>().
//
// Example:
//   auto summary = JsonReader::read<Data::PortfolioSummary>("portfolio.json");

namespace Data {

// ── DataPoint ─────────────────────────────────────────────────────────────────
// One entry in the "data" time-series array.
//
//   { "time": "2021-06-10T16:17:45.025688229Z", "value": 0 }

struct DataPoint {
    QString time;       ///< ISO-8601 timestamp string as-is from JSON
    double  value{0.0};

    static DataPoint fromJson(const QJsonObject& obj);
};

// ── PortfolioSummary ──────────────────────────────────────────────────────────
// Top-level portfolio JSON document.
//
//  {
//    "data": [ ... ],
//    "timeframe": "5y",
//    "overall_portfolio_amount": 15000.38,
//    "performance": 15,
//    "gains": 3500.2222,
//    "dividends": 250.15,
//    "buy_amount": 15000.5,
//    "buy_count": 5,
//    "sell_amount": 150.03,
//    "sell_count": 7,
//    "best_asset": ""
//  }

struct PortfolioSummary {
    QVector<DataPoint> data;

    QString timeframe;
    double  overallPortfolioAmount{0.0};
    double  performance{0.0};
    double  gains{0.0};
    double  dividends{0.0};
    double  buyAmount{0.0};
    int     buyCount{0};
    double  sellAmount{0.0};
    int     sellCount{0};
    QString bestAsset;

    static PortfolioSummary fromJson(const QJsonObject& obj);
};

} // namespace Data
