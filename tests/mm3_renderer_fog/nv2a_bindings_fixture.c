/* Generic D3D11 binding fixture, from MattSG/xboxrecomp 45c01f65
 * (tests/nv2a_bindings/test_bindings.c); the fork dropped it on its v0.16 rebase. */
/* Inspect the real D3D11 slots and pixels produced by the actual renderer.
 * The reference restores all original per-draw binds immediately before the
 * real DrawIndexed, independently of the optimized helpers' bookkeeping. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_1.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned shader_calls, vs_cb_calls, srv_calls, ps_cb1_calls, layout_calls;
static unsigned ps_shader_calls, ps_sampler_calls, draw_calls, ps_map_calls;
static int reference, pixel_reference, fail_map, fail_ps_map, fail_buffers;
static void set_vs(ID3D11DeviceContext *, ID3D11VertexShader *, ID3D11ClassInstance *const *, UINT);
static void set_ps(ID3D11DeviceContext *, ID3D11PixelShader *, ID3D11ClassInstance *const *, UINT);
static void set_ps_samplers(ID3D11DeviceContext *, UINT, UINT, ID3D11SamplerState *const *);
static void set_vs_cb(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer *const *);
static void set_vs_srv(ID3D11DeviceContext *, UINT, UINT, ID3D11ShaderResourceView *const *);
static void set_ps_cb(ID3D11DeviceContext *, UINT, UINT, ID3D11Buffer *const *);
static void set_layout(ID3D11DeviceContext *, ID3D11InputLayout *);
static void draw_indexed(ID3D11DeviceContext *, UINT, UINT, INT);
static HRESULT map_resource(ID3D11DeviceContext *, ID3D11Resource *, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE *);
static HRESULT create_buffer(ID3D11Device *, const D3D11_BUFFER_DESC *, const D3D11_SUBRESOURCE_DATA *, ID3D11Buffer **);
#undef ID3D11DeviceContext_VSSetShader
#undef ID3D11DeviceContext_PSSetShader
#undef ID3D11DeviceContext_PSSetSamplers
#undef ID3D11DeviceContext_VSSetConstantBuffers
#undef ID3D11DeviceContext_VSSetShaderResources
#undef ID3D11DeviceContext_PSSetConstantBuffers
#undef ID3D11DeviceContext_IASetInputLayout
#undef ID3D11DeviceContext_DrawIndexed
#undef ID3D11DeviceContext_Map
#undef ID3D11Device_CreateBuffer
#define ID3D11DeviceContext_VSSetShader set_vs
#define ID3D11DeviceContext_PSSetShader set_ps
#define ID3D11DeviceContext_PSSetSamplers set_ps_samplers
#define ID3D11DeviceContext_VSSetConstantBuffers set_vs_cb
#define ID3D11DeviceContext_VSSetShaderResources set_vs_srv
#define ID3D11DeviceContext_PSSetConstantBuffers set_ps_cb
#define ID3D11DeviceContext_IASetInputLayout set_layout
#define ID3D11DeviceContext_DrawIndexed draw_indexed
#define ID3D11DeviceContext_Map map_resource
#define ID3D11Device_CreateBuffer create_buffer
#include "../../tools/xboxrecomp/src/video/nv2a_d3d11.c"
#include "../../tools/xboxrecomp/tests/nv2a_test_host.h"

static const NvD3DState *pixel_state;
static uint8_t ram[0x400000];
static HWND test_window;
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)ram; }
HWND xbox_FramebufferWindowHandle(void) { return test_window; }
int xbox_FramebufferScanSource(uint32_t *va, uint32_t *pitch) { return 0; }
int xbox_FramebufferOverlay(XboxOverlay *ovl) { return 0; }
void xbox_FramebufferNoteFlip(uint32_t draws) {}
int xbox_GetDisplayMode(uint32_t *pitch, uint32_t *bpp, uint32_t *w, uint32_t *h) { (void)pitch; (void)bpp; (void)w; (void)h; return 0; }
void xbox_FramebufferRenderHostOverlay(void *device, void *context, void *target) {}
const uint32_t (*nv2a_vsh_program(uint32_t *start, uint32_t *version))[4] { return NULL; }
const float (*nv2a_vsh_constants(uint32_t *version))[4] { return NULL; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)

static void set_vs(ID3D11DeviceContext *c, ID3D11VertexShader *s, ID3D11ClassInstance *const *i, UINT n)
{ shader_calls++; c->lpVtbl->VSSetShader(c,s,i,n); }
static void set_ps(ID3D11DeviceContext *c, ID3D11PixelShader *s, ID3D11ClassInstance *const *i, UINT n)
{ ps_shader_calls++; c->lpVtbl->PSSetShader(c,s,i,n); }
static void set_ps_samplers(ID3D11DeviceContext *c, UINT slot, UINT n, ID3D11SamplerState *const *s)
{ ps_sampler_calls++; c->lpVtbl->PSSetSamplers(c,slot,n,s); }
static void set_vs_cb(ID3D11DeviceContext *c, UINT slot, UINT n, ID3D11Buffer *const *b)
{ vs_cb_calls++; c->lpVtbl->VSSetConstantBuffers(c,slot,n,b); }
static void set_vs_srv(ID3D11DeviceContext *c, UINT slot, UINT n, ID3D11ShaderResourceView *const *v)
{ srv_calls++; c->lpVtbl->VSSetShaderResources(c,slot,n,v); }
static void set_ps_cb(ID3D11DeviceContext *c, UINT slot, UINT n, ID3D11Buffer *const *b)
{ if(slot==1) ps_cb1_calls++; c->lpVtbl->PSSetConstantBuffers(c,slot,n,b); }
static void set_layout(ID3D11DeviceContext *c, ID3D11InputLayout *l)
{ layout_calls++; c->lpVtbl->IASetInputLayout(c,l); }

static __declspec(noinline) void legacy_bind(void)
{
    ID3D11Buffer *b[4]={s_cb_ub,NULL,s_cb_prog,s_cb_vconst};
    s_ctx->lpVtbl->IASetInputLayout(s_ctx,NULL);
    s_ctx->lpVtbl->VSSetShader(s_ctx,s_ub_vs,NULL,0);
    s_ctx->lpVtbl->VSSetConstantBuffers(s_ctx,0,4,b);
    s_ctx->lpVtbl->VSSetShaderResources(s_ctx,0,1,&s_raw_srv);
    s_ctx->lpVtbl->PSSetConstantBuffers(s_ctx,1,1,&s_cb_ps);
}
static __declspec(noinline) void cached_bind(void)
{
    bind_vertex_shader(s_ub_vs);
    bind_vertex_constants(s_cb_ub);
}
static void expected_pixel_bindings(const NvD3DState *st, ID3D11PixelShader **ps,
                                    ID3D11SamplerState *samplers[4])
{
    unsigned n;
    *ps=s_psv[(st->flat_shade?1u:0u)|(st->z_perspective?2u:0u)];
    for(n=0;n<4;n++) {
        unsigned mode=st->rc_seen?(st->rc.stage_program>>(n*5))&31u:
                      n==0&&(st->tex_used&1);
        samplers[n]=s_linear_clamp;
        if(mode_needs_texture(mode)&&st->tex[n].addr&&st->tex[n].width&&st->tex[n].height)
            samplers[n]=sampler(&st->tex[n],1,0); /* Fixtures have one guest mip. */
    }
}
static __declspec(noinline) void legacy_pixel_bind(void)
{
    ID3D11PixelShader *ps; ID3D11SamplerState *samplers[4];
    expected_pixel_bindings(pixel_state,&ps,samplers);
    s_ctx->lpVtbl->PSSetShader(s_ctx,ps,NULL,0);
    s_ctx->lpVtbl->PSSetSamplers(s_ctx,0,4,samplers);
}
static void draw_indexed(ID3D11DeviceContext *c, UINT n, UINT start, INT base)
{
    if(reference) legacy_bind();
    if(pixel_reference) legacy_pixel_bind();
    draw_calls++; c->lpVtbl->DrawIndexed(c,n,start,base);
}
static HRESULT map_resource(ID3D11DeviceContext *c, ID3D11Resource *r, UINT sub,
                            D3D11_MAP kind, UINT flags, D3D11_MAPPED_SUBRESOURCE *m)
{
    if(r==(ID3D11Resource *)s_cb_ps) {
        ps_map_calls++;
        if(fail_ps_map) { fail_ps_map--; return E_FAIL; }
    }
    if(fail_map && r==(ID3D11Resource *)s_raw) { fail_map=0; return E_FAIL; }
    return c->lpVtbl->Map(c,r,sub,kind,flags,m);
}
static HRESULT create_buffer(ID3D11Device *d, const D3D11_BUFFER_DESC *desc,
                             const D3D11_SUBRESOURCE_DATA *data, ID3D11Buffer **b)
{
    if(fail_buffers) { *b=NULL; return E_OUTOFMEMORY; }
    return d->lpVtbl->CreateBuffer(d,desc,data,b);
}

