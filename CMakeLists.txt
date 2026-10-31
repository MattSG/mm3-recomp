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
if(XBOXRECOMP_RENDERER STREQUAL "SDL3_GPU")
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

file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS
    "${RECOMP_GEN_DIR}/*.c")
# Setup reuses generated functions on an existing checkout. Refresh the
# project's compatibility header so pulling fixes does not require regeneration.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/mm3_recomp_types.h"
    "${RECOMP_GEN_DIR}/recomp_types.h" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/tools/xboxrecomp/templates/runtime/recomp_types.h"
    "${RECOMP_GEN_DIR}/recomp_base.h" COPYONLY)

# Generated functions move between split chunks whenever the generator's
# function list changes, so find each project hook's owner instead of naming
# a chunk. Hooks rename the generated definition to <name>_original and the
# project supplies <name>.
set(MM3_RENAMED_FUNCTIONS
    sub_00170E5D sub_000145DC sub_001F02AD
    sub_0025968E                 # widescreen: wraps the projection-matrix builder
    sub_001DB6E8 sub_001F2A71 sub_001F443D  # skip intro: movie play, frame, skip check
)
if(MM3_MOVIE_INPUT_TRACE)
    list(APPEND MM3_RENAMED_FUNCTIONS sub_0035BDC0)
endif()
if(MM3_FRONTEND_WATCH)
    list(APPEND MM3_RENAMED_FUNCTIONS sub_000FA37E sub_0011935B sub_001253F4 sub_0008E420 sub_0008E3C0)
    set_property(SOURCE "src/recomp_manual.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "MM3_HEAP_WATCH_WRAPPER=1")
endif()
# Render distance renames definitions only: renaming every reference with a
# compile definition would bypass the hooks for callers in the same chunk.
set(MM3_DISTANCE_FUNCTIONS sub_000D9E2E sub_000E2D5B sub_000F4AC3 sub_000FE71E
    sub_001630A7 sub_001661D0 sub_002256C7 sub_00248518 sub_002203E5 sub_0022F298 sub_0015A929 sub_000F8DCE sub_000F8750)
set(MM3_DEFINITION_ONLY_FUNCTIONS ${MM3_DISTANCE_FUNCTIONS})
# Native menu layout, focus, activation and composite control geometry.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001C5935 sub_000C2FA7 sub_00246894
    sub_001118C7 sub_00111CBF sub_001C1585 sub_00103E41 sub_00103FBD sub_001038D0
    sub_0010DDC0 sub_001C44F8 sub_00117B03 sub_00117C99 sub_00117D11
    sub_00104F7B sub_0010514E sub_0010553A)
# The main loop and world update can share a generated chunk. Rename only
# these definitions so direct and same-chunk calls enter the clocked wrappers.
set(MM3_SIMULATION_FUNCTIONS sub_00025422 sub_0002545A sub_001E7590 sub_001EC8E6
    sub_0022DBBF)                # and the race AI's update, kept at 30 Hz
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS ${MM3_SIMULATION_FUNCTIONS})
# Building diagnostics keep same-chunk calls routed through opt-in wrappers.
set(MM3_BUILDING_FUNCTIONS sub_00168A77 sub_001C7B15 sub_001A67BA sub_00166E01
    sub_001681FB sub_00167DA5 sub_00166B3E sub_00162F75)
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS ${MM3_BUILDING_FUNCTIONS})
# Retire struck tree collisions and animate their native models while falling.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0016AEDB sub_000E1F49 sub_001D137B)
# Preserve same-chunk near-camera calls to the chase-distance adjustment.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001EEC53)
# Cache map texture ownership at native load/reconfiguration/destruction boundaries.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0011C742 sub_0011CAAC sub_0007CEC3)
# Preserve same-chunk locale calls to the custom vehicle-name extension.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0011C194)
# Preserve same-chunk catalog calls to the retired-car selection fallback.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001DDFC5)
# The saved language (and its voices): menu base constructor and updateLanguage.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001193A6 sub_00124822)
# The hot visibility leaf keeps its balanced x87 scratch stack in native locals.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001C793A)
# Factory profiles bind the real constructed controller and leave retail cars
# on their original torque and gearbox paths, including same-chunk calls.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0021ED64 sub_001D0E2D sub_001CB7E8 sub_001CB881 sub_001CB9A5 sub_001CF7B4)
if(MM3_UNLOCK_ALL_CARS)
    list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001DEA30)
