#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static unsigned char memory[0x440000];
uint32_t g_esp, g_eax, g_edi;
static double host_time;
static int enabled = 1, manual, native_states, native_worlds, native_ai;

#define QueryPerformanceCounter test_counter
#define QueryPerformanceFrequency test_frequency
#define Sleep test_sleep
#include "timing.c"

int QueryPerformanceCounter(LARGE_INTEGER *v) { v->QuadPart = (long long)(host_time * 1000000); return 1; }
int QueryPerformanceFrequency(LARGE_INTEGER *v) { v->QuadPart = 1000000; return 1; }
void Sleep(unsigned long ms) { host_time += (double)ms / 1000; }
int mm3_mod_on(int mod) { (void)mod; return enabled; }
int recomp_frame_limit_from_env(void) { return 144; }
void nv2a_d3d_manual_present(int on) { manual = on; }
int mm3_pause_frozen(void) { return 0; }
void nv2a_d3d_repeat_frame(void) { host_time += 1.0 / 144; }
int xbox_GuestMapped(uint32_t va, uint32_t bytes) { (void)va; (void)bytes; return 0; }
void sub_00025422_original(void) { g_esp += 8; }
void sub_0002545A_original(void) { ++native_states; g_esp += 8; }
void sub_001E6C87_original(void)
{
    MEMD(g_edi + 0x30) = .001;
    MEMD(g_edi + 0x38) = host_time;
    MEMF(g_edi + 0x4C) = MEMF(g_edi + 0x54) = .001f;
    g_esp += 4;
}
void sub_001E7590_original(void) { ++native_worlds; g_esp += 8; }
void sub_001EC8E6_original(void) { g_esp += 12; }
void sub_0022DBBF_original(void) { ++native_ai; g_esp += 12; }

static uint32_t fixture_state = 0x11000, fixture_state_value = 0x123456;
static void state_step(uint32_t game)
{
    g_esp = 0x10000; g_eax = fixture_state;
    MEM32(g_esp) = 0x001E7852u; MEM32(g_esp + 4) = game;
    MEM32(g_eax) = fixture_state_value;
    sub_0002545A();
    assert(g_esp == 0x10008);
}

int main(void)
{
    uint32_t game = 0x12000;
    int i;
#ifdef _WIN32
    _putenv_s("MM3_SIM_HZ", "30");
#else
    setenv("MM3_SIM_HZ", "30", 1);
#endif
    for (i = 0; i < 300; ++i) {
        state_step(game);
        assert(manual && fabs(MEMD(game + 0x30) - 1.0 / 30) < 1e-12);
        g_edi = game; g_esp = 0x10000; MEM32(g_esp) = 0x001E788Au;
        sub_001E6C87();
        assert(g_esp == 0x10004);
        assert(fabs(MEMD(game + 0x38) - (i + 1) / 30.0) < 1e-9);
        g_esp = 0x10000; MEM32(g_esp) = 0x001E7895u;
        sub_001E7590(); assert(g_esp == 0x10008);
        g_esp = 0x10000; sub_0022DBBF(); assert(g_esp == 0x1000C);
    }
    assert(native_states == 300 && native_worlds == 300 && native_ai == 300);
    assert(host_time > 9.96 && host_time < 10.01);
    host_time += 5;
    MEM8(0x429DB2) = 1; /* pause boundary drops suspension debt */
    state_step(game);
    assert(clock_state.ticks == 1 && clock_state.dropped_seconds == 0);
    MEM8(0x429DB2) = 0; state_step(game);
    MEM32(game + 0x88) = 0x1234; state_step(game);
    state_step(0x14000);
    fixture_state_value = 0x654321; state_step(0x14000);
    enabled = 0;
    g_esp = 0x10000; MEM32(g_esp) = 0x001E788Au; g_edi = game;
    sub_001E6C87();
    assert(MEMD(game + 0x30) == .001);
    puts("MM3 replacement timing wrapper contracts passed");
    return 0;
}
