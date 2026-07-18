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
constexpr int kColName = 4;
constexpr int kColDescription = 5;
constexpr int kColDebit = 6;
constexpr int kColCredit = 7;
constexpr int kColBalanceAfter = 8;
constexpr int kColCategory = 9;
constexpr int kColSubcategory = 10;
constexpr int kColSourceTemplate = 11;
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
            "  name              TEXT    NOT NULL DEFAULT '',"
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
        return false;
    }

    // `name` didn't exist when this table first shipped — for a database
    // created before this column was added, CREATE TABLE IF NOT EXISTS above
    // is a no-op, so add it explicitly if missing (no-op once it's there).
    // No `confidence` column — see the class-header schema comment for why
    // that's a deliberate, review-screen-only concept, never persisted here.
    return ensureColumn(QStringLiteral("name"), QStringLiteral("TEXT NOT NULL DEFAULT ''"));
}

bool TransactionsTable::ensureColumn(const QString& columnName, const QString& columnDdl)
{
    QSqlQuery pragma(db());
    if (!pragma.exec(QStringLiteral("PRAGMA table_info(transactions);"))) {
        qWarning() << "[TransactionsTable] ensureColumn(" << columnName << "): table_info error:" << pragma.lastError().text();
        return false;
    }

    bool hasColumn = false;
    while (pragma.next()) {
        // PRAGMA table_info columns: cid, name, type, notnull, dflt_value, pk
        if (pragma.value(1).toString().compare(columnName, Qt::CaseInsensitive) == 0) {
            hasColumn = true;
            break;
        }
    }

    if (hasColumn) {
        return true;
    }

    QSqlQuery alter(db());
    const bool success = alter.exec(QStringLiteral("ALTER TABLE transactions ADD COLUMN %1 %2;")
                                         .arg(columnName, columnDdl));
    if (!success) {
        qWarning() << "[TransactionsTable] ensureColumn(" << columnName << "): ALTER TABLE error:" << alter.lastError().text();
    }
    return success;
}

bool TransactionsTable::insertBatch(int accountId, const QVector<Data::Transaction>& txs)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO transactions"
        " (account_id, confirmation_date, operation_date, name, description, debit,"
        "  credit, balance_after, category, subcategory, source_template)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
    ));

    for (const Data::Transaction& transaction : txs) {
        query.addBindValue(accountId);
        query.addBindValue(dateToDb(transaction.confirmationDate));
        query.addBindValue(dateToDb(transaction.operationDate));
        query.addBindValue(transaction.name);
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
        "SELECT id, account_id, confirmation_date, operation_date, name, description,"
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
        transaction.name              = query.value(kColName).toString();
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

bool TransactionsTable::updateById(const Data::Transaction& tx)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE transactions SET confirmation_date = ?, operation_date = ?, name = ?,"
        " description = ?, debit = ?, credit = ?, category = ?, subcategory = ?"
        " WHERE id = ?;"
    ));
    query.addBindValue(dateToDb(tx.confirmationDate));
    query.addBindValue(dateToDb(tx.operationDate));
    query.addBindValue(tx.name);
    query.addBindValue(tx.description);
    query.addBindValue(tx.debit);
    query.addBindValue(tx.credit);
    query.addBindValue(tx.category);
    query.addBindValue(tx.subcategory);
    query.addBindValue(tx.id);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[TransactionsTable] updateById error:" << query.lastError().text();
    }
    return success;
}

std::optional<Data::Transaction> TransactionsTable::latestOnOrBefore(int accountId, const QDate& date) const
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, account_id, confirmation_date, operation_date, name, description,"
        " debit, credit, balance_after, category, subcategory, source_template"
        " FROM transactions WHERE account_id = ? AND operation_date != '' AND operation_date <= ?"
        " ORDER BY operation_date DESC, id DESC LIMIT 1;"
    ));
    query.addBindValue(accountId);
    query.addBindValue(dateToDb(date));

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    Data::Transaction transaction;
    transaction.id               = query.value(kColId).toInt();
    transaction.accountId        = query.value(kColAccountId).toInt();
    transaction.confirmationDate = dateFromDb(query.value(kColConfirmationDate).toString());
    transaction.operationDate    = dateFromDb(query.value(kColOperationDate).toString());
    transaction.name             = query.value(kColName).toString();
    transaction.description      = query.value(kColDescription).toString();
    transaction.debit            = query.value(kColDebit).toDouble();
    transaction.credit           = query.value(kColCredit).toDouble();
    transaction.balanceAfter     = query.value(kColBalanceAfter).toDouble();
    transaction.category         = query.value(kColCategory).toString();
    transaction.subcategory      = query.value(kColSubcategory).toString();
    transaction.sourceTemplate   = query.value(kColSourceTemplate).toString();
    return transaction;
}