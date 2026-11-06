# SDL3 for the SDL3 GPU renderer and its window, the audio output, and the game controllers everywhere.
# An installed SDL3 (3.2 or newer) is used when CMake finds one; otherwise the
# pinned release is downloaded and built as a static library.
#
# Defines the target SDL3::SDL3.
if(TARGET SDL3::SDL3)
    return()
endif()
find_package(SDL3 3.2 CONFIG QUIET)
if(SDL3_FOUND)
    message(STATUS "SDL3: using installed ${SDL3_VERSION}")
    return()
endif()
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)   # archive timestamps: extraction time
endif()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
find_package(Git REQUIRED)
FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
    PATCH_COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${CMAKE_CURRENT_LIST_DIR}/sdl3-d3d12-descriptors.patch")
FetchContent_MakeAvailable(SDL3)
if(NOT TARGET SDL3::SDL3 AND TARGET SDL3::SDL3-static)
    add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()
message(STATUS "SDL3: building 3.4.16 from source")
