# Architecture Notes — Finance Dashboard

> **Audience:** developer / maintainer (Claude or human).  
> This file tracks design decisions, file responsibilities, and maintenance instructions. Keep it up to date when the structure changes.

---

## Project Identity

| Field | Value |
|---|---|
| Name | FinanceDashboard |
| Version | 0.2.0 |
| Language | C++17 |
| UI framework | **Qt Quick / QML + Qt Graphs** (migrating from Qt Widgets + Qt Charts) |
| Build system | CMake 3.20+ (`qt_add_qml_module`) |
| Database | SQLite via Qt6::Sql |
| Target platform | Windows (primary), Linux/macOS compatible |
| Reference project | [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples) — patterns for every chart type |

---

## App Status — PROJECT.md pages (UI pass)

**Design rule: UI in QML, algorithms/analytics/data in C++.**
Each page gets a C++ *controller* class (`QML_ELEMENT`, in `src/controllers/`) holding all logic; the `.qml` file only renders.

The app was restructured around the 6 pages from **PROJECT.md**. Current state: **UI-only pass** — every controller serves MOCK data (marked `[MOCK]` in the UI); DB wiring comes next.

| Page | Controller | Charts / widgets | Data |
|---|---|---|---|
| Overview (Net Worth) | `NetWorthController` | 4 KPI cards · stacked `AreaSeries` by account type · allocation donut (`PieSeries`) · net-change bars (green/red `BarSet`s) | MOCK |
| Cash Flow | `CashFlowController` | income/expenses grouped bars · rolling 3-month `LineSeries` · category donut w/ month `ComboBox` · stacked category bars · biggest-expenses table | MOCK |
| Savings | `SavingsController` | KPI cards · balance `LineSeries` per account · ceiling/goal progress bars (pure QML) · yearly interest grouped bars | MOCK |
| Investments | `InvestmentsController` | KPI cards · portfolio vs cumulative-deposits lines (gap = real P&L) · allocation donut · monthly perf % bars | MOCK |
| Projects | `ProjectsController` | project `ComboBox` · KPI cards · projection-vs-target lines · **loan simulator (REAL annuity math in C++)** · save-vs-borrow comparison | MOCK projects, real loan math |
| Data Entry / Reports | `DataEntryController` | snapshot form per account · transaction entry with **manual** category/subcategory picker (`CategoryController`, see § Category Management) · history table · annual summary | MOCK, nothing persisted |
| ↳ New Monthly Snapshot (import) | `SnapshotImportController` | template + file picker · statement header info · editable/categorizable transaction table · account auto-match · parse log popup — see § Statement Import | **REAL parsing**, MOCK save |
| ↳ Manage Categories | `CategoryController` (`QML_SINGLETON`) | add/rename/recolor/remove categories and subcategories — see § Category Management | **REAL** (SQLite-backed, `categories`/`subcategories` tables) |

**Previous generations kept on disk but NOT compiled/registered:**
- `qml/tabs/*` + `EntryController`, `DatabaseController`, `AnalyticsController`, `OverviewController`, `PortfolioController`, `ImportController` — the ported snapshot tabs. **Reuse these when wiring Data Entry / real DB** (they already do save/load/diff against `Database`).
- `src/MainWindow.*`, `src/tabs/*` — the original Qt Widgets app.

The `db/`, `models/`, `utils/` layers are still compiled and `main.cpp` still opens the SQLite DB — ready for the wiring phase. **Categories/subcategories are the first slice wired to the real DB** (`categories`/`subcategories` tables — see § Category Management); every other domain here is still MOCK.

---

## Directory Map

