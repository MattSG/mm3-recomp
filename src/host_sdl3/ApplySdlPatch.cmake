# Apply a patch to a fetched source tree (SDL3, RmlUi) once. Run as a FetchContent PATCH_COMMAND:
#   cmake -DPATCH=<file> -P ApplySdlPatch.cmake      (working directory: the fetched source)
# --no-index matters: the source sits under build/, inside this repository's work
# tree, where a plain "git apply" silently skips files git does not track.
find_package(Git REQUIRED)
# The patch must be LF (.gitattributes: *.patch eol=lf): git apply rejects a CRLF patch as corrupt,
# and CMake's file(WRITE) cannot produce an LF copy on Windows (it writes CRLF).
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --no-index --ignore-whitespace --reverse --check "${PATCH}"
    RESULT_VARIABLE _applied OUTPUT_QUIET ERROR_QUIET)
if(_applied EQUAL 0)
    message(STATUS "Patch already applied: ${PATCH}")
    return()
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --no-index --ignore-whitespace "${PATCH}"
    RESULT_VARIABLE _rc ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "Patch ${PATCH} does not apply: ${_err}")
endif()
message(STATUS "Patched: ${PATCH}")
