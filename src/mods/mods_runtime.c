#include "platform/xbox_winnt.h"
#include <stdio.h>
#include <stdlib.h>
#include "mods.h"
#include "render_commands.h"

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
extern void mm3_dvd_media_init(void);                            /* dvd_media.c */

void mm3_mods_init(void)
{
    int i;
    extern void mm3_debug_overlay_init(void);
    mm3_dvd_media_init();
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
