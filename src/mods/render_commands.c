#include <string.h>
#include "render_commands.h"

static int render_nop(const NvD3DState *state, uint32_t subchannel, uint32_t value)
{
    static uint32_t words[MM3_FOG_WORDS - 2];
    static unsigned next;
    if (!state || subchannel != 0) { next = 0; return 0; }
    if (!next) { if (value != MM3_FOG_TAG) return 0; next = 1; return 1; }
    words[next++ - 1] = value;
    if (next == MM3_FOG_WORDS - 1) {
        float values[8];
        memcpy(values, words, 4 * sizeof(float));
        memcpy(values + 4, words + 6, 4 * sizeof(float));
        nv2a_d3d_distance_fog(state, values[0], values[1], values[2], values[3],
                             words[4], words[5], values + 4);
        next = 0;
    }
    return 1;
}

void mm3_render_commands_init(void)
{
    nv2a_pb_set_nop_hook(render_nop);
}
