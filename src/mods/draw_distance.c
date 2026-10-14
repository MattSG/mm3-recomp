#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "mods.h"

extern void nv2a_d3d_set_depth_view_remap(float scale, float knee);

/* The sky fog ramps (Skies/<sky>_fog.cdds) are indexed by w-buffer depth as a
 * fraction of the far plane, authored for the 700 m city cap: Paris morning
 * haze starts near 0.47 and the sky mask at 0.78 (546 m). Scaling the far
 * plane would push that band past where the city's geometry ends (~900 m at
 * 2x), leaving crisp blocks against the sky. Keep the haze where the Xbox
 * had it and stretch the band from 0.78 out to the new far plane.
 * ponytail: one knee for every sky; read it per ramp if a sky's band differs. */
#define FOG_KNEE 0.78f
/* PAL MM3: increase the title's own visibility/LOD distances before its
 * native D3D11 translation sees geometry. Keep the original fade fractions,
 * frustum construction, streaming and temporary render-pass save/restore.
 * Two knobs, both 1 = Xbox:
 *   RECOMP_DRAW_DISTANCE (default 4): object/pedestrian/traffic LODs, blob
 *     shadows and the particle overdraw cap. Everything stays inside the
 *     Xbox city view distance, so nothing low-detail is exposed.
 *   RECOMP_VIEW_DISTANCE (default 2): city view distance and far plane.
 *     Past the 700 m it was built for the title only has flat untextured
 *     blocks (Paris X 643.2 Z 52.6 down the Seine); the depth-haze remap
 *     fades them into the sky (63% at 700 m, 99.6% at 1400 m), so 4 looks
 *     the same as 2 and only costs more. */
static float env_scale(const char *name, float fallback)
{
    const char *env = getenv(name);
    char *end = NULL;
    float value = env ? strtof(env, &end) : fallback;
    if (env && (end == env || *end || !isfinite(value) || value < 1.0f || value > 4.0f)) {
        fprintf(stderr, "[MM3_DISTANCE] invalid %s; expected 1..4, using %g\n", name, fallback);
        value = fallback;
    }
    return value;
}

static float distance_scale(void)
{
    static float scale;
    if (!scale) {
        scale = mm3_mod_on(MM3_MOD_DRAW_DISTANCE) ? env_scale("RECOMP_DRAW_DISTANCE", 4.0f) : 1.0f;
        fprintf(stderr, "[MM3_DISTANCE] LOD distance multiplier %.3g\n", scale);
    }
    return scale;
}

static float view_scale(void)
{
    static float scale;
    if (!scale) {
        scale = mm3_mod_on(MM3_MOD_DRAW_DISTANCE) ? env_scale("RECOMP_VIEW_DISTANCE", 2.0f) : 1.0f;
        /* MM3_FOG_REMAP=0 keeps the scaled band, for A/B comparisons. */
        if (scale > 1.0f && (!getenv("MM3_FOG_REMAP") || strcmp(getenv("MM3_FOG_REMAP"), "0")))
            nv2a_d3d_set_depth_view_remap(scale, FOG_KNEE);
        fprintf(stderr, "[MM3_DISTANCE] view distance multiplier %.3g\n", scale);
    }
    return scale;
}

static void pedestrian_defaults(void)
{
    static int done;
    if (!done) {
        float scale = distance_scale();
        unsigned i;
        /* XBE defaults are {20,30,40,80,160}. The title's supersampled
         * passes save these effective values and restore them unchanged. */
        for (i = 0; i < 5; ++i) MEMF(0x0039ADC0u + 4 * i) *= scale;
        done = 1;
    }
}

extern void sub_000E2D5B_original(void);
void sub_000E2D5B(void)
{
    uint32_t caller = MEM32(g_esp);
    /* Object-manager construction and the explicit objMSetLodDistance
     * callback receive original distances. 0x211273/0x2114ED instead
     * install and restore already effective values for temporary passes. */
    if (caller == 0x000E4672u || caller == 0x000E2E94u) {
        pedestrian_defaults();
        MEMF(g_esp + 4) *= distance_scale();
    }
    sub_000E2D5B_original();
}

extern void sub_000D9E2E_original(void);
void sub_000D9E2E(void)
{
    sub_000D9E2E_original();
    /* This setter validates all five positive, increasing distances and
     * copies them to the global array. Do not alter failed requests. */
    if (LO8(g_eax)) {
        unsigned i;
        for (i = 0; i < 5; ++i) MEMF(0x0039ADC0u + 4 * i) *= distance_scale();
    }
}

extern void sub_000FE71E_original(void);
void sub_000FE71E(void)
{
    uint32_t caller = MEM32(g_esp);
    /* City construction, DSS loading and renderSetViewDistance. Exclude
     * cubemap passes (0x22CE39/0x22CFE3): their values come from the
     * effective camera distance and their restore must stay exact. */
    if (caller == 0x001002CEu || caller == 0x00100ED6u || caller == 0x00210EDEu)
        MEMF(g_esp + 4) *= view_scale();
    /* City loading applies a title-specific distance cap after reading the
     * DSS. Its local is then passed straight to the camera projection, so
     * update both together rather than leaving the camera at the Xbox cap. */
    if (caller == 0x00224C4Eu) {
        MEMF(g_esp + 4) *= view_scale();
        MEMF(g_ebp + 8) = MEMF(g_esp + 4);
    }
    sub_000FE71E_original();
}

