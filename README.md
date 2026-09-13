# Finance Dashboard

A desktop finance analytics application built with **C++17**, **Qt Quick / QML**, and **Qt
Graphs**. It's a personal finance dashboard: import bank statements, categorize transactions,
track accounts, and (as the analytics layer is wired up) see net worth, cash flow, savings, and
investment trends over time — all data stays local in a SQLite file, with no bank connectivity
or external data fetching.

For what the app is trying to do product-wise, see `PROJECT.md`. For how it's built, see
`ARCHITECTURE.MD`. For how it's tested, packaged and released, see `CI-CD.md`. For the current
backlog and version history, see `TODO.md` and `version.md`.

---

## Install (no build required)

Grab the latest [release](https://github.com/nicolas-goyon/FinanceDashboard/releases) and pick
one file. Qt travels inside every package — nothing else to install.

| Platform | File | What you do |
|---|---|---|
| Windows | `FinanceDashboard-<version>-windows-x64.msi` | Double-click. Start-menu shortcut and uninstaller included. |
| Windows (portable) | `…-windows-x64.zip` | Unzip, run `FinanceDashboard.exe`. |
| Linux | `FinanceDashboard-<version>-linux-x86_64.AppImage` | `chmod +x`, run. |
| Linux (Debian/Ubuntu) | `finance-dashboard_<version>_amd64.deb` | `sudo apt install ./…deb` |
| Linux (portable) | `…-linux-x86_64.tar.gz` | Extract, run `bin/FinanceDashboard`. |

Releases are cut automatically from `main` — see `CI-CD.md`. Linux builds need glibc 2.39+
(Ubuntu 24.04+, Debian 13+, Fedora 40+); on anything older, build from source below.

Your data lives in one local SQLite file and never leaves the machine:
`%APPDATA%\DataAnalysis\FinanceDashboard\` on Windows,
`~/.local/share/DataAnalysis/FinanceDashboard/` on Linux.

---

## Prerequisites

| Tool | Version |
|---|---|
| Qt | 6.8+ (developed against **6.11**) — include the **Qt Graphs** and **Qt Quick** modules |
| CMake | 3.21+ (3.30+ only if you want to build the Windows `.msi` yourself) |
| C++ compiler | MSVC 2022 (recommended on Windows) or MinGW 13+ |

Qt Charts and Qt Widgets are not required.

---

## Building & Running

### 1. Configure

```powershell
# From the FinanceDashboard directory
cmake -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"
```

### 2. Build

```powershell
cmake --build build --config Release
```

### 3. Run

```powershell
.\build\Release\FinanceDashboard.exe
```

Or open the project in **Qt Creator** (`File → Open File or Project → CMakeLists.txt`) and
press **Run**.

There is no manual `windeployqt` step any more. To produce a copy that runs on a machine with
no Qt installed, install the project instead — `qt_generate_deploy_qml_app_script()` pulls in
every Qt library, plugin and QML module the binary actually uses:

```powershell
cmake --install build --config Release --prefix stage
```

That `stage/` directory is exactly what the installers and the AppImage are built from. The
version number comes from `version.json`, not from `CMakeLists.txt` — bump it with
`python tools/version.py bump`.

### Tests

```bash
cmake -B build -DFD_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Project layout, at a glance

```
FinanceDashboard/
├── qml/        — UI only (QML). No business logic.
├── src/        — Controllers, database layer, models, statement-import templates (all C++).
├── tests/      — CTest registration for the predictor self-test harness.
├── tools/      — Static-analysis runners, the doc generator, and version.py (version/changelog).
├── packaging/  — App icon, Linux .desktop/AppStream metadata, AppImage and .deb build scripts.
└── .github/    — The CI/CD pipeline. See CI-CD.md.
```

---

## Documentation

- **`ARCHITECTURE.MD`** — start here for anything design-related.
- **`CI-CD.md`** — the build/test/package/release pipeline, and how to change it.
- **`PROJECT.md`** — product spec: target pages, charts, features.
- **`TODO.md`** — known limitations, bugs, and planned work.
- **`version.md`** — changelog. Generated from `version.json`; edit that, or use
  `python tools/version.py add <Section> "<text>"`.

## Changelog and version

The version number and the changelog live in one place, `version.json`:

```bash
python tools/version.py current                       # what's in progress
python tools/version.py add Fixed "…what you fixed."  # log it as you go
python tools/version.py check                         # what CI enforces
```

`CMakeLists.txt` reads that version, so it is also the app's version, the installer's version
and the release tag's version. Never type a version number anywhere else.

## Static analysis

clang-tidy and qmllint run on every push (as non-blocking jobs — see `CI-CD.md`). Locally:

```powershell
./tools/run-static-analysis.ps1
```

`tools/generate_docs.py` regenerates a file-by-file source listing from each file's banner
comment. It writes into `docs/`, which this project no longer keeps, so it is not wired into
CI; run it by hand if you want the listing back.

Chart patterns come from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples)
— always check that project and https://doc.qt.io/qt-6/qtgraphs-index.html before adding a new
chart property.
