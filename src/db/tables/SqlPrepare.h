#ifndef FINANCEDASHBOARD_DB_TABLES_SQLPREPARE_H
#define FINANCEDASHBOARD_DB_TABLES_SQLPREPARE_H

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QtLogging>

namespace Sql {

// QSqlQuery::prepare() returns false when SQLite cannot prepare the
// statement: a damaged file, a missing table or column. exec() then fails
// with a misleading error ("Parameter count mismatch"). So each table class
// checks prepare(), logs the real error with this function and returns its
// failure value (#49). `where` names the method: "[AccountsTable] insert".
// https://doc.qt.io/qt-6/qsqlquery.html#prepare
inline void logPrepareError(const QSqlQuery& query, const char* where)
{
    qWarning().noquote() << where << "prepare error:" << query.lastError().text();
}

} // namespace Sql

#endif // FINANCEDASHBOARD_DB_TABLES_SQLPREPARE_H