static void check_slots(ID3D11VertexShader *expected, ID3D11Buffer *b0)
{
    ID3D11VertexShader *vs=NULL;
    ID3D11Buffer *buffers[4]={0}, *ps=NULL;
    ID3D11ShaderResourceView *srv=NULL;
    ID3D11InputLayout *layout=NULL;
    ID3D11DeviceContext_VSGetShader(s_ctx,&vs,NULL,NULL);
    ID3D11DeviceContext_VSGetConstantBuffers(s_ctx,0,4,buffers);
    ID3D11DeviceContext_PSGetConstantBuffers(s_ctx,1,1,&ps);
    ID3D11DeviceContext_VSGetShaderResources(s_ctx,0,1,&srv);
    ID3D11DeviceContext_IAGetInputLayout(s_ctx,&layout);
    if(vs!=expected || buffers[0]!=b0 || buffers[1])
        fprintf(stderr,"slots VS actual=%p expected=%p b0 actual=%p expected=%p b1=%p\n",
                vs,expected,buffers[0],b0,buffers[1]);
    CHECK(vs==expected && buffers[0]==b0 && !buffers[1]);
    CHECK(buffers[2]==s_cb_prog && buffers[3]==s_cb_vconst);
    CHECK(ps==s_cb_ps && srv==s_raw_srv && !layout);
    if(vs) ID3D11VertexShader_Release(vs);
    if(buffers[0]) ID3D11Buffer_Release(buffers[0]);
    ID3D11Buffer_Release(buffers[2]); ID3D11Buffer_Release(buffers[3]);
    ID3D11Buffer_Release(ps); ID3D11ShaderResourceView_Release(srv);
}

