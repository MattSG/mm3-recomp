/* White-box tests of the SDL_GPU renderer (src/host_sdl3/video/nv2a_sdlgpu.c) on a real Vulkan/D3D12
 * device. They #include the renderer so they can reach its static state, count and fail the SDL
 * calls it makes, and read its render target back. Run one case per process: renderer_bindings <case>
 * (common, init_failure, pixels, host_draw, failures, ps_bindings, ps_reset, overlay_upload, resize).
 * Restored from the toolkit's old tests/nv2a_bindings and ported to the host module. */
/* The SDL3 GPU renderer's draw bindings, the counterpart of test_bindings.c.
 * The renderer records each draw's pipeline, uniforms, samplers and vertex and
 * index ranges into a batch, which replays on the GPU at the next flush. The
 * tests read the results back: the exact colour each draw leaves, the uniform
 * pushes the batch makes (constants that did not change are not pushed), the
 * samplers and textures each draw binds, and the ring's behaviour when its
 * space runs out or a map fails. The reference is the colour and texel each
 * draw must produce, written from the draw's own data. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Calls the renderer makes into SDL are counted or failed before it is
 * included. Each real function is taken first, so a wrapper calls it, not
 * itself. */
static void (*const real_push_vertex)(SDL_GPUCommandBuffer *, Uint32, const void *, Uint32) = SDL_PushGPUVertexUniformData;
static void (*const real_push_fragment)(SDL_GPUCommandBuffer *, Uint32, const void *, Uint32) = SDL_PushGPUFragmentUniformData;
static void (*const real_bind_fragment_samplers)(SDL_GPURenderPass *, Uint32, const SDL_GPUTextureSamplerBinding *, Uint32) = SDL_BindGPUFragmentSamplers;
static void *(*const real_map_transfer)(SDL_GPUDevice *, SDL_GPUTransferBuffer *, bool) = SDL_MapGPUTransferBuffer;
static SDL_GPUBuffer *(*const real_create_buffer)(SDL_GPUDevice *, const SDL_GPUBufferCreateInfo *) = SDL_CreateGPUBuffer;
static void test_push_vertex(SDL_GPUCommandBuffer *c, Uint32 slot, const void *data, Uint32 n);
static void test_push_fragment(SDL_GPUCommandBuffer *c, Uint32 slot, const void *data, Uint32 n);
static void test_bind_fragment_samplers(SDL_GPURenderPass *rp, Uint32 first,
                                        const SDL_GPUTextureSamplerBinding *b, Uint32 n);
static void *test_map_transfer(SDL_GPUDevice *dev, SDL_GPUTransferBuffer *tb, bool cycle);
static SDL_GPUBuffer *test_create_buffer(SDL_GPUDevice *dev, const SDL_GPUBufferCreateInfo *ci);
#define SDL_PushGPUVertexUniformData test_push_vertex
#define SDL_PushGPUFragmentUniformData test_push_fragment
#define SDL_BindGPUFragmentSamplers test_bind_fragment_samplers
#define SDL_MapGPUTransferBuffer test_map_transfer
#define SDL_CreateGPUBuffer test_create_buffer

static unsigned vertex_pushes, fragment_pushes, bind_calls;
static int fail_vertex_map, fail_buffers;
static SDL_GPUTextureSamplerBinding bound[13];

#include "video/nv2a_sdlgpu.c"        /* include path: src/host_sdl3 */
#include "nv2a_test_host.h"          /* include path: the toolkit's tests/ */
#include "renderer_host.h"

static void test_push_vertex(SDL_GPUCommandBuffer *c, Uint32 slot, const void *data, Uint32 n)
{
    vertex_pushes++;
    real_push_vertex(c, slot, data, n);
}
static void test_push_fragment(SDL_GPUCommandBuffer *c, Uint32 slot, const void *data, Uint32 n)
{
    fragment_pushes++;
    real_push_fragment(c, slot, data, n);
}
static void test_bind_fragment_samplers(SDL_GPURenderPass *rp, Uint32 first,
                                        const SDL_GPUTextureSamplerBinding *b, Uint32 n)
{
    if (first == 0 && n == 13) {
        memcpy(bound, b, sizeof bound);
        bind_calls++;
    }
    real_bind_fragment_samplers(rp, first, b, n);
}
static void *test_map_transfer(SDL_GPUDevice *dev, SDL_GPUTransferBuffer *tb, bool cycle)
{
    if (fail_vertex_map && tb == s_vtb) {
        fail_vertex_map--;
        return NULL;
    }
    return real_map_transfer(dev, tb, cycle);
}
static SDL_GPUBuffer *test_create_buffer(SDL_GPUDevice *dev, const SDL_GPUBufferCreateInfo *ci)
{
    if (fail_buffers) {
        fail_buffers = 0;
        return NULL;
    }
    return real_create_buffer(dev, ci);
}

