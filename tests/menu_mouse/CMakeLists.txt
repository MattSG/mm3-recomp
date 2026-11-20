cmake_minimum_required(VERSION 3.20)
project(mm3_menu_mouse_contract C)
set(CMAKE_C_STANDARD 11)
# The contract drives the SDL3 cursors, so it needs SDL3 (find_package SDL3 or
# -DCMAKE_PREFIX_PATH) and a video driver at run time (SDL_VIDEO_DRIVER=dummy
# has no cursors; use a display or Xvfb).
find_package(SDL3 REQUIRED CONFIG)
set(RECOMP_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/recomp/gen" CACHE PATH
    "Generated recompilation headers")
configure_file(../../src/mm3_recomp_types.h recomp_types.h COPYONLY)
add_executable(menu_mouse_contract ../menu_mouse_native_contract.c)
add_executable(mods_init_contract ../mm3_mods_init_contract.c)
foreach(target menu_mouse_contract mods_init_contract)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}" "${RECOMP_GEN_DIR}"
        ../../src ../../tools/xboxrecomp/src
        ../../tools/xboxrecomp/src/kernel ../../tools/xboxrecomp/src/video)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    endif()
    target_compile_definitions(${target} PRIVATE XBOXRECOMP_SDL3=1)
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
    if(NOT WIN32)
        # The runtime's Win32 shims (SRW locks, GetTickCount64, ...) off Windows.
        target_sources(${target} PRIVATE ../../tools/xboxrecomp/src/platform/win32_compat.c)
        target_link_libraries(${target} PRIVATE m pthread)
    endif()
endforeach()
enable_testing()
add_test(NAME native_mouse COMMAND menu_mouse_contract)
add_test(NAME mods_enabled COMMAND mods_init_contract enabled)
add_test(NAME mods_disabled COMMAND mods_init_contract disabled)

target_sources(mods_init_contract PRIVATE ../../src/feature_switch.c)
