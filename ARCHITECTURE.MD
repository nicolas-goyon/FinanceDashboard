# Architecture — Finance Dashboard

> **Audience:** developer / maintainer (Claude or human).
> This file is an index. Design decisions and file-by-file detail live under `docs/` — this
> page just tells you where to look. Keep this index (and the linked page) up to date when the
> structure changes; log the change itself in `version.md`.

## Project Identity

| Field | Value |
|---|---|
| Name | FinanceDashboard |
| Version | 0.2.1 — see `version.md` for the changelog |
| Language | C++17 |
| UI framework | Qt Quick / QML + Qt Graphs |
| Build system | CMake 3.20+ (`qt_add_qml_module`) |
| Database | SQLite via Qt6::Sql |
| Target platform | Windows (primary), Linux/macOS compatible |
| Reference project | [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples) — patterns for every chart type |

## Design rule (the one thing to remember)

**UI lives in QML. Algorithms, analytics, and data access live in C++.** Every page gets a
matching C++ *controller* class (`QML_ELEMENT`, in `src/controllers/<domain>/`) that holds all
logic; the `.qml` file only renders what the controller hands it. See `docs/backend.md` for the
full pattern and `docs/build-and-tooling.md` for the checklist to follow when adding a new
page.

## Where to look

| Topic | Doc |
|---|---|
| Full directory layout & file responsibilities | [`docs/directory-map.md`](docs/directory-map.md) |
| Sidebar navigation / `currentViewId` pattern | [`docs/navigation.md`](docs/navigation.md) |
| Shared QML component library | [`docs/shared-components.md`](docs/shared-components.md) |
| Statement import (parsing, reconciliation, report) | [`docs/statement-import.md`](docs/statement-import.md) |
| Category management | [`docs/category-management.md`](docs/category-management.md) |
| Account management (accounts, hashes, types/categories) | [`docs/account-management.md`](docs/account-management.md) |
| Balance checkpoints | [`docs/balance-checkpoints.md`](docs/balance-checkpoints.md) |
| Account History page | [`docs/account-history.md`](docs/account-history.md) |
| Transactions sidebar category | [`docs/transactions.md`](docs/transactions.md) |
| Database management (reset/reseed/per-account cleanup) | [`docs/database-management.md`](docs/database-management.md) |
| QML/C++ boundary pattern & C++ component responsibilities | [`docs/backend.md`](docs/backend.md) |
| Styling / Catppuccin palette | [`docs/styling.md`](docs/styling.md) |
| Qt Graphs pitfalls learned on this project | [`docs/charting-gotchas.md`](docs/charting-gotchas.md) |
| Build, static analysis, new-page checklist | [`docs/build-and-tooling.md`](docs/build-and-tooling.md) |
| Page/controller status (MOCK vs. REAL) | [`docs/app-status.md`](docs/app-status.md) |
| Automated documentation generation | [`docs/automated-docs.md`](docs/automated-docs.md) |
| Auto-generated file listing (regenerate, don't hand-edit) | [`docs/generated/source-tree.md`](docs/generated/source-tree.md) |

## Other top-level docs

- `README.md` — what the app is and how to build/run it.
- `PROJECT.md` — product spec (target pages, charts, features). Check before adding features.
- `TODO.md` — backlog: known limitations, bugs, cleanups.
- `version.md` — changelog (current version + every previous one).

## Before inventing a Qt Graphs property or QML API

Check [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples) and
https://doc.qt.io/qt-6/qtgraphs-index.html first. Never guess a property name or value — see
`docs/charting-gotchas.md` for pitfalls already found the hard way. If you're not sure how
something should work, stop and ask rather than guessing.
