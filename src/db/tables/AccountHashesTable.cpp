#include "AccountHashesTable.h"
#include "SqlPrepare.h"
#include "models/Account.h"
#include "utils/AccountHashing.h"

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
    if (!query.prepare(QStringLiteral(
        "INSERT INTO account_hashes (account_id, hash, source) VALUES (?, ?, ?);"
    ))) {
        Sql::logPrepareError(query, "[AccountHashesTable] insert");
        return -1;
    }
    query.addBindValue(accountId);
    query.addBindValue(AccountHashing::storedForm(hash));
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
    if (!query.prepare(QStringLiteral("DELETE FROM account_hashes WHERE id = ?;"))) {
        Sql::logPrepareError(query, "[AccountHashesTable] remove");
        return false;
    }
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
    if (!query.prepare(QStringLiteral("DELETE FROM account_hashes WHERE account_id = ?;"))) {
        Sql::logPrepareError(query, "[AccountHashesTable] removeForAccount");
        return false;
    }
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
    if (!query.prepare(QStringLiteral(
        "SELECT 1 FROM account_hashes WHERE account_id = ? AND hash = ? LIMIT 1;"
    ))) {
        Sql::logPrepareError(query, "[AccountHashesTable] existsForAccount");
        return false;
    }
    query.addBindValue(accountId);
    query.addBindValue(AccountHashing::storedForm(hash));

    if (!query.exec()) {
        qWarning() << "[AccountHashesTable] existsForAccount error:" << query.lastError().text();
        return false;
    }
    return query.next();
}

int AccountHashesTable::accountOf(const QString& hash) const
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("SELECT account_id FROM account_hashes WHERE hash = ? ORDER BY id DESC LIMIT 1;"))) {
        Sql::logPrepareError(query, "[AccountHashesTable] accountOf");
        return -1;
    }
    query.addBindValue(AccountHashing::storedForm(hash));
    if (!query.exec()) {
        qWarning() << "[AccountHashesTable] accountOf error:" << query.lastError().text();
        return -1;
    }
    return query.next() ? query.value(0).toInt() : -1;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — the two strings are
// a hash and a literal source ("auto"/"manual"), as in insert().
bool AccountHashesTable::moveToAccount(const QString& hash, int accountId, const QString& source)
{
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral("UPDATE account_hashes SET account_id = ?, source = ? WHERE hash = ?;"))) {
        Sql::logPrepareError(query, "[AccountHashesTable] moveToAccount");
        return false;
    }
    query.addBindValue(accountId);
    query.addBindValue(source);
    query.addBindValue(AccountHashing::storedForm(hash));
    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountHashesTable] moveToAccount error:" << query.lastError().text();
    }
    return success;
}

QVector<Data::AccountHash> AccountHashesTable::allForAccount(int accountId) const
{
    QVector<Data::AccountHash> result;
    QSqlQuery query(db());
    if (!query.prepare(QStringLiteral(
        "SELECT id, hash, source FROM account_hashes"
        " WHERE account_id = ? ORDER BY id ASC;"
    ))) {
        Sql::logPrepareError(query, "[AccountHashesTable] allForAccount");
        return {};
    }
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