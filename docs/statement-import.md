# Statement Import ("Import Transactions")

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.

Reached from the sidebar (Transactions → "Import Transactions"). Lets the user pick a
bank/extract file, parse it into editable+categorizable transactions, and review parse issues
before a real save.

## Design decisions

- **Persistence scope: parse is REAL, save is REAL.** `SnapshotImportController::
  save(accountId, accountLabel)` builds a `Data::Transaction` per included row and writes them
  through `Database::insertTransactions()` (see `docs/account-management.md` for the
  `transactions` table). It also auto-links the statement's account-hash fingerprint to the
  chosen account (`Database::accountHasHash()`/`addAccountHash()`, source `"auto"`) if that
  account doesn't have it on file yet.
- **Category-prediction confidence: review-screen-only, never persisted.** No prediction model
  exists yet (see `TODO.md`) — every row's `confidence` stays at its struct default (`0.0`) and
  category/subcategory stay empty until the user picks them. Once the user reviews a row and
  hits Save, only the resulting `category`/`subcategory` are written; the confidence score is
  discarded. **Deliberately kept out of the `transactions` table / `Data::Transaction`** — an
  earlier version of this schema briefly had a `confidence` column, corrected per product
  decision.
- **Template design: one hardcoded C++ class per format, not a declarative/config-driven
  reader.** Formats aren't simple tables (multi-line headers, PDF layouts planned) — each
  format gets real parsing logic. New format = new `ImportTemplate` subclass + one line in
  `ImportTemplateRegistry::all()`.
- **Account matching: auto by *stored* account-hash fingerprint only, otherwise manual.**
  `SnapshotImportController::matchAccountIndex()` never compares the statement's IBAN/BIC to an
  account's IBAN/BIC directly — accounts don't store iban/bic/number at all (removed for
  security, see `docs/account-management.md`). Instead the shared `AccountHashing::compute()`
  (`src/utils/AccountHashing.h`, HMAC-SHA256 of the normalized IBAN keyed by the normalized
  BIC) is used once on the statement's IBAN/BIC, and the result is compared against each
  account's *stored* `hashes` list — an account only matches if it has actually been linked. No
  match → the QML page shows a `ComboBox` for a manual pick, plus a "not linked" badge. The
  fingerprint is also surfaced as `statementInfo.accountHash` (pretty, "—" when empty) and
  `statementInfo.accountHashRaw` (raw) and shown in the header so the match is auditable.
- **`AccountHashing::compute()`** lives in `src/utils/AccountHashing.h`, shared by
  `AccountController` and `SnapshotImportController` — both call sites must use this one
  function so the statement side and account side never drift.
- **"Not linked" badge + inline linking.** `ImportSnapshotPage.qml` shows a small badge next to
  the target-account picker driven by the *selected* account's own `linked`/`hashCount` fields
  — independent of whether the loaded statement happened to auto-match it. A "Link this
  account" button appears when the statement has a computable fingerprint the selected account
  doesn't have yet; it calls `AccountController.linkAccountHash(accountIndex, hash)` directly
  (source `"auto"`).
- **No C++-to-C++ singleton coupling between the two controllers.** `SnapshotImportController::
  save()` writes straight to `Database` rather than reaching into the `AccountController`
  singleton from C++. The QML side calls `AccountController.reload()` right after
  `controller.save(...)` so the singleton's cached state picks up the write.
- **Transaction table columns: resizable + persisted, QML-only.** `ImportSnapshotPage.qml`
  holds a page-local `columnWidths` `QtObject` (one `property real` per column) that both the
  header row and every data-row cell bind their `Layout.preferredWidth` to. Each header cell is
  a `ColumnHeaderCell.qml` (see `docs/shared-components.md`) with a drag handle. Persisted
  across restarts through `QtCore`'s `Settings` QML type (`import QtCore`,
  https://doc.qt.io/qt-6/qml-qtcore-settings.html), one `property alias` per column under
  category `"ImportTransactionsColumns"` — not the deprecated `Qt.labs.settings`.

## Import Report / Reconciliation

