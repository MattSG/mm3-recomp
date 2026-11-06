# Drives the xboxrecomp hooks through mm3's real SDL3 host stack (src/host_sdl3):
# a virtual SDL gamepad through the pad provider and the toolkit's shared pad logic,
# xa2_* on SDL's dummy audio driver, an SDL_GPU device from nv2a_d3d_init (skipped
# without a GPU), and registration of the window, video, kernel and NV2A hooks.
# No game files needed. The first configure builds SDL3 and DXC (long).
cmake_minimum_required(VERSION 3.24)
project(mm3_host_sdl3_hooks LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
get_filename_component(_mm3 "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(XBOXRECOMP_DIR "${_mm3}/tools/xboxrecomp" CACHE PATH "xboxrecomp checkout")
set(XBOXRECOMP_RENDERER EXTERNAL CACHE STRING "" FORCE)
set(XBOXRECOMP_APU_OUTPUT EXTERNAL CACHE STRING "" FORCE)
set(XBOXRECOMP_BUILD_D3D8 OFF CACHE BOOL "" FORCE)
set(XBOXRECOMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${_mm3}/src/host_sdl3 host_sdl3)
add_subdirectory(${XBOXRECOMP_DIR} xboxrecomp)
add_executable(host_sdl3_hooks main.c)
target_link_libraries(host_sdl3_hooks PRIVATE xboxrecomp mm3_host_sdl3)
target_include_directories(host_sdl3_hooks PRIVATE ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/input
    ${XBOXRECOMP_DIR}/src/video ${XBOXRECOMP_DIR}/src/kernel ${XBOXRECOMP_DIR}/src/apu ${XBOXRECOMP_DIR}/src/platform)
if(MSVC)
    target_link_options(host_sdl3_hooks PRIVATE /WHOLEARCHIVE:mm3_host_sdl3.lib /WHOLEARCHIVE:xbox_d3d8.lib)
endif()
mm3_copy_runtime_dlls(host_sdl3_hooks)
enable_testing()
add_test(NAME host_sdl3_hooks COMMAND host_sdl3_hooks)
