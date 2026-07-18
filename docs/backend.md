# QML/C++ Boundary & Backend Component Responsibilities

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

## The pattern

Every active controller (see `docs/app-status.md` and `docs/directory-map.md`'s `controllers/`
listing) follows the `*DataProvider` pattern from
[QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples):

1. Controller class in `src/controllers/<domain>/`, `Q_OBJECT` + `QML_ELEMENT` (`QML_SINGLETON`
   too for `CategoryController`/`AccountController` — see `docs/category-management.md` /
   `docs/account-management.md` for why).
2. Registered by listing the `.h/.cpp` under `SOURCES` of `qt_add_qml_module` in
   `CMakeLists.txt`. **The controller's directory must also be on
   `target_include_directories`** — the auto-generated type-registration `.cpp` includes
   headers by basename.
3. Instantiated *in QML* (`NetWorthController { id: controller }`), not via context properties.
4. Data crosses the boundary as `Q_PROPERTY` (scalars, with `NOTIFY`) and `Q_INVOKABLE
   QVariantList` (arrays).
5. QML never computes: sums, cumulative layers, axis ranges, diffs all come pre-computed from
   C++. (See `TODO.md` for the handful of spots that currently deviate from this.)

If you need the history of how a specific page/table was first ported to QML from the original
Qt Widgets app, check git log from before this doc pass — the previous-generation controllers
(`AnalyticsController`, `EntryController`, the legacy `DatabaseController`, `OverviewController`,
`PortfolioController`, the legacy `ImportController`) were deleted once nothing referenced them
and aren't on disk anymore.

## `db/Database.h/.cpp`

- Meyers singleton — `Database::instance()`.
- Owns the `QSqlDatabase` connection (`"finance_dashboard"` named connection).
- Owns one table object per table (`SnapshotsTable`, `AssetEntriesTable`, `CategoriesTable`,
  `SubcategoriesTable`, `AccountsTable`, `AccountHashesTable`, `AccountCategoriesTable`,
  `AccountTypesTable`, `TransactionsTable`, `AccountBalanceCheckpointsTable`).
- `open()` enables WAL mode then delegates `createTable()` to each table object.
- `saveSnapshot()`, `removeCategory()`, and `removeAccount()` are the only methods that touch
  multiple tables (cross-table writes/cascades).
- `loadCategories()`, `addCategory()`/`renameCategory()`/`setCategoryColor()`/
  `removeCategory()` and the subcategory equivalents — see `docs/category-management.md`.
- `loadAccounts()`, `addAccount()`/`updateAccount()`/`removeAccount()`,
  `addAccountHash()`/`removeAccountHash()`/`accountHasHash()`, `loadAccountCategories()`/
  `addAccountCategory()`/`renameAccountCategory()`/`setAccountCategoryColor()`/
  `removeAccountCategory()`, `loadAccountTypes()`/`addAccountType()`/`renameAccountType()`/
  `removeAccountType()`, `insertTransactions()`/`transactionsForAccount()`/
  `transactionCountForAccount()`/`updateTransaction()`/`clearTransactionsForAccount()`/
  `latestTransactionOnOrBefore()` — see `docs/account-management.md`.
  `loadBalanceCheckpoints()`/`addBalanceCheckpoint()`/`removeBalanceCheckpoint()`/
  `latestCheckpointOnOrBefore()` — see `docs/balance-checkpoints.md`.
- `resetAll()` — drops and recreates every table this app owns, in one transaction. Used
  exclusively by `DatabaseManagementController` — see `docs/database-management.md`.
  Deliberately does **not** reseed anything itself.
- **DB file location:** `%APPDATA%\DataAnalysis\FinanceDashboard\finance_dashboard.db`
  (Windows).

## `db/tables/SnapshotsTable.h/.cpp`

- `upsert(date)` — INSERT … ON CONFLICT DO NOTHING (idempotent).
- `idForDate(date)` → `std::optional<int>`; `latest()` → `std::optional<QPair<int, QDate>>`.
- `allDates()` → `QVector<QDate>` descending.

## `db/tables/AssetEntriesTable.h/.cpp`

