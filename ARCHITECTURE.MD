# Architecture — Finance Dashboard

> **Audience:** developer / maintainer (Claude or human).
> This file is an index. Design decisions and file-by-file detail live under `docs/` — this
> page just tells you where to look. Keep this index (and the linked page) up to date when the
> structure changes; log the change itself in `version.md`.

## Project Identity

| Field | Value |
|---|---|
| Name | FinanceDashboard |
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
checkpoints; if one write fails, nothing is written. Bind the text of a `TEXT NOT NULL` column
through `Sql::text()` (`src/db/tables/SqlText.h`), because Qt binds a null `QString` as SQL
`NULL`.

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
| Tests (`ctest`): category predictor, statement-import save path | `tests/CMakeLists.txt`; the app and the tests share `FD_CORE_SOURCES` in `CMakeLists.txt` |
| Version number & changelog (single source of truth) | [`CI-CD.md`](CI-CD.md) § Working with the changelog |

## Other top-level docs

- `README.md` — what the app is, where to download it, and how to build/run it.
- `CI-CD.md` — how the app is built, tested, packaged and released. Read this before touching
  `.github/`, `packaging/`, `version.json` or the install/CPack half of `CMakeLists.txt`.
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
