/* Execute the real HUD helper offline. Mock only the native quad boundary;
 * every produced vertex, guest argument and restored register is inspected. */
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"

static unsigned contract_cos_count, contract_sin_count;
static double contract_cos(double angle) { ++contract_cos_count; return cos(angle); }
static double contract_sin(double angle) { ++contract_sin_count; return sin(angle); }
#define cos contract_cos
#define sin contract_sin
#include "../src/mm3_speedometer.c"
#undef cos
#undef sin

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp, g_fs_base;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3;
RECOMP_TLS RecompMmx g_mm4, g_mm5, g_mm6, g_mm7;

static unsigned char contract_memory[0x400000];
static const uint32_t contract_frame = 0x3D000, contract_stack = 0x3C000;
static const uint32_t contract_node = 0x17000, contract_renderer = 0x18000;
static const uint32_t contract_effects = 0x19000;

struct contract_state {
    uint32_t registers[10];
    double fp[8];
    int top, cmp, df;
    uint16_t cc, control;
    RecompXmm xmm[8];
    RecompMmx mm[8];
};

struct contract_quad {
    float vertex[4][9];
};
static struct contract_quad contract_quads[2048];
static unsigned contract_quad_count;

static struct contract_state contract_snapshot(void)
{
    struct contract_state state;
    uint32_t registers[] = {g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi,
        g_ebp, g_seh_ebp, g_fs_base};
    RecompXmm xmm[] = {g_xmm0, g_xmm1, g_xmm2, g_xmm3, g_xmm4, g_xmm5, g_xmm6, g_xmm7};
    RecompMmx mm[] = {g_mm0, g_mm1, g_mm2, g_mm3, g_mm4, g_mm5, g_mm6, g_mm7};
    memset(&state, 0, sizeof state);
    memcpy(state.registers, registers, sizeof registers);
    memcpy(state.fp, g_fp_stack, sizeof state.fp);
    memcpy(state.xmm, xmm, sizeof xmm);
    memcpy(state.mm, mm, sizeof mm);
    state.top = g_fp_top; state.cmp = g_fp_cmp; state.df = g_df;
    state.cc = g_fp_cc; state.control = g_fp_control_word;
    return state;
}

static void contract_clobber(void)
{
    g_eax = g_ecx = g_edx = g_ebx = g_esi = g_edi = 0xBAD;
    g_ebp = g_seh_ebp = g_fs_base = 0xBAD;
    g_fp_top = g_fp_cmp = g_df = 7;
    g_fp_control_word = g_fp_cc = 0xBAD;
    memset(g_fp_stack, 0xBB, sizeof g_fp_stack);
    memset(&g_xmm0, 0xBB, sizeof g_xmm0); memset(&g_xmm1, 0xBB, sizeof g_xmm1);
    memset(&g_xmm2, 0xBB, sizeof g_xmm2); memset(&g_xmm3, 0xBB, sizeof g_xmm3);
    memset(&g_xmm4, 0xBB, sizeof g_xmm4); memset(&g_xmm5, 0xBB, sizeof g_xmm5);
    memset(&g_xmm6, 0xBB, sizeof g_xmm6); memset(&g_xmm7, 0xBB, sizeof g_xmm7);
    memset(&g_mm0, 0xBB, sizeof g_mm0); memset(&g_mm1, 0xBB, sizeof g_mm1);
    memset(&g_mm2, 0xBB, sizeof g_mm2); memset(&g_mm3, 0xBB, sizeof g_mm3);
    memset(&g_mm4, 0xBB, sizeof g_mm4); memset(&g_mm5, 0xBB, sizeof g_mm5);
    memset(&g_mm6, 0xBB, sizeof g_mm6); memset(&g_mm7, 0xBB, sizeof g_mm7);
}