```
FinanceDashboard/
│
├── CMakeLists.txt          — qt_add_executable + qt_add_qml_module (URI "FinanceDashboard").
├── README.md               — User-facing: how to build and run.
├── ARCHITECTURE.MD         — This file.
├── PROJECT.md              — Product spec: target pages/charts/features (Overview,
│                             Cash Flow, Savings, Investments, Projects, Data Entry;
│                             category editing + prediction). Check before adding features.
│
├── qml/                    — ALL UI. No business logic here.
│   ├── Main.qml            — ApplicationWindow + left sidebar (collapsible category
│   │                         list, see § Sidebar Navigation) + StackLayout.
│   ├── components/
│   │   ├── KpiCard.qml     — LABEL / value / sub stat card.
│   │   ├── ChartCard.qml   — Titled card hosting a chart (default property slot).
│   │   ├── SidebarCategory.qml — One collapsible sidebar entry (see § Sidebar Navigation).
│   │   └── ColorComboBox.qml — ComboBox that fills its own background (field and
│   │                         each dropdown row) with the item's color, with text
│   │                         color and drop-down arrow (a custom Canvas indicator)
│   │                         both picked per-item for contrast. Used for the
│   │                         category/subcategory pickers on ImportSnapshotPage.qml
│   │                         (see § Statement Import).
│   ├── pages/              — One .qml per PROJECT.md page (see status table above),
│   │   │                     plus OtherLegacyPage.qml and ImportSnapshotPage.qml
│   │   │                     (neither is a PROJECT.md page on its own).
│   │   ├── OverviewPage.qml, CashFlowPage.qml, SavingsPage.qml,
│   │   ├── InvestmentsPage.qml, ProjectsPage.qml, DataEntryPage.qml
│   │   ├── OtherLegacyPage.qml — lists the unregistered qml/tabs/* files below.
│   │   ├── ImportSnapshotPage.qml — Data Entry → "New Monthly Snapshot" statement
│   │   │                         import wizard (see § Statement Import).
│   │   └── CategoriesManagePage.qml — Data Entry → "Manage Categories": add/rename/
│   │                             recolor/remove categories + subcategories (see
│   │                             § Category Management).
│   └── tabs/               — PREVIOUS GENERATION (unregistered) — snapshot tabs,
│                             kept as reference for the DB wiring phase. Surfaced
│                             in the UI under sidebar category "Other - To remove".
│
├── src/
│   ├── main.cpp            — QGuiApplication + QQuickStyle::setStyle("Basic") +
│   │                         DB open + QQmlApplicationEngine
│   │                         (engine.loadFromModule("FinanceDashboard", "Main")).
│   │
│   ├── controllers/        — C++ backends exposed to QML via QML_ELEMENT.
│   │   │                     One subfolder per page/domain (grouped so the folder
│   │   │                     doesn't read as one flat pile of 14 unrelated classes).
│   │   │                     Each active subfolder must also be listed in
│   │   │                     `target_include_directories` in CMakeLists.txt (the
│   │   │                     auto-generated qml type-registration .cpp includes
│   │   │                     headers by basename).
│   │   ├── overview/NetWorthController.h/.cpp    — Overview page (MOCK).
│   │   ├── cashflow/CashFlowController.h/.cpp    — Cash Flow page (MOCK; rolling avg + month aggregation in C++).
│   │   ├── savings/SavingsController.h/.cpp     — Savings page (MOCK).
│   │   ├── investments/InvestmentsController.h/.cpp — Investments page (MOCK).
│   │   ├── projects/ProjectsController.h/.cpp    — Projects page (MOCK projects; REAL annuity loan math).
│   │   ├── dataentry/DataEntryController.h/.cpp   — Data Entry page (MOCK; categories come from
│   │   │   CategoryController, not this controller — see § Category Management).
│   │   ├── dataentry/SnapshotImportController.h/.cpp — ImportSnapshotPage backend (REAL parsing via
│   │   │   src/import/, MOCK save — see § Statement Import). Lives alongside
│   │   │   DataEntryController — both belong to the Data Entry domain (the page
│   │   │   itself plus its "New Monthly Snapshot" action).
│   │   ├── dataentry/CategoryController.h/.cpp    — Category/subcategory CRUD (`QML_SINGLETON`,
│   │   │   SQLite-backed via `Database`) — see § Category Management. Grouped under dataentry/ with
│   │   │   its siblings (not a separate category/ subfolder) — it's a Data Entry
│   │   │   sidebar action like SnapshotImportController; being a QML_SINGLETON
│   │   │   (shared instance, since it's consumed by two other pages too) is a
│   │   │   registration detail, not a reason for its own directory.
│   │   └── legacy/ (unregistered) AnalyticsController, EntryController, DatabaseController,
│   │       OverviewController, PortfolioController, ImportController — previous
│   │       generation; EntryController/DatabaseController already talk to the real DB.
│   │       Not in CMakeLists.txt SOURCES, not on the include path, not compiled.
│   │       (Note: this legacy ImportController — JSON portfolio import — is unrelated
│   │       to SnapshotImportController; names collide conceptually only, not in code.)
│   │
│   ├── import/              — Statement-import template system (REAL logic, UI-independent,
│   │   │                     no QML_ELEMENT). See § Statement Import.
│   │   ├── ImportTemplate.h         — Abstract interface every template implements.
│   │   ├── ImportTemplateRegistry.h/.cpp — Static list of available templates.
│   │   └── CheckingCsvTemplateV1.h/.cpp  — First template (checking-account CSV export).
│   │
│   ├── data/
│   │   └── DebugData.h/.cpp    — Mock/sample data factory. No external deps.
│   │
│   ├── models/
│   │   ├── PortfolioData.h/.cpp    — DataPoint + PortfolioSummary structs (JSON import).
│   │   ├── AssetEntry.h/.cpp       — AssetEntry + FinancialSnapshot structs.
│   │   │                             Both implement fromJson/toJson for JsonReader compat.
│   │   ├── ImportedStatement.h     — ImportedTransaction + ImportedStatement (header-only,
│   │   │                             staging data for the import review screen, no DB yet).
│   │   ├── ImportLog.h             — ImportLogEntry (header-only) — parse warnings/errors.
│   │   └── Category.h              — Data::Category + Data::Subcategory (header-only POD,
│   │                                  mirrors the `categories`/`subcategories` table rows,
│   │                                  `id` field for targeting CRUD by row — see § Category
│   │                                  Management).
│   │
│   ├── db/
│   │   ├── Database.h/.cpp      — Singleton orchestrator. Owns the QSqlDatabase
│   │   │                          connection and the four table objects. All SQL is
│   │   │                          delegated; Database only handles transactions.
│   │   └── tables/
│   │       ├── SnapshotsTable.h/.cpp      — All SQL for the `snapshots` table.
│   │       ├── AssetEntriesTable.h/.cpp   — All SQL for the `asset_entries` table.
│   │       ├── CategoriesTable.h/.cpp     — All SQL for the `categories` table.
│   │       └── SubcategoriesTable.h/.cpp  — All SQL for the `subcategories` table.
│   │
│   ├── utils/
│   │   ├── Formatter.h        — Header-only display utilities (namespace Fmt).
│   │   ├── FileFinder.h       — Header-only file-location helpers.
│   │   └── JsonReader.h       — Header-only generic JSON deserializer.
│   │   (CategoryKeywords.h removed — keyword auto-suggestion replaced by the
│   │    user-managed CategoryController list, see § Category Management.)
│   │
│   ├── MainWindow.h/.cpp   — LEGACY (not compiled). Reference for QML shell.
│   └── tabs/               — LEGACY (not compiled). Reference for tab ports.
│
└── resources/              — Placeholder. Add .qrc file + icons here when needed.
```

