/* Ported from lakshya16157/midtown-madness-3-PC-Native (GPL-3.0), adapted to this project's mod layer:
 * switched by its Mm3Mod entry (mods.c), off unless the launcher's Mods tab turns it on. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "mods.h"

/* "Time limits" (MM3_TIME_PERCENT; 100 = the Xbox).
 *
 * Every time limit the scripts set against the player is the timer named
 * race_timer. Blitz races and timed Career jobs create it with
 * timerCreate("race_timer", seconds, timeOut): the handler sub_00201695 hands
 * it to the timer list's create, sub_001F5F24, with the name std::string in
 * EAX and the seconds at [ESP+4]. Jobs top it up with
 * timerChangeDelta("race_timer", seconds) for each delivery or checkpoint: the
 * handler sub_002014BC keeps the seconds at [EBP-4] and the name at EBP-0x20,
 * reads the time left with sub_001F5A5A (returning to 0x201515), adds the
 * seconds and sets the sum. The seconds of a new race_timer and of each
 * bonus are scaled. Penalties (negative changes), the other timers (survive
 * this long, voices, AI checks) and the race clock are left alone. */
extern void sub_001F5F24_original(void);
extern void sub_001F5A5A_original(void);

#define CHANGE_DELTA_RETURN 0x00201515u
#define LOG_LINES 200u

static float time_scale(void)
{
    static float scale = -1.0f;
    if (!mm3_mod_on(MM3_MOD_TIME_LIMITS)) return 1.0f;
    if (scale < 0.0f) {
        const char *v = getenv("MM3_TIME_PERCENT");
        int percent = v && *v ? atoi(v) : 100;
        scale = percent >= 10 && percent <= 1000 ? (float)percent / 100.0f : 1.0f;
        if (scale != 1.0f) fprintf(stderr, "[MM3_TIME] time limits x%.2f (Time limits mod)\n", scale);
    }
    return scale;
}

static int guest_range(uint32_t address, uint32_t size)
{
    return address >= 0x10000u && address < 0x84000000u && size < 0x84000000u - address;
}

/* MSVC std::string: characters inline at +4 below 16 of capacity (+0x18),
 * else a pointer there; length at +0x14. */
static int is_race_timer(uint32_t string)
{
    static const char name[] = "race_timer";
    uint32_t length, text, i;
    if (!guest_range(string, 0x1Cu)) return 0;
    length = MEM32(string + 0x14u);
    if (length != sizeof name - 1u) return 0;
    text = MEM32(string + 0x18u) >= 16u ? MEM32(string + 4u) : string + 4u;
    if (!guest_range(text, length)) return 0;
    for (i = 0; i < length; ++i)
        if (MEM8(text + i) != (uint8_t)name[i]) return 0;
    return 1;
}

static void scale_seconds(uint32_t address, const char *what)
{
    static unsigned lines;
    float seconds = MEMF(address), scaled = seconds * time_scale();
    if (!(seconds > 0.0f && seconds < 100000.0f)) return;
    MEMF(address) = scaled;
    if (lines < LOG_LINES) {
        ++lines;
        fprintf(stderr, "[MM3_TIME] %s %.1f s -> %.1f s\n", what, seconds, scaled);
    }
}

void sub_001F5F24(void)
{
    if (time_scale() != 1.0f && is_race_timer(g_eax)) scale_seconds(g_esp + 4u, "time limit");
    sub_001F5F24_original();
}

void sub_001F5A5A(void)
{
    uint32_t frame = g_ebp;
    if (time_scale() != 1.0f && MEM32(g_esp) == CHANGE_DELTA_RETURN &&
        frame > g_esp && frame - g_esp <= 0x40u && is_race_timer(frame - 0x20u))
        scale_seconds(frame - 4u, "time bonus");
    sub_001F5A5A_original();
}
