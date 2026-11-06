#include "gpu_device.h"

#include <stdio.h>
#include <string.h>

#define RING_BYTES (32u * 1024u * 1024u)

static int make_backbuffer(XGpu *g, Uint32 w, Uint32 h)
{
    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = w;
    ti.height = h;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    g->backbuffer = SDL_CreateGPUTexture(g->device, &ti);
    g->width = w;
    g->height = h;
    return g->backbuffer != NULL;
}

int xgpu_init(XGpu *g, const char *title, int width, int height, int hidden)
{
    SDL_WindowFlags flags = hidden ? SDL_WINDOW_HIDDEN : 0;

    memset(g, 0, sizeof(*g));
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[gpu] SDL video init failed: %s\n", SDL_GetError());
        return 0;
    }
    g->window = SDL_CreateWindow(title, width, height, flags);
    if (!g->window) {
        fprintf(stderr, "[gpu] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }
    g->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                    SDL_GPU_SHADERFORMAT_MSL, false, NULL);
    if (!g->device) {
        fprintf(stderr, "[gpu] SDL_CreateGPUDevice failed: %s\n", SDL_GetError());
        xgpu_shutdown(g);
        return 0;
    }
    if (!SDL_ClaimWindowForGPUDevice(g->device, g->window)) {
        fprintf(stderr, "[gpu] ClaimWindow failed: %s\n", SDL_GetError());
        xgpu_shutdown(g);
        return 0;
    }
    g->swap_format = SDL_GetGPUSwapchainTextureFormat(g->device, g->window);
    g->present_format = g->swap_format;
    if (!make_backbuffer(g, (Uint32)width, (Uint32)height)) {
        fprintf(stderr, "[gpu] backbuffer failed: %s\n", SDL_GetError());
        xgpu_shutdown(g);
        return 0;
    }
    fprintf(stderr, "[gpu] %s device, %dx%d\n", SDL_GetGPUDeviceDriver(g->device), width, height);
    return 1;
}

void xgpu_end_passes(XGpu *g)
{
    if (g->render) { SDL_EndGPURenderPass(g->render); g->render = NULL; }
    if (g->copy)   { SDL_EndGPUCopyPass(g->copy);     g->copy = NULL; }
}

void xgpu_shutdown(XGpu *g)
{
    if (g->cmd) {
        xgpu_end_passes(g);
        if (g->ring_ptr) SDL_UnmapGPUTransferBuffer(g->device, g->ring);
        SDL_CancelGPUCommandBuffer(g->cmd);
        g->cmd = NULL;
    }
    if (g->device) {
        SDL_WaitForGPUIdle(g->device);
        if (g->backbuffer) SDL_ReleaseGPUTexture(g->device, g->backbuffer);
        if (g->ring) SDL_ReleaseGPUTransferBuffer(g->device, g->ring);
        if (g->window) SDL_ReleaseWindowFromGPUDevice(g->device, g->window);
        SDL_DestroyGPUDevice(g->device);
    }
    if (g->window) SDL_DestroyWindow(g->window);
    memset(g, 0, sizeof(*g));
}

SDL_GPUCommandBuffer *xgpu_cmd(XGpu *g)
{
    if (g->cmd)
        return g->cmd;
    if (!g->ring) {
        SDL_GPUTransferBufferCreateInfo ci;
        SDL_zero(ci);
        ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ci.size = RING_BYTES;
        g->ring = SDL_CreateGPUTransferBuffer(g->device, &ci);
        g->ring_size = RING_BYTES;
    }
    g->cmd = SDL_AcquireGPUCommandBuffer(g->device);
    /* cycle: if an earlier submission still reads the ring, SDL hands back a
     * fresh backing buffer instead of stalling. */
    g->ring_ptr = g->ring ? (Uint8 *)SDL_MapGPUTransferBuffer(g->device, g->ring, true) : NULL;
    g->ring_used = 0;
    return g->cmd;
}