endif()

list(JOIN MM3_RENAMED_FUNCTIONS "|" hook_names)
list(JOIN MM3_DEFINITION_ONLY_FUNCTIONS "|" distance_names)
set(MM3_SEAM_FUNCTIONS sub_00344900 sub_003449B0 sub_0013FE96 sub_0014E808)
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_dma_publish_seam.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_collision_call_seam.cmake")
list(APPEND MM3_SEAM_FUNCTIONS sub_0023F09B)
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_speedometer_seam.cmake")
list(APPEND MM3_SEAM_FUNCTIONS sub_0011CA25 sub_0011D1E8 sub_0011D808 sub_0011CAE8)
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_map_zoom_seam.cmake")
list(APPEND MM3_SEAM_FUNCTIONS sub_00201BCA sub_0011CAE8 sub_00120091 sub_00116535 sub_0014BAE9 sub_0014C121)
list(REMOVE_DUPLICATES MM3_SEAM_FUNCTIONS)
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_race_guidance_seam.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_tree_contact_seam.cmake")
list(APPEND MM3_SEAM_FUNCTIONS sub_0015BF5A sub_0015D2DD)
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_dsound_isr_seam.cmake")
list(APPEND MM3_SEAM_FUNCTIONS ${MM3_DSOUND_GUARD_FUNCTIONS})
if(MM3_NATIVE_FENCE_WAIT)
    list(APPEND MM3_SEAM_FUNCTIONS sub_00344640)
    include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_fence_wait_seam.cmake")
endif()
list(JOIN MM3_SEAM_FUNCTIONS "|" seam_names)
# recomp_runtime_entries.c holds the project's replacement x87 helper bodies.
set(hook_scan_sources ${RECOMP_GEN_SOURCES})
list(FILTER hook_scan_sources EXCLUDE REGEX "/recomp_runtime_entries[.]c$")
foreach(generated_source IN LISTS hook_scan_sources)
    file(STRINGS "${generated_source}" hooked_definitions
        REGEX "^void (${hook_names}|${distance_names}|${seam_names})[(]void[)]$")
    foreach(definition IN LISTS hooked_definitions)
        string(REGEX REPLACE "^void (sub_[0-9A-F]+)[(]void[)]$" "\\1" hooked "${definition}")
        if(DEFINED owner_${hooked})
            message(FATAL_ERROR "${hooked} is defined in ${owner_${hooked}} and ${generated_source}")
        endif()
        set(owner_${hooked} "${generated_source}")
    endforeach()
endforeach()
foreach(hooked IN LISTS MM3_RENAMED_FUNCTIONS MM3_DEFINITION_ONLY_FUNCTIONS MM3_SEAM_FUNCTIONS)
    if(NOT DEFINED owner_${hooked})
        message(FATAL_ERROR "No generated definition of ${hooked} in ${RECOMP_GEN_DIR}")
    endif()
endforeach()
if(MM3_FRONTEND_WATCH)
    if(MSVC)
        set(watch_include "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/mm3_frontend_watch_hook.h")
    else()
        set(watch_include "-include" "${CMAKE_CURRENT_SOURCE_DIR}/src/mm3_frontend_watch_hook.h")
    endif()
    set_property(SOURCE ${RECOMP_GEN_SOURCES} APPEND PROPERTY COMPILE_OPTIONS ${watch_include})
endif()