void sub_001C1585(void)
{
    uint32_t quad = MEM32(g_esp + 8), rectangle;
    unsigned vertex, component;
    assert(g_eax == contract_effects && g_edx == contract_renderer);
    assert(g_ecx == 0x102 && g_ebx == 0x104 && g_esi == contract_renderer && g_edi == 0x106);
    assert(g_ebp == contract_frame && g_seh_ebp == contract_frame);
    assert(g_fs_base == 0x109);
    assert(g_fp_top == 2 && g_fp_cmp == 3 && g_df == 0);
    assert(g_xmm0.u[0] == 0x11111111 && g_mm7.ud[0] == 0x28282828);
    assert(g_esp == contract_stack - 0x500 - 12);
    assert(MEM32(g_esp) == 0x0023FAD2u && MEM32(g_esp + 4) == 0);
    assert(quad == contract_stack - 0x500 + 0x240);
    rectangle = MEM32(quad);
    assert(rectangle == contract_stack - 0x500 + 0x200);
    assert(MEM32(quad + 4) == 7 && MEM32(quad + 8) == 8);
    assert(MEMF(rectangle) == 465.0f && MEMF(rectangle + 4) == 315.0f);
    assert(MEMF(rectangle + 8) == 132.0f && MEMF(rectangle + 12) == 132.0f);
    assert(MEMF(rectangle + 16) == 0.0f);
    assert(contract_quad_count < sizeof contract_quads / sizeof contract_quads[0]);
    memcpy(contract_quads[contract_quad_count].vertex, (const void *)XBOX_PTR(quad + 12), 4 * 9 * sizeof(float));
    for (vertex = 0; vertex < 4; ++vertex) {
        const float *values = contract_quads[contract_quad_count].vertex[vertex];
        for (component = 0; component < 9; ++component) assert(isfinite(values[component]));
        assert(fabsf(values[0]) < 4096 && fabsf(values[1]) < 4096);
        assert(values[2] == 0 && values[8] == 1);
        assert(values[3] == (float)(vertex == 1 || vertex == 2));
        assert(values[4] == (float)(vertex >= 2));
        for (component = 5; component < 9; ++component)
            assert(values[component] >= 0 && values[component] <= 1);
    }
    {
        const float (*v)[9] = contract_quads[contract_quad_count].vertex;
        float area = (v[1][0] - v[0][0]) * (v[2][1] - v[0][1]) -
                     (v[1][1] - v[0][1]) * (v[2][0] - v[0][0]);
        assert(area > 0); /* Matches the retail quad winding/culling. */
    }
    ++contract_quad_count;
    g_esp += 12; /* Actual sub_001C1585/native draw uses RET 8. */
    contract_clobber();
}

static int contract_close(float a, float b)
{
    return fabsf(a - b) < .0002f;
}

static void contract_initialize(const char *alias, unsigned type, int metric)
{
    size_t length = strlen(alias);
    unsigned i;
    uint32_t text = 0x13000 + 0x1DC;
    memset(contract_memory, 0, sizeof contract_memory);
    g_xbox_mem_offset = (ptrdiff_t)contract_memory;
    MEM32(0x3C5CDC) = 0x11000; MEM32(0x11000 + 0x54) = 0x12000;
    MEM32(0x12000 + 0xD8) = 0x13000;
    MEM32(text + 0x14) = (uint32_t)length;
    MEM32(text + 0x18) = length < 16 ? 15 : 31;
    if (length < 16) memcpy((void *)XBOX_PTR(text + 4), alias, length + 1);
    else { MEM32(text + 4) = 0x14000; memcpy((void *)XBOX_PTR(0x14000), alias, length + 1); }
    MEM32(contract_frame - 36) = type;
    MEM8(contract_frame + 0x13) = (unsigned char)metric;
    MEM32(contract_frame + 0x18) = contract_effects;
    MEMF(contract_frame - 76) = 450; MEMF(contract_frame - 72) = 300;
    for (i = 0; i < 10; ++i) {
        float start = 135.0f + (float)i * 3.0f;
        MEMF(0x39E264 + i * 4) = start;
        MEMF(0x39E28C + i * 4) = 540.0f - start - 270.0f;
    }
    memset((void *)XBOX_PTR(contract_stack), 0xA5, 32);
    g_esp = contract_stack; g_eax = 0x101; g_ecx = 0x102; g_edx = 0x103;
    g_ebx = 0x104; g_esi = contract_renderer; g_edi = 0x106;
    g_ebp = 0x107; g_seh_ebp = 0x108; g_fs_base = 0x109;
    g_fp_top = 2; g_fp_cmp = 3; g_df = 0;
    g_fp_control_word = 0x37F; g_fp_cc = 0x4000;
    for (i = 0; i < 8; ++i) g_fp_stack[i] = 1.5 + i;
    memset(&g_xmm0, 0x11, sizeof g_xmm0); memset(&g_xmm1, 0x12, sizeof g_xmm1);
    memset(&g_xmm2, 0x13, sizeof g_xmm2); memset(&g_xmm3, 0x14, sizeof g_xmm3);
    memset(&g_xmm4, 0x15, sizeof g_xmm4); memset(&g_xmm5, 0x16, sizeof g_xmm5);
    memset(&g_xmm6, 0x17, sizeof g_xmm6); memset(&g_xmm7, 0x18, sizeof g_xmm7);
    memset(&g_mm0, 0x21, sizeof g_mm0); memset(&g_mm1, 0x22, sizeof g_mm1);
    memset(&g_mm2, 0x23, sizeof g_mm2); memset(&g_mm3, 0x24, sizeof g_mm3);
    memset(&g_mm4, 0x25, sizeof g_mm4); memset(&g_mm5, 0x26, sizeof g_mm5);
    memset(&g_mm6, 0x27, sizeof g_mm6); memset(&g_mm7, 0x28, sizeof g_mm7);
    contract_quad_count = 0;
}

