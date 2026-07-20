# Version History — Finance Dashboard

This file is the single top-level changelog for the app. It tracks the current version and
every previous one, in the same style as a standard `CHANGELOG.md` (newest first). It is meant
to answer "what changed and when," at a glance — for *why* something is built the way it is,
see `ARCHITECTURE.MD` and `docs/`.

**Format:** each version gets a heading, a one-line summary, and `Added` / `Changed` / `Fixed`
/ `Docs` subsections (omit any that don't apply). Version numbers follow the same value as
`CMakeLists.txt`'s `project(FinanceDashboard VERSION x.y.z ...)`.

**Maintenance rule** (see the project instructions): every reasonable change gets logged here
under the current in-progress version, at the time it's made — not reconstructed after the
fact. Small fixes to a feature that's still unreleased/in-progress in the *same* version don't
need their own extra line; fold them into the feature's own entry instead of listing "fixed a
bug in the thing I just added two lines above."

---

## v0.2.1 (current)

Started as a documentation/project-hygiene pass; picked up an Account Details feature-and-fix
pass in the same in-progress version.

### Added
- `TODO.md` — standing backlog of known limitations, bugs, and cleanups found during a full
  file-by-file review, split out of what used to be `ARCHITECTURE.MD`'s "Known Limitations /
  TODOs" section.
- `version.md` (this file) — top-level changelog, replacing the ad-hoc "Feature pass —
  ..." narrative headers that used to accumulate at the top of `ARCHITECTURE.MD`.
- `docs/` folder — `ARCHITECTURE.MD` split into focused per-topic files (see `ARCHITECTURE.MD`
  for the index). Includes a new `docs/automated-docs.md` describing the doc-generation script.
- `tools/generate_docs.py` — generates `docs/generated/source-tree.md` (a file/module listing
  with descriptions pulled from `@brief`-style header comments) so that listing no longer has
  to be hand-maintained. See `docs/automated-docs.md`.
- **Account Details — balance chart time-range + Y-axis controls.** Time range presets (`7D` /
  `1M` / `6M` / `1Y` / `From start` / `Custom`, rolling from the account's own last transaction
  date, `Custom` via plain `yyyy-MM-dd` fields) plus a Y-axis scale toggle: "Auto" (existing
  tight-fit behavior) vs. "From 0" (pins the axis minimum to 0). See `docs/account-history.md`.
- **Account Details — "Income by category" / "Expenses by category" pies.** Two new 2-level
  nested pie charts — category ring (inner) + subcategory ring (outer), each ring its own
  `PieSeries` in its own `GraphsView` stacked via `anchors.fill` (not Qt Graphs' native
  `PieSlice` sub-slice feature — turned out to render the opposite ring order needed; and not
  two `PieSeries` sharing one `GraphsView` either — that made `onClicked` stop firing on both
  rings). Both rings' slices show labels and are independently clickable (shows "Category —"/
  "Subcategory —" + amount + percentage). Income/expense is derived per-transaction from
  credit/debit, not a tag on `Category`. Backed by the new
  `AccountController::accountCategoryBreakdown(accountIndex)`. See `docs/account-history.md` and
  `docs/charting-gotchas.md`.

### Changed
- `README.md` rewritten to be a short, professional front door (what the app is, prerequisites,
  build/run steps) instead of an exhaustive page-by-page feature list — the exhaustive material
  moved into `docs/`.
- `ARCHITECTURE.MD` trimmed to an index/overview page that links into `docs/*.md` instead of
  holding every design-decision narrative inline. Added a "Charts should have hover (or click)
  feedback" guideline.
- **Account Details — transaction history table is now horizontally scrollable** instead of
  overflowing past the card's border on a narrow window: the header/row `Rectangle`s use a fixed
  `Layout.preferredWidth` and the table sits inside a `ScrollView`, which only shows a horizontal
  scrollbar when the card is narrower than the table. See `docs/account-history.md`.
- **Account Details — all five `GraphsView`s (balance chart + both pies' inner/outer rings) use
  an explicit, higher-contrast `GraphsTheme`** instead of `ColorScheme.Dark` defaults, which read
  as too washed-out against this app's near-black cards — same Catppuccin Mocha tokens already
  documented in `docs/styling.md`. See `docs/account-history.md`.
- **`HoverPoint.qml`'s hover hit-box enlarged (26×26 → 34×34), and its tooltip now flips
  below/clamps horizontally instead of always centering above the point** — the small hit-box
  made hovering hard to land, and always-above-centered positioning cropped the tooltip for any
  point near the chart's top edge or a side. Both are shared-component fixes, so they apply to
  every page's line-chart hover (Account Details, Savings, Cash Flow, Investments, Overview,
  Projects). See `docs/charting-gotchas.md`.

