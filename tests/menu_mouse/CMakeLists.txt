cmake_minimum_required(VERSION 3.20)
project(mm3_menu_mouse_contract C)
set(CMAKE_C_STANDARD 11)
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
    target_compile_options(${target} PRIVATE /UNDEBUG)
    target_link_libraries(${target} PRIVATE user32 gdi32 xinput)
endforeach()
enable_testing()
add_test(NAME native_mouse COMMAND menu_mouse_contract)
add_test(NAME mods_enabled COMMAND mods_init_contract enabled)
add_test(NAME mods_disabled COMMAND mods_init_contract disabled)
