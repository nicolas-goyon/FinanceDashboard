#include "Database.h"
#include "models/Account.h"
#include "models/AssetEntry.h"
#include "models/Category.h"
#include "models/Transaction.h"

#include <QDate>
#include <QLatin1String>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVector>
#include <QDebug>
#include <QtLogging>
#include <array>
#include <optional>

// ── Singleton ─────────────────────────────────────────────────────────────────

Database::Database()
    : m_snapshots(kConnection)
    , m_assetEntries(kConnection)
    , m_categoriesTable(kConnection)
    , m_subcategoriesTable(kConnection)
    , m_accountsTable(kConnection)
    , m_accountHashesTable(kConnection)
    , m_accountCategoriesTable(kConnection)
    , m_accountTypesTable(kConnection)
    , m_transactionsTable(kConnection)
{}

Database& Database::instance()
{
    static Database database;
    return database;
}

// ── Open / init ───────────────────────────────────────────────────────────────

bool Database::open(const QString& path)
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                      QLatin1String(kConnection));
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "[Database] Cannot open:" << m_lastError;
        return false;
    }

    // WAL mode: better read concurrency, faster writes
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));

    // Each table class creates its own schema
    if (!m_snapshots.createTable()             ||
        !m_assetEntries.createTable()          ||
        !m_categoriesTable.createTable()       ||
        !m_subcategoriesTable.createTable()    ||
        !m_accountCategoriesTable.createTable()||
        !m_accountTypesTable.createTable()     ||
        !m_accountsTable.createTable()         ||
        !m_accountHashesTable.createTable()    ||
        !m_transactionsTable.createTable())
    {
        m_lastError = "Failed to create tables.";
        return false;
    }

    return true;
}

bool    Database::isOpen()     const { return m_db.isOpen(); }
QString Database::lastError()  const { return m_lastError; }

// ── Save ──────────────────────────────────────────────────────────────────────

bool Database::saveSnapshot(const Data::FinancialSnapshot& snapshot)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // 1. Ensure the date row exists
    if (!m_snapshots.upsert(snapshot.date())) {
        m_db.rollback();
        m_lastError = "Could not upsert snapshot date.";
        return false;
    }

    // 2. Resolve the row id
    const auto snapshotId = m_snapshots.idForDate(snapshot.date());
    if (!snapshotId) {
        m_db.rollback();
        m_lastError = "Could not resolve snapshot id.";
        return false;
    }

    // 3. Full replace: delete old entries, insert new ones
    if (!m_assetEntries.deleteForSnapshot(*snapshotId)                              ||
        !m_assetEntries.insertBatch(*snapshotId, "stock", snapshot.stocks())       ||
        !m_assetEntries.insertBatch(*snapshotId, "coin",  snapshot.coins())        ||
        !m_assetEntries.insertBatch(*snapshotId, "metal", snapshot.metals())       ||
        !m_assetEntries.insertBatch(*snapshotId, "etf",   snapshot.etfs()))
    {
        m_db.rollback();
        m_lastError = "Failed to write asset entries.";
        return false;
    }

    m_db.commit();
    return true;
}

// ── Load ──────────────────────────────────────────────────────────────────────

std::optional<Data::FinancialSnapshot>
Database::loadSnapshot(const QDate& date) const
{
    if (!isOpen()) { return std::nullopt; }

    const auto snapshotId = m_snapshots.idForDate(date);
    if (!snapshotId) { return std::nullopt; }

    return m_assetEntries.loadForSnapshot(*snapshotId, date);
}

std::optional<Data::FinancialSnapshot>
Database::loadLatestSnapshot() const
{
    if (!isOpen()) { return std::nullopt; }

    const auto latest = m_snapshots.latest();
    if (!latest) { return std::nullopt; }

    return m_assetEntries.loadForSnapshot(latest->first, latest->second);
}

QVector<QDate> Database::listDates() const
{
    return m_snapshots.allDates();
}

// ── Categories ──────────────────────────────────────────────────────────────────

QVector<Data::Category> Database::loadCategories() const
{
    QVector<Data::Category> cats = m_categoriesTable.all();
    for (Data::Category& cat : cats) {
        cat.subcategories = m_subcategoriesTable.allForCategory(cat.id);
    }
    return cats;
}

int Database::addCategory(const QString& name, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_categoriesTable.insert(name, color);
}

bool Database::renameCategory(int categoryId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_categoriesTable.rename(categoryId, newName);
}

bool Database::setCategoryColor(int categoryId, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_categoriesTable.setColor(categoryId, color);
}

bool Database::removeCategory(int categoryId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // Manual cascade: subcategories first, then the category row itself —
    // same "delete-then-insert" transaction style as saveSnapshot().
    if (!m_subcategoriesTable.removeForCategory(categoryId) || !m_categoriesTable.remove(categoryId)) {
        m_db.rollback();
        m_lastError = "Failed to remove category.";
        return false;
    }

    m_db.commit();
    return true;
}

int Database::addSubcategory(int categoryId, const QString& name, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_subcategoriesTable.insert(categoryId, name, color);
}

