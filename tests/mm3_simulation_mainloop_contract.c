/* Execute the actual generated main loop, state machine and state-change
 * helper against the production simulation wrapper. Only other title callees
 * and virtual callbacks are fixtures. The extracted functions keep their
 * real labels, return addresses, arguments and backward branches. */
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <setjmp.h>
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

enum { GAME = 0x1000, GAME_VTABLE = 0x2000, STATE = 0x2200,
       STATE_VTABLE = 0x2300, STACK = 0xD000,
       ENTER_STATE = 0x101, UPDATE_STATE = 0x102, RENDER = 0x103 };
static unsigned char memory[0x10000];
static jmp_buf finished;
static unsigned frames, sampled_frames, target_frames, state_entries, state_updates;
static unsigned world_calls;
static unsigned frame_world_calls, drawn_frames, skipped_frames;
static int simulation_on, simulation_hz, render_hz, irregular, render_every_frame;
static double elapsed, world_time, frame_raw;
static float frame_clamped, frame_average;

int mm3_mod_on(int mod)
{
    assert(mod == MM3_MOD_FRAMERATE);
    return simulation_on;
}

static void return_guest(uint32_t pop) { g_esp += pop; }
void sub_001E7627(void) { return_guest(8); }
void sub_00170316(void)
{
    /* Startup timer data deliberately differs from every real frame. */
    MEMD(GAME + 0x30) = 0.8;
    return_guest(4);
}
void sub_00247571(void) { g_eax = STATE; return_guest(4); }
void sub_0020F7EB(void) { return_guest(8); }
void sub_0024CADA(void) { g_eax = STATE; return_guest(4); }
void sub_0024C787(void) { return_guest(16); }
void sub_001E6C50(void) { MEMD(GAME + 0x30) = 0.7; return_guest(4); }
void sub_001E6C87(void)
{
    static const double factors[] = { 0.1, 0.3, 2.4, 1.2 };
    assert(MEM32(g_esp) == 0x001E788Au && g_edi == GAME);
    assert(state_updates == frames + 1u);
    assert(sampled_frames == frames);
    frame_raw = (irregular ? factors[frames % 4u] : 1.0) / render_hz;
    frame_clamped = (float)(frame_raw * 0.8);
    frame_average = (float)(frame_raw * 0.6);
    MEMD(GAME + 0x30) = frame_raw;
    MEMF(GAME + 0x4C) = frame_clamped;
    MEMF(GAME + 0x54) = frame_average;
    elapsed += frame_raw;
    MEMD(GAME + 0x38) = elapsed;
    ++sampled_frames;
    frame_world_calls = 0;
    return_guest(4);
}
void sub_001EC8E6_original(void)
{
    assert(MEM32(g_esp) == 0x001E78AAu);
    assert(MEM32(g_esp + 4u) == 0u && MEMF(g_esp + 8u) == frame_average);
    assert(!simulation_on || render_every_frame || frame_world_calls > 0u);
    assert(MEMD(GAME + 0x30) == frame_raw);
    assert(MEMF(GAME + 0x4C) == frame_clamped && MEMF(GAME + 0x54) == frame_average);
    ++drawn_frames;
    return_guest(12);
}
void sub_001F373E(void) { return_guest(8); }
void sub_001F3065(void) { return_guest(16); }
void sub_001F34F7(void) { g_eax = 0; return_guest(8); }
void sub_001F33CF(void) { return_guest(12); }

void sub_00025422_original(void);
void sub_0002545A_original(void);
void sub_001E7590_original(void)
{
    float step = (float)(1.0 / simulation_hz);
    assert(MEM32(g_esp) == 0x001E7895u && g_edi == GAME);
    assert(sampled_frames == frames + 1u);
    assert(MEM32(g_esp + 4u) == 0u);
    if (simulation_on) {
        assert(MEMD(GAME + 0x30) == (double)step);
        assert(MEMF(GAME + 0x4C) == step);
        assert(MEMF(GAME + 0x54) == step);
    } else {
        assert(MEMD(GAME + 0x30) == frame_raw);
        assert(MEMF(GAME + 0x4C) == frame_clamped);
        assert(MEMF(GAME + 0x54) == frame_average);
    }
    world_time += MEMD(GAME + 0x30);
    ++world_calls;
    ++frame_world_calls;
    return_guest(8);
}

static void fixture_sleep(DWORD milliseconds)
{
    assert(milliseconds == 1 && simulation_on && !render_every_frame && frame_world_calls == 0u);
    assert(g_esp == STACK - 24u);
    ++skipped_frames;
}

