/**
 * D3D8 draw dispatch on SDL_GPU: pipeline cache, uniforms, textures, draw.
 *
 * The previous backend let the layer set shaders, input layout and state objects one at a
 * time. SDL_GPU bakes all of that into a graphics pipeline, so the other
 * modules now only *describe* what is bound (shaders, vertex layout, uniform
 * data, render states, textures) and d3d8_pipe_draw() finds or builds the
 * matching pipeline and issues the draw.
 *
 * Vertex attribute locations are a convention between the shader generators
 * and the layouts they hand to d3d8_pipe_set_layout(); SDL_GPU only sees the
 * numbers.
 */

#include "d3d8_internal.h"
#include <stdio.h>
#include <string.h>

#define UBO_SLOTS    4
#define UBO_MAX_SIZE 4096
#define PIPE_CACHE   512
#define UP_RING_SIZE (8u * 1024u * 1024u)

typedef struct UboSlot {
    Uint8  data[UBO_MAX_SIZE];
    Uint32 size;
} UboSlot;

typedef struct PipeKey {
    SDL_GPUShader        *vs, *ps;
    Uint32                num_attrs;
    SDL_GPUVertexAttribute attrs[D3D8_MAX_ATTRS];
    Uint32                stride;
    Uint32                prim;
    D3D8StateKey          st;
    Uint32                color_fmt, depth_fmt;
} PipeKey;

typedef struct PipeEntry {
    PipeKey                  key;
    SDL_GPUGraphicsPipeline *pipe;
} PipeEntry;

static struct {
    XGpuShader        vs, ps;
    D3D8VertexLayout  layout;
    UboSlot           vubo[UBO_SLOTS], pubo[UBO_SLOTS];
    PipeEntry         cache[PIPE_CACHE];
    int               cache_count;
    XGpuTex          *white;                 /* bound where a stage has no texture */
    SDL_GPUBuffer    *up_ring;               /* vertex/index data for *UP draws */
    Uint32            up_offset;
    int               up_fresh;              /* first upload since the frame began */
} P;

