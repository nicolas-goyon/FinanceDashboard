# Balance Checkpoints

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

"At this time the amount of money in the bank was that" — full transaction history since an
account was opened usually can't be imported (old paper statements, discontinued export
formats, etc.), so the user can instead record known-good anchor points: "the balance of
account X was Y on date Z". An account can carry several checkpoints over time.

- **Managed on `AccountsManagePage.qml`**, a "Balance checkpoints" row per account (chip list —
  date/balance/note — plus "＋ Add checkpoint"), directly below the "Hashes" row, same
  visual/interaction pattern (`FormDialog` for add, inline "×" for remove). No date-picker
  widget exists in this app (see `TODO.md`), so the date field is plain `"yyyy-MM-dd"` text,
  parsed/validated in `AccountController::addBalanceCheckpoint()` (rejects and shows a status
  message on anything `QDate::fromString(..., Qt::ISODate)` can't parse).
- **`AccountController.balanceCheckpoints(accountIndex)`** is a `Q_INVOKABLE` (reads straight
  from `Database` every call, same as `transactionsForAccount()`), not a `NOTIFY`-able property
  — `AccountsManagePage.qml` re-evaluates it via a page-level `checkpointsTick` counter bumped
  after every add/remove.
- **Two consumers: reconciliation and Account History.** `SnapshotImportController::
  checkReconciliation()`'s check 1 reads `Database::latestCheckpointOnOrBefore(accountId, date)`
  as its preferred "what did we last know" reference (falling back to the latest saved
  transaction's `balance_after`) — see `docs/statement-import.md`.
  `AccountController::accountBalanceHistory()`'s balance-over-time line chart uses the same
  lookup to seed the running balance before the account's first transaction — see
  `docs/account-history.md`.
- **Not just manually added.** `SnapshotImportController::save()` auto-adds one for the
  statement's declared `balanceStart` and one for `balanceEnd` on every successful import (note
  text prefixed `"Auto — statement start/end (…)"`, never overwriting an existing checkpoint on
  that date). The chip list shows both kinds identically (the `note` field is the only visible
  distinction).
- **No DB-level cascade** — `Database::removeAccount()` calls
  `AccountBalanceCheckpointsTable::removeForAccount()` manually, alongside the existing
  hashes/transactions cascade.

## Balance checkpoint DB schema

```sql
CREATE TABLE account_balance_checkpoints (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  account_id INTEGER NOT NULL,
  date       TEXT    NOT NULL,             -- ISO-8601
  balance    REAL    NOT NULL DEFAULT 0,
  note       TEXT    NOT NULL DEFAULT ''
);
CREATE INDEX idx_balance_checkpoints_account ON account_balance_checkpoints(account_id);
```

- **`AccountBalanceCheckpointsTable`** (`src/db/tables/AccountBalanceCheckpointsTable.h/.cpp`)
  owns all SQL for this table — same shape as `AccountHashesTable`: `createTable()`,
  `insert()`/`remove()`/`removeForAccount()` (cascade helper), `allForAccount()` (oldest date
  first), and `latestOnOrBefore(accountId, date)` (most recent checkpoint on or before `date`,
  or `nullopt` — a checkpoint dated *after* `date` is never used to validate an earlier
  statement).
- **`Database`** exposes `loadBalanceCheckpoints()`/`addBalanceCheckpoint()`/
  `removeBalanceCheckpoint()`/`latestCheckpointOnOrBefore()`, and `resetAll()`/`removeAccount()`
  both include this table.
- **`Data::AccountBalanceCheckpoint`** (`models/Account.h`) — `{ id, accountId, date, balance,
  note }`, header-only POD.
