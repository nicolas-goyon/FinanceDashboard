#ifndef FINANCEDASHBOARD_DB_TABLES_ACCOUNTSTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_ACCOUNTSTABLE_H

#include "models/Account.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── AccountsTable ─────────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `accounts` table (see ARCHITECTURE.MD §
// Account Management). Mirrors CategoriesTable's shape. Hashes are NOT
// populated here — Database::loadAccounts() fills those in from
// AccountHashesTable, same composition pattern as
// Database::loadCategories()/loadSnapshot().
//
// Deliberately does NOT store iban/bic/account-number (product decision —
// see ARCHITECTURE.MD § Account Management; these were MOCK fields used only
// to demo hash derivation, removed once that became a real, sensitive
// concern). Matching a statement to an account is purely via the
// account_hashes table now.
//
//   CREATE TABLE accounts (
//     id       INTEGER PRIMARY KEY AUTOINCREMENT,
//     name     TEXT    NOT NULL UNIQUE,
//     type     TEXT    NOT NULL DEFAULT '',
//     category TEXT    NOT NULL DEFAULT '',
//     currency TEXT    NOT NULL DEFAULT 'EUR',
//     notes    TEXT    NOT NULL DEFAULT ''
//   );

class AccountsTable
{
public:
    explicit AccountsTable(QString connectionName);

    bool createTable();

    // Returns the new row id, or -1 on failure (including a UNIQUE(name)
    // violation — callers check for duplicate names themselves first).
    int insert(const QString& name, const QString& type, const QString& category,
               const QString& currency, const QString& notes);

    // Full-row update (all fields at once — no per-field setters, unlike
    // CategoriesTable, since AccountController's edit dialog always submits
    // every field together).
    bool update(int accountId, const QString& name, const QString& type, const QString& category,
                const QString& currency, const QString& notes);

    // Deletes the account row itself only — does NOT cascade to hashes or
    // transactions. Callers must delete those first (see
    // Database::removeAccount(), which does all three in one transaction).
    bool remove(int accountId);

    // Renames (#34). The accounts store their type and category names as
    // text, not as ids: Database::renameAccountType()/renameAccountCategory()
    // call these in the same SQL transaction as the rename itself.
    bool renameType(const QString& oldName, const QString& newName);
    bool renameCategory(const QString& oldName, const QString& newName);

    // All accounts, oldest first (insertion order via row id). Hashes left empty.
    [[nodiscard]] QVector<Data::Account> all() const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_AC