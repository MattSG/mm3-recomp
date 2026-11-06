/**
 * SDL_GPU device, window and backbuffer for the D3D8 layer.
 *
 * One window, one device. The D3D8 backbuffer is an offscreen texture owned
 * here, not the swapchain image: that keeps GetBackBuffer/LockRect/CopyRects
 * working and lets a readback in the middle of a frame submit and restart the
 * command buffer without touching the swapchain. Present blits the backbuffer
 * to the swapchain and presents.
 *
 * SDL_GPU does not allow copy passes inside render passes (or the reverse), but
 * D3D8 interleaves uploads and draws freely. Everything therefore goes through
 * xgpu_copy_pass() / xgpu_render_pass(), which end the other kind first.
 *
 * Uploads and downloads work with or without a frame in progress.
 */
#ifndef XBOX_GPU_DEVICE_H
#define XBOX_GPU_DEVICE_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

struct XGpu;

/* The attachments a render pass draws into. Passes are reused while the
 * targets stay the same; any change ends the pass and starts a new one. */
typedef struct XGpuTargets {
    SDL_GPUTexture *color;               /* NULL = the backbuffer */
    SDL_GPUTexture *depth;               /* may be NULL */
    Uint32          color_mip, color_layer;   /* which level/face of `color` */
} XGpuTargets;

/* Which attachments a new render pass clears instead of loading. */
typedef struct XGpuClear {
    int   color, depth, stencil;
    float rgba[4];
    float z;
    Uint8 stencil_value;
} XGpuClear;

typedef struct XGpu {
    SDL_Window            *window;
    SDL_GPUDevice         *device;
    SDL_GPUCommandBuffer  *cmd;          /* NULL until something needs it */
    SDL_GPUTexture        *backbuffer;   /* owned, BGRA8, color target + sampler */
    SDL_GPUTextureFormat   swap_format;
    Uint32                 width, height;    /* backbuffer size */

    SDL_GPURenderPass     *render;
    SDL_GPUCopyPass       *copy;
    XGpuTargets            targets;      /* of the open render pass */

    /* Upload ring: mapped for as long as a command buffer is open. */
    SDL_GPUTransferBuffer *ring;
    Uint8                 *ring_ptr;
    Uint32                 ring_size, ring_used;

    int                    in_frame;     /* between xgpu_begin_frame and present */

    /* Optional: draw the backbuffer into the swapchain image yourself (gamma,
     * scaling filters). Return nonzero if you did; zero falls back to a blit. */
    int                  (*present_hook)(struct XGpu *g, SDL_GPUTexture *swap,
                                         Uint32 swap_w, Uint32 swap_h);
    SDL_GPUTextureFormat   present_format;   /* swapchain format, for pipelines */
} XGpu;

/* hidden != 0 creates the window without showing it (tests, headless runs).
 * Returns 0 on failure with the reason on stderr. */
int  xgpu_init(XGpu *g, const char *title, int width, int height, int hidden);
void xgpu_shutdown(XGpu *g);

/* Marks the start of a frame. Always succeeds once xgpu_init has. */
int  xgpu_begin_frame(XGpu *g);

/* The current command buffer, opening one if needed. */
SDL_GPUCommandBuffer *xgpu_cmd(XGpu *g);

/* Ends whichever pass is open. */
void xgpu_end_passes(XGpu *g);

/* The open copy pass, ending any render pass first. */
SDL_GPUCopyPass *xgpu_copy_pass(XGpu *g);

/* The render pass for `t`. Reuses the open pass when the targets match and no
 * clear is asked for; otherwise ends it and starts one, loading existing
 * contents except where `clear` says otherwise. */
SDL_GPURenderPass *xgpu_render_pass(XGpu *g, XGpuTargets t, const XGpuClear *clear);

/* Space in the upload ring for `size` bytes (16-byte aligned). Write through
 * the returned pointer; (*tb, *offset) is where the GPU reads it from. A request
 * that does not fit gets its own transfer buffer, freed once the GPU is done.
 * Returns NULL on failure. */
void *xgpu_stage(XGpu *g, Uint32 size, SDL_GPUTransferBuffer **tb, Uint32 *offset);

/* Copies `size` bytes into a GPU buffer at `offset`. */
int xgpu_upload_buffer(XGpu *g, SDL_GPUBuffer *buf, Uint32 offset,
                       const void *data, Uint32 size);

/* The same with a cycle flag: true lets SDL swap in a fresh backing store if
 * the GPU is still reading the old one (earlier recorded draws keep theirs).
 * Used for the dynamic vertex ring. */
int xgpu_upload_buffer_ex(XGpu *g, SDL_GPUBuffer *buf, Uint32 offset,
                          const void *data, Uint32 size, int cycle);

/* Copies a w*h*d block of texels into mip level `mip` of `layer`. `row_bytes`
 * is the source row pitch in bytes; for block-compressed formats w/h are the
 * block-aligned texel size and rows are block rows (the caller passes the
 * number of rows in `rows`). */
int xgpu_upload_texture(XGpu *g, SDL_GPUTexture *tex, Uint32 mip, Uint32 layer,
                        Uint32 x, Uint32 y, Uint32 z, Uint32 w, Uint32 h, Uint32 d,
                        const void *data, Uint32 row_bytes, Uint32 rows);

/* Submits what is recorded so far. wait != 0 blocks until the GPU finishes. */
void xgpu_flush(XGpu *g, int wait);

/* Synchronous readback of a w*h region of one mip/layer into `out`
 * (tightly packed rows of `row_bytes`). Ends the open passes and flushes. */
int xgpu_download_texture(XGpu *g, SDL_GPUTexture *tex, Uint32 mip, Uint32 layer,
                          Uint32 x, Uint32 y, Uint32 w, Uint32 h,
                          void *out, Uint32 row_bytes);

/* Clears the backbuffer. */
void xgpu_clear_backbuffer(XGpu *g, float r, float g_, float b, float a);

/* Blits the backbuffer to the window and presents. */
void xgpu_present(XGpu *g);

/* A reference-counted GPU texture. SDL_GPU textures have no refcount, but D3D8
 * surfaces keep their parent texture alive, so the D3D8 layer holds these. */
typedef struct XGpuTex {
    SDL_GPUTexture        *tex;
    int                    refs;
    SDL_GPUTextureType     type;
    SDL_GPUTextureFormat   format;
    Uint32                 width, height, depth_or_layers, levels;
    int                    borrowed;      /* not ours to release (the backbuffer) */
} XGpuTex;

/* Returns NULL on failure. The caller owns one reference. */
XGpuTex *xgputex_create(XGpu *g, SDL_GPUTextureType type, SDL_GPUTextureFormat format,
                        Uint32 w, Uint32 h, Uint32 depth_or_layers, Uint32 levels,
                        SDL_GPUTextureUsageFlags usage);
/* Wraps a texture owned elsewhere; unref never releases it. */
XGpuTex *xgputex_wrap(SDL_GPUTexture *tex, SDL_GPUTextureFormat format, Uint32 w, Uint32 h);
XGpuTex *xgputex_ref(XGpuTex *t);
void     xgputex_unref(XGpu *g, XGpuTex *t);

#endif
