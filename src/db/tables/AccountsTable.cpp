#include "AccountsTable.h"
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

namespace {
// Column indices for the SELECT in all() (must match its column list order).
constexpr int kColId = 0;
constexpr int kColName = 1;
constexpr int kColType = 2;
constexpr int kColCategory = 3;
constexpr int kColCurrency = 4;
constexpr int kColNotes = 5;
} // namespace

AccountsTable::AccountsTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase AccountsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

bool AccountsTable::createTable()
{
    QSqlQuery query(db());
    const bool success = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS accounts ("
        "  id       INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name     TEXT    NOT NULL UNIQUE,"
        "  type     TEXT    NOT NULL DEFAULT '',"
        "  category TEXT    NOT NULL DEFAULT '',"
        "  currency TEXT    NOT NULL DEFAULT 'EUR',"
        "  notes    TEXT    NOT NULL DEFAULT ''"
        ");"
    ));
    if (!success) {
        qWarning() << "[AccountsTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors the table's
// (name, type, category, currency, notes) column order used consistently by
// every call site (Database::addAccount() and AccountController).
int AccountsTable::insert(const QString& name, const QString& type, const QString& category,
                           const QString& currency, const QString& notes)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO accounts (name, type, category, currency, notes)"
        " VALUES (?, ?, ?, ?, ?);"
    ));
    query.addBindValue(name);
    query.addBindValue(type);
    query.addBindValue(category);
    query.addBindValue(currency);
    query.addBindValue(notes);

    if (!query.exec()) {
        qWarning() << "[AccountsTable] insert error:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — mirrors the table's
// (name, type, category, currency, notes) column order used consistently by
// every call site (Database::updateAccount() and AccountController).
bool AccountsTable::update(int accountId, const QString& name, const QString& type, const QString& category,
                           const QString& currency, const QString& notes)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE accounts SET name = ?, type = ?, category = ?,"
        " currency = ?, notes = ? WHERE id = ?;"
    ));
    query.addBindValue(name);
    query.addBindValue(type);
    query.addBindValue(category);
    query.addBindValue(currency);
    query.addBindValue(notes);
    query.addBindValue(accountId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountsTable] update error:" << query.lastError().text();
    }
    return success;
}

bool AccountsTable::remove(int accountId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM accounts WHERE id = ?;"));
    query.addBindValue(accountId);

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[AccountsTable] remove error:" << query.lastError().text();
    }
    return success;
}

QVector<Data::Account> AccountsTable::all() const
{
    QVector<Data::Account> result;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT id, name, type, category, currency, notes"
            " FROM accounts ORDER BY id ASC;"))) {
        qWarning() << "[AccountsTable] all error:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Data::Account acc;
        acc.id       = query.value(kColId).toInt();
        acc.name     = query.value(kColName).toString();
        acc.type     = query.value(kColType).toString();
        acc.category = query.value(kColCategory).toString();
        acc.currency = query.value(kColCurrency).toString();
        acc.notes    = query.value(kColNotes).toString();
        result << acc;
    }
    return result;
}