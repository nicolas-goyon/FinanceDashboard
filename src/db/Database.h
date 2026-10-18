#ifndef FINANCEDASHBOARD_DB_DATABASE_H
#define FINANCEDASHBOARD_DB_DATABASE_H

#include "db/tables/SnapshotsTable.h"
#include "db/tables/AssetEntriesTable.h"
#include "db/tables/CategoriesTable.h"
#include "db/tables/SubcategoriesTable.h"
#include "db/tables/AccountsTable.h"
#include "db/tables/AccountHashesTable.h"
#include "db/tables/AccountCategoriesTable.h"
#include "db/tables/AccountTypesTable.h"
#include "db/tables/TransactionsTable.h"
#include "db/tables/AccountBalanceCheckpointsTable.h"
#include "db/tables/CategoryRulesTable.h"
#include "models/Category.h"
#include "models/CategoryRule.h"
#include "models/Account.h"
#include "models/Transaction.h"

#include <optional>
#include <QString>
#include <QDate>
#include <QVector>
#include <QSqlDatabase>

// ── Database ──────────────────────────────────────────────────────────────────
//
// Singleton that owns the SQLite connection and orchestrates the table
// classes.  All SQL lives in the table classes; Database only manages
// connection lifetime and cross-table operations (transactions).
//
// Call Database::instance().open(path) once in main() before any tab runs.

class Database
{
public:
    static Database& instance();

    // Open (and initialise) the SQLite file at `path`.
    // Creates tables on first run.  Returns false on failure.
    bool    open(const QString& path);
    [[nodiscard]] bool    isOpen() const;
    [[nodiscard]] QString lastError() const;

    // ── Table accessors (for future direct use by specialised services) ────────
    SnapshotsTable&         snapshots()             { return m_snapshots; }
    AssetEntriesTable&      assetEntries()          { return m_assetEntries; }
    CategoriesTable&        categoriesTable()       { return m_categoriesTable; }
    SubcategoriesTable&     subcategoriesTable()    { return m_subcategoriesTable; }
    AccountsTable&          accountsTable()         { return m_accountsTable; }
    AccountHashesTable&     accountHashesTable()    { return m_accountHashesTable; }
    AccountCategoriesTable& accountCategoriesTable(){ return m_accountCategoriesTable; }
    AccountTypesTable&      accountTypesTable()     { return m_accountTypesTable; }
    TransactionsTable&      transactionsTable()     { return m_transactionsTable; }
    AccountBalanceCheckpointsTable& balanceCheckpointsTable() { return m_balanceCheckpointsTable; }
    CategoryRulesTable&     categoryRulesTable()    { return m_categoryRulesTable; }

    // Raw Qt database handle — needed by QSqlTableModel.
    [[nodiscard]] QSqlDatabase sqlDatabase() const  { return m_db; }

    // ── High-level operations ─────────────────────────────────────────────────

    // Insert or replace the snapshot for its date (runs in a transaction).
    bool saveSnapshot(const Data::FinancialSnapshot& snapshot);

    // Load the snapshot for a specific date, or nullopt if none exists.
    [[nodiscard]] std::optional<Data::FinancialSnapshot> loadSnapshot(const QDate& date) const;

    // Load the most recently stored snapshot, or nullopt.
    [[nodiscard]] std::optional<Data::FinancialSnapshot> loadLatestSnapshot() const;

    // All stored dates, newest first.
    [[nodiscard]] QVector<QDate> listDates() const;

    // ── Categories (see ARCHITECTURE.MD § Category Management) ─────────────────

    // Every category, insertion order, with its subcategories populated.
    [[nodiscard]] QVector<Data::Category> loadCategories() const;

    // Returns the new category's row id, or -1 on failure (DB not open, or a
    // UNIQUE(name) violation — callers check for duplicate names first).
    int  addCategory(const QString& name, const QString& color);
    bool renameCategory(int categoryId, const QString& newName);
    bool setCategoryColor(int categoryId, const QString& color);
    // Deletes the category and all of its subcategories (one transaction).
    bool removeCategory(int categoryId);

    // Returns the new subcategory's row id, or -1 on failure.
    int  addSubcategory(int categoryId, const QString& name, const QString& color);
    bool renameSubcategory(int subcategoryId, const QString& newName);
    bool setSubcategoryColor(int subcategoryId, const QString& color);
    bool removeSubcategory(int subcategoryId);

    // ── Accounts (see ARCHITECTURE.MD § Account Management) ─────────────────────

    // Every account, insertion order, with its hashes populated.
    [[nodiscard]] QVector<Data::Account> loadAccounts() const;

    // Returns the new account's row id, or -1 on failure (DB not open, or a
    // UNIQUE(name) violation — callers check for duplicate names first). No
    // iban/bic/number params — deliberately not stored (security decision,
    // see § Account Management).
    int  addAccount(const QString& name, const QString& type, const QString& category,
                     const QString& currency, const QString& notes);
    bool updateAccount(int accountId, const QString& name, const QString& type, const QString& category,
                        const QString& currency, const QString& notes);
    // Deletes the account and all of its hashes and transactions (one transaction).
    bool removeAccount(int accountId);

    // Returns the new hash row's id, or -1 on failure (including an
    // already-linked duplicate hash for this account).
    int  addAccountHash(int accountId, const QString& hash, const QString& source);
    bool removeAccountHash(int hashId);
    [[nodiscard]] bool accountHasHash(int accountId, const QString& hash) const;

    // ── Account categories (usage buckets: Day-to-day, Savings, …) ──────────────

