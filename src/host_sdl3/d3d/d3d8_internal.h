/**
 * D3D8 Compatibility Layer - Internal Header
 *
 * Shared types and declarations for the D3D8→SDL_GPU implementation.
 * Not part of the public API - only included by d3d8_*.c files.
 */

#ifndef BURNOUT3_D3D8_INTERNAL_H
#define BURNOUT3_D3D8_INTERNAL_H

#define COBJMACROS
#include "d3d8_xbox.h"

/* D3D8 device accessor used by recompiled code. */
IDirect3DDevice8 *d3d8_GetDevice(void);

#include "gfx/gpu_device.h"

/* ================================================================
 * GPU device accessors (implemented in d3d8_device.c)
 * ================================================================ */

IDirect3DDevice8    *d3d8_GetDevice(void);
XGpu                *d3d8_GetGpu(void);
UINT                 d3d8_GetBackbufferWidth(void);
UINT                 d3d8_GetBackbufferHeight(void);

/* Presentation-only gamma, applied when the backbuffer is blitted to the window. */
void d3d8_gamma_set(const D3DGAMMARAMP *ramp);
void d3d8_gamma_get(D3DGAMMARAMP *ramp);
void d3d8_gamma_shutdown(void);
/* XGpu.present_hook: draws the backbuffer into the swapchain image through the ramp. */
int  d3d8_gamma_present(XGpu *g, SDL_GPUTexture *swap, Uint32 swap_w, Uint32 swap_h);

/* Current render state array accessor */
const DWORD         *d3d8_GetRenderStates(void);
const DWORD         *d3d8_GetTSS(DWORD stage);

/* Base texture currently bound to a texture stage (2D/cube/volume),
 * or NULL if nothing is bound. */
IDirect3DBaseTexture8 *d3d8_GetStageTexture(DWORD stage);

/* Transform accessors */
const D3DMATRIX     *d3d8_GetTransform(D3DTRANSFORMSTATETYPE type);

/* Lighting accessors (d3d8_device.c) */
const D3DLIGHT8     *d3d8_GetLight(DWORD index);
BOOL                 d3d8_GetLightEnable(DWORD index);
const D3DMATERIAL8  *d3d8_GetMaterial(void);
UINT                 d3d8_GetNumLights(void);
DWORD                d3d8_GetCurrentFVF(void);

/* ================================================================
 * Resource wrapper structures
 * ================================================================ */

typedef struct D3D8VertexBuffer {
    IDirect3DVertexBuffer8  iface;      /* COM interface (must be first) */
    LONG                    ref_count;
    SDL_GPUBuffer          *gpu_buffer;
    UINT                    size;
    DWORD                   fvf;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* System memory for Lock */
    BOOL                    locked;
    BOOL                    dirty;
} D3D8VertexBuffer;

typedef struct D3D8IndexBuffer {
    IDirect3DIndexBuffer8   iface;
    LONG                    ref_count;
    SDL_GPUBuffer          *gpu_buffer;
    UINT                    size;
    D3DFORMAT               format;     /* INDEX16 or INDEX32 */
    DWORD                   usage;
    BYTE                   *sys_mem;
    BOOL                    locked;
    BOOL                    dirty;
} D3D8IndexBuffer;

typedef struct D3D8Texture {
    IDirect3DTexture8       iface;
    LONG                    ref_count;
    XGpuTex                *gpu;        /* sampled by any bound base texture, see d3d8_base_gpu_texture */
    UINT                    width;
    UINT                    height;
    UINT                    levels;
    D3DFORMAT               d3d8_format;
    SDL_GPUTextureFormat    gpu_format;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* All mip levels, back to back (level 0 first) */
    UINT                    pitch;      /* Row pitch of level 0 */
    BOOL                    locked;
    BOOL                    dirty;
    UINT                    palette;    /* Palette index (texture stage) this P8 texture bakes */
} D3D8Texture;

