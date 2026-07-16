#include "AssetEntriesTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

AssetEntriesTable::AssetEntriesTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase AssetEntriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool AssetEntriesTable::createTable()
{
    QSqlQuery q(db());
    const bool ok =
        q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS asset_entries ("
            "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  snapshot_id INTEGER NOT NULL,"
            "  category    TEXT    NOT NULL,"
            "  name        TEXT    NOT NULL,"
            "  value       REAL    NOT NULL DEFAULT 0.0"
            ");"
        )) &&
        q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_entries_snapshot"
            " ON asset_entries(snapshot_id);"
        ));

    if (!ok)
        qWarning() << "[AssetEntriesTable] createTable error:" << q.lastError().text();
    return ok;
}

// ── Write ─────────────────────────────────────────────────────────────────────

bool AssetEntriesTable::deleteForSnapshot(int snapshotId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "DELETE FROM asset_entries WHERE snapshot_id = ?;"
    ));
    q.addBindValue(snapshotId);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AssetEntriesTable] deleteForSnapshot error:" << q.lastError().text();
    return ok;
}

bool AssetEntriesTable::insertBatch(int snapshotId,
                                     const QString& category,
                                     const QVector<Data::AssetEntry>& entries)
{
    if (entries.isEmpty()) return true;

    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO asset_entries (snapshot_id, category, name, value)"
        " VALUES (?, ?, ?, ?);"
    ));

    for (const Data::AssetEntry& e : entries) {
        q.addBindValue(snapshotId);
        q.addBindValue(category);
        q.addBindValue(e.name);
        q.addBindValue(e.value);
        if (!q.exec()) {
            qWarning() << "[AssetEntriesTable] insertBatch error:" << q.lastError().text();
            return false;
        }
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

std::optional<Data::FinancialSnapshot>
AssetEntriesTable::loadForSnapshot(int snapshotId, const QDate& date) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT category, name, value"
        " FROM asset_entries"
        " WHERE snapshot_id = ?"
        " ORDER BY id ASC;"
    ));
    q.addBindValue(snapshotId);

    if (!q.exec()) {
        qWarning() << "[AssetEntriesTable] loadForSnapshot error:" << q.lastError().text();
        return std::nullopt;
    }

    Data::FinancialSnapshot snap;
    snap.date = date;

    while (q.next()) {
        const QString cat = q.value(0).toString();
        Data::AssetEntry entry;
        entry.name  = q.value(1).toString();
        entry.value = q.value(2).toDouble();

        if      (cat == QLatin1String("stock")) snap.stocks << entry;
        else if (cat == QLatin1String("coin"))  snap.coins  << entry;
        else if (cat == QLatin1String("metal")) snap.metals << entry;
        else if (cat == QLatin1String("etf"))   snap.etfs   << entry;
    }

    return snap;
}
