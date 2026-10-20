#ifndef FINANCEDASHBOARD_DB_TABLES_SETTINGSTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_SETTINGSTABLE_H

#include <QSqlDatabase>
#include <QString>

// ── SettingsTable ─────────────────────────────────────────────────────────────
//
// Owns all SQL that reads and writes the `settings` table: the state of the
// app that belongs to this database file, as key and value text. Schema
// version 3 adds the table (Database::migrateTo3(), with its own SQL): it is
// not in the base schema, so this class has no createTable().
//
//   CREATE TABLE settings (
//     key   TEXT NOT NULL PRIMARY KEY,
//     value TEXT NOT NULL
//   );
//
// Keys:
//   seeded.categories  "1" when the default categories were written once (#37)
//   seeded.accounts    "1" when the default account types, account categories
//                      and demo accounts were written once (#37)

class SettingsTable
{
public:
    explicit SettingsTable(QString connectionName);

    // The value of `key`, or an empty string when the key is not set.
    [[nodiscard]] QString value(const QString& key) const;
    // Sets `key` to `value` (adds the key, or replaces its value).
    bool setValue(const QString& key, const QString& value);

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_SETTINGSTABLE_H
