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
