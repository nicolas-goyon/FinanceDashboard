#include "AccountsTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

AccountsTable::AccountsTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase AccountsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountsTable::createTable()
{
    QSqlQuery q(db());
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS accounts ("
        "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name     TEXT    NOT NULL UNIQUE,"
        "  type     TEXT    NOT NULL DEFAULT '',"
        "  category TEXT    NOT NULL DEFAULT '',"
        "  currency TEXT    NOT NULL DEFAULT 'EUR',"
        "  notes    TEXT    NOT NULL DEFAULT ''"
        ");"
    ));
    if (!ok)
        qWarning() << "[AccountsTable] createTable error:" << q.lastError().text();
    return ok;
}

int AccountsTable::insert(const QString& name, const QString& type, const QString& category,
                           const QString& currency, const QString& notes)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO accounts (name, type, category, currency, notes)"
        " VALUES (?, ?, ?, ?, ?);"
    ));
    q.addBindValue(name);
    q.addBindValue(type);
    q.addBindValue(category);
    q.addBindValue(currency);
    q.addBindValue(notes);

    if (!q.exec()) {
        qWarning() << "[AccountsTable] insert error:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool AccountsTable::update(int id, const QString& name, const QString& type, const QString& category,
                           const QString& currency, const QString& notes)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "UPDATE accounts SET name = ?, type = ?, category = ?,"
        " currency = ?, notes = ? WHERE id = ?;"
    ));
    q.addBindValue(name);
    q.addBindValue(type);
    q.addBindValue(category);
    q.addBindValue(currency);
    q.addBindValue(notes);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountsTable] update error:" << q.lastError().text();
    return ok;
}

bool AccountsTable::remove(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM accounts WHERE id = ?;"));
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountsTable] remove error:" << q.lastError().text();
    return ok;
}

QVector<Data::Account> AccountsTable::all() const
{
    QVector<Data::Account> result;
    QSqlQuery q(db());
    if (!q.exec(QStringLiteral(
            "SELECT id, name, type, category, currency, notes"
            " FROM accounts ORDER BY id ASC;"))) {
        qWarning() << "[AccountsTable] all error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Data::Account acc;
        acc.id       = q.value(0).toInt();
        acc.name     = q.value(1).toString();
        acc.type     = q.value(2).toString();
        acc.category = q.value(3).toString();
        acc.currency = q.value(4).toString();
        acc.notes    = q.value(5).toString();
        result << acc;
    }
    return result;
}
