# The retail full-screen speedometer has a fixed, labeled analog dial. Its
# camera maximum fields only calibrate dashboard mesh needles. Reuse the
# game's uncapped split-screen numeric speed path for sport/Koenigsegg HUDs,
# then draw the original analog needle with its original calibration.
# Keep every native text allocation, units conversion and destruction intact.
function(mm3_add_speedometer_readout source_variable)
    set(content "${${source_variable}}")
    set(original_selector [=[loc_0023F82D: ;
    _fa = (uint32_t)(MEM32(ebp + -344)) & 0xFFFFFFFFu; _fb = (uint32_t)(1) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp MEM32(ebp + -344), 1 (32-bit) */
    if (CMP_NE(_fa, _fb)) goto loc_0023F8F8; /* jne: not equal / not zero */]=])
    set(replacement_selector [=[loc_0023F82D: ;
    _fa = (uint32_t)(MEM32(ebp + -344)) & 0xFFFFFFFFu; _fb = (uint32_t)(1) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp MEM32(ebp + -344), 1 (32-bit) */
    /* Main sport/Koenigsegg gauges also show the native numeric speed. */
    if (CMP_NE(_fa, _fb) && MEM32(ebp + -36) != 1u && MEM32(ebp + -36) != 4u)
        goto loc_0023F8F8;]=])
    set(original_rectangle [=[    MEMF(ebp + -104) = (float)fp_top(); fp_pop(); /* fstp */
    fp_push(0.0); /* fldz */
    MEMF(ebp + -92) = (float)fp_top(); fp_pop(); /* fstp */
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0023F870u); RECOMP_ABI_CALL(0x00018CBCu, sub_00018CBC); /* call 0x00018CBC */]=])
    set(replacement_rectangle [=[    MEMF(ebp + -104) = (float)fp_top(); fp_pop(); /* fstp */
    fp_push(0.0); /* fldz */
    MEMF(ebp + -92) = (float)fp_top(); fp_pop(); /* fstp */
    if (MEM32(ebp + -344) != 1u) {
        /* Place the extra readout below the full-screen dial. */
        MEMF(ebp + -108) = MEMF(ebp + -76) + 30.0f;
        MEMF(ebp + -104) = MEMF(ebp + -72) + 144.0f;
    }
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0023F870u); RECOMP_ABI_CALL(0x00018CBCu, sub_00018CBC); /* call 0x00018CBC */]=])
    set(original_continuation [=[loc_0023F8F3: ;
    goto loc_0023FB95;

loc_0023F8F8: ;]=])
    set(replacement_continuation [=[loc_0023F8F3: ;
    if (MEM32(ebp + -344) != 1u) {
        /* The integer formatter used ESI for a guest string; recover the
         * renderer before resuming the unchanged analog needle path. */
        esi = MEM32(ebp + 8);
        goto loc_0023F8F8;
    }
    goto loc_0023FB95;

loc_0023F8F8: ;]=])
    string(FIND "${content}" "\r\n" crlf_position)
    if(NOT crlf_position LESS 0)
        foreach(block selector rectangle continuation)
            string(REPLACE "\n" "\r\n" original_${block} "${original_${block}}")
            string(REPLACE "\n" "\r\n" replacement_${block} "${replacement_${block}}")
        endforeach()
    endif()
    string(FIND "${content}" "void sub_0023F09B(void)" owner_position)
    string(REGEX MATCHALL "loc_0023F82D:" labels "${content}")
    list(LENGTH labels label_count)
    if(owner_position LESS 0 OR NOT label_count EQUAL 1)
        message(FATAL_ERROR "MM3 speedometer readout: original 0x0023F09B HUD draw is missing or duplicated")
    endif()
    foreach(block selector rectangle continuation)
        string(FIND "${content}" "${original_${block}}" block_position)
        if(block_position LESS owner_position)
            message(FATAL_ERROR "MM3 speedometer readout: original ${block} block changed")
        endif()
        string(REPLACE "${original_${block}}" "${replacement_${block}}" content "${content}")
    endforeach()
    set(${source_variable} "${content}" PARENT_SCOPE)
endfunction()