typedef struct D3D8Surface {
    IDirect3DSurface8       iface;
    LONG                    ref_count;
    XGpuTex                *gpu;
    UINT                    width;
    UINT                    height;
    D3DFORMAT               format;
    D3DPOOL                 pool;
    DWORD                   usage;

    /* The mip level and array layer (cube face) this surface aliases.
     * Both 0 for a standalone offscreen surface. */
    UINT                    mip;
    UINT                    layer;
    BOOL                    is_depth;

    /* Multisample state (requested Xbox type; the backend renders single-sampled). */
    D3DMULTISAMPLE_TYPE     multsample_type;
    UINT                    sample_count;

    /* Surface LockRect: the region is read back into lock_buf and written
     * back on UnlockRect. */
    BYTE                   *lock_buf;
    BYTE                   *locked_bits;
    INT                     locked_pitch;
    UINT                    lock_x;
    UINT                    lock_y;
    UINT                    lock_w;
    UINT                    lock_h;
    BOOL                    locked;
    BOOL                    lock_readonly;

    /* P8 (palettized) surfaces: raw index data lives in the parent
     * texture's sys_mem. LockRect returns the indices directly instead
     * of the palette-expanded BGRA that the GPU texture holds. */
    BOOL                    palettized;
    const BYTE             *palette_sys;  /* raw level data (1 byte/texel) */
    UINT                    palette_pitch;
    UINT                    palette_index; /* stage palette baked into it */
} D3D8Surface;

/* 2D texture. See tex_* implementation.
 *
 * D3D8CubeTexture/D3D8VolumeTexture intentionally mirror the field
 * layout of D3D8Texture up to and including the `gpu` member (a
 * "layout overlay"), so dev_SetTexture can fetch the GPU texture of any
 * bound base texture through the D3D8Texture offset. */
typedef struct D3D8CubeTexture {
    IDirect3DCubeTexture8   iface;
    LONG                    ref_count;
    XGpuTex                *gpu;
    UINT                    width;      /* edge length */
    UINT                    height;     /* == width (cube faces are square) */
    UINT                    levels;
    D3DFORMAT               d3d8_format;
    SDL_GPUTextureFormat    gpu_format;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* 6 faces * mip chain per face */
    UINT                    pitch;      /* row pitch of face level 0 */
    BOOL                    locked;
    BOOL                    dirty;
    UINT                    palette;    /* Palette index (texture stage) this P8 texture bakes */
} D3D8CubeTexture;

/* 3D texture. The leading fields mirror D3D8CubeTexture
 * up to `gpu` (layout overlay) when read through the D3D8Texture prefix.
 * Note `depth` is placed AFTER `levels` so width/height/levels stay at the
 * same offsets as the 2D types. */
typedef struct D3D8VolumeTexture {
    IDirect3DVolumeTexture8 iface;
    LONG                    ref_count;
    XGpuTex                *gpu;
    UINT                    width;
    UINT                    height;
    UINT                    levels;
    UINT                    depth;
    D3DFORMAT               d3d8_format;
    SDL_GPUTextureFormat    gpu_format;
    DWORD                   usage;
    BYTE                   *sys_mem;    /* all levels, back to back */
    UINT                    pitch;      /* row pitch of level 0 */
    BOOL                    locked;
    BOOL                    dirty;
    UINT                    palette;    /* Palette index this P8 texture bakes */
} D3D8VolumeTexture;

/* A single volume level handed out by GetVolumeLevel(). */
typedef struct D3D8Volume {
    IDirect3DVolume8        iface;
    LONG                    ref_count;
    IDirect3DVolumeTexture8 *parent;    /* owning texture (AddRef'd) */
    UINT                    level;
} D3D8Volume;

/* ================================================================
 * Format conversion (d3d8_resources.c)
 * ================================================================ */

SDL_GPUTextureFormat d3d8_to_gpu_format(D3DFORMAT fmt);   /* INVALID if unsupported */
UINT        d3d8_format_bpp(D3DFORMAT fmt);
BOOL        d3d8_format_is_compressed(D3DFORMAT fmt);
UINT        d3d8_row_pitch(D3DFORMAT fmt, UINT width);

/* Is the Xbox format a depth/stencil format? */
BOOL d3d8_format_is_depth(D3DFORMAT fmt);

