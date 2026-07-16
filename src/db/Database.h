#pragma once

#include "db/tables/SnapshotsTable.h"
#include "db/tables/AssetEntriesTable.h"

#include <optional>
#include <QString>
#include <QDate>
#include <QVector>
#include <QSqlDatabase>

// ── Database ──────────────────────────────────────────────────────────────────
//
// Singleton that owns the SQLite connection and orchestrates the two table
// classes.  All SQL lives in the table classes; Database only manages
// connection lifetime and cross-table operations (transactions).
//
// Call Database::instance().open(path) once in main() before any tab runs.

class Database
{
public:
    static Database& instance();

    // Open (and initialise) the SQLite file at `path`.
    // Creates tables on first run.  Returns false on failure.
    bool    open(const QString& path);
    bool    isOpen() const;
    QString lastError() const;

    // ── Table accessors (for future direct use by specialised services) ────────
    SnapshotsTable&    snapshots()    { return m_snapshots; }
    AssetEntriesTable& assetEntries() { return m_assetEntries; }

    // Raw Qt database handle — needed by QSqlTableModel.
    QSqlDatabase sqlDatabase() const  { return m_db; }

    // ── High-level operations ─────────────────────────────────────────────────

    // Insert or replace the snapshot for its date (runs in a transaction).
    bool saveSnapshot(const Data::FinancialSnapshot& snapshot);

    // Load the snapshot for a specific date, or nullopt if none exists.
    std::optional<Data::FinancialSnapshot> loadSnapshot(const QDate& date) const;

    // Load the most recently stored snapshot, or nullopt.
    std::optional<Data::FinancialSnapshot> loadLatestSnapshot() const;

    // All stored dates, newest first.
    QVector<QDate> listDates() const;

private:
    static constexpr const char* kConnection = "finance_dashboard";

    Database();

    QSqlDatabase      m_db;
    QString           m_lastError;
    SnapshotsTable    m_snapshots;
    AssetEntriesTable m_assetEntries;
};
