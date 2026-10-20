cmake_minimum_required(VERSION 3.21)

# ── Version — single source of truth is version.json ─────────────────────────
# The version is NOT typed here. version.json's "current" field is read below
# and fed straight into project(... VERSION ...), so the build, the installers,
# the release tag and the changelog can never drift apart. The release flow
# sets it (see CI-CD.md); change it with `python tools/version.py bump`, never
# by editing a number in this file.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.json" FD_VERSION_JSON)
string(JSON FD_VERSION GET "${FD_VERSION_JSON}" current)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/version.json")

project(FinanceDashboard VERSION ${FD_VERSION} LANGUAGES CXX)
message(STATUS "FinanceDashboard version ${PROJECT_VERSION} (from version.json)")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── App identity — prod or test (#130) ────────────────────────────────────────
# The Test app ("Finance Dashboard Test") installs next to the prod app and
# runs at the same time, with its own:
#   - application name: its data folder and settings are separate
#     (QStandardPaths::AppDataLocation, QSettings);
#   - Windows setup identity: AppId, install folder, Start menu folder,
#     shortcuts and uninstall entry;
#   - Linux desktop entry, icon and AppStream id (the AppImage);
#   - icon (an orange "TEST" band, packaging/icons/make-icons.py) and window
#     title ("Finance Dashboard (Test)").
# The experimental and preprod builds use it (#129). One identity for each
# build directory:  cmake -B build-test -DFD_APP_IDENTITY=test
# The .deb and the .tar.gz are for the prod app only.
set(FD_APP_IDENTITY "prod" CACHE STRING "The identity of the app: prod or test (#130)")
set_property(CACHE FD_APP_IDENTITY PROPERTY STRINGS prod test)
# The application name of the prod app: its data folder. The Test app copies
# the prod database from there ("Copy my prod database", #130).
set(FD_PROD_APP_NAME "FinanceDashboard")
if(FD_APP_IDENTITY STREQUAL "prod")
    set(FD_APP_IS_TEST      0)
    set(FD_APP_NAME         "${FD_PROD_APP_NAME}")     # data folder, settings, package and file names
    set(FD_APP_DISPLAY_NAME "Finance Dashboard")       # shortcuts, Start menu folder
    set(FD_APP_WINDOW_TITLE "Finance Dashboard")
    set(FD_APP_FILE_ID      "finance-dashboard")       # desktop entry and icon files
    set(FD_APP_METAINFO_ID  "io.github.nicolas_goyon.FinanceDashboard")
    # The AppId MUST stay the same for the lifetime of the product: see the
    # CPack part below.
    set(FD_INNOSETUP_APP_ID "9F2B4E7C-3A61-4C0D-9E8B-5D7A1C46F0B3")
elseif(FD_APP_IDENTITY STREQUAL "test")
    set(FD_APP_IS_TEST      1)
    set(FD_APP_NAME         "FinanceDashboardTest")
    set(FD_APP_DISPLAY_NAME "Finance Dashboard Test")
    set(FD_APP_WINDOW_TITLE "Finance Dashboard (Test)")
    set(FD_APP_FILE_ID      "finance-dashboard-test")
    set(FD_APP_METAINFO_ID  "io.github.nicolas_goyon.FinanceDashboardTest")
    set(FD_INNOSETUP_APP_ID "30296817-401F-4AB5-8B40-42BF8DC81C7F")
else()
    message(FATAL_ERROR "FD_APP_IDENTITY is \"${FD_APP_IDENTITY}\": use prod or test.")
endif()
message(STATUS "App identity: ${FD_APP_IDENTITY} (${FD_APP_DISPLAY_NAME})")

# ── Pre-release suffix (#129) ─────────────────────────────────────────────────
# The experimental and preprod pre-releases of the Test app add a SemVer
# suffix to the version of version.json: 0.2.2-exp.57, 0.2.2-exp.61.pr12,
# 0.2.2-rc.70 (the number is the run number of the CI workflow). CI computes
# it (tools/release_flow.py channel) and passes it with
# -DFD_VERSION_SUFFIX=-exp.57. It goes into the version of the app
# (QCoreApplication::applicationVersion(), which the updater compares, #117),
# the setup and the package file names. Empty for a release and a local build.
set(FD_VERSION_SUFFIX "" CACHE STRING "Pre-release suffix of the version (#129): -exp.<run>, -exp.<run>.pr<number> or -rc.<run>")
if(NOT FD_VERSION_SUFFIX STREQUAL "")
    # SemVer identifiers: a number with no leading zero, or digits and letters
    # with at least one letter.
    if(NOT FD_VERSION_SUFFIX MATCHES "^-(exp|rc)(\\.(0|[1-9][0-9]*|[0-9A-Za-z]*[A-Za-z][0-9A-Za-z]*))+$")
        message(FATAL_ERROR "FD_VERSION_SUFFIX is \"${FD_VERSION_SUFFIX}\": use -exp.<run>, -exp.<run>.pr<number> or -rc.<run>.")
    endif()
    if(NOT FD_APP_IDENTITY STREQUAL "test")
        message(FATAL_ERROR "FD_VERSION_SUFFIX is for the pre-releases, which are Test app builds: add "
                            "-DFD_APP_IDENTITY=test, or clear the suffix with -DFD_VERSION_SUFFIX= (a build "
                            "directory keeps it).")
    endif()
endif()
set(FD_FULL_VERSION "${PROJECT_VERSION}${FD_VERSION_SUFFIX}")
message(STATUS "Version of the packages: ${FD_FULL_VERSION}")

include(GNUInstallDirs)

# Windows installs/portable ZIPs are flat: FinanceDashboard.exe sits at the root
# of the install directory with its DLLs beside it, the way every other Windows
# app does it. On Linux the FHS-ish bin/ + lib/ split is what the AppImage and
# the .deb expect, so GNUInstallDirs' defaults stand.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
endif()

# Qt Quick / QML + Qt Graphs (2D). Qt Charts and Widgets are no longer used.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Graphs Sql QuickControls2 Network)

qt_standard_project_setup(REQUIRES 6.10)

# QTP0004: auto-generate qmldir for QML files in subdirectories
qt_policy(SET QTP0004 NEW)

# QML modules build into <build>/qml/<URI path>. For an executable, the default
# is <build>/<URI path>, here <build>/FinanceDashboard. On Linux the executable
# has no extension and also wants <build>/FinanceDashboard, so the link fails
# with "cannot open output file FinanceDashboard: Is a directory". Qt adds this
# directory to the import path of qmllint and qmlcachegen.
# https://doc.qt.io/qt-6/cmake-variable-qt-qml-output-directory.html
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# ── UI-independent C++ (db / models / utils / predict / import) ──────────────
# The app below and the tests (tests/CMakeLists.txt) compile this same list, so
# a file added here is in both. Absolute paths, because tests/ is another
# source directory.
set(FD_CORE_SOURCES
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
    src/db/tables/SettingsTable.h
    src/db/tables/SettingsTable.cpp
    src/db/tables/SqlPrepare.h
    src/db/tables/SqlText.h
    src/db/Database.h
    src/db/Database.cpp
    src/utils/Formatter.h
    src/utils/AccountHashing.h
    src/utils/StatusMessage.h
    src/utils/StatusMessage.cpp
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
    # The update check (#117): versions and the list of releases. Qt Core only,
    # no network — UpdateController below downloads the list.
    src/update/SemVer.h
    src/update/SemVer.cpp
    src/update/ReleaseFeed.h
    src/update/ReleaseFeed.cpp
)
list(TRANSFORM FD_CORE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# ── Executable — entry point + start-up stage + the core above ──────────────────
qt_add_executable(FinanceDashboard
    src/main.cpp
    # Start-up stage: requirement checks + initialization, run before the main
    # window loads (see docs/startup.md).
    src/startup/StartupChecks.h
    src/startup/StartupChecks.cpp
    # The network access of the QML engine: local files and resources only
    # (#117, ARCHITECTURE.MD § Network rule).
    src/startup/LocalOnlyNetwork.h
    src/startup/LocalOnlyNetwork.cpp
    ${FD_CORE_SOURCES}
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
        src/controllers/updates/UpdateController.h
        src/controllers/updates/UpdateController.cpp
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
        qml/pages/UpdatesPage.qml
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
    src/controllers/updates
)

target_link_libraries(FinanceDashboard PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Graphs
    Qt6::Sql
    Qt6::QuickControls2
    # The update check (#117): QNetworkAccessManager, HTTPS to api.github.com.
    Qt6::Network
)

target_compile_definitions(FinanceDashboard PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
)

# For main.cpp only:
#   - FD_VERSION_STRING, the version with its pre-release suffix (#129):
#     main.cpp passes it to QGuiApplication::setApplicationVersion(), so the
#     running app reports the same number as version.json, the installer and
#     the release tag. Never hard-code a version string in C++.
#   - the identity of the app (#130): the application name (the data folder
#     and the settings), the window title and the desktop entry, and for the
#     Test app the application name of the prod app (its database).
# On main.cpp alone, a new identity or suffix in a build directory rebuilds one
# file, not the whole app (CI builds both identities in one directory).
set_source_files_properties(src/main.cpp PROPERTIES COMPILE_DEFINITIONS
    "FD_VERSION_STRING=\"${FD_FULL_VERSION}\";FD_APP_NAME=\"${FD_APP_NAME}\";FD_APP_WINDOW_TITLE=\"${FD_APP_WINDOW_TITLE}\";FD_APP_FILE_ID=\"${FD_APP_FILE_ID}\";FD_APP_IS_TEST=${FD_APP_IS_TEST};FD_PROD_APP_NAME=\"${FD_PROD_APP_NAME}\"")

# The icon of the identity, for the window and the taskbar (main.cpp sets it
# from this resource) and, on Windows, for the .exe (app.rc).
set_source_files_properties("packaging/icons/${FD_APP_FILE_ID}-256.png"
    PROPERTIES QT_RESOURCE_ALIAS "app-icon.png")
qt_add_resources(FinanceDashboard "app_icon"
    PREFIX "/"
    FILES "packaging/icons/${FD_APP_FILE_ID}-256.png"
)
if(WIN32)
    # The icon resource of the .exe: Explorer, the shortcuts of the setup and
    # the taskbar show it. Qt names it IDI_ICON1.
    # https://doc.qt.io/qt-6/appicon.html
    set(FD_APP_ICO "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/${FD_APP_FILE_ID}.ico")
    configure_file(packaging/windows/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    target_sources(FinanceDashboard PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()

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
# Everything the CI packages (Windows setup, portable ZIP, AppImage, .deb,
# .tar.gz) is built from this one install tree — there is no second, packaging-only copy of
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
# Those of the identity of the build (#130): make-appimage.sh takes them from
# the install tree.
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/${FD_APP_FILE_ID}.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES packaging/icons/${FD_APP_FILE_ID}-256.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
            RENAME ${FD_APP_FILE_ID}.png)
    install(FILES packaging/linux/${FD_APP_METAINFO_ID}.metainfo.xml
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
#   Windows: cpack -G INNOSETUP -> FinanceDashboard-<version>-windows-x64.exe (setup)
#            cpack -G ZIP       -> FinanceDashboard-<version>-windows-x64.zip
#   Linux:   cpack -G TGZ   -> FinanceDashboard-<version>-linux-x86_64.tar.gz
# The Linux AppImage and .deb are built from the same install tree by
# packaging/linux/make-appimage.sh and packaging/linux/make-deb.sh — CPack has
# no AppImage generator and its DEB generator can't produce the /opt layout
# this app wants, so those two live in scripts where they can be read and run
# by hand. See CI-CD.md.
set(CPACK_PACKAGE_NAME              "${FD_APP_NAME}")
set(CPACK_PACKAGE_VENDOR            "DataAnalysis")
set(CPACK_PACKAGE_CONTACT           "Nicolas Goyon <https://github.com/nicolas-goyon>")
# With the pre-release suffix (#129): Inno Setup's AppVersion, and the file
# names below.
set(CPACK_PACKAGE_VERSION           "${FD_FULL_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Local-first personal finance dashboard — import statements, categorize, track net worth")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://github.com/nicolas-goyon/FinanceDashboard")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${FD_APP_NAME}")
# The setup's licence page needs a .txt or a .rtf; the repo's LICENSE has no
# extension, so copy it to one CPack recognises rather than keeping a second,
# hand-maintained copy of the licence text that can silently drift from it.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
               "${CMAKE_CURRENT_BINARY_DIR}/License.txt" COPYONLY)
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_CURRENT_BINARY_DIR}/License.txt")
set(CPACK_VERBATIM_VARIABLES        ON)
if(NOT WIN32)
    set(CPACK_STRIP_FILES ON)   # no `strip` in an MSVC toolchain — Unix only
endif()
# Start-menu shortcut and optional desktop shortcut: <exe basename> <label>. The
# Inno Setup generator makes a shortcut for each .exe with this base name,
# anywhere in the install tree, so the flat Windows layout set at the top of
# this file is fine.
set(CPACK_PACKAGE_EXECUTABLES       "FinanceDashboard" "${FD_APP_DISPLAY_NAME}")
set(CPACK_CREATE_DESKTOP_LINKS      "FinanceDashboard")

if(WIN32)
    set(CPACK_SYSTEM_NAME "windows-x64")
else()
    set(CPACK_SYSTEM_NAME "linux-${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME "${FD_APP_NAME}-${FD_FULL_VERSION}-${CPACK_SYSTEM_NAME}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "FinanceDashboard-${PROJECT_VERSION}-source")

if(WIN32)
    set(CPACK_GENERATOR "INNOSETUP;ZIP")

    # Inno Setup installer. The CPack generator needs CMake >= 3.27 and finds
    # ISCC (Inno Setup 6 or later) on PATH or in its default install folder.
    # https://cmake.org/cmake/help/latest/cpack_gen/innosetup.html
    if(CMAKE_VERSION VERSION_LESS 3.27)
        message(WARNING
            "The CPack Inno Setup generator needs CMake >= 3.27; this is "
            "${CMAKE_VERSION}, so `cpack` cannot make the Windows setup. "
            "Building and running the app is not affected.")
    endif()

    # The AppId MUST stay the same for the lifetime of the product: Setup
    # updates an installed copy in place only when the AppId is the same. By
    # default CPack takes it from CPACK_PACKAGE_INSTALL_REGISTRY_KEY (the install
    # folder name), so set it explicitly. No braces: Inno Setup reads "{" as the
    # start of a constant, and UsePreviousPrivileges (default yes) needs an AppId
    # without constants.
    # The Test app has its own AppId (#130): its setup installs, updates and
    # uninstalls it without touching the prod app.
    # https://jrsoftware.org/ishelp/topic_setup_appid.htm
    set(CPACK_INNOSETUP_SETUP_AppId "${FD_INNOSETUP_APP_ID}")
    # AppVersion is CPACK_PACKAGE_VERSION, with the pre-release suffix (#129).
    # The version resource of the setup takes numbers only: up to four,
    # missing ones are zeros (https://jrsoftware.org/ishelp/topic_setup_versioninfoversion.htm).
    set(CPACK_INNOSETUP_SETUP_VersionInfoVersion "${PROJECT_VERSION}")

    # The person who installs chooses "for me only" (the default, no
    # administrator rights, %LOCALAPPDATA%\Programs) or "for all users"
    # (administrator rights, Program Files). A later setup uses the mode of the
    # installed copy and does not ask again.
    # https://jrsoftware.org/ishelp/topic_setup_privilegesrequiredoverridesallowed.htm
    set(CPACK_INNOSETUP_SETUP_PrivilegesRequired "lowest")
    set(CPACK_INNOSETUP_SETUP_PrivilegesRequiredOverridesAllowed "dialog")

    # CPack writes the architecture as "x64", which Inno Setup 6.3+ accepts with
    # a deprecation warning. "x64compatible" also allows Arm64 Windows 11, which
    # runs x64 apps through emulation.
    # https://jrsoftware.org/ishelp/topic_archidentifiers.htm
    set(CPACK_INNOSETUP_ARCHITECTURE "x64")
    set(CPACK_INNOSETUP_SETUP_ArchitecturesAllowed "x64compatible")
    set(CPACK_INNOSETUP_SETUP_ArchitecturesInstallIn64BitMode "x64compatible")

    set(CPACK_INNOSETUP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icons/${FD_APP_FILE_ID}.ico")
    set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
    set(CPACK_INNOSETUP_PROGRAM_MENU_FOLDER "${FD_APP_DISPLAY_NAME}")
    # "Launch Finance Dashboard" check box on the last page of the setup.
    set(CPACK_INNOSETUP_RUN_EXECUTABLES "FinanceDashboard")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
