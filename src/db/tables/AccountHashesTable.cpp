#include "AccountHashesTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

AccountHashesTable::AccountHashesTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase AccountHashesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountHashesTable::createTable()
{
    QSqlQuery q(db());
    const bool ok =
        q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS account_hashes ("
            "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  account_id INTEGER NOT NULL,"
            "  hash       TEXT    NOT NULL,"
            "  source     TEXT    NOT NULL DEFAULT 'manual',"
            "  UNIQUE(account_id, hash)"
            ");"
        )) &&
        q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_account_hashes_account"
            " ON account_hashes(account_id);"
        )) &&
        q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_account_hashes_hash"
            " ON account_hashes(hash);"
        ));

    if (!ok)
        qWarning() << "[AccountHashesTable] createTable error:" << q.lastError().text();
    return ok;
}

int AccountHashesTable::insert(int accountId, const QString& hash, const QString& source)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO account_hashes (account_id, hash, source) VALUES (?, ?, ?);"
    ));
    q.addBindValue(accountId);
    q.addBindValue(hash);
    q.addBindValue(source);

    if (!q.exec()) {
        qWarning() << "[AccountHashesTable] insert error:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool AccountHashesTable::remove(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM account_hashes WHERE id = ?;"));
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountHashesTable] remove error:" << q.lastError().text();
    return ok;
}

bool AccountHashesTable::removeForAccount(int accountId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM account_hashes WHERE account_id = ?;"));
    q.addBindValue(accountId);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[AccountHashesTable] removeForAccount error:" << q.lastError().text();
    return ok;
}

bool AccountHashesTable::existsForAccount(int accountId, const QString& hash) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT 1 FROM account_hashes WHERE account_id = ? AND hash = ? LIMIT 1;"
    ));
    q.addBindValue(accountId);
    q.addBindValue(hash);

    if (!q.exec()) {
        qWarning() << "[AccountHashesTable] existsForAccount error:" << q.lastError().text();
        return false;
    }
    return q.next();
}

QVector<Data::AccountHash> AccountHashesTable::allForAccount(int accountId) const
{
    QVector<Data::AccountHash> result;
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, hash, source FROM account_hashes"
        " WHERE account_id = ? ORDER BY id ASC;"
    ));
    q.addBindValue(accountId);

    if (!q.exec()) {
        qWarning() << "[AccountHashesTable] allForAccount error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Data::AccountHash h;
        h.id     = q.value(0).toInt();
        h.hash   = q.value(1).toString();
        h.source = q.value(2).toString();
        result << h;
    }
    return result;
}
