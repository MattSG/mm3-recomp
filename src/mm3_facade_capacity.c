#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "recomp_types.h"
#include "mm3_render_distance.h"

extern void sub_001633B7_original(void);

/* PAL facade streams use the capacities below for both allocation checks and
 * actual VB creation. The allocation constructor reads them before requesting
 * two 28-byte opaque/alpha VBs and two 24-byte far VBs. Change them only there;
 * increasing an already allocated pool's counter would write beyond its VB.
 *
 * More visible facade cells need more space as the view area grows. Bound the
 * multiplier at four: the largest stream has 49,152 vertices, safely below the
 * guest draw commands' 16-bit starting vertex. The six VBs together grow by
 * 2,482,176 bytes at the default 2x distance. Detail descriptors, shaders and
 * the separate 32-byte detail stream retain their original budgets. */
void sub_001633B7(void)
{
    uint32_t self = g_esi;
    float scale = mm3_draw_distance_scale();
    uint32_t factor = (uint32_t)ceilf(scale * scale);
    uint32_t opaque, alpha, far_vertices;

    if (factor > 4u) factor = 4u;
    if (factor <= 1u) {
        sub_001633B7_original();
        return;
    }
    opaque = MEM32(self + 0x1ED0u);
    alpha = MEM32(self + 0x1F2Cu);
    far_vertices = MEM32(self + 0x1F88u);
    /* Respect custom capacities and repeated creation using expanded pools. */
    if (opaque != 0x3000u || alpha != 0x800u || far_vertices != 0x200u) {
        sub_001633B7_original();
        return;
    }
    MEM32(self + 0x1ED0u) = opaque * factor;
    MEM32(self + 0x1F2Cu) = alpha * factor;
    MEM32(self + 0x1F88u) = far_vertices * factor;
    sub_001633B7_original();
    if (!LO8(g_eax)) {
        /* Failed creation follows the title's existing failure path. Keeping
         * the old limits is conservative even if a partial larger VB exists. */
        MEM32(self + 0x1ED0u) = opaque;
        MEM32(self + 0x1F2Cu) = alpha;
        MEM32(self + 0x1F88u) = far_vertices;
        fprintf(stderr, "[MM3_DISTANCE] facade VB creation failed; retained stock capacities\n");
        return;
    }
    fprintf(stderr,
            "[MM3_DISTANCE] facade VB capacity multiplier %u opaque28=%u alpha28=%u far24=%u extra_bytes=%u\n",
            factor, opaque * factor, alpha * factor, far_vertices * factor,
            2u * (factor - 1u) * ((opaque + alpha) * 28u + far_vertices * 24u));
}
