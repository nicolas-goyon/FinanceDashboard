#include "SettingsTable.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QtLogging>
#include <utility>

SettingsTable::SettingsTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase SettingsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

QString SettingsTable::value(const QString& key) const
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?;"));
    query.addBindValue(key);
    if (!query.exec()) {
        qWarning() << "[SettingsTable] value error:" << query.lastError().text();
        return QStringLiteral("");
    }
    return query.next() ? query.value(0).toString() : QStringLiteral("");
}

bool SettingsTable::setValue(const QString& key, const QString& value)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO settings (key, value) VALUES (?, ?)"
        " ON CONFLICT(key) DO UPDATE SET value = excluded.value;"
    ));
    // Both columns are required (no DEFAULT): not through Sql::text(). The
    // keys and the values are constants of Database.
    query.addBindValue(key);
    query.addBindValue(value);
    const bool success = query.exec();
    if (!success) {
        qWarning() << "[SettingsTable] setValue error:" << query.lastError().text();
    }
    return success;
}
