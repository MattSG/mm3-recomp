#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp_types.h"
#include "mods.h"

/* The skip-intro mod: the boot movies (dice, msgs, intro) end at their
 * first frame, the way a press of Start ends them -- the movie loop at
 * 0x1DB981 asks 0x1F443D whether skip was pressed, and the mod answers
 * yes. Story and attract movies play as before. The movie name comes from
 * the std::string 0x1DB6E8 is passed (16-byte inline buffer, else a
 * pointer). With MM3_MOVIE_INPUT_TRACE the hooks also report what they see
 * (tools/powershell/Run-MM3MovieSkip.ps1). */
void sub_001DB6E8_original(void);
void sub_001F2A71_original(void);
void sub_001F443D_original(void);

uint32_t mm3_movie_total, mm3_movie_frame;   /* the playing movie, frames */
static int s_skip;                            /* this movie is a boot movie */

static void movie_name(uint32_t str, char *out, size_t cap)
{
    uint32_t len = MEM32(str + 20), res = MEM32(str + 24);
    uint32_t text = res >= 16 ? MEM32(str + 4) : str + 4;
    size_t i;
    if (len >= cap)
        len = (uint32_t)cap - 1;
    for (i = 0; i < len && text; i++)
        out[i] = (char)MEM8(text + i);
    out[i] = 0;
}

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
    movie_name(MEM32(g_esp + 8), name, sizeof name);
    mm3_movie_total = mm3_movie_frame = 0;
    s_skip = mm3_mod_on(MM3_MOD_SKIP_INTRO) && boot_movie(name);
    if (s_skip) {
        fprintf(stderr, "[MOD] skip_intro: %s\n", name);
        fflush(stderr);
    }
#ifdef MM3_MOVIE_INPUT_TRACE
    ULONGLONG started = GetTickCount64();
    fprintf(stderr, "[MOVIE_BEGIN] %s t=%llu\n", name, (unsigned long long)started);
    fflush(stderr);
#endif
    sub_001DB6E8_original();
    s_skip = 0;
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
    if (s_skip)
        g_eax = (g_eax & ~0xFFu) | 1u;   /* skip pressed */
#ifdef MM3_MOVIE_INPUT_TRACE
    if (g_eax & 255u) {
        fprintf(stderr, "[MOVIE_SKIP_INPUT] action=%u controller=%08X frame=%u/%u\n",
                action, controller, mm3_movie_frame, mm3_movie_total);
        fflush(stderr);
    }
#endif
}
