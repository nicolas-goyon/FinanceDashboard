#include "SubcategoriesTable.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

SubcategoriesTable::SubcategoriesTable(const QString& connectionName)
    : m_connectionName(connectionName)
{}

QSqlDatabase SubcategoriesTable::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

// ── DDL ───────────────────────────────────────────────────────────────────────

bool SubcategoriesTable::createTable()
{
    QSqlQuery q(db());
    const bool ok =
        q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS subcategories ("
            "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  category_id INTEGER NOT NULL,"
            "  name        TEXT    NOT NULL,"
            "  color       TEXT    NOT NULL,"
            "  UNIQUE(category_id, name)"
            ");"
        )) &&
        q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_subcategories_category"
            " ON subcategories(category_id);"
        ));

    if (!ok)
        qWarning() << "[SubcategoriesTable] createTable error:" << q.lastError().text();
    return ok;
}

// ── Write ─────────────────────────────────────────────────────────────────────

int SubcategoriesTable::insert(int categoryId, const QString& name, const QString& color)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "INSERT INTO subcategories (category_id, name, color) VALUES (?, ?, ?);"
    ));
    q.addBindValue(categoryId);
    q.addBindValue(name);
    q.addBindValue(color);

    if (!q.exec()) {
        qWarning() << "[SubcategoriesTable] insert error:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool SubcategoriesTable::rename(int id, const QString& newName)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE subcategories SET name = ? WHERE id = ?;"));
    q.addBindValue(newName);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[SubcategoriesTable] rename error:" << q.lastError().text();
    return ok;
}

bool SubcategoriesTable::setColor(int id, const QString& color)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("UPDATE subcategories SET color = ? WHERE id = ?;"));
    q.addBindValue(color);
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[SubcategoriesTable] setColor error:" << q.lastError().text();
    return ok;
}

bool SubcategoriesTable::remove(int id)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM subcategories WHERE id = ?;"));
    q.addBindValue(id);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[SubcategoriesTable] remove error:" << q.lastError().text();
    return ok;
}

bool SubcategoriesTable::removeForCategory(int categoryId)
{
    QSqlQuery q(db());
    q.prepare(QStringLiteral("DELETE FROM subcategories WHERE category_id = ?;"));
    q.addBindValue(categoryId);

    const bool ok = q.exec();
    if (!ok)
        qWarning() << "[SubcategoriesTable] removeForCategory error:" << q.lastError().text();
    return ok;
}

// ── Read ──────────────────────────────────────────────────────────────────────

QVector<Data::Subcategory> SubcategoriesTable::allForCategory(int categoryId) const
{
    QVector<Data::Subcategory> result;
    QSqlQuery q(db());
    q.prepare(QStringLiteral(
        "SELECT id, name, color FROM subcategories"
        " WHERE category_id = ? ORDER BY id ASC;"
    ));
    q.addBindValue(categoryId);

    if (!q.exec()) {
        qWarning() << "[SubcategoriesTable] allForCategory error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Data::Subcategory sub;
        sub.id    = q.value(0).toInt();
        sub.name  = q.value(1).toString();
        sub.color = q.value(2).toString();
        result << sub;
    }
    return result;
}
