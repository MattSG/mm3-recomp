#include <stdio.h>
#include <stdlib.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"

/* Unresolved call targets (recomp_stubs_unresolved.c) are addresses the
 * translation never generated. A stub returns without running the body,
 * so each one reached at run time is a missing function. Stop before the
 * generated stub guesses stack cleanup and pretends its body executed. */
int mm3_stub_hit(uint32_t va)
{
    uint32_t caller = 0;
    SIZE_T copied = 0;
#if defined(_WIN32)
    if (g_esp >= 0x10000u && g_esp <= 0x07FFFFFCu)
        ReadProcessMemory(GetCurrentProcess(), (const void *)XBOX_PTR(g_esp),
                          &caller, sizeof caller, &copied);
#endif
    fprintf(stderr, "[STUB] unresolved 0x%08X reached from 0x%08X (esp=0x%08X); stopping\n",
            va, copied == sizeof caller ? caller : 0, g_esp);
    fflush(stderr);
    abort();
}

/* RECOMP_DEAD_BRANCH (mm3_recomp_types.h): a conditional jump whose target
 * label is outside its function, reached with its condition true. This is a
 * residual translation failure, never a valid fallthrough. MEM32(esp) is not a
 * return address here (this is mid-function), so no caller is reported. */
int mm3_dead_branch_hit(uint32_t target)
{
    fprintf(stderr, "[DEAD-BRANCH] taken jump to 0x%08X, label outside its function (esp=0x%08X); stopping\n",
            target, g_esp);
    fflush(stderr);
    abort();
}
