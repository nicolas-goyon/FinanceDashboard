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
| Transactions Management | *(none — minimal placeholder)* | header text pointing at this category's two sidebar actions | Placeholder only — see § Transactions Management |
| ↳ Import Transactions | `SnapshotImportController` | template + file picker · statement header info · editable/categorizable transaction table · account auto-match (against stored hashes) · "not linked" badge + inline link action · parse log popup — see § Statement Import | **REAL parsing, REAL save** (`transactions` table) |
| ↳ Manage Categories | `CategoryController` (`QML_SINGLETON`) | add/rename/recolor/remove categories and subcategories — see § Category Management | **REAL** (SQLite-backed, `categories`/`subcategories` tables) |
| Account Management | *(none — minimal placeholder)* | header text pointing at this category's two sidebar actions | Placeholder only — see § Account Management |
| ↳ Manage Accounts | `AccountController` (`QML_SINGLETON`) | add/edit/remove accounts · currency selector (EUR/USD) · manage per-account hash links · "Transactions" button routes to a full page — see § Account Management | **REAL** (SQLite-backed, `accounts`/`account_hashes`/`transactions` tables) |
| ↳ Account types & categories | `AccountController` (`QML_SINGLETON`) | add/rename/remove account types · add/rename/recolor/remove account categories — see § Account Management | **REAL** (SQLite-backed, `account_types`/`account_categories` tables) |
| Database Management | `DatabaseManagementController` | "Reset Database" (wipe to empty) / "Reset to Mock Data" (wipe + reseed + sample transactions), both behind a confirmation dialog — see § Database Management | **REAL** (operates directly on the SQLite file via `Database::resetAll()`) |

**Previous-generation code removed (milestone cleanup pass):** the ported snapshot tabs
(`qml/tabs/*`, `EntryController`, `DatabaseController`, `AnalyticsController`,
`OverviewController`, `PortfolioController`, `ImportController`), the original Qt Widgets app
(`src/MainWindow.*`, `src/tabs/*`), and the superseded `DataEntryController.h/.cpp` /
`DataEntryPage.qml` are no longer on disk at all — previously kept unregistered "as
reference," now deleted outright since the QML/Qt-Graphs rewrite is the only UI going
forward and nothing referenced them (see git history before this pass if the old Widgets
version is ever needed again). The "Other - To remove" sidebar category that listed the
`qml/tabs/*` files is gone too, along with `qml/pages/OtherLegacyPage.qml`.

The `db/`, `models/`, `utils/` layers are still compiled and `main.cpp` still opens the SQLite DB — ready for the wiring phase. **Categories/subcategories, accounts/account-hashes/account-types/account-categories, and transactions are now wired to the real DB** (see § Category Management, § Account Management); the 5 page controllers (Overview/Cash Flow/Savings/Investments/Projects) are still MOCK.