int xgpu_begin_frame(XGpu *g)
{
    g->in_frame = 1;
    return xgpu_cmd(g) != NULL;
}

SDL_GPUCopyPass *xgpu_copy_pass(XGpu *g)
{
    if (g->render) { SDL_EndGPURenderPass(g->render); g->render = NULL; }
    if (!g->copy && xgpu_cmd(g))
        g->copy = SDL_BeginGPUCopyPass(g->cmd);
    return g->copy;
}

SDL_GPURenderPass *xgpu_render_pass(XGpu *g, XGpuTargets t, const XGpuClear *clear)
{
    SDL_GPUColorTargetInfo c;
    SDL_GPUDepthStencilTargetInfo d;
    SDL_GPUTexture *color = t.color ? t.color : g->backbuffer;

    if (!xgpu_cmd(g))
        return NULL;
    if (g->render && !clear && g->targets.color == t.color && g->targets.depth == t.depth &&
        g->targets.color_mip == t.color_mip && g->targets.color_layer == t.color_layer)
        return g->render;
    xgpu_end_passes(g);

    SDL_zero(c);
    c.texture = color;
    c.mip_level = t.color_mip;
    c.layer_or_depth_plane = t.color_layer;
    c.store_op = SDL_GPU_STOREOP_STORE;
    if (clear && clear->color) {
        c.load_op = SDL_GPU_LOADOP_CLEAR;
        c.clear_color.r = clear->rgba[0]; c.clear_color.g = clear->rgba[1];
        c.clear_color.b = clear->rgba[2]; c.clear_color.a = clear->rgba[3];
    } else {
        c.load_op = SDL_GPU_LOADOP_LOAD;
    }
    SDL_zero(d);
    if (t.depth) {
        d.texture = t.depth;
        d.store_op = SDL_GPU_STOREOP_STORE;
        d.stencil_store_op = SDL_GPU_STOREOP_STORE;
        /* Depth and stencil share a texture but D3D8 clears them separately. */
        d.load_op = (clear && clear->depth) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        d.stencil_load_op = (clear && clear->stencil) ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        if (clear) {
            d.clear_depth = clear->z;
            d.clear_stencil = clear->stencil_value;
        }
    }
    g->render = SDL_BeginGPURenderPass(g->cmd, &c, 1, t.depth ? &d : NULL);
    g->targets = t;
    return g->render;
}

void *xgpu_stage(XGpu *g, Uint32 size, SDL_GPUTransferBuffer **tb, Uint32 *offset)
{
    Uint32 at;
    SDL_GPUTransferBufferCreateInfo ci;
    SDL_GPUTransferBuffer *own;
    void *p;

    xgpu_cmd(g);                          /* maps the ring */
    at = (g->ring_used + 15u) & ~15u;
    if (g->ring_ptr && at + size <= g->ring_size) {
        g->ring_used = at + size;
        *tb = g->ring;
        *offset = at;
        return g->ring_ptr + at;
    }
    /* Does not fit this frame's ring: give the upload its own buffer. Releasing
     * it right away is safe, SDL destroys it once the GPU is done with it. */
    SDL_zero(ci);
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ci.size = size;
    own = SDL_CreateGPUTransferBuffer(g->device, &ci);
    if (!own)
        return NULL;
    p = SDL_MapGPUTransferBuffer(g->device, own, false);
    SDL_ReleaseGPUTransferBuffer(g->device, own);
    *tb = own;
    *offset = 0;
    return p;
}

/* Outside a frame nothing will submit the command buffer, so uploads do. */
static void flush_if_idle(XGpu *g)
{
    if (!g->in_frame)
        xgpu_flush(g, 0);
}

int xgpu_upload_buffer(XGpu *g, SDL_GPUBuffer *buf, Uint32 offset,
                       const void *data, Uint32 size)
{
    return xgpu_upload_buffer_ex(g, buf, offset, data, size, 0);
}

