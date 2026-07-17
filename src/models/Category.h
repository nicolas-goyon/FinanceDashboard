#ifndef FINANCEDASHBOARD_MODELS_CATEGORY_H
#define FINANCEDASHBOARD_MODELS_CATEGORY_H

#include <QString>
#include <QVector>

// ── Data::Category / Data::Subcategory ───────────────────────────────────────
//
// Plain data holders for the category/subcategory list, mirroring the row
// shape of the `categories` / `subcategories` DB tables (see
// db/tables/CategoriesTable.h, db/tables/SubcategoriesTable.h). `id` is the
// SQLite row id (-1 for a not-yet-persisted instance); CategoryController
// uses it to target CRUD operations by row instead of by name — see
// ARCHITECTURE.MD § Category Management.

namespace Data {

struct Subcategory {
    int     id = -1;
    QString name;
    QString color;
};

struct Category {
    int                  id = -1;
    QString              name;
    QString              color;
    QVector<Subcategory> subcategories;
};

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_CATEGORY_H