HRESULT d3d8_pipe_init(void)
{
    XGpu *g = d3d8_GetGpu();
    SDL_GPUBufferCreateInfo bi;
    const Uint8 px[4] = { 255, 255, 255, 255 };

    memset(&P, 0, sizeof(P));
    P.white = xgputex_create(g, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                             1, 1, 1, 1, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    if (!P.white)
        return E_FAIL;
    xgpu_upload_texture(g, P.white->tex, 0, 0, 0, 0, 0, 1, 1, 1, px, 4, 1);

    SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX;
    bi.size = UP_RING_SIZE;
    P.up_ring = SDL_CreateGPUBuffer(g->device, &bi);
    if (!P.up_ring)
        return E_OUTOFMEMORY;
    P.up_fresh = 1;
    return S_OK;
}

void d3d8_pipe_shutdown(void)
{
    XGpu *g = d3d8_GetGpu();
    int i;

    if (g && g->device) {
        for (i = 0; i < P.cache_count; i++)
            SDL_ReleaseGPUGraphicsPipeline(g->device, P.cache[i].pipe);
        if (P.up_ring)
            SDL_ReleaseGPUBuffer(g->device, P.up_ring);
        xgputex_unref(g, P.white);
    }
    memset(&P, 0, sizeof(P));
}

void d3d8_pipe_begin_frame(void)
{
    P.up_offset = 0;
    P.up_fresh = 1;
}

void d3d8_pipe_set_vs(const XGpuShader *vs) { P.vs = *vs; }
void d3d8_pipe_set_ps(const XGpuShader *ps) { P.ps = *ps; }

void d3d8_pipe_set_layout(const D3D8VertexLayout *l)
{
    P.layout = *l;
}

void d3d8_pipe_set_uniforms(int fragment, Uint32 slot, const void *data, Uint32 size)
{
    UboSlot *u;
    if (slot >= UBO_SLOTS || size > UBO_MAX_SIZE)
        return;
    u = fragment ? &P.pubo[slot] : &P.vubo[slot];
    memcpy(u->data, data, size);
    u->size = size;
}

/* Copies draw data into the streaming ring; returns the buffer and offset the
 * draw should read from, or 0 if it does not fit. */
int d3d8_pipe_stream(const void *data, Uint32 size, SDL_GPUBuffer **buf, Uint32 *offset)
{
    Uint32 at = (P.up_offset + 15u) & ~15u;
    int cycle = P.up_fresh;

    if (size > UP_RING_SIZE)
        return 0;
    if (at + size > UP_RING_SIZE) {      /* wrap: SDL keeps the old store for draws already recorded */
        at = 0;
        cycle = 1;
    }
    if (!xgpu_upload_buffer_ex(d3d8_GetGpu(), P.up_ring, at, data, size, cycle))
        return 0;
    P.up_fresh = 0;
    P.up_offset = at + size;
    *buf = P.up_ring;
    *offset = at;
    return 1;
}

static SDL_GPUGraphicsPipeline *get_pipeline(const PipeKey *key)
{
    XGpu *g = d3d8_GetGpu();
    SDL_GPUGraphicsPipelineCreateInfo pi;
    SDL_GPUVertexBufferDescription vbd;
    SDL_GPUColorTargetDescription ct;
    const D3D8StateKey *s = &key->st;
    int i;

    for (i = 0; i < P.cache_count; i++)
        if (memcmp(&P.cache[i].key, key, sizeof(*key)) == 0)
            return P.cache[i].pipe;
    if (P.cache_count >= PIPE_CACHE) {
        fprintf(stderr, "D3D8: pipeline cache full\n");
        return NULL;
    }

    SDL_zero(pi); SDL_zero(vbd); SDL_zero(ct);
    pi.vertex_shader = key->vs;
    pi.fragment_shader = key->ps;

    vbd.slot = 0;
    vbd.pitch = key->stride;
    vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    pi.vertex_input_state.vertex_buffer_descriptions = &vbd;
    pi.vertex_input_state.num_vertex_buffers = key->num_attrs ? 1 : 0;
    pi.vertex_input_state.vertex_attributes = key->attrs;
    pi.vertex_input_state.num_vertex_attributes = key->num_attrs;
    pi.primitive_type = (SDL_GPUPrimitiveType)key->prim;

    pi.rasterizer_state.fill_mode = (SDL_GPUFillMode)s->fill;
    pi.rasterizer_state.cull_mode = (SDL_GPUCullMode)s->cull;
    pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    pi.rasterizer_state.enable_depth_clip = true;

    pi.depth_stencil_state.compare_op = (SDL_GPUCompareOp)s->depth_func;
    pi.depth_stencil_state.enable_depth_test = s->depth_test != 0;
    pi.depth_stencil_state.enable_depth_write = s->depth_write != 0;
    pi.depth_stencil_state.enable_stencil_test = s->stencil_enable != 0;
    pi.depth_stencil_state.compare_mask = s->stencil_read;
    pi.depth_stencil_state.write_mask = s->stencil_write;
    pi.depth_stencil_state.front_stencil_state.compare_op = (SDL_GPUCompareOp)s->stencil_func;
    pi.depth_stencil_state.front_stencil_state.fail_op = (SDL_GPUStencilOp)s->stencil_fail;
    pi.depth_stencil_state.front_stencil_state.depth_fail_op = (SDL_GPUStencilOp)s->stencil_zfail;
    pi.depth_stencil_state.front_stencil_state.pass_op = (SDL_GPUStencilOp)s->stencil_pass;
    pi.depth_stencil_state.back_stencil_state = pi.depth_stencil_state.front_stencil_state;

    ct.format = (SDL_GPUTextureFormat)key->color_fmt;
    ct.blend_state.enable_blend = s->blend_enable != 0;
    ct.blend_state.src_color_blendfactor = (SDL_GPUBlendFactor)s->src_color;
    ct.blend_state.dst_color_blendfactor = (SDL_GPUBlendFactor)s->dst_color;
    ct.blend_state.color_blend_op = (SDL_GPUBlendOp)s->blend_op;
    ct.blend_state.src_alpha_blendfactor = (SDL_GPUBlendFactor)s->src_alpha;
    ct.blend_state.dst_alpha_blendfactor = (SDL_GPUBlendFactor)s->dst_alpha;
    ct.blend_state.alpha_blend_op = (SDL_GPUBlendOp)s->blend_op;
    ct.blend_state.color_write_mask = s->color_mask;
    ct.blend_state.enable_color_write_mask = true;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    if (key->depth_fmt != SDL_GPU_TEXTUREFORMAT_INVALID) {
        pi.target_info.depth_stencil_format = (SDL_GPUTextureFormat)key->depth_fmt;
        pi.target_info.has_depth_stencil_target = true;
    }

    P.cache[P.cache_count].pipe = SDL_CreateGPUGraphicsPipeline(g->device, &pi);
    if (!P.cache[P.cache_count].pipe) {
        fprintf(stderr, "D3D8: pipeline create failed: %s\n", SDL_GetError());
        return NULL;
    }
    P.cache[P.cache_count].key = *key;
    return P.cache[P.cache_count++].pipe;
}

int d3d8_pipe_draw(const D3D8Draw *d)
{
    XGpu *g = d3d8_GetGpu();
    XGpuTargets t;
    SDL_GPUTextureFormat color_fmt, depth_fmt;
    Uint32 tw, th;
    PipeKey key;
    SDL_GPUGraphicsPipeline *pipe;
    SDL_GPURenderPass *rp;
    SDL_GPUBufferBinding vb;
    SDL_GPUTextureSamplerBinding tsb[4];
    D3DVIEWPORT8 vp;
    SDL_GPUViewport sv;
    const DWORD *rs = d3d8_GetRenderStates();
    Uint32 i, ns;

    if (!P.vs.shader || !P.ps.shader || !d->count)
        return 0;

    d3d8_GetCurrentTargets(&t, &color_fmt, &depth_fmt, &tw, &th);

    memset(&key, 0, sizeof(key));
    key.vs = P.vs.shader;
    key.ps = P.ps.shader;
    key.num_attrs = P.layout.num_attrs;
    memcpy(key.attrs, P.layout.attrs, sizeof(SDL_GPUVertexAttribute) * P.layout.num_attrs);
    key.stride = d->vb_stride;
    key.prim = (Uint32)d->prim;
    d3d8_states_get_key(&key.st);
    key.color_fmt = (Uint32)color_fmt;
    key.depth_fmt = (Uint32)depth_fmt;
    if (depth_fmt == SDL_GPU_TEXTUREFORMAT_INVALID) {
        key.st.depth_test = key.st.depth_write = key.st.stencil_enable = 0;
        key.st.depth_func = key.st.stencil_func = SDL_GPU_COMPAREOP_ALWAYS;
    }

    pipe = get_pipeline(&key);
    if (!pipe)
        return 0;
    rp = xgpu_render_pass(g, t, NULL);
    if (!rp)
        return 0;

    SDL_BindGPUGraphicsPipeline(rp, pipe);

    d3d8_GetViewport(&vp);
    sv.x = (float)vp.X; sv.y = (float)vp.Y;
    sv.w = (float)(vp.Width ? vp.Width : tw);
    sv.h = (float)(vp.Height ? vp.Height : th);
    sv.min_depth = vp.MinZ; sv.max_depth = vp.MaxZ;
    SDL_SetGPUViewport(rp, &sv);
    if (rs && key.st.stencil_enable)
        SDL_SetGPUStencilReference(rp, (Uint8)rs[D3DRS_STENCILREF]);

    /* Every uniform buffer the shaders declare must be pushed. */
    for (i = 0; i < P.vs.num_uniform_buffers && i < UBO_SLOTS; i++)
        SDL_PushGPUVertexUniformData(g->cmd, i, P.vubo[i].data,
                                     P.vubo[i].size ? P.vubo[i].size : 16);
    for (i = 0; i < P.ps.num_uniform_buffers && i < UBO_SLOTS; i++)
        SDL_PushGPUFragmentUniformData(g->cmd, i, P.pubo[i].data,
                                       P.pubo[i].size ? P.pubo[i].size : 16);

    ns = P.ps.num_samplers > 4 ? 4 : P.ps.num_samplers;
    for (i = 0; i < ns; i++) {
        IDirect3DBaseTexture8 *bt = d3d8_GetStageTexture(i);
        SDL_GPUTexture *tex = bt ? d3d8_base_gpu_texture(bt) : NULL;
        SDL_GPUSampler *smp = d3d8_states_sampler(i);
        tsb[i].texture = tex ? tex : P.white->tex;
        tsb[i].sampler = smp;
        if (!smp) return 0;
    }
    if (ns)
        SDL_BindGPUFragmentSamplers(rp, 0, tsb, ns);

    SDL_zero(vb);
    vb.buffer = d->vb;
    vb.offset = d->vb_offset;
    if (d->vb)
        SDL_BindGPUVertexBuffers(rp, 0, &vb, 1);

    if (d->ib) {
        SDL_GPUBufferBinding ib;
        SDL_zero(ib);
        ib.buffer = d->ib;
        ib.offset = d->ib_offset;
        SDL_BindGPUIndexBuffer(rp, &ib, d->ib32 ? SDL_GPU_INDEXELEMENTSIZE_32BIT
                                                : SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(rp, d->count, 1, d->first, d->base_vertex, 0);
    } else {
        SDL_DrawGPUPrimitives(rp, d->count, 1, d->first, 0);
    }
    return 1;
}
