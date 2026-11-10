#include <stdio.h>
#include <stdlib.h>
#include "platform/xbox_winnt.h"
#include "platform/frame_limit.h"
#include "recomp_types.h"
#include "nv2a_d3d11.h"
#include "mods.h"
#include "mm3_tick_clock.h"

extern void sub_00025422_original(void);
extern void sub_0002545A_original(void);
extern void sub_001E6C87_original(void);
extern void sub_001E7590_original(void);
extern void sub_001EC8E6_original(void);
extern void sub_0022DBBF_original(void);

/* The entire 1E7849..1E792F iteration is a tick, including the state machine
 * and post-render callbacks. Presentation requests never enter guest code. */
static Mm3TickClock clock_state;
static uint32_t owner, state, scene;
static int initialized, hz, fps;
static unsigned char paused;
static double time_origin;
static FILE *trace;
static unsigned world_calls, ai_calls;
/* Repeat completed images without advancing the guest simulation. */
static void present(void)
{
    nv2a_d3d_repeat_frame();
}

static double seconds(void)
{
    LARGE_INTEGER now;
    static LARGE_INTEGER frequency;
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)frequency.QuadPart;
}

/* The hold-the-race mod (pause_hold.c) slows the world to a hundred-thousandth: each tick publishes a
 * negligible step and the game's clock stands still, then continues from where it stopped. */
extern int mm3_pause_frozen(void);
#define HOLD_RATE 100000.0

static void publish_time(uint32_t game)
{
    static int was_held;
    static unsigned long long hold_start, held_total;
    int held = mm3_pause_frozen();
    double step = held ? clock_state.step / HOLD_RATE : clock_state.step;
    if (held && !was_held) hold_start = clock_state.ticks;
    if (!held && was_held) held_total += clock_state.ticks - hold_start;
    was_held = held;
    MEMD(game + 0x30) = step;
    MEMD(game + 0x38) = time_origin + (double)((held ? hold_start : clock_state.ticks) - held_total) * clock_state.step;
    MEMF(game + 0x4C) = (float)step;
    MEMF(game + 0x54) = (float)step;
}

static void begin_tick(uint32_t game, uint32_t current_state)
{
    double now = seconds();
    uint32_t current_scene = MEM32(game + 0x88);
    unsigned char current_pause = MEM8(0x429DB2u);
    if (!initialized) {
        const char *path = getenv("MM3_TIMING_CSV");
        hz = mm3_tick_rate(getenv("MM3_SIM_HZ"));
        fps = recomp_frame_limit_from_env();
        if (path && *path) {
            trace = fopen(path, "w");
            if (trace) fprintf(trace, "host_seconds,tick,state,scene,paused,dt,dropped_seconds,world_calls,ai_calls,presents\n");
            else fprintf(stderr, "[TIMING] cannot open trace %s\n", path);
        }
        fprintf(stderr, "[TIMING] replacement: complete native ticks at %d Hz, presentation %d FPS; higher tick rates experimental\n", hz, fps);
    }
    if (!initialized || owner != game || state != current_state ||
        scene != current_scene || paused != current_pause) {
        /* Discard catch-up and presentation history at lifecycle boundaries. */
        mm3_tick_clock_reset(&clock_state, now, hz, fps);
        time_origin = MEMD(game + 0x38);
        owner = game; state = current_state; scene = current_scene; paused = current_pause;
        initialized = 1;
        world_calls = ai_calls = 0;
        nv2a_d3d_manual_present(1);
    }
    if (trace && clock_state.ticks) {
        fprintf(trace, "%.9f,%llu,%08X,%08X,%u,%.9f,%.9f,%u,%u,%llu\n",
                now, (unsigned long long)clock_state.ticks, state, scene, paused,
                clock_state.step, clock_state.dropped_seconds, world_calls, ai_calls,
                (unsigned long long)clock_state.presents);
        if (clock_state.ticks % (unsigned)hz == 0) fflush(trace);
    }
    world_calls = ai_calls = 0;
    for (;;) {
        now = seconds();
        if (mm3_tick_clock_take(&clock_state, now)) break;
        if (mm3_tick_clock_present(&clock_state, now))
            present();
        else
            Sleep(1);
    }
    publish_time(game);
}

void sub_00025422(void) { sub_00025422_original(); }

void sub_0002545A(void)
{
    if (mm3_mod_on(MM3_MOD_FRAMERATE) && MEM32(g_esp) == 0x001E7852u)
        begin_tick(MEM32(g_esp + 4), MEM32(g_eax));
    sub_0002545A_original();
}

void sub_001E6C87(void)
{
    uint32_t game = g_edi;
    int scheduled = initialized && owner == game && mm3_mod_on(MM3_MOD_FRAMERATE) &&
                    MEM32(g_esp) == 0x001E788Au;
    sub_001E6C87_original();
    /* Keep native counter/history maintenance, then publish the logical tick
     * to every subsequent callback, not just the world-update subtree. */
    if (scheduled) publish_time(game);
}

void sub_001E7590(void)
{
    if (initialized && MEM32(g_esp) == 0x001E7895u) ++world_calls;
    sub_001E7590_original();
}

void sub_0022DBBF(void)
{
    if (initialized) ++ai_calls;
    /* No shared every-N-call gate: preserve native dispatch ownership. */
    sub_0022DBBF_original();
}


void sub_001EC8E6(void)
{
    /* Once per logical tick. Never rewind guest queues or replay guest calls. */
    sub_001EC8E6_original();
}
