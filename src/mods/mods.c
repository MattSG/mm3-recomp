#include "platform/xbox_winnt.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mods.h"
#include "render_commands.h"

const Mm3Mod mm3_mods[MM3_MOD_COUNT] = {
    [MM3_MOD_DEBUG_OVERLAY] = {"debug_overlay", "MM3_DEBUG_OVERLAY", "ImGui debug tools",
        "In-game interface for developer commands and native teleport. Configure its toggle under Controls.", 0},
    [MM3_MOD_WIDESCREEN] = {"widescreen", "MM3_MOD_WIDESCREEN", "Widescreen",
        "The 3D view widens to the window and the race HUD moves out to its edges. Off: 4:3 with side bars.", 1},
    [MM3_MOD_DRAW_DISTANCE] = {"draw_distance", "MM3_MOD_DRAW_DISTANCE", "Extended draw distance",
        "Traffic, pedestrians, objects, shadows and the city drawn further, as set under Graphics, with native depth fog replacing the fog texture and low-resolution horizon strip. Off: Xbox distances, fog and horizon.", 1},
    [MM3_MOD_SKY] = {"sky", "MM3_MOD_SKY", "Smooth skyline",
        "The distant skyline panorama filtered smoothly instead of in blocks and steps.", 1},
    [MM3_MOD_MENU_MOUSE] = {"menu_mouse", "MM3_MOD_MENU_MOUSE", "Menu mouse",
        "Pointing at a menu item selects it, a click confirms. Works with menu keys on (Controls).", 1},
    [MM3_MOD_DEV_TOOLS] = {"dev_tools", "MM3_MOD_DEV_TOOLS", "Developer tools",
        "The retail developer HUD, debug menu and cameras; their options appear under Settings and Controls. Needs tools\\debug-features\\prepare.py.", 0},
    [MM3_MOD_TELEPORT] = {"teleport", "MM3_DEBUG_TELEPORT", "Teleport debug pipe",
        "Local position and teleport commands for scripts. Use ImGui debug tools for the in-game interface. See DEBUG_TELEPORT.md.", 0},
    [MM3_MOD_COLLISION_FIX] = {"collision_fix", "MM3_MOD_COLLISION_FIX", "Collision cell guard",
        "Out-of-range face indices in a Paris collision cell are repaired at load; without it the game can freeze or drop the car through the road.", 1, 1},
    [MM3_MOD_SKIP_INTRO] = {"skip_intro", "MM3_MOD_SKIP_INTRO", "Skip intro movies",
        "The logo and intro movies at start-up are skipped, as if you pressed Start. Story movies still play.", 1},
    [MM3_MOD_FOV] = {"fov", "MM3_MOD_FOV", "Field of view",
        "The race camera widened as set under Graphics. Off: the Xbox view.", 1},
    [MM3_MOD_FRAMERATE] = {"framerate", "MM3_MOD_FRAMERATE", "Console-rate simulation",
        "The world updates at the Xbox's rate whatever the frame rate (30 a second, or 60 under Settings), so handling, speed and the AI play as on the console. Off: tied to the frame rate.", 1},
    [MM3_MOD_TRANSLATION] = {"translation", "MM3_MOD_TRANSLATION", "Translation corrections",
        "Hand-written fixes where the generated code differed from the original game (src/mm3_*.c, recomp_manual.c). "
        "Off: the generated code runs without them wherever it has a body of its own (register/SEH preservation, the stream copy). Functions the generator never produced stay filled in.", 1},
    [MM3_MOD_KEYBOARD_DRIVING] = {"keyboard_driving", "MM3_MOD_KEYBOARD_DRIVING", "Keyboard driving",
        "With keyboard controls, the arrows drive and steer in a race, R/F change gear, Space is the handbrake and C the camera; Escape goes back. Off: the arrows are only the d-pad.", 1},
    [MM3_MOD_FLARE_DEPTH] = {"flare_depth", "MM3_MOD_FLARE_DEPTH", "Lamp flare depth",
        "Street-lamp flares are also tested against the scene's depth, so they stay hidden behind buildings and cars. Off: only the game's own visibility test.", 1},
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

/* Input follows the primary car's live state. The pause/movie singletons
 * switch immediately; AI updates cannot make a menu look like driving. */
extern int mm3_keyboard_driving(void);
static int in_menu(void)
{
    return !mm3_keyboard_driving();
}

extern void xbox_InputSetMenuProbe(int (*in_menu)(void));
#include "nv2a_d3d11.h"
extern const NvOverlayHooks mm3_hud_hooks;                        /* widescreen_hud.c */
extern void mm3_menu_mouse_init(void);                           /* menu_mouse.c */
extern void mm3_dev_tools_tick(void);                             /* dev_tools.c */
extern void mm3_debug_teleport_init(void);                        /* teleport.c */
extern void mm3_keyboard_driving_init(int (*driving)(void));      /* keyboard_driving.c */

void mm3_mods_init(void)
{
    int i;
    extern void mm3_debug_overlay_init(void);
    mm3_debug_overlay_init();
    mm3_render_commands_init();
    xbox_InputSetMenuProbe(in_menu);
    /* Without the widescreen hooks the title draws 4:3; show it that way. */
    if (!mm3_mod_on(MM3_MOD_WIDESCREEN))
        _putenv_s("RECOMP_ASPECT", "4:3");
    nv2a_d3d_set_overlay_hooks(&mm3_hud_hooks);
    if (mm3_mod_on(MM3_MOD_MENU_MOUSE))
        mm3_menu_mouse_init();
    if (mm3_mod_on(MM3_MOD_KEYBOARD_DRIVING))
        mm3_keyboard_driving_init(mm3_keyboard_driving);
    if (mm3_mod_on(MM3_MOD_TELEPORT) || mm3_mod_on(MM3_MOD_DEBUG_OVERLAY))
        mm3_debug_teleport_init();
    for (i = 0; i < MM3_MOD_COUNT; i++)
        fprintf(stderr, "[MOD] %s %s\n", mm3_mods[i].id, mm3_mod_on(i) ? "on" : "off");
}

void mm3_mods_player_update(void)
{
    if (mm3_mod_on(MM3_MOD_DEV_TOOLS) || mm3_mod_on(MM3_MOD_DEBUG_OVERLAY))
        mm3_dev_tools_tick();
}
