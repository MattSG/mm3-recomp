include("${CMAKE_CURRENT_LIST_DIR}/../scripts/mm3_fence_wait_seam.cmake")
set(original [=[void sub_00344640(void)
{
loc_00344733: ;
    ecx = MEM32(edx);
    edi = esi;
    _fb = (uint32_t)(ecx) & 0xFFFFFFFFu; _fbs = (int32_t)(int32_t)(_fb); /* sub source, before the write */
    edi = edi - ecx;
    _fa = (uint32_t)(edi) & 0xFFFFFFFFu; _fas = (int32_t)(int32_t)(_fa); /* sub result */
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fb = (uint32_t)(edi) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp eax, edi (32-bit) */
    if (CMP_B(_fa, _fb)) goto loc_00344733; /* jb: below (unsigned <) */
loc_0034473D: ;
    POP32(esp, edi);
}
]=])
if(DEFINED REJECT_CASE)
    if(REJECT_CASE STREQUAL "duplicate")
        string(APPEND original "loc_00344733: ;\n")
    elseif(REJECT_CASE STREQUAL "changed")
        string(REPLACE "CMP_B(_fa, _fb)" "CMP_BE(_fa, _fb)" original "${original}")
    elseif(REJECT_CASE STREQUAL "missing")
        string(REPLACE "loc_00344733: ;" "loc_00344734: ;" original "${original}")
    else()
        message(FATAL_ERROR "Unknown rejection case")
    endif()
    mm3_add_fence_wait(original)
    message(FATAL_ERROR "Invalid guest loop was accepted")
endif()
foreach(line_endings LF CRLF)
    set(expected "${original}")
    set(newline "\n")
    if(line_endings STREQUAL "CRLF")
        set(newline "\r\n")
        string(REPLACE "\n" "${newline}" expected "${expected}")
    endif()
    set(transformed "${expected}")
    mm3_add_fence_wait(transformed)
    string(REGEX MATCHALL "xbox_Nv2aFenceWait[(]edx, esi, eax[)]" calls "${transformed}")
    list(LENGTH calls call_count)
    if(NOT call_count EQUAL 1)
        message(FATAL_ERROR "Seam must insert exactly one host call")
    endif()
    string(REPLACE "    xbox_Nv2aFenceWait(edx, esi, eax);${newline}" "" restored "${transformed}")
    string(REPLACE "#include <stdint.h>${newline}extern void xbox_Nv2aFenceWait(uint32_t fence_va, uint32_t submitted, uint32_t allowed_distance);${newline}"
        "" restored "${restored}")
    if(NOT restored STREQUAL expected)
        message(FATAL_ERROR "Original guest instructions were changed")
    endif()
endforeach()
foreach(reject_case duplicate changed missing)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DREJECT_CASE=${reject_case}"
        -P "${CMAKE_CURRENT_LIST_FILE}" RESULT_VARIABLE rejected
        OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(rejected EQUAL 0 OR NOT error MATCHES "original 0x00344733 fence loop")
        message(FATAL_ERROR "Invalid ${reject_case} loop was not rejected: ${error}")
    endif()
endforeach()
message(STATUS "PASS: exact fence seam, unchanged guest instructions, LF/CRLF and invalid anchors")
