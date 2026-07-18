# App Status — PROJECT.md pages

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.
> This table is hand-maintained today — see `docs/automated-docs.md` for the plan to
> reduce how much of it needs manual upkeep.

**Design rule: UI in QML, algorithms/analytics/data in C++.** Each page gets a C++
*controller* class (`QML_ELEMENT`, in `src/controllers/`) holding all logic; the `.qml` file
only renders. See `docs/backend.md` for the full pattern.

The app is built around the 6 pages from **`PROJECT.md`**. Current state: the 5 analytics pages
are a **UI-only pass** — their controllers serve MOCK data (marked `[MOCK]` in the UI); DB
wiring is tracked in `TODO.md`. The former 6th page ("Data Entry") was split into three lighter
categories (Transactions, Account Management, Database Management) once the product asked for
account management, hash management, and a schema-reset tool to be reachable independently.

| Page | Controller | Charts / widgets | Data |
|---|---|---|---|
| Overview (Net Worth) | `NetWorthController` | 4 KPI cards · stacked `AreaSeries` by account type · allocation donut (`PieSeries`) · net-change bars (green/red `BarSet`s) | MOCK |
| ↳ Account Details | *(none — reads `AccountController`)* | balance-over-time `LineSeries`, KPIs, transaction list | **REAL** — see `docs/account-history.md` |
| Cash Flow | `CashFlowController` | income/expenses grouped bars · rolling 3-month `LineSeries` · category donut w/ month `ComboBox` · stacked category bars · biggest-expenses table | MOCK |
| Savings | `SavingsController` | KPI cards · balance `LineSeries` per account · ceiling/goal progress bars (pure QML) · yearly interest grouped bars | MOCK |
| Investments | `InvestmentsController` | KPI cards · portfolio vs cumulative-deposits lines (gap = real P&L) · allocation donut · monthly perf % bars | MOCK |
| Projects | `ProjectsController` | project `ComboBox` · KPI cards · projection-vs-target lines · **loan simulator (REAL annuity math in C++)** · save-vs-borrow comparison | MOCK projects, real loan math |
| Transactions | *(none — minimal placeholder)* | header text pointing at this category's three sidebar actions | Placeholder only — see `docs/transactions.md` |
| ↳ Import Transactions | `SnapshotImportController` | template + file picker · statement header info · editable/categorizable transaction table · account auto-match (against stored hashes) · "not linked" badge + inline link action · parse log popup | **REAL parsing, REAL save** — see `docs/statement-import.md` |
| ↳ Categories & Subcategories | `CategoryController` (`QML_SINGLETON`) | add/rename/recolor/remove categories and subcategories | **REAL** — see `docs/category-management.md` |
| ↳ View & Edit Transactions | `AccountController` (`QML_SINGLETON`) | account picker · full transaction table with colored category/subcategory badges · "✎ Edit" modal | **REAL** — see `docs/account-management.md` |
| Account Management | *(none — minimal placeholder)* | header text pointing at this category's two sidebar actions | Placeholder only |
| ↳ Manage Accounts | `AccountController` (`QML_SINGLETON`) | add/edit/remove accounts · currency selector (EUR/USD) · manage per-account hash links and balance checkpoints · "Transactions" button | **REAL** — see `docs/account-management.md` |
| ↳ Account types & categories | `AccountController` (`QML_SINGLETON`) | add/rename/remove account types · add/rename/recolor/remove account categories | **REAL** — see `docs/account-management.md` |
| Database Management | `DatabaseManagementController` | "Reset Database" / "Reset to Mock Data" / per-account cleanup, all behind confirmation dialogs | **REAL** — see `docs/database-management.md` |

## Deleted previous-generation code

The original Qt Widgets app (`src/MainWindow.*`, `src/tabs/*`) and an intermediate,
ported-but-never-wired QML generation (`qml/tabs/*`, `EntryController`, `DatabaseController`,
`OverviewController`, `PortfolioController`, `ImportController`, the superseded
`DataEntryController.h/.cpp` / `DataEntryPage.qml`) were deleted outright once nothing on disk
referenced them — recoverable from git history before that cleanup pass if the old Widgets
version is ever needed again.

The `db/`, `models/`, `utils/` layers are still compiled and `main.cpp` still opens the SQLite
DB. **Categories/subcategories, accounts/account-hashes/account-types/account-categories,
transactions, and balance checkpoints are wired to the real DB**; the 5 analytics-page
controllers (Overview/Cash Flow/Savings/Investments/Projects) are still MOCK — see `TODO.md`.
