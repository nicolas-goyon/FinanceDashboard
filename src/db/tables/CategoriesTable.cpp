#include "CategoriesTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

CategoriesTable::CategoriesTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase CategoriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool CategoriesTable::createTable()
{
    QSqlQuery q(db());
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS categories ("
        "  id    INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name  TEXT    NOT NULL UNIQUE,"
        "  color TEXT    NOT NULL"
        ");"
    ));
    if (!ok)
        qWarning() << "[CategoriesTable] createTable error:" << q.lastError().text();
    return ok;
}

// ── Write ─────────────────────────────────────────────────────────────────────

int CategoriesTable::insert(const QString& name, const QString& color)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO categories (name, color) VALUES (?, ?);"
    ));
    q.addBindValue(name);
    q.addBindValue(color);

    if (!q.exec()) {
        qWarning() << "[CategoriesTable] insert error:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool CategoriesTable::rename(int id, const QString& newName)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE categories SET name = ? WHERE id = ?;"));
    q.addBindValue(newName);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[CategoriesTable] rename error:" << q.lastError().text();
    return ok;
}

bool CategoriesTable::setColor(int id, const QString& color)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE categories SET color = ? WHERE id = ?;"));
    q.addBindValue(color);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[CategoriesTable] setColor error:" << q.lastError().text();
    return ok;
}

bool CategoriesTable::remove(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM categories WHERE id = ?;"));
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[CategoriesTable] remove error:" << q.lastError().text();
    return ok;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QVector<Data::Category> CategoriesTable::all() const
{
    QVector<Data::Category> result;
    QSqlQuery q(db());
    if (!q.exec(QStringLiteral("SELECT id, name, color FROM categories ORDER BY id ASC;"))) {
        qWarning() << "[CategoriesTable] all error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Data::Category cat;
        cat.id    = q.value(0).toInt();
        cat.name  = q.value(1).toString();
        cat.color = q.value(2).toString();
        result << cat;
    }
    return result;
}