#define Sleep fixture_sleep
#include "../src/mods/framerate.c"
#undef Sleep

static void virtual_call(uint32_t address, uint32_t saved_esp, uint32_t site)
{
    assert(saved_esp >= g_esp + 4u);
    if (address == ENTER_STATE) {
        assert(site == 0x00025451u && g_ecx == STATE);
        assert(state_entries == 0u && state_updates == 0u);
        ++state_entries;
        return_guest(8);
    } else if (address == UPDATE_STATE) {
        assert(site == 0x00025466u && g_ecx == STATE);
        assert(state_entries == 1u && state_updates == frames);
        ++state_updates;
        g_eax = STATE; /* No transition: nested state-change helper returns false. */
        return_guest(8);
    } else {
        double step = 1.0 / simulation_hz;
        assert(address == RENDER && site == 0x001E7920u && g_ecx == GAME);
        assert(g_esp == STACK - 28u); /* All repeated callees pop exactly once. */
        assert(state_entries == 1u && state_updates == frames + 1u);
        assert(sampled_frames == frames + 1u);
        assert(drawn_frames + skipped_frames == frames + 1u);
        assert(MEMD(GAME + 0x30) == frame_raw);
        assert(MEMF(GAME + 0x4C) == frame_clamped);
        assert(MEMF(GAME + 0x54) == frame_average);
        assert(fabs(world_time - elapsed) <= step + 0.00001);
        ++frames;
        if (frames == target_frames) longjmp(finished, 1);
        return_guest(4);
        /* Deliberately poison the value visible to next frame's state update.
         * Only the fresh timer, sampled after that update, may drive ticks. */
        MEMD(GAME + 0x30) = 0.6;
    }
}

#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(address, saved, site) virtual_call((address), (saved), (site))
#define eax g_eax
#define ecx g_ecx
#define edx g_edx
#define esp g_esp
#define ebx g_ebx
#define esi g_esi
#define edi g_edi
#include "simulation_mainloop_generated.h"

int main(int argc, char **argv)
{
    static char stderr_buffer[64 * 1024];
    const char *drawing = getenv("MM3_RENDER_EVERY_FRAME");
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    assert(argc == 5);
    setvbuf(stderr, stderr_buffer, _IOFBF, sizeof stderr_buffer);
    simulation_hz = atoi(argv[1]);
    render_hz = atoi(argv[2]);
    irregular = atoi(argv[3]);
    simulation_on = atoi(argv[4]);
    render_every_frame = drawing && strcmp(drawing, "1") == 0;
    assert(simulation_hz == 30 || simulation_hz == 60);
    target_frames = (unsigned)(6 * render_hz);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    MEM32(GAME) = GAME_VTABLE;
    MEM32(GAME_VTABLE + 0xC) = RENDER;
    MEM32(STATE) = STATE_VTABLE;
    MEM32(STATE_VTABLE + 4) = ENTER_STATE;
    MEM32(STATE_VTABLE + 8) = UPDATE_STATE;
    g_ebx = GAME;
    g_esp = STACK;
    g_fp_control_word = 0x37fu;
    MEM32(STACK) = 0x12345678u;
    if (!setjmp(finished)) sub_001E77F3();
    assert(frames == target_frames);
    assert(state_entries == 1u && state_updates == target_frames);
    assert(sampled_frames == target_frames);
    assert(fabs(elapsed - 6.0) < 0.00001);
    assert(fabs(world_time - elapsed) <= 1.0 / simulation_hz + 0.00001);
    if (simulation_on && render_hz > simulation_hz) assert(world_calls < frames);
    if (!simulation_on) assert(world_calls == frames);
    assert(drawn_frames + skipped_frames == frames);
    if (!simulation_on || render_every_frame) assert(drawn_frames == frames && skipped_frames == 0u);
    if (simulation_on && !render_every_frame && render_hz > simulation_hz) assert(skipped_frames > 0u);
    printf("PASS actual generated loop: sim=%d render=%d irregular=%d enabled=%d every_frame=%d frames=%u entries=%u drawn=%u skipped=%u world=%.6f elapsed=%.6f\n",
           simulation_hz, render_hz, irregular, simulation_on, render_every_frame, frames, state_entries,
           drawn_frames, skipped_frames, world_time, elapsed);
    if (getenv("MM3_SIM_TRACE_HARD_EXIT")) {
        /* Model the smoke harness killing a process with buffered stderr:
         * diagnostics must already have flushed, before C runtime shutdown. */
        fflush(stdout);
        _Exit(0);
    }
    return 0;
}
