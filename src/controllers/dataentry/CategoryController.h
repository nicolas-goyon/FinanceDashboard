#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqml.h>

// ── CategoryController ────────────────────────────────────────────────────────
//
// Session-wide source of truth for the category / subcategory list, each with
// its own color. Backs qml/pages/CategoriesManagePage.qml (add/rename/recolor/
// remove) and is read from wherever a transaction gets categorized: the "Add
// transaction" form on DataEntryPage.qml and the review table on
// ImportSnapshotPage.qml (see § Category Management in ARCHITECTURE.MD).
//
// QML_SINGLETON (unlike every other controller here, which is instantiated
// per-page): categories are a cross-cutting concept edited from one page and
// consumed on two others, so it must be one shared instance for edits to show
// up immediately elsewhere in the same run — a per-page instance (the usual
// pattern) would silently reset on every page navigation. Still in-memory
// mock data only, same as the rest of the app (no DB table yet) — it just
// resets on app restart, consistent with the "in-memory mock" scope decided
// for this pass.
//
// Lives in src/controllers/dataentry/ alongside DataEntryController and
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
// QAbstractListModel):
//   categories: [{ name, color, subcategories: [{ name, color }, ...] }, ...]

class CategoryController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)

public:
    explicit CategoryController(QObject* parent = nullptr);

    QVariantList categories() const;

    // Convenience lookups (used to fill a color swatch given only names,
    // e.g. a transaction row that stores category/subcategory as strings).
    Q_INVOKABLE QString categoryColor(const QString& categoryName) const;
    Q_INVOKABLE QString subcategoryColor(const QString& categoryName, const QString& subcategoryName) const;

    // Default color to pre-fill the "add subcategory" dialog with: a lighter
    // or darker HSL shade of the parent category's own color, chosen to be
    // distinct from that category's existing subcategory colors (siblings
    // only — not checked against other categories). Returns the category's
    // own color unchanged if categoryIndex is invalid or no distinct shade
    // was found within the tried steps.
    Q_INVOKABLE QString suggestSubcategoryColor(int categoryIndex) const;

    // Category CRUD (index into the top-level categories list).
    Q_INVOKABLE void addCategory(const QString& name, const QString& color);
    Q_INVOKABLE void renameCategory(int index, const QString& newName);
    Q_INVOKABLE void setCategoryColor(int index, const QString& color);
    Q_INVOKABLE void removeCategory(int index);

    // Subcategory CRUD (categoryIndex into categories, subIndex into that
    // category's subcategories).
    Q_INVOKABLE void addSubcategory(int categoryIndex, const QString& name, const QString& color);
    Q_INVOKABLE void renameSubcategory(int categoryIndex, int subIndex, const QString& newName);
    Q_INVOKABLE void setSubcategoryColor(int categoryIndex, int subIndex, const QString& color);
    Q_INVOKABLE void removeSubcategory(int categoryIndex, int subIndex);

signals:
    void categoriesChanged();

private:
    struct Subcategory {
        QString name;
        QString color;
    };
    struct Category {
        QString name;
        QString color;
        QVector<Subcategory> subcategories;
    };

    QVector<Category> m_categories;
};
