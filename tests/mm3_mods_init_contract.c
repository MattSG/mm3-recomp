/* Settings are cached, so run enabled/disabled modes in separate processes. */
#include <assert.h>
#include "../src/mods/mods.c"

static int driving;
static int render_commands_initialized;
void mm3_render_commands_init(void) { ++render_commands_initialized; }
static int (*menu_probe)(void);
static unsigned mouse_init_calls;
static const NvOverlayHooks *overlay;
static unsigned sky_calls, teleport_calls, dev_calls;
static int (*keyboard_probe)(void);
static uint32_t flare_program;
const NvOverlayHooks mm3_hud_hooks = {0};

int mm3_keyboard_driving(void) { return driving; }
void xbox_InputSetMenuProbe(int (*probe)(void)) { menu_probe = probe; }
void mm3_menu_mouse_init(void) { ++mouse_init_calls; }
void nv2a_d3d_set_overlay_hooks(const NvOverlayHooks *hooks) { overlay = hooks; }
void nv2a_d3d_set_bspline_blend(uint32_t source, uint32_t destination)
{
    assert(source == 0x304 && destination == 0x305);
    ++sky_calls;
}
void mm3_debug_teleport_init(void) { ++teleport_calls; }
void mm3_dev_tools_tick(void) { ++dev_calls; }
void mm3_debug_overlay_init(void) {}
void mm3_keyboard_driving_init(int (*probe)(void)) { keyboard_probe = probe; }
void nv2a_d3d_set_flare_depth_test(uint32_t program) { flare_program = program; }

int main(int argc, char **argv)
{
    int enabled;
    assert(argc == 2);
    enabled = !strcmp(argv[1], "enabled");
    _putenv_s("MM3_MOD_WIDESCREEN", enabled ? "1" : "0");
    _putenv_s("RECOMP_ASPECT", "16:10");
    _putenv_s("RECOMP_WINDOW_SIZE", "1600x900");
    _putenv_s("MM3_MOD_SKY", "1");
    _putenv_s("MM3_MOD_MENU_MOUSE", "1");
    _putenv_s("MM3_DEBUG_TELEPORT", "1");
    _putenv_s("MM3_MOD_DEV_TOOLS", "1");
    _putenv_s("MM3_MOD_COLLISION_FIX", "0");
    _putenv_s("MM3_MOD_KEYBOARD_DRIVING", enabled ? "1" : "0");
    _putenv_s("MM3_MOD_FLARE_DEPTH", enabled ? "1" : "0");
    mm3_mods_init();
    assert(render_commands_initialized == 1);
    assert(!strcmp(getenv("RECOMP_ASPECT"), enabled ? "16:10" : "4:3"));
    assert(!strcmp(getenv("RECOMP_WINDOW_SIZE"), "1600x900"));
    assert(overlay == &mm3_hud_hooks); /* UV sanitizer also runs in 4:3. */
    assert(mouse_init_calls == 1);
    assert(sky_calls == 1 && teleport_calls == 1);
    assert(mm3_mod_on(MM3_MOD_COLLISION_FIX)); /* Required guard stays on. */
    assert(keyboard_probe == (enabled ? mm3_keyboard_driving : NULL));
    assert(flare_program == (enabled ? 0x8421u : 0u)); /* Flares.psh */
    driving = 0; assert(menu_probe && menu_probe());
    driving = 1; assert(!menu_probe());
    driving = 0; assert(menu_probe());
    mm3_mods_player_update();
    assert(dev_calls == 1 && teleport_calls == 1);
    puts("Mod initialization, aspect and primary-car menu probe contract passed.");
    return 0;
}
