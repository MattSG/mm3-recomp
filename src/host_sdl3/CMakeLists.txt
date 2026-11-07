# mm3's SDL3 host stack, built against an upstream xboxrecomp that was
# configured with the backends left out:
#
#   set(XBOXRECOMP_RENDERER EXTERNAL)       # before add_subdirectory(xboxrecomp)
#   set(XBOXRECOMP_APU_OUTPUT EXTERNAL)
#   set(XBOXRECOMP_BUILD_D3D8 OFF)
#   set(XBOXRECOMP_DIR tools/xboxrecomp)
#   add_subdirectory(src/host_sdl3)         # BEFORE the toolkit: it defines xbox_d3d8
#   add_subdirectory(${XBOXRECOMP_DIR} ...)
#   target_link_libraries(<game> PRIVATE xboxrecomp mm3_host_sdl3)
#
# It provides the contracts documented in xboxrecomp's
# docs/technical/host-backends.md: nv2a_d3d_* (video/nv2a_sdlgpu.c), the
# framebuffer window (video/fb_window_sdl.c, fb_present_sdl.c), xa2_*
# (apu/apu_sdl.c), the xbox_d3d8 target (d3d/, gfx/) and the host pad provider
# (input/sdl_pads.c, installed by mm3_sdl3_install_pads()).
set(MM3_HOST_SDL3_DIR ${CMAKE_CURRENT_LIST_DIR})
list(APPEND CMAKE_MODULE_PATH ${MM3_HOST_SDL3_DIR})
if(NOT XBOXRECOMP_DIR)
    message(FATAL_ERROR "set XBOXRECOMP_DIR to the xboxrecomp checkout before host_sdl3")
endif()
include(${XBOXRECOMP_DIR}/cmake/Nv2aHlsl.cmake)   # NV2A_HLSL_PARTS
include(xbox_sdl3)      # SDL3 and SDL_shadercross
include(Dxc)
include(Nv2aSdlgpu)     # xbr_sdlgpu_shaders()

set(_gen ${CMAKE_CURRENT_BINARY_DIR}/generated)
xbr_sdlgpu_shaders(${_gen} _shader_headers)

add_library(xbox_d3d8 STATIC
    d3d/d3d8_device.c d3d/d3d8_gamma.c d3d/d3d8_resources.c d3d/d3d8_shaders.c
    d3d/d3d8_states.c d3d/d3d8_combiners.c d3d/d3d8_vsh.c d3d/d3d8_pipeline.c
    d3d/d3d8_clear.c gfx/gpu_device.c gfx/gpu_shader.c gfx/sdl_events.c)
# d3d8_swizzle.h and the other shared headers stay in the toolkit.
target_include_directories(xbox_d3d8 PUBLIC ${MM3_HOST_SDL3_DIR}/d3d ${MM3_HOST_SDL3_DIR}
    ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/d3d)
target_link_libraries(xbox_d3d8 PUBLIC platform xbox_input SDL3::SDL3
    SDL3_shadercross::SDL3_shadercross-static)

add_library(mm3_host_sdl3 STATIC
    video/nv2a_sdlgpu.c video/fb_window_sdl.c video/fb_present_sdl.c
    apu/apu_sdl.c input/sdl_pads.c ${_shader_headers})
target_include_directories(mm3_host_sdl3 PRIVATE ${_gen} ${MM3_HOST_SDL3_DIR}
    ${MM3_HOST_SDL3_DIR}/video ${MM3_HOST_SDL3_DIR}/gfx
    ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video ${XBOXRECOMP_DIR}/src/kernel
    ${XBOXRECOMP_DIR}/src/input ${XBOXRECOMP_DIR}/src/apu ${XBOXRECOMP_DIR}/src/d3d)
target_link_libraries(mm3_host_sdl3 PUBLIC xbox_d3d8 xbox_video xbox_nv2a xbox_apu xbox_input
    SDL3::SDL3)
# The runtime's headers say "the SDL3 window" under this macro.
target_compile_definitions(mm3_host_sdl3 PUBLIC XBOXRECOMP_SDL3=1)
if(NV2A_DEPTH_VIEW_SHADER)
    target_compile_definitions(mm3_host_sdl3 PRIVATE XBOXRECOMP_DEPTH_VIEW_SHADER=1)
endif()

# DXC (dxcompiler.dll, dxil.dll) and the toolkit's FFmpeg DLLs beside an executable.
function(mm3_copy_runtime_dlls target)
    xbox_copy_runtime_dlls(${target})
    xbox_copy_dxc(${target})
endfunction()
