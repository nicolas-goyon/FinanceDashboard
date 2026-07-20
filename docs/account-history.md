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
- **Transaction table has a fixed content width and scrolls horizontally, not the page.** The
  table's 7 columns (`txCol.rowWidth`, currently `1022`) don't shrink — on a narrow window they
  used to render straight past the `ChartCard`'s right edge with nothing to clip or scroll it.
  Fix: the header/row `Rectangle`s use an explicit `Layout.preferredWidth: txCol.rowWidth`
  instead of `Layout.fillWidth: true`, and `txCol` (the `ColumnLayout`) sits inside a
  `ScrollView` on its own — per doc.qt.io's `ScrollView` "Sizing" section, a single-child
  `ScrollView` derives its content size from that child's implicit size, so it only shows a
  horizontal scrollbar when the card is actually narrower than the table.
- **Balance chart controls, grouped in one `chartControls` `ColumnLayout`** anchored above the
  `GraphsView` (direct anchored child of `ChartCard`'s content `Item`, same convention the
  chart/`EmptyState` already used — not a fill-parent Layout, since its own height changes when
  the custom-range row shows/hides and the `GraphsView` below anchors to whatever height results):
  - **Time range (`page.chartRange`)** — `PillButton`s for `7D`/`1M`/`6M`/`1Y`/`From start`/
    `Custom`, rolling (not calendar-aligned) and anchored to the account's own **last**
    transaction date (`maxTimeMs`), not "today" — matters for accounts whose history doesn't
    reach the present. Only changes the `DateTimeAxis` window
    (`axisRangeMinMs`/`axisRangeMaxMs`) that's shown — `balanceSeries` always holds every point
    (see `rebuildChart()`); this never filters `page.history` itself. Same degenerate-range pad
    as `maxTimeMs` (below) applies to `axisRangeMaxMs` too. `Custom` reveals two plain
    `"yyyy-MM-dd"` `TextField`s (`page.customStartIso`/`customEndIso`) — **not** a date-picker
    widget; same convention as the balance-checkpoint date field on `AccountsManagePage.qml`
    ("no date-picker widget in this app yet").
  - **Y-axis scale (`page.axisStartAtZero`)** — `PillButton`s for "Auto" (the original tight-fit
    `balanceAxisMin`/`Max`, padded ±10%, capped at 0) vs. "From 0" (pins `axisY.min` to `0` so
    growth reads against a fixed baseline). Only the `GraphsView`'s `axisY.min` binding switches
    on it; `balanceAxisMin` itself is untouched.
- **Two "2-level" pie charts — "Income by category" / "Expenses by category".** Income/expense
  is derived **per-transaction from credit/debit**, not a tag on `Category`/`CategoryController`
  — a transaction with `credit > 0` contributes to the income pie, one with `debit > 0`
  contributes to the expense pie (both, for the rare row with both) — so the same category can
  legitimately appear on both pies. Backed by
  **`AccountController::accountCategoryBreakdown(accountIndex)`** (mirrors
  `SnapshotImportController::buildImportReport()`'s `byCategory` aggregation, but split by
  direction and by *magnitude* rather than net, since a pie slice can't be negative):
  `{ income: [{ category, totalRaw, totalText, subcategories: [{ subcategory, totalRaw,
  totalText }, ...] }, ...], expenses: [ ...same shape... ] }`. Each pie is **two `PieSeries`,
  each in its own `GraphsView`, stacked via `anchors.fill`** inside a plain `Item` host
  (`incomePieHost`/`expensePieHost`) — a category ring (`pieSize: 0.55`/`holeSize: 0.28`, inner
  `GraphsView`) and a subcategory ring (`pieSize: 0.92`/`holeSize: 0.58`, outer `GraphsView`,
  `angleSpanVisibleLimit: 10` to declutter labels on thin slices). Deliberately *not* Qt Graphs'
  native `PieSlice` sub-slice feature (nothing in Qt's docs specifies which ring a sub-slice
  renders as relative to its parent — turned out to be the *opposite* of what was needed, see
  `docs/charting-gotchas.md`), and deliberately *not* two `PieSeries` sharing one `GraphsView`
  either — that shared-view version had both rings' `onClicked` stop firing entirely, so each
  ring gets its own fully isolated `GraphsView` instead, matching the only single-series-per-view
  configuration Qt Graphs actually demonstrates and every other pie in this app already uses.
  Both rings' subcategory (or category) slices need `labelVisible = true` set explicitly per
  slice in `buildCategoryRings()` — it doesn't inherit from the other ring. Built imperatively in
  `buildCategoryRings()`/`rebuildBreakdownPies()`, called from the page-level `reload()` — same
  "page-level, not each series' own `Component.onCompleted`" convention as `rebuildChart()`.
  Colors resolved in QML via `CategoryController.categoryColor()`/`subcategoryColor()`, same
  lookups the transaction badges use. `PieSeries` has no hover at all (confirmed against Qt's
  C++ class docs, not just the QML ones — see `docs/charting-gotchas.md`) — clicking a slice on
  either ring updates a small text line below the chart (prefixed "Category —"/"Subcategory —"
  so it's clear which ring was clicked), same click-to-inspect pattern as
  `ImportSnapshotPage.qml`'s category pie.
- **Chart theme is explicitly overridden on all five `GraphsView`s on this page** (the balance
  line chart, plus the inner/outer ring `GraphsView`s of both pies) rather than left at
  `GraphsTheme`'s `ColorScheme.Dark` defaults —
  the defaults read as too low-contrast against this app's near-black Catppuccin Mocha cards.
  Uses only palette tokens already listed in `docs/styling.md`: `plotAreaBackgroundColor:
  "#181825"` (Mantle — a visibly distinct, slightly lighter "recessed" panel against the card's
  own Surface 0 `"#1e1e2e"`), `grid`/axis line colors `"#313244"` (Surface 1 — "borders,
  dividers", the same semantic use as everywhere else), axis/grid label text `"#6c7086"`
  (Overlay 0 — "muted labels"), pie slice label text `"#cdd6f4"` (Text — primary, since slice
  labels are the pie's main informational content, not a muted axis tick), and
  `backgroundVisible: false` so the `GraphsView`'s own background doesn't add a second
  clashing layer on top of `ChartCard`'s.