static uint8_t ram[0x400000];
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)ram; }
int xbox_FramebufferScanSource(uint32_t *va, uint32_t *pitch) { return 0; }
int xbox_FramebufferOverlay(XboxOverlay *ovl) { return 0; }
void xbox_FramebufferNoteFlip(uint32_t draws) {}
void xbox_FramebufferRenderHostOverlay(void *device, void *context, void *target) { (void)device; (void)context; (void)target; }
const uint32_t (*nv2a_vsh_program(uint32_t *start, uint32_t *version))[4] { return NULL; }
const float (*nv2a_vsh_constants(uint32_t *version))[4] { return NULL; }

static const float colours[3][4] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 } };
static const uint32_t expected_argb[3] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu };

static NvD3DState draw_state(void)
{
    NvD3DState st = {0};
    st.color_addr = 0x20000; st.color_pitch = 64 * 4;
    st.zeta_addr = 0x40000; st.zeta_pitch = 64 * 4;
    st.surface_format = 0x25; st.clip_w = st.clip_h = 64;
    st.color_mask = 0x01010101; st.blend_sfactor = 1; st.blend_equation = 0x8006;
    st.front_face = 0x901; st.depth_test = 1; st.depth_func = 0x207;
    return st;
}

/* One flat-coloured triangle covering pixel (20,20), shifted right by moved. */
static void triangle(const NvD3DState *st, unsigned colour, unsigned moved)
{
    float positions[3][4] = { { 8, 8, 0, 1 }, { 56, 8, 0, 1 }, { 8, 56, 0, 1 } };
    NvD3DAttrib attr[16] = {0};
    float def[16][4] = { { 0 } };
    uint32_t idx[3] = { 0, 1, 2 };
    unsigned i;
    for (i = 0; i < 3; i++) positions[i][0] += (float)moved;
    memcpy(def[3], colours[colour], sizeof def[3]);
    attr[0] = (NvD3DAttrib){ 1, 2, 4, 16, (const uint8_t *)positions };
    nv2a_d3d_draw_ub(st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 0, 2, NULL);
}

/* The colour target's pixels, after every draw recorded so far. */
static void read_target(uint32_t pixels[64 * 64])
{
    Surface *s = surface_find(0x20000);
    CHECK(s && s->iw == 64 && s->ih == 64);
    gpu_download(s->tex, 64, 64, pixels);
}

/* Something that changes the batch's target and state between draws: the
 * clear, Y16 views of the colour and depth buffers, and the present blit and
 * composite (into a scratch texture). */
static void auxiliary(unsigned which, const NvD3DState *st)
{
    Surface *s = surface_find(0x20000);
    Depth *d;
    PresentConsts pc = { 0 };
    SDL_GPUTexture *out;
    int clear = 1;
    switch (which) {
    case 0: return;
    case 1: nv2a_d3d_clear(st, 0xF0, 0xFF000000, 0, 0, 0, 63, 63); return;
    case 2: CHECK(surface_y16(s)); return;
    case 3:
        d = depth_get(st->zeta_addr, 64, 64);
        CHECK(d && depth_y16(d));
        return;
    default:
        out = make_texture(64, 64, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, USAGE_TARGET);
        CHECK(out);
        if (which == 4) CHECK(blit_span(s->tex, NULL, out, &clear, 64, 64, -1.0f, 0, 1, 0, 1));
        else composite(out, 64, 64, s, &pc, 0);
        release_texture(&out);
    }
}

