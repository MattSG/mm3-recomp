/* SDL3 GPU counterpart of test_flare_depth.c: lamp glow sprites must not shine
 * through an opaque hill's depth. The decision table (flare_needs_depth) and
 * the depth-tested glows are the same as on D3D11; pixels are read back from
 * the GPU. The SDL renderer has one sample per target: RECOMP_MSAA > 1 is
 * accepted and warned about, and the results must not change. */
#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../tools/xboxrecomp/src/video/nv2a_sdlgpu.c"
/* The native vertex program and constants come from this test, not the game. */
#define NV2A_TEST_OWN_VSH
#include "../../tools/xboxrecomp/tests/nv2a_sdlgpu_test_host.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

#include "../../src/mods/render_policy.c"
static int depth_policy_enabled;
int mm3_mod_on(int mod) { return mod == MM3_MOD_FLARE_DEPTH && depth_policy_enabled; }
static void test_set_depth_policy(uint32_t enabled)
{
    NvOverlayHooks hooks = {0};
    depth_policy_enabled = enabled != 0;
    hooks.force_depth_test = mm3_render_force_depth;
    nv2a_d3d_set_overlay_hooks(&hooks);
}

static int native_fixture;
static uint32_t native_program[136][4] = {
    {0, 0x0020001B, 0x0836106C, 0x2F100FF8},
    {0, 0x0420061B, 0x083613FC, 0x5011F818},
    {0, 0x002008FF, 0x0836106C, 0x2070F828},
    {0, 0x0240081B, 0x1436186C, 0x2F20F824},
    {0, 0x0060201B, 0x2436106C, 0x3070F800},
    {0, 0x00200200, 0x0836106C, 0x2070F830},
    {0, 0x00200E1B, 0x0836106C, 0x2070F838},
    {0, 0x0020101B, 0x0836106C, 0x2070F840},
    {0, 0x0020121B, 0x0836106C, 0x2070F848},
    {0, 0x0020141B, 0x0836106C, 0x2070F850},
    {0, 0x0020161B, 0x0836106C, 0x2070F858},
    {0, 0x0020181B, 0x0836106C, 0x2070F861}
};
static float native_constants[192][4] = {{1, 1, 16777215, 1}, {.53125f, .53125f, 0, 0}};

const uint32_t (*nv2a_vsh_program(uint32_t *start, uint32_t *version))[4]
{
    *start = 0; *version = 1;
    return native_fixture ? (const uint32_t (*)[4])native_program : NULL;
}
const float (*nv2a_vsh_constants(uint32_t *version))[4]
{
    *version = 1;
    return native_fixture ? (const float (*)[4])native_constants : NULL;
}

/* A target's texel, read back from the GPU (BGRA, as the guest sees it). */
static uint32_t pixel(Surface *surface, unsigned x, unsigned y)
{
    uint32_t *texels = malloc((size_t)surface->iw * surface->ih * 4);
    uint32_t result = 0xDEADBEEFu;
    if (!texels) return result;
    if (nv2a_test_read_level(surface->tex, surface->iw, surface->ih, 0, 0, texels))
        result = texels[y * surface->iw + x];
    free(texels);
    return result;
}

/* Clear colour and/or depth of the state's targets (the guest's clear path). */
static void clear_color(const NvD3DState *st, uint32_t argb)
{
    nv2a_d3d_clear(st, 0xF0, argb, 0, 0, 0, 0xFFFF, 0xFFFF);
}

static void clear_depth(const NvD3DState *st, float depth)
{
    /* D24 in the high bits of zstencil for the 24-bit depth format. */
    nv2a_d3d_clear(st, 1, 0, (uint32_t)(depth * 16777215.0f) << 8, 0, 0, 0xFFFF, 0xFFFF);
}

static void glow(const NvD3DState *st, float depth)
{
    static const uint32_t indices[] = {0, 1, 2, 2, 1, 3};
    float vertices[4][4] = {{0, 0, 0, 1}, {48, 0, 0, 1}, {0, 48, 0, 1}, {48, 48, 0, 1}};
    float defaults[16][4] = {{0}};
    NvD3DAttrib attributes[16] = {{0}};
    unsigned n;
    for (n = 0; n < 4; ++n) {
        vertices[n][2] = depth * 16777215.0f;
        vertices[n][3] = st->z_perspective ? 1.0f / vertices[n][2] : 1.0f;
    }
    defaults[3][1] = defaults[3][3] = 1; /* green */
    for (n = 9; n < 13; ++n) defaults[n][3] = 1;
    attributes[0].enabled = 1;
    attributes[0].type = 2;
    attributes[0].size = 4;
    attributes[0].stride = sizeof vertices[0];
    attributes[0].data = (const uint8_t *)vertices;
    nv2a_d3d_draw_ub(st, NV_D3D_TRIANGLES, attributes, defaults, 0, indices, 6, 0, 3, NULL);
}

