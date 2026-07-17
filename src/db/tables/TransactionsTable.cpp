#include "TransactionsTable.h"
#include "models/Transaction.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <Qt>
#include <QtLogging>
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

// Column indices for the SELECT in allForAccount() (must match its column list order).
constexpr int kColId = 0;
constexpr int kColAccountId = 1;
constexpr int kColConfirmationDate = 2;
constexpr int kColOperationDate = 3;
constexpr int kColDescription = 4;
constexpr int kColDebit = 5;
constexpr int kColCredit = 6;
constexpr int kColBalanceAfter = 7;
constexpr int kColCategory = 8;
constexpr int kColSubcategory = 9;
constexpr int kColSourceTemplate = 10;
} // namespace

TransactionsTable::TransactionsTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase TransactionsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool TransactionsTable::createTable()
{
    QSqlQuery query(db());
    const bool success =
        query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS transactions ("
            "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  account_id        INTEGER NOT NULL,"
            "  confirmation_date TEXT    NOT NULL DEFAULT '',"
            "  operation_date    TEXT    NOT NULL DEFAULT '',"
            "  description       TEXT    NOT NULL DEFAULT '',"
            "  debit             REAL    NOT NULL DEFAULT 0,"
            "  credit            REAL    NOT NULL DEFAULT 0,"
            "  balance_after     REAL    NOT NULL DEFAULT 0,"
            "  category          TEXT    NOT NULL DEFAULT '',"
            "  subcategory       TEXT    NOT NULL DEFAULT '',"
            "  source_template   TEXT    NOT NULL DEFAULT ''"
            ");"
        )) &&
        query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_transactions_account"
            " ON transactions(account_id);"
        ));

    if (!success) {
        qWarning() << "[TransactionsTable] createTable error:" << query.lastError().text();
    }
    return success;
}

bool TransactionsTable::insertBatch(int accountId, const QVector<Data::Transaction>& txs)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO transactions"
        " (account_id, confirmation_date, operation_date, description, debit,"
        "  credit, balance_after, category, subcategory, source_template)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
    ));

    for (const Data::Transaction& transaction : txs) {
        query.addBindValue(accountId);
        query.addBindValue(dateToDb(transaction.confirmationDate));
        query.addBindValue(dateToDb(transaction.operationDate));
        query.addBindValue(transaction.description);
        query.addBindValue(transaction.debit);
        query.addBindValue(transaction.credit);
        query.addBindValue(transaction.balanceAfter);
        query.addBindValue(transaction.category);
        query.addBindValue(transaction.subcategory);
        query.addBindValue(transaction.sourceTemplate);

        if (!query.exec()) {
            qWarning() << "[TransactionsTable] insertBatch error:" << query.lastError().text();
            return false;
        }
    }
    return true;
}

bool TransactionsTable::removeForAccount(int accountId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM transactions WHERE account_id = ?;"));
    query.addBindValue(accountId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[TransactionsTable] removeForAccount error:" << query.lastError().text();
    }
    return success;
}

QVector<Data::Transaction> TransactionsTable::allForAccount(int accountId) const
{
    QVector<Data::Transaction> result;
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, account_id, confirmation_date, operation_date, description,"
        " debit, credit, balance_after, category, subcategory, source_template"
        " FROM transactions WHERE account_id = ?"
        " ORDER BY operation_date DESC, id DESC;"
    ));
    query.addBindValue(accountId);

    if (!query.exec()) {
        qWarning() << "[TransactionsTable] allForAccount error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::Transaction transaction;
        transaction.id               = query.value(kColId).toInt();
        transaction.accountId         = query.value(kColAccountId).toInt();
        transaction.confirmationDate  = dateFromDb(query.value(kColConfirmationDate).toString());
        transaction.operationDate     = dateFromDb(query.value(kColOperationDate).toString());
        transaction.description       = query.value(kColDescription).toString();
        transaction.debit             = query.value(kColDebit).toDouble();
        transaction.credit            = query.value(kColCredit).toDouble();
        transaction.balanceAfter      = query.value(kColBalanceAfter).toDouble();
        transaction.category          = query.value(kColCategory).toString();
        transaction.subcategory       = query.value(kColSubcategory).toString();
        transaction.sourceTemplate    = query.value(kColSourceTemplate).toString();
        result.append(transaction);
    }
    return result;
}

int TransactionsTable::countForAccount(int accountId) const
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE account_id = ?;"));
    query.addBindValue(accountId);

    if (!query.exec() || !query.next()) {
        qWarning() << "[TransactionsTable] countForAccount error:" << query.lastError().text();
        return 0;
    }
    return query.value(0).toInt();
}