#include "SnapshotsTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

SnapshotsTable::SnapshotsTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase SnapshotsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool SnapshotsTable::createTable()
{
    QSqlQuery q(db());
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS snapshots ("
        "  id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  date TEXT    NOT NULL UNIQUE"
        ");"
    ));
    if (!ok)
        qWarning() << "[SnapshotsTable] createTable error:" << q.lastError().text();
    return ok;
}

// ── Write ─────────────────────────────────────────────────────────────────────

bool SnapshotsTable::upsert(const QDate& date)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO snapshots (date) VALUES (?)"
        " ON CONFLICT(date) DO NOTHING;"
    ));
    q.addBindValue(date.toString(Qt::ISODate));

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[SnapshotsTable] upsert error:" << q.lastError().text();
    return ok;
}

// ── Read ──────────────────────────────────────────────────────────────────────

std::optional<int> SnapshotsTable::idForDate(const QDate& date) const
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("SELECT id FROM snapshots WHERE date = ?;"));
    q.addBindValue(date.toString(Qt::ISODate));

    if (!q.exec() || !q.next())
        return std::nullopt;
    return q.value(0).toInt();
}

std::optional<QPair<int, QDate>> SnapshotsTable::latest() const
{
    QSqlQuery q(db());
    if (!q.exec(QStringLiteral(
            "SELECT id, date FROM snapshots ORDER BY date DESC LIMIT 1;")))
        return std::nullopt;

    if (!q.next())
        return std::nullopt;

    const int   id   = q.value(0).toInt();
    const QDate date = QDate::fromString(q.value(1).toString(), Qt::ISODate);
    return qMakePair(id, date);
}

QVector<QDate> SnapshotsTable::allDates() const
{
    QVector<QDate> result;
    QSqlQuery q(db());
    if (!q.exec(QStringLiteral(
            "SELECT date FROM snapshots ORDER BY date DESC;")))
        return result;

    while (q.next())
        result << QDate::fromString(q.value(0).toString(), Qt::ISODate);
    return result;
}