static NvD3DState draw_state(void)
{
    NvD3DState st={0};
    st.color_addr=0x20000; st.color_pitch=64*4;
    st.zeta_addr=0x40000; st.zeta_pitch=64*4;
    st.surface_format=0x25; st.clip_w=st.clip_h=64;
    st.color_mask=0x01010101; st.blend_sfactor=1; st.blend_equation=0x8006;
    st.front_face=0x901; st.depth_test=1; st.depth_func=0x207;
    return st;
}
static void triangle(const NvD3DState *st, unsigned color, unsigned moved)
{
    float positions[3][4]={{8,8,0,1},{56,8,0,1},{8,56,0,1}};
    NvD3DAttrib attr[16]={0}; float def[16][4]={{0}};
    uint32_t idx[3]={0,1,2}; unsigned i;
    for(i=0;i<3;i++) positions[i][0]+=(float)moved;
    def[3][color%3]=1; def[3][3]=1;
    def[9][0]=-.4375f; def[9][1]=.1875f; def[9][3]=1;
    attr[0].enabled=1; attr[0].type=2; attr[0].size=4;
    attr[0].stride=16; attr[0].data=(const uint8_t *)positions;
    pixel_state=st;
    nv2a_d3d_draw_ub(st,NV_D3D_TRIANGLES,attr,def,0,idx,3,0,2,NULL);
}
static void read_pixels(uint32_t pixels[64*64])
{
    Surface *s=surface_find(0x20000);
    ID3D11Texture2D *stage;
    D3D11_TEXTURE2D_DESC desc; D3D11_MAPPED_SUBRESOURCE map;
    unsigned y;
    CHECK(s && s->iw==64 && s->ih==64);
    surface_resolve(s);
    ID3D11Texture2D_GetDesc(s->tex,&desc);
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=desc.MiscFlags=0;
    desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(s_dev,&desc,NULL,&stage)));
    ID3D11DeviceContext_CopyResource(s_ctx,(ID3D11Resource *)stage,(ID3D11Resource *)s->tex);
    CHECK(SUCCEEDED(ID3D11DeviceContext_Map(s_ctx,(ID3D11Resource *)stage,0,D3D11_MAP_READ,0,&map)));
    for(y=0;y<64;y++) memcpy(pixels+y*64,(uint8_t *)map.pData+y*map.RowPitch,64*4);
    ID3D11DeviceContext_Unmap(s_ctx,(ID3D11Resource *)stage,0);
    ID3D11Texture2D_Release(stage);
}
static void auxiliary(unsigned which, const NvD3DState *st)
{
    Surface *s=surface_find(0x20000); Depth *d;
    ID3D11Texture2D *out=NULL; ID3D11RenderTargetView *rtv=NULL;
    PresentConsts pc={0};
    if(which==0) return;
    if(which==1) {
        nv2a_d3d_clear(st,0xF0,0xFF000000,0,0,0,63,63);
        check_slots(s_clear_vs,s_cb_clear); return;
    }
    if(which==2) { CHECK(surface_y16(s)); check_slots(s_y16_vs,s_cb_ub); return; }
    if(which==3) {
        d=depth_get(st->zeta_addr,64,64,1); CHECK(d);
        CHECK(depth_y16(d)); check_slots(s_y16_vs,s_cb_ub); return;
    }
    out=make_texture(64,64,1,1,DXGI_FORMAT_B8G8R8A8_UNORM,D3D11_BIND_RENDER_TARGET,0);
    CHECK(out && SUCCEEDED(ID3D11Device_CreateRenderTargetView(s_dev,(ID3D11Resource *)out,NULL,&rtv)));
    if(which==4) blit(s->srv,rtv,64,64,1);
    else composite(rtv,64,64,s,&pc,0);
    check_slots(s_present_vs,s_cb_present);
    ID3D11RenderTargetView_Release(rtv); ID3D11Texture2D_Release(out);
}

