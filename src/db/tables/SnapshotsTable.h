#ifndef FINANCEDASHBOARD_DB_TABLES_SNAPSHOTSTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_SNAPSHOTSTABLE_H

#include <optional>
#include <QString>
#include <QDate>
#include <QVector>
#include <QPair>
#include <QSqlDatabase>

// ── SnapshotsTable ────────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `snapshots` table.
//
//   CREATE TABLE snapshots (
//     id   INTEGER PRIMARY KEY AUTOINCREMENT,
//     date TEXT    NOT NULL UNIQUE          -- ISO-8601: YYYY-MM-DD
//   );
//
// Constructed with a connection name; looks up the live QSqlDatabase at
// call time via QSqlDatabase::database(connectionName), so it is safe to
// construct before Database::open() is called.

class SnapshotsTable
{
public:
    explicit SnapshotsTable(QString connectionName);

    // Create the table (and any related indexes).  Called once by Database::open().
    bool createTable();

    // Insert the date if it does not already exist.
    // Idempotent — safe to call every save.
    bool upsert(const QDate& date);

    // Return the row id for a given date, or nullopt if not found.
    [[nodiscard]] std::optional<int> idForDate(const QDate& date) const;

    // Return {id, date} of the snapshot with the highest date, or nullopt.
    [[nodiscard]] std::optional<QPair<int, QDate>> latest() const;

    // All stored dates, newest first.
    [[nodiscard]] QVector<QDate> allDates() const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_SNAPSHOTSTABLE_H