### Fixed
- **Account Details — the 3 KPI cards (Current balance / Transactions / Type-Category) are now
  exactly equal width and height.** Width: `Layout.fillWidth: true` alone left the middle card a
  hair narrower than its neighbors (QtQuick Layouts' leftover-pixel remainder handling wasn't
  symmetric); all three now bind an explicit, identical `Layout.preferredWidth`. Height: the
  Transactions card's empty `sub` line made QtQuick Layouts exclude it from the card's
  implicit-height calculation (a hair shorter than its two neighbors, which both have a real sub
  line) — now shows "All time" instead of an empty string.

### Docs
- See `Added`/`Changed`/`Fixed` above.

---

## v0.2.0 — Baseline

Starting point for this changelog. Everything below was already built before `version.md`
existed; recorded here as one baseline entry rather than reconstructed commit-by-commit. Matches
`CMakeLists.txt`'s `project(... VERSION 0.2.0 ...)` at the time this file was introduced.

### Added
**Navigation & shell**
- `Main.qml` app shell: collapsible left sidebar (`SidebarCategory.qml`) + `StackLayout`, driven
  by a single `currentViewId` source of truth. "ADMIN" section divider above Account Management
  / Database Management.
- Shared QML component library: `PageHeader`, `PillButton`, `GhostButton`, `StatusBadge`,
  `FormRow`, `ColorPickerRow`, `FormDialog`, `ConfirmDialog`, `EmptyState`, `ColumnHeaderCell`,
  `KpiCard`, `ChartCard`, `SidebarCategory`, `ColorComboBox`.

**Analytics pages (UI pass, mock data)**
- Overview (net worth KPIs, stacked area by account type, allocation donut, monthly net-change
  bars), Cash Flow (income/expense bars, rolling average, category donut/stacked bars, biggest
  expenses), Savings (balance lines, ceiling/goal progress, yearly interest), Investments
  (portfolio vs. deposits, allocation, monthly performance), Projects (goal tracking + a real
  C++ annuity-math loan simulator). All five still serve `[MOCK]` data pending DB wiring.

**Transactions**
- Statement import wizard (`ImportSnapshotPage.qml` / `SnapshotImportController`): real CSV
  parsing (`SumeriaCcpCsvExtractionTemplate`), editable/categorizable review table with
  resizable + persisted columns, account auto-match by stored hash fingerprint, parse log
  popup, real save to the `transactions` table.
- Import report + reconciliation: post-save report (income/expense/net, category/subcategory
  breakdown with a pie chart), two-check balance reconciliation against declared statement
  balances, a mismatch-resolution dialog (review kept rows / add an adjustment transaction).
- Auto-remembered balance checkpoints from every statement's declared start/end balance.
- Categories & Subcategories management (add/rename/recolor/remove, SQLite-backed).
- View & Edit Transactions: full per-account transaction table with colored category/
  subcategory badges and an edit modal.

**Accounts & admin**
- Manage Accounts: add/edit/remove accounts (name/type/category/currency/notes — no
  IBAN/BIC/account number stored), per-account hash-fingerprint management, per-account
  balance-checkpoint management, a routed per-account transactions view.
- Account types & categories management (SQLite-backed CRUD).
- Account History: read-only per-account view (current balance, balance-over-time line chart
  computed by replaying transactions, transaction list).
- Database Management: whole-database reset (empty or reseeded with mock data) and per-account
  cleanup (clear transactions / delete account), all behind confirmation dialogs.

**Data layer**
- SQLite persistence (`Database` singleton + one table class per table) for categories,
  subcategories, accounts, account hashes, account categories, account types, transactions, and
  balance checkpoints.
- HMAC-SHA256 account-hash fingerprinting (`AccountHashing.h`), shared between import matching
  and manual account linking.

**Tooling**
- clang-tidy (build-time opt-in + on-demand script) and qmllint (auto-wired via
  `qt_add_qml_module`) static analysis, plus `tools/run-static-analysis.ps1` to run both and log
  to `logs/`.

### Removed
- The original Qt Widgets app and the intermediate ported-but-unwired QML generation
  (`AnalyticsController`, `EntryController`, the legacy `DatabaseController`,
  `OverviewController`, `PortfolioController`, the legacy `ImportController`, `qml/tabs/*`) —
  deleted outright once nothing referenced them; recoverable from git history before this
  baseline if ever needed again.
- Keyword-based category auto-suggestion (`CategoryKeywords.h`) — replaced by the user-managed
  category list.
- `iban`/`bic`/`number` columns on the `accounts` table — dropped for security; accounts are now
  matched only via stored hash fingerprints.
