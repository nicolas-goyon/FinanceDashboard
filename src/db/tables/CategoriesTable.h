#pragma once

#include "models/Category.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── CategoriesTable ───────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `categories` table.
//
//   CREATE TABLE categories (
//     id    INTEGER PRIMARY KEY AUTOINCREMENT,
//     name  TEXT    NOT NULL UNIQUE,
//     color TEXT    NOT NULL
//   );
//
// Constructed with a connection name; looks up the live QSqlDatabase at call
// time via QSqlDatabase::database(connectionName), so it is safe to construct
// before Database::open() is called. Mirrors SnapshotsTable's shape.

class CategoriesTable
{
public:
    explicit CategoriesTable(const QString& connectionName);

    // Create the table. Called once by Database::open().
    bool createTable();

    // Insert a new category. Returns the new row id, or -1 on failure
    // (including a UNIQUE(name) violation). Callers are expected to check
    // for duplicate names themselves first — same convention
    // CategoryController already used for its in-memory list.
    int insert(const QString& name, const QString& color);

    bool rename(int id, const QString& newName);
    bool setColor(int id, const QString& color);

    // Deletes the category row itself only — does NOT cascade to
    // subcategories. Callers must delete those first (see
    // Database::removeCategory(), which does both in one transaction).
    bool remove(int id);

    // All categories, oldest first (insertion order via row id).
    // Subcategories are NOT populated here — Database::loadCategories()
    // fills those in from SubcategoriesTable.
    QVector<Data::Category> all() const;

private:
    QSqlDatabase db() const;
    QString m_connectionName;
};
