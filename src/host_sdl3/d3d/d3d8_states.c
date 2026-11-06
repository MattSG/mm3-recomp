/**
 * D3D8 Render State -> SDL_GPU State Translation
 *
 * Converts D3D8 render state values into the fixed-function parts of an
 * SDL_GPU graphics pipeline:
 *   - Blend (alpha blending, colour write mask)
 *   - Depth/stencil (z-test, z-write, stencil)
 *   - Rasteriser (cull mode, fill mode)
 * and into cached samplers (texture filtering, addressing).
 *
 * SDL_GPU bakes all of this into the pipeline object, so nothing is applied
 * here: d3d8_states_get_key() returns a plain, memcmp-comparable key that the
 * pipeline cache (d3d8_pipeline.c) hashes and builds a pipeline from.
 */

#include "d3d8_internal.h"
#include <string.h>
#include <stdio.h>

/* ================================================================
 * D3D8 -> SDL_GPU enum translation
 * ================================================================ */

static Uint8 blend_factor(DWORD b)
{
    switch (b) {
    case D3DBLEND_ZERO:         return SDL_GPU_BLENDFACTOR_ZERO;
    case D3DBLEND_ONE:          return SDL_GPU_BLENDFACTOR_ONE;
    case D3DBLEND_SRCCOLOR:     return SDL_GPU_BLENDFACTOR_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:  return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    case D3DBLEND_SRCALPHA:     return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:  return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:    return SDL_GPU_BLENDFACTOR_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:    return SDL_GPU_BLENDFACTOR_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
    case D3DBLEND_SRCALPHASAT:  return SDL_GPU_BLENDFACTOR_SRC_ALPHA_SATURATE;
    default:                    return SDL_GPU_BLENDFACTOR_ONE;
    }
}

/* Colour factors are not meaningful in the alpha channel; use their alpha
 * components, as the previous backend did. */
static Uint8 blend_alpha_factor(Uint8 f)
{
    switch (f) {
    case SDL_GPU_BLENDFACTOR_SRC_COLOR:           return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    case SDL_GPU_BLENDFACTOR_DST_COLOR:           return SDL_GPU_BLENDFACTOR_DST_ALPHA;
    case SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
    default:                                      return f;
    }
}

static Uint8 compare_op(DWORD c)
{
    switch (c) {
    case D3DCMP_NEVER:        return SDL_GPU_COMPAREOP_NEVER;
    case D3DCMP_LESS:         return SDL_GPU_COMPAREOP_LESS;
    case D3DCMP_EQUAL:        return SDL_GPU_COMPAREOP_EQUAL;
    case D3DCMP_LESSEQUAL:    return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    case D3DCMP_GREATER:      return SDL_GPU_COMPAREOP_GREATER;
    case D3DCMP_NOTEQUAL:     return SDL_GPU_COMPAREOP_NOT_EQUAL;
    case D3DCMP_GREATEREQUAL: return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
    case D3DCMP_ALWAYS:       return SDL_GPU_COMPAREOP_ALWAYS;
    default:                  return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    }
}

static Uint8 stencil_op(DWORD op)
{
    switch (op) {
    case 1: return SDL_GPU_STENCILOP_KEEP;
    case 2: return SDL_GPU_STENCILOP_ZERO;
    case 3: return SDL_GPU_STENCILOP_REPLACE;
    case 4: return SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP;
    case 5: return SDL_GPU_STENCILOP_DECREMENT_AND_CLAMP;
    case 6: return SDL_GPU_STENCILOP_INVERT;
    case 7: return SDL_GPU_STENCILOP_INCREMENT_AND_WRAP;
    case 8: return SDL_GPU_STENCILOP_DECREMENT_AND_WRAP;
    default: return SDL_GPU_STENCILOP_KEEP;
    }
}

static Uint8 blend_op(DWORD op)
{
    switch (op) {
    case 2: return SDL_GPU_BLENDOP_SUBTRACT;
    case 3: return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
    case 4: return SDL_GPU_BLENDOP_MIN;
    case 5: return SDL_GPU_BLENDOP_MAX;
    default: return SDL_GPU_BLENDOP_ADD;
    }
}

void d3d8_states_get_key(D3D8StateKey *k)
{
    const DWORD *rs = d3d8_GetRenderStates();

    memset(k, 0, sizeof(*k));            /* zeroed padding keeps the key memcmp-able */
    if (!rs) {
        k->depth_func = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        k->src_color = k->src_alpha = SDL_GPU_BLENDFACTOR_ONE;
        k->dst_color = k->dst_alpha = SDL_GPU_BLENDFACTOR_ZERO;
        k->color_mask = 0x0F;
        k->cull = SDL_GPU_CULLMODE_BACK;
        return;
    }

    k->blend_enable = rs[D3DRS_ALPHABLENDENABLE] ? 1 : 0;
    k->src_color = blend_factor(rs[D3DRS_SRCBLEND]);
    k->dst_color = blend_factor(rs[D3DRS_DESTBLEND]);
    k->src_alpha = blend_alpha_factor(k->src_color);
    k->dst_alpha = blend_alpha_factor(k->dst_color);
    k->blend_op = blend_op(rs[D3DRS_BLENDOP] ? rs[D3DRS_BLENDOP] : 1);
    k->color_mask = (Uint8)(rs[D3DRS_COLORWRITEENABLE] & 0x0F);

    k->depth_test = rs[D3DRS_ZENABLE] ? 1 : 0;
    k->depth_write = rs[D3DRS_ZWRITEENABLE] ? 1 : 0;
    k->depth_func = compare_op(rs[D3DRS_ZFUNC]);

    k->stencil_enable = rs[D3DRS_STENCILENABLE] ? 1 : 0;
    k->stencil_read = (Uint8)(rs[D3DRS_STENCILMASK] & 0xFF);
    k->stencil_write = (Uint8)(rs[D3DRS_STENCILWRITEMASK] & 0xFF);
    k->stencil_func = compare_op(rs[D3DRS_STENCILFUNC]);
    k->stencil_fail = stencil_op(rs[D3DRS_STENCILFAIL]);
    k->stencil_zfail = stencil_op(rs[D3DRS_STENCILZFAIL]);
    k->stencil_pass = stencil_op(rs[D3DRS_STENCILPASS]);

    /* SDL_GPU has no point fill; wireframe stands in as before. */
    k->fill = (rs[D3DRS_FILLMODE] == D3DFILL_POINT || rs[D3DRS_FILLMODE] == D3DFILL_WIREFRAME)
                  ? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
    switch (rs[D3DRS_CULLMODE]) {
    case D3DCULL_NONE: k->cull = SDL_GPU_CULLMODE_NONE; break;
    case D3DCULL_CW:   k->cull = SDL_GPU_CULLMODE_FRONT; break;  /* as the D3D8 layer mapped it */
    default:           k->cull = SDL_GPU_CULLMODE_BACK; break;
    }
}

