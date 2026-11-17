#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/xbox_winnt.h"
#include "recomp_types.h"
#include "mods.h"
#include "xbox_std_string.h"

/* The skip-intro mod: the boot movies (dice, msgs, intro) are not played.
 * 0x1DB6E8 returns at once with what a skip at the first frame returns
 * (AL = 1); played and skipped there, each drew its black first frame in the
 * middle of the loading screen. Story and attract movies play as before, and
 * the movie loop at 0x1DB981 still asks 0x1F443D whether skip was pressed. The movie name comes from
 * the std::string 0x1DB6E8 is passed (16-byte inline buffer, else a
 * pointer). With MM3_MOVIE_INPUT_TRACE the hooks also report what they see
 * (tools/scripts/run_movie_skip.py). */
void sub_001DB6E8_original(void);
void sub_001F2A71_original(void);
void sub_001F443D_original(void);

uint32_t mm3_movie_total, mm3_movie_frame;   /* the playing movie, frames */


/* The game names movies by stem ("dice"); a path or ".bik" is allowed for. */
static int boot_movie(const char *name)
{
    static const char *const boot[] = {"dice", "msgs", "intro"};
    const char *base = strrchr(name, '\\');
    char stem[64];
    base = base ? base + 1 : name;
    snprintf(stem, sizeof stem, "%s", base);
    if (strchr(stem, '.'))
        *strchr(stem, '.') = 0;
    for (int i = 0; i < 3; i++)
        if (!_stricmp(stem, boot[i]))
            return 1;
    return 0;
}

void sub_001DB6E8(void)
{
    char name[96];
    xbox_read_msvc_std_string(MEM32(g_esp + 8), name, sizeof name);
    mm3_movie_total = mm3_movie_frame = 0;
    if (mm3_mod_on(MM3_MOD_SKIP_INTRO) && boot_movie(name)) {
        fprintf(stderr, "[MOD] skip_intro: %s\n", name);
        fflush(stderr);
        g_eax = (g_eax & ~0xFFu) | 1u;   /* skipped */
        g_esp += 16;                      /* ret 12 */
        return;
    }
#ifdef MM3_MOVIE_INPUT_TRACE
    ULONGLONG started = GetTickCount64();
    fprintf(stderr, "[MOVIE_BEGIN] %s t=%llu\n", name, (unsigned long long)started);
    fflush(stderr);
#endif
    sub_001DB6E8_original();
#ifdef MM3_MOVIE_INPUT_TRACE
    fprintf(stderr, "[MOVIE_END] name=%s total=%u frame=%u elapsed_ms=%llu result=%02X\n", name,
            mm3_movie_total, mm3_movie_frame, (unsigned long long)(GetTickCount64() - started), g_eax & 255u);
    fflush(stderr);
    {   /* this worker's TLS registers, for post-intro waits */
        extern void xbox_WatchdogStart(void);
        if (mm3_movie_total == 3114 && getenv("MM3_POST_MOVIE_WATCHDOG"))
            xbox_WatchdogStart();
    }
#endif
}

void sub_001F2A71(void)
{
    uint32_t handle = MEM32(g_ecx + 0xCu);
#ifdef MM3_MOVIE_INPUT_TRACE
    uint32_t total = MEM32(handle + 8u), frame = MEM32(handle + 0xCu);
    if (total != mm3_movie_total || frame / 30u != mm3_movie_frame / 30u) {
        fprintf(stderr, "[MOVIE_FRAME] total=%u frame=%u\n", total, frame);
        fflush(stderr);
    }
#endif
    mm3_movie_total = MEM32(handle + 8u);
    mm3_movie_frame = MEM32(handle + 0xCu);
    sub_001F2A71_original();
}

void sub_001F443D(void)
{
    uint32_t caller = MEM32(g_esp);
#ifdef MM3_MOVIE_INPUT_TRACE
    uint32_t action = g_eax, controller = g_ebx;
#endif
    sub_001F443D_original();
    if (caller != 0x001DB981u)
        return;
#ifdef MM3_MOVIE_INPUT_TRACE
    if (g_eax & 255u) {
        fprintf(stderr, "[MOVIE_SKIP_INPUT] action=%u controller=%08X frame=%u/%u\n",
                action, controller, mm3_movie_frame, mm3_movie_total);
        fflush(stderr);
    }
#endif
}
