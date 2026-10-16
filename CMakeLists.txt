cmake_minimum_required(VERSION 3.21)

# ── Version — single source of truth is version.json ─────────────────────────
# The version is NOT typed here. version.json's "current" field is read below
# and fed straight into project(... VERSION ...), so the build, the installers,
# the release tag and the changelog can never drift apart. Bump it with
# `python tools/version.py bump` (CI does this automatically on every release —
# see CI-CD.md), never by editing a number in this file.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.json" FD_VERSION_JSON)
string(JSON FD_VERSION GET "${FD_VERSION_JSON}" current)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/version.json")

project(FinanceDashboard VERSION ${FD_VERSION} LANGUAGES CXX)
message(STATUS "FinanceDashboard version ${PROJECT_VERSION} (from version.json)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Windows installs/portable ZIPs are flat: FinanceDashboard.exe sits at the root
# of the install directory with its DLLs beside it, the way every other Windows
# app does it. On Linux the FHS-ish bin/ + lib/ split is what the AppImage and
# the .deb expect, so GNUInstallDirs' defaults stand.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
endif()

# Qt Quick / QML + Qt Graphs (2D). Qt Charts and Widgets are no longer used.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Graphs Sql QuickControls2)

qt_standard_project_setup(REQUIRES 6.10)

# QTP0004: auto-generate qmldir for QML files in subdirectories
qt_policy(SET QTP0004 NEW)

# ── Executable — entry point + UI-independent C++ (db / models / utils) ───────
qt_add_executable(FinanceDashboard
    src/main.cpp
    # Start-up stage: requirement checks + initialization, run before the main
    # window loads (see docs/startup.md).
    src/startup/StartupChecks.h
    src/startup/StartupChecks.cpp
    src/models/AssetEntry.h
    src/models/AssetEntry.cpp
    src/models/ImportedStatement.h
    src/models/ImportLog.h
    src/models/Category.h
    src/models/Account.h
    src/models/Transaction.h
    src/models/CategoryRule.h
    src/db/tables/SnapshotsTable.h
    src/db/tables/SnapshotsTable.cpp
    src/db/tables/AssetEntriesTable.h
    src/db/tables/AssetEntriesTable.cpp
    src/db/tables/CategoriesTable.h
    src/db/tables/CategoriesTable.cpp
    src/db/tables/SubcategoriesTable.h
    src/db/tables/SubcategoriesTable.cpp
    src/db/tables/AccountsTable.h
    src/db/tables/AccountsTable.cpp
    src/db/tables/AccountHashesTable.h
    src/db/tables/AccountHashesTable.cpp
    src/db/tables/AccountCategoriesTable.h
    src/db/tables/AccountCategoriesTable.cpp
    src/db/tables/AccountTypesTable.h
    src/db/tables/AccountTypesTable.cpp
    src/db/tables/TransactionsTable.h
    src/db/tables/TransactionsTable.cpp
    src/db/tables/AccountBalanceCheckpointsTable.h
    src/db/tables/AccountBalanceCheckpointsTable.cpp
    src/db/tables/CategoryRulesTable.h
    src/db/tables/CategoryRulesTable.cpp
    src/db/Database.h
    src/db/Database.cpp
    src/utils/Formatter.h
    src/utils/AccountHashing.h
    # Category prediction (see docs/category-prediction.md). UI-independent
    # and Qt-Core-only — no QML_ELEMENT, no QObject; SnapshotImportController
    # below owns an instance and does the QML plumbing.
    src/predict/LabelNormalizer.h
    src/predict/LabelNormalizer.cpp
    src/predict/CategoryPredictor.h
    src/predict/CategoryPredictor.cpp
    # Statement-import template system (see ARCHITECTURE.MD § Statement Import).
    # UI-independent — no QML_ELEMENT here, that's SnapshotImportController below.
    src/import/ImportTemplate.h
    src/import/ImportTemplateRegistry.h
    src/import/ImportTemplateRegistry.cpp
    src/import/SumeriaCcpCsvExtractionTemplate.h
    src/import/SumeriaCcpCsvExtractionTemplate.cpp
)

# NOTE — tools/predictor_selftest.cpp is deliberately NOT listed above. It has
# its own main() and is a standalone sanity harness for src/predict/; adding it
# here would give the app two entry points. It is no longer hand-run only:
# tests/CMakeLists.txt builds it as its own target and registers it with CTest
# (`cmake -DFD_BUILD_TESTS=ON`, then `ctest`), and CI runs it on every push.
# Turning it into a proper Qt Test case is still the plan — see TODO.md.

# ── QML module — controllers (QML_ELEMENT) + QML UI ───────────────────────────
qt_add_qml_module(FinanceDashboard
    URI FinanceDashboard
    VERSION 1.0
    SOURCES
        # PROJECT.md pages (mock data — UI pass)
        # One subfolder per page/domain under src/controllers/ (see ARCHITECTURE.MD § Directory Map).
        src/controllers/overview/NetWorthController.h
        src/controllers/overview/NetWorthController.cpp
        src/controllers/cashflow/CashFlowController.h
        src/controllers/cashflow/CashFlowController.cpp
        src/controllers/savings/SavingsController.h
        src/controllers/savings/SavingsController.cpp
        src/controllers/investments/InvestmentsController.h
        src/controllers/investments/InvestmentsController.cpp
        src/controllers/projects/ProjectsController.h
        src/controllers/projects/ProjectsController.cpp
        src/controllers/dataentry/SnapshotImportController.h
        src/controllers/dataentry/SnapshotImportController.cpp
        src/controllers/dataentry/CategoryController.h
        src/controllers/dataentry/CategoryController.cpp
        src/controllers/dataentry/AccountController.h
        src/controllers/dataentry/AccountController.cpp
        src/controllers/database/DatabaseManagementController.h
        src/controllers/database/DatabaseManagementController.cpp
    QML_FILES
        qml/Main.qml
        qml/StartupErrorWindow.qml
        qml/components/KpiCard.qml
        qml/components/ChartCard.qml
        qml/components/HoverPoint.qml
        qml/components/HoverBar.qml
        qml/components/MoneyFlow.qml
        qml/components/SidebarCategory.qml
        qml/components/ColorComboBox.qml
        qml/components/PageHeader.qml
        qml/components/PillButton.qml
        qml/components/GhostButton.qml
        qml/components/StatusBadge.qml
        qml/components/ColumnHeaderCell.qml
        qml/components/FormRow.qml
        qml/components/ColorPickerRow.qml
        qml/components/FormDialog.qml
        qml/components/ConfirmDialog.qml
        qml/components/EmptyState.qml
        qml/pages/OverviewPage.qml
        qml/pages/CashFlowPage.qml
        qml/pages/SavingsPage.qml
        qml/pages/InvestmentsPage.qml
        qml/pages/ProjectsPage.qml
        qml/pages/TransactionsManagementPage.qml
        qml/pages/AccountManagementLandingPage.qml
        qml/pages/DatabaseManagementPage.qml
        qml/pages/ImportSnapshotPage.qml
        qml/pages/CategoriesManagePage.qml
        qml/pages/AccountsManagePage.qml
        qml/pages/AccountTypesCategoriesPage.qml
        qml/pages/AccountTransactionsPage.qml
        qml/pages/AccountHistoryPage.qml
)

# NOTE — qml/pages/ImportSnapshotPage.qml (FileDialog) and
# qml/pages/CategoriesManagePage.qml / qml/pages/AccountsManagePage.qml
# (ColorDialog) import QtQuick.Dialogs, used only from QML (no C++ API call),
# so qt_add_qml_module's import scanning
# should resolve/deploy it automatically without an explicit find_package/link
# step — same as other QML-only modules here. If the build fails to find the
# QtQuick.Dialogs plugin, add "QuickDialogs2" to the find_package(Qt6 ...) line
# above and Qt6::QuickDialogs2 to target_link_libraries() below.
#
# Same reasoning applies to qml/pages/ImportSnapshotPage.qml's `import QtCore`
# (the Settings QML type, https://doc.qt.io/qt-6/qml-qtcore-settings.html,
# used to persist the transaction table's column widths — see ARCHITECTURE.MD
# § Statement Import). QtCore's QML plugin ("Qt Qml Core") ships as part of
# qtdeclarative alongside QtQuick/QtQml, no separate find_package component
# needed — same auto-resolution as QtQuick.Dialogs above.

# Each active controller's subfolder must be on the include path so the
# auto-generated qml type registration .cpp can find the headers by basename.
target_include_directories(FinanceDashboard PRIVATE
    src
    src/controllers/overview
    src/controllers/cashflow
    src/controllers/savings
    src/controllers/investments
    src/controllers/projects
    src/controllers/dataentry
    src/controllers/database
)

target_link_libraries(FinanceDashboard PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Graphs
    Qt6::Sql
    Qt6::QuickControls2
)

target_compile_definitions(FinanceDashboard PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
    # main.cpp passes this to QGuiApplication::setApplicationVersion() so the
    # running app reports the same number as version.json / the installer /
    # the release tag. Never hard-code a version string in C++.
    FD_VERSION_STRING="${PROJECT_VERSION}"
)

# Windows: hide the console window in Release builds
if(WIN32)
    set_target_properties(FinanceDashboard PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()

# ── Launcher (Windows) ─────────────────────────────────────────────────────────
# Tiny Win32 program with NO Qt dependency, linked statically, that checks the
# app's requirements (app/requirements.txt, OS version) before starting it, and
# reports loader failures (missing DLL) the app itself can't. The Docker build
# ships it as dist/<variant>/FinanceDashboard.exe next to app/ — see
# docs/startup.md. (On Linux the launcher is a shell script,
# launcher/linux/FinanceDashboard.sh, so nothing to build.)
if(WIN32)
    add_executable(FinanceDashboardLauncher WIN32 launcher/windows/main.cpp)
    set_target_properties(FinanceDashboardLauncher PROPERTIES
        # Not a Qt target — keep qt_standard_project_setup()'s AUTOMOC etc. off it.
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
        # MSVC: static C runtime (https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html)
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    # MinGW / llvm-mingw: link the C++ runtime statically → no DLL besides Windows' own.
    target_link_options(FinanceDashboardLauncher PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-static>
    )
    target_link_libraries(FinanceDashboardLauncher PRIVATE user32)
endif()

# ── Static analysis — clang-tidy (opt-in) ──────────────────────────────────
# OFF by default so normal dev builds stay fast. Enable with:
#   cmake -B cmake-build-debug -DENABLE_CLANG_TIDY=ON
# Checks are configured in .clang-tidy at the project root — see
# ARCHITECTURE.MD § Static Analysis. For a one-off run without reconfiguring,
# use tools/run-clang-tidy.ps1 instead (works regardless of this option).
option(ENABLE_CLANG_TIDY "Run clang-tidy on FinanceDashboard sources during the build (uses CMAKE_<LANG>_CLANG_TIDY, see https://cmake.org/cmake/help/latest/prop_tgt/LANG_CLANG_TIDY.html)" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        message(STATUS "ENABLE_CLANG_TIDY is ON — using ${CLANG_TIDY_EXE}")
        set_target_properties(FinanceDashboard PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
        )
        # Silence clang-tidy for generated files (AUTOMOC/AUTORCC output,
        # the qml type-registration .cpp, qmltyperegistrations, etc.) by
        # dropping an all-checks-off .clang-tidy at the build directory
        # root. clang-tidy walks up from the file being linted and uses the
        # nearest .clang-tidy it finds, so anything physically inside the
        # build tree hits this before the project-root .clang-tidy. This is
        # a workaround (not an official Qt/CMake mechanism) — regenerated on
        # every configure so it survives a clean build dir.
        file(WRITE "${CMAKE_BINARY_DIR}/.clang-tidy" "Checks: '-*'\n")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy was not found on PATH — install LLVM (includes clang-tidy) and re-configure. See ARCHITECTURE.MD § Static Analysis.")
    endif()
endif()

# ── Tests — ctest (opt-in) ─────────────────────────────────────────────────
# OFF by default so a normal IDE build stays exactly as fast as before. CI
# turns it on (`cmake -DFD_BUILD_TESTS=ON`) and runs `ctest`. See tests/.
option(FD_BUILD_TESTS "Build the test suite and register it with CTest" OFF)
if(FD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Install — a self-contained, runnable directory ────────────────────────────
# `cmake --install <build> --prefix <dir>` produces a directory that runs on a
# machine with no Qt installed: the app, every Qt library it actually links,
# the Qt plugins, the QML modules and a qt.conf that points at them.
#
# Qt does the hard part. qt_generate_deploy_qml_app_script() emits a CMake
# script that runs at install time and scans the built binary for its real
# runtime dependencies:
#   Windows — drives windeployqt (DLLs + plugins + QML, flat next to the .exe)
#   Linux   — file(GET_RUNTIME_DEPENDENCIES) into lib/, plugins/ and qml/,
#             with RPATH rewritten to $ORIGIN/../lib, skipping system libraries
# https://doc.qt.io/qt-6/cmake-deployment.html
#
# Everything the CI packages (MSI, portable ZIP, AppImage, .deb, .tar.gz) is
# built from this one install tree — there is no second, packaging-only copy of
# the "which files ship" question. See CI-CD.md.
install(TARGETS FinanceDashboard
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Belt and braces on Linux: the deploy script rewrites RPATH itself, but saying
# it here too means the installed binary finds ../lib even if the scan is ever
# skipped (e.g. a component-only install).
if(UNIX AND NOT APPLE)
    set_target_properties(FinanceDashboard PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
endif()

# Linux desktop-integration files. Harmless on Windows (the block is skipped),
# used by both the .deb and the AppImage. See packaging/linux/.
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/finance-dashboard.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES packaging/icons/finance-dashboard-256.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
            RENAME finance-dashboard.png)
    install(FILES packaging/linux/io.github.nicolas_goyon.FinanceDashboard.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
endif()

# NO_UNSUPPORTED_PLATFORM_ERROR + DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM:
# a platform Qt has no runtime-dependency support for (a static Qt, a
# cross-build) then still installs this project's own QML modules instead of
# aborting the configure, so `cmake --install` keeps working there — it just
# does not bundle Qt. MACOS_BUNDLE_POST_BUILD is deliberately not passed: newer
# Qt prints a NOTICE saying it no longer does anything.
qt_generate_deploy_qml_app_script(
    TARGET FinanceDashboard
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})

# ── CPack — installers and portable archives ──────────────────────────────────
# Run from the build directory after `cmake --install`-able configure:
#   Windows: cpack -G WIX   -> FinanceDashboard-<version>-windows-x64.msi
#            cpack -G ZIP   -> FinanceDashboard-<version>-windows-x64.zip
#   Linux:   cpack -G TGZ   -> FinanceDashboard-<version>-linux-x86_64.tar.gz
# The Linux AppImage and .deb are built from the same install tree by
# packaging/linux/make-appimage.sh and packaging/linux/make-deb.sh — CPack has
# no AppImage generator and its DEB generator can't produce the /opt layout
# this app wants, so those two live in scripts where they can be read and run
# by hand. See CI-CD.md.
set(CPACK_PACKAGE_NAME              "FinanceDashboard")
set(CPACK_PACKAGE_VENDOR            "DataAnalysis")
set(CPACK_PACKAGE_CONTACT           "Nicolas Goyon <https://github.com/nicolas-goyon>")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Local-first personal finance dashboard — import statements, categorize, track net worth")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://github.com/nicolas-goyon/FinanceDashboard")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "FinanceDashboard")
# The MSI's licence page needs a .rtf or a .txt; the repo's LICENSE has no
# extension, so copy it to one CPack recognises rather than keeping a second,
# hand-maintained copy of the licence text that can silently drift from it.
# CPack converts the .txt to RTF itself.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
               "${CMAKE_CURRENT_BINARY_DIR}/License.txt" COPYONLY)
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_BINARY_DIR}/License.txt")
set(CPACK_VERBATIM_VARIABLES        ON)
if(NOT WIN32)
    set(CPACK_STRIP_FILES ON)   # no `strip` in an MSVC toolchain — Unix only
endif()
# Start-menu (and desktop) shortcut: <exe basename> <label>. The WiX generator
# finds the executable anywhere in the install tree, so the flat Windows layout
# set at the top of this file is fine.
set(CPACK_PACKAGE_EXECUTABLES       "FinanceDashboard" "Finance Dashboard")
set(CPACK_CREATE_DESKTOP_LINKS      "FinanceDashboard")

if(WIN32)
    set(CPACK_SYSTEM_NAME "windows-x64")
else()
    set(CPACK_SYSTEM_NAME "linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME "FinanceDashboard-${PROJECT_VERSION}-${CPACK_SYSTEM_NAME}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "FinanceDashboard-${PROJECT_VERSION}-source")

if(WIN32)
    set(CPACK_GENERATOR "WIX;ZIP")

    # WiX .NET tools (v4/v5, `dotnet tool install --global wix`) rather than the
    # unmaintained WiX 3 candle/light pair. CPack adds -ext WixToolset.UI.wixext
    # by itself when this is 4. Requires CMake >= 3.30.
    # https://cmake.org/cmake/help/latest/cpack_gen/wix.html
    set(CPACK_WIX_VERSION 4)
    if(CMAKE_VERSION VERSION_LESS 3.30)
        message(WARNING
            "CPACK_WIX_VERSION (WiX .NET tools) needs CMake >= 3.30; this is "
            "${CMAKE_VERSION}, so `cpack -G WIX` would look for WiX 3's "
            "candle/light instead and fail. Building and running the app is "
            "unaffected — only MSI creation is. CI uses a newer CMake.")
    endif()

    # MUST stay constant for the lifetime of the product: Windows uses it to
    # recognise that 0.3.0 is an upgrade of 0.2.1 rather than a second, parallel
    # installation. Generate a new one only if this ever becomes a different app.
    set(CPACK_WIX_UPGRADE_GUID "9F2B4E7C-3A61-4C0D-9E8B-5D7A1C46F0B3")
    set(CPACK_WIX_ARCHITECTURE  "x64")
    set(CPACK_WIX_INSTALL_SCOPE "perMachine")
    set(CPACK_WIX_PRODUCT_ICON  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/finance-dashboard.ico")
    set(CPACK_WIX_UI_REF        "WixUI_InstallDir")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Finance Dashboard")
    set(CPACK_WIX_PROPERTY_ARPHELPLINK "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
