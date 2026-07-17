#ifndef FINANCEDASHBOARD_MODELS_ASSETENTRY_H
#define FINANCEDASHBOARD_MODELS_ASSETENTRY_H

#include <QString>
#include <QVector>
#include <QDate>
#include <QJsonObject>
#include <QJsonArray>

#include <utility>

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
//
// Fields are private (cppcoreguidelines-non-private-member-variables-in-classes
// — this type has behavior via toJson()/fromJson(), unlike the plain aggregate
// structs in Category.h/Account.h/Transaction.h, which have no member
// functions and so aren't flagged). Accessors below are the only call sites
// this affects: AssetEntry.cpp, AssetEntriesTable.cpp, Database.cpp.

struct AssetEntry {
    AssetEntry() = default;
    AssetEntry(QString entryName, double entryValue)
        : m_name(std::move(entryName)), m_value(entryValue)
    {}

    [[nodiscard]] const QString& name() const { return m_name; }
    [[nodiscard]] double value() const { return m_value; }
    void setName(QString entryName) { m_name = std::move(entryName); }
    void setValue(double entryValue) { m_value = entryValue; }

    [[nodiscard]] QJsonObject toJson() const;
    static AssetEntry fromJson(const QJsonObject& obj);

private:
    QString m_name;
    double  m_value{0.0};
};

// ── FinancialSnapshot ─────────────────────────────────────────────────────────

struct FinancialSnapshot {
    FinancialSnapshot() = default;

    [[nodiscard]] const QDate& date() const { return m_date; }
    void setDate(QDate snapshotDate) { m_date = snapshotDate; }

    [[nodiscard]] const QVector<AssetEntry>& stocks() const { return m_stocks; }
    [[nodiscard]] const QVector<AssetEntry>& coins()  const { return m_coins; }
    [[nodiscard]] const QVector<AssetEntry>& metals() const { return m_metals; }
    [[nodiscard]] const QVector<AssetEntry>& etfs()   const { return m_etfs; }

    void setStocks(QVector<AssetEntry> entries) { m_stocks = std::move(entries); }
    void setCoins(QVector<AssetEntry> entries)  { m_coins  = std::move(entries); }
    void setMetals(QVector<AssetEntry> entries) { m_metals = std::move(entries); }
    void setEtfs(QVector<AssetEntry> entries)   { m_etfs   = std::move(entries); }

    void addStock(AssetEntry entry) { m_stocks << std::move(entry); }
    void addCoin(AssetEntry entry)  { m_coins  << std::move(entry); }
    void addMetal(AssetEntry entry) { m_metals << std::move(entry); }
    void addEtf(AssetEntry entry)   { m_etfs   << std::move(entry); }

    [[nodiscard]] QJsonObject toJson() const;
    static FinancialSnapshot fromJson(const QJsonObject& obj);

private:
    QDate               m_date{QDate::currentDate()};
    QVector<AssetEntry> m_stocks;
    QVector<AssetEntry> m_coins;
    QVector<AssetEntry> m_metals;
    QVector<AssetEntry> m_etfs;
};

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_ASSETENTRY_H
