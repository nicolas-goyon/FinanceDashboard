#pragma once

#include "db/tables/SnapshotsTable.h"
#include "db/tables/AssetEntriesTable.h"
#include "db/tables/CategoriesTable.h"
#include "db/tables/SubcategoriesTable.h"
#include "db/tables/AccountsTable.h"
#include "db/tables/AccountHashesTable.h"
#include "db/tables/AccountCategoriesTable.h"
#include "db/tables/AccountTypesTable.h"
#include "db/tables/TransactionsTable.h"
#include "models/Category.h"
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
    bool    isOpen() const;
    QString lastError() const;

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

    // Raw Qt database handle — needed by QSqlTableModel.
    QSqlDatabase sqlDatabase() const  { return m_db; }

    // ── High-level operations ─────────────────────────────────────────────────

    // Insert or replace the snapshot for its date (runs in a transaction).
    bool saveSnapshot(const Data::FinancialSnapshot& snapshot);

    // Load the snapshot for a specific date, or nullopt if none exists.
    std::optional<Data::FinancialSnapshot> loadSnapshot(const QDate& date) const;

    // Load the most recently stored snapshot, or nullopt.
    std::optional<Data::FinancialSnapshot> loadLatestSnapshot() const;

    // All stored dates, newest first.
    QVector<QDate> listDates() const;

    // ── Categories (see ARCHITECTURE.MD § Category Management) ─────────────────

    // Every category, insertion order, with its subcategories populated.
    QVector<Data::Category> loadCategories() const;

    // Returns the new category's row id, or -1 on failure (DB not open, or a
    // UNIQUE(name) violation — callers check for duplicate names first).
    int  addCategory(const QString& name, const QString& color);
    bool renameCategory(int id, const QString& newName);
    bool setCategoryColor(int id, const QString& color);
    // Deletes the category and all of its subcategories (one transaction).
    bool removeCategory(int id);

    // Returns the new subcategory's row id, or -1 on failure.
    int  addSubcategory(int categoryId, const QString& name, const QString& color);
    bool renameSubcategory(int id, const QString& newName);
    bool setSubcategoryColor(int id, const QString& color);
    bool removeSubcategory(int id);

    // ── Accounts (see ARCHITECTURE.MD § Account Management) ─────────────────────

    // Every account, insertion order, with its hashes populated.
    QVector<Data::Account> loadAccounts() const;

    // Returns the new account's row id, or -1 on failure (DB not open, or a
    // UNIQUE(name) violation — callers check for duplicate names first). No
    // iban/bic/number params — deliberately not stored (security decision,
    // see § Account Management).
    int  addAccount(const QString& name, const QString& type, const QString& category,
                     const QString& currency, const QString& notes);
    bool updateAccount(int id, const QString& name, const QString& type, const QString& category,
                        const QString& currency, const QString& notes);
    // Deletes the account and all of its hashes and transactions (one transaction).
    bool removeAccount(int id);

    // Returns the new hash row's id, or -1 on failure (including an
    // already-linked duplicate hash for this account).
    int  addAccountHash(int accountId, const QString& hash, const QString& source);
    bool removeAccountHash(int id);
    bool accountHasHash(int accountId, const QString& hash) const;

    // ── Account categories (usage buckets: Day-to-day, Savings, …) ──────────────

    QVector<Data::AccountCategory> loadAccountCategories() const;
    int  addAccountCategory(const QString& name, const QString& color);
    bool renameAccountCategory(int id, const QString& newName);
    bool setAccountCategoryColor(int id, const QString& color);
    bool removeAccountCategory(int id);

    // ── Account types (banking products: CCP, Livret A, …) ──────────────────────

    QVector<Data::AccountType> loadAccountTypes() const;
    int  addAccountType(const QString& name);
    bool renameAccountType(int id, const QString& newName);
    bool removeAccountType(int id);

    // ── Transactions (persisted, categorized — see § Statement Import) ──────────

    // Appends every transaction in the batch to `accountId` (does not clear
    // existing rows — see TransactionsTable::insertBatch()).
    bool insertTransactions(int accountId, const QVector<Data::Transaction>& txs);
    QVector<Data::Transaction> transactionsForAccount(int accountId) const;
    int  transactionCountForAccount(int accountId) const;

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
};
