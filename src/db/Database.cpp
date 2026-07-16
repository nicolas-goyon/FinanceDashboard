#include "Database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// ── Singleton ─────────────────────────────────────────────────────────────────

Database::Database()
    : m_snapshots(kConnection)
    , m_assetEntries(kConnection)
    , m_categoriesTable(kConnection)
    , m_subcategoriesTable(kConnection)
{}

Database& Database::instance()
{
    static Database db;
    return db;
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
    if (!m_snapshots.createTable()         ||
        !m_assetEntries.createTable()      ||
        !m_categoriesTable.createTable()   ||
        !m_subcategoriesTable.createTable())
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
    if (!m_snapshots.upsert(snapshot.date)) {
        m_db.rollback();
        m_lastError = "Could not upsert snapshot date.";
        return false;
    }

    // 2. Resolve the row id
    const auto id = m_snapshots.idForDate(snapshot.date);
    if (!id) {
        m_db.rollback();
        m_lastError = "Could not resolve snapshot id.";
        return false;
    }

    // 3. Full replace: delete old entries, insert new ones
    if (!m_assetEntries.deleteForSnapshot(*id)                              ||
        !m_assetEntries.insertBatch(*id, "stock", snapshot.stocks)         ||
        !m_assetEntries.insertBatch(*id, "coin",  snapshot.coins)          ||
        !m_assetEntries.insertBatch(*id, "metal", snapshot.metals)         ||
        !m_assetEntries.insertBatch(*id, "etf",   snapshot.etfs))
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
    if (!isOpen()) return std::nullopt;

    const auto id = m_snapshots.idForDate(date);
    if (!id) return std::nullopt;

    return m_assetEntries.loadForSnapshot(*id, date);
}

std::optional<Data::FinancialSnapshot>
Database::loadLatestSnapshot() const
{
    if (!isOpen()) return std::nullopt;

    const auto latest = m_snapshots.latest();
    if (!latest) return std::nullopt;

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
    for (Data::Category& cat : cats)
        cat.subcategories = m_subcategoriesTable.allForCategory(cat.id);
    return cats;
}

int Database::addCategory(const QString& name, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return -1; }
    return m_categoriesTable.insert(name, color);
}

bool Database::renameCategory(int id, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_categoriesTable.rename(id, newName);
}

bool Database::setCategoryColor(int id, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_categoriesTable.setColor(id, color);
}

bool Database::removeCategory(int id)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }

    m_db.transaction();

    // Manual cascade: subcategories first, then the category row itself —
    // same "delete-then-insert" transaction style as saveSnapshot().
    if (!m_subcategoriesTable.removeForCategory(id) || !m_categoriesTable.remove(id)) {
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

bool Database::renameSubcategory(int id, const QString& newName)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.rename(id, newName);
}

bool Database::setSubcategoryColor(int id, const QString& color)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.setColor(id, color);
}

bool Database::removeSubcategory(int id)
{
    if (!isOpen()) { m_lastError = "Database not open."; return false; }
    return m_subcategoriesTable.remove(id);
}
