#pragma once

#include "models/Category.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── SubcategoriesTable ────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `subcategories` table.
//
//   CREATE TABLE subcategories (
//     id          INTEGER PRIMARY KEY AUTOINCREMENT,
//     category_id INTEGER NOT NULL,
//     name        TEXT    NOT NULL,
//     color       TEXT    NOT NULL,
//     UNIQUE(category_id, name)
//   );
//   CREATE INDEX idx_subcategories_category ON subcategories(category_id);
//
// Constructed with a connection name; looks up the live QSqlDatabase at call
// time, so it is safe to construct before Database::open() is called.

class SubcategoriesTable
{
public:
    explicit SubcategoriesTable(const QString& connectionName);

    // Create the table and its index. Called once by Database::open().
    bool createTable();

    // Insert a new subcategory under categoryId. Returns the new row id, or
    // -1 on failure (including the UNIQUE(category_id, name) violation).
    // Callers check for duplicate names themselves first, same convention
    // as CategoriesTable::insert().
    int insert(int categoryId, const QString& name, const QString& color);

    bool rename(int id, const QString& newName);
    bool setColor(int id, const QString& color);
    bool remove(int id);

    // Delete every subcategory belonging to categoryId. Called by
    // Database::removeCategory() before deleting the category row itself —
    // a manual cascade, same "delete-then-insert" transaction style as
    // AssetEntriesTable::deleteForSnapshot().
    bool removeForCategory(int categoryId);

    // All subcategories for one category, oldest first (insertion order).
    QVector<Data::Subcategory> allForCategory(int categoryId) const;

private:
    QSqlDatabase db() const;
    QString m_connectionName;
};
