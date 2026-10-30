#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#if !defined(_WIN32)
#define _putenv_s(k,v) (*(v) ? setenv(k,v,1) : unsetenv(k))
#endif
#include "../src/mods/render_policy.c"
const uint32_t (*nv2a_vsh_program(uint32_t *start, uint32_t *version))[4] { (void)start; (void)version; return NULL; }
const float (*nv2a_vsh_constants(uint32_t *version))[4] { (void)version; return NULL; }

static int enabled = 1;
int mm3_mod_on(int mod) { (void)mod; return enabled; }
int main(void)
{
    for (unsigned target = 0; target < 4; ++target) {
        float snap = 1, offset = 0;
        uint32_t width = target < 2 ? 640 : 128;
        uint32_t height = (target & 1) ? 128 : 480;
        mm3_render_vertex_alignment(width,height,&snap,&offset);
        assert(snap == (target == 0 ? 0.f : 1.f));
        assert(offset == (target == 0 ? .53125f : 0.f));
    }
    NvD3DState base = {0}, changed;
    unsigned i;
    base.tex_used = 1; base.blend_enable = 1;
    base.blend_sfactor = 0x304; base.blend_dfactor = 0x305;
    base.tex[0].color = 0x0F; base.tex[0].width = base.tex[0].height = 256;
    base.tex[0].filter = (6u << 16) | (2u << 24);
    assert(mm3_render_cubic_filter(&base,0,9));
    assert(!mm3_render_cubic_filter(&base,1,9));
    assert(!mm3_render_cubic_filter(&base,0,4));
    enabled = 0; assert(!mm3_render_cubic_filter(&base,0,9)); enabled = 1;
    for (i = 0; i < 9; ++i) {
        changed = base;
        switch (i) {
        case 0: changed.depth_test = 1; break;
        case 1: changed.blend_enable = 0; break;
        case 2: changed.blend_sfactor = 0x302; break;
        case 3: changed.blend_dfactor = 0x303; break;
        case 4: changed.tex[0].width = 128; break;
        case 5: changed.tex[0].height = 128; break;
        case 6: changed.tex[0].color = 0x0C; break;
        case 7: changed.tex[0].filter = (5u << 16) | (2u << 24); break;
        case 8: changed.tex[0].cube = 1; break;
        }
        assert(!mm3_render_cubic_filter(&changed,0,9));
    }
    changed = base; changed.tex[0].filter = (6u << 16) | (1u << 24);
    assert(!mm3_render_cubic_filter(&changed,0,9));
    return 0;
}
