#include "AccountTypesTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

AccountTypesTable::AccountTypesTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase AccountTypesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountTypesTable::createTable()
{
    QSqlQuery q(db());
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS account_types ("
        "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name  TEXT    NOT NULL UNIQUE"
        ");"
    ));
    if (!ok)
        qWarning() << "[AccountTypesTable] createTable error:" << q.lastError().text();
    return ok;
}

int AccountTypesTable::insert(const QString& name)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("INSERT INTO account_types (name) VALUES (?);"));
    q.addBindValue(name);

    if (!q.exec()) {
        qWarning() << "[AccountTypesTable] insert error:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool AccountTypesTable::rename(int id, const QString& newName)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE account_types SET name = ? WHERE id = ?;"));
    q.addBindValue(newName);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountTypesTable] rename error:" << q.lastError().text();
    return ok;
}

bool AccountTypesTable::remove(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM account_types WHERE id = ?;"));
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountTypesTable] remove error:" << q.lastError().text();
    return ok;
}

QVector<Data::AccountType> AccountTypesTable::all() const
{
    QVector<Data::AccountType> result;
    QSqlQuery q(db());
    if (!q.exec(QStringLiteral("SELECT id, name FROM account_types ORDER BY id ASC;"))) {
        qWarning() << "[AccountTypesTable] all error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Data::AccountType t;
        t.id   = q.value(0).toInt();
        t.name = q.value(1).toString();
        result << t;
    }
    return result;
}
