#ifndef FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_CATEGORYCONTROLLER_H
#define FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_CATEGORYCONTROLLER_H

#include "models/Category.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqml.h>

// ── CategoryController ────────────────────────────────────────────────────────
//
// Session-wide source of truth for the category / subcategory list, each with
// its own color. Backs qml/pages/CategoriesManagePage.qml (add/rename/recolor/
// remove) and is read from wherever a transaction gets categorized: the
// review table on ImportSnapshotPage.qml (see § Category Management in
// ARCHITECTURE.MD).
//
// QML_SINGLETON (unlike every other controller here, which is instantiated
// per-page): categories are a cross-cutting concept edited from one page and
// consumed on two others, so it must be one shared instance for edits to show
// up immediately elsewhere in the same run — a per-page instance (the usual
// pattern) would silently reset on every page navigation.
//
// DB-backed (categories/subcategories tables — see db/tables/CategoriesTable.h,
// db/tables/SubcategoriesTable.h, Database::loadCategories() and friends):
// the constructor loads the current list from Database::instance(), or seeds
// the original mock defaults (and persists them) the very first time the app
// runs against an empty DB. Every CRUD method here writes through to the DB
// first and only updates the in-memory list (m_categories) if that write
// succeeded, so a failed DB write never produces a phantom in-memory-only
// entry that would vanish on the next reload.
//
// Lives in src/controllers/dataentry/ alongside AccountController and
// SnapshotImportController — grouped by domain (Data Entry) like every other
// controller folder, rather than its own single-class subfolder. Being a
// QML_SINGLETON is a registration detail (the macro), not a reason for a
// separate directory.
//
// Replaces the old CategoryKeywords.h keyword-suggestion pass (removed per
// product decision — no more auto-fill; users pick category/subcategory
// manually everywhere).
//
// Data shape (rebuilt into a QVariantList on every categories() read, same
// convention as SnapshotImportController::transactionRows() — no separate
// QAbstractListModel). Row ids (Data::Category::id / Data::Subcategory::id)
// are kept internally only, not exposed in this QVariantList — QML still
// addresses categories/subcategories by list index, unchanged from before
// the DB-backed rewrite:
//   categories: [{ name, color, subcategories: [{ name, color }, ...] }, ...]

class CategoryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(QString      statusText READ statusText NOTIFY statusChanged)

public:
    explicit CategoryController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList categories() const;
    [[nodiscard]] QString      statusText() const { return m_statusText; }

    // Convenience lookups (used to fill a color swatch given only names,
    // e.g. a transaction row that stores category/subcategory as strings).
    Q_INVOKABLE [[nodiscard]] QString categoryColor(const QString& categoryName) const;
    Q_INVOKABLE [[nodiscard]] QString subcategoryColor(const QString& categoryName, const QString& subcategoryName) const;

    // Default color to pre-fill the "add subcategory" dialog with: a lighter
    // or darker HSL shade of the parent category's own color, chosen to be
    // distinct from that category's existing subcategory colors (siblings
    // only — other categories' colors are allowed to repeat, per product
    // decision, since they're a different visual group).
    // Falls back to the category's own color if categoryIndex is invalid or
    // every tried shade collides (many existing subcategories). Pure
    // algorithm in C++, per the "UI in QML, algorithms in C++" rule —
    // CategoriesManagePage.qml's "+ Subcategory" action just calls it and
    // passes the result as the dialog's initial color; the user can still
    // override it via the color picker before saving.
    Q_INVOKABLE [[nodiscard]] QString suggestSubcategoryColor(int categoryIndex) const;

    // Category CRUD (index into the top-level categories list). Each of
    // these writes through Database::instance() first; the in-memory list
    // (and the categoriesChanged() signal) only updates if that write
    // succeeded.
    Q_INVOKABLE void addCategory(const QString& name, const QString& color);
    Q_INVOKABLE void renameCategory(int index, const QString& newName);
    Q_INVOKABLE void setCategoryColor(int index, const QString& color);
    Q_INVOKABLE void removeCategory(int index);

    // Subcategory CRUD (categoryIndex into categories, subIndex into that
    // category's subcategories). Same write-through-then-update convention.
    Q_INVOKABLE void addSubcategory(int categoryIndex, const QString& name, const QString& color);
    Q_INVOKABLE void renameSubcategory(int categoryIndex, int subIndex, const QString& newName);
    Q_INVOKABLE void setSubcategoryColor(int categoryIndex, int subIndex, const QString& color);
    Q_INVOKABLE void removeSubcategory(int categoryIndex, int subIndex);

    // Re-reads categories/subcategories from Database — no auto-reseed, even
    // if the result is empty (deliberately: this is also what "Reset
    // Database" in Database Management uses to refresh this singleton after
    // wiping the table, and that action must leave the list genuinely empty,
    // not silently repopulate it — see ARCHITECTURE.MD § Database
    // Management). "Reset to Mock" reseeds explicitly through Database
    // first, then calls this to pick the fresh rows up.
    Q_INVOKABLE void reload();

signals:
    void categoriesChanged();
    void statusChanged();

private:
    // Seeds the original mock category/subcategory list into the DB (and
    // m_categories) the first time the app runs against an empty categories
    // table. Called from the constructor, and from reload(), whenever
    // Database::loadCategories() comes back empty.
    void seedDefaults();

    // Sets a transient status message (e.g. "duplicate name" / "removed"),
    // auto-clearing after 3s — same pattern and timing as
    // AccountController::setStatus(), added here so both QML_SINGLETON
    // controllers with near-identical CRUD shapes give the same user
    // feedback on success/failure instead of CategoryController silently
    // no-op'ing (see ARCHITECTURE.MD § Shared Components).
    void setStatus(const QString& msg);

    QVector<Data::Category> m_categories;
    QString m_statusText;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_DATAENTRY_CATEGORYCONTROLLER_H