static void check_pixel(unsigned colour, uint32_t pixels[64 * 64])
{
    if (pixels[20 * 64 + 20] != expected_argb[colour])
        fprintf(stderr, "pixel %08X, expected %08X\n", pixels[20 * 64 + 20], expected_argb[colour]);
    CHECK(pixels[20 * 64 + 20] == expected_argb[colour]);
}

static void test_pixels(void)
{
    NvD3DState st = draw_state();
    uint32_t pixels[64 * 64];
    unsigned pass, before, before_fragment;
    for (pass = 0; pass < 6; pass++) {
        nv2a_d3d_clear(&st, 0xF0, 0xFF000000, 0, 0, 0, 63, 63);
        triangle(&st, 0, 0);
        read_target(pixels); check_pixel(0, pixels);
        auxiliary(pass, &st);
        triangle(&st, 1, 2);
        read_target(pixels); check_pixel(1, pixels);
        /* Another draw in the same command buffer: only its vertex uniform
         * block (the ring offsets) is pushed, not the unchanged constants. */
        triangle(&st, 2, 4);
        flush_batch();
        before = vertex_pushes;
        before_fragment = fragment_pushes;
        triangle(&st, 2, 4);
        flush_batch();
        CHECK(vertex_pushes - before == 1 && fragment_pushes == before_fragment);
        read_target(pixels); check_pixel(2, pixels);
    }
    /* Ring wrap: a draw that does not fit the vertex and index rings starts
     * a new command buffer, so the offsets start over. */
    submit(NULL);
    cmd();   /* a command buffer is open, as it is whenever a draw has recorded data */
    s_vpos = VB_BYTES - 16; s_ipos = IB_BYTES - 4;
    triangle(&st, 1, 0);
    CHECK(s_vpos < 1024 && s_ipos < 1024);
    read_target(pixels); check_pixel(1, pixels);
    puts("PASS: exact GPU colours through clear, Y16 colour/depth, blit and composite, unchanged-constant pushes and ring wrap");
}

static void test_failures(void)
{
    NvD3DState st = draw_state();
    uint32_t pixels[64 * 64];
    nv2a_d3d_clear(&st, 0xF0, 0xFF000000, 0, 0, 0, 63, 63);
    triangle(&st, 1, 0);
    read_target(pixels); check_pixel(1, pixels);
    /* A failed vertex ring map draws nothing and leaves the target alone. */
    submit(NULL);
    fail_vertex_map = 1;
    triangle(&st, 2, 0);
    CHECK(!fail_vertex_map);
    read_target(pixels); check_pixel(1, pixels);
    triangle(&st, 2, 0);
    read_target(pixels); check_pixel(2, pixels);
    puts("PASS: failed vertex ring map skips the draw without corrupting later draws; retry draws the exact colour");
}

static NvD3DTexture textured(void)
{
    NvD3DTexture t = {0};
    unsigned x, y;
    t.addr = 0x70000; t.color = 0x12; t.format = 0x1200;
    t.width = t.height = 8; t.pitch = 32; t.levels = 1;
    t.filter = (1u << 16) | (1u << 24); t.address = 0x010101;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            uint32_t c = 0xFF000000u | ((32 + x * 27) << 16) | ((64 + y * 21) << 8) | (32 + x * 25);
            memcpy(ram + 0x70000 + (y * 8 + x) * 4, &c, 4);
        }
    return t;
}
static uint32_t expected_texel(unsigned x, unsigned y)
{
    return 0xFF000000u | ((32 + x * 27) << 16) | ((64 + y * 21) << 8) | (32 + x * 25);
}

/* A draw sampling texture 0 at texel (3.5, 5.5), with the renderer's texture
 * and sampler set for it. */
static void textured_triangle(const NvD3DState *st, unsigned moved)
{
    float positions[3][4] = { { 8, 8, 0, 1 }, { 56, 8, 0, 1 }, { 8, 56, 0, 1 } };
    NvD3DAttrib attr[16] = {0};
    float def[16][4] = { { 0 } };
    uint32_t idx[3] = { 0, 1, 2 };
    unsigned i;
    for (i = 0; i < 3; i++) positions[i][0] += (float)moved;
    def[3][0] = def[3][1] = def[3][2] = def[3][3] = 1;
    def[9][0] = 3.5f; def[9][1] = 5.5f; def[9][3] = 1;
    attr[0] = (NvD3DAttrib){ 1, 2, 4, 16, (const uint8_t *)positions };
    nv2a_d3d_draw_ub(st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 0, 2, NULL);
}

