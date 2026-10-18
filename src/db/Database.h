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

#include <QHash>
#include <QSet>
#include <QStringList>

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
    // Creates tables on first run, then upgrades the schema to the version of
    // this app (see upgrade()). Returns false on failure. After a failure the
    // connection is closed (isOpen() is false), so nothing writes to a file
    // that this version of the app cannot use; openError() tells why.
    bool    open(const QString& path);
    // Closes the connection; open() can then open another file (tests).
    void    close();
    [[nodiscard]] bool    isOpen() const;
    [[nodiscard]] QString lastError() const;

    // Why the last open() failed, for the hint of the fatal start-up step
    // (StartupChecks). NewerSchema and UpgradeFailed leave the file as it was.
    enum class OpenError {
        None,
        CannotOpen,    // the file cannot be opened, read or created
        NewerSchema,   // a newer version of the app saved the file
        UpgradeFailed, // the copy or the upgrade failed; the file is unchanged
    };
    [[nodiscard]] OpenError openError() const { return m_openError; }

    // ── Schema version (PRAGMA user_version) ─────────────────────────────────
    // The schema version that this app writes. Each version after 0 is one
    // ordered migration step in upgrade().
    static constexpr int kSchemaVersion = 2;
    [[nodiscard]] int schemaVersion() const;
    // Empty, unless the last open() upgraded an existing database: then one
    // or two sentences for the user (old and new version, where the backup
    // copy is, how many duplicate transactions were removed).
    [[nodiscard]] QString upgradeReport() const { return m_upgradeReport; }

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
    // A row that the account already has (same fingerprint, see
    // TransactionsTable) is skipped; `inserted`, when not null, receives the
    // number of rows written.
    bool saveImport(int accountId, const QVector<Data::Transaction>& txs, const QString& accountHash,
                    const QVector<Data::AccountBalanceCheckpoint>& checkpoints, int* inserted = nullptr);
    // The fingerprints of the rows that `accountId` already has, each with
    // the operation date of its row (see TransactionsTable).
    [[nodiscard]] QHash<QString, QDate> transactionFingerprints(int accountId) const;
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

    // open() without the close() on failure.
    bool openFile(const QString& path);
    // Sets m_openError and m_lastError, logs, returns false.
    bool failOpen(OpenError error, const QString& message);
    // A copy of the open file next to it (VACUUM INTO), before an upgrade.
    bool backUp(const QString& path, QString& backupPath);
    // createTable() of every table class: the base schema (version 0).
    bool createBaseTables();
    // What the migration steps did, for the upgrade report.
    struct UpgradeStats {
        bool droppedAccountNumbers = false; // step 1 removed iban / bic / number
        int  removedDuplicates = 0;         // step 2
        int  removedWithOtherValues = 0;    // of those: another category, label, date…
        int  reusedReferences = 0;          // of those: same bank reference, another operation date
    };
    // The iban / bic / number columns that the accounts table still has.
    [[nodiscard]] QStringList accountNumberColumns() const;
    // Empties those columns in the copy that backUp() made, and compacts it,
    // so that the copy keeps no account number (maintainer decision).
    bool clearAccountNumbersInCopy(const QString& backupPath, const QStringList& columns);
    // The migration steps from `from` + 1 up to kSchemaVersion, then
    // PRAGMA user_version. Runs in the caller's SQL transaction, so that the
    // file is either at kSchemaVersion or unchanged.
    bool upgrade(int from, UpgradeStats& stats);
    bool migrateTo1(UpgradeStats& stats); // accounts: drop iban / bic / number
    bool migrateTo2(UpgradeStats& stats); // transactions: fingerprint + UNIQUE index
    [[nodiscard]] QStringList columnsOf(const QString& table) const;

    QSqlDatabase            m_db;
    QString                 m_lastError;
    QString                 m_upgradeReport;
    OpenError               m_openError = OpenError::None;
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