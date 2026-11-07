/**
 * D3D8 Compatibility Device Implementation (SDL_GPU)
 *
 * Implements the Xbox D3D8 IDirect3DDevice8 interface on SDL3's GPU API.
 * The game's translated RenderWare code calls D3D8 methods through
 * COM vtables; this layer translates those calls to SDL_GPU.
 *
 * Architecture:
 * - One SDL window and SDL_GPU device (gfx/gpu_device.c). The D3D8 backbuffer
 *   is an offscreen texture blitted to the window at Present.
 * - Render state tracking: D3D8 states are kept here and turned into a
 *   pipeline description at draw time (d3d8_pipeline.c, d3d8_states.c)
 * - Texture/buffer management: D3D8 resource handles wrap SDL_GPU resources
 * - Fixed-function pipeline: emulated via generated shaders (the Xbox D3D8
 *   pipeline is configurable but not fully programmable)
 */

#include "d3d8_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Internal device state
 * ================================================================ */

/* Maximum tracked render states, texture stages, and transforms */
#define MAX_RENDER_STATES    256
#define MAX_TEXTURE_STAGES   4
#define MAX_TSS_STATES       32
#define MAX_TRANSFORMS       512
#define MAX_LIGHTS           8

typedef struct D3D8DeviceState {
    /* SDL_GPU device, window and backbuffer */
    XGpu                    gpu;
    XGpuTex                *backbuffer;     /* borrowed view of gpu.backbuffer */
    XGpuTex                *default_depth;

    UINT                    width;
    UINT                    height;
    D3DFORMAT               backbuffer_format;

    /* State tracking */
    DWORD                   render_states[MAX_RENDER_STATES];
    DWORD                   tss[MAX_TEXTURE_STAGES][MAX_TSS_STATES];
    D3DMATRIX               transforms[MAX_TRANSFORMS];
    D3DVIEWPORT8            viewport;
    D3DMATERIAL8            material;
    D3DLIGHT8               lights[MAX_LIGHTS];
    BOOL                    light_enable[MAX_LIGHTS];

    /* Current shader/FVF */
    DWORD                   vertex_shader;
    DWORD                   pixel_shader;

    /* Scene state */
    BOOL                    in_scene;

    /* Reference count */
    LONG                    ref_count;
} D3D8DeviceState;

/* Global device instance (Xbox has a single D3D device) */
static D3D8DeviceState g_device_state;
static IDirect3DDevice8 g_device;
static BOOL g_device_initialized = FALSE;

/* Current resource bindings */
static IDirect3DVertexBuffer8 *g_cur_vb = NULL;
static UINT                    g_cur_vb_stride = 0;
static IDirect3DIndexBuffer8  *g_cur_ib = NULL;
static UINT                    g_cur_ib_base_vertex = 0;
static IDirect3DBaseTexture8  *g_cur_textures[4] = { NULL };

/* Bound offscreen render target / depth stencil (NULL = default) */
static D3D8Surface *g_cur_rt = NULL;
static D3D8Surface *g_cur_ds = NULL;

/* Palettized texture palettes (256 ARGB entries per stage) */
#define D3D8_PALETTE_ENTRIES 256
static DWORD g_palettes[D3D8_MAX_PALETTES][D3D8_PALETTE_ENTRIES];
static BOOL  g_palettes_initialized = FALSE;

static void d3d8_palettes_init(void)
{
    int p, i;
    if (g_palettes_initialized) return;
    for (p = 0; p < D3D8_MAX_PALETTES; p++)
        for (i = 0; i < D3D8_PALETTE_ENTRIES; i++)
            g_palettes[p][i] = 0xFF000000u | ((DWORD)i * 0x010101u); /* gray ramp */
    g_palettes_initialized = TRUE;
}

void d3d8_SetPalette(DWORD stage, const DWORD *entries)
{
    d3d8_palettes_init();
    if (stage >= D3D8_MAX_PALETTES) return;
    if (entries)
        memcpy(g_palettes[stage], entries, sizeof(g_palettes[stage]));
    else {
        for (int i = 0; i < D3D8_PALETTE_ENTRIES; i++)
            g_palettes[stage][i] = 0xFF000000u | ((DWORD)i * 0x010101u);
    }
}

const DWORD *d3d8_GetPalette(DWORD stage)
{
    d3d8_palettes_init();
    return (stage < D3D8_MAX_PALETTES) ? g_palettes[stage] : g_palettes[0];
}

/* Forward declarations */
static const IDirect3DDevice8Vtbl g_device_vtbl;
static void up_ring_shutdown(void);

/* Window events: the game's internal loop drives rendering, so nothing else
 * pumps the window. Called from every present. The pump (src/video/sdl_events.c)
 * is shared with the framebuffer window so the two never steal each other's
 * events, and it keeps the held-key state for the RECOMP_KEYBOARD pad stand-in. */
void xbox_PumpEvents(void);

static HRESULT d3d8_present(void)
{
    xbox_PumpEvents();
    xgpu_present(&g_device_state.gpu);
    xgpu_begin_frame(&g_device_state.gpu);
    d3d8_pipe_begin_frame();
    return S_OK;
}

/* ================================================================
 * Public frame pump (called from recompiled game code)
 * ================================================================ */
void d3d8_PresentFrame(void)
{
    if (g_device_state.gpu.device)
        d3d8_present();
}

/* ================================================================
 * Internal accessors (used by d3d8_resources/shaders/states)
 * ================================================================ */

IDirect3DDevice8    *d3d8_GetDevice(void) { return &g_device; }
XGpu                *d3d8_GetGpu(void) { return &g_device_state.gpu; }
void                 d3d8_GetViewport(D3DVIEWPORT8 *vp) { *vp = g_device_state.viewport; }
UINT                 d3d8_GetBackbufferWidth(void) { return g_device_state.width; }
UINT                 d3d8_GetBackbufferHeight(void) { return g_device_state.height; }
const DWORD         *d3d8_GetRenderStates(void) { return g_device_state.render_states; }
const DWORD         *d3d8_GetTSS(DWORD stage) { return (stage < MAX_TEXTURE_STAGES) ? g_device_state.tss[stage] : NULL; }
IDirect3DBaseTexture8 *d3d8_GetStageTexture(DWORD stage) { return (stage < 4) ? g_cur_textures[stage] : NULL; }
const D3DMATRIX     *d3d8_GetTransform(D3DTRANSFORMSTATETYPE type) {
    return ((DWORD)type < MAX_TRANSFORMS) ? &g_device_state.transforms[(DWORD)type] : NULL;
}

const D3DLIGHT8     *d3d8_GetLight(DWORD index) {
    return (index < MAX_LIGHTS) ? &g_device_state.lights[index] : NULL;
}

BOOL                 d3d8_GetLightEnable(DWORD index) {
    return (index < MAX_LIGHTS) ? g_device_state.light_enable[index] : FALSE;
}

