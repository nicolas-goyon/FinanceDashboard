# Account Management ("Manage Accounts" / "Account types & categories")

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

Own sidebar category, grouped under the sidebar's "ADMIN" divider (see `docs/navigation.md`)
since it's account/type/category CRUD, not day-to-day analysis. Two sidebar actions: "Manage
Accounts" (`AccountsManagePage.qml`) lets the user add, edit, and remove accounts, manage each
account's stored hash fingerprints and balance checkpoints, and open a full page of an
account's persisted transactions (routes to `AccountTransactionsPage.qml`, which lives in the
Transactions category — see `docs/transactions.md`); "Account types & categories"
(`AccountTypesCategoriesPage.qml`) manages the two classification lists accounts are built
from.

- **No iban/bic/number stored on an account — removed for security.** `Data::Account` used to
  carry `iban`/`bic`/`number` alongside name/type/category/currency/notes; the product decision
  was to stop persisting that data at all. This means (1) the `accounts` table dropped all
  three columns (see schema below), and (2) `AccountController` no longer auto-derives a hash
  fingerprint *at account-creation time*. An account only gains a hash by being linked from the
  import review screen (`AccountController::linkAccountHash()`, source `"auto"`) or added
  manually (`AccountController::addHash()`, source `"manual"`).
- **Two classification fields per account, not one — and both are DB-managed.** `type` is the
  actual banking/financial product (`"CCP"`, `"Livret A"`, `"LDDS"`, `"PEL"`,
  `"Assurance Vie"`, `"PEA"`, `"CTO"`, `"Other"` by default) — a full DB-backed CRUD list
  (`account_types` table, `Data::AccountType`) so new financial products can be added without a
  code change. `category` is the usage bucket (`"Day-to-day"`, `"Savings"`, `"Investments"`,
  `"Urgent savings"` by default) — DB-managed via its own CRUD surface (`account_categories`
  table), same add/rename/recolor/remove pattern `CategoryController` uses, just without
  subcategories and (for `type`) without color at all. Badge colors on
  `AccountsManagePage.qml` are keyed off `category`, not `type`.
- **Currency is a fixed 2-option selector, not free text.** `AccountsManagePage.qml`'s
  add/edit dialog uses a plain `ComboBox` over `[{code:"EUR",label:"EUR €"},
  {code:"USD",label:"USD $"}]` instead of a `TextField` — only these two currencies are needed
  for now.
- **Hashes are their own table, not a column** (`account_hashes`), because an account can carry
  *multiple* fingerprints. `hashCount`/`linked` on each `accounts()` row let QML show a "not
  linked yet" badge without walking the hashes array itself.
- **"Transactions" opens a routed page, not a modal.** The transactions table is significant
  enough content that it shouldn't live in a `Popup`. `AccountsManagePage.qml` emits
  `viewTransactionsRequested(accountIndex)`; `Main.qml` catches it and routes to
  `AccountTransactionsPage`. That page's own `closeRequested()` returns to Manage Accounts, not
  the sidebar view.
- **`AccountController` (src/controllers/dataentry/AccountController.h/.cpp) is a
  `QML_SINGLETON`**, same reasoning as `CategoryController`: accounts are cross-cutting (edited
  on `AccountsManagePage.qml`/`AccountTypesCategoriesPage.qml`, read on
  `ImportSnapshotPage.qml` and `AccountTransactionsPage.qml`).
- **`reload()` instead of a C++-to-C++ back-reference.** `SnapshotImportController::save()`
  writes straight to `Database` rather than calling into the `AccountController` singleton from
  C++. `AccountController::reload()` re-reads accounts, account categories, and account types
  and re-emits the matching `*Changed()` signals; QML calls it itself after a save. Deliberately
  does **not** reseed defaults if the DB comes back empty — same reasoning as
  `CategoryController::reload()` (see `docs/database-management.md`).
