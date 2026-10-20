#include "CategoriesTable.h"
#include "SqlPrepare.h"
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

CategoriesTable::CategoriesTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase CategoriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool CategoriesTable::createTable()
{
    QSqlQuery query(db());
    const bool success = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS categories ("
        "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name  TEXT    NOT NULL UNIQUE,"
        "  color TEXT    NOT NULL"
        ");"
    ));
    if (!success) {
        qWarning() << "[CategoriesTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// ── Write ─────────────────────────────────────────────────────────────────────

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors the table's
// (name, color) column order; every call site passes them in that order.
int CategoriesTable::insert(const QString& name, const QString& color)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "INSERT INTO categories (name, color) VALUES (?, ?);"
    ))) {
        Sql::logPrepareError(query, "[CategoriesTable] insert");
        return -1;
    }
    query.addBindValue(name);
    query.addBindValue(color);

    if (!query.exec()) {
        qWarning() << "[CategoriesTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool CategoriesTable::rename(int categoryId, const QString& newName)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE categories SET name = ? WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[CategoriesTable] rename");
        return false;
    }
    query.addBindValue(newName);
    query.addBindValue(categoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[CategoriesTable] rename error:" << query.lastError().text();
    }
    return success;
}

bool CategoriesTable::setColor(int categoryId, const QString& color)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE categories SET color = ? WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[CategoriesTable] setColor");
        return false;
    }
    query.addBindValue(color);
    query.addBindValue(categoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[CategoriesTable] setColor error:" << query.lastError().text();
    }
    return success;
}

bool CategoriesTable::remove(int categoryId)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("DELETE FROM categories WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[CategoriesTable] remove");
        return false;
    }
    query.addBindValue(categoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[CategoriesTable] remove error:" << query.lastError().text();
    }
    return success;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QVector<Data::Category> CategoriesTable::all() const
{
    QVector<Data::Category> result;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral("SELECT id, name, color FROM categories ORDER BY id ASC;"))) {
        qWarning() << "[CategoriesTable] all error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::Category cat;
        cat.id    = query.value(0).toInt();
        cat.name  = query.value(1).toString();
        cat.color = query.value(2).toString();
        result << cat;
    }
    return result;
}