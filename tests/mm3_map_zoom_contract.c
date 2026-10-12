/* The stock map rebuilds its scalar before every draw. Execute the real native
 * road and checkpoint geometry with only resource/input/GPU boundaries faked. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
static unsigned char memory[0x440000];
static const uint32_t stack = 0x80000, node = 0x10000, second_node = 0x10200;
static const uint32_t player = 0x20000, players = 0x21000, property = 0x22000;
static const uint32_t layout = 0x23000, markers = 0x24000;
typedef struct { uint32_t texture; float xy[4][2]; } Draw;
static Draw draws[16];
static unsigned draw_count;
static int player_present = 1;
static int zoom_markers;
static const char *check = "speed";

#include "production_map_zoom.c"

static void push_float(double value) {
    g_fp_top = (g_fp_top + 7) & 7;
    g_fp_stack[g_fp_top] = value;
}
void retail_marker(void);
void zoom_marker(void);
static void fake_icall(uint32_t address) {
    if (address == 0x100) { push_float(0); g_esp += 8; return; }
    if (address == 0x0011CAE8) {
        if (zoom_markers) zoom_marker(); else retail_marker();
        return;
    }
    if (address == 0x0002DC7E) {
        memset((void *)XBOX_PTR(g_ecx), 0, 0x24); g_esp += 4; return;
    }
    fprintf(stderr, "unexpected map indirect call %08X\n", address);
    abort();
}
void sub_0011E76F(void) { g_eax = 0x25000; g_esp += 4; }
void sub_0011E647(void) { g_eax = player_present ? players : 0; g_esp += 4; }
void sub_001F0399(void) {
    g_eax = (uint32_t)(int32_t)g_fp_stack[g_fp_top];
    g_fp_top = (g_fp_top + 1) & 7; g_esp += 4;
}
void sub_001F33A2(void) { g_eax = 0; g_esp += 4; }
void sub_001F3979(void) { assert(!"unreachable race input branch"); }
void sub_0004EE0D(void) { g_eax = 1; g_esp += 4; }
void sub_002414D8(void) { assert(!"unreachable alternate marker color branch"); }
void sub_00012D29(void) { assert(!"unreachable wrapped animation branch"); }
void sub_000629BE(void) { g_eax = 0; g_esp += 4; }
void sub_0006AEB6(void) { assert(!"unreachable map movement branch"); }
void sub_001E4A45(void) { g_eax = 0xFFFFFFE8u; g_esp += 4; }
void sub_001C1585(void) {
    uint32_t texture = MEM32(g_esp + 4), vertices = MEM32(g_esp + 8);
    unsigned i;
    assert(draw_count < sizeof draws / sizeof draws[0]);
    draws[draw_count].texture = texture;
    for (i = 0; i < 4; ++i) {
        draws[draw_count].xy[i][0] = MEMF(vertices + 0xC + i * 0x24);
        draws[draw_count].xy[i][1] = MEMF(vertices + 0x10 + i * 0x24);
    }
    ++draw_count; g_esp += 12;
}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target, saved, site) fake_icall(target)
#undef RECOMP_ICALL_SAFE
#define RECOMP_ICALL_SAFE(target, saved) fake_icall(target)
#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#include "native_map_zoom.inc"

static void near(float got, float expected) {
    if (!isfinite(got) || fabsf(got - expected) > 0.00015f) {
        fprintf(stderr, "map zoom mismatch (%s): got %.9g expected %.9g\n", check, got, expected);
        abort();
    }
}
static void fresh_scale(uint32_t identity, uint32_t record, float scale) {
    MEMF(identity + 0x50) = scale;
    mm3_map_zoom_apply(identity, record);
}
static float settle(uint32_t identity, int speed, float baseline) {
    unsigned i;
    MEM32(player + 4) = (uint32_t)speed;
    mm3_map_zoom_reset(identity);
    fresh_scale(identity, player, baseline);
    for (i = 0; i < 600; ++i) mm3_map_zoom_update(identity, 1.0f / 60.0f);
    fresh_scale(identity, player, baseline);
    return baseline / MEMF(identity + 0x50);
}
static void abi_start(uint32_t identity) {
    g_eax = 0xABABABAB; g_ecx = identity; g_edx = 0x12345678;
    g_esp = stack; g_ebp = 0x71000; g_seh_ebp = g_ebp;
    g_ebx = 0x11223344; g_esi = 0x22334455; g_edi = 0x33445566;
    g_fp_top = 0; g_fp_control_word = 0x37F; g_df = 0;
    memset(g_fp_stack, 0, sizeof g_fp_stack);
    MEM32(stack) = 0xDEADBEEF;
    MEM32(stack + 4) = 0x25000; MEM32(stack + 8) = layout;
    MEM32(stack + 12) = 0x26262626; MEM32(stack + 16) = layout;
    MEM32(stack + 20) = 0x27272727; MEM32(stack + 24) = 0xA5A5A5A5;
}
static void abi_end(unsigned consumed) {
    assert(g_esp == stack + consumed && g_fp_top == 0);
    assert(g_ebx == 0x11223344 && g_esi == 0x22334455 && g_edi == 0x33445566);
    assert(MEM32(stack + 24) == 0xA5A5A5A5);
}
static void speed_contract(void) {
    const int speeds[] = {0, 19, 20, 45, 70, 95, 120, 121, 400, -120};
    const float spans[] = {1, 1, 1, 1.0390625f, 1.125f, 1.2109375f, 1.25f, 1.25f, 1.25f, 1.25f};
    unsigned i;
    for (i = 0; i < sizeof speeds / sizeof speeds[0]; ++i)
        near(settle(node, speeds[i], 2), spans[i]);
    mm3_map_zoom_reset(node);
    MEM32(player + 4) = 120; fresh_scale(node, player, 2);
    near(MEMF(node + 0x50), 2); /* Draw announces the target, not a time step. */
    for (i = 0; i < 30; ++i) fresh_scale(node, player, 2);
    near(MEMF(node + 0x50), 2);
    mm3_map_zoom_update(node, 0.1f); fresh_scale(node, player, 2);
    near(2 / MEMF(node + 0x50), 1 + .25f * (1 - expf(-.1f / .6f)));
    mm3_map_zoom_reset(node); fresh_scale(node, player, 2);
    mm3_map_zoom_update(node, 10); fresh_scale(node, player, 2);
    near(2 / MEMF(node + 0x50), 1 + .25f * (1 - expf(-.1f / .6f)));
    for (i = 0; i < 4; ++i) {
        const float invalid[] = {0, -.1f, NAN, INFINITY};
        float before = MEMF(node + 0x50);
        mm3_map_zoom_update(node, invalid[i]); fresh_scale(node, player, 2);
        near(MEMF(node + 0x50), before);
    }
    /* Equal simulation time behaves the same at different update rates. */
    mm3_map_zoom_reset(node); mm3_map_zoom_reset(second_node);
    fresh_scale(node, player, 2); fresh_scale(second_node, player, 2);
    for (i = 0; i < 60; ++i) mm3_map_zoom_update(node, 1.0f / 60.0f);
    for (i = 0; i < 30; ++i) mm3_map_zoom_update(second_node, 1.0f / 30.0f);
    fresh_scale(node, player, 2); fresh_scale(second_node, player, 2);
    near(MEMF(node + 0x50), MEMF(second_node + 0x50));
    MEM32(player + 4) = 0; fresh_scale(node, player, 2);
    for (i = 0; i < 60; ++i) {
        mm3_map_zoom_update(node, 1.0f / 60.0f);
        mm3_map_zoom_update(second_node, 1.0f / 60.0f);
    }
    fresh_scale(node, player, 2);
    MEM32(player + 4) = 120; fresh_scale(second_node, player, 2);
    assert(MEMF(node + 0x50) > MEMF(second_node + 0x50));
    mm3_map_zoom_reset(node); fresh_scale(node, player, 2); near(MEMF(node + 0x50), 2);
    fresh_scale(node, 0, 2); near(MEMF(node + 0x50), 2);
    mm3_map_zoom_reset(0); mm3_map_zoom_update(0, .1f); mm3_map_zoom_apply(0, player);
    for (i = 0; i < 3; ++i) {
        const float invalid[] = {0, -1, INFINITY};
        fresh_scale(node, player, invalid[i]);
        assert(MEMF(node + 0x50) == invalid[i]);
    }
}
static Draw *texture_draw(Draw *list, unsigned count, unsigned texture) {
    unsigned i;
    for (i = 0; i < count; ++i) if (list[i].texture == texture) return &list[i];
    fprintf(stderr, "missing native texture %u (draws=%u)\n", texture, count); abort();
}
static void native_contract(void) {
    unsigned large, turn, split, i, axis;
    Draw original[16];
    for (split = 0; split < 2; ++split) for (large = 0; large < 2; ++large) for (turn = 0; turn < 4; ++turn) {
        float native_scale = 1.2f * (split ? .75f : 1.0f);
        float span = 1.25f, cx = large ? 320.0f : 114.0f, cy = large ? 240.0f : 384.0f;
        unsigned original_count;
        Draw *roads, *roads_zoom, *checkpoint, *checkpoint_zoom, *circle, *circle_zoom;
        MEMF(node + 0x94) = cx; MEMF(node + 0x98) = cy;
        MEMF(node + 0xB4) = large ? 3.0f : 1.0f;
        MEMF(node + 0xBC) = 3; MEM8(node + 0x35) = (uint8_t)split;
        MEMF(layout) = cx - 64; MEMF(layout + 4) = cy - 64;
        MEMF(layout + 8) = 128; MEMF(layout + 12) = 128; MEMF(layout + 16) = 0;
        MEMF(player) = (float)turn * .3926990817f; MEM32(player + 4) = 120;
        MEMF(player + 0x34) = 1.2f;
        MEMF(player + 0x10) = 500; MEMF(player + 0x18) = 500;
        MEMF(node + 0x54) = 0; MEMF(node + 0x58) = 1000;
        MEMF(node + 0x5C) = 0; MEMF(node + 0x60) = 1000;
        MEMF(markers) = -20; MEMF(markers + 8) = 12; MEM32(markers + 12) = 0;
        MEM8(node + 0x34) = 1;
        player_present = 1; draw_count = 0;
        zoom_markers = 0;
        abi_start(node); retail_draw(); abi_end(24);
        original_count = draw_count; memcpy(original, draws, sizeof original);
        mm3_map_zoom_reset(node);
        zoom_markers = 1;
        draw_count = 0; abi_start(node); zoom_draw(); abi_end(24);
        near(MEMF(node + 0x50), native_scale);
        assert(draw_count == original_count);
        for (i = 0; i < draw_count; ++i) {
            unsigned vertex;
            assert(draws[i].texture == original[i].texture);
            for (axis = 0; axis < 2; ++axis) {
                float centre = 0;
                for (vertex = 0; vertex < 4; ++vertex)
                    centre += original[i].xy[vertex][axis] * .25f;
                for (vertex = 0; vertex < 4; ++vertex)
                    near(draws[i].xy[vertex][axis], draws[i].texture == 4 ?
                        centre + (original[i].xy[vertex][axis] - centre) * .75f :
                        original[i].xy[vertex][axis]);
            }
        }
        for (i = 0; i < 600; ++i) mm3_map_zoom_update(node, 1.0f / 60.0f);
        draw_count = 0; abi_start(node); zoom_draw(); abi_end(24);
        assert(draw_count == original_count); near(MEMF(node + 0x50), native_scale / span);
        /* Single-precision smoothing asymptotically approaches the cap; use
         * its observed scalar for the much larger native road coordinates. */
        span = native_scale / MEMF(node + 0x50);
        roads = texture_draw(original, original_count, 1); roads_zoom = texture_draw(draws, draw_count, 1);
        checkpoint = texture_draw(original, original_count, 4); checkpoint_zoom = texture_draw(draws, draw_count, 4);
        circle = texture_draw(original, original_count, 2); circle_zoom = texture_draw(draws, draw_count, 2);
        for (i = 0; i < 4; ++i) for (axis = 0; axis < 2; ++axis) {
            float centre = axis ? cy : cx;
            check = "native road scale";
            near(roads_zoom->xy[i][axis], centre + (roads->xy[i][axis] - centre) / span);
            check = "map window";
            near(circle_zoom->xy[i][axis], circle->xy[i][axis]);
        }
        for (axis = 0; axis < 2; ++axis) {
            float a = 0, b = 0, centre = axis ? cy : cx;
            for (i = 0; i < 4; ++i) { a += checkpoint->xy[i][axis] * .25f; b += checkpoint_zoom->xy[i][axis] * .25f; }
            check = "checkpoint centre";
            near(b, centre + (a - centre) / span);
            check = "checkpoint icon size";
            for (i = 0; i < 4; ++i)
                near(checkpoint_zoom->xy[i][axis] - b, (checkpoint->xy[i][axis] - a) * .75f / span);
        }
        /* Native scale reconstruction prevents zoom accumulating over draws. */
        draw_count = 0; abi_start(node); zoom_draw(); abi_end(24);
        near(MEMF(node + 0x50), native_scale / span);
        player_present = 0; draw_count = 0; abi_start(node); zoom_draw(); abi_end(24);
        near(MEMF(node + 0x50), split ? .75f : 1.0f);
    }
    /* The initialization seam resets stale identity state before native stores. */
    settle(node, 120, 1); MEM8(node + 0x35) = 0;
    MEMF(layout) = 50; MEMF(layout + 4) = 320;
    abi_start(node); zoom_reset(); abi_end(16);
    fresh_scale(node, player, 1); near(MEMF(node + 0x50), 1);
    /* The actual native update argument is +0x10 before its frame prologue. */
    MEM32(player + 4) = 120; fresh_scale(node, player, 1);
    MEMF(node + 0x9C) = MEMF(node + 0x94); MEMF(node + 0xA0) = MEMF(node + 0x98);
    MEMF(node + 0xAC) = MEMF(node + 0xB0) = 0;
    abi_start(node); MEMF(stack + 16) = .1f; zoom_update(); abi_end(20);
    fresh_scale(node, player, 1);
    near(1 / MEMF(node + 0x50), 1 + .25f * (1 - expf(-.1f / .6f)));
}
static void native_scale_progression(void) {
    float stopped;
    unsigned i;
    /* Checkpoint proximity changes the stock scale independently of speed.
     * Preserve this layout's first valid framing and let speed zoom it out. */
    mm3_map_zoom_reset(node);
    MEM32(player + 4) = 0; MEMF(player + 0x34) = .728f;
    fresh_scale(node, player, .728f); stopped = MEMF(node + 0x50);
    near(stopped, .728f);
    MEM32(player + 4) = 120; MEMF(player + 0x34) = 1.4f;
    fresh_scale(node, player, 1.4f); near(MEMF(node + 0x50), stopped);
    for (i = 0; i < 600; ++i) mm3_map_zoom_update(node, 1.0f / 60.0f);
    fresh_scale(node, player, 1.4f); near(MEMF(node + 0x50), stopped / 1.25f);
    assert(MEMF(node + 0x50) < stopped); near(MEMF(player + 0x34), 1.4f);
    /* At fixed speed, repeated native checkpoint updates cannot zoom back in. */
    for (i = 0; i < 4; ++i) {
        const float stock[] = {.5f, .728f, 1, 1.4f};
        MEMF(player + 0x34) = stock[i]; fresh_scale(node, player, stock[i]);
        near(MEMF(node + 0x50), stopped / 1.25f); near(MEMF(player + 0x34), stock[i]);
    }
    /* A reused node captures its new layout, while a null player or invalid
     * stock scale cannot establish a stale default baseline. */
    mm3_map_zoom_reset(node); MEM32(player + 4) = 0;
    fresh_scale(node, 0, 1); near(MEMF(node + 0x50), 1);
    fresh_scale(node, player, NAN); assert(isnan(MEMF(node + 0x50)));
    fresh_scale(node, player, -1); near(MEMF(node + 0x50), -1);
    fresh_scale(node, player, .9f); near(MEMF(node + 0x50), .9f);
}
int main(void) {
    g_xbox_mem_offset = (ptrdiff_t)memory; native_constants();
    MEM32(node) = 0x26000; MEM32(0x26000 + 0xC4) = 0x0011CAE8;
    MEM32(node + 8) = 1; MEM32(node + 0x20) = 2; MEM32(node + 0x24) = 3; MEM32(node + 0x10) = 4;
    MEM32(node + 0x8C) = property; MEM32(property) = property + 0x100;
    MEM32(property + 0x100 + 0x5C) = 0x100;
    MEM32(players + 0x14) = player; MEM32(0x407F78 + 4) = markers;
    speed_contract(); native_contract(); native_scale_progression();
    puts("PASS map zoom: 20-120 km/h/25% cap, smooth simulation time, pause/reset/independent nodes, native small/big roads/checkpoints/rotation and ABI");
    return 0;
}