# 0x252AF and the runtime-proven entries are supplied by the project. The
# generator also lists them in the aggregate unresolved-stub TU; rename those
# fallbacks so the real definitions own the dispatch symbols.
set(RECOMP_UNRESOLVED_STUB "${RECOMP_GEN_DIR}/recomp_stubs_unresolved.c")
set_property(SOURCE "${RECOMP_UNRESOLVED_STUB}" APPEND PROPERTY
    COMPILE_DEFINITIONS "sub_000252AF=sub_000252AF_unresolved")
# Stop at the first stub that runs (recomp_unresolved_stub_stop in xboxrecomp).
set_property(SOURCE "${RECOMP_UNRESOLVED_STUB}" APPEND PROPERTY
    COMPILE_DEFINITIONS "RECOMP_UNRESOLVED_STUB_OBSERVE=recomp_unresolved_stub_stop")
# Prototypes those two generated files use without declaring (see the header).
if(MSVC)
    set(mm3_gen_prototypes "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/mm3_gen_prototypes.h")
else()
    set(mm3_gen_prototypes "-include" "${CMAKE_CURRENT_SOURCE_DIR}/src/mm3_gen_prototypes.h")
endif()
set_property(SOURCE "${RECOMP_UNRESOLVED_STUB}" "${RECOMP_GEN_DIR}/recomp_runtime_entries.c"
    APPEND PROPERTY COMPILE_OPTIONS ${mm3_gen_prototypes})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/mm3_runtime_entry_repairs.json")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mm3_runtime_entry_repairs.json" runtime_entries_json)
string(JSON runtime_entry_count LENGTH "${runtime_entries_json}")
math(EXPR runtime_entry_last "${runtime_entry_count} - 1")
foreach(runtime_entry_index RANGE 0 ${runtime_entry_last})
    string(JSON runtime_entry_start GET "${runtime_entries_json}" ${runtime_entry_index} start)
    string(SUBSTRING "${runtime_entry_start}" 2 8 runtime_entry_suffix)
    set_property(SOURCE "${RECOMP_UNRESOLVED_STUB}" APPEND PROPERTY
        COMPILE_DEFINITIONS "sub_${runtime_entry_suffix}=sub_${runtime_entry_suffix}_unresolved")
endforeach()

# All wrappers rename only their generated definitions. A compile definition
# also renames same-chunk calls, bypassing the wrapper (including the world
# scheduler). Preserve the fork's loop seams and other source properties in
# one transformed copy per owning chunk.
set(chunk_hooks ${MM3_RENAMED_FUNCTIONS} ${MM3_DEFINITION_ONLY_FUNCTIONS})
set(distance_owners "")
foreach(hooked IN LISTS chunk_hooks MM3_SEAM_FUNCTIONS)
    list(APPEND distance_owners "${owner_${hooked}}")
