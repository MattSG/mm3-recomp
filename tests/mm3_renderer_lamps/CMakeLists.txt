cmake_minimum_required(VERSION 3.20)
project(mm3_renderer_lamps C)
enable_testing()

# SDL3 GPU backend: built wherever the SDL3 GPU renderer builds.
include(../../tools/xboxrecomp/cmake/Nv2aSdlgpu.cmake)
add_executable(nv2a_flare_depth_sdlgpu test_flare_depth_sdlgpu.c)
target_include_directories(nv2a_flare_depth_sdlgpu PRIVATE ../../tools/xboxrecomp/src ../../tools/xboxrecomp/src/video)
xbr_nv2a_sdlgpu_target(nv2a_flare_depth_sdlgpu)
foreach(samples 1 4)
    foreach(scale 1 3)
        add_test(NAME nv2a_flare_depth_sdlgpu_${samples}x_scale${scale}
            COMMAND nv2a_flare_depth_sdlgpu ${samples} ${scale})
    endforeach()
endforeach()

if(NOT WIN32)
    message(STATUS "nv2a_flare_depth: D3D11 renderer test, Windows only")
    return()
endif()
include(../../tools/xboxrecomp/cmake/Nv2aHlsl.cmake)   # the renderer's embedded HLSL
add_executable(nv2a_flare_depth test_flare_depth.c)
target_include_directories(nv2a_flare_depth PRIVATE ../../tools/xboxrecomp/src ../../tools/xboxrecomp/src/video)
target_link_libraries(nv2a_flare_depth PRIVATE d3d11 dxgi dxguid d3dcompiler)
foreach(samples 1 4)
    foreach(scale 1 3)
        add_test(NAME nv2a_flare_depth_${samples}x_scale${scale}
            COMMAND nv2a_flare_depth ${samples} ${scale})
    endforeach()
endforeach()
xbr_embed_nv2a_hlsl(nv2a_flare_depth)