static void check_textured_bindings(const NvD3DTexture *t)
{
    unsigned n;
    CHECK(bound[0].texture && bound[0].texture != s_dummy_2d);
    CHECK(bound[0].sampler == sampler(t, 1, 0, no_pinned_samplers));
    for (n = 1; n < 4; n++) CHECK(bound[n].texture == s_dummy_2d && bound[n].sampler);
    for (n = 4; n < 8; n++) CHECK(bound[n].texture == s_dummy_cube);
    CHECK(bound[8].texture == s_dummy_2d);
}

static void test_ps_bindings(void)
{
    NvD3DState st = draw_state();
    NvD3DTexture t = textured();
    uint32_t pixels[64 * 64];
    unsigned pass;
    st.tex_used = 1; st.tex[0] = t;
    for (pass = 0; pass < 6; pass++) {
        nv2a_d3d_clear(&st, 0xF0, 0xFF000000, 0, 0, 0, 63, 63);
        textured_triangle(&st, 0);
        read_target(pixels);
        CHECK(pixels[20 * 64 + 20] == expected_texel(3, 5));
        check_textured_bindings(&t);
        auxiliary(pass, &st);
        textured_triangle(&st, 2);
        read_target(pixels);
        CHECK(pixels[20 * 64 + 20] == expected_texel(3, 5));
        check_textured_bindings(&t);
    }
    /* Table churn: sampler objects bound by a batch are retained by SDL until
     * the batch runs, so a draw after many evictions still binds and samples. */
    {
        unsigned i;
        for (i = 0; i < 2 * SAMP_SLOTS; i++) {
            NvD3DTexture u = t;
            u.control0 = (i << 6) & 0x3FFFFFC0u;
            CHECK(sampler(&u, 1, 0, no_pinned_samplers));
        }
    }
    textured_triangle(&st, 4);
    read_target(pixels);
    CHECK(pixels[20 * 64 + 20] == expected_texel(3, 5));
    check_textured_bindings(&t);
    puts("PASS: textured draws bind the texture, sampler and dummy slots exactly across clear, Y16, blit and composite, and after sampler table churn");
}

static void test_ps_reset(void)
{
    NvD3DState st = draw_state();
    NvD3DTexture t = textured();
    uint32_t pixels[64 * 64];
    unsigned before_bind, before_fragment;
    st.tex_used = 1; st.tex[0] = t;
    textured_triangle(&st, 0);
    read_target(pixels); CHECK(pixels[20 * 64 + 20] == expected_texel(3, 5));
    /* A new command buffer starts with no pushed data: the first draw pushes
     * every fragment constant again and rebinds the samplers. */
    submit(NULL);
    before_bind = bind_calls; before_fragment = fragment_pushes;
    textured_triangle(&st, 0);
    read_target(pixels);
    CHECK(pixels[20 * 64 + 20] == expected_texel(3, 5));
    CHECK(bind_calls == before_bind + 1 && fragment_pushes > before_fragment);
    check_textured_bindings(&t);
    puts("PASS: a new command buffer re-pushes fragment constants and rebinds samplers; the same texel is drawn");
}

/* The overlay hooks see each attribute's original data, and their changes to
 * the uploaded copy are the only ones the GPU receives. Interleaved attributes
 * share one copy, so the cleanup of the metadata tag reaches only that copy. */
