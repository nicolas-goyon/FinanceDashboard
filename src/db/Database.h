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
#include "db/tables/SettingsTable.h"
#include "models/Category.h"
#include "models/CategoryRule.h"
#include "models/Account.h"
#include "models/Transaction.h"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <cstdint>
#include <functional>
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
    // A message for the last failure, when the method sets one. The write
    // methods refuse an empty name, currency, account hash or hash source, and
    // an invalid checkpoint date, before any write, with a message here (#34,
    // #125).
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
    static constexpr int kSchemaVersion = 6;
    [[nodiscard]] int schemaVersion() const;
    // Empty, unless the last open() upgraded an existing database: then one
    // or more sentences for the user (old and new version, where the backup
    // copy is, how many duplicate transactions were removed, how many rows
    // had an amount rounded to the cent, how many links of an account hash
    // to another account were removed, what happened to the category rules).
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
    // iban/bic/number params — deliberately not stored (product decision,
    // see § Account Management).
    int  addAccount(const QString& name, const QString& type, const QString& category,
                     const QString& currency, const QString& notes);
    bool updateAccount(int accountId, const QString& name, const QString& type, const QString& category,
                        const QString& currency, const QString& notes);
    // Deletes the account and all of its hashes and transactions (one transaction).
    bool removeAccount(int accountId);

    // A hash belongs to one account (#47), in lower case
    // (AccountHashing::storedForm()); these methods take it in any case.
    // addAccountHash() is a hash that the user typed: it must be a
    // fingerprint (AccountHashing::isFingerprint()), and another account must
    // not have it. Returns the new hash row's id, or -1 with a message in
    // lastError() (also when this account already has the hash).
    int  addAccountHash(int accountId, const QString& hash, const QString& source);
    // Links a fingerprint of a statement to `accountId`: added, or moved from
    // the account that had it (the user picked another account at import).
    // `legacyHash`: the fingerprint of the same statement from before #50
    // (AccountHashing::legacyCompute()), removed from the account that has it
    // (#50). One SQL transaction. False with a message in lastError() on
    // failure.
    bool linkAccountHash(int accountId, const QString& hash, const QString& source,
                         const QString& legacyHash = QString());
    bool removeAccountHash(int hashId);
    [[nodiscard]] bool accountHasHash(int accountId, const QString& hash) const;
    // The id of the account that has `hash`, or -1.
    [[nodiscard]] int accountOfHash(const QString& hash) const;
    // The name of an account, for the log (its id when it has none).
    [[nodiscard]] QString accountName(int accountId) const;

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
    // account fingerprint link (when `accountHash` is not empty: added, or
    // moved from another account, see linkAccountHash()) and the balance
    // checkpoints. If one write
    // fails, nothing is written and lastError() tells which write failed.
    // A row that the account already has (same fingerprint, see
    // TransactionsTable) is skipped; `inserted`, when not null, receives the
    // number of rows written.
    // `legacyHash`: the fingerprint of the same statement from before #50
    // (AccountHashing::legacyCompute()); `accountHash` replaces it (#50).
    bool saveImport(int accountId, const QVector<Data::Transaction>& txs, const QString& accountHash,
                    const QVector<Data::AccountBalanceCheckpoint>& checkpoints, int* inserted = nullptr,
                    const QString& legacyHash = QString());
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
    // Last transaction of `accountId` booked on or before `date` — see
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

    // ── Default data (#37) ─────────────────────────────────────────────────────
    // The controllers write their default data the first time that a database
    // is used, and the settings table records it, so that an empty table is
    // not taken for a first run: after "Reset Database", the lists stay empty,
    // across restarts too.
    enum class Defaults : std::uint8_t { Categories, Accounts };
    [[nodiscard]] bool defaultsWritten(Defaults set) const;
    // Runs `write` (the add*() calls of the defaults of `set`; false when one
    // fails) and records the defaults as written, in one SQL transaction. If a
    // write fails, nothing is written or recorded, and the next start tries
    // again. Returns false then (lastError() tells why).
    bool writeDefaults(Defaults set, const std::function<bool()>& write);

    // ── Whole-database reset (see ARCHITECTURE.MD § Database Management) ────────
    // Drops every table this app owns and recreates them empty, in one
    // transaction. Intended for DatabaseManagementController — lets the
    // developer/user wipe the SQLite file back to a blank slate without
    // deleting the file itself (useful whenever the schema changes during
    // development, and as a user-facing "start over" action). The settings
    // table stays, and records that the defaults were written: the
    // controllers do not write them again (#37).
    bool resetAll();

    // ── Copy of another database (the Test app, #130) ──────────────────────────
    // Replaces the open database with a copy of the database at `sourcePath`
    // (the database of the prod app, for "Copy my prod database"), then opens
    // the copy. open() upgrades it when its schema is older, with its backup
    // copy and its upgradeReport(), as at a start of a new version.
    //   - The source is opened read-only, and VACUUM INTO makes a consistent
    //     copy, also while the prod app writes to it. SQLite can leave the
    //     -wal and -shm files of the source next to it; the prod app removes
    //     them when it closes.
    //   - The open database is kept: renamed to <name>.before-copy-<date>.db,
    //     in `keptPath`.
    //   - A source with a newer schema, or that cannot be copied, is refused
    //     before any change. A copy that cannot be opened (a failed upgrade)
    //     is kept as <name>.copy-failed-<date>.db, and the database from
    //     before comes back.
    // Returns false on failure, with the cause in lastError().
    bool replaceWithCopyOf(const QString& sourcePath, QString& keptPath);

