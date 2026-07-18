# Category Management ("Categories & Subcategories")

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

Reached from the sidebar (Transactions → "Categories & Subcategories"). Lets the user add,
rename, recolor, and remove categories and their subcategories — the same list read everywhere
a transaction gets classified (the statement-import review table on `ImportSnapshotPage.qml`).

- **Replaces keyword auto-suggestion.** The previous system (`utils/CategoryKeywords.h`)
  guessed a category by matching hardcoded tokens against the transaction description — not
  anything correlated to the user's own history. Removed per product decision: categories/
  subcategories are a user-managed list, picked manually everywhere, no auto-fill.
- **`CategoryController` (src/controllers/dataentry/CategoryController.h/.cpp) is a
  `QML_SINGLETON`**, unlike most other controllers (one instance per page). Categories are
  cross-cutting — edited on one page, consumed on two others — so a per-page instance would
  silently reset on every page navigation.
- **`reload()`** re-reads `categories`/`subcategories` from `Database` and re-emits
  `categoriesChanged()` — deliberately does **not** reseed defaults if the DB comes back empty
  (unlike the constructor, which seeds on first launch against a genuinely empty DB). See
  `docs/database-management.md` for why this matters for "Reset Database."
- **`statusText`/`statusChanged()`** — every CRUD method calls a private `setStatus(msg)` on
  both success ("✓ Category added") and failure ("⚠ A category with this name already
  exists"/"⚠ Could not save the category"), auto-clearing after 3s. Color-only changes
  (`setCategoryColor`/`setSubcategoryColor`) stay silent on purpose, matching
  `AccountController::setAccountCategoryColor()`'s convention.
  `CategoriesManagePage.qml`'s header shows it via a `StatusBadge`.
- **Data shape**, rebuilt into a `QVariantList` on every `categories()` read (no separate
  `QAbstractListModel`): `[{ name, color, subcategories: [{ name, color }, ...] }, ...]`. Row
  ids (`Data::Category::id` / `Data::Subcategory::id`) are kept in the internal
  `QVector<Data::Category> m_categories` only, not exposed in this `QVariantList` — QML still
  addresses categories/subcategories by list index.
- **CRUD**: `addCategory`/`renameCategory`/`setCategoryColor`/`removeCategory` and the
  subcategory equivalents (indexed by `categoryIndex`/`subIndex` into the current list).
  `categoryColor(name)`/`subcategoryColor(categoryName, subName)` are convenience lookups for
  anywhere that only has the category as a string (e.g. a stored transaction row).
- **`suggestSubcategoryColor(categoryIndex)`** — `Q_INVOKABLE`, returns the default color
  `CategoriesManagePage.qml` pre-fills when adding a new subcategory: the parent category's own
  color, shifted lighter/darker in the HSL lightness channel (`QColor::getHslF`/`fromHslF`) in
  increasing ~8% steps, alternating sign, until a shade distinct from that category's *existing
  subcategory colors* is found (siblings only). Falls back to the category's own color if
  `categoryIndex` is invalid or every tried shade collides.
- **Scope: SQLite-backed** — `categories`/`subcategories` tables (below), the first domain in
  the app wired to the real DB. On construction, `CategoryController` calls
  `Database::instance().loadCategories()`; on an empty DB (first launch) it calls the private
  `seedDefaults()`. Every CRUD method writes through `Database::instance()` first and only
  updates `m_categories` (and emits `categoriesChanged()`) if that write succeeded.

## Category/subcategory DB schema

```sql
CREATE TABLE categories (
  id    INTEGER PRIMARY KEY AUTOINCREMENT,
  name  TEXT    NOT NULL UNIQUE,
  color TEXT    NOT NULL
);

CREATE TABLE subcategories (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  category_id INTEGER NOT NULL,
  name        TEXT    NOT NULL,
  color       TEXT    NOT NULL,
  UNIQUE(category_id, name)
);
CREATE INDEX idx_subcategories_category ON subcategories(category_id);
```

- **`CategoriesTable`/`SubcategoriesTable`** (`src/db/tables/`) own all SQL for their table:
  `createTable()`, CRUD by row id, `all()`/`allForCategory(id)` readers ordered by row id
  (insertion order — there is no explicit sort-order column).
- **No DB-level cascade** (no foreign keys are declared anywhere in this schema).
  `Database::removeCategory(id)` does the cascade manually in a transaction:
  `SubcategoriesTable::removeForCategory(id)` then `CategoriesTable::remove(id)`.
- **`Database::loadCategories()`** composes both tables: reads all categories, then fills each
  one's `subcategories` from `SubcategoriesTable::allForCategory(id)`.
- **Duplicate-name checks stay in `CategoryController`, not SQL** — the controller checks for a
  case-insensitive duplicate name before mutating its in-memory list; the DB layer's `UNIQUE`
  constraints are a backstop, not the primary validation path.
- **`CategoriesManagePage.qml`**: color picker uses `QtQuick.Dialogs.ColorDialog` (native
  platform dialog, verified against
  doc.qt.io/qt-6/qml-qtquick-dialogs-colordialog.html). Each edit dialog owns a
  `selectedColor: string` property (hex) and a matching `ColorDialog` instance; the hex swatch
  button copies `selectedColor` into the `ColorDialog` right before `open()` (a declarative
  binding breaks permanently the first time the dialog's own `selectedColor` is set
  imperatively). One shared `Popup` handles both add and rename/recolor for categories, and a
  second for subcategories — an `editIndex`/`editSubIndex` property (`-1` = add mode) switches
  the dialog's title/button label and which controller method gets called on confirm.