endforeach()
list(REMOVE_DUPLICATES distance_owners)
# Regeneration must refresh every transformed copy, including definition-only
# hooks, even when the generated filenames stay the same or owners move chunks.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${distance_owners})
foreach(distance_input IN LISTS distance_owners)
    get_filename_component(distance_name "${distance_input}" NAME)
    set(distance_output "${CMAKE_CURRENT_BINARY_DIR}/hooked_chunks/${distance_name}")
    file(READ "${distance_input}" distance_content)
    foreach(hooked IN LISTS chunk_hooks)
        if(owner_${hooked} STREQUAL distance_input)
            string(REPLACE "void ${hooked}(void)" "void ${hooked}_original(void)"
                distance_content "${distance_content}")
        endif()
    endforeach()
    if(MM3_NATIVE_FENCE_WAIT AND owner_sub_00344640 STREQUAL distance_input)
        mm3_add_fence_wait(distance_content)
    endif()
    foreach(dma_owner sub_00344900 sub_003449B0)
        if(owner_${dma_owner} STREQUAL distance_input)
            mm3_add_dma_publish(distance_content ${dma_owner})
        endif()
    endforeach()
    foreach(collision_owner sub_0013FE96 sub_0014E808)
        if(owner_${collision_owner} STREQUAL distance_input)
            mm3_fix_collision_calls(distance_content ${collision_owner})
        endif()
    endforeach()
    if(owner_sub_0023F09B STREQUAL distance_input)
        mm3_add_speedometer_readout(distance_content)
    endif()
    foreach(map_owner sub_0011CA25 sub_0011D1E8 sub_0011D808 sub_0011CAE8)
        if(owner_${map_owner} STREQUAL distance_input)
            mm3_add_map_zoom(distance_content ${map_owner})
        endif()
    endforeach()
    foreach(race_owner sub_00201BCA sub_0011CAE8 sub_00120091 sub_00116535 sub_0014BAE9 sub_0014C121)
        if(owner_${race_owner} STREQUAL distance_input)
            mm3_add_race_guidance(distance_content ${race_owner})
        endif()
    endforeach()
    if(owner_sub_001D137B STREQUAL distance_input)
        mm3_add_tree_contacts(distance_content)
    endif()
    if(owner_sub_0016AEDB STREQUAL distance_input)
        mm3_add_tree_mesh(distance_content)
    endif()
    if(owner_sub_0027AF28 STREQUAL distance_input)
        foreach(dsound_owner IN LISTS MM3_DSOUND_GUARD_FUNCTIONS)
            if(NOT owner_${dsound_owner} STREQUAL distance_input)
                message(FATAL_ERROR "MM3 DirectSound ISR sync: ${dsound_owner} moved to another chunk")
            endif()
        endforeach()
        mm3_add_dsound_isr_sync(distance_content)
    endif()
    foreach(tree_owner sub_0015BF5A sub_0015D2DD)
        if(owner_${tree_owner} STREQUAL distance_input)
            mm3_add_tree_render(distance_content ${tree_owner})
        endif()
    endforeach()
    file(CONFIGURE OUTPUT "${distance_output}" CONTENT "${distance_content}" @ONLY)
    foreach(distance_property COMPILE_DEFINITIONS COMPILE_OPTIONS)
        get_source_file_property(distance_value "${distance_input}" ${distance_property})
        if(distance_value)
            set_property(SOURCE "${distance_output}" PROPERTY ${distance_property} "${distance_value}")
        endif()
    endforeach()
    list(REMOVE_ITEM RECOMP_GEN_SOURCES "${distance_input}")
    list(APPEND RECOMP_GEN_SOURCES "${distance_output}")
endforeach()

add_executable(${PROJECT_NAME}
    src/derived_cache.c
    src/fixed_step.c
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
    src/mods/render_commands.c
    src/mods/render_policy.c
    src/mods/debug_overlay.cpp
    src/mods/dev_tools.c
    src/mods/menu_mouse.c
    src/mods/collision_fix.c
    src/mods/widescreen_hud.c
    src/mods/skip_intro.c
    src/mods/dvd_media.c
    src/mods/framerate.c
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
if(XBOXRECOMP_RENDERER STREQUAL "SDL3_GPU")
    set_property(SOURCE ${MM3_RMLUI_BACKEND} src/mm3_launcher.cpp src/ui/rmlui_host.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS RMLUI_SDL_VERSION_MAJOR=3)
endif()

set_property(SOURCE src/recomp_manual.c APPEND PROPERTY COMPILE_DEFINITIONS
    "recomp_lookup_manual=recomp_lookup_manual_base" "sub_00083A6C=sub_00083A6C_original")
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

target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp mm3_imgui RmlUi::RmlUi
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
if(WIN32 AND XBOXRECOMP_RENDERER STREQUAL "D3D11")
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
if(XBOXRECOMP_RENDERER STREQUAL "SDL3_GPU")
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
mm3_render_fog_shaders(mm3_recomp "${XBOXRECOMP_RENDERER}")
