# Directory Map

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

```
FinanceDashboard/
│
├── CMakeLists.txt          — qt_add_executable + qt_add_qml_module (URI "FinanceDashboard").
├── README.md               — User-facing: what the app is, how to build and run it.
├── ARCHITECTURE.MD         — Index into docs/ (this folder).
├── PROJECT.md              — Product spec: target pages/charts/features (Overview,
│                             Cash Flow, Savings, Investments, Projects, Data Entry;
│                             category editing + prediction). Check before adding features.
├── TODO.md                 — Backlog: known limitations, bugs, cleanups.
├── version.md              — Changelog (current + previous versions).
│
├── qml/                    — ALL UI. No business logic here.
│   ├── Main.qml            — ApplicationWindow + left sidebar (collapsible category
│   │                         list, see docs/navigation.md) + StackLayout, all driven
│   │                         by one `currentViewId` source of truth.
│   ├── components/         — Shared building blocks (see docs/shared-components.md for the
│   │   │                     dialog/button/badge family).
│   │   ├── KpiCard.qml     — LABEL / value / sub stat card.
│   │   ├── ChartCard.qml   — Titled card hosting a chart (default property slot).
│   │   ├── SidebarCategory.qml — One collapsible sidebar entry (see docs/navigation.md).
│   │   ├── ColorComboBox.qml — ComboBox that fills its own background (field and
│   │   │                     each dropdown row) with the item's color, with text
│   │   │                     color and drop-down arrow (a custom Canvas indicator)
│   │   │                     both picked per-item for contrast. Used for the
│   │   │                     category/subcategory pickers on ImportSnapshotPage.qml
│   │   │                     (see docs/statement-import.md).
│   │   ├── PageHeader.qml  — Shared page-header row (back arrow, title, "[MOCK]"
│   │   │                     marker, trailing content slot) — see docs/shared-components.md.
│   │   ├── PillButton.qml  — The big colored action button (primary/neutral/danger).
│   │   ├── GhostButton.qml — Small bordered inline-row action (Edit/Remove/etc).
│   │   ├── StatusBadge.qml — Colored pill for status/state indicators.
│   │   ├── ColumnHeaderCell.qml — Table header label with a drag-to-resize
│   │   │                     handle overlaid on its right edge. Used by
│   │   │                     ImportSnapshotPage.qml's transaction table (see
│   │   │                     docs/statement-import.md).
│   │   ├── FormRow.qml     — Label + TextField row for dialogs.
│   │   ├── ColorPickerRow.qml — Swatch + "Choose color…" + hex text + owns its ColorDialog.
│   │   ├── FormDialog.qml  — Shared add/edit Popup shell (title + form rows + Cancel/Save).
│   │   ├── ConfirmDialog.qml — Shared destructive-confirmation Popup shell.
│   │   ├── HoverPoint.qml / HoverBar.qml — chart hover-tooltip delegates (see
│   │   │                     docs/charting-gotchas.md).
│   │   ├── MoneyFlow.qml   — pure-QML Canvas Sankey diagram (Cash Flow page).
│   │   └── EmptyState.qml  — "No … yet" placeholder text.
│   └── pages/              — One .qml per PROJECT.md page (see docs/app-status.md),
│       │                     plus the Transactions/Account/Database Management pages
│       │                     (none of these three are a PROJECT.md page on their own).
│       ├── OverviewPage.qml, CashFlowPage.qml, SavingsPage.qml,
│       ├── InvestmentsPage.qml, ProjectsPage.qml
│       ├── TransactionsManagementPage.qml — "Transactions" category's own landing
│       │                         page. File/type name intentionally differs from the
│       │                         sidebar label — see docs/transactions.md.
│       ├── ImportSnapshotPage.qml — Transactions → "Import Transactions" statement
│       │                         import wizard (see docs/statement-import.md).
│       ├── CategoriesManagePage.qml — Transactions → "Categories & Subcategories":
│       │                         add/rename/recolor/remove categories +
│       │                         subcategories (see docs/category-management.md).
│       ├── AccountManagementLandingPage.qml — "Account Management" category's own
│       │                         landing page. Minimal placeholder, same convention
│       │                         as TransactionsManagementPage.qml — see
│       │                         docs/account-management.md.
│       ├── AccountsManagePage.qml — Account Management → "Manage Accounts": add/
│       │                         edit/remove accounts (name/type/category/currency/
│       │                         notes — no iban/bic/number), manage each account's
│       │                         hash links and balance checkpoints, and a
│       │                         "Transactions" button that routes to
│       │                         AccountTransactionsPage.qml (a real page, not a
│       │                         modal) — see docs/account-management.md.
│       ├── AccountTransactionsPage.qml — Transactions → "View & Edit Transactions".
│       │                         Two entry points: its own sidebar action (no preset
│       │                         account) and AccountsManagePage.qml's per-row
│       │                         "Transactions" button (preset account). Full
│       │                         transaction table with colored category/subcategory
│       │                         badges and a "✎ Edit" modal — see
│       │                         docs/account-management.md.
│       ├── AccountTypesCategoriesPage.qml — Account Management → "Account types &
│       │                         categories": add/rename/remove account types,
│       │                         add/rename/recolor/remove account categories — see
│       │                         docs/account-management.md.
│       ├── AccountHistoryPage.qml — Overview → "Account Details": read-only
│       │                         per-account view (current balance, balance-over-time
│       │                         line chart, transaction list) — see
│       │                         docs/account-history.md.
│       └── DatabaseManagementPage.qml — "Database Management" category's own landing
│                                 page (no sidebar actions — the real "Reset
│                                 Database"/"Reset to Mock Data" buttons live
│                                 directly on this page) — see
│                                 docs/database-management.md.
│
└── src/
    ├── main.cpp            — QGuiApplication + QQuickStyle::setStyle("Basic") +
    │                         DB open + QQmlApplicationEngine
    │                         (engine.loadFromModule("FinanceDashboard", "Main")).
    │
    ├── controllers/        — C++ backends exposed to QML via QML_ELEMENT.
    │   │                     One subfolder per page/domain. Each active subfolder
    │   │                     must also be listed in `target_include_directories` in
    │   │                     CMakeLists.txt (the auto-generated qml type-registration
    │   │                     .cpp includes headers by basename).
    │   ├── overview/NetWorthController.h/.cpp    — Overview page (MOCK).
    │   ├── cashflow/CashFlowController.h/.cpp    — Cash Flow page (MOCK; rolling avg + month aggregation in C++).
    │   ├── savings/SavingsController.h/.cpp     — Savings page (MOCK).
    │   ├── investments/InvestmentsController.h/.cpp — Investments page (MOCK).
    │   ├── projects/ProjectsController.h/.cpp    — Projects page (MOCK projects; REAL annuity loan math).
    │   ├── dataentry/SnapshotImportController.h/.cpp — ImportSnapshotPage backend (REAL parsing via
    │   │   src/import/, REAL save — see docs/statement-import.md). Backs Transactions
    │   │   → "Import Transactions".
    │   ├── dataentry/CategoryController.h/.cpp    — Category/subcategory CRUD (`QML_SINGLETON`,
    │   │   SQLite-backed) — see docs/category-management.md.
    │   ├── dataentry/AccountController.h/.cpp     — Account/account-type/account-category CRUD
    │   │   + hash + balance-checkpoint management (`QML_SINGLETON`, SQLite-backed) — see
    │   │   docs/account-management.md.
    │   └── database/DatabaseManagementController.h/.cpp — Backend for
    │       DatabaseManagementPage.qml (`QML_ELEMENT`, **not** a `QML_SINGLETON`) — see
    │       docs/database-management.md.
    │
    ├── import/              — Statement-import template system (REAL logic, UI-independent,
    │   │                     no QML_ELEMENT). See docs/statement-import.md.
    │   ├── ImportTemplate.h         — Abstract interface every template implements.
    │   ├── ImportTemplateRegistry.h/.cpp — Static list of available templates.
    │   └── SumeriaCcpCsvExtractionTemplate.h/.cpp  — First template (Sumeria CCP checking-account CSV export).
    │
    ├── models/
    │   ├── AssetEntry.h/.cpp       — AssetEntry + FinancialSnapshot structs (still compiled,
    │   │                             backs `SnapshotsTable`/`AssetEntriesTable` in `db/` —
    │   │                             see docs/backend.md; this subsystem has no current
    │   │                             UI/controller consumer, see TODO.md).
    │   ├── ImportedStatement.h     — ImportedTransaction + ImportedStatement (header-only,
    │   │                             staging data for the import review screen, no DB yet).
    │   ├── ImportLog.h             — ImportLogEntry (header-only) — parse warnings/errors.
    │   ├── Category.h              — Data::Category + Data::Subcategory (header-only POD,
    │   │                              mirrors the `categories`/`subcategories` table rows).
    │   ├── Account.h               — Data::Account + Data::AccountHash + Data::AccountCategory
    │   │                              + Data::AccountType (header-only POD, mirrors the
    │   │                              accounts-domain table rows — no iban/bic/number, see
    │   │                              docs/account-management.md).
    │   └── Transaction.h           — Data::Transaction (header-only POD, mirrors the
    │                                  `transactions` table rows).
    │
    ├── db/
    │   ├── Database.h/.cpp      — Singleton orchestrator. Owns the QSqlDatabase
    │   │                          connection and every table object. All SQL is
    │   │                          delegated; Database only handles transactions.
    │   └── tables/
    │       ├── SnapshotsTable.h/.cpp        — All SQL for the `snapshots` table.
    │       ├── AssetEntriesTable.h/.cpp     — All SQL for the `asset_entries` table.
    │       ├── CategoriesTable.h/.cpp       — All SQL for the `categories` table.
    │       ├── SubcategoriesTable.h/.cpp    — All SQL for the `subcategories` table.
    │       ├── AccountsTable.h/.cpp         — All SQL for the `accounts` table (no iban/bic/
    │       │                                  number columns).
    │       ├── AccountHashesTable.h/.cpp    — All SQL for the `account_hashes` table.
    │       ├── AccountCategoriesTable.h/.cpp — All SQL for the `account_categories` table.
    │       ├── AccountTypesTable.h/.cpp     — All SQL for the `account_types` table (mirrors
    │       │                                  AccountCategoriesTable, no color column).
    │       ├── TransactionsTable.h/.cpp     — All SQL for the `transactions` table.
    │       └── AccountBalanceCheckpointsTable.h/.cpp — All SQL for the
    │                                           `account_balance_checkpoints` table — see
    │                                           docs/balance-checkpoints.md.
    │
    ├── utils/
    │   ├── Formatter.h        — Header-only display utilities (namespace Fmt):
    │   │                        Fmt::money() (French-locale currency) and
    │   │                        Fmt::dateOrDash() ("dd/MM/yyyy" or "—").
    │   └── AccountHashing.h   — Header-only HMAC-SHA256(IBAN, key=BIC) fingerprint,
    │                            shared by AccountController and
    │                            SnapshotImportController — see docs/statement-import.md.
    │
    └── resources/              — Placeholder. Add .qrc file + icons here when needed.
```

Chart patterns come from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples).
Always check that project and https://doc.qt.io/qt-6/qtgraphs-index.html before adding a new
chart property — never invent one.
