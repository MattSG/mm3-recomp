/* Host side for the SDL3 GPU renderer's standalone test builds. Include after
 * nv2a_sdlgpu.c. The renderer's framebuffer and guest-memory callbacks are
 * provided here (no presentation, no game): the test sets nv2a_test_ram to its
 * guest RAM image. The window is fb_window_sdl.c's own, opened by
 * nv2a_d3d_init (hidden or offscreen when SDL_VIDEO_DRIVER asks for it). */
#ifndef NV2A_SDLGPU_TEST_HOST_H
#define NV2A_SDLGPU_TEST_HOST_H

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *nv2a_test_ram;
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)nv2a_test_ram; }
/* No window: the GPU tests draw to their own targets. The renderer's window
 * (fb_window_sdl.c) pulls in the game's input layer; these stand in for it. */
SDL_Window *fbw_sdl_open(SDL_WindowFlags extra_flags) { (void)extra_flags; return NULL; }
void fbw_sdl_pump(void) {}
SDL_Window *xbox_FramebufferSdlWindow(void) { return NULL; }
int xbox_FramebufferPaused(void) { return 0; }
int xbox_FramebufferStartHidden(void) { return 0; }
void xbox_FramebufferSdlHints(void) {}
void recomp_sampler_watch_current_thread(void) {}
void recomp_sampler_report(double ms) { (void)ms; }
int xbox_FramebufferScanSource(uint32_t *va, uint32_t *pitch) { (void)va; (void)pitch; return 0; }
int xbox_FramebufferOverlay(XboxOverlay *ovl) { (void)ovl; return 0; }
void xbox_FramebufferNoteFlip(uint32_t draws) { (void)draws; }
void xbox_FramebufferRenderHostOverlay(void *device, void *context, void *target)
{
    (void)device; (void)context; (void)target;
}
#ifndef NV2A_TEST_OWN_VSH   /* a test that replays a native vertex program defines these */
const uint32_t (*nv2a_vsh_program(uint32_t *start, uint32_t *version))[4] { (void)start; (void)version; return NULL; }
const float (*nv2a_vsh_constants(uint32_t *version))[4] { (void)version; return NULL; }
#endif

