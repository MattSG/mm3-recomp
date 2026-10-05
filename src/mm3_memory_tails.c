#include "recomp_types.h"

void sub_00093B04_gen(void);

/* Reverse memmove's seven-DWORD remainder jumps into 0x93AFC through its
 * computed jump table. It is a continuation of the caller's saved frame,
 * not a fresh ABI call. The existing generated 0x93B04 tail handles the
 * remaining six DWORDs and restores that frame. Missing this entry silently
 * dropped the copy and its epilogue when runtime dispatch failed.
 * Original bytes at 0x93AFC: 8B 44 8E 1C 89 44 8F 1C.
 */
void sub_00093AFC(void)
{
    g_eax = MEM32(g_esi + g_ecx * 4u + 0x1Cu);
    MEM32(g_edi + g_ecx * 4u + 0x1Cu) = g_eax;
    sub_00093B04_gen();
}

/* These jump-table targets share 0x93860's saved EBP/EDI/ESI frame.
 * Dispatching them as absent functions loses both the remainder copy and
 * the epilogue. Translate the original descending DWORD and byte copies. */
static void reverse_remainder(unsigned words, int adjust)
{
    uint32_t frame = g_seh_ebp;
    for (unsigned i = words; i != 0; --i) {
        g_eax = MEM32(g_esi + g_ecx * 4u + i * 4u);
        MEM32(g_edi + g_ecx * 4u + i * 4u) = g_eax;
    }
    if (adjust) {
        g_eax = g_ecx * 4u;
        g_esi += g_eax;
        g_edi += g_eax;
    }
    /* 0x93B48 indexes 0, 1, 2, 3 trailing bytes at +3, +2, +1. */
    for (unsigned i = 0; i < g_edx; ++i) {
        SET_LO8(g_eax, MEM8(g_esi + 3u - i));
        MEM8(g_edi + 3u - i) = LO8(g_eax);
    }
    g_eax = MEM32(frame + 8u);
    POP32(g_esp, g_esi);
    POP32(g_esp, g_edi);
    g_esp = frame;
    POP32(g_esp, g_ebp);
    g_esp += 4u;
}

static void reverse_5(void) { reverse_remainder(5, 1); }
static void reverse_4(void) { reverse_remainder(4, 1); }
static void reverse_3(void) { reverse_remainder(3, 1); }
static void reverse_2(void) { reverse_remainder(2, 1); }
static void reverse_1(void) { reverse_remainder(1, 1); }
static void reverse_0(void) { reverse_remainder(0, 0); }

typedef void (*mm3_recomp_func_t)(void);
mm3_recomp_func_t mm3_lookup_memory_tail(uint32_t va)
{
    switch (va) {
    case 0x00093B0Cu: return reverse_5;
    case 0x00093B14u: return reverse_4;
    case 0x00093B1Cu: return reverse_3;
    case 0x00093B24u: return reverse_2;
    case 0x00093B2Cu: return reverse_1;
    case 0x00093B3Fu: return reverse_0;
    default: return (mm3_recomp_func_t)0;
    }
}
