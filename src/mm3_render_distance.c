#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "mm3_render_distance.h"

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
 * RECOMP_DRAW_DISTANCE=1 provides the Xbox baseline for comparisons. */
static float distance_scale(void)
{
    static float scale;
    if (!scale) {
        const char *env = getenv("RECOMP_DRAW_DISTANCE");
        char *end = NULL;
        float value = env ? strtof(env, &end) : 2.0f;
        if (env && (end == env || *end || !isfinite(value) || value < 1.0f || value > 4.0f)) {
            fprintf(stderr, "[MM3_DISTANCE] invalid RECOMP_DRAW_DISTANCE; expected 1..4, using 2\n");
            value = 2.0f;
        }
        scale = value;
        /* MM3_FOG_REMAP=0 keeps the scaled band, for A/B comparisons. */
        if (!getenv("MM3_FOG_REMAP") || strcmp(getenv("MM3_FOG_REMAP"), "0"))
            nv2a_d3d_set_depth_view_remap(scale, FOG_KNEE);
        fprintf(stderr, "[MM3_DISTANCE] draw/LOD distance multiplier %.3g\n", scale);
    }
    return scale;
}

float mm3_draw_distance_scale(void)
{
    return distance_scale();
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
        MEMF(g_esp + 4) *= distance_scale();
    /* City loading applies a title-specific distance cap after reading the
     * DSS. Its local is then passed straight to the camera projection, so
     * update both together rather than leaving the camera at the Xbox cap. */
    if (caller == 0x00224C4Eu) {
        MEMF(g_esp + 4) *= distance_scale();
        MEMF(g_ebp + 8) = MEMF(g_esp + 4);
    }
    sub_000FE71E_original();
}

extern void sub_001661D0_original(void);
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

void sub_001661D0(void)
{
    uint32_t caller = MEM32(g_esp);
    /* Facade construction uses {80,280}; renderFacadeSetZones supplies
     * explicit Lua overrides. Let the original setter recompute its fade
     * coefficients from the larger distances. The supersampled-pass
     * save/restore calls at 0x211220/0x21153D already use effective values. */
    if (caller == 0x0015FA1Du || caller == 0x0020FABAu) {
        MEMF(g_esp + 4) *= distance_scale();
        MEMF(g_esp + 8) *= distance_scale();
    }
    sub_001661D0_original();
    {
        float scale = distance_scale();
        float near_distance = MEMF(0x003C04E0u);
        /* The native facade-detail fade uses squared camera distance and
         * fixed offsets of 10 m / 1 m below the near zone. Scaling only the
         * zone left its blend just 9 m wide: 150..159 m at the default 2x,
         * instead of the corresponding 140..158 m. Scale those offsets too.
         * Rebuild from the validated effective zone after every setter call,
         * including temporary-pass install/restore; never scale saved values
         * again. Leave unusual small zones and the Xbox baseline untouched. */
        if (scale > 1.0f && isfinite(near_distance) && near_distance > 10.0f * scale) {
            double fade_start = (double)near_distance - 10.0 * scale;
            double fade_end = (double)near_distance - scale;
            double slope = 1.0 / (fade_start * fade_start - fade_end * fade_end);
            MEMF(0x003B8134u) = (float)(-fade_end * fade_end * slope);
            MEMF(0x003B8138u) = (float)slope;
        }
    }
}

extern void sub_000F4AC3_original(void);
void sub_000F4AC3(void)
{
    uint32_t caller = MEM32(g_esp);
    /* Explicit view-distance settings supply an original distance. FOV
     * changes (0x210E1E) reuse the stored far plane. City loading
     * (0x224C73) supplies the already scaled city distance and cap. */
    if (caller == 0x00210EB4u)
        MEMF(g_esp + 0x10) *= distance_scale();
    sub_000F4AC3_original();
}

extern void sub_002256C7_original(void);
static unsigned particle_budget(void)
{
    static unsigned budget;
    if (!budget) {
        const char *env = getenv("MM3_PARTICLE_MAX_OVERDRAW");
        char *end = NULL;
        long value = env ? strtol(env, &end, 10) : 24;
        if (env && (end == env || *end || value < 1 || value > 255)) {
            fprintf(stderr, "[MM3_DISTANCE] invalid MM3_PARTICLE_MAX_OVERDRAW; expected 1..255, using 24\n");
            value = 24;
        }
        budget = (unsigned)value;
    }
    return budget;
}

void sub_002256C7(void)
{
    uint32_t self = g_ecx, ps;
    sub_002256C7_original();
    /* Keep a bounded effects budget at the extended view distance. Raising
     * the Xbox budget of 12 to its maximum of 255 allowed twenty-one times
     * as much overlapping smoke/spray during collisions and burnouts.
     * Explicit title/Lua settings remain authoritative. */
    ps = MEM32(self + 4);
    if (ps && MEM32(ps + 0x524) == 12u && distance_scale() > 1.0f) {
        unsigned budget = particle_budget();
        MEM32(ps + 0x524) = budget;
        fprintf(stderr, "[MM3_DISTANCE] particle max overdraw 12 -> %u\n", budget);
    }
}
