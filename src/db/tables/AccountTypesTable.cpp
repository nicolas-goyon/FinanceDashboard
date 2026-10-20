#include "AccountTypesTable.h"
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

AccountTypesTable::AccountTypesTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase AccountTypesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountTypesTable::createTable()
{
    QSqlQuery query(db());
    const bool success = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS account_types ("
        "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name  TEXT    NOT NULL UNIQUE"
        ");"
    ));
    if (!success) {
        qWarning() << "[AccountTypesTable] createTable error:" << query.lastError().text();
    }
    return success;
}

int AccountTypesTable::insert(const QString& name)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("INSERT INTO account_types (name) VALUES (?);"))) {
        Sql::logPrepareError(query, "[AccountTypesTable] insert");
        return -1;
    }
    query.addBindValue(name);

    if (!query.exec()) {
        qWarning() << "[AccountTypesTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool AccountTypesTable::rename(int typeId, const QString& newName)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE account_types SET name = ? WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[AccountTypesTable] rename");
        return false;
    }
    query.addBindValue(newName);
    query.addBindValue(typeId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountTypesTable] rename error:" << query.lastError().text();
    }
    return success;
}

bool AccountTypesTable::remove(int typeId)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("DELETE FROM account_types WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[AccountTypesTable] remove");
        return false;
    }
    query.addBindValue(typeId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountTypesTable] remove error:" << query.lastError().text();
    }
    return success;
}

QVector<Data::AccountType> AccountTypesTable::all() const
{
    QVector<Data::AccountType> result;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral("SELECT id, name FROM account_types ORDER BY id ASC;"))) {
        qWarning() << "[AccountTypesTable] all error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::AccountType type;
        type.id   = query.value(0).toInt();
        type.name = query.value(1).toString();
        result << type;
    }
    return result;
}