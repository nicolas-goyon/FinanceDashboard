# Architecture — Finance Dashboard

> **Audience:** the developers and maintainers of the app, human or Claude.
> This file is the whole architecture document. There is no `docs/` folder (decision of
> 2026-10-17, #102). The class comments in the code give the detail.
> Update this file when the structure or a rule changes. Log the change in `version.json`
> (`python tools/version.py add <Section> "<text>"`).

## Project identity

| Field | Value |
|---|---|
| Name | FinanceDashboard |
| App identities | Prod ("Finance Dashboard") and Test ("Finance Dashboard Test", `-DFD_APP_IDENTITY=test`). See `CMakeLists.txt` § App identity (#130). |
| Version | In `version.json`. The changelog is `version.md`. |
| Language | C++17 |
| UI framework | Qt Quick / QML and Qt Graphs |
| Qt version | 6.10 or later (`qt_standard_project_setup(REQUIRES 6.10)`) |
| Build system | CMake 3.21+ (`qt_add_qml_module`, `qt_generate_deploy_qml_app_script`). Optional Docker build: `README.md` § Building with Docker. |
| Database | SQLite through Qt6::Sql |
| Platforms | Windows and Linux. CI builds and packages both. |
| Chart reference | [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples) |

The Test app has its own data folder, settings, installer identity, desktop entry, icon and
window title. Its Database Management page can copy the prod database ("Copy my prod
database").

## Directory map

```
qml/Main.qml          The window, the sidebar and the navigation (currentViewId).
qml/pages/            One file for each page. Display only.
qml/components/       Shared QML components (ChartCard, PillButton, FormDialog, HoverPoint, …).
qml/components/Theme.qml  Every colour of the QML (a singleton, `Theme.<name>`).
src/controllers/      One subfolder for each domain. One controller class for each page.
src/controllers/ui/   QML singletons with no page, for example Fonts (the fixed-pitch font).
src/db/               Database (connection, transactions, upgrades) and tables/ (one class per table).
src/models/           Plain data structs (Account, Transaction, Category, …).
src/import/           Statement templates (ImportTemplate, SumeriaCcpCsvExtractionTemplate).
src/predict/          Category prediction (CategoryPredictor, LabelNormalizer).
src/update/           Release list, versions and SHA256SUMS parsing, with no network.
src/startup/          Start-up checks (StartupChecks) and the QML network block (LocalOnlyNetwork).
src/utils/            Money, AccountHashing and other helpers.
tests/                The CTest suites. tests/CMakeLists.txt has one comment for each test.
tools/                version.py, release_flow.py, Docker wrappers, requirement checks, analyzers.
launcher/             The start-up launcher (Windows exe, Linux script).
packaging/            Icon, Linux metadata, Windows setup, AppImage and .deb scripts.
docker/               The Docker build image and its scripts.
```

## Design rule

Put the UI in QML. Put the algorithms, the analytics and the data access in C++.

- Each page has one C++ controller class (`QML_ELEMENT`) in `src/controllers/<domain>/`.
- The controller contains all the logic.
- The `.qml` file only shows the data that the controller gives to it.
- Take every colour from `Theme` (`qml/components/Theme.qml`). Do not write a hex colour in a
  page or a component. Add a new colour to `Theme.qml` first.
- Use `QML_SINGLETON` only for data that many pages share (example: `CategoryController`,
  `AccountController`).

Do long work in a worker thread, not in the GUI thread. The example is the category prediction
of an import (`SnapshotImportController`, `QtConcurrent::run`, #52):

1. The `Database` connection works only in the thread that made it (the GUI thread). So the
   controller reads the database first and gives copies to the worker.
2. The result comes back to the GUI thread through a `QFutureWatcher`.
3. A new job cancels the older job (`QPromise::isCanceled()`). The worker checks it after the
   training and between two rows. So quick clicks do not make a queue of jobs.
4. Before the rows get the result, the controller emits `predictionsAboutToBeApplied()`. The
   page then gives the text of a focused NAME field to `setRowName()`. This is necessary because
   the new rows replace the field.

## Add a page

1. Make `src/controllers/<domain>/XxxController.h` and `.cpp` (`QML_ELEMENT`).
2. For a new domain, make a new subfolder. Add it to `target_include_directories` in
   `CMakeLists.txt`.
3. The controller gives its data as `Q_PROPERTY` values or `Q_INVOKABLE` functions. QML does not
   calculate.
4. Make `qml/pages/XxxPage.qml`. Use the shared components of `qml/components/`. Do not make a
   new button or badge when a component exists.
5. Add the controller files to `SOURCES` and the page to `QML_FILES` of `qt_add_qml_module` in
   `CMakeLists.txt`.
6. Add the page to the navigation in `qml/Main.qml`:
   - For a sidebar category page, add `{ id, label, actions }` to `categories`. Add the page to
     the `StackLayout` at the same position.
   - For a page that an action opens, add its id to `overlayViewIds`. Add a `Loader` and a
     `Component`, as the other overlay pages do.
7. Add a changelog entry (`python tools/version.py add <Section> "<text>"`).

## Data rule

### Transactions and writes

- Put writes that belong together in one SQL transaction, in one `Database` method.
- Example: `Database::saveImport()` writes the rows of a statement, the account link and the
  balance checkpoints. If one write fails, the method writes nothing.
- Start a new transaction with `Database::inTransaction()`. It checks `BEGIN` and `COMMIT`.
- The upgrade, `saveImport()` and `resetAll()` check `BEGIN` and `COMMIT` in their own code.
- Check every result.
- A table class that cannot prepare its statement logs the SQLite error
  (`Sql::logPrepareError()`, `src/db/tables/SqlPrepare.h`). Then it returns its failure value.

### Text columns

- Qt binds a null `QString` as SQL `NULL`. The `DEFAULT` of a column does not apply to an
  explicit `NULL`.
- Bind the text of an optional column (`TEXT NOT NULL DEFAULT ''`) through `Sql::text()`
  (`src/db/tables/SqlText.h`). It writes `''`.
- Do not change a required value (a name, a currency, an account hash, a date) to make it valid.
- The `Database` method refuses an empty required value before any write. It puts a message in
  `lastError()` (`tests/db_write_test.cpp`).

### Names stored as text

- Transactions and category rules keep the category and subcategory names as text, not as ids.
- Accounts keep their type and category names as text, not as ids.
- So a rename also changes those rows, in the same SQL transaction. The methods are
  `Database::renameCategory()`, `renameSubcategory()`, `renameAccountType()` and
  `renameAccountCategory()`.
- A subcategory rename changes only the rows of its own category.
- The controllers refuse a rename to a name that exists, in any case, as `add` does.

### Default data

- The controllers write their default data the first time that the app uses a database. The
  default data is the categories, account types, account categories and demo accounts.
- `Database::writeDefaults()` writes the data and a record in the `settings` table, in one SQL
  transaction.
- So an empty table does not mean a first run. If the write fails, nothing stays, and the next
  start tries again.
- "Reset Database" keeps the records. So the default data does not come back, also after a
  restart.

### Copy of the prod database (Test app)

The Test app replaces its database with a copy of the prod database through
`Database::replaceWithCopyOf()` ("Copy my prod database", #130). The steps:

1. The app opens the prod file read-only and copies it with `VACUUM INTO`. The copy includes the
   WAL of the prod file.
2. The app closes the Test file and keeps it as `finance_dashboard.before-copy-<date>.db`.
3. The copy gets the name of the Test file.
4. `open()` opens the copy as at a start. So it upgrades an older copy.

If the copy does not open, the app keeps it as `finance_dashboard.copy-failed-<date>.db`, and
the Test file comes back. If another connection uses the Test file (the Test app is open two
times), the app does not replace it (`tests/db_copy_test.cpp`). The read-only connection can
leave an empty `-wal` file and a `-shm` file next to the prod file. The prod app removes them
when it closes.

### Schema upgrades

- The schema has a version (`PRAGMA user_version`, `Database::kSchemaVersion`).
- `createTable()` is the base schema. Do not change it.
- Write each change after it as one migration step in `Database::upgrade()`, with its own SQL.
- Add a test for each step in `tests/db_migration_test.cpp`.
- Before the app upgrades a database, it saves a copy next to it (`VACUUM INTO`).
- The base tables and every step run in one SQL transaction. So the file is upgraded or
  unchanged.
- The app stops at start-up when a database does not open, with the cause. The causes are: the
  file cannot be read, a newer version saved it, or the upgrade failed. The app closes the
  connection, and `StartupChecks` shows a fatal step.

### Amounts

- An amount is a `double` in the models and a `REAL` in SQLite, in the unit of its account
  (`src/utils/Money.h`, #33).
- The table classes round each amount to the cent before they write it.
- `Database` refuses an amount that is not a number or that is larger than
  `Money::kMaxAmount` (10^13).
- A sum or a comparison that must be exact adds whole cents (`Money::toCents()`, a `qint64`).
  Examples: the reconciliation of an import, a running balance, a total.
- Do not add doubles for these. In a double, 0.10 + 0.20 is not 0.30.

### Account hashes

- An account hash (`account_hashes`) links the statements of a bank account to an account of
  the app. The import then suggests that account.
- `AccountHashing::compute()` calculates an HMAC-SHA256 of the IBAN of a statement, with the BIC
  as the key. The app does not keep the IBAN or the BIC.
- The hash is not a security feature. The BIC is public, and a person who has the database can
  find an IBAN by brute force. `src/utils/AccountHashing.h` gives the limit.
- The app uses the BIC on 11 characters: `PSSTFRPP` is `PSSTFRPPXXX` (#50).
- The import also tries the hash of an 8-character BIC from before #50. Save (or "Link this
  account") replaces it, also when it is on another account. The review screen tells the user.
- A hash belongs to one account, in lower case (#47). A save to another account moves the hash
  there (`Database::linkAccountHash()`).
- A hash that the user types must have 64 hexadecimal characters. No other account can have it.

### Category rules

- The pattern of a category rule (`category_rules`) is a merchant key of
  `Predict::LabelNormalizer::normalize()`, not a raw label.
- When the keys change (a new noise word, another rule for the tokens), add a schema step. The
  step normalizes the stored patterns again, as step 6 does (#40, #55).
- In that step, a rule with only generic words goes. Rules that get one key merge when they give
  one category. They go when they give different categories, because one rule cannot give two.
- The upgrade message gives the counts. The log names each rule.
- Such a step calls the normalizer of the build that runs it. So `normalize()` of a key must give
  the same key. `tools/predictor_selftest.cpp` checks it.
- A key has no direction words ("TRANSFER TO", "RECEIVED FROM"). A rule does not look at the
  sign of a row. A rule covers the debits and the credits of a name (decision of 2026-10-22).

### Transaction identity

- The `fingerprint` of a transaction is its identity in the statement of the bank
  (`TransactionsTable::fingerprints()`).
- When the row has a bank reference (`bankReference()`), the fingerprint is the reference and the
  amount. Else it is every field.
- The app does not save again a row that the account already has.
- Calculate the fingerprints on all the rows of a statement, before you filter them. The second
  identical row of a statement has occurrence 2 in its fingerprint.
- The identity supposes one reference for each transaction. The import reports a skipped row
  that has the reference and the amount of a saved row, but another operation date. The report
  links to the issue in `TransactionsTable::kReusedReferenceIssueUrl`.

## Network rule

The app makes two network calls. Both are in `src/controllers/updates/` (#117).

1. The update check of `UpdateController`. It reads the list of releases of the project
   (`GET api.github.com/repos/.../releases`). It runs at each start, unless the user stopped it,
   and on "Check now".
2. The download of `UpdateInstaller`. It runs only when the user clicks "Install" (setup and
   AppImage). It downloads `SHA256SUMS.txt` and the package of the newest release, from
   `github.com/nicolas-goyon/FinanceDashboard/releases/download/...`.

About the download:

- It follows redirects to GitHub only (`github.com`, `*.githubusercontent.com`).
- It checks the SHA-256 before it uses the file. It checks the setup again just before it runs
  it.

About both calls:

- They send no token, no cookie and no data of the user.
- GitHub sees the internet address of the computer and the version of the app (the
  `User-Agent` header).
- They send `Accept-Language: en`, not the language of the computer.
- `src/update/` reads the release list, the versions (SemVer, the channels of #129), the kind of
  installation and `SHA256SUMS.txt`, with no network.
- `tests/update_test.cpp` tests the check and the download with a local test server
  (127.0.0.1). It never calls GitHub.
- They share one `QNetworkAccessManager` (`UpdateController::m_network`, no cookie). It is not the
  manager of the QML engine.

The QML engine has no network access:

- `main.cpp` gives it `Startup::LocalOnlyNetworkFactory` (`src/startup/LocalOnlyNetwork.h`).
- Its network access (an `Image`, a `Loader`, an image in rich text) reads only local files and
  `qrc:` resources.
- An `http` or `https` address fails at once, with no connection.
- QML opens a `file:` address itself. On Windows, `file://server/...` is a network share.
- So the release notes, which are text from GitHub, go through `UpdateController::safeNotes()`
  before QML shows them. It removes their images, background images and style sheets.
- `openPage()` opens only the pages of the project on GitHub.

Do not add another network call without a decision of the maintainer. The app promises that the
data stays on the computer.

## Charts

### Before you use a Qt Graphs property

1. Find the property in [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples).
2. Read its page in the Qt Graphs documentation (https://doc.qt.io/qt-6/qtgraphs-index.html).
3. Do not guess a property name or a value.
4. If you are not sure how a feature works, stop and ask the maintainer.

### Hover or click feedback

Each chart must let the user read the exact values. A new chart without feedback needs a
reason.

| Series | Feedback |
|---|---|
| `LineSeries` | Hover tooltip through `pointDelegate` (`qml/components/HoverPoint.qml`). |
| `BarSeries` | Hover tooltip through `barDelegate` (`qml/components/HoverBar.qml`). |
| `AreaSeries` | It has no `pointDelegate`. Put a `LineSeries` on each layer border, with a `pointDelegate`. `HoverPoint.overrideValues` shows the value of the layer. |
| `PieSeries` | Click (`clicked(slice)`). Qt Graphs has no hover signal for pies. |

### Known Qt Graphs behaviour

- A `PieSeries` must set `selectable: true`. The default is `false`, and then Qt Graphs emits no
  `clicked` signal.
- In Qt Graphs 6.10, the hit test of a click ignores `holeSize`. A click in the hole of a donut
  gives a slice. When two `PieSeries` share one `GraphsView`, a click on one ring can come from
  the other series.
- The 2-level pies of Account Details are one `GraphsView` with two `PieSeries`.
  `showRingSliceInfo()` in `AccountHistoryPage.qml` finds the ring and the slice from the
  position of the press.
- The hole of a `PieSeries` is relative to its own radius: the inner edge is at
  `pieSize × holeSize`.
- Do not give a series the colour `"transparent"`. Qt Graphs replaces a colour with alpha 0 by
  the theme colour. This applies to `color` and to `borderColor`.
- A delegate gets the colour of its series as `pointColor`. A value that you give to
  `pointColor` does not stay.
- In a `barDelegate` or a `pointDelegate`, show the tooltip with a `ToolTip`
  (QtQuick.Controls), not with a child `Rectangle`. The delegates are siblings, the renderer
  sets their `z` on each update, and the plot area clips them. A `ToolTip` shows in the overlay
  of the window (`HoverBar.qml`, `HoverPoint.qml`).
- Do not use `font.family: "monospace"`. Windows has no font with that name. Use
  `Fonts.monospace`.
- A `DateTimeAxis` or `ValueAxis` with min equal to max stopped the app with a native crash.
  Add padding when the range is empty (example: `maxTimeMs` in `AccountHistoryPage.qml`).
- The `onCompleted` handler of a child runs before the handler of its parent. Fill a series from
  the handler of the page, not from the series.
- Axis labels showed `?` for `€`. Put the unit in the `ChartCard` title. Tooltips are QML `Text`
  and show `€`.
- Qt Graphs has no Sankey series. `qml/components/MoneyFlow.qml` draws it with a QML `Canvas`.

## Where to look

| Topic | Where |
|---|---|
| Navigation (`currentViewId`, sidebar, overlay pages) | `qml/Main.qml` (comments at the top) |
| Statement import, review and report | `src/controllers/dataentry/SnapshotImportController.h`, `qml/pages/ImportSnapshotPage.qml`, `src/import/` |
| Category prediction | `src/predict/CategoryPredictor.h`, `src/predict/LabelNormalizer.h` |
| Categories, accounts, account types | `src/controllers/dataentry/CategoryController.h`, `AccountController.h` |
| Database, schema, upgrades | `src/db/Database.h`, `src/db/tables/` |
| Database management (reset, demo data, clean-up) | `src/controllers/database/DatabaseManagementController.h` |
| Update check and installation | `src/controllers/updates/UpdateController.h`, `UpdateInstaller.h`, `src/update/`, `packaging/windows/update-restart.iss`, `README.md` § Updates |
| Start-up checks and the launcher | `src/startup/StartupChecks.h`, `launcher/` |
| Docker build and requirement check | `README.md` § Building with Docker, `.env.example` |
| Tests | `tests/CMakeLists.txt`. The app and the tests share `FD_CORE_SOURCES` in `CMakeLists.txt`. |
| Build, test, package and release pipeline | [`CI-CD.md`](CI-CD.md) |
| Version number and changelog | [`CI-CD.md`](CI-CD.md) § Working with the changelog |

## Other documents

- `README.md`: what the app is, how to install it, how to build and run it.
- `CI-CD.md`: how CI builds, tests, packages and releases the app. Read it before you change
  `.github/`, `packaging/`, `version.json` or the install part of `CMakeLists.txt`.
- `CONTRIBUTING.md`: branches, commit messages, labels and the definition of done. Read it
  before you make a branch or a pull request.
- `PROJECT.md`: the product specification (pages, charts, features). Read it before you add a
  feature.
- `TODO.md`: known limitations, bugs and clean-ups.
- `version.md`: the changelog. `tools/version.py` generates it from `version.json`. Do not edit
  it by hand.
- `LICENSE`: MIT. The Windows setup also shows it.
