/* Execute the retail automatic gearbox and threshold-builder tail extracted
 * from generated code. Vehicle velocity is controlled; this checks the
 * native consumers rather than predicting road acceleration or drag. */
#include <assert.h>
#include <stdio.h>
#include "recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp, g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top, g_fp_cmp, g_df;
RECOMP_TLS uint16_t g_fp_cc, g_fp_control_word;

typedef struct CarCase {
    const char *name;
    unsigned gears;
    float ratios[8]; /* reverse plus all seven forward gears */
    float radius, final_drive, peak, turbo_peak, turbo_size;
    float cutoff, minimum_shift_rpm, down_factor, shift_time;
} CarCase;
#include "gearing_cases.inc"

static unsigned char memory[0x400000];
static const uint32_t controller = 0x1000, sim = 0x2000;
static const uint32_t engine = 0x3000, body = 0x4000, input = 0x5000;
static const uint32_t ratios = 0x6000, thresholds = 0x7000, stack = 0xD000;
static const uint32_t guard = 0x7FA55AA5u;

/* Only the unrelated driver-input randomizer is outside the extracted body.
 * Automatic tests use input flags=0; reaching it would be a fixture error. */
static void sub_001CAF3E(void) { assert(!"Unexpected randomized driver input"); }

#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#include "production_gearing.inc"

static void build_thresholds(const CarCase *car)
{
    unsigned i;
    unsigned char sim_before[0x200];
    double previous = 0.0;
    double peak = car->turbo_size == 0.0f ? car->peak : car->turbo_peak;
    assert(peak < car->cutoff); /* Shift is reachable before engine cutoff. */
    memset(memory, 0, sizeof memory);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    g_fp_top = 0; g_fp_control_word = 0x37F; g_df = 0;
    memset(g_fp_stack, 0, sizeof g_fp_stack);
    MEMF(0x384D08) = 10.0f;
    MEMF(0x384EEC) = 0.3f;
    MEMF(0x384EF4) = 0.0f;
    MEMF(0x384EFC) = 50.0f;
    MEMF(0x384F04) = 30.0f;
    MEMF(0x384F08) = 100.0f;
    MEMF(0x385DCC) = 60.0f;
    MEMF(0x385F24) = 0.25f;
    MEMF(0x3860F8) = 3.6f;
    MEM32(controller + 0x3C) = sim;
    MEM32(controller + 0x38) = input;
    MEM32(sim) = engine;
    MEM32(sim + 4) = body;
    MEM32(sim + 0x3C) = ratios;
    MEM32(sim + 0x40) = thresholds;
    MEM32(sim + 0x50) = car->gears;
    MEMF(sim + 0x14) = car->radius * 6.283185307179586f;
    MEMF(sim + 0x90) = car->final_drive;
    MEMF(sim + 0xCC) = car->peak;
    MEMF(sim + 0xD0) = car->turbo_peak;
    MEMF(sim + 0xAC) = car->turbo_size;
    MEMF(sim + 0xD4) = car->minimum_shift_rpm;
    MEMF(sim + 0xFC) = 0.1f; /* Native ThrottleMinLimit. */
    MEMF(sim + 0x104) = car->down_factor;
    MEMF(sim + 0xF4) = car->shift_time;
    for (i = 0; i <= car->gears; ++i)
        MEMF(ratios + i * 4) = car->ratios[i];
    /* Native allocations are exactly N+1 ratios and N thresholds. Guard both
     * ends so the seventh gear and the single-speed path cannot hide an
     * off-by-one write in this deliberately spacious guest memory fixture. */
    MEM32(ratios - 4) = MEM32(ratios + (car->gears + 1) * 4) = guard;
    MEM32(thresholds - 4) = MEM32(thresholds + car->gears * 4) = guard;
    memcpy(sim_before, memory + sim, sizeof sim_before);
    g_ebx = controller; g_esi = 0xBEEF; g_edi = 0xCAFE; g_esp = stack;
    MEM32(stack) = 0x12345678;
    MEM32(stack + 4) = 0;
    contract_build_thresholds();
    assert(g_esp == stack + 8 && g_fp_top == 0);
    assert(g_ebx == controller && g_esi == 0xBEEF && g_edi == 0xCAFE);
    assert(!memcmp(sim_before, memory + sim, sizeof sim_before));
    for (i = 0; i < car->gears; ++i) {
        double expected = car->radius * 6.283185307179586 * 3.6 /
            (car->final_drive * 60.0) * (peak - 50.0) / car->ratios[i + 1];
        double actual = MEMF(thresholds + i * 4);
        assert(fabs(actual - expected) < 0.001);
        assert(actual > previous);
        previous = actual;
    }
}

static void assert_tables(const CarCase *car)
{
    unsigned i;
    assert(MEM32(ratios - 4) == guard);
    assert(MEM32(ratios + (car->gears + 1) * 4) == guard);
    assert(MEM32(thresholds - 4) == guard);
    assert(MEM32(thresholds + car->gears * 4) == guard);
    for (i = 0; i <= car->gears; ++i)
        assert(MEMF(ratios + i * 4) == car->ratios[i]);
    assert(MEMF(sim + 0xCC) == car->peak);
    assert(MEMF(sim + 0xD0) == car->turbo_peak);
}

