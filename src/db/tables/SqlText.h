#ifndef FINANCEDASHBOARD_DB_TABLES_SQLTEXT_H
#define FINANCEDASHBOARD_DB_TABLES_SQLTEXT_H

#include <QString>

namespace Sql {

// Qt SQL binds a null QString as SQL NULL, and a column's DEFAULT does not
// apply to an explicit NULL. A TEXT NOT NULL DEFAULT '' column then fails with
// "NOT NULL constraint failed". A default-constructed QString and a QString
// after clear() are null; a QString from QML is not.
// Bind the text of such a column through this function.
// https://doc.qt.io/qt-6/qsqlquery.html#addBindValue
inline QString text(const QString& value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

} // namespace Sql

#endif // FINANCEDASHBOARD_DB_TABLES_SQLTEXT_H
