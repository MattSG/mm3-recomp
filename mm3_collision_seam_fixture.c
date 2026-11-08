void sub_0013FE96(void)
{
    uint32_t ebp = 0;
    ebp = g_ebp;  /* prologue saves caller's frame */
    int _flags = 0; /* fallback flag var */
    uint32_t _fa = 0, _fb = 0;
    int32_t _fas = 0, _fbs = 0;
    (void)_fa; (void)_fb; (void)_fas; (void)_fbs;
    #define fp_push(v) do { double _fp_value = (v); \
        g_fp_top = (g_fp_top + 7u) & 7u; \
        g_fp_stack[g_fp_top] = _fp_value; } while (0)
    #define fp_pop() (g_fp_top = (g_fp_top + 1u) & 7u)
    #define fp_top() g_fp_stack[g_fp_top]
    #define fp_st(i) g_fp_stack[(g_fp_top + (i)) & 7u]
    #define fp_st1() fp_st(1)

loc_0013FE96: ;
    PUSH32(esp, ebp);
    ebp = esp;
    g_ebp = ebp; /* publish frame for frameless callees */
    g_seh_ebp = ebp;
    _fb = (uint32_t)(0x1C) & 0xFFFFFFFFu; _fbs = (int32_t)(int32_t)(_fb); /* sub source, before the write */
    esp = esp - 0x1C;
    _fa = (uint32_t)(esp) & 0xFFFFFFFFu; _fas = (int32_t)(int32_t)(_fa); /* sub result */
    PUSH32(esp, ebx);
    fp_push(0.0); /* fldz */
    PUSH32(esp, esi);
    MEMF(ebp + -4) = (float)fp_top(); fp_pop(); /* fstp */
    esi = eax;
    PUSH32(esp, edi);
    eax = ebp + -28;
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, eax);
    eax = ebp + -4;
    edi = ecx;
    ecx = MEM32(ebp + 0x10);
    PUSH32(esp, eax);
    eax = MEM32(ecx);
    PUSH32(esp, esi);
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0013FEB9u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0013FEB6u); } /* indirect call */
    }

loc_0013FEB9: ;
    edx = MEM32(ebp + 0xC);
    ebx = eax;
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0013FEC3u); RECOMP_ABI_CALL(0x001C7E60u, sub_001C7E60); /* call 0x001C7E60 */

loc_0013FEC3: ;
    _fa = (uint32_t)(LO8(eax)) & 0xFFu; _fb = (uint32_t)(0) & 0xFFu;
    _fas = (int32_t)(int8_t)(_fa); _fbs = (int32_t)(int8_t)(_fb); /* cmp LO8(eax), 0 (8-bit) */
    if (CMP_EQ(_fa, _fb)) goto loc_0013FED7; /* je: equal / zero */

loc_0013FEC7: ;
    fp_push(MEMF(ebp + -4)); /* fld float */
    g_fp_cmp = RECOMP_FCMP(fp_top(), MEMF(0x384EF4)); g_fp_cc = RECOMP_FCMP_CC(g_fp_cmp); fp_pop(); /* fcomp dword ptr [0x384ef4] */
    eax = (eax & 0xFFFF0000u) | (uint32_t)(uint16_t)(((g_fp_top & 7u) << 11) | g_fp_cc); /* fnstsw ax <- fpu status */
    _fa = (uint32_t)(HI8(eax)) & 0xFFu; _fb = (uint32_t)(0x44) & 0xFFu;
    _fas = (int32_t)(int8_t)(_fa); _fbs = (int32_t)(int8_t)(_fb); /* test HI8(eax), 0x44 (8-bit) */
    if (RECOMP_PARITY8((_fa) & (_fb))) goto loc_0013FF06; /* jp: parity */

loc_0013FED7: ;
    ecx = MEM32(ebp + 0x14);
    eax = ebp + -28;
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, eax);
    eax = ebp + -4;
    PUSH32(esp, eax);
    eax = MEM32(ecx);
    PUSH32(esp, esi);
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0013FEE8u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0013FEE5u); } /* indirect call */
    }

loc_0013FEE8: ;
    edx = MEM32(ebp + 0xC);
    ebx = eax;
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0013FEF2u); RECOMP_ABI_CALL(0x001C7E60u, sub_001C7E60); /* call 0x001C7E60 */

loc_0013FEF2: ;
    _fa = (uint32_t)(LO8(eax)) & 0xFFu; _fb = (uint32_t)(0) & 0xFFu;
    _fas = (int32_t)(int8_t)(_fa); _fbs = (int32_t)(int8_t)(_fb); /* cmp LO8(eax), 0 (8-bit) */
    if (CMP_EQ(_fa, _fb)) goto loc_0013FF53; /* je: equal / zero */

