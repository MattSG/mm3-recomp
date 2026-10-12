/* Real native checkpoint quad creation, projection, rotation and guest ABI;
 * only player property lookup and GPU submission are capture boundaries. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
static unsigned char memory[0x440000];
static const uint32_t stack = 0x80000, node = 0x10000, property = 0x22000;
static const uint32_t layout = 0x23000, markers = 0x24000, position = 0x28000;
static const uint32_t objects[4] = {0x30018, 0x30118, 0x30218, 0x30318};
static const float points[4][2] = {{-22, -8}, {-10, 14}, {9, 10}, {23, -5}};
typedef struct { uint32_t texture; float xy[4][2], color[4]; } Draw;
static Draw draws[256];
static unsigned draw_count, active_player, marker_count, world_draw_count;
static uint32_t world_color;
static const char *check = "guidance";

#include "production_race_guidance.c"

static void fake_icall(uint32_t address) {
    if (address == 0x100) {
        g_fp_top = (g_fp_top + 7) & 7; g_fp_stack[g_fp_top] = active_player;
        g_esp += 8; return;
    }
    if (address == 0x0002DC7E) {
        memset((void *)XBOX_PTR(g_ecx), 0, 0x24); g_esp += 4; return;
    }
    fprintf(stderr, "unexpected guidance indirect call %08X\n", address); abort();
}
void sub_001F0399(void) {
    g_eax = (uint32_t)(int32_t)g_fp_stack[g_fp_top];
    g_fp_top = (g_fp_top + 1) & 7; g_esp += 4;
}
void sub_00078DF9(void) { g_eax = 0; g_esp += 4; }
void sub_0011FE40(void) { assert(!"unexpected active HUD player"); }
void sub_0004EE0D(void) { g_eax = marker_count; g_esp += 4; }
void sub_002414D8(void) { assert(!"unexpected non-checkpoint color"); }
void sub_00012D29(void) {
    g_fp_top = (g_fp_top + 7) & 7; g_fp_stack[g_fp_top] = floorf(MEMF(g_esp + 4));
    g_esp += 8;
}
/* World geometry/resource boundaries; native draw runs through the exact
 * packed-color branch and reuses +0x40 as a float before that branch. */
