#include "SnapshotsTable.h"

#include <QPair>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <Qt>
#include <QtLogging>
#include <optional>
#include <utility>

SnapshotsTable::SnapshotsTable(QString connectionName)
    : m_connectionName(std::move(connectionName))
{}

QSqlDatabase SnapshotsTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool SnapshotsTable::createTable()
{
    QSqlQuery query(db());
    const bool success = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS snapshots ("
        "  id   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  date TEXT    NOT NULL UNIQUE"
        ");"
    ));
    if (!success) {
        qWarning() << "[SnapshotsTable] createTable error:" << query.lastError().text();
    }
    return success;
}

// ── Write ─────────────────────────────────────────────────────────────────────

bool SnapshotsTable::upsert(const QDate& date)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO snapshots (date) VALUES (?)"
        " ON CONFLICT(date) DO NOTHING;"
    ));
    query.addBindValue(date.toString(Qt::ISODate));

    const bool success = query.exec();
    if (!success) {
        qWarning() << "[SnapshotsTable] upsert error:" << query.lastError().text();
    }
    return success;
}

// ── Read ──────────────────────────────────────────────────────────────────────

std::optional<int> SnapshotsTable::idForDate(const QDate& date) const
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT id FROM snapshots WHERE date = ?;"));
    query.addBindValue(date.toString(Qt::ISODate));

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }
    return query.value(0).toInt();
}

std::optional<QPair<int, QDate>> SnapshotsTable::latest() const
{
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT id, date FROM snapshots ORDER BY date DESC LIMIT 1;"))) {
        return std::nullopt;
    }

    if (!query.next()) {
        return std::nullopt;
    }

    const int   snapshotId = query.value(0).toInt();
    const QDate date       = QDate::fromString(query.value(1).toString(), Qt::ISODate);
    return qMakePair(snapshotId, date);
}

QVector<QDate> SnapshotsTable::allDates() const
{
    QVector<QDate> result;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT date FROM snapshots ORDER BY date DESC;"))) {
        return result;
    }

    while (query.next()) {
        result << QDate::fromString(query.value(0).toString(), Qt::ISODate);
    }
    return result;
}