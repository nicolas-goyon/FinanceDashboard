cmake_minimum_required(VERSION 3.20)
project(FinanceDashboard VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt Quick / QML + Qt Graphs (2D). Qt Charts and Widgets are no longer used.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Graphs Sql QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

# QTP0004: auto-generate qmldir for QML files in subdirectories
qt_policy(SET QTP0004 NEW)

# ── Executable — entry point + UI-independent C++ (db / models / utils) ───────
qt_add_executable(FinanceDashboard
    src/main.cpp
    src/models/AssetEntry.h
    src/models/AssetEntry.cpp
    src/models/ImportedStatement.h
    src/models/ImportLog.h
    src/models/Category.h
    src/models/Account.h
    src/models/Transaction.h
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
    src/db/Database.h
    src/db/Database.cpp
    src/utils/Formatter.h
    src/utils/AccountHashing.h
    # Statement-import template system (see ARCHITECTURE.MD § Statement Import).
    # UI-independent — no QML_ELEMENT here, that's SnapshotImportController below.
    src/import/ImportTemplate.h
    src/import/ImportTemplateRegistry.h
    src/import/ImportTemplateRegistry.cpp
    src/import/CheckingCsvTemplateV1.h
    src/import/CheckingCsvTemplateV1.cpp
)

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
)

# NOTE — qml/pages/ImportSnapshotPage.qml (FileDialog) and
# qml/pages/CategoriesManagePage.qml / qml/pages/AccountsManagePage.qml
# (ColorDialog) import QtQuick.Dialogs, used only from QML (no C++ API call),
# so qt_add_qml_module's import scanning
# should resolve/deploy it automatically without an explicit find_package/link
# step — same as other QML-only modules here. If the build fails to find the
# QtQuick.Dialogs plugin, add "QuickDialogs2" to the find_package(Qt6 ...) line
# above and Qt6::QuickDialogs2 to target_link_libraries() below.

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
)

# Windows: hide the console window in Release builds
if(WIN32)
    set_target_properties(FinanceDashboard PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
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