- **Save button: brief disable + gated by reconciliation, then a report step.**
  `ImportSnapshotPage.qml`'s `requestSave()` runs `SnapshotImportController::
  checkReconciliation(accountId)` first. If check 2 (below) fails, a dialog opens ("Balance
  doesn't add up") instead of saving. Otherwise `doSave()` sets `page.saveBusy = true`
  (disables the Save button, double-click guard), calls `controller.save()`, reloads
  `AccountController`/`CategoryController`, and sets `page.step = 2`.
- **Step 2 ("import report") is a third step inside the same `Item`/controller, not a new
  overlay page** — `Main.qml`'s overlay `Loader`s are gated by `active:`, which destroys the
  page (and its inline `SnapshotImportController { id: controller }`) the moment
  `currentViewId` changes away — a separate overlay would lose the freshly-built report.
- **`SnapshotImportController::checkReconciliation(accountId)`** — read-only, callable
  repeatedly (e.g. every time a KEEP checkbox changes). Two checks against the *currently
  parsed* statement:
  - **Check 1 ("start balance"), informational only, never blocks Save.** Compares the
    statement's declared `balanceStart` against the best "what did we last know" reference: the
    latest `AccountBalanceCheckpoint` (see `docs/balance-checkpoints.md`) or, failing that, the
    latest saved transaction's `balanceAfter`. If neither exists, the check is skipped entirely
    (`"hasStartReference": false`) — an explicit product requirement ("if the import is before
    all record, don't take today's money").
  - **Check 2 ("end balance"), blocks Save on mismatch.** `balanceStart + sum(credit - debit)`
    over every currently-*included* row, compared against the statement's declared `balanceEnd`.
    A `kReconciliationEpsilon` (0.01) tolerance absorbs float rounding, not genuine
    discrepancies.
  - Returns `{"available": false}` if nothing's been parsed or `accountId` is invalid.
- **Mismatch dialog ("Balance doesn't add up")** — opened by `requestSave()` when check 2
  fails. Two explicit choices, neither of which auto-saves:
  - **"Review kept transactions"** — closes the dialog; the user toggles KEEP checkboxes and
    clicks Save again.
  - **"Add adjustment transaction"** — calls `addReconciliationAdjustment(accountId)`, which
    appends one synthetic row for the exact gap, dated at `balanceEndDate` (falls back to
    `periodTo`), tagged with a category named **"Reconciliation Adjustment"**.
    `ensureCategoryExists()` creates that category (neutral grey `#6c7086`) the first time it's
    needed. The new row still goes through the normal review/Save flow.
- **The report itself** (`SnapshotImportController::lastImportReport`) is built by
  `buildImportReport()` inside `save()`, from the rows *actually written*, plus a
  `checkReconciliation()` snapshot taken *before* the write. Shape:
  ```
  { accountLabel, transactionCount, totalIncomeText, totalExpenseText, netChangeText,
    byCategory: [{ category, totalText, totalRaw,
                   subcategories: [{ subcategory, totalText, totalRaw }, ...] }, ...],
    reconciliation: <checkReconciliation() result> }
  ```
  Category/subcategory grouping and sums are computed in C++ (`QMap<QString, double>`); color
  lookups for the report's badges are resolved in QML via
  `CategoryController.categoryColor()`/`subcategoryColor()`.
- **"By category" is a pie chart, plus a colored legend.** `ImportSnapshotPage.qml`'s report
  step builds a `PieSeries` (`GraphsView`/`GraphsTheme.ColorScheme.Dark`, `pieSize: 0.75`,
  `holeSize: 0.42`), same recipe as `CashFlowPage.qml`'s "Spending by category" donut. Slice
  size is each category's **magnitude** (`Math.abs(totalRaw)`), not the signed net total — a
  meaningful "share of the whole" needs unsigned magnitudes; the exact signed totals are shown
  in the legend rows below.
- **The statement's declared start/end balances are remembered automatically.**
  `SnapshotImportController::save()` calls a private `rememberStatementBalance(accountId, date,
  balance, note)` twice — once for `balanceStart`/`balanceStartDate`, once for
  `balanceEnd`/`balanceEndDate` — right after the transactions write succeeds. Each call is a
  no-op if the account already has an `AccountBalanceCheckpoint` dated exactly `date`; otherwise
  it calls `Database::addBalanceCheckpoint()` with a note like `"Auto — statement start
  (filename.csv)"`.

## `ImportTemplate` (src/import/ImportTemplate.h)

Abstract interface: `id()`, `displayName()`, `fileFilter()` (QtQuick.Dialogs `nameFilters`
entry), and `parse(filePath, outStatement, outLog) -> bool`. Row-level failures are appended to
`outLog` (`Data::ImportLogEntry`, Info/Warning/Error) and the row is skipped — `parse()` only
returns `false` for fatal failures (file unreadable, header not found).

