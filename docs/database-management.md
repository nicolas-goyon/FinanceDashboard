# Database Management

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

Own sidebar category, no sub-actions — `qml/pages/DatabaseManagementPage.qml` holds the real
controls directly (no landing-page/action split like Transactions or Account Management, since
there's only one small set of actions). Grouped under the sidebar's "ADMIN" divider (see
`docs/navigation.md`). Useful for picking up structural DB changes during development without a
migration script (see `docs/account-management.md`'s schema notes for a concrete example —
dropping iban/bic/number needed exactly this).

- **Two whole-database actions, both behind a confirmation dialog** (irreversible): "Reset
  Database" wipes every table to genuinely empty; "Reset to Mock Data" wipes everything then
  reseeds the same default categories/account-categories/account-types/demo-accounts used on
  first launch, plus a handful of fabricated sample transactions (so the transactions view has
  something to show immediately).
- **Per-account cleanup** — narrower than a full reset. A "Per-account cleanup" `ChartCard` on
  `DatabaseManagementPage.qml` offers, for a picked account:
  - **"Clear transactions"** — `DatabaseManagementController::
    clearTransactionsForAccount(accountId)` → `Database::clearTransactionsForAccount()` →
    `TransactionsTable::removeForAccount()`. Deletes every transaction for that account only;
    the account itself, its hashes, and its balance checkpoints are untouched.
  - **"Delete account"** — calls `AccountController.removeAccount(accountIndex)` directly from
    QML (reuses the existing cascade, `Database::removeAccount()`, which also removes balance
    checkpoints). Same action as the "×" button on `AccountsManagePage.qml`, offered here too
    for convenience.
  Both actions are behind their own `ConfirmDialog`, separate from the two whole-database
  dialogs above.
- **`DatabaseManagementController` (src/controllers/database/DatabaseManagementController.h/.cpp)
  is a plain `QML_ELEMENT`, not a `QML_SINGLETON`** — unlike `CategoryController`/
  `AccountController`, it has no state of its own worth keeping alive across navigation; it
  just triggers an action and reports a transient `statusText`.
- **`Database::resetAll()`** drops and recreates every table this app owns, in one transaction.
  Both controller actions call it first; `resetToMock()` then calls its own private
  `seedMockData()`.
- **Reset (to zero) vs. reload must stay decoupled from reseed-on-empty.** `CategoryController::
  reload()` and `AccountController::reload()` are pure re-reads with no reseed branch — only
  the constructor (first launch) and `DatabaseManagementController::resetToMock()` (explicit
  request) ever seed data. If `reload()` reseeded on an empty DB, "Reset Database" would be
  silently defeated the moment QML called `reload()` right after it to refresh the singletons'
  caches.
- **`resetToMock()` duplicates seed *values*, not seed *code*.** Rather than reaching into
  `CategoryController::seedDefaults()`/`AccountController::seedDefaults()` (both private, and
  both already auto-invoked from their own constructors on first launch), `seedMockData()`
  writes its own copy of the same category/subcategory, account-category, account-type, and
  demo-account values directly through `Database`'s `add*()` methods, then builds ~6 fabricated
  `Data::Transaction` rows (salary credit, rent/groceries/transport/utilities/leisure debits,
  dated relative to `QDate::currentDate()` so they never look stale) against the first demo
  checking account. This keeps `DatabaseManagementController` from needing a back-reference into
  two unrelated singletons just to trigger their private seeding logic — the tradeoff is that
  the seed *values* exist in two places and must be kept in sync by hand (see `TODO.md`).
- **QML, not C++, refreshes the singletons.** Same "no C++-to-C++ singleton coupling" pattern
  as `SnapshotImportController::save()` — `DatabaseManagementController` never touches
  `CategoryController`/`AccountController` directly. `DatabaseManagementPage.qml` listens for
  `databaseReset()` and calls `CategoryController.reload()` / `AccountController.reload()`
  itself.
