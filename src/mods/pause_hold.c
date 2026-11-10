/* Ported from lakshya16157/midtown-madness-3-PC-Native (GPL-3.0), adapted to this project's mod layer.
 *
 * "Hold the race": a key (F2, or MM3_PAUSE_KEY, a Win32 virtual key) holds the race still without the
 * game's pause menu and lets it run again.
 *
 * The game keeps its frame time from one timer object (sub_00170316 reads the counter through
 * sub_0016FC89, a bare rdtsc scaled to 733 MHz, and turns the difference into seconds; sub_001E6C87
 * averages those into the frame time every system steps with). Holding that counter holds the cars,
 * traffic, police, particles, camera and race clock together, and the draw carries on. The game's own
 * pause stays what it is: Start, with its menu.
 *
 * The counter is not stopped but slowed to a hundred-thousandth: a frame time of exactly zero would
 * divide by zero in anything that divides by it. A car's physics runs whole steps of 1/120 s from the
 * time it has gathered, so at this rate a car takes its next step after about fourteen minutes. The
 * game never sees the time spent held: the clock continues from where it stopped.
 *
 * Only the game thread reads the counter this way and only it toggles; a thread of our own counts the
 * key's presses (xbox_FramebufferKeyDown is true only while the game window has the focus). Held away
 * from a race view, while the game's own pause is open (its menu needs a running clock), or with System
 * Link (the other players' clocks would run on) it lets go. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"
#include "mods.h"

extern int xbox_FramebufferKeyDown(int vk);
extern void sub_0016FC89_original(void);
extern void sub_001EA16C_original(void);

#define RATE 100000ull           /* the held clock runs at 1/RATE */
#define PAUSE_ACTIVE 0x00429DB2u /* the game's own pause singleton +2 (open 001E2CD5, resume 001E2BDA) */

static uint64_t s_cut;           /* real ticks the game's clock has dropped */
static uint64_t s_hold_real, s_hold_game;
static volatile LONG s_frozen;
static volatile LONG s_presses;
static volatile LONG s_drawn;    /* GetTickCount of the last race view drawn */
static volatile LONG s_thread;

/* Whether the race is held: the console-rate timing (timing.c) slows its ticks the same way. */
int mm3_pause_frozen(void) { return InterlockedCompareExchange(&s_frozen, 0, 0) != 0; }

/* A race view was drawn in the last 1.5 s. */
static int race_view_recent(void)
{
    return GetTickCount() - (DWORD)InterlockedCompareExchange(&s_drawn, 0, 0) <= 1500u;
}

static int pause_key(void)
{
    const char *v = getenv("MM3_PAUSE_KEY");
    int vk = v && *v ? atoi(v) : 0;
    return vk >= 1 && vk <= 255 && !(vk >= 0x78 && vk <= 0x7A) ? vk : 0x71;   /* F2; never F9-F11 */
}

static DWORD WINAPI watch(void *unused)
{
    int key = pause_key(), was = 0;
    (void)unused;
    fprintf(stderr, "[MM3_PAUSE] hold key 0x%02X\n", key);
    fflush(stderr);
    for (;;) {
        int down = xbox_FramebufferKeyDown(key) != 0;
        if (down && !was) InterlockedIncrement(&s_presses);
        was = down;
        Sleep(4);
    }
    return 0;
}

static int allowed(void)
{
    static int network = -1;
    if (network < 0) {
        const char *v = getenv("RECOMP_NET");
        network = v && *v && strcmp(v, "0") != 0;
        if (network) {
            fprintf(stderr, "[MM3_PAUSE] System Link is on: the hold key is off (the other players' clocks would go on)\n");
            fflush(stderr);
        }
    }
    return !network;
}

static uint64_t game_now(uint64_t real)
{
    if (!s_frozen) return real >= s_cut ? real - s_cut : 0;
    return s_hold_game + (real - s_hold_real) / RATE;
}

static void let_go(uint64_t real, const char *why)
{
    if (!s_frozen) return;
    s_cut = real - game_now(real);
    InterlockedExchange(&s_frozen, 0);
    fprintf(stderr, "[MM3_PAUSE] race running again (%s)\n", why);
    fflush(stderr);
}

static uint64_t pause_clock(uint64_t real)
{
    int presses = (int)InterlockedExchange(&s_presses, 0);
    int race = race_view_recent(), paused = MEM8(PAUSE_ACTIVE) != 0;
    if (!race) presses = 0;               /* a press made in a menu is not for the next race */
    if (s_frozen && (!race || paused)) let_go(real, !race ? "no race view" : "the game's pause opened");
    if ((presses & 1) && allowed()) {
        if (s_frozen) {
            let_go(real, "key");
        } else if (!paused) {
            s_hold_real = real;
            s_hold_game = game_now(real);
            InterlockedExchange(&s_frozen, 1);
            fprintf(stderr, "[MM3_PAUSE] race held\n");
            fflush(stderr);
        }
    }
    return game_now(real);
}

/* The engine timer's counter, as the game reads it. */
void sub_0016FC89(void)
{
    uint64_t real, game;
    sub_0016FC89_original();
    if (!mm3_mod_on(MM3_MOD_PAUSE_HOLD)) return;
    if (!InterlockedExchange(&s_thread, 1)) {
        HANDLE thread = CreateThread(NULL, 0, watch, NULL, 0, NULL);
        if (thread) {
            SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
            CloseHandle(thread);
        }
    }
    real = ((uint64_t)g_edx << 32) | g_eax;
    game = pause_clock(real);
    g_eax = (uint32_t)game;
    g_edx = (uint32_t)(game >> 32);
}

/* The race camera's view matrix is built once per camera per frame while a race is drawn: that is
 * how a race view is told from menus and loading. Player 1's camera only. */
void sub_001EA16C(void)
{
    uint32_t camera = MEM32(g_esp + 4u), manager = MEM32(0x003C5CDCu);
    sub_001EA16C_original();
    if (mm3_mod_on(MM3_MOD_PAUSE_HOLD) && manager >= 0x10000u && camera == MEM32(manager + 0x54u))
        InterlockedExchange(&s_drawn, (LONG)GetTickCount());
}
