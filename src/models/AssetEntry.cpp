#include "AssetEntry.h"

#include <QJsonValue>

namespace Data {

// ── AssetEntry ────────────────────────────────────────────────────────────────

QJsonObject AssetEntry::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("name")]  = name;
    obj[QStringLiteral("value")] = value;
    return obj;
}

AssetEntry AssetEntry::fromJson(const QJsonObject& obj)
{
    AssetEntry e;
    e.name  = obj.value(QStringLiteral("name")).toString();
    e.value = obj.value(QStringLiteral("value")).toDouble();
    return e;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static QJsonArray entriesToJson(const QVector<AssetEntry>& entries)
{
    QJsonArray arr;
    for (const AssetEntry& e : entries)
        arr.append(e.toJson());
    return arr;
}

static QVector<AssetEntry> entriesFromJson(const QJsonArray& arr)
{
    QVector<AssetEntry> result;
    result.reserve(arr.size());
    for (const QJsonValue& v : arr)
        result << AssetEntry::fromJson(v.toObject());
    return result;
}

// ── FinancialSnapshot ─────────────────────────────────────────────────────────

QJsonObject FinancialSnapshot::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("date")]   = date.toString(Qt::ISODate);
    obj[QStringLiteral("stocks")] = entriesToJson(stocks);
    obj[QStringLiteral("coins")]  = entriesToJson(coins);
    obj[QStringLiteral("metals")] = entriesToJson(metals);
    obj[QStringLiteral("etfs")]   = entriesToJson(etfs);
    return obj;
}

FinancialSnapshot FinancialSnapshot::fromJson(const QJsonObject& obj)
{
    FinancialSnapshot s;
    const QString dateStr = obj.value(QStringLiteral("date")).toString();
    if (!dateStr.isEmpty())
        s.date = QDate::fromString(dateStr, Qt::ISODate);

    s.stocks = entriesFromJson(obj.value(QStringLiteral("stocks")).toArray());
    s.coins  = entriesFromJson(obj.value(QStringLiteral("coins")).toArray());
    s.metals = entriesFromJson(obj.value(QStringLiteral("metals")).toArray());
    s.etfs   = entriesFromJson(obj.value(QStringLiteral("etfs")).toArray());
    return s;
}

} // namespace Data
