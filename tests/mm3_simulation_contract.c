/* Exercise the production simulation wrappers with synthetic translated calls.
 * The oracle is elapsed world time and the guest calling convention, not a
 * second accumulator implementation. No generated game code or assets needed. */
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp_types.h"
#include "../src/mods/mods.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp, g_fs_base;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;

enum { PROCESSES, MACHINE, WORLD, STAGES };
static unsigned char memory[0x2000];
static const uint32_t game = 0x100u, stack = 0x1000u;
static unsigned calls[STAGES], active_stage;
static uint32_t active_return, active_argument;
static int simulation_on, expected_hz, render_every_frame;
static double world_time, frame_raw;
static float frame_clamped, frame_average;
static unsigned render_calls, render_waits;
static uint32_t render_return;

int mm3_mod_on(int mod)
{
    assert(mod == MM3_MOD_FRAMERATE);
    return simulation_on;
}

static void seed_registers(void)
{
    unsigned i;
    g_eax = 0x10203040u;
    g_ecx = 0x20304050u;
    g_edx = 0x30405060u;
    g_ebx = game;
    g_esi = 0x40506070u;
    g_edi = game;
    g_ebp = 0x50607080u;
    g_seh_ebp = 0x60708090u;
    g_fs_base = 0x708090A0u;
    g_fp_top = 3; g_fp_cmp = 2; g_df = 1;
    g_fp_cc = 0x41u; g_fp_control_word = 0x37fu;
    for (i = 0; i < 8; ++i) g_fp_stack[i] = i + 0.25;
    memset(&g_xmm0, 0x11, sizeof g_xmm0);
    memset(&g_xmm7, 0x77, sizeof g_xmm7);
}

static void assert_preserved_registers(void)
{
    unsigned i;
    assert(g_ebx == game && g_edi == game);
    assert(g_esi == 0x40506070u && g_ebp == 0x50607080u);
    assert(g_seh_ebp == 0x60708090u && g_fs_base == 0x708090A0u);
    assert(g_fp_top == 3 && g_fp_cmp == 2 && g_df == 1);
    assert(g_fp_cc == 0x41u && g_fp_control_word == 0x37fu);
    for (i = 0; i < 8; ++i) assert(g_fp_stack[i] == i + 0.25);
    assert(g_xmm0.u[0] == 0x11111111u && g_xmm7.u[3] == 0x77777777u);
}

static uint32_t result_for(unsigned stage, unsigned count)
{
    return 0xD0000000u + stage * 0x1000u + count;
}

static void translated_return(unsigned stage)
{
    uint32_t result;
    assert(active_stage == stage);
    assert(g_esp == stack);
    assert(MEM32(stack) == active_return);
    assert(MEM32(stack + 4u) == active_argument);
    assert(MEM32(stack - 4u) == 0xBADC0FFEu);
    assert(MEM32(stack + 8u) == 0xF00DBABEu);
    assert(MEM32(stack + 12u) == 0xCAFED00Du);
    /* Each repeated call must receive the original implicit arguments and
     * volatile inputs, even though a preceding call consumed/clobbered them. */
    assert(g_eax == 0x10203040u && g_ecx == 0x20304050u);
    assert(g_edx == 0x30405060u);
    assert_preserved_registers();
    ++calls[stage];
    result = result_for(stage, calls[stage]);
    g_eax = result;
    g_ecx = result ^ 0x00FF00FFu;
    g_edx = result ^ 0xFF00FF00u;
    /* A legal translated callee may reuse its consumed stack area. Stress
     * that the wrapper reconstitutes it for a second/third/fourth call. */
    MEM32(stack) = 0xDEAD0000u + stage;
    MEM32(stack + 4u) = 0xBEEF0000u + stage;
    g_esp += 8u; /* RET 4: discard return address and one argument. */
}

void sub_00025422_original(void) { translated_return(PROCESSES); }
void sub_0002545A_original(void) { translated_return(MACHINE); }
void sub_001E7590_original(void)
{
    float sim_dt = (float)(1.0 / expected_hz);
    assert(g_edi == game);
    if (simulation_on && active_return == 0x001E7895u) {
        /* All three copies must have the exact simulation step, even when
         * the render frame's raw/clamped/averaged values are different. */
        assert(MEMD(game + 0x30u) == (double)sim_dt);
        assert(MEMF(game + 0x4Cu) == sim_dt);
        assert(MEMF(game + 0x54u) == sim_dt);
    } else {
        assert(MEMD(game + 0x30u) == frame_raw);
        assert(MEMF(game + 0x4Cu) == frame_clamped);
        assert(MEMF(game + 0x54u) == frame_average);
    }
    world_time += MEMD(game + 0x30u);
    translated_return(WORLD);
}