- **Data shape** (no separate `QAbstractListModel`):
  ```
  accounts: [{ id, name, type, category, currency, notes,
               hashes: [{ id, hash, source }, ...], hashCount, linked }, ...]
  accountCategories: [{ name, color }, ...]
  accountTypes: [{ name }, ...]
  ```
  Unlike `CategoryController.categories`, each account row exposes its own `id` —
  `SnapshotImportController::save()` and `matchAccountIndex()` need the real id to write/compare
  against, not just a list index.
- **CRUD**: `addAccount`/`updateAccount`/`removeAccount` (index into `accounts()`, no
  iban/bic/number parameters); `addAccountCategory`/`renameAccountCategory`/
  `setAccountCategoryColor`/`removeAccountCategory`; `addAccountType`/`renameAccountType`/
  `removeAccountType` (no color); `addHash`/`linkAccountHash` (source `"manual"` vs `"auto"`)/
  `removeHash`. `accountCategoryColor(name)`/`accountHasHash(accountIndex, hash)` are
  convenience lookups. Every mutating method writes through `Database::instance()` first and
  only updates the in-memory list if that write succeeded.
- **`removeAccount()` cascades hashes, balance checkpoints, and transactions**
  (`Database::removeAccount()`, one transaction) — `AccountsManagePage.qml` shows an explicit
  confirmation dialog before calling it.
- **`accountBalanceHistory(accountIndex)`/`accountBalanceSummary(accountIndex)`** — the
  balance-over-time line chart + current-balance KPI data for `AccountHistoryPage.qml` — see
  `docs/account-history.md`.
- **`transactionsForAccount(accountIndex)`** — `Q_INVOKABLE`, formats every persisted
  `Data::Transaction` for that account with `Fmt::money`/date strings for
  `AccountTransactionsPage.qml`. Also includes the row's real `id` and raw ISO
  dates/amounts alongside the display-formatted text, so the edit dialog can pre-fill without
  re-parsing `Fmt`-formatted strings.
