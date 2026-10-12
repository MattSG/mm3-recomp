#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mods.h"

const Mm3Mod mm3_mods[MM3_MOD_COUNT] = {
    [MM3_MOD_WIDESCREEN] = {"widescreen", "MM3_MOD_WIDESCREEN", "Widescreen",
        "The 3D view widens to the window and the race HUD moves out to its edges. Off: 4:3 with side bars.", 1},
    [MM3_MOD_DRAW_DISTANCE] = {"draw_distance", "MM3_MOD_DRAW_DISTANCE", "Extended draw distance",
        "Traffic, pedestrians, objects, shadows and the city drawn further, as set under Graphics. Off: Xbox distances.", 1},
    [MM3_MOD_SKY] = {"sky", "MM3_MOD_SKY", "Smooth skyline",
        "The distant skyline panorama filtered smoothly instead of in blocks and steps.", 1},
    [MM3_MOD_MENU_MOUSE] = {"menu_mouse", "MM3_MOD_MENU_MOUSE", "Menu mouse",
        "Pointing at a menu item selects it, a click confirms. Works with menu keys on (Controls).", 1},
    [MM3_MOD_DEV_TOOLS] = {"dev_tools", "MM3_MOD_DEV_TOOLS", "Developer tools",
        "The retail developer HUD, debug menu and cameras; their options appear under Settings and Controls. Needs tools\\debug-features\\prepare.py.", 0},
    [MM3_MOD_TELEPORT] = {"teleport", "MM3_DEBUG_TELEPORT", "Teleport debugger",
        "A live car position panel and teleport by coordinates. See DEBUG_TELEPORT.md.", 0},
    [MM3_MOD_OFFLINE] = {"offline", "MM3_MOD_OFFLINE", "Offline network",
        "Skips the frontend's wait for the network while Network (Settings) is off. Off, with no network: startup takes about 9 s longer.", 1},
    [MM3_MOD_COLLISION_FIX] = {"collision_fix", "MM3_MOD_COLLISION_FIX", "Collision cell guard",
        "Out-of-range face indices in a Paris collision cell are repaired at load; without it the game can freeze or drop the car through the road.", 1, 1},
    [MM3_MOD_DISC_CHECK] = {"disc_check", "MM3_MOD_DISC_CHECK", "Disc check bypass",
        "Skips the DVD authentication the emulated drive cannot pass.", 1, 1},
    [MM3_MOD_TRANSLATION] = {"translation", "MM3_MOD_TRANSLATION", "Translation corrections",
        "Hand-written fixes where the generated code differed from the original game (src/mm3_*.c, recomp_manual.c). "
        "Off: the generated code runs without them wherever it has a body of its own (register/SEH preservation, the stream copy). Functions the generator never produced stay filled in.", 1},
};

int mm3_mod_on(int mod)
{
    static int state[MM3_MOD_COUNT];             /* 0 unread, 1 off, 2 on */
    if (mm3_mods[mod].required)
        return 1;
    if (!state[mod]) {
        const char *v = getenv(mm3_mods[mod].env);
        state[mod] = (v && *v ? strcmp(v, "0") != 0 : mm3_mods[mod].default_on) ? 2 : 1;
    }
    return state[mod] == 2;
}

/* Player updates only run in gameplay, so a gap means a menu: the frontend,
 * car select or a pause. RECOMP_MENU_KEYS follows it. Not a mod: input. */
static volatile DWORD last_update;
static int in_menu(void)
{
    return GetTickCount() - last_update > 300;
}

extern void xbox_InputSetMenuProbe(int (*in_menu)(void));
extern void xbox_InputSetMenuPointer(int (*pointer)(float x, float y, int *offset));
extern void nv2a_d3d_set_bspline_blend(uint32_t sfactor, uint32_t dfactor);
#include "nv2a_d3d11.h"
extern const NvOverlayHooks mm3_hud_hooks;                        /* widescreen_hud.c */
extern int mm3_menu_pointer(float x, float y, int *offset);       /* menu_mouse.c */
extern void mm3_dev_tools_tick(void);                             /* dev_tools.c */
extern void mm3_debug_teleport_init(void);                        /* teleport.c */

void mm3_mods_init(void)
{
    int i;
    xbox_InputSetMenuProbe(in_menu);
    /* Without the widescreen hooks the title draws 4:3; show it that way. */
    if (!mm3_mod_on(MM3_MOD_WIDESCREEN) && !getenv("RECOMP_ASPECT"))
        _putenv_s("RECOMP_ASPECT", "4:3");
    /* The horizon panorama: a 256x256 strip around the whole horizon drawn
     * without depth test through destination alpha, where the haze pass
     * left sky, magnified ten times and more. */
    if (mm3_mod_on(MM3_MOD_WIDESCREEN))
        nv2a_d3d_set_overlay_hooks(&mm3_hud_hooks);
    if (mm3_mod_on(MM3_MOD_SKY))
        nv2a_d3d_set_bspline_blend(0x304, 0x305);      /* DST_ALPHA, ONE_MINUS_DST_ALPHA */
    if (mm3_mod_on(MM3_MOD_MENU_MOUSE))
        xbox_InputSetMenuPointer(mm3_menu_pointer);
    if (mm3_mod_on(MM3_MOD_TELEPORT))
        mm3_debug_teleport_init();
    for (i = 0; i < MM3_MOD_COUNT; i++)
        fprintf(stderr, "[MOD] %s %s\n", mm3_mods[i].id, mm3_mod_on(i) ? "on" : "off");
}

void mm3_mods_player_update(void)
{
    last_update = GetTickCount();
    if (mm3_mod_on(MM3_MOD_DEV_TOOLS))
        mm3_dev_tools_tick();
}