static const NvD3DAttrib *overlay_source;
static unsigned overlay_sanitize_calls, overlay_place_calls;
static uint8_t *overlay_tag_copy;
static int overlay_place(const NvOverlayDraw *d, float *dx, float *squeeze, float *shift)
{
    CHECK(d->attrib == overlay_source && d->lo == 2 && d->count == 3 && d->ni == 3);
    CHECK(d->attrib[2].type == 1 && d->attrib[2].stride == 16);
    CHECK(d->gw == 640 && d->gh == 480 && d->narrow < 0.76f);
    overlay_place_calls++;
    *squeeze = d->narrow;
    return 0;
}
static void overlay_sanitize(uint8_t *copy, const NvD3DAttrib *a, unsigned attribute,
                             uint32_t lo, uint32_t count)
{
    unsigned n;
    CHECK(a == overlay_source + attribute && lo == 2 && count == 3);
    for (n = 0; n < count; ++n)
        CHECK(!memcmp(copy + n * a->stride, a->data + (lo + n) * a->stride, attribute == 9 ? 4 : 8));
    overlay_sanitize_calls++;
    if (attribute != 2) return;
    overlay_tag_copy = copy;
    for (n = 0; n < count; ++n) memset(copy + n * a->stride, 0, 4);
}
static void test_overlay_upload(void)
{
    float positions[6][4] = { { 0 } };
    int16_t packed[6][8], original[6][8];
    NvD3DAttrib attr[16] = {0};
    float def[16][4] = { { 0 } };
    const uint32_t idx[3] = { 2, 3, 4 };
    NvD3DState st = draw_state();
    NvOverlayHooks hooks = {0};
    unsigned n, k;
    for (n = 0; n < 6; ++n) {
        positions[n][0] = 8.0f + n * 4; positions[n][1] = 8.0f + (n & 1) * 32;
        positions[n][3] = 1.0f;
        for (k = 0; k < 8; ++k) packed[n][k] = (int16_t)(n * 16 + k + 1);
    }
    memcpy(original, packed, sizeof packed);
    attr[0] = (NvD3DAttrib){ 1, 2, 4, 16, (const uint8_t *)positions };
    attr[2] = (NvD3DAttrib){ 1, 1, 4, 16, (const uint8_t *)packed };
    attr[9] = (NvD3DAttrib){ 1, 1, 2, 16, (const uint8_t *)packed + 8 };
    def[3][0] = def[3][3] = 1;
    overlay_source = attr;
    hooks.place = overlay_place; hooks.sanitize_vertex_upload = overlay_sanitize;
    nv2a_d3d_set_overlay_hooks(&hooks);
    nv2a_d3d_draw_ub(&st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 2, 4, NULL);
    CHECK(overlay_place_calls == 0 && overlay_sanitize_calls == 3);
    CHECK(!memcmp(original, packed, sizeof packed));
    /* The copy the GPU uploads (still mapped until the batch runs) has the
     * tags cleared; the title's data is untouched. */
    CHECK(overlay_tag_copy);
    for (n = 0; n < 3; ++n) {
        const int16_t *uploaded = (const int16_t *)(overlay_tag_copy + n * 16);
        CHECK(!uploaded[0] && !uploaded[1]);
        CHECK(!memcmp(uploaded + 2, original[n + 2] + 2, 8));
    }
    /* A wide surface reaches the placement hook. */
    st.color_addr = 0x80000; st.color_pitch = 640 * 4;
    st.zeta_addr = 0; st.depth_test = 0; st.clip_w = 640; st.clip_h = 480;
    s_aspect = 16.0f / 9.0f;
    nv2a_d3d_draw_ub(&st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 2, 4, positions + 2);
    CHECK(overlay_place_calls == 1 && overlay_sanitize_calls == 6);
    CHECK(!memcmp(original, packed, sizeof packed));
    nv2a_d3d_set_overlay_hooks(NULL);
    nv2a_d3d_draw_ub(&st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 2, 4, positions + 2);
    CHECK(overlay_place_calls == 1 && overlay_sanitize_calls == 6);
    puts("PASS: original attributes reach title placement; interleaved metadata cleanup changes only the uploaded copy, including non-wide draws; clearing hooks retires callbacks");
}

static void test_init_failure(void)
{
    /* Set before init (main): the first buffer creation fails. */
    CHECK(!nv2a_d3d_active());
    CHECK(!nv2a_d3d_init());
    CHECK(!nv2a_d3d_init());
    CHECK(!nv2a_d3d_active());
    nv2a_d3d_clear(&(NvD3DState){ 0 }, 0xF0, 0xFF000000, 0, 0, 0, 63, 63);
    puts("PASS: failed initialization publishes no renderer and cannot be used by draws");
}