---

## Sidebar Navigation

`Main.qml` replaced the old top `TabBar` with a left sidebar (`qml/components/SidebarCategory.qml`, repeated via a `Repeater` over `Main.qml`'s `categories` array). Each entry mirrors 1:1 with a PROJECT.md page, plus a 7th non-spec entry:

- **Category header** (label) — click selects the category and drives `StackLayout.currentIndex` (`Main.qml`'s `root.currentIndex`). Highlighted with a left accent bar (`#89b4fa`) + bold text when active.
- **Arrow toggle** — a separate `MouseArea` (28px, right side of the header row) expands/collapses the category's action list *without* changing the selected page. State is local (`SidebarCategory.expanded`), not persisted.
- **Action list** — pre-filled `Text` labels (`categories[i].actions` in `Main.qml`), sourced from the bullet points already in `PROJECT.md` for that page. Clicking any action emits `SidebarCategory.actionTriggered(action)`; **two are wired up so far**, both under Data Entry — "New Monthly Snapshot" (see § Statement Import below) and "Manage Categories" (see § Category Management) — every other action is still a no-op placeholder. `Main.qml`'s `Repeater` delegate is where new actions get wired: add a condition to the `onActionTriggered` handler.
- **"Other - To remove"** — 7th category, not a PROJECT.md page. Lists the unregistered `qml/tabs/*.qml` legacy files (see Directory Map) as a reminder they're cleanup candidates. Renders `qml/pages/OtherLegacyPage.qml` when selected.

To add a new sidebar category: append `{ label, actions }` to `Main.qml`'s `categories` array, and add the matching page as the next child of the `StackLayout` (order must match array order — `StackLayout.currentIndex` is a plain array index, no id-based lookup).

**Overlay pages** (full-content pages reached from an action, not a category — e.g. ImportSnapshotPage, CategoriesManagePage): `Main.qml` uses one `Loader` per overlay, anchored over the same content area as the `StackLayout`, each gated by its own `property bool showXxxOverlay` and toggled visible/hidden alongside the `StackLayout` (`StackLayout.visible: !root.showImportOverlay && !root.showCategoriesOverlay`). Opening one overlay also clears the other flag so only one shows at a time. Each overlay page emits a `closeRequested()` signal to clear its flag and return to the sidebar-driven view. This is the pattern to copy for the next wired-up action.

---

## Statement Import ("New Monthly Snapshot")

Reached from the sidebar (Data Entry → "New Monthly Snapshot"). Lets the user pick a
bank/extract file, parse it into editable+categorizable transactions, and review parse
issues before a (currently mock) save. Design decisions made when this was built:

- **Persistence scope: parse is REAL, save is MOCK.** `SnapshotImportController::save()`
  only sets `statusText`, same convention as `DataEntryController::saveSnapshot()`. No
  `transactions` DB table exists yet — see Known Limitations/TODOs. The parsing itself is
  real (no mock data): `CheckingCsvTemplateV1` actually reads and parses the CSV format.
- **Template design: one hardcoded C++ class per format, not a declarative/config-driven
  reader.** Decided against a generic column-mapping descriptor because formats aren't
  simple tables (multi-line headers, PDF layouts planned) — each format gets real parsing
  logic. New format = new `ImportTemplate` subclass + one line in
  `ImportTemplateRegistry::all()`.
- **Account matching: auto by account-hash fingerprint, fallback to account number, then
  manual.** `SnapshotImportController::matchAccountIndex()` never compares the statement's
  IBAN/BIC to an account's IBAN/BIC directly. Instead `SnapshotImportController::accountHash()`
  normalizes (uppercase, strip spaces) and computes HMAC-SHA256(IBAN, key=BIC) for both the
  statement and each candidate account, and only the resulting fingerprints are compared —
  mocking a lookup-by-fingerprint DB call rather than a field-by-field bank-identifier
  comparison. If neither side has an IBAN to hash, falls back to a direct `number` match. No
  match at all → the QML page shows a `ComboBox` for a manual pick. The fingerprint is also
  surfaced as `statementInfo.accountHash` and shown in the `ImportSnapshotPage.qml` header
  so the match is auditable. `iban`/`bic`/`number` are still MOCK fields on
  `DataEntryController`'s accounts (see `DataEntryController::m_accounts`); `bic` was added
  alongside them for this feature.

### `ImportTemplate` (src/import/ImportTemplate.h)
Abstract interface: `id()`, `displayName()`, `fileFilter()` (QtQuick.Dialogs `nameFilters`
entry), and `parse(filePath, outStatement, outLog) -> bool`. Row-level failures are
appended to `outLog` (`Data::ImportLogEntry`, Info/Warning/Error) and the row is skipped —
`parse()` only returns `false` for fatal failures (file unreadable, header not found).

### `CheckingCsvTemplateV1` (src/import/CheckingCsvTemplateV1.cpp)
First (and currently only) template. Format is documented verbatim in the header comment.
Key implementation notes:
- **Parses the whole file in one pass, NOT line-by-line.** `Description and identifier` is
  a quoted field that can contain **embedded newlines** (a genuinely multi-line
  description) as well as commas. An earlier version read the file with `QTextStream`'s
  `readLine()` and parsed each physical line independently — that silently corrupted any
  statement containing a multi-line description (the embedded newline ended the "row" early,
  and everything after it shifted by one or more bogus rows). Fixed by `parseCsvRecords()`:
  reads the entire file (`readAll()`) and tokenizes it in a single quote-aware pass, where a
  `\n` only ends a record when it occurs *outside* an open quote — inside quotes it's kept as
  part of the field. Each `CsvRecord` also carries its starting physical line number (for log
  messages) and the raw (possibly multi-line) source text (for the log popup, rendered with
  embedded newlines shown as "↵" via `previewRawText()`). `normalizeField()` then collapses
  any embedded newlines to spaces in field values that get stored (e.g. the final
  `ImportedTransaction::description`).
- Locates the transaction header row by content match (record containing both "confirmation
  date" and "operation date", case-insensitive) rather than a fixed line number, so a stray
  blank line doesn't break parsing.
- Metadata lines (holder name, IBAN, period, …) are matched by label, not position.
- CSV tokenization (quote-aware, `""`-escape) lives in `parseCsvRecords()` itself now — there
  is no separate per-line splitter function anymore.
- `parseAmount()` tolerates "1 234.56 €", "1234,56", spaces/narrow-no-break-spaces as
  thousands separators — disambiguates `,` vs `.` as the decimal separator by taking
  whichever appears **last** in the string when both are present.
- Dates use `MM/dd/yyyy` per the spec as given (not the French `dd/MM/yyyy` — if a real
  export turns out to use `dd/MM/yyyy`, fix `parseStatementDate()`, this was not verified
  against a real file).
- Category/subcategory are left **empty** at parse time — picked manually in the review
  table against `CategoryController` (see § Category Management). The old
  `CategoryKeywords::suggest()` keyword auto-fill was removed per product decision: it
  guessed from a hardcoded token table matching the transaction description text, not
  from anything correlated to the user's own data, and was confusing (categories showed
  up "already filled" with no real signal behind them).
- **Known limitation:** assumes UTF-8 file encoding. French bank CSV exports are sometimes
  Latin-1/Windows-1252 — if real files come through garbled, add encoding detection/fallback.

### `SnapshotImportController` (src/controllers/dataentry/SnapshotImportController.h/.cpp)
QML_ELEMENT backend for `ImportSnapshotPage.qml`. Holds one `Data::ImportedStatement` +
its `QVector<Data::ImportLogEntry>` log after `parseFile(fileUrl, templateId)`.
`transactionRows()`/`statementInfo()` format everything with `Fmt::money`/date strings for
QML — the QML page does no formatting itself, following the existing controller pattern.
Row edits (`setRowCategory`, `setRowSubcategory`, `setRowIncluded`) mutate the controller's
own statement and re-emit `statementChanged()`; there is no separate `QAbstractListModel`,
just a recomputed `QVariantList` each time (`transactionRows()`), consistent with the mock
history/annual tables in `DataEntryController`. `setRowCategory`/`setRowIncluded` skip the
emit when the value isn't actually changing — needed because `transactionRows` also backs
the `Repeater`'s `model` directly: `CheckBox.checked` defaults to `false` while
`ImportedTransaction::included` defaults to `true`, so the delegate's initial binding
assignment used to fire `onCheckedChanged` → `setRowIncluded(true)` → `statementChanged()`
re-entrantly while the model was still being built, which QML reported as "Binding loop
detected for property model". The no-op guard breaks that loop at the source.

### `ImportSnapshotPage.qml`
Two-step page (`step: 0` pick template+file, `step: 1` review/edit) inside a single `Item`,
no separate QML files per step. Uses `QtQuick.Dialogs.FileDialog` (nameFilters driven by
the selected template's `fileFilter()`). Each transaction row has two
`qml/components/ColorComboBox.qml`s — category then subcategory, sourced from
`CategoryController.categories` (the subcategory list is filtered live to the row's
currently-picked category). `ColorComboBox` fills its own background (field and each
dropdown row) with the item's `color` role, with text color chosen per item for
contrast, so the picked category/subcategory color is visible while choosing and once
chosen — plain `ComboBox` before this only showed names. (An earlier version showed the
color as a small swatch badge ahead of the name instead of the background, which also ate
into the 115px column width; separately, its collapsed-field contentItem reserved
`indicator.width + spacing` for the dropdown arrow a second time on top of what the Basic
style's own ComboBox.qml already reserves via `control.rightPadding` — that double
reservation, copied from the Qt docs' "Customizing ComboBox" example without accounting
for the fact that example also supplies its own custom `indicator`, was the real cause of
names cropping to 2-3 characters despite visible space in the box. `ColorComboBox` now
uses only a small fixed inset instead.) The parse log is a `Popup` (list of level/line/message/
rawLine), reachable both from a badge in the statement-info card and a "View log" footer
button. `closeRequested()` returns to the sidebar view (see § Sidebar Navigation →
Overlay pages).

---

## Category Management ("Manage Categories")

Reached from the sidebar (Data Entry → "Manage Categories"). Lets the user add, rename,
recolor, and remove categories and their subcategories — the same list read everywhere a
transaction gets classified (the "Add transaction" form on `DataEntryPage.qml` and the
statement-import review table on `ImportSnapshotPage.qml`).

- **Replaces keyword auto-suggestion.** The previous system (`utils/CategoryKeywords.h`)
  guessed a category by matching hardcoded tokens (e.g. "carrefour" → Groceries) against
  the transaction description — not anything correlated to the user's own history, despite
  looking that way in the UI. Removed per product decision: categories/subcategories are
  now a user-managed list, picked manually everywhere, no auto-fill.
- **`CategoryController` (src/controllers/dataentry/CategoryController.h/.cpp) is a
  `QML_SINGLETON`**, unlike every other controller here (one instance per page).
  Categories are cross-cutting — edited on one page, consumed on two others — so a
  per-page instance (the usual pattern) would silently reset on every page navigation;
  the singleton keeps one shared instance for the whole run. Lives under
  `src/controllers/dataentry/` (not its own `category/` subfolder) alongside
  `DataEntryController`/`SnapshotImportController` — grouped by domain like every other
  controller folder; moved here from a standalone `category/` folder since being a
  `QML_SINGLETON` is a registration detail, not a reason for its own directory.
- **Data shape**, rebuilt into a `QVariantList` on every `categories()` read (same
  convention as `SnapshotImportController::transactionRows()` — no separate
  `QAbstractListModel`): `[{ name, color, subcategories: [{ name, color }, ...] }, ...]`.
  Row ids (`Data::Category::id` / `Data::Subcategory::id`) are kept in the internal
  `QVector<Data::Category> m_categories` only, not exposed in this QVariantList — QML
  still addresses categories/subcategories by list index, same as before the DB-backed
  rewrite below.
- **CRUD**: `addCategory`/`renameCategory`/`setCategoryColor`/`removeCategory` and the
  subcategory equivalents (indexed by `categoryIndex`/`subIndex` into the current list).
  `categoryColor(name)`/`subcategoryColor(categoryName, subName)` are convenience lookups
  for anywhere that only has the category as a string (e.g. a stored transaction row).
- **`suggestSubcategoryColor(categoryIndex)`** — `Q_INVOKABLE`, returns the default color
  `CategoriesManagePage.qml` pre-fills when adding a new subcategory: the parent
  category's own color, shifted lighter/darker in the HSL lightness channel (`QColor::
  getHslF`/`fromHslF`, verified against Qt's `QColor` API, no invented properties) in
  increasing ~8% steps, alternating sign, until a shade distinct from that category's
  *existing subcategory colors* is found (siblings only — other categories' colors are
  allowed to repeat, per product decision, since they're a different visual group).
  Falls back to the category's own color if categoryIndex is invalid or every tried shade
  collides (many existing subcategories). Pure algorithm in C++, per the "UI in QML,
  algorithms in C++" rule — `CategoriesManagePage.qml`'s "+ Subcategory" action just calls
  it and passes the result as the dialog's initial color; the user can still override it
  via the color picker before saving.
- **Scope: SQLite-backed** — `categories`/`subcategories` tables (see below), the first
  domain in the app wired to the real DB (every other controller is still MOCK). On
  construction, `CategoryController` calls `Database::instance().loadCategories()`; on an
  empty DB (first launch) it calls the private `seedDefaults()`, which writes the original
  mock category/subcategory list through `Database::addCategory()`/`addSubcategory()` so it
  persists from then on instead of resetting every run. Every CRUD method writes through
  `Database::instance()` first and only updates `m_categories` (and emits
  `categoriesChanged()`) if that write succeeded — a failed DB write (DB not open, a
  UNIQUE-constraint collision on a bug elsewhere, etc.) never produces a phantom
  in-memory-only entry that would silently vanish on the next reload.

### Category/subcategory DB schema

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

- **`CategoriesTable`/`SubcategoriesTable`** (`src/db/tables/`) own all SQL for their table,
  same shape as `SnapshotsTable`/`AssetEntriesTable`: `createTable()`, CRUD by row id,
  `all()`/`allForCategory(id)` readers ordered by row id (insertion order — there is no
  explicit sort-order column; nothing reorders categories today).
- **No DB-level cascade** (no `ON DELETE CASCADE` — no foreign keys are declared at all
  in this schema, consistent with the rest of the app). `Database::removeCategory(id)`
  does the cascade manually in a transaction: `SubcategoriesTable::removeForCategory(id)`
  then `CategoriesTable::remove(id)` — same "delete-then-insert" transaction style as
  `Database::saveSnapshot()`.
- **`Database::loadCategories()`** composes both tables: reads all categories, then fills
  each one's `subcategories` from `SubcategoriesTable::allForCategory(id)` — mirrors how
  `AssetEntriesTable::loadForSnapshot()` is composed by `Database::loadSnapshot()`.
- **Duplicate-name checks stay in `CategoryController`, not SQL** — the controller already
  checked for a case-insensitive duplicate name before mutating its in-memory list, and
  that check is unchanged; the DB layer's `UNIQUE` constraints are a backstop, not the
  primary validation path (`insert()` returns `-1` on any failure, duplicate or otherwise,
  and the controller treats that as "write failed, don't touch the in-memory list").
- **`CategoriesManagePage.qml`**: color picker uses `QtQuick.Dialogs.ColorDialog` (native
  platform dialog, verified against doc.qt.io/qt-6/qml-qtquick-dialogs-colordialog.html —
  `selectedColor`/`options` properties, inherited `open()`/`accepted()` from `Dialog`).
  Reverses an earlier decision to use a fixed 14-swatch `Flow` instead (kept the picker
  custom-drawn/inline) — replaced per product decision so any color is pickable, not just
  the 14 Catppuccin accents. Each edit dialog (`categoryDialog`/`subcategoryDialog`) owns a
  `selectedColor: string` property (hex) and a matching `ColorDialog` instance; the hex
  swatch button copies `selectedColor` into the `ColorDialog` right before `open()` (a
  declarative binding was tried first but breaks permanently the first time the dialog's
  own `selectedColor` is set imperatively by user interaction — see `color.r/g/b`-based
  `page.colorToHex()` for the reverse `color` → `"#rrggbb"` conversion, which avoids relying
  on the undocumented format of `color.toString()`). One shared `Popup` handles both add
  and rename/recolor for categories, and a second for subcategories — an `editIndex`/
  `editSubIndex` property (`-1` = add mode) switches the dialog's title/button label and
  which controller method gets called on confirm.

---

## QML/C++ Boundary — the pattern

Follows the `*DataProvider` pattern from QtGraphsExamples:

1. Controller class in `src/controllers/<domain>/`, `Q_OBJECT` + `QML_ELEMENT`.
2. Registered by listing the `.h/.cpp` under `SOURCES` of `qt_add_qml_module` in `CMakeLists.txt`. **The controller's directory must also be on `target_include_directories`** — the auto-generated type-registration `.cpp` includes headers by basename.
3. Instantiated *in QML* (`AnalyticsController { id: controller }`), not via context properties.
4. Data crosses the boundary as `Q_PROPERTY` (scalars, with `NOTIFY`) and `Q_INVOKABLE QVariantList` (arrays).
5. QML never computes: sums, cumulative layers, axis ranges, diffs all come pre-computed from C++.

### `AnalyticsController`
- `reload()` — reads `Database::listDates()` + `loadSnapshot()`, computes per-category totals and cumulative boundaries (bottom → top: **ETFs, Metals, Coins, Stocks**).
- `timestamps()` — snapshot dates as ms-since-epoch (X values for `DateTimeAxis`).
- `etfsBound() / metalsBound() / coinsBound() / stocksBound()` — cumulative upper boundaries (same length as `timestamps()`).
- Properties: `hasData`, `maxTotal` (peak × 1.12, min 1000), `minTimeMs` / `maxTimeMs` (padded ±12 h when only one snapshot exists so the range is never zero).

### `AnalyticsTab.qml`
- Boundary `LineSeries` are declared **outside** `GraphsView` so they are never drawn as lines; each `AreaSeries` references them as `upperSeries`/`lowerSeries` (bottom layer omits `lowerSeries` → fills from 0). This mirrors QtGraphsExamples → `areachart/stacked`.
- X axis: `DateTimeAxis` (Qt Graphs, verified against Qt 6.11 docs), `labelFormat: "d MMM yy"`.
- Y axis: `ValueAxis`, `labelFormat: "%.0f €"`.
- Empty state shown when `!controller.hasData`.
- `refresh()` — clears the boundary series (`XYSeries.clear()`, verified in 6.11 docs), calls `controller.reload()`, re-appends. Triggered from `Main.qml` when EntryTab or DatabaseTab emit `saved()`.

### Other controllers (same pattern)
- **`EntryController`** — owns four `CategoryModel`s (`QAbstractListModel`, `QML_ANONYMOUS`; roles: `name`, `valueText`, `diffText`, `diffState`). All parsing (C-locale), diff %, totals and DB save/load in C++. Note: `setRowValue` only emits `dataChanged` for the diff roles — emitting `valueText` would reset the TextField cursor mid-typing. Emits `snapshotSaved()`.
- **`DatabaseController`** — internal `DbTableModel` (QSqlTableModel subclass, defined in the .cpp with `#include "DatabaseController.moc"`): column 0 read-only, and `setData` remaps `Qt::DisplayRole` → `Qt::EditRole` because QML TableView's edit delegate writes via the `display` role. `OnManualSubmit` buffering with save/revert/refresh, like the legacy tab. Emits `changesSaved()`.
- **`OverviewController` / `PortfolioController`** — expose pre-formatted `QVariantList`s (debug data, same as legacy).
- **`ImportController`** — `importFile(path)` → FileFinder + `JsonReader::read<PortfolioSummary>`; exposes summary rows, capped 200-point preview, status. `localPath(url)` converts `FileDialog.selectedFile` (file:// URL) to a local path.

### QML delegate gotcha (learned during the port)
When a TableView/Repeater delegate declares ANY `required property`, model roles are **no longer injected as context properties** — every role used (`display`, etc.) must then be declared as a `required property` too. The `TableView.editDelegate` is a separate component and can still use context roles (matches the Qt docs example).

---

## Component Responsibilities (unchanged C++ layer)

### `db/Database.h/.cpp`
- Meyers singleton — `Database::instance()`.
- Owns the `QSqlDatabase` connection (`"finance_dashboard"` named connection).
- Owns `SnapshotsTable m_snapshots`, `AssetEntriesTable m_assetEntries`,
  `CategoriesTable m_categoriesTable`, `SubcategoriesTable m_subcategoriesTable`.
- `open()` enables WAL mode then delegates `createTable()` to each table object.
- `saveSnapshot()` and `removeCategory()` are the only methods that touch transactions
  (cross-table writes).
- `loadCategories()`, `addCategory()`/`renameCategory()`/`setCategoryColor()`/
  `removeCategory()` and the subcategory equivalents — see § Category Management.
- **DB file location:** `%APPDATA%\DataAnalysis\FinanceDashboard\finance_dashboard.db` (Windows).

### `db/tables/SnapshotsTable.h/.cpp`
- `upsert(date)` — INSERT … ON CONFLICT DO NOTHING (idempotent).
- `idForDate(date)` → `std::optional<int>`; `latest()` → `std::optional<QPair<int, QDate>>`.
- `allDates()` → `QVector<QDate>` descending.

### `db/tables/AssetEntriesTable.h/.cpp`
- `createTable()` also creates `idx_entries_snapshot` index.
- `deleteForSnapshot(id)` + `insertBatch(id, category, entries)` are always called together by `Database::saveSnapshot()`.
- `loadForSnapshot(id, date)` rebuilds a `FinancialSnapshot`.
- **To add a new asset category:** add a branch in `loadForSnapshot`, an `insertBatch` call in `saveSnapshot`, and a new layer in `AnalyticsController::reload()` + `AnalyticsTab.qml`.

### `db/tables/CategoriesTable.h/.cpp` / `db/tables/SubcategoriesTable.h/.cpp`
- See § Category Management for the full schema and CRUD surface.
- `insert()` returns the new row id (`int`), or `-1` on any failure (duplicate name via the
  `UNIQUE` constraint included) — callers (`CategoryController`) check for duplicates
  themselves first and treat `-1` as "write failed, don't touch the in-memory list".
- `all()` / `allForCategory(categoryId)` order by row id ascending (insertion order).

### `models/AssetEntry.h/.cpp`
- `Data::AssetEntry` — `{name, value}`; `Data::FinancialSnapshot` — `{date, stocks[], coins[], metals[], etfs[]}`. Both with `toJson()` / `fromJson()`.

### `models/Category.h`
- Header-only POD: `Data::Subcategory` — `{id, name, color}`; `Data::Category` — `{id, name, color, subcategories[]}`. `id` is the SQLite row id (`-1` for a not-yet-persisted instance). No JSON (de)serialization — unlike `AssetEntry`, nothing round-trips these through a file.

### `models/PortfolioData.h/.cpp`
- `Data::DataPoint` — `{time, value}`; `Data::PortfolioSummary` — top-level portfolio JSON document. Both `JsonReader`-compatible.

### `utils/`
- `Formatter.h` — `Fmt::money(double, bool showPlus)` French-locale currency; `Fmt::gCurrency` (EUR/USD). *TODO: expose to QML (wrap in a QML singleton or controller Q_INVOKABLE) so formatting stays in C++.*
- `FileFinder.h` — `exists()`, `findFirst()`, `findFiles()`.
- `JsonReader.h` — `read<T>(filePath)` → `std::optional<T>`; `T` needs `static T::fromJson(QJsonObject)`.

### `data/DebugData.h`
- Factories: `sampleHoldings()`, `sampleSummary()`, `samplePerformance()`. Keep for tests and for Overview/Portfolio until those tabs are wired to the DB.

---

## Styling

**Controls style: `Basic`.** Set via `QQuickStyle::setStyle("Basic")` in `src/main.cpp`, right after `QGuiApplication` is constructed and before the QML engine loads anything (per [QQuickStyle docs](https://doc.qt.io/qt-6/qquickstyle.html) — must run before QML importing `QtQuick.Controls` is loaded). Requires `Qt6::QuickControls2` (linked in `CMakeLists.txt`). Native styles (Windows, macOS, ...) reject `background:`/`contentItem:` customization on Controls — this app customizes `TextField`, `ComboBox`, `Popup`, etc. everywhere, so `Basic` (Qt's own recommendation for full customization) is required to avoid the "current style does not support customization" runtime warning/breakage.

Catppuccin Mocha palette, currently hardcoded as hex literals in each `.qml` file (same approach as QtGraphsExamples). *TODO: centralize in a QML singleton `Theme.qml` once a second tab is ported.*

| Token | Hex | Used for |
|---|---|---|
| Base | `#11111b` | Window background |
| Mantle | `#181825` | Tab bar background |
| Surface 0 | `#1e1e2e` | Cards, empty-state background |
| Surface 1 | `#313244` | Borders, dividers |
| Overlay 0 | `#6c7086` | Muted labels, inactive tabs |
| Text | `#cdd6f4` | Primary text |
| Blue | `#89b4fa` | Active tab, Stocks layer |
| Green | `#a6e3a1` | ETFs layer, positive P&L |
| Yellow | `#f9e2af` | Metals layer |
| Red/Pink | `#f38ba8` | Coins layer, negative P&L |

Chart fills use the same colors at ~43 % opacity (`#6e` alpha prefix).

---

## Build Instructions (for maintainer)

Requires **Qt 6.8+ with the Qt Graphs module** (developed against 6.11). Qt Charts is no longer needed.

### Windows (MSVC)
```powershell
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
cmake --build build --config Release
cd build/Release && windeployqt.exe --qmldir ../../qml FinanceDashboard.exe
```
Note the `--qmldir` flag — required so windeployqt ships the QML/Graphs runtime.

### Qt Creator (easiest)
Open `CMakeLists.txt` → auto-configures → press Run.

---

## Porting a Tab (checklist)

1. Read the legacy Widgets file in `src/tabs/` — it is the spec.
2. Create `src/controllers/<domain>/XxxController.h/.cpp` (`QML_ELEMENT`); move every calculation there. Pick the subfolder matching the page/domain (add a new one if it's a new page), and add that subfolder to `target_include_directories` in CMakeLists.txt.
3. Create `qml/tabs/XxxTab.qml` — rendering only.
4. Register both in `CMakeLists.txt` (`SOURCES` + `QML_FILES` of `qt_add_qml_module`).
5. Replace the `PlaceholderTab` instance in `qml/Main.qml`.
6. For charts: copy the closest pattern from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples); verify any new property against https://doc.qt.io/qt-6/qtgraphs-index.html — **never invent property names**.
7. Delete the legacy `src/tabs/` file; update the Migration Status table above.

---

## Charting Gotchas (learned on this project)

- **Axis labels cannot render "€"** — `ValueAxis.labelFormat` shows `?` for non-ASCII. Units go in the ChartCard title; tooltips are QML `Text` and render € fine.
- **Hover support**: `LineSeries` → `pointDelegate` (`qml/components/HoverPoint.qml`, uses injected `pointValueX/Y`, `pointIndex`); `BarSeries` → `barDelegate` (`HoverBar.qml`, injected `barValue`/`barLabel`) — BarSeries has NO hover signals. `PieSeries` has **no hover at all**; only `clicked(slice)` → pages show slice details on click.
- **AreaSeries has no pointDelegate** — stacked areas get transparent `LineSeries` "hover carriers" on each boundary; tooltip shows the layer's own (non-cumulative) value via `HoverPoint.overrideValues`.
- **Child `Component.onCompleted` runs before the parent's** — series filled by a page-level handler must be appended there, not in the child.
- **Sankey**: no Qt Graphs series type — `qml/components/MoneyFlow.qml` is a pure-QML Canvas port of QtGraphsExamples → flowchart.
- **Inline `component` declarations** must sit at the document root, not nested in a Popup.
- Donuts use `pieSize: 0.75` / `holeSize: 0.42` (0.85 clipped the labels).

---

## Known Limitations / TODOs

- [ ] **Wire pages to real data** — replace the MOCK generators in the 6 page controllers with DB aggregation. The schema needs to grow: per-account monthly snapshots + categorized transactions (see PROJECT.md). Reuse `EntryController`/`DatabaseController` patterns.
- [ ] Range selector (12–24 month window) on time charts — PROJECT.md calls for it on all pages.
- [ ] Loan simulator: add optional insurance (assurance emprunteur) flat % input.
- [ ] Delete previous-generation files once the new UI is validated: `src/MainWindow.*`, `src/tabs/*`, `qml/tabs/*`.
- [ ] `Fmt::money` not yet reachable from QML — Y-axis uses `labelFormat: "%.0f €"` instead of French-locale formatting.
- [ ] Centralize palette in a `Theme.qml` singleton.
- [ ] No `.qrc` resource file — icons and fonts should be added under `resources/`.
- [ ] No tests — add a `tests/` directory with Qt Test when data logic grows.
- [ ] **Statement import save is mock** — `SnapshotImportController::save()` doesn't persist. Needs a `transactions` table (date, description, debit/credit, category, account, source template) — see PROJECT.md's "Optional categorized transactions" and § Statement Import.
- [ ] `CheckingCsvTemplateV1` assumes UTF-8 and `MM/dd/yyyy` dates, per the spec as given — neither was verified against a real exported file yet. Fix if a real file parses wrong.
- [ ] PDF statement templates are not implemented — `ImportTemplate` was designed to support them (interface doesn't assume CSV), but no PDF library is linked yet.
- [ ] `qml/pages/ImportSnapshotPage.qml` (`FileDialog`) and `qml/pages/CategoriesManagePage.qml` (`ColorDialog`) import `QtQuick.Dialogs` with no explicit `find_package`/`target_link_libraries` for it in `CMakeLists.txt` (relying on `qt_add_qml_module`'s automatic import scanning). If the build can't find the plugin, add `QuickDialogs2` — see the NOTE in `CMakeLists.txt`.
- [ ] Sidebar action list items are still mostly inert placeholders (only Data Entry → "New Monthly Snapshot" and "Manage Categories" are wired) — see § Sidebar Navigation.
- [ ] `CheckingCsvTemplateV1::parseAmount()` currently has the "€" removal and the final space removal **commented out** (see the `//` lines in the .cpp). As written, an amount like `"1234.56 €"` will fail to convert (the trailing `€` breaks `toDouble()`), and any input relying on space-stripping (e.g. narrow-space-separated thousands not already covered by the two explicit space-char removals above it) won't be cleaned either. This was left as-is per an in-progress edit — revisit once real exported files are available to confirm what actually needs stripping.
- [ ] `src/controllers/category/` is an empty leftover directory from moving `CategoryController` into `src/controllers/dataentry/` — the sandbox this move was done in couldn't `rmdir` it (mount permission issue). Delete it by hand; it's not referenced by `CMakeLists.txt` or anything else, so it's harmless to the build in the meantime.
- [ ] `CategoryController::removeCategory()`/`removeSubcategory()` delete unconditionally — there is no `transactions` table yet (see the Statement import TODO above) so nothing can reference a category by id today, but once one exists, deleting an in-use category will need either a guard (refuse/warn) or an explicit "reassign to Other" step; revisit when the transactions table lands.
- [ ] No reordering of categories/subcategories — `CategoriesTable.all()`/`SubcategoriesTable.allForCategory()` order by row id (insertion order) only, no explicit sort-order column. Add one if drag-to-reorder is ever wanted.