const D3DMATERIAL8  *d3d8_GetMaterial(void) {
    return &g_device_state.material;
}

DWORD                d3d8_GetCurrentFVF(void) {
    return g_cur_vb ? ((D3D8VertexBuffer *)g_cur_vb)->fvf : 0;
}

UINT                 d3d8_GetNumLights(void) {
    return MAX_LIGHTS;
}

/* What the next draw renders into. An offscreen target without a depth surface
 * of its own gets none (the default depth buffer is window-sized). */
void d3d8_GetCurrentTargets(XGpuTargets *t, SDL_GPUTextureFormat *color_fmt,
                            SDL_GPUTextureFormat *depth_fmt, Uint32 *w, Uint32 *h)
{
    XGpuTex *depth = NULL;

    memset(t, 0, sizeof(*t));
    if (g_cur_rt && g_cur_rt->gpu) {
        t->color = g_cur_rt->gpu->tex;
        t->color_mip = g_cur_rt->mip;
        t->color_layer = g_cur_rt->layer;
        *color_fmt = g_cur_rt->gpu->format;
        *w = g_cur_rt->width;
        *h = g_cur_rt->height;
    } else {
        t->color = NULL;
        *color_fmt = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        *w = g_device_state.width;
        *h = g_device_state.height;
    }
    if (g_cur_ds && g_cur_ds->gpu)
        depth = g_cur_ds->gpu;
    else if (!g_cur_rt)
        depth = g_device_state.default_depth;
    t->depth = depth ? depth->tex : NULL;
    *depth_fmt = depth ? depth->format : SDL_GPU_TEXTUREFORMAT_INVALID;
}

/* ================================================================
 * Device initialization helpers
 * ================================================================ */

static HRESULT gpu_create_device(D3D8DeviceState *state, D3DPRESENT_PARAMETERS *pp)
{
    UINT w = pp->BackBufferWidth ? pp->BackBufferWidth : 640;
    UINT h = pp->BackBufferHeight ? pp->BackBufferHeight : 480;
    const char *hide;

    /* RECOMP_HIDE_WINDOW=1 renders without showing a window (tests, CI). */
    hide = getenv("RECOMP_HIDE_WINDOW");
    if (!xgpu_init(&state->gpu, "xboxrecomp", (int)w, (int)h, hide && *hide && *hide != '0')) {
        fprintf(stderr, "D3D8: Failed to create SDL_GPU device\n");
        return E_FAIL;
    }
    if (!pp->Windowed)
        SDL_SetWindowFullscreen(state->gpu.window, true);

    state->width = w;
    state->height = h;
    state->gpu.present_hook = d3d8_gamma_present;
    return S_OK;
}

