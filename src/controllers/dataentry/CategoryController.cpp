#include "CategoryController.h"

#include <QColor>
#include <QSet>
#include <QVariantMap>

CategoryController::CategoryController(QObject* parent)
    : QObject(parent)
{
    // MOCK: seed data, in-memory only — resets on app restart, no DB table yet
    // (see ARCHITECTURE.MD § Category Management). Same category names as the
    // old DataEntryController mock list, now with a color each and a few
    // illustrative subcategories. Subcategory colors are shades of their
    // parent category's color (same hue/saturation, lightness offset by
    // ~8% per step) rather than unrelated Mocha accents, so a category and
    // its subcategories read as one visual family in legends/swatches —
    // same lightness-step convention as suggestSubcategoryColor() below,
    // computed offline with QColor::getHslF/fromHslF so the seed data and
    // the live "add subcategory" default stay visually consistent.
    m_categories = {
        Category{ "Rent", "#b4befe", {
            Subcategory{ "Base rent", "#b4befe" },
            Subcategory{ "Charges",   "#8c9bfd" },
        } },
        Category{ "Groceries", "#a6e3a1", {
            Subcategory{ "Supermarket", "#a6e3a1" },
            Subcategory{ "Market",      "#88da82" },
        } },
        Category{ "Transport", "#89dceb", {
            Subcategory{ "Public transit", "#89dceb" },
            Subcategory{ "Fuel",           "#66d2e5" },
            Subcategory{ "Rideshare",      "#ace6f1" },
        } },
        Category{ "Utilities", "#f9e2af", {
            Subcategory{ "Electricity", "#f9e2af" },
            Subcategory{ "Internet",    "#f6d489" },
        } },
        Category{ "Leisure", "#cba6f7", {
            Subcategory{ "Restaurants",    "#cba6f7" },
            Subcategory{ "Subscriptions",  "#b581f4" },
        } },
        Category{ "Other", "#6c7086", {} },
    };
}

QVariantList CategoryController::categories() const
{
    QVariantList out;
    out.reserve(m_categories.size());
    for (const Category& cat : m_categories) {
        QVariantList subs;
        subs.reserve(cat.subcategories.size());
        for (const Subcategory& sub : cat.subcategories) {
            subs.append(QVariantMap{
                { "name",  sub.name },
                { "color", sub.color },
            });
        }
        out.append(QVariantMap{
            { "name",          cat.name },
            { "color",         cat.color },
            { "subcategories", subs },
        });
    }
    return out;
}

QString CategoryController::categoryColor(const QString& categoryName) const
{
    for (const Category& cat : m_categories)
        if (cat.name == categoryName)
            return cat.color;
    return QString();
}

QString CategoryController::subcategoryColor(const QString& categoryName, const QString& subcategoryName) const
{
    for (const Category& cat : m_categories)
        if (cat.name == categoryName)
            for (const Subcategory& sub : cat.subcategories)
                if (sub.name == subcategoryName)
                    return sub.color;
    return QString();
}

QString CategoryController::suggestSubcategoryColor(int categoryIndex) const
{
    static const QString kFallback = QStringLiteral("#89b4fa"); // app default accent

    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return kFallback;

    const Category& cat = m_categories[categoryIndex];
    const QColor base(cat.color);
    if (!base.isValid())
        return cat.color.isEmpty() ? kFallback : cat.color;

    QSet<QString> used;
    for (const Subcategory& sub : cat.subcategories)
        used.insert(sub.color.toLower());

    // QColor::getHslF/fromHslF take float* / float in Qt 6 (not qreal), even
    // though qreal is double on desktop platforms — verified against the
    // build error against C:/Qt/6.11.1/mingw_64/include/QtGui/qcolor.h.
    float h, s, l, a;
    base.getHslF(&h, &s, &l, &a);

    // Walk outward from the base lightness in increasing steps, alternating
    // lighter/darker, until a shade distinct from every sibling subcategory
    // color is found (siblings only — other categories' colors are allowed
    // to repeat, they're a different visual group).
    for (int step = 1; step <= 10; ++step) {
        const float delta = step * 0.08f; // ~8% lightness per step
        for (int sign : { 1, -1 }) {
            const float candidateL = qBound(0.05f, l + sign * delta, 0.95f);
            const QColor candidate = QColor::fromHslF(h, s, candidateL, a);
            const QString hex = candidate.name(QColor::HexRgb);
            if (!used.contains(hex.toLower()))
                return hex;
        }
    }

    // Every tried shade collided (many existing subcategories) — fall back
    // to the category's own color; the user can still recolor manually.
    return cat.color;
}

void CategoryController::addCategory(const QString& name, const QString& color)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    for (const Category& cat : m_categories)
        if (cat.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return; // no duplicate names

    m_categories.append(Category{ trimmed, color, {} });
    emit categoriesChanged();
}

void CategoryController::renameCategory(int index, const QString& newName)
{
    if (index < 0 || index >= m_categories.size())
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == m_categories[index].name)
        return;
    m_categories[index].name = trimmed;
    emit categoriesChanged();
}

void CategoryController::setCategoryColor(int index, const QString& color)
{
    if (index < 0 || index >= m_categories.size())
        return;
    if (m_categories[index].color == color)
        return;
    m_categories[index].color = color;
    emit categoriesChanged();
}

void CategoryController::removeCategory(int index)
{
    if (index < 0 || index >= m_categories.size())
        return;
    m_categories.removeAt(index);
    emit categoriesChanged();
}

void CategoryController::addSubcategory(int categoryIndex, const QString& name, const QString& color)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    Category& cat = m_categories[categoryIndex];
    for (const Subcategory& sub : cat.subcategories)
        if (sub.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return; // no duplicate names within the same category

    cat.subcategories.append(Subcategory{ trimmed, color });
    emit categoriesChanged();
}

void CategoryController::renameSubcategory(int categoryIndex, int subIndex, const QString& newName)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    Category& cat = m_categories[categoryIndex];
    if (subIndex < 0 || subIndex >= cat.subcategories.size())
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == cat.subcategories[subIndex].name)
        return;
    cat.subcategories[subIndex].name = trimmed;
    emit categoriesChanged();
}

void CategoryController::setSubcategoryColor(int categoryIndex, int subIndex, const QString& color)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    Category& cat = m_categories[categoryIndex];
    if (subIndex < 0 || subIndex >= cat.subcategories.size())
        return;
    if (cat.subcategories[subIndex].color == color)
        return;
    cat.subcategories[subIndex].color = color;
    emit categoriesChanged();
}

void CategoryController::removeSubcategory(int categoryIndex, int subIndex)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    Category& cat = m_categories[categoryIndex];
    if (subIndex < 0 || subIndex >= cat.subcategories.size())
        return;
    cat.subcategories.removeAt(subIndex);
    emit categoriesChanged();
}
