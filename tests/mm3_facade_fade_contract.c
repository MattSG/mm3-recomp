/* Exercise the production facade wrapper against the setter's guest ABI.
 * The oracle samples the Xbox fade at proportional camera distances, so it
 * detects a moved threshold whose blend width was accidentally left at 9 m. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/mm3_render_distance.c"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_esp, g_ebp;
static unsigned char memory[0x400000], expected[0x400000];
static unsigned setter_calls;
static float original_bias, original_slope;
static const uint32_t stack = 0x1000;

void nv2a_d3d_set_depth_view_remap(float scale, float knee)
{
    assert(scale >= 1.0f && scale <= 4.0f && knee == 0.78f);
}
void sub_000E2D5B_original(void) {}
void sub_000D9E2E_original(void) {}
void sub_000FE71E_original(void) {}
void sub_0022F298_original(void) {}
void sub_000F4AC3_original(void) {}
void sub_002256C7_original(void) {}

void sub_001661D0_original(void)
{
    double near_distance = MEMF(g_esp + 4), far_distance = MEMF(g_esp + 8);
    double start, end, slope;
    ++setter_calls;
    if (near_distance < 0.0) near_distance = 0.0;
    if (far_distance < near_distance + 50.0) far_distance = near_distance + 50.0;
    MEMF(0x003C04E0u) = (float)near_distance;
    MEMF(0x003C04E4u) = (float)far_distance;
    start = near_distance - 10.0;
    end = near_distance - 1.0;
    slope = 1.0 / (start * start - end * end);
    original_bias = MEMF(0x003B8134u) = (float)(-end * end * slope);
    original_slope = MEMF(0x003B8138u) = (float)slope;
    /* Other fade constants and guest register results are owned by retail. */
    MEM32(0x003B8160u) = 0x1234ABCD;
    g_eax = 0x11223344;
    g_ecx = 0x55667788;
    g_esp += 12;
    memcpy(expected, memory, sizeof memory);
}

static void call_setter(uint32_t caller, float near_distance, float far_distance)
{
    g_esp = stack;
    MEM32(stack) = caller;
    MEMF(stack + 4) = near_distance;
    MEMF(stack + 8) = far_distance;
    sub_001661D0();
    assert(g_esp == stack + 12 && g_eax == 0x11223344 && g_ecx == 0x55667788);
    memcpy(expected + 0x003B8134u, memory + 0x003B8134u, 8);
    assert(memcmp(memory, expected, sizeof memory) == 0);
}

static void check_fade(float scale, float retail_near, float retail_far)
{
    unsigned i;
    double bias = MEMF(0x003B8134u), slope = MEMF(0x003B8138u);
    double start = retail_near - 10.0, end = retail_near - 1.0;
    assert(MEMF(0x003C04E0u) == retail_near * scale);
    assert(MEMF(0x003C04E4u) == retail_far * scale);
    /* Normal retail endpoints are 70 m opaque and 79 m transparent. */
    for (i = 0; i <= 1000; ++i) {
        double distance = start + 9.0 * i / 1000.0;
        double wanted = (end * end - distance * distance) / (end * end - start * start);
        double effective_distance = distance * scale;
        assert(fabs(bias + slope * effective_distance * effective_distance - wanted) < 0.00001);
    }
}

int main(int argc, char **argv)
{
    float scale, saved_near, saved_far;
    uint32_t saved_fade[2];
    assert(argc == 2);
    scale = (float)atof(argv[1]);
    assert(scale == 1.0f || scale == 2.0f || scale == 4.0f);
    _putenv_s("RECOMP_DRAW_DISTANCE", argv[1]);
    memset(memory, 0xA5, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    call_setter(0x0015FA1Du, 80.0f, 280.0f); /* construction */
    check_fade(scale, 80.0f, 280.0f);
    call_setter(0x0020FABAu, 80.0f, 280.0f); /* explicit Lua setter */
    check_fade(scale, 80.0f, 280.0f);
    saved_near = MEMF(0x003C04E0u);
    saved_far = MEMF(0x003C04E4u);
    memcpy(saved_fade, memory + 0x003B8134u, sizeof saved_fade);
    call_setter(0x00211220u, saved_near * 3.0f, saved_far * 3.0f);
    check_fade(scale, 240.0f, 840.0f);
    call_setter(0x0021153Du, saved_near, saved_far);
    check_fade(scale, 80.0f, 280.0f);
    assert(memcmp(saved_fade, memory + 0x003B8134u, sizeof saved_fade) == 0);
    call_setter(0x0020FABAu, 10.0f, 280.0f); /* no positive fade start */
    assert(MEMF(0x003B8134u) == original_bias && MEMF(0x003B8138u) == original_slope);
    call_setter(0x0020FABAu, -5.0f, 280.0f); /* preserve native validation */
    assert(MEMF(0x003C04E0u) == 0.0f);
    assert(MEMF(0x003B8134u) == original_bias && MEMF(0x003B8138u) == original_slope);
    assert(setter_calls == 6);
    printf("facade fade contract passed (scale %.0f)\n", scale);
    return 0;
}
