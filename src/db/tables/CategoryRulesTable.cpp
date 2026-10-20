#include "CategoryRulesTable.h"
#include "SqlPrepare.h"
#include "SqlText.h"
#include "models/CategoryRule.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <QtLogging>
#include <utility>

namespace {
// Column indices for the SELECT column list shared by all() and byPattern().
constexpr int kColId = 0;
constexpr int kColPattern = 1;
constexpr int kColCategory = 2;
constexpr int kColSubcategory = 3;
constexpr int kColPriority = 4;
} // namespace

CategoryRulesTable::CategoryRulesTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase CategoryRulesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool CategoryRulesTable::createTable()
{
    QSqlQuery query(db());
    const bool success = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS category_rules ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  pattern     TEXT    NOT NULL UNIQUE,"
        "  category    TEXT    NOT NULL,"
        "  subcategory TEXT    NOT NULL DEFAULT '',"
        "  priority    INTEGER NOT NULL DEFAULT 0"
        ");"
    ));
    if (!success) {
        qWarning() << "[CategoryRulesTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// ── Write ─────────────────────────────────────────────────────────────────────

int CategoryRulesTable::insert(const Data::CategoryRule& rule)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "INSERT INTO category_rules (pattern, category, subcategory, priority)"
        " VALUES (?, ?, ?, ?);"
    ))) {
        Sql::logPrepareError(query, "[CategoryRulesTable] insert");
        return -1;
    }
    query.addBindValue(Sql::text(rule.pattern));
    query.addBindValue(Sql::text(rule.category));
    query.addBindValue(Sql::text(rule.subcategory));
    query.addBindValue(rule.priority);

    if (!query.exec()) {
        qWarning() << "[CategoryRulesTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors the table's
// (category, subcategory) column order; every call site passes them in that
// order, same convention as CategoriesTable::insert().
bool CategoryRulesTable::updateTarget(int ruleId, const QString& category, const QString& subcategory)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "UPDATE category_rules SET category = ?, subcategory = ? WHERE id = ?;"
    ))) {
        Sql::logPrepareError(query, "[CategoryRulesTable] updateTarget");
        return false;
    }
    query.addBindValue(Sql::text(category));
    query.addBindValue(Sql::text(subcategory));
    query.addBindValue(ruleId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[CategoryRulesTable] updateTarget error:" << query.lastError().text();
    }
    return success;
}

bool CategoryRulesTable::remove(int ruleId)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("DELETE FROM category_rules WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[CategoryRulesTable] remove");
        return false;
    }
    query.addBindValue(ruleId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[CategoryRulesTable] remove error:" << query.lastError().text();
    }
    return success;
}

bool CategoryRulesTable::renameCategory(const QString& oldName, const QString& newName)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE category_rules SET category = ? WHERE category = ?;"))) {
        Sql::logPrepareError(query, "[CategoryRulesTable] renameCategory");
        return false;
    }
    query.addBindValue(newName);
    query.addBindValue(oldName);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[CategoryRulesTable] renameCategory error:" << query.lastError().text();
    }
    return success;
}

bool CategoryRulesTable::renameSubcategory(const QString& category, const QString& oldName, const QString& newName)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE category_rules SET subcategory = ? WHERE category = ? AND subcategory = ?;"))) {
        Sql::logPrepareError(query, "[CategoryRulesTable] renameSubcategory");
        return false;
    }
    query.addBindValue(newName);
    query.addBindValue(category);
    query.addBindValue(oldName);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[CategoryRulesTable] renameSubcategory error:" << query.lastError().text();
    }
    return success;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QVector<Data::CategoryRule> CategoryRulesTable::all() const
{
    QVector<Data::CategoryRule> result;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT id, pattern, category, subcategory, priority"
            " FROM category_rules ORDER BY id ASC;"))) {
        qWarning() << "[CategoryRulesTable] all error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::CategoryRule rule;
        rule.id          = query.value(kColId).toInt();
        rule.pattern     = query.value(kColPattern).toString();
        rule.category    = query.value(kColCategory).toString();
        rule.subcategory = query.value(kColSubcategory).toString();
        rule.priority    = query.value(kColPriority).toInt();
        result.append(rule);
    }
    return result;
}

Data::CategoryRule CategoryRulesTable::byPattern(const QString& pattern) const
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "SELECT id, pattern, category, subcategory, priority"
        " FROM category_rules WHERE pattern = ? LIMIT 1;"
    ))) {
        Sql::logPrepareError(query, "[CategoryRulesTable] byPattern");
        return {};
    }
    query.addBindValue(pattern);

    if (!query.exec()) {
        qWarning() << "[CategoryRulesTable] byPattern error:" << query.lastError().text();
        return {};
    }
    if (!query.next()) {
        return {}; // no rule for this merchant yet — id stays -1
    }

    Data::CategoryRule rule;
    rule.id          = query.value(kColId).toInt();
    rule.pattern     = query.value(kColPattern).toString();
    rule.category    = query.value(kColCategory).toString();
    rule.subcategory = query.value(kColSubcategory).toString();
    rule.priority    = query.value(kColPriority).toInt();
    return rule;
}
