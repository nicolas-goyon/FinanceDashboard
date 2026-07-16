#include "CategoryController.h"

#include "db/Database.h"

#include <QColor>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

CategoryController::CategoryController(QObject* parent)
    : QObject(parent)
{
    // DB-backed (see ARCHITECTURE.MD § Category Management): load whatever is
    // already persisted, or seed the original mock defaults on a fresh DB.
    m_categories = Database::instance().loadCategories();
    if (m_categories.isEmpty())
        seedDefaults();
}

void CategoryController::reload()
{
    // Deliberately no reseed-if-empty here — see the header doc comment.
    m_categories = Database::instance().loadCategories();
    emit categoriesChanged();
}

void CategoryController::setStatus(const QString& msg)
{
    m_statusText = msg;
    emit statusChanged();
    QTimer::singleShot(3000, this, [this]() {
        m_statusText.clear();
        emit statusChanged();
    });
}

void CategoryController::seedDefaults()
{
    // Same category names/colors/subcategories as the original in-memory
    // mock list, now written through Database so they persist across
    // restarts instead of resetting every run. Subcategory colors are
    // shades of their parent category's color (same hue/saturation,
    // lightness offset by ~8% per step) rather than unrelated Mocha
    // accents, so a category and its subcategories read as one visual
    // family in legends/swatches — same lightness-step convention as
    // suggestSubcategoryColor() below.
    struct SeedSub { QString name; QString color; };
    struct SeedCat { QString name; QString color; QVector<SeedSub> subs; };

    static const QVector<SeedCat> kSeed = {
        { "Rent", "#b4befe", {
            { "Base rent", "#b4befe" },
            { "Charges",   "#8c9bfd" },
        } },
        { "Groceries", "#a6e3a1", {
            { "Supermarket", "#a6e3a1" },
            { "Market",      "#88da82" },
        } },
        { "Transport", "#89dceb", {
            { "Public transit", "#89dceb" },
            { "Fuel",           "#66d2e5" },
            { "Rideshare",      "#ace6f1" },
        } },
        { "Utilities", "#f9e2af", {
            { "Electricity", "#f9e2af" },
            { "Internet",    "#f6d489" },
        } },
        { "Leisure", "#cba6f7", {
            { "Restaurants",   "#cba6f7" },
            { "Subscriptions", "#b581f4" },
        } },
        { "Other", "#6c7086", {} },
    };

    Database& db = Database::instance();
    for (const SeedCat& sc : kSeed) {
        const int catId = db.addCategory(sc.name, sc.color);
        if (catId < 0)
            continue; // DB not open / insert failed — skip, retried next launch

        Data::Category cat;
        cat.id    = catId;
        cat.name  = sc.name;
        cat.color = sc.color;

        for (const SeedSub& ss : sc.subs) {
            const int subId = db.addSubcategory(catId, ss.name, ss.color);
            if (subId < 0)
                continue;
            cat.subcategories.append(Data::Subcategory{ subId, ss.name, ss.color });
        }

        m_categories.append(cat);
    }
}