loc_0013FEF6: ;
    fp_push(MEMF(ebp + -4)); /* fld float */
    g_fp_cmp = RECOMP_FCMP(fp_top(), MEMF(0x384EF4)); g_fp_cc = RECOMP_FCMP_CC(g_fp_cmp); fp_pop(); /* fcomp dword ptr [0x384ef4] */
    eax = (eax & 0xFFFF0000u) | (uint32_t)(uint16_t)(((g_fp_top & 7u) << 11) | g_fp_cc); /* fnstsw ax <- fpu status */
    _fa = (uint32_t)(HI8(eax)) & 0xFFu; _fb = (uint32_t)(0x44) & 0xFFu;
    _fas = (int32_t)(int8_t)(_fa); _fbs = (int32_t)(int8_t)(_fb); /* test HI8(eax), 0x44 (8-bit) */
    if ((!RECOMP_PARITY8((_fa) & (_fb)))) goto loc_0013FF53; /* jnp: not parity */

loc_0013FF06: ;
    fp_push(MEMF(ebp + -4)); /* fld float */
    eax = MEM32(ebp + 0xC);
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(esi)); /* fmul dword ptr [esi] */
    fp_push(MEMF(ebp + -4)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(esi + 4)); /* fmul dword ptr [esi + 4] */
    fp_push(MEMF(ebp + -4)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(esi + 8)); /* fmul dword ptr [esi + 8] */
    MEMF(ebp + -20) = (float)fp_top(); fp_pop(); /* fstp */
    fp_push(1.0); /* fld1 */
    fp_top() = RECOMP_FP_PC(fp_top() - MEMF(ebp + -4)); /* fsub dword ptr [ebp - 4] */
    { double _t = fp_top(); fp_push(_t); } /* fld st(0) */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(eax)); /* fmul dword ptr [eax] */
    { double _t = fp_st1(); fp_push(_t); } /* fld st(1) */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(eax + 4)); /* fmul dword ptr [eax + 4] */
    MEMF(ebp + -12) = (float)fp_top(); fp_pop(); /* fstp */
    { double _t = fp_st1(); fp_push(_t); } /* fld st(1) */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(eax + 8)); /* fmul dword ptr [eax + 8] */
    MEMF(ebp + -8) = (float)fp_top(); fp_pop(); /* fstp */
    fp_top() = RECOMP_FP_PC(fp_top() + g_fp_stack[(g_fp_top + 3) & 7]); /* fadd st(3) */
    MEMF(edi) = (float)fp_top(); fp_pop(); /* fstp */
    fp_pop(); /* fstp st(0) */
    fp_push(MEMF(ebp + -12)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() + fp_st1()); /* fadd st(1) */
    MEMF(edi + 4) = (float)fp_top(); fp_pop(); /* fstp */
    fp_pop(); /* fstp st(0) */
    fp_pop(); /* fstp st(0) */
    fp_push(MEMF(ebp + -8)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() + MEMF(ebp + -20)); /* fadd dword ptr [ebp - 0x14] */
    MEMF(edi + 8) = (float)fp_top(); fp_pop(); /* fstp */
    goto loc_0013FF65;

loc_0013FF53: ;
    ecx = MEM32(ebp + 0x10);
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, MEM32(ebp + 0x14));
    eax = MEM32(ecx);
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x64); PUSH32(esp, 0x0013FF5Eu); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0013FF5Bu); } /* indirect call */
    }

loc_0013FF5E: ;
    esi = eax;
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0013FF65u); RECOMP_ABI_CALL(0x0014E808u, sub_0014E808); /* call 0x0014E808 */

loc_0013FF65: ;
    eax = edi;
    POP32(esp, edi);
    POP32(esp, esi);
    POP32(esp, ebx);
    esp = ebp;
    POP32(esp, ebp); /* leave */
    g_ebp = ebp; g_seh_ebp = ebp; esp += 20; return; /* ret 16 */

    #undef fp_push
    #undef fp_pop
    #undef fp_top
    #undef fp_st
    #undef fp_st1
}

