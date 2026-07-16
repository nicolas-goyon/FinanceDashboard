#pragma once

#include "models/Account.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── AccountCategoriesTable ────────────────────────────────────────────────────
//
// Owns all SQL that touches the `account_categories` table — the usage-bucket
// list for accounts (Day-to-day, Savings, Investments, Urgent savings, …),
// user-manageable from AccountsManagePage.qml (see ARCHITECTURE.MD § Account
// Management). Same shape as CategoriesTable, just without subcategories.
//
//   CREATE TABLE account_categories (
//     id    INTEGER PRIMARY KEY AUTOINCREMENT,
//     name  TEXT    NOT NULL UNIQUE,
//     color TEXT    NOT NULL
//   );

class AccountCategoriesTable
{
public:
    explicit AccountCategoriesTable(const QString& connectionName);

    bool createTable();

    // Returns the new row id, or -1 on failure (including a UNIQUE(name)
    // violation — callers check for duplicate names themselves first).
    int  insert(const QString& name, const QString& color);
    bool rename(int id, const QString& newName);
    bool setColor(int id, const QString& color);
    bool remove(int id);

    // All categories, oldest first (insertion order via row id).
    QVector<Data::AccountCategory> all() const;

private:
    QSqlDatabase db() const;
    QString m_connectionName;
};
