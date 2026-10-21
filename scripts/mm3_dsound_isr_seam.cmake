# DirectSound raises a counter (+0x2BC of its APU object) before it edits the
# voice lists, and its interrupt leaves an idle voice linked while the counter
# is up. Here the interrupt runs beside the title rather than between two of
# its instructions, so wait out a running one right after each raise
# (xbox_IrqlSyncInterrupts). All seven raises sit in one chunk; fail closed if
# the lifted text changes rather than guessing other sites.
set(MM3_DSOUND_GUARD_FUNCTIONS sub_0027AF28 sub_0027AF8B sub_0027B01F sub_0027B0F5
    sub_0027DECF sub_0027E341)
function(mm3_add_dsound_isr_sync source_variable)
    set(content "${${source_variable}}")
    set(total 0)
    foreach(reg esi eax)
        set(raise "MEM32(${reg} + 0x2BC) = MEM32(${reg} + 0x2BC) + 1;")
        string(REGEX MATCHALL "MEM32\\(${reg} \\+ 0x2BC\\) = MEM32\\(${reg} \\+ 0x2BC\\) \\+ 1" raises "${content}")
        list(LENGTH raises count)
        math(EXPR total "${total} + ${count}")
        string(REPLACE "${raise}" "${raise} xbox_IrqlSyncInterrupts();" content "${content}")
    endforeach()
    if(NOT total EQUAL 7)
        message(FATAL_ERROR "MM3 DirectSound ISR sync: expected 7 voice-list guard raises, found ${total}")
    endif()
    string(FIND "${content}" "\r\n" crlf_position)
    if(crlf_position LESS 0)
        set(newline "\n")
    else()
        set(newline "\r\n")
    endif()
    set(${source_variable} "extern void xbox_IrqlSyncInterrupts(void);${newline}${content}" PARENT_SCOPE)
endfunction()
