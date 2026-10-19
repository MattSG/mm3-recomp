#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "nv2a_shader_cache.h"
#include "mm3_distance_fog.inc"
static ID3D11Device *fog_device;
static ID3D11DeviceContext *fog_context;
static int fog_upload(ID3D11Buffer *buffer,const void *data,size_t bytes)
{
    D3D11_MAPPED_SUBRESOURCE map;
    if (FAILED(ID3D11DeviceContext_Map(fog_context,(ID3D11Resource *)buffer,0,
                                      D3D11_MAP_WRITE_DISCARD,0,&map))) return 0;
    memcpy(map.pData,data,bytes);
    ID3D11DeviceContext_Unmap(fog_context,(ID3D11Resource *)buffer,0);
    return 1;
}
static ID3D11VertexShader *s_native_fog_vs;
static ID3D11PixelShader *s_native_fog_ps[2];
static ID3D11Buffer *s_native_fog_cb;
static ID3D11BlendState *s_native_fog_blend, *s_native_fog_mask;
static ID3D11DepthStencilState *s_native_fog_depth;
static ID3D11RasterizerState *s_native_fog_raster;

static ID3DBlob *native_fog_compile(const char *entry, const char *target, int msaa)
{
    D3D_SHADER_MACRO macros[] = {{"FOG_MSAA", msaa ? "1" : "0"}, {NULL,NULL}};
    ID3DBlob *code = NULL, *error = NULL;
    HRESULT hr = nv_shader_cache_compile(s_native_fog_hlsl, sizeof s_native_fog_hlsl-1,
        "native_distance_fog", macros, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &code, &error, NULL);
    if (FAILED(hr)) fprintf(stderr,"[FOG] %s: %s\n",entry,
                           error ? (const char *)ID3D10Blob_GetBufferPointer(error) : "compile failed");
    if (error) ID3D10Blob_Release(error);
    return code;
}

static int native_fog_init(void)
{
    ID3DBlob *code;
    D3D11_BUFFER_DESC cb = {48,D3D11_USAGE_DYNAMIC,D3D11_BIND_CONSTANT_BUFFER,D3D11_CPU_ACCESS_WRITE,0,0};
    D3D11_BLEND_DESC blend = {0};
    D3D11_DEPTH_STENCIL_DESC depth = {0};
    D3D11_RASTERIZER_DESC raster = {0};
    if (s_native_fog_vs) return 1;
    code = native_fog_compile("fog_vs","vs_5_0",0);
    if (!code) goto fail;
    HRESULT hr = ID3D11Device_CreateVertexShader(fog_device,ID3D10Blob_GetBufferPointer(code),
        ID3D10Blob_GetBufferSize(code),NULL,&s_native_fog_vs);
    ID3D10Blob_Release(code);
    if (FAILED(hr)) goto fail;
    for (int i = 0; i < 2; ++i) {
        code = native_fog_compile("fog_ps","ps_5_0",i);
        if (!code) goto fail;
        hr = ID3D11Device_CreatePixelShader(fog_device,ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code),NULL,&s_native_fog_ps[i]);
        ID3D10Blob_Release(code);
        if (FAILED(hr)) goto fail;
    }
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.ScissorEnable = TRUE;
    raster.DepthClipEnable = TRUE;
    if (FAILED(ID3D11Device_CreateBuffer(fog_device,&cb,NULL,&s_native_fog_cb)) ||
        FAILED(ID3D11Device_CreateBlendState(fog_device,&blend,&s_native_fog_blend)) ||
        FAILED(ID3D11Device_CreateDepthStencilState(fog_device,&depth,&s_native_fog_depth)) ||
        FAILED(ID3D11Device_CreateRasterizerState(fog_device,&raster,&s_native_fog_raster))) goto fail;
    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
    if (FAILED(ID3D11Device_CreateBlendState(fog_device,&blend,&s_native_fog_mask))) goto fail;
    return 1;
fail:
    if (s_native_fog_vs) { ID3D11VertexShader_Release(s_native_fog_vs); s_native_fog_vs = NULL; }
    for (int i = 0; i < 2; ++i)
        if (s_native_fog_ps[i]) { ID3D11PixelShader_Release(s_native_fog_ps[i]); s_native_fog_ps[i] = NULL; }
    if (s_native_fog_cb) { ID3D11Buffer_Release(s_native_fog_cb); s_native_fog_cb = NULL; }
    if (s_native_fog_blend) { ID3D11BlendState_Release(s_native_fog_blend); s_native_fog_blend = NULL; }
    if (s_native_fog_mask) { ID3D11BlendState_Release(s_native_fog_mask); s_native_fog_mask = NULL; }
    if (s_native_fog_depth) { ID3D11DepthStencilState_Release(s_native_fog_depth); s_native_fog_depth = NULL; }
    if (s_native_fog_raster) { ID3D11RasterizerState_Release(s_native_fog_raster); s_native_fog_raster = NULL; }
    return 0;
}

static int mm3_fog_draw(const NvHostTarget *target,const float input[12],int mask)
{
    float params[12]; memcpy(params,input,sizeof params);
    fog_device = target->device; fog_context = target->context;
    if (!native_fog_init() || !fog_upload(s_native_fog_cb,params,sizeof params)) return 0;
    ID3D11RenderTargetView *rtv = target->colour;
    ID3D11ShaderResourceView *depth = target->depth;
    D3D11_VIEWPORT vp = {0,0,(float)target->width,(float)target->height,0,1};
    D3D11_RECT sc = {0,0,(LONG)target->width,(LONG)target->height};
    ID3D11DeviceContext_OMSetRenderTargets(fog_context,1,&rtv,NULL);
    ID3D11DeviceContext_RSSetViewports(fog_context,1,&vp);
    ID3D11DeviceContext_RSSetScissorRects(fog_context,1,&sc);
    ID3D11DeviceContext_RSSetState(fog_context,s_native_fog_raster);
    ID3D11DeviceContext_OMSetDepthStencilState(fog_context,s_native_fog_depth,0);
    ID3D11DeviceContext_OMSetBlendState(fog_context,s_native_fog_blend,NULL,0xFFFFFFFFu);
    ID3D11DeviceContext_IASetInputLayout(fog_context,NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(fog_context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(fog_context,s_native_fog_vs,NULL,0);
    ID3D11DeviceContext_PSSetShader(fog_context,s_native_fog_ps[target->samples > 1],NULL,0);
    ID3D11DeviceContext_PSSetConstantBuffers(fog_context,0,1,&s_native_fog_cb);
    ID3D11DeviceContext_PSSetShaderResources(fog_context,0,1,&depth);
    ID3D11DeviceContext_Draw(fog_context,3,0);
    /* Optional mask for subsequent destination-alpha composition. */
    params[7] = 1;
    if (mask && fog_upload(s_native_fog_cb,params,sizeof params)) {
        ID3D11DeviceContext_OMSetBlendState(fog_context,s_native_fog_mask,NULL,0xFFFFFFFFu);
        ID3D11DeviceContext_Draw(fog_context,3,0);
    }
    return 1;
}
