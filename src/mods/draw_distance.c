#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "nv2a_pb_reserve.h"
#include "mods.h"
#include "render_commands.h"

/* PAL MM3: increase the title's own visibility/LOD distances before its
 * native D3D11 translation sees geometry. Keep the original fade fractions,
 * frustum construction, streaming and temporary render-pass save/restore.
 * Two knobs, both 1 = Xbox:
 *   RECOMP_DRAW_DISTANCE (default 4): object/pedestrian/traffic LODs, blob
 *     shadows and the particle overdraw cap. Everything stays inside the
 *     Xbox city view distance, so nothing low-detail is exposed.
 *   RECOMP_VIEW_DISTANCE (default 2): city view distance and far plane.
 *     A smooth per-pixel depth haze replaces the distance-fog texture pass. */
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

/* Extended visibility can exhaust the native 256-entry flare queue before
 * vehicle lamps are submitted. Scale its constructor budget so the original
 * allocator grows both the records and every buffered vertex allocation. */
extern void sub_00248518_original(void);
void sub_00248518(void)
{
    if (MEM32(g_esp) == 0x0022C89Eu && g_eax == 256u &&
        mm3_mod_on(MM3_MOD_DRAW_DISTANCE)) {
        g_eax = (uint32_t)(256.f * fmaxf(distance_scale(), view_scale()));
        fprintf(stderr, "[MM3_DISTANCE] flare capacity %u\n", g_eax);
    }
    sub_00248518_original();
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

/* Replace the Xbox Fog.dma draw with an ordered native host fog pass.
 * The camera supplies the W-buffer scale and sky colour, never a fog texture. */
extern void sub_000F8DCE_original(void);
extern void sub_00344770(void);

/* The native D3D rollover runs with the caller's registers restored after it. */
static void fog_pb_rollover(void *user)
{
    uint32_t eax=g_eax, ecx=g_ecx, edx=g_edx, ebp=g_ebp, seh=g_seh_ebp;
    uint32_t bytes=MEM32(0x352E00);
    (void)user;
    PUSH32(g_esp,bytes); PUSH32(g_esp,bytes>>1); PUSH32(g_esp,0);
    sub_00344770();
    g_eax=eax; g_ecx=ecx; g_edx=edx; g_ebp=ebp; g_seh_ebp=seh;
}

void sub_000F8DCE(void)
{
    if (!mm3_mod_on(MM3_MOD_DRAW_DISTANCE)) { sub_000F8DCE_original(); return; }
    uint32_t camera = MEM32(g_ecx+0xC), device = MEM32(0x351F48);
    float depth_scale = MEMF(camera+0x1ED8), far_clip = MEMF(camera+0x1FB0);
    /* The fork reserves the record, rolling the native cursor over first when
     * its soft limit is too close. A missing reservation omits the sky mask. */
    uint32_t cursor = xbox_pb_reserve(device, MM3_FOG_WORDS*4, fog_pb_rollover, NULL);
    if (cursor) {
        MEM32(cursor) = MM3_FOG_HEADER; /* Eleven non-increasing NV097_NO_OPERATION parameters. */
        MEM32(cursor+4) = MM3_FOG_TAG;
        MEMF(cursor+8) = 1.f/depth_scale;
        MEMF(cursor+12) = far_clip;
        MEMF(cursor+16) = fminf(300.f,far_clip*.4f);
        MEMF(cursor+20) = far_clip*.9f;
        MEM32(cursor+24) = MEM32(g_esp+4);
        MEM32(cursor+28) = MM3_FOG_SKY_MASK | MM3_FOG_SKY_FILL | MM3_FOG_SKY_FADE; /* Sky mask and untextured distant air. */
        /* World-up dotted with a camera ray: pitch/roll and projection follow
         * the native cloud dome. Fade its lower edge over about nine degrees. */
        MEMF(cursor+32) = MEMF(camera+0x1F34)*depth_scale/MEMF(camera+0x1EE4);
        MEMF(cursor+36) = MEMF(camera+0x1F38)*depth_scale/MEMF(camera+0x1EF8);
        MEMF(cursor+40) = MEMF(camera+0x1F3C);
        MEMF(cursor+44) = 1.f/.16f;
    }
    g_eax = (g_eax & 0xFFFFFF00u) | 1;
    g_esp += 16;
}

/* HorizonMap.dma is a separate low-resolution city panorama, not depth fog.
 * Keep Atmosphere.dma (cloud sky) and the host depth fog; omit this strip. */
extern void sub_000F8750_original(void);
void sub_000F8750(void)
{
    if (!mm3_mod_on(MM3_MOD_DRAW_DISTANCE)) { sub_000F8750_original(); return; }
    g_eax = (g_eax & 0xFFFFFF00u) | 1;
    g_esp += 8; /* ret 4 */
}
