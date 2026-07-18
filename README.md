# Finance Dashboard

A desktop finance analysis dashboard built with **C++17**, **Qt Quick / QML**, and **Qt Graphs**.

> 🚧 **UI pass:** the app now implements the 6 pages from `PROJECT.md` with mock data (marked `[MOCK]` in the UI). Real data wiring is the next phase. See `ARCHITECTURE.MD § App Status`.

---

## Description

Finance Dashboard is a standalone desktop application for monitoring portfolio data, holdings, and analytics. UI is pure QML; all algorithms, analytics, and data access are C++.

| Page | What it shows |
|---|---|
| **Overview** | Net worth KPIs, stacked area by account type, allocation donut, monthly net change bars |
| ↳ **Account Details** | Read-only view of one account: current balance, a line chart of the balance over time (computed by replaying every transaction chronologically), and its transaction history with colored category/subcategory badges. Not a management page — no add/edit/delete here (see `ARCHITECTURE.MD § Account History`). |
| **Cash Flow** | Income vs expenses bars, rolling 3-month average, spending by category (donut + stacked bars), biggest expenses |
| **Savings** | Balance lines per account, progress toward livret ceilings and goals, yearly interest, blended yield |
| **Investments** | Portfolio value vs cumulative deposits (the gap is real P&L), allocation, monthly performance |
| **Projects** | Per-project goal tracking with projection vs target, plus a loan simulator (real annuity math) |
| **Transactions** (renamed from "Transactions Management") | Minimal placeholder landing page pointing at its three sidebar actions below (see `ARCHITECTURE.MD § Transactions`). |
| ↳ **Import Transactions** (renamed from "New Monthly Snapshot") | Import a bank statement file (CSV today, more formats later), review/edit/categorize the parsed transactions, check parse warnings/errors in a log popup, match against a real account by its stored hash fingerprint (with a "not linked yet" badge + inline link action). Parsing and saving are both real — saved transactions land in the `transactions` table. Save briefly disables itself, runs a reconciliation check (declared vs. recorded/computed balances — offers to review kept rows or add an adjustment transaction on mismatch), then shows an import report (income/expense/net, a pie chart + colored legend by category, and the reconciliation results). The statement's own declared start/end balances are also remembered automatically as balance checkpoints. The review table's column widths are drag-resizable and remembered across restarts (see `ARCHITECTURE.MD § Statement Import`). |
| ↳ **Categories & Subcategories** (renamed from "Manage Categories") | Add, rename, recolor, and remove categories and subcategories used everywhere a transaction gets classified. SQLite-backed — the list now persists across restarts (see `ARCHITECTURE.MD § Category Management`). |
| ↳ **View & Edit Transactions** (moved here from Account Management) | Full table of one account's saved transactions, with colored category/subcategory badges and a "✎ Edit" modal per row. Has its own account picker (defaults to the first account) when reached from the sidebar directly; also still reachable pre-selected from Manage Accounts' per-row "Transactions" button (see `ARCHITECTURE.MD § Account Management`). |
| **Account Management** *(ADMIN)* | Minimal placeholder landing page pointing at its two sidebar actions below (see `ARCHITECTURE.MD § Account Management`). |
| ↳ **Manage Accounts** | Add, edit, and remove accounts (name, type, category, currency selector EUR €/USD $, notes — no IBAN/BIC/account number stored, removed for security); manage each account's linked hash fingerprints and balance checkpoints ("the balance was X on this date" — used to validate imports, see `ARCHITECTURE.MD § Balance Checkpoints`); a "Transactions" button opens the Transactions category's "View & Edit Transactions" page, pre-selected to that account. SQLite-backed. |
| ↳ **Account types & categories** | Add/rename/remove account types (CCP, Livret A, LDDS, PEL, Assurance Vie, PEA, CTO, Other, …) and add/rename/recolor/remove account categories (Day-to-day, Savings, Investments, Urgent savings, …). SQLite-backed. |
| **Database Management** *(ADMIN)* | Reset the local database to empty, or wipe and reseed default categories/accounts/types plus a few sample transactions. Also supports per-account cleanup — clear just one account's transactions, or delete an account entirely (cascades hashes/checkpoints/transactions). All actions require confirmation and are irreversible (see `ARCHITECTURE.MD § Database Management`). |

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

## Static Analysis

CLI-based, plain text output. See `ARCHITECTURE.MD § Static Analysis` for
full details/rationale.

```powershell
# Run everything at once, logging each analyzer to its own file (logs/*.log)
./tools/run-static-analysis.ps1

# clang-tidy — on demand (needs LLVM installed, see ARCHITECTURE.MD)
./tools/run-clang-tidy.ps1

# clang-tidy — build-time (opt-in, off by default)
cmake -B cmake-build-debug -DENABLE_CLANG_TIDY=ON

# qmllint — already wired via qt_add_qml_module, no setup needed
cmake --build cmake-build-debug --target FinanceDashboard_qmllint
```

clazy was evaluated but skipped for now — it isn't officially supported on
MinGW (this project's toolchain), only MSVC.

---

## Project Structure

```
FinanceDashboard/
├── CMakeLists.txt          # qt_add_executable + qt_add_qml_module
├── README.md
├── ARCHITECTURE.MD
├── PROJECT.md              # Product spec: target pages, charts, features
├── qml/                    # UI (QML only, no logic)
│   ├── Main.qml            # App shell — sidebar (collapsible categories) + StackLayout,
│   │                       # both driven by one `currentViewId` (see ARCHITECTURE.MD §
│   │                       # Sidebar Navigation)
│   ├── components/         # SidebarCategory.qml, ChartCard.qml, KpiCard.qml,
│   │                       # ColorComboBox.qml, plus the shared page/dialog/button/badge
│   │                       # library (PageHeader, PillButton, GhostButton, StatusBadge,
│   │                       # FormRow, ColorPickerRow, FormDialog, ConfirmDialog,
│   │                       # EmptyState, ColumnHeaderCell) — see ARCHITECTURE.MD §
│   │                       # Shared Components
│   └── pages/              # OverviewPage, CashFlowPage, SavingsPage, InvestmentsPage,
│                           # ProjectsPage, ImportSnapshotPage, CategoriesManagePage,
│                           # TransactionsManagementPage, AccountManagementLandingPage,
│                           # AccountsManagePage, AccountTransactionsPage,
│                           # AccountTypesCategoriesPage, AccountHistoryPage,
│                           # DatabaseManagementPage
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
    │   │                   #   account-category/hash CRUD, SQLite-backed)
    │   └── database/       #   DatabaseManagementController (QML_ELEMENT — reset/reseed
    │                       #   the SQLite file)
    ├── import/             # Statement-import template system (ImportTemplate + templates)
    ├── models/             # Data structs (JSON-compatible where noted; Category.h/
    │                       # Account.h/Transaction.h are not)
    ├── db/                 # SQLite layer (Database singleton + table classes, incl.
    │                       # categories/subcategories, accounts/account_hashes/
    │                       # account_categories/account_types, transactions,
    │                       # account_balance_checkpoints;
    │                       # Database::resetAll() wipes+recreates every table)
    └── utils/              # Formatter (Fmt::money/Fmt::dateOrDash) / AccountHashing (header-only)
```

Chart patterns come from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples).