private:
    static constexpr const char* kConnection = "finance_dashboard";

    Database();

    // open() without the close() on failure.
    bool openFile(const QString& path);
    // Sets m_openError and m_lastError, logs, returns false.
    bool failOpen(OpenError error, const QString& message);
    // A copy of the open file next to it (VACUUM INTO), before an upgrade.
    bool backUp(const QString& path, QString& backupPath);
    // For replaceWithCopyOf(): a copy of `sourcePath`, opened read-only, in
    // `copyPath` (VACUUM INTO). Refuses a source with a newer schema.
    bool copyReadOnly(const QString& sourcePath, const QString& copyPath);
    // For replaceWithCopyOf(): moves the file `source` to `target`; a
    // failure is logged and set in m_lastError.
    bool moveFile(const QString& source, const QString& target);
    // createTable() of every table class: the base schema (version 0).
    bool createBaseTables();
    // What the migration steps did, for the upgrade report.
    struct UpgradeStats {
        bool droppedAccountNumbers = false; // step 1 removed iban / bic / number
        int  removedDuplicates = 0;         // step 2
        int  removedWithOtherValues = 0;    // of those: another category, label, date…
        int  reusedReferences = 0;          // of those: same bank reference, another operation date
        int  roundedRows = 0;               // step 4: rows with an amount rounded to the cent
        int  removedHashLinks = 0;          // step 5: links of a hash to another account
        int  rewrittenRules = 0;            // step 6: a new pattern
        int  removedGenericRules = 0;       // step 6: the pattern had only generic words
        int  mergedRules = 0;               // step 6: a newer rule has the same pattern and category
        int  conflictingRules = 0;          // step 6: one pattern, different categories: removed
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
    bool migrateTo3();                    // settings table, with the defaults already written
    bool migrateTo4(UpgradeStats& stats); // amounts rounded to the cent (#33)
    // Step 4 for one table: rounds `columns` (REAL amounts) of each row to the
    // cent, and counts the rows that changed.
    bool roundAmountsToCents(const QString& table, const QStringList& columns, int& changedRows);
    bool migrateTo5(UpgradeStats& stats); // account hashes: lower case, one account each (#47)
    bool migrateTo6(UpgradeStats& stats); // category rules: the label keys of #40 and #55
    // Removes the fingerprint from before #50 (logged when it was on
    // another account than `accountId`). Inside the caller's transaction.
    bool removeLegacyHash(int accountId, const QString& legacyHash);
    // linkAccountHash() without its checks of the connection: also used by
    // saveImport(), inside its SQL transaction.
    // The two strings are a hash and a literal source, as in
    // AccountHashesTable::insert().
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    bool linkHashTo(int accountId, const QString& hash, const QString& source);
    // Records in the settings table that the defaults of `set` are written.
    // In the SQL transaction of writeDefaults() or resetAll().
    bool recordDefaults(Defaults set);
    [[nodiscard]] QStringList columnsOf(const QString& table) const;
    // Runs `steps` in one SQL transaction: commit when they return true, roll
    // back otherwise. `what` names the operation in m_lastError and the log;
    // the message keeps what a failed step put in m_lastError. A BEGIN or a
    // COMMIT that fails makes it fail too (#49): inside an open transaction,
    // BEGIN fails, as SQLite has no nested transactions.
    bool inTransaction(const QString& what, const std::function<bool()>& steps);

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
    SettingsTable           m_settingsTable;
};

#endif // FINANCEDASHBOARD_DB_DATABASE_H