QVariantList CategoryController::categories() const
{
    QVariantList out;
    out.reserve(m_categories.size());
    for (const Data::Category& cat : m_categories) {
        QVariantList subs;
        subs.reserve(cat.subcategories.size());
        for (const Data::Subcategory& sub : cat.subcategories) {
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
    for (const Data::Category& cat : m_categories)
        if (cat.name == categoryName)
            return cat.color;
    return QString();
}

QString CategoryController::subcategoryColor(const QString& categoryName, const QString& subcategoryName) const
{
    for (const Data::Category& cat : m_categories)
        if (cat.name == categoryName)
            for (const Data::Subcategory& sub : cat.subcategories)
                if (sub.name == subcategoryName)
                    return sub.color;
    return QString();
}

QString CategoryController::suggestSubcategoryColor(int categoryIndex) const
{
    static const QString kFallback = QStringLiteral("#89b4fa"); // app default accent

    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return kFallback;

    const Data::Category& cat = m_categories[categoryIndex];
    const QColor base(cat.color);
    if (!base.isValid())
        return cat.color.isEmpty() ? kFallback : cat.color;

    QSet<QString> used;
    for (const Data::Subcategory& sub : cat.subcategories)
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
    for (const Data::Category& cat : m_categories)
        if (cat.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(QStringLiteral("⚠  A category with this name already exists"));
            return; // no duplicate names
        }

    const int id = Database::instance().addCategory(trimmed, color);
    if (id < 0) {
        setStatus(QStringLiteral("⚠  Could not save the category"));
        return; // DB write failed — nothing to show, avoid a phantom entry
    }

    Data::Category cat;
    cat.id    = id;
    cat.name  = trimmed;
    cat.color = color;
    m_categories.append(cat);
    emit categoriesChanged();
    setStatus(QStringLiteral("✓  Category added"));
}

void CategoryController::renameCategory(int index, const QString& newName)
{
    if (index < 0 || index >= m_categories.size())
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == m_categories[index].name)
        return;
    if (!Database::instance().renameCategory(m_categories[index].id, trimmed)) {
        setStatus(QStringLiteral("⚠  Could not rename the category"));
        return;
    }
    m_categories[index].name = trimmed;
    emit categoriesChanged();
    setStatus(QStringLiteral("✓  Category renamed"));
}

void CategoryController::setCategoryColor(int index, const QString& color)
{
    if (index < 0 || index >= m_categories.size())
        return;
    if (m_categories[index].color == color)
        return;
    if (!Database::instance().setCategoryColor(m_categories[index].id, color))
        return;
    m_categories[index].color = color;
    emit categoriesChanged();
}

void CategoryController::removeCategory(int index)
{
    if (index < 0 || index >= m_categories.size())
        return;
    if (!Database::instance().removeCategory(m_categories[index].id)) {
        setStatus(QStringLiteral("⚠  Could not remove the category"));
        return;
    }
    m_categories.removeAt(index);
    emit categoriesChanged();
    setStatus(QStringLiteral("✓  Category removed"));
}

void CategoryController::addSubcategory(int categoryIndex, const QString& name, const QString& color)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    Data::Category& cat = m_categories[categoryIndex];
    for (const Data::Subcategory& sub : cat.subcategories)
        if (sub.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(QStringLiteral("⚠  A subcategory with this name already exists"));
            return; // no duplicate names within the same category
        }

    const int id = Database::instance().addSubcategory(cat.id, trimmed, color);
    if (id < 0) {
        setStatus(QStringLiteral("⚠  Could not save the subcategory"));
        return;
    }

    cat.subcategories.append(Data::Subcategory{ id, trimmed, color });
    emit categoriesChanged();
    setStatus(QStringLiteral("✓  Subcategory added"));
}

void CategoryController::renameSubcategory(int categoryIndex, int subIndex, const QString& newName)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    Data::Category& cat = m_categories[categoryIndex];
    if (subIndex < 0 || subIndex >= cat.subcategories.size())
        return;
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == cat.subcategories[subIndex].name)
        return;
    if (!Database::instance().renameSubcategory(cat.subcategories[subIndex].id, trimmed)) {
        setStatus(QStringLiteral("⚠  Could not rename the subcategory"));
        return;
    }
    cat.subcategories[subIndex].name = trimmed;
    emit categoriesChanged();
    setStatus(QStringLiteral("✓  Subcategory renamed"));
}

void CategoryController::setSubcategoryColor(int categoryIndex, int subIndex, const QString& color)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    Data::Category& cat = m_categories[categoryIndex];
    if (subIndex < 0 || subIndex >= cat.subcategories.size())
        return;
    if (cat.subcategories[subIndex].color == color)
        return;
    if (!Database::instance().setSubcategoryColor(cat.subcategories[subIndex].id, color))
        return;
    cat.subcategories[subIndex].color = color;
    emit categoriesChanged();
}

void CategoryController::removeSubcategory(int categoryIndex, int subIndex)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size())
        return;
    Data::Category& cat = m_categories[categoryIndex];
    if (subIndex < 0 || subIndex >= cat.subcategories.size())
        return;
    if (!Database::instance().removeSubcategory(cat.subcategories[subIndex].id)) {
        setStatus(QStringLiteral("⚠  Could not remove the subcategory"));
        return;
    }
    cat.subcategories.removeAt(subIndex);
    emit categoriesChanged();
    setStatus(QStringLiteral("✓  Subcategory removed"));
}
