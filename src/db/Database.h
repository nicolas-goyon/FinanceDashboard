#pragma once

#include "db/tables/SnapshotsTable.h"
#include "db/tables/AssetEntriesTable.h"
#include "db/tables/CategoriesTable.h"
#include "db/tables/SubcategoriesTable.h"
#include "models/Category.h"

#include <optional>
#include <QString>
#include <QDate>
#include <QVector>
#include <QSqlDatabase>

// ── Database ──────────────────────────────────────────────────────────────────
//
// Singleton that owns the SQLite connection and orchestrates the table
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
    SnapshotsTable&      snapshots()          { return m_snapshots; }
    AssetEntriesTable&   assetEntries()       { return m_assetEntries; }
    CategoriesTable&     categoriesTable()    { return m_categoriesTable; }
    SubcategoriesTable&  subcategoriesTable() { return m_subcategoriesTable; }

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

    // ── Categories (see ARCHITECTURE.MD § Category Management) ─────────────────

    // Every category, insertion order, with its subcategories populated.
    QVector<Data::Category> loadCategories() const;

    // Returns the new category's row id, or -1 on failure (DB not open, or a
    // UNIQUE(name) violation — callers check for duplicate names first).
    int  addCategory(const QString& name, const QString& color);
    bool renameCategory(int id, const QString& newName);
    bool setCategoryColor(int id, const QString& color);
    // Deletes the category and all of its subcategories (one transaction).
    bool removeCategory(int id);

    // Returns the new subcategory's row id, or -1 on failure.
    int  addSubcategory(int categoryId, const QString& name, const QString& color);
    bool renameSubcategory(int id, const QString& newName);
    bool setSubcategoryColor(int id, const QString& color);
    bool removeSubcategory(int id);

private:
    static constexpr const char* kConnection = "finance_dashboard";

    Database();

    QSqlDatabase        m_db;
    QString             m_lastError;
    SnapshotsTable      m_snapshots;
    AssetEntriesTable   m_assetEntries;
    CategoriesTable     m_categoriesTable;
    SubcategoriesTable  m_subcategoriesTable;
};
