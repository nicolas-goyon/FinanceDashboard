# ── Tests ─────────────────────────────────────────────────────────────────────
# Built only when FD_BUILD_TESTS=ON (see the top-level CMakeLists.txt), so a
# normal IDE build is untouched. CI configures with -DFD_BUILD_TESTS=ON and runs
# `ctest --output-on-failure`.
#
# predictor_selftest
#   tools/predictor_selftest.cpp is the existing hand-run sanity harness for
#   src/predict/ — its own main(), prints one PASS/FAIL line per check and exits
#   non-zero if any failed, which is exactly the contract CTest wants. It stays
#   where it is (README, TODO.md and version.md all point at that path); this
#   file just gives it a build target and a test registration.
#
#   The two src/predict/*.cpp files are compiled a second time here rather than
#   shared with the app target through a library. That is deliberate: two small,
#   Qt-Core-only translation units cost nothing to recompile, and keeping the app
#   target's structure untouched means enabling tests can't change how the
#   shipped binary is built.
#
#   Replacing this with a real Qt Test case (TODO.md § no tests) means adding
#   Test to the top-level find_package components and swapping the add_test()
#   below for qt_add_test() — nothing else here changes.

add_executable(predictor_selftest
    ${CMAKE_SOURCE_DIR}/tools/predictor_selftest.cpp
    ${CMAKE_SOURCE_DIR}/src/predict/LabelNormalizer.cpp
    ${CMAKE_SOURCE_DIR}/src/predict/CategoryPredictor.cpp
)

target_include_directories(predictor_selftest PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(predictor_selftest PRIVATE Qt6::Core)

# The harness is console-only and must never pick up the WIN32_EXECUTABLE
# treatment the app gets — its output is the test result.
set_target_properties(predictor_selftest PROPERTIES WIN32_EXECUTABLE OFF)

add_test(NAME predictor_selftest COMMAND predictor_selftest)
set_tests_properties(predictor_selftest PROPERTIES
    TIMEOUT 120
    # Nothing in the harness touches the DB or the GUI, but say so anyway so a
    # headless CI runner can never trip over a platform-plugin lookup.
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# import_save_test
#   The statement-import save path, end to end: the real SnapshotImportController
#   and the real Database on a temporary SQLite file (#16, #17, #29). Qt Test,
#   no GUI (QTEST_GUILESS_MAIN). It compiles the same core sources as the app
#   (FD_CORE_SOURCES in the top-level CMakeLists.txt) plus the controller.

find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(import_save_test
    import_save_test.cpp
    ${FD_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry/SnapshotImportController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry/SnapshotImportController.cpp
)
target_include_directories(import_save_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry
)
# Qt6::Qml for the QML_ELEMENT macros in the controller header only.
target_link_libraries(import_save_test PRIVATE Qt6::Core Qt6::Sql Qt6::Qml Qt6::Test)
set_target_properties(import_save_test PROPERTIES WIN32_EXECUTABLE OFF)

add_test(NAME import_save_test COMMAND import_save_test)
set_tests_properties(import_save_test PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# db_migration_test
#   Schema versioning (PRAGMA user_version) and each migration step, on
#   databases built the way older versions of the app left them (#20, #18).

qt_add_executable(db_migration_test
    db_migration_test.cpp
    ${FD_CORE_SOURCES}
)
target_include_directories(db_migration_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(db_migration_test PRIVATE Qt6::Core Qt6::Sql Qt6::Test)
set_target_properties(db_migration_test PROPERTIES WIN32_EXECUTABLE OFF)

add_test(NAME db_migration_test COMMAND db_migration_test)
set_tests_properties(db_migration_test PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# db_write_test
#   The writes of the table classes: a null optional text, and the empty required
#   values that the Database refuses (#125); renames that reach every row that
#   uses the name, with the duplicate checks of the controllers (#34); the
#   default data written once per database (#37), the mock data (#38), the
#   order of the balance history (#36), the currency of an account (#48), and
#   the checked results of prepare(), BEGIN and COMMIT (#49). It compiles the
#   core sources plus three controllers.

qt_add_executable(db_write_test
    db_write_test.cpp
    ${FD_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry/AccountController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry/AccountController.cpp
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry/CategoryController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry/CategoryController.cpp
    ${CMAKE_SOURCE_DIR}/src/controllers/database/DatabaseManagementController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/database/DatabaseManagementController.cpp
)
target_include_directories(db_write_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/controllers/dataentry
    ${CMAKE_SOURCE_DIR}/src/controllers/database
)
# Qt6::Qml for the QML_ELEMENT macros in the controller headers; Qt6::Gui for
# QColor in CategoryController.
target_link_libraries(db_write_test PRIVATE Qt6::Core Qt6::Gui Qt6::Sql Qt6::Qml Qt6::Test)
set_target_properties(db_write_test PROPERTIES WIN32_EXECUTABLE OFF)

add_test(NAME db_write_test COMMAND db_write_test)
set_tests_properties(db_write_test PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# db_copy_test
#   "Copy my prod database" of the Test app (#130): Database::replaceWithCopyOf()
#   and DatabaseManagementController::copyProdDatabase(), with a prod and a Test
#   database in a temporary folder. The prod file does not change; an older one
#   is upgraded in the copy; a newer one, a file that is not a database and a
#   missing file are refused; a copy that does not open is kept and the Test
#   data comes back; a Test database that another connection uses is not
#   replaced. It compiles the core sources plus the controller.
qt_add_executable(db_copy_test
    db_copy_test.cpp
    ${FD_CORE_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/controllers/database/DatabaseManagementController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/database/DatabaseManagementController.cpp
)
target_include_directories(db_copy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/controllers/database
)
# Qt6::Qml for the QML_ELEMENT macro in the controller header.
target_link_libraries(db_copy_test PRIVATE Qt6::Core Qt6::Sql Qt6::Qml Qt6::Test)
set_target_properties(db_copy_test PROPERTIES WIN32_EXECUTABLE OFF)
add_test(NAME db_copy_test COMMAND db_copy_test)
set_tests_properties(db_copy_test PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# calc_test
#   The calculations behind the texts and charts of the pages: the Projects page
#   (#41), the money flow and the rolling average of the Cash Flow page (#42,
#   #56), the performance of the Investments page (#56) and the money format
#   (#48). No database. It compiles the page controllers that it tests.

qt_add_executable(calc_test
    calc_test.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Formatter.h
    ${CMAKE_SOURCE_DIR}/src/controllers/projects/ProjectsController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/projects/ProjectsController.cpp
    ${CMAKE_SOURCE_DIR}/src/controllers/cashflow/CashFlowController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/cashflow/CashFlowController.cpp
    ${CMAKE_SOURCE_DIR}/src/controllers/investments/InvestmentsController.h
    ${CMAKE_SOURCE_DIR}/src/controllers/investments/InvestmentsController.cpp
)
target_include_directories(calc_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/controllers/projects
    ${CMAKE_SOURCE_DIR}/src/controllers/cashflow
    ${CMAKE_SOURCE_DIR}/src/controllers/investments
)
# Qt6::Qml for the QML_ELEMENT macros in the controller headers.
target_link_libraries(calc_test PRIVATE Qt6::Core Qt6::Qml Qt6::Test)
set_target_properties(calc_test PROPERTIES WIN32_EXECUTABLE OFF)

add_test(NAME calc_test COMMAND calc_test)
set_tests_properties(calc_test PROPERTIES
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Every test target, so that CI and a local build need one target name:
#   cmake --build build --target fd_tests
add_custom_target(fd_tests DEPENDS predictor_selftest import_save_test db_migration_test db_write_test db_copy_test calc_test)