/* ================================================================
 * Samplers
 * ================================================================ */

static SDL_GPUSamplerAddressMode address_mode(DWORD mode)
{
    switch (mode) {
    case D3DTADDRESS_MIRROR:     return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    case D3DTADDRESS_CLAMP:      return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    /* SDL_GPU has no border colour or mirror-once; these are the nearest. */
    case D3DTADDRESS_BORDER:     return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    case D3DTADDRESS_MIRRORONCE: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
    default:                     return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    }
}

typedef struct SamplerEntry {
    Uint8 min_linear, mag_linear, mip_linear, au, av, aniso;
    SDL_GPUSampler *sampler;
} SamplerEntry;

#define SAMPLER_CACHE 64
static SamplerEntry g_samplers[SAMPLER_CACHE];
static int          g_sampler_count;

SDL_GPUSampler *d3d8_states_sampler(DWORD stage)
{
    const DWORD *tss;
    SamplerEntry key;
    SDL_GPUSamplerCreateInfo si;
    int i;

    if (stage >= 4) return NULL;
    tss = d3d8_GetTSS(stage);
    if (!tss) return NULL;

    memset(&key, 0, sizeof(key));
    key.mag_linear = (tss[D3DTSS_MAGFILTER] == D3DTEXF_LINEAR || tss[D3DTSS_MAGFILTER] == D3DTEXF_ANISOTROPIC);
    key.min_linear = (tss[D3DTSS_MINFILTER] == D3DTEXF_LINEAR || tss[D3DTSS_MINFILTER] == D3DTEXF_ANISOTROPIC);
    key.mip_linear = (tss[D3DTSS_MIPFILTER] == D3DTEXF_LINEAR);
    key.au = (Uint8)address_mode(tss[D3DTSS_ADDRESSU] ? tss[D3DTSS_ADDRESSU] : D3DTADDRESS_WRAP);
    key.av = (Uint8)address_mode(tss[D3DTSS_ADDRESSV] ? tss[D3DTSS_ADDRESSV] : D3DTADDRESS_WRAP);
    if (tss[D3DTSS_MAGFILTER] == D3DTEXF_ANISOTROPIC || tss[D3DTSS_MINFILTER] == D3DTEXF_ANISOTROPIC) {
        DWORD a = tss[D3DTSS_MAXANISOTROPY] ? tss[D3DTSS_MAXANISOTROPY] : 1;
        key.aniso = (Uint8)(a > 16 ? 16 : a);
    }

    for (i = 0; i < g_sampler_count; i++) {
        const SamplerEntry *e = &g_samplers[i];
        if (e->min_linear == key.min_linear && e->mag_linear == key.mag_linear &&
            e->mip_linear == key.mip_linear && e->au == key.au && e->av == key.av &&
            e->aniso == key.aniso)
            return e->sampler;
    }
    if (g_sampler_count >= SAMPLER_CACHE) {
        fprintf(stderr, "D3D8: sampler cache full, reusing the first sampler\n");
        return g_samplers[0].sampler;
    }

    SDL_zero(si);
    si.min_filter = key.min_linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    si.mag_filter = key.mag_linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
    si.mipmap_mode = key.mip_linear ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    si.address_mode_u = (SDL_GPUSamplerAddressMode)key.au;
    si.address_mode_v = (SDL_GPUSamplerAddressMode)key.av;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    si.max_lod = 1000.0f;
    si.enable_anisotropy = key.aniso > 1;
    si.max_anisotropy = (float)(key.aniso ? key.aniso : 1);
    key.sampler = SDL_CreateGPUSampler(d3d8_GetGpu()->device, &si);
    if (!key.sampler) {
        fprintf(stderr, "D3D8: CreateSampler failed: %s\n", SDL_GetError());
        return NULL;
    }
    g_samplers[g_sampler_count++] = key;
    return key.sampler;
}

HRESULT d3d8_states_init(void)
{
    return S_OK;
}

void d3d8_states_shutdown(void)
{
    XGpu *g = d3d8_GetGpu();
    int i;
    for (i = 0; i < g_sampler_count; i++)
        if (g && g->device && g_samplers[i].sampler)
            SDL_ReleaseGPUSampler(g->device, g_samplers[i].sampler);
    g_sampler_count = 0;
    memset(g_samplers, 0, sizeof(g_samplers));
}
