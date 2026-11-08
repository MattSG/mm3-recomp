cmake_minimum_required(VERSION 3.20)
project(mm3_recomp C CXX)

# The recompiled game code uses x86 SIMD intrinsics for the guest's MMX/SSE,
# and the runtime traps hardware registers by decoding x86-64 instructions,
# so the host must be x86-64 (Windows, Linux or an Intel Mac).
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    message(FATAL_ERROR "MM3 needs an x86-64 host; this is ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Recompiled x86 code relies on what MSVC does anyway: signed arithmetic wraps
# like the CPU's, and guest memory is read through whatever pointer type an
# instruction implies. GCC and Clang optimise both away as undefined
# behaviour unless told not to.
if(NOT MSVC)
    add_compile_options(-fwrapv -fno-strict-aliasing)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/xboxrecomp" CACHE PATH
    "Path to the local xboxrecomp toolkit")
set(RECOMP_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen" CACHE PATH
    "Directory containing generated recompilation C sources")
option(MM3_RECOMP_ABI_CHECK "Check guest callee-saved register contracts" OFF)
option(MM3_MOVIE_INPUT_TRACE "Observe movie completion and skip input" OFF)
option(MM3_UNLOCK_ALL_CARS "Include the optional all-car unlock override" ON)
option(MM3_FRONTEND_WATCH "Trace writes to the RootLevel child pointer" OFF)
option(MM3_TRACY "Profile the host rendering pipeline with Tracy" OFF)
option(MM3_NATIVE_FENCE_WAIT "Wait on native GPU notifications in the direct guest fence loop" ON)
option(MM3_DSP_AUTO_INLINE "Allow automatic inlining in the optimized DSP interpreter" ON)
set(MM3_TIMING_SOURCE src/mods/timing.c)
if(MM3_TRACY)
    set(MM3_TRACY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/bin/tracy-0.14.1" CACHE PATH "Tracy 0.14.1 sources")
    set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy client" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "Record only while a profiler is connected" FORCE)
    add_subdirectory("${MM3_TRACY_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/tracy" EXCLUDE_FROM_ALL)
endif()

# The audio DSP programs MM3 runs, saved there by the game (src/main.c; setup.py
# captures them from a short run) and recompiled to C by each build; derived
# from the user's game like src/recomp/gen.
set(XBOXRECOMP_DSP_PROGRAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game_files/dsp")
# MM3 owns its depth-view compatibility shader; both host renderers use it.
set(XBOXRECOMP_NV2A_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/shaders/nv2a")
# MM3 carries its own SDL3 host stack (src/host_sdl3) and builds the toolkit
# with its renderer, window, audio device and D3D8 layer left out. MM3_RENDERER
# is MM3's own choice (setup.py passes it as -DXBOXRECOMP_RENDERER=SDL3_GPU);
# the toolkit always sees EXTERNAL.
if(NOT XBOXRECOMP_RENDERER OR XBOXRECOMP_RENDERER STREQUAL "EXTERNAL")
    set(XBOXRECOMP_RENDERER "SDL3_GPU")
endif()
set(MM3_RENDERER "${XBOXRECOMP_RENDERER}")
unset(XBOXRECOMP_RENDERER)        # a normal variable would shadow the cache value below
set(XBOXRECOMP_RENDERER EXTERNAL CACHE STRING "" FORCE)
set(XBOXRECOMP_APU_OUTPUT EXTERNAL CACHE STRING "" FORCE)
set(XBOXRECOMP_BUILD_D3D8 OFF CACHE BOOL "" FORCE)
add_subdirectory(src/host_sdl3)     # before the toolkit: it defines xbox_d3d8
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)
# MM3 VP-only playback was measured about 10 dB above GP output (-10.7 vs
# -20.7 dBFS in a race). Keep its existing calibration outside the runtime.
target_compile_definitions(xbox_apu PRIVATE XBOXRECOMP_APU_VP_MONITOR_GAIN=0.316f)
if(MSVC AND MM3_DSP_AUTO_INLINE)
    # RelWithDebInfo defaults to /Ob1, which leaves small non-inline interpreter
    # helpers as calls. Override only this measured hot source, retaining its
    # existing arithmetic and Debug build behavior.
    set_property(SOURCE "${XBOXRECOMP_DIR}/src/apu/dsp/interp/dsp_cpu.c"
        TARGET_DIRECTORY xbox_apu APPEND PROPERTY
        COMPILE_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:/Ob2>")
endif()


# Settings launcher (src/mm3_launcher.cpp): RmlUi on its Win32/D3D11 backend,
# or with the SDL3 GPU renderer its SDL3 platform and SDL GPU renderer;
# FreeType for its fonts. Static, fetched once at configure time.
include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)   # archive timestamps: extraction time (CMake 3.24+)
endif()
FetchContent_Declare(freetype
    URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz
    URL_HASH SHA256=bc5c898e4756d373e0d991bab053036c5eb2aa7c0d5c67e8662ddc6da40c4103)
FetchContent_Declare(rmlui
    URL https://github.com/mikke89/RmlUi/archive/refs/tags/6.3.tar.gz
    URL_HASH SHA256=d977298bb6147610e5984d5db85ddf284020d655a8713913f6982074f1dbdede)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)
set(SKIP_INSTALL_ALL ON)
foreach(ft_dep ZLIB BZIP2 PNG HARFBUZZ BROTLI)
    set(FT_DISABLE_${ft_dep} ON)
endforeach()
FetchContent_MakeAvailable(freetype)
add_library(Freetype::Freetype ALIAS freetype)
FetchContent_MakeAvailable(rmlui)
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.tar.gz
    URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb)
