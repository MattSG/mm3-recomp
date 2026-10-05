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
option(MM3_FRONTEND_WATCH "Trace writes to the RootLevel child pointer" OFF)

add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS
    "${RECOMP_GEN_DIR}/*.c")
if(MM3_FRONTEND_WATCH)
    set_property(SOURCE "src/recomp_manual.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "MM3_HEAP_WATCH_WRAPPER=1")
    # Seeded functions can move between split files after regeneration.
    # Bind diagnostic aliases to their actual definition instead of an index.
  foreach(wrapped_function sub_000FA37E sub_0011935B sub_001253F4 sub_0008E420 sub_0008E3C0)
        set(wrapper_owners "")
        foreach(generated_source IN LISTS RECOMP_GEN_SOURCES)
            file(STRINGS "${generated_source}" wrapper_signature
                REGEX "^void ${wrapped_function}\\(void\\)$" LIMIT_COUNT 1)
            if(wrapper_signature)
                list(APPEND wrapper_owners "${generated_source}")
            endif()
        endforeach()
        list(LENGTH wrapper_owners wrapper_owner_count)
        if(NOT wrapper_owner_count EQUAL 1)
            message(FATAL_ERROR "Expected one generated definition for ${wrapped_function}; found ${wrapper_owner_count}")
        endif()
        set_property(SOURCE ${wrapper_owners} APPEND PROPERTY
            COMPILE_DEFINITIONS "${wrapped_function}=${wrapped_function}_original")
    endforeach()
    if(MSVC)
        set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0000.c" "${RECOMP_GEN_DIR}/recomp_0011.c" "${RECOMP_GEN_DIR}/recomp_0012.c" "${RECOMP_GEN_DIR}/recomp_0017.c" "${RECOMP_GEN_DIR}/recomp_0020.c" "${RECOMP_GEN_DIR}/recomp_stubs_unresolved.c" APPEND PROPERTY
            COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/mm3_frontend_watch_hook.h")
    else()
        set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0000.c" "${RECOMP_GEN_DIR}/recomp_0011.c" "${RECOMP_GEN_DIR}/recomp_0012.c" "${RECOMP_GEN_DIR}/recomp_0017.c" "${RECOMP_GEN_DIR}/recomp_0020.c" "${RECOMP_GEN_DIR}/recomp_stubs_unresolved.c" APPEND PROPERTY
            COMPILE_OPTIONS "-include" "${CMAKE_CURRENT_SOURCE_DIR}/src/mm3_frontend_watch_hook.h")
    endif()
endif()

# 0x252AF is supplied by recomp_manual.c.  The generator also lists it in the
# aggregate unresolved-stub TU; rename that one fallback so the real override
# owns the dispatch symbol without editing generated output.
set(RECOMP_UNRESOLVED_STUB "${RECOMP_GEN_DIR}/recomp_stubs_unresolved.c")
if(EXISTS "${RECOMP_UNRESOLVED_STUB}")
    set_source_files_properties("${RECOMP_UNRESOLVED_STUB}"
        PROPERTIES COMPILE_DEFINITIONS "sub_000252AF=sub_000252AF_unresolved")
endif()

if(MM3_MOVIE_INPUT_TRACE)
    set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0017.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "sub_001DB6E8=sub_001DB6E8_original")
    set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0018.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "sub_001F2A71=sub_001F2A71_original;sub_001F443D=sub_001F443D_original")
    set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0026.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "sub_0035BDC0=sub_0035BDC0_original")
endif()

if(MM3_OFFLINE_XNET)
    set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0016.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "sub_0019531D=sub_0019531D_original")
endif()

add_executable(${PROJECT_NAME}
    src/main.c
 src/host_graphics.c
 src/mm3_memory_compat.c
 src/mm3_memory_tails.c
 src/mm3_runtime_entries.c
 src/mm3_asset_trace.c
 src/mm3_stream_compat.c
 src/recomp_manual.c
 src/recomp_trace_probe.c
 ${RECOMP_GEN_SOURCES})

set_property(SOURCE src/recomp_manual.c APPEND PROPERTY COMPILE_DEFINITIONS
    "recomp_lookup_manual=recomp_lookup_manual_base")
set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0014.c" APPEND PROPERTY
    COMPILE_DEFINITIONS "sub_00170E5D=sub_00170E5D_original")
set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0000.c" APPEND PROPERTY
    COMPILE_DEFINITIONS "sub_000145DC=sub_000145DC_original")
set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_0018.c" APPEND PROPERTY
    COMPILE_DEFINITIONS "sub_001F02AD=sub_001F02AD_original")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/mm3_runtime_entry_repairs.json" runtime_entries_json)
string(JSON runtime_entry_count LENGTH "${runtime_entries_json}")
math(EXPR runtime_entry_last "${runtime_entry_count} - 1")
foreach(runtime_entry_index RANGE 0 ${runtime_entry_last})
    string(JSON runtime_entry_start GET "${runtime_entries_json}" ${runtime_entry_index} start)
    string(SUBSTRING "${runtime_entry_start}" 2 8 runtime_entry_suffix)
    set_property(SOURCE "${RECOMP_GEN_DIR}/recomp_stubs_unresolved.c" APPEND PROPERTY
        COMPILE_DEFINITIONS "sub_${runtime_entry_suffix}=sub_${runtime_entry_suffix}_unresolved")
endforeach()

if(MM3_MOVIE_INPUT_TRACE)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_movie_input_trace.c)
endif()
if(MM3_OFFLINE_XNET)
    target_sources(${PROJECT_NAME} PRIVATE src/mm3_offline_compat.c)
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
