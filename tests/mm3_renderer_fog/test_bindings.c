/* Actual production MM3 fog over the generic renderer's GPU fixture. */
#define main generic_bindings_main
#include "../../tools/xboxrecomp/tests/nv2a_bindings/test_bindings.c"
#undef main
#include "../../src/mods/render_fog.c"
static uint32_t native_fog_pixel(NvD3DState *st, float depth, float start)
{
    float rhw = 1.f/(depth*16777216.f);
    float positions[3][4] = {{8,8,0,0},{56,8,0,0},{8,56,0,0}};
    NvD3DAttrib attr[16] = {0}; float def[16][4] = {{0}};
    uint32_t idx[3] = {0,1,2}, pixels[64*64];
    for (unsigned i = 0; i < 3; ++i) {
        positions[i][3] = rhw;
        positions[i][2] = (1000.f/990.f - 10.f/(990.f*depth))*16777215.f;
    }
    attr[0] = (NvD3DAttrib){1,2,4,16,(const uint8_t *)positions};
    def[3][0] = 1; def[3][3] = .5f;
    pixel_state = st;
    nv2a_d3d_clear(st,0xF1,0x80FF0000,0xFFFFFF00,0,0,63,63);
    nv2a_d3d_draw_ub(st,NV_D3D_TRIANGLES,attr,def,0,idx,3,0,2,NULL);
    read_pixels(pixels); uint32_t alpha = pixels[16*64+16] >> 24;
    /* At the postprocess marker, UI/glare setup may already have disabled
     * W-buffer writes. The stored scene depth still contains linear W. */
    NvD3DState post = *st; post.z_perspective = 0; post.depth_mask = 0;
    mm3_render_fog(&post,1000.f/16777216.f,1000,start,800,0xFF0000FF,0,NULL);
    read_pixels(pixels);
    CHECK((pixels[16*64+16]>>24) == alpha); /* Fog preserves destination alpha. */
    uint32_t colour = pixels[16*64+16]&0xFFFFFF;
    mm3_render_fog(&post,1000.f/16777216.f,1000,start,800,0xFF0000FF,MM3_FOG_SKY_MASK,NULL);
    read_pixels(pixels);
    CHECK((pixels[16*64+16]>>24) == (isfinite(start) ? (depth >= .99999f ? 255u : 0u) : alpha));
    uint32_t masked_colour = pixels[16*64+16]&0xFFFFFF;
    /* The next panorama draw must affect sky alone, never nearby geometry. */
    post.depth_test = 0; post.blend_enable = 1;
    post.blend_sfactor = 0x304; post.blend_dfactor = 0x305;
    triangle(&post,1,0); read_pixels(pixels);
    if (isfinite(start)) CHECK((pixels[16*64+16]&0xFFFFFF) == (depth >= .99999f ? 0x00FF00 : masked_colour));
    return colour;
}

static void test_native_fog(void)
{
    NvD3DState st = draw_state(); uint32_t pixels[64*64];
    st.z_perspective = 1; st.depth_mask = 1;
    CHECK(native_fog_pixel(&st,.1f,200) == 0xFF0000);
    uint32_t middle = native_fog_pixel(&st,.5f,200);
    CHECK(abs((int)(middle>>16)-81) <= 2 && abs((int)(middle&255)-174) <= 2);
    uint32_t distant = native_fog_pixel(&st,.8f,200);
    CHECK((distant>>16) <= 4 && (distant&255) >= 251);
    CHECK(native_fog_pixel(&st,1,200) == 0xFF0000); /* Default keeps clear/sky colour. */
    mm3_render_fog(&st,1000.f/16777216.f,1000,200,800,0xFF0000FF,MM3_FOG_SKY_FILL,NULL);
    read_pixels(pixels);
    CHECK((pixels[16*64+16]&0xFFFFFF) == 0x0000FF); /* No panorama: clear depth becomes untextured air. */
    /* The sky mask fades continuously above the camera's world horizon. */
    const float sky_planes[2][4] = {{0,1,0,1},{.6f,.8f,-.1f,1}};
    for (unsigned plane = 0; plane < 2; ++plane) {
        nv2a_d3d_clear(&st,0xF1,0xFF0000FF,0xFFFFFF00,0,0,63,63);
        const float *sky_fade = sky_planes[plane];
        mm3_render_fog(&st,1000.f/16777216.f,1000,200,800,0xFF0000FF,
            MM3_FOG_SKY_MASK | MM3_FOG_SKY_FADE,sky_fade);
        read_pixels(pixels);
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x) {
            float h = sky_fade[0]*(2.f*(x+.5f)/64-1) + sky_fade[1]*(1.f-2.f*(y+.5f)/64) + sky_fade[2];
            float fade = fminf(1,fmaxf(0,h));
            int expected = (int)roundf(255*fade*fade*(3-2*fade));
            CHECK(abs((int)(pixels[y*64+x]>>24)-expected) <= 1);
        }
        int sky_alpha = pixels[16*64+16]>>24;
        NvD3DState panorama = st; panorama.depth_test = panorama.depth_mask = 0;
        panorama.blend_enable = 1; panorama.blend_sfactor = 0x304; panorama.blend_dfactor = 0x305;
        triangle(&panorama,1,0); read_pixels(pixels);
        CHECK(abs((int)((pixels[16*64+16]>>8)&255)-sky_alpha) <= 1);
        CHECK(abs((int)(pixels[16*64+16]&255)-(255-sky_alpha)) <= 1);
    }
    CHECK(native_fog_pixel(&st,.8f,NAN) == 0xFF0000);
    native_fog_pixel(&st,.8f,200);
    st.depth_test = 0; st.depth_mask = 0;
    triangle(&st,1,0); read_pixels(pixels);
    CHECK((pixels[16*64+16]&0xFFFFFF) == 0x00FF00); /* UI RGB after the pass stays unchanged. */
    puts("PASS: dedicated fog pass reads depth, preserves near/sky/alpha, optionally rebuilds skyline alpha, fades mid/far, rejects invalid ranges, and restores subsequent UI rendering");
}

int main(int argc,char **argv) {
 CHECK(argc == 2); _putenv_s("RECOMP_MSAA",!strcmp(argv[1],"msaa") ? "4" : "1");
 _putenv_s("RECOMP_RENDER_SCALE","1"); _putenv_s("RECOMP_ASPECT","4:3");
 _putenv_s("RECOMP_PS_SPEC","0"); _putenv_s("RECOMP_FPS_LIMIT","0");
 CHECK(nv2a_d3d_init()); test_native_fog(); return 0;
}
