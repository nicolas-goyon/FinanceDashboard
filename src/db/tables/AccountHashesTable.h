#ifndef FINANCEDASHBOARD_DB_TABLES_ACCOUNTHASHESTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_ACCOUNTHASHESTABLE_H

#include "models/Account.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── AccountHashesTable ────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `account_hashes` table — the "is this account
// linked to a statement fingerprint" list (see ARCHITECTURE.MD § Account
// Management / § Statement Import). An account can carry multiple hashes
// (e.g. one for each export format of its bank), but a hash belongs to one
// account only (#47): the import suggests that account.
//
//   CREATE TABLE account_hashes (
//     id         INTEGER PRIMARY KEY AUTOINCREMENT,
//     account_id INTEGER NOT NULL,
//     hash       TEXT    NOT NULL,
//     source     TEXT    NOT NULL DEFAULT 'manual',  -- 'auto' | 'manual'
//     UNIQUE(account_id, hash)
//   );
//   CREATE INDEX idx_account_hashes_account ON account_hashes(account_id);
//   CREATE UNIQUE INDEX idx_account_hashes_hash ON account_hashes(hash);  -- schema version 5
//
// createTable() is the base schema: its index on `hash` is not UNIQUE.
// Schema step 5 (Database::migrateTo5()) puts the hashes in lower case,
// keeps the most recent link of a hash that several accounts had, and
// replaces that index with a UNIQUE one of the same name.
//
// The hashes are stored in lower case (AccountHashing::storedForm()), and
// each method below takes a hash in any case.

class AccountHashesTable
{
public:
    explicit AccountHashesTable(QString connectionName);

    bool createTable();

    // Returns the new row id, or -1 on failure (including a hash that an
    // account already has: the UNIQUE index on `hash`).
    int  insert(int accountId, const QString& hash, const QString& source);
    bool remove(int hashId);
    // Removes `hash` from the account that has it (no row: nothing to do).
    bool removeHash(const QString& hash);
    // Cascade helper for Database::removeAccount().
    bool removeForAccount(int accountId);

    [[nodiscard]] bool existsForAccount(int accountId, const QString& hash) const;
    // The id of the account that has `hash`, or -1 (none, or a failed read).
    [[nodiscard]] int accountOf(const QString& hash) const;
    // Gives `hash` to `accountId`, with `source`: the link of the earlier
    // account goes (#47). False on failure.
    bool moveToAccount(const QString& hash, int accountId, const QString& source);

    // All hashes for one account, oldest first (insertion order via row id).
    [[nodiscard]] QVector<Data::AccountHash> allForAccount(int accountId) const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_ACCOUNTHASHESTABL