/* The native Flares vertex program/constants and draw, as in test_flare_depth.c. */
static void native_glow(const NvD3DState *st)
{
    static const uint32_t indices[] = {0, 1, 2, 2, 1, 3};
    float vertices[4][14] = {
        {298.299f, 162.611f, .999532f, 2.71648e-7f},
        {322.299f, 162.611f, .999532f, 2.71648e-7f},
        {298.299f, 186.611f, .999532f, 2.71648e-7f},
        {322.299f, 186.611f, .999532f, 2.71648e-7f}
    };
    float defaults[16][4] = {{0}};
    NvD3DAttrib attributes[16] = {{0}};
    unsigned n;
    defaults[3][1] = 1; defaults[3][3] = .5f;
    for (n = 9; n < 13; ++n) defaults[n][3] = 1;
    attributes[0].enabled = 1;
    attributes[0].type = 2;
    attributes[0].size = 4;
    attributes[0].stride = sizeof vertices[0];
    attributes[0].data = (const uint8_t *)vertices;
    native_fixture = 1;
    nv2a_d3d_draw_ub(st, NV_D3D_TRIANGLES, attributes, defaults, 1, indices, 6, 0, 3, NULL);
    native_fixture = 0;
}

int main(int argc, char **argv)
{
    NvD3DState st = {0}, other;
    Surface *mask, *target;
    Depth *depth;
    unsigned n, wbuffer, enabled;
    uint32_t gw, gh, zmax;
    uint8_t *ram;
    const char *samples = argc > 1 ? argv[1] : "1";
    const char *scale = argc > 2 ? argv[2] : "1";
    ram = calloc(1, 0x400000);
    CHECK(ram);
    nv2a_test_ram = ram;
    nv2a_test_setenv("RECOMP_MSAA", samples);
    nv2a_test_setenv("RECOMP_RENDER_SCALE", scale);
    CHECK(nv2a_test_init());
    mask = surface_get(0x20000, 64 * 4, 8, 64, 64, 0);
    CHECK(mask);
    st.color_addr = 0x10000;
    st.color_pitch = st.zeta_pitch = 64 * 4;
    st.zeta_addr = 0x30000;
    st.surface_format = 0x28;
    st.clip_w = st.clip_h = 64;
    st.color_mask = 0x00010101; /* RGB; leave the visibility alpha intact. */
    st.depth_test = 1;
    st.depth_func = 0x203;
    CHECK(setup_pipeline(&st, NV_D3D_TRIANGLES, &target, &gw, &gh, &zmax, 1));
    depth = s_cur_ds;
    CHECK(depth);
    st.depth_test = 0;
    st.blend_enable = 1;
    st.blend_sfactor = st.blend_dfactor = 1;
    st.blend_equation = 0x8006;
    st.rc_seen = 1;
    st.rc.final0 = 4;       /* Final D = primary diffuse. */
    st.rc.final1 = 0x1400;  /* Final G = primary diffuse alpha. */
    st.rc.stage_program = 0x8421;
    st.tex_used = 15;
    for (n = 0; n < 4; ++n) {
        /* Native Flares/Flares.psh: three occluders, then the flare. */
        st.tex[n].addr = n < 3 ? mask->va : 0x60000;
        st.tex[n].color = 0x12;
        st.tex[n].pitch = 64 * 4;
        st.tex[n].width = st.tex[n].height = 64;
        st.tex[n].levels = 1;
        /* Native traced linear magnification; minification/aniso varies. */
        st.tex[n].filter = 0x02062000;
    }
    for (n = 0; n < 64 * 64; ++n) ((uint32_t *)(ram + 0x60000))[n] = 0xFFFFFFFFu;
    test_set_depth_policy(0x8421);
    CHECK(draw_forces_depth(&st));
    other = st; other.tex[3] = other.tex[0]; CHECK(!draw_forces_depth(&other)); /* bloom */
    other = st; other.tex_used = 1; CHECK(draw_forces_depth(&other)); /* native RC fallback mask */
    other = st; other.rc.stage_program = 1; CHECK(!draw_forces_depth(&other)); /* HUD */
    other = st; other.blend_dfactor = 0x303; CHECK(!draw_forces_depth(&other)); /* glass */
    other = st; other.depth_test = 1; CHECK(!draw_forces_depth(&other)); /* native depth */
    other = st; other.tex[2].addr += 16; CHECK(!draw_forces_depth(&other)); /* unrelated taps */
    other = st; other.tex[0] = st.tex[3]; other.tex[3] = st.tex[0];
    CHECK(!draw_forces_depth(&other)); /* reversed stage order is not native */
    other = st; other.rc.stage_program = 0x8000; CHECK(!draw_forces_depth(&other)); /* Flares2 */
    other = st; other.tex[1].filter = 0x01000000;
    CHECK(!draw_forces_depth(&other)); /* native visibility pass uses linear magnification */
    for (wbuffer = 0; wbuffer < 2; ++wbuffer) {
        st.z_perspective = wbuffer;
        for (enabled = 0; enabled < 2; ++enabled) {
            test_set_depth_policy(enabled ? 0x8421u : 0u);
            clear_color(&st, 0xFFFF0000u);
            target->gen++;
            clear_depth(&st, .5f);
            glow(&st, .75f);
            CHECK(pixel(target, (unsigned)(16 * s_scale), (unsigned)(16 * s_scale)) ==
                  (enabled ? 0xFFFF0000u : 0xFFFFFF00u));
            if (enabled) {
                glow(&st, .25f);
                CHECK(pixel(target, (unsigned)(16 * s_scale), (unsigned)(16 * s_scale)) == 0xFFFFFF00u);
            }
        }
    }
    /* Replay the native VF and register-combiner program. Live diagnostics
     * confirm Z buffering; also exercise W buffering with the same native RHW. */
    mask = surface_get(0x100000, 640 * 4, 8, 640, 480, 0);
    CHECK(mask);
    {
        NvD3DState clear = {0};
        clear.color_addr = 0x100000;
        clear.color_pitch = 640 * 4;
        clear.surface_format = 0x128;
        clear.clip_w = 640; clear.clip_h = 480;
        clear_color(&clear, 0xFFFFFFFFu);
        mask->gen++;
    }
    st.color_addr = 0x200000;
    st.zeta_addr = 0x300000;
    st.color_pitch = st.zeta_pitch = 640 * 4;
    st.clip_w = 640; st.clip_h = 480;
    st.surface_format = 0x128;
    st.tex_used = 1; /* Actual native register-combiner draw state. */
    st.blend_sfactor = 0x302;
    st.rc.control = 0x11102;
    st.rc.final0 = 0x0C040000; st.rc.final1 = 0x00001C80;
    st.rc.color_icw[0] = 0xCBD40000; st.rc.alpha_icw[0] = 0xD830D930;
    st.rc.color_ocw[0] = 0x000100C0; st.rc.alpha_ocw[0] = 0x00030C00;
    st.rc.color_icw[1] = 0xCCDB0000; st.rc.alpha_icw[1] = 0xDC30DA30;
    st.rc.color_ocw[1] = 0x000000C0; st.rc.alpha_ocw[1] = 0x00030C00;
    for (n = 0; n < 3; ++n) {
        st.tex[n].addr = mask->va;
        st.tex[n].width = 640; st.tex[n].height = 480; st.tex[n].pitch = 640 * 4;
    }
    for (wbuffer = 0; wbuffer < 2; ++wbuffer) {
        float lamp_depth = wbuffer ? 1.0f / (2.71648e-7f * 16777216.0f) : .999532f;
        st.z_perspective = wbuffer;
        for (enabled = 0; enabled < 2; ++enabled) {
            test_set_depth_policy(enabled ? 0x8421u : 0u);
            CHECK(setup_pipeline(&st, NV_D3D_TRIANGLES, &target, &gw, &gh, &zmax, 1));
            depth = s_cur_ds;
            if (!depth) depth = depth_find(phys(st.zeta_addr));
            if (!depth) depth = depth_get(st.zeta_addr, target->iw, target->ih);
            CHECK(depth);
            clear_color(&st, 0xFFFF0000u);
            target->gen++;
            clear_depth(&st, lamp_depth - .0001f);
            native_glow(&st);
            CHECK(pixel(target, (unsigned)(306 * target->sx), (unsigned)(170 * target->sy)) ==
                  (enabled ? 0xFFFF0000u : 0xFFFFFF00u));
            if (enabled) {
                clear_depth(&st, lamp_depth + .0001f);
                native_glow(&st);
                CHECK(pixel(target, (unsigned)(306 * target->sx), (unsigned)(170 * target->sy)) == 0xFFFFFF00u);
            }
        }
    }
    /* Flares disable native depth testing and may change Z_PERSPECTIVE.
     * The forced test must use the scene buffer's encoding, not that toggle. */
    test_set_depth_policy(0x8421);
    for (wbuffer = 0; wbuffer < 2; ++wbuffer) {
        NvD3DState scene = st;
        float lamp_depth = wbuffer ? 1.0f / (2.71648e-7f * 16777216.0f) : .999532f;
        scene.z_perspective = wbuffer;
        st.z_perspective = !wbuffer;
        clear_color(&st, 0xFFFF0000u);
        clear_depth(&scene, lamp_depth - .0001f);
        native_glow(&st);
        CHECK(pixel(target, (unsigned)(306 * target->sx), (unsigned)(170 * target->sy)) == 0xFFFF0000u);
        clear_depth(&scene, lamp_depth + .0001f);
        native_glow(&st);
        CHECK(pixel(target, (unsigned)(306 * target->sx), (unsigned)(170 * target->sy)) == 0xFFFFFF00u);
    }
    free(ram);
    puts("PASS: synthetic and native VF/combiner glows fail occluded depth and retain visible lamps; Z/W-buffer and MSAA request/scale");
    return 0;
}
