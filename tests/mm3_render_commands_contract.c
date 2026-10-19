#include <assert.h>
#include <string.h>
#include "../src/mods/render_commands.c"
static int (*hook)(const NvD3DState *, uint32_t, uint32_t);
static unsigned calls;
static NvD3DState state;
void nv2a_pb_set_nop_hook(int (*fn)(const NvD3DState *, uint32_t, uint32_t)) { hook = fn; }
void mm3_render_fog(const NvD3DState *st, float scale, float far_clip,
    float start, float end, uint32_t argb, uint32_t flags, const float sky[4])
{
    assert(st == &state && scale == 1 && far_clip == 1000 && start == 300 && end == 900);
    assert(argb == 0xFF123456 && flags == 7);
    assert(sky[0] == 0 && sky[1] == 1 && sky[2] == 0 && sky[3] == 6.25f);
    calls++;
}
int main(void)
{
    uint32_t words[] = {0x3F800000,0x447A0000,0x43960000,0x44610000,
                       0xFF123456,7,0,0x3F800000,0,0x40C80000};
    unsigned i;
    mm3_render_commands_init();
    assert(hook && !hook(&state, 0, 5));
    assert(hook(&state, 0, MM3_FOG_TAG));
    assert(hook(&state, 0, words[0]));
    assert(!hook(NULL, 0, 0));
    assert(!hook(&state, 0, words[1]) && calls == 0);
    assert(!hook(&state, 1, MM3_FOG_TAG));
    assert(!hook(&state, 0, words[0]));
    assert(hook(&state, 0, MM3_FOG_TAG));
    for (i = 0; i < sizeof words / sizeof words[0]; ++i) assert(hook(&state, 0, words[i]));
    assert(calls == 1 && !hook(&state, 0, 5));
    return 0;
}