static void test_pixels(void)
{
    uint32_t expected[64*64], actual[64*64];
    NvD3DState st=draw_state(); unsigned pass, mode, rawmode;
    int supported=s_raw_nooverwrite;
    for(rawmode=0;rawmode<2;rawmode++) {
        if(rawmode && !supported) continue;
        s_raw_nooverwrite=(int)rawmode;
        for(pass=0;pass<6;pass++) {
            for(mode=0;mode<2;mode++) {
                unsigned before_shader, before_cb;
                reference=!mode;
                nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
                triangle(&st,0,0); check_slots(s_ub_vs,s_cb_ub);
                auxiliary(pass,&st);
                /* Force both dynamic rings to wrap. Update raw vertices and
                 * constants after the auxiliary shader changed the context. */
                s_raw_pos=VB_BYTES-16; s_ib_pos=IB_BYTES-4;
                triangle(&st,1,2); check_slots(s_ub_vs,s_cb_ub);
                CHECK(s_raw_pos<1024 && s_ib_pos<1024);
                before_shader=shader_calls; before_cb=vs_cb_calls;
                triangle(&st,2,4); check_slots(s_ub_vs,s_cb_ub);
                CHECK(shader_calls==before_shader && vs_cb_calls==before_cb);
                read_pixels(mode?actual:expected);
            }
            CHECK(!memcmp(expected,actual,sizeof actual));
            CHECK((actual[16*64+20]&0xFFFFFF)!=0); /* A real visible triangle. */
        }
    }
    CHECK(layout_calls==1 && srv_calls==1 && ps_cb1_calls==1);
    puts("PASS: exact reference GPU pixels, actual fixed/changed slots, six auxiliary paths, updated raw data/constants, DISCARD/NO_OVERWRITE and both ring wraps");
}
static void test_failures(void)
{
    NvD3DState st=draw_state(); uint32_t expected[64*64], actual[64*64];
    reference=1; nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
    triangle(&st,1,0); read_pixels(expected);
    reference=0; nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
    fail_map=1; triangle(&st,2,0); CHECK(!fail_map);
    check_slots(s_clear_vs,s_cb_clear);
    triangle(&st,1,0); check_slots(s_ub_vs,s_cb_ub); read_pixels(actual);
    CHECK(!memcmp(expected,actual,sizeof actual));
    bind_vertex_shader(NULL); bind_vertex_constants(NULL); check_slots(NULL,NULL);
    triangle(&st,1,0); check_slots(s_ub_vs,s_cb_ub);
    CHECK(layout_calls==1 && srv_calls==1 && ps_cb1_calls==1);
    puts("PASS: failed raw Map leaves truthful binding state; retry restores exact pixels; NULL shader/constants bind and recover");
}
static void test_fixed_updates(void)
{
    const char *hlsl=
        "cbuffer P:register(b2){float4 a;} cbuffer K:register(b3){float4 b;}"
        "ByteAddressBuffer V:register(t0);"
        "struct O{float4 p:SV_Position;float4 c:COLOR0;};"
        "O vertex(uint id:SV_VertexID){O o;o.p=asfloat(V.Load4(id*16));o.c=a*b;return o;}"
        "float4 pixel(O o):SV_Target{return o.c;}";
    NvD3DState st=draw_state(); ID3DBlob *blob=NULL,*error=NULL;
    ID3D11VertexShader *vs=NULL; ID3D11PixelShader *ps=NULL;
    uint32_t expected[64*64],actual[64*64],previous[64*64];
    unsigned step,mode;
    CHECK(SUCCEEDED(D3DCompile(hlsl,strlen(hlsl),NULL,NULL,NULL,"vertex","vs_5_0",0,0,&blob,&error)));
    CHECK(SUCCEEDED(ID3D11Device_CreateVertexShader(s_dev,ID3D10Blob_GetBufferPointer(blob),ID3D10Blob_GetBufferSize(blob),NULL,&vs)));
    ID3D10Blob_Release(blob); if(error) ID3D10Blob_Release(error); error=NULL;
    CHECK(SUCCEEDED(D3DCompile(hlsl,strlen(hlsl),NULL,NULL,NULL,"pixel","ps_5_0",0,0,&blob,&error)));
    CHECK(SUCCEEDED(ID3D11Device_CreatePixelShader(s_dev,ID3D10Blob_GetBufferPointer(blob),ID3D10Blob_GetBufferSize(blob),NULL,&ps)));
    ID3D10Blob_Release(blob); if(error) ID3D10Blob_Release(error);
    for(step=0;step<3;step++) for(mode=0;mode<2;mode++) {
        float positions[3][4]={{-.75f,.75f,0,1},{.75f,.75f,0,1},{-.75f,-.75f,0,1}};
        float a[4]={0,0,0,1},b[4]={.5f,.5f,.5f,1};
        Surface *s; uint32_t gw,gh,zmax; D3D11_MAPPED_SUBRESOURCE map;
        a[step]=1; b[step]=.25f*(step+1);
        nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
        CHECK(setup_pipeline(&st,NV_D3D_TRIANGLES,&s,&gw,&gh,&zmax));
        CHECK(SUCCEEDED(ID3D11DeviceContext_Map(s_ctx,(ID3D11Resource *)s_raw,0,D3D11_MAP_WRITE_DISCARD,0,&map)));
        memcpy(map.pData,positions,sizeof positions);
        ID3D11DeviceContext_Unmap(s_ctx,(ID3D11Resource *)s_raw,0);
        CHECK(upload_cb(s_cb_prog,a,sizeof a) && upload_cb(s_cb_vconst,b,sizeof b));
        if(!mode) {
            /* Independent reference: restore original immutable slots after
             * the buffer DISCARDs; optimized mode keeps the initial binds. */
            ID3D11Buffer *buffers[3]={NULL,s_cb_prog,s_cb_vconst};
            s_ctx->lpVtbl->VSSetConstantBuffers(s_ctx,1,3,buffers);
            s_ctx->lpVtbl->VSSetShaderResources(s_ctx,0,1,&s_raw_srv);
            s_ctx->lpVtbl->PSSetConstantBuffers(s_ctx,1,1,&s_cb_ps);
        }
        bind_vertex_shader(vs); bind_vertex_constants(s_cb_ub);
        bind_pixel_shader(ps);
        ID3D11DeviceContext_Draw(s_ctx,3,0);
        check_slots(vs,s_cb_ub); read_pixels(mode?actual:expected);
        if(mode) {
            CHECK(!memcmp(expected,actual,sizeof actual));
            CHECK((actual[16*64+16]&0xFFFFFF)!=0);
            if(step) CHECK(memcmp(previous,actual,sizeof actual)!=0);
            memcpy(previous,actual,sizeof actual);
        }
    }
    CHECK(layout_calls==1 && srv_calls==1 && ps_cb1_calls==1);
    bind_vertex_shader(s_ub_vs); /* Lifetime cache never points to released VS. */
    bind_pixel_shader(s_psv[0]);
    ID3D11VertexShader_Release(vs); ID3D11PixelShader_Release(ps);
    puts("PASS: GPU reads freshly DISCARDed VS b2/b3 and raw t0 through retained initial binds; exact reference pixels and changing colors");
}
static void test_resize(void)
{
    NvD3DState st=draw_state(); uint32_t expected[64*64],actual[64*64];
    reference=1; nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
    triangle(&st,1,0); read_pixels(expected); reference=0;
    test_window=CreateWindowExW(0,L"STATIC",L"binding regression",WS_POPUP,0,0,256,128,NULL,NULL,NULL,NULL);
    CHECK(test_window); _putenv_s("RECOMP_ASPECT","");
    display_config(0); Sleep(350); display_config(0);
    CHECK(!surface_find(0x20000) && !s_cur_rt && !s_cur_ds);
    check_slots(s_ub_vs,s_cb_ub);
    nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
    triangle(&st,1,0); check_slots(s_ub_vs,s_cb_ub); read_pixels(actual);
    CHECK(!memcmp(expected,actual,sizeof actual));
    CHECK(layout_calls==1 && srv_calls==1 && ps_cb1_calls==1);
    DestroyWindow(test_window); test_window=NULL;
    puts("PASS: real settled resize releases surfaces, retains immutable slots and restores exact draw pixels");
}
static void check_pixel_slots(ID3D11PixelShader *expected, ID3D11SamplerState *samplers[4])
{
    ID3D11PixelShader *ps=NULL; ID3D11SamplerState *actual[4]={0}; unsigned i;
    ID3D11DeviceContext_PSGetShader(s_ctx,&ps,NULL,NULL);
    ID3D11DeviceContext_PSGetSamplers(s_ctx,0,4,actual);
    CHECK(ps==expected);
    for(i=0;i<4;i++) {
        CHECK(actual[i]==samplers[i]);
        if(actual[i]) ID3D11SamplerState_Release(actual[i]);
    }
    if(ps) ID3D11PixelShader_Release(ps);
}
static void check_draw_pixel_slots(const NvD3DState *st)
{
    ID3D11PixelShader *ps; ID3D11SamplerState *samplers[4];
    expected_pixel_bindings(st,&ps,samplers); check_pixel_slots(ps,samplers);
}
static NvD3DState textured_state(void)
{
    NvD3DState st=draw_state(); unsigned x,y;
    st.tex_used=1;
    st.tex[0].addr=0x70000; st.tex[0].color=0x12; st.tex[0].format=0x1200;
    st.tex[0].width=st.tex[0].height=8; st.tex[0].pitch=32; st.tex[0].levels=1;
    st.tex[0].filter=(1u<<16)|(1u<<24); st.tex[0].address=0x010101;
    for(y=0;y<8;y++) for(x=0;x<8;x++) {
        uint32_t color=0xFF000000u|((32+x*27)<<16)|((64+y*21)<<8)|(32+x*25);
        memcpy(ram+0x70000+(y*8+x)*4,&color,4);
    }
    return st;
}
static void test_ps_bindings(void)
{
    uint32_t expected[64*64],actual[64*64]; unsigned pass,mode;
    NvD3DState st=textured_state();
    for(pass=0;pass<6;pass++) for(mode=0;mode<2;mode++) {
        unsigned before_ps,before_sampler;
        ID3D11PixelShader *aux_ps; ID3D11SamplerState *aux_samplers[4];
        pixel_reference=!mode;
        st.flat_shade=pass&1; st.tex[0].address=0x010101;
        nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
        triangle(&st,2,0); check_draw_pixel_slots(&st);
        expected_pixel_bindings(&st,&aux_ps,aux_samplers);
        auxiliary(pass,&st);
        if(pass==1) aux_ps=s_clear_ps;
        if(pass==2) aux_ps=s_y16_ps[0];
        if(pass==3) aux_ps=s_y16d_ps[0];
        if(pass>=4) { aux_ps=s_present_ps; aux_samplers[0]=s_linear_clamp; }
        if(pass==4) aux_samplers[1]=s_point_clamp;
        check_pixel_slots(aux_ps,aux_samplers);
        triangle(&st,2,2); check_draw_pixel_slots(&st);
        before_ps=ps_shader_calls; before_sampler=ps_sampler_calls;
        triangle(&st,2,4); check_draw_pixel_slots(&st);
        CHECK(ps_shader_calls==before_ps && ps_sampler_calls==before_sampler);
        st.tex[0].address=0x030303;
        triangle(&st,2,6); check_draw_pixel_slots(&st);
        CHECK(ps_shader_calls==before_ps && ps_sampler_calls==before_sampler+1);
        read_pixels(mode?actual:expected);
        if(mode) {
            CHECK(!memcmp(expected,actual,sizeof actual));
            CHECK((actual[16*64+20]&0xFFFFFF)!=0);
        }
    }
    pixel_reference=0;
    {
        ID3D11SamplerState *slots[4]={s_point_clamp,s_linear_clamp,s_point_clamp,s_linear_clamp};
        ID3D11SamplerState *short_write[2]={NULL,s_point_clamp};
        unsigned before;
        bind_pixel_shader(NULL); bind_pixel_samplers(4,slots); check_pixel_slots(NULL,slots);
        before=ps_shader_calls; bind_pixel_shader(NULL); CHECK(ps_shader_calls==before);
        bind_pixel_samplers(2,short_write); slots[0]=NULL; slots[1]=s_point_clamp;
        check_pixel_slots(NULL,slots); before=ps_sampler_calls;
        bind_pixel_samplers(2,short_write); CHECK(ps_sampler_calls==before);
        bind_pixel_samplers(0,short_write); CHECK(ps_sampler_calls==before);
        triangle(&st,2,0); check_draw_pixel_slots(&st);
    }
    puts("PASS: exact reference textured pixels and actual PS slots across clear, color/depth Y16, blit, composite and draw; repeated/changed/partial/NULL bindings");
}
static void test_ps_failure(void)
{
    NvD3DState st=draw_state(); uint32_t before[64*64],after[64*64]; unsigned initial;
    for(initial=0;initial<2;initial++) {
        unsigned draws,maps; PSConsts last;
        nv2a_d3d_clear(&st,0xF0,0xFF000000,0,0,0,63,63);
        triangle(&st,1,0); read_pixels(before);
        last=s_uploaded_pc;
        if(initial) s_uploaded_pc_valid=0;
        st.alpha_ref+=37; draws=draw_calls; maps=ps_map_calls;
        fail_ps_map=1; triangle(&st,2,0);
        CHECK(!fail_ps_map && draw_calls==draws && ps_map_calls==maps+1);
        CHECK(!memcmp(&last,&s_uploaded_pc,sizeof last));
        if(initial) CHECK(!s_uploaded_pc_valid);
        read_pixels(after); CHECK(!memcmp(before,after,sizeof before));
        triangle(&st,2,0); CHECK(draw_calls==draws+1 && ps_map_calls==maps+2);
        CHECK(s_uploaded_pc_valid && s_uploaded_pc.alpha_ref==st.alpha_ref);
        read_pixels(after); CHECK(memcmp(before,after,sizeof before)!=0);
        triangle(&st,2,0); CHECK(ps_map_calls==maps+2);
    }
    puts("PASS: failed first/changed PS constant Maps skip DrawIndexed and preserve pixels/cache; unchanged state retries successfully and then reuses the upload");
}
static void test_ps_reset(void)
{
    NvD3DState st=textured_state(); ID3D11SamplerState *bound[4]; ID3D11PixelShader *ps;
    unsigned i,before_ps,before_sampler; uint32_t before[64*64],after[64*64];
    triangle(&st,2,0); read_pixels(before); expected_pixel_bindings(&st,&ps,bound);
    /* Table eviction releases its reference; a bound sampler remains owned
     * by the actual context and therefore cannot be recycled underneath it. */
    for(i=0;i<STATE_SLOTS*4;i++) {
        NvD3DTexture t=st.tex[0]; t.border=0xFF000000u+i;
        CHECK(sampler(&t,1,0));
    }
    check_pixel_slots(ps,bound);
    triangle(&st,2,0); check_draw_pixel_slots(&st); read_pixels(after);
    CHECK(!memcmp(before,after,sizeof before));
    ID3D11DeviceContext_ClearState(s_ctx);
    bind_fixed_resources(); CHECK(!s_bound_ps_valid && !s_bound_ps_samplers_valid);
    before_ps=ps_shader_calls; before_sampler=ps_sampler_calls;
    triangle(&st,2,0); check_draw_pixel_slots(&st); read_pixels(after);
    CHECK(ps_shader_calls==before_ps+1 && ps_sampler_calls==before_sampler+1);
    CHECK(!memcmp(before,after,sizeof before));
    test_window=CreateWindowExW(0,L"STATIC",L"pixel binding resize",WS_POPUP,0,0,256,128,NULL,NULL,NULL,NULL);
    CHECK(test_window); _putenv_s("RECOMP_ASPECT","");
    display_config(0); Sleep(350); display_config(0);
    CHECK(!surface_find(0x20000));
    triangle(&st,2,0); check_draw_pixel_slots(&st); read_pixels(after);
    CHECK(!memcmp(before,after,sizeof before));
    DestroyWindow(test_window); test_window=NULL;
    puts("PASS: context-owned sampler survives cache eviction; actual ClearState plus fixed-resource reset restores PS binds and pixels; settled resize remains valid");
}