static void test_resize(void)
{
    NvD3DState st = draw_state();
    uint32_t pixels[64 * 64];
    SDL_Window *win = xbox_FramebufferSdlWindow();
    CHECK(win);
    nv2a_d3d_clear(&st, 0xF0, 0xFF000000, 0, 0, 0, 63, 63);
    triangle(&st, 1, 0);
    read_target(pixels); check_pixel(1, pixels);
    CHECK(SDL_SetWindowSize(win, 320, 200));
    CHECK(SDL_SyncWindow(win));   /* a hidden window applies its size change lazily */
    SDL_PumpEvents();
    display_config(0);
    SDL_Delay(RESIZE_SETTLE_MS + 50);
    SDL_PumpEvents();
    display_config(0);
    CHECK(!surface_find(0x20000) && !s_cur_rt && !s_cur_ds);
    nv2a_d3d_clear(&st, 0xF0, 0xFF000000, 0, 0, 0, 63, 63);
    triangle(&st, 1, 0);
    read_target(pixels); check_pixel(1, pixels);
    puts("PASS: a settled window resize releases surfaces and the next draw renders the exact colour");
}

static void test_common(void)
{
    nv2a_common_checks();
    puts("PASS: shared layout, texel and draw helpers (nv2a_render_common.h)");
}

static unsigned host_draw_calls;
static int host_clear(const NvHostTarget *target,void *user)
{
    CHECK(target->device == s_dev && target->context == s_cmd);
    CHECK(target->width == 64 && target->height == 64 && target->depth_bits == 24);
    CHECK(target->samples == 1 && target->colour && target->depth);
    ++host_draw_calls;
    if (!user) return 0;
    SDL_GPUColorTargetInfo colour = {0};
    colour.texture = target->colour; colour.clear_color.g = colour.clear_color.a = 1;
    colour.load_op = SDL_GPU_LOADOP_CLEAR; colour.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(target->context,&colour,1,NULL);
    CHECK(pass); SDL_EndGPURenderPass(pass); return 1;
}
static void test_host_draw(void)
{
    NvD3DState st = draw_state(), missing;
    uint32_t pixels[64*64];
    triangle(&st,0,0);
    CHECK(nv2a_d3d_host_draw(&st,host_clear,&st));
    read_target(pixels); CHECK((pixels[16*64+16]&0xFFFFFF) == 0x00FF00);
    CHECK(!nv2a_d3d_host_draw(&st,host_clear,NULL));
    read_target(pixels); CHECK((pixels[16*64+16]&0xFFFFFF) == 0x00FF00);
    CHECK(!nv2a_d3d_host_draw(NULL,host_clear,&st));
    CHECK(!nv2a_d3d_host_draw(&st,NULL,&st));
    missing = st; missing.zeta_addr = 0;
    CHECK(!nv2a_d3d_host_draw(&missing,host_clear,&st)); CHECK(host_draw_calls == 2);
    triangle(&st,0,0); read_target(pixels);
    CHECK((pixels[16*64+16]&0xFFFFFF) == 0xFF0000);
    puts("PASS: native target callback ordering, metadata, rejection and subsequent guest rendering");
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    test_setenv("RECOMP_RENDER_SCALE", "1");
    test_setenv("RECOMP_ANISO", "16");
    test_setenv("RECOMP_FPS_LIMIT", "0");
    if (!strcmp(argv[1], "common")) { test_common(); return 0; }
    if (!strcmp(argv[1], "init_failure")) {
        fail_buffers = 1;
        test_init_failure();
        return 0;
    }
    if (!strcmp(argv[1], "resize")) {
        test_setenv("RECOMP_ASPECT", "");
        open_hidden_window();
    } else {
        test_setenv("RECOMP_ASPECT", "4:3");
    }
    CHECK(nv2a_d3d_init());
    if (!strcmp(argv[1], "pixels")) test_pixels();
    else if (!strcmp(argv[1], "host_draw")) test_host_draw();
    else if (!strcmp(argv[1], "failures")) test_failures();
    else if (!strcmp(argv[1], "ps_bindings")) test_ps_bindings();
    else if (!strcmp(argv[1], "ps_reset")) test_ps_reset();
    else if (!strcmp(argv[1], "overlay_upload")) test_overlay_upload();
    else if (!strcmp(argv[1], "resize")) test_resize();
    else CHECK(0);
    return 0;
}