static int contract_draw(double speed)
{
    struct contract_state before = contract_snapshot(), after;
    unsigned char frame[0x180], stack[32];
    int result;
    memcpy(frame, (const void *)XBOX_PTR(contract_frame - 352), sizeof frame);
    memcpy(stack, (const void *)XBOX_PTR(contract_stack), sizeof stack);
    result = mm3_speedometer_draw(contract_node, contract_frame, speed);
    after = contract_snapshot();
    assert(!memcmp(&before, &after, sizeof before));
    assert(!memcmp(frame, (const void *)XBOX_PTR(contract_frame - 352), sizeof frame));
    assert(!memcmp(stack, (const void *)XBOX_PTR(contract_stack), sizeof stack));
    return result;
}

/* Decode the emitted seven-segment rectangles instead of trusting the
 * configured value or the production formatter's input. */
static unsigned contract_decode_number(unsigned *position, unsigned expected,
                                        float x, float y)
{
    static const unsigned masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    unsigned actual[4] = {0}, value = 0, i;
    char expected_text[8];
    size_t length;
    snprintf(expected_text, sizeof expected_text, "%u", expected);
    length = strlen(expected_text);
    assert(length <= 4);
    x -= ((float)length * 4.0f - 1) * .5f; y -= 3.5f;
    while (*position < contract_quad_count &&
           contract_close(contract_quads[*position].vertex[0][5], .93f)) {
        const float (*v)[9] = contract_quads[(*position)++].vertex;
        float dx = v[0][0] - x, dy = v[0][1] - y;
        float width = v[1][0] - v[0][0], height = v[3][1] - v[0][1];
        unsigned column = (unsigned)floorf((dx + .02f) / 4.0f), segment = 99;
        assert(column < length);
        dx -= (float)column * 4;
        if (contract_close(width, 3) && contract_close(height, .8f) && contract_close(dx, 0)) {
            if (contract_close(dy, 0)) segment = 0;
            else if (contract_close(dy, 3.1f)) segment = 6;
            else if (contract_close(dy, 6.2f)) segment = 3;
        } else if (contract_close(width, .8f) && contract_close(height, 3)) {
            if (contract_close(dx, 2.2f) && contract_close(dy, .5f)) segment = 1;
            else if (contract_close(dx, 2.2f) && contract_close(dy, 3.5f)) segment = 2;
            else if (contract_close(dx, 0) && contract_close(dy, .5f)) segment = 5;
            else if (contract_close(dx, 0) && contract_close(dy, 3.5f)) segment = 4;
        }
        assert(segment < 7 && !(actual[column] & (1u << segment)));
        actual[column] |= 1u << segment;
    }
    for (i = 0; i < length; ++i) {
        unsigned digit;
        for (digit = 0; digit < 10 && masks[digit] != actual[i]; ++digit) {}
        assert(digit < 10);
        value = value * 10 + digit;
    }
    return value;
}

