# Finance Dashboard

A desktop finance analytics application built with **C++17**, **Qt Quick / QML**, and **Qt
Graphs**. It's a personal finance dashboard: import bank statements, categorize transactions,
track accounts, and (as the analytics layer is wired up) see net worth, cash flow, savings, and
investment trends over time — all data stays local in a SQLite file, with no bank connectivity
or external data fetching.

For what the app is trying to do product-wise, see `PROJECT.md`. For how it's built, see
`ARCHITECTURE.MD` (which indexes into `docs/`). For the current backlog and version history,
see `TODO.md` and `version.md`.

---

## Prerequisites

| Tool | Version |
|---|---|
| Qt | 6.8+ (developed against **6.11**) — include the **Qt Graphs** and **Qt Quick** modules |
| CMake | 3.20+ |
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

### 3. Deploy Qt DLLs (first run only)

```powershell
cd build/Release
windeployqt.exe --qmldir ../../qml FinanceDashboard.exe
```

The `--qmldir` flag is required so the QML and Qt Graphs runtime files are deployed.

### 4. Run

```powershell
.\build\Release\FinanceDashboard.exe
```

Or open the project in **Qt Creator** (`File → Open File or Project → CMakeLists.txt`) and
press **Run**.

---

## Project layout, at a glance

```
FinanceDashboard/
├── qml/        — UI only (QML). No business logic.
├── src/        — Controllers, database layer, models, statement-import templates (all C++).
├── docs/       — Design-decision detail, split by topic (see ARCHITECTURE.MD's index).
└── tools/      — PowerShell static-analysis runners + the doc-generation script below.
```

The full breakdown (every file and what it's for) lives in
[`docs/directory-map.md`](docs/directory-map.md), not here — see `ARCHITECTURE.MD` for the
complete documentation index.

---

## Documentation & static analysis

- **`ARCHITECTURE.MD`** — start here for anything design-related; it indexes into `docs/*.md`.
- **`docs/build-and-tooling.md`** — clang-tidy / qmllint setup and the "add a new page"
  checklist.
- **`TODO.md`** — known limitations, bugs, and planned work.
- **`version.md`** — changelog (current version + history).

### Auto-generated docs

`docs/generated/source-tree.md` (a file-by-file listing with descriptions) is generated from
the source tree rather than hand-maintained. Regenerate it after adding, removing, or renaming
a file under `src/` or `qml/`:

```bash
python tools/generate_docs.py          # writes docs/generated/source-tree.md
python tools/generate_docs.py --check  # exit 1 if the file is stale, no write (for CI)
```

It works by reading each file's existing banner comment (the `// ── Name ── ... description`
block most files already start with) — no Python dependencies, no new comment convention to
adopt on files that already follow that style. See
[`docs/automated-docs.md`](docs/automated-docs.md) for the exact convention, current
limitations, and how to extend it.

Static analysis (clang-tidy + qmllint) is likewise scripted rather than manual — see
`docs/build-and-tooling.md` for setup, or just run:

```powershell
./tools/run-static-analysis.ps1
```

Chart patterns come from [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples)
— always check that project and https://doc.qt.io/qt-6/qtgraphs-index.html before adding a new
chart property.
