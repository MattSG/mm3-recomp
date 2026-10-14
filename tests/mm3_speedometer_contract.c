/* The actual original/transformed HUD speed block and retail integer
 * conversion are extracted by mm3_speedometer_regression.py. Only guest
 * text allocation and rendering boundaries are mocked. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;

static unsigned char memory[0x400000];
static const uint32_t frame = 0xD000, stack = 0xC000;
static const uint32_t node = 0x1000, renderer = 0x2000;
static const uint32_t speed_data = 0x3000, metric_data = 0x4000;
static const uint32_t text_data = 0x5000, font = 0x6000;
static double speed, metric;
static unsigned text_calls, needle_calls, destroyed, constructors, formatted;
static int displayed;
static float rectangle[5];
static int use_override;
static unsigned override_calls;

int mm3_speedometer_draw(uint32_t hud, uint32_t caller_frame, double velocity)
{
    assert(hud == node && caller_frame == frame);
    assert(g_esi == renderer);
    assert(velocity == speed);
    assert(MEMF(frame + 8) == 531.0f); /* Getter setup overwrote renderer slot. */
    ++override_calls;
    return use_override;
}

static void mock_indirect(uint32_t target)
{
    assert(MEM32(g_esp + 4) == renderer);
    assert(target == 0x1234 || target == 0x5678);
    assert(g_ecx == (target == 0x1234 ? metric_data : speed_data));
    g_fp_top = (g_fp_top + 7) & 7;
    g_fp_stack[g_fp_top] = target == 0x1234 ? metric : speed;
    g_esp += 8; /* Native data getter RET 4. */
}
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target, saved, site) mock_indirect(target)

static void sub_00018CBC(void)
{
    assert(g_edi == frame - 320);
    ++constructors;
    g_esp += 4;
}

static void sub_00018F65(void)
{
    assert(g_esi == frame - 320);
    displayed = (int32_t)MEM32(g_esp + 4);
    snprintf((char *)XBOX_PTR(text_data), 32, "%d", displayed);
    ++formatted;
    g_esp += 8;
}

static void sub_00013DCA(void)
{
    assert(g_ecx == frame - 84 && MEM32(g_esp + 4) == frame - 296);
    /* Exercise the guest-owned heap string branch before native destruction. */
    MEM32(g_ecx + 4) = text_data;
    MEM32(g_ecx + 24) = 32;
    g_esp += 8;
}

static void sub_0024CF2C(void)
{
    assert(g_eax == font && g_ecx == frame - 20);
    assert(MEM32(g_esp + 4) == 0);
    g_esp += 8;
}

static void sub_001C498D(void)
{
    uint32_t rect = MEM32(g_esp + 4);
    assert(g_edi == renderer && g_eax == frame - 20);
    assert(rect == frame - 108);
    assert(MEM32(g_esp + 8) == 0x7777 && MEM32(g_esp + 12) == 0x8888);
    assert(MEM32(g_esp + 16) == text_data);
    assert(atoi((char *)XBOX_PTR(text_data)) == displayed);
    memcpy(rectangle, (const void *)XBOX_PTR(rect), sizeof rectangle);
    ++text_calls;
    g_esp += 20; /* Native text draw RET 16. */
}

static void sub_001C455D(void)
{
    assert(g_ecx == frame - 20);
    g_esp += 4;
}

static void sub_000127A9(void)
{
    assert(g_ecx == frame - 84 || g_ecx == frame - 296);
    assert(MEM32(g_esp + 4) == 1);
    ++destroyed;
    g_esp += 8;
}

#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#include "production_speedometer.inc"

static void check(void (*branch)(void), int modified, unsigned type,
                  int split, int is_metric, double velocity)
{
    int readout = split;   /* split-screen keeps its native numeric readout */
    (void)modified;
    memset(memory, 0, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    g_esp = stack; g_ebp = frame; g_ebx = node; g_esi = renderer;
    g_fp_top = 0; g_fp_control_word = 0x37F;
    memset(g_fp_stack, 0, sizeof g_fp_stack);
    MEM32(node + 0x24) = speed_data;
    MEM32(node + 0x2C) = metric_data;
    MEM32(node + 0x18) = font;
    MEM32(speed_data) = speed_data + 0x100;
    MEM32(metric_data) = metric_data + 0x100;
    MEM32(speed_data + 0x100 + 0x5C) = 0x5678;
    MEM32(metric_data + 0x100 + 0x5C) = 0x1234;
    MEM32(frame - 344) = split ? 1 : 0;
    MEM32(frame - 36) = type;
    MEMF(frame - 76) = 450.0f;
    MEMF(frame - 72) = 300.0f;
    MEM32(frame + 8) = renderer;
    MEM32(frame + 0x14) = 0x7777;
    MEM32(frame + 0x18) = 0x8888;
    MEMF(0x384D08) = 10.0f;
    MEMF(0x385D78) = 18.0f;
    MEMF(0x386420) = 0.6213712096214294f;
    metric = is_metric ? 1.0 : 0.0;
    speed = velocity;
    text_calls = needle_calls = destroyed = constructors = formatted = 0;
    displayed = -99999;
    branch();
    assert(text_calls == (unsigned)readout);
    assert(needle_calls == (unsigned)!split);
    assert(constructors == text_calls && formatted == text_calls);
    assert(destroyed == 2 * text_calls);
    assert(g_esp == stack && g_fp_top == 0);
    assert(g_ebx == node);
    if (!split) assert(g_esi == renderer);
    if (readout) {
        float value = is_metric ? (float)velocity :
            (float)(velocity * (double)MEMF(0x386420));
        assert(displayed == (int)value);
        assert(rectangle[0] == 460.0f);
        assert(rectangle[1] == 318.0f);
        assert(rectangle[2] == 105.0f && rectangle[3] == 20.0f);
        assert(rectangle[4] == 0.0f);
    }
}

int main(void)
{
    static const double velocities[] = {0, 123.75, 420.25, 800, 1200, -40};
    unsigned type, split, units, i;
    for (type = 0; type <= 4; ++type)
        for (split = 0; split <= 1; ++split)
            for (units = 0; units <= 1; ++units)
                for (i = 0; i < sizeof velocities / sizeof velocities[0]; ++i) {
                    check(contract_original, 0, type, split, units, velocities[i]);
                    check(contract_modified, 1, type, split, units, velocities[i]);
                }
    for (i = 0; i < sizeof velocities / sizeof velocities[0]; ++i) {
        unsigned configured;
        for (configured = 0; configured <= 1; ++configured) {
            check(contract_modified, 1, 1, 0, 1, velocities[i]);
            MEMF(0x38641C) = 81.0f;
            use_override = configured;
            override_calls = needle_calls = 0;
            needle_modified();
            assert(override_calls == 1 && needle_calls == !configured);
            assert(g_esp == stack && g_fp_top == 0);
            assert(g_ebx == speed_data && g_esi == renderer);
        }
    }
    puts("PASS: split-screen numeric speed kept for all gauge types; "
         "analog continuation, units, custom-dial override, stack and text ownership");
    return 0;
}