/* The title can identify metadata from the original arrays while cleaning
 * only the private GPU copy. Nonzero lo and overlapping attributes exercise
 * grouped interleaved uploads, not merely a callback invoked by a fixture. */
static const NvD3DAttrib *overlay_source;
static const uint8_t *overlay_copy_base;
static size_t overlay_tag_offset;
static unsigned overlay_sanitize_calls, overlay_place_calls;
static int overlay_place(const NvOverlayDraw *d, float *dx, float *squeeze, float *shift)
{
    CHECK(d->attrib == overlay_source && d->lo == 2 && d->count == 3 && d->ni == 3);
    CHECK(d->attrib[2].type == 1 && d->attrib[2].stride == 16);
    CHECK(d->gw == 640 && d->gh == 480 && d->narrow < 0.76f);
    overlay_place_calls++;
    *squeeze = d->narrow;
    return 0;
}
static void overlay_sanitize(uint8_t *copy, const NvD3DAttrib *a, unsigned attribute,
                             uint32_t lo, uint32_t count)
{
    unsigned n;
    CHECK(a == overlay_source + attribute && lo == 2 && count == 3);
    for (n = 0; n < count; ++n)
        CHECK(!memcmp(copy + n * a->stride, a->data + (lo + n) * a->stride, attribute == 9 ? 4 : 8));
    overlay_sanitize_calls++;
    if (attribute == 0) overlay_copy_base = copy;
    if (attribute != 2) return;
    overlay_tag_offset = (size_t)(copy - overlay_copy_base);
    for (n = 0; n < count; ++n) memset(copy + n * a->stride, 0, 4);
}
static void test_overlay_upload(void)
{
    float positions[6][4] = {{0}};
    int16_t packed[6][8], original[6][8];
    NvD3DAttrib attr[16] = {0}; float def[16][4] = {{0}};
    const uint32_t idx[3] = {2, 3, 4};
    NvD3DState st = draw_state();
    NvOverlayHooks hooks = {0};
    ID3D11Buffer *stage = NULL; D3D11_BUFFER_DESC desc = {0};
    D3D11_MAPPED_SUBRESOURCE mapped; D3D11_BOX box = {0, 0, 0, 512, 1, 1};
    unsigned n;
    for (n = 0; n < 6; ++n) {
        positions[n][0] = 8.0f + n * 4; positions[n][1] = 8.0f + (n & 1) * 32;
        positions[n][3] = 1.0f;
        for (unsigned k = 0; k < 8; ++k) packed[n][k] = (int16_t)(n * 16 + k + 1);
    }
    memcpy(original, packed, sizeof packed);
    attr[0] = (NvD3DAttrib){1, 2, 4, 16, (const uint8_t *)positions};
    attr[2] = (NvD3DAttrib){1, 1, 4, 16, (const uint8_t *)packed};
    attr[9] = (NvD3DAttrib){1, 1, 2, 16, (const uint8_t *)packed + 8};
    def[3][0] = def[3][3] = 1;
    overlay_source = attr;
    hooks.place = overlay_place; hooks.sanitize_vertex_upload = overlay_sanitize;
    nv2a_d3d_set_overlay_hooks(&hooks);
    s_raw_nooverwrite = 0;
    nv2a_d3d_draw_ub(&st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 2, 4, NULL);
    CHECK(overlay_place_calls == 0 && overlay_sanitize_calls == 3);
    CHECK(!memcmp(original, packed, sizeof packed));
    desc.ByteWidth = 512; desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(s_dev, &desc, NULL, &stage)));
    ID3D11DeviceContext_CopySubresourceRegion(s_ctx, (ID3D11Resource *)stage, 0, 0, 0, 0,
                                             (ID3D11Resource *)s_raw, 0, &box);
    CHECK(SUCCEEDED(ID3D11DeviceContext_Map(s_ctx, (ID3D11Resource *)stage, 0,
                                           D3D11_MAP_READ, 0, &mapped)));
    for (n = 0; n < 3; ++n) {
        const int16_t *uploaded = (const int16_t *)((const uint8_t *)mapped.pData +
                                                    overlay_tag_offset + n * 16);
        CHECK(!uploaded[0] && !uploaded[1]);
        CHECK(!memcmp(uploaded + 2, original[n + 2] + 2, 8));
    }
    ID3D11DeviceContext_Unmap(s_ctx, (ID3D11Resource *)stage, 0);
    ID3D11Buffer_Release(stage);
    st.color_addr = 0x80000; st.color_pitch = 640 * 4;
    st.zeta_addr = 0; st.depth_test = 0; st.clip_w = 640; st.clip_h = 480;
    s_aspect = 16.0f / 9.0f;
    nv2a_d3d_draw_ub(&st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 2, 4, positions + 2);
    CHECK(overlay_place_calls == 1 && overlay_sanitize_calls == 6);
    CHECK(!memcmp(original, packed, sizeof packed));
    nv2a_d3d_set_overlay_hooks(NULL);
    nv2a_d3d_draw_ub(&st, NV_D3D_TRIANGLES, attr, def, 0, idx, 3, 2, 4, positions + 2);
    CHECK(overlay_place_calls == 1 && overlay_sanitize_calls == 6);
    puts("PASS: original attributes reach title placement; interleaved metadata cleanup changes only GPU copies, including non-wide draws; clearing hooks retires callbacks");
}

