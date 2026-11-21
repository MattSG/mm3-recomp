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
set(XBOXRECOMP_INPUT_SDL2 OFF CACHE BOOL "" FORCE)   # the SDL3 host feeds the key table itself
set(XBOXRECOMP_SDL3 ON CACHE BOOL "" FORCE)
# The game's own depth-view compatibility shader, so the renderer tests see what the game links.
set(XBOXRECOMP_NV2A_SHADER_DIR "${CMAKE_CURRENT_LIST_DIR}/../../src/mods/shaders/nv2a")
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

# --- White-box renderer tests ------------------------------------------------------------------
# They #include src/host_sdl3/video/nv2a_sdlgpu.c, so they do not link mm3_host_sdl3 (that would
# define the renderer twice); they need its generated shaders and a Vulkan (or D3D12) device.
# Without a GPU, Mesa's lavapipe under Xvfb is enough: xvfb-run -a ctest.
include(${_mm3}/cmake/MM3RenderFog.cmake)
function(mm3_renderer_test target source)
    add_executable(${target} ${source})
    if(NOT ARGN STREQUAL "STUB_WINDOW")     # the stub host replaces the window module
        target_sources(${target} PRIVATE ${_mm3}/src/host_sdl3/video/fb_window_sdl.c)
    endif()
    add_dependencies(${target} mm3_host_sdl3)          # generates the SDL_GPU shader headers
    target_include_directories(${target} PRIVATE
        ${_mm3}/src ${_mm3}/src/host_sdl3 ${_mm3}/src/host_sdl3/video ${_mm3}/src/host_sdl3/gfx
        ${CMAKE_BINARY_DIR}/host_sdl3/generated ${CMAKE_CURRENT_LIST_DIR}
        ${XBOXRECOMP_DIR}/tests ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
        ${XBOXRECOMP_DIR}/src/kernel ${XBOXRECOMP_DIR}/src/input)
    target_compile_definitions(${target} PRIVATE XBOXRECOMP_SDL3=1 XBOXRECOMP_DEPTH_VIEW_SHADER=1)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -fwrapv -fno-strict-aliasing)
    endif()
    target_link_libraries(${target} PRIVATE platform xbox_input SDL3::SDL3)
    if(NOT ARGN STREQUAL "STUB_WINDOW")
        mm3_render_fog_shaders(${target})
    endif()
endfunction()
mm3_renderer_test(renderer_bindings renderer_bindings.c)
mm3_renderer_test(renderer_fog renderer_fog.c)
foreach(_case common init_failure pixels host_draw failures ps_bindings ps_reset overlay_upload resize)
    add_test(NAME renderer_bindings_${_case} COMMAND renderer_bindings ${_case})
endforeach()
add_test(NAME renderer_fog COMMAND renderer_fog)
mm3_renderer_test(renderer_lamps renderer_lamps.c STUB_WINDOW)
foreach(_samples 1 4)
    foreach(_scale 1 3)
        add_test(NAME renderer_lamps_${_samples}x_scale${_scale} COMMAND renderer_lamps ${_samples} ${_scale})
    endforeach()
endforeach()