void sub_001EC8E6_original(void)
{
    assert(g_esp == stack && MEM32(stack) == render_return);
    assert(MEM32(stack + 4u) == 0x11223344u);
    assert(MEM32(stack + 8u) == 0x55667788u);
    assert_preserved_registers();
    ++render_calls;
    g_eax = 0x99887766u;
    g_ecx = 0x88776655u;
    g_edx = 0x77665544u;
    g_esp += 12u; /* RET 8: return address and two arguments. */
}

static void fixture_sleep(DWORD milliseconds)
{
    assert(milliseconds == 1 && simulation_on && !render_every_frame);
    assert(g_esp == stack + 12u);
    ++render_waits;
}

#define Sleep fixture_sleep
#include "../src/mods/framerate.c"
#undef Sleep

static unsigned invoke_render(uint32_t return_address)
{
    unsigned before = render_calls, waits = render_waits;
    render_return = return_address;
    seed_registers();
    g_esp = stack;
    MEM32(stack - 4u) = 0xBADC0FFEu;
    MEM32(stack) = return_address;
    MEM32(stack + 4u) = 0x11223344u;
    MEM32(stack + 8u) = 0x55667788u;
    MEM32(stack + 12u) = 0xCAFED00Du;
    sub_001EC8E6();
    assert(g_esp == stack + 12u);
    assert_preserved_registers();
    assert(MEM32(stack - 4u) == 0xBADC0FFEu);
    assert(MEM32(stack + 12u) == 0xCAFED00Du);
    if (render_calls == before) {
        assert(render_waits == waits + 1u);
        assert(g_eax == 0x10203040u && g_ecx == 0x20304050u && g_edx == 0x30405060u);
        assert(MEM32(stack) == return_address);
        assert(MEM32(stack + 4u) == 0x11223344u && MEM32(stack + 8u) == 0x55667788u);
    } else {
        assert(render_waits == waits);
        assert(g_eax == 0x99887766u && g_ecx == 0x88776655u && g_edx == 0x77665544u);
    }
    return render_calls - before;
}

static unsigned invoke(unsigned stage, uint32_t return_address)
{
    unsigned before = calls[stage], count;
    uint32_t result;
    active_stage = stage;
    active_return = return_address;
    active_argument = 0x12340000u + stage;
    seed_registers();
    g_esp = stack;
    MEM32(stack - 4u) = 0xBADC0FFEu;
    MEM32(stack) = active_return;
    MEM32(stack + 4u) = active_argument;
    MEM32(stack + 8u) = 0xF00DBABEu;
    MEM32(stack + 12u) = 0xCAFED00Du;
    if (stage == PROCESSES) sub_00025422();
    else if (stage == MACHINE) sub_0002545A();
    else sub_001E7590();
    count = calls[stage] - before;
    assert(g_esp == stack + 8u); /* No double-pop on catch-up; pop on skip. */
    assert_preserved_registers();
    assert(MEM32(stack - 4u) == 0xBADC0FFEu);
    assert(MEM32(stack + 8u) == 0xF00DBABEu);
    assert(MEM32(stack + 12u) == 0xCAFED00Du);
    if (!count) {
        assert(g_eax == 0x10203040u && g_ecx == 0x20304050u);
        assert(g_edx == 0x30405060u);
        assert(MEM32(stack) == active_return);
        assert(MEM32(stack + 4u) == active_argument);
    } else {
        result = result_for(stage, calls[stage]);
        assert(g_eax == result && g_ecx == (result ^ 0x00FF00FFu));
        assert(g_edx == (result ^ 0xFF00FF00u));
    }
    return count;
}

static void render_times(double raw)
{
    frame_raw = raw;
    frame_clamped = (float)(raw * 0.8);
    frame_average = (float)(raw * 0.6);
    MEMD(game + 0x30u) = frame_raw;
    MEMF(game + 0x4Cu) = frame_clamped;
    MEMF(game + 0x54u) = frame_average;
}

static unsigned frame(double raw)
{
    unsigned machine_calls, worlds;
    /* The real loop updates states using the preceding timer value, then
     * samples the current timer immediately before its world call. A stale
     * value must not drive the simulation accumulator. Startup ran once. */
    render_times(10.0 / expected_hz);
    machine_calls = invoke(MACHINE, 0x001E7852u);
    assert(machine_calls == 1u);
    render_times(raw);
    worlds = invoke(WORLD, 0x001E7895u);
    assert(worlds <= 4u);
    /* The rendering side must recover each original frame time exactly. */
    assert(memcmp((void *)XBOX_PTR(game + 0x30u), &frame_raw, sizeof frame_raw) == 0);
    assert(memcmp((void *)XBOX_PTR(game + 0x4Cu), &frame_clamped, sizeof frame_clamped) == 0);
    assert(memcmp((void *)XBOX_PTR(game + 0x54u), &frame_average, sizeof frame_average) == 0);
    assert(invoke_render(0x001E78AAu) == (unsigned)(!simulation_on || render_every_frame || worlds != 0u));
    return worlds;
}

