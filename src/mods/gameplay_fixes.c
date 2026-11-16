#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "recomp_types.h"
#include "xbox_memory_layout.h"
#include "mods.h"

extern void sub_00088A6E_original(void);
extern void sub_00235D1C_original(void);
extern void sub_00237392_original(void);

static int rental_script(uint32_t name)
{
    static const char suffix[] = "washington/career/01/00logic.lua";
    uint32_t length, i;
    for (length = 0; length < 512; ++length) {
        if (!xbox_GuestMapped(name + length, 1)) return 0;
        if (!MEM8(name + length)) break;
    }
    if (length == 512 || length < sizeof suffix - 1) return 0;
    name += length - (uint32_t)(sizeof suffix - 1);
    for (i = 0; i < sizeof suffix - 1; ++i) {
        unsigned char c = MEM8(name + i);
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != suffix[i]) return 0;
    }
    return 1;
}

/* The fallback must exist before the synchronous stop check can cancel it.
 * Swap only these adjacent statements in this mission's source buffer; the
 * archive, script length and all other Lua remain unchanged. */
static int fix_rental(char *source, uint32_t length)
{
    static const char check[] = "waitForCompetitorToStop()";
    static const char timer[] = "timerCreate(\"competitor_stop_fallback\", comp_stop_fallback_time, competitorStopFallback)";
    uint32_t i, next;
    for (i = 0; i + sizeof check - 1 <= length; ++i) {
        if (memcmp(source + i, check, sizeof check - 1)) continue;
        next = i + sizeof check - 1;
        while (next < length && (source[next] == ' ' || source[next] == '\t' ||
                                source[next] == '\r' || source[next] == '\n')) ++next;
        if (next + sizeof timer - 1 > length ||
            memcmp(source + next, timer, sizeof timer - 1)) continue;
        memmove(source + i + sizeof timer - 1, source + i + sizeof check - 1,
                next - i - (sizeof check - 1));
        memcpy(source + i, timer, sizeof timer - 1);
        memcpy(source + next + sizeof timer - sizeof check, check, sizeof check - 1);
        return 1;
    }
    return 0;
}

/* lua_dobuffer: ECX state, EDX bytes, [ESP+4] length, [ESP+8] name;
 * the original owns both stack arguments and every return register. */
void sub_00088A6E(void)
{
    if (mm3_mod_on(MM3_MOD_RENTAL_FIX) && xbox_GuestMapped(g_esp, 12)) {
        uint32_t length = MEM32(g_esp + 4);
        if (length && length <= 1024 * 1024 && xbox_GuestMapped(g_edx, length) &&
            rental_script(MEM32(g_esp + 8)))
            if (fix_rental((char *)XBOX_PTR(g_edx), length))
                fprintf(stderr, "[MM3_RENTAL_FIX] scheduled fallback before rival stop check\n");
    }
    sub_00088A6E_original();
}

static int leaving_old_film(uint32_t filter)
{
    return mm3_mod_on(MM3_MOD_EASTER_EGG_FIX) &&
        xbox_GuestMapped(filter, 12) && MEM32(filter + 8) == 6;
}

static void reset_old_film_time(int old_film)
{
    /* This singleton's +0x70 is milliseconds per update, not a multiplier.
     * Do not clear a different timing override installed by the game. */
    if (old_film && xbox_GuestMapped(0x00431638u, 4) && MEMF(0x00431638u) == 100.0f)
        MEMF(0x00431638u) = 0.0f;
}

void sub_00235D1C(void)
{
    int old_film = leaving_old_film(g_ecx);
    sub_00235D1C_original();
    reset_old_film_time(old_film);
}

/* Resource cleanup clears the mode too, before the next activation callback. */
void sub_00237392(void)
{
    int old_film = leaving_old_film(g_ecx);
    sub_00237392_original();
    reset_old_film_time(old_film);
}
