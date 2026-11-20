/* Harness for the SDL3 GPU renderer's white-box tests (nv2a_*_sdlgpu.c). Include
 * after ../../src/video/nv2a_sdlgpu.c, so its static state and helpers are in
 * scope. Provides:
 *   - gpu_download: pixels of a GPU texture, through the renderer's own copy
 *     pass and fence, after all recorded work has run;
 *   - nv2a_common_checks(): the shared helpers of nv2a_render_common.h (the
 *     same code the D3D11 renderer uses), run by every platform's SDL test. */
#ifndef NV2A_SDLGPU_HOST_H
#define NV2A_SDLGPU_HOST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/xbox_winnt.h"

#ifndef CHECK
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
#endif

static void test_setenv(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

/* The window module's input and title state (fb_present.c and fb_window.h).
 * fb_window_sdl.c is built into these tests; the renderer does not read these
 * variables, so they stay at their zero values. */
volatile unsigned char fbw_key_down[256];
volatile float fbw_mouse_x, fbw_mouse_y;
volatile LONG fbw_mouse_moves;
volatile LONG fbw_inactive;
void (*fbw_close_handler)(void);
XboxCursor (*fbw_cursor_handler)(void);
int (*fbw_mouse_press_handler)(float x, float y);
XboxHostOverlay fbw_host_overlay;
int fbw_title_due(uint16_t *out, size_t count) { (void)out; (void)count; return 0; }
void fbw_initial_size(int work_w, int work_h, int *w, int *h)
{
    (void)work_w; (void)work_h;
    *w = 640; *h = 480;
}

/* Most cases run headless: nv2a_d3d_init opens no window unless the
 * RECOMP_FB_WINDOW variable asks for one. A case that needs a window (for a
 * resize) opens it hidden first, and init then adopts that window. */
static void open_hidden_window(void)
{
    test_setenv("RECOMP_FB_WINDOW", "1");
    CHECK(fbw_sdl_open(SDL_WINDOW_HIDDEN));
}

/* Pixels of a GPU texture (BGRA, tightly packed rows), after every recorded
 * draw and copy. Uses the renderer's batch and command buffer, so it must not
 * run inside a render or copy pass of its own. */
static void gpu_download(SDL_GPUTexture *t, uint32_t w, uint32_t h, uint32_t *out)
{
    SDL_GPUTransferBufferCreateInfo ci;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUTextureRegion src;
    SDL_GPUTextureTransferInfo dst;
    SDL_GPUFence *fence = NULL;
    const uint8_t *m;
    memset(&ci, 0, sizeof ci);
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ci.size = w * h * 4;
    tb = SDL_CreateGPUTransferBuffer(s_dev, &ci);
    CHECK(tb);
    CHECK(copy_begin());
    memset(&src, 0, sizeof src);
    memset(&dst, 0, sizeof dst);
    src.texture = t; src.w = w; src.h = h; src.d = 1;
    dst.transfer_buffer = tb;
    SDL_DownloadFromGPUTexture(s_copy, &src, &dst);
    copy_end();
    submit(&fence);
    CHECK(fence);
    CHECK(SDL_WaitForGPUFences(s_dev, true, &fence, 1));
    SDL_ReleaseGPUFence(s_dev, fence);
    m = (const uint8_t *)SDL_MapGPUTransferBuffer(s_dev, tb, false);
    CHECK(m);
    memcpy(out, m, (size_t)w * h * 4);
    SDL_UnmapGPUTransferBuffer(s_dev, tb);
    SDL_ReleaseGPUTransferBuffer(s_dev, tb);
}

/* A guest texture's layout, against hand-computed sizes. */
static void check_shared_layouts(void)
{
    NvD3DTexture t;
    TexLayout L;
    int kind = -1;
    memset(&t, 0, sizeof t);
    /* 8x8 A8R8G8B8, four levels: 64+16+4+1 texels of 4 bytes. */
    t.color = 0x06; t.width = t.height = 8; t.levels = 4;
    CHECK(tex_layout(&t, &L));
    CHECK(L.kind == K_SWZ && L.bpp == 4 && L.levels == 4 && L.faces == 1);
    CHECK(L.level_off[1] == 256 && L.level_off[2] == 320 && L.level_off[3] == 336);
    CHECK(L.total == 340 && L.face_stride == 340);
    CHECK(host_levels(&L, &t) == 4); /* a title's short chain is completed to 1x1 */
    /* Levels beyond the chain are clamped; a single level stays single. */
    t.levels = 15;
    CHECK(tex_layout(&t, &L) && L.levels == 4);
    t.levels = 1;
    CHECK(tex_layout(&t, &L) && L.levels == 1 && host_levels(&L, &t) == 1);
    /* Cube faces start 128-byte aligned after the first face. */
    t.cube = 1; t.levels = 4;
    CHECK(tex_layout(&t, &L) && L.faces == 6 && L.face_stride == 384);
    CHECK(L.total == 384 * 5 + 340);
    t.cube = 0;
    /* DXT1 counts 4x4 blocks of 8 bytes. */
    memset(&t, 0, sizeof t);
    t.color = 0x0C; t.width = t.height = 8; t.levels = 2;
    CHECK(tex_layout(&t, &L) && L.kind == K_DXT && L.bpp == 8);
    CHECK(L.level_off[1] == 32 && L.total == 40);
    CHECK(tex_format(0x0E, &kind) == 16 && kind == K_DXT);
    /* Linear textures are one pitch-by-height block. */
    memset(&t, 0, sizeof t);
    t.color = 0x12; t.width = 16; t.height = 4; t.pitch = 80; t.levels = 1;
    CHECK(tex_layout(&t, &L) && L.kind == K_LIN && L.total == 320 && L.levels == 1);
    t.pitch = 0;
    CHECK(!tex_layout(&t, &L));
    /* Invalid colour formats, sizes and out-of-range texture dimensions. */
    t.color = 0xFF; t.pitch = 80;
    CHECK(!tex_layout(&t, &L));
    t.color = 0x06; t.width = 5000; t.height = 8;
    CHECK(!tex_layout(&t, &L));
    t.width = 0;
    CHECK(!tex_layout(&t, &L));
}

static void *const no_pinned_samplers[4] = { NULL };

/* Single texels and their channel expansion (argb = BGRA bytes). */
static void check_shared_texels(void)
{
    static const uint8_t px16[2] = { 0x1F, 0x00 };   /* R5G6B5: blue 31 */
    static const uint8_t px32[4] = { 0x10, 0x20, 0x30, 0x40 };
    uint32_t v;
    CHECK(x5(31) == 255 && x5(0) == 0 && x6(63) == 255 && x4(15) == 255);
    CHECK(argb(0x80, 1, 2, 3) == 0x80010203u);
    CHECK(texel(0x05, px16, NULL, 0) == 0xFF0000FFu);          /* R5G6B5 blue */
    CHECK(texel(0x04, px16, NULL, 0) == argb(0, 0, x4(1), 255));  /* A4R4G4B4: g=1, b=15 */
    CHECK(texel(0x06, px32, NULL, 0) == 0x40302010u);          /* A8R8G8B8 */
    CHECK(texel(0x07, px32, NULL, 0) == 0xFF302010u);          /* X8R8G8B8 */
    CHECK(texel(0x00, px32, NULL, 0) == 0xFF101010u);          /* Y8 */
    CHECK(texel(0x0B, px32, NULL, 0) == 0);          /* I8 refuses missing palette */
    v = texel(0x19, px32, NULL, 0);                             /* A8: white with alpha */
    CHECK(v == 0x10FFFFFFu);
    CHECK(texel(0x1A, px32, NULL, 0) == 0x20101010u);          /* A8Y8 */
    CHECK(texel(0x99, px32, NULL, 0) == 0xFFFF00FFu);          /* unknown: magenta */
    CHECK(surface_color_bpp(0x3) == 2 && surface_color_bpp(0x6) == 4);
    CHECK(surface_color_bpp(0x9) == 1 && surface_color_bpp(0xF) == 0);
    CHECK(surface_alpha_mode(0x1) == 0 && surface_alpha_mode(0x2) == 1);
    CHECK(surface_alpha_mode(0x6) == 0 && surface_alpha_mode(0x7) == 1);
    CHECK(surface_alpha_mode(0x5) == 1 && surface_alpha_mode(0xB) == -1);
}

/* Index rotation for flat shading, vertex attribute sizes, and draw helpers. */
static void check_shared_draw_helpers(void)
{
    uint32_t tri[6] = { 0, 0, 0, 0, 0, 0 }, src[3] = { 4, 5, 6 };
    uint32_t line[2] = { 0, 0 }, lsrc[2] = { 9, 8 };
    float c[4];
    copy_indices(tri, src, 3, 4, NV_D3D_TRIANGLES, 1);
    CHECK(tri[0] == 2 && tri[1] == 0 && tri[2] == 1);     /* last vertex leads; winding kept */
    copy_indices(tri, src, 3, 4, NV_D3D_TRIANGLES, 0);
    CHECK(tri[0] == 0 && tri[1] == 1 && tri[2] == 2);
    copy_indices(line, lsrc, 2, 8, NV_D3D_LINES, 1);
    CHECK(line[0] == 0 && line[1] == 1);
    copy_indices(line, lsrc, 2, 8, NV_D3D_LINES, 0);
    CHECK(line[0] == 1 && line[1] == 0);
    CHECK(attr_bytes(2, 4) == 16 && attr_bytes(4, 3) == 3 && attr_bytes(0, 1) == 4);
    CHECK(attr_bytes(1, 2) == 4 && attr_bytes(6, 1) == 4 && attr_bytes(2, 5) == 0);
    CHECK(attr_bytes(9, 1) == 0);
    CHECK(mode_needs_texture(1) && mode_needs_texture(18) && !mode_needs_texture(0));
    unpack4(0x80FF4020u, c);
    CHECK(c[0] == 1.0f && fabsf(c[2] - 32.0f / 255.0f) < 1e-6f);   /* R FF, B 20 */
    CHECK(fabsf(c[3] - 128.0f / 255.0f) < 1e-6f);                   /* A 80 */
    CHECK(clamp_aspect(1.0f) == 4.0f / 3.0f && clamp_aspect(9.0f) == 4.0f);
}

/* Mip reduction: a 2x2 box with a unit dimension kept at one texel. */
static void check_shared_halving(void)
{
    uint32_t px[4] = { 0x00000000u, 0x40404040u, 0x80808080u, 0xC0C0C0C0u }, out[1];
    halve_bgra(px, 2, 2, out);
    CHECK(out[0] == 0x60606060u);
    px[0] = 0xFF000000u; px[1] = 0xFF000000u;
    halve_bgra(px, 2, 1, out);
    CHECK(out[0] == 0xFF000000u);
}

/* Everything in nv2a_render_common.h a platform's SDL build runs. */
static void nv2a_common_checks(void)
{
    check_shared_layouts();
    check_shared_texels();
    check_shared_draw_helpers();
    check_shared_halving();
}


/* Standalone renderer fixtures do not request executor replay. */
void nv2a_pb_interp_enable(int on) { (void)on; abort(); }
int nv2a_pb_replay(float t) { (void)t; abort(); return 0; }
int xbox_GetDisplayMode(uint32_t *pitch, uint32_t *bpp, uint32_t *w, uint32_t *h)
{ (void)pitch; (void)bpp; (void)w; (void)h; return 0; }

#endif
