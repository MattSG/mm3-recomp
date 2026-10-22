/* Exercise title-owned overlay callbacks with real runtime public types.
 * No D3D device, game process or saved profile is opened. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mods.h"

static int paused, widescreen = 1;
static const uint32_t map_base = 0x823E3000u, map_marker = 0x8246A000u;
int mm3_mod_on(int mod) { return mod != MM3_MOD_WIDESCREEN || widescreen; }
int mm3_pause_ui_active(void) { return paused; }
int mm3_map_texture_role(uint32_t address) {
    return address == map_base ? 1 : address == map_marker ? 2 : 0;
}
#include "../src/mods/render_policy.c"
const uint32_t (*nv2a_vsh_program(uint32_t *start, uint32_t *version))[4] { (void)start; (void)version; return NULL; }
const float (*nv2a_vsh_constants(uint32_t *version))[4] { (void)version; return NULL; }

#include "../src/mods/widescreen_hud.c"

static int16_t packed_uv[64][8];
static NvD3DAttrib submitted_uv;
typedef struct { float x[16], squeeze, shift; int use_dx; } Result;

static void close_float(float actual, float expected) {
    if (fabsf(actual - expected) >= 0.0002f) {
        fprintf(stderr, "overlay mismatch: got %.6f, expected %.6f\n", actual, expected);
        abort();
    }
}
static void quad(float p[4][4], float left, float right, float top, float bottom) {
    float q[4][4] = {{left,top,0,1}, {right,top,0,1}, {right,bottom,0,1}, {left,bottom,0,1}};
    memcpy(p, q, sizeof q);
}
static void tag_route(uint32_t first, uint32_t count) {
    uint32_t i;
    memset(packed_uv, 0, sizeof packed_uv);
    for (i = first; i < first + count; ++i) {
        packed_uv[i][4] = 23117; packed_uv[i][5] = -17493;
    }
    submitted_uv.enabled = 1; submitted_uv.type = 1;
    submitted_uv.size = 4; submitted_uv.stride = 16;
    submitted_uv.data = (const uint8_t *)&packed_uv[0][4];
}
static Result draw(const float (*pos)[4], uint32_t count, const uint32_t *idx,
                   uint32_t ni, uint32_t lo, float aspect, int textured,
                   int reads_wide, uint32_t texture) {
    NvD3DAttrib attr[16] = {{0}};
    NvOverlayDraw d;
    Result r = {{0}, 1, 0, 0};
    float dx[16] = {0};
    uint32_t i;
    assert(count <= 16);
    memset(&d, 0, sizeof d);
    attr[2] = submitted_uv;
    d.pos = pos; d.idx = idx; d.attrib = attr;
    d.ni = ni; d.lo = lo; d.count = count; d.per = 3;
    d.gw = 640; d.gh = 480; d.narrow = (4.0f / 3.0f) / aspect;
    d.x0 = d.y0 = 1e30f; d.x1 = d.y1 = -1e30f;
    d.tex = texture; d.textured = textured; d.reads_wide = reads_wide;
    for (i = 0; i < ni; ++i) {
        const float *v = pos[idx[i] - lo];
        if (v[0] < d.x0) d.x0 = v[0];
        if (v[0] > d.x1) d.x1 = v[0];
        if (v[1] < d.y0) d.y0 = v[1];
        if (v[1] > d.y1) d.y1 = v[1];
    }
    r.use_dx = mm3_hud_hooks.place(&d, dx, &r.squeeze, &r.shift);
    for (i = 0; i < count; ++i) {
        float px = pos[i][0] + (r.use_dx ? dx[i] : 0);
        r.x[i] = ((px * (2.0f / 640) - 1) * r.squeeze + r.shift + 1) * 320;
    }
    return r;
}
static Result box(float left, float right, float top, float bottom,
                  float aspect, int textured, int reads_wide, uint32_t texture) {
    float p[4][4];
    const uint32_t idx[6] = {17,18,19,17,19,20};
    quad(p, left, right, top, bottom);
    return draw(p, 4, idx, 6, 17, aspect, textured, reads_wide, texture);
}
static void seed(void) {
    memset(&submitted_uv, 0, sizeof submitted_uv);
    paused = 0; widescreen = 1;
    mm3_hud_set_edge(0); mm3_hud_set_edge(1);
    mm3_hud_set_start(50, 320, 178, 448);
    mm3_hud_hooks.frame();
    s_hud_prev = 1;
    s_hud_tex[0] = 501; s_hud_tex_n = 1;
}
static void map_contract(float aspect) {
    float n = (4.0f / 3.0f) / aspect;
    Result base, marker;
    int x;
    seed();
    base = box(50, 178, 320, 448, aspect, 1, 0, map_base);
    close_float(base.x[0], 50*n); close_float(base.x[1], 178*n);
    for (x = -20; x <= 660; x += 10) {
        marker = box((float)x, (float)x + 16, 180, 196, aspect, 1, 0, map_marker);
        close_float(marker.x[0], base.x[0] + ((float)x - 50)*n);
        close_float(marker.x[1] - marker.x[0], 16*n);
    }
    /* Zoomed/full maps use their own base anchor even when the minimap was
     * absent in the previous frame. Icons never choose an independent edge. */
    mm3_hud_hooks.frame(); s_hud_prev = 0;
    base = box(200, 440, 100, 400, aspect, 1, 0, map_base);
    marker = box(490, 506, 180, 196, aspect, 1, 0, map_marker);
    close_float(marker.x[0], base.x[0] + 290*n);
    mm3_hud_hooks.frame();
    assert(!InterlockedCompareExchange(&s_frame_map_valid, 0, 0));
    /* Missing base cannot reuse an anchor left by another map/frame. */
    marker = box(490, 506, 180, 196, aspect, 1, 0, map_marker);
    close_float(marker.shift, 0);
}
static void pause_contract(float aspect) {
    Result a, b, c, dimmer;
    float n = (4.0f / 3.0f) / aspect;
    int i;
    seed(); paused = 1;
    dimmer = box(0, 640, 0, 480, aspect, 0, 0, 0);
    close_float(dimmer.x[0], 0); close_float(dimmer.x[1], 640);
    for (i = 0; i < 2; ++i) {
        s_frame_tag_window = i;
        a = box(208, 232, 170, 200, aspect, 1, 0, 901);
        b = box(232, 408, 170, 200, aspect, 0, 0, 0);
        c = box(408, 432, 170, 200, aspect, 1, 0, 902);
        close_float(a.x[0], 320 + (208-320)*n);
        close_float(a.x[1], b.x[0]); close_float(b.x[1], c.x[0]);
        assert(s_frame_tag_window == i && s_hud_tex_n == 1);
    }
}
static void route_contract(float aspect) {
    float p[12][4], n = (4.0f / 3.0f) / aspect;
    const uint32_t idx[18] = {0,1,2,0,2,3,4,5,6,4,6,7,8,9,10,8,10,11};
    Result base, r;
    uint8_t copy[64], before[64];
    int i;
    seed();
    base = box(50, 178, 320, 448, aspect, 1, 0, map_base);
    quad(p, 20, 80, 100, 120); quad(p+4, 310, 330, 100, 120);
    quad(p+8, 330, 390, 100, 120); tag_route(4, 4);
    r = draw(p, 12, idx, 18, 0, aspect, 0, 0, 0);
    close_float(r.x[0], 20*n); close_float(r.x[1], 80*n);
    for (i = 4; i < 8; ++i) close_float(r.x[i], base.x[0] + (p[i][0]-50)*n);
    close_float(r.x[8], 640-310*n); close_float(r.x[9], 640-250*n);
    /* Full-screen dimmers in a shared material stay full width while the
     * tagged route alone receives the map transform. */
    quad(p, 0, 640, 0, 480); quad(p+4, 310, 330, 180, 196); tag_route(4, 4);
    r = draw(p, 8, idx, 12, 0, aspect, 0, 0, 0);
    close_float(r.x[0], 0); close_float(r.x[1], 640);
    for (i = 4; i < 8; ++i) close_float(r.x[i], base.x[0] + (p[i][0]-50)*n);
    /* Cleanup applies to the private copied attribute; tags in queued guest
     * memory remain available for classification and unrelated lanes survive. */
    tag_route(17, 4);
    memcpy(copy, submitted_uv.data + 17*16, sizeof copy); memcpy(before, copy, sizeof copy);
    mm3_hud_hooks.sanitize_vertex_upload(copy, &submitted_uv, 2, 17, 4);
    for (i = 0; i < 4; ++i) {
        assert(!memcmp(copy + i*16, "\0\0\0\0", 4));
        assert(!memcmp(copy + i*16+4, before + i*16+4, 12));
        assert(packed_uv[17+i][4] == 23117 && packed_uv[17+i][5] == -17493);
    }
    memcpy(copy, before, sizeof copy);
    mm3_hud_hooks.sanitize_vertex_upload(copy, &submitted_uv, 3, 17, 4);
    assert(!memcmp(copy, before, sizeof copy));
    widescreen = 0;
    r = box(50, 178, 320, 448, aspect, 1, 0, map_base);
    close_float(r.x[0], 50); close_float(r.x[1], 178);
    mm3_hud_hooks.sanitize_vertex_upload(copy, &submitted_uv, 2, 17, 4);
    assert(!memcmp(copy, "\0\0\0\0", 4)); /* Required even in 4:3. */
}
int main(void) {
    assert(mm3_hud_hooks.vertex_alignment == mm3_render_vertex_alignment);
    static const float aspects[] = {1.6f, 16.0f/9.0f, 21.0f/9.0f};
    unsigned i;
    for (i = 0; i < sizeof aspects/sizeof aspects[0]; ++i) {
        map_contract(aspects[i]); pause_contract(aspects[i]); route_contract(aspects[i]);
    }
    puts("PASS overlay: common map/marker anchor, centred pause caps, mixed route/HUD/dimmer draws, private UV cleanup and mod gating");
    return 0;
}
