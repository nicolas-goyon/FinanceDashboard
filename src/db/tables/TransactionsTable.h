#ifndef FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_TRANSACTIONSTABLE_H

#include "models/Transaction.h"

#include <optional>
#include <QDate>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
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
//     source_template   TEXT    NOT NULL DEFAULT '',
//     fingerprint       TEXT    NOT NULL DEFAULT ''   -- schema version 2, see below
//   );
//   CREATE INDEX idx_transactions_account ON transactions(account_id);
//   CREATE UNIQUE INDEX idx_transactions_fingerprint
//     ON transactions(account_id, fingerprint);        -- schema version 2
//
// **fingerprint** — the identity of a row as the bank exported it, a SHA-256
// (see fingerprint()): of the bank's reference ("Internal ref." in the
// description) and the debit and credit in cents when the row has one, else
// of the dates, the debit, credit and balance_after in cents, and the
// description. With the UNIQUE index, the same statement
// row can be saved only once per account, so a re-import or an overlapping
// statement adds only the new rows (insertBatch() skips a fingerprint that the
// account has: ON CONFLICT … DO NOTHING; any other failure stops the save).
// Two identical rows in one statement are two transactions: the second one
// has its occurrence number in its hash (fingerprints()), so it is saved too,
// and a re-import still matches each one to one. updateById() keeps the
// fingerprint: after an edit, a re-import of the original statement still
// finds the row. The column and the index are added by schema migration 2
// (Database::upgrade()), not by createTable(), which keeps the base schema.
//
// **No `confidence` column on purpose.** The (future) category-prediction
// model's per-row confidence is a review-screen-only concept
// (Data::ImportedTransaction::confidence, ARCHITECTURE.MD § Statement
// Import) — once the user reviews/fixes a row and saves, only the resulting
// category/subcategory get written here; confidence is discarded, not
// persisted alongside them.
//
// `name` was added after the table already shipped, before schema
// versioning — createTable() runs an `ALTER TABLE ... ADD COLUMN` (guarded by
// a `PRAGMA table_info` check, since SQLite versions before 3.35 don't support
// `ADD COLUMN IF NOT EXISTS`), so that a file from before it reaches the base
// schema. createTable() is the base schema (version 0) and must not change:
// a new column or index is a migration step in Database::upgrade(), never a
// new `ensureColumn()` call here. createTable() runs before the steps, so
// SQL here that used a column added by a step would fail on every old file.
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
    // account over time should accumulate, not overwrite). A row whose
    // fingerprint the account already has is skipped (ON CONFLICT DO
    // NOTHING); any other constraint failure (a NULL amount, say) returns
    // false. A row keeps its `fingerprint` when it has one, else gets the one
    // from fingerprints(txs). `inserted`, when not null, receives the number
    // of rows written.
    bool insertBatch(int accountId, const QVector<Data::Transaction>& txs, int* inserted = nullptr);

    // The fingerprint of a row (see the schema comment above). Two rows with
    // the same dates, amounts in cents, balance after and description have
    // the same fingerprint; `occurrence` 2, 3, … makes the 2nd, 3rd, …
    // identical row of one batch different (1 is the plain fingerprint).
    [[nodiscard]] static QString fingerprint(const Data::Transaction& tx, int occurrence = 1);
    [[nodiscard]] static QString fingerprint(const QString& operationDate, const QString& confirmationDate,
                                             double debit, double credit, double balanceAfter,
                                             const QString& description, int occurrence = 1);
    // The fingerprints of a batch (one statement), in order: the k-th row
    // that has the same plain fingerprint as earlier rows gets occurrence k.
    [[nodiscard]] static QStringList fingerprints(const QVector<Data::Transaction>& txs);
    // The bank's own id of a transaction, from its description: the
    // "Internal ref." of the Sumeria export (the only template so far), or ""
    // when there is none. A template with another kind of id adds it here.
    [[nodiscard]] static QString bankReference(const QString& description);
    // The identity by reference assumes that the bank gives each transaction
    // its own reference. A saved row with the same reference and amount but
    // another operation date breaks that assumption: the import and the
    // upgrade count such rows and link to this issue.
    static constexpr const char* kReusedReferenceIssueUrl =
        "https://github.com/nicolas-goyon/FinanceDashboard/issues/126";

    // The fingerprints that `accountId` already has, each with the operation
    // date of its row (invalid when the row has none).
    [[nodiscard]] QHash<QString, QDate> operationDatesByFingerprint(int accountId) const;

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

    // Renames (#34). The rows store the category and subcategory names as
    // text, not as ids: Database::renameCategory()/renameSubcategory() call
    // these in the same SQL transaction as the rename itself. A subcategory
    // rename changes only the rows of its own category.
    bool renameCategory(const QString& oldName, const QString& newName);
    bool renameSubcategory(const QString& category, const QString& oldName, const QString& newName);

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
