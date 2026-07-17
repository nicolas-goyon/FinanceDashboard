#include "AssetEntry.h"

#include <QJsonValue>
#include <QVector>
#include <Qt>

namespace Data {

// ── AssetEntry ────────────────────────────────────────────────────────────────

QJsonObject AssetEntry::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("name"),  m_name);
    obj.insert(QStringLiteral("value"), m_value);
    return obj;
}

AssetEntry AssetEntry::fromJson(const QJsonObject& obj)
{
    AssetEntry entry;
    entry.setName(obj.value(QStringLiteral("name")).toString());
    entry.setValue(obj.value(QStringLiteral("value")).toDouble());
    return entry;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {

QJsonArray entriesToJson(const QVector<AssetEntry>& entries)
{
    QJsonArray arr;
    for (const AssetEntry& entry : entries) {
        arr.append(entry.toJson());
    }
    return arr;
}

QVector<AssetEntry> entriesFromJson(const QJsonArray& arr)
{
    QVector<AssetEntry> result;
    result.reserve(arr.size());
    for (const auto& value : arr) {
        result << AssetEntry::fromJson(value.toObject());
    }
    return result;
}

} // namespace

// ── FinancialSnapshot ─────────────────────────────────────────────────────────

QJsonObject FinancialSnapshot::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("date"),   m_date.toString(Qt::ISODate));
    obj.insert(QStringLiteral("stocks"), entriesToJson(m_stocks));
    obj.insert(QStringLiteral("coins"),  entriesToJson(m_coins));
    obj.insert(QStringLiteral("metals"), entriesToJson(m_metals));
    obj.insert(QStringLiteral("etfs"),   entriesToJson(m_etfs));
    return obj;
}

FinancialSnapshot FinancialSnapshot::fromJson(const QJsonObject& obj)
{
    FinancialSnapshot snapshot;
    const QString dateStr = obj.value(QStringLiteral("date")).toString();
    if (!dateStr.isEmpty()) {
        snapshot.setDate(QDate::fromString(dateStr, Qt::ISODate));
    }

    snapshot.setStocks(entriesFromJson(obj.value(QStringLiteral("stocks")).toArray()));
    snapshot.setCoins(entriesFromJson(obj.value(QStringLiteral("coins")).toArray()));
    snapshot.setMetals(entriesFromJson(obj.value(QStringLiteral("metals")).toArray()));
    snapshot.setEtfs(entriesFromJson(obj.value(QStringLiteral("etfs")).toArray()));
    return snapshot;
}

} // namespace Data