int xgpu_upload_buffer_ex(XGpu *g, SDL_GPUBuffer *buf, Uint32 offset,
                          const void *data, Uint32 size, int cycle)
{
    SDL_GPUTransferBuffer *tb;
    SDL_GPUTransferBufferLocation src;
    SDL_GPUBufferRegion dst;
    Uint32 at;
    void *p = xgpu_stage(g, size, &tb, &at);
    SDL_GPUCopyPass *cp;

    if (!p)
        return 0;
    memcpy(p, data, size);
    cp = xgpu_copy_pass(g);
    if (!cp)
        return 0;
    SDL_zero(src); SDL_zero(dst);
    src.transfer_buffer = tb; src.offset = at;
    dst.buffer = buf; dst.offset = offset; dst.size = size;
    SDL_UploadToGPUBuffer(cp, &src, &dst, cycle != 0);
    flush_if_idle(g);
    return 1;
}

int xgpu_upload_texture(XGpu *g, SDL_GPUTexture *tex, Uint32 mip, Uint32 layer,
                        Uint32 x, Uint32 y, Uint32 z, Uint32 w, Uint32 h, Uint32 d,
                        const void *data, Uint32 row_bytes, Uint32 rows)
{
    SDL_GPUTransferBuffer *tb;
    SDL_GPUTextureTransferInfo src;
    SDL_GPUTextureRegion dst;
    Uint32 at, total = row_bytes * rows * d;
    void *p = xgpu_stage(g, total, &tb, &at);
    SDL_GPUCopyPass *cp;

    if (!p)
        return 0;
    memcpy(p, data, total);
    cp = xgpu_copy_pass(g);
    if (!cp)
        return 0;
    SDL_zero(src); SDL_zero(dst);
    src.transfer_buffer = tb; src.offset = at;
    src.pixels_per_row = w; src.rows_per_layer = h;
    dst.texture = tex; dst.mip_level = mip; dst.layer = layer;
    dst.x = x; dst.y = y; dst.z = z; dst.w = w; dst.h = h; dst.d = d;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    flush_if_idle(g);
    return 1;
}

void xgpu_flush(XGpu *g, int wait)
{
    SDL_GPUFence *fence;

    if (!g->cmd)
        return;
    xgpu_end_passes(g);
    if (g->ring_ptr) {
        SDL_UnmapGPUTransferBuffer(g->device, g->ring);
        g->ring_ptr = NULL;
    }
    if (wait) {
        fence = SDL_SubmitGPUCommandBufferAndAcquireFence(g->cmd);
        if (fence) {
            SDL_WaitForGPUFences(g->device, true, &fence, 1);
            SDL_ReleaseGPUFence(g->device, fence);
        }
    } else {
        SDL_SubmitGPUCommandBuffer(g->cmd);
    }
    g->cmd = NULL;
}

int xgpu_download_texture(XGpu *g, SDL_GPUTexture *tex, Uint32 mip, Uint32 layer,
                          Uint32 x, Uint32 y, Uint32 w, Uint32 h,
                          void *out, Uint32 row_bytes)
{
    SDL_GPUTransferBufferCreateInfo ci;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUTextureRegion src;
    SDL_GPUTextureTransferInfo dst;
    SDL_GPUCopyPass *cp;
    Uint8 *p;

    SDL_zero(ci);
    ci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ci.size = row_bytes * h;
    tb = SDL_CreateGPUTransferBuffer(g->device, &ci);
    if (!tb)
        return 0;
    cp = xgpu_copy_pass(g);
    if (!cp) {
        SDL_ReleaseGPUTransferBuffer(g->device, tb);
        return 0;
    }
    SDL_zero(src); SDL_zero(dst);
    src.texture = tex; src.mip_level = mip; src.layer = layer;
    src.x = x; src.y = y; src.w = w; src.h = h; src.d = 1;
    dst.transfer_buffer = tb; dst.pixels_per_row = w; dst.rows_per_layer = h;
    SDL_DownloadFromGPUTexture(cp, &src, &dst);
    xgpu_flush(g, 1);                      /* the pixels exist once this returns */
    p = (Uint8 *)SDL_MapGPUTransferBuffer(g->device, tb, false);
    if (p) {
        memcpy(out, p, (size_t)row_bytes * h);
        SDL_UnmapGPUTransferBuffer(g->device, tb);
    }
    SDL_ReleaseGPUTransferBuffer(g->device, tb);
    return p != NULL;
}