static void check_timing(int render_hz, int irregular)
{
    unsigned i, zero_frames = 0;
    double elapsed = 0.0, duration = 6.0, step = 1.0 / expected_hz;
    static const double frame_factors[] = {0.1, 0.3, 2.4, 1.2};
    for (i = 0; i < (unsigned)(6 * render_hz); ++i) {
        double raw = (irregular ? frame_factors[i % 4u] : 1.0) / render_hz;
        elapsed += raw;
        if (!frame(raw)) ++zero_frames;
        /* A scheduler may lag by at most one simulation tick or round a
         * boundary by a few float ULPs; it must not scale world speed with FPS. */
        assert(fabs(world_time - elapsed) <= step + 0.00001);
    }
    assert(fabs(world_time - duration) <= step + 0.00001);
    assert(calls[PROCESSES] == 1u); /* Initial state entry is startup-only. */
    assert(calls[MACHINE] == (unsigned)(6 * render_hz));
    if (render_hz > expected_hz) assert(zero_frames > 0);
    printf("PASS timing: sim=%d render=%d irregular=%d elapsed=%.6f world=%.6f calls=%u zero_frames=%u\n",
           expected_hz, render_hz, irregular, elapsed, world_time, calls[WORLD], zero_frames);
}

static void check_zero_and_catchup(void)
{
    double step = 1.0 / expected_hz;
    assert(frame(step * 0.25) == 0u);
    assert(frame(step * 0.125) == 0u);
    assert(frame(step * 2.75) >= 2u);
    assert(fabs(world_time - step * 3.125) <= step + 0.00001);
    /* A long hitch has bounded work. The dropped hitch must not be queued
     * indefinitely into the following short frame. */
    assert(frame(step * 10.0) == 4u);
    assert(frame(step * 0.1) == 0u);
    printf("PASS ABI: sim=%d zero ticks, repeated arguments/registers, bounded hitch\n", expected_hz);
}

static void check_other_callers(void)
{
    render_times(10.0 / expected_hz);
    assert(invoke(PROCESSES, 0x00123456u) == 1u);
    assert(invoke(MACHINE, 0x00654321u) == 1u);
    assert(invoke(WORLD, 0x00123456u) == 1u);
    world_time = 0.0;
    /* Out-of-loop calls must not consume the main-loop scheduler's time. */
    assert(frame(0.25 / expected_hz) == 0u);
    /* An alternate render caller and disabling the mod bypass a cached zero. */
    assert(invoke_render(0x00123456u) == 1u);
    simulation_on = 0;
    assert(invoke_render(0x001E78AAu) == 1u);
    simulation_on = 1;
    puts("PASS alternate callers: state changes, state machine and other world calls preserve original semantics without consuming frame time");
}

static void check_invalid_times(void)
{
    assert(frame(0.0) == 0u);
    assert(frame(-0.01) == 0u);
    assert(frame(NAN) == 0u);
    assert(frame(INFINITY) == 0u);
    assert(world_time == 0.0);
    assert(frame(2.0) == 4u);
    assert(frame(0.1 / expected_hz) == 0u);
    assert(frame(1e100) == 4u);
    assert(frame(0.1 / expected_hz) == 0u);
    puts("PASS invalid timers: zero/backward/NaN/infinity add no time; long hitches are bounded before integer conversion");
}

static void check_disabled(void)
{
    unsigned i;
    assert(!simulation_on);
    for (i = 0; i < 90u; ++i) assert(frame(1.0 / 90.0) == 1u);
    assert(fabs(world_time - 1.0) < 0.000001);
    render_times(0.001);
    assert(invoke(PROCESSES, 0x00123456u) == 1u);
    assert(invoke(MACHINE, 0x00654321u) == 1u);
    puts("PASS disabled: original per-frame calls, three frame times, return values and guest ABI");
}

int main(int argc, char **argv)
{
    const char *enabled = getenv("MM3_MOD_FRAMERATE");
    const char *drawing = getenv("MM3_RENDER_EVERY_FRAME");
    assert(argc == 4);
    expected_hz = atoi(argv[2]);
    assert(expected_hz == 30 || expected_hz == 60);
    simulation_on = !enabled || strcmp(enabled, "0") != 0;
    render_every_frame = drawing && strcmp(drawing, "1") == 0;
    memset(memory, 0xA5, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    render_times(10.0 / expected_hz);
    assert(invoke(PROCESSES, 0x001E781Du) == 1u); /* Exactly one initial entry. */
    /* A render before the first world update must keep its original semantics. */
    assert(invoke_render(0x001E78AAu) == 1u);
    if (!strcmp(argv[1], "timing")) check_timing(atoi(argv[3]), 0);
    else if (!strcmp(argv[1], "irregular")) check_timing(atoi(argv[3]), 1);
    else if (!strcmp(argv[1], "abi")) check_zero_and_catchup();
    else if (!strcmp(argv[1], "callers")) check_other_callers();
    else if (!strcmp(argv[1], "invalid")) check_invalid_times();
    else if (!strcmp(argv[1], "off")) check_disabled();
    else assert(!"Unknown contract mode");
    return 0;
}
