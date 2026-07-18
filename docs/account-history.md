# Account History

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

"Not to manage, but to see history of transactions, amounts, a line graph of the balance over
time, and the current balance." Deliberately separate from `AccountsManagePage.qml`/
`AccountTransactionsPage.qml`: no add/edit/delete/hash/checkpoint actions anywhere on this
page.

- **Sidebar label "Account Details", lives under Overview.** The backing QML file/type is
  `AccountHistoryPage.qml`/`AccountHistoryPage` — only the sidebar label and action id
  (`"accountDetails"`) differ from the file name, same "rename the label, leave the class name
  alone" precedent `SnapshotImportController` set for "Import Transactions". Reachable straight
  from the sidebar rather than via a button on an account row, since it's a standalone view
  over *any* account (has its own account-picker `ComboBox` in the page header). Same overlay
  `Loader` pattern as every other sidebar action (see `docs/navigation.md`) —
  `onCloseRequested: root.currentViewId = "overview"`.
- **No dedicated controller.** `AccountHistoryPage.qml` reads `AccountController` (the
  `QML_SINGLETON`) directly, same as `AccountTransactionsPage.qml` — it only needs
  account-domain data that already belongs there (`accountBalanceHistory()`/
  `accountBalanceSummary()`, plus the existing `transactionsForAccount()`). A new page doesn't
  automatically need a new controller class when it's a read-only view entirely within one
  existing domain.
- **`AccountController::accountBalanceHistory(accountIndex)`** — the "using each transaction to
  compute the balance each day" line chart data. Reads every persisted transaction for the
  account, drops any with no valid `operation_date`, sorts chronologically
  (`Database::transactionsForAccount()` returns most-recent-first for the read-only tables —
  this is the one call site that needs the opposite order), then walks them emitting one point
  per **calendar day that has at least one transaction** (cumulative balance through that day's
  last transaction; days with no activity have no point, so the chart connects straight across
  gaps).
  - **Starting balance:** the latest `AccountBalanceCheckpoint` on or before the first
    transaction's date, if one exists (see `docs/balance-checkpoints.md`). If there's no
    checkpoint at all, the balance *just before* the first transaction is **inferred** from
    that transaction's own `balance_after` minus its own `(credit - debit)`, rather than
    starting the line at a misleading 0.
  - Returns `[]` if the account has no dated transactions (chart shows an `EmptyState`
    instead).
  - Shape: `[{ dateIso, dateText, timeMs, balance, balanceText }, ...]` ascending by date.
    `timeMs` (`QDate::startOfDay().toMSecsSinceEpoch()`) is what the `LineSeries`/
    `DateTimeAxis` actually plot against.
- **`AccountController::accountBalanceSummary(accountIndex)`** — the page-header KPIs. Current
  balance = the last point of `accountBalanceHistory()`; if that's empty, falls back to the
  latest balance checkpoint on or before today, or `0`/`"—"` if the account has neither.
  `{ currentBalanceText, currentBalanceRaw, asOfDateText, transactionCount, hasHistory }`.
- **Chart: same `GraphsView`/`DateTimeAxis`/`ValueAxis`/`LineSeries` recipe as
  `SavingsPage.qml`'s "Balance over time"**, with one difference: the `LineSeries` here is
  populated **imperatively** (`balanceSeries.clear()` then `append()` in a page-level
  `rebuildChart()`) instead of once in `Component.onCompleted`, because the account picker can
  switch which account's history is plotted without the page itself being recreated. `reload()`
  (called on `Component.onCompleted` and whenever `selectedAccountIndex` changes) re-fetches
  `summary`/`history`/`rows` and then calls `rebuildChart()`.
- **`DateTimeAxis` with `min === max` reproducibly crashes the app** (native crash, no QML
  console error) — hit for any account whose transactions all fall on a single calendar day.
  **Fix: always pad a degenerate axis range** — see `AccountHistoryPage.qml`'s `maxTimeMs`
  (pads by one day whenever the raw max wouldn't exceed the raw min). See
  `docs/charting-gotchas.md`.
- **Transaction list is read-only** — same colored category/subcategory badge treatment as
  `AccountTransactionsPage.qml`, but no "✎ Edit" column and no description popup — kept
  intentionally minimal since this page's whole point is "look, don't touch."