void xgpu_clear_backbuffer(XGpu *g, float r, float g_, float b, float a)
{
    XGpuClear cl;
    XGpuTargets t;

    SDL_zero(cl); SDL_zero(t);
    cl.color = 1;
    cl.rgba[0] = r; cl.rgba[1] = g_; cl.rgba[2] = b; cl.rgba[3] = a;
    xgpu_render_pass(g, t, &cl);
    xgpu_end_passes(g);
}

void xgpu_present(XGpu *g)
{
    SDL_GPUTexture *swap = NULL;
    Uint32 sw = 0, sh = 0;

    if (xgpu_cmd(g)) {
        xgpu_end_passes(g);
        if (SDL_WaitAndAcquireGPUSwapchainTexture(g->cmd, g->window, &swap, &sw, &sh) && swap &&
            !(g->present_hook && g->present_hook(g, swap, sw, sh))) {
            SDL_GPUBlitInfo bi;
            SDL_zero(bi);
            bi.source.texture = g->backbuffer;
            bi.source.w = g->width; bi.source.h = g->height;
            bi.destination.texture = swap;
            bi.destination.w = sw; bi.destination.h = sh;
            bi.load_op = SDL_GPU_LOADOP_DONT_CARE;
            bi.filter = SDL_GPU_FILTER_LINEAR;
            SDL_BlitGPUTexture(g->cmd, &bi);
        }
        xgpu_flush(g, 0);                  /* submitting presents the acquired image */
    }
    g->in_frame = 0;
}

XGpuTex *xgputex_create(XGpu *g, SDL_GPUTextureType type, SDL_GPUTextureFormat format,
                        Uint32 w, Uint32 h, Uint32 depth_or_layers, Uint32 levels,
                        SDL_GPUTextureUsageFlags usage)
{
    SDL_GPUTextureCreateInfo ti;
    XGpuTex *t;

    SDL_zero(ti);
    ti.type = type;
    ti.format = format;
    ti.usage = usage;
    ti.width = w;
    ti.height = h;
    ti.layer_count_or_depth = depth_or_layers;
    ti.num_levels = levels;
    t = (XGpuTex *)SDL_calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    t->tex = SDL_CreateGPUTexture(g->device, &ti);
    if (!t->tex) {
        fprintf(stderr, "[gpu] texture create failed (fmt=%d %ux%u): %s\n",
                (int)format, w, h, SDL_GetError());
        SDL_free(t);
        return NULL;
    }
    t->refs = 1;
    t->type = type;
    t->format = format;
    t->width = w; t->height = h;
    t->depth_or_layers = depth_or_layers;
    t->levels = levels;
    return t;
}

XGpuTex *xgputex_ref(XGpuTex *t)
{
    if (t)
        t->refs++;
    return t;
}

void xgputex_unref(XGpu *g, XGpuTex *t)
{
    if (t && --t->refs <= 0) {
        if (!t->borrowed)
            SDL_ReleaseGPUTexture(g->device, t->tex);
        SDL_free(t);
    }
}

XGpuTex *xgputex_wrap(SDL_GPUTexture *tex, SDL_GPUTextureFormat format, Uint32 w, Uint32 h)
{
    XGpuTex *t = (XGpuTex *)SDL_calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    t->tex = tex;
    t->refs = 1;
    t->type = SDL_GPU_TEXTURETYPE_2D;
    t->format = format;
    t->width = w; t->height = h;
    t->depth_or_layers = 1;
    t->levels = 1;
    t->borrowed = 1;
    return t;
}
