#ifndef FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H

#include "models/Transaction.h"

#include <optional>
#include <QDate>
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
//     name              TEXT    NOT NULL DEFAULT '',  -- short label, extracted/edited at import
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
// **No `confidence` column on purpose.** The (future) category-prediction
// model's per-row confidence is a review-screen-only concept
// (Data::ImportedTransaction::confidence, ARCHITECTURE.MD § Statement
// Import) — once the user reviews/fixes a row and saves, only the resulting
// category/subcategory get written here; confidence is discarded, not
// persisted alongside them.
//
// `name` was added after the table already shipped — createTable() runs an
// `ALTER TABLE ... ADD COLUMN` (guarded by a `PRAGMA table_info` check, since
// SQLite versions before 3.35 don't support `ADD COLUMN IF NOT EXISTS`) so
// existing databases pick it up without a manual migration step.
// `ensureColumn()` is a reusable helper (name+DDL parameters) for exactly
// this — any future additive column should get its own `ensureColumn()` call
// in `createTable()`.
//
// **updateById()** — added for the per-account transactions page's "✎ Edit"
// action (AccountTransactionsPage.qml, see ARCHITECTURE.MD § Account
// Management). Only confirmation_date/operation_date/name/description/
// debit/credit/category/subcategory are editable — `account_id`,
// `balance_after`, and `source_template` are deliberately left alone: moving
// a transaction to a different account or hand-editing the bank-declared
// running balance isn't supported by this UI, and `balance_after` is NOT
// recomputed for later rows when debit/credit changes (a known limitation —
// see ARCHITECTURE.MD § Account Management's "editing breaks balance
// continuity" note). No delete-by-id yet — not requested.

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

    // Every *categorized* transaction across every account, oldest first.
    // Rows with an empty category are excluded in SQL rather than filtered by
    // the caller — they carry no signal and there can be a lot of them.
    //
    // This is the category predictor's entire training set (see
    // src/predict/CategoryPredictor.h): it deliberately ignores account
    // boundaries, because "CARREFOUR MARKET is Groceries" is a fact about the
    // user, not about which of their two checking accounts paid. Ordered
    // oldest-first so the predictor's recurrence detection sees a natural
    // timeline without re-sorting.
    [[nodiscard]] QVector<Data::Transaction> allCategorized() const;

    // Updates confirmation_date/operation_date/name/description/debit/
    // credit/category/subcategory for the row matching `tx.id` — see the
    // class comment above for what's deliberately NOT editable.
    bool updateById(const Data::Transaction& tx);

    // Most recent transaction for this account with operation_date <=
    // `date` (rows with no operation date are never returned), or nullopt —
    // used by SnapshotImportController::checkReconciliation() as a fallback
    // "what did we last record for this account" reference when there's no
    // closer AccountBalanceCheckpoint (see ARCHITECTURE.MD § Statement
    // Import — reconciliation).
    [[nodiscard]] std::optional<Data::Transaction> latestOnOrBefore(int accountId, const QDate& date) const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    // Adds `columnName` to an already-existing `transactions` table if it's
    // not already there (no-op otherwise) — see the schema comment above.
    // `columnDdl` is the column's type+default, e.g. "TEXT NOT NULL DEFAULT ''"
    // or "REAL NOT NULL DEFAULT 0".
    bool ensureColumn(const QString& columnName, const QString& columnDdl);

    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H