/* bpp of the data actually uploaded to the GPU (post-conversion). */
UINT d3d8_upload_bpp(D3DFORMAT fmt);

/* Does this format need software conversion at upload time? */
BOOL d3d8_format_has_conversion(D3DFORMAT fmt);

/* Is this a palettized (P8) format? */
BOOL d3d8_format_is_palettized(D3DFORMAT fmt);

/* Convert one linear (unswizzled) row of pixels from the Xbox layout
 * to the layout expected by the GPU texture. dst holds
 * d3d8_upload_bpp()/8 * width bytes per row. src is the raw
 * d3d8_format_bpp()/8 * width pitch. `palette` is the palette index
 * (texture stage) P8 texels are expanded through. */
void d3d8_convert_linear_pixels(D3DFORMAT fmt, UINT width, UINT height,
                                const BYTE *src, BYTE *dst, UINT palette);

/* Surface implementation (d3d8_resources.c) */
IDirect3DSurface8 *d3d8_surface_create(XGpuTex *texture,
                                       UINT mip_slice,
                                       UINT array_slice,
                                       UINT width, UINT height,
                                       D3DFORMAT fmt, D3DPOOL pool,
                                       DWORD usage,
                                       D3DMULTISAMPLE_TYPE multsample_type,
                                       const BYTE *raw_level_data,
                                       UINT palette_index);

/* Wrap a GPU texture (back buffer, etc.) as a surface. */
IDirect3DSurface8 *xbox_d3d8_surface_wrap(XGpuTex *texture,
                                          UINT width, UINT height,
                                          D3DFORMAT fmt);

HRESULT d3d8_CreateImageSurfaceImpl(UINT Width, UINT Height, D3DFORMAT Format,
                                    IDirect3DSurface8 **ppSurface);

/* Fetch the GPU texture of any bound base texture (2D, cube or volume).
 * All three implementations keep it at the same offset as D3D8Texture. */
SDL_GPUTexture *d3d8_base_gpu_texture(IDirect3DBaseTexture8 *texture);

/* Upload a vertex/index buffer's system copy if Unlock left it dirty. */
void d3d8_vb_sync(IDirect3DVertexBuffer8 *vb);
void d3d8_ib_sync(IDirect3DIndexBuffer8 *ib);

/* Read the D3DFORMAT of any base texture. */
D3DFORMAT d3d8_base_format(IDirect3DBaseTexture8 *texture);
void      d3d8_base_set_palette(IDirect3DBaseTexture8 *texture, UINT palette);

/* Re-upload every level of a P8 (palettized) texture through its
 * current stage palette. No-op for non-palettized textures. */
void d3d8_refresh_palette(IDirect3DBaseTexture8 *texture);

/* Cube/volume texture implementations (d3d8_resources.c) */
HRESULT d3d8_CreateCubeTextureImpl(UINT EdgeLength, UINT Levels, DWORD Usage,
                                   D3DFORMAT Format, IDirect3DCubeTexture8 **ppTex);
HRESULT d3d8_CreateVolumeTextureImpl(UINT Width, UINT Height, UINT Depth,
                                     UINT Levels, DWORD Usage, D3DFORMAT Format,
                                     IDirect3DVolumeTexture8 **ppTex);

/* ================================================================
 * Palette management (d3d8_device.c)
 * ================================================================ */

#define D3D8_MAX_PALETTES 4
void            d3d8_SetPalette(DWORD stage, const DWORD *entries);
const DWORD    *d3d8_GetPalette(DWORD stage);

/* Resource creation (d3d8_resources.c) */
HRESULT d3d8_CreateVertexBufferImpl(UINT Length, DWORD Usage, DWORD FVF, IDirect3DVertexBuffer8 **ppVB);
HRESULT d3d8_CreateIndexBufferImpl(UINT Length, DWORD Usage, D3DFORMAT Format, IDirect3DIndexBuffer8 **ppIB);
HRESULT d3d8_CreateTextureImpl(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, IDirect3DTexture8 **ppTex);

/* ================================================================
 * Shader management (d3d8_shaders.c)
 * ================================================================ */

