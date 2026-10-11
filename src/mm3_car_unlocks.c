#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"

extern void sub_001DEA30_original(void);

/* PAL vehicle availability refresh. ESI identifies an already loaded catalog
 * entry; +0x29 is its LOCKED byte. The parser and both refresh paths use this
 * function before the menu builds its vehicle list. Keep the original profile
 * evaluation, RET 4 and AL unlock-notification result, then expose the entry.
 * Hidden bonus cars are included once unlocked by the existing list builder.
 * Catalog loading, race classes and paint/profile keys retain their semantics.
 * MM3_UNLOCK_ALL_CARS=0 restores the original availability rules. */
void sub_001DEA30(void)
{
    uint32_t vehicle = g_esi;
    const char *setting = getenv("MM3_UNLOCK_ALL_CARS");

    sub_001DEA30_original();
    if (!setting || strcmp(setting, "0")) {
        static int reported;
        if (!reported) {
            fprintf(stderr, "[MM3_CARS] all installed cars available\n");
            fflush(stderr);
            reported = 1;
        }
        MEM8(vehicle + 0x29u) = 0;
    }
}
