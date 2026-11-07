#ifndef FINANCEDASHBOARD_MODELS_DEFAULTCATEGORIES_H
#define FINANCEDASHBOARD_MODELS_DEFAULTCATEGORIES_H

#include <QString>
#include <QVector>

// ── Default categories ───────────────────────────────────────────────────────
//
// The categories that a new database gets (CategoryController::seedDefaults()),
// that "Reset categories to the defaults" writes back (Database management),
// and that the mock data uses. One list for all three.
//
// The names are translated with the language of the app at the time they are
// written (context "DefaultCategories"): a database made in French gets French
// names. After that they are data: changing the language does not rename them.
//
// A subcategory gets a shade of the colour of its category (the same hue,
// another lightness), so that a category and its subcategories read as one
// family in the charts.
namespace Data {

struct DefaultSubcategory {
    QString name;
    QString color;
};

struct DefaultCategory {
    QString name;
    QString color;
    QVector<DefaultSubcategory> subcategories;
};

[[nodiscard]] QVector<DefaultCategory> defaultCategories();

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_DEFAULTCATEGORIES_H
