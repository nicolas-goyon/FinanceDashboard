#pragma once

#include <QString>
#include <QVector>
#include <QDate>
#include <QJsonObject>
#include <QJsonArray>

// ── Data::AssetEntry / Data::FinancialSnapshot ────────────────────────────────
//
// Manual-entry data model for the Entry tab.
// Supports serialisation to/from JSON so snapshots can be saved and reloaded.
//
// JSON shape produced by toJson():
//
//   {
//     "date":    "2024-01-15",
//     "stocks":  [ {"name": "AAPL",    "value": 150.50 }, ... ],
//     "coins":   [ {"name": "BTC",     "value": 42000.0}, ... ],
//     "metals":  [ {"name": "Gold",    "value": 1850.0 }, ... ],
//     "etfs":    [ {"name": "IWDA.AS", "value": 78.30  }, ... ]
//   }

namespace Data {

// ── AssetEntry ────────────────────────────────────────────────────────────────

struct AssetEntry {
    QString name;
    double  value{0.0};

    QJsonObject toJson() const;
    static AssetEntry fromJson(const QJsonObject& obj);
};

// ── FinancialSnapshot ─────────────────────────────────────────────────────────

struct FinancialSnapshot {
    QDate               date{QDate::currentDate()};

    QVector<AssetEntry> stocks;
    QVector<AssetEntry> coins;
    QVector<AssetEntry> metals;
    QVector<AssetEntry> etfs;

    QJsonObject toJson() const;
    static FinancialSnapshot fromJson(const QJsonObject& obj);
};

} // namespace Data
