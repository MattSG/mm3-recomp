#include <stdio.h>
#include <stdlib.h>
#include "../../tools/xboxrecomp/src/kernel/nv2a_vsh_interp.h"
#include "render_policy.h"
#include "mods.h"

static uint32_t gpu_address(uint32_t address)
{
    return address >= 0x80000000u ? address - 0x80000000u : address;
}

int mm3_render_force_depth(const NvD3DState *st, uint32_t width, uint32_t height)
{
    static int noted;
    static unsigned traced;
    uint32_t address, reject = 0;
    unsigned n;
    /* tex_used describes only the fixed-function fallback in the pushbuffer
     * executor. Register-combiner draws select sampled stages in stage_program,
     * selected here from the authored Flares/Flares.psh stage program. */
    if (!mm3_mod_on(MM3_MOD_FLARE_DEPTH) || !st->rc_seen || st->rc.stage_program != 0x8421u)
        return 0;
    address = gpu_address(st->tex[0].addr);
    if (!address || gpu_address(st->tex[3].addr) == address) return 0;
    if (st->depth_test || st->depth_mask || !st->zeta_addr) reject |= 1u;
    if (!st->blend_enable || st->blend_dfactor != 1u || st->blend_equation != 0x8006u)
        reject |= 2u;
    if (!width || !height || width != st->clip_w || height != st->clip_h) reject |= 4u;
    /* The flare shader: t0..t2 sample the screen visibility mask;
     * t3 samples the lamp texture. Four framebuffer taps are bloom, and
     * one/two textures can be HUD or road light passes: none qualify. */
    for (n = 0; n < 3; ++n) {
        const NvD3DTexture *t = &st->tex[n];
        if (gpu_address(t->addr) != address || t->cube || t->color != 0x12u ||
            ((t->filter >> 24) & 15u) != 2u)
            reject |= 8u;
    }
    if (traced < 4 && getenv("RECOMP_FLARE_TRACE") &&
        gpu_address(st->tex[1].addr) == address && gpu_address(st->tex[2].addr) == address) {
        fprintf(stderr, "[MM3_RENDER] lamp pass reject=%X clip=%ux%u mask=%ux%u depth=%u write=%u zeta=%08X wbuffer=%u tex_used=%X filter=%08X/%08X/%08X address=%08X/%08X/%08X\n",
                reject, st->clip_w, st->clip_h, width, height,
                st->depth_test, st->depth_mask, st->zeta_addr, st->z_perspective,
                st->tex_used, st->tex[0].filter, st->tex[1].filter, st->tex[2].filter,
                st->tex[0].address, st->tex[1].address, st->tex[2].address);
        if (!traced) {
            uint32_t start = 0, version = 0, cver = 0, k;
            const uint32_t (*prog)[4] = nv2a_vsh_program(&start, &version);
            const float (*consts)[4] = nv2a_vsh_constants(&cver);
            if (prog && consts) {
                for (k = start; k < start + 16 && k < 136u; ++k) {
                    fprintf(stderr, "[MM3_RENDER] lamp vp %u %08X %08X %08X %08X\n",
                            k - start, prog[k][0], prog[k][1], prog[k][2], prog[k][3]);
                    if (prog[k][3] & 1u) break;
                }
                for (k = 0; k < 2; ++k)
                    fprintf(stderr, "[MM3_RENDER] lamp c%u %.9g %.9g %.9g %.9g\n", k,
                            consts[k][0], consts[k][1], consts[k][2], consts[k][3]);
            }
        }
        ++traced;
    }
    if (reject) return 0;
    if (!noted) {
        fprintf(stderr, "[MM3_RENDER] lamp visibility sprites use scene depth\n");
        noted = 1;
    }
    return 1;
}

int mm3_render_cubic_filter(const NvD3DState *st, int source_is_target, uint32_t levels)
{
    const NvD3DTexture *t = &st->tex[0];
    unsigned min_filter = (t->filter >> 16) & 255u;
    /* MM3's authored horizon panorama, magnified through destination alpha. */
    return mm3_mod_on(MM3_MOD_SKY) && !source_is_target && !t->cube && t->color == 0x0Fu &&
        t->width == 256 && t->height == 256 && levels > 4 &&
        (min_filter == 2 || min_filter == 4 || min_filter == 6) &&
        ((t->filter >> 24) & 15u) != 1 && !st->depth_test && st->blend_enable &&
        st->blend_sfactor == 0x304 && st->blend_dfactor == 0x305;
}

/* Called and consumed on the renderer thread, including flip resets. */
static float loading_phase;
float mm3_loading_phase(void) { return loading_phase; }
int mm3_render_idle(uint32_t now_ms, uint32_t idle_ms, int movie)
{
    /* Read once: the renderer asks on every idle present. */
    static int spinner = -1;
    if (spinner < 0) {
        const char *enabled = getenv("RECOMP_LOADING_SPINNER");
        spinner = !enabled || atoi(enabled) != 0;
    }
    loading_phase = spinner && !movie && idle_ms > 700
        ? 1.f + (float)(now_ms % 1000) / 1000.f : 0.f;
    return loading_phase > 0;
}

/* The authored display passes need position and texture interpolation shifted
 * together; offscreen ripple targets keep pixel-zero edge snapping. */
void mm3_render_vertex_alignment(uint32_t width, uint32_t height, float *snap, float *offset)
{
    if (width == 640 && height == 480) {
        *snap = 0.0f;
        *offset = 0.53125f;
    }
}
