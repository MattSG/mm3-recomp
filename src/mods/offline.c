#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "mods.h"

/* Temporary offline compatibility: MM3's network object's readiness method
 * masks bit 0x20 from XNetGetTitleXnAddr and returns whether any other bit is
 * set. The initialization method at 0x195341 polls this virtual method every
 * 100 ms. With the incomplete guest network stack it never finishes, blocking
 * frontend initialization after the movies. Preserve network initialization
 * and bypass only its readiness predicate. This does not implement networking;
 * With RECOMP_NET enabled, keep the original readiness check. The build
 * option MM3_OFFLINE_XNET or mod MM3_MOD_OFFLINE can disable the bypass.
 * Evidence: post-intro worker stack contains 0x1953A0 and repeatedly calls
 * 0x19531D while no further frontend resources are opened.
 */
extern void sub_0019531D_original(void);
void sub_0019531D(void)
{
    static int reported;
    static int bypass = -1;
    if (bypass < 0) {
        const char *mode = getenv("RECOMP_NET");
        bypass = mm3_mod_on(MM3_MOD_OFFLINE) && (!mode || !*mode || !strcmp(mode, "0"));
    }
    if (!bypass) {
        sub_0019531D_original();
        return;
    }
    if (!reported) {
        reported = 1;
        fprintf(stderr, "[MM3_OFFLINE] network readiness forced to 1\n");
    }
    g_eax = 1;
    g_esp += 4; /* original cdecl ret, no arguments */
}