FetchContent_MakeAvailable(imgui)
if(MM3_RENDERER STREQUAL "SDL3_GPU")
    # RmlUi's SDL GPU renderer loads textures through SDL3_image; the
    # launcher's images need no external codecs.
    set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "" FORCE)
    foreach(format AVIF JXL TIF WEBP)
        set(SDLIMAGE_${format} OFF CACHE BOOL "" FORCE)
    endforeach()
    FetchContent_Declare(SDL3_image
        URL https://github.com/libsdl-org/SDL_image/releases/download/release-3.4.6/SDL3_image-3.4.6.tar.gz
        URL_HASH SHA256=d2e4637ae700f72e5196b8fbd749850ed2e5e1e09c5a5be8d06ff55aaccf3b01)
    FetchContent_MakeAvailable(SDL3_image)
    set(MM3_RMLUI_BACKEND
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_SDL_GPU.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Backend_SDL_GPU.cpp)
    set(MM3_RMLUI_BACKEND_LIBS SDL3::SDL3 SDL3_image::SDL3_image)
    set(MM3_IMGUI_BACKEND
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)
    set(MM3_IMGUI_BACKEND_LIBS SDL3::SDL3)
    set(MM3_D3D11_LIBS)
else()
    set(MM3_RMLUI_BACKEND
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_Win32.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_DX11.cpp
        ${rmlui_SOURCE_DIR}/Backends/RmlUi_Backend_Win32_DX11.cpp)
    set(MM3_RMLUI_BACKEND_LIBS d3d11 dxgi d3dcompiler dwmapi)
    set(MM3_IMGUI_BACKEND
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
    set(MM3_IMGUI_BACKEND_LIBS d3d11 d3dcompiler dwmapi)
    set(MM3_D3D11_LIBS d3d11 dxgi d3dcompiler dwmapi)
endif()
# Windows' pad API stays XInput with either renderer; elsewhere pads come
# through SDL3 (tools/xboxrecomp/src/input).
if(WIN32)
    set(MM3_WIN32_LIBS xinput user32)
    set(MM3_LIBM)
else()
    set(MM3_WIN32_LIBS)
    # The platform layer's fenv calls need libm (the xboxrecomp umbrella adds it
    # too, but the focused checks link only the layers they use).
    set(MM3_LIBM m)
endif()
add_library(mm3_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${MM3_IMGUI_BACKEND})
target_include_directories(mm3_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(mm3_imgui PUBLIC ${MM3_IMGUI_BACKEND_LIBS})
if(MM3_TRACY)
    foreach(profile_target xbox_kernel xbox_video)
        target_link_libraries(${profile_target} PRIVATE Tracy::TracyClient)
        target_compile_definitions(${profile_target} PRIVATE RECOMP_TRACY=1)
    endforeach()
endif()

include(cmake/MM3GeneratedCode.cmake)
mm3_prepare_generated_sources(RECOMP_GEN_SOURCES "${RECOMP_GEN_DIR}")

add_executable(${PROJECT_NAME}
    src/derived_cache.c
    src/fixed_step.c
    src/mm3_tick_clock.c
    src/feature_switch.c
    src/env_range.c
    src/string_ci.c
    src/slot_publish.c
    src/geometry/camera_math.c
    src/geometry/piece_cluster.c
    src/geometry/road_graph.c
    src/geometry/kb_spline.c
    src/geometry/polyline.c
    src/geometry/thick_line.c
    src/geometry/vec3_rotate.c
    src/main.c
    src/mm3_asset_cache.c
       src/mm3_host_video.c
    src/mm3_launcher.cpp
    src/ui/rmlui_host.cpp
    ${MM3_RMLUI_BACKEND}
    src/mods/mods.c
    src/mods/mods_runtime.c
    src/mods/render_commands.c
    src/mods/render_policy.c
    src/mods/debug_overlay.cpp
    src/mods/dev_tools.c
    src/mods/menu_mouse.c
    src/mods/collision_fix.c
    src/mods/widescreen_hud.c
    src/mods/skip_intro.c
    src/mods/dvd_media.c
    ${MM3_TIMING_SOURCE}
    src/mods/keyboard_driving.c
 src/mm3_memory_tails.c
 src/mm3_runtime_entries.c
 src/mm3_asset_trace.c
 src/recomp_manual.c
 src/mm3_map_zoom.c
 src/mm3_race_guidance.c
 src/mm3_building_diagnostics.c
 src/mm3_tree_diagnostics.c
 src/mm3_visibility.c
 src/mm3_custom_car_names.c
 src/mm3_language.c
 src/mm3_car_selection_compat.c
 src/mm3_speedometer.c
 src/mm3_factory_physics.c
 src/mods/widescreen.c
 src/mods/draw_distance.c
 src/mods/teleport.c
 src/recomp_trace_probe.c
 ${RECOMP_GEN_SOURCES})

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3.rc)
endif()
if(MM3_RENDERER STREQUAL "SDL3_GPU")
    set_property(SOURCE ${MM3_RMLUI_BACKEND} src/mm3_launcher.cpp src/ui/rmlui_host.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS RMLUI_SDL_VERSION_MAJOR=3)
endif()

set_property(SOURCE src/recomp_manual.c APPEND PROPERTY COMPILE_DEFINITIONS
    "recomp_lookup_manual=recomp_lookup_manual_base")
if(MM3_MOVIE_INPUT_TRACE)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_movie_input_trace.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MM3_MOVIE_INPUT_TRACE)
endif()
if(MM3_UNLOCK_ALL_CARS)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_car_unlocks.c)
endif()
if(MM3_FRONTEND_WATCH)
    # A hardware write watchpoint: debug registers through the Win32 exception
    # CONTEXT, or a perf_event breakpoint on Linux. Other hosts have neither.
    if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "MM3_FRONTEND_WATCH needs Windows or Linux hardware watchpoints")
    endif()
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_frontend_watch.c)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/geometry
    src/recomp
    ${RECOMP_GEN_DIR}
    ${XBOXRECOMP_DIR}/include
    ${XBOXRECOMP_DIR}/src/video)

