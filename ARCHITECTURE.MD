# Architecture — Finance Dashboard

> **Audience:** developer / maintainer (Claude or human).
> This file is an index. Design decisions and file-by-file detail live under `docs/` — this
> page just tells you where to look. Keep this index (and the linked page) up to date when the
> structure changes; log the change itself in `version.md`.

## Project Identity

| Field | Value |
|---|---|
| Name | FinanceDashboard |
| App identities | Prod ("Finance Dashboard") and Test ("Finance Dashboard Test", `-DFD_APP_IDENTITY=test`): separate data folder, settings, installer identity, desktop entry, icon and window title — see `CMakeLists.txt` § App identity (#130). The Test app can copy the prod database ("Copy my prod database", Database Management page) |
| Version | Read from `version.json` — see `version.md` for the changelog |
| Language | C++17 |
| UI framework | Qt Quick / QML + Qt Graphs |
| Build system | CMake 3.21+ (`qt_add_qml_module`, `qt_generate_deploy_qml_app_script`); optional Docker build via `compose.yaml` + `.env` — see `README.md` § Building with Docker |
| Database | SQLite via Qt6::Sql |
| Target platform | Windows (primary), Linux/macOS compatible — Windows and Linux are built and packaged by CI |
| Reference project | [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples) — patterns for every chart type |

## Design rule (the one thing to remember)

**UI lives in QML. Algorithms, analytics, and data access live in C++.** Every page gets a
matching C++ *controller* class (`QML_ELEMENT`, in `src/controllers/<domain>/`) that holds all
logic; the `.qml` file only renders what the controller hands it. See `docs/backend.md` for the
full pattern and `docs/build-and-tooling.md` for the checklist to follow when adding a new
page.

## Data rule

Writes that belong together go in one SQL transaction, in one `Database` method. Example:
`Database::saveImport()` writes the rows of a statement, the account link and the balance
checkpoints; if one write fails, nothing is written. Qt binds a null `QString` as SQL `NULL`, and
the `DEFAULT` of a column does not apply to an explicit `NULL`. So bind the text of an optional
column (`TEXT NOT NULL DEFAULT ''`) through `Sql::text()` (`src/db/tables/SqlText.h`), which
writes `''`. A required value (a name, a currency, an account hash, a date) is not changed: the
`Database` method refuses an empty one before any write, with a message in `lastError()`
(`tests/db_write_test.cpp`). Check every result: a new transaction goes through
`Database::inTransaction()`, which checks `BEGIN` and `COMMIT` (the upgrade, `saveImport()` and
`resetAll()` check them in their own code), and a table class that cannot prepare its statement
logs the SQLite error (`Sql::logPrepareError()`, `src/db/tables/SqlPrepare.h`) and returns its
failure value.

Transactions and category rules keep the category and subcategory names as text, and accounts
keep their type and category names as text, not ids. So a rename changes those rows too, in the
same SQL transaction (`Database::renameCategory()`, `renameSubcategory()`, `renameAccountType()`,
`renameAccountCategory()`). A subcategory rename changes only the rows of its own category. The
controllers refuse a rename to a name that exists, in any case, as `add` does.

The controllers write their default data (categories, account types, account categories, demo
accounts) the first time that a database is used, and record it in the `settings` table, in one
SQL transaction (`Database::writeDefaults()`). So an empty table is not a first run, and a write
that fails leaves nothing: the next start tries again. "Reset Database" keeps the records, so
the default data does not come back, across restarts too.

The Test app replaces its database with a copy of the prod database through
`Database::replaceWithCopyOf()` ("Copy my prod database", #130). The prod file is opened read-only
and copied with `VACUUM INTO`, which includes its WAL; then the Test file is closed and kept
(`finance_dashboard.before-copy-<date>.db`), the copy takes its name, and `open()` opens it as at a
start, so it upgrades an older copy. A copy that does not open is kept
(`finance_dashboard.copy-failed-<date>.db`) and the Test file comes back. A Test file that another
connection uses (the Test app open twice) is not replaced (`tests/db_copy_test.cpp`). The read-only
connection can leave an empty `-wal` file and a `-shm` file next to the prod file; the prod app
removes them when it closes.

The schema has a version (`PRAGMA user_version`, `Database::kSchemaVersion`). `createTable()`
is the base schema and does not change; each change after it is one migration step in
`Database::upgrade()`, written with its own SQL, with a test in `tests/db_migration_test.cpp`.
Before it upgrades an existing database, the app saves a copy next to it (`VACUUM INTO`), before
any change. The base tables and every step run in one SQL transaction, so the file is either
upgraded or unchanged. A database that does not open (it cannot be read, a newer version saved
it, or the upgrade failed) closes the connection and stops the app at start-up, with the cause
(a fatal step in `StartupChecks`).

A transaction's `fingerprint` is its identity in the bank's statement
(`TransactionsTable::fingerprints()`): the bank's reference and the amount when the row has a
reference (`bankReference()`), else every field. A row that the account already has is not saved
again. Compute the fingerprints of a statement on all its rows, before you filter them: the
second identical row of a statement has occurrence 2 in its fingerprint. A skipped row with the
reference and amount of a saved row but another operation date is reported, with a link to the
issue in `TransactionsTable::kReusedReferenceIssueUrl`: the identity assumes one reference per
transaction.

## Network rule

The app makes one network call: the update check of `UpdateController`
(`src/controllers/updates/`, #117). At each start (unless the user turned it off) and on "Check
now", it reads the list of releases of the project (`GET api.github.com/repos/.../releases`), with
no token, no cookie and no data of the user. GitHub sees the internet address of the computer and
the version of the app (the `User-Agent` header); the request sends `Accept-Language: en`, not the
language of the computer. `src/update/` reads that list and the versions (SemVer, the channels of
#129) with no network. `tests/update_test.cpp` tests the check with a local test server
(127.0.0.1), never with GitHub.

The QML engine has no network access: `main.cpp` gives it `Startup::LocalOnlyNetworkFactory`
(`src/startup/LocalOnlyNetwork.h`), so its network access (an `Image`, a `Loader`, an image in
rich text) reads local files and `qrc:` resources only. An `http`/`https` address fails at once,
with no connection. QML opens a `file:` address itself, and on Windows `file://server/...` is a
network share. So the release notes, text from GitHub, go through `UpdateController::safeNotes()`
before QML shows them: it removes their images, background images and style sheets. The update
check has its own `QNetworkAccessManager` (no cookie), not the one of the engine. `openPage()`
opens the pages of the project on GitHub only.

Any other network call needs a decision of the maintainer first: the app promises that the data
never leaves the computer.

## Where to look

| Topic | Doc |
|---|---|
| Full directory layout & file responsibilities | [`docs/directory-map.md`](docs/directory-map.md) |
| Sidebar navigation / `currentViewId` pattern | [`docs/navigation.md`](docs/navigation.md) |
| Shared QML component library | [`docs/shared-components.md`](docs/shared-components.md) |
| Statement import (parsing, reconciliation, report) | [`docs/statement-import.md`](docs/statement-import.md) |
| Category management | [`docs/category-management.md`](docs/category-management.md) |
| Category prediction (auto-fill on import) | [`docs/category-prediction.md`](docs/category-prediction.md) |
| Account management (accounts, hashes, types/categories) | [`docs/account-management.md`](docs/account-management.md) |
| Balance checkpoints | [`docs/balance-checkpoints.md`](docs/balance-checkpoints.md) |
| Account History page | [`docs/account-history.md`](docs/account-history.md) |
| Transactions sidebar category | [`docs/transactions.md`](docs/transactions.md) |
| Database management (reset/reseed/per-account cleanup) | [`docs/database-management.md`](docs/database-management.md) |
| Update check: Updates page, banner, channels, versions | `src/controllers/updates/UpdateController.h` (class comment), `src/update/`, `src/startup/LocalOnlyNetwork.h` (no network for QML), `README.md` § Updates |
| QML/C++ boundary pattern & C++ component responsibilities | [`docs/backend.md`](docs/backend.md) |
| Styling / Catppuccin palette | [`docs/styling.md`](docs/styling.md) |
| Qt Graphs pitfalls learned on this project | [`docs/charting-gotchas.md`](docs/charting-gotchas.md) |
| Build, static analysis, new-page checklist | [`docs/build-and-tooling.md`](docs/build-and-tooling.md) |
| Docker build (any machine; Windows/Linux targets, shared/static Qt), pre-build requirement check, build logs | [`README.md`](README.md) § Building with Docker; options in `.env.example` |
| Start-up: launcher + in-app requirement checks / initialization (`src/startup/`) | Header comment of `src/startup/StartupChecks.h`; `launcher/` |
| Page/controller status (MOCK vs. REAL) | [`docs/app-status.md`](docs/app-status.md) |
| Automated documentation generation | [`docs/automated-docs.md`](docs/automated-docs.md) |
| Auto-generated file listing (regenerate, don't hand-edit) | [`docs/generated/source-tree.md`](docs/generated/source-tree.md) |
| Build, test, package and release pipeline | [`CI-CD.md`](CI-CD.md) |
| Tests (`ctest`): category predictor, statement-import save path, schema upgrades, database writes, copy of the prod database, update check, page calculations | `tests/CMakeLists.txt` (one comment for each test); the app and the tests share `FD_CORE_SOURCES` in `CMakeLists.txt` |
| Version number & changelog (single source of truth) | [`CI-CD.md`](CI-CD.md) § Working with the changelog |

## Other top-level docs

- `README.md` — what the app is, where to download it, and how to build/run it.
- `CI-CD.md` — how the app is built, tested, packaged and released. Read this before touching
  `.github/`, `packaging/`, `version.json` or the install/CPack half of `CMakeLists.txt`.
- `CONTRIBUTING.md` — gitflow branches, commit messages, labels and the definition of done.
  Read it before you create a branch or a pull request.
- `PROJECT.md` — product spec (target pages, charts, features). Check before adding features.
- `TODO.md` — backlog: known limitations, bugs, cleanups.
- `version.md` — changelog (current version + every previous one). **Generated** from
  `version.json`; edit that file, or run `python tools/version.py add <Section> "<text>"`.
- `LICENSE` — MIT. Also feeds the Windows installer's licence page.

## Before inventing a Qt Graphs property or QML API

Check [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples) and
https://doc.qt.io/qt-6/qtgraphs-index.html first. Never guess a property name or value — see
`docs/charting-gotchas.md` for pitfalls already found the hard way. If you're not sure how
something should work, stop and ask rather than guessing.

## Charts should have hover (or click) feedback

Every chart in this app should let the user get exact values back out of it, not just read the
shape of the data — `LineSeries`/`BarSeries` via a hover tooltip (`pointDelegate`/`barDelegate`,
see `docs/charting-gotchas.md`), `PieSeries` via `onClicked` (it has no hover signal at all —
confirmed against Qt's own C++ class docs, not assumed). Treat a new chart with neither as the
exception that needs a reason, not the default.
