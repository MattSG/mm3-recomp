cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/../scripts/mm3_dma_publish_seam.cmake")
set(original [=[void sub_00344900(void)
{
loc_0034491C: ;
    eax = MEM32(edx + 0x17C0);
    MEM32(eax + 0x40) = esi;
loc_00344940: ;
    ecx = MEM32(eax + 0x3240);
    if (ecx != MEM32(eax + 0x3244)) goto loc_00344940;
    esi = MEM32(eax + 0x400700);
    MEM32(ecx + 0x40) = eax;
    MEM32(ecx + 0x44) = eax;
}
void sub_003449B0(void)
{
loc_003449CB: ;
    MEM32(eax + 0x40) = edx;
loc_003449F0: ;
    ecx = MEM32(eax + 0x3240);
    if (ecx != MEM32(eax + 0x3244)) goto loc_003449F0;
    ecx = MEM32(eax + 0x400700);
}
void unrelated(void)
{
    MEM32(eax + 0x40) = esi;
}
]=])
if(DEFINED REJECT_CASE)
    if(REJECT_CASE STREQUAL "store")
        string(REPLACE "MEM32(eax + 0x40) = esi;" "MEM32(eax + 0x44) = esi;" original "${original}")
    elseif(REJECT_CASE STREQUAL "read")
        string(REPLACE "ecx = MEM32(eax + 0x3240);" "ecx = MEM32(eax + 0x3244);" original "${original}")
    elseif(REJECT_CASE STREQUAL "duplicate_store")
        string(REPLACE "loc_00344940: ;" "    MEM32(eax + 0x40) = esi;\nloc_00344940: ;" original "${original}")
    elseif(REJECT_CASE STREQUAL "duplicate_owner")
        string(APPEND original "\nvoid sub_00344900(void)\n{}\n")
    elseif(REJECT_CASE STREQUAL "label")
        string(REPLACE "loc_00344940: ;" "loc_00344941: ;" original "${original}")
    else()
        message(FATAL_ERROR "Unknown rejection case")
    endif()
    mm3_add_dma_publish(original sub_00344900)
    message(FATAL_ERROR "Invalid publication seam accepted")
endif()
foreach(line_endings LF CRLF)
    set(expected "${original}")
    set(newline "\n")
    if(line_endings STREQUAL "CRLF")
        set(newline "\r\n")
        string(REPLACE "\n" "${newline}" expected "${expected}")
    endif()
    set(transformed "${expected}")
    mm3_add_dma_publish(transformed sub_00344900)
    mm3_add_dma_publish(transformed sub_003449B0)
    string(REGEX MATCHALL "xbox_Nv2aDmaWait[(]eax[)]" waits "${transformed}")
    list(LENGTH waits wait_count)
    string(REGEX MATCHALL "extern void xbox_Nv2aDmaPublish" declarations "${transformed}")
    list(LENGTH declarations declaration_count)
    if(NOT wait_count EQUAL 2 OR NOT declaration_count EQUAL 1)
        message(FATAL_ERROR "Expected two waits and one declaration block")
    endif()
    string(REPLACE "    xbox_Nv2aDmaWait(eax);${newline}" "" restored "${transformed}")
    string(REPLACE "    xbox_Nv2aDmaPublish(eax + 0x40u, esi);" "    MEM32(eax + 0x40) = esi;" restored "${restored}")
    string(REPLACE "    xbox_Nv2aDmaPublish(eax + 0x40u, edx);" "    MEM32(eax + 0x40) = edx;" restored "${restored}")
    set(declarations "#include <stdint.h>${newline}extern void xbox_Nv2aDmaPublish(uint32_t put_va, uint32_t value);${newline}extern void xbox_Nv2aDmaWait(uint32_t pfifo_base_va);${newline}")
    string(REPLACE "${declarations}" "" restored "${restored}")
    if(NOT restored STREQUAL expected)
        message(FATAL_ERROR "Publication seam changed other guest instructions")
    endif()
endforeach()
foreach(reject_case store read duplicate_store duplicate_owner label)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DREJECT_CASE=${reject_case}"
        -P "${CMAKE_CURRENT_LIST_FILE}" RESULT_VARIABLE rejected
        OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(rejected EQUAL 0 OR NOT error MATCHES "MM3 DMA publication")
        message(FATAL_ERROR "Invalid ${reject_case} seam was not rejected: ${error}")
    endif()
endforeach()
message(STATUS "PASS: queue publication seams, original wait reads, software stores, LF/CRLF and rejected anchors")
