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

/* The six remainder entries are labels inside the generated 0x93B04 tail, which
 * the generator builds with them as resume entries (MEMMOVE_RESUME). Each wrapper
 * sets g_recomp_resume and runs the tail from that label, so the copy and the
 * epilogue come from the same translated body as the 0x93B04 entry. */
static void resume_remainder(uint32_t entry)
{
    g_recomp_resume = entry;
    sub_00093B04_gen();
}

static void remainder_5(void) { resume_remainder(0x00093B0Cu); }
static void remainder_4(void) { resume_remainder(0x00093B14u); }
static void remainder_3(void) { resume_remainder(0x00093B1Cu); }
static void remainder_2(void) { resume_remainder(0x00093B24u); }
static void remainder_1(void) { resume_remainder(0x00093B2Cu); }
static void remainder_0(void) { resume_remainder(0x00093B3Fu); }

typedef void (*mm3_recomp_func_t)(void);
mm3_recomp_func_t mm3_lookup_memory_tail(uint32_t va)
{
    switch (va) {
    case 0x00093B0Cu: return remainder_5;
    case 0x00093B14u: return remainder_4;
    case 0x00093B1Cu: return remainder_3;
    case 0x00093B24u: return remainder_2;
    case 0x00093B2Cu: return remainder_1;
    case 0x00093B3Fu: return remainder_0;
    default: return (mm3_recomp_func_t)0;
    }
}
