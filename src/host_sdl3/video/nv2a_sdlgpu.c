/**
 * SDL3 GPU renderer for the NV2A pushbuffer executor: the same interface and
 * behaviour as the D3D11 renderer (nv2a_d3d11.h, nv2a_d3d11.c), on Vulkan,
 * Direct3D 12 or Metal through SDL3. Built with XBOXRECOMP_RENDERER=SDL3_GPU.
 *
 * The pieces are the D3D11 renderer's -- surfaces keyed by physical address,
 * a content-hashed texture cache, one uber shader for the register combiners,
 * CPU-write synchronisation, write-back at flips, presentation -- and so are
 * their comments there. What differs is how work reaches the GPU:
 *
 *   SDL3 GPU records copies and draws in separate passes, and a buffer can be
 *   written only in a copy pass. A draw's vertex and index data is therefore
 *   appended to mapped transfer buffers as the draw arrives, and the draw is
 *   recorded into a list for the current target (a "batch"). Changing target,
 *   copying or uploading anything else, reading back or presenting ends the
 *   batch: one copy pass uploads its vertex and index data, then its render
 *   pass replays the draws. Every other upload or copy ends the batch first,
 *   so GPU work happens in exactly the order the executor asked for it.
 *
 * Differences from the D3D11 renderer, all for SDL3 GPU's sake: targets are
 * single-sample (SDL cannot sample multisampled textures, which the depth
 * reads need) -- render scale supersampling remains; border addressing is
 * done in the shader (PSB); there are no occlusion queries, so the
 * NV2A's z-pass pixel count is counted by a pass of its own (zpass_*); pixel
 * shaders are not specialised per combiner state.
 *
 * Everything here runs on the one thread that executes the pushbuffer, which
 * also opens the window and handles its events (fb_window_sdl.c).
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "platform/frame_limit.h"
#include <string.h>

#include "nv2a_d3d11.h"
#include "nv2a_pacing.h"
#include "fb_present.h"
#include "fb_window_sdl.h"
#include "platform/recomp_profile.h"
#include "../d3d/d3d8_swizzle.h"
#include "../kernel/kernel.h"
#include "nv2a_render_common.h"
#include "nv2a_sampler_cache.h"
#include "nv2a_texture_hash.h"

#include "sdlgpu/ub_vs.spirv.h"
#include "sdlgpu/ps_main.spirv.h"
#include "sdlgpu/ps_wdepth.spirv.h"
#include "sdlgpu/ps_main_flat.spirv.h"
#include "sdlgpu/ps_wdepth_flat.spirv.h"
#include "sdlgpu/present_vs.spirv.h"
#include "sdlgpu/present_ps.spirv.h"
#include "sdlgpu/clear_vs.spirv.h"
#include "sdlgpu/clear_ps.spirv.h"
#include "sdlgpu/y16_vs.spirv.h"
#include "sdlgpu/y16_ps.spirv.h"
#ifdef XBOXRECOMP_DEPTH_VIEW_SHADER
#include "sdlgpu/y16d_ps.spirv.h"
#endif
#if defined(_WIN32)
#include "sdlgpu/ub_vs.dxil.h"
#include "sdlgpu/ps_main.dxil.h"
#include "sdlgpu/ps_wdepth.dxil.h"
#include "sdlgpu/ps_main_flat.dxil.h"
#include "sdlgpu/ps_wdepth_flat.dxil.h"
#include "sdlgpu/present_vs.dxil.h"
#include "sdlgpu/present_ps.dxil.h"
#include "sdlgpu/clear_vs.dxil.h"
#include "sdlgpu/clear_ps.dxil.h"
#include "sdlgpu/y16_vs.dxil.h"
#include "sdlgpu/y16_ps.dxil.h"
#ifdef XBOXRECOMP_DEPTH_VIEW_SHADER
#include "sdlgpu/y16d_ps.dxil.h"
#endif
#define DXIL(name) name##_dxil, sizeof name##_dxil
#else
#define DXIL(name) NULL, 0
#endif

extern ptrdiff_t xbox_GetMemoryOffset(void);

#define CONTIG_BASE 0x80000000u
#define CONTIG_SIZE 0x04000000u
#define GUEST_W 640u
#define GUEST_H 480u

static SDL_GPUDevice *s_dev;
static SDL_Window *s_window;
static int s_init_state;               /* 0 untried, 1 ok, -1 failed */
static SDL_GPUCommandBuffer *s_cmd;
static uint32_t s_cmd_serial = 1;      /* bumped per command buffer */

static int64_t qpc(void) { return (int64_t)SDL_GetPerformanceCounter(); }
static int64_t qpf(void) { return (int64_t)SDL_GetPerformanceFrequency(); }

static void fail(const char *what)
{
    fprintf(stderr, "[SDLGPU] %s failed: %s\n", what, SDL_GetError());
    fflush(stderr);
}

/* SDL's own GPU errors (failed buffer maps, device loss) go to the debugger on
 * a GUI-subsystem build, so route warnings and errors to stderr where the
 * run's log keeps them. */
static void SDLCALL sdl_log_to_stderr(void *user, int category, SDL_LogPriority priority, const char *message)
{
    (void)user;
    fprintf(stderr, "[SDL:%d/%d] %s\n", category, (int)priority, message);
    fflush(stderr);
}

/* ------------------------------------------------------------- profiling */

/* RECOMP_D3D_PROFILE: per-frame cost of each stage, printed once a second. */
enum { PROF_VERTEX, PROF_TEXTURE, PROF_DRAW, PROF_SYNC, PROF_WRITEBACK,
       PROF_PRESENT, PROF_PACE, PROF_EXEC, PROF_COUNT };
static const char *const s_prof_name[PROF_COUNT] = {
    "vertex", "texture", "draw", "cpu-sync", "writeback", "present", "pace", "exec(all)" };
static int64_t s_prof[PROF_COUNT];
static uint64_t s_prof_verts, s_prof_draws, s_prof_passes;
static int s_prof_on = -1;

void nv2a_d3d_prof_add(int slot, int64_t ticks, uint32_t verts)
{
    s_prof[slot] += ticks;
    s_prof_verts += verts;
}

int nv2a_d3d_prof_enabled(void)
{
    if (s_prof_on < 0) s_prof_on = getenv("RECOMP_D3D_PROFILE") != NULL;
    return s_prof_on;
}

static void prof_report(void)
{
    static int64_t last, frames, prev, worst;
    int64_t now = qpc(), freq = qpf();
    int i;
    if (!nv2a_d3d_prof_enabled()) return;
    frames++;
    if (prev && now - prev > worst) worst = now - prev;
    prev = now;
    if (!last) { last = now; return; }
    if (now - last < freq) return;
    fprintf(stderr, "[SDLGPU_PROF] %.1f fps (worst %.1f ms) |",
            frames * (double)freq / (double)(now - last), worst * 1000.0 / (double)freq);
    for (i = 0; i < PROF_COUNT; i++)
        fprintf(stderr, " %s %.2f", s_prof_name[i],
                s_prof[i] * 1000.0 / (double)freq / (double)frames);
    fprintf(stderr, " ms/frame | %llu verts %llu draws %llu passes/frame\n",
            (unsigned long long)(s_prof_verts / frames), (unsigned long long)(s_prof_draws / frames),
            (unsigned long long)(s_prof_passes / frames));
    fflush(stderr);
    memset(s_prof, 0, sizeof s_prof);
    s_prof_verts = s_prof_draws = s_prof_passes = 0;
    frames = 0;
    worst = 0;
    last = now;
}

/* RECOMP_FRAME_CSV=<path>: one row per flip, as the D3D11 renderer writes. */
static FILE *s_frame_csv_file;
static int s_frame_csv_tried;
static int64_t s_frame_csv_present, s_frame_csv_present_prev, s_frame_csv_present_interval;

static void frame_csv_init(void)
{
    if (!s_frame_csv_tried) {
        const char *e = getenv("RECOMP_FRAME_CSV");
        s_frame_csv_tried = 1;
        if (e && (s_frame_csv_file = fopen(e, "w")) != NULL) {
            setvbuf(s_frame_csv_file, NULL, _IOFBF, 1 << 16);
            fprintf(s_frame_csv_file, "qpc_ms,frame_ms,pace_ms,present_qpc_ms,present_ms\n");
        }
    }
}

static void frame_csv_present(int64_t now)
{
    s_frame_csv_present_interval = s_frame_csv_present_prev ? now - s_frame_csv_present_prev : 0;
    s_frame_csv_present_prev = s_frame_csv_present = now;
}

static void frame_csv(int64_t pace)
{
    static int64_t prev;
    static unsigned rows;
    int64_t now;
    double ms;
    frame_csv_init();
    now = qpc();
    if (!s_frame_csv_file) { prev = now; return; }
    ms = 1000.0 / (double)qpf();
    if (prev) {
        fprintf(s_frame_csv_file, "%.3f,%.4f,%.4f,", now * ms, (now - prev) * ms, pace * ms);
        if (s_frame_csv_present) {
            fprintf(s_frame_csv_file, "%.3f,", s_frame_csv_present * ms);
            if (s_frame_csv_present_interval)
                fprintf(s_frame_csv_file, "%.4f", s_frame_csv_present_interval * ms);
        } else {
            fputc(',', s_frame_csv_file);
        }
        fputc('\n', s_frame_csv_file);
    }
    if (++rows % 120 == 0) fflush(s_frame_csv_file);
    prev = now;
    s_frame_csv_present = 0;
}

static uint32_t s_frame = 1;
/* Bumped at every pushbuffer kick (see nv2a_d3d11.c). */
static uint32_t s_epoch = 1;

void nv2a_d3d_kick(void)
{
    s_epoch++;
}

/* Internal resolution relative to the guest's (see nv2a_d3d11.c). */
static float s_scale = 1.0f, s_cur_sx = 1.0f, s_cur_sy = 1.0f;
static uint32_t s_aniso = 16;

static NvOverlayHooks s_overlay;

void nv2a_d3d_set_overlay_hooks(const NvOverlayHooks *hooks)
{
    memset(&s_overlay, 0, sizeof s_overlay);
    if (hooks) s_overlay = *hooks;
}

static volatile float s_aspect;

/* The window's size in pixels; 0 if there is no window or it is minimised. */
static int client_size(uint32_t *w, uint32_t *h)
{
    int pw = 0, ph = 0;
    SDL_Window *win = xbox_FramebufferSdlWindow();
    if (!win || (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED) ||
        !SDL_GetWindowSizeInPixels(win, &pw, &ph) || pw <= 0 || ph <= 0)
        return 0;
    *w = (uint32_t)pw;
    *h = (uint32_t)ph;
    return 1;
}

float nv2a_d3d_display_aspect(void)
{
    float a = s_aspect;
    uint32_t w, h;
    if (a) return a;
    if ((a = env_aspect()) != 0) return s_aspect = a;
    if (client_size(&w, &h)) return s_aspect = clamp_aspect((float)w / (float)h);
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        /* No window yet (sizing it): the display it will open on. */
        const SDL_DisplayMode *m = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        if (m && m->h > 0) return clamp_aspect((float)m->w / (float)m->h);
    }
    return 4.0f / 3.0f;
}

static float widen(void)
{
    return nv2a_d3d_display_aspect() * 3.0f / 4.0f;
}

static int guest_ok(uint32_t va, size_t bytes)
{
    if (va < 0x10000u) return 0;
    if (va < 0x08000000u) return (uint64_t)va + bytes <= 0x08000000u;
    if (va >= CONTIG_BASE && va < CONTIG_BASE + CONTIG_SIZE)
        return (uint64_t)va + bytes <= (uint64_t)CONTIG_BASE + CONTIG_SIZE;
    return 0;
}

static const uint8_t *guest(uint32_t va)
{
    return (const uint8_t *)xbox_GetMemoryOffset() + va;
}

static uint32_t phys(uint32_t va)
{
    return va >= CONTIG_BASE ? va - CONTIG_BASE : va;
}

/* ---------------------------------------------------------------- shaders */

enum { SH_UB_VS, SH_PS_MAIN, SH_PS_MAIN_FLAT, SH_PS_WDEPTH, SH_PS_WDEPTH_FLAT,
       SH_PRESENT_VS, SH_PRESENT_PS, SH_CLEAR_VS, SH_CLEAR_PS, SH_Y16_VS, SH_Y16_PS,
#ifdef XBOXRECOMP_DEPTH_VIEW_SHADER
       SH_Y16D_PS,
#endif
       SH_COUNT };

typedef struct {
    const unsigned char *spirv; size_t spirv_size;
    const unsigned char *dxil; size_t dxil_size;
    const char *entry;
    SDL_GPUShaderStage stage;
    Uint32 samplers, storage_buffers, uniforms;
} ShaderSource;

#define SPIRV(name) name##_spirv, sizeof name##_spirv
static const ShaderSource s_shader_src[SH_COUNT] = {
    [SH_UB_VS] = { SPIRV(ub_vs), DXIL(ub_vs), "ub_vs", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1, 4 },
    [SH_PS_MAIN] = { SPIRV(ps_main), DXIL(ps_main), "ps_main", SDL_GPU_SHADERSTAGE_FRAGMENT, 13, 0, 3 },
    [SH_PS_MAIN_FLAT] = { SPIRV(ps_main_flat), DXIL(ps_main_flat), "ps_main", SDL_GPU_SHADERSTAGE_FRAGMENT, 13, 0, 3 },
    [SH_PS_WDEPTH] = { SPIRV(ps_wdepth), DXIL(ps_wdepth), "ps_wdepth", SDL_GPU_SHADERSTAGE_FRAGMENT, 13, 0, 3 },
    [SH_PS_WDEPTH_FLAT] = { SPIRV(ps_wdepth_flat), DXIL(ps_wdepth_flat), "ps_wdepth", SDL_GPU_SHADERSTAGE_FRAGMENT, 13, 0, 3 },
    [SH_PRESENT_VS] = { SPIRV(present_vs), DXIL(present_vs), "present_vs", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1 },
    [SH_PRESENT_PS] = { SPIRV(present_ps), DXIL(present_ps), "present_ps", SDL_GPU_SHADERSTAGE_FRAGMENT, 4, 0, 1 },
    [SH_CLEAR_VS] = { SPIRV(clear_vs), DXIL(clear_vs), "clear_vs", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1 },
    [SH_CLEAR_PS] = { SPIRV(clear_ps), DXIL(clear_ps), "clear_ps", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 1 },
    [SH_Y16_VS] = { SPIRV(y16_vs), DXIL(y16_vs), "y16_vs", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0 },
    [SH_Y16_PS] = { SPIRV(y16_ps), DXIL(y16_ps), "y16_ps", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0 },
#ifdef XBOXRECOMP_DEPTH_VIEW_SHADER
    [SH_Y16D_PS] = { SPIRV(y16d_ps), DXIL(y16d_ps), "y16d_ps", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 1 },
#endif
};
static SDL_GPUShader *s_shader[SH_COUNT];