static HRESULT gpu_create_render_targets(D3D8DeviceState *state)
{
    XGpuTex *depth;

    state->backbuffer = xgputex_wrap(state->gpu.backbuffer, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                     state->gpu.width, state->gpu.height);
    depth = xgputex_create(&state->gpu, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
                           state->width, state->height, 1, 1,
                           SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    state->default_depth = depth;
    return (state->backbuffer && depth) ? S_OK : E_FAIL;
}

static void d3d8_init_default_states(D3D8DeviceState *state)
{
    /* Set Xbox D3D8 default render states */
    memset(state->render_states, 0, sizeof(state->render_states));
    state->render_states[D3DRS_ZENABLE]           = 1;
    state->render_states[D3DRS_FILLMODE]          = D3DFILL_SOLID;
    state->render_states[D3DRS_SHADEMODE]         = 2; /* D3DSHADE_GOURAUD */
    state->render_states[D3DRS_ZWRITEENABLE]      = TRUE;
    state->render_states[D3DRS_ALPHATESTENABLE]    = FALSE;
    state->render_states[D3DRS_SRCBLEND]          = D3DBLEND_ONE;
    state->render_states[D3DRS_DESTBLEND]         = D3DBLEND_ZERO;
    state->render_states[D3DRS_CULLMODE]          = D3DCULL_CCW;
    state->render_states[D3DRS_ZFUNC]             = D3DCMP_LESSEQUAL;
    state->render_states[D3DRS_ALPHAREF]          = 0;
    state->render_states[D3DRS_ALPHAFUNC]         = D3DCMP_ALWAYS;
    state->render_states[D3DRS_ALPHABLENDENABLE]   = FALSE;
    state->render_states[D3DRS_FOGENABLE]         = FALSE;
    state->render_states[D3DRS_STENCILENABLE]     = FALSE;
    state->render_states[D3DRS_COLORWRITEENABLE]  = 0x0F;

    /* Default viewport */
    state->viewport.X = 0;
    state->viewport.Y = 0;
    state->viewport.Width = state->width;
    state->viewport.Height = state->height;
    state->viewport.MinZ = 0.0f;
    state->viewport.MaxZ = 1.0f;

    /* Default texture stage states:
     * By default each stage reads its own texcoord set (0,1,2,3). */
    memset(state->tss, 0, sizeof(state->tss));
    for (int s = 0; s < MAX_TEXTURE_STAGES; s++)
        state->tss[s][D3DTSS_TEXCOORDINDEX] = (DWORD)s;

    /* Identity matrices */
    for (int i = 0; i < MAX_TRANSFORMS; i++) {
        memset(&state->transforms[i], 0, sizeof(D3DMATRIX));
        state->transforms[i]._11 = 1.0f;
        state->transforms[i]._22 = 1.0f;
        state->transforms[i]._33 = 1.0f;
        state->transforms[i]._44 = 1.0f;
    }

    state->vertex_shader = 0;
    state->pixel_shader = 0;
    state->in_scene = FALSE;
}

/* ================================================================
 * IDirect3DDevice8 method implementations
 * ================================================================ */

static HRESULT __stdcall dev_QueryInterface(IDirect3DDevice8 *self, const IID *riid, void **ppv)
{
    (void)self; (void)riid; (void)ppv;
    return E_NOINTERFACE;
}

static ULONG __stdcall dev_AddRef(IDirect3DDevice8 *self)
{
    (void)self;
    return InterlockedIncrement(&g_device_state.ref_count);
}

static ULONG __stdcall dev_Release(IDirect3DDevice8 *self)
{
    (void)self;
    LONG ref = InterlockedDecrement(&g_device_state.ref_count);
    if (ref <= 0) {
        /* Cleanup subsystems first */
        d3d8_clear_shutdown();
        d3d8_pipe_shutdown();
        d3d8_gamma_shutdown();
        d3d8_vsh_shutdown();
        d3d8_combiners_shutdown();
        d3d8_states_shutdown();
        d3d8_shaders_shutdown();

        /* Cleanup GPU resources */
        D3D8DeviceState *s = &g_device_state;
        if (s->default_depth) { xgputex_unref(&s->gpu, s->default_depth); s->default_depth = NULL; }
        if (s->backbuffer) { xgputex_unref(&s->gpu, s->backbuffer); s->backbuffer = NULL; }
        xgpu_shutdown(&s->gpu);
        g_device_initialized = FALSE;
    }
    return (ULONG)ref;
}

static HRESULT __stdcall dev_GetDirect3D(IDirect3DDevice8 *self, IDirect3D8 **ppD3D8)
{
    (void)self; (void)ppD3D8;
    /* TODO: return the factory */
    return E_NOTIMPL;
}

static HRESULT __stdcall dev_GetDeviceCaps(IDirect3DDevice8 *self, void *pCaps)
{
    (void)self; (void)pCaps;
    /* TODO: fill with Xbox NV2A capabilities */
    return S_OK;
}

static HRESULT __stdcall dev_GetDisplayMode(IDirect3DDevice8 *self, void *pMode)
{
    (void)self; (void)pMode;
    return S_OK;
}

static HRESULT __stdcall dev_GetCreationParameters(IDirect3DDevice8 *self, void *pParams)
{
    (void)self; (void)pParams;
    return S_OK;
}

static HRESULT __stdcall dev_Reset(IDirect3DDevice8 *self, D3DPRESENT_PARAMETERS *pPP)
{
    (void)self; (void)pPP;
    /* TODO: resize swap chain */
    return S_OK;
}

static DWORD g_d3d_begin_count = 0;
static DWORD g_d3d_end_count = 0;
static DWORD g_d3d_clear_count = 0;
static DWORD g_d3d_draw_count = 0;
static DWORD g_d3d_settransform_count = 0;
static DWORD g_d3d_setrs_count = 0;
static DWORD g_d3d_settexture_count = 0;

static HRESULT __stdcall dev_Present(IDirect3DDevice8 *self, const RECT *src, const RECT *dst, HWND hWnd, void *pDirty)
{
    static DWORD frame_count = 0;
    static DWORD last_tick = 0;
    (void)self; (void)src; (void)dst; (void)hWnd; (void)pDirty;

    frame_count++;
    DWORD now = GetTickCount();
    if (last_tick == 0) last_tick = now;
    if (now - last_tick >= 2000) {
        fprintf(stderr, "  [D3D] %.1fs: %u present (%.1f fps), %u begin, %u end, "
                "%u clear, %u draw, %u xform, %u rs, %u tex\n",
                (now - last_tick) / 1000.0, frame_count,
                frame_count * 1000.0 / (now - last_tick),
                g_d3d_begin_count, g_d3d_end_count,
                g_d3d_clear_count, g_d3d_draw_count,
                g_d3d_settransform_count, g_d3d_setrs_count,
                g_d3d_settexture_count);
        fflush(stderr);
        frame_count = 0;
        g_d3d_begin_count = g_d3d_end_count = 0;
        g_d3d_clear_count = g_d3d_draw_count = 0;
        g_d3d_settransform_count = g_d3d_setrs_count = 0;
        g_d3d_settexture_count = 0;
        last_tick = now;
    }

    return d3d8_present();
}

static HRESULT __stdcall dev_GetBackBuffer(IDirect3DDevice8 *self, INT iBackBuffer, DWORD Type, IDirect3DSurface8 **ppSurface)
{
    (void)self; (void)iBackBuffer; (void)Type;
    if (!ppSurface) return E_INVALIDARG;
    if (!g_device_state.backbuffer) return E_FAIL;

    *ppSurface = d3d8_surface_create(g_device_state.backbuffer, 0, 0,
                                     g_device_state.width,
                                     g_device_state.height,
                                     D3DFMT_X8R8G8B8,
                                     D3DPOOL_DEFAULT, D3DUSAGE_RENDERTARGET,
                                     D3DMULTISAMPLE_NONE, NULL, 0);
    return *ppSurface ? S_OK : E_OUTOFMEMORY;
}

static HRESULT __stdcall dev_BeginScene(IDirect3DDevice8 *self)
{
    (void)self;
    g_device_state.in_scene = TRUE;
    g_d3d_begin_count++;
    return S_OK;
}

static HRESULT __stdcall dev_EndScene(IDirect3DDevice8 *self)
{
    (void)self;
    g_device_state.in_scene = FALSE;
    g_d3d_end_count++;
    return S_OK;
}

static HRESULT __stdcall dev_Clear(IDirect3DDevice8 *self, DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
    (void)self;
    g_d3d_clear_count++;

    /* Clear the currently bound targets (default if none set). D3D8 clears the
     * viewport, or the rectangles given inside it. A clear that covers the whole
     * target is a render-pass clear; anything smaller is a scissored draw
     * (d3d8_clear.c), because a pass can only clear a whole attachment. */
    {
        XGpuTargets t;
        XGpuClear cl;
        SDL_GPUTextureFormat cf, df;
        Uint32 tw, th, i;
        SDL_Rect clip, rects[64];
        int nrects = 0;
        int do_color = (Flags & D3DCLEAR_TARGET) != 0;
        int do_depth, do_stencil;
        float rgba[4];

        d3d8_GetCurrentTargets(&t, &cf, &df, &tw, &th);
        do_depth = (Flags & D3DCLEAR_ZBUFFER) && t.depth;
        do_stencil = (Flags & D3DCLEAR_STENCIL) && t.depth;
        if (!do_color && !do_depth && !do_stencil)
            return S_OK;
        rgba[0] = ((Color >> 16) & 0xFF) / 255.0f;   /* R */
        rgba[1] = ((Color >>  8) & 0xFF) / 255.0f;   /* G */
        rgba[2] = ((Color >>  0) & 0xFF) / 255.0f;   /* B */
        rgba[3] = ((Color >> 24) & 0xFF) / 255.0f;   /* A */

        /* The area D3D8 clears: the viewport, clipped to the target. */
        clip.x = (int)g_device_state.viewport.X;
        clip.y = (int)g_device_state.viewport.Y;
        clip.w = (int)(g_device_state.viewport.Width ? g_device_state.viewport.Width : tw);
        clip.h = (int)(g_device_state.viewport.Height ? g_device_state.viewport.Height : th);
        if (clip.x < 0) { clip.w += clip.x; clip.x = 0; }
        if (clip.y < 0) { clip.h += clip.y; clip.y = 0; }
        if (clip.x + clip.w > (int)tw) clip.w = (int)tw - clip.x;
        if (clip.y + clip.h > (int)th) clip.h = (int)th - clip.y;

        if (Count == 0 || !pRects) {
            rects[nrects++] = clip;
        } else {
            for (i = 0; i < Count && nrects < 64; i++) {
                SDL_Rect r;
                int x1 = (int)pRects[i].x1, y1 = (int)pRects[i].y1;
                int x2 = (int)pRects[i].x2, y2 = (int)pRects[i].y2;
                if (x1 < clip.x) x1 = clip.x;
                if (y1 < clip.y) y1 = clip.y;
                if (x2 > clip.x + clip.w) x2 = clip.x + clip.w;
                if (y2 > clip.y + clip.h) y2 = clip.y + clip.h;
                if (x2 <= x1 || y2 <= y1) continue;
                r.x = x1; r.y = y1; r.w = x2 - x1; r.h = y2 - y1;
                rects[nrects++] = r;
            }
        }
        if (nrects == 0 || clip.w <= 0 || clip.h <= 0)
            return S_OK;

        if (nrects == 1 && rects[0].x == 0 && rects[0].y == 0 &&
            rects[0].w == (int)tw && rects[0].h == (int)th) {
            memset(&cl, 0, sizeof(cl));
            if (do_color) { cl.color = 1; memcpy(cl.rgba, rgba, sizeof(rgba)); }
            if (do_depth) { cl.depth = 1; cl.z = Z; }
            if (do_stencil) { cl.stencil = 1; cl.stencil_value = (Uint8)Stencil; }
            xgpu_render_pass(&g_device_state.gpu, t, &cl);
        } else {
            for (i = 0; i < (Uint32)nrects; i++)
                d3d8_clear_rect(t, cf, df, tw, th, &rects[i], do_color, rgba,
                                do_depth != 0, Z, do_stencil != 0, (Uint8)Stencil);
        }
    }

    return S_OK;
}

static HRESULT __stdcall dev_SetTransform(IDirect3DDevice8 *self, D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix)
{
    (void)self;
    g_d3d_settransform_count++;
    if ((DWORD)State < MAX_TRANSFORMS && pMatrix) {
        g_device_state.transforms[(DWORD)State] = *pMatrix;
    }
    return S_OK;
}

static HRESULT __stdcall dev_GetTransform(IDirect3DDevice8 *self, D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
    (void)self;
    if ((DWORD)State < MAX_TRANSFORMS && pMatrix) {
        *pMatrix = g_device_state.transforms[(DWORD)State];
    }
    return S_OK;
}

static HRESULT __stdcall dev_SetRenderState(IDirect3DDevice8 *self, D3DRENDERSTATETYPE State, DWORD Value)
{
    (void)self;
    g_d3d_setrs_count++;
    if ((DWORD)State < MAX_RENDER_STATES) {
        g_device_state.render_states[(DWORD)State] = Value;
    }
    /* Mark combiner state dirty if any PS register combiner state changed */
    if ((DWORD)State >= D3DRS_PSALPHAINPUTS0 && (DWORD)State <= D3DRS_PSINPUTTEXTURE) {
        d3d8_combiners_mark_dirty();
    }
    return S_OK;
}

static HRESULT __stdcall dev_GetRenderState(IDirect3DDevice8 *self, D3DRENDERSTATETYPE State, DWORD *pValue)
{
    (void)self;
    if ((DWORD)State < MAX_RENDER_STATES && pValue) {
        *pValue = g_device_state.render_states[(DWORD)State];
    }
    return S_OK;
}

static HRESULT __stdcall dev_SetTextureStageState(IDirect3DDevice8 *self, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
    (void)self;
    if (Stage < MAX_TEXTURE_STAGES && (DWORD)Type < MAX_TSS_STATES) {
        g_device_state.tss[Stage][(DWORD)Type] = Value;
    }
    return S_OK;
}

static HRESULT __stdcall dev_GetTextureStageState(IDirect3DDevice8 *self, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue)
{
    (void)self;
    if (Stage < MAX_TEXTURE_STAGES && (DWORD)Type < MAX_TSS_STATES && pValue) {
        *pValue = g_device_state.tss[Stage][(DWORD)Type];
    }
    return S_OK;
}

static HRESULT __stdcall dev_SetTexture(IDirect3DDevice8 *self, DWORD Stage, IDirect3DBaseTexture8 *pTexture)
{
    (void)self;
    g_d3d_settexture_count++;
    if (Stage >= 4) return E_INVALIDARG;
    g_cur_textures[Stage] = pTexture;

    /* The texture is bound at draw time (d3d8_pipe_draw). */
    if (pTexture) {
        /* P8 textures bake a palette; record which stage palette they use
         * so SetPalette can re-bake them later. */
        if (d3d8_format_is_palettized(d3d8_base_format(pTexture)))
            d3d8_base_set_palette(pTexture, Stage);
        /* Mark texture stage as active */
        if (g_device_state.tss[Stage][D3DTSS_COLOROP] == D3DTOP_DISABLE)
            g_device_state.tss[Stage][D3DTSS_COLOROP] = D3DTOP_MODULATE;
    } else {
        g_device_state.tss[Stage][D3DTSS_COLOROP] = D3DTOP_DISABLE;
    }
    return S_OK;
}

static HRESULT __stdcall dev_GetTexture(IDirect3DDevice8 *self, DWORD Stage, IDirect3DBaseTexture8 **ppTexture)
{
    (void)self;
    if (!ppTexture || Stage >= MAX_TEXTURE_STAGES) return E_INVALIDARG;
    *ppTexture = g_cur_textures[Stage];
    if (*ppTexture) IDirect3DBaseTexture8_AddRef(*ppTexture);
    return S_OK;
}

static HRESULT __stdcall dev_SetStreamSource(IDirect3DDevice8 *self, UINT StreamNumber, IDirect3DVertexBuffer8 *pStreamData, UINT Stride)
{
    (void)self;
    if (StreamNumber != 0) return S_OK; /* Only stream 0 supported */
    g_cur_vb = pStreamData;
    g_cur_vb_stride = Stride;
    return S_OK;
}

static HRESULT __stdcall dev_GetStreamSource(IDirect3DDevice8 *self, UINT StreamNumber, IDirect3DVertexBuffer8 **ppStreamData, UINT *pStride)
{
    (void)self;
    if (!ppStreamData || StreamNumber != 0) return E_INVALIDARG;
    *ppStreamData = g_cur_vb;
    if (*ppStreamData) (*ppStreamData)->lpVtbl->AddRef(*ppStreamData);
    if (pStride) *pStride = g_cur_vb_stride;
    return S_OK;
}

static HRESULT __stdcall dev_SetIndices(IDirect3DDevice8 *self, IDirect3DIndexBuffer8 *pIndexData, UINT BaseVertexIndex)
{
    (void)self;
    g_cur_ib = pIndexData;
    g_cur_ib_base_vertex = BaseVertexIndex;
    return S_OK;
}

static HRESULT __stdcall dev_GetIndices(IDirect3DDevice8 *self, IDirect3DIndexBuffer8 **ppIndexData, UINT *pBaseVertexIndex)
{
    (void)self;
    if (!ppIndexData) return E_INVALIDARG;
    *ppIndexData = g_cur_ib;
    if (*ppIndexData) (*ppIndexData)->lpVtbl->AddRef(*ppIndexData);
    if (pBaseVertexIndex) *pBaseVertexIndex = g_cur_ib_base_vertex;
    return S_OK;
}

static int map_primitive_type(D3DPRIMITIVETYPE pt, UINT count, UINT *out_count,
                              SDL_GPUPrimitiveType *prim)
{
    switch (pt) {
    case D3DPT_TRIANGLELIST:  *out_count = count * 3; *prim = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; return 1;
    case D3DPT_TRIANGLESTRIP: *out_count = count + 2; *prim = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP; return 1;
    case D3DPT_TRIANGLEFAN:   *out_count = count * 3; *prim = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; return 1;
    case D3DPT_LINELIST:      *out_count = count * 2; *prim = SDL_GPU_PRIMITIVETYPE_LINELIST; return 1;
    case D3DPT_LINESTRIP:     *out_count = count + 1; *prim = SDL_GPU_PRIMITIVETYPE_LINESTRIP; return 1;
    case D3DPT_POINTLIST:     *out_count = count;     *prim = SDL_GPU_PRIMITIVETYPE_POINTLIST; return 1;
    case D3DPT_QUADLIST:      *out_count = count * 6; *prim = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; return 1;
    default:                  *out_count = 0; return 0;
    }
}

/* ================================================================
 * Triangle fan / quad list → triangle list conversion
 *
 * SDL_GPU doesn't support triangle fans or quad lists.
 * Convert vertex data in-place to triangle list.
 * Returns malloc'd buffer (caller must free) or NULL if no conversion needed.
 * ================================================================ */

static void *convert_fan_or_quad(D3DPRIMITIVETYPE pt, const void *src,
                                  UINT prim_count, UINT stride,
                                  UINT *out_vertex_count)
{
    BYTE *dst;
    const BYTE *s = (const BYTE *)src;
    UINT i;

    if (pt == D3DPT_TRIANGLEFAN) {
        /* Fan: vertex 0 is the hub, each triangle is (0, i+1, i+2) */
        UINT tri_verts = prim_count * 3;
        dst = (BYTE *)malloc(tri_verts * stride);
        if (!dst) return NULL;

        for (i = 0; i < prim_count; i++) {
            memcpy(dst + (i * 3 + 0) * stride, s, stride);                      /* v0 (hub) */
            memcpy(dst + (i * 3 + 1) * stride, s + (i + 1) * stride, stride);   /* v[i+1] */
            memcpy(dst + (i * 3 + 2) * stride, s + (i + 2) * stride, stride);   /* v[i+2] */
        }
        *out_vertex_count = tri_verts;
        return dst;
    }

    if (pt == D3DPT_QUADLIST) {
        /* Quad list: each quad (v0,v1,v2,v3) → 2 triangles (v0,v1,v2), (v0,v2,v3) */
        UINT tri_verts = prim_count * 6;
        dst = (BYTE *)malloc(tri_verts * stride);
        if (!dst) return NULL;

        for (i = 0; i < prim_count; i++) {
            const BYTE *q = s + i * 4 * stride;
            memcpy(dst + (i * 6 + 0) * stride, q + 0 * stride, stride);  /* v0 */
            memcpy(dst + (i * 6 + 1) * stride, q + 1 * stride, stride);  /* v1 */
            memcpy(dst + (i * 6 + 2) * stride, q + 2 * stride, stride);  /* v2 */
            memcpy(dst + (i * 6 + 3) * stride, q + 0 * stride, stride);  /* v0 */
            memcpy(dst + (i * 6 + 4) * stride, q + 2 * stride, stride);  /* v2 */
            memcpy(dst + (i * 6 + 5) * stride, q + 3 * stride, stride);  /* v3 */
        }
        *out_vertex_count = tri_verts;
        return dst;
    }

    return NULL; /* no conversion needed */
}

/* ================================================================
 * Draw entry points
 *
 * Each draw asks the shader modules to describe what is bound (shaders, vertex
 * layout, uniforms), then hands the buffers and range to d3d8_pipe_draw.
 * ================================================================ */

static void prepare_draw_state(void)
{
    d3d8_shaders_prepare_draw(g_device_state.vertex_shader);
    d3d8_combiners_prepare_draw();   /* overrides the pixel shader if a combiner program is active */
}

static HRESULT __stdcall dev_DrawPrimitive(IDirect3DDevice8 *self, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
    D3D8Draw d;
    UINT vertex_count;
    D3D8VertexBuffer *vb = (D3D8VertexBuffer *)g_cur_vb;
    (void)self;
    g_d3d_draw_count++;

    if (!vb || !map_primitive_type(PrimitiveType, PrimitiveCount, &vertex_count, &d.prim) || !vertex_count)
        return E_INVALIDARG;

    d3d8_vb_sync(g_cur_vb);
    prepare_draw_state();

    d.vb = vb->gpu_buffer; d.vb_offset = 0; d.vb_stride = g_cur_vb_stride;
    d.ib = NULL; d.ib_offset = 0; d.ib32 = 0;
    d.count = vertex_count; d.first = StartVertex; d.base_vertex = 0;
    d3d8_pipe_draw(&d);
    return S_OK;
}

static HRESULT __stdcall dev_DrawIndexedPrimitive(IDirect3DDevice8 *self, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount)
{
    D3D8Draw d;
    UINT index_count;
    D3D8VertexBuffer *vb = (D3D8VertexBuffer *)g_cur_vb;
    D3D8IndexBuffer *ib = (D3D8IndexBuffer *)g_cur_ib;
    (void)self; (void)MinVertexIndex; (void)NumVertices;
    g_d3d_draw_count++;

    if (!vb || !ib || !map_primitive_type(PrimitiveType, PrimitiveCount, &index_count, &d.prim) || !index_count)
        return E_INVALIDARG;

    d3d8_vb_sync(g_cur_vb);
    d3d8_ib_sync(g_cur_ib);
    prepare_draw_state();

    d.vb = vb->gpu_buffer; d.vb_offset = 0; d.vb_stride = g_cur_vb_stride;
    d.ib = ib->gpu_buffer; d.ib_offset = 0; d.ib32 = (ib->format == D3DFMT_INDEX32);
    d.count = index_count; d.first = StartIndex; d.base_vertex = (Sint32)g_cur_ib_base_vertex;
    d3d8_pipe_draw(&d);
    return S_OK;
}

static HRESULT __stdcall dev_DrawPrimitiveUP(IDirect3DDevice8 *self, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexData, UINT VertexStreamZeroStride)
{
    D3D8Draw d;
    UINT vertex_count;
    const void *draw_data = pVertexData;
    void *converted = NULL;
    int ok;
    (void)self;
    g_d3d_draw_count++;

    if (!pVertexData || !VertexStreamZeroStride) return E_INVALIDARG;
    if (!map_primitive_type(PrimitiveType, PrimitiveCount, &vertex_count, &d.prim) || !vertex_count)
        return E_INVALIDARG;

    /* Convert triangle fans and quad lists to triangle lists */
    if (PrimitiveType == D3DPT_TRIANGLEFAN || PrimitiveType == D3DPT_QUADLIST) {
        converted = convert_fan_or_quad(PrimitiveType, pVertexData,
                                         PrimitiveCount, VertexStreamZeroStride,
                                         &vertex_count);
        if (converted) draw_data = converted;
    }

    ok = d3d8_pipe_stream(draw_data, vertex_count * VertexStreamZeroStride, &d.vb, &d.vb_offset);
    if (converted) free(converted);
    if (!ok) return E_OUTOFMEMORY;

    prepare_draw_state();
    d.vb_stride = VertexStreamZeroStride;
    d.ib = NULL; d.ib_offset = 0; d.ib32 = 0;
    d.count = vertex_count; d.first = 0; d.base_vertex = 0;
    d3d8_pipe_draw(&d);
    return S_OK;
}

static HRESULT __stdcall dev_DrawIndexedPrimitiveUP(IDirect3DDevice8 *self, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexData, UINT VertexStreamZeroStride)
{
    D3D8Draw d;
    UINT index_count, idx_bytes;
    (void)self; (void)MinVertexIndex;
    g_d3d_draw_count++;

    if (!pVertexData || !pIndexData || !VertexStreamZeroStride) return E_INVALIDARG;
    if (!map_primitive_type(PrimitiveType, PrimitiveCount, &index_count, &d.prim) || !index_count)
        return E_INVALIDARG;

    idx_bytes = (IndexDataFormat == D3DFMT_INDEX32) ? 4 : 2;
    if (!d3d8_pipe_stream(pVertexData, NumVertices * VertexStreamZeroStride, &d.vb, &d.vb_offset))
        return E_OUTOFMEMORY;
    if (!d3d8_pipe_stream(pIndexData, index_count * idx_bytes, &d.ib, &d.ib_offset))
        return E_OUTOFMEMORY;

    prepare_draw_state();
    d.vb_stride = VertexStreamZeroStride;
    d.ib32 = (IndexDataFormat == D3DFMT_INDEX32);
    d.count = index_count; d.first = 0; d.base_vertex = 0;
    d3d8_pipe_draw(&d);
    return S_OK;
}

static HRESULT __stdcall dev_CreateTexture(IDirect3DDevice8 *self, UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8 **ppTexture)
{
    (void)self; (void)Pool;
    return d3d8_CreateTextureImpl(Width, Height, Levels, Usage, Format, ppTexture);
}

static HRESULT __stdcall dev_CreateImageSurface(IDirect3DDevice8 *self, UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8 **ppSurface)
{
    (void)self;
    return d3d8_CreateImageSurfaceImpl(Width, Height, Format, ppSurface);
}

static HRESULT __stdcall dev_CreateCubeTexture(IDirect3DDevice8 *self, UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture8 **ppCubeTexture)
{
    (void)self; (void)Pool;
    return d3d8_CreateCubeTextureImpl(EdgeLength, Levels, Usage, Format, ppCubeTexture);
}

static HRESULT __stdcall dev_CreateVolumeTexture(IDirect3DDevice8 *self, UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture8 **ppVolumeTexture)
{
    (void)self; (void)Pool;
    return d3d8_CreateVolumeTextureImpl(Width, Height, Depth, Levels, Usage, Format, ppVolumeTexture);
}

static HRESULT __stdcall dev_CreateVertexBuffer(IDirect3DDevice8 *self, UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8 **ppVertexBuffer)
{
    (void)self; (void)Pool;
    return d3d8_CreateVertexBufferImpl(Length, Usage, FVF, ppVertexBuffer);
}

static HRESULT __stdcall dev_CreateIndexBuffer(IDirect3DDevice8 *self, UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8 **ppIndexBuffer)
{
    (void)self; (void)Pool;
    return d3d8_CreateIndexBufferImpl(Length, Usage, Format, ppIndexBuffer);
}

static HRESULT __stdcall dev_CreateRenderTarget(IDirect3DDevice8 *self, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, BOOL Lockable, IDirect3DSurface8 **ppSurface)
{
    SDL_GPUTextureFormat gf;
    XGpuTex *tex;
    (void)self; (void)Lockable;

    if (!ppSurface || !Width || !Height) return E_INVALIDARG;

    if (d3d8_msaa_sample_count(MultiSample) > 1)
        fprintf(stderr, "D3D8: MSAA render target requested; rendering single-sampled\n");

    gf = d3d8_to_gpu_format(Format);
    if (gf == SDL_GPU_TEXTUREFORMAT_INVALID) return E_INVALIDARG;
    tex = xgputex_create(d3d8_GetGpu(), SDL_GPU_TEXTURETYPE_2D, gf, Width, Height, 1, 1,
                         SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (!tex) {
        fprintf(stderr, "D3D8: CreateRenderTarget failed (fmt=0x%X %ux%u)\n", Format, Width, Height);
        return E_OUTOFMEMORY;
    }

    *ppSurface = d3d8_surface_create(tex, 0, 0, Width, Height, Format,
                                     D3DPOOL_DEFAULT, D3DUSAGE_RENDERTARGET,
                                     MultiSample, NULL, 0);
    xgputex_unref(d3d8_GetGpu(), tex);
    return *ppSurface ? S_OK : E_OUTOFMEMORY;
}

static HRESULT __stdcall dev_CreateDepthStencilSurface(IDirect3DDevice8 *self, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, IDirect3DSurface8 **ppSurface)
{
    SDL_GPUTextureFormat gf;
    D3DFORMAT surface_format = Format;
    XGpuTex *tex;
    (void)self;

    if (!ppSurface || !Width || !Height) return E_INVALIDARG;

    gf = d3d8_to_gpu_format(Format);
    if (gf == SDL_GPU_TEXTUREFORMAT_R16_FLOAT) {
        /* F16 has no depth equivalent; fall back to D16. */
        fprintf(stderr, "D3D8: F16 depth surface falling back to D16\n");
        gf = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        surface_format = D3DFMT_D16;
    }
    if (d3d8_msaa_sample_count(MultiSample) > 1)
        fprintf(stderr, "D3D8: MSAA depth surface requested; rendering single-sampled\n");

    tex = xgputex_create(d3d8_GetGpu(), SDL_GPU_TEXTURETYPE_2D, gf, Width, Height, 1, 1,
                         SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET);
    if (!tex) {
        fprintf(stderr, "D3D8: CreateDepthStencilSurface failed (fmt=0x%X)\n", Format);
        return E_OUTOFMEMORY;
    }

    *ppSurface = d3d8_surface_create(tex, 0, 0, Width, Height, surface_format,
                                     D3DPOOL_DEFAULT, D3DUSAGE_DEPTHSTENCIL,
                                     MultiSample, NULL, 0);
    xgputex_unref(d3d8_GetGpu(), tex);
    return *ppSurface ? S_OK : E_OUTOFMEMORY;
}

static HRESULT __stdcall dev_SetRenderTarget(IDirect3DDevice8 *self, IDirect3DSurface8 *pRenderTarget, IDirect3DSurface8 *pZStencilSurface)
{
    (void)self;

    if (g_cur_rt) { IDirect3DSurface8_Release(&g_cur_rt->iface); g_cur_rt = NULL; }
    if (g_cur_ds) { IDirect3DSurface8_Release(&g_cur_ds->iface); g_cur_ds = NULL; }

    if (pRenderTarget) {
        D3D8Surface *rt = (D3D8Surface *)pRenderTarget;
        if (!rt->gpu || rt->is_depth) {
            fprintf(stderr, "D3D8: SetRenderTarget on non-renderable surface\n");
            return E_INVALIDARG;
        }
        /* The backbuffer surface means "the default target". */
        if (rt->gpu != g_device_state.backbuffer) {
            g_cur_rt = rt;
            IDirect3DSurface8_AddRef(pRenderTarget);
        }
    }

    if (pZStencilSurface) {
        D3D8Surface *ds = (D3D8Surface *)pZStencilSurface;
        if (!ds->gpu || !ds->is_depth) {
            fprintf(stderr, "D3D8: SetRenderTarget with non-depth stencil surface\n");
        } else {
            g_cur_ds = ds;
            IDirect3DSurface8_AddRef(pZStencilSurface);
        }
    }
    return S_OK;
}

static HRESULT __stdcall dev_GetRenderTarget(IDirect3DDevice8 *self, IDirect3DSurface8 **ppRenderTarget)
{
    (void)self;
    if (!ppRenderTarget) return E_INVALIDARG;
    if (g_cur_rt) {
        *ppRenderTarget = &g_cur_rt->iface;
        IDirect3DSurface8_AddRef(*ppRenderTarget);
    } else {
        *ppRenderTarget = NULL;
    }
    return S_OK;
}

static HRESULT __stdcall dev_GetDepthStencilSurface(IDirect3DDevice8 *self, IDirect3DSurface8 **ppZStencilSurface)
{
    (void)self;
    if (!ppZStencilSurface) return E_INVALIDARG;
    if (g_cur_ds) {
        *ppZStencilSurface = &g_cur_ds->iface;
        IDirect3DSurface8_AddRef(*ppZStencilSurface);
    } else {
        *ppZStencilSurface = NULL;
    }
    return S_OK;
}

static HRESULT __stdcall dev_SetViewport(IDirect3DDevice8 *self, const D3DVIEWPORT8 *pViewport)
{
    (void)self;
    if (pViewport) {
        g_device_state.viewport = *pViewport;   /* applied at draw time */
    }
    return S_OK;
}

static HRESULT __stdcall dev_GetViewport(IDirect3DDevice8 *self, D3DVIEWPORT8 *pViewport)
{
    (void)self;
    if (pViewport) *pViewport = g_device_state.viewport;
    return S_OK;
}

static HRESULT __stdcall dev_SetMaterial(IDirect3DDevice8 *self, const D3DMATERIAL8 *pMaterial)
{
    (void)self;
    if (pMaterial) g_device_state.material = *pMaterial;
    return S_OK;
}

static HRESULT __stdcall dev_GetMaterial(IDirect3DDevice8 *self, D3DMATERIAL8 *pMaterial)
{
    (void)self;
    if (pMaterial) *pMaterial = g_device_state.material;
    return S_OK;
}

static HRESULT __stdcall dev_SetLight(IDirect3DDevice8 *self, DWORD Index, const D3DLIGHT8 *pLight)
{
    (void)self;
    if (Index < MAX_LIGHTS && pLight) g_device_state.lights[Index] = *pLight;
    return S_OK;
}

static HRESULT __stdcall dev_GetLight(IDirect3DDevice8 *self, DWORD Index, D3DLIGHT8 *pLight)
{
    (void)self;
    if (Index < MAX_LIGHTS && pLight) *pLight = g_device_state.lights[Index];
    return S_OK;
}

static HRESULT __stdcall dev_LightEnable(IDirect3DDevice8 *self, DWORD Index, BOOL Enable)
{
    (void)self;
    if (Index < MAX_LIGHTS) g_device_state.light_enable[Index] = Enable;
    return S_OK;
}

static HRESULT __stdcall dev_CreateVertexShader(IDirect3DDevice8 *self, const DWORD *pDeclaration, const DWORD *pFunction, DWORD *pHandle, DWORD Usage)
{
    (void)self; (void)pDeclaration; (void)Usage;
    if (!pHandle) return E_INVALIDARG;
    if (!pFunction) return E_INVALIDARG;
    /* Count instructions: each is 4 DWORDs, last has bit 0 of word[3] set (END flag) */
    {
        int i, num_insns = 0;
        for (i = 0; i < 136; i++) {
            num_insns++;
            if (pFunction[i * 4 + 3] & 1) break;  /* END bit in last word */
        }
        return d3d8_vsh_create_shader(pFunction, num_insns, pHandle);
    }
}

static HRESULT __stdcall dev_SetVertexShader(IDirect3DDevice8 *self, DWORD Handle)
{
    (void)self;
    g_device_state.vertex_shader = Handle;
    return S_OK;
}

static HRESULT __stdcall dev_GetVertexShader(IDirect3DDevice8 *self, DWORD *pHandle)
{
    (void)self;
    if (pHandle) *pHandle = g_device_state.vertex_shader;
    return S_OK;
}

static HRESULT __stdcall dev_SetVertexShaderConstant(IDirect3DDevice8 *self, INT Register, const void *pConstantData, DWORD ConstantCount)
{
    (void)self;
    d3d8_vsh_set_constant(Register, pConstantData, ConstantCount);
    return S_OK;
}

static HRESULT __stdcall dev_SetPixelShader(IDirect3DDevice8 *self, DWORD Handle)
{
    (void)self;
    g_device_state.pixel_shader = Handle;
    d3d8_combiners_set_pixel_shader(Handle);
    return S_OK;
}

static HRESULT __stdcall dev_GetPixelShader(IDirect3DDevice8 *self, DWORD *pHandle)
{
    (void)self;
    if (pHandle) *pHandle = g_device_state.pixel_shader;
    return S_OK;
}

static HRESULT __stdcall dev_SetPixelShaderConstant(IDirect3DDevice8 *self, INT Register, const void *pConstantData, DWORD ConstantCount)
{
    (void)self; (void)Register; (void)pConstantData; (void)ConstantCount;
    return S_OK;
}

static void __stdcall dev_SetGammaRamp(IDirect3DDevice8 *self, DWORD Flags, const D3DGAMMARAMP *pRamp)
{
    (void)self; (void)Flags; /* Applied at the next presentation on both flag paths. */
    d3d8_gamma_set(pRamp);
}

static void __stdcall dev_GetGammaRamp(IDirect3DDevice8 *self, D3DGAMMARAMP *pRamp)
{
    (void)self;
    d3d8_gamma_get(pRamp);
}

static HRESULT __stdcall dev_SetPalette(IDirect3DDevice8 *self, DWORD PaletteNumber, const void *pEntries)
{
    (void)self;
    if (PaletteNumber >= D3D8_MAX_PALETTES) return E_INVALIDARG;
    d3d8_SetPalette(PaletteNumber, (const DWORD *)pEntries);

    /* A P8 texture bound to this stage bakes the palette at upload; re-bake
     * the raw indices so animated / colorized palettes take effect. */
    if (PaletteNumber < 4) {
        IDirect3DBaseTexture8 *tex = d3d8_GetStageTexture(PaletteNumber);
        if (tex) d3d8_refresh_palette(tex);
    }
    return S_OK;
}

static HRESULT __stdcall dev_BeginPush(IDirect3DDevice8 *self, DWORD Count, DWORD **ppPush)
{
    (void)self; (void)Count; (void)ppPush;
    /* TODO: Xbox push buffer emulation */
    return E_NOTIMPL;
}

static HRESULT __stdcall dev_EndPush(IDirect3DDevice8 *self, DWORD *pPush)
{
    (void)self; (void)pPush;
    return E_NOTIMPL;
}

static HRESULT __stdcall dev_Swap(IDirect3DDevice8 *self, DWORD Flags)
{
    (void)self; (void)Flags;

    return d3d8_present();
}

/* ================================================================
 * Vtable
 * ================================================================ */

static const IDirect3DDevice8Vtbl g_device_vtbl = {
    dev_QueryInterface,
    dev_AddRef,
    dev_Release,
    dev_GetDirect3D,
    dev_GetDeviceCaps,
    dev_GetDisplayMode,
    dev_GetCreationParameters,
    dev_Reset,
    dev_Present,
    dev_GetBackBuffer,
    dev_BeginScene,
    dev_EndScene,
    dev_Clear,
    dev_SetTransform,
    dev_GetTransform,
    dev_SetRenderState,
    dev_GetRenderState,
    dev_SetTextureStageState,
    dev_GetTextureStageState,
    dev_SetTexture,
    dev_GetTexture,
    dev_SetStreamSource,
    dev_GetStreamSource,
    dev_SetIndices,
    dev_GetIndices,
    dev_DrawPrimitive,
    dev_DrawIndexedPrimitive,
    dev_DrawPrimitiveUP,
    dev_DrawIndexedPrimitiveUP,
    dev_CreateTexture,
    dev_CreateVertexBuffer,
    dev_CreateIndexBuffer,
    dev_CreateRenderTarget,
    dev_CreateDepthStencilSurface,
    dev_SetRenderTarget,
    dev_GetRenderTarget,
    dev_GetDepthStencilSurface,
    dev_SetViewport,
    dev_GetViewport,
    dev_SetMaterial,
    dev_GetMaterial,
    dev_SetLight,
    dev_GetLight,
    dev_LightEnable,
    dev_SetVertexShader,
    dev_GetVertexShader,
    dev_SetVertexShaderConstant,
    dev_SetPixelShader,
    dev_GetPixelShader,
    dev_SetPixelShaderConstant,
    dev_SetGammaRamp,
    dev_GetGammaRamp,
    dev_SetPalette,
    dev_BeginPush,
    dev_EndPush,
    dev_Swap,
    /* Extensions appended by xboxrecomp (slots after the historical ones) */
    dev_CreateImageSurface,
    dev_CreateCubeTexture,
    dev_CreateVolumeTexture,
};

/* ================================================================
 * Public API
 * ================================================================ */

IDirect3DDevice8 *xbox_GetD3DDevice(void)
{
    return g_device_initialized ? &g_device : NULL;
}

/* ================================================================
 * IDirect3D8 factory implementation
 * ================================================================ */

static IDirect3D8 g_d3d8;
static LONG g_d3d8_ref = 0;

static HRESULT __stdcall d3d8_QueryInterface(IDirect3D8 *self, const IID *riid, void **ppv)
{
    (void)self; (void)riid; (void)ppv;
    return E_NOINTERFACE;
}

static ULONG __stdcall d3d8_AddRef(IDirect3D8 *self)
{
    (void)self;
    return (ULONG)InterlockedIncrement(&g_d3d8_ref);
}

static ULONG __stdcall d3d8_Release(IDirect3D8 *self)
{
    (void)self;
    return (ULONG)InterlockedDecrement(&g_d3d8_ref);
}

static HRESULT __stdcall d3d8_CreateDevice(IDirect3D8 *self, UINT Adapter, DWORD DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPP, IDirect3DDevice8 **ppDevice)
{
    (void)self; (void)Adapter; (void)DeviceType; (void)BehaviorFlags;
    HRESULT hr;

    if (!pPP || !ppDevice) return E_INVALIDARG;

    memset(&g_device_state, 0, sizeof(g_device_state));
    g_device_state.ref_count = 1;

    (void)hFocusWindow;    /* the window is SDL's, not the title's */

    hr = gpu_create_device(&g_device_state, pPP);
    if (FAILED(hr)) return hr;

    hr = gpu_create_render_targets(&g_device_state);
    if (FAILED(hr)) return hr;

    d3d8_init_default_states(&g_device_state);

    hr = d3d8_pipe_init();
    if (FAILED(hr)) {
        fprintf(stderr, "D3D8: Pipeline init failed: 0x%08lX\n", hr);
        return hr;
    }
    xgpu_begin_frame(&g_device_state.gpu);

    /* Initialize shader and state subsystems */
    hr = d3d8_shaders_init();
    if (FAILED(hr)) {
        fprintf(stderr, "D3D8: Shader init failed: 0x%08lX\n", hr);
        return hr;
    }

    hr = d3d8_states_init();
    if (FAILED(hr)) {
        fprintf(stderr, "D3D8: State init failed: 0x%08lX\n", hr);
        return hr;
    }

    hr = d3d8_combiners_init();
    if (FAILED(hr)) {
        fprintf(stderr, "D3D8: Combiner init failed: 0x%08lX\n", hr);
        /* Non-fatal: fall back to fixed-function pixel shaders */
    }

    hr = d3d8_vsh_init();
    if (FAILED(hr)) {
        fprintf(stderr, "D3D8: VSH init failed: 0x%08lX\n", hr);
        /* Non-fatal: fall back to FVF vertex shaders */
    }

    g_device.lpVtbl = &g_device_vtbl;
    g_device_initialized = TRUE;

    *ppDevice = &g_device;
    fprintf(stderr, "D3D8: Device created (%ux%u)\n", g_device_state.width, g_device_state.height);
    return S_OK;
}

static const IDirect3D8Vtbl g_d3d8_vtbl = {
    d3d8_QueryInterface,
    d3d8_AddRef,
    d3d8_Release,
    d3d8_CreateDevice,
};

IDirect3D8 *xbox_Direct3DCreate8(UINT SDKVersion)
{
    (void)SDKVersion;
    g_d3d8.lpVtbl = &g_d3d8_vtbl;
    g_d3d8_ref = 1;
    return &g_d3d8;
}
