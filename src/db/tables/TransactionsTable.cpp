#include "TransactionsTable.h"
#include "SqlText.h"
#include "models/Transaction.h"

#include <QCryptographicHash>
#include <QHash>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <Qt>
#include <QtLogging>
#include <cmath>
#include <utility>

namespace {
// An invalid date is stored as '', not as NULL: the date columns are
// TEXT NOT NULL (see SqlText.h).
QString dateToDb(const QDate& date)
{
    return date.isValid() ? date.toString(Qt::ISODate) : QStringLiteral("");
}

QDate dateFromDb(const QString& dateStr)
{
    return dateStr.isEmpty() ? QDate() : QDate::fromString(dateStr, Qt::ISODate);
}

// Column indices for the row-reading SELECTs (allForAccount(),
// allCategorized(), latestOnOrBefore()) — all three share one column list, so
// these must match its order.
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

QString TransactionsTable::fingerprint(const QString& operationDate, const QString& confirmationDate,
                                       double debit, double credit, double balanceAfter,
                                       const QString& description, int occurrence)
{
    // Amounts in cents, so that 19.99 parsed twice, or read back from the
    // REAL column, gives the same text.
    const auto cents = [](double amount) { return QString::number(std::llround(amount * 100.0)); };
    // Maintainer decision (2026-10-18): a row with a bank reference is that
    // reference and its amount. A later export that changes its value date or
    // its running balance still finds it. Without a reference: every field.
    const QString reference = bankReference(description);
    QStringList fields;
    if (!reference.isEmpty()) {
        fields = {QStringLiteral("ref"), reference, cents(debit), cents(credit)};
    } else {
        fields = {
            Sql::text(operationDate), Sql::text(confirmationDate),
            cents(debit), cents(credit), cents(balanceAfter),
            Sql::text(description),
        };
    }
    // The first occurrence keeps the plain form: schema migration 2 computed
    // the fingerprints of the rows that were already saved that way.
    if (occurrence > 1) {
        fields.append(QString::number(occurrence));
    }
    // U+001F (unit separator) cannot occur in a date or a number, and
    // separates the fields without ambiguity.
    const QString canonical = fields.join(QChar(0x1F));
    return QString::fromLatin1(
        QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString TransactionsTable::bankReference(const QString& description)
{
    // "Internal ref.: 06c60f6b-…" — the exact label of the bank, with its case.
    // The last match wins: the bank's reference block ends the description,
    // after the text that the sender of a transfer wrote, which can contain
    // the same words. The id has letters, digits and hyphens only, so
    // punctuation after it is not part of it.
    static const QRegularExpression kInternalRef(QStringLiteral("\\bInternal ref\\.:\\s*([0-9A-Za-z-]+)"));
    QString reference;
    QRegularExpressionMatchIterator matches = kInternalRef.globalMatch(description);
    while (matches.hasNext()) {
        reference = matches.next().captured(1);
    }
    return reference;
}

QString TransactionsTable::fingerprint(const Data::Transaction& tx, int occurrence)
{
    return fingerprint(dateToDb(tx.operationDate), dateToDb(tx.confirmationDate),
                       tx.debit, tx.credit, tx.balanceAfter, tx.description, occurrence);
}

QStringList TransactionsTable::fingerprints(const QVector<Data::Transaction>& txs)
{
    QStringList result;
    result.reserve(txs.size());
    QHash<QString, int> occurrences; // plain fingerprint → rows with it so far
    for (const Data::Transaction& tx : txs) {
        const QString plain = fingerprint(tx);
        const int occurrence = ++occurrences[plain];
        result.append(occurrence == 1 ? plain : fingerprint(tx, occurrence));
    }
    return result;
}

QHash<QString, QDate> TransactionsTable::operationDatesByFingerprint(int accountId) const
{
    QHash<QString, QDate> result;
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT fingerprint, operation_date FROM transactions WHERE account_id = ?;"));
    query.addBindValue(accountId);
    if (!query.exec()) {
        qWarning() << "[TransactionsTable] operationDatesByFingerprint error:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.insert(query.value(0).toString(), QDate::fromString(query.value(1).toString(), Qt::ISODate));
    }
    return result;
}

bool TransactionsTable::insertBatch(int accountId, const QVector<Data::Transaction>& txs, int* inserted)
{
    if (inserted != nullptr) {
        *inserted = 0;
    }
    QSqlQuery query(db());
    // ON CONFLICT on the fingerprint index only: INSERT OR IGNORE would also
    // skip a row that breaks NOT NULL (a NaN amount binds as NULL) without
    // an error.
    query.prepare(QStringLiteral(
        "INSERT INTO transactions"
        " (account_id, confirmation_date, operation_date, name, description, debit,"
        "  credit, balance_after, category, subcategory, source_template, fingerprint)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT (account_id, fingerprint) DO NOTHING;"
    ));

    const QStringList computed = fingerprints(txs);
    for (qsizetype i = 0; i < txs.size(); ++i) {
        const Data::Transaction& transaction = txs.at(i);
        query.addBindValue(accountId);
        query.addBindValue(dateToDb(transaction.confirmationDate));
        query.addBindValue(dateToDb(transaction.operationDate));
        query.addBindValue(Sql::text(transaction.name));
        query.addBindValue(Sql::text(transaction.description));
        query.addBindValue(transaction.debit);
        query.addBindValue(transaction.credit);
        query.addBindValue(transaction.balanceAfter);
        query.addBindValue(Sql::text(transaction.category));
        query.addBindValue(Sql::text(transaction.subcategory));
        query.addBindValue(Sql::text(transaction.sourceTemplate));
        query.addBindValue(transaction.fingerprint.isEmpty() ? computed.at(i) : transaction.fingerprint);

        if (!query.exec()) {
            qWarning() << "[TransactionsTable] insertBatch error:" << query.lastError().text();
            return false;
        }
        if (inserted != nullptr) {
            *inserted += query.numRowsAffected(); // 0 when the row was ignored
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

QVector<Data::Transaction> TransactionsTable::allCategorized() const
{
    QVector<Data::Transaction> result;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT id, account_id, confirmation_date, operation_date, name, description,"
            " debit, credit, balance_after, category, subcategory, source_template"
            " FROM transactions WHERE category != ''"
            " ORDER BY operation_date ASC, id ASC;"))) {
        qWarning() << "[TransactionsTable] allCategorized error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::Transaction transaction;
        transaction.id                = query.value(kColId).toInt();
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
    query.addBindValue(Sql::text(tx.name));
    query.addBindValue(Sql::text(tx.description));
    query.addBindValue(tx.debit);
    query.addBindValue(tx.credit);
    query.addBindValue(Sql::text(tx.category));
    query.addBindValue(Sql::text(tx.subcategory));
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