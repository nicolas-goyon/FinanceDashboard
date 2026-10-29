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
- [ ] **`LabelNormalizer`'s stop-word list covers French labels and the English labels of the Sumeria export (#55)** — another bank's export still works (identifier stripping is language-agnostic) but produces noisier merchant keys, which mostly costs the `exact` rung its hit rate. Add its boilerplate when a second bank's template is added; schema step 6 (`Database::migrateTo6()`) shows how to normalize the stored rule patterns again.
- [ ] **A category rule covers both directions of a transfer** — since #55, a merchant key has no direction words: "Transfer to JOHN DOE" and "Payment received from JOHN DOE" both give "JOHN DOE". So a rule taught from a rent payment ("Housing / Rent") also gives its category, with full confidence, to a refund from that person, and schema step 6 widens the migrated rules ("TRANSFER TO JOHN DOE" becomes "JOHN DOE") in the same way. Maintainer decision of 2026-10-22: keep it, and document it. If it matters later: skip a rule whose category has only the other sign (as `CategoryPredictor::signIsPlausible()` does for the other rungs), or keep the sign of the row in the rule.
- [ ] **A label with only boilerplate and a month name gets the month as its key** — "FACTURE MARS" gives "MARS", the key of a merchant MARS (`src/predict/LabelNormalizer.h`). A month next to a day or a year ("05 MARS", "MARS 2026") is a date and goes.
- [ ] **A short code next to another name is taken for a reference** — "AS24 LYON" gives the key "LYON", and "AS24" alone gives "AS24" (`src/predict/LabelNormalizer.h`). A rule that tells a short name ("AS24", "O2") from a short reference ("T3", "4972B") next to another word needs real labels to check it on; a change of the keys also needs a schema step for the stored rule patterns.
- [ ] **No diagnostics view for `SnapshotImportController::evaluatePredictor()`** — the leave-one-out accuracy numbers are computed and exposed to QML but nothing displays them yet. Natural home is Database Management, next to the other developer-facing actions.
- [ ] **`snapshots`/`asset_entries` DB tables are dead weight** — still compiled and wired into `Database`, but nothing has read or written them since the legacy controllers were deleted. Decide: build the "wire pages to real data" item on top of this existing schema, or drop it in favor of the per-transaction model.
- [ ] Range selector (12–24 month window) on time charts — `PROJECT.md` calls for it on all pages.
- [ ] Loan simulator: add optional insurance (assurance emprunteur) flat % input.
- [ ] `Fmt::money` not yet reachable from QML — Y-axis uses `labelFormat: "%.0f €"` instead of French-locale formatting.
- [ ] Centralize the Catppuccin palette in a `Theme.qml` singleton (currently hardcoded hex literals per file).
- [ ] The UI has no icon or font resources — add them with `qt_add_resources()`, as the app icon in `CMakeLists.txt`, when needed.
- [ ] No tests — add a `tests/` directory with Qt Test when data logic grows.
- [ ] `SumeriaCcpCsvExtractionTemplate` reads UTF-8, or Windows-1252 when the file is not valid UTF-8 (#45), and assumes `MM/dd/yyyy` dates, per the spec as given — the dates were not verified against a real exported file yet.
- [ ] PDF statement templates are not implemented — `ImportTemplate` supports them by design, but no PDF library is linked yet.
- [ ] `ImportSnapshotPage.qml`/`CategoriesManagePage.qml`/`AccountsManagePage.qml` import `QtQuick.Dialogs` with no explicit `find_package`/`target_link_libraries` in `CMakeLists.txt` — relies on `qt_add_qml_module`'s automatic import scanning. Add `QuickDialogs2` explicitly if a build ever fails to find the plugin.
- [ ] Most sidebar action placeholders on the first 5 (`PROJECT.md`) categories are still inert — only a handful of actions are wired (see `docs/navigation.md`).
- [ ] `CategoryController::removeCategory()`/`removeSubcategory()` and `AccountController::removeAccountCategory()` delete unconditionally — no foreign keys anywhere in this schema, so a transaction/account already using a deleted category just keeps the stale name as plain text. Needs a guard or a "reassign" step if this proves confusing in practice.
- [ ] No reordering of categories/subcategories/account-categories — every reader orders by insertion order only. Add a sort-order column if drag-to-reorder is ever wanted.
- [ ] **Editing a transaction's debit/credit does not recompute `balance_after`** for that row or any later one. Acceptable for now (display-only field), revisit if something starts relying on it.
- [ ] No date-picker widget anywhere in the app — date fields are plain `"yyyy-MM-dd"` text, validated in C++. Add one (`DayOfWeekRow`/`MonthGrid`/`Calendar`, verify against doc.qt.io first) if manual typing proves error-prone.
- [ ] `extractTransactionName()` only has two verified real description shapes so far — revisit once more real bank export samples are available.
- [ ] `DatabaseManagementController::seedMockData()` duplicates the same seed values as `CategoryController`/`AccountController`'s `seedDefaults()` rather than calling them (both private) — the two must be kept in sync by hand if either changes.
- [ ] Not yet componentized (deliberately deferred): a `hoverable`/`onClicked` option on `ChartCard` (so `ProjectsPage.qml`'s bespoke clickable cards could use the shared component); a thin wrapper around the repeated `GraphsView { theme: GraphsTheme { ... } }` boilerplate (appears 10+ times across the 5 analytics pages).

---

## Bugs to fix

- [ ] **Unguarded chart-title string interpolation** — `CashFlowPage.qml` concatenates `controller.monthLabels[controller.selectedMonth]` directly into chart titles with no bounds check, unlike the rest of the app's defensive-display convention (e.g. `AccountHistoryPage.qml` ternary-guards `page.account !== null` before display). Low risk today (mock data populates synchronously) but worth aligning.

---

## Naming / professionalism cleanups

- [ ] **Six truncated include-guard comments** in `src/db/tables/` — the trailing `// FINANCEDASHBOARD_DB_TABLES_...` comment after `#endif` is cut off mid-identifier (and missing a trailing newline) in `AccountsTable.h`, `CategoriesTable.h`, `SubcategoriesTable.h`, `AccountCategoriesTable.h`, `AccountTypesTable.h`, and `AccountHashesTable.h`. `Database.h` is also missing a trailing newline (its comment is intact). Harmless to compilation, but worth cleaning up — check `git blame`/diff first in case it points at a real truncated save rather than a cosmetic slip.
- [ ] **Emoji icons clash with the app's flat icon language** — 🔍 (`ImportSnapshotPage.qml`, `AccountTransactionsPage.qml`) and ⚖ (`ProjectsPage.qml`) are full-color emoji glyphs; every other icon in the app (✎, ×, ＋, ✓, ⚠, ←, ▾, ▸) is a flat monochrome Unicode symbol that inherits the surrounding text color. Swap both for a monochrome equivalent (or a small `Canvas`-drawn glyph, same approach `ColorComboBox.qml` already uses for its dropdown arrow) so they don't visually clash with the Catppuccin palette.
- [ ] `GhostButton.qml`'s doc comment enumerating known use sites doesn't mention the 🔍 usage added later — minor doc drift, update alongside the emoji fix above.

---

## Convention drift (should follow existing in-app patterns)

- [ ] **Multi-line `// NOLINTNEXTLINE(...)` comments don't actually suppress anything** — the directive applies to the *next line only*, so wherever the explanation continues onto a second comment line (`CategoriesTable::insert()`, `AccountHashing::compute()`, `CategoryRulesTable::updateTarget()`, `splitPair()` in `CategoryPredictor.cpp`, …) the suppressed check still fires; verified with clang-tidy against `bugprone-easily-swappable-parameters`. Fix by putting the explanation *above* the `NOLINTNEXTLINE` line rather than after it, and apply the same shape everywhere so the convention stays uniform.

- [ ] **`AccountController`'s account-category/account-type CRUD gives little status feedback** — only `renameAccountCategory`/`renameAccountType` call `setStatus()` (#34); `addAccountCategory`/`removeAccountCategory`/`addAccountType`/`removeAccountType` never do, unlike `CategoryController`'s equivalent methods (which all report success/failure via `statusText`) and unlike `AccountController`'s own account/hash CRUD. A duplicate-name rejection or DB failure here is currently a silent no-op. Mirror `CategoryController`'s pattern.
- [ ] **`SnapshotsTable`'s read methods don't log query failures** — `idForDate()`, `latest()`, `allDates()` skip the `qWarning()` on `query.exec()` failure that every sibling table's read methods include (`AccountsTable::all()`, `CategoriesTable::all()`, `AccountHashesTable::allForAccount()`, etc.). Add for parity/debuggability.
- [ ] **Duplicated month math in `ProjectsController`** — `rebuildCards()` and `recomputeProject()` repeat the same `monthsToTarget` calculation byte-for-byte (the completion date is one `completionOf()` since #41). `Formatter.h`'s `Fmt::dateOrDash()` was hoisted out for exactly this kind of duplication — apply the same fix here (e.g. a private `monthsUntil(QDate, QDate)` helper).
- [ ] **Duplicated debit/credit text formatting** — `AccountController::transactionsForAccount()` and `SnapshotImportController::transactionRows()` both inline the same `debit > 0.0 ? Fmt::money(-debit) : "—"` / `credit > 0.0 ? Fmt::money(credit, true) : "—"` formula. Hoist into `Fmt::` alongside `dateOrDash()`.
- [ ] **Hash/checkpoint chip removers on `AccountsManagePage.qml` skip `GhostButton`** — both the hash-chip "×" and the balance-checkpoint-chip "×" are still bare `Text + TapHandler` with no hover feedback, the exact pattern the shared-components pass was meant to eliminate everywhere (and this same file already uses `GhostButton{variant:"danger"}` for its category-row "×" a few lines above). Swap both to `GhostButton`.
- [ ] **`ProjectsPage.qml`'s detail view hand-rolls its own header** instead of reusing `PageHeader`, unlike every other page (including the list view directly above it in the same file). Consequence: back navigation doesn't use the standard "←" affordance, and the detail view never shows the `[MOCK]` badge the list view has.
- [ ] **Transaction table headers duplicated instead of reusing `ColumnHeaderCell`** — `AccountHistoryPage.qml` and `AccountTransactionsPage.qml` each hand-roll a near-identical fixed-width table header, while `ColumnHeaderCell.qml` already exists as a shared component (used by `ImportSnapshotPage.qml`). Worth consolidating if a third table header needs a similar layout again.
- [ ] **Money formatting duplicated across six QML call sites** — `HoverBar.qml`, `HoverPoint.qml`, `MoneyFlow.qml` (`euro()`), `OverviewPage.qml`, `CashFlowPage.qml`, and `ImportSnapshotPage.qml` each independently call `.toLocaleString(Qt.locale("fr-FR"), 'f', decimals) + " €"` for chart tooltips/click handlers (these only ever receive raw numbers from Qt Graphs, not controller-formatted text). Six places to update if locale/decimals/currency ever changes — worth a shared QML helper once `Fmt::money` is exposed to QML (see the existing TODO above).

---

## Minor cleanups

- [ ] `ProjectsController.cpp` — name the magic number `180` (months, "15-year" comparison scenario) as a `constexpr`, matching this file's existing `kHouseDepositTarget`/`kChartMaxHeadroom`/`kSampleLoan` convention. `.clang-tidy`'s `cppcoreguidelines-*` set is likely to flag it as-is.
- [ ] `SumeriaCcpCsvExtractionTemplate.cpp` — `QString(value).remove(QLatin1Char(' '))` for the IBAN field constructs a redundant copy since `value` is already a `QString`; simplify to a plain assignment + `remove()`.
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