static void benchmark(void)
{
    const unsigned iterations=200000;
    double old[7],cached[7]; LARGE_INTEGER f; unsigned sample,i,j;
    QueryPerformanceFrequency(&f);
    bind_vertex_shader(s_ub_vs); bind_vertex_constants(s_cb_ub);
    for(sample=0;sample<7;sample++) {
        unsigned order;
        for(order=0;order<2;order++) {
            int legacy=(sample+order)%2; int64_t begin=qpc(),end;
            for(i=0;i<iterations;i++) {
                if(legacy) legacy_bind();
                else cached_bind();
            }
            end=qpc(); (legacy?old:cached)[sample]=(end-begin)*1e9/f.QuadPart/iterations;
        }
    }
    for(i=0;i<7;i++) for(j=i+1;j<7;j++) {
        double t;
        if(old[j]<old[i]) {t=old[i];old[i]=old[j];old[j]=t;}
        if(cached[j]<cached[i]) {t=cached[i];cached[i]=cached[j];cached[j]=t;}
    }
    printf("BIND %u repetitions x 5 actual legacy COM calls, 7 interleaved samples; median total %.3f ms legacy / %.3f ms retained\n",
           iterations,old[3]*iterations/1e6,cached[3]*iterations/1e6);
    printf("BIND actual driver median: five legacy binds %.2f ns/draw, retained binding checks %.2f ns/draw; %.3f ms saved per 800 draws\n",old[3],cached[3],(old[3]-cached[3])*800/1e6);
}
static unsigned host_draw_calls;
static int host_clear(const NvHostTarget *target,void *user)
{
    CHECK(target->device == s_dev && target->context == s_ctx);
    CHECK(target->width == 64 && target->height == 64 && target->depth_bits == 24);
    CHECK(target->samples == 1 && target->colour && target->depth);
    ++host_draw_calls;
    ID3D11DeviceContext_ClearState((ID3D11DeviceContext *)target->context);
    if (user) {
        const float green[4] = {0,1,0,1};
        ID3D11DeviceContext_ClearRenderTargetView((ID3D11DeviceContext *)target->context,
                                                 (ID3D11RenderTargetView *)target->colour,green);
    }
    return user != NULL;
}
static void test_host_draw(void)
{
    NvD3DState st = draw_state(), missing;
    uint32_t pixels[64*64];
    triangle(&st,0,0);
    CHECK(nv2a_d3d_host_draw(&st,host_clear,&st));
    read_pixels(pixels); CHECK((pixels[16*64+16]&0xFFFFFF) == 0x00FF00);
    CHECK(!nv2a_d3d_host_draw(&st,host_clear,NULL));
    read_pixels(pixels); CHECK((pixels[16*64+16]&0xFFFFFF) == 0x00FF00);
    CHECK(!nv2a_d3d_host_draw(NULL,host_clear,&st));
    CHECK(!nv2a_d3d_host_draw(&st,NULL,&st));
    missing = st; missing.zeta_addr = 0;
    CHECK(!nv2a_d3d_host_draw(&missing,host_clear,&st)); CHECK(host_draw_calls == 2);
    triangle(&st,0,0); read_pixels(pixels);
    CHECK((pixels[16*64+16]&0xFFFFFF) == 0xFF0000);
    puts("PASS: native target callback ordering, metadata, rejection and guest binding restoration");
}

