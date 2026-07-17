#include "AccountHashesTable.h"
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

AccountHashesTable::AccountHashesTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase AccountHashesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountHashesTable::createTable()
{
    QSqlQuery query(db());
    const bool success =
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS account_hashes ("
            "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  account_id INTEGER NOT NULL,"
            "  hash       TEXT    NOT NULL,"
            "  source     TEXT    NOT NULL DEFAULT 'manual',"
            "  UNIQUE(account_id, hash)"
            ");"
        )) &&
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_account_hashes_account"
            " ON account_hashes(account_id);"
        )) &&
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_account_hashes_hash"
            " ON account_hashes(hash);"
        ));

    if (!success) {
        qWarning() << "[AccountHashesTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors the table's
// (hash, source) column order; every call site passes a literal source
// ("manual"/"auto"), not a variable that could be swapped with hash.
int AccountHashesTable::insert(int accountId, const QString& hash, const QString& source)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO account_hashes (account_id, hash, source) VALUES (?, ?, ?);"
    ));
    query.addBindValue(accountId);
    query.addBindValue(hash);
    query.addBindValue(source);

    if (!query.exec()) {
        qWarning() << "[AccountHashesTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool AccountHashesTable::remove(int hashId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM account_hashes WHERE id = ?;"));
    query.addBindValue(hashId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountHashesTable] remove error:" << query.lastError().text();
    }
    return success;
}

bool AccountHashesTable::removeForAccount(int accountId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM account_hashes WHERE account_id = ?;"));
    query.addBindValue(accountId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountHashesTable] removeForAccount error:" << query.lastError().text();
    }
    return success;
}

bool AccountHashesTable::existsForAccount(int accountId, const QString& hash) const
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM account_hashes WHERE account_id = ? AND hash = ? LIMIT 1;"
    ));
    query.addBindValue(accountId);
    query.addBindValue(hash);

    if (!query.exec()) {
        qWarning() << "[AccountHashesTable] existsForAccount error:" << query.lastError().text();
        return false;
    }
    return query.next();
}

QVector<Data::AccountHash> AccountHashesTable::allForAccount(int accountId) const
{
    QVector<Data::AccountHash> result;
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, hash, source FROM account_hashes"
        " WHERE account_id = ? ORDER BY id ASC;"
    ));
    query.addBindValue(accountId);

    if (!query.exec()) {
        qWarning() << "[AccountHashesTable] allForAccount error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::AccountHash accountHash;
        accountHash.id     = query.value(0).toInt();
        accountHash.hash   = query.value(1).toString();
        accountHash.source = query.value(2).toString();
        result << accountHash;
    }
    return result;
}