## `SumeriaCcpCsvExtractionTemplate` (src/import/SumeriaCcpCsvExtractionTemplate.cpp)

First (and currently only) template. Format is documented verbatim in the header comment. Key
implementation notes:

- **Parses the whole file in one pass, NOT line-by-line.** `Description and identifier` is a
  quoted field that can contain **embedded newlines** as well as commas. `parseCsvRecords()`
  reads the entire file (`readAll()`) and tokenizes it in a single quote-aware pass, where a
  `\n` only ends a record when it occurs *outside* an open quote. Each `CsvRecord` also carries
  its starting physical line number (for log messages) and the raw source text (rendered with
  embedded newlines shown as "↵" via `previewRawText()`). `normalizeField()` then collapses any
  embedded newlines to spaces in stored field values.
- Locates the transaction header row by content match (case-insensitive "confirmation date" +
  "operation date"), not a fixed line number.
- Metadata lines (holder name, IBAN, period, …) are matched by label, not position.
- `parseAmount()` tolerates "1 234.56 €", "1234,56", spaces/narrow-no-break-spaces as thousands
  separators — disambiguates `,` vs `.` as the decimal separator by taking whichever appears
  **last** in the string when both are present. See `TODO.md` for the currently-commented-out
  "€" stripping.
- Dates use `MM/dd/yyyy` per the spec as given (not French `dd/MM/yyyy`) — unverified against a
  real export file, see `TODO.md`.
- Category/subcategory are left **empty** at parse time — picked manually against
  `CategoryController` (see `docs/category-management.md`). The old keyword auto-fill
  (`CategoryKeywords::suggest()`) was removed per product decision.
- **Known limitation:** assumes UTF-8 file encoding.
- **`extractTransactionName()`** derives `ImportedTransaction::name` from the raw description
  field: every line from the start, up to (but not including) the first line that looks like a
  reference line (`isReferenceLine()`), joined with a single space. Two verified real shapes so
  far — see the function's doc comment in the source file for both. **Deliberately one rule for
  every transaction type** — no special-casing per product decision; see `TODO.md` if a new
  shape needs different handling. `description` itself (the full text) is unaffected — `name`
  is a separate, additional field.

## `SnapshotImportController` (src/controllers/dataentry/SnapshotImportController.h/.cpp)

`QML_ELEMENT` backend for `ImportSnapshotPage.qml`. Holds one `Data::ImportedStatement` + its
`QVector<Data::ImportLogEntry>` log after `parseFile(fileUrl, templateId)`.
`transactionRows()`/`statementInfo()` format everything with `Fmt::money`/date strings for QML.
Row edits (`setRowCategory`, `setRowSubcategory`, `setRowIncluded`, `setRowName`) mutate the
controller's own statement and re-emit `statementChanged()`; there is no separate
`QAbstractListModel`, just a recomputed `QVariantList` each time. `setRowCategory`/
`setRowIncluded`/`setRowName` skip the emit when the value isn't actually changing — needed to
avoid a QML "Binding loop detected for property model" during initial delegate construction.

## `ImportSnapshotPage.qml`

Two-step page (`step: 0` pick template+file, `step: 1` review/edit) inside a single `Item`.
Accounts come from `AccountController` (`QML_SINGLETON`). Uses `QtQuick.Dialogs.FileDialog`
(nameFilters driven by the selected template's `fileFilter()`). Each transaction row has an
editable "NAME" `TextField` and, instead of an inline description column, a small 🔍
`GhostButton` opens `descriptionPopup` (one shared `Popup` instance per page) showing the full
text in a read-only, selectable `TextEdit`. Each row also has two `ColorComboBox`es (category
then subcategory, live-filtered), which fill their own background with the item's color so the
picked color is visible while choosing. The parse log is a `Popup` (list of
level/line/message/rawLine), reachable both from a badge in the statement-info card and a "View
log" footer button. `closeRequested()` returns to the sidebar view (see `docs/navigation.md`).

**CONFIDENCE column** (90px, right after SUBCATEGORY, before KEEP) surfaces
`ImportedTransaction::confidence` as a `StatusBadge` via two page-level JS helpers:
`confidenceVariant(confidence)` (danger below `0.5`, warning below `0.8`, success at/above) and
`confidenceText(confidence)` (`Math.round(confidence * 100) + "%"`). Both special-case
`confidence <= 0` as `"neutral"` / `"—"` — `0.0` means "not scored yet," not "the model is 0%
confident." Since no prediction model exists yet, every row always shows "—" today.