void sub_0014E808(void)
{
    uint32_t ebp = 0;
    ebp = g_ebp;  /* prologue saves caller's frame */
    int _flags = 0; /* fallback flag var */
    uint32_t _fa = 0, _fb = 0;
    int32_t _fas = 0, _fbs = 0;
    (void)_fa; (void)_fb; (void)_fas; (void)_fbs;
    #define fp_push(v) do { double _fp_value = (v); \
        g_fp_top = (g_fp_top + 7u) & 7u; \
        g_fp_stack[g_fp_top] = _fp_value; } while (0)
    #define fp_pop() (g_fp_top = (g_fp_top + 1u) & 7u)
    #define fp_top() g_fp_stack[g_fp_top]
    #define fp_st(i) g_fp_stack[(g_fp_top + (i)) & 7u]
    #define fp_st1() fp_st(1)

loc_0014E808: ;
    PUSH32(esp, ebp);
    ebp = esp;
    g_ebp = ebp; /* publish frame for frameless callees */
    g_seh_ebp = ebp;
    _fb = (uint32_t)(0x28) & 0xFFFFFFFFu; _fbs = (int32_t)(int32_t)(_fb); /* sub source, before the write */
    esp = esp - 0x28;
    _fa = (uint32_t)(esp) & 0xFFFFFFFFu; _fas = (int32_t)(int32_t)(_fa); /* sub result */
    ecx = MEM32(esi);
    eax = MEM32(ecx);
    { uint32_t _icall_esp = g_esp;
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x70); PUSH32(esp, 0x0014E815u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E812u); } /* indirect call */
    }

loc_0014E815: ;
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fb = (uint32_t)(1) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp eax, 1 (32-bit) */
    if (CMP_NE(_fa, _fb)) goto loc_0014E838; /* jne: not equal / not zero */

loc_0014E81A: ;
    ecx = MEM32(esi + 4);
    eax = MEM32(ecx);
    { uint32_t _icall_esp = g_esp;
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x70); PUSH32(esp, 0x0014E822u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E81Fu); } /* indirect call */
    }

loc_0014E822: ;
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fb = (uint32_t)(2) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp eax, 2 (32-bit) */
    if (CMP_NE(_fa, _fb)) goto loc_0014E838; /* jne: not equal / not zero */

loc_0014E827: ;
    ecx = MEM32(esi);
    eax = MEM32(ecx);
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, 0);
    PUSH32(esp, 0);
    PUSH32(esp, edi);
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x54); PUSH32(esp, 0x0014E833u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E830u); } /* indirect call */
    }

loc_0014E833: ;
    goto loc_0014E904;

loc_0014E838: ;
    ecx = MEM32(esi + 4);
    eax = MEM32(ecx);
    { uint32_t _icall_esp = g_esp;
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x70); PUSH32(esp, 0x0014E840u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E83Du); } /* indirect call */
    }

loc_0014E840: ;
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fb = (uint32_t)(1) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp eax, 1 (32-bit) */
    if (CMP_NE(_fa, _fb)) goto loc_0014E86D; /* jne: not equal / not zero */

loc_0014E845: ;
    ecx = MEM32(esi);
    eax = MEM32(ecx);
    { uint32_t _icall_esp = g_esp;
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x70); PUSH32(esp, 0x0014E84Cu); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E849u); } /* indirect call */
    }

loc_0014E84C: ;
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fb = (uint32_t)(2) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fbs = (int32_t)(int32_t)(_fb); /* cmp eax, 2 (32-bit) */
    if (CMP_NE(_fa, _fb)) goto loc_0014E86D; /* jne: not equal / not zero */

loc_0014E851: ;
    ecx = MEM32(esi + 4);
    eax = MEM32(ecx);
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, 0);
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x4C); PUSH32(esp, 0x0014E85Bu); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E858u); } /* indirect call */
    }

loc_0014E85B: ;
    ecx = MEM32(esi + 4);
    edx = MEM32(ecx);
    eax--;
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu;
    _fas = (int32_t)(int32_t)(_fa); _fb = (_fa == 0x7FFFFFFFu); /* dec result/SF/OF; CF unchanged */
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, eax);
    PUSH32(esp, 0);
    PUSH32(esp, edi);
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(edx + 0x54); PUSH32(esp, 0x0014E868u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E865u); } /* indirect call */
    }

loc_0014E868: ;
    goto loc_0014E904;

loc_0014E86D: ;
    ecx = MEM32(esi);
    eax = MEM32(ecx);
    PUSH32(esp, ebx);
    { uint32_t _icall_esp = g_esp;
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0014E875u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E872u); } /* indirect call */
    }

