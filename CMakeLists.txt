cmake_minimum_required(VERSION 3.20)
project(mm3_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/xboxrecomp" CACHE PATH
    "Path to the local xboxrecomp toolkit")
set(RECOMP_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen" CACHE PATH
    "Directory containing generated recompilation C sources")
option(MM3_RECOMP_ABI_CHECK "Check guest callee-saved register contracts" OFF)
option(MM3_MOVIE_INPUT_TRACE "Observe movie completion and skip input" OFF)
option(MM3_OFFLINE_XNET "Bypass MM3 network readiness for offline startup" ON)
option(MM3_UNLOCK_ALL_CARS "Make every installed car selectable" ON)
option(MM3_FRONTEND_WATCH "Trace writes to the RootLevel child pointer" OFF)
option(MM3_TRACY "Profile the host rendering pipeline with Tracy" OFF)
option(MM3_NATIVE_FENCE_WAIT "Wait on native GPU notifications in the direct guest fence loop" ON)
option(MM3_DSP_AUTO_INLINE "Allow automatic inlining in the optimized DSP interpreter" ON)
if(MM3_TRACY)
    enable_language(CXX)
    set(MM3_TRACY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/bin/tracy-0.14.1" CACHE PATH "Tracy 0.14.1 sources")
    set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy client" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "Record only while a profiler is connected" FORCE)
    add_subdirectory("${MM3_TRACY_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/tracy" EXCLUDE_FROM_ALL)
endif()

add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)
if(MSVC AND MM3_DSP_AUTO_INLINE)
    # RelWithDebInfo defaults to /Ob1, which leaves small non-inline interpreter
    # helpers as calls. Override only this measured hot source, retaining its
    # existing arithmetic and Debug build behavior.
    set_property(SOURCE "${XBOXRECOMP_DIR}/src/apu/dsp/interp/dsp_cpu.c"
        TARGET_DIRECTORY xbox_apu APPEND PROPERTY
        COMPILE_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:/Ob2>")
endif()
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

# Generated functions move between split chunks whenever the generator's
# function list changes, so find each project hook's owner instead of naming
# a chunk. Hooks rename the generated definition to <name>_original and the
# project supplies <name>.
set(MM3_RENAMED_FUNCTIONS
    sub_00170E5D sub_000145DC sub_001F02AD
    sub_0025968E)                # widescreen: wraps the projection-matrix builder
if(MM3_MOVIE_INPUT_TRACE)
    list(APPEND MM3_RENAMED_FUNCTIONS sub_001DB6E8 sub_001F2A71 sub_001F443D sub_0035BDC0)
endif()
if(MM3_OFFLINE_XNET)
    list(APPEND MM3_RENAMED_FUNCTIONS sub_0019531D)
endif()
if(MM3_FRONTEND_WATCH)
    list(APPEND MM3_RENAMED_FUNCTIONS sub_000FA37E sub_0011935B sub_001253F4 sub_0008E420 sub_0008E3C0)
    set_property(SOURCE "src/recomp_manual.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "MM3_HEAP_WATCH_WRAPPER=1")
endif()
# Render distance renames definitions only: renaming every reference with a
# compile definition would bypass the hooks for callers in the same chunk.
set(MM3_DISTANCE_FUNCTIONS sub_000D9E2E sub_000E2D5B sub_000F4AC3 sub_000FE71E
    sub_001661D0 sub_002256C7 sub_002203E5 sub_0022F298)
set(MM3_DEFINITION_ONLY_FUNCTIONS ${MM3_DISTANCE_FUNCTIONS})
# Keep same-chunk calls directed to the recovered x87 helper definitions.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0009BC04 sub_0009BC35 sub_0009BC52)
# Preserve same-chunk locale calls to the custom vehicle-name extension.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_0011C194)
# The CRT MMX probe must preserve the stack effects of PUSHFD/POPFD.
list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_00093770)
if(MM3_UNLOCK_ALL_CARS)
    list(APPEND MM3_DEFINITION_ONLY_FUNCTIONS sub_001DEA30)
endif()

list(JOIN MM3_RENAMED_FUNCTIONS "|" hook_names)
list(JOIN MM3_DEFINITION_ONLY_FUNCTIONS "|" distance_names)
set(MM3_SEAM_FUNCTIONS sub_00344900 sub_003449B0 sub_0013FE96 sub_0014E808)
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_dma_publish_seam.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/mm3_collision_call_seam.cmake")
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
foreach(hooked IN LISTS MM3_RENAMED_FUNCTIONS)
    set_property(SOURCE "${owner_${hooked}}" APPEND PROPERTY
        COMPILE_DEFINITIONS "${hooked}=${hooked}_original")
endforeach()
foreach(seam IN LISTS MM3_SEAM_FUNCTIONS)
    # A regenerated owner must regenerate the copied seam too, even when the
    # generated filenames stay the same or the function moves to another chunk.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${owner_${seam}}")
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

# Definition and loop hooks share one transformed copy per owning chunk,
# carrying over its other compile properties even after functions move chunks.
set(distance_owners "")
foreach(hooked IN LISTS MM3_DEFINITION_ONLY_FUNCTIONS)
    list(APPEND distance_owners "${owner_${hooked}}")
endforeach()
foreach(hooked IN LISTS MM3_SEAM_FUNCTIONS)
    list(APPEND distance_owners "${owner_${hooked}}")
endforeach()
list(REMOVE_DUPLICATES distance_owners)
foreach(distance_input IN LISTS distance_owners)
    get_filename_component(distance_name "${distance_input}" NAME)
    set(distance_output "${CMAKE_CURRENT_BINARY_DIR}/render_distance/${distance_name}")
    file(READ "${distance_input}" distance_content)
    foreach(hooked IN LISTS MM3_DEFINITION_ONLY_FUNCTIONS)
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
    src/main.c
    src/mm3_debug_features.c
 src/mm3_memory_compat.c
 src/mm3_memory_tails.c
 src/mm3_runtime_entries.c
 src/mm3_asset_trace.c
 src/mm3_stream_compat.c
 src/recomp_manual.c
 src/mm3_widescreen.c
 src/mm3_render_distance.c
 src/mm3_debug_teleport.c
 src/mm3_custom_car_names.c
 src/mm3_cpu_compat.c
 src/recomp_trace_probe.c
 ${RECOMP_GEN_SOURCES})

set_property(SOURCE src/recomp_manual.c APPEND PROPERTY COMPILE_DEFINITIONS
    "recomp_lookup_manual=recomp_lookup_manual_base" "sub_00083A6C=sub_00083A6C_original")
if(MM3_MOVIE_INPUT_TRACE)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_movie_input_trace.c)
endif()
if(MM3_OFFLINE_XNET)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_offline_compat.c)
endif()
if(MM3_UNLOCK_ALL_CARS)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_car_unlocks.c)
endif()
if(MM3_FRONTEND_WATCH)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_frontend_watch.c)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/recomp
    ${RECOMP_GEN_DIR}
    ${XBOXRECOMP_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)

# MM3's CRT uses NtAllocateVirtualMemory reserve requests as heap-backed
# buffers.  Keep the vendor runtime untouched: redirect its public reserve
# seam to the existing Xbox heap on the ELF build used by this checkout.
if(NOT MSVC)
 target_link_options(${PROJECT_NAME} PRIVATE
  "-Wl,--wrap=xbox_ReserveAlloc"
  "-Wl,--wrap=recomp_trace_esp")
endif()

if(MM3_RECOMP_ABI_CHECK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE RECOMP_ABI_CHECK)
    target_compile_definitions(xbox_kernel PRIVATE RECOMP_ABI_CHECK)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
