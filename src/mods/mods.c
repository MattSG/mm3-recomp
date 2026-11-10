#include "mods.h"
#include "feature_switch.h"

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
        "Fixed native game ticks; intermediate frames repeat the completed image. 30 Hz is the compatibility target; other tick rates are experimental. Off: original frame-driven timing.", 1},
    [MM3_MOD_TRANSLATION] = {"translation", "MM3_MOD_TRANSLATION", "Translation corrections",
        "Hand-written fixes where the generated code differed from the original game (src/mm3_*.c, recomp_manual.c). "
        "Off: the generated code runs without them wherever it has a body of its own (register/SEH preservation, the stream copy). Functions the generator never produced stay filled in.", 1},
    [MM3_MOD_KEYBOARD_DRIVING] = {"keyboard_driving", "MM3_MOD_KEYBOARD_DRIVING", "Keyboard driving",
        "With keyboard controls, the arrows drive and steer in a race, R/F change gear, Space is the handbrake and C the camera; Escape goes back. Off: the arrows are only the d-pad.", 1},
    [MM3_MOD_FLARE_DEPTH] = {"flare_depth", "MM3_MOD_FLARE_DEPTH", "Lamp flare depth",
        "Street-lamp flares are also tested against the scene's depth, so they stay hidden behind buildings and cars. Off: only the game's own visibility test.", 1},
    [MM3_MOD_TIME_LIMITS] = {"time_limits", "MM3_MOD_TIME_LIMITS", "Time limits",
        "Blitz and timed career jobs: the time you start with and each time bonus are scaled by the Time limit setting. Off: the Xbox limits.", 0},
    [MM3_MOD_AI_DIFFICULTY] = {"ai_difficulty", "MM3_MOD_AI_DIFFICULTY", "Opponents' difficulty",
        "The race opponents drive harder or easier, 0 to 9 as set under Opponents' difficulty; 5 is the Xbox game. Off: the Xbox opponents.", 0},
    [MM3_MOD_DAMAGE] = {"damage", "MM3_MOD_DAMAGE", "Car damage",
        "Scales the damage crashes do: Damage dealt for every car but yours, Damage received for your own. Off: the Xbox damage.", 0},
    [MM3_MOD_PAUSE_HOLD] = {"pause_hold", "MM3_MOD_PAUSE_HOLD", "Hold the race (F2)",
        "F2 holds the whole race still without the pause menu (cars, traffic, police, race clock) and F2 again lets it run; the picture keeps drawing. Another key: MM3_PAUSE_KEY (Win32 virtual key). Not with System Link.", 0},
    [MM3_MOD_CAR_LIGHTS] = {"car_lights", "MM3_MOD_CAR_LIGHTS", "Lit tail lights",
        "Tail and brake lamps glow brighter, so the rear of every car reads as lit at night. Off: the Xbox lamp brightness, which is dim (a car's tail lamps are drawn at 0.3 to 0.5).", 1},
};

int mm3_mod_on(int mod)
{
    static int state[MM3_MOD_COUNT];             /* 0 unread, 1 off, 2 on */
    return feature_switch_on(mm3_mods[mod].env, mm3_mods[mod].default_on,
                             mm3_mods[mod].required, &state[mod]);
}
