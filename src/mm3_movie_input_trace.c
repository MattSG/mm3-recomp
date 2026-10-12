#include <stdio.h>
#include <stdint.h>
#include "recomp_types.h"

/* MM3_MOVIE_INPUT_TRACE: USB enumeration as the movies see it. The movie
 * boundaries, frames and skip decisions are reported by the skip-intro mod's
 * hooks (src/mods/skip_intro.c) in this build. Observation only. */
void sub_0035BDC0_original(void);

void sub_0035BDC0(void)
{
    uint32_t request = MEM32(g_esp + 4u);
    fprintf(stderr, "[USB_ENUM] status=%08X length=%u descriptor=%08X/%08X cancelled=%u\n",
            MEM32(request + 4u), MEM32(request + 0x14u),
            MEM32(0x3BD9E4u), MEM32(0x3BD9E8u), MEM8(0x3BD961u));
    sub_0035BDC0_original();
}