static void nv2a_test_setenv(const char *name, const char *value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static void nv2a_test_default_env(const char *name, const char *value)
{
    if (!getenv(name)) nv2a_test_setenv(name, value);
}

/* The renderer settings every test starts from: one sample, unscaled, no
 * specialised shaders, no frame cap (a test may preset them before this).
 * Without a display, SDL's offscreen video driver stands in for the window
 * (the GPU device does not need one). */
static int nv2a_test_init(void)
{
    nv2a_test_default_env("RECOMP_MSAA", "1");
    nv2a_test_default_env("RECOMP_RENDER_SCALE", "1");
    nv2a_test_default_env("RECOMP_PS_SPEC", "0");
    nv2a_test_default_env("RECOMP_FPS_LIMIT", "0");
#ifndef _WIN32
    if (!getenv("SDL_VIDEO_DRIVER") && !getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"))
        nv2a_test_setenv("SDL_VIDEO_DRIVER", "offscreen");
#endif
    return nv2a_d3d_init();
}

/* Blocking readback of one level/layer of a texture, tightly packed, with
 * bytes_per_texel bytes per texel (4 for BGRA, 8 for RGBA16). Submits
 * everything recorded so far first, so the copy sees every draw and upload
 * issued before this call. */
static int nv2a_test_read_texels(SDL_GPUTexture *tex, uint32_t w, uint32_t h, uint32_t bytes_per_texel,
                                 uint32_t level, uint32_t layer, void *out)
{
    SDL_GPUTransferBufferCreateInfo ci;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUCopyPass *pass;
    SDL_GPUTextureRegion src;
    SDL_GPUTextureTransferInfo dst;
    SDL_GPUFence *fence = NULL;
    void *p;
    int ok = 0;
    copy_end();
    submit(NULL);
    memset(&ci, 0, sizeof ci);
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ci.size = w * h * bytes_per_texel;
    tb = SDL_CreateGPUTransferBuffer(s_dev, &ci);
    if (!tb) return 0;
    if (!cmd()) { SDL_ReleaseGPUTransferBuffer(s_dev, tb); return 0; }
    pass = SDL_BeginGPUCopyPass(s_cmd);
    memset(&src, 0, sizeof src);
    memset(&dst, 0, sizeof dst);
    src.texture = tex; src.mip_level = level; src.layer = layer;
    src.w = w; src.h = h; src.d = 1;
    dst.transfer_buffer = tb; dst.pixels_per_row = w; dst.rows_per_layer = h;
    SDL_DownloadFromGPUTexture(pass, &src, &dst);
    SDL_EndGPUCopyPass(pass);
    submit(&fence);
    if (fence) {
        SDL_WaitForGPUFences(s_dev, true, &fence, 1);
        SDL_ReleaseGPUFence(s_dev, fence);
        p = SDL_MapGPUTransferBuffer(s_dev, tb, false);
        if (p) {
            memcpy(out, p, (size_t)w * h * bytes_per_texel);
            SDL_UnmapGPUTransferBuffer(s_dev, tb);
            ok = 1;
        }
    }
    SDL_ReleaseGPUTransferBuffer(s_dev, tb);
    return ok;
}

/* One level/layer as BGRA texels, the renderer's native pixel order. */
static int nv2a_test_read_level(SDL_GPUTexture *tex, uint32_t w, uint32_t h,
                                uint32_t level, uint32_t layer, uint32_t *out)
{
    return nv2a_test_read_texels(tex, w, h, 4, level, layer, out);
}

/* Texel (x, y) of the colour target at guest address color_va, as the GPU
 * holds it now (0xDEADBEEF if the target is missing or unreadable). */
static uint32_t nv2a_test_target_pixel(uint32_t color_va, uint32_t x, uint32_t y)
{
    Surface *s = surface_find(phys(color_va));
    uint32_t *texels, result = 0xDEADBEEFu;
    if (!s || !s->tex) return result;
    texels = malloc((size_t)s->iw * s->ih * 4);
    if (!texels) return result;
    if (nv2a_test_read_level(s->tex, s->iw, s->ih, 0, 0, texels))
        result = texels[y * s->iw + x];
    free(texels);
    return result;
}

/* A triangle covering the centre of a 64x64 clip, drawn with the fixed-function
 * diffuse colour (ARGB) through the renderer's normal draw path. */
static void nv2a_test_triangle(const NvD3DState *st, uint32_t argb, int program)
{
    static const uint32_t indices[] = {0, 1, 2};
    float vertices[3][4] = {{-8, -8, .5f, 1}, {200, -8, .5f, 1}, {-8, 200, .5f, 1}};
    float defaults[16][4] = {{0}};
    NvD3DAttrib attributes[16] = {{0}};
    unpack4(argb, defaults[3]);
    attributes[0].enabled = 1;
    attributes[0].type = 2;
    attributes[0].size = 4;
    attributes[0].stride = sizeof vertices[0];
    attributes[0].data = (const uint8_t *)vertices;
    nv2a_d3d_draw_ub(st, NV_D3D_TRIANGLES, attributes, defaults, program, indices, 3, 0, 2, NULL);
}


/* Standalone renderer fixtures do not request executor replay. */
void nv2a_pb_interp_enable(int on) { (void)on; abort(); }
int nv2a_pb_replay(float t) { (void)t; abort(); return 0; }
int xbox_GetDisplayMode(uint32_t *pitch, uint32_t *bpp, uint32_t *w, uint32_t *h)
{ (void)pitch; (void)bpp; (void)w; (void)h; return 0; }

#endif
