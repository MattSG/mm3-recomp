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