/* A small pass bound at the scene's depth address (a 40x30-style brightness
 * probe) must not take the scene depth's place for a later Y16 read of it. */
static void test_depth_texture(void)
{
    NvD3DState st = draw_state(), probe = st;
    uint32_t p = phys(st.zeta_addr);
    Depth *scene, *little;
    nv2a_d3d_clear(&st,0xF3,0xFF000000,0x80000000u,0,0,63,63);
    scene = depth_find(p); CHECK(scene && scene->w == 64 && scene->h == 64);
    probe.color_addr = 0x60000; probe.color_pitch = 8*4;
    probe.clip_w = 8; probe.clip_h = 6; probe.stencil_test = 1;
    nv2a_d3d_clear(&probe,0x02,0,0,0,0,7,5);
    little = depth_find(p); CHECK(little && little != scene && little->w == 8 && little->h == 6);
    CHECK(depth_find_texture(p,128,64) == scene);      /* each depth pixel is two texels */
    CHECK(depth_find_texture(p,16,6) == little);        /* both cover it: the last bound */
    CHECK(depth_find_texture(p,256,128) == little);     /* none covers it: the last bound */
    CHECK(depth_find_texture(p+0x1000,128,64) == NULL);
    puts("PASS: a Y16 read of the scene depth skips a smaller depth buffer bound at its address since");
}

