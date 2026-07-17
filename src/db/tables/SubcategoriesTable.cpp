#include "SubcategoriesTable.h"
#include "models/Category.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <QtLogging>
#include <utility>

SubcategoriesTable::SubcategoriesTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase SubcategoriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool SubcategoriesTable::createTable()
{
    QSqlQuery query(db());
    const bool success =
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS subcategories ("
            "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  category_id INTEGER NOT NULL,"
            "  name        TEXT    NOT NULL,"
            "  color       TEXT    NOT NULL,"
            "  UNIQUE(category_id, name)"
            ");"
        )) &&
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_subcategories_category"
            " ON subcategories(category_id);"
        ));

    if (!success) {
        qWarning() << "[SubcategoriesTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// ── Write ─────────────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors the table's
// (name, color) column order; every call site passes them in that order.
int SubcategoriesTable::insert(int categoryId, const QString& name, const QString& color)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO subcategories (category_id, name, color) VALUES (?, ?, ?);"
    ));
    query.addBindValue(categoryId);
    query.addBindValue(name);
    query.addBindValue(color);

    if (!query.exec()) {
        qWarning() << "[SubcategoriesTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool SubcategoriesTable::rename(int subcategoryId, const QString& newName)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE subcategories SET name = ? WHERE id = ?;"));
    query.addBindValue(newName);
    query.addBindValue(subcategoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[SubcategoriesTable] rename error:" << query.lastError().text();
    }
    return success;
}

bool SubcategoriesTable::setColor(int subcategoryId, const QString& color)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE subcategories SET color = ? WHERE id = ?;"));
    query.addBindValue(color);
    query.addBindValue(subcategoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[SubcategoriesTable] setColor error:" << query.lastError().text();
    }
    return success;
}

bool SubcategoriesTable::remove(int subcategoryId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM subcategories WHERE id = ?;"));
    query.addBindValue(subcategoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[SubcategoriesTable] remove error:" << query.lastError().text();
    }
    return success;
}

bool SubcategoriesTable::removeForCategory(int categoryId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM subcategories WHERE category_id = ?;"));
    query.addBindValue(categoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[SubcategoriesTable] removeForCategory error:" << query.lastError().text();
    }
    return success;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QVector<Data::Subcategory> SubcategoriesTable::allForCategory(int categoryId) const
{
    QVector<Data::Subcategory> result;
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, name, color FROM subcategories"
        " WHERE category_id = ? ORDER BY id ASC;"
    ));
    query.addBindValue(categoryId);

    if (!query.exec()) {
        qWarning() << "[SubcategoriesTable] allForCategory error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::Subcategory sub;
        sub.id    = query.value(0).toInt();
        sub.name  = query.value(1).toString();
        sub.color = query.value(2).toString();
        result << sub;
    }
    return result;
}