static int create_shaders(void)
{
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(s_dev);
    int i;
    for (i = 0; i < SH_COUNT; i++) {
        const ShaderSource *src = &s_shader_src[i];
        SDL_GPUShaderCreateInfo ci;
        memset(&ci, 0, sizeof ci);
        ci.entrypoint = src->entry;
        ci.stage = src->stage;
        ci.num_samplers = src->samplers;
        ci.num_storage_buffers = src->storage_buffers;
        ci.num_uniform_buffers = src->uniforms;
        if ((formats & SDL_GPU_SHADERFORMAT_DXIL) && src->dxil) {
            ci.format = SDL_GPU_SHADERFORMAT_DXIL;
            ci.code = src->dxil;
            ci.code_size = src->dxil_size;
        } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
            ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
            ci.code = src->spirv;
            ci.code_size = src->spirv_size;
        } else {
            fprintf(stderr, "[SDLGPU] the GPU driver takes no shader format this build carries\n");
            return 0;
        }
        if (!(s_shader[i] = SDL_CreateGPUShader(s_dev, &ci))) {
            fprintf(stderr, "[SDLGPU] shader %s: %s\n", src->entry, SDL_GetError());
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------- command buffers */

static SDL_GPUCommandBuffer *cmd(void)
{
    if (!s_cmd) {
        s_cmd = SDL_AcquireGPUCommandBuffer(s_dev);
        if (!s_cmd) fail("SDL_AcquireGPUCommandBuffer");
    }
    return s_cmd;
}

/* Vertex and index data for the draws of the open command buffer: appended to
 * mapped transfer buffers, uploaded to the GPU buffers at the same offsets
 * when each batch ends. Both start over (and cycle) with each command buffer. */
#define VB_BYTES (32u << 20)
#define IB_BYTES (16u << 20)
#define UPLOAD_BYTES (64u << 20)
static SDL_GPUBuffer *s_vbuf, *s_ibuf;
static SDL_GPUTransferBuffer *s_vtb, *s_itb, *s_utb;
static uint8_t *s_vmap, *s_imap;
static uint32_t s_vpos, s_ipos, s_upos;
static uint32_t s_vcmd, s_icmd, s_ucmd;     /* command buffer each was last cycled for */

static uint8_t *ring_reserve(SDL_GPUTransferBuffer *tb, uint8_t **map, uint32_t *pos,
                             uint32_t *serial, uint32_t cap, uint32_t bytes)
{
    if (bytes > cap - *pos) return NULL;
    if (!*map) {
        *map = (uint8_t *)SDL_MapGPUTransferBuffer(s_dev, tb, *serial != s_cmd_serial);
        if (!*map) { fail("SDL_MapGPUTransferBuffer"); return NULL; }
        *serial = s_cmd_serial;
    }
    return *map + *pos;
}

/* ------------------------------------------------------------- batches */

typedef enum { OP_PIPE, OP_VUNI, OP_FUNI, OP_SAMPLERS, OP_DRAWI, OP_DRAW } OpKind;
typedef struct {
    uint8_t kind, slot;
    uint32_t a, b, c;               /* draw: count, first; uniform: arena offset, size */
    SDL_GPUGraphicsPipeline *pipe;
    SDL_GPUViewport vp;
    SDL_Rect sc;
    SDL_FColor blend;
    uint8_t stencil_ref;
} Op;

typedef struct {
    SDL_GPUTexture *color, *depth;
    int color_clear, depth_clear;    /* clear on load: a new target's contents */
    SDL_FColor clear_color;
    Op *ops;
    size_t nops, cap;
    uint8_t *arena;
    size_t arena_len, arena_cap;
    uint32_t v0, v1, i0, i1;         /* vertex/index bytes this batch wrote */
    int open;
} Batch;
static Batch s_b;

static void *arena_push(const void *data, size_t bytes, uint32_t *offset)
{
    size_t at = (s_b.arena_len + 15) & ~(size_t)15;
    if (at + bytes > s_b.arena_cap) {
        size_t cap = s_b.arena_cap ? s_b.arena_cap : 1 << 20;
        uint8_t *a;
        while (cap < at + bytes) cap *= 2;
        a = (uint8_t *)realloc(s_b.arena, cap);
        if (!a) return NULL;
        s_b.arena = a;
        s_b.arena_cap = cap;
    }
    if (data) memcpy(s_b.arena + at, data, bytes);
    s_b.arena_len = at + bytes;
    *offset = (uint32_t)at;
    return s_b.arena + at;
}

static Op *op_add(OpKind kind)
{
    Op *o;
    if (s_b.nops == s_b.cap) {
        size_t cap = s_b.cap ? s_b.cap * 2 : 1024;
        Op *ops = (Op *)realloc(s_b.ops, cap * sizeof *ops);
        if (!ops) return NULL;
        s_b.ops = ops;
        s_b.cap = cap;
    }
    o = &s_b.ops[s_b.nops++];
    memset(o, 0, sizeof *o);
    o->kind = (uint8_t)kind;
    return o;
}

/* Uniform data last recorded for each stage and slot in this command buffer:
 * SDL keeps pushed data for the command buffer, so equal data is not pushed
 * again. */
#define UNI_SLOTS 4
static uint8_t *s_last_uni[2][UNI_SLOTS];
static uint32_t s_last_uni_len[2][UNI_SLOTS];
static uint32_t s_last_uni_cmd;

static void rec_uniform(int fragment, uint32_t slot, const void *data, uint32_t bytes)
{
    Op *o;
    uint32_t off;
    if (s_last_uni_cmd != s_cmd_serial) {
        memset(s_last_uni_len, 0, sizeof s_last_uni_len);
        s_last_uni_cmd = s_cmd_serial;
    }
    if (s_last_uni_len[fragment][slot] == bytes && s_last_uni[fragment][slot] &&
        !memcmp(s_last_uni[fragment][slot], data, bytes))
        return;
    if (!arena_push(data, bytes, &off) || !(o = op_add(fragment ? OP_FUNI : OP_VUNI)))
        return;
    o->slot = (uint8_t)slot;
    o->a = off;
    o->b = bytes;
    {
        uint8_t *keep = (uint8_t *)realloc(s_last_uni[fragment][slot], bytes);
        if (keep) {
            memcpy(keep, data, bytes);
            s_last_uni[fragment][slot] = keep;
            s_last_uni_len[fragment][slot] = bytes;
        } else {
            s_last_uni_len[fragment][slot] = 0;
        }
    }
}

static void rec_samplers(const SDL_GPUTextureSamplerBinding *b, uint32_t n)
{
    uint32_t off;
    Op *o;
    if (!arena_push(b, n * sizeof *b, &off) || !(o = op_add(OP_SAMPLERS))) return;
    o->a = off;
    o->b = n;
}

static void submit(SDL_GPUFence **fence);
static void graveyard_empty(void);
static void copy_end(void);

/* End the open batch: upload its vertex/index data, then replay its draws in
 * one render pass. */
static void flush_batch(void)
{
    SDL_GPUCommandBuffer *c;
    SDL_GPUColorTargetInfo ct;
    SDL_GPUDepthStencilTargetInfo dt;
    SDL_GPURenderPass *rp;
    SDL_GPUGraphicsPipeline *pipe = NULL;
    SDL_GPUViewport vp;
    SDL_Rect sc;
    SDL_FColor blend;
    int vp_set = 0, sc_set = 0, blend_set = 0, ref_set = -1;
    size_t i;
    if (!s_b.open) return;
    /* Texture uploads may leave a copy pass open before a target change. */
    copy_end();
    s_b.open = 0;
    if (!s_b.nops && !s_b.color_clear && !s_b.depth_clear) goto reset;
    if (!(c = cmd())) goto reset;
    if (s_b.v1 > s_b.v0 || s_b.i1 > s_b.i0) {
        SDL_GPUCopyPass *cp;
        if (s_vmap) { SDL_UnmapGPUTransferBuffer(s_dev, s_vtb); s_vmap = NULL; }
        if (s_imap) { SDL_UnmapGPUTransferBuffer(s_dev, s_itb); s_imap = NULL; }
        cp = SDL_BeginGPUCopyPass(c);
        if (s_b.v1 > s_b.v0) {
            SDL_GPUTransferBufferLocation from = { s_vtb, s_b.v0 };
            SDL_GPUBufferRegion to = { s_vbuf, s_b.v0, s_b.v1 - s_b.v0 };
            SDL_UploadToGPUBuffer(cp, &from, &to, s_b.v0 == 0);
        }
        if (s_b.i1 > s_b.i0) {
            SDL_GPUTransferBufferLocation from = { s_itb, s_b.i0 };
            SDL_GPUBufferRegion to = { s_ibuf, s_b.i0, s_b.i1 - s_b.i0 };
            SDL_UploadToGPUBuffer(cp, &from, &to, s_b.i0 == 0);
        }
        SDL_EndGPUCopyPass(cp);
    }
    memset(&ct, 0, sizeof ct);
    memset(&dt, 0, sizeof dt);
    ct.texture = s_b.color;
    ct.load_op = s_b.color_clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    ct.clear_color = s_b.clear_color;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    dt.texture = s_b.depth;
    dt.load_op = dt.stencil_load_op = s_b.depth_clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    dt.store_op = dt.stencil_store_op = SDL_GPU_STOREOP_STORE;
    dt.clear_depth = 1.0f;
    dt.clear_stencil = 0;
    rp = SDL_BeginGPURenderPass(c, s_b.color ? &ct : NULL, s_b.color ? 1 : 0,
                                s_b.depth ? &dt : NULL);
    if (!rp) { fail("SDL_BeginGPURenderPass"); goto reset; }
    s_prof_passes++;
    for (i = 0; i < s_b.nops; i++) {
        const Op *o = &s_b.ops[i];
        switch (o->kind) {
        case OP_PIPE:
            if (o->pipe != pipe) {
                SDL_BindGPUGraphicsPipeline(rp, o->pipe);
                pipe = o->pipe;
            }
            if (o->a) {          /* the uber vertex shader's data */
                SDL_GPUBufferBinding ib = { s_ibuf, 0 };
                SDL_BindGPUVertexStorageBuffers(rp, 0, &s_vbuf, 1);
                SDL_BindGPUIndexBuffer(rp, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            }
            break;
        case OP_VUNI:
            SDL_PushGPUVertexUniformData(c, o->slot, s_b.arena + o->a, o->b);
            break;
        case OP_FUNI:
            SDL_PushGPUFragmentUniformData(c, o->slot, s_b.arena + o->a, o->b);
            break;
        case OP_SAMPLERS:
            SDL_BindGPUFragmentSamplers(rp, 0, (const SDL_GPUTextureSamplerBinding *)(s_b.arena + o->a), o->b);
            break;
        case OP_DRAWI:
        case OP_DRAW:
            if (!vp_set || memcmp(&vp, &o->vp, sizeof vp)) {
                vp = o->vp; vp_set = 1;
                SDL_SetGPUViewport(rp, &vp);
            }
            if (!sc_set || memcmp(&sc, &o->sc, sizeof sc)) {
                sc = o->sc; sc_set = 1;
                SDL_SetGPUScissor(rp, &sc);
            }
            if (!blend_set || memcmp(&blend, &o->blend, sizeof blend)) {
                blend = o->blend; blend_set = 1;
                SDL_SetGPUBlendConstants(rp, blend);
            }
            if (ref_set != o->stencil_ref) {
                ref_set = o->stencil_ref;
                SDL_SetGPUStencilReference(rp, o->stencil_ref);
            }
            if (o->kind == OP_DRAWI) SDL_DrawGPUIndexedPrimitives(rp, o->a, 1, o->b, 0, 0);
            else SDL_DrawGPUPrimitives(rp, o->a, 1, o->b, 0);
            break;
        }
    }
    SDL_EndGPURenderPass(rp);
reset:
    graveyard_empty();
    s_b.nops = 0;
    s_b.arena_len = 0;
    s_b.color = s_b.depth = NULL;
    s_b.color_clear = s_b.depth_clear = 0;
    s_b.v0 = s_b.v1 = s_vpos;
    s_b.i0 = s_b.i1 = s_ipos;
}

/* Draws from here on go to color/depth (either may be NULL). A target
 * created since it was last drawn starts cleared (*clear is reset). */
static void target(SDL_GPUTexture *color, int *color_clear, SDL_GPUTexture *depth, int *depth_clear)
{
    if (s_b.open && s_b.color == color && s_b.depth == depth &&
        !(color_clear && *color_clear) && !(depth_clear && *depth_clear))
        return;
    flush_batch();
    s_b.open = 1;
    s_b.color = color;
    s_b.depth = depth;
    s_b.color_clear = color_clear && *color_clear;
    s_b.depth_clear = depth_clear && *depth_clear;
    memset(&s_b.clear_color, 0, sizeof s_b.clear_color);
    if (color_clear) *color_clear = 0;
    if (depth_clear) *depth_clear = 0;
    s_b.v0 = s_b.v1 = s_vpos;
    s_b.i0 = s_b.i1 = s_ipos;
}

/* Submit the command buffer, optionally fenced; data rings start over. */
static void submit(SDL_GPUFence **fence)
{
    flush_batch();
    if (fence) *fence = NULL;
    if (!s_cmd) return;
    if (s_vmap) { SDL_UnmapGPUTransferBuffer(s_dev, s_vtb); s_vmap = NULL; }
    if (s_imap) { SDL_UnmapGPUTransferBuffer(s_dev, s_itb); s_imap = NULL; }
    RECOMP_PROFILE_BEGIN("SDL GPU submission");
    if (fence) {
        *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(s_cmd);
        /* Submitted, but no fence to wait on: a readback must not map until
         * the GPU is idle. */
        if (!*fence) SDL_WaitForGPUIdle(s_dev);
    } else {
        SDL_SubmitGPUCommandBuffer(s_cmd);
    }
    RECOMP_PROFILE_END();
    s_cmd = NULL;
    s_cmd_serial++;
    s_vpos = s_ipos = s_upos = 0;
    s_b.v0 = s_b.v1 = s_b.i0 = s_b.i1 = 0;
}

/* --------------------------------------------------------- copy passes */

static SDL_GPUCopyPass *s_copy;

/* A copy pass in order after everything recorded so far. */
static SDL_GPUCopyPass *copy_begin(void)
{
    flush_batch();
    if (!s_copy && cmd()) s_copy = SDL_BeginGPUCopyPass(s_cmd);
    return s_copy;
}

static void copy_end(void)
{
    if (s_copy) { SDL_EndGPUCopyPass(s_copy); s_copy = NULL; }
}

/* Anything that records outside a copy pass ends one first. */
static void outside_copy(void)
{
    copy_end();
}

/* Upload pixels to one level/layer of a texture. */
static int upload_texture_extent(SDL_GPUTexture *t, uint32_t level, uint32_t layer,
                          uint32_t w, uint32_t h, uint32_t depth, const void *data, uint32_t bytes)
{
    SDL_GPUTransferBuffer *tb = s_utb;
    uint32_t offset;
    uint8_t *p;
    int temp = 0;
    SDL_GPUTextureTransferInfo src;
    SDL_GPUTextureRegion dst;
    if (bytes > UPLOAD_BYTES) {
        SDL_GPUTransferBufferCreateInfo ci = { SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes, 0 };
        tb = SDL_CreateGPUTransferBuffer(s_dev, &ci);
        if (!tb) { fail("transfer buffer"); return 0; }
        temp = 1;
        offset = 0;
        p = (uint8_t *)SDL_MapGPUTransferBuffer(s_dev, tb, false);
    } else {
        if (bytes > UPLOAD_BYTES - ((s_upos + 255) & ~255u)) {
            copy_end();
            submit(NULL);
        }
        offset = (s_upos + 255) & ~255u;
        p = (uint8_t *)SDL_MapGPUTransferBuffer(s_dev, tb, s_ucmd != s_cmd_serial);
        s_ucmd = s_cmd_serial;
        if (p) p += offset;
        s_upos = offset + bytes;
    }
    if (!p) { fail("SDL_MapGPUTransferBuffer"); if (temp) SDL_ReleaseGPUTransferBuffer(s_dev, tb); return 0; }
    memcpy(p, data, bytes);
    SDL_UnmapGPUTransferBuffer(s_dev, tb);
    if (!copy_begin()) { if (temp) SDL_ReleaseGPUTransferBuffer(s_dev, tb); return 0; }
    memset(&src, 0, sizeof src);
    memset(&dst, 0, sizeof dst);
    src.transfer_buffer = tb;
    src.offset = offset;
    dst.texture = t;
    dst.mip_level = level;
    dst.layer = layer;
    dst.w = w;
    dst.h = h;
    dst.d = depth;
    SDL_UploadToGPUTexture(s_copy, &src, &dst, false);
    if (temp) SDL_ReleaseGPUTransferBuffer(s_dev, tb);   /* released once the GPU is done */
    return 1;
}

static int upload_texture(SDL_GPUTexture *t, uint32_t level, uint32_t layer,
                          uint32_t w, uint32_t h, const void *data, uint32_t bytes)
{
    return upload_texture_extent(t, level, layer, w, h, 1, data, bytes);
}

static void copy_texture(SDL_GPUTexture *dst, uint32_t dst_level, SDL_GPUTexture *src,
                         uint32_t src_level, uint32_t w, uint32_t h)
{
    SDL_GPUTextureLocation a, b;
    if (!copy_begin()) return;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    a.texture = src; a.mip_level = src_level;
    b.texture = dst; b.mip_level = dst_level;
    SDL_CopyGPUTextureToTexture(s_copy, &a, &b, w, h, 1, false);
}

/* ----------------------------------------------------------- textures */

static SDL_GPUTexture *make_texture(uint32_t w, uint32_t h, uint32_t levels, int cube,
                                    SDL_GPUTextureFormat f, SDL_GPUTextureUsageFlags usage)
{
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTexture *t;
    memset(&ci, 0, sizeof ci);
    ci.type = cube ? SDL_GPU_TEXTURETYPE_CUBE : SDL_GPU_TEXTURETYPE_2D;
    ci.format = f;
    ci.usage = usage;
    ci.width = w ? w : 1;
    ci.height = h ? h : 1;
    ci.layer_count_or_depth = cube ? 6 : 1;
    ci.num_levels = levels ? levels : 1;
    ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
    t = SDL_CreateGPUTexture(s_dev, &ci);
    if (!t) fail("SDL_CreateGPUTexture");
    return t;
}

static SDL_GPUTexture *make_volume(uint32_t w, uint32_t h, uint32_t d, uint32_t levels)
{
    SDL_GPUTextureCreateInfo ci = {0};
    ci.type = SDL_GPU_TEXTURETYPE_3D;
    ci.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = w; ci.height = h; ci.layer_count_or_depth = d;
    ci.num_levels = levels; ci.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(s_dev, &ci);
}

/* Textures released while a batch may still name them wait for its end. */
static SDL_GPUTexture **s_graveyard;
static size_t s_graveyard_n, s_graveyard_cap;

static void release_texture(SDL_GPUTexture **t)
{
    if (!*t) return;
    if (s_b.open && s_b.nops) {
        if (s_graveyard_n == s_graveyard_cap) {
            size_t cap = s_graveyard_cap ? s_graveyard_cap * 2 : 64;
            SDL_GPUTexture **g = (SDL_GPUTexture **)realloc(s_graveyard, cap * sizeof *g);
            if (!g) { flush_batch(); SDL_ReleaseGPUTexture(s_dev, *t); *t = NULL; return; }
            s_graveyard = g;
            s_graveyard_cap = cap;
        }
        s_graveyard[s_graveyard_n++] = *t;
    } else {
        SDL_ReleaseGPUTexture(s_dev, *t);
    }
    *t = NULL;
}

/* Samplers evicted from a full cache. The draw being set up may already hold
 * the evicted one (looked up earlier in the same setup_pipeline call), so it
 * lives until the batch that records that draw has been replayed. */
static SDL_GPUSampler **s_samp_graveyard;
static size_t s_samp_graveyard_n, s_samp_graveyard_cap;

static void graveyard_empty(void)
{
    size_t i;
    for (i = 0; i < s_graveyard_n; i++) SDL_ReleaseGPUTexture(s_dev, s_graveyard[i]);
    s_graveyard_n = 0;
    for (i = 0; i < s_samp_graveyard_n; i++) SDL_ReleaseGPUSampler(s_dev, s_samp_graveyard[i]);
    s_samp_graveyard_n = 0;
}

/* ------------------------------------------------------------- pipelines */

enum { FMT_NONE, FMT_BGRA8, FMT_RGBA16, FMT_R32F };
static SDL_GPUTextureFormat s_depth_format;

typedef struct {
    uint8_t vs, ps, color, depth, prim, blend_enable, write_mask, cull;
    uint8_t front_ccw, zt, zw, zfunc, st, sfunc, rmask, wmask;
    uint8_t sfail, szfail, szpass, pad;
    uint16_t sf, df, eq, pad2;
    int32_t bias;
    float slope;
} PipeKey;

typedef struct { PipeKey key; SDL_GPUGraphicsPipeline *pipe; } PipeSlot;
#define PIPE_SLOTS 4096
static PipeSlot *s_pipes;

static SDL_GPUBlendFactor blend_factor(uint32_t f, int alpha)
{
    switch (f) {
    case 0x0000: return SDL_GPU_BLENDFACTOR_ZERO;
    case 0x0001: return SDL_GPU_BLENDFACTOR_ONE;
    case 0x0300: return alpha ? SDL_GPU_BLENDFACTOR_SRC_ALPHA : SDL_GPU_BLENDFACTOR_SRC_COLOR;
    case 0x0301: return alpha ? SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    case 0x0302: return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case 0x0303: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case 0x0304: return SDL_GPU_BLENDFACTOR_DST_ALPHA;
    case 0x0305: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    case 0x0306: return alpha ? SDL_GPU_BLENDFACTOR_DST_ALPHA : SDL_GPU_BLENDFACTOR_DST_COLOR;
    case 0x0307: return alpha ? SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA : SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
    case 0x0308: return SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE;
    case 0x8001: case 0x8003: return SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
    case 0x8002: case 0x8004: return SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
    default: return SDL_GPU_BLENDFACTOR_ONE;
    }
}

static SDL_GPUBlendOp blend_op(uint32_t e)
{
    switch (e) {
    /* F005/F006: as the D3D11 renderer maps them (see blend_op there). */
    case 0x800A: case 0xF005: return SDL_GPU_BLENDOP_SUBTRACT;
    case 0x800B: case 0xF006: return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
    case 0x8007: return SDL_GPU_BLENDOP_MIN;
    case 0x8008: return SDL_GPU_BLENDOP_MAX;
    default: return SDL_GPU_BLENDOP_ADD;
    }
}

static SDL_GPUCompareOp cmp(uint32_t f)
{
    switch (f) {
    case 0x200: return SDL_GPU_COMPAREOP_NEVER;
    case 0x201: return SDL_GPU_COMPAREOP_LESS;
    case 0x202: return SDL_GPU_COMPAREOP_EQUAL;
    case 0x203: return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    case 0x204: return SDL_GPU_COMPAREOP_GREATER;
    case 0x205: return SDL_GPU_COMPAREOP_NOT_EQUAL;
    case 0x206: return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
    default: return SDL_GPU_COMPAREOP_ALWAYS;
    }
}

static uint8_t sop_code(uint32_t o)
{
    switch (o) {
    case 0x0000: return 1;   /* zero */
    case 0x1E01: return 2;   /* replace */
    case 0x1E02: return 3;   /* increment, clamp */
    case 0x1E03: return 4;   /* decrement, clamp */
    case 0x150A: return 5;   /* invert */
    case 0x8507: return 6;   /* increment, wrap */
    case 0x8508: return 7;   /* decrement, wrap */
    default: return 0;       /* keep */
    }
}

static SDL_GPUStencilOp sop(uint8_t code)
{
    static const SDL_GPUStencilOp ops[8] = {
        SDL_GPU_STENCILOP_KEEP, SDL_GPU_STENCILOP_ZERO, SDL_GPU_STENCILOP_REPLACE,
        SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP, SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP,
        SDL_GPU_STENCILOP_INVERT, SDL_GPU_STENCILOP_INCREMENT_AND_WRAP,
        SDL_GPU_STENCILOP_DECREMENT_AND_WRAP };
    return ops[code & 7];
}

static SDL_GPUTextureFormat color_format(uint8_t f)
{
    switch (f) {
    case FMT_BGRA8: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case FMT_RGBA16: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM;
    case FMT_R32F: return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    default: return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

static SDL_GPUGraphicsPipeline *pipeline(const PipeKey *k)
{
    uint64_t h = hash_mem((const uint8_t *)k, sizeof *k);
    uint32_t i, n;
    for (n = 0; n < PIPE_SLOTS; n++) {
        PipeSlot *s = &s_pipes[(h + n) & (PIPE_SLOTS - 1)];
        if (s->pipe && !memcmp(&s->key, k, sizeof *k)) return s->pipe;
        if (!s->pipe) break;
    }
    i = (uint32_t)((h + n) & (PIPE_SLOTS - 1));
    if (n == PIPE_SLOTS) {
        /* Full: replace the home slot. A pipeline a recorded draw names stays
         * alive until SDL is done with it (release is deferred by SDL). */
        flush_batch();
        i = (uint32_t)(h & (PIPE_SLOTS - 1));
        SDL_ReleaseGPUGraphicsPipeline(s_dev, s_pipes[i].pipe);
        s_pipes[i].pipe = NULL;
    }
    {
        SDL_GPUGraphicsPipelineCreateInfo ci;
        SDL_GPUColorTargetDescription ctd;
        SDL_GPUStencilOpState so;
        memset(&ci, 0, sizeof ci);
        memset(&ctd, 0, sizeof ctd);
        ci.vertex_shader = s_shader[k->vs];
        ci.fragment_shader = s_shader[k->ps];
        ci.primitive_type = (SDL_GPUPrimitiveType)k->prim;
        ci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        ci.rasterizer_state.cull_mode = k->cull == 1 ? SDL_GPU_CULLMODE_FRONT :
                                        k->cull == 2 ? SDL_GPU_CULLMODE_BACK : SDL_GPU_CULLMODE_NONE;
        ci.rasterizer_state.front_face = k->front_ccw ? SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
                                                      : SDL_GPU_FRONTFACE_CLOCKWISE;
        ci.rasterizer_state.depth_bias_constant_factor = (float)k->bias;
        ci.rasterizer_state.depth_bias_slope_factor = k->slope;
        ci.rasterizer_state.enable_depth_bias = k->bias != 0 || k->slope != 0.0f;
        ci.rasterizer_state.enable_depth_clip = false;
        ci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        ci.depth_stencil_state.enable_depth_test = k->zt != 0;
        ci.depth_stencil_state.enable_depth_write = k->zw != 0;
        ci.depth_stencil_state.compare_op = cmp(0x200 | k->zfunc);
        ci.depth_stencil_state.enable_stencil_test = k->st != 0;
        ci.depth_stencil_state.compare_mask = k->rmask;
        ci.depth_stencil_state.write_mask = k->wmask;
        so.fail_op = sop(k->sfail);
        so.depth_fail_op = sop(k->szfail);
        so.pass_op = sop(k->szpass);
        so.compare_op = cmp(0x200 | k->sfunc);
        ci.depth_stencil_state.front_stencil_state = so;
        ci.depth_stencil_state.back_stencil_state = so;
        if (k->color) {
            ctd.format = color_format(k->color);
            ctd.blend_state.enable_blend = k->blend_enable != 0;
            ctd.blend_state.src_color_blendfactor = blend_factor(k->sf, 0);
            ctd.blend_state.dst_color_blendfactor = blend_factor(k->df, 0);
            ctd.blend_state.src_alpha_blendfactor = blend_factor(k->sf, 1);
            ctd.blend_state.dst_alpha_blendfactor = blend_factor(k->df, 1);
            ctd.blend_state.color_blend_op = ctd.blend_state.alpha_blend_op = blend_op(k->eq);
            ctd.blend_state.enable_color_write_mask = true;
            ctd.blend_state.color_write_mask = (SDL_GPUColorComponentFlags)(k->write_mask & 15);
            ci.target_info.color_target_descriptions = &ctd;
            ci.target_info.num_color_targets = 1;
        }
        ci.target_info.has_depth_stencil_target = k->depth != 0;
        ci.target_info.depth_stencil_format = k->depth ? s_depth_format : SDL_GPU_TEXTUREFORMAT_INVALID;
        RECOMP_PROFILE_BEGIN("SDL GPU pipeline creation");
        s_pipes[i].pipe = SDL_CreateGPUGraphicsPipeline(s_dev, &ci);
        RECOMP_PROFILE_END();
        if (!s_pipes[i].pipe) { fail("SDL_CreateGPUGraphicsPipeline"); return NULL; }
        s_pipes[i].key = *k;
        return s_pipes[i].pipe;
    }
}

static void pipe_key_plain(PipeKey *k, uint8_t vs, uint8_t ps, uint8_t color, uint8_t prim)
{
    memset(k, 0, sizeof *k);
    k->vs = vs; k->ps = ps; k->color = color; k->prim = prim;
    k->write_mask = 15;
    k->zfunc = 7; k->sfunc = 7;
}

/* ---------------------------------------------------------------- samplers */

typedef Nv2aSamplerSlot SampSlot;
#define SAMP_SLOTS 1024
static SampSlot s_samp[SAMP_SLOTS];
static SDL_GPUSampler *s_linear_clamp, *s_point_clamp;
static SDL_GPUTexture *s_dummy_2d, *s_dummy_cube, *s_dummy_volume;

static SDL_GPUSamplerAddressMode addr_mode(uint32_t m)
{
    switch (m) {
    case 1: return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    case 2: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    default: return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;   /* 4, border: see PSB */
    }
}

static SDL_GPUSampler *sampler(const NvD3DTexture *t, int levels, int reduce,
                               void *const *pinned)
{
    uint32_t minf = (t->filter >> 16) & 0xFF, magf = (t->filter >> 24) & 0xF;
    int bias13 = (int)(t->filter & 0x1FFF);
    uint64_t key;
    uint32_t i;
    size_t slot;
    int found;
    if (bias13 & 0x1000) bias13 -= 0x2000;
    key = (uint64_t)minf | ((uint64_t)magf << 8) | ((uint64_t)(bias13 & 0x1FFF) << 12) |
          ((uint64_t)(levels & 15) << 25) | ((uint64_t)(reduce != 0) << 29) |
          ((uint64_t)(t->address & 0xFFFFF) << 31);
    key ^= (uint64_t)((t->control0 >> 6) & 0xFFFFFF) * 0xFF51AFD7ED558CCDull;
    i = (uint32_t)(key ^ (key >> 29) ^ (key >> 47)) * 2654435761u;
    slot = nv2a_sampler_slot(s_samp, SAMP_SLOTS, i & (SAMP_SLOTS - 1), key,
                             pinned, 4, &found);
    if (slot == SAMP_SLOTS) return NULL;
    if (found) return (SDL_GPUSampler *)s_samp[slot].resource;
    if (s_samp[slot].resource) {
        if (s_samp_graveyard_n == s_samp_graveyard_cap) {
            size_t cap = s_samp_graveyard_cap ? s_samp_graveyard_cap * 2 : 16;
            SDL_GPUSampler **g = (SDL_GPUSampler **)realloc(s_samp_graveyard, cap * sizeof *g);
            if (!g) return NULL;        /* the caller falls back to its default sampler */
            s_samp_graveyard = g;
            s_samp_graveyard_cap = cap;
        }
        s_samp_graveyard[s_samp_graveyard_n++] = (SDL_GPUSampler *)s_samp[slot].resource;
    }
    {
        SampSlot *s = &s_samp[slot];
        SDL_GPUSamplerCreateInfo d;
        int min_lin = minf == 2 || minf == 4 || minf == 6;
        int mip_lin = minf == 5 || minf == 6;
        int mag_lin = magf != 1;
        memset(&d, 0, sizeof d);
        d.min_filter = min_lin ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        d.mag_filter = mag_lin ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        d.mipmap_mode = mip_lin ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        d.address_mode_u = addr_mode(t->address & 0xF);
        d.address_mode_v = addr_mode((t->address >> 8) & 0xF);
        d.address_mode_w = addr_mode((t->address >> 16) & 0xF);
        d.enable_anisotropy = min_lin && mag_lin && s_aniso > 1;
        d.max_anisotropy = (float)s_aniso;
        d.mip_lod_bias = (float)bias13 / 256.0f;
        if (minf <= 2 || minf == 7 || levels <= 1) {
            d.min_lod = d.max_lod = 0;
        } else {
            d.min_lod = (float)((t->control0 >> 18) & 0xFFF) / 256.0f;
            d.max_lod = (float)((t->control0 >> 6) & 0xFFF) / 256.0f;
            if (d.max_lod < d.min_lod) d.max_lod = 1000.0f;
        }
        if (reduce) {
            d.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            d.min_lod = 0;
            d.max_lod = 1000.0f;
        }
        s->resource = SDL_CreateGPUSampler(s_dev, &d);
        s->key = key;
        if (!s->resource) fail("SDL_CreateGPUSampler");
        return (SDL_GPUSampler *)s->resource;
    }
}

/* ------------------------------------------------------------------ init */

static void display_config(int now);

int nv2a_d3d_init(void)
{
    const char *e;
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV;
    if (s_init_state)
        return s_init_state > 0;
    s_init_state = -1;
    nv2a_d3d_prof_enabled();
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_WARN);
    SDL_SetLogOutputFunction(sdl_log_to_stderr, NULL);
#if defined(_WIN32)
    formats |= SDL_GPU_SHADERFORMAT_DXIL;
#endif
    xbox_FramebufferSdlHints();      /* before video starts: the driver choice */
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { fail("SDL video"); return 0; }
    {
        /* RECOMP_SDL_GPU_DRIVER: vulkan, direct3d12 or metal; else SDL's pick. */
        SDL_PropertiesID p = SDL_CreateProperties();
        SDL_SetBooleanProperty(p, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
        SDL_SetBooleanProperty(p, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN,
                               (formats & SDL_GPU_SHADERFORMAT_DXIL) != 0);
        SDL_SetBooleanProperty(p, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN,
                               getenv("RECOMP_D3D_DEBUG") != NULL);
        SDL_SetBooleanProperty(p, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, false);
        if ((e = getenv("RECOMP_SDL_GPU_DRIVER")) != NULL && *e)
            SDL_SetStringProperty(p, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING, e);
        s_dev = SDL_CreateGPUDeviceWithProperties(p);
        SDL_DestroyProperties(p);
    }
    if (!s_dev) { fail("SDL_CreateGPUDevice"); return 0; }
    fprintf(stderr, "[SDLGPU] device: %s\n", SDL_GetGPUDeviceDriver(s_dev));
    if (!create_shaders()) return 0;
    s_pipes = (PipeSlot *)calloc(PIPE_SLOTS, sizeof *s_pipes);
    if (!s_pipes) return 0;
    s_depth_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
    if (!SDL_GPUTextureSupportsFormat(s_dev, s_depth_format, SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER))
        s_depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
    {
        SDL_GPUBufferCreateInfo b;
        SDL_GPUTransferBufferCreateInfo t;
        memset(&b, 0, sizeof b);
        memset(&t, 0, sizeof t);
        b.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        b.size = VB_BYTES;
        s_vbuf = SDL_CreateGPUBuffer(s_dev, &b);
        b.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        b.size = IB_BYTES;
        s_ibuf = SDL_CreateGPUBuffer(s_dev, &b);
        t.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        t.size = VB_BYTES;
        s_vtb = SDL_CreateGPUTransferBuffer(s_dev, &t);
        t.size = IB_BYTES;
        s_itb = SDL_CreateGPUTransferBuffer(s_dev, &t);
        t.size = UPLOAD_BYTES;
        s_utb = SDL_CreateGPUTransferBuffer(s_dev, &t);
        if (!s_vbuf || !s_ibuf || !s_vtb || !s_itb || !s_utb) { fail("buffer creation"); return 0; }
    }
    {
        SDL_GPUSamplerCreateInfo d;
        memset(&d, 0, sizeof d);
        d.min_filter = d.mag_filter = SDL_GPU_FILTER_LINEAR;
        d.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        d.address_mode_u = d.address_mode_v = d.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        d.max_lod = 1000.0f;
        s_linear_clamp = SDL_CreateGPUSampler(s_dev, &d);
        d.min_filter = d.mag_filter = SDL_GPU_FILTER_NEAREST;
        d.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        s_point_clamp = SDL_CreateGPUSampler(s_dev, &d);
    }
    {
        static const uint32_t black[6] = { 0xFF000000u, 0xFF000000u, 0xFF000000u,
                                           0xFF000000u, 0xFF000000u, 0xFF000000u };
        int f;
        s_dummy_2d = make_texture(1, 1, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        s_dummy_cube = make_texture(1, 1, 1, 1, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        s_dummy_volume = make_volume(1, 1, 1, 1);
        if (!s_dummy_volume || !s_dummy_2d || !s_dummy_cube || !s_linear_clamp || !s_point_clamp) return 0;
        upload_texture(s_dummy_2d, 0, 0, 1, 1, black, 4);
        upload_texture(s_dummy_volume, 0, 0, 1, 1, black, 4);
        for (f = 0; f < 6; f++) upload_texture(s_dummy_cube, 0, (uint32_t)f, 1, 1, black + f, 4);
        copy_end();
    }
    /* The window: opened on this thread, which presents to it. */
    s_window = fbw_sdl_open(0);
    if (s_window) {
        if (!SDL_ClaimWindowForGPUDevice(s_dev, s_window)) {
            fail("SDL_ClaimWindowForGPUDevice");
            s_window = NULL;
        } else {
            int vsync = getenv("RECOMP_VSYNC") ? atoi(getenv("RECOMP_VSYNC")) : 0;
            SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
            if (!vsync) {
                if (SDL_WindowSupportsGPUPresentMode(s_dev, s_window, SDL_GPU_PRESENTMODE_IMMEDIATE))
                    mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
                else if (SDL_WindowSupportsGPUPresentMode(s_dev, s_window, SDL_GPU_PRESENTMODE_MAILBOX))
                    mode = SDL_GPU_PRESENTMODE_MAILBOX;
            }
            SDL_SetGPUSwapchainParameters(s_dev, s_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
        }
    }
    display_config(1);
    if ((e = getenv("RECOMP_ANISO")) != NULL) s_aniso = (uint32_t)atoi(e);
    if (s_aniso < 1) s_aniso = 1;
    if (s_aniso > 16) s_aniso = 16;
    if ((e = getenv("RECOMP_MSAA")) != NULL && atoi(e) > 1)
        fprintf(stderr, "[SDLGPU] RECOMP_MSAA=%s: targets are single-sample here; render scale supersamples\n", e);
    s_init_state = 1;
    fprintf(stderr, "[SDLGPU] renderer ready: %ux%u internal, display aspect %.3f, %ux AF\n",
            (uint32_t)(GUEST_W * s_scale * widen() + 0.5f), (uint32_t)(GUEST_H * s_scale + 0.5f),
            nv2a_d3d_display_aspect(), s_aniso);
    fflush(stderr);
    return 1;
}

int nv2a_d3d_active(void)
{
    return s_init_state > 0;
}

/* --------------------------------------------------------------- surfaces */

typedef struct {
    uint32_t phys, va, pitch, bpp, w, h, iw, ih;
    uint32_t color_fmt;
    float sx, sy;
    int swizzled;
    SDL_GPUTexture *tex;
    int clear;                      /* contents undefined until drawn */
    uint32_t gen, y16_gen;
    SDL_GPUTexture *y16;
    int y16_clear;
    SDL_GPUTexture *mip;
    uint32_t mip_gen;
    SDL_GPUTexture *chain;
    int chain_levels;
    uint32_t chain_gen[16];
    uint8_t *shadow;
    int gpu_dirty;
    int cpu_present_pending;
    uint32_t synced_frame, used_frame;
    uint32_t seq;
} Surface;

typedef struct {
    uint32_t phys, iw, ih;
    SDL_GPUTexture *tex;
    int clear;
    SDL_GPUTexture *y16;
    int y16_clear;
    uint32_t zf, w, h;
    uint32_t gen, y16_gen, seq;
    int depth_mode; /* 0 unknown, 1 Z, 2 W: encoding of the last depth writer */
    uint32_t used_frame;
} Depth;

#define MAX_SURFACES 32
#define MAX_DEPTHS 16
static Surface s_surf[MAX_SURFACES];
static Depth s_depth[MAX_DEPTHS];
static Surface *s_cur_rt;
static Depth *s_cur_ds;
static uint32_t s_aa_x = 1, s_aa_y = 1;

#define USAGE_TARGET (SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)

/* Draw columns [u0, u1) of src into columns [x0, x1) of dst (fractions of
 * each width), full height; with a mask, only where the mask is set. */
static int blit_span(SDL_GPUTexture *src, SDL_GPUTexture *mask, SDL_GPUTexture *dst, int *dst_clear,
                     uint32_t dst_w, uint32_t dst_h, float alpha,
                     float x0, float x1, float u0, float u1)
{
    PresentConsts pc;
    PipeKey k;
    SDL_GPUGraphicsPipeline *p;
    SDL_GPUTextureSamplerBinding b[4];
    Op *o;
    memset(&pc, 0, sizeof pc);
    pc.dst_rect[0] = x0 * 2 - 1; pc.dst_rect[1] = 1; pc.dst_rect[2] = x1 * 2 - 1; pc.dst_rect[3] = -1;
    pc.src_uv[0] = u0; pc.src_uv[2] = u1; pc.src_uv[3] = 1;
    pc.src_size[0] = pc.src_size[1] = pc.src_size[2] = pc.src_size[3] = 1;
    pc.force_alpha[0] = alpha;
    pc.ovl_flags[3] = mask ? 1 : 0;
    pipe_key_plain(&k, SH_PRESENT_VS, SH_PRESENT_PS, FMT_BGRA8, SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP);
    if (!(p = pipeline(&k))) return 0;
    target(dst, dst_clear, NULL, NULL);
    if (!(o = op_add(OP_PIPE))) return 0;
    o->pipe = p;
    rec_uniform(0, 0, &pc, sizeof pc);
    rec_uniform(1, 0, &pc, sizeof pc);
    b[0].texture = src; b[0].sampler = s_linear_clamp;
    b[1].texture = mask ? mask : s_dummy_2d; b[1].sampler = s_point_clamp;
    b[2].texture = src; b[2].sampler = s_point_clamp;
    b[3].texture = mask ? mask : s_dummy_2d; b[3].sampler = s_linear_clamp;
    rec_samplers(b, 4);
    if (!(o = op_add(OP_DRAW))) return 0;
    o->a = 4; o->b = 0;
    o->vp.w = (float)dst_w; o->vp.h = (float)dst_h; o->vp.max_depth = 1;
    o->sc.w = (int)dst_w; o->sc.h = (int)dst_h;
    s_cur_rt = NULL;
    s_cur_ds = NULL;
    return 1;
}

static void guest_span(const Surface *s, float *x0, float *x1)
{
    float f = s->sx > s->sy * 1.001f ? s->sy / s->sx : 1.0f;
    *x0 = (1.0f - f) * 0.5f;
    *x1 = (1.0f + f) * 0.5f;
}

/* Guest rows of a surface as BGRA, with the format's fixed alpha. */
static void surface_rows_to_bgra(const Surface *s, const uint8_t *src, uint32_t pitch, uint32_t *out)
{
    int amode = surface_alpha_mode(s->color_fmt);
    uint32_t x, y;
    for (y = 0; y < s->h; y++) {
        const uint8_t *row = src + (size_t)y * pitch;
        uint32_t *dst = out + (size_t)y * s->w;
        if (s->bpp == 4) {
            memcpy(dst, row, (size_t)s->w * 4);
            if (amode >= 0)
                for (x = 0; x < s->w; x++)
                    dst[x] = (dst[x] & 0xFFFFFFu) | (amode ? 0xFF000000u : 0);
        } else if (s->bpp == 2) {
            const uint16_t *p = (const uint16_t *)row;
            for (x = 0; x < s->w; x++) {
                uint32_t v = p[x], r, g, b, a;
                if ((s->color_fmt & 0xF) == 0x3) {
                    r = d3d8_expand_channel((v >> 11) & 31, 5);
                    g = d3d8_expand_channel((v >> 5) & 63, 6);
                    b = d3d8_expand_channel(v & 31, 5);
                } else {
                    r = d3d8_expand_channel((v >> 10) & 31, 5);
                    g = d3d8_expand_channel((v >> 5) & 31, 5);
                    b = d3d8_expand_channel(v & 31, 5);
                }
                a = amode == 0 ? 0 : 255;
                dst[x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        } else {
            memset(dst, 0, (size_t)s->w * 4);
        }
    }
}

/* Upload textures for guest images, reused while the size holds. */
static SDL_GPUTexture *s_up_tex[2];
static uint32_t s_up_w[2], s_up_h[2];
static uint32_t *s_up_pixels, *s_mask_pixels;
static size_t s_up_cap, s_mask_cap;
static uint8_t *s_swz_rows;
static size_t s_swz_cap;

static SDL_GPUTexture *upload_slot(int k, const uint32_t *pixels, uint32_t w, uint32_t h)
{
    if (!s_up_tex[k] || s_up_w[k] != w || s_up_h[k] != h) {
        release_texture(&s_up_tex[k]);
        s_up_tex[k] = make_texture(w, h, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                   SDL_GPU_TEXTUREUSAGE_SAMPLER);
        if (!s_up_tex[k]) return NULL;
        s_up_w[k] = w; s_up_h[k] = h;
    }
    return upload_texture(s_up_tex[k], 0, 0, w, h, pixels, w * h * 4) ? s_up_tex[k] : NULL;
}

static uint32_t *scratch_pixels(size_t count)
{
    if (count > s_up_cap) {
        free(s_up_pixels);
        s_up_pixels = (uint32_t *)malloc(count * 4);
        s_up_cap = s_up_pixels ? count : 0;
    }
    return s_up_pixels;
}

static const uint8_t *surface_guest_rows(const Surface *s, uint32_t *pitch)
{
    size_t bytes;
    *pitch = s->pitch;
    if (!s->swizzled) return guest(s->va);
    *pitch = s->w * s->bpp;
    bytes = (size_t)*pitch * s->h;
    if (bytes > s_swz_cap) {
        free(s_swz_rows);
        s_swz_rows = (uint8_t *)malloc(bytes);
        s_swz_cap = s_swz_rows ? bytes : 0;
    }
    if (!s_swz_rows) return NULL;
    xbox_unswizzle_rect(s_swz_rows, guest(s->va), s->w, s->h, s->bpp);
    return s_swz_rows;
}

static uint8_t *s_sync_rows;
static size_t s_sync_cap;

/* CPU writes to a surface's guest memory, merged over what the GPU drew (see
 * surface_sync_impl in nv2a_d3d11.c). */
static void surface_sync_impl(Surface *s, int force)
{
    uint32_t rowb = s->w * s->bpp, y, x, x0 = 0xFFFFFFFFu, x1 = 0, y0 = 0xFFFFFFFFu, y1 = 0;
    uint32_t pitch;
    size_t bytes = (size_t)rowb * s->h, count = (size_t)s->w * s->h;
    uint32_t *px, *mask;
    const uint8_t *g;
    SDL_GPUTexture *img, *msk = NULL;
    float span0, span1;
    if (!s->bpp || !guest_ok(s->va, (size_t)s->pitch * s->h))
        return;
    if (!force && s->synced_frame == s_epoch)
        return;
    s->synced_frame = s_epoch - 1;
    g = surface_guest_rows(s, &pitch);
    if (!g) return;
    if (s->shadow) {
        for (y = 0; y < s->h; y++)
            if (memcmp(g + (size_t)y * pitch, s->shadow + (size_t)y * rowb, rowb))
                break;
        if (y == s->h) { s->synced_frame = s_epoch; return; }
    }
    if (bytes > s_sync_cap) {
        uint8_t *rows = (uint8_t *)realloc(s_sync_rows, bytes);
        if (!rows) return;
        s_sync_rows = rows;
        s_sync_cap = bytes;
    }
    for (y = 0; y < s->h; y++)
        memcpy(s_sync_rows + (size_t)y * rowb, g + (size_t)y * pitch, rowb);
    if (s->shadow) {
        if (s_mask_cap < count) {
            uint32_t *pixels = (uint32_t *)realloc(s_mask_pixels, count * 4);
            if (!pixels) return;
            s_mask_pixels = pixels;
            s_mask_cap = count;
        }
        memset(s_mask_pixels, 0, count * 4);
        for (y = 0; y < s->h; y++) {
            const uint8_t *row = s_sync_rows + (size_t)y * rowb;
            const uint8_t *sh = s->shadow + (size_t)y * rowb;
            if (!memcmp(row, sh, rowb)) continue;
            if (y0 == 0xFFFFFFFFu) y0 = y;
            y1 = y;
            mask = s_mask_pixels + (size_t)y * s->w;
            for (x = 0; x < s->w; x++)
                if (memcmp(row + x * s->bpp, sh + x * s->bpp, s->bpp)) {
                    mask[x] = 0xFFFFFFFFu;
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                }
        }
        if (y0 == 0xFFFFFFFFu) { s->synced_frame = s_epoch; return; }
        if (getenv("RECOMP_D3D_TRACE"))
            fprintf(stderr, "[SDLGPU] CPU wrote surface %08X (frame %u) %u,%u-%u,%u\n",
                    s->phys, s_frame, x0, y0, x1, y1);
    }
    px = scratch_pixels(count);
    if (!px) return;
    surface_rows_to_bgra(s, s_sync_rows, rowb, px);
    img = upload_slot(0, px, s->w, s->h);
    if (!img) return;
    if (s->shadow) {
        msk = upload_slot(1, s_mask_pixels, s->w, s->h);
        if (!msk) return;
    }
    guest_span(s, &span0, &span1);
    if (!blit_span(img, msk, s->tex, &s->clear, s->iw, s->ih, -1.0f, span0, span1, 0, 1))
        return;
    {
        uint8_t *old_shadow = s->shadow;
        s->shadow = s_sync_rows;
        s_sync_rows = old_shadow;
        s_sync_cap = old_shadow ? bytes : 0;
    }
    s->synced_frame = s_epoch;
    s->gen++;
}

static void surface_sync(Surface *s, int force)
{
    int64_t t0 = s_prof_on > 0 ? qpc() : 0;
    surface_sync_impl(s, force);
    if (s_prof_on > 0) s_prof[PROF_SYNC] += qpc() - t0;
}

static void writeback_drop_surface(Surface *s);

static void surface_release(Surface *s)
{
    writeback_drop_surface(s);
    if (s_cur_rt == s) s_cur_rt = NULL;
    release_texture(&s->tex);
    release_texture(&s->y16);
    release_texture(&s->mip);
    release_texture(&s->chain);
    free(s->shadow);
    memset(s, 0, sizeof *s);
}

static uint32_t s_surf_seq;

static Surface *surface_find(uint32_t p)
{
    Surface *best = NULL;
    int i;
    for (i = 0; i < MAX_SURFACES; i++)
        if (s_surf[i].tex && s_surf[i].phys == p && (!best || s_surf[i].seq > best->seq))
            best = &s_surf[i];
    return best;
}

static Surface *surface_get(uint32_t va, uint32_t pitch, uint32_t fmt,
                            uint32_t w, uint32_t h, int swizzled)
{
    uint32_t p = phys(va), bpp = surface_color_bpp(fmt);
    Surface *s = NULL, *victim = NULL;
    int i;
    for (i = 0; i < MAX_SURFACES; i++) {
        Surface *c = &s_surf[i];
        if (c->tex && c->phys == p && c->w >= w && c->h >= h && c->pitch == pitch &&
            c->bpp == bpp && c->swizzled == swizzled) {
            c->color_fmt = fmt;
            c->used_frame = s_frame;
            c->seq = ++s_surf_seq;
            return c;
        }
    }
    for (i = 0; i < MAX_SURFACES && !s; i++)
        if (s_surf[i].tex && s_surf[i].phys == p && s_surf[i].pitch == pitch &&
            s_surf[i].bpp == bpp && s_surf[i].swizzled == swizzled)
            s = &s_surf[i];
    if (s) {
        if (w < s->w && s->pitch == pitch) w = s->w;
        if (h < s->h && s->pitch == pitch) h = s->h;
        victim = s;
    } else {
        for (i = 0; i < MAX_SURFACES && !victim; i++)
            if (!s_surf[i].tex) victim = &s_surf[i];
        if (!victim)
            for (i = 0; i < MAX_SURFACES; i++)
                if (!victim || s_surf[i].used_frame < victim->used_frame)
                    victim = &s_surf[i];
        if (!victim) victim = &s_surf[0];
    }
    surface_release(victim);
    s = victim;
    s->phys = p; s->va = va; s->pitch = pitch; s->bpp = bpp;
    s->w = w; s->h = h; s->color_fmt = fmt; s->swizzled = swizzled;
    s->sy = s_scale;
    s->sx = (!swizzled && w == GUEST_W && h == GUEST_H) ? s_scale * widen() : s_scale;
    s->iw = (uint32_t)(w * s->sx + 0.5f);
    s->ih = (uint32_t)(h * s->sy + 0.5f);
    s->tex = make_texture(s->iw, s->ih, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, USAGE_TARGET);
    if (!s->tex) { memset(s, 0, sizeof *s); return NULL; }
    s->clear = 1;                   /* cleared to zero by its first pass */
    s->used_frame = s_frame;
    s->seq = ++s_surf_seq;
    s->gen++;
    surface_sync(s, 1);
    if (s->clear) {                 /* nothing drawn: still clear it now */
        target(s->tex, &s->clear, NULL, NULL);
    }
    if (getenv("RECOMP_D3D_TRACE"))
        fprintf(stderr, "[SDLGPU] surface %08X %ux%u pitch %u fmt %X%s\n", p, w, h,
                pitch, fmt & 0xF, swizzled ? " swizzled" : "");
    return s;
}

static void depth_release(Depth *d)
{
    if (s_cur_ds == d) s_cur_ds = NULL;
    release_texture(&d->y16);
    release_texture(&d->tex);
    memset(d, 0, sizeof *d);
}

static Depth *depth_find(uint32_t p)
{
    Depth *best = NULL;
    int i;
    for (i = 0; i < MAX_DEPTHS; i++)
        if (s_depth[i].tex && s_depth[i].phys == p && s_depth[i].zf != 1 &&
            (!best || s_depth[i].seq > best->seq))
            best = &s_depth[i];
    return best;
}

/* A Y16 texture has two texels per Z24S8 pixel. Prefer the newest
 * buffer covering the read over a smaller pass at the same address. */
static Depth *depth_find_texture(uint32_t p, uint32_t width, uint32_t height)
{
    Depth *best = NULL;
    int i;
    for (i = 0; i < MAX_DEPTHS; i++) {
        Depth *d = &s_depth[i];
        if (d->tex && d->phys == p && d->zf != 1 && 2 * d->w >= width && d->h >= height &&
            (!best || d->seq > best->seq))
            best = d;
    }
    return best ? best : depth_find(p);
}

static Depth *depth_get(uint32_t va, uint32_t iw, uint32_t ih)
{
    uint32_t p = phys(va);
    Depth *d, *victim = NULL;
    int i;
    for (i = 0; i < MAX_DEPTHS; i++) {
        d = &s_depth[i];
        if (d->tex && d->phys == p && d->iw == iw && d->ih == ih) {
            d->used_frame = s_frame;
            return d;
        }
    }
    for (i = 0; i < MAX_DEPTHS && !victim; i++)
        if (!s_depth[i].tex) victim = &s_depth[i];
    for (i = 0; i < MAX_DEPTHS; i++)
        if (!victim || s_depth[i].used_frame < victim->used_frame)
            victim = &s_depth[i];
    d = victim;
    depth_release(d);
    d->tex = make_texture(iw, ih, 1, 0, s_depth_format,
                          SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (!d->tex) return NULL;
    d->clear = 1;
    d->phys = p; d->iw = iw; d->ih = ih; d->used_frame = s_frame;
    return d;
}

/* The colour surface and depth buffer a draw or clear writes, made current. */
static int bind_targets(const NvD3DState *st, int want_depth, Surface **out,
                        Depth **dout, uint32_t *gw, uint32_t *gh)
{
    uint32_t fmt = st->surface_format, w, h;
    int swizzled = ((fmt >> 8) & 0xF) == 2;
    Surface *s = NULL;
    Depth *d = NULL;
    uint32_t aa = (fmt >> 12) & 0xF;
    s_aa_x = aa ? 2 : 1;
    s_aa_y = aa == 2 ? 2 : 1;
    if (swizzled) {
        w = 1u << ((fmt >> 16) & 0xF);
        h = 1u << ((fmt >> 24) & 0xF);
        s_aa_x = s_aa_y = 1;
    } else {
        w = (st->clip_x + st->clip_w) * s_aa_x;
        h = (st->clip_y + st->clip_h) * s_aa_y;
    }
    if (!w || !h || w > 4096 || h > 4096)
        return 0;
    if (st->color_addr && surface_color_bpp(fmt) &&
        guest_ok(st->color_addr, (size_t)(swizzled ? w * surface_color_bpp(fmt) : st->color_pitch) * h)) {
        s = surface_get(st->color_addr, swizzled ? w * surface_color_bpp(fmt) : st->color_pitch,
                        fmt, w, h, swizzled);
        if (s) surface_sync(s, 0);
    }
    if (want_depth && st->zeta_addr && ((fmt >> 4) & 0xF)) {
        float wx = (w == GUEST_W && h == GUEST_H) ? s_scale * widen() : s_scale;
        uint32_t iw = s ? s->iw : (uint32_t)(w * wx + 0.5f);
        uint32_t ih = s ? s->ih : (uint32_t)(h * s_scale + 0.5f);
        d = depth_get(st->zeta_addr, iw, ih);
    }
    if (!s && !d)
        return 0;
    if (s && d && (d->iw != s->iw || d->ih != s->ih))
        d = NULL;
    if (s) s->gen++;
    if (d) { d->gen++; d->seq = ++s_surf_seq; d->zf = (fmt >> 4) & 0xF; d->w = w; d->h = h; }
    target(s ? s->tex : NULL, s ? &s->clear : NULL, d ? d->tex : NULL, d ? &d->clear : NULL);
    s_cur_rt = s;
    s_cur_ds = d;
    s_cur_sx = s ? s->sx : (float)d->iw / (float)w;
    s_cur_sy = s ? s->sy : (float)d->ih / (float)h;
    *out = s; *dout = d;
    *gw = (s ? s->w : w) / s_aa_x;
    *gh = (s ? s->h : h) / s_aa_y;
    return 1;
}

/* --------------------------------------------------------------- textures */

typedef struct TexEntry {
    uint32_t phys, format, pitch, w, h, palette, palette_len, used;
    uint64_t hash, pal_hash;
    uint32_t checked_epoch;
    size_t bytes;
    SDL_GPUTexture *tex;
    struct TexEntry *next;
} TexEntry;

#define TEX_BUCKETS 1024
static TexEntry *s_tex_bucket[TEX_BUCKETS];
static size_t s_tex_bytes, s_tex_count;
#define TEX_BUDGET ((size_t)768 << 20)

static void tex_free(TexEntry *e)
{
    release_texture(&e->tex);
    s_tex_bytes -= e->bytes;
    s_tex_count--;
    free(e);
}

static void tex_evict(void)
{
    while (s_tex_bytes > TEX_BUDGET) {
        TexEntry **best = NULL, **pp;
        int b;
        for (b = 0; b < TEX_BUCKETS; b++)
            for (pp = &s_tex_bucket[b]; *pp; pp = &(*pp)->next)
                if ((*pp)->used != s_frame && (!best || (*pp)->used < (*best)->used))
                    best = pp;
        if (!best) return;
        {
            TexEntry *e = *best;
            *best = e->next;
            tex_free(e);
        }
    }
}

/* Guest DXT blocks already have the BC1/2/3 layout (see nv2a_d3d11.c). */
static SDL_GPUTextureFormat texture_host_format(const NvD3DTexture *t, const TexLayout *L, int hl)
{
    static const SDL_GPUTextureFormat formats[3] = {
        SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM, SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM,
        SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM };
    static int support[2][3], checked[2][3];
    int index, cube = t->cube ? 1 : 0;
    if (tex_volume(t) || L->kind != K_DXT || hl != L->levels || (t->width & 3) || (t->height & 3))
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    index = t->color == 0x0C ? 0 : t->color == 0x0E ? 1 : 2;
    if (!checked[cube][index]) {
        support[cube][index] = SDL_GPUTextureSupportsFormat(s_dev, formats[index],
            cube ? SDL_GPU_TEXTURETYPE_CUBE : SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        checked[cube][index] = 1;
    }
    return support[cube][index] ? formats[index] : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
}

static SDL_GPUTexture *texture_get_impl(const NvD3DTexture *t, int *guest_levels)
{
    TexLayout L;
    uint32_t p = phys(t->addr), pal_p = t->palette;
    uint32_t key = (p >> 7) ^ (t->format * 2654435761u) ^ (t->pitch << 3) ^ t->width;
    TexEntry *e, **head = &s_tex_bucket[key & (TEX_BUCKETS - 1)];
    uint64_t h, ph = 0;
    uint32_t *pal = NULL, *px = NULL;
    SDL_GPUTextureFormat host_format;
    int hl, is_new, native;

    if (guest_levels) *guest_levels = 1;
    if (!tex_layout(t, &L)) return NULL;
    if (guest_levels) *guest_levels = L.levels;
    if (!guest_ok(t->addr, L.total)) {
        texture_refused(t, "span_outside_ram", L.total);
        return NULL;
    }
    if (t->color == 0x0B && (!pal_p || !guest_ok(pal_p, t->palette_len * 4))) {
        texture_refused(t, pal_p ? "palette_outside_ram" : "palette_missing", t->palette_len * 4);
        return NULL;
    }
    for (e = *head; e; e = e->next)
        if (e->phys == p && e->format == t->format && e->pitch == t->pitch &&
            e->w == t->width && e->h == t->height && e->palette == pal_p &&
            e->palette_len == t->palette_len)
            break;
    if (e && e->checked_epoch == s_epoch) {
        e->used = s_frame;
        return e->tex;
    }
    h = hash_mem(guest(t->addr), L.total);
    if (t->color == 0x0B && pal_p && guest_ok(pal_p, t->palette_len * 4)) {
        pal = (uint32_t *)guest(pal_p);
        ph = hash_mem((const uint8_t *)pal, t->palette_len * 4);
    }
    if (e && e->hash == h && e->pal_hash == ph) {
        e->checked_epoch = s_epoch;
        e->used = s_frame;
        return e->tex;
    }
    hl = host_levels(&L, t);
    host_format = texture_host_format(t, &L, hl);
    native = host_format != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    if (!native) {
        px = scratch_pixels((size_t)t->width * t->height * L.level_d[0]);
        if (!px) {
            if (e) e->used = s_frame;
            return e ? e->tex : NULL;
        }
    }
    is_new = !e;
    if (!e) {
        e = (TexEntry *)calloc(1, sizeof *e);
        if (!e) return NULL;
        e->tex = tex_volume(t) ? make_volume(t->width, t->height, L.level_d[0], (uint32_t)hl)
                              : make_texture(t->width, t->height, (uint32_t)hl, L.faces == 6, host_format,
                              SDL_GPU_TEXTUREUSAGE_SAMPLER);
        if (!e->tex) { free(e); return NULL; }
        e->phys = p; e->format = t->format; e->pitch = t->pitch;
        e->w = t->width; e->h = t->height; e->palette = pal_p;
        e->palette_len = t->palette_len;
        {
            uint32_t w = t->width, ht = t->height;
            int lv;
            for (lv = 0; lv < hl; lv++) {
                e->bytes += L.level_d[lv < L.levels ? lv : L.levels - 1] * (native ? (size_t)((w + 3) / 4) * ((ht + 3) / 4) * L.bpp * L.faces
                                   : (size_t)w * ht * 4 * L.faces);
                if (w > 1) w /= 2;
                if (ht > 1) ht /= 2;
            }
        }
    }
    {
        int f, lv;
        for (f = 0; f < L.faces; f++) {
            uint32_t w, hh;
            for (lv = 0; lv < L.levels; lv++) {
                const uint8_t *src = guest(t->addr) + f * L.face_stride + L.level_off[lv];
                if (native) {
                    uint32_t bytes = ((L.level_w[lv] + 3) / 4) * ((L.level_h[lv] + 3) / 4) * (uint32_t)L.bpp;
                    upload_texture(e->tex, (uint32_t)lv, (uint32_t)f, L.level_w[lv], L.level_h[lv], src, bytes);
                } else {
                    decode_level(t, &L, src, lv, pal, px);
                    upload_texture_extent(e->tex, (uint32_t)lv, (uint32_t)f, L.level_w[lv], L.level_h[lv], L.level_d[lv],
                                   px, L.level_w[lv] * L.level_h[lv] * L.level_d[lv] * 4);
                }
            }
            if (hl <= L.levels) continue;
            w = L.level_w[L.levels - 1]; hh = L.level_h[L.levels - 1];
            for (; lv < hl; lv++) {
                halve_bgra(px, w, hh, px);
                if (w > 1) w /= 2;
                if (hh > 1) hh /= 2;
                upload_texture(e->tex, (uint32_t)lv, (uint32_t)f, w, hh, px, w * hh * 4);
            }
        }
    }
    e->hash = h;
    e->pal_hash = ph;
    e->checked_epoch = s_epoch;
    e->used = s_frame;
    if (is_new) {
        e->next = *head;
        *head = e;
        s_tex_bytes += e->bytes;
        s_tex_count++;
    }
    tex_evict();
    return e->tex;
}

static SDL_GPUTexture *texture_get_with_levels(const NvD3DTexture *t, int *guest_levels)
{
    int64_t t0 = s_prof_on > 0 ? qpc() : 0;
    SDL_GPUTexture *v = texture_get_impl(t, guest_levels);
    if (s_prof_on > 0) s_prof[PROF_TEXTURE] += qpc() - t0;
    return v;
}

/* ------------------------------------------------------------------- draw */

/* Scratch copy for render-to-texture reads of the target being drawn. */
static SDL_GPUTexture *s_feedback_tex;
static uint32_t s_feedback_w, s_feedback_h;

static SDL_GPUTexture *feedback_copy(Surface *s)
{
    if (!s_feedback_tex || s_feedback_w != s->iw || s_feedback_h != s->ih) {
        release_texture(&s_feedback_tex);
        s_feedback_tex = make_texture(s->iw, s->ih, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                      SDL_GPU_TEXTUREUSAGE_SAMPLER);
        if (!s_feedback_tex) return NULL;
        s_feedback_w = s->iw; s_feedback_h = s->ih;
    }
    copy_texture(s_feedback_tex, 0, s->tex, 0, s->iw, s->ih);
    return s_feedback_tex;
}

static int wclip_scissored(const NvD3DState *st)
{
    return !(st->wclip_type & 1) && (st->wclip_mask & ~1u) == 0;
}

static SDL_Rect scissor_rect(const NvD3DState *st, uint32_t gw, uint32_t gh)
{
    SDL_Rect r;
    uint32_t x0 = st->clip_x, y0 = st->clip_y;
    uint32_t x1 = st->clip_x + st->clip_w, y1 = st->clip_y + st->clip_h;
    if (wclip_scissored(st) && (st->wclip_mask & 1)) {
        uint32_t h = st->wclip_h[0], v = st->wclip_v[0];
        if (x0 < (h & 0xFFF)) x0 = h & 0xFFF;
        if (y0 < (v & 0xFFF)) y0 = v & 0xFFF;
        if (x1 > (h >> 16 & 0xFFF) + 1) x1 = (h >> 16 & 0xFFF) + 1;
        if (y1 > (v >> 16 & 0xFFF) + 1) y1 = (v >> 16 & 0xFFF) + 1;
    }
    if (x1 > gw) x1 = gw;
    if (y1 > gh) y1 = gh;
    if (x0 > x1) x0 = x1;
    if (y0 > y1) y0 = y1;
    r.x = (int)(x0 * s_aa_x * s_cur_sx + 0.5f);
    r.y = (int)(y0 * s_aa_y * s_cur_sy + 0.5f);
    r.w = (int)(x1 * s_aa_x * s_cur_sx + 0.5f) - r.x;
    r.h = (int)(y1 * s_aa_y * s_cur_sy + 0.5f) - r.y;
    return r;
}

static uint32_t write_mask(const NvD3DState *st, Surface *s)
{
    uint32_t m = 0, cm = st->color_mask;
    if (cm & 0x00010000u) m |= SDL_GPU_COLORCOMPONENT_R;
    if (cm & 0x00000100u) m |= SDL_GPU_COLORCOMPONENT_G;
    if (cm & 0x00000001u) m |= SDL_GPU_COLORCOMPONENT_B;
    if ((cm & 0x01000000u) && (!s || surface_alpha_mode(s->color_fmt) < 0))
        m |= SDL_GPU_COLORCOMPONENT_A;
    return m;
}

static int s_draw_textured, s_draw_reads_wide;
static uint32_t s_draw_tex_addr;
static int s_draw_bspline;

extern const uint32_t (*nv2a_vsh_program(uint32_t *start, uint32_t *version))[4];
extern const float (*nv2a_vsh_constants(uint32_t *version))[4];

static int draw_forces_depth(const NvD3DState *st)
{
    Surface *source;
    if (!s_overlay.force_depth_test) return 0;
    source = surface_find(phys(st->tex[0].addr));
    return s_overlay.force_depth_test(st, source ? source->w : 0, source ? source->h : 0);
}

/* The levels of a swizzled render target found as one chain (see
 * surface_chain in nv2a_d3d11.c). */
static SDL_GPUTexture *surface_chain(Surface *rt, const NvD3DTexture *t, int *levels)
{
    Surface *lv[16];
    uint32_t off = 0, k, n = 0, bpp = rt->bpp;
    for (k = 0; k < t->levels && k < 16; k++) {
        uint32_t w = t->width >> k, h = t->height >> k;
        Surface *c;
        if (!w) w = 1;
        if (!h) h = 1;
        c = k ? surface_find(phys(t->addr) + off) : rt;
        if (!c || !c->swizzled || c->w != w || c->h != h || c->bpp != bpp || c == s_cur_rt)
            break;
        lv[n++] = c;
        off += w * h * bpp;
    }
    if (n < 2) return NULL;
    if (!rt->chain || rt->chain_levels != (int)n) {
        release_texture(&rt->chain);
        rt->chain = make_texture(rt->iw, rt->ih, n, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                 SDL_GPU_TEXTUREUSAGE_SAMPLER);
        if (!rt->chain) return NULL;
        rt->chain_levels = (int)n;
        for (k = 0; k < n; k++) rt->chain_gen[k] = lv[k]->gen - 1;
    }
    for (k = 0; k < n; k++) {
        uint32_t dw = rt->iw >> k, dh = rt->ih >> k;
        if (!dw) dw = 1;
        if (!dh) dh = 1;
        surface_sync(lv[k], 0);
        if (rt->chain_gen[k] == lv[k]->gen) continue;
        copy_texture(rt->chain, k, lv[k]->tex, 0, lv[k]->iw < dw ? lv[k]->iw : dw,
                     lv[k]->ih < dh ? lv[k]->ih : dh);
        rt->chain_gen[k] = lv[k]->gen;
    }
    *levels = (int)n;
    return rt->chain;
}

/* A mip-mapped copy of a surface for reductions (see surface_mips there). */
static SDL_GPUTexture *surface_mips(Surface *s)
{
    if (!s->mip) {
        uint32_t levels = 1, m = s->iw > s->ih ? s->iw : s->ih;
        while (m > 1) { m >>= 1; levels++; }
        s->mip = make_texture(s->iw, s->ih, levels, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, USAGE_TARGET);
        if (!s->mip) return NULL;
        s->mip_gen = s->gen - 1;
    }
    if (s->mip_gen != s->gen) {
        copy_texture(s->mip, 0, s->tex, 0, s->iw, s->ih);
        copy_end();
        if (cmd()) SDL_GenerateMipmapsForGPUTexture(s_cmd, s->mip);
        s->mip_gen = s->gen;
    }
    return s->mip;
}

/* A 32-bit surface or a depth buffer read as LU_IMAGE_Y16 (see y16_ps). */
static void y16_draw(SDL_GPUTexture *dst, int *dst_clear, uint32_t w, uint32_t h, int shader,
                     SDL_GPUTexture *src, const float *cc)
{
    PipeKey k;
    SDL_GPUGraphicsPipeline *p;
    SDL_GPUTextureSamplerBinding b = { src, s_point_clamp };
    Op *o;
    pipe_key_plain(&k, SH_Y16_VS, (uint8_t)shader, FMT_RGBA16, SDL_GPU_PRIMITIVETYPE_TRIANGLELIST);
    if (!(p = pipeline(&k))) return;
    target(dst, dst_clear, NULL, NULL);
    if (!(o = op_add(OP_PIPE))) return;
    o->pipe = p;
    if (cc) rec_uniform(1, 0, cc, 32);
    rec_samplers(&b, 1);
    if (!(o = op_add(OP_DRAW))) return;
    o->a = 3;
    o->vp.w = (float)w; o->vp.h = (float)h; o->vp.max_depth = 1;
    o->sc.w = (int)w; o->sc.h = (int)h;
    s_cur_rt = NULL;
    s_cur_ds = NULL;
}

static SDL_GPUTexture *surface_y16(Surface *s)
{
    if (!s->y16) {
        s->y16 = make_texture(2 * s->iw, s->ih, 1, 0, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM, USAGE_TARGET);
        if (!s->y16) return NULL;
        s->y16_clear = 1;
        s->y16_gen = s->gen - 1;
    }
    if (s->y16_gen != s->gen) {
        y16_draw(s->y16, &s->y16_clear, 2 * s->iw, s->ih, SH_Y16_PS, s->tex, NULL);
        s->y16_gen = s->gen;
    }
    return s->y16;
}

static SDL_GPUTexture *depth_y16(Depth *d)
{
#ifndef XBOXRECOMP_DEPTH_VIEW_SHADER
    static int noted;
    (void)d;
    if (!noted) { noted=1; fprintf(stderr,"[NV2A] depth texture conversion unavailable; supply a port shader\n"); }
    return NULL;
#else
    if (!d->y16) {
        d->y16 = make_texture(2 * d->iw, d->ih, 1, 0, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM, USAGE_TARGET);
        if (!d->y16) return NULL;
        d->y16_clear = 1;
        d->y16_gen = d->gen - 1;
    }
    if (d->y16_gen != d->gen) {
        float cc[8] = { 0 };
        /* Host texels per guest pixel, for a port-owned conversion shader. */
        cc[2] = d->w ? (float)d->iw / (float)d->w : 0.0f;
        y16_draw(d->y16, &d->y16_clear, 2 * d->iw, d->ih, SH_Y16D_PS, d->tex, cc);
        d->y16_gen = d->gen;
    }
    return d->y16;
#endif
}

typedef struct {
    float tex_border[4][4];
    uint32_t tex_border_uv[4];
} PSBorder;

/* The dynamic state setup_pipeline chose, for the draw that follows it. */
static struct {
    SDL_GPUViewport vp;
    SDL_Rect sc;
    SDL_FColor blend;
    uint8_t ref;
} s_last_draw_state;

static Op *draw_op(OpKind kind, uint32_t count, uint32_t first)
{
    Op *o = op_add(kind);
    if (!o) return NULL;
    o->a = count;
    o->b = first;
    o->vp = s_last_draw_state.vp;
    o->sc = s_last_draw_state.sc;
    o->blend = s_last_draw_state.blend;
    o->stencil_ref = s_last_draw_state.ref;
    return o;
}

/* Everything a draw needs apart from its vertices, recorded into the batch
 * (see setup_pipeline in nv2a_d3d11.c, which this follows step for step). */
static int setup_pipeline(const NvD3DState *st, int topology, Surface **out_s,
                          uint32_t *out_gw, uint32_t *out_gh, uint32_t *out_zmax, int indexed)
{
    Surface *s;
    Depth *d;
    Depth *texture_depth[4] = { 0 };
    uint32_t gw, gh, n, zf;
    PSConsts pc;
    PSBorder pb;
    SDL_GPUTexture *srv[13] = { 0 };
    void *smp[4] = { 0 };
    int cull = 0, bias = 0, flare_depth = draw_forces_depth(st), wdepth;
    float slope = 0, bf[4];
    PipeKey k;
    SDL_GPUGraphicsPipeline *pipe;
    Op *o;
    (void)indexed;
    s_draw_textured = s_draw_reads_wide = 0;
    s_draw_tex_addr = 0;
    s_draw_bspline = 0;

    {
        static int init;
        static long skip_cmask = -1, skip_sf = -1;
        if (!init) {
            const char *e;
            init = 1;
            if ((e = getenv("RECOMP_D3D_SKIP_CMASK")) != NULL) skip_cmask = strtol(e, NULL, 16);
            if ((e = getenv("RECOMP_D3D_SKIP_SFACTOR")) != NULL) skip_sf = strtol(e, NULL, 16);
        }
        if ((skip_cmask >= 0 && st->color_mask == (uint32_t)skip_cmask) ||
            (skip_sf >= 0 && st->blend_enable && st->blend_sfactor == (uint32_t)skip_sf))
            return 0;
    }
    if (st->cull_enable)
        cull = st->cull_face == 0x404 ? 1 : st->cull_face == 0x405 ? 2 : 3;
    if (cull == 3 && topology == NV_D3D_TRIANGLES)
        return 0;
    for (n = 0; n < 4; n++) {
        const NvD3DTexture *t = &st->tex[n];
        Surface *rt;
        Depth *zt;
        if (t->color != 0x35 || t->cube || !t->addr) continue;
        if ((zt = depth_find_texture(phys(t->addr), t->width, t->height)) != NULL) {
            if (!depth_y16(zt)) return 0;
            /* bind_targets changes recency; sample the view converted here. */
            texture_depth[n] = zt;
            continue;
        }
        rt = surface_find(phys(t->addr));
        if (rt && rt->bpp == 4) { surface_sync(rt, 0); surface_y16(rt); }
    }
    if (!bind_targets(st, st->depth_test || st->stencil_test || flare_depth, &s, &d, &gw, &gh))
        return 0;
    zf = (st->surface_format >> 4) & 0xF;
    if (d && st->depth_test && st->depth_mask)
        d->depth_mode = st->z_perspective ? 2 : 1;
    /* A flare disables depth itself; forcing a test must retain the scene
     * buffer's Z/W encoding even if its native draw changed CONTROL0. */
    wdepth = flare_depth && d && d->depth_mode ? d->depth_mode == 2 : st->z_perspective;
    if (s && write_mask(st, s)) s->gpu_dirty = 1;

    memset(&pc, 0, sizeof pc);
    memset(&pb, 0, sizeof pb);
    for (n = 0; n < 4; n++) {
        const NvD3DTexture *t = &st->tex[n];
        uint32_t mode = (st->rc.stage_program >> (n * 5)) & 31;
        SDL_GPUTexture *view = NULL;
        Surface *rt;
        Depth *zt;
        int kind = K_SWZ, levels = 1, reduce = 0;
        pc.tex_scale[n][0] = pc.tex_scale[n][1] = 1.0f;
        pc.tex_scale[n][2] = pc.tex_scale[n][3] = 1.0f;
        if (!st->rc_seen && n == 0)
            mode = (st->tex_used & 1) ? 1 : 0;
        if (!mode_needs_texture(mode) || !t->addr || !t->width || !t->height)
            continue;
        tex_format(t->color, &kind);
        rt = !tex_volume(t) && !t->cube && kind != K_DXT && t->color != 0x0B ? surface_find(phys(t->addr)) : NULL;
        s_draw_textured = 1;
        if (!s_draw_tex_addr) s_draw_tex_addr = t->addr;
        if (rt && rt->sx > rt->sy * 1.001f) s_draw_reads_wide = 1;
        zt = texture_depth[n];
        if (zt && zt->y16) {
            view = zt->y16;
            pc.tex_scale[n][0] = 1.0f / (float)(2 * zt->w);
            pc.tex_scale[n][1] = 1.0f / (float)zt->h;
        } else if (rt) {
            surface_sync(rt, 0);
            view = rt == s ? feedback_copy(rt) : rt->tex;
            if (t->color == 0x35 && rt->bpp == 4 && rt->y16) {
                view = rt->y16;
                pc.tex_scale[n][0] = 1.0f / (float)(2 * rt->w);
                pc.tex_scale[n][1] = 1.0f / (float)rt->h;
            } else if (kind == K_LIN) {
                pc.tex_scale[n][0] = 1.0f / (float)rt->w;
                pc.tex_scale[n][1] = 1.0f / (float)rt->h;
                if (s && s != rt && s->iw < rt->iw && s->ih < rt->ih) {
                    SDL_GPUTexture *m = surface_mips(rt);
                    if (m) { view = m; reduce = 1; }
                }
            } else {
                pc.tex_scale[n][0] = (float)t->width / (float)rt->w;
                pc.tex_scale[n][1] = (float)t->height / (float)rt->h;
                if (t->levels > 1 && rt->swizzled && rt != s_cur_rt) {
                    SDL_GPUTexture *c = surface_chain(rt, t, &levels);
                    if (c) view = c;
                }
            }
        } else {
            view = texture_get_with_levels(t, &levels);
            if (kind == K_LIN) {
                pc.tex_scale[n][0] = 1.0f / (float)t->width;
                pc.tex_scale[n][1] = 1.0f / (float)t->height;
            }
        }
        if (kind == K_LIN) {
            pc.tex_scale[n][2] = pc.tex_scale[n][0];
            pc.tex_scale[n][3] = pc.tex_scale[n][1];
        } else {
            pc.tex_scale[n][2] = pc.tex_scale[n][3] = 1.0f;
        }
        if (!view) continue;
        if (tex_volume(t)) srv[9 + n] = view;
        else if (t->cube && !rt) srv[4 + n] = view; else srv[n] = view;
        smp[n] = sampler(t, levels, reduce, smp);
        if (n == 0 && !t->cube && !tex_volume(t) && s_overlay.cubic_filter_0 &&
            s_overlay.cubic_filter_0(st, rt != NULL, (uint32_t)levels))
            s_draw_bspline = 1;
        pc.tex_info[n][0] = (t->control0 >> 2) & 1;
        pc.tex_info[n][1] = (t->filter >> 28) | (tex_volume(t) ? 256u : 0u);
        if (rt && kind == K_LIN && t->color != 0x35 && (rt->iw > rt->w || rt->ih > rt->h)) {
            unsigned mag = (t->filter >> 24) & 15u;
            unsigned min = (t->filter >> 16) & 255u;
            if (mag == 1 && min != 2 && min != 4 && min != 6) pc.tex_info[n][2] = 1;
            /* Constant linear clamp taps need the same guest-sized footprint. */
            if (mag == 2 && (t->address & 0xF) == 3 && ((t->address >> 8) & 0xF) == 3)
                pc.tex_info[n][2] = 2;
        }
        memcpy(pc.bump_mat[n], t->bump_mat, sizeof pc.bump_mat[n]);
        pc.bump_lum[n][0] = t->bump_scale;
        pc.bump_lum[n][1] = t->bump_offset;
        /* Border addressing, done in the shader here (PSB). */
        pb.tex_border_uv[n] = ((t->address & 0xF) == 4 ? 1u : 0u) |
                              (((t->address >> 8) & 0xF) == 4 ? 2u : 0u) |
                              (((t->address >> 16) & 0xF) == 4 ? 4u : 0u);
        unpack4(t->border, pb.tex_border[n]);
    }
    for (n = 0; n < 4; n++)
        if (!smp[n]) smp[n] = s_linear_clamp;

    /* A sampled surface's CPU upload or a copy ended this draw's batch;
     * make its targets current again. */
    if (!(s_b.open && s_b.color == (s ? s->tex : NULL) && s_b.depth == (d ? d->tex : NULL)) &&
        !bind_targets(st, st->depth_test || st->stencil_test || flare_depth, &s, &d, &gw, &gh))
        return 0;

    for (n = 0; n < 8; n++) {
        pc.c_icw[n] = st->rc.color_icw[n];
        pc.a_icw[n] = st->rc.alpha_icw[n];
        pc.c_ocw[n] = st->rc.color_ocw[n];
        pc.a_ocw[n] = st->rc.alpha_ocw[n];
        unpack4(st->rc.factor0[n], pc.cf0[n]);
        unpack4(st->rc.factor1[n], pc.cf1[n]);
    }
    unpack4(st->rc.final_c0, pc.fin_c0);
    unpack4(st->rc.final_c1, pc.fin_c1);
    pc.fin0 = st->rc.final0;
    pc.fin1 = st->rc.final1;
    pc.rc_control = st->rc.control;
    pc.stage_prog = st->rc_seen ? st->rc.stage_program : (st->tex_used & 1);
    pc.alpha_func = st->alpha_func;
    pc.alpha_ref = st->alpha_ref;
    pc.ps_flags = (st->rc_seen ? 1u : 0u) | (st->alpha_test ? 2u : 0u) | (s_draw_bspline ? 4u : 0u);
    pc.clip_plane = st->clip_plane_mode;
    pc.other_input = st->other_stage_input;
    pc.dot_map = st->dot_rgb_mapping;
    pc.fog_color[0] = (st->fog_color & 0xFF) / 255.0f;
    pc.fog_color[1] = ((st->fog_color >> 8) & 0xFF) / 255.0f;
    pc.fog_color[2] = ((st->fog_color >> 16) & 0xFF) / 255.0f;
    pc.fog_color[3] = (st->fog_color >> 24) / 255.0f;
    if (wdepth && d) {
        uint32_t zm = zf == 1 ? 0xFFFFu : 0xFFFFFFu;
        if (st->poly_offset_fill) {
            pc.zp0[0] = st->poly_offset_factor;
            pc.zp0[1] = st->poly_offset_units;
        }
        pc.zp0[2] = s_cur_sx;
        pc.zp0[3] = s_cur_sy;
        pc.zp1[0] = 1.0f / ((float)zm + 1.0f);
        pc.zp1[1] = (float)zm;
    }
    if (!wclip_scissored(st)) {
        int w;
        pc.wclip_mode = st->wclip_type & 1 ? 2u : 1u;
        for (w = 0; w < 8; w++) {
            uint32_t h = st->wclip_h[w], v = st->wclip_v[w];
            if (!(st->wclip_mask >> w & 1)) continue;
            pc.wclip[w][0] = (float)(h & 0xFFF) * s_aa_x * s_cur_sx;
            pc.wclip[w][1] = (float)(v & 0xFFF) * s_aa_y * s_cur_sy;
            pc.wclip[w][2] = (float)((h >> 16 & 0xFFF) + 1) * s_aa_x * s_cur_sx;
            pc.wclip[w][3] = (float)((v >> 16 & 0xFFF) + 1) * s_aa_y * s_cur_sy;
        }
    }
    /* Texture reads can end the batch. */
    if (!(s_b.open && s_b.color == (s ? s->tex : NULL) && s_b.depth == (d ? d->tex : NULL)) &&
        !bind_targets(st, st->depth_test || st->stencil_test || flare_depth, &s, &d, &gw, &gh))
        return 0;

    if (st->poly_offset_fill && !(wdepth && d)) {
        bias = (int)st->poly_offset_units;
        slope = st->poly_offset_factor * s_cur_sy;
    }
    memset(&k, 0, sizeof k);
    {
        uint32_t v = (st->flat_shade ? 1u : 0u) | (wdepth && d ? 2u : 0u);
        static const uint8_t ps[4] = { SH_PS_MAIN, SH_PS_MAIN_FLAT, SH_PS_WDEPTH, SH_PS_WDEPTH_FLAT };
        k.vs = SH_UB_VS;
        k.ps = ps[v];
    }
    k.color = s ? FMT_BGRA8 : FMT_NONE;
    k.depth = d ? 1 : 0;
    k.prim = (uint8_t)(topology == NV_D3D_LINES ? SDL_GPU_PRIMITIVETYPE_LINELIST :
                       topology == NV_D3D_POINTS ? SDL_GPU_PRIMITIVETYPE_POINTLIST :
                       SDL_GPU_PRIMITIVETYPE_TRIANGLELIST);
    k.blend_enable = st->blend_enable ? 1 : 0;
    k.sf = (uint16_t)st->blend_sfactor;
    k.df = (uint16_t)st->blend_dfactor;
    k.eq = (uint16_t)st->blend_equation;
    k.write_mask = (uint8_t)(s ? write_mask(st, s) : 0);
    k.cull = (uint8_t)(cull == 3 ? 0 : cull);
    k.front_ccw = st->front_face == 0x901;
    k.bias = bias;
    k.slope = slope;
    if (d) {
        k.zt = (st->depth_test || flare_depth) ? 1 : 0;
        k.zfunc = (uint8_t)((flare_depth ? 0x203u : st->depth_func) & 7);
        k.zw = flare_depth ? 0 : (st->depth_mask ? 1 : 0);
        k.st = st->stencil_test ? 1 : 0;
        k.sfunc = (uint8_t)(st->stencil_func & 7);
        k.rmask = (uint8_t)st->stencil_func_mask;
        k.wmask = (uint8_t)st->stencil_write_mask;
        k.sfail = sop_code(st->stencil_fail);
        k.szfail = sop_code(st->stencil_zfail);
        k.szpass = sop_code(st->stencil_zpass);
    } else {
        k.zfunc = 7; k.sfunc = 7;
    }
    if (!(pipe = pipeline(&k))) return 0;
    if ((st->blend_sfactor | st->blend_dfactor) & 0x8000) {
        float c[4];
        unpack4(st->blend_color, c);
        if (st->blend_sfactor == 0x8003 || st->blend_sfactor == 0x8004 ||
            st->blend_dfactor == 0x8003 || st->blend_dfactor == 0x8004)
            bf[0] = bf[1] = bf[2] = bf[3] = c[3];
        else
            memcpy(bf, c, sizeof bf);
    } else {
        bf[0] = bf[1] = bf[2] = bf[3] = 0;
    }
    if (!(o = op_add(OP_PIPE))) return 0;
    o->pipe = pipe;
    o->a = 1;
    rec_uniform(1, 1, &pc, sizeof pc);
    rec_uniform(1, 2, &pb, sizeof pb);
    {
        SDL_GPUTextureSamplerBinding b[13];
        for (n = 0; n < 4; n++) {
            b[n].texture = srv[n] ? srv[n] : s_dummy_2d;
            b[n].sampler = (SDL_GPUSampler *)smp[n];
            b[4 + n].texture = srv[4 + n] ? srv[4 + n] : s_dummy_cube;
            b[4 + n].sampler = (SDL_GPUSampler *)smp[n];
            b[9 + n].texture = srv[9 + n] ? srv[9 + n] : s_dummy_volume;
            b[9 + n].sampler = (SDL_GPUSampler *)smp[n];
        }
        b[8].texture = s_dummy_2d;  /* reserved slot */
        b[8].sampler = s_linear_clamp;
        rec_samplers(b, 13);
    }
    s_last_draw_state.vp.w = (float)(s ? s->iw : d->iw);
    s_last_draw_state.vp.h = (float)(s ? s->ih : d->ih);
    s_last_draw_state.vp.max_depth = 1;
    s_last_draw_state.sc = scissor_rect(st, gw, gh);
    s_last_draw_state.blend.r = bf[0]; s_last_draw_state.blend.g = bf[1];
    s_last_draw_state.blend.b = bf[2]; s_last_draw_state.blend.a = bf[3];
    s_last_draw_state.ref = (uint8_t)(st->stencil_ref & 0xFF);
    *out_s = s;
    *out_gw = gw;
    *out_gh = gh;
    *out_zmax = zf == 1 ? 0xFFFFu : 0xFFFFFFu;
    return 1;
}

#define NV_VSH_SLOTS 136

int nv2a_d3d_widescreen(void)
{
    return widen() > 1.001f;
}

/* Last presentation rectangle, normalized to client coordinates. */
static float s_present_rect[4];
static SDL_SpinLock s_present_lock;

void nv2a_d3d_get_present_rect(float present[4])
{
    SDL_LockSpinlock(&s_present_lock);
    memcpy(present, s_present_rect, sizeof s_present_rect);
    SDL_UnlockSpinlock(&s_present_lock);
}

/* ------------------------------------------------ z-pass pixel counting */

/* NV097_SET_ZPASS_PIXEL_COUNT_ENABLE / CLEAR_REPORT_VALUE / GET_REPORT.
 * SDL3 GPU has no occlusion queries: each draw made while counting is also
 * drawn, before itself, into a float target of the same size with additive
 * blending (depth and stencil tested, nothing written to them); the count is
 * that target's sum, read back when the title asks for the report. */
static int s_zpass_on;
static uint64_t s_zpass;
static SDL_GPUTexture *s_zcount;
static uint32_t s_zcount_w, s_zcount_h;
static int s_zcount_clear, s_zcount_used;

static void zpass_collect(void);

void nv2a_d3d_zpass_enable(int on)
{
    if (!nv2a_d3d_init()) return;
    zpass_collect();
    s_zpass_on = on != 0;
}

void nv2a_d3d_zpass_clear(void)
{
    zpass_collect();
    s_zpass = 0;
}

uint32_t nv2a_d3d_zpass_read(void)
{
    double px;
    zpass_collect();
    /* In guest pixels: the host target has scale^2 as many. */
    px = (double)s_zpass / ((double)s_cur_sx * s_cur_sy);
    return px > 4294967295.0 ? 0xFFFFFFFFu : (uint32_t)px;
}

/* ------------------------------------------- vertex processing on the GPU */

static uint32_t s_zero_block[4];

void nv2a_d3d_draw_ub(const NvD3DState *st, int topology, const NvD3DAttrib attr[16],
                      const float def[16][4], int program, const uint32_t *idx, uint32_t ni,
                      uint32_t lo, uint32_t hi, const float (*pos)[4])
{
    static float *dx;
    static uint32_t dx_cap;
    static uint32_t zero_prog[NV_VSH_SLOTS][4];
    static float zero_consts[192][4];
    struct { const uint8_t *base; uint32_t stride, span; } grp[16];
    uint32_t ngrp = 0, i, count = hi - lo + 1, gw, gh, zmax, gof[16];
    UbConsts uc;
    Surface *s;
    uint8_t *p;
    uint32_t total = 0, at = 0, ib_bytes = ni * 4, need, first_index;
    float squeeze = 1.0f, shift = 0.0f, yscale = 1.0f;
    int use_dx = 0;
    int64_t t_draw;

    if (!nv2a_d3d_init() || !ni || hi < lo || ib_bytes > IB_BYTES)
        return;
    memset(&uc, 0, sizeof uc);
    memcpy(uc.def, def, sizeof uc.def);
    for (i = 0; i < 16; i++) {
        uint32_t g, bytes = attr_bytes(attr[i].type, attr[i].size);
        if (!attr[i].enabled) continue;
        uc.at[i][2] = attr[i].type;
        uc.at[i][3] = attr[i].size;
        if (!attr[i].data || !bytes) { uc.at[i][2] = 0xFF; continue; }
        for (g = 0; g < ngrp; g++)
            if (grp[g].stride == attr[i].stride && attr[i].data >= grp[g].base &&
                attr[i].data + bytes <= grp[g].base + (attr[i].stride ? attr[i].stride : 64))
                break;
        if (g == ngrp) {
            grp[g].base = attr[i].data;
            grp[g].stride = attr[i].stride;
            grp[g].span = 0;
            ngrp++;
        }
        if ((uint32_t)(attr[i].data - grp[g].base) + bytes > grp[g].span)
            grp[g].span = (uint32_t)(attr[i].data - grp[g].base) + bytes;
        gof[i] = g;
        uc.at[i][0] = (uint32_t)(attr[i].data - grp[g].base);
        uc.at[i][1] = attr[i].stride;
    }
    for (i = 0; i < ngrp; i++)
        total += (((grp[i].stride ? (count - 1) * grp[i].stride : 0) + grp[i].span) + 15) & ~15u;
    need = total + count * 4;
    if (need > VB_BYTES)
        return;
    /* Room for this draw's data in the current command buffer, or a new one. */
    if (need > VB_BYTES - s_vpos || ib_bytes > IB_BYTES - s_ipos)
        submit(NULL);

    t_draw = s_prof_on > 0 ? qpc() : 0;
    s_prof_draws++;
    if (!setup_pipeline(st, topology, &s, &gw, &gh, &zmax, 1))
        return;
    if (pos && s && s->sx > s->sy * 1.001f && s_overlay.place) {
        float xlo = 1e30f, xhi = -1e30f, ylo = 1e30f, yhi = -1e30f;
        uint32_t k;
        int flat = 1;
        for (k = 0; k < ni && flat; k++) {
            const float *v = pos[idx[k] - lo];
            if (v[3] != 1.0f) flat = 0;
            if (v[0] < xlo) xlo = v[0];
            if (v[0] > xhi) xhi = v[0];
            if (v[1] < ylo) ylo = v[1];
            if (v[1] > yhi) yhi = v[1];
        }
        if (flat) {
            NvOverlayDraw d;
            if (count > dx_cap) {
                float *nd = (float *)realloc(dx, (size_t)count * sizeof *nd);
                if (nd) { dx = nd; dx_cap = count; }
            }
            d.pos = pos; d.idx = idx; d.attrib = attr; d.ni = ni; d.lo = lo; d.count = count;
            d.per = topology == NV_D3D_TRIANGLES ? 3 : topology == NV_D3D_LINES ? 2 : 1;
            d.gw = (float)gw; d.gh = (float)gh; d.narrow = s->sy / s->sx;
            d.x0 = xlo; d.y0 = ylo; d.x1 = xhi; d.y1 = yhi;
            d.tex = s_draw_tex_addr; d.textured = s_draw_textured; d.reads_wide = s_draw_reads_wide;
            use_dx = s_overlay.place(&d, count <= dx_cap ? dx : NULL, &squeeze, &shift);
        }
    }

    /* setup_pipeline may have submitted (a full upload ring): recheck room. */
    if (need > VB_BYTES - s_vpos || ib_bytes > IB_BYTES - s_ipos)
        return;
    if (!(p = ring_reserve(s_vtb, &s_vmap, &s_vpos, &s_vcmd, VB_BYTES, total + (use_dx ? count * 4 : 0))))
        return;
    {
        uint32_t goff[16];
        for (i = 0; i < ngrp; i++) {
            uint32_t bytes = (grp[i].stride ? (count - 1) * grp[i].stride : 0) + grp[i].span;
            memcpy(p + at, grp[i].base + (size_t)lo * grp[i].stride, bytes);
            goff[i] = s_vpos + at;
            at += (bytes + 15) & ~15u;
        }
        for (i = 0; i < 16; i++)
            if (attr[i].enabled && uc.at[i][2] != 0xFF)
                uc.at[i][0] += goff[gof[i]];
        if (s_overlay.sanitize_vertex_upload)
            for (i = 0; i < 16; ++i)
                if (attr[i].enabled && attr[i].data && uc.at[i][2] != 0xFF)
                    s_overlay.sanitize_vertex_upload(p + uc.at[i][0] - s_vpos, &attr[i], i, lo, count);
    }
    uc.info[2] = 0xFFFFFFFFu;
    if (use_dx) {
        memcpy(p + at, dx, count * 4);
        uc.info[2] = s_vpos + at;
        at += count * 4;
    }
    if (!(p = ring_reserve(s_itb, &s_imap, &s_ipos, &s_icmd, IB_BYTES, ib_bytes)))
        return;
    copy_indices((uint32_t *)p, idx, ni, lo, topology, st->flat_shade);
    first_index = s_ipos / 4;

    uc.info[1] = program ? 1u : 0u;
    {
        uint32_t start = 0, pver, cver;
        const uint32_t (*prog)[4] = nv2a_vsh_program(&start, &pver);
        const float (*consts)[4] = nv2a_vsh_constants(&cver);
        if (program) {
            if (!prog || !consts) return;
            uc.info[0] = start;
        }
        /* Every slot the vertex shader declares gets data at least once per
         * command buffer; equal data is not pushed again (rec_uniform). */
        rec_uniform(0, 2, prog ? (const void *)prog : (const void *)zero_prog, NV_VSH_SLOTS * 16);
        rec_uniform(0, 3, consts ? (const void *)consts : (const void *)zero_consts, 192 * 16);
        rec_uniform(0, 1, s_zero_block, sizeof s_zero_block);
    }
    uc.map[0] = 2.0f / (float)gw;
    uc.map[1] = 2.0f / (float)gh;
    uc.map[2] = 1.0f / (float)zmax;
    uc.map[3] = squeeze;
    uc.fogi[0] = st->fog_enable;
    uc.fogi[1] = st->fog_mode;
    uc.fogf[0] = st->fog_param[0];
    uc.fogf[1] = st->fog_param[1];
    uc.place[0] = shift;
    uc.place[1] = yscale;
    vertex_alignment(&s_overlay, s ? s->w : 0, s ? s->h : 0, uc.place);
    rec_uniform(0, 0, &uc, sizeof uc);
    rec_uniform(1, 0, s_zero_block, sizeof s_zero_block);
    draw_op(OP_DRAWI, ni, first_index);
    s_vpos += (at + 255) & ~255u;
    s_ipos += (ib_bytes + 255) & ~255u;
    s_b.v1 = s_vpos;
    s_b.i1 = s_ipos;
    if (s_zpass_on && s) {
        /* Count the draw's passing pixels: the same draw again, before the
         * real one would be wrong (it has been recorded already), so the
         * count pass tests against depth as it stands after this draw only
         * when the draw does not write depth; that is how titles issue
         * visibility tests (depth writes off). */
        extern void nv2a_sdlgpu_zcount(const NvD3DState *st, Surface *s, uint32_t first_index,
                                       uint32_t ni, const UbConsts *uc);
        nv2a_sdlgpu_zcount(st, s, first_index, ni, &uc);
    }
    s_prof_verts += count;
    if (s_prof_on > 0) s_prof[PROF_DRAW] += qpc() - t_draw;
}

/* The count pass for one draw (see z-pass above): its vertex data is already
 * in this command buffer's rings, so only the pipeline and target differ. */
void nv2a_sdlgpu_zcount(const NvD3DState *st, Surface *s, uint32_t first_index,
                        uint32_t ni, const UbConsts *uc)
{
    PipeKey k;
    SDL_GPUGraphicsPipeline *pipe;
    Op *o;
    Depth *d = s_cur_ds;
    SDL_GPUTexture *depth = s_b.depth;
    int none = 0;
    (void)uc;
    if (!s_zcount || s_zcount_w != s->iw || s_zcount_h != s->ih) {
        release_texture(&s_zcount);
        s_zcount = make_texture(s->iw, s->ih, 1, 0, SDL_GPU_TEXTUREFORMAT_R32_FLOAT, USAGE_TARGET);
        if (!s_zcount) return;
        s_zcount_w = s->iw; s_zcount_h = s->ih;
        s_zcount_clear = 1;
    }
    /* The batch's draws so far, then the count target with the same depth. */
    target(s_zcount, &s_zcount_clear, depth, NULL);
    pipe_key_plain(&k, SH_UB_VS, SH_CLEAR_PS, FMT_R32F, SDL_GPU_PRIMITIVETYPE_TRIANGLELIST);
    k.depth = depth ? 1 : 0;
    k.blend_enable = 1;
    k.sf = 0x0001; k.df = 0x0001; k.eq = 0x8006;
    if (d && depth) {
        k.zt = st->depth_test ? 1 : 0;
        k.zfunc = (uint8_t)(st->depth_func & 7);
        k.st = st->stencil_test ? 1 : 0;
        k.sfunc = (uint8_t)(st->stencil_func & 7);
        k.rmask = (uint8_t)st->stencil_func_mask;
    }
    if (!(pipe = pipeline(&k))) return;
    if (!(o = op_add(OP_PIPE))) return;
    o->pipe = pipe;
    o->a = 1;
    {
        /* clear_ps returns ccol: one per pixel that passes. */
        float one[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
        rec_uniform(1, 0, one, sizeof one);
    }
    o = draw_op(OP_DRAWI, ni, first_index);
    s_zcount_used = 1;
    (void)none;
    /* Back to the real target for the draws that follow. */
    target(s->tex, &s->clear, s_cur_ds ? s_cur_ds->tex : NULL, s_cur_ds ? &s_cur_ds->clear : NULL);
}

static void zpass_collect(void)
{
    SDL_GPUTransferBuffer *tb;
    SDL_GPUFence *fence = NULL;
    SDL_GPUTextureRegion r;
    SDL_GPUTextureTransferInfo info;
    uint32_t levels = 1, m, lw, lh;
    SDL_GPUTexture *sum;
    if (!s_zcount_used) return;
    s_zcount_used = 0;
    /* Sum: mipmaps of the count target average it; the last level times the
     * pixel count is the total. */
    m = s_zcount_w > s_zcount_h ? s_zcount_w : s_zcount_h;
    while (m > 1) { m >>= 1; levels++; }
    sum = make_texture(s_zcount_w, s_zcount_h, levels, 0, SDL_GPU_TEXTUREFORMAT_R32_FLOAT, USAGE_TARGET);
    if (!sum) return;
    copy_texture(sum, 0, s_zcount, 0, s_zcount_w, s_zcount_h);
    copy_end();
    if (!cmd()) { SDL_ReleaseGPUTexture(s_dev, sum); return; }
    SDL_GenerateMipmapsForGPUTexture(s_cmd, sum);
    {
        SDL_GPUTransferBufferCreateInfo ci = { SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, 4, 0 };
        tb = SDL_CreateGPUTransferBuffer(s_dev, &ci);
    }
    if (!tb || !copy_begin()) { SDL_ReleaseGPUTexture(s_dev, sum); return; }
    memset(&r, 0, sizeof r);
    memset(&info, 0, sizeof info);
    r.texture = sum; r.mip_level = levels - 1; r.w = r.h = r.d = 1;
    info.transfer_buffer = tb;
    SDL_DownloadFromGPUTexture(s_copy, &r, &info);
    copy_end();
    submit(&fence);
    if (fence) {
        float avg = 0;
        const float *v;
        SDL_WaitForGPUFences(s_dev, true, &fence, 1);
        SDL_ReleaseGPUFence(s_dev, fence);
        v = (const float *)SDL_MapGPUTransferBuffer(s_dev, tb, false);
        if (v) { avg = *v; SDL_UnmapGPUTransferBuffer(s_dev, tb); }
        lw = s_zcount_w; lh = s_zcount_h;
        s_zpass += (uint64_t)(avg * (double)lw * (double)lh + 0.5);
    }
    SDL_ReleaseGPUTransferBuffer(s_dev, tb);
    SDL_ReleaseGPUTexture(s_dev, sum);
    s_zcount_clear = 1;          /* counts start again from zero */
}

/* ------------------------------------------------------------------ clear */

void nv2a_d3d_clear(const NvD3DState *st, uint32_t flags, uint32_t color,
                    uint32_t zstencil, uint32_t x0, uint32_t y0,
                    uint32_t x1, uint32_t y1)
{
    Surface *s;
    Depth *d;
    uint32_t gw, gh, zf, mask = 0;
    float cc[8];
    PipeKey k;
    SDL_GPUGraphicsPipeline *pipe;
    Op *o;
    int zclear = (flags & 1) != 0, sclear = (flags & 2) != 0;

    if (!nv2a_d3d_init())
        return;
    if (!bind_targets(st, zclear || sclear, &s, &d, &gw, &gh))
        return;
    if (s) {
        if (flags & 0x10) mask |= SDL_GPU_COLORCOMPONENT_R;
        if (flags & 0x20) mask |= SDL_GPU_COLORCOMPONENT_G;
        if (flags & 0x40) mask |= SDL_GPU_COLORCOMPONENT_B;
        if ((flags & 0x80) && surface_alpha_mode(s->color_fmt) < 0)
            mask |= SDL_GPU_COLORCOMPONENT_A;
    }
    if (!d) { zclear = sclear = 0; }
    if (!mask && !zclear && !sclear)
        return;
    if (s && mask) s->gpu_dirty = 1;
    zf = (st->surface_format >> 4) & 0xF;
    unpack4(color, cc);
    if (s && (s->color_fmt & 0xF) == 0x3) {
        cc[0] = (float)(((color >> 19) & 31) * 255 / 31) / 255.0f;
        cc[1] = (float)(((color >> 10) & 63) * 255 / 63) / 255.0f;
        cc[2] = (float)(((color >> 3) & 31) * 255 / 31) / 255.0f;
    }
    memset(cc + 4, 0, 16);
    cc[4] = zf == 1 ? (float)(zstencil & 0xFFFF) / 65535.0f
                    : (float)(zstencil >> 8) / 16777215.0f;
    if (x1 >= gw) x1 = gw - 1;
    if (y1 >= gh) y1 = gh - 1;
    if (x0 > x1 || y0 > y1)
        return;
    if (d && zclear) d->depth_mode = st->z_perspective ? 2 : 1;
    pipe_key_plain(&k, SH_CLEAR_VS, SH_CLEAR_PS, s ? FMT_BGRA8 : FMT_NONE,
                   SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP);
    k.write_mask = (uint8_t)mask;
    k.depth = d ? 1 : 0;
    k.zt = (uint8_t)zclear; k.zw = (uint8_t)zclear; k.zfunc = 7;
    k.st = (uint8_t)sclear; k.sfunc = 7; k.rmask = 0xFF; k.wmask = 0xFF;
    k.sfail = k.szfail = k.szpass = sop_code(0x1E01);
    if (!(pipe = pipeline(&k))) return;
    if (!(o = op_add(OP_PIPE))) return;
    o->pipe = pipe;
    rec_uniform(0, 0, cc, sizeof cc);
    rec_uniform(1, 0, cc, sizeof cc);
    s_last_draw_state.vp.x = s_last_draw_state.vp.y = 0;
    s_last_draw_state.vp.w = (float)(s ? s->iw : d->iw);
    s_last_draw_state.vp.h = (float)(s ? s->ih : d->ih);
    s_last_draw_state.vp.min_depth = 0;
    s_last_draw_state.vp.max_depth = 1;
    s_last_draw_state.sc.x = (int)(x0 * s_aa_x * s_cur_sx + 0.5f);
    s_last_draw_state.sc.y = (int)(y0 * s_aa_y * s_cur_sy + 0.5f);
    s_last_draw_state.sc.w = (int)((x1 + 1) * s_aa_x * s_cur_sx + 0.5f) - s_last_draw_state.sc.x;
    s_last_draw_state.sc.h = (int)((y1 + 1) * s_aa_y * s_cur_sy + 0.5f) - s_last_draw_state.sc.y;
    memset(&s_last_draw_state.blend, 0, sizeof s_last_draw_state.blend);
    s_last_draw_state.ref = (uint8_t)(zf == 1 ? 0 : (zstencil & 0xFF));
    draw_op(OP_DRAW, 4, 0);
}

/* ---------------------------------------------------- native host drawing */

int nv2a_d3d_host_draw(const NvD3DState *st,
                       int (*draw)(const NvHostTarget *, void *), void *user)
{
    Surface *s = NULL; Depth *d = NULL; uint32_t gw, gh;
    if (!st || !draw || !nv2a_d3d_init() || !bind_targets(st,1,&s,&d,&gw,&gh) ||
        !s || !d || !d->tex || s->iw != d->iw || s->ih != d->ih) return 0;
    /* Flush also applies any pending initial target clears. */
    flush_batch();
    copy_end();
    if (!cmd()) return 0;
    NvHostTarget target = {s_dev,s_cmd,s->tex,d->tex,s->iw,s->ih,
                           d->zf == 1 ? 16u : 24u,1};
    int result = draw(&target,user);
    memset(s_last_uni_len,0,sizeof s_last_uni_len);
    s_cur_rt = NULL; s_cur_ds = NULL;
    if (result) { s->gpu_dirty = 1; ++s->gen; }
    return result;
}

/* -------------------------------------------------------------- write-back */

/* What the GPU rendered goes back to guest memory when the title flips (see
 * nv2a_d3d11.c): downloads are recorded at the flip and fenced; the ack
 * thread's polls (nv2a_d3d_writeback_busy) copy them out once done. */
typedef struct {
    uint32_t w, h;
    int pending;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUTexture *down;
    int down_clear;
} WbSlot;
static WbSlot s_wb[MAX_SURFACES];
static int s_wb_pending;
static uint32_t s_wb_mask;
static SDL_GPUFence *s_wb_fence;

static void writeback_drop_surface(Surface *s)
{
    unsigned slot = (unsigned)(s - s_surf);
    s_wb[slot].pending = 0;
    s_wb_mask &= ~(1u << slot);
    s_wb_pending = s_wb_mask != 0;
}

static int wb_slot_ready(WbSlot *k, uint32_t w, uint32_t h, int need_down)
{
    if (k->tb && (k->w != w || k->h != h)) {
        SDL_ReleaseGPUTransferBuffer(s_dev, k->tb);
        release_texture(&k->down);
        memset(k, 0, sizeof *k);
    }
    if (!k->tb) {
        SDL_GPUTransferBufferCreateInfo ci = { SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, w * h * 4, 0 };
        if (!(k->tb = SDL_CreateGPUTransferBuffer(s_dev, &ci))) return 0;
        k->w = w; k->h = h;
    }
    if (need_down && !k->down) {
        k->down = make_texture(w, h, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, USAGE_TARGET);
        if (!k->down) return 0;
        k->down_clear = 1;
    }
    return 1;
}

static int wb_surface_ok(const Surface *s)
{
    return s->tex && !s->swizzled && (s->bpp == 2 || s->bpp == 4) &&
           surface_find(s->phys) == s && s->shadow && guest_ok(s->va, (size_t)s->pitch * s->h);
}

/* Read back what is ready; wait: block until all of it is. 1 when none left. */
static int writeback_finish(int wait)
{
    int i;
    if (!s_wb_pending) {
        /* Every copy was dropped while its fence was pending: release it. */
        if (s_wb_fence) {
            SDL_ReleaseGPUFence(s_dev, s_wb_fence);
            s_wb_fence = NULL;
        }
        return 1;
    }
    if (s_wb_fence) {
        if (!wait && !SDL_QueryGPUFence(s_dev, s_wb_fence))
            return 0;
        SDL_WaitForGPUFences(s_dev, true, &s_wb_fence, 1);
        SDL_ReleaseGPUFence(s_dev, s_wb_fence);
        s_wb_fence = NULL;
    }
    for (i = 0; i < MAX_SURFACES; i++) {
        Surface *s = &s_surf[i];
        WbSlot *k = &s_wb[i];
        const uint8_t *m;
        uint8_t *dst;
        uint32_t x, y, rowb;
        int amode, r565;
        if (!(s_wb_mask & (1u << i)))
            continue;
        if (!wb_surface_ok(s) || k->w != s->w || k->h != s->h) {
            k->pending = 0;
            s_wb_mask &= ~(1u << i);
            continue;
        }
        /* A busy map keeps this copy pending for the next poll. */
        if (!(m = (const uint8_t *)SDL_MapGPUTransferBuffer(s_dev, k->tb, false)))
            continue;
        k->pending = 0;
        s_wb_mask &= ~(1u << i);
        dst = (uint8_t *)guest(s->va);
        rowb = s->w * s->bpp;
        amode = surface_alpha_mode(s->color_fmt);
        r565 = (s->color_fmt & 0xF) == 0x3;
        for (y = 0; y < s->h; y++) {
            const uint32_t *src = (const uint32_t *)(m + (size_t)y * s->w * 4);
            uint8_t *row = dst + (size_t)y * s->pitch;
            /* Rows the CPU changed since the executor last looked win. */
            if (memcmp(row, s->shadow + (size_t)y * rowb, rowb))
                continue;
            if (s->bpp == 4) {
                if (amode < 0) {
                    memcpy(row, src, rowb);
                } else {
                    uint32_t a = amode ? 0xFF000000u : 0, *o = (uint32_t *)row;
                    for (x = 0; x < s->w; x++) o[x] = (src[x] & 0xFFFFFFu) | a;
                }
            } else {
                uint16_t *o = (uint16_t *)row;
                for (x = 0; x < s->w; x++) {
                    uint32_t c = src[x];
                    o[x] = r565 ? (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x1F))
                                : (uint16_t)((amode == 0 ? 0 : 0x8000) | ((c >> 9) & 0x7C00) |
                                             ((c >> 6) & 0x03E0) | ((c >> 3) & 0x1F));
                }
            }
            memcpy(s->shadow + (size_t)y * rowb, row, rowb);
        }
        SDL_UnmapGPUTransferBuffer(s_dev, k->tb);
    }
    s_wb_pending = s_wb_mask != 0;
    return !s_wb_pending;
}

int nv2a_d3d_writeback_busy(void)
{
    if (!s_wb_pending || writeback_finish(0))
        return 0;
    return 1;
}

static void writeback_surfaces(void)
{
    static int enabled = -1;
    int i;
    if (enabled < 0)
        enabled = !getenv("RECOMP_D3D_NO_WRITEBACK");
    if (!enabled)
        return;
    writeback_finish(1);
    for (i = 0; i < MAX_SURFACES; i++) {
        Surface *s = &s_surf[i];
        WbSlot *k = &s_wb[i];
        SDL_GPUTextureRegion r;
        SDL_GPUTextureTransferInfo info;
        int scaled;
        if (!s->gpu_dirty || !wb_surface_ok(s))
            continue;
        scaled = s->iw != s->w || s->ih != s->h;
        if (!wb_slot_ready(k, s->w, s->h, scaled))
            continue;
        if (scaled) {
            float x0, x1;
            guest_span(s, &x0, &x1);
            /* A failed downsample leaves the surface dirty for the next batch. */
            if (!blit_span(s->tex, NULL, k->down, &k->down_clear, s->w, s->h, -1.0f, 0, 1, x0, x1))
                continue;
        }
        if (!copy_begin()) continue;
        memset(&r, 0, sizeof r);
        memset(&info, 0, sizeof info);
        r.texture = scaled ? k->down : s->tex;
        r.w = s->w; r.h = s->h; r.d = 1;
        info.transfer_buffer = k->tb;
        SDL_DownloadFromGPUTexture(s_copy, &r, &info);
        s->gpu_dirty = 0;
        k->pending = 1;
        s_wb_mask |= 1u << i;
        s_wb_pending = 1;
    }
    copy_end();
    if (s_wb_pending)
        submit(&s_wb_fence);
}

/* ---------------------------------------------------------------- present */

static SDL_GPUTexture *s_ovl_tex;
static uint32_t s_ovl_w, s_ovl_h;
static uint32_t s_ovl_serial = 0xFFFFFFFFu;
static uint64_t s_last_present, s_last_flip;
static Surface *s_shown;

/* Read a texture back and write it as a 24-bit BMP. */
static void write_bmp(SDL_GPUTexture *tex, uint32_t w, uint32_t h, const char *path)
{
    SDL_GPUTransferBufferCreateInfo ci = { SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, w * h * 4, 0 };
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(s_dev, &ci);
    SDL_GPUTextureRegion r;
    SDL_GPUTextureTransferInfo info;
    SDL_GPUFence *fence = NULL;
    const uint8_t *m;
    FILE *f;
    if (!tb || !copy_begin()) { if (tb) SDL_ReleaseGPUTransferBuffer(s_dev, tb); return; }
    memset(&r, 0, sizeof r);
    memset(&info, 0, sizeof info);
    r.texture = tex; r.w = w; r.h = h; r.d = 1;
    info.transfer_buffer = tb;
    SDL_DownloadFromGPUTexture(s_copy, &r, &info);
    copy_end();
    submit(&fence);
    if (fence) {
        SDL_WaitForGPUFences(s_dev, true, &fence, 1);
        SDL_ReleaseGPUFence(s_dev, fence);
    }
    if ((m = (const uint8_t *)SDL_MapGPUTransferBuffer(s_dev, tb, false)) != NULL) {
        uint32_t row = (w * 3 + 3) & ~3u, img = row * h, tot = 54 + img, y, x;
        uint8_t hdr[54], *line = (uint8_t *)calloc(1, row);
        f = fopen(path, "wb");
        if (f && line) {
            memset(hdr, 0, sizeof hdr);
            hdr[0] = 'B'; hdr[1] = 'M';
            memcpy(hdr + 2, &tot, 4);
            hdr[10] = 54; hdr[14] = 40;
            memcpy(hdr + 18, &w, 4);
            memcpy(hdr + 22, &h, 4);
            hdr[26] = 1; hdr[28] = 24;
            memcpy(hdr + 34, &img, 4);
            fwrite(hdr, 1, 54, f);
            for (y = 0; y < h; y++) {
                const uint8_t *src = m + (size_t)(h - 1 - y) * w * 4;
                for (x = 0; x < w; x++) {
                    line[x * 3] = src[x * 4];
                    line[x * 3 + 1] = src[x * 4 + 1];
                    line[x * 3 + 2] = src[x * 4 + 2];
                }
                fwrite(line, 1, row, f);
            }
            fprintf(stderr, "[SDLGPU] captured %s\n", path);
        }
        if (f) fclose(f);
        free(line);
        SDL_UnmapGPUTransferBuffer(s_dev, tb);
    }
    SDL_ReleaseGPUTransferBuffer(s_dev, tb);
}

/* RECOMP_PRESENT_CAPTURE=<prefix>: create <prefix>.flag to capture the next
 * presented images (as many as the flag file's number, default 1). */
static void capture_image(SDL_GPUTexture *tex, uint32_t w, uint32_t h)
{
    static const char *prefix = (const char *)-1;
    static unsigned remaining, frame;
    char path[1024];
    FILE *f;
    if (prefix == (const char *)-1) prefix = getenv("RECOMP_PRESENT_CAPTURE");
    if (!prefix) return;
    if (!remaining) {
        int count = 0;
        snprintf(path, sizeof path, "%s.flag", prefix);
        f = fopen(path, "rb");
        if (!f) return;
        if (fscanf(f, "%d", &count) != 1 || count < 1) count = 1;
        fclose(f);
        remove(path);
        remaining = (unsigned)count;
    }
    snprintf(path, sizeof path, "%s-%03u.bmp", prefix, frame++);
    write_bmp(tex, w, h, path);
    remaining--;
}

/* Draw surface s (with the overlay in *pc) into a w x h target, letterboxed
 * to the display aspect when asked. */
static void composite(SDL_GPUTexture *dst, uint32_t w, uint32_t h, Surface *s,
                      PresentConsts *pc, int letterbox)
{
    float target_w = (float)w, target_h = (float)h, x0 = 0, y0 = 0;
    PipeKey k;
    SDL_GPUGraphicsPipeline *p;
    SDL_GPUTextureSamplerBinding b[4];
    int clear = 1;
    Op *o;
    if (letterbox) {
        target_h = target_w / nv2a_d3d_display_aspect();
        if (target_h > (float)h) {
            target_h = (float)h;
            target_w = target_h * nv2a_d3d_display_aspect();
        }
        x0 = ((float)w - target_w) * 0.5f;
        y0 = ((float)h - target_h) * 0.5f;
        SDL_LockSpinlock(&s_present_lock);
        s_present_rect[0] = x0 / w; s_present_rect[1] = y0 / h;
        s_present_rect[2] = target_w / w; s_present_rect[3] = target_h / h;
        SDL_UnlockSpinlock(&s_present_lock);
    }
    pc->dst_rect[0] = x0 / w * 2 - 1;
    pc->dst_rect[1] = 1 - y0 / h * 2;
    pc->dst_rect[2] = (x0 + target_w) / w * 2 - 1;
    pc->dst_rect[3] = 1 - (y0 + target_h) / h * 2;
    pc->src_uv[0] = pc->src_uv[1] = 0;
    pc->src_uv[2] = 1; pc->src_uv[3] = 1;
    pc->src_size[0] = (float)s->w; pc->src_size[1] = (float)s->h;
    pc->src_size[2] = (float)s->iw; pc->src_size[3] = (float)s->ih;
    pc->force_alpha[0] = 1.0f;
    if (pc->ovl_flags[0] && s->sx > s->sy * 1.001f) {
        pc->ovl_in[2] = s->sx / s->sy;
        pc->ovl_in[3] = (float)s->w * 0.5f;
    }
    pipe_key_plain(&k, SH_PRESENT_VS, SH_PRESENT_PS, FMT_BGRA8, SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP);
    if (!(p = pipeline(&k))) return;
    target(dst, &clear, NULL, NULL);
    s_b.clear_color.a = 1.0f;          /* black */
    if (!(o = op_add(OP_PIPE))) return;
    o->pipe = p;
    rec_uniform(0, 0, pc, sizeof *pc);
    rec_uniform(1, 0, pc, sizeof *pc);
    b[0].texture = s->tex; b[0].sampler = s_linear_clamp;
    b[1].texture = pc->ovl_flags[0] && s_ovl_tex ? s_ovl_tex : s_dummy_2d; b[1].sampler = s_point_clamp;
    b[2].texture = s->tex; b[2].sampler = s_point_clamp;
    b[3].texture = b[1].texture; b[3].sampler = s_linear_clamp;
    rec_samplers(b, 4);
    memset(&s_last_draw_state, 0, sizeof s_last_draw_state);
    s_last_draw_state.vp.w = (float)w; s_last_draw_state.vp.h = (float)h;
    s_last_draw_state.vp.max_depth = 1;
    s_last_draw_state.sc.w = (int)w; s_last_draw_state.sc.h = (int)h;
    draw_op(OP_DRAW, 4, 0);
    s_cur_rt = NULL;
    s_cur_ds = NULL;
}

/* The overlay, uploaded when a new frame arrived, as present constants. */
static void prepare_overlay(PresentConsts *pc)
{
    XboxOverlay ovl;
    if (!xbox_FramebufferOverlay(&ovl))
        return;
    if (ovl.serial != s_ovl_serial || !s_ovl_tex) {
        uint32_t *px = scratch_pixels((size_t)ovl.in_w * ovl.in_h), x, y;
        if (px) {
            for (y = 0; y < ovl.in_h; y++)
                for (x = 0; x + 1 < ovl.in_w; x += 2)
                    yuv_pair(ovl.yuy2 + (size_t)y * ovl.pitch + x * 2, 0,
                             px + (size_t)y * ovl.in_w + x);
            if (!s_ovl_tex || s_ovl_w != ovl.in_w || s_ovl_h != ovl.in_h) {
                release_texture(&s_ovl_tex);
                s_ovl_tex = make_texture(ovl.in_w, ovl.in_h, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
                                         SDL_GPU_TEXTUREUSAGE_SAMPLER);
                s_ovl_w = ovl.in_w; s_ovl_h = ovl.in_h;
            }
            if (s_ovl_tex)
                upload_texture(s_ovl_tex, 0, 0, ovl.in_w, ovl.in_h, px, ovl.in_w * ovl.in_h * 4);
        }
        s_ovl_serial = ovl.serial;
    }
    if (!s_ovl_tex)
        return;
    pc->ovl_flags[0] = 1;
    pc->ovl_flags[1] = ovl.color_key_enabled;
    pc->ovl_flags[2] = ovl.color_key;
    pc->ovl_out[0] = (float)ovl.out_x; pc->ovl_out[1] = (float)ovl.out_y;
    pc->ovl_out[2] = (float)ovl.out_w; pc->ovl_out[3] = (float)ovl.out_h;
    pc->ovl_walk[0] = ovl.start_s / 1048576.0f;
    pc->ovl_walk[1] = ovl.start_t / 1048576.0f;
    pc->ovl_walk[2] = ovl.ds_dx / 1048576.0f;
    pc->ovl_walk[3] = ovl.dt_dy / 1048576.0f;
    pc->ovl_in[0] = (float)ovl.in_w; pc->ovl_in[1] = (float)ovl.in_h;
    pc->ovl_in[2] = 1.0f;
}

/* RECOMP_CAPTURE_SURFACE: the composited guest image at internal resolution. */
static SDL_GPUTexture *s_cap_tex;
static uint32_t s_cap_w, s_cap_h;
/* The last image composited for the window, for nv2a_d3d_repeat_frame and
 * resizes: what each present blits to the swapchain. */
static SDL_GPUTexture *s_kept;
static uint32_t s_kept_w, s_kept_h;

typedef struct FlipPacing { int done; int64_t ticks; } FlipPacing;
static void pace_vblank(void);

static int pace_after_present(void)
{
    static int after = -1;
    if (after < 0) {
        const char *e = getenv("RECOMP_PACE_AFTER_PRESENT");
        after = e && atoi(e) != 0;
    }
    return after;
}

static void pace_flip(FlipPacing *pacing)
{
    int64_t begin;
    if (!pacing || pacing->done) return;
    pacing->done = 1;
    begin = qpc();
    pace_vblank();
    pacing->ticks = qpc() - begin;
    if (s_prof_on > 0) s_prof[PROF_PACE] += pacing->ticks;
}

/* The kept image to the window, with the host UI over it; 1 if presented. */
static int present_kept_image(FlipPacing *pacing)
{
    SDL_GPUTexture *sc = NULL;
    Uint32 sw = 0, sh = 0;
    int shown = 0;
    if (!s_window || !s_kept) return 0;
    copy_end();
    flush_batch();
    if (!cmd()) return 0;
    RECOMP_PROFILE_BEGIN("SDL GPU acquire swapchain");
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(s_cmd, s_window, &sc, &sw, &sh))
        fail("SDL_WaitAndAcquireGPUSwapchainTexture");
    RECOMP_PROFILE_END();
    if (sc) {
        SDL_GPUBlitInfo bi;
        memset(&bi, 0, sizeof bi);
        bi.source.texture = s_kept;
        bi.source.w = s_kept_w; bi.source.h = s_kept_h;
        bi.destination.texture = sc;
        bi.destination.w = sw; bi.destination.h = sh;
        bi.load_op = SDL_GPU_LOADOP_DONT_CARE;
        bi.filter = SDL_GPU_FILTER_LINEAR;
        SDL_BlitGPUTexture(s_cmd, &bi);
        xbox_FramebufferRenderHostOverlay(s_dev, s_cmd, sc);
        shown = 1;
    }
    frame_csv_init();
    if (pacing && !pace_after_present()) pace_flip(pacing);
    submit(NULL);
    if (shown && pacing && s_frame_csv_file) frame_csv_present(qpc());
    s_last_present = SDL_GetTicks();
    return shown;
}

static int keep_surface(Surface *s)
{
    PresentConsts pc;
    uint32_t w, h;
    if (!s)
        return 0;
    memset(&pc, 0, sizeof pc);
    prepare_overlay(&pc);
    if (getenv("RECOMP_CAPTURE_SURFACE")) {
        if (!s_cap_tex || s_cap_w != s->iw || s_cap_h != s->ih) {
            release_texture(&s_cap_tex);
            s_cap_tex = make_texture(s->iw, s->ih, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, USAGE_TARGET);
            s_cap_w = s->iw; s_cap_h = s->ih;
        }
        if (s_cap_tex) {
            PresentConsts cap = pc;
            composite(s_cap_tex, s->iw, s->ih, s, &cap, 0);
            capture_image(s_cap_tex, s->iw, s->ih);
        }
    }
    if (!s_window || !client_size(&w, &h)) {
        return 0;
    }
    if (!s_kept || s_kept_w != w || s_kept_h != h) {
        release_texture(&s_kept);
        s_kept = make_texture(w, h, 1, 0, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, USAGE_TARGET);
        s_kept_w = w; s_kept_h = h;
        if (!s_kept) return 0;
    }
    composite(s_kept, w, h, s, &pc, 1);
    if (!getenv("RECOMP_CAPTURE_SURFACE"))
        capture_image(s_kept, w, h);
    return 1;
}

static void present_surface(Surface *s, FlipPacing *pacing)
{
    s_last_present = SDL_GetTicks();
    if (!keep_surface(s)) {
        if (s && pacing) pace_flip(pacing);
        return;
    }
    if (present_kept_image(pacing))
        s->cpu_present_pending = 0;
}

/* The flip completes at the display's next vertical blank (see pace_vblank
 * in nv2a_d3d11.c): RECOMP_FPS_LIMIT=N sets the rate (default 60, 0 =
 * unpaced). The wait is SDL_DelayPrecise, then a short spin. */
static void pace_vblank(void)
{
    static int limit = -1;
    static int64_t next;
    int64_t now, freq = qpf();
    extern int xbox_FramebufferPaused(void);
    while (xbox_FramebufferPaused()) {
        SDL_Delay(50);
        fbw_sdl_pump();
        next = 0;
    }
    if (limit < 0) {
        limit = recomp_frame_limit_from_env();
    }
    if (limit <= 0)
        return;
    now = qpc();
    if (!nv_pacing_deadline(&next, now, freq, limit))
        return;
    {
        int64_t spin = freq / 1000, remain = next - now;
        if (remain > spin)
            SDL_DelayPrecise((Uint64)((double)(remain - spin) * 1e9 / (double)freq));
        while (qpc() < next) SDL_CPUPauseInstruction();
    }
}

/* Internal resolution and aspect follow the window (see display_config in
 * nv2a_d3d11.c). */
#define RESIZE_SETTLE_MS 300
static void display_config(int now)
{
    static uint32_t seen_w, seen_h, done_w, done_h;
    static uint64_t seen_at;
    const char *e = getenv("RECOMP_RENDER_SCALE");
    float scale, aspect = env_aspect();
    uint32_t w, h;
    int i, deferred;
    if (!client_size(&w, &h)) {
        if (!now) return;
        w = (uint32_t)(GUEST_H * nv2a_d3d_display_aspect()); h = GUEST_H;
    }
    if (!now) {
        if (w != seen_w || h != seen_h) { seen_w = w; seen_h = h; seen_at = SDL_GetTicks(); return; }
        if ((w == done_w && h == done_h) || SDL_GetTicks() - seen_at < RESIZE_SETTLE_MS) return;
    }
    if (!aspect) aspect = clamp_aspect((float)w / (float)h);
    deferred = !now && s_aspect && fabsf(aspect - s_aspect) > 1e-4f &&
               s_overlay.hold_aspect && s_overlay.hold_aspect();
    if (deferred) aspect = s_aspect;
    if (e && atof(e) >= 0.5 && atof(e) <= 8.0) {
        scale = (float)atof(e);
    } else {
        const char *ss = getenv("RECOMP_SUPERSAMPLE");
        int n = ss ? atoi(ss) : 2;
        float fit_h = (float)h < (float)w / aspect ? (float)h : (float)w / aspect;
        if (n < 1) n = 1;
        if (n > 4) n = 4;
        scale = fit_h / (float)GUEST_H * (float)n;
        if (scale < 1.0f) scale = 1.0f;
        if (scale > 8.0f) scale = 8.0f;
    }
    if (!deferred) { done_w = w; done_h = h; }
    if (!now && fabsf(scale - s_scale) < 1e-3f && fabsf(aspect - s_aspect) < 1e-4f)
        return;
    if (!now) {
        flush_batch();
        for (i = 0; i < MAX_SURFACES; i++)
            if (s_surf[i].tex) surface_release(&s_surf[i]);
        for (i = 0; i < MAX_DEPTHS; i++)
            depth_release(&s_depth[i]);
        s_shown = NULL;
    }
    s_scale = scale;
    s_aspect = aspect;
    fprintf(stderr, "[SDLGPU] rendering %ux%u (window %ux%u, aspect %.3f)\n",
            (uint32_t)(GUEST_W * s_scale * widen() + 0.5f), (uint32_t)(GUEST_H * s_scale + 0.5f),
            w, h, aspect);
    fflush(stderr);
}

static Surface *scan_surface(uint32_t va, uint32_t pitch)
{
    /* The size the title gave AvSetDisplayMode, when that scanout has this
     * flip's pitch. Otherwise a 640x480 display is guessed from the pitch, and
     * each distinct address (first 16) is logged so a wrong guess shows up. */
    static uint32_t guessed[16];
    static unsigned guessed_n;
    uint32_t bpp, w, h, mpitch;
    Surface *s = surface_find(phys(va));
    if (!(xbox_GetDisplayMode(&mpitch, &bpp, &w, &h) && mpitch == pitch)) {
        bpp = pitch >= 640 * 4 ? 4 : 2;
        w = pitch / bpp; h = 480;
        if (w > 640) w = 640;
        if (!s) {
            unsigned i;
            for (i = 0; i < guessed_n && guessed[i] != va; i++) ;
            if (i == guessed_n && guessed_n < 16) {
                guessed[guessed_n++] = va;
                fprintf(stderr, "[SCAN_GUESS] addr=%08X phys=%08X pitch=%u guessed=%ux%u bpp=%u (no surface at address)\n",
                        va, phys(va), pitch, w, h, bpp);
                fflush(stderr);
            }
        }
    }
    if (!s || s->pitch != pitch)
        s = surface_get(va, pitch, bpp == 4 ? 0x5 : 0x3, w, h, 0);
    if (s) surface_sync(s, 1);
    return s;
}

static SDL_AtomicInt s_manual_present;
void nv2a_d3d_manual_present(int enabled)
{
    SDL_SetAtomicInt(&s_manual_present, enabled != 0);
}

void nv2a_d3d_flip_present(uint32_t surface_addr, uint32_t pitch, int show)
{
    Surface *s;
    FlipPacing pacing = { 0, 0 };
    show = show && !SDL_GetAtomicInt(&s_manual_present);
    if (!nv2a_d3d_init())
        return;
    fbw_sdl_pump();
    if (s_overlay.frame) s_overlay.frame();
    s = surface_find(phys(surface_addr));
    if (!s && pitch)
        s = scan_surface(surface_addr, pitch);
    s_shown = s;
    s_last_flip = SDL_GetTicks();
    if (s_overlay.idle_present) s_overlay.idle_present((uint32_t)s_last_flip, 0, 0);
    {
        int64_t t0 = s_prof_on > 0 ? qpc() : 0, t1;
        writeback_surfaces();
        t1 = s_prof_on > 0 ? qpc() : 0;
        if (show) present_surface(s, &pacing);
        else if (SDL_GetAtomicInt(&s_manual_present) && keep_surface(s)) {
            /* Freeze this completed frame before the guest reuses its target. */
            flush_batch();
            submit(NULL);
        }
        if (s_prof_on > 0) {
            s_prof[PROF_WRITEBACK] += t1 - t0;
            s_prof[PROF_PRESENT] += qpc() - t1 - pacing.ticks;
        }
    }
    if (show) {
        pace_flip(&pacing);
        frame_csv(pacing.ticks);
    }
    RECOMP_PROFILE_FRAME();
    prof_report();
    display_config(0);
    s_frame++;
}

void nv2a_d3d_flip(uint32_t surface_addr, uint32_t pitch)
{
    nv2a_d3d_flip_present(surface_addr, pitch, 1);
}

/* The title's thread asks for a frame by raising a flag and waits for this thread to lower it; it
 * sleeps on a semaphore the presenter posts when it is done (see nv2a_d3d11.c: yielding in a loop kept
 * a core busy). A stale post only makes one wait return early, and the loop rechecks the flag; the short
 * timeout is a safety net. The semaphore is made by the title's thread before it raises its flag. */
static SDL_Semaphore *s_request_done;

static void request_raise(SDL_AtomicInt *flag)
{
    if (!s_request_done)
        s_request_done = SDL_CreateSemaphore(0);
    SDL_SetAtomicInt(flag, 1);
}

static void request_wait(SDL_AtomicInt *flag)
{
    while (SDL_GetAtomicInt(flag)) {
        if (s_request_done)
            SDL_WaitSemaphoreTimeout(s_request_done, 10);
        else
            SDL_Delay(0);
    }
}

static void request_complete(SDL_AtomicInt *flag)
{
    SDL_SetAtomicInt(flag, 0);
    if (s_request_done)
        SDL_SignalSemaphore(s_request_done);
}

/* The last flipped frame shown again, paced like a flip (see nv2a_d3d11.c). */
static SDL_AtomicInt s_repeat;

void nv2a_d3d_repeat_frame(void)
{
    if (!s_shown)
        return;
    request_raise(&s_repeat);
    request_wait(&s_repeat);
}

void nv2a_d3d_tick(void)
{
    uint64_t now;
    uint32_t va, pitch;
    if (s_init_state <= 0 && !nv2a_d3d_init())
        return;
    fbw_sdl_pump();
    if (SDL_GetAtomicInt(&s_repeat)) {
        FlipPacing pacing = { SDL_GetAtomicInt(&s_manual_present) != 0, 0 };
        if (!present_kept_image(&pacing))
            present_surface(s_shown, &pacing);
        pace_flip(&pacing);
        frame_csv(pacing.ticks);
        s_frame++;
        request_complete(&s_repeat);
        return;
    }
    now = SDL_GetTicks();
    if (now - s_last_present < 16)
        return;
    XboxOverlay ovl;
    int movie = xbox_FramebufferOverlay(&ovl);
    int redraw = s_overlay.idle_present && s_overlay.idle_present(
        (uint32_t)now, s_last_flip ? (uint32_t)(now - s_last_flip) : 0, movie);
    if (xbox_FramebufferScanSource(&va, &pitch)) {
        Surface *s = scan_surface(va, pitch);
        present_surface(s, NULL);
        s_frame++;
        return;
    }
    if (s_shown && now - s_last_flip > 50) {
        uint32_t generation = s_shown->gen;
        if (!s_wb_pending)
            surface_sync(s_shown, 1);
        if (s_shown->gen != generation)
            s_shown->cpu_present_pending = 1;
        if (s_shown->cpu_present_pending || movie ||
            (redraw && now - s_last_present >= 33))
            present_surface(s_shown, NULL);
        else
            s_last_present = now;
    }
}
