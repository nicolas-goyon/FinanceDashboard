#include "AssetEntriesTable.h"
#include "SqlPrepare.h"
#include "models/AssetEntry.h"

#include <QLatin1String>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <QtLogging>
#include <optional>
#include <utility>

AssetEntriesTable::AssetEntriesTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase AssetEntriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool AssetEntriesTable::createTable()
{
    QSqlQuery query(db());
    const bool success =
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS asset_entries ("
            "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  snapshot_id INTEGER NOT NULL,"
            "  category    TEXT    NOT NULL,"
            "  name        TEXT    NOT NULL,"
            "  value       REAL    NOT NULL DEFAULT 0.0"
            ");"
        )) &&
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_entries_snapshot"
            " ON asset_entries(snapshot_id);"
        ));

    if (!success) {
        qWarning() << "[AssetEntriesTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// ── Write ─────────────────────────────────────────────────────────────────────

bool AssetEntriesTable::deleteForSnapshot(int snapshotId)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "DELETE FROM asset_entries WHERE snapshot_id = ?;"
    ))) {
        Sql::logPrepareError(query, "[AssetEntriesTable] deleteForSnapshot");
        return false;
    }
    query.addBindValue(snapshotId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AssetEntriesTable] deleteForSnapshot error:" << query.lastError().text();
    }
    return success;
}

bool AssetEntriesTable::insertBatch(int snapshotId,
                                     const QString& category,
                                     const QVector<Data::AssetEntry>& entries)
{
    if (entries.isEmpty()) { return true; }

    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "INSERT INTO asset_entries (snapshot_id, category, name, value)"
        " VALUES (?, ?, ?, ?);"
    ))) {
        Sql::logPrepareError(query, "[AssetEntriesTable] insertBatch");
        return false;
    }

    for (const Data::AssetEntry& entry : entries) {
        query.addBindValue(snapshotId);
        query.addBindValue(category);
        query.addBindValue(entry.name());
        query.addBindValue(entry.value());
        if (!query.exec()) {
            qWarning() << "[AssetEntriesTable] insertBatch error:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

// ── Read ──────────────────────────────────────────────────────────────────────

std::optional<Data::FinancialSnapshot>
AssetEntriesTable::loadForSnapshot(int snapshotId, const QDate& date) const
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "SELECT category, name, value"
        " FROM asset_entries"
        " WHERE snapshot_id = ?"
        " ORDER BY id ASC;"
    ))) {
        Sql::logPrepareError(query, "[AssetEntriesTable] loadForSnapshot");
        return std::nullopt;
    }
    query.addBindValue(snapshotId);

    if (!query.exec()) {
        qWarning() << "[AssetEntriesTable] loadForSnapshot error:" << query.lastError().text();
        return std::nullopt;
    }

    Data::FinancialSnapshot snap;
    snap.setDate(date);

    while (query.next()) {
        const QString cat = query.value(0).toString();
        Data::AssetEntry entry;
        entry.setName(query.value(1).toString());
        entry.setValue(query.value(2).toDouble());

        if (cat == QLatin1String("stock")) {
            snap.addStock(entry);
        } else if (cat == QLatin1String("coin")) {
            snap.addCoin(entry);
        } else if (cat == QLatin1String("metal")) {
            snap.addMetal(entry);
        } else if (cat == QLatin1String("etf")) {
            snap.addEtf(entry);
        }
    }

    return snap;
}