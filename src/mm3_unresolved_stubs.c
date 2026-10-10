#include <stdio.h>
#include "recomp_types.h"

/* Unresolved call targets (recomp_stubs_unresolved.c) are addresses the
 * translation never generated. A stub returns without running the body,
 * so each one reached at run time is a missing function: logged once per
 * address with its caller, for mm3_runtime_entry_repairs.json. */
int mm3_stub_hit(uint32_t va)
{
    static uint32_t seen[512];
    static int n;
    int i;
    for (i = 0; i < n; i++)
        if (seen[i] == va) return 0;
    if (n < 512) seen[n++] = va;
    fprintf(stderr, "[STUB] unresolved 0x%08X reached from 0x%08X\n", va, MEM32(g_esp));
    fflush(stderr);
    return 0;
}
