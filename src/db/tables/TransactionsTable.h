#ifndef FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H

#include "models/Transaction.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── TransactionsTable ─────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `transactions` table — persisted, categorized
// transactions linked to an account. First real write path is
// SnapshotImportController::save() (see ARCHITECTURE.MD § Statement Import);
// first read path is the per-account transactions table on
// AccountsManagePage.qml (see § Account Management).
//
//   CREATE TABLE transactions (
//     id                INTEGER PRIMARY KEY AUTOINCREMENT,
//     account_id        INTEGER NOT NULL,
//     confirmation_date TEXT    NOT NULL DEFAULT '',  -- ISO-8601 or ''
//     operation_date    TEXT    NOT NULL DEFAULT '',  -- ISO-8601 or ''
//     description       TEXT    NOT NULL DEFAULT '',
//     debit             REAL    NOT NULL DEFAULT 0,
//     credit            REAL    NOT NULL DEFAULT 0,
//     balance_after     REAL    NOT NULL DEFAULT 0,
//     category          TEXT    NOT NULL DEFAULT '',
//     subcategory       TEXT    NOT NULL DEFAULT '',
//     source_template   TEXT    NOT NULL DEFAULT ''
//   );
//   CREATE INDEX idx_transactions_account ON transactions(account_id);
//
// No update/remove-by-id surface yet — this pass only needs append (on
// import save) and read-all-for-account (for the management page's
// transactions view). Editing/deleting a single persisted transaction is a
// future addition once there's a UI for it.

class TransactionsTable
{
public:
    explicit TransactionsTable(QString connectionName);

    bool createTable();

    // Appends every row in `txs` (does not replace/clear existing rows for
    // this account — this is not the "delete-then-insert" pattern
    // AssetEntriesTable uses, since multiple statement imports for the same
    // account over time should accumulate, not overwrite).
    bool insertBatch(int accountId, const QVector<Data::Transaction>& txs);

    // Cascade helper for Database::removeAccount().
    bool removeForAccount(int accountId);

    // All transactions for one account, most recent operation date first.
    [[nodiscard]] QVector<Data::Transaction> allForAccount(int accountId) const;

    [[nodiscard]] int countForAccount(int accountId) const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H
