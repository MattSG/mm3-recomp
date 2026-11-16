#include <stdint.h>
#include "recomp_types.h"
#include "xbox_memory_layout.h"
#include "guest_state.h"
#include "mods.h"

extern void sub_001E50A9_original(void);
extern void sub_001E5E9D_original(void);
extern void sub_0012530C_original(void);
extern void sub_001218F7_original(void);
extern void sub_001238B7_original(void);
extern void sub_001E12E3(void);
extern void sub_001E0F69(void);

static int enabled(int category)
{
    return mm3_mod_on(MM3_MOD_UNLOCK_ALL) || mm3_mod_on(category);
}

void sub_001E50A9(void)
{
    /* Only the catalog's paint-count refresh. Other key queries still see
     * actual collection state, including paint pickups and script events. */
    int paints = MEM32(g_esp) == 0x001DE940u && enabled(MM3_MOD_UNLOCK_PAINTS);
    sub_001E50A9_original();
    if (paints) SET_LO8(g_eax, 1);
}

static uint32_t installed_count(uint32_t city, uint32_t type)
{
    uint32_t stack = g_esp;
    if (city <= 1 && (type == 2 || type == 3) && stack >= 0x10200u &&
        xbox_GuestMapped(stack - 0x110u, 0x110u)) {
        xbox_guest_state state;
        uint32_t count;
        xbox_guest_state_save(&state);
        g_esp = stack - 0x100u;
        PUSH32(g_esp, type); PUSH32(g_esp, 0); sub_001E12E3();
        PUSH32(g_esp, g_eax); g_eax = city; PUSH32(g_esp, 0); sub_001E0F69();
        count = g_eax; /* Installed race count, including installed DLC. */
        xbox_guest_state_restore(&state, state.eax);
        return count;
    }
    return 0;
}

void sub_001E5E9D(void)
{
    uint32_t city = MEM32(g_esp + 4), type = MEM32(g_esp + 8);
    sub_001E5E9D_original();
    if (enabled(MM3_MOD_UNLOCK_RACES) && city <= 1 && (type == 2 || type == 3))
        g_eax = installed_count(city, type);
}

static void open_jobs(uint32_t menu)
{
    if (xbox_GuestMapped(menu, 0x1B8)) {
        /* The title's unlockAllRaces uses 8 as the all-jobs sentinel. */
        MEM32(menu + 0x1B0) = 8;
        MEM32(menu + 0x1B4) = 8;
    }
}

void sub_0012530C(void)
{
    uint32_t menu = g_eax;
    sub_0012530C_original();
    if (enabled(MM3_MOD_UNLOCK_RACES)) open_jobs(menu);
}

void sub_001218F7(void)
{
    uint32_t menu = g_edi, profile = MEM32(0x003C5CB4u), city = MEM32(0x003B8580u);
    int unlock = enabled(MM3_MOD_UNLOCK_RACES) && profile &&
        xbox_GuestMapped(profile, 0x150) && xbox_GuestMapped(menu, 0x1B8);
    /* This menu callback copies its displayed unlock counts back to the
     * profile. Feed it real counts, so even an overlapping save cannot see
     * the cheat's display values. A city change loads its own real counts. */
    if (unlock && city <= 1) {
        MEM32(menu + 0x1B0) = MEM32(profile + 0x140 + city * 4);
        MEM32(menu + 0x1B4) = MEM32(profile + 0x148 + city * 4);
    }
    sub_001218F7_original();
    if (unlock) open_jobs(menu);
}

void sub_001238B7(void)
{
    uint32_t menu = g_esi, job = 0, mission = 0, city = MEM32(0x003B8578u);
    int unlock = enabled(MM3_MOD_UNLOCK_RACES) && xbox_GuestMapped(menu, 0x1B8);
    if (unlock) { job = MEM32(menu + 0x154); mission = MEM32(menu + 0x158); }
    sub_001238B7_original(); /* Returns the selected city in EAX. */
    if (unlock && g_eax <= 1) {
        MEM32(menu + 0x148) = installed_count(g_eax, 2);
        MEM32(menu + 0x150) = installed_count(g_eax, 3);
        if (city != g_eax || job > 6) job = mission = 0;
        if (mission >= (job == 6 ? 3u : 4u)) mission = 0;
        MEM32(menu + 0x154) = job;
        MEM32(menu + 0x158) = mission;
        MEM32(menu + 0x15C) = 7;
        MEM32(menu + 0x160) = job == 6 ? 3 : 4;
    }
}
