#include "AccountCategoriesTable.h"
#include "SqlPrepare.h"
#include "models/Account.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <QtLogging>
#include <utility>

AccountCategoriesTable::AccountCategoriesTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase AccountCategoriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountCategoriesTable::createTable()
{
    QSqlQuery query(db());
    const bool success = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS account_categories ("
        "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name  TEXT    NOT NULL UNIQUE,"
        "  color TEXT    NOT NULL"
        ");"
    ));
    if (!success) {
        qWarning() << "[AccountCategoriesTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors the table's
// (name, color) column order; every call site passes them in that order.
int AccountCategoriesTable::insert(const QString& name, const QString& color)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "INSERT INTO account_categories (name, color) VALUES (?, ?);"
    ))) {
        Sql::logPrepareError(query, "[AccountCategoriesTable] insert");
        return -1;
    }
    query.addBindValue(name);
    query.addBindValue(color);

    if (!query.exec()) {
        qWarning() << "[AccountCategoriesTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool AccountCategoriesTable::rename(int accountCategoryId, const QString& newName)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE account_categories SET name = ? WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[AccountCategoriesTable] rename");
        return false;
    }
    query.addBindValue(newName);
    query.addBindValue(accountCategoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountCategoriesTable] rename error:" << query.lastError().text();
    }
    return success;
}

bool AccountCategoriesTable::setColor(int accountCategoryId, const QString& color)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE account_categories SET color = ? WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[AccountCategoriesTable] setColor");
        return false;
    }
    query.addBindValue(color);
    query.addBindValue(accountCategoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountCategoriesTable] setColor error:" << query.lastError().text();
    }
    return success;
}

bool AccountCategoriesTable::remove(int accountCategoryId)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("DELETE FROM account_categories WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[AccountCategoriesTable] remove");
        return false;
    }
    query.addBindValue(accountCategoryId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountCategoriesTable] remove error:" << query.lastError().text();
    }
    return success;
}

QVector<Data::AccountCategory> AccountCategoriesTable::all() const
{
    QVector<Data::AccountCategory> result;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral("SELECT id, name, color FROM account_categories ORDER BY id ASC;"))) {
        qWarning() << "[AccountCategoriesTable] all error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::AccountCategory cat;
        cat.id    = query.value(0).toInt();
        cat.name  = query.value(1).toString();
        cat.color = query.value(2).toString();
        result << cat;
    }
    return result;
}