/* Facade zones {80,280}: buildings nearer than the first get the full facade
 * path, nearer than the second the textured mid path, the rest the flat far
 * path. Both scale with the view distance, capped at 2: the mid and near
 * paths fill the facade manager's per-frame vertex pools, and once a pool
 * is full the allocator (sub_00162F75) returns 0 and the building is drawn
 * with only its dark cloud-shadow pass -- the slabs seen at Montmartre
 * (X 1298.7 Z 1167.7 facing 225 degrees) when the zones were scaled alone. */
static float facade_scale(void)
{
    float scale = view_scale();
    return scale > 2.0f ? 2.0f : scale;
}

extern void sub_001630A7_original(void);
void sub_001630A7(void)
{
    uint32_t self;
    sub_001630A7_original();
    /* The constructor sets the pool capacities sub_001633B7 later sizes its
     * vertex buffers from: 0x3000 and 0x800 facade vertices, 0x200 and
     * 0x600 in two side pools. Twice the zone distances cover four times the
     * buildings; 5x keeps the large pool's vertex numbers under 0x10000.
     * About 3.7 MB more guest memory. */
    if (facade_scale() > 1.0f) {
        self = g_eax;
        MEM32(self + 0x1ED0) *= 5;
        MEM32(self + 0x1F2C) *= 5;
        MEM32(self + 0x1F88) *= 5;
        MEM32(self + 0x48) *= 5;
    }
}

extern void sub_001661D0_original(void);
void sub_001661D0(void)
{
    uint32_t caller = MEM32(g_esp);
    /* Facade construction and renderFacadeSetZones (Lua) supply original
     * distances; the supersampled-pass save/restore calls at 0x211220 and
     * 0x21153D already use effective values. */
    if (caller == 0x0015FA1Du || caller == 0x0020FABAu) {
        MEMF(g_esp + 4) *= facade_scale();
        MEMF(g_esp + 8) *= facade_scale();
    }
    sub_001661D0_original();
    {
        float scale = facade_scale();
        float near_distance = MEMF(0x003C04E0u);
        /* Scale the original 10 m / 1 m detail fade offsets as well as the
         * facade zones. Rebuild from the validated effective zone after
         * every setter, including temporary pass save/restore. */
        if (scale > 1.0f && isfinite(near_distance) && near_distance > 10.0f * scale) {
            double fade_start = (double)near_distance - 10.0 * scale;
            double fade_end = (double)near_distance - scale;
            double slope = 1.0 / (fade_start * fade_start - fade_end * fade_end);
            MEMF(0x003B8134u) = (float)(-fade_end * fade_end * slope);
            MEMF(0x003B8138u) = (float)slope;
        }
    }
}

extern void sub_0022F298_original(void);
void sub_0022F298(void)
{
    uint32_t caller = MEM32(g_esp);
    /* Blob shadows have a separate clip distance and reciprocal fade.
     * Scale initialization and explicit Lua settings, leaving temporary
     * supersampled-pass install/restore calls unchanged. */
    if (caller == 0x0022F9D7u || caller == 0x0022F338u)
        MEMF(g_esp + 4) *= distance_scale();
    sub_0022F298_original();
}

extern void sub_000F4AC3_original(void);
void sub_000F4AC3(void)
{
    uint32_t caller = MEM32(g_esp);
    /* Explicit view-distance settings supply an original distance. FOV
     * changes (0x210E1E) reuse the stored far plane. City loading
     * (0x224C73) supplies the already scaled city distance and cap. */
    if (caller == 0x00210EB4u)
        MEMF(g_esp + 0x10) *= view_scale();
    sub_000F4AC3_original();
}

extern void sub_002256C7_original(void);
static unsigned particle_budget(void)
{
    static unsigned budget;
    if (!budget) {
        const char *env = getenv("MM3_PARTICLE_MAX_OVERDRAW");
        char *end = NULL;
        long value = env && *env ? strtol(env, &end, 10) : 255;
        if (env && *env && (end == env || *end || value < 1 || value > 255)) {
            fprintf(stderr, "[MM3_DISTANCE] invalid MM3_PARTICLE_MAX_OVERDRAW; expected 1..255, using 255\n");
            value = 255;
        }
        budget = (unsigned)value;
    }
    return budget;
}

void sub_002256C7(void)
{
    uint32_t self = g_ecx, ps;
    sub_002256C7_original();
    /* The particle manager starts its systems with a maximum overdraw of 12
     * (an NV2A fill-rate budget, copied into every emitter); its own
     * particleSetMaxOverdraw setter accepts up to 255. */
    ps = MEM32(self + 4);
    if (ps && MEM32(ps + 0x524) == 12u && distance_scale() > 1.0f) {
        MEM32(ps + 0x524) = particle_budget();
        fprintf(stderr, "[MM3_DISTANCE] particle max overdraw 12 -> %u\n", MEM32(ps + 0x524));
    }
}
