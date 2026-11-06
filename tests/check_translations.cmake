# translations_test: every text of the app has a French translation.
#
# Runs lupdate on the sources (qml/ and src/) into a copy of the .ts file, so
# a text added with qsTr()/tr() but not in the .ts counts too, then fails when
# a message is unfinished or vanished from the copy.
#
#   cmake -DLUPDATE=<path> -DSOURCE_DIR=<repo> -DTS=<.ts file> -DWORK_DIR=<dir> -P check_translations.cmake
get_filename_component(name "${TS}" NAME)
set(copy "${WORK_DIR}/${name}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(COPY_FILE "${TS}" "${copy}")
execute_process(
    COMMAND "${LUPDATE}" -silent -no-obsolete -locations none -extensions qml,cpp,h
            "${SOURCE_DIR}/qml" "${SOURCE_DIR}/src" -ts "${copy}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "lupdate failed (${result}):\n${output}")
endif()
file(READ "${copy}" ts)
string(REGEX MATCHALL "<message>[^<]*<source>[^<]*</source>[^<]*<translation type=\"unfinished\"" missing "${ts}")
list(LENGTH missing count)
if(count GREATER 0)
    set(list "")
    foreach(entry IN LISTS missing)
        string(REGEX REPLACE ".*<source>([^<]*)</source>.*" "\\1" source "${entry}")
        string(APPEND list "  ${source}\n")
    endforeach()
    message(FATAL_ERROR "${count} text(s) have no French translation. Run\n"
                        "  cmake --build <build> --target update_translations\n"
                        "and translate them in ${TS}:\n${list}")
endif()
message(STATUS "Every text has a French translation.")
