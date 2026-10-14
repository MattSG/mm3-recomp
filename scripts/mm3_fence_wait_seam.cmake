# Insert a host wait at one known guest fence loop. Keep its original final
# read, uint32 subtraction, flags and branch intact. Fail closed if the lifted
# block changes, rather than guessing another wait site after regeneration.
function(mm3_add_fence_wait source_variable)
    set(content "${${source_variable}}")
    set(original_block [=[loc_00344733: ;
    ecx = MEM32(edx);
    edi = esi;
    _fb = (uint32_t)(ecx) & 0xFFFFFFFFu; _fbs = (int32_t)(int32_t)(_fb); /* sub source, before the write */
    edi = edi - ecx;
    _fa = (uint32_t)(edi) & 0xFFFFFFFFu; _fas = (int32_t)(int32_t)(_fa); /* sub result */
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fb = (uint32_t)(edi) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp eax, edi (32-bit) */
    if (CMP_B(_fa, _fb)) goto loc_00344733; /* jb: below (unsigned <) */]=])
    string(FIND "${content}" "\r\n" crlf_position)
    if(crlf_position LESS 0)
        set(newline "\n")
    else()
        set(newline "\r\n")
        string(REPLACE "\n" "${newline}" original_block "${original_block}")
    endif()
    string(REGEX MATCHALL "loc_00344733:" labels "${content}")
    list(LENGTH labels label_count)
    string(FIND "${content}" "${original_block}" block_position)
    string(FIND "${content}" "void sub_00344640(void)" owner_position)
    if(NOT label_count EQUAL 1 OR block_position LESS 0 OR
       owner_position LESS 0 OR block_position LESS owner_position)
        message(FATAL_ERROR "MM3 native fence wait: original 0x00344733 fence loop is missing, duplicated or changed")
    endif()
    string(REPLACE "loc_00344733: ;${newline}"
        "loc_00344733: ;${newline}    xbox_Nv2aFenceWait(edx, esi, eax);${newline}"
        waited_block "${original_block}")
    string(REPLACE "${original_block}" "${waited_block}" content "${content}")
    set(declaration "#include <stdint.h>${newline}extern void xbox_Nv2aFenceWait(uint32_t fence_va, uint32_t submitted, uint32_t allowed_distance);${newline}")
    set(${source_variable} "${declaration}${content}" PARENT_SCOPE)
endfunction()
