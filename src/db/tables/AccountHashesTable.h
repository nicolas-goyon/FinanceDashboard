#ifndef FINANCEDASHBOARD_DB_TABLES_ACCOUNTHASHESTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_ACCOUNTHASHESTABLE_H

#include "models/Account.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── AccountHashesTable ────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `account_hashes` table: the account tags
// (#203, see utils/AccountHashing.h and ARCHITECTURE.MD § Account tags). A
// tag is one byte of a slow hash of an IBAN, so many bank accounts have the
// same tag: a tag can be on two accounts, once on each. The import suggests
// an account when exactly one account has the tag.
//
//   CREATE TABLE account_hashes (
//     id         INTEGER PRIMARY KEY AUTOINCREMENT,
//     account_id INTEGER NOT NULL,
//     hash       TEXT    NOT NULL,                   -- the tag: 2 hex characters
//     source     TEXT    NOT NULL DEFAULT 'manual',  -- 'auto' | 'manual'
//     UNIQUE(account_id, hash)
//   );
//   CREATE INDEX idx_account_hashes_account ON account_hashes(account_id);
//   CREATE INDEX idx_account_hashes_hash ON account_hashes(hash);
//
// Schema step 5 (Database::migrateTo5()) made the index on `hash` UNIQUE
// (one account for each 64-character hash, #47). Step 7 removes those hashes
// and makes the index a plain one again.
//
// The tags are stored in lower case (AccountHashing::storedForm()), and
// each method below takes a tag in any case.

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