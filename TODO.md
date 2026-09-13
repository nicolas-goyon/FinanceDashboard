# TODO — Finance Dashboard

> Working backlog: planned features, known limitations, bugs, and cleanups found during review.
> When picking something up, check it off and — if it's more than a trivial fix — add an entry
> to `version.md` under the in-progress version (see that file's own instructions).

---

## Planned features / known limitations

Carried over from the former "Known Limitations / TODOs" section of `ARCHITECTURE.MD` (moved
here so the architecture doc stays a design record, not a task tracker).

- [ ] **Wire pages to real data** — replace the MOCK generators in the 5 mock page controllers (Overview/Cash Flow/Savings/Investments/Projects) with DB aggregation. The schema needs to grow: per-account monthly snapshots + categorized transactions (see `PROJECT.md`).
- [ ] **No "show low-confidence rows first" sort/filter on the import review table** — the predictor now fills `confidence` for real (see `docs/category-prediction.md`) and the badge colors it, but on a long statement the amber/red rows still have to be found by scrolling. `SnapshotImportController::predictionInfo.reviewCount` already knows how many there are.
- [ ] **Category rules have no management page** — they can be created from the import review screen's RULE column and are honoured by the predictor, but there is no way to list, edit, re-target or delete them short of editing the `category_rules` table by hand. Needs a page (natural home: alongside `CategoriesManagePage.qml`), backed by the already-existing `Database::loadCategoryRules()`/`removeCategoryRule()`.
- [ ] **Predictor's `fuzzy` rung scans every known merchant key per row** — O(rows × distinct keys) token-set Jaccard (`CategoryPredictor::matchFuzzyKey()`). Fine at this scale (hundreds of keys); would want a token-inverted candidate filter before it if the key count ever reaches tens of thousands.
- [ ] **`LabelNormalizer`'s stop-word list is French-specific** — a non-French bank export still works (identifier stripping and the empty-key fallback passes are language-agnostic) but produces noisier merchant keys, which mostly costs the `exact` rung its hit rate. Revisit if a second bank's template is ever added.
- [ ] **No diagnostics view for `SnapshotImportController::evaluatePredictor()`** — the leave-one-out accuracy numbers are computed and exposed to QML but nothing displays them yet. Natural home is Database Management, next to the other developer-facing actions.
- [ ] **`snapshots`/`asset_entries` DB tables are dead weight** — still compiled and wired into `Database`, but nothing has read or written them since the legacy controllers were deleted. Decide: build the "wire pages to real data" item on top of this existing schema, or drop it in favor of the per-transaction model.
- [ ] Range selector (12–24 month window) on time charts — `PROJECT.md` calls for it on all pages.
- [ ] Loan simulator: add optional insurance (assurance emprunteur) flat % input.
- [ ] `Fmt::money` not yet reachable from QML — Y-axis uses `labelFormat: "%.0f €"` instead of French-locale formatting.
- [ ] Centralize the Catppuccin palette in a `Theme.qml` singleton (currently hardcoded hex literals per file).
- [ ] No `.qrc` resource file — icons and fonts should be added under `resources/` when needed.
- [ ] No tests — add a `tests/` directory with Qt Test when data logic grows.
- [ ] `SumeriaCcpCsvExtractionTemplate` assumes UTF-8 and `MM/dd/yyyy` dates, per the spec as given — neither was verified against a real exported file yet.
- [ ] PDF statement templates are not implemented — `ImportTemplate` supports them by design, but no PDF library is linked yet.
- [ ] `ImportSnapshotPage.qml`/`CategoriesManagePage.qml`/`AccountsManagePage.qml` import `QtQuick.Dialogs` with no explicit `find_package`/`target_link_libraries` in `CMakeLists.txt` — relies on `qt_add_qml_module`'s automatic import scanning. Add `QuickDialogs2` explicitly if a build ever fails to find the plugin.
- [ ] Most sidebar action placeholders on the first 5 (`PROJECT.md`) categories are still inert — only a handful of actions are wired (see `docs/navigation.md`).
- [ ] `SumeriaCcpCsvExtractionTemplate::parseAmount()` has the "€" removal and trailing-space removal commented out — an amount like `"1234.56 €"` currently fails to convert. Left as an in-progress edit; revisit once real exported files are available.
- [ ] `CategoryController::removeCategory()`/`removeSubcategory()` and `AccountController::removeAccountCategory()` delete unconditionally — no foreign keys anywhere in this schema, so a transaction/account already using a deleted category just keeps the stale name as plain text. Needs a guard or a "reassign" step if this proves confusing in practice.
- [ ] No reordering of categories/subcategories/account-categories — every reader orders by insertion order only. Add a sort-order column if drag-to-reorder is ever wanted.
- [ ] **Editing a transaction's debit/credit does not recompute `balance_after`** for that row or any later one. Acceptable for now (display-only field), revisit if something starts relying on it.
- [ ] No date-picker widget anywhere in the app — date fields are plain `"yyyy-MM-dd"` text, validated in C++. Add one (`DayOfWeekRow`/`MonthGrid`/`Calendar`, verify against doc.qt.io first) if manual typing proves error-prone.
- [ ] No general migration path for existing SQLite files when the schema changes. The workaround during active development is Database Management → "Reset Database"/"Reset to Mock Data". `transactions.name` is the one column added via a guarded `ALTER TABLE` instead of requiring a reset — follow that pattern for future additive columns.
- [ ] `extractTransactionName()` only has two verified real description shapes so far — revisit once more real bank export samples are available.
- [ ] `DatabaseManagementController::seedMockData()` duplicates the same seed values as `CategoryController`/`AccountController`'s `seedDefaults()` rather than calling them (both private) — the two must be kept in sync by hand if either changes.
- [ ] Not yet componentized (deliberately deferred): a `hoverable`/`onClicked` option on `ChartCard` (so `ProjectsPage.qml`'s bespoke clickable cards could use the shared component); a thin wrapper around the repeated `GraphsView { theme: GraphsTheme { ... } }` boilerplate (appears 10+ times across the 5 analytics pages).

---

## Bugs to fix

- [ ] **Year rollover in transaction dates** — `SumeriaCcpCsvExtractionTemplate.cpp` (`parseStatementDateNoYear`, used in `parseTransactionRow`): row dates only carry `MM/dd`, so the year is always taken from the statement's `periodFrom.year()`. A statement whose period crosses a calendar year boundary (e.g. Dec 20 → Jan 20) will stamp January rows with the wrong (December) year. Fix: use `periodTo.year()` when the row's month is earlier than `periodFrom.month()`, or validate the reconstructed date falls inside `[periodFrom, periodTo]`.
- [ ] **Cash Flow Sankey balance can silently break** — `CashFlowController.cpp` (`flowOutputs()`): the header doc comment promises inputs/outputs totals stay equal, but the leftover/savings bucket is clamped with `qMax(leftover, 0.0)`. In a month where spending exceeds income, `flowOutputs()`'s total would drop below `flowInputs()`'s — masked today only because mock data never overspends.
- [ ] **`ProjectsPage.qml` loan-rate field can go stale** — `rateField`'s `text: controller.loanRatePct.toFixed(2)` is a page-level (not per-delegate) declarative binding. Every other input in the app either lives in a Repeater delegate (recreated on model change) or is set imperatively on dialog-open; this one isn't, so any user keystroke breaks the binding permanently — switching projects afterward can leave a stale value on screen. Fix: set `rateField.text` imperatively (e.g. on `onProjectChanged`), matching the rest of the app's pattern.
- [ ] **Unguarded account lookups in Database Management dialogs** — `DatabaseManagementPage.qml`'s `clearTransactionsDialog.openFor()`/`deleteAccountDialog.openFor()` dereference `AccountController.accounts[idx].name` with no bounds/null guard, unlike `AccountHistoryPage.qml`/`AccountTransactionsPage.qml` which guard explicitly. Verify whether `ComboBox.currentIndex` auto-clamps when the backing array shrinks after a delete; if not, a second click before reselecting will throw.
- [ ] **`ImportSnapshotPage.qml` file picker assumes a template is always selected** — `fileDialog.onAccepted` calls `controller.parseFile(selectedFile, page.currentTemplate.id)` with no null check. Harmless while exactly one import template exists, but the button that opens the dialog isn't disabled when `controller.templates.length === 0`, so this is a latent crash once a second template ships or the list can be empty.
- [ ] **Missing `pragma ComponentBehavior: Bound`** on `InvestmentsPage.qml` and `ImportSnapshotPage.qml` — every other page using `required property` inside a Repeater delegate declares this pragma; these two don't, despite using `required property var modelData` themselves. Verify against doc.qt.io whether this has a real behavioral effect here, then align for consistency.
- [ ] **Unguarded chart-title string interpolation** — `CashFlowPage.qml` concatenates `controller.monthLabels[controller.selectedMonth]` directly into chart titles with no bounds check, unlike the rest of the app's defensive-display convention (e.g. `AccountHistoryPage.qml` ternary-guards `page.account !== null` before display). Low risk today (mock data populates synchronously) but worth aligning.
- [ ] **`Database::open()` silently ignores a WAL-mode failure** — `Database.cpp`, the `PRAGMA journal_mode = WAL;` exec's return value isn't checked/logged, unlike every other query-failure path in the same file. Add a `qWarning()` on failure for parity.

---

## Naming / professionalism cleanups

- [ ] **Six truncated include-guard comments** in `src/db/tables/` — the trailing `// FINANCEDASHBOARD_DB_TABLES_...` comment after `#endif` is cut off mid-identifier (and missing a trailing newline) in `AccountsTable.h`, `CategoriesTable.h`, `SubcategoriesTable.h`, `AccountCategoriesTable.h`, `AccountTypesTable.h`, and `AccountHashesTable.h`. `Database.h` is also missing a trailing newline (its comment is intact). Harmless to compilation, but worth cleaning up — check `git blame`/diff first in case it points at a real truncated save rather than a cosmetic slip.
- [ ] **Emoji icons clash with the app's flat icon language** — 🔍 (`ImportSnapshotPage.qml`, `AccountTransactionsPage.qml`) and ⚖ (`ProjectsPage.qml`) are full-color emoji glyphs; every other icon in the app (✎, ×, ＋, ✓, ⚠, ←, ▾, ▸) is a flat monochrome Unicode symbol that inherits the surrounding text color. Swap both for a monochrome equivalent (or a small `Canvas`-drawn glyph, same approach `ColorComboBox.qml` already uses for its dropdown arrow) so they don't visually clash with the Catppuccin palette.
- [ ] `GhostButton.qml`'s doc comment enumerating known use sites doesn't mention the 🔍 usage added later — minor doc drift, update alongside the emoji fix above.

---

## Convention drift (should follow existing in-app patterns)

- [ ] **Multi-line `// NOLINTNEXTLINE(...)` comments don't actually suppress anything** — the directive applies to the *next line only*, so wherever the explanation continues onto a second comment line (`CategoriesTable::insert()`, `AccountHashing::compute()`, `CategoryRulesTable::updateTarget()`, `splitPair()` in `CategoryPredictor.cpp`, …) the suppressed check still fires; verified with clang-tidy against `bugprone-easily-swappable-parameters`. Fix by putting the explanation *above* the `NOLINTNEXTLINE` line rather than after it, and apply the same shape everywhere so the convention stays uniform.

- [ ] **`AccountController`'s account-category/account-type CRUD gives no status feedback** — `addAccountCategory`/`renameAccountCategory`/`removeAccountCategory`/`addAccountType`/`renameAccountType`/`removeAccountType` never call `setStatus()`, unlike `CategoryController`'s equivalent methods (which all report success/failure via `statusText`) and unlike `AccountController`'s own account/hash CRUD. A duplicate-name rejection or DB failure here is currently a silent no-op. Mirror `CategoryController`'s pattern.
- [ ] **`SnapshotsTable`'s read methods don't log query failures** — `idForDate()`, `latest()`, `allDates()` skip the `qWarning()` on `query.exec()` failure that every sibling table's read methods include (`AccountsTable::all()`, `CategoriesTable::all()`, `AccountHashesTable::allForAccount()`, etc.). Add for parity/debuggability.
- [ ] **Duplicated month/completion-date math in `ProjectsController`** — `rebuildCards()` and `recomputeProject()` repeat the same `monthsToTarget` and `monthsToComplete`/`completion` calculations byte-for-byte. `Formatter.h`'s `Fmt::dateOrDash()` was hoisted out for exactly this kind of duplication — apply the same fix here (e.g. a private `monthsUntil(QDate, QDate)` helper).
- [ ] **Duplicated debit/credit text formatting** — `AccountController::transactionsForAccount()` and `SnapshotImportController::transactionRows()` both inline the same `debit > 0.0 ? Fmt::money(-debit) : "—"` / `credit > 0.0 ? Fmt::money(credit, true) : "—"` formula. Hoist into `Fmt::` alongside `dateOrDash()`.
- [ ] **Hash/checkpoint chip removers on `AccountsManagePage.qml` skip `GhostButton`** — both the hash-chip "×" and the balance-checkpoint-chip "×" are still bare `Text + TapHandler` with no hover feedback, the exact pattern the shared-components pass was meant to eliminate everywhere (and this same file already uses `GhostButton{variant:"danger"}` for its category-row "×" a few lines above). Swap both to `GhostButton`.
- [ ] **`ProjectsPage.qml`'s detail view hand-rolls its own header** instead of reusing `PageHeader`, unlike every other page (including the list view directly above it in the same file). Consequence: back navigation doesn't use the standard "←" affordance, and the detail view never shows the `[MOCK]` badge the list view has.
- [ ] **Transaction table headers duplicated instead of reusing `ColumnHeaderCell`** — `AccountHistoryPage.qml` and `AccountTransactionsPage.qml` each hand-roll a near-identical fixed-width table header, while `ColumnHeaderCell.qml` already exists as a shared component (used by `ImportSnapshotPage.qml`). Worth consolidating if a third table header needs a similar layout again.
- [ ] **Money formatting duplicated across six QML call sites** — `HoverBar.qml`, `HoverPoint.qml`, `MoneyFlow.qml` (`euro()`), `OverviewPage.qml`, `CashFlowPage.qml`, and `ImportSnapshotPage.qml` each independently call `.toLocaleString(Qt.locale("fr-FR"), 'f', decimals) + " €"` for chart tooltips/click handlers (these only ever receive raw numbers from Qt Graphs, not controller-formatted text). Six places to update if locale/decimals/currency ever changes — worth a shared QML helper once `Fmt::money` is exposed to QML (see the existing TODO above).

---

## Minor cleanups

- [ ] `ProjectsController.cpp` — name the magic numbers `180` (months, "15-year" comparison scenario) and `10000.0` (nominal loan principal when there's no funding gap) as `constexpr`, matching this file's existing `kHouseDepositTarget`/`kChartMaxHeadroom` convention. `.clang-tidy`'s `cppcoreguidelines-*` set is likely to flag both as-is.
- [ ] `SumeriaCcpCsvExtractionTemplate.cpp` — `QString(value).remove(QLatin1Char(' '))` for the IBAN field constructs a redundant copy since `value` is already a `QString`; simplify to a plain assignment + `remove()`.
- [ ] `Formatter.h` — trailing-comment alignment is inconsistent between the EUR/USD branches of the currency suffix block; a cosmetic nit.
- [ ] `MoneyFlow.qml` — `function repaint()` is dead code (nothing calls it; `onInputsChanged`/`onOutputsChanged` call `canvas.requestPaint()` directly instead).
- [ ] `AccountHistoryPage.qml` — the "one day in ms" constant (`86400000`) is repeated twice with no named property.
- [ ] `AccountHistoryPage.qml` and `AccountTransactionsPage.qml` — the category/subcategory badge truncation width (`Math.min(implicitWidth, 118)`) is the same magic number repeated across both files.
- [ ] `AccountsManagePage.qml` and `ImportSnapshotPage.qml` — the `(count === 1 ? "" : "es")` hash/hashes pluralization is duplicated verbatim in both files.
- [ ] `CashFlowPage.qml`'s month-picker `onActivated: (index) => controller.selectedMonth = index` uses an explicit arrow-function parameter, while every other picker in the app uses the implicit form (`onActivated: page.x = currentIndex`). Pick one convention.
- [ ] `AccountsManagePage.qml` — `Repeater { model: (page.checkpointsTick, AccountController.balanceCheckpoints(accRow.index)) }` uses the JS comma operator to force a binding dependency; it's explained in a comment but reads like a typo on a skim. Consider a small named helper property instead.

---

## Documentation follow-ups

- [ ] Once the automated doc-generation script (see `docs/automated-docs.md`) is wired into a regular habit, drop any architecture-doc file/module listings that duplicate what the script now produces on demand.
- [ ] ~40 files under `src/`/`qml/` still have source comments referencing `ARCHITECTURE.MD §<Section Name>` — those sections moved into `docs/*.md` when `ARCHITECTURE.MD` was split into an index. Not rewritten in this pass (mechanical, cross-file, higher risk of drift than doing it by hand file-by-file at a natural touch point). Update a file's own `§` references to the matching `docs/*.md` path the next time you're editing that file anyway, rather than as a dedicated sweep.