target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp mm3_host_sdl3 mm3_imgui RmlUi::RmlUi
    ${MM3_RMLUI_BACKEND_LIBS} ${MM3_WIN32_LIBS})
target_include_directories(${PROJECT_NAME} PRIVATE ${rmlui_SOURCE_DIR}/Backends)

# Focused offline checks; explicitly built, never part of the game build.
# Each one links the platform layer, which supplies the <platform/xbox_winnt.h>
# vocabulary off Windows (on Windows it only adds the include path).
foreach(check teleport dev_tools draw_distance)
    add_executable(mm3_${check}_contract EXCLUDE_FROM_ALL tests/mm3_${check}_contract.c)
    target_include_directories(mm3_${check}_contract PRIVATE src src/recomp ${RECOMP_GEN_DIR}
        ${XBOXRECOMP_DIR}/include ${XBOXRECOMP_DIR}/src/video ${XBOXRECOMP_DIR}/src/kernel)
    target_link_libraries(mm3_${check}_contract PRIVATE platform ${MM3_WIN32_LIBS} ${MM3_LIBM})
    if(MSVC)
        target_compile_options(mm3_${check}_contract PRIVATE /UNDEBUG)
    endif()
endforeach()

# Win32-only checks, so not registered elsewhere. The overlay check drives the
# WM_* message path of debug_overlay.cpp with the Win32/DX11 ImGui backend
# (the SDL3 build reads SDL events instead, and the SDL3 build's ImGui backend
# is not linked here), and the input check drives the XInput port ABI, which
# xinput_device.c uses only on Windows (SDL gamepads elsewhere).
if(WIN32 AND MM3_RENDERER STREQUAL "D3D11")
    add_executable(mm3_overlay_contract EXCLUDE_FROM_ALL tests/mm3_overlay_contract.cpp)
    target_include_directories(mm3_overlay_contract PRIVATE src src/recomp ${RECOMP_GEN_DIR}
        ${XBOXRECOMP_DIR}/include ${XBOXRECOMP_DIR}/src/video)
    target_link_libraries(mm3_overlay_contract PRIVATE mm3_imgui ${MM3_WIN32_LIBS})
    if(MSVC)
        target_compile_options(mm3_overlay_contract PRIVATE /UNDEBUG)
    endif()
