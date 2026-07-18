# Build, Static Analysis & Adding a New Page

> Part of the architecture docs — see `ARCHITECTURE.MD` for the index.
> For plain build/run steps, see `README.md` — this page covers maintainer-level tooling.

## Creating a New Sidebar Page (checklist)

Written to lean on `docs/shared-components.md` / `docs/navigation.md` so a new page starts from
the app's existing patterns instead of a blank file:

1. Create `src/controllers/<domain>/XxxController.h/.cpp` (`QML_ELEMENT`; `QML_SINGLETON` only
   if the data is genuinely cross-cutting — see `docs/category-management.md` for the
   reasoning). Pick the subfolder matching the page/domain (add a new one if it's a new
   domain), and add that subfolder to `target_include_directories` in `CMakeLists.txt`. Follow
   the `*DataProvider` pattern in `docs/backend.md`: QML never computes, it only renders
   `Q_PROPERTY`/`Q_INVOKABLE QVariantList` output.
2. Create `qml/pages/XxxPage.qml` — rendering only. Start from `PageHeader` for the title row,
   `ChartCard`/`KpiCard` for chart/stat sections, and `PillButton`/`GhostButton`/`StatusBadge`/
   `FormDialog`/`FormRow`/`ColorPickerRow`/`ConfirmDialog`/`EmptyState` for any CRUD/dialog/list
   UI — see `docs/shared-components.md` for what each one covers before hand-rolling a button or
   badge.
3. Register both in `CMakeLists.txt` (`SOURCES` + `QML_FILES` of `qt_add_qml_module`).
4. Wire it into navigation (see `docs/navigation.md`): append `{ id, label, actions }` to
   `Main.qml`'s `categories` array and add the page as the next `StackLayout` child in the same
   position (for a top-level category page), or add a `currentViewId`-gated `Loader` +
   `Component` pair following the existing overlay pattern (for an action-triggered page).
5. For charts: copy the closest pattern from
   [QtGraphsExamples](https://github.com/nicolas-goyon/QtGraphsExamples); verify any new
   property against https://doc.qt.io/qt-6/qtgraphs-index.html — **never invent property
   names**. See `docs/charting-gotchas.md` for pitfalls already found.
6. Update `docs/app-status.md`, the relevant `docs/*.md` file, and `version.md`.

## Static Analysis

CLI-only, text output (no HTML/graph reports). Toolchain context: this project builds with
CLion's bundled MinGW g++ against Qt 6.11.1 mingw_64 (see
`cmake-build-debug/CMakeCache.txt`), Ninja generator, `compile_commands.json` already generated
at `cmake-build-debug/compile_commands.json`.

### clang-tidy

Config: `.clang-tidy` at the project root (comprehensive/aggressive check set —
`bugprone-*`, `cert-*`, `clang-analyzer-*`, `concurrency-*`, `cppcoreguidelines-*`, `misc-*`,
`modernize-*`, `performance-*`, `portability-*`, `readability-*` — with only the non-Qt/
non-relevant check families and two documented Qt-friction checks disabled; see the comment
block at the top of that file for the reasoning). Two ways to run it:

1. **Build-time (opt-in):** `option(ENABLE_CLANG_TIDY)` in `CMakeLists.txt`, OFF by default.
   `cmake -B cmake-build-debug -DENABLE_CLANG_TIDY=ON` then build normally — clang-tidy runs
   alongside the compiler (`CXX_CLANG_TIDY` target property,
   https://cmake.org/cmake/help/latest/prop_tgt/LANG_CLANG_TIDY.html) and prints warnings
   inline in the build log. Generated files (AUTOMOC/AUTORCC output, qml type-registration
   .cpp) are silenced via an all-checks-off `.clang-tidy` written into the build directory root
   at configure time (a workaround — clang-tidy's nearest-directory config lookup, not an
   official Qt/CMake feature).
2. **On-demand:** `tools/run-clang-tidy.ps1` — reads `compile_commands.json`, runs clang-tidy on
   every `src/` file, plain text to the console. `./tools/run-clang-tidy.ps1 -BuildDir
   cmake-build-debug`.

**Install (Windows):** clang-tidy isn't bundled with the MinGW toolchain this project builds
with. Install LLVM (ships `clang-tidy.exe`) — official installer at https://releases.llvm.org,
or `winget install LLVM.LLVM` — and ensure its `bin/` is on PATH. clang-tidy sometimes needs
`--extra-arg=-Wno-unknown-warning-option` when fed a GCC/MinGW-flavored compile command, since
GCC accepts flags clang doesn't recognize.

### clazy

**Not set up.** clazy's own README states it's only tested on Windows with MSVC — MinGW is
unsupported ("Problems building clazy with MinGW" is a known open issue). A prebuilt
`clazy-standalone.exe` exists via MSYS2's `mingw-w64-clang-x86_64-clazy` package (independent of
this project's MinGW build), which is the path to take if Qt-specific checks (signal/slot
misuse, needless `detach()`, etc.) are wanted later.

### Running everything at once

`tools/run-static-analysis.ps1` runs every analyzer set up above (currently clang-tidy +
qmllint; clazy is skipped) and logs each one's full text output to its own file under `logs/`
(gitignored) — `logs/clang-tidy.log`, `logs/qmllint.log` — while still streaming to the
console. Prints a one-line clean/issues-found summary per analyzer at the end.

```powershell
./tools/run-static-analysis.ps1
# custom build dir / log dir:
./tools/run-static-analysis.ps1 -BuildDir cmake-build-debug -LogDir logs
```

### qmllint

**Already wired — no setup needed.** `qt_add_qml_module` in `CMakeLists.txt` auto-creates a
`FinanceDashboard_qmllint` target (and an `all_qmllint` convenience target) for every
`QML_FILES` entry. Run it with:

```
cmake --build cmake-build-debug --target FinanceDashboard_qmllint
```

Plain text warnings to the console. (Source: https://doc.qt.io/qt-6/qt-add-qml-module.html —
"a separate linting target will be automatically created if any .qml files are added ... unless
the NO_LINT option is given.")
