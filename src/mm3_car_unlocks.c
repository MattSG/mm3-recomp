#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"

extern void sub_001DEA30_original(void);

/* PAL vehicle availability refresh. ESI identifies an already loaded catalog
 * entry; +0x29 is its LOCKED byte. The parser and both refresh paths use this
 * function before the menu builds its vehicle list. Keep the original profile
 * evaluation, RET 4 and AL unlock-notification result. The optional override
 * exposes the entry, including hidden bonus cars in the existing list builder.
 * Catalog loading, race classes and paint/profile keys retain their semantics.
 * MM3_UNLOCK_ALL_CARS or the master MM3_UNLOCK_ALL =1 overrides availability. */
/* Only "1" enables (tests/mm3_car_unlock_contract.c); read once, this runs per catalog entry. */
static int env_on(const char *name)
{
    const char *v = getenv(name);
    return v && !strcmp(v, "1");
}

void sub_001DEA30(void)
{
    static int unlock_all;   /* 0 unread, 1 off, 2 on */
    uint32_t vehicle = g_esi;

    if (!unlock_all)
        unlock_all = env_on("MM3_UNLOCK_ALL_CARS") || env_on("MM3_UNLOCK_ALL") ? 2 : 1;
    sub_001DEA30_original();
    if (unlock_all == 2) {
        static int reported;
        if (!reported) {
            fprintf(stderr, "[MM3_CARS] all installed cars available\n");
            fflush(stderr);
            reported = 1;
        }
        MEM8(vehicle + 0x29u) = 0;
    }
}
