cmake_minimum_required(VERSION 3.20)
project(mm3_mod_contracts C)
set(CMAKE_C_STANDARD 11)
# Use the project's canonical runtime header; no generated game code or assets.
configure_file(../../src/mm3_recomp_types.h recomp_types.h COPYONLY)
add_executable(mm3_collision_contract ../mm3_collision_contract.c)
target_include_directories(mm3_collision_contract PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    ../../tools/xboxrecomp/include)
if(MSVC)
    target_compile_options(mm3_collision_contract PRIVATE /UNDEBUG)
endif()
enable_testing()
add_test(NAME collision COMMAND mm3_collision_contract)
