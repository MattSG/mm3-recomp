/**
 * Partial clears for IDirect3DDevice8::Clear.
 *
 * D3D8 clears the viewport, or the rectangles given inside it. A render pass
 * can only clear a whole attachment, so a clear that does not cover the whole
 * target is a draw instead: a fullscreen triangle, scissored to the rectangle,
 * that writes the colour, depth (SV_Depth) and stencil (replace op) the caller
 * asked for. Whole-target clears still use the pass's own clear operations.
 */
#include "d3d8_internal.h"
#include <stdio.h>
#include <string.h>

static const char k_vs[] =
    "float4 main(uint id : SV_VertexID) : SV_Position {\n"
    "    return float4(id == 2 ? 3 : -1, id == 1 ? 3 : -1, 0, 1); }\n";

/* x = depth for the pass that writes depth. */
static const char k_ps_color[] =
    "cbuffer C : register(b0, space3) { float4 color; float4 depth; };\n"
    "float4 main() : SV_Target { return color; }\n";

static const char k_ps_depth[] =
    "cbuffer C : register(b0, space3) { float4 color; float4 depth; };\n"
    "struct Out { float4 c : SV_Target; float d : SV_Depth; };\n"
    "Out main() { Out o; o.c = color; o.d = depth.x; return o; }\n";

#define CLEAR_PIPES 16

typedef struct ClearPipe {
    Uint32                   color_fmt, depth_fmt;
    Uint8                    do_color, do_depth, do_stencil;
    SDL_GPUGraphicsPipeline *pipe;
} ClearPipe;

static XGpuShader g_vs, g_ps_color, g_ps_depth;
static ClearPipe  g_pipes[CLEAR_PIPES];
static int        g_pipe_count;

void d3d8_clear_shutdown(void)
{
    XGpu *g = d3d8_GetGpu();
    int i;
    if (g && g->device) {
        for (i = 0; i < g_pipe_count; i++)
            SDL_ReleaseGPUGraphicsPipeline(g->device, g_pipes[i].pipe);
        xgpu_release_shader(g->device, &g_vs);
        xgpu_release_shader(g->device, &g_ps_color);
        xgpu_release_shader(g->device, &g_ps_depth);
    }
    g_pipe_count = 0;
    memset(g_pipes, 0, sizeof(g_pipes));
}

static int ensure_shaders(XGpu *g)
{
    if (g_vs.shader)
        return 1;
    if (!xgpu_compile_hlsl_ex(g->device, k_vs, "main", 0, "clear_vs", &g_vs) ||
        !xgpu_compile_hlsl_ex(g->device, k_ps_color, "main", 1, "clear_ps", &g_ps_color) ||
        !xgpu_compile_hlsl_ex(g->device, k_ps_depth, "main", 1, "clear_ps_depth", &g_ps_depth)) {
        d3d8_clear_shutdown();
        return 0;
    }
    return 1;
}

static SDL_GPUGraphicsPipeline *get_pipe(XGpu *g, SDL_GPUTextureFormat cf, SDL_GPUTextureFormat df,
                                         int do_color, int do_depth, int do_stencil)
{
    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_GPUColorTargetDescription ct;
    ClearPipe *e;
    int i, has_depth = (df != SDL_GPU_TEXTUREFORMAT_INVALID);

    for (i = 0; i < g_pipe_count; i++) {
        e = &g_pipes[i];
        if (e->color_fmt == (Uint32)cf && e->depth_fmt == (Uint32)df &&
            e->do_color == do_color && e->do_depth == do_depth && e->do_stencil == do_stencil)
            return e->pipe;
    }
    if (g_pipe_count >= CLEAR_PIPES)
        return NULL;

    SDL_zero(pi); SDL_zero(ct);
    pi.vertex_shader = g_vs.shader;
    pi.fragment_shader = has_depth ? g_ps_depth.shader : g_ps_color.shader;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.enable_depth_clip = false;     /* a clear value is not geometry */

    ct.format = cf;
    ct.blend_state.color_write_mask = do_color ? 0xF : 0;
    ct.blend_state.enable_color_write_mask = true;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    if (has_depth) {
        pi.target_info.depth_stencil_format = df;
        pi.target_info.has_depth_stencil_target = true;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        pi.depth_stencil_state.enable_depth_write = do_depth != 0;
        if (do_stencil) {
            pi.depth_stencil_state.enable_stencil_test = true;
            pi.depth_stencil_state.compare_mask = 0xFF;
            pi.depth_stencil_state.write_mask = 0xFF;
            pi.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
            pi.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_REPLACE;
            pi.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_REPLACE;
            pi.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_REPLACE;
            pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;
        }
    }

    e = &g_pipes[g_pipe_count];
    e->pipe = SDL_CreateGPUGraphicsPipeline(g->device, &pi);
    if (!e->pipe) {
        fprintf(stderr, "D3D8: clear pipeline failed: %s\n", SDL_GetError());
        return NULL;
    }
    e->color_fmt = (Uint32)cf; e->depth_fmt = (Uint32)df;
    e->do_color = (Uint8)do_color; e->do_depth = (Uint8)do_depth; e->do_stencil = (Uint8)do_stencil;
    g_pipe_count++;
    return e->pipe;
}

/* Clears `r` (target coordinates, already clipped to the target) in the
 * render pass for `t`. Returns 0 if the clear could not be drawn. */
int d3d8_clear_rect(XGpuTargets t, SDL_GPUTextureFormat cf, SDL_GPUTextureFormat df,
                    Uint32 target_w, Uint32 target_h, const SDL_Rect *r,
                    int do_color, const float rgba[4], int do_depth, float z,
                    int do_stencil, Uint8 stencil)
{
    XGpu *g = d3d8_GetGpu();
    SDL_GPURenderPass *rp;
    SDL_GPUGraphicsPipeline *pipe;
    SDL_GPUViewport vp;
    SDL_Rect full;
    float u[8];
    int has_depth = (df != SDL_GPU_TEXTUREFORMAT_INVALID);

    if (!has_depth) { do_depth = 0; do_stencil = 0; }
    if (!do_color && !do_depth && !do_stencil)
        return 1;
    if (!ensure_shaders(g))
        return 0;
    pipe = get_pipe(g, cf, df, do_color, do_depth, do_stencil);
    if (!pipe)
        return 0;
    rp = xgpu_render_pass(g, t, NULL);          /* loads what is there */
    if (!rp)
        return 0;

    SDL_BindGPUGraphicsPipeline(rp, pipe);
    vp.x = 0; vp.y = 0; vp.w = (float)target_w; vp.h = (float)target_h;
    vp.min_depth = 0.0f; vp.max_depth = 1.0f;
    SDL_SetGPUViewport(rp, &vp);
    SDL_SetGPUScissor(rp, r);
    if (do_stencil)
        SDL_SetGPUStencilReference(rp, stencil);
    u[0] = rgba[0]; u[1] = rgba[1]; u[2] = rgba[2]; u[3] = rgba[3];
    u[4] = z; u[5] = u[6] = u[7] = 0.0f;
    SDL_PushGPUFragmentUniformData(g->cmd, 0, u, sizeof(u));
    SDL_DrawGPUPrimitives(rp, 3, 1, 0, 0);

    full.x = 0; full.y = 0; full.w = (int)target_w; full.h = (int)target_h;
    SDL_SetGPUScissor(rp, &full);                /* later draws are not scissored */
    return 1;
}
