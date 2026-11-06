/* Gamma is a display transform, not a modification to guest render targets.
 *
 * It is applied while the backbuffer is copied to the window: a fullscreen
 * pass reads the backbuffer through the ramp and writes the swapchain image.
 * The backbuffer itself is never touched, so there is nothing to restore if
 * presentation fails. An identity ramp costs nothing (the plain blit runs). */
#include "d3d8_internal.h"
#include <stdio.h>
#include <string.h>

static D3DGAMMARAMP g_ramp;
static BOOL g_initialized, g_enabled;

static XGpuShader              g_vs, g_ps;
static SDL_GPUGraphicsPipeline *g_pipe;
static SDL_GPUSampler          *g_sampler;
static SDL_GPUTextureFormat     g_pipe_format = SDL_GPU_TEXTUREFORMAT_INVALID;

static const char k_gamma_vs[] =
    "float4 main(uint id : SV_VertexID) : SV_Position {\n"
    "    return float4(id == 2 ? 3 : -1, id == 1 ? 3 : -1, 0, 1); }\n";

/* ramp[v] holds the corrected value for channel value v (0..255) in .rgb. */
static const char k_gamma_ps[] =
    "Texture2D pixels : register(t0, space2);\n"
    "SamplerState samp : register(s0, space2);\n"
    "cbuffer Gamma : register(b0, space3) { float4 ramp[256]; float4 inv_size; };\n"
    "float4 main(float4 p : SV_Position) : SV_Target {\n"
    "    float4 c = pixels.Sample(samp, p.xy * inv_size.xy);\n"
    "    uint3 i = (uint3)(saturate(c.rgb) * 255.0 + 0.5);\n"
    "    return float4(ramp[i.r].r, ramp[i.g].g, ramp[i.b].b, c.a); }\n";

static void gamma_release_resources(void)
{
    XGpu *g = d3d8_GetGpu();
    if (g && g->device) {
        if (g_pipe) SDL_ReleaseGPUGraphicsPipeline(g->device, g_pipe);
        if (g_sampler) SDL_ReleaseGPUSampler(g->device, g_sampler);
        xgpu_release_shader(g->device, &g_vs);
        xgpu_release_shader(g->device, &g_ps);
    }
    g_pipe = NULL;
    g_sampler = NULL;
    memset(&g_vs, 0, sizeof(g_vs));
    memset(&g_ps, 0, sizeof(g_ps));
    g_pipe_format = SDL_GPU_TEXTUREFORMAT_INVALID;
}

void d3d8_gamma_shutdown(void)
{
    gamma_release_resources();
    g_initialized = g_enabled = FALSE;
}

void d3d8_gamma_get(D3DGAMMARAMP *ramp)
{
    UINT i;
    if (!g_initialized) {
        for (i = 0; i < 256; i++)
            g_ramp.red[i] = g_ramp.green[i] = g_ramp.blue[i] = (WORD)(i * 257);
        g_initialized = TRUE;
    }
    if (ramp) *ramp = g_ramp;
}

void d3d8_gamma_set(const D3DGAMMARAMP *ramp)
{
    UINT i;
    if (!ramp) return;
    g_ramp = *ramp;
    g_initialized = TRUE;
    g_enabled = FALSE;
    for (i = 0; i < 256; i++) {
        if (ramp->red[i] != i * 257 || ramp->green[i] != i * 257 || ramp->blue[i] != i * 257)
            g_enabled = TRUE;
    }
}

static int gamma_create(XGpu *g, SDL_GPUTextureFormat swap_format)
{
    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_GPUColorTargetDescription ct;
    SDL_GPUSamplerCreateInfo si;

    if (g_pipe && g_pipe_format == swap_format)
        return 1;
    gamma_release_resources();

    if (!xgpu_compile_hlsl_ex(g->device, k_gamma_vs, "main", 0, "gamma_vs", &g_vs) ||
        !xgpu_compile_hlsl_ex(g->device, k_gamma_ps, "main", 1, "gamma_ps", &g_ps)) {
        gamma_release_resources();
        return 0;
    }
    SDL_zero(si);                      /* nearest, clamp: the pass is 1:1 */
    si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    g_sampler = SDL_CreateGPUSampler(g->device, &si);

    SDL_zero(pi); SDL_zero(ct);
    ct.format = swap_format;
    pi.vertex_shader = g_vs.shader;
    pi.fragment_shader = g_ps.shader;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    g_pipe = SDL_CreateGPUGraphicsPipeline(g->device, &pi);
    if (!g_pipe || !g_sampler) {
        fprintf(stderr, "D3D8: gamma pipeline failed: %s\n", SDL_GetError());
        gamma_release_resources();
        return 0;
    }
    g_pipe_format = swap_format;
    return 1;
}

/* Present hook (see XGpu.present_hook): draws the backbuffer into `swap`
 * through the gamma ramp. Returns 0 when gamma is off so the plain blit runs. */
int d3d8_gamma_present(XGpu *g, SDL_GPUTexture *swap, Uint32 swap_w, Uint32 swap_h)
{
    SDL_GPUColorTargetInfo c;
    SDL_GPURenderPass *rp;
    SDL_GPUTextureSamplerBinding tsb;
    struct { float ramp[256][4]; float inv_size[4]; } u;
    SDL_GPUViewport vp;
    UINT i;

    if (!g_enabled)
        return 0;
    if (!gamma_create(g, g->present_format))
        return 0;

    for (i = 0; i < 256; i++) {
        u.ramp[i][0] = g_ramp.red[i] / 65535.0f;
        u.ramp[i][1] = g_ramp.green[i] / 65535.0f;
        u.ramp[i][2] = g_ramp.blue[i] / 65535.0f;
        u.ramp[i][3] = 0;
    }
    /* The pass is scaled to the window: sample by position within it. */
    u.inv_size[0] = 1.0f / (float)swap_w;
    u.inv_size[1] = 1.0f / (float)swap_h;
    u.inv_size[2] = u.inv_size[3] = 0;

    SDL_zero(c);
    c.texture = swap;
    c.load_op = SDL_GPU_LOADOP_DONT_CARE;
    c.store_op = SDL_GPU_STOREOP_STORE;
    rp = SDL_BeginGPURenderPass(g->cmd, &c, 1, NULL);
    if (!rp)
        return 0;
    SDL_BindGPUGraphicsPipeline(rp, g_pipe);
    vp.x = 0; vp.y = 0; vp.w = (float)swap_w; vp.h = (float)swap_h; vp.min_depth = 0; vp.max_depth = 1;
    SDL_SetGPUViewport(rp, &vp);
    SDL_PushGPUFragmentUniformData(g->cmd, 0, &u, sizeof(u));
    tsb.texture = g->backbuffer;
    tsb.sampler = g_sampler;
    SDL_BindGPUFragmentSamplers(rp, 0, &tsb, 1);
    SDL_DrawGPUPrimitives(rp, 3, 1, 0, 0);
    SDL_EndGPURenderPass(rp);
    return 1;
}