**Milestone cleanup pass (this revision):** fixed the sidebar not showing the current
page/overlay (see § Sidebar Navigation — replaced `currentIndex` + five independent
`showXxxOverlay` booleans with one `currentViewId` source of truth); extracted a shared QML
component library (see § Shared Components) and rebuilt every CRUD/dialog-heavy page on top
of it, fixing several small drift bugs along the way (a badge color typo and a missing
border on `ProjectsPage.qml`'s off-track badge, an inverted zebra-stripe on
`CashFlowPage.qml`, a handful of buttons with no hover feedback); deleted all previous-
generation code (see above); added `CategoryController.statusText` for parity with
`AccountController` (both are the app's two `QML_SINGLETON`s with near-identical CRUD
shapes — `CategoryController` used to fail silently); hoisted a duplicated
`dateText()`/date-formatting helper into `Fmt::dateOrDash()`.

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
│   │                         list, see § Sidebar Navigation) + StackLayout, all driven
│   │                         by one `currentViewId` source of truth.
│   ├── components/         — Shared building blocks (see § Shared Components for the
│   │   │                     dialog/button/badge family added in the cleanup pass).
│   │   ├── KpiCard.qml     — LABEL / value / sub stat card.
│   │   ├── ChartCard.qml   — Titled card hosting a chart (default property slot).
│   │   ├── SidebarCategory.qml — One collapsible sidebar entry (see § Sidebar Navigation).
│   │   ├── ColorComboBox.qml — ComboBox that fills its own background (field and
│   │   │                     each dropdown row) with the item's color, with text
│   │   │                     color and drop-down arrow (a custom Canvas indicator)
│   │   │                     both picked per-item for contrast. Used for the
│   │   │                     category/subcategory pickers on ImportSnapshotPage.qml
│   │   │                     (see § Statement Import).
│   │   ├── PageHeader.qml  — Shared page-header row (back arrow, title, "[MOCK]"
│   │   │                     marker, trailing content slot) — see § Shared Components.
│   │   ├── PillButton.qml  — The big colored action button (primary/neutral/danger).
│   │   ├── GhostButton.qml — Small bordered inline-row action (Edit/Remove/etc).
│   │   ├── StatusBadge.qml — Colored pill for status/state indicators.
│   │   ├── FormRow.qml     — Label + TextField row for dialogs.
│   │   ├── ColorPickerRow.qml — Swatch + "Choose color…" + hex text + owns its ColorDialog.
│   │   ├── FormDialog.qml  — Shared add/edit Popup shell (title + form rows + Cancel/Save).
│   │   ├── ConfirmDialog.qml — Shared destructive-confirmation Popup shell.
│   │   └── EmptyState.qml  — "No … yet" placeholder text.
│   ├── pages/              — One .qml per PROJECT.md page (see status table above),
│   │   │                     plus the Transactions/Account/Database Management pages
│   │   │                     (none of these three are a PROJECT.md page on their own).
│   │   ├── OverviewPage.qml, CashFlowPage.qml, SavingsPage.qml,
│   │   ├── InvestmentsPage.qml, ProjectsPage.qml
│   │   ├── TransactionsManagementPage.qml — "Transactions Management" category's own
│   │   │                         landing page. Minimal placeholder (header + text
│   │   │                         pointing at its two sidebar actions below) — see
│   │   │                         § Transactions Management.
│   │   ├── ImportSnapshotPage.qml — Transactions Management → "Import Transactions"
│   │   │                         statement import wizard (see § Statement Import).
│   │   ├── CategoriesManagePage.qml — Transactions Management → "Manage Categories":
│   │   │                         add/rename/recolor/remove categories + subcategories
│   │   │                         (see § Category Management).
│   │   ├── AccountManagementLandingPage.qml — "Account Management" category's own
│   │   │                         landing page. Minimal placeholder, same convention
│   │   │                         as TransactionsManagementPage.qml — see § Account
│   │   │                         Management.
│   │   ├── AccountsManagePage.qml — Account Management → "Manage Accounts": add/
│   │   │                         edit/remove accounts (name/type/category/currency/
│   │   │                         notes — no iban/bic/number, see § Account
│   │   │                         Management), manage each account's hash links, and
│   │   │                         a "Transactions" button that routes to
│   │   │                         AccountTransactionsPage.qml (a real page, not a
│   │   │                         modal).
│   │   ├── AccountTransactionsPage.qml — Routed page (reached from
│   │   │                         AccountsManagePage.qml's "Transactions" button, not
│   │   │                         a sidebar action) showing one account's persisted
│   │   │                         transactions read-only — see § Account Management.
│   │   ├── AccountTypesCategoriesPage.qml — Account Management → "Account types &
│   │   │                         categories": add/rename/remove account types,
│   │   │                         add/rename/recolor/remove account categories — see
│   │   │                         § Account Management.
│   │   └── DatabaseManagementPage.qml — "Database Management" category's own landing
│   │                             page (no sidebar actions — the real "Reset
│   │                             Database"/"Reset to Mock Data" buttons live
│   │                             directly on this page) — see § Database Management.
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
│   │   ├── dataentry/SnapshotImportController.h/.cpp — ImportSnapshotPage backend (REAL parsing via
│   │   │   src/import/, REAL save — writes to the `transactions` table and
│   │   │   auto-links the account hash, see § Statement Import). Backs Transactions
│   │   │   Management → "Import Transactions".
│   │   ├── dataentry/CategoryController.h/.cpp    — Category/subcategory CRUD (`QML_SINGLETON`,
│   │   │   SQLite-backed via `Database`) — see § Category Management. Grouped under dataentry/ with
│   │   │   its siblings (not a separate category/ subfolder) — it's a Transactions
│   │   │   Management sidebar action like SnapshotImportController; being a
│   │   │   QML_SINGLETON (shared instance, since it's consumed by two other pages
│   │   │   too) is a registration detail, not a reason for its own directory. Also
│   │   │   exposes `reload()` — re-reads from `Database` with no reseed-if-empty
│   │   │   (see § Database Management for why that matters).
│   │   ├── dataentry/AccountController.h/.cpp     — Account/account-type/account-category CRUD
│   │   │   + hash management (`QML_SINGLETON`, SQLite-backed via `Database`) — see §
│   │   │   Account Management. Same domain-folder/QML_SINGLETON reasoning as
│   │   │   CategoryController — read by ImportSnapshotPage and its own
│   │   │   AccountsManagePage/AccountTypesCategoriesPage/AccountTransactionsPage.
│   │   │   Also exposes `reload()` — same no-reseed-if-empty contract as
│   │   │   CategoryController::reload().
│   │   ├── database/DatabaseManagementController.h/.cpp — Backend for
│   │   │   DatabaseManagementPage.qml (`QML_ELEMENT`, **not** a `QML_SINGLETON` — no
│   │   │   state to keep alive across navigation). `resetDatabase()`/`resetToMock()`
│   │   │   both call `Database::resetAll()`; see § Database Management for the
│   │   │   reset-vs-reseed design and why this controller duplicates seed values
│   │   │   instead of calling into CategoryController/AccountController's private
│   │   │   `seedDefaults()`.
│   │   ~~└── legacy/~~ — deleted in the milestone cleanup pass (AnalyticsController,
│   │       EntryController, DatabaseController, OverviewController, PortfolioController,
│   │       ImportController — the previous-generation controllers; none were ever in
│   │       CMakeLists.txt SOURCES or on the include path, so removing them changed
│   │       nothing about what compiles). If a legacy `ImportController` (JSON portfolio
│   │       import) is ever needed again, it's unrelated to `SnapshotImportController` —
│   │       names collided conceptually only, never in code.
│   │
│   ├── import/              — Statement-import template system (REAL logic, UI-independent,
│   │   │                     no QML_ELEMENT). See § Statement Import.
│   │   ├── ImportTemplate.h         — Abstract interface every template implements.
│   │   ├── ImportTemplateRegistry.h/.cpp — Static list of available templates.
│   │   └── CheckingCsvTemplateV1.h/.cpp  — First template (checking-account CSV export).
│   │
│   ├── models/
│   │   ├── AssetEntry.h/.cpp       — AssetEntry + FinancialSnapshot structs (still compiled,
│   │   │                             backs `SnapshotsTable`/`AssetEntriesTable` in `db/` —
│   │   │                             see the note at the bottom of § Component
│   │   │                             Responsibilities: this whole snapshot subsystem has no
│   │   │                             current UI/controller consumer, kept out of this pass's
│   │   │                             deletions since it's DB-schema-level, not UI code).
│   │   ├── ImportedStatement.h     — ImportedTransaction + ImportedStatement (header-only,
│   │   │                             staging data for the import review screen, no DB yet).
│   │   ├── ImportLog.h             — ImportLogEntry (header-only) — parse warnings/errors.
│   │   ├── Category.h              — Data::Category + Data::Subcategory (header-only POD,
│   │   │                              mirrors the `categories`/`subcategories` table rows,
│   │   │                              `id` field for targeting CRUD by row — see § Category
│   │   │                              Management).
│   │   ├── Account.h               — Data::Account + Data::AccountHash + Data::AccountCategory
│   │   │                              + Data::AccountType (header-only POD, mirrors the
│   │   │                              `accounts`/`account_hashes`/`account_categories`/
│   │   │                              `account_types` table rows — see § Account Management).
│   │   │                              `Data::Account` no longer carries iban/bic/number —
│   │   │                              removed for security (see § Account Management).
│   │   └── Transaction.h           — Data::Transaction (header-only POD, mirrors the
│   │                                  `transactions` table rows — see § Account Management /
│   │                                  § Statement Import).
│   │
│   ├── db/
│   │   ├── Database.h/.cpp      — Singleton orchestrator. Owns the QSqlDatabase
│   │   │                          connection and the eight table objects. All SQL is
│   │   │                          delegated; Database only handles transactions.
│   │   └── tables/
│   │       ├── SnapshotsTable.h/.cpp        — All SQL for the `snapshots` table.
│   │       ├── AssetEntriesTable.h/.cpp     — All SQL for the `asset_entries` table.
│   │       ├── CategoriesTable.h/.cpp       — All SQL for the `categories` table.
│   │       ├── SubcategoriesTable.h/.cpp    — All SQL for the `subcategories` table.
│   │       ├── AccountsTable.h/.cpp         — All SQL for the `accounts` table (no iban/bic/
│   │       │                                  number columns — see § Account Management).
│   │       ├── AccountHashesTable.h/.cpp    — All SQL for the `account_hashes` table.
│   │       ├── AccountCategoriesTable.h/.cpp — All SQL for the `account_categories` table.
│   │       ├── AccountTypesTable.h/.cpp     — All SQL for the `account_types` table (mirrors
│   │       │                                  AccountCategoriesTable, no color column).
│   │       └── TransactionsTable.h/.cpp     — All SQL for the `transactions` table.
│   │
│   ├── utils/
│   │   ├── Formatter.h        — Header-only display utilities (namespace Fmt):
│   │   │                        Fmt::money() (French-locale currency) and
│   │   │                        Fmt::dateOrDash() ("dd/MM/yyyy" or "—") — the latter
│   │   │                        hoisted out of two duplicated dateText() helpers in
│   │   │                        AccountController.cpp/SnapshotImportController.cpp.
│   │   └── AccountHashing.h   — Header-only HMAC-SHA256(IBAN, key=BIC) fingerprint,
│   │                            shared by AccountController and
│   │                            SnapshotImportController — see § Statement Import.
│   │   (CategoryKeywords.h removed — keyword auto-suggestion replaced by the
│   │    user-managed CategoryController list, see § Category Management.)
│   │
│   ~~├── MainWindow.h/.cpp~~ — deleted in the milestone cleanup pass (was LEGACY,
│   ~~└── tabs/~~              not compiled; both were the original Qt Widgets app).
│
└── resources/              — Placeholder. Add .qrc file + icons here when needed.
```

---

## Sidebar Navigation

`Main.qml` replaced the old top `TabBar` with a left sidebar (`qml/components/SidebarCategory.qml`, repeated via a `Repeater` over `Main.qml`'s `categories` array). The 8 entries mirror the first 5 PROJECT.md pages 1:1, plus **Transactions Management**, **Account Management**, **Database Management** — the former 6th PROJECT.md page ("Data Entry") split into three lighter categories once the product asked for account management, hash management, and a schema-reset tool to be reachable independently rather than bundled under one page (see § Transactions Management / § Account Management / § Database Management).

### Single source of truth: `currentViewId`

**Fixed in the milestone cleanup pass — the sidebar used to fail to highlight the current page whenever an overlay was open.** The previous model tracked navigation as `root.currentIndex` (a `StackLayout` position) *plus* five independent `property bool showXxxOverlay` flags, each of which every navigation entry point had to clear by hand. Clicking a sidebar *action* (e.g. "Manage Accounts") only ever flipped its own overlay flag — it never touched `currentIndex` — so the sidebar kept highlighting whichever category/page was selected *before*, not "Account Management," the whole time an overlay was open. There was no single place that answered "what's on screen right now."

This is now `root.currentViewId: string` — the **one** property that says what's on screen. Every category and every wired action has a matching id (e.g. `"overview"`, `"transactionsManagement"`, `"importTransactions"`, `"manageAccounts"`):

- **Category header** (label) — click sets `root.currentViewId = category.id`. Highlighted with a left accent bar (`#89b4fa`) + bold text whenever `currentViewId` is the category's own id, one of its `actions[].id`, or one of its `relatedViewIds` (see below) — computed once by `Main.qml`'s `isCategoryActive(category)` and handed down as `SidebarCategory.current`, rather than each entry point maintaining its own "am I selected" logic.
- **Arrow toggle** — a separate `MouseArea` (28px, right side of the header row) expands/collapses the category's action list *without* changing `currentViewId`. `SidebarCategory.expanded` auto-flips true the moment the category becomes active (`onCurrentChanged`), so navigating in from elsewhere (e.g. the "Transactions" button inside Manage Accounts) always reveals which action put you there — the user can still collapse/expand freely afterward.
- **Action list** — `{ id, label }` objects (`categories[i].actions` in `Main.qml`; every category's actions are this shape now, including the still-unwired placeholders on the first 5 PROJECT.md categories, for one consistent shape app-wide). Clicking an action emits `SidebarCategory.actionTriggered(actionId)`; the row for the *currently active* action is highlighted the same way the category header is (accent bar + bold + blue text via `SidebarCategory.currentActionId`), not just the category. **Four actions are wired up so far**: Transactions Management → `"importTransactions"` (see § Statement Import) and `"manageCategories"` (see § Category Management); Account Management → `"manageAccounts"` and `"accountTypes"` (see § Account Management). Database Management has no actions at all — its landing page holds the real controls directly (see § Database Management). Every other action on the first 5 categories is still a no-op placeholder — `Main.qml`'s `onActionTriggered` only sets `currentViewId` when the id is in `root.overlayViewIds`.

To add a new sidebar category: append `{ id, label, actions }` to `Main.qml`'s `categories` array, and add the matching page as the next child of the `StackLayout` (order must still match array order — `stackIndex` looks a category up by id, but positionally, same "array order = child order" contract as before, just keyed by id instead of a bare int so callers never juggle magic numbers).

**Overlay pages** (full-content pages reached from an action, not a category — e.g. ImportSnapshotPage, CategoriesManagePage, AccountsManagePage, AccountTypesCategoriesPage, AccountTransactionsPage): `Main.qml` uses one `Loader` per overlay, anchored over the same content area as the `StackLayout`, each gated by `active: root.currentViewId === "someId"`. `root.overlayViewIds` lists every id that means "show an overlay, not the StackLayout" (`importTransactions`, `manageCategories`, `manageAccounts`, `accountTypes`, `accountTransactions`); `StackLayout.visible: !root.isOverlayView` reads that one list instead of checking five booleans by hand. Each overlay page emits a `closeRequested()` signal that sets `root.currentViewId` back to wherever "close" should land — this is the pattern to copy for the next wired-up action.

  `AccountTransactionsPage` is reached from *inside* `AccountsManagePage` (its "Transactions" button, not a sidebar action) via `AccountsManagePage.viewTransactionsRequested(accountIndex)` — `Main.qml` stores the index in `root.accountTransactionsIndex` and sets `root.currentViewId = "accountTransactions"`. Its own `closeRequested()` sets `currentViewId` back to `"manageAccounts"`, not to the sidebar-driven `StackLayout` — it was reached from that page, not from the sidebar, so the back arrow should go back to where the user came from. Because `"accountTransactions"` is never one of any category's `actions[].id` (it's not a sidebar action), the **Account Management** category declares it in a `relatedViewIds: ["accountTransactions"]` array instead — `isCategoryActive()` checks that too, so the sidebar still highlights "Account Management" while this page is open.

  **The gotcha this replaced is now structurally impossible to reintroduce**: every navigation entry point (category click, action click, "Transactions" button, every overlay's `closeRequested()`) does exactly one thing — assign `root.currentViewId` — instead of remembering to clear four *other* flags first. There is nothing left to forget.

---

## Shared Components

Added in the milestone cleanup pass to answer "the files are huge, make new pages faster to
build" and "homogenize the UI." Before this, `CategoriesManagePage.qml`,
`AccountsManagePage.qml`, and `AccountTypesCategoriesPage.qml` each hand-rolled their own
copy of the same page-header, add/edit-dialog, confirm-dialog, badge, and button patterns —
consistent in spirit but drifting in the details (a badge color typo, an inverted zebra
stripe, buttons with no hover feedback on some pages but not others). All of `qml/components/`
below lives in the same `FinanceDashboard` QML module as every page, so no `import` line is
needed to use them (same as `SidebarCategory`/`ChartCard`/`KpiCard` before this pass) — just
reference the type name directly, same convention `Main.qml` already used for pages.

- **`PageHeader.qml`** — `RowLayout`: optional back arrow (`showBack`), `title`, optional
  bare-red `"[MOCK]"` marker (`mock: true`, styled to match what the 5 analytics pages
  already used — not upgraded to a pill, to avoid an unrequested visual change), a stretch,
  then any caller-supplied trailing content (add buttons, `StatusBadge`s, filters) via the
  default property (`content`, same slot convention as `ChartCard.content`). `showBack:
  true` on pages reached by drilling into a sidebar action (Categories/Accounts/
  AccountTypes/Import/AccountTransactions management pages); left `false` on top-level
  category pages and the 5 mock analytics pages. `backClicked()` signal — caller decides
  what "back" means (usually `closeRequested()`, sometimes returns to a different overlay,
  see § Sidebar Navigation).
- **`PillButton.qml`** — the big colored action button (Add/Save/Cancel/Delete in dialogs,
  "+ Add X" header buttons, Database Management's trigger buttons, Import's "Choose
  file"/"Save"/"Link this account"). `variant: "primary" | "neutral" | "danger"`; width is
  implicit (label + padding) unless the caller sets an explicit `width`/`height` — every
  page used to hand-pick its own pixel width for what was conceptually the same button.
  `fontSize`/`bold`/`horizontalPadding` are overridable for the handful of smaller/denser
  instances (e.g. the 24px-tall "Link this account" button on `ImportSnapshotPage.qml`).
- **`GhostButton.qml`** — small bordered inline-row action ("✎ Edit", "＋ Subcategory", "＋
  Add hash", "Transactions", and the "×" remove square). `variant: "neutral"` keeps the
  caller's `textColor` at rest and adds a grey hover tint; `variant: "danger"` mutes the
  text to grey at rest and flashes red on hover (matches the "×" remove buttons' original
  behavior exactly). The chip edit/remove glyphs on `AccountTypesCategoriesPage.qml` used to
  be bare `Text` + `TapHandler` with **no hover feedback at all**, unlike every other
  Edit/Remove button in the app — `GhostButton` gives them the same affordance as everywhere
  else.
- **`StatusBadge.qml`** — colored pill for every status/state indicator: parse-success/
  failure pills, the linked/not-linked badge, parse issue-count and log-level badges, and
  the project on-track/gap badge. `variant: "success" | "warning" | "danger" | "neutral"`;
  `pillHeight` stays caller-set (the app genuinely uses this at several sizes on purpose —
  26px status pills, 22-24px inline chips, 18px log-entry badges) with `radius` always half
  the height so it's always a true pill. Fixed two drift bugs found while extracting this:
  `ProjectsPage.qml`'s off-track badge used `"#3d1a28"` (a transposed digit — every other
  danger badge in the app uses `"#3d2028"`) and had no `border.color` set at all, unlike
  every other badge instance.
- **`FormRow.qml`** — one "label + TextField" row, styled identically everywhere a dialog
  needs free-text input. The `TextField` recipe (`bg #11111b`, `border #313244` / focus
  `#89b4fa`) was already 100% consistent across every dialog before this was extracted —
  this just removes the copy-paste (including `ProjectsPage.qml`'s page-local `component
  FormField`, which was the same shape with a wider default label column, now just
  `FormRow { labelWidth: 150 }`).
- **`ColorPickerRow.qml`** — 32×32 circle swatch + "Choose color…" pill + hex text, owns its
  own native `ColorDialog` instance internally (verified against
  doc.qt.io/qt-6/qml-qtquick-dialogs-colordialog.html). **Deliberately not a shared/singleton
  `ColorDialog`** — each dialog needs an independent in-progress color, and a declarative
  binding to a shared instance breaks permanently the first time the user picks a color (see
  § Category Management) — a fresh `ColorPickerRow` per usage sidesteps that entirely rather
  than working around it.
- **`FormDialog.qml`** — shared shell for every add/edit `Popup` (categories, subcategories,
  accounts, account types, account categories, add-hash, new-project). Caller supplies
  `dialogTitle`, `saveText`, and form rows via the default property (same `content` slot
  convention as `ChartCard`/`PageHeader`); `saveClicked()` fires instead of auto-closing, so
  the caller can validate first and decide whether to actually close — matches every
  existing dialog's "trim, bail out on empty, otherwise call the controller and close"
  pattern exactly.
- **`ConfirmDialog.qml`** — shared shell for every destructive-action confirmation (delete
  category, delete account, delete account type/category, reset database, reset to mock
  data). `confirmVariant` defaults to `"danger"` but stays overridable per call — Database
  Management's "Reset to Mock Data" confirm step deliberately keeps the red/danger styling
  even though its own trigger button is blue/primary (both actions are equally
  irreversible), so the override exists for the *next* case where trigger and confirm colors
  should actually match, rather than hard-coding red everywhere.
- **`EmptyState.qml`** — "No … yet" placeholder text shown under a list's header when its
  model is empty. Style was already 100% consistent across every instance.

**Not extracted, and why:** the account/category badge (arbitrary per-category color, not
one of the 4 `StatusBadge` variants), the hash chips on `AccountsManagePage.qml`, and the
per-page data tables (`ImportSnapshotPage.qml`'s transaction review table,
`AccountTransactionsPage.qml`'s read-only table, `ProjectsPage.qml`'s loan-scenario table) —
each has different columns/widths, so a generic `DataTable` would need more configuration
surface than it would save in code. `ProjectsPage.qml`'s hoverable project-card list (list
view) also stays bespoke — it reimplements `ChartCard`'s look but needs click/hover
affordance `ChartCard` doesn't have; a `hoverable` option on `ChartCard` itself would be the
next step if a second page needs the same pattern.

---

## Statement Import ("Import Transactions")

Reached from the sidebar (Transactions Management → "Import Transactions", renamed from
"New Monthly Snapshot" once Data Entry was split into Transactions/Account/Database
Management — see § Transactions Management). Lets the user pick a
bank/extract file, parse it into editable+categorizable transactions, and review parse
issues before a real save. Design decisions made when this was built:

- **Persistence scope: parse is REAL, save is now REAL too.** `SnapshotImportController::
  save(accountId, accountLabel)` builds a `Data::Transaction` per included row and writes
  them through `Database::insertTransactions()` (see § Account Management for the
  `transactions` table). It also auto-links the statement's account-hash fingerprint to the
  chosen account (`Database::accountHasHash()`/`addAccountHash()`, source `"auto"`) if that
  account doesn't have it on file yet. This changed once accounts became a real, DB-backed
  concept (`AccountController` — see § Account Management); before that there was nothing
  real to save a transaction *against*. The parsing itself was always real (no mock data):
  `CheckingCsvTemplateV1` actually reads and parses the CSV format.
- **Template design: one hardcoded C++ class per format, not a declarative/config-driven
  reader.** Decided against a generic column-mapping descriptor because formats aren't
  simple tables (multi-line headers, PDF layouts planned) — each format gets real parsing
  logic. New format = new `ImportTemplate` subclass + one line in
  `ImportTemplateRegistry::all()`.
- **Account matching: auto by *stored* account-hash fingerprint only, otherwise manual.**
  `SnapshotImportController::matchAccountIndex()` never compares the statement's IBAN/BIC to
  an account's IBAN/BIC directly — accounts don't store iban/bic/number at all anymore (see
  § Account Management, removed for security). Instead the shared `AccountHashing::compute()`
  (`src/utils/AccountHashing.h`, HMAC-SHA256 of the normalized IBAN keyed by the normalized
  BIC) is used once on the statement's IBAN/BIC, and the result is compared against each
  account's *stored* `hashes` list (the `account_hashes` table, read via
  `AccountController.accounts`) — an account only matches if it has actually been linked
  (auto-derived at creation time when iban/bic were still stored on `Account`, auto-added on
  a prior save, or manually added on `AccountsManagePage.qml`). There is no account-number
  fallback anymore — no match at all → the QML page shows a `ComboBox` for a manual pick,
  plus a "not linked" badge (see below). The fingerprint is also surfaced as
  `statementInfo.accountHash` (pretty, "—" when empty) and `statementInfo.accountHashRaw`
  (raw, empty string when none) and shown in the `ImportSnapshotPage.qml` header so the match
  is auditable.
- **`AccountHashing::compute()` was extracted from `SnapshotImportController::accountHash()`
  into `src/utils/AccountHashing.h`** once `AccountController` needed the exact same HMAC
  computation to auto-derive a stored hash for an account with an IBAN+BIC on file. Both
  call sites must use this one function — an independently re-derived HMAC would risk
  normalization drift between the statement side and the account side.
- **"Not linked" badge + inline linking.** `ImportSnapshotPage.qml` shows a small badge next
  to the target-account picker driven by the *selected* account's own `linked`/`hashCount`
  fields (from `AccountController.accounts`) — independent of whether the loaded statement
  happened to auto-match it. A "Link this account" button appears when the statement has a
  computable fingerprint (`statementInfo.accountHashRaw`) the selected account doesn't have
  yet; it calls `AccountController.linkAccountHash(accountIndex, hash)` directly (source
  `"auto"`) so the account is usable for auto-match on the *next* import without waiting for
  a save. `AccountController.accountHasHash()` is the guard that hides the button once
  already linked.
- **No C++-to-C++ singleton coupling between the two controllers.** `SnapshotImportController::
  save()` writes straight to `Database` (transactions + hash) rather than reaching into the
  `AccountController` QML_SINGLETON from C++. The QML side is responsible for calling
  `AccountController.reload()` right after `controller.save(...)` so the singleton's cached
  `accounts`/`hashes` picks up the write — see `AccountController::reload()`'s doc comment
  for why this was chosen over a static self-pointer trick.

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
no separate QML files per step. Accounts come from `AccountController` (QML_SINGLETON) —
this page used to instantiate `DataEntryController` just to read its mock accounts list;
that's gone now that accounts are a real, shared, DB-backed concept. Uses
`QtQuick.Dialogs.FileDialog` (nameFilters driven by the selected template's `fileFilter()`).
Each transaction row has two
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

Reached from the sidebar (Transactions Management → "Manage Categories"). Lets the user add,
rename, recolor, and remove categories and their subcategories — the same list read
everywhere a transaction gets classified (the statement-import review table on
`ImportSnapshotPage.qml`).

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
  `SnapshotImportController`/`AccountController` — grouped by domain like every other
  controller folder; moved here from a standalone `category/` folder since being a
  `QML_SINGLETON` is a registration detail, not a reason for its own directory.
- **`reload()`** re-reads `categories`/`subcategories` from `Database` and re-emits
  `categoriesChanged()` — deliberately does **not** reseed defaults if the DB comes back
  empty (unlike the constructor, which does seed on first launch against a genuinely empty
  DB). This distinction matters for `DatabaseManagementController::resetDatabase()` — see §
  Database Management: if `reload()` silently reseeded on empty, "Reset Database" could never
  produce a truly empty database once QML called `reload()` afterward.
- **`statusText`/`statusChanged()`** — added in the milestone cleanup pass for parity with
  `AccountController` (both are the app's two `QML_SINGLETON`s with near-identical CRUD
  shapes). Every CRUD method now calls the private `setStatus(msg)` on both success
  ("✓ Category added") and failure ("⚠ A category with this name already exists"/"⚠ Could
  not save the category"), auto-clearing after 3s — before this, `CategoryController` simply
  returned early on a duplicate name or DB-write failure with zero UI feedback, unlike every
  equivalent `AccountController` method. Color-only changes (`setCategoryColor`/
  `setSubcategoryColor`) stay silent on purpose, matching
  `AccountController::setAccountCategoryColor()`'s existing convention.
  `CategoriesManagePage.qml`'s header shows it via a `StatusBadge`, same as
  `AccountsManagePage.qml` already did for `AccountController.statusText`.
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

## Account Management ("Manage Accounts" / "Account types & categories")

Own sidebar category (split out of the former "Data Entry" page — see §
Transactions Management). Two sidebar actions: "Manage Accounts"
(`AccountsManagePage.qml`) lets the user add, edit, and remove accounts, manage each
account's stored hash fingerprints, and open a full page of an account's persisted
transactions; "Account types & categories" (`AccountTypesCategoriesPage.qml`) manages the
two classification lists accounts are built from. Replaces the MOCK accounts list that
used to live on `DataEntryController` (name/type/iban/bic/number, nothing persisted) —
accounts became a real, cross-cutting, DB-backed concept once statement-import save needed
a real account to save transactions against, and once the product asked for hash
management and a per-account transactions view.

- **No iban/bic/number stored on an account — removed for security.** `Data::Account` used
  to carry `iban`/`bic`/`number` alongside name/type/category/currency/notes; the product
  decision was to stop persisting that data at all. This has two knock-on effects: (1) the
  `accounts` table dropped all three columns (see schema below), and (2) `AccountController`
  no longer auto-derives a hash fingerprint *at account-creation time* — with no iban/bic on
  the account row, there's nothing to compute `AccountHashing::compute()` from there anymore.
  An account only gains a hash by being linked from the import review screen
  (`AccountController::linkAccountHash()`, source `"auto"`) or added manually
  (`AccountController::addHash()`, source `"manual"`) — see § Statement Import's account
  matching bullet, which was updated to drop its old "fallback to account number" path for
  the same reason (there's no `number` field left to fall back to).
- **Two classification fields per account, not one — and both are now DB-managed.** `type`
  is the actual banking/financial product (`"CCP"`, `"Livret A"`, `"LDDS"`, `"PEL"`,
  `"Assurance Vie"`, `"PEA"`, `"CTO"`, `"Other"` by default) — originally a fixed
  `QStringList` (round 1), promoted to a full DB-backed CRUD list (`account_types` table,
  `Data::AccountType`) in round 2 per product decision, so new financial products can be
  added without a code change. `category` is the usage bucket (`"Day-to-day"`, `"Savings"`,
  `"Investments"`, `"Urgent savings"` by default) — DB-managed via its own CRUD surface
  (`account_categories` table), same add/rename/recolor/remove pattern `CategoryController`
  uses for categories, just without subcategories and (for `type`) without color at all.
  Badge colors on `AccountsManagePage.qml` are keyed off `category` (via
  `AccountController.accountCategoryColor()`), not `type` — `type` has no color concept.
- **Currency is a fixed 2-option selector, not free text.** `AccountsManagePage.qml`'s
  add/edit dialog uses a plain `ComboBox` over `[{code:"EUR",label:"EUR €"},
  {code:"USD",label:"USD $"}]` (`textRole: "label"`) instead of a `TextField` — per product
  decision, only these two currencies are needed for now. `Data::Account::currency` still
  stores the plain code (`"EUR"`/`"USD"`), same as before; only the input widget changed.
- **Hashes are their own table, not a column** (`account_hashes`), because an account can
  carry *multiple* fingerprints: any number of manually-added ones, or ones linked straight
  from the import review screen (`AccountController.linkAccountHash()`, see § Statement
  Import's "Link this account" action). `hashCount`/`linked` on each `accounts()` row let
  QML show a "not linked yet" badge without walking the hashes array itself — this is the
  badge shown on `ImportSnapshotPage.qml` next to the target-account picker, and the
  `⚠ not linked` / `✓ linked · N hashes` badge on `AccountsManagePage.qml`'s account rows.
- **"Transactions" opens a routed page, not a modal.** Product decision: the transactions
  table is significant enough content that it shouldn't live in a `Popup`. `AccountsManagePage.qml`
  emits `viewTransactionsRequested(accountIndex)` instead of opening a popup;
  `Main.qml` catches it, stores the index in `root.accountTransactionsIndex`, swaps
  `showAccountsOverlay` for `showAccountTransactionsOverlay`, and instantiates
  `AccountTransactionsPage { accountIndex: root.accountTransactionsIndex }`. That page's own
  `closeRequested()` returns to Manage Accounts, not the sidebar view (see § Sidebar
  Navigation). The backing data/logic is unchanged —
  `AccountController::transactionsForAccount(accountIndex)` still does the formatting.
- **`AccountController` (src/controllers/dataentry/AccountController.h/.cpp) is a
  `QML_SINGLETON`**, same reasoning as `CategoryController`: accounts are cross-cutting
  (edited on `AccountsManagePage.qml`/`AccountTypesCategoriesPage.qml`, read on
  `ImportSnapshotPage.qml` and `AccountTransactionsPage.qml`), so a per-page instance would
  silently reset on navigation.
- **`reload()` instead of a C++-to-C++ back-reference.** `SnapshotImportController::save()`
  writes straight to `Database` (see § Statement Import) rather than calling into the
  `AccountController` singleton from C++ — QML_SINGLETON instances are normally only reached
  from QML, and reaching for a static self-pointer trick to call it from another controller
  would couple two controllers that otherwise don't know about each other. Instead
  `AccountController::reload()` re-reads accounts, account categories, and account types
  from `Database` and re-emits `accountsChanged()`/`accountCategoriesChanged()`/
  `accountTypesChanged()`; `ImportSnapshotPage.qml`'s Save button calls `controller.save(...)`
  then `AccountController.reload()` itself. Deliberately does **not** reseed defaults if the
  DB comes back empty — same reasoning as `CategoryController::reload()`, see § Database
  Management for why.
- **Data shape**, rebuilt into a `QVariantList` on every `accounts()`/`accountCategories()`/
  `accountTypes()` read (same convention as `CategoryController` — no separate
  `QAbstractListModel`):
  ```
  accounts: [{ id, name, type, category, currency, notes,
               hashes: [{ id, hash, source }, ...], hashCount, linked }, ...]
  accountCategories: [{ name, color }, ...]
  accountTypes: [{ name }, ...]
  ```
  Unlike `CategoryController.categories`, each account row exposes its own `id` (the
  `accounts` table row id) — `SnapshotImportController::save()` and
  `matchAccountIndex()` need the real id to write/compare against, not just a list index.
- **CRUD**: `addAccount`/`updateAccount`/`removeAccount` (index into `accounts()`, name/
  type/category/currency/notes only — no iban/bic/number parameters anymore);
  `addAccountCategory`/`renameAccountCategory`/`setAccountCategoryColor`/
  `removeAccountCategory` (index into `accountCategories()`); `addAccountType`/
  `renameAccountType`/`removeAccountType` (index into `accountTypes()`, no color);
  `addHash`/`linkAccountHash` (source `"manual"` vs `"auto"` respectively, both no-op on an
  empty or already-stored hash)/`removeHash` (accountIndex + hashIndex).
  `accountCategoryColor(name)`/`accountHasHash(accountIndex, hash)` are convenience
  lookups. Every mutating method writes through `Database::instance()` first and only
  updates the in-memory list (and emits its `*Changed()` signal) if that write succeeded —
  same "no phantom in-memory-only entry" discipline as `CategoryController`.
- **`removeAccount()` cascades hashes and transactions** (`Database::removeAccount()`, one
  transaction: `AccountHashesTable::removeForAccount()` → `TransactionsTable::
  removeForAccount()` → `AccountsTable::remove()`) — `AccountsManagePage.qml` shows an
  explicit confirmation dialog before calling it, same pattern as
  `CategoriesManagePage.qml`'s delete-category confirmation.
- **`transactionsForAccount(accountIndex)`** — `Q_INVOKABLE`, formats every persisted
  `Data::Transaction` for that account (`Database::transactionsForAccount()`) with
  `Fmt::money`/date strings for `AccountTransactionsPage.qml` — the "table with all
  transactions registered at that account" from the product request. Read-only for this
  pass; editing/deleting a single persisted transaction is a future addition once there's a
  UI need for it (see Known Limitations/TODOs).
- **Seeding**: on an empty DB, `seedDefaults()` writes the 8 default account types (CCP,
  Livret A, LDDS, PEL, Assurance Vie, PEA, CTO, Other), the 4 default account categories
  (Day-to-day, Savings, Investments, Urgent savings), and the same 6 demo accounts the old
  `DataEntryController` mock used to hardcode — now without any iban/bic/number, and
  without any hash auto-derivation (nothing to derive from). All 6 demo accounts start
  "not linked"; the badge has something real to show without needing a save first, same as
  before, just without a pre-linked pair now that there's no iban/bic to seed a hash from.

### Account / account-hash / account-category / account-type / transaction DB schema

```sql
CREATE TABLE account_categories (
  id    INTEGER PRIMARY KEY AUTOINCREMENT,
  name  TEXT    NOT NULL UNIQUE,
  color TEXT    NOT NULL
);

CREATE TABLE account_types (
  id    INTEGER PRIMARY KEY AUTOINCREMENT,
  name  TEXT    NOT NULL UNIQUE
);

CREATE TABLE accounts (
  id       INTEGER PRIMARY KEY AUTOINCREMENT,
  name     TEXT    NOT NULL UNIQUE,
  type     TEXT    NOT NULL DEFAULT '',
  category TEXT    NOT NULL DEFAULT '',
  currency TEXT    NOT NULL DEFAULT 'EUR',
  notes    TEXT    NOT NULL DEFAULT ''
);

CREATE TABLE account_hashes (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  account_id INTEGER NOT NULL,
  hash       TEXT    NOT NULL,
  source     TEXT    NOT NULL DEFAULT 'manual',  -- 'auto' | 'manual'
  UNIQUE(account_id, hash)
);
CREATE INDEX idx_account_hashes_account ON account_hashes(account_id);
CREATE INDEX idx_account_hashes_hash    ON account_hashes(hash);

CREATE TABLE transactions (
  id                INTEGER PRIMARY KEY AUTOINCREMENT,
  account_id        INTEGER NOT NULL,
  confirmation_date TEXT    NOT NULL DEFAULT '',  -- ISO-8601 or ''
  operation_date    TEXT    NOT NULL DEFAULT '',  -- ISO-8601 or ''
  description       TEXT    NOT NULL DEFAULT '',
  debit             REAL    NOT NULL DEFAULT 0,
  credit             REAL    NOT NULL DEFAULT 0,
  balance_after     REAL    NOT NULL DEFAULT 0,
  category          TEXT    NOT NULL DEFAULT '',
  subcategory       TEXT    NOT NULL DEFAULT '',
  source_template   TEXT    NOT NULL DEFAULT ''
);
CREATE INDEX idx_transactions_account ON transactions(account_id);
```

- **`accounts` no longer has `iban`/`bic`/`number` columns** — dropped entirely (not just
  hidden in the UI) per the security-driven product decision above. Any account created
  before this change would need a manual migration/reset; there is no migration script
  because the DB is still in active development (see § Database Management's "Reset
  Database" for the intended way to pick up a schema change during this phase).
- **No DB-level cascade** (no foreign keys declared anywhere in this schema, consistent
  with the rest of the app) — `Database::removeAccount()` does the accounts/hashes/
  transactions cascade manually in one transaction (see above).
- **`transactions` has no update/remove-by-id surface yet** — `TransactionsTable::
  insertBatch()` only appends (accumulates across multiple statement imports for the same
  account, does not replace/clear), and `allForAccount()` is read-only. Editing or deleting
  a single persisted transaction needs a UI first; not built this pass.
- **`account_categories`/`account_types`/`account_hashes` removal has no "in use" guard**,
  same known limitation as `CategoryController::removeCategory()`: deleting a category,
  type, or hash that an account/transaction still references by string leaves that string
  in place (not a foreign key, so nothing breaks), it just stops appearing in the
  manageable list. Revisit if this becomes confusing in practice.
- **`AccountsTable`/`AccountHashesTable`/`AccountCategoriesTable`/`AccountTypesTable`/
  `TransactionsTable`** (`src/db/tables/`) each own all SQL for their table, same shape as
  `CategoriesTable`/`SubcategoriesTable`: `createTable()`, CRUD by row id, `all()`/
  `allForAccount(id)` readers. `AccountsTable::all()` leaves `hashes` empty —
  `Database::loadAccounts()` composes it from `AccountHashesTable::allForAccount(id)`,
  mirroring how `Database::loadCategories()` composes subcategories.

### `AccountsManagePage.qml`
Same visual language as `CategoriesManagePage.qml` (Popup dialogs, `colorToHex()` helper).
No longer owns the account-category CRUD section — that moved to
`AccountTypesCategoriesPage.qml` (below) once the product asked for a dedicated page for
it. One list section — accounts (one block per account: header row with name/type/
category badge/linked badge/actions, a details row for currency/notes — no iban/bic/number
anymore, see above — and a hashes row with a chip per stored hash plus "+ Add hash"). The
account edit dialog's type and category fields are both plain `ComboBox`es (not
`ColorComboBox`) — deliberately: `ColorComboBox.currentIndex` is bound declaratively from
`currentName` internally, and that binding permanently breaks the first time the user picks
an item (documented in `ColorComboBox.qml` itself); every existing use of `ColorComboBox` in
this app lives inside a `Repeater` delegate that gets recreated whenever its model changes,
which resets the binding for free. This dialog's `Popup.contentItem` is not recreated
between opens, so a `ColorComboBox` here would show a stale highlighted item the second time
the dialog opens after any prior interaction with it — plain `ComboBox`es with an
imperatively-set `currentIndex` (set in `openForNew()`/`openForEdit()`) avoid that class of
bug entirely, at the cost of not tinting the field with the category's color. The currency
field is a third plain `ComboBox` over the fixed EUR/USD pair (see above). The
"Transactions" button emits `viewTransactionsRequested(accountIndex)` (see above) instead of
opening a popup.

### `AccountTransactionsPage.qml`
Routed page (not a `Popup`) reached from `AccountsManagePage.qml`'s "Transactions" button —
see the bullet above. Takes `property int accountIndex` (set by `Main.qml` before showing),
computes the account and its formatted rows from `AccountController.accounts`/
`transactionsForAccount(accountIndex)`, and renders the same read-only transaction table
(date/description/debit/credit/balance/category) the old `transactionsPopup` used to show,
plus a back arrow (`closeRequested()`).

### `AccountTypesCategoriesPage.qml`
Reached from the sidebar (Account Management → "Account types & categories"). Manages both
lists accounts are classified by: account types (plain name CRUD — add/rename/remove via a
chip list + dialogs, no color) and account categories (name+color CRUD — chip list +
dialogs + a `ColorDialog` for recoloring, same pattern `CategoriesManagePage.qml` uses for
categories). This is the page the account-category CRUD section moved out of
`AccountsManagePage.qml` into, once the product asked for a dedicated types/categories page
rather than bundling it into the accounts list page.

---

## Transactions Management

Own sidebar category — the renamed, narrowed successor to the old "Data Entry" category
(also renamed from "Data Entry" per product decision, since accounts and database resets
moved to their own categories, leaving just the transaction-import + category-management
actions here). `qml/pages/TransactionsManagementPage.qml` is this category's own landing
page: a minimal placeholder (header + text pointing at the two sidebar actions below) — no
controller, no mock content, since both actions already have their own real pages. Actions:

- **"Import Transactions"** (renamed from "New Monthly Snapshot") — `ImportSnapshotPage.qml`
  / `SnapshotImportController`, see § Statement Import.
- **"Manage Categories"** — `CategoriesManagePage.qml` / `CategoryController`, see §
  Category Management.

`src/controllers/dataentry/DataEntryController.h/.cpp` and `qml/pages/DataEntryPage.qml`
(the old landing page, with its MOCK snapshot form and transaction-entry form) were
superseded by this split, and were deleted outright in the milestone cleanup pass — see §
App Status.

---

## Database Management

Own sidebar category, no sub-actions — `qml/pages/DatabaseManagementPage.qml` holds the
real controls directly (no landing-page/action split like Transactions Management or
Account Management, since there's only one small set of actions and no reason to hide them
behind a click). Added per product request: useful for picking up structural DB changes
during development without a migration script (see the `accounts` schema note above for a
concrete example — dropping iban/bic/number needs exactly this).

- **Two actions, both behind a confirmation dialog** (irreversible): "Reset Database" wipes
  every table to genuinely empty; "Reset to Mock Data" wipes everything then reseeds the
  same default categories/account-categories/account-types/demo-accounts used on first
  launch, plus a handful of fabricated sample transactions (so the transactions view has
  something to show immediately, per product decision when this was scoped).
- **`DatabaseManagementController` (src/controllers/database/DatabaseManagementController.h/.cpp)
  is a plain `QML_ELEMENT`, not a `QML_SINGLETON`** — unlike `CategoryController`/
  `AccountController`, it has no state of its own worth keeping alive across navigation; it
  just triggers an action and reports a transient `statusText`.
- **`Database::resetAll()`** (see Component Responsibilities) drops and recreates every
  table in one transaction. Both controller actions call it first; `resetToMock()` then
  calls its own private `seedMockData()`.
- **Reset (to zero) vs. reload must stay decoupled from reseed-on-empty.** This was almost
  built the other way: the first draft had `CategoryController::reload()` and
  `AccountController::reload()` reseed their defaults if the DB came back empty after a
  reload, mirroring what their constructors do on a genuinely empty DB at first launch. That
  would have silently defeated "Reset Database" — QML calls both `reload()`s right after
  `resetDatabase()` to refresh the singletons' caches, and an auto-reseed there would
  immediately repopulate the "empty" database. Fixed before it shipped: both `reload()`
  methods are pure re-reads with no reseed branch; only the constructor (first launch) and
  `DatabaseManagementController::resetToMock()` (explicit request) ever seed data.
- **`resetToMock()` duplicates seed *values*, not seed *code*.** Rather than reaching into
  `CategoryController::seedDefaults()`/`AccountController::seedDefaults()` (both private,
  and both already auto-invoked from their own constructors on first launch), `seedMockData()`
  writes its own copy of the same category/subcategory, account-category, account-type, and
  demo-account values directly through `Database`'s `add*()` methods, then builds ~6
  fabricated `Data::Transaction` rows (salary credit, rent/groceries/transport/utilities/
  leisure debits, dated relative to `QDate::currentDate()` so they never look stale) against
  the first demo checking account and writes them via `Database::insertTransactions()`. This
  keeps `DatabaseManagementController` from needing a back-reference into two unrelated
  singletons just to trigger their private seeding logic — the tradeoff is that the seed
  *values* (category names/colors, account type names, etc.) exist in two places and must be
  kept in sync by hand if either changes.
- **QML, not C++, refreshes the singletons.** Same "no C++-to-C++ singleton coupling"
  pattern as `SnapshotImportController::save()` (see § Statement Import) —
  `DatabaseManagementController` never touches `CategoryController`/`AccountController`
  directly. `DatabaseManagementPage.qml` listens for `databaseReset()` and calls
  `CategoryController.reload()` / `AccountController.reload()` itself.

---

## QML/C++ Boundary — the pattern

Every active controller (see § App Status's table and § Directory Map's `controllers/`
listing) follows the `*DataProvider` pattern from QtGraphsExamples:

1. Controller class in `src/controllers/<domain>/`, `Q_OBJECT` + `QML_ELEMENT` (`QML_SINGLETON`
   too for `CategoryController`/`AccountController` — see § Category Management / § Account
   Management for why).
2. Registered by listing the `.h/.cpp` under `SOURCES` of `qt_add_qml_module` in `CMakeLists.txt`. **The controller's directory must also be on `target_include_directories`** — the auto-generated type-registration `.cpp` includes headers by basename.
3. Instantiated *in QML* (`NetWorthController { id: controller }`), not via context properties.
4. Data crosses the boundary as `Q_PROPERTY` (scalars, with `NOTIFY`) and `Q_INVOKABLE QVariantList` (arrays).
5. QML never computes: sums, cumulative layers, axis ranges, diffs all come pre-computed from C++.

*(The original Qt Widgets app and its ported-to-QML-but-unwired intermediate generation —
`AnalyticsController`/`AnalyticsTab.qml`, `EntryController`, the legacy `DatabaseController`,
`OverviewController`, `PortfolioController`, the legacy `ImportController` — were deleted in
the milestone cleanup pass once nothing referenced them; see § App Status. If you need the
history of how a specific page/table was first ported to QML, check git log before this
pass rather than looking for those files on disk.)*

### QML delegate gotcha (still relevant, general Qt Quick behavior)
When a TableView/Repeater delegate declares ANY `required property`, model roles are **no longer injected as context properties** — every role used (`display`, etc.) must then be declared as a `required property` too. The `TableView.editDelegate` is a separate component and can still use context roles (matches the Qt docs example).

---

## Component Responsibilities (unchanged C++ layer)

### `db/Database.h/.cpp`
- Meyers singleton — `Database::instance()`.
- Owns the `QSqlDatabase` connection (`"finance_dashboard"` named connection).
- Owns `SnapshotsTable m_snapshots`, `AssetEntriesTable m_assetEntries`,
  `CategoriesTable m_categoriesTable`, `SubcategoriesTable m_subcategoriesTable`,
  `AccountsTable m_accountsTable`, `AccountHashesTable m_accountHashesTable`,
  `AccountCategoriesTable m_accountCategoriesTable`, `AccountTypesTable m_accountTypesTable`,
  `TransactionsTable m_transactionsTable`.
- `open()` enables WAL mode then delegates `createTable()` to each table object.
- `saveSnapshot()`, `removeCategory()`, and `removeAccount()` are the only methods that
  touch transactions (cross-table writes).
- `loadCategories()`, `addCategory()`/`renameCategory()`/`setCategoryColor()`/
  `removeCategory()` and the subcategory equivalents — see § Category Management.
- `loadAccounts()`, `addAccount()`/`updateAccount()`/`removeAccount()` (name/type/category/
  currency/notes only — no iban/bic/number, see § Account Management),
  `addAccountHash()`/`removeAccountHash()`/`accountHasHash()`, `loadAccountCategories()`/
  `addAccountCategory()`/`renameAccountCategory()`/`setAccountCategoryColor()`/
  `removeAccountCategory()`, `loadAccountTypes()`/`addAccountType()`/`renameAccountType()`/
  `removeAccountType()`, `insertTransactions()`/`transactionsForAccount()`/
  `transactionCountForAccount()` — see § Account Management.
- `resetAll()` — drops and recreates every table this app owns, in one transaction. Used
  exclusively by `DatabaseManagementController` — see § Database Management. Deliberately
  does **not** reseed anything itself (that's `DatabaseManagementController::resetToMock()`'s
  job) so it can serve both "reset to zero" and "reset to mock" call sites.
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

### `models/Account.h` / `models/Transaction.h`
- `Data::AccountHash` — `{id, hash, source}`; `Data::Account` — `{id, name, type, category, currency, notes, hashes[]}` (no iban/bic/number — removed for security, see § Account Management); `Data::AccountCategory` — `{id, name, color}`; `Data::AccountType` — `{id, name}`. Same header-only POD convention as `Category.h` — see § Account Management for the `type` vs `category` split.
- `Data::Transaction` — `{id, accountId, confirmationDate, operationDate, description, debit, credit, balanceAfter, category, subcategory, sourceTemplate}`. Distinct from `Data::ImportedTransaction` (`models/ImportedStatement.h`), which is the pre-save staging row on the import review screen — this is the persisted shape once `SnapshotImportController::save()` writes it.

### `db/tables/AccountsTable.h/.cpp` / `AccountHashesTable.h/.cpp` / `AccountCategoriesTable.h/.cpp` / `AccountTypesTable.h/.cpp` / `TransactionsTable.h/.cpp`
- See § Account Management for the full schema and CRUD surface — same shape/conventions as `CategoriesTable`/`SubcategoriesTable`. `AccountTypesTable` mirrors `AccountCategoriesTable` minus the color column.
- `TransactionsTable::insertBatch()` only appends (no delete-then-insert like `AssetEntriesTable`) since multiple statement imports for the same account should accumulate, not overwrite.

### `utils/`
- `Formatter.h` — `Fmt::money(double, bool showPlus)` French-locale currency; `Fmt::dateOrDash(const QDate&)` display date formatting; `Fmt::gCurrency` (EUR/USD). *TODO: expose `Fmt::money` to QML (wrap in a QML singleton or controller Q_INVOKABLE) so formatting stays in C++.*
- `AccountHashing.h` — HMAC-SHA256(IBAN, key=BIC) fingerprint, see § Statement Import.

**Deleted in the milestone cleanup pass, fully orphaned (zero references anywhere once found,
not even from `Database`):** `models/PortfolioData.h/.cpp` (`Data::DataPoint`/
`Data::PortfolioSummary`, JSON portfolio import), `utils/JsonReader.h` (generic JSON
deserializer `PortfolioData` was the only consumer of), `utils/FileFinder.h` (file-location
helpers only the legacy `ImportController` called), and `data/DebugData.h/.cpp` (mock
holdings/summary/performance factories only the legacy `PortfolioController`/
`OverviewController` read). All four were leftover support code for the deleted legacy
generation (see § App Status) with no path back into the current app — unlike
`AssetEntry.h`/`SnapshotsTable`/`AssetEntriesTable` (see § Directory Map's `models/` entry),
which are still compiled and wired into `Database`, just without a current controller
reading them; that's a DB-schema-level question out of scope for this pass, left as-is.

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

## Creating a New Sidebar Page (checklist)

Supersedes the old "Porting a Tab" checklist, which described copying a legacy Qt Widgets
tab — there's nothing left to port (see § App Status). This is the current path for adding a
page, written to lean on § Shared Components / § Sidebar Navigation so a new page starts
from the app's existing patterns instead of a blank file:

1. Create `src/controllers/<domain>/XxxController.h/.cpp` (`QML_ELEMENT`; `QML_SINGLETON`
   only if the data is genuinely cross-cutting — see § Category Management for the
   reasoning). Pick the subfolder matching the page/domain (add a new one if it's a new
   domain), and add that subfolder to `target_include_directories` in `CMakeLists.txt`.
   Follow the `*DataProvider` pattern in § QML/C++ Boundary: QML never computes, it only
   renders `Q_PROPERTY`/`Q_INVOKABLE QVariantList` output.
2. Create `qml/pages/XxxPage.qml` — rendering only. Start from `PageHeader` for the title
   row, `ChartCard`/`KpiCard` for chart/stat sections, and `PillButton`/`GhostButton`/
   `StatusBadge`/`FormDialog`/`FormRow`/`ColorPickerRow`/`ConfirmDialog`/`EmptyState` for
   any CRUD/dialog/list UI — see § Shared Components for what each one covers before
   hand-rolling a button or badge.
3. Register both in `CMakeLists.txt` (`SOURCES` + `QML_FILES` of `qt_add_qml_module`).
4. Wire it into navigation (see § Sidebar Navigation): append `{ id, label, actions }` to
   `Main.qml`'s `categories` array and add the page as the next `StackLayout` child in the
   same position (for a top-level category page), or add a `currentViewId`-gated `Loader` +
   `Component` pair following the existing overlay pattern (for an action-triggered page).
5. For charts: copy the closest pattern from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples); verify any new property against https://doc.qt.io/qt-6/qtgraphs-index.html — **never invent property names**.
6. Update this file's § App Status table/directory listings and `README.md`'s page table.

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

- [ ] **Wire pages to real data** — replace the MOCK generators in the 5 mock page controllers (Overview/Cash Flow/Savings/Investments/Projects) with DB aggregation. The schema needs to grow: per-account monthly snapshots + categorized transactions (see PROJECT.md).
- [ ] **The `snapshots`/`asset_entries` DB tables (`SnapshotsTable`, `AssetEntriesTable`, `models/AssetEntry.h`) are still compiled and wired into `Database`, but have had no controller reading or writing them since the legacy `AnalyticsController`/`EntryController` were deleted this pass** (see § App Status / § Component Responsibilities). Decide either to build the "wire pages to real data" item above on top of this existing snapshot schema, or to drop it in favor of the newer per-transaction model (`transactions` table) — right now the app carries a second, currently-dead persistence model alongside the live one. Flagged rather than acted on: dropping a DB table is a bigger call than deleting unused UI/model code, and wasn't part of this pass's scope.
- [ ] Range selector (12–24 month window) on time charts — PROJECT.md calls for it on all pages.
- [ ] Loan simulator: add optional insurance (assurance emprunteur) flat % input.
- [ ] `Fmt::money` not yet reachable from QML — Y-axis uses `labelFormat: "%.0f €"` instead of French-locale formatting.
- [ ] Centralize palette in a `Theme.qml` singleton.
- [ ] No `.qrc` resource file — icons and fonts should be added under `resources/`.
- [ ] No tests — add a `tests/` directory with Qt Test when data logic grows.
- [ ] `CheckingCsvTemplateV1` assumes UTF-8 and `MM/dd/yyyy` dates, per the spec as given — neither was verified against a real exported file yet. Fix if a real file parses wrong.
- [ ] PDF statement templates are not implemented — `ImportTemplate` was designed to support them (interface doesn't assume CSV), but no PDF library is linked yet.
- [ ] `qml/pages/ImportSnapshotPage.qml` (`FileDialog`) and `qml/pages/CategoriesManagePage.qml`/`qml/pages/AccountsManagePage.qml` (`ColorDialog`) import `QtQuick.Dialogs` with no explicit `find_package`/`target_link_libraries` for it in `CMakeLists.txt` (relying on `qt_add_qml_module`'s automatic import scanning). If the build can't find the plugin, add `QuickDialogs2` — see the NOTE in `CMakeLists.txt`.
- [ ] Sidebar action list items are still mostly inert placeholders on the first 5 (PROJECT.md) categories — only Transactions Management → "Import Transactions"/"Manage Categories" and Account Management → "Manage Accounts"/"Account types & categories" are wired; Database Management has no actions (its landing page is the real page) — see § Sidebar Navigation.
- [ ] `CheckingCsvTemplateV1::parseAmount()` currently has the "€" removal and the final space removal **commented out** (see the `//` lines in the .cpp). As written, an amount like `"1234.56 €"` will fail to convert (the trailing `€` breaks `toDouble()`), and any input relying on space-stripping (e.g. narrow-space-separated thousands not already covered by the two explicit space-char removals above it) won't be cleaned either. This was left as-is per an in-progress edit — revisit once real exported files are available to confirm what actually needs stripping.
- [ ] `CategoryController::removeCategory()`/`removeSubcategory()` and `AccountController::removeAccountCategory()` delete unconditionally — no foreign keys anywhere in this schema, so nothing breaks, but a transaction/account already using a deleted category/account-category just keeps the stale name as plain text. Needs either a guard (refuse/warn) or an explicit "reassign" step if this proves confusing in practice.
- [ ] No reordering of categories/subcategories/account-categories — every `all()`/`allForXxx()` reader orders by row id (insertion order) only, no explicit sort-order column. Add one if drag-to-reorder is ever wanted.
- [ ] **Persisted transactions have no edit/delete UI** — `TransactionsTable` only supports append (`insertBatch`) and read-all-for-account (`allForAccount`); the transactions popup on `AccountsManagePage.qml` is read-only. Add update/delete-by-id + a UI once there's a real need to fix a mis-categorized or duplicate saved transaction.
- [ ] No migration path for existing SQLite files when the schema changes (e.g. the `accounts` table dropping `iban`/`bic`/`number` in round 2) — the intended workaround during active development is Database Management → "Reset Database"/"Reset to Mock Data" (see § Database Management), which wipes and recreates every table from scratch. Add real migrations before this app has real user data worth preserving across schema changes.
- [ ] `DatabaseManagementController::seedMockData()` duplicates the same seed values as `CategoryController::seedDefaults()`/`AccountController::seedDefaults()` rather than calling them (both are private) — see § Database Management. If either seed list changes, the other must be updated by hand or the two will drift.
- [ ] **Not yet componentized (deliberately deferred, see § Shared Components):** a `hoverable`/`onClicked` option on `ChartCard` (would let `ProjectsPage.qml`'s bespoke clickable project cards use the shared component instead of reimplementing its look); a thin wrapper around the repeated `GraphsView { theme: GraphsTheme { colorScheme: GraphsTheme.ColorScheme.Dark [...] } }` boilerplate that appears 10+ times across the 5 analytics pages. Neither blocked this pass, but both are candidates if a second page needs the same shape.