bool Database::renameSubcategory(int subcategoryId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.rename(subcategoryId, newName);
}

bool Database::setSubcategoryColor(int subcategoryId, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.setColor(subcategoryId, color);
}

bool Database::removeSubcategory(int subcategoryId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.remove(subcategoryId);
}

// ── Accounts ──────────────────────────────────────────────────────────────────

QVector<Data::Account> Database::loadAccounts() const
{
    QVector<Data::Account> accs = m_accountsTable.all();
    for (Data::Account& acc : accs) {
        acc.hashes = m_accountHashesTable.allForAccount(acc.id);
    }
    return accs;
}

int Database::addAccount(const QString& name, const QString& type, const QString& category,
                          const QString& currency, const QString& notes)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountsTable.insert(name, type, category, currency, notes);
}

bool Database::updateAccount(int accountId, const QString& name, const QString& type, const QString& category,
                              const QString& currency, const QString& notes)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountsTable.update(accountId, name, type, category, currency, notes);
}

bool Database::removeAccount(int accountId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // Manual cascade: hashes and transactions first, then the account row
    // itself — same "delete-then-insert" transaction style as
    // removeCategory()/saveSnapshot().
    if (!m_accountHashesTable.removeForAccount(accountId) ||
        !m_transactionsTable.removeForAccount(accountId)  ||
        !m_accountsTable.remove(accountId)) {
        m_db.rollback();
        m_lastError = "Failed to remove account.";
        return false;
    }

    m_db.commit();
    return true;
}

int Database::addAccountHash(int accountId, const QString& hash, const QString& source)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountHashesTable.insert(accountId, hash, source);
}

bool Database::removeAccountHash(int hashId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountHashesTable.remove(hashId);
}

bool Database::accountHasHash(int accountId, const QString& hash) const
{
    if (!isOpen()) { return false; }
    return m_accountHashesTable.existsForAccount(accountId, hash);
}

// ── Account categories ────────────────────────────────────────────────────────

QVector<Data::AccountCategory> Database::loadAccountCategories() const
{
    return m_accountCategoriesTable.all();
}

int Database::addAccountCategory(const QString& name, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountCategoriesTable.insert(name, color);
}

bool Database::renameAccountCategory(int accountCategoryId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountCategoriesTable.rename(accountCategoryId, newName);
}

bool Database::setAccountCategoryColor(int accountCategoryId, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountCategoriesTable.setColor(accountCategoryId, color);
}

bool Database::removeAccountCategory(int accountCategoryId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountCategoriesTable.remove(accountCategoryId);
}

// ── Account types ─────────────────────────────────────────────────────────────

QVector<Data::AccountType> Database::loadAccountTypes() const
{
    return m_accountTypesTable.all();
}

int Database::addAccountType(const QString& name)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_accountTypesTable.insert(name);
}

bool Database::renameAccountType(int typeId, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountTypesTable.rename(typeId, newName);
}

bool Database::removeAccountType(int typeId)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_accountTypesTable.remove(typeId);
}

// ── Transactions ──────────────────────────────────────────────────────────────

bool Database::insertTransactions(int accountId, const QVector<Data::Transaction>& txs)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_transactionsTable.insertBatch(accountId, txs);
}

QVector<Data::Transaction> Database::transactionsForAccount(int accountId) const
{
    if (!isOpen()) { return {}; }
    return m_transactionsTable.allForAccount(accountId);
}

int Database::transactionCountForAccount(int accountId) const
{
    if (!isOpen()) { return 0; }
    return m_transactionsTable.countForAccount(accountId);
}

// ── Whole-database reset ──────────────────────────────────────────────────────

bool Database::resetAll()
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // Drop every table this app owns (children before parents doesn't
    // actually matter here — no FK constraints are declared anywhere in
    // this schema — but listed in roughly the same order tables were
    // introduced for readability).
    static const std::array<const char*, 9> kTables = {
        "asset_entries", "snapshots",
        "subcategories", "categories",
        "account_hashes", "transactions", "accounts",
        "account_categories", "account_types",
    };

    QSqlQuery query(m_db);
    for (const char* table : kTables) {
        if (!query.exec(QStringLiteral("DROP TABLE IF EXISTS %1;").arg(QLatin1String(table)))) {
            m_db.rollback();
            m_lastError = QStringLiteral("Failed to drop table %1: %2")
                              .arg(QLatin1String(table), query.lastError().text());
            return false;
        }
    }

    // Recreate every table empty (each createTable() is CREATE TABLE IF NOT
    // EXISTS, so this is safe to call again right after dropping them all).
    if (!m_snapshots.createTable()             ||
        !m_assetEntries.createTable()          ||
        !m_categoriesTable.createTable()       ||
        !m_subcategoriesTable.createTable()    ||
        !m_accountCategoriesTable.createTable()||
        !m_accountTypesTable.createTable()     ||
        !m_accountsTable.createTable()         ||
        !m_accountHashesTable.createTable()    ||
        !m_transactionsTable.createTable())
    {
        m_db.rollback();
        m_lastError = "Failed to recreate tables after reset.";
        return false;
    }

    m_db.commit();
    return true;
}
