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

Documentation and project-hygiene pass — no behavior changes.

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

### Changed
- `README.md` rewritten to be a short, professional front door (what the app is, prerequisites,
  build/run steps) instead of an exhaustive page-by-page feature list — the exhaustive material
  moved into `docs/`.
- `ARCHITECTURE.MD` trimmed to an index/overview page that links into `docs/*.md` instead of
  holding every design-decision narrative inline.

### Docs
- See `Added`/`Changed` above — this entire version is a documentation pass.

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
