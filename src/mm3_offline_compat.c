#include <stdio.h>
#include "recomp_types.h"

/* Temporary offline compatibility: MM3's network object's readiness method
 * masks bit 0x20 from XNetGetTitleXnAddr and returns whether any other bit is
 * set. The initialization method at 0x195341 polls this virtual method every
 * 100 ms. With the incomplete guest network stack it never finishes, blocking
 * frontend initialization after the movies. Preserve network initialization
 * and bypass only its readiness predicate. This does not implement networking;
 * disable MM3_OFFLINE_XNET to restore the generated method for future work.
 * Evidence: post-intro worker stack contains 0x1953A0 and repeatedly calls
 * 0x19531D while no further frontend resources are opened.
 */
void sub_0019531D(void)
{
    static int reported;
    if (!reported) {
        reported = 1;
        fprintf(stderr, "[MM3_OFFLINE] network readiness forced to 1\n");
    }
    g_eax = 1;
    g_esp += 4; /* original cdecl ret, no arguments */
}