static void shift_command(float velocity_kmh, float rpm, float dt, unsigned command)
{
    MEMF(body + 0x20) = velocity_kmh / 3.6f;
    MEMF(body + 0x24) = MEMF(body + 0x28) = 0.0f;
    MEMF(sim + 0xC) = rpm;
    MEMF(sim + 0x24) = 1.0f; /* Full throttle, no brake. */
    MEMF(sim + 0x28) = 0.0f;
    g_ecx = controller; g_esp = stack;
    g_ebx = 0xABCDEF01; g_esi = 0x23456789; g_edi = 0x456789AB;
    MEM32(stack) = 0x001CC188;
    MEM32(stack + 4) = command;
    MEMF(stack + 8) = dt;
    sub_001CC5D1();
    assert(g_esp == stack + 12 && g_fp_top == 0);
    assert(g_ebx == 0xABCDEF01 && g_esi == 0x23456789 && g_edi == 0x456789AB);
}

static void shift_step(float velocity_kmh, float rpm, float dt)
{
    shift_command(velocity_kmh, rpm, dt, 0);
}

static void test_car(const CarCase *car)
{
    unsigned gear;
    float peak = car->turbo_size == 0.0f ? car->peak : car->turbo_peak;
    assert(car->gears >= 1 && car->gears <= 7);
    build_thresholds(car);
    for (gear = 1; gear <= car->gears; ++gear) {
        float boundary = MEMF(thresholds + (gear - 1) * 4);
        MEM32(sim + 0x2C) = gear;
        MEM32(sim + 0x5C) = 0; /* Native automatic transmission. */
        MEM32(engine) = MEM32(engine + 0x10) = 0;
        shift_step(boundary - 0.1f, peak - 50.0f, 0.01f);
        assert(MEM32(sim + 0x2C) == gear && MEM32(engine + 0x10) == 0);
        shift_step(boundary + 0.1f, peak - 50.0f, 0.01f);
        if (gear == car->gears) {
            assert(MEM32(sim + 0x2C) == gear && MEM32(engine + 0x10) == 0);
        } else {
            assert(MEM32(sim + 0x2C) == gear + 1);
            assert(MEM32(engine + 0x10) == 1);
            assert(fabs(MEMF(engine + 0x14) - car->shift_time) < 0.00001f);
            /* A rising speed during a shift must not skip another gear. */
            shift_step(boundary + 100.0f, peak, car->shift_time * 0.5f);
            assert(MEM32(sim + 0x2C) == gear + 1 && MEM32(engine + 0x10) == 1);
            assert(MEM8(controller + 0x5C) == 1);
            shift_step(boundary + 100.0f, peak, car->shift_time);
            assert(MEM32(sim + 0x2C) == gear + 1 && MEM32(engine + 0x10) == 0);
        }
        assert_tables(car);
    }
    /* Every adjacent automatic downshift boundary, including the last gear,
     * is reachable and has hysteresis rather than immediately shifting back. */
    for (gear = 2; gear <= car->gears; ++gear) {
        float boundary = MEMF(thresholds + (gear - 2) * 4) * car->down_factor;
        MEM32(sim + 0x2C) = gear;
        MEM32(engine) = MEM32(engine + 0x10) = 0;
        shift_step(boundary + 0.1f, peak, 0.01f);
        assert(MEM32(sim + 0x2C) == gear);
        shift_step(boundary - 0.1f, peak, 0.01f);
        assert(MEM32(sim + 0x2C) == gear - 1);
        assert_tables(car);
    }
    /* A sudden stop may skip down several ratios, but never below first. */
    MEM32(sim + 0x2C) = car->gears;
    MEM32(engine) = MEM32(engine + 0x10) = 0;
    shift_step(0.0f, 1000.0f, 0.01f);
    assert(MEM32(sim + 0x2C) == 1);
    MEM32(engine) = MEM32(engine + 0x10) = 0;
    shift_step(0.0f, 1000.0f, 0.01f);
    assert(MEM32(sim + 0x2C) == 1);
    if (car->gears == 1) {
        shift_step(1000.0f, car->cutoff, 0.01f);
        assert(MEM32(sim + 0x2C) == 1 && MEM32(engine + 0x10) == 0);
    }
    /* Manual upper bounds must use N as well: the final gear is reachable
     * and an extra upshift cannot read past the forward ratio array. */
    MEM32(sim + 0x5C) = 1;
    for (gear = 1; gear <= car->gears; ++gear) {
        MEM32(sim + 0x2C) = gear;
        MEM32(engine) = MEM32(engine + 0x10) = 0;
        shift_command(100.0f, peak, 0.01f, 2);
        assert(MEM32(sim + 0x2C) == (gear < car->gears ? gear + 1 : gear));
        if (gear > 1) {
            MEM32(sim + 0x2C) = gear;
            MEM32(engine) = MEM32(engine + 0x10) = 0;
            shift_command(100.0f, peak, 0.01f, 4);
            assert(MEM32(sim + 0x2C) == gear - 1);
        }
        assert_tables(car);
    }
    printf("PASS: %s; gears 1 through %u, native threshold boundaries, "
           "up/down shifts, shift timing, manual bounds, guarded arrays and ABI\n",
           car->name, car->gears);
}

int main(void)
{
    unsigned i;
    for (i = 0; i < sizeof cars / sizeof cars[0]; ++i)
        test_car(&cars[i]);
    return 0;
}
