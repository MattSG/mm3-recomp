#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>
#include "recomp_types.h"

void sub_001DB6E8_original(void);
void sub_001F2A71_original(void);
void sub_001F443D_original(void);
void sub_0035BDC0_original(void);
void xbox_WatchdogStart(void);

void sub_0035BDC0(void)
{
    uint32_t request = MEM32(g_esp + 4u);
    fprintf(stderr, "[USB_ENUM] status=%08X length=%u descriptor=%08X/%08X cancelled=%u\n",
            MEM32(request + 4u), MEM32(request + 0x14u),
            MEM32(0x3BD9E4u), MEM32(0x3BD9E8u), MEM8(0x3BD961u));
    sub_0035BDC0_original();
}

/* Observation only. Input comes through the normal emulated USB report;
 * these wrappers never change guest registers, movie frames, or results. */
static uint32_t movie_total, movie_frame;
static ULONGLONG movie_started;
static const char *movie_name(void)
{
    return movie_total == 144 ? "dice.bik" : movie_total == 308 ? "msgs.bik"
        : movie_total == 3114 ? "intro.bik"
        : movie_total == 3092 ? "AttractMode2.bik"
        : movie_total == 3108 ? "AttractMode0.bik"
        : movie_total == 3250 ? "AttractMode1.bik" : "unknown";
}

void sub_001DB6E8(void)
{
    movie_total = movie_frame = 0;
    movie_started = GetTickCount64();
    fprintf(stderr, "[MOVIE_BEGIN] t=%llu\n", (unsigned long long)movie_started);
    fflush(stderr);
    sub_001DB6E8_original();
    fprintf(stderr, "[MOVIE_END] name=%s total=%u frame=%u elapsed_ms=%llu result=%02X\n",
            movie_name(), movie_total, movie_frame,
            (unsigned long long)(GetTickCount64() - movie_started), g_eax & 255u);
    fflush(stderr);
    /* Register this worker's TLS registers, rather than the host entry
     * thread's idle stack, when investigating post-intro waits. */
    if (movie_total == 3114 && getenv("MM3_POST_MOVIE_WATCHDOG"))
        xbox_WatchdogStart();
}

void sub_001F2A71(void)
{
    uint32_t handle = MEM32(g_ecx + 0xCu);
    uint32_t total = MEM32(handle + 8u);
    uint32_t frame = MEM32(handle + 0xCu);
    if (total != movie_total || frame / 30u != movie_frame / 30u) {
        fprintf(stderr, "[MOVIE_FRAME] total=%u frame=%u elapsed_ms=%llu\n",
                total, frame, (unsigned long long)(GetTickCount64() - movie_started));
        fflush(stderr);
    }
    movie_total = total;
    movie_frame = frame;
    sub_001F2A71_original();
}

void sub_001F443D(void)
{
    uint32_t action = g_eax;
    uint32_t controller = g_ebx;
    uint32_t caller = MEM32(g_esp);
    sub_001F443D_original();
    if (caller == 0x001DB981u && (g_eax & 255u)) {
        fprintf(stderr, "[MOVIE_SKIP_INPUT] action=%u controller=%08X frame=%u/%u\n",
                action, controller, movie_frame, movie_total);
        fflush(stderr);
    }
}