void sub_00013A63(void) {
    float x = MEMF(g_eax), y = MEMF(g_eax + 4), z = MEMF(g_eax + 8);
    g_fp_top = (g_fp_top + 7) & 7; g_fp_stack[g_fp_top] = sqrt(x*x + y*y + z*z);
    g_esp += 4;
}
void sub_0020C1C9(void) { g_eax = 1; g_esp += 8; }
void sub_00012BFA(void) { memset((void *)XBOX_PTR(g_ecx), 0, 48); g_esp += 4; }
void sub_0001944A(void) { g_esp += 4; }
void sub_00019B8D(void) {
    uint32_t result = MEM32(g_esp + 4);
    memset((void *)XBOX_PTR(result), 0, 48);
    MEMF(result) = MEMF(result + 16) = MEMF(result + 32) = 1;
    g_eax = result; g_esp += 8;
}
void sub_00164019(void) { g_esp += 20; }
void sub_0025373E(void) { g_fp_top = (g_fp_top + 1) & 7; g_esp += 4; }
void sub_00019C06(void) { g_esp += 4; }
void sub_0001BB30(void) { world_color = MEM32(g_esp + 72); ++world_draw_count; g_esp += 100; }
void sub_00013A34(void) { assert(!"unexpected world arrow"); }
void sub_0001BADD(void) { assert(!"unexpected world arrow draw"); }
void sub_001C1585(void) {
    uint32_t texture = MEM32(g_esp + 4), vertices = MEM32(g_esp + 8);
    unsigned i;
    assert(draw_count < sizeof draws / sizeof draws[0]);
    assert(MEM32(vertices + 4) == 7 && MEM32(vertices + 8) == 8);
    draws[draw_count].texture = texture;
    for (i = 0; i < 4; ++i) {
        draws[draw_count].xy[i][0] = MEMF(vertices + 0xC + i * 0x24);
        draws[draw_count].xy[i][1] = MEMF(vertices + 0x10 + i * 0x24);
        draws[draw_count].color[i] = MEMF(vertices + 0x20 + i * 4);
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
#include "native_race_guidance.inc"

static void near(float got, float expected) {
    if (!isfinite(got) || fabsf(got - expected) > .001f) {
        fprintf(stderr, "%s: got %.9g expected %.9g\n", check, got, expected); abort();
    }
}
static void publish(unsigned player, unsigned current) {
    unsigned i;
    mm3_race_guidance_begin(player);
    for (i = 0; i < 4; ++i) {
        MEMF(position) = points[i][0]; MEMF(position + 8) = points[i][1];
        mm3_race_guidance_checkpoint(player, objects[i], position, objects[current]);
    }
    mm3_race_guidance_end(player);
}
static void abi_start(unsigned outside) {
    g_eax = 0xABABABAB; g_ecx = node; g_edx = 0x12345678;
    g_esp = stack; g_ebp = 0x71000; g_seh_ebp = g_ebp;
    g_ebx = 0x11223344; g_esi = 0x22334455; g_edi = 0x33445566;
    g_fp_top = 0; g_fp_control_word = 0x37F; g_df = 0;
    memset(g_fp_stack, 0, sizeof g_fp_stack);
    MEM32(stack) = 0xDEADBEEF;
    MEM32(stack + 4) = 0x25000; MEM32(stack + 8) = layout;
    MEM32(stack + 12) = 0x26262626; MEM32(stack + 16) = outside;
    MEM32(stack + 20) = 0xA5A5A5A5;
    draw_count = 0;
}
static void abi_end(void) {
    assert(g_esp == stack + 20 && g_fp_top == 0);
    assert(g_ebx == 0x11223344 && g_esi == 0x22334455 && g_edi == 0x33445566);
    assert(MEM32(stack + 20) == 0xA5A5A5A5);
}
static float centre(const Draw *draw, unsigned axis) {
    return .25f * (draw->xy[0][axis] + draw->xy[1][axis] + draw->xy[2][axis] + draw->xy[3][axis]);
}
static float edge(const Draw *draw) {
    return hypotf(draw->xy[1][0] - draw->xy[0][0], draw->xy[1][1] - draw->xy[0][1]);
}
static void native_contract(void) {
    unsigned large, turn, split, i, axis;
    const unsigned order[4] = {2, 0, 1, 3}; /* Native vector puts current first. */
    for (split = 0; split < 2; ++split) for (large = 0; large < 2; ++large) for (turn = 0; turn < 8; ++turn) {
        Draw retail[4], guided[4];
        active_player = split; marker_count = 4; publish(active_player, 2);
        MEM32(0x407F78 + active_player * 0x10 + 4) = markers;
        MEMF(node + 0x94) = large ? 320.0f : 114.0f;
        MEMF(node + 0x98) = large ? 240.0f : split ? 180.0f : 384.0f;
        MEMF(node + 0x50) = large ? 1.2f : .8f;
        MEMF(node + 0x4C) = turn * .3926990817f;
        MEMF(node + 0x54) = 0; MEMF(node + 0x58) = 1000;
        MEMF(node + 0xB4) = large ? 3.0f : 1.0f;
        MEMF(node + 0xAC) = .125f;
        MEMF(layout + 8) = large ? 256.0f : 128.0f;
        for (i = 0; i < 4; ++i) {
            MEMF(markers + i * 24) = points[order[i]][0];
            MEMF(markers + i * 24 + 8) = points[order[i]][1];
            MEM32(markers + i * 24 + 12) = order[i] == 2 ? 0x80 : order[i] == 1 ? 0x40 : 0;
        }
        abi_start(0); retail_marker(); abi_end();
        assert(draw_count == 4);
        for (i = 0; i < 4; ++i) retail[order[3 - i]] = draws[i];
        abi_start(0); sub_0011CAE8(); abi_end();
        assert(draw_count == 7);
        for (i = 0; i < 4; ++i) guided[order[3 - i]] = draws[3 + i];
        for (i = 0; i < 4; ++i) {
            unsigned rank = i == 2 ? 0 : i == 3 ? 1 : 2;
            near(guided[i].color[3], rank == 0 ? 1 : rank == 1 ? .75f : .14f);
            near(edge(&guided[i]), edge(&retail[i]) * (rank == 0 ? 1.4f : rank == 1 ? 1.15f : .75f));
            for (axis = 0; axis < 2; ++axis) near(centre(&guided[i], axis), centre(&retail[i], axis));
        }
        /* Course endpoints coincide with real native icons across small,
         * expanded, split-screen and rotated maps, independent of vector order. */
        for (i = 0; i < 3; ++i) {
            assert(draws[i].texture == 0);
            near(draws[i].color[3], i == 1 ? .9f : .6f);
            for (axis = 0; axis < 2; ++axis) {
                check = "course start matches native checkpoint";
                near(.5f * (draws[i].xy[0][axis] + draws[i].xy[3][axis]), centre(&guided[i], axis));
                check = "course end matches native checkpoint";
                near(.5f * (draws[i].xy[1][axis] + draws[i].xy[2][axis]), centre(&guided[i + 1], axis));
            }
        }
        abi_start(1); sub_0011CAE8(); abi_end(); assert(draw_count == 0);
    }
}
static void clipping_and_state_contract(void) {
    unsigned i;
    uint32_t original_color = 0x12345678;
    /* Both points may be outside the map while their segment crosses it. */
    mm3_race_guidance_begin(0);
    for (i = 0; i < 2; ++i) {
        MEMF(position) = i ? 200.0f : -200.0f; MEMF(position + 8) = 0;
        mm3_race_guidance_checkpoint(0, objects[i], position, objects[0]);
    }
    mm3_race_guidance_end(0); active_player = 0; marker_count = 0;
    MEMF(node + 0x4C) = 0; MEMF(node + 0x50) = 1;
    MEMF(layout + 8) = 128; MEMF(node + 0xB4) = 1;
    abi_start(0); sub_0011CAE8(); abi_end(); assert(draw_count == 1);
    near(.5f * (draws[0].xy[0][0] + draws[0].xy[3][0]), MEMF(node + 0x94) + 56);
    near(.5f * (draws[0].xy[1][0] + draws[0].xy[2][0]), MEMF(node + 0x94) - 56);
    /* The real always-executed HUD boundary clears both old players even
     * when Cruise/no players skips checkpoint publication altogether. */
    publish(0, 2); publish(1, 0);
    abi_start(0); sub_00120091();
    assert(g_esp == stack + 8 && g_fp_top == 0);
    assert(g_ebx == 0x11223344 && g_esi == 0x22334455 && g_edi == 0x33445566);
    abi_start(0); sub_0011CAE8(); abi_end(); assert(draw_count == 0);
    assert(mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) == original_color);
    publish(0, 2);
    assert((mm3_race_world_checkpoint_color(objects[2] - 0x18, original_color) & 255) == 255);
    assert((mm3_race_world_checkpoint_color(objects[3] - 0x18, original_color) & 255) == 150);
    assert((mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) & 255) == 28);
    /* World priorities belong to the rendered viewport, rather than taking
     * the strongest color of either split-screen player's target. */
    publish(1, 0);
    assert((mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) & 255) == 28);
    MEM32(0x40000 + 0x15218) = 1;
    assert((mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) & 255) == 255);
    assert((mm3_race_world_checkpoint_color(objects[2] - 0x18, original_color) & 255) == 28);
    MEM32(0x40000 + 0x15218) = 2;
    assert(mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) == original_color);
    MEM32(0x40000 + 0x15218) = 0;
    assert(mm3_race_world_checkpoint_color(0x35000, original_color) == original_color);
    publish(0, 3); mm3_race_guidance_begin(1);
    /* Blitz's final target must not advertise the start as a second target. */
    assert((mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) & 255) == 28);
    MEM32(0x407C1C) = 3; publish(0, 3);
    assert((mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) & 255) == 150);
    MEM32(0x407C1C) = 2;
    mm3_race_guidance_begin(0);
    MEMF(position) = NAN; mm3_race_guidance_checkpoint(0, objects[0], position, objects[0]);
    mm3_race_guidance_end(0);
    assert(mm3_race_world_checkpoint_color(objects[0] - 0x18, original_color) == original_color);
}
static void course_closure_and_revisit_contract(void) {
    unsigned i, axis;
    const uint32_t color = 0x29000, size = color + 16;
    active_player = 0; marker_count = 0;
    MEMF(node + 0x4C) = 0; MEMF(node + 0x50) = 1;
    MEMF(node + 0x54) = 0; MEMF(node + 0x58) = 1024;
    MEMF(layout + 8) = 256; MEMF(node + 0xB4) = 1;
    MEM32(0x407C1C) = 2; publish(0, 0);
    abi_start(0); sub_0011CAE8(); abi_end(); assert(draw_count == 3);
    MEM32(0x407C1C) = 3; publish(0, 0);
    abi_start(0); sub_0011CAE8(); abi_end(); assert(draw_count == 4);
    /* The circuit's closing segment ends at the first/current checkpoint. */
    near(draws[3].color[3], .9f);
    for (axis = 0; axis < 2; ++axis) {
        float base = MEMF(node + 0x94 + 4 * axis), sign = axis ? 1.0f : -1.0f;
        near(.5f * (draws[3].xy[0][axis] + draws[3].xy[3][axis]), base + sign * points[3][axis]);
        near(.5f * (draws[3].xy[1][axis] + draws[3].xy[2][axis]), base + sign * points[0][axis]);
    }
    MEM32(0x407C1C) = 2; mm3_race_guidance_begin(0);
    /* Checkpoint 2 revisits checkpoint 0's coordinates. Both the native
     * current bit and coordinate fallback keep the actual target prominent. */
    for (i = 0; i < 4; ++i) {
        MEMF(position) = points[i == 2 ? 0 : i][0];
        MEMF(position + 8) = points[i == 2 ? 0 : i][1];
        mm3_race_guidance_checkpoint(0, objects[i], position, objects[2]);
    }
    mm3_race_guidance_end(0);
    MEMF(markers) = points[0][0]; MEMF(markers + 8) = points[0][1];
    for (i = 0; i < 2; ++i) {
        MEM32(markers + 12) = i ? 0x80 : 0;
        MEMF(size) = 10;
        mm3_race_map_checkpoint_style(0x407F78, markers, color, size);
        near(MEMF(color + 12), 1); near(MEMF(size), 14);
    }
}
static void native_world_contract(void) {
    unsigned i;
    const unsigned alphas[4] = {28, 28, 255, 150};
    publish(0, 2); mm3_race_guidance_begin(1);
    for (i = 0; i < 4; ++i) {
        uint32_t object = objects[i] - 0x18;
        MEM8(object + 0x70) = 1; MEM32(object + 0x54) = 0x40000;
        MEM32(0x40000 + 0x15368) = 0x58000;
        MEMF(object + 0x7C) = 10; MEMF(object + 0x88) = 0;
        abi_start(0); g_ecx = object; MEM32(stack + 4) = 0;
        world_draw_count = 0; sub_0020C9F9();
        assert(g_esp == stack + 8 && g_fp_top == 0);
        assert(g_ebx == 0x11223344 && g_esi == 0x22334455 && g_edi == 0x33445566);
        assert(world_draw_count == 1);
        assert(world_color == (0x80FFAA00u | alphas[i]));
    }
}
int main(void) {
    unsigned i;
    g_xbox_mem_offset = (ptrdiff_t)memory; native_constants();
    MEM32(0x3BA52C) = 0x80FFAA88;
    MEM32(0x407C1C) = 2;
    for (i = 0; i < 4; ++i) MEM32(objects[i] - 0x18 + 0x54) = 0x40000;
    MEM32(node + 0x10) = 4;
    MEM32(node + 0x8C) = property; MEM32(property) = property + 0x100;
    MEM32(property + 0x100 + 0x5C) = 0x100;
    native_contract(); clipping_and_state_contract(); course_closure_and_revisit_contract(); native_world_contract();
    puts("PASS race guidance: native small/expanded/split/rotated maps, route alignment/closure, checkpoint priorities, repeated crossings, clipping, per-view world hoops, HUD reset and guest ABI");
    return 0;
}
