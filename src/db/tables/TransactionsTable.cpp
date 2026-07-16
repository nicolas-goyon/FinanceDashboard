#include "TransactionsTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

namespace {
QString dateToDb(const QDate& d)
{
    return d.isValid() ? d.toString(Qt::ISODate) : QString();
}

QDate dateFromDb(const QString& s)
{
    return s.isEmpty() ? QDate() : QDate::fromString(s, Qt::ISODate);
}
} // namespace

TransactionsTable::TransactionsTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase TransactionsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool TransactionsTable::createTable()
{
    QSqlQuery q(db());
    const bool ok =
        q.exec(QStringLiteral(
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
        q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_transactions_account"
            " ON transactions(account_id);"
        ));

    if (!ok)
        qWarning() << "[TransactionsTable] createTable error:" << q.lastError().text();
    return ok;
}

bool TransactionsTable::insertBatch(int accountId, const QVector<Data::Transaction>& txs)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO transactions"
        " (account_id, confirmation_date, operation_date, description, debit,"
        "  credit, balance_after, category, subcategory, source_template)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
    ));

    for (const Data::Transaction& tx : txs) {
        q.addBindValue(accountId);
        q.addBindValue(dateToDb(tx.confirmationDate));
        q.addBindValue(dateToDb(tx.operationDate));
        q.addBindValue(tx.description);
        q.addBindValue(tx.debit);
        q.addBindValue(tx.credit);
        q.addBindValue(tx.balanceAfter);
        q.addBindValue(tx.category);
        q.addBindValue(tx.subcategory);
        q.addBindValue(tx.sourceTemplate);

        if (!q.exec()) {
            qWarning() << "[TransactionsTable] insertBatch error:" << q.lastError().text();
            return false;
        }
    }
    return true;
}

bool TransactionsTable::removeForAccount(int accountId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM transactions WHERE account_id = ?;"));
    q.addBindValue(accountId);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[TransactionsTable] removeForAccount error:" << q.lastError().text();
    return ok;
}

QVector<Data::Transaction> TransactionsTable::allForAccount(int accountId) const
{
    QVector<Data::Transaction> result;
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, account_id, confirmation_date, operation_date, description,"
        " debit, credit, balance_after, category, subcategory, source_template"
        " FROM transactions WHERE account_id = ?"
        " ORDER BY operation_date DESC, id DESC;"
    ));
    q.addBindValue(accountId);

    if (!q.exec()) {
        qWarning() << "[TransactionsTable] allForAccount error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Data::Transaction tx;
        tx.id               = q.value(0).toInt();
        tx.accountId         = q.value(1).toInt();
        tx.confirmationDate  = dateFromDb(q.value(2).toString());
        tx.operationDate     = dateFromDb(q.value(3).toString());
        tx.description       = q.value(4).toString();
        tx.debit             = q.value(5).toDouble();
        tx.credit            = q.value(6).toDouble();
        tx.balanceAfter      = q.value(7).toDouble();
        tx.category          = q.value(8).toString();
        tx.subcategory       = q.value(9).toString();
        tx.sourceTemplate    = q.value(10).toString();
        result << tx;
    }
    return result;
}

int TransactionsTable::countForAccount(int accountId) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE account_id = ?;"));
    q.addBindValue(accountId);

    if (!q.exec() || !q.next()) {
        qWarning() << "[TransactionsTable] countForAccount error:" << q.lastError().text();
        return 0;
    }
    return q.value(0).toInt();
}
