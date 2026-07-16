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
| **Data Entry** | Monthly snapshot form, transaction entry with a manual category/subcategory picker, history, annual summary |
| ↳ **New Monthly Snapshot** (import) | Import a bank statement file (CSV today, more formats later), review/edit/categorize the parsed transactions, check parse warnings/errors in a log popup. Parsing is real; saving is still mock (see `ARCHITECTURE.MD § Statement Import`). |
| ↳ **Manage Categories** | Add, rename, recolor, and remove categories and subcategories used everywhere a transaction gets classified (see `ARCHITECTURE.MD § Category Management`). |

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
│   │                       # ImportSnapshotPage, CategoriesManagePage
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
    │   ├── dataentry/      #   DataEntryController, SnapshotImportController,
    │   │                   #   CategoryController (QML_SINGLETON — category/subcategory CRUD)
    │   └── legacy/         #   Unregistered previous-gen controllers, kept as reference
    ├── import/             # Statement-import template system (ImportTemplate + templates)
    ├── models/             # Data structs (JSON-compatible)
    ├── db/                 # SQLite layer (Database singleton + table classes)
    ├── utils/              # Formatter / FileFinder / JsonReader (header-only)
    ├── data/               # Debug/sample data factory
    ├── MainWindow.* , tabs/  # LEGACY Widgets UI — not compiled, kept as porting reference
    └── ...
```

Chart patterns come from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples).