- **Editing a persisted transaction.** `AccountController::updateTransaction(transactionId,
  confirmationDateIso, operationDateIso, name, description, debit, credit, category,
  subcategory)` writes through `Database::updateTransaction()` → `TransactionsTable::
  updateById()` (matched by the row's real `id`, not a list index). Deliberately does **not**
  touch `account_id`, `balance_after`, or `source_template` — moving a transaction to a
  different account isn't supported, and `balance_after` is not recomputed for this or any
  later row when debit/credit changes (see `TODO.md`). This method doesn't touch any
  `NOTIFY`-able property, so `AccountTransactionsPage.qml` re-invokes
  `transactionsForAccount()` explicitly after a successful edit (`reloadRows()`).
- **`AccountTransactionsPage.qml`'s "✎ Edit" dialog uses plain `ComboBox`es for
  category/subcategory, not `ColorComboBox`** — the dialog's `Popup.contentItem` is one shared
  instance reused across every row's edit click, and `ColorComboBox.currentIndex`'s declarative
  binding to `currentName` breaks permanently the first time the user picks an item (fine
  inside a `Repeater` delegate that gets recreated on every model change, not fine in a dialog
  that persists across opens). `currentIndex` is set imperatively in `openFor()`, read back in
  `onSaveClicked`.
- **Seeding**: on an empty DB, `seedDefaults()` writes the 8 default account types, the 4
  default account categories, and 6 demo accounts — without any iban/bic/number, and without
  any hash auto-derivation.

## Account / account-hash / account-category / account-type / transaction DB schema

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
  name              TEXT    NOT NULL DEFAULT '',  -- short label, see below
  description       TEXT    NOT NULL DEFAULT '',
  debit             REAL    NOT NULL DEFAULT 0,
  credit            REAL    NOT NULL DEFAULT 0,
  balance_after     REAL    NOT NULL DEFAULT 0,
  category          TEXT    NOT NULL DEFAULT '',
  subcategory       TEXT    NOT NULL DEFAULT '',
  source_template   TEXT    NOT NULL DEFAULT ''
);
CREATE INDEX idx_transactions_account ON transactions(account_id);
```

- **`accounts` no longer has `iban`/`bic`/`number` columns** — dropped entirely per the
  security-driven product decision above. There is no migration script (see `TODO.md`); the DB
  is still in active development — see `docs/database-management.md`'s "Reset Database" for the
  intended way to pick up a schema change during this phase.
- **`transactions.name`** — a short, per-row label distinct from `description`. Auto-extracted
  at parse time (`extractTransactionName()` in `SumeriaCcpCsvExtractionTemplate.cpp`) and
  user-editable afterward in the import review table. **Added after the table already
  shipped** — because it's additive/non-destructive, `TransactionsTable::createTable()` runs a
  guarded `ALTER TABLE ... ADD COLUMN` (`ensureColumn()`, checked via `PRAGMA table_info`)
  instead of requiring a reset. Any future additive column should follow this pattern rather
  than the reset-only precedent.
- **No `confidence` column here — deliberate, not an oversight.** See
  `docs/statement-import.md`'s design-decision note.
- **No DB-level cascade** — `Database::removeAccount()` does the accounts/hashes/checkpoints/
  transactions cascade manually in one transaction.
- **`transactions` has an update-by-id surface** (`TransactionsTable::updateById()`) — see the
  `updateTransaction()` bullet above for what's editable. `insertBatch()` only appends
  (accumulates across multiple statement imports for the same account); there is still no
  delete-by-id. `TransactionsTable::latestOnOrBefore(accountId, date)` is the fallback "what did
  we last record" reference `SnapshotImportController::checkReconciliation()` uses.
- **`account_categories`/`account_types`/`account_hashes` removal has no "in use" guard** — see
  `TODO.md`.
- **`AccountsTable`/`AccountHashesTable`/`AccountCategoriesTable`/`AccountTypesTable`/
  `TransactionsTable`** (`src/db/tables/`) each own all SQL for their table, same shape as
  `CategoriesTable`/`SubcategoriesTable`. `AccountsTable::all()` leaves `hashes` empty —
  `Database::loadAccounts()` composes it from `AccountHashesTable::allForAccount(id)`.

## `AccountsManagePage.qml`

Same visual language as `CategoriesManagePage.qml` (Popup dialogs, `colorToHex()` helper). One
list section — accounts (one block per account: header row with name/type/category badge/
linked badge/actions, a details row for currency/notes, a hashes row with a chip per stored
hash plus "+ Add hash", and a balance-checkpoints row, see `docs/balance-checkpoints.md`). The
account edit dialog's type and category fields are both plain `ComboBox`es (not
`ColorComboBox`), same "shared dialog instance" reasoning as the transaction edit dialog above.
The currency field is a third plain `ComboBox` over the fixed EUR/USD pair. The "Transactions"
button emits `viewTransactionsRequested(accountIndex)` instead of opening a popup.

## `AccountTransactionsPage.qml`

Routed page (not a `Popup`). Two entry points: its own sidebar action under Transactions (no
preset account — shows an account picker, defaults to the first account) and
`AccountsManagePage.qml`'s per-row "Transactions" button (preset account, no picker needed but
still shown) — see `docs/navigation.md` for the dual-entry-point mechanics. Renders the same
read-only transaction table (date/name/description/debit/credit/balance/category/subcategory)
plus a back arrow (`closeRequested()`). Uses the same wide-NAME/description-popup layout as
`ImportSnapshotPage.qml`'s transaction table — `name` is plain `Text` here, not editable, apart
from the "✎ Edit" modal.

## `AccountTypesCategoriesPage.qml`

Reached from the sidebar (Account Management → "Account types & categories"). Manages both
lists accounts are classified by: account types (plain name CRUD — add/rename/remove via a
chip list + dialogs, no color) and account categories (name+color CRUD — chip list + dialogs +
a `ColorDialog` for recoloring, same pattern `CategoriesManagePage.qml` uses for categories).