loc_0014E875: ;
    _fb = (uint32_t)(0x24) & 0xFFFFFFFFu; _fbs = (int32_t)(int32_t)(_fb); /* add source, before the write */
    eax = eax + 0x24;
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fas = (int32_t)(int32_t)(_fa); /* add result */
    ecx = MEM32(eax);
    MEM32(ebp + -16) = ecx;
    ecx = MEM32(eax + 4);
    eax = MEM32(eax + 8);
    MEM32(ebp + -12) = ecx;
    ecx = MEM32(esi + 4);
    MEM32(ebp + -8) = eax;
    eax = MEM32(ecx);
    { uint32_t _icall_esp = g_esp;
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0014E891u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E88Eu); } /* indirect call */
    }

loc_0014E891: ;
    _fb = (uint32_t)(0x24) & 0xFFFFFFFFu; _fbs = (int32_t)(int32_t)(_fb); /* add source, before the write */
    eax = eax + 0x24;
    _fa = (uint32_t)(eax) & 0xFFFFFFFFu; _fas = (int32_t)(int32_t)(_fa); /* add result */
    ecx = MEM32(eax);
    MEM32(ebp + -28) = ecx;
    ecx = MEM32(eax + 4);
    eax = MEM32(eax + 8);
    MEM32(ebp + -20) = eax;
    eax = ebp + -40;
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, eax);
    eax = ebp + -4;
    PUSH32(esp, eax);
    MEM32(ebp + -24) = ecx;
    ecx = MEM32(esi + 4);
    eax = ebp + -28;
    PUSH32(esp, eax);
    eax = MEM32(ecx);
    g_ebp = ebp; g_seh_ebp = ebp; { uint32_t _icall_target = MEM32(eax + 0x5C); PUSH32(esp, 0x0014E8B9u); RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0014E8B6u); } /* indirect call */
    }

loc_0014E8B9: ;
    edx = ebp + -16;
    ebx = eax;
    g_ebp = ebp; /* frame stays current across calls */
    g_seh_ebp = ebp;
    PUSH32(esp, 0x0014E8C3u); RECOMP_ABI_CALL(0x001C7E60u, sub_001C7E60); /* call 0x001C7E60 */

loc_0014E8C3: ;
    fp_push(MEMF(ebp + -28)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() - MEMF(ebp + -16)); /* fsub dword ptr [ebp - 0x10] */
    POP32(esp, ebx);
    fp_push(MEMF(ebp + -24)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() - MEMF(ebp + -12)); /* fsub dword ptr [ebp - 0xc] */
    fp_push(MEMF(ebp + -20)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() - MEMF(ebp + -8)); /* fsub dword ptr [ebp - 8] */
    MEMF(ebp + -20) = (float)fp_top(); fp_pop(); /* fstp */
    { double _t = fp_st1(); fp_push(_t); } /* fld st(1) */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(ebp + -4)); /* fmul dword ptr [ebp - 4] */
    MEMF(ebp + -40) = (float)fp_top(); fp_pop(); /* fstp */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(ebp + -4)); /* fmul dword ptr [ebp - 4] */
    fp_st1() = fp_top(); fp_pop(); /* fstp st(1) */
    fp_push(MEMF(ebp + -20)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() * MEMF(ebp + -4)); /* fmul dword ptr [ebp - 4] */
    fp_push(MEMF(ebp + -40)); /* fld float */
    fp_top() = RECOMP_FP_PC(fp_top() + MEMF(ebp + -16)); /* fadd dword ptr [ebp - 0x10] */
    MEMF(edi) = (float)fp_top(); fp_pop(); /* fstp */
    { double _t = fp_st1(); fp_push(_t); } /* fld st(1) */
    fp_top() = RECOMP_FP_PC(fp_top() + MEMF(ebp + -12)); /* fadd dword ptr [ebp - 0xc] */
    MEMF(edi + 4) = (float)fp_top(); fp_pop(); /* fstp */
    fp_top() = RECOMP_FP_PC(fp_top() + MEMF(ebp + -8)); /* fadd dword ptr [ebp - 8] */
    MEMF(edi + 8) = (float)fp_top(); fp_pop(); /* fstp */
    fp_pop(); /* fstp st(0) */

loc_0014E904: ;
    eax = edi;
    esp = ebp;
    POP32(esp, ebp); /* leave */
    g_ebp = ebp; g_seh_ebp = ebp; esp += 4; return; /* ret */

    #undef fp_push
    #undef fp_pop
    #undef fp_top
    #undef fp_st
    #undef fp_st1
}

void unrelated(void)
{
    RECOMP_ICALL_SAFE_AT(_icall_target, _icall_esp, 0x0013FEB6u);
}
