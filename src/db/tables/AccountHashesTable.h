#pragma once

#include "models/Account.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── AccountHashesTable ────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `account_hashes` table — the "is this account
// linked to a statement fingerprint" list (see ARCHITECTURE.MD § Account
// Management / § Statement Import). An account can carry multiple hashes
// (e.g. an auto-derived one from IBAN+BIC, plus manually-added ones for old
// account numbers/formats) — that's the whole point of pulling this out of a
// single `accounts.hash` column into its own table.
//
//   CREATE TABLE account_hashes (
//     id         INTEGER PRIMARY KEY AUTOINCREMENT,
//     account_id INTEGER NOT NULL,
//     hash       TEXT    NOT NULL,
//     source     TEXT    NOT NULL DEFAULT 'manual',  -- 'auto' | 'manual'
//     UNIQUE(account_id, hash)
//   );
//   CREATE INDEX idx_account_hashes_account ON account_hashes(account_id);
//   CREATE INDEX idx_account_hashes_hash    ON account_hashes(hash);

class AccountHashesTable
{
public:
    explicit AccountHashesTable(const QString& connectionName);

    bool createTable();

    // Returns the new row id, or -1 on failure (including a duplicate hash
    // already stored for this account via the UNIQUE(account_id, hash)
    // constraint — callers may treat that as "already linked", not an error).
    int  insert(int accountId, const QString& hash, const QString& source);
    bool remove(int id);
    // Cascade helper for Database::removeAccount().
    bool removeForAccount(int accountId);

    bool existsForAccount(int accountId, const QString& hash) const;

    // All hashes for one account, oldest first (insertion order via row id).
    QVector<Data::AccountHash> allForAccount(int accountId) const;

private:
    QSqlDatabase db() const;
    QString m_connectionName;
};
