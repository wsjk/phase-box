# Sets PHASEBOX_VERSION from git, for both the simulator build and the firmware build.
#   On a release commit:   0.3.0              (tag v0.3.0, made by .github/workflows/release.yml)
#   After a release:       0.3.0-2-gabc1234   (2 commits after v0.3.0)
#   Uncommitted changes:   ...-dirty
#   No release tag yet:    0.0.0-gabc1234
# It is read when CMake configures, so re-run cmake to refresh it after a new commit or tag.
find_package(Git QUIET)
set(PHASEBOX_VERSION "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*" --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
        OUTPUT_VARIABLE _pb_describe OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _pb_result ERROR_QUIET)
    if(_pb_result EQUAL 0)
        string(REGEX REPLACE "^v" "" PHASEBOX_VERSION "${_pb_describe}")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --always --dirty
            WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
            OUTPUT_VARIABLE _pb_describe OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _pb_result ERROR_QUIET)
        if(_pb_result EQUAL 0)
            set(PHASEBOX_VERSION "0.0.0-g${_pb_describe}")
        endif()
    endif()
endif()
message(STATUS "Phase Box version: ${PHASEBOX_VERSION}")
