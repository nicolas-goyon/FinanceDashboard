#ifndef FINANCEDASHBOARD_DB_TABLES_ACCOUNTBALANCECHECKPOINTSTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_ACCOUNTBALANCECHECKPOINTSTABLE_H

#include "models/Account.h"

#include <optional>
#include <QDate>
#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── AccountBalanceCheckpointsTable ─────────────────────────────────────────────
//
// Owns all SQL that touches the `account_balance_checkpoints` table — the
// "magic number" anchor points from the product request ("at this time the
// amount of money in the bank was that"), used because full transaction
// history since account opening usually can't be imported. See
// ARCHITECTURE.MD § Balance Checkpoints / § Statement Import
// (reconciliation). An account can carry several checkpoints over time.
//
//   CREATE TABLE account_balance_checkpoints (
//     id         INTEGER PRIMARY KEY AUTOINCREMENT,
//     account_id INTEGER NOT NULL,
//     date       TEXT    NOT NULL,             -- ISO-8601
//     balance    REAL    NOT NULL DEFAULT 0,
//     note       TEXT    NOT NULL DEFAULT ''
//   );
//   CREATE INDEX idx_balance_checkpoints_account ON account_balance_checkpoints(account_id);
//
// No DB-level cascade (no foreign keys anywhere in this schema, consistent
// with every other table) — Database::removeAccount() calls
// removeForAccount() manually, same pattern as AccountHashesTable/
// TransactionsTable.

class AccountBalanceCheckpointsTable
{
public:
    explicit AccountBalanceCheckpointsTable(QString connectionName);

    bool createTable();

    // Returns the new row id, or -1 on failure.
    int  insert(int accountId, const QDate& date, double balance, const QString& note);
    bool remove(int checkpointId);
    // Cascade helper for Database::removeAccount().
    bool removeForAccount(int accountId);

    // All checkpoints for one account, oldest date first.
    [[nodiscard]] QVector<Data::AccountBalanceCheckpoint> allForAccount(int accountId) const;

    // Most recent checkpoint for this account with date <= `date`, or
    // nullopt if none (e.g. the account has no checkpoint at all, or every
    // checkpoint is dated after `date` — a checkpoint from the future is
    // never used to validate a past/present statement).
    [[nodiscard]] std::optional<Data::AccountBalanceCheckpoint> latestOnOrBefore(int accountId, const QDate& date) const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_ACCOUNTBALANCECHECKPOINTSTABLE_H