- `createTable()` also creates `idx_entries_snapshot` index.
- `deleteForSnapshot(id)` + `insertBatch(id, category, entries)` are always called together by
  `Database::saveSnapshot()`.
- `loadForSnapshot(id, date)` rebuilds a `FinancialSnapshot`.
- No current controller reads or writes this table or `SnapshotsTable` — see `TODO.md`.

## `db/tables/CategoriesTable.h/.cpp` / `db/tables/SubcategoriesTable.h/.cpp`

- See `docs/category-management.md` for the full schema and CRUD surface.
- `insert()` returns the new row id (`int`), or `-1` on any failure (duplicate name via the
  `UNIQUE` constraint included) — callers (`CategoryController`) check for duplicates
  themselves first and treat `-1` as "write failed, don't touch the in-memory list".
- `all()` / `allForCategory(categoryId)` order by row id ascending (insertion order).

## `models/AssetEntry.h/.cpp`

- `Data::AssetEntry` — `{name, value}`; `Data::FinancialSnapshot` — `{date, stocks[], coins[],
  metals[], etfs[]}`. Both with `toJson()` / `fromJson()`.
- Fields are private with getter/setter accessors rather than public members —
  `cppcoreguidelines-non-private-member-variables-in-classes` flags public data on a type with
  behavior (unlike the plain PODs below, which have no member functions and so aren't
  flagged). Call sites: `AssetEntry.cpp`, `AssetEntriesTable.cpp`, `Database.cpp`.

## `models/Category.h`

- Header-only POD: `Data::Subcategory` — `{id, name, color}`; `Data::Category` — `{id, name,
  color, subcategories[]}`. `id` is the SQLite row id (`-1` for a not-yet-persisted instance).
  No JSON (de)serialization — unlike `AssetEntry`, nothing round-trips these through a file.

## `models/Account.h` / `models/Transaction.h`

- `Data::AccountHash` — `{id, hash, source}`; `Data::Account` — `{id, name, type, category,
  currency, notes, hashes[]}` (no iban/bic/number — see `docs/account-management.md`);
  `Data::AccountCategory` — `{id, name, color}`; `Data::AccountType` — `{id, name}`. Same
  header-only POD convention as `Category.h`.
- `Data::Transaction` — `{id, accountId, confirmationDate, operationDate, name, description,
  debit, credit, balanceAfter, category, subcategory, sourceTemplate}`. Distinct from
  `Data::ImportedTransaction` (`models/ImportedStatement.h`), which is the pre-save staging row
  on the import review screen. **No `confidence` field** — see `docs/statement-import.md`.

## `db/tables/AccountsTable.h/.cpp` / `AccountHashesTable.h/.cpp` / `AccountCategoriesTable.h/.cpp` / `AccountTypesTable.h/.cpp` / `TransactionsTable.h/.cpp`

- See `docs/account-management.md` for the full schema and CRUD surface — same shape/
  conventions as `CategoriesTable`/`SubcategoriesTable`. `AccountTypesTable` mirrors
  `AccountCategoriesTable` minus the color column.
- `TransactionsTable::insertBatch()` only appends (no delete-then-insert like
  `AssetEntriesTable`) since multiple statement imports for the same account should
  accumulate, not overwrite.

## `utils/`

- `Formatter.h` — `Fmt::money(double, bool showPlus)` French-locale currency;
  `Fmt::dateOrDash(const QDate&)` display date formatting; `Fmt::gCurrency` (EUR/USD). Not yet
  reachable from QML — see `TODO.md`.
- `AccountHashing.h` — HMAC-SHA256(IBAN, key=BIC) fingerprint, see `docs/statement-import.md`.

**Fully orphaned code deleted in an earlier cleanup pass** (zero references anywhere, not even
from `Database`): `models/PortfolioData.h/.cpp`, `utils/JsonReader.h`, `utils/FileFinder.h`,
`data/DebugData.h/.cpp` — leftover support code for the deleted legacy generation, unlike
`AssetEntry.h`/`SnapshotsTable`/`AssetEntriesTable` (see `models/AssetEntry.h` above), which are
still compiled and wired into `Database`, just without a current controller reading them.
