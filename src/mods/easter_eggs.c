#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recomp_types.h"
#include "xbox_memory_layout.h"
#include "guest_state.h"
#include "mods.h"
#include "easter_eggs.h"

extern void sub_00235F5B_original(void);
extern void sub_00235D84(void);
extern void sub_00235EB6_original(void);
static uint32_t active_filter;

int mm3_old_film_active(void)
{
    return xbox_GuestMapped(active_filter, 12) && MEM32(active_filter + 8) == 6;
}

void sub_00235EB6(void)
{
    active_filter = g_ecx;
    sub_00235EB6_original();
}

void sub_00235F5B(void)
{
    uint32_t filter = g_ecx;
    active_filter = filter;
    /* Match the retail filter's single-player guards. Apply on the native
     * filter thread with a live player, after its resources have loaded. */
    if (mm3_mod_on(MM3_MOD_AUTO_EASTER_EGG) && MEM32(0x407C1C) == 1 &&
        MEM32(0x407C20) == 0 && xbox_GuestMapped(g_esp, 8) && MEM32(g_esp + 4) &&
        xbox_GuestMapped(filter, 0x20)) {
        const char *choice = getenv("MM3_EASTER_EGG_EFFECT");
        uint32_t mode = choice && !strcmp(choice, "8") ? 8u : 6u;
        if (MEM32(filter + 8) != mode && g_esp >= 0x10200u &&
            xbox_GuestMapped(g_esp - 0x110u, 0x110u)) {
            xbox_guest_state state;
            uint32_t color = 0x3859F8u + 3u * (mode - 6u);
            xbox_guest_state_save(&state);
            g_esp -= 0x100u; g_esi = filter; g_eax = mode;
            PUSH32(g_esp, 0); sub_00235D84();
            xbox_guest_state_restore(&state, state.eax);
            fprintf(stderr, "[MM3_EASTER_EGG] %s activated\n", mode == 6 ? "old film" : "sketch");
            /* The landmark trigger sets this tint after the mode setter. */
            MEM32(filter + 0xC) = 0xFF000000u | ((uint32_t)MEM8(color) << 16) |
                ((uint32_t)MEM8(color + 1) << 8) | MEM8(color + 2);
        }
    }
    sub_00235F5B_original();
}