endif()

# GNU linkers only: route recomp_trace_esp through src/recomp_trace_probe.c
# (an opt-in RECOMP_TRACE_EFD85 diagnostic).
if(NOT MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--wrap=recomp_trace_esp")
endif()

if(MM3_RECOMP_ABI_CHECK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RECOMP_ABI_CHECK)
    target_compile_definitions(xbox_kernel PRIVATE RECOMP_ABI_CHECK)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

add_executable(mm3_launcher_settings_contract EXCLUDE_FROM_ALL
    tests/mm3_launcher_settings_contract.cpp src/mods/mods.c src/feature_switch.c
    ${XBOXRECOMP_DIR}/src/input/keymap_spec.c ${MM3_RMLUI_BACKEND})
target_include_directories(mm3_launcher_settings_contract PRIVATE src
    ${rmlui_SOURCE_DIR}/Backends ${XBOXRECOMP_DIR}/src/video ${XBOXRECOMP_DIR}/src/input)
target_link_libraries(mm3_launcher_settings_contract PRIVATE platform RmlUi::RmlUi
    ${MM3_RMLUI_BACKEND_LIBS} ${MM3_WIN32_LIBS} ${MM3_LIBM})
if(MM3_RENDERER STREQUAL "SDL3_GPU")
    # The launcher source selects its SDL3 path and RmlUi's SDL3 renderer on
    # these; the contract includes the launcher source, so set them here.
    target_compile_definitions(mm3_launcher_settings_contract PRIVATE
        XBOXRECOMP_SDL3=1 RMLUI_SDL_VERSION_MAJOR=3)
endif()
if(MSVC)
    target_compile_options(mm3_launcher_settings_contract PRIVATE /UNDEBUG)
endif()

# The distance enhancement belongs to this port, including host shaders.
target_sources(mm3_recomp PRIVATE src/mods/render_fog.c)
include(cmake/MM3RenderFog.cmake)
mm3_render_fog_shaders(mm3_recomp "${MM3_RENDERER}")

mm3_copy_runtime_dlls(mm3_recomp)