HRESULT d3d8_shaders_init(void);
void    d3d8_shaders_shutdown(void);

/* Select programmable/FVF vertex state and refresh fixed-function pixel state. */
void    d3d8_shaders_prepare_draw(DWORD handle);

/* ================================================================
 * NV2A Register Combiner pixel shaders (d3d8_combiners.c)
 * ================================================================ */

#include "d3d8_combiners.h"

/* ================================================================
 * NV2A Programmable Vertex Shaders (d3d8_vsh.c)
 * ================================================================ */

#include "d3d8_vsh.h"

/* ================================================================
 * Render state management (d3d8_states.c)
 * ================================================================ */

HRESULT d3d8_states_init(void);
void    d3d8_states_shutdown(void);

/* The fixed-function half of a pipeline, as a memcmp-comparable key. */
typedef struct D3D8StateKey {
    Uint8 blend_enable, src_color, dst_color, src_alpha, dst_alpha, blend_op, color_mask;
    Uint8 depth_test, depth_write, depth_func;
    Uint8 stencil_enable, stencil_read, stencil_write;
    Uint8 stencil_func, stencil_fail, stencil_zfail, stencil_pass;
    Uint8 fill, cull;
} D3D8StateKey;

void            d3d8_states_get_key(D3D8StateKey *key);
SDL_GPUSampler *d3d8_states_sampler(DWORD stage);

/* ================================================================
 * Pipeline cache and draw dispatch (d3d8_pipeline.c)
 * ================================================================ */

#include "gfx/gpu_shader.h"

#define D3D8_MAX_ATTRS 16

/* Vertex input for one vertex buffer (slot 0). Attribute locations are the
 * shader generators' convention; the stride comes from the stream at draw. */
typedef struct D3D8VertexLayout {
    Uint32                 num_attrs;
    SDL_GPUVertexAttribute attrs[D3D8_MAX_ATTRS];
} D3D8VertexLayout;

typedef struct D3D8Draw {
    SDL_GPUPrimitiveType prim;
    SDL_GPUBuffer       *vb;
    Uint32               vb_offset, vb_stride;
    SDL_GPUBuffer       *ib;                /* NULL = not indexed */
    Uint32               ib_offset;
    int                  ib32;              /* 32-bit indices */
    Uint32               count;             /* vertices or indices */
    Uint32               first;             /* first vertex or first index */
    Sint32               base_vertex;
} D3D8Draw;

HRESULT d3d8_pipe_init(void);
void    d3d8_pipe_shutdown(void);
void    d3d8_pipe_begin_frame(void);
void    d3d8_pipe_set_vs(const XGpuShader *vs);
void    d3d8_pipe_set_ps(const XGpuShader *ps);
void    d3d8_pipe_set_layout(const D3D8VertexLayout *layout);
/* Data for uniform buffer `slot` (HLSL register bN); pushed at the next draw. */
void    d3d8_pipe_set_uniforms(int fragment, Uint32 slot, const void *data, Uint32 size);
/* Copies *UP draw data into the streaming ring. */
int     d3d8_pipe_stream(const void *data, Uint32 size, SDL_GPUBuffer **buf, Uint32 *offset);
int     d3d8_pipe_draw(const D3D8Draw *draw);

/* Partial clears (d3d8_clear.c): colour, depth and stencil inside one rectangle. */
int  d3d8_clear_rect(XGpuTargets t, SDL_GPUTextureFormat cf, SDL_GPUTextureFormat df,
                     Uint32 target_w, Uint32 target_h, const SDL_Rect *r,
                     int do_color, const float rgba[4], int do_depth, float z,
                     int do_stencil, Uint8 stencil);
void d3d8_clear_shutdown(void);

/* Supplied by d3d8_device.c: what the next draw renders into, and the viewport. */
void    d3d8_GetCurrentTargets(XGpuTargets *t, SDL_GPUTextureFormat *color_fmt,
                               SDL_GPUTextureFormat *depth_fmt, Uint32 *w, Uint32 *h);
void    d3d8_GetViewport(D3DVIEWPORT8 *vp);

#endif /* BURNOUT3_D3D8_INTERNAL_H */
