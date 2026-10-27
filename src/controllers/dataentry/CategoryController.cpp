#include "CategoryController.h"

#include "db/Database.h"
#include "models/Category.h"

#include <QColor>
#include <QDebug>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <Qt>
#include <QtLogging>
#include <QtMinMax>
#include <qtmetamacros.h> // for the emit macro (misc-include-cleaner)

namespace {
constexpr int kStatusClearDelayMs = 3000; // how long a status message stays visible
constexpr int kMaxShadeSteps = 10;        // how many lightness steps to try before giving up
} // namespace

CategoryController::CategoryController(QObject* parent)
    : QObject(parent)
    , m_status(this, kStatusClearDelayMs, [this]() { emit statusChanged(); })
{
    // DB-backed (see ARCHITECTURE.MD § Category Management): load whatever is
    // already persisted, or seed the original mock defaults on a fresh DB.
    // Only once per database: an empty table after "Reset Database" is not a
    // fresh DB, and stays empty (#37).
    m_categories = Database::instance().loadCategories();
    if (m_categories.isEmpty() && !Database::instance().defaultsWritten(Database::Defaults::Categories)) {
        seedDefaults();
        m_categories = Database::instance().loadCategories(); // all of them, or none
    }
}

void CategoryController::reload()
{
    // Deliberately no reseed-if-empty here — see the header doc comment.
    m_categories = Database::instance().loadCategories();
    emit categoriesChanged();
}

void CategoryController::setStatus(StatusMessage::Level level, const QString& msg)
{
    m_status.show(level, msg);
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

    // All the defaults and their record in one SQL transaction: when a write
    // fails, nothing is written, and the next start tries again (#37).
    Database& database = Database::instance();
    const bool written = database.writeDefaults(Database::Defaults::Categories, [&database]() {
        for (const SeedCat& seedCat : kSeed) {
            const int catId = database.addCategory(seedCat.name, seedCat.color);
            if (catId < 0) {
                return false;
            }
            for (const SeedSub& seedSub : seedCat.subs) {
                if (database.addSubcategory(catId, seedSub.name, seedSub.color) < 0) {
                    return false;
                }
            }
        }
        return true;
    });
    if (!written) {
        qWarning() << "[CategoryController] could not write the default categories:" << database.lastError();
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
    for (const Data::Category& cat : m_categories) {
        if (cat.name == categoryName) {
            return cat.color;
        }
    }
    return {};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from CategoriesManagePage.qml.
QString CategoryController::subcategoryColor(const QString& categoryName, const QString& subcategoryName) const
{
    for (const Data::Category& cat : m_categories) {
        if (cat.name == categoryName) {
            for (const Data::Subcategory& sub : cat.subcategories) {
                if (sub.name == subcategoryName) {
                    return sub.color;
                }
            }
        }
    }
    return {};
}

QString CategoryController::suggestSubcategoryColor(int categoryIndex) const
{
    static const QString kFallback = QStringLiteral("#89b4fa"); // app default accent

    if (categoryIndex < 0 || categoryIndex >= m_categories.size()) {
        return kFallback;
    }

    const Data::Category& cat = m_categories.at(categoryIndex);
    const QColor base(cat.color);
    if (!base.isValid()) {
        return cat.color.isEmpty() ? kFallback : cat.color;
    }

    QSet<QString> used;
    for (const Data::Subcategory& sub : cat.subcategories) {
        used.insert(sub.color.toLower());
    }

    // QColor::getHslF/fromHslF take float* / float in Qt 6 (not qreal), even
    // though qreal is double on desktop platforms — verified against the
    // build error against C:/Qt/6.11.1/mingw_64/include/QtGui/qcolor.h.
    float hue = 0.0F;
    float sat = 0.0F;
    float light = 0.0F;
    float alpha = 0.0F;
    base.getHslF(&hue, &sat, &light, &alpha);

    // Walk outward from the base lightness in increasing steps, alternating
    // lighter/darker, until a shade distinct from every sibling subcategory
    // color is found (siblings only — other categories' colors are allowed
    // to repeat, they're a different visual group).
    for (int step = 1; step <= kMaxShadeSteps; ++step) {
        const float delta = static_cast<float>(step) * 0.08F; // ~8% lightness per step
        for (const int sign : { 1, -1 }) {
            const float candidateL = qBound(0.05F, light + (static_cast<float>(sign) * delta), 0.95F);
            const QColor candidate = QColor::fromHslF(hue, sat, candidateL, alpha);
            QString hex = candidate.name(QColor::HexRgb);
            if (!used.contains(hex.toLower())) {
                return hex;
            }
        }
    }

    // Every tried shade collided (many existing subcategories) — fall back
    // to the category's own color; the user can still recolor manually.
    return cat.color;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from CategoriesManagePage.qml.
void CategoryController::addCategory(const QString& name, const QString& color)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    for (const Data::Category& cat : m_categories) {
        if (cat.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  A category with this name already exists"));
            return; // no duplicate names
        }
    }

    const int categoryId = Database::instance().addCategory(trimmed, color);
    if (categoryId < 0) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not save the category"));
        return; // DB write failed — nothing to show, avoid a phantom entry
    }

    Data::Category cat;
    cat.id    = categoryId;
    cat.name  = trimmed;
    cat.color = color;
    m_categories.append(cat);
    emit categoriesChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Category added"));
}

void CategoryController::renameCategory(int index, const QString& newName)
{
    if (index < 0 || index >= m_categories.size()) {
        return;
    }
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == m_categories.at(index).name) {
        return;
    }
    // Same check as addCategory() (#34). A change of case only is a rename.
    for (qsizetype i = 0; i < m_categories.size(); ++i) {
        if (i != index && m_categories.at(i).name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  A category with this name already exists"));
            return;
        }
    }
    // The rename also reaches the transactions and the rules that use the name.
    if (!Database::instance().renameCategory(m_categories.at(index).id, trimmed)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not rename the category"));
        return;
    }
    // index bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_categories[index].name = trimmed; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit categoriesChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Category renamed"));
}

