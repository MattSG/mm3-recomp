# SDL3 for every host-facing module (window, input, audio, graphics).
include(FetchContent)
# Prefer an installed SDL3; otherwise build the pinned release statically.
if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 CONFIG QUIET)
endif()
if(NOT TARGET SDL3::SDL3)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.18
        GIT_SHALLOW TRUE
        # D3D12 GPU descriptor-heap overflow (crashes in CopyDescriptorsSimple).
        PATCH_COMMAND "${CMAKE_COMMAND}" "-DPATCH=${CMAKE_CURRENT_LIST_DIR}/sdl3-d3d12-descriptors.patch"
            -P "${CMAKE_CURRENT_LIST_DIR}/ApplySdlPatch.cmake")
    FetchContent_MakeAvailable(SDL3)
endif()

# SDL_shadercross: the games' shaders are generated as HLSL while they run,
# and SDL_GPU only takes SPIR-V / DXIL / MSL, so the HLSL is compiled and
# cross-compiled at run time (DXC + SPIRV-Cross, built from the vendored
# submodules; the first build is long).
if(NOT TARGET SDL3_shadercross::SDL3_shadercross-static)
    set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "" FORCE)
    set(ENABLE_SPIRV_CODEGEN ON CACHE BOOL "" FORCE)
    set(SDLSHADERCROSS_CLI OFF CACHE BOOL "" FORCE)
    set(SDLSHADERCROSS_SHARED OFF CACHE BOOL "" FORCE)
    set(SDLSHADERCROSS_STATIC ON CACHE BOOL "" FORCE)
    set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(shadercross
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
        GIT_TAG 1ff05bec573988a98ef9e0260b4da44f512b8367
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(shadercross)
endif()

# DXC loads dxcompiler.dll / dxil.dll at run time. Without the vendored copies
# next to the executable the loader finds a system dxcompiler.dll, which has no
# SPIR-V back end ("SPIR-V CodeGen not available"). Call this for every
# executable that compiles shaders.
function(xbox_copy_dxc target)
    if(WIN32 AND TARGET dxcompiler AND TARGET dxildll)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:dxcompiler> $<TARGET_FILE:dxildll> $<TARGET_FILE_DIR:${target}>)
    endif()
endfunction()