static void contract_check_geometry(unsigned type, int metric, float maximum, double speed)
{
    const float cx = 531, cy = 381;
    unsigned position = 40, i, index = type * 2u + (metric ? 0u : 1u);
    double start = 135 + index * 3, end = start + 270;
    double value = metric ? speed : speed * 0.6213712096214294;
    double angle;
    const struct contract_quad *needle;
    assert(contract_quad_count > 60);
    /* Compare cached background vertices with the original uncached angle
     * expressions, including their exact double-to-float rounding. */
    for (i = 0; i < 40; ++i) {
        double a = i * 6.283185307179586 / 40.0;
        double b = (i + 1) * 6.283185307179586 / 40.0;
        const float xy[4][2] = {{cx, cy},
            {cx + 56.0f * (float)cos(a), cy + 56.0f * (float)sin(a)},
            {cx + 56.0f * (float)cos(b), cy + 56.0f * (float)sin(b)}, {cx, cy}};
        unsigned vertex;
        for (vertex = 0; vertex < 4; ++vertex)
            assert(!memcmp(contract_quads[i].vertex[vertex], xy[vertex], 2 * sizeof(float)));
    }
    for (i = 0; i <= 16; ++i) {
        double a = (start + (end - start) * i / 16.0) * 0.017453292519943295;
        assert(contract_close(contract_quads[position].vertex[0][5], .73f));
        ++position;
        if (!(i % 2)) {
            unsigned expected = (unsigned)(maximum * i / 16.0f + .5f);
            assert(contract_decode_number(&position, expected,
                cx + 42 * (float)cos(a), cy + 42 * (float)sin(a)) == expected);
        }
    }
    if (!isfinite(value) || value < 0) value = 0;
    if (value > maximum) value = maximum;
    angle = (start + (end - start) * value / maximum) * 0.017453292519943295;
    needle = &contract_quads[contract_quad_count - 2];
    assert(needle->vertex[0][5] == 1 && contract_close(needle->vertex[0][6], .26f));
    assert(contract_close((needle->vertex[1][0] + needle->vertex[2][0]) * .5f, cx + 64 * (float)cos(angle)));
    assert(contract_close((needle->vertex[1][1] + needle->vertex[2][1]) * .5f, cy + 64 * (float)sin(angle)));
    assert(contract_close((needle->vertex[0][0] + needle->vertex[3][0]) * .5f, cx - 5 * (float)cos(angle)));
    assert(contract_close((needle->vertex[0][1] + needle->vertex[3][1]) * .5f, cy - 5 * (float)sin(angle)));
}

static void contract_fallbacks(void)
{
    static const char *aliases[] = {"UnknownCar", "Avant", "Avalanche", "Corvette", "ChevySSR", "Crossfire", "NewBeetle"};
    unsigned i;
    for (i = 0; i < sizeof aliases / sizeof aliases[0]; ++i) {
        contract_initialize(aliases[i], 1, 1);
        assert(!contract_draw(100) && !contract_quad_count);
    }
    for (i = 0; i <= 5; ++i) if (i != 1 && i != 4) {
        contract_initialize("Koenigsegg", i, 1);
        assert(!contract_draw(100) && !contract_quad_count);
    }
    contract_initialize("Koenigsegg", 4, 1);
    MEM32(contract_frame - 344) = 1;
    assert(!contract_draw(100) && !contract_quad_count);
    contract_initialize("Koenigsegg", 4, 1);
    MEM32(0x3C5CDC) = 0;
    assert(!contract_draw(100) && !contract_quad_count);
    contract_initialize("Koenigsegg", 4, 1);
    MEM32(0x13000 + 0x1DC + 0x14) = 32;
    assert(!contract_draw(100) && !contract_quad_count);
    contract_initialize("Koenigsegg", 4, 1);
    MEM8(0x13000 + 0x1DC + 4 + 10) = 'X';
    assert(!contract_draw(100) && !contract_quad_count);
}

static void contract_added_cars_retain_retail(void)
{
    static const char *aliases[] = {
        "Panoz", "McLaren_F1", "Form1", "Ford_GTcar", "Senna_Road",
        "Senna_GTR1", "FXXK_Racer", "Regera_GT1", "AventadorX", "MRly",
        "ThisAliasIsLongerThan15"
    };
    unsigned alias, type, metric;
    int loaded = initialized;
    /* These aliases have valid ranges in the legacy sidecar fixture. Added
     * cars must still leave the complete native texture and needle path in
     * control, without submitting replacement quads or loading that file. */
    for (alias = 0; alias < sizeof aliases / sizeof aliases[0]; ++alias)
        for (type = 1; type <= 4; type += 3)
            for (metric = 0; metric <= 1; ++metric) {
                contract_initialize(aliases[alias], type, (int)metric);
                assert(!contract_draw(400) && !contract_quad_count);
                assert(initialized == loaded);
            }
}

