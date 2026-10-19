#include "AccountBalanceCheckpointsTable.h"
#include "SqlText.h"
#include "models/Account.h"

#include <QDate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <Qt>
#include <QtLogging>
#include <optional>
#include <utility>

namespace {
QString dateToDb(const QDate& date)
{
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}

QDate dateFromDb(const QString& dateStr)
{
    return dateStr.isEmpty() ? QDate() : QDate::fromString(dateStr, Qt::ISODate);
}
} // namespace

AccountBalanceCheckpointsTable::AccountBalanceCheckpointsTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase AccountBalanceCheckpointsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountBalanceCheckpointsTable::createTable()
{
    QSqlQuery query(db());
    const bool success =
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS account_balance_checkpoints ("
            "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  account_id INTEGER NOT NULL,"
            "  date       TEXT    NOT NULL,"
            "  balance    REAL    NOT NULL DEFAULT 0,"
            "  note       TEXT    NOT NULL DEFAULT ''"
            ");"
        )) &&
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_balance_checkpoints_account"
            " ON account_balance_checkpoints(account_id);"
        ));

    if (!success) {
        qWarning() << "[AccountBalanceCheckpointsTable] createTable error:" << query.lastError().text();
    }
    return success;
}

int AccountBalanceCheckpointsTable::insert(int accountId, const QDate& date, double balance, const QString& note)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO account_balance_checkpoints (account_id, date, balance, note) VALUES (?, ?, ?, ?);"
    ));
    query.addBindValue(accountId);
    query.addBindValue(dateToDb(date));          // required: NULL for an invalid date fails
    query.addBindValue(balance);
    query.addBindValue(Sql::text(note));         // optional (DEFAULT ''), see SqlText.h

    if (!query.exec()) {
        qWarning() << "[AccountBalanceCheckpointsTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool AccountBalanceCheckpointsTable::remove(int checkpointId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM account_balance_checkpoints WHERE id = ?;"));
    query.addBindValue(checkpointId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountBalanceCheckpointsTable] remove error:" << query.lastError().text();
    }
    return success;
}

bool AccountBalanceCheckpointsTable::removeForAccount(int accountId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM account_balance_checkpoints WHERE account_id = ?;"));
    query.addBindValue(accountId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountBalanceCheckpointsTable] removeForAccount error:" << query.lastError().text();
    }
    return success;
}

QVector<Data::AccountBalanceCheckpoint> AccountBalanceCheckpointsTable::allForAccount(int accountId) const
{
    QVector<Data::AccountBalanceCheckpoint> result;
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, account_id, date, balance, note FROM account_balance_checkpoints"
        " WHERE account_id = ? ORDER BY date ASC, id ASC;"
    ));
    query.addBindValue(accountId);

    if (!query.exec()) {
        qWarning() << "[AccountBalanceCheckpointsTable] allForAccount error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::AccountBalanceCheckpoint checkpoint;
        checkpoint.id        = query.value(0).toInt();
        checkpoint.accountId = query.value(1).toInt();
        checkpoint.date      = dateFromDb(query.value(2).toString());
        checkpoint.balance   = query.value(3).toDouble();
        checkpoint.note      = query.value(4).toString();
        result << checkpoint;
    }
    return result;
}

std::optional<Data::AccountBalanceCheckpoint>
AccountBalanceCheckpointsTable::latestOnOrBefore(int accountId, const QDate& date) const
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, account_id, date, balance, note FROM account_balance_checkpoints"
        " WHERE account_id = ? AND date <= ?"
        " ORDER BY date DESC, id DESC LIMIT 1;"
    ));
    query.addBindValue(accountId);
    query.addBindValue(dateToDb(date));

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    Data::AccountBalanceCheckpoint checkpoint;
    checkpoint.id        = query.value(0).toInt();
    checkpoint.accountId = query.value(1).toInt();
    checkpoint.date      = dateFromDb(query.value(2).toString());
    checkpoint.balance   = query.value(3).toDouble();
    checkpoint.note      = query.value(4).toString();
    return checkpoint;
}
