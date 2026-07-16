#pragma once

#include "models/Account.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── AccountTypesTable ─────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `account_types` table — the banking/
// financial product list for accounts (CCP, Livret A, Assurance Vie, …),
// user-manageable from AccountTypesCategoriesPage.qml (see ARCHITECTURE.MD §
// Account Management). Same shape as AccountCategoriesTable, just without a
// `color` column — types aren't used for badge coloring, categories are.
//
//   CREATE TABLE account_types (
//     id    INTEGER PRIMARY KEY AUTOINCREMENT,
//     name  TEXT    NOT NULL UNIQUE
//   );

class AccountTypesTable
{
public:
    explicit AccountTypesTable(const QString& connectionName);

    bool createTable();

    // Returns the new row id, or -1 on failure (including a UNIQUE(name)
    // violation — callers check for duplicate names themselves first).
    int  insert(const QString& name);
    bool rename(int id, const QString& newName);
    bool remove(int id);

    // All types, oldest first (insertion order via row id).
    QVector<Data::AccountType> all() const;

private:
    QSqlDatabase db() const;
    QString m_connectionName;
};
