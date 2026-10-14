# Select the struck tree before the native speed/hop gate. Its first impact
# still uses native solid resistance/effects; the wrapper retires its instance.
function(mm3_add_tree_contacts source_variable)
    set(content "${${source_variable}}")
    set(original [=[    MEM32(ebp + 0x64) = ecx;
    _fa = (uint32_t)(HI8(eax)) & 0xFFu; _fb = (uint32_t)(0x41) & 0xFFu;
    _fas = (int32_t)(int8_t)(_fa); _fbs = (int32_t)(int8_t)(_fb); /* test HI8(eax), 0x41 (8-bit) */
    if (TEST_Z(_fa, _fb)) goto loc_001D1723;]=])
    set(replacement [=[    MEM32(ebp + 0x64) = ecx;
    if (mm3_tree_use_solid_contact(ecx, MEMF(ebp + 0x6C))) goto loc_001D1723;
    _fa = (uint32_t)(HI8(eax)) & 0xFFu; _fb = (uint32_t)(0x41) & 0xFFu;
    _fas = (int32_t)(int8_t)(_fa); _fbs = (int32_t)(int8_t)(_fb); /* test HI8(eax), 0x41 (8-bit) */
    if (TEST_Z(_fa, _fb)) goto loc_001D1723;]=])
    string(FIND "${content}" "\r\n" crlf)
    if(NOT crlf LESS 0)
        string(REPLACE "\n" "\r\n" original "${original}")
        string(REPLACE "\n" "\r\n" replacement "${replacement}")
    endif()
    string(FIND "${content}" "${original}" position)
    string(FIND "${content}" "mm3_tree_use_solid_contact" already)
    if(position LESS 0 OR NOT already LESS 0)
        message(FATAL_ERROR "MM3 tree contact: original hop boundary changed or already patched")
    endif()
    string(LENGTH "${original}" length)
    math(EXPR after "${position} + ${length}")
    string(SUBSTRING "${content}" ${after} -1 tail)
    string(FIND "${tail}" "${original}" duplicate)
    if(NOT duplicate LESS 0)
        message(FATAL_ERROR "MM3 tree contact: duplicate hop boundary")
    endif()
    string(REPLACE "${original}" "${replacement}" content "${content}")
    set(declaration "#include <stdint.h>\nextern int mm3_tree_use_solid_contact(uint32_t, float);\n")
    if(NOT crlf LESS 0)
        string(REPLACE "\n" "\r\n" declaration "${declaration}")
    endif()
    set(${source_variable} "${declaration}${content}" PARENT_SCOPE)
endfunction()

# The tree billboard builds upright vertices from the transform's translation,
# ignoring its rotated basis. Falling draws must use the full native mesh.
function(mm3_add_tree_mesh source_variable)
    set(content "${${source_variable}}")
    set(original [=[loc_0016B04E: ;
    fp_push(MEMF(ebp + 8)); /* fld float */]=])
    set(replacement [=[loc_0016B04E: ;
    if (mm3_tree_fall_drawing()) goto loc_0016B072;
    fp_push(MEMF(ebp + 8)); /* fld float */]=])
    string(FIND "${content}" "\r\n" crlf)
    if(NOT crlf LESS 0)
        string(REPLACE "\n" "\r\n" original "${original}")
        string(REPLACE "\n" "\r\n" replacement "${replacement}")
    endif()
    string(FIND "${content}" "${original}" position)
    string(FIND "${content}" "mm3_tree_fall_drawing" already)
    set(mesh_boundary [=[loc_0016B072: ;
    esi = MEM32(ebp + 0xC);

loc_0016B075: ;]=])
    if(NOT crlf LESS 0)
        string(REPLACE "\n" "\r\n" mesh_boundary "${mesh_boundary}")
    endif()
    string(FIND "${content}" "${mesh_boundary}" mesh_entry)
    if(position LESS 0 OR NOT already LESS 0 OR mesh_entry LESS 0)
        message(FATAL_ERROR "MM3 tree mesh: billboard boundary changed or already patched")
    endif()
    string(LENGTH "${original}" length)
    math(EXPR after "${position} + ${length}")
    string(SUBSTRING "${content}" ${after} -1 tail)
    string(FIND "${tail}" "${original}" duplicate)
    if(NOT duplicate LESS 0)
        message(FATAL_ERROR "MM3 tree mesh: duplicate billboard boundary")
    endif()
    string(REPLACE "${original}" "${replacement}" content "${content}")
    set(${source_variable} "extern int mm3_tree_fall_drawing(void);\n${content}" PARENT_SCOPE)
endfunction()

# Only the two static-piece RENDER loops may see fallen pieces after collision
# retirement. The native availability bits and all collision producers remain
# untouched. Match complete labelled boundaries so regeneration fails closed.
function(mm3_add_tree_render source_variable owner)
    set(content "${${source_variable}}")
    if(owner STREQUAL "sub_0015BF5A")
        set(original [=[loc_0015BF9D: ;
    eax = MEM32(ebp + -16);
    _fa = (uint32_t)(MEM32(edi + 0xC)) & 0xFFFFFFFFu; _fb = (uint32_t)(eax) & 0xFFFFFFFFu;]=])
        set(replacement [=[loc_0015BF9D: ;
    eax = MEM32(ebp + -16);
    _fa = mm3_tree_render_mask(edi, MEM32(edi + 0xC), 1u); _fb = (uint32_t)(eax) & 0xFFFFFFFFu;]=])
    elseif(owner STREQUAL "sub_0015D2DD")
        set(original [=[loc_0015E417: ;
    eax = MEM32(ebp + -4);
    ecx = MEM32(ebp + -20);
    _fa = (uint32_t)(MEM32(eax + 8)) & 0xFFFFFFFFu; _fb = (uint32_t)(ecx) & 0xFFFFFFFFu;]=])
        set(replacement [=[loc_0015E417: ;
    eax = MEM32(ebp + -4);
    ecx = MEM32(ebp + -20);
    _fa = mm3_tree_render_mask(eax, MEM32(eax + 8), 0u); _fb = (uint32_t)(ecx) & 0xFFFFFFFFu;]=])
    else()
        message(FATAL_ERROR "MM3 tree render: unknown owner ${owner}")
    endif()
    string(FIND "${content}" "\r\n" crlf)
    if(NOT crlf LESS 0)
        string(REPLACE "\n" "\r\n" original "${original}")
        string(REPLACE "\n" "\r\n" replacement "${replacement}")
    endif()
    string(FIND "${content}" "${original}" position)
    if(position LESS 0)
        message(FATAL_ERROR "MM3 tree render: ${owner} availability boundary changed or already patched")
    endif()
    string(LENGTH "${original}" length)
    math(EXPR after "${position} + ${length}")
    string(SUBSTRING "${content}" ${after} -1 tail)
    string(FIND "${tail}" "${original}" duplicate)
    if(NOT duplicate LESS 0)
        message(FATAL_ERROR "MM3 tree render: duplicate ${owner} availability boundary")
    endif()
    string(REPLACE "${original}" "${replacement}" content "${content}")
    set(declaration "extern uint32_t mm3_tree_render_mask(uint32_t, uint32_t, unsigned);\n")
    string(FIND "${content}" "${declaration}" already_declared)
    if(already_declared LESS 0)
        set(content "#include <stdint.h>\n${declaration}${content}")
    endif()
    set(${source_variable} "${content}" PARENT_SCOPE)
endfunction()
