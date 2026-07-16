# Finance Dashboard

A desktop finance analysis dashboard built with **C++17**, **Qt Quick / QML**, and **Qt Graphs**.

> 🚧 **UI pass:** the app now implements the 6 pages from `PROJECT.md` with mock data (marked `[MOCK]` in the UI). Real data wiring is the next phase. See `ARCHITECTURE.MD § App Status`.

---

## Description

Finance Dashboard is a standalone desktop application for monitoring portfolio data, holdings, and analytics. UI is pure QML; all algorithms, analytics, and data access are C++.

| Page | What it shows |
|---|---|
| **Overview** | Net worth KPIs, stacked area by account type, allocation donut, monthly net change bars |
| **Cash Flow** | Income vs expenses bars, rolling 3-month average, spending by category (donut + stacked bars), biggest expenses |
| **Savings** | Balance lines per account, progress toward livret ceilings and goals, yearly interest, blended yield |
| **Investments** | Portfolio value vs cumulative deposits (the gap is real P&L), allocation, monthly performance |
| **Projects** | Per-project goal tracking with projection vs target, plus a loan simulator (real annuity math) |
| **Transactions Management** | Minimal placeholder landing page pointing at its two sidebar actions below (see `ARCHITECTURE.MD § Transactions Management`). |
| ↳ **Import Transactions** (renamed from "New Monthly Snapshot") | Import a bank statement file (CSV today, more formats later), review/edit/categorize the parsed transactions, check parse warnings/errors in a log popup, match against a real account by its stored hash fingerprint (with a "not linked yet" badge + inline link action). Parsing and saving are both real — saved transactions land in the `transactions` table (see `ARCHITECTURE.MD § Statement Import`). |
| ↳ **Manage Categories** | Add, rename, recolor, and remove categories and subcategories used everywhere a transaction gets classified. SQLite-backed — the list now persists across restarts (see `ARCHITECTURE.MD § Category Management`). |
| **Account Management** | Minimal placeholder landing page pointing at its two sidebar actions below (see `ARCHITECTURE.MD § Account Management`). |
| ↳ **Manage Accounts** | Add, edit, and remove accounts (name, type, category, currency selector EUR €/USD $, notes — no IBAN/BIC/account number stored, removed for security); manage each account's linked hash fingerprints; a "Transactions" button opens a full page of an account's saved transactions. SQLite-backed. |
| ↳ **Account types & categories** | Add/rename/remove account types (CCP, Livret A, LDDS, PEL, Assurance Vie, PEA, CTO, Other, …) and add/rename/recolor/remove account categories (Day-to-day, Savings, Investments, Urgent savings, …). SQLite-backed. |
| **Database Management** | Reset the local database to empty, or wipe and reseed default categories/accounts/types plus a few sample transactions — useful when the schema changes during development. Both actions require confirmation and are irreversible (see `ARCHITECTURE.MD § Database Management`). |

---

## Prerequisites

| Tool | Version |
|---|---|
| Qt | 6.8+ (developed against **6.11**) — include the **Qt Graphs** and **Qt Quick** modules |
| CMake | 3.20+ |
| C++ compiler | MSVC 2022 (recommended on Windows) or MinGW 13+ |

Qt Charts and Qt Widgets are no longer required.

---

## Building & Running

### 1. Configure

```powershell
# From the FinanceDashboard directory
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
```

### 2. Build

```powershell
cmake --build build --config Release
```

### 3. Deploy Qt DLLs (first run only)

```powershell
cd build/Release
windeployqt.exe --qmldir ../../qml FinanceDashboard.exe
```

The `--qmldir` flag is required so the QML and Qt Graphs runtime files are deployed.

### 4. Run

```powershell
.\build\Release\FinanceDashboard.exe
```

Or open the project in **Qt Creator** (`File → Open File or Project → CMakeLists.txt`) and press **Run**.

---

## Project Structure

```
FinanceDashboard/
├── CMakeLists.txt          # qt_add_executable + qt_add_qml_module
├── README.md
├── ARCHITECTURE.MD
├── PROJECT.md              # Product spec: target pages, charts, features
├── qml/                    # UI (QML only, no logic)
│   ├── Main.qml            # App shell — sidebar (collapsible categories) + StackLayout
│   ├── components/         # SidebarCategory.qml, ChartCard.qml, KpiCard.qml, ...
│   ├── pages/              # OverviewPage, CashFlowPage, ..., OtherLegacyPage,
│   │                       # ImportSnapshotPage, CategoriesManagePage,
│   │                       # TransactionsManagementPage, AccountManagementLandingPage,
│   │                       # AccountsManagePage, AccountTransactionsPage,
│   │                       # AccountTypesCategoriesPage, DatabaseManagementPage
│   └── tabs/               # One .qml per tab (+ EntryCategory.qml component)
└── src/
    ├── main.cpp            # QGuiApplication + QQuickStyle::setStyle("Basic") + QQmlApplicationEngine
    ├── controllers/        # C++ backends exposed to QML (QML_ELEMENT), one subfolder
    │   │                   # per page/domain:
    │   ├── overview/       #   NetWorthController
    │   ├── cashflow/       #   CashFlowController
    │   ├── savings/        #   SavingsController
    │   ├── investments/    #   InvestmentsController
    │   ├── projects/       #   ProjectsController
    │   ├── dataentry/      #   SnapshotImportController, CategoryController
    │   │                   #   (QML_SINGLETON — category/subcategory CRUD, SQLite-backed),
    │   │                   #   AccountController (QML_SINGLETON — account/account-type/
    │   │                   #   account-category/hash CRUD, SQLite-backed). Also holds the
    │   │                   #   unregistered, superseded DataEntryController.h/.cpp.
    │   ├── database/       #   DatabaseManagementController (QML_ELEMENT — reset/reseed
    │   │                   #   the SQLite file)
    │   └── legacy/         #   Unregistered previous-gen controllers, kept as reference
    ├── import/             # Statement-import template system (ImportTemplate + templates)
    ├── models/             # Data structs (JSON-compatible where noted; Category.h/
    │                       # Account.h/Transaction.h are not)
    ├── db/                 # SQLite layer (Database singleton + table classes, incl.
    │                       # categories/subcategories, accounts/account_hashes/
    │                       # account_categories/account_types, transactions;
    │                       # Database::resetAll() wipes+recreates every table)
    ├── utils/              # Formatter / FileFinder / JsonReader / AccountHashing (header-only)
    ├── data/               # Debug/sample data factory
    ├── MainWindow.* , tabs/  # LEGACY Widgets UI — not compiled, kept as porting reference
    └── ...
```

Chart patterns come from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples).
