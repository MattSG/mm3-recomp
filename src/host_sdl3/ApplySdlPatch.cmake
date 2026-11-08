# Apply a patch to the SDL3 source tree once. Run as a FetchContent PATCH_COMMAND:
#   cmake -DPATCH=<file> -P ApplySdlPatch.cmake      (working directory: the SDL source)
# --no-index matters: the source sits under build/, inside this repository's work
# tree, where a plain "git apply" silently skips files git does not track.
find_package(Git REQUIRED)
# The patch must be LF (.gitattributes: *.patch eol=lf): git apply rejects a CRLF patch as corrupt,
# and CMake's file(WRITE) cannot produce an LF copy on Windows (it writes CRLF).
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --no-index --ignore-whitespace --reverse --check "${PATCH}"
    RESULT_VARIABLE _applied OUTPUT_QUIET ERROR_QUIET)
if(_applied EQUAL 0)
    message(STATUS "SDL3 patch already applied: ${PATCH}")
    return()
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --no-index --ignore-whitespace "${PATCH}"
    RESULT_VARIABLE _rc ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "SDL3 patch ${PATCH} does not apply: ${_err}")
endif()
message(STATUS "SDL3 patched: ${PATCH}")