static void contract_check_cached_geometry(void)
{
    static struct contract_quad expected[2048];
    unsigned count;
    /* Force a new arc, then hold it fixed as speed and geometry do in the
     * next frame. Every emitted float and guest register must be unchanged. */
    contract_initialize("Koenigsegg", 4, 1);
    MEMF(0x39E264 + 8 * 4) += 1.0f;
    contract_cos_count = contract_sin_count = 0;
    assert(contract_draw(150));
    assert(contract_cos_count == 18 && contract_sin_count == 18);
    count = contract_quad_count;
    memcpy(expected, contract_quads, count * sizeof expected[0]);
    contract_quad_count = contract_cos_count = contract_sin_count = 0;
    assert(contract_draw(150));
    assert(contract_cos_count == 1 && contract_sin_count == 1);
    assert(contract_quad_count == count);
    assert(!memcmp(expected, contract_quads, count * sizeof expected[0]));
    /* Changing either boundary must invalidate the cached directions. */
    MEMF(0x39E28C + 8 * 4) += 2.0f;
    contract_quad_count = contract_cos_count = contract_sin_count = 0;
    assert(contract_draw(150));
    assert(contract_cos_count == 18 && contract_sin_count == 18);
    assert(memcmp(expected, contract_quads, count * sizeof expected[0]));
}

int main(int argc, char **argv)
{
    static const struct { const char *alias; float kmh, mph; } cars[] = {
        {"koenigsegg", 800, 500}, {"VIPER", 650, 404}, {"Lotus", 500, 311},
        {"AudiTT", 1200, 750}
    };
    static const double fractions[] = {0, .5, 1, 2, -1};
    unsigned car, type, metric, speed;
    assert(argc == 2);
    contract_added_cars_retain_retail();
    if (strcmp(argv[1], "configured")) {
        contract_initialize("Koenigsegg", 4, 1);
        assert(!contract_draw(800) && !contract_quad_count);
        contract_initialize("Viper", 1, 0);
        assert(!contract_draw(800) && !contract_quad_count);
        puts("PASS: absent/invalid settings retain the complete retail dial and guest state");
        return 0;
    }
    for (car = 0; car < sizeof cars / sizeof cars[0]; ++car)
        for (type = 1; type <= 4; type += 3)
            for (metric = 0; metric <= 1; ++metric)
                for (speed = 0; speed < sizeof fractions / sizeof fractions[0]; ++speed) {
                    float maximum = metric ? cars[car].kmh : cars[car].mph;
                    double kmh = maximum * fractions[speed] / (metric ? 1.0 : 0.6213712096214294);
                    contract_initialize(cars[car].alias, type, (int)metric);
                    assert(contract_draw(kmh));
                    if (!car && type == 1 && !metric && !speed)
                        assert(contract_cos_count == 59 && contract_sin_count == 59);
                    contract_check_geometry(type, (int)metric, maximum, kmh);
                }
    contract_initialize("Koenigsegg", 4, 1);
    assert(contract_draw(NAN)); contract_check_geometry(4, 1, 800, NAN);
    contract_initialize("Koenigsegg", 4, 1);
    assert(contract_draw(INFINITY)); contract_check_geometry(4, 1, 800, INFINITY);
    /* Keep heap-backed guest-string coverage for an eligible retail alias. */
    contract_initialize("AudiTT", 4, 1);
    MEM32(0x13000 + 0x1DC + 0x18) = 31;
    MEM32(0x13000 + 0x1DC + 4) = 0x14000;
    memcpy((void *)XBOX_PTR(0x14000), "AudiTT", 7);
    assert(contract_draw(600)); contract_check_geometry(4, 1, 1200, 600);
    contract_added_cars_retain_retail();
    contract_fallbacks();
    contract_check_cached_geometry();
    puts("PASS: added cars retain complete retail dials before/after legacy settings load; 83 unchanged original overrides, decoded labels, calibrated clamped needles, native ABI, complete guest state, fallbacks and exact cached geometry");
    return 0;
}
