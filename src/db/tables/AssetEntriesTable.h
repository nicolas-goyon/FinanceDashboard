#ifndef FINANCEDASHBOARD_DB_TABLES_ASSETENTRIESTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_ASSETENTRIESTABLE_H

#include "models/AssetEntry.h"

#include <optional>
#include <QString>
#include <QDate>
#include <QVector>
#include <QSqlDatabase>

// ── AssetEntriesTable ─────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `asset_entries` table.
//
//   CREATE TABLE asset_entries (
//     id          INTEGER PRIMARY KEY AUTOINCREMENT,
//     snapshot_id INTEGER NOT NULL,
//     category    TEXT    NOT NULL,   -- 'stock' | 'coin' | 'metal' | 'etf'
//     name        TEXT    NOT NULL,
//     value       REAL    NOT NULL DEFAULT 0.0
//   );
//   CREATE INDEX idx_entries_snapshot ON asset_entries(snapshot_id);
//
// Constructed with a connection name; looks up the live QSqlDatabase at
// call time, so it is safe to construct before Database::open() is called.

class AssetEntriesTable
{
public:
    explicit AssetEntriesTable(QString connectionName);

    // Create the table and its index.  Called once by Database::open().
    bool createTable();

    // Delete all entries that belong to a snapshot.
    // Called before re-inserting on save (full replace semantics).
    bool deleteForSnapshot(int snapshotId);

    // Insert a batch of entries for one snapshot+category in one go.
    bool insertBatch(int snapshotId,
                     const QString& category,
                     const QVector<Data::AssetEntry>& entries);

    // Reconstruct a FinancialSnapshot from the stored rows for a snapshot.
    // Returns nullopt only on a SQL error; an empty snapshot is still returned
    // when there are simply no entries.
    [[nodiscard]] std::optional<Data::FinancialSnapshot> loadForSnapshot(int snapshotId,
                                                            const QDate& date) const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_ASSETENTRIESTABLE_H
