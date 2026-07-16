#include "AccountCategoriesTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

AccountCategoriesTable::AccountCategoriesTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase AccountCategoriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountCategoriesTable::createTable()
{
    QSqlQuery q(db());
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS account_categories ("
        "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name  TEXT    NOT NULL UNIQUE,"
        "  color TEXT    NOT NULL"
        ");"
    ));
    if (!ok)
        qWarning() << "[AccountCategoriesTable] createTable error:" << q.lastError().text();
    return ok;
}

int AccountCategoriesTable::insert(const QString& name, const QString& color)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO account_categories (name, color) VALUES (?, ?);"
    ));
    q.addBindValue(name);
    q.addBindValue(color);

    if (!q.exec()) {
        qWarning() << "[AccountCategoriesTable] insert error:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool AccountCategoriesTable::rename(int id, const QString& newName)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE account_categories SET name = ? WHERE id = ?;"));
    q.addBindValue(newName);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountCategoriesTable] rename error:" << q.lastError().text();
    return ok;
}

bool AccountCategoriesTable::setColor(int id, const QString& color)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE account_categories SET color = ? WHERE id = ?;"));
    q.addBindValue(color);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountCategoriesTable] setColor error:" << q.lastError().text();
    return ok;
}

bool AccountCategoriesTable::remove(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM account_categories WHERE id = ?;"));
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountCategoriesTable] remove error:" << q.lastError().text();
    return ok;
}

QVector<Data::AccountCategory> AccountCategoriesTable::all() const
{
    QVector<Data::AccountCategory> result;
    QSqlQuery q(db());
    if (!q.exec(QStringLiteral("SELECT id, name, color FROM account_categories ORDER BY id ASC;"))) {
        qWarning() << "[AccountCategoriesTable] all error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Data::AccountCategory cat;
        cat.id    = q.value(0).toInt();
        cat.name  = q.value(1).toString();
        cat.color = q.value(2).toString();
        result << cat;
    }
    return result;
}