    [[nodiscard]] QVector<Data::AccountCategory> loadAccountCategories() const;
    int  addAccountCategory(const QString& name, const QString& color);
    bool renameAccountCategory(int accountCategoryId, const QString& newName);
    bool setAccountCategoryColor(int accountCategoryId, const QString& color);
    bool removeAccountCategory(int accountCategoryId);

    // ── Account types (banking products: CCP, Livret A, …) ──────────────────────

    [[nodiscard]] QVector<Data::AccountType> loadAccountTypes() const;
    int  addAccountType(const QString& name);
    bool renameAccountType(int typeId, const QString& newName);
    bool removeAccountType(int typeId);

    // ── Transactions (persisted, categorized — see § Statement Import) ──────────

    // Appends every transaction in the batch to `accountId` (does not clear
    // existing rows — see TransactionsTable::insertBatch()).
    bool insertTransactions(int accountId, const QVector<Data::Transaction>& txs);
    // Saves an imported statement in one SQL transaction: the rows, the
    // account fingerprint link (when `accountHash` is not empty and the
    // account does not have it yet) and the balance checkpoints. If one write
    // fails, nothing is written and lastError() tells which write failed.
    bool saveImport(int accountId, const QVector<Data::Transaction>& txs, const QString& accountHash,
                    const QVector<Data::AccountBalanceCheckpoint>& checkpoints);
    [[nodiscard]] QVector<Data::Transaction> transactionsForAccount(int accountId) const;
    [[nodiscard]] int  transactionCountForAccount(int accountId) const;
    // Edits an already-persisted row in place (by Data::Transaction::id) —
    // see TransactionsTable::updateById() for exactly which fields this
    // touches. Returns false if the DB isn't open or the write failed.
    bool updateTransaction(const Data::Transaction& tx);
    // Deletes every transaction for `accountId` but leaves the account and
    // its hashes/checkpoints in place — the "Clear transactions" action on
    // Database Management (see ARCHITECTURE.MD § Database Management),
    // distinct from removeAccount()'s full cascade below.
    bool clearTransactionsForAccount(int accountId);
    // Most recent transaction for `accountId` on or before `date` — see
    // TransactionsTable::latestOnOrBefore(). Used by
    // SnapshotImportController::checkReconciliation() as the fallback
    // "previously recorded balance" reference when there's no closer
    // balance checkpoint.
    [[nodiscard]] std::optional<Data::Transaction> latestTransactionOnOrBefore(int accountId, const QDate& date) const;
    // Every categorized transaction across every account, oldest first — the
    // category predictor's training set (see TransactionsTable::
    // allCategorized() for why it ignores account boundaries, and
    // src/predict/CategoryPredictor.h for what it does with them).
    [[nodiscard]] QVector<Data::Transaction> allCategorizedTransactions() const;

    // ── Category rules (top rung of category prediction — see
    //    docs/category-prediction.md) ─────────────────────────────────────────

    [[nodiscard]] QVector<Data::CategoryRule> loadCategoryRules() const;
    // "Teach or re-teach": if a rule already exists for this (already
    // normalized — see Data::CategoryRule::pattern) merchant key, it is
    // repointed at the new category/subcategory and its id returned;
    // otherwise a new rule is inserted. Never creates a second rule for the
    // same pattern, so correcting yourself replaces the old instruction
    // instead of leaving two contradictory ones for the predictor to pick
    // between. Returns -1 on failure or if pattern/category is empty.
    int  addCategoryRule(const QString& pattern, const QString& category, const QString& subcategory);
    bool removeCategoryRule(int ruleId);

    // ── Balance checkpoints (the "magic number" anchor points — see
    //    ARCHITECTURE.MD § Balance Checkpoints) ──────────────────────────────

    [[nodiscard]] QVector<Data::AccountBalanceCheckpoint> loadBalanceCheckpoints(int accountId) const;
    // Returns the new checkpoint's row id, or -1 on failure.
    int  addBalanceCheckpoint(int accountId, const QDate& date, double balance, const QString& note);
    bool removeBalanceCheckpoint(int checkpointId);
    // Most recent checkpoint for `accountId` on or before `date`, or
    // nullopt if the account has none (or none dated early enough) — see
    // AccountBalanceCheckpointsTable::latestOnOrBefore().
    [[nodiscard]] std::optional<Data::AccountBalanceCheckpoint> latestCheckpointOnOrBefore(int accountId, const QDate& date) const;

    // ── Whole-database reset (see ARCHITECTURE.MD § Database Management) ────────
    // Drops every table this app owns and recreates them empty, in one
    // transaction. Intended for DatabaseManagementController — lets the
    // developer/user wipe the SQLite file back to a blank slate without
    // deleting the file itself (useful whenever the schema changes during
    // development, and as a user-facing "start over" action).
    bool resetAll();

private:
    static constexpr const char* kConnection = "finance_dashboard";

    Database();

    QSqlDatabase            m_db;
    QString                 m_lastError;
    SnapshotsTable          m_snapshots;
    AssetEntriesTable       m_assetEntries;
    CategoriesTable         m_categoriesTable;
    SubcategoriesTable      m_subcategoriesTable;
    AccountsTable           m_accountsTable;
    AccountHashesTable      m_accountHashesTable;
    AccountCategoriesTable  m_accountCategoriesTable;
    AccountTypesTable       m_accountTypesTable;
    TransactionsTable       m_transactionsTable;
    AccountBalanceCheckpointsTable m_balanceCheckpointsTable;
    CategoryRulesTable      m_categoryRulesTable;
};

#endif // FINANCEDASHBOARD_DB_DATABASE_H