void CategoryController::setCategoryColor(int index, const QString& color)
{
    if (index < 0 || index >= m_categories.size()) {
        return;
    }
    if (m_categories.at(index).color == color) {
        return;
    }
    if (!Database::instance().setCategoryColor(m_categories.at(index).id, color)) {
        return;
    }
    // index bounds-checked above; QList's at() is const-only, so mutating
    // through operator[] is genuinely required here.
    m_categories[index].color = color; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit categoriesChanged();
}

void CategoryController::removeCategory(int index)
{
    if (index < 0 || index >= m_categories.size()) {
        return;
    }
    if (!Database::instance().removeCategory(m_categories.at(index).id)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not remove the category"));
        return;
    }
    m_categories.removeAt(index);
    emit categoriesChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Category removed"));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from CategoriesManagePage.qml.
void CategoryController::addSubcategory(int categoryIndex, const QString& name, const QString& color)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size()) {
        return;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    // categoryIndex bounds-checked above; QList's at() is const-only, so
    // mutating through operator[] is genuinely required here.
    Data::Category& cat = m_categories[categoryIndex]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    for (const Data::Subcategory& sub : cat.subcategories) {
        if (sub.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  A subcategory with this name already exists"));
            return; // no duplicate names within the same category
        }
    }

    const int subcategoryId = Database::instance().addSubcategory(cat.id, trimmed, color);
    if (subcategoryId < 0) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not save the subcategory"));
        return;
    }

    cat.subcategories.append(Data::Subcategory{ subcategoryId, trimmed, color });
    emit categoriesChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Subcategory added"));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from CategoriesManagePage.qml as (categoryIndex, subIndex, ...).
void CategoryController::renameSubcategory(int categoryIndex, int subIndex, const QString& newName)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size()) {
        return;
    }
    // categoryIndex bounds-checked above; QList's at() is const-only, so
    // mutating through operator[] is genuinely required here.
    Data::Category& cat = m_categories[categoryIndex]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (subIndex < 0 || subIndex >= cat.subcategories.size()) {
        return;
    }
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || trimmed == cat.subcategories.at(subIndex).name) {
        return;
    }
    // Same check as addSubcategory(), in this category (#34).
    for (qsizetype i = 0; i < cat.subcategories.size(); ++i) {
        if (i != subIndex && cat.subcategories.at(i).name.compare(trimmed, Qt::CaseInsensitive) == 0) {
            setStatus(StatusMessage::Level::Warning, QStringLiteral("⚠  A subcategory with this name already exists"));
            return;
        }
    }
    // The rename also reaches the transactions and the rules of this category.
    if (!Database::instance().renameSubcategory(cat.subcategories.at(subIndex).id, trimmed)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not rename the subcategory"));
        return;
    }
    // subIndex bounds-checked above; same const-only at() limitation.
    cat.subcategories[subIndex].name = trimmed; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit categoriesChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Subcategory renamed"));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from CategoriesManagePage.qml as (categoryIndex, subIndex, ...).
void CategoryController::setSubcategoryColor(int categoryIndex, int subIndex, const QString& color)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size()) {
        return;
    }
    // categoryIndex bounds-checked above; QList's at() is const-only, so
    // mutating through operator[] is genuinely required here.
    Data::Category& cat = m_categories[categoryIndex]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (subIndex < 0 || subIndex >= cat.subcategories.size()) {
        return;
    }
    if (cat.subcategories.at(subIndex).color == color) {
        return;
    }
    if (!Database::instance().setSubcategoryColor(cat.subcategories.at(subIndex).id, color)) {
        return;
    }
    // subIndex bounds-checked above; same const-only at() limitation.
    cat.subcategories[subIndex].color = color; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    emit categoriesChanged();
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) — Q_INVOKABLE, called
// positionally from CategoriesManagePage.qml as (categoryIndex, subIndex).
void CategoryController::removeSubcategory(int categoryIndex, int subIndex)
{
    if (categoryIndex < 0 || categoryIndex >= m_categories.size()) {
        return;
    }
    // categoryIndex bounds-checked above; QList's at() is const-only, so
    // mutating through operator[] is genuinely required here.
    Data::Category& cat = m_categories[categoryIndex]; // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if (subIndex < 0 || subIndex >= cat.subcategories.size()) {
        return;
    }
    if (!Database::instance().removeSubcategory(cat.subcategories.at(subIndex).id)) {
        setStatus(StatusMessage::Level::Danger, QStringLiteral("⚠  Could not remove the subcategory"));
        return;
    }
    cat.subcategories.removeAt(subIndex);
    emit categoriesChanged();
    setStatus(StatusMessage::Level::Success, QStringLiteral("✓  Subcategory removed"));
}
