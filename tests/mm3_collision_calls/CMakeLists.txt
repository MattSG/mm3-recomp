cmake_minimum_required(VERSION 3.20)
project(mm3_collision_calls C)

enable_testing()
add_executable(mm3_collision_call_stack_test ../mm3_collision_call_stack_test.c)
target_include_directories(mm3_collision_call_stack_test PRIVATE ../../src ../../tools/xboxrecomp/include)
target_compile_features(mm3_collision_call_stack_test PRIVATE c_std_11)
if(NOT MSVC)
    target_link_libraries(mm3_collision_call_stack_test PRIVATE m)
endif()
add_test(NAME mm3-collision-stack-reuse COMMAND mm3_collision_call_stack_test)
add_test(NAME mm3-collision-call-seam
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/../mm3_collision_call_seam_test.cmake)
