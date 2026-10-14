# These three original +0x5C virtual calls deliberately retain PUSH3 for the
# immediately following sub_001C7E60 (RET12). Only failed dispatch needs the
# caller-cleanup macro; resolved dispatch and every guest instruction stay as
# generated. Strict owner/argument/reuse anchors make regeneration fail closed.
function(mm3_fix_collision_calls source_variable function_name)
    set(content "${${source_variable}}")
    if(function_name STREQUAL "sub_0013FE96")
        set(sites 0013FEB6 0013FEE5)
    elseif(function_name STREQUAL "sub_0014E808")
        set(sites 0014E8B6)
    else()
        message(FATAL_ERROR "MM3 collision argument cleanup: unsupported owner ${function_name}")
    endif()
    string(REGEX MATCHALL "void ${function_name}[(]void[)]" owners "${content}")
    list(LENGTH owners owner_count)
    string(FIND "${content}" "void ${function_name}(void)" owner_start)
    if(NOT owner_count EQUAL 1 OR owner_start LESS 0)
        message(FATAL_ERROR "MM3 collision argument cleanup: missing or duplicate ${function_name}")
    endif()
    string(SUBSTRING "${content}" ${owner_start} -1 tail)
    string(FIND "${tail}" "\nvoid " owner_end)
    if(owner_end LESS 0)
        set(body "${tail}")
    else()
        string(SUBSTRING "${tail}" 0 ${owner_end} body)
    endif()
    string(FIND "${content}" "\r\n" crlf_position)
    set(newline "\n")
    if(NOT crlf_position LESS 0)
        set(newline "\r\n")
    endif()
    set(transformed "${body}")
    foreach(site IN LISTS sites)
        if(site STREQUAL "0013FEB6")
            set(block [=[    eax = ebp + -28;
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, eax);
    eax = ebp + -4;
    edi = ecx;
    ecx = MEM32(ebp + 0x10);
    PUSH32(esp, eax);
    eax = MEM32(ecx);
    PUSH32(esp, esi);
    { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0013FEB9u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0013FEB6u); } /* indirect call */
    }

loc_0013FEB9: ;
    edx = MEM32(ebp + 0xC);
    ebx = eax;
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0013FEC3u); RECOMP_ABI_CALL(0x001C7E60u, sub_001C7E60); /* call 0x001C7E60 */]=])
        elseif(site STREQUAL "0013FEE5")
            set(block [=[loc_0013FED7: ;
    ecx = MEM32(ebp + 0x14);
    eax = ebp + -28;
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, eax);
    eax = ebp + -4;
    PUSH32(esp, eax);
    eax = MEM32(ecx);
    PUSH32(esp, esi);
    { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0013FEE8u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0013FEE5u); } /* indirect call */
    }

loc_0013FEE8: ;
    edx = MEM32(ebp + 0xC);
    ebx = eax;
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0013FEF2u); RECOMP_ABI_CALL(0x001C7E60u, sub_001C7E60); /* call 0x001C7E60 */]=])
        else()
            set(block [=[    eax = ebp + -40;
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, eax);
    eax = ebp + -4;
    PUSH32(esp, eax);
    MEM32(ebp + -24) = ecx;
    ecx = MEM32(esi + 4);
    eax = ebp + -28;
    PUSH32(esp, eax);
    eax = MEM32(ecx);
    { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0014E8B9u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E8B6u); } /* indirect call */
    }

loc_0014E8B9: ;
    edx = ebp + -16;
    ebx = eax;
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0014E8C3u); RECOMP_ABI_CALL(0x001C7E60u, sub_001C7E60); /* call 0x001C7E60 */]=])
        endif()
        string(REPLACE "\n" "${newline}" block "${block}")
        string(REGEX MATCHALL "0x${site}u" matches "${body}")
        list(LENGTH matches site_count)
        string(FIND "${body}" "${block}" block_start)
        if(NOT site_count EQUAL 1 OR block_start LESS 0)
            message(FATAL_ERROR "MM3 collision argument cleanup: original ${function_name} PUSH3/reuse at 0x${site} is missing, duplicated or changed")
        endif()
        string(REPLACE "RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x${site}u)"
            "RECOMP_ICALL_SAFE_AT_CC(_icall_target, _icall_esp, 0x${site}u)"
            fixed_block "${block}")
        string(REPLACE "${block}" "${fixed_block}" transformed "${transformed}")
    endforeach()
    string(REPLACE "${body}" "${transformed}" content "${content}")
    set(${source_variable} "${content}" PARENT_SCOPE)
endfunction()