int main(int argc,char **argv)
{
    CHECK(argc==2);
    _putenv_s("RECOMP_MSAA","1"); _putenv_s("RECOMP_RENDER_SCALE","1");
    _putenv_s("RECOMP_ASPECT","4:3"); _putenv_s("RECOMP_PS_SPEC","0");
    _putenv_s("RECOMP_FPS_LIMIT","0");
    if(!strcmp(argv[1],"init_failure")) {
        fail_buffers=1; CHECK(!nv2a_d3d_init()); CHECK(!nv2a_d3d_init());
        CHECK(!s_bound_vs_valid && !s_bound_vs_b0_valid);
        CHECK(!s_bound_ps_valid && !s_bound_ps_samplers_valid);
        CHECK(!layout_calls && !srv_calls && !ps_cb1_calls && !vs_cb_calls);
        puts("PASS: failed initialization publishes no fixed bindings and cannot use cached state"); return 0;
    }
    CHECK(nv2a_d3d_init()); check_slots(NULL,NULL);
    if(!strcmp(argv[1],"pixels")) test_pixels();
    else if(!strcmp(argv[1],"host_draw")) test_host_draw();
    else if(!strcmp(argv[1],"fixed_updates")) test_fixed_updates();
    else if(!strcmp(argv[1],"failures")) test_failures();
    else if(!strcmp(argv[1],"resize")) test_resize();
    else if(!strcmp(argv[1],"ps_bindings")) test_ps_bindings();
    else if(!strcmp(argv[1],"ps_failure")) test_ps_failure();
    else if(!strcmp(argv[1],"ps_reset")) test_ps_reset();
    else if(!strcmp(argv[1],"overlay_upload")) test_overlay_upload();
    else if(!strcmp(argv[1],"depth_texture")) test_depth_texture();
    else if(!strcmp(argv[1],"benchmark")) benchmark();
    else CHECK(0);
    return 0;
}

/* These renderer-only fixtures never request executor frame replay. */
void nv2a_pb_interp_enable(int on) { (void)on; abort(); }
long nv2a_pb_frames_done(void) { abort(); return 0; }
int nv2a_pb_replay(float t) { (void)t; abort(); return 0; }
