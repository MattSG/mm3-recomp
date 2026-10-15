/* Settings launcher shown when the exe is started without RECOMP_PB_EXEC
 * (double-click). Scripts and harnesses set it and boot straight in. The
 * choices are the README's RECOMP_* variables; mm3_settings.ini stores them
 * as KEY=VALUE next to game_files\ and Play exports them to the runtime. */
#include <RmlUi/Core.h>
#include <RmlUi_Backend.h>
#include <RmlUi_Renderer_DX11.h>
#include <windows.h>
#include <dwmapi.h>
#include <Xinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mods/mods.h"

namespace {

struct Choice {
    Rml::String value, label;
};

struct Option {
    Rml::String header, label, hint, env, def, value;
    Rml::Vector<Choice> choices;
    int page; /* 0 Settings, 1 Controls, 2 Mods */
    bool toggle; /* Off/On: drawn as a switch */
    bool fixed;  /* one choice: shown, not changeable */
    bool dev;    /* a developer tool: shown while that mod is on */
    bool text;   /* typed rather than chosen */
};

/* A key for a pad input (RECOMP_KEYMAP, defaults from
 * tools/xboxrecomp/src/input/xinput_device.c) or a developer hotkey
 * (MM3_DEV_HOTKEYS, src/mods/dev_tools.c). name is its name there. */
struct Bind {
    Rml::String header, label, name;
    int def, vk;
    Rml::String key; /* vk's display name */
    Rml::String env = "RECOMP_KEYMAP";
    bool dev = false;
};

Rml::Vector<Option> g_options = {
    {"Display", "Mode", "F11 or Alt+Enter switches in game.", "RECOMP_FULLSCREEN", "", "",
     {{"", "Windowed"}, {"1", "Fullscreen"}}},
    {"", "Window size", "Starting size of the game window.", "RECOMP_WINDOW_SIZE", "", "",
     {{"", "Auto"}, {"1280x720", "720p"}, {"1600x900", "900p"}, {"1920x1080", "1080p"}, {"2560x1440", "1440p"}}},
    {"", "Aspect ratio", "Widescreen HUD follows it.", "RECOMP_ASPECT", "", "",
     {{"", "Window"}, {"16:10", "16:10"}, {"16:9", "16:9"}, {"4:3", "4:3"}}},
    {"", "Frame limit", "", "RECOMP_FPS_LIMIT", "60", "",
     {{"30", "30"}, {"60", "60"}, {"120", "120"}, {"144", "144"}, {"165", "165"}, {"240", "240"},
      {"0", "Unlimited"}}},
    {"", "Tick rate", "World updates a second, whatever the frame rate. The Xbox ran 30.",
     "MM3_SIM_HZ", "60", "", {{"30", "30 (Xbox)"}, {"60", "60"}, {"90", "90"}}},
    {"", "Draw every frame", "Also draw frames between world updates; they show the last update again.",
     "MM3_RENDER_EVERY_FRAME", "0", "", {{"0", "Off"}, {"1", "On"}}, 0, true},
    {"", "V-Sync", "", "RECOMP_VSYNC", "0", "",
     {{"0", "Off"}, {"1", "On"}}},
    {"", "In the background", "When another window is in front.", "RECOMP_PAUSE_UNFOCUSED", "1", "",
     {{"1", "Pause"}, {"0", "Keep running"}}},
    {"Graphics", "Internal resolution", "Fixed multiple of 480 lines; overrides supersampling.",
     "RECOMP_RENDER_SCALE", "", "", {{"", "Auto"}, {"1", "480p"}, {"1.5", "720p"}, {"2", "960p"}, {"3", "1440p"}, {"4", "1920p"}}},
    {"", "Supersampling", "Pixels rendered per window pixel.", "RECOMP_SUPERSAMPLE", "2", "",
     {{"1", "1x"}, {"2", "2x"}, {"3", "3x"}, {"4", "4x"}}},
    {"", "Anti-aliasing", "MSAA samples.", "RECOMP_MSAA", "4", "",
     {{"1", "Off"}, {"2", "2x"}, {"4", "4x"}, {"8", "8x"}}},
    {"", "Texture filtering", "Anisotropic filtering.", "RECOMP_ANISO", "16", "",
     {{"1", "Off"}, {"4", "4x"}, {"8", "8x"}, {"16", "16x"}}},
    {"", "Detail distance", "Traffic, pedestrians, objects and shadows.", "RECOMP_DRAW_DISTANCE", "4", "",
     {{"1", "Xbox"}, {"2", "2x"}, {"3", "3x"}, {"4", "4x"}}},
    {"", "View distance", "How far the city is drawn.", "RECOMP_VIEW_DISTANCE", "2", "",
     {{"1", "Xbox"}, {"2", "2x"}, {"3", "3x"}, {"4", "4x"}}},
    {"", "Field of view", "How wide the race camera sees.", "MM3_FOV", "", "",
     {{"", "Xbox"}, {"110", "+10%"}, {"120", "+20%"}, {"130", "+30%"}}},
    {"Network", "Network", "Work in progress. System link works between copies on one PC or a LAN; "
     "Xbox Live is untested.", "RECOMP_NET", "", "", {{"", "Off"}, {"nat", "On"}}},
    {"", "Xbox Live", "Work in progress: reaches Insignia's servers, but signing in needs an account "
     "made on xemu (scripts/import_xemu_accounts.py) and is unverified.", "", "", "", {{"", "Insignia (WIP)"}}},
    {"", "Direct play", "Work in progress, untested between separate PCs. Other players to link with, "
     "as address:port, comma separated. One side is enough: the other is found when it first hears "
     "from you.", "RECOMP_NET_PEERS", "", "", {{"", ""}}},
    {"", "Your port", "UDP port other players reach you on. Forward it on your router to be found.",
     "RECOMP_NET_PORT", "3075", "", {{"", ""}}},
    {"Developer", "Developer HUD", "Position, damage, speed, RPM and gear. Needs tools\\debug-features\\prepare.py.",
     "MM3_DEV_HUD", "", "", {{"", "Off"}, {"1", "On"}}},
    {"", "Debug menu", "The retail developer menu (D-pad). Needs prepare.py.",
     "MM3_DEBUG_MENU", "", "", {{"", "Off"}, {"1", "On"}}},
    {"", "Extended cameras", "Developer camera modes. Needs prepare.py.",
     "MM3_EXTENDED_CAMERAS", "", "", {{"", "Off"}, {"1", "On"}}},
    {"", "E3 preset", "Runs the shipped E3settings.lua. Needs prepare.py.",
     "MM3_E3_PRESET", "", "", {{"", "Off"}, {"1", "On"}}},
    {"Debug overlay", "Show / hide key", "Works independently of gameplay keyboard input.",
     "MM3_OVERLAY_KEY", "73", "", {{"73", "F4"}, {"74", "F5"}, {"77", "F8"}, {"2D", "Insert"}}, 1},
    {"", "Show / hide controller", "Hold both buttons. Uses physical controller 1 buttons.",
     "MM3_OVERLAY_PAD", "60", "", {{"0", "Unbound"}, {"60", "Back + left stick"}, {"C0", "Both sticks"}, {"300", "LB + RB"}}, 1},
    {"", "Show / hide mouse", "Optional mouse button toggle.",
     "MM3_OVERLAY_MOUSE", "0", "", {{"0", "Unbound"}, {"4", "Middle"}, {"5", "Side 1"}, {"6", "Side 2"}}, 1},
    {"Keyboard and mouse", "Use keyboard and mouse", "Plays as controller 1, alongside any pad.",
     "RECOMP_KEYBOARD", "", "", {{"", "Off"}, {"1", "On"}}, 1},
    {"", "Menu keys and mouse", "In menus: arrows, WASD or your left-stick keys move, pointing at an item "
     "selects it, Space or a click confirms, Esc or right click goes back.", "RECOMP_MENU_KEYS", "", "",
     {{"", "Off"}, {"1", "On"}}, 1},
    {"", "Developer hotkeys", "Toggle the developer tools in a race with the keys below. Needs prepare.py.",
     "MM3_DEV_HOTKEYS_ON", "", "", {{"", "Off"}, {"1", "On"}}, 1},
};

Rml::Vector<Bind> g_binds = {
    {"Buttons", "A", "A", 'Z'}, {"", "B", "B", 'X'}, {"", "X", "X", 'A'}, {"", "Y", "Y", 'S'},
    {"", "Black", "BLACK", 'E'}, {"", "White", "WHITE", 'Q'},
    {"", "Start", "START", VK_RETURN}, {"", "Back", "BACK", VK_BACK},
    {"Triggers", "Left trigger", "LT", '1'}, {"", "Right trigger", "RT", '3'},
    {"D-pad", "Up", "UP", VK_UP}, {"", "Down", "DOWN", VK_DOWN},
    {"", "Left", "LEFT", VK_LEFT}, {"", "Right", "RIGHT", VK_RIGHT},
    {"Left stick", "Up", "LY+", VK_NUMPAD8}, {"", "Down", "LY-", VK_NUMPAD2},
    {"", "Left", "LX-", VK_NUMPAD4}, {"", "Right", "LX+", VK_NUMPAD6}, {"", "Click", "LS", VK_SHIFT},
    {"Right stick", "Up", "RY+", 'I'}, {"", "Down", "RY-", 'K'},
    {"", "Left", "RX-", 'J'}, {"", "Right", "RX+", 'L'}, {"", "Click", "RS", VK_CONTROL},
    /* Controller buttons (RECOMP_PADMAP): vk is the source button + 1. */
    {"Controller", "A", "A", 1, 0, "", "RECOMP_PADMAP"}, {"", "B", "B", 2, 0, "", "RECOMP_PADMAP"},
    {"", "X", "X", 3, 0, "", "RECOMP_PADMAP"}, {"", "Y", "Y", 4, 0, "", "RECOMP_PADMAP"},
    {"", "Black", "BLACK", 6, 0, "", "RECOMP_PADMAP"}, {"", "White", "WHITE", 5, 0, "", "RECOMP_PADMAP"},
    {"", "Start", "START", 7, 0, "", "RECOMP_PADMAP"}, {"", "Back", "BACK", 8, 0, "", "RECOMP_PADMAP"},
    {"", "Left stick click", "LS", 9, 0, "", "RECOMP_PADMAP"}, {"", "Right stick click", "RS", 10, 0, "", "RECOMP_PADMAP"},
    {"", "D-pad up", "UP", 11, 0, "", "RECOMP_PADMAP"}, {"", "D-pad down", "DOWN", 12, 0, "", "RECOMP_PADMAP"},
    {"", "D-pad left", "LEFT", 13, 0, "", "RECOMP_PADMAP"}, {"", "D-pad right", "RIGHT", 14, 0, "", "RECOMP_PADMAP"},
    {"", "Left trigger", "LT", 15, 0, "", "RECOMP_PADMAP"}, {"", "Right trigger", "RT", 16, 0, "", "RECOMP_PADMAP"},
    {"Developer hotkeys", "Developer HUD", "HUD", VK_F1, 0, "", "MM3_DEV_HOTKEYS"},
    {"", "Debug menu", "MENU", VK_F2, 0, "", "MM3_DEV_HOTKEYS"},
    {"", "Extended cameras", "CAMERAS", VK_F3, 0, "", "MM3_DEV_HOTKEYS"},
};

const char *const kSettingsFile = "mm3_settings.ini";
bool g_missing;
int g_result = -1; /* -1 running, 0 quit, 1 play */
int g_tab;
int g_splash = 1;    /* 1 shown, 2 fading out, 0 gone */
int g_capture = -1;  /* g_binds index waiting for a key */
bool g_armed;        /* every key was up since the capture began */
Rml::String g_capture_label;
Rml::DataModelHandle g_model;
/* Keyboard and controller focus: a row of the current tab, shown from the
 * first arrow or pad press until the mouse moves. */
int g_row;
int g_fo = -1, g_fb = -1; /* the focused option or bind, -1 none */
bool g_nav;
bool g_pad;    /* the last input was a controller: show its button hints */
bool g_scroll; /* bring the focused row into view after the next update */
int g_type = -1; /* a text option whose field takes the keyboard next update */

const char kDocument[] = R"RML(
<rml><head><title>Midtown Madness 3</title><style>
body { width: 100%; height: 100%; font-family: Segoe UI; font-size: 14dp; color: #e6e7ea;
       background-color: #121418; display: flex; flex-direction: column; }
div, h1, h2 { display: block; }
#head { display: flex; align-items: flex-end; padding: 22dp 28dp 0dp 28dp; border-bottom: 1dp #ffffff12; }
#title { flex: 1; padding-bottom: 14dp; }
.tab { margin-left: 20dp; padding: 0dp 0dp 12dp 0dp; color: #80848f; cursor: pointer;
       border-bottom: 2dp transparent; }
.tab:hover { color: #e6e7ea; }
.tab.on { color: #ffffff; border-bottom-color: #e8a33d; }
h1 { font-size: 20dp; font-weight: bold; color: #ffffff; }
#sub { margin-top: 2dp; font-size: 12dp; color: #80848f; }
#list { flex: 1; min-height: 0; padding: 0dp 18dp 8dp 18dp; overflow-y: auto; }
h2 { margin: 18dp 10dp 4dp 10dp; font-size: 11dp; font-weight: bold; letter-spacing: 1dp; color: #e8a33d; text-transform: uppercase; }
.opt { display: flex; align-items: center; padding: 10dp 10dp 10dp 8dp; border-bottom: 1dp #ffffff0f;
       border-left: 2dp transparent; }
.opt.focus { background-color: #191c21; border-left-color: #e8a33d; }
.opt.focus .seg .btn.on { background-color: #e8a33d; color: #17120a; }
.opt.focus .key { background-color: #2a2e36; color: #ffffff; }
.fixed { font-size: 13dp; color: #80848f; }
input.field { flex: 0 0 200dp; width: 200dp; padding: 5dp 10dp; border-radius: 6dp; background-color: #1d2026; color: #e6e7ea;
              font-size: 13dp; cursor: text; }
input.field:focus { background-color: #2a2e36; }
.opt.focus input.field { background-color: #2a2e36; }
.text { flex: 1 1 0; min-width: 0; padding-right: 16dp; }
.hint { margin-top: 1dp; font-size: 12dp; color: #80848f; }
.seg { flex: 0 1 auto; padding: 2dp; border-radius: 6dp; background-color: #1d2026; text-align: right; }
.seg .btn { display: inline-block; padding: 4dp 10dp; border-radius: 4dp; font-size: 13dp; color: #9a9ea8;
            cursor: pointer; }
.seg .btn:hover { color: #ffffff; }
.seg .btn.on { background-color: #3a3f4a; color: #ffffff; }
.switch { position: relative; width: 38dp; height: 22dp; border-radius: 11dp; background-color: #2a2e36;
          cursor: pointer; transition: background-color 0.15s cubic-out; }
.switch .knob { position: absolute; left: 3dp; top: 3dp; width: 16dp; height: 16dp; border-radius: 8dp;
                background-color: #9a9ea8; transition: left background-color 0.15s cubic-out; }
.switch.on { background-color: #e8a33d; }
.switch.on .knob { left: 19dp; background-color: #ffffff; }
.opt.compact { padding-top: 5dp; padding-bottom: 5dp; }
.key { min-width: 120dp; padding: 4dp 12dp; border-radius: 6dp; text-align: center; font-size: 13dp;
       background-color: #1d2026; cursor: pointer; }
.key:hover { background-color: #2a2e36; }
.key.none { color: #5d616b; }
#capture { position: absolute; left: 0dp; top: 0dp; width: 100%; height: 100%; z-index: 10;
           display: flex; align-items: center; justify-content: center; background-color: #000000b8; }
#card { padding: 22dp 30dp; border-radius: 10dp; background-color: #1d2026; text-align: center; }
#card .big { font-size: 16dp; color: #ffffff; margin: 4dp 0dp; }
#splash { position: absolute; left: 0dp; top: 0dp; width: 100%; height: 100%; z-index: 20;
          display: flex; flex-direction: column; align-items: center; justify-content: center;
          background-color: #121418; opacity: 1; transition: opacity 0.35s cubic-in-out; }
#splash.fade { opacity: 0; }
#splash .name { font-size: 30dp; font-weight: bold; color: #ffffff; }
#splash .sub { margin-top: 4dp; font-size: 13dp; color: #80848f; }
#splash .bar { position: relative; margin-top: 28dp; width: 180dp; height: 3dp; border-radius: 2dp;
               background-color: #2a2d34; overflow: hidden; }
#splash .fill { position: absolute; left: 0dp; top: 0dp; width: 40%; height: 100%; border-radius: 2dp;
                background-color: #e8a33d; animation: 1.1s cubic-in-out infinite sweep; }
@keyframes sweep { from { left: -40%; } to { left: 100%; } }
#foot { display: flex; align-items: center; padding: 14dp 28dp 14dp 16dp; border-top: 1dp #ffffff12;
        background-color: #0e1013; }
.ghost { padding: 7dp 12dp; margin-right: 4dp; border-radius: 6dp; color: #9a9ea8; cursor: pointer; }
.ghost:hover { color: #ffffff; background-color: #1d2026; }
#hints { flex: 0 0 auto; white-space: nowrap; font-size: 12dp; color: #9a9ea8; padding-left: 4dp; }
.glyph { display: inline-block; min-width: 12dp; padding: 1dp 5dp; margin: 0dp 4dp 0dp 12dp; border-radius: 4dp;
         background-color: #2a2e36; color: #e6e7ea; font-size: 11dp; font-weight: bold; text-align: center; }
.glyph.pair { margin-left: 2dp; }
.glyph.start { vertical-align: 2dp; margin: 0dp 8dp 0dp 0dp; background-color: #00000026; color: #17120a; }
#status { flex: 1; font-size: 12dp; color: #e8a33d; text-align: right; padding-right: 12dp; }
#play { padding: 8dp 28dp; border-radius: 6dp; font-weight: bold; color: #17120a;
        background-color: #e8a33d; cursor: pointer; }
#play:hover { background-color: #f2b456; }
#play.off { background-color: #2a2d34; color: #5d616b; cursor: arrow; }
scrollbarvertical { width: 4dp; }
scrollbarvertical sliderbar { background-color: #2f333c; border-radius: 2dp; }
</style></head>
<body data-model="launcher">
  <div id="head">
    <div id="title">
      <h1>Midtown Madness 3</h1>
      <div id="sub">Saved when you press Play.</div>
    </div>
    <div class="tab" data-class-on="tab == 0" data-event-click="tab = 0">Settings</div>
    <div class="tab" data-class-on="tab == 1" data-event-click="tab = 1">Controls</div>
    <div class="tab" data-class-on="tab == 2" data-event-click="tab = 2">Mods</div>
  </div>
  <div id="list">
    <div data-for="o, i : options"><div data-if="o.page == tab && (dev || !o.dev)" data-attr-id="'o' + i">
      <h2 data-if="o.header != ''">{{ o.header }}</h2>
      <div class="opt" data-class-focus="fo == i">
        <div class="text"><div>{{ o.label }}</div><div class="hint" data-if="o.hint != ''">{{ o.hint }}</div></div>
        <div class="fixed" data-if="o.fixed">{{ o.choices[0].label }}</div>
        <input class="field" type="text" data-if="o.text" data-value="o.value"/>
        <div class="seg" data-if="!o.toggle && !o.fixed && !o.text"><div class="btn" data-for="c : o.choices" data-class-on="c.value == o.value"
             data-event-click="set(i, c.value)">{{ c.label }}</div></div>
        <div class="switch" data-if="o.toggle" data-class-on="o.value != o.choices[0].value" data-event-click="toggle(i)"><div class="knob"/></div>
      </div>
    </div></div>
    <div data-if="tab == 1">
      <div data-for="b, i : binds"><div data-if="dev || !b.dev" data-attr-id="'b' + i">
        <h2 data-if="b.header != ''">{{ b.header }}</h2>
        <div class="opt compact" data-class-focus="fb == i">
          <div class="text">{{ b.label }}</div>
          <div class="key" data-class-none="b.vk == 0" data-event-click="rebind(i)">{{ b.key }}</div>
        </div>
      </div></div>
    </div>
  </div>
  <div id="splash" data-if="splash" data-class-fade="splash == 2" data-event-click="splash = 0">
    <div class="name">Midtown Madness 3</div>
    <div class="sub">Recompiled for PC</div>
    <div class="bar"><div class="fill"/></div>
  </div>
  <div id="capture" data-if="capturing"><div id="card">
    <div class="hint">{{ capture_label }}</div>
    <div class="big">{{ capture_pad ? 'Press a controller button' : 'Press a key or mouse button' }}</div>
    <div class="hint">{{ pad ? 'Esc or B to cancel' : 'Esc to cancel' }}</div>
  </div></div>
  <div id="foot">
    <div class="ghost" data-if="!pad" data-event-click="reset()">Defaults</div>
    <div class="ghost" data-if="!pad" data-event-click="quit()">Quit</div>
    <div id="hints" data-if="pad"><span class="glyph">LB</span><span class="glyph pair">RB</span>Tabs<span
      class="glyph">A</span>Change<span class="glyph">Y</span>Defaults<span class="glyph">Back</span>Quit</div>
    <div id="status"><span data-if="missing">game_files\default.xbe not found. Run scripts\setup.ps1.</span></div>
    <div id="play" data-class-off="missing" data-event-click="play()"><span class="glyph start" data-if="pad">Start</span>Play</div>
  </div>
</body></rml>
)RML";

Rml::String key_name(int vk)
{
    switch (vk) {
    case 0: return "Unbound";
    case VK_LBUTTON: return "Left mouse";
    case VK_RBUTTON: return "Right mouse";
    case VK_MBUTTON: return "Middle mouse";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    }
    LONG param = (LONG)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16;
    if (vk >= VK_PRIOR && vk <= VK_DELETE) /* the extended navigation block, not the numpad */
        param |= 1 << 24;
    char name[64];
    if ((param >> 16) && GetKeyNameTextA(param, name, sizeof name)) {
        /* Windows names some keys in capitals (ENTER, BACKSPACE). */
        for (char *c = name + 1; *c; c++)
            if (c[-1] != ' ')
                *c = (char)tolower((unsigned char)*c);
        return name;
    }
    snprintf(name, sizeof name, "Key %02X", vk);
    return name;
}

/* The controller's buttons by RECOMP_PADMAP's names, and their XInput bits
 * (tools/xboxrecomp/src/input/xinput_device.c); the last two are triggers. */
const char *const kPadSources[16] = {"A", "B", "X", "Y", "LB", "RB", "START", "BACK", "LS", "RS",
                                     "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT"};
const char *const kPadSourceNames[16] = {"A", "B", "X", "Y", "LB", "RB", "Start", "Back", "Left stick",
                                         "Right stick", "D-pad up", "D-pad down", "D-pad left",
                                         "D-pad right", "Left trigger", "Right trigger"};
const WORD kPadBits[14] = {XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
                           XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_START,
                           XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
                           XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
                           XINPUT_GAMEPAD_DPAD_RIGHT};

bool pad_bind(const Bind &b) { return b.env == "RECOMP_PADMAP"; }

void set_key(Bind &b, int vk)
{
    b.vk = vk;
    if (pad_bind(b))
        b.key = vk >= 1 && vk <= 16 ? kPadSourceNames[vk - 1] : "Unbound";
    else
        b.key = key_name(vk);
}

/* NAME:VK,... with VK in hex, as the runtime reads RECOMP_KEYMAP. */
void parse_keymap(const char *env, const char *p)
{
    for (Bind &b : g_binds) {
        if (b.env != env)
            continue;
        const char *at = strstr(p, (b.name + ":").c_str());
        /* Names are matched whole: LX- must not match inside RLX-. */
        while (at && at != p && at[-1] != ',')
            at = strstr(at + 1, (b.name + ":").c_str());
        if (!at)
            continue;
        const char *v = at + b.name.size() + 1;
        if (!pad_bind(b)) {
            set_key(b, (int)strtoul(v, NULL, 16) & 0xFF);
            continue;
        }
        for (int s = 0; s < 16; s++) { /* NAME:SOURCE, the source by name */
            size_t n = strlen(kPadSources[s]);
            if (!strncmp(v, kPadSources[s], n) && (v[n] == ',' || !v[n]))
                set_key(b, s + 1);
        }
    }
}

Rml::String keymap(const char *env)
{
    Rml::String out;
    char item[32];
    for (const Bind &b : g_binds) {
        if (b.env != env)
            continue;
        if (pad_bind(b) && (b.vk < 1 || b.vk > 16))
            continue;
        if (pad_bind(b))
            snprintf(item, sizeof item, "%s%s:%s", out.empty() ? "" : ",", b.name.c_str(), kPadSources[b.vk - 1]);
        else
            snprintf(item, sizeof item, "%s%s:%X", out.empty() ? "" : ",", b.name.c_str(), b.vk);
        out += item;
    }
    return out;
}

void reset_all()
{
    for (Option &o : g_options)
        o.value = o.def;
    for (Bind &b : g_binds)
        set_key(b, b.def);
}

bool valid(const Option &o, const Rml::String &v)
{
    if (o.text) /* one ini line: printable, bounded */
        return v.size() < 200 && v.find_first_of("\r\n") == Rml::String::npos;
    for (const Choice &c : o.choices)
        if (c.value == v)
            return true;
    return false;
}

/* The developer options and hotkeys show while the developer tools mod is on. */
bool g_dev;

void options_changed()
{
    g_dev = false;
    for (const Option &o : g_options)
        if (o.env == mm3_mods[MM3_MOD_DEV_TOOLS].env)
            g_dev = o.value == "1";
    if (g_model) {
        g_model.DirtyVariable("options");
        g_model.DirtyVariable("dev");
    }
}

/* The Mods tab: one switch per entry of src/mods/mods.c. The translation
 * corrections are for developers: MM3_MOD_TRANSLATION=0 only. */
void add_mod_options()
{
    for (int i = 0; i < MM3_MOD_COUNT; i++) {
        if (i == MM3_MOD_TRANSLATION)
            continue;
        Option o;
        o.header = i ? "" : "Changes to the original game";
        o.label = mm3_mods[i].name;
        o.hint = mm3_mods[i].description;
        o.env = mm3_mods[i].env;
        o.def = mm3_mods[i].default_on || mm3_mods[i].required ? "1" : "0";
        if (mm3_mods[i].required)
            o.choices = {{"1", "Always on"}};
        else
            o.choices = {{"0", "Off"}, {"1", "On"}};
        o.page = 2;
        o.toggle = false;
        g_options.push_back(o);
    }
}

/* Profiles and explicit environment settings seed the UI. Saved choices are
 * loaded afterward, including empty Auto values that remove a seeded setting. */
void load_environment_settings()
{
    for (Option &o : g_options) {
        const char *value = getenv(o.env.c_str());
        if (!value || o.fixed)
            continue;
        if (valid(o, value)) {
            o.value = value;
        } else if (o.env == "RECOMP_RENDER_SCALE") {
            /* Profiles use 2.0/3.0 while the matching UI choices use 2/3. */
            char *end;
            double scale = strtod(value, &end);
            if (end == value || *end)
                continue;
            for (const Choice &c : o.choices)
                if (!c.value.empty() && strtod(c.value.c_str(), nullptr) == scale) {
                    o.value = c.value;
                    break;
                }
        }
    }
    for (const char *env : {"RECOMP_KEYMAP", "RECOMP_PADMAP", "MM3_DEV_HOTKEYS"})
        if (const char *value = getenv(env))
            parse_keymap(env, value);
}

void load_settings()
{
    add_mod_options();
    reset_all();
    for (Option &o : g_options)
    {
        o.toggle = o.choices.size() == 2 && o.choices[0].label == "Off";
        o.text = o.choices.size() == 1 && o.choices[0].label.empty(); /* {{"", ""}}: typed */
        o.fixed = o.choices.size() == 1 && !o.text;
        o.dev = o.env == "MM3_DEV_HUD" || o.env == "MM3_DEBUG_MENU" || o.env == "MM3_EXTENDED_CAMERAS" ||
                o.env == "MM3_E3_PRESET" || o.env == "MM3_DEV_HOTKEYS_ON";
    }
    for (Bind &b : g_binds)
        b.dev = b.env == "MM3_DEV_HOTKEYS";
    load_environment_settings();
    FILE *f = fopen(kSettingsFile, "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = 0;
        if (!strcmp(line, "RECOMP_KEYMAP") || !strcmp(line, "MM3_DEV_HOTKEYS") || !strcmp(line, "RECOMP_PADMAP"))
            parse_keymap(line, eq + 1);
        for (Option &o : g_options)
            if (o.env == line && valid(o, eq + 1))
                o.value = eq + 1;
    }
    fclose(f);
}

void save_and_export()
{
    FILE *f = fopen(kSettingsFile, "w");
    for (const Option &o : g_options) {
        if (o.env.empty())   /* a note, not a setting */
            continue;
        if (f)
            fprintf(f, "%s=%s\n", o.env.c_str(), o.value.c_str());
        _putenv_s(o.env.c_str(), o.value.c_str()); /* "" removes the variable */
    }
    const Rml::String keys = keymap("RECOMP_KEYMAP"), hotkeys = keymap("MM3_DEV_HOTKEYS"),
                      pad = keymap("RECOMP_PADMAP");
    if (f) {
        fprintf(f, "RECOMP_KEYMAP=%s\nMM3_DEV_HOTKEYS=%s\nRECOMP_PADMAP=%s\n", keys.c_str(), hotkeys.c_str(),
                pad.c_str());
        fclose(f);
    }
    _putenv_s("RECOMP_KEYMAP", keys.c_str());
    _putenv_s("RECOMP_PADMAP", pad.c_str());
    _putenv_s("MM3_DEV_HOTKEYS", getenv("MM3_DEV_HOTKEYS_ON") ? hotkeys.c_str() : "");
    /* What scripts\play.ps1 sets: the window, D3D11 executor, pads and audio. */
    _putenv_s("RECOMP_PB_EXEC", "1");
    _putenv_s("RECOMP_FB_WINDOW", "1");
    _putenv_s("RECOMP_USB", "1");
    if (!getenv("RECOMP_APU_DSP_ACK"))
        _putenv_s("RECOMP_APU_DSP_ACK", "0x80458810");
}

/* Started from build\<config>\: run from the folder above that holds game_files\. */
bool find_game_dir()
{
    char dir[MAX_PATH];
    if (GetFileAttributesA("game_files\\default.xbe") != INVALID_FILE_ATTRIBUTES)
        return true;
    if (!GetModuleFileNameA(NULL, dir, MAX_PATH))
        return false;
    for (char *slash; (slash = strrchr(dir, '\\')) != NULL;) {
        *slash = 0;
        char probe[MAX_PATH];
        snprintf(probe, sizeof probe, "%s\\game_files\\default.xbe", dir);
        if (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES)
            return SetCurrentDirectoryA(dir) != 0;
    }
    return false;
}

void play()
{
    if (!g_missing)
        g_result = 1;
}

void end_capture()
{
    g_capture = -1;
    g_model.DirtyVariable("capturing");
    g_model.DirtyVariable("capture_pad");
    g_model.DirtyVariable("binds");
}

/* Keys and buttons are read from the OS rather than RmlUi so they arrive as
 * the virtual keys the game window records. The press that opened the
 * prompt is let go first. */
void poll_capture()
{
    if (g_capture < 0 || !GetActiveWindow() || pad_bind(g_binds[g_capture]))
        return;   /* controller binds are captured in poll_pad */
    int down = 0;
    for (int vk = 1; vk < 0xFF && !down; vk++)
        if (vk != VK_ESCAPE && !(vk >= VK_LSHIFT && vk <= VK_RMENU) && (GetAsyncKeyState(vk) & 0x8000))
            down = vk;
    if (!g_armed) {
        g_armed = !down;
    } else if (down) {
        set_key(g_binds[g_capture], down);
        end_capture();
    }
}

/* The splash shows while the launcher comes up, then fades; a key or click skips it. */
void tick_splash(double now)
{
    static double start = -1;
    int was = g_splash;
    if (start < 0) start = now;
    if (g_splash == 1 && now - start > 1.2) { g_splash = 2; start = now - 1.2; }
    if (g_splash == 2 && now - start > 1.6) g_splash = 0;
    if (g_splash != was) g_model.DirtyVariable("splash");
}

void start_rebind(size_t i)
{
    if (i >= g_binds.size() || g_capture >= 0)
        return;
    /* Headers only name the first input of a group; find this one's. */
    size_t g = i;
    while (g > 0 && g_binds[g].header.empty())
        g--;
    g_capture_label = g_binds[g].header + ": " + g_binds[i].label;
    g_capture = (int)i;
    g_armed = false;
    g_model.DirtyVariable("capture_label");
    g_model.DirtyVariable("capturing");
    g_model.DirtyVariable("capture_pad");
}

struct Row {
    bool bind;
    int i;
};

Rml::Vector<Row> rows()
{
    Rml::Vector<Row> r;
    for (int i = 0; i < (int)g_options.size(); i++)
        if (g_options[i].page == g_tab && (g_dev || !g_options[i].dev))
            r.push_back({false, i});
    if (g_tab == 1)
        for (int i = 0; i < (int)g_binds.size(); i++)
            if (g_dev || !g_binds[i].dev)
                r.push_back({true, i});
    return r;
}

void show_focus()
{
    const Rml::Vector<Row> r = rows();
    g_row = Rml::Math::Clamp(g_row, 0, (int)r.size() - 1);
    g_fo = g_nav && !r[g_row].bind ? r[g_row].i : -1;
    g_fb = g_nav && r[g_row].bind ? r[g_row].i : -1;
    g_model.DirtyVariable("fo");
    g_model.DirtyVariable("fb");
    g_scroll = g_nav;
}

/* d -1/+1 through the choices; wrap goes round instead of stopping at the ends. */
void step(Option &o, int d, bool wrap)
{
    const int n = (int)o.choices.size();
    int k = 0;
    while (k < n && o.choices[k].value != o.value)
        k++;
    k = wrap ? (k + d + n) % n : Rml::Math::Clamp(k + d, 0, n - 1);
    o.value = o.choices[k].value;
    options_changed();
}

enum Nav { UP, DOWN, LEFT, RIGHT, ACCEPT, PREV_TAB, NEXT_TAB };

void navigate(Nav act)
{
    if (act == PREV_TAB || act == NEXT_TAB) {
        g_tab = (g_tab + (act == NEXT_TAB ? 1 : 2)) % 3;
        g_model.DirtyVariable("tab");
        return; /* the loop resets the row for the new tab */
    }
    if (!g_nav) { /* the first press shows where the focus is */
        g_nav = true;
        show_focus();
        return;
    }
    const Row f = rows()[g_row];
    if (act == UP || act == DOWN)
        g_row += act == UP ? -1 : 1;
    else if (f.bind && act == ACCEPT)
        start_rebind(f.i);
    else if (!f.bind && g_options[f.i].text)
        g_type = act == ACCEPT ? f.i : g_type;   /* the loop focuses its field */
    else if (!f.bind)
        step(g_options[f.i], act == LEFT ? -1 : 1, act == ACCEPT);
    show_focus();
}

void reset()
{
    reset_all();
    options_changed();
    g_model.DirtyVariable("binds");
}

void set_pad(bool pad)
{
    if (pad != g_pad) {
        g_pad = pad;
        g_model.DirtyVariable("pad");
    }
}

/* The first connected controller: D-pad or left stick moves (held, it
 * repeats), A changes, LB/RB switch tabs, Y defaults, Start plays, Back
 * quits, B cancels a key prompt. Read only while the launcher is in front. */
void poll_pad(double now)
{
    static int slot = -1;
    static double scan, repeat;
    static WORD prev;
    static int held = -1;
    XINPUT_STATE s = {};
    if (slot < 0 && now >= scan) { /* polling empty slots is slow: once a second */
        scan = now + 1;
        for (int p = 0; p < XUSER_MAX_COUNT && slot < 0; p++)
            if (XInputGetState(p, &s) == ERROR_SUCCESS)
                slot = p;
    }
    if (slot >= 0 && XInputGetState(slot, &s) != ERROR_SUCCESS)
        slot = -1;
    const bool front = GetActiveWindow() != NULL;
    WORD b = front ? s.Gamepad.wButtons : 0;
    const int x = s.Gamepad.sThumbLX, y = s.Gamepad.sThumbLY;
    if (front && abs(y) > 16000 && abs(y) >= abs(x))
        b |= y > 0 ? XINPUT_GAMEPAD_DPAD_UP : XINPUT_GAMEPAD_DPAD_DOWN;
    else if (front && abs(x) > 16000)
        b |= x > 0 ? XINPUT_GAMEPAD_DPAD_RIGHT : XINPUT_GAMEPAD_DPAD_LEFT;
    const WORD pressed = b & ~prev;
    prev = b;
    if (pressed)
        set_pad(true);
    if (g_splash) {
        if (pressed) {
            g_splash = 0;
            g_model.DirtyVariable("splash");
        }
        return;
    }
    if (g_capture >= 0) {
        held = -1;
        if (!pad_bind(g_binds[g_capture])) {   /* a key prompt: B cancels */
            if (pressed & XINPUT_GAMEPAD_B)
                end_capture();
            return;
        }
        /* A controller bind: the first button (or trigger) to go down after
         * every one was let go. Esc or a click cancels. */
        const WORD raw = front ? s.Gamepad.wButtons : 0;
        const bool lt = front && s.Gamepad.bLeftTrigger > 128, rt = front && s.Gamepad.bRightTrigger > 128;
        if (!g_armed) {
            g_armed = !raw && !lt && !rt;
            return;
        }
        int source = lt ? 15 : rt ? 16 : 0;
        for (int i = 0; i < 14 && !source; i++)
            if (raw & kPadBits[i])
                source = i + 1;
        if (source) {
            set_key(g_binds[g_capture], source);
            end_capture();
        }
        return;
    }
    if (pressed & XINPUT_GAMEPAD_START) play();
    if (pressed & XINPUT_GAMEPAD_BACK) g_result = 0;
    if (pressed & XINPUT_GAMEPAD_Y) reset();
    if (pressed & XINPUT_GAMEPAD_A) navigate(ACCEPT);
    if (pressed & XINPUT_GAMEPAD_LEFT_SHOULDER) navigate(PREV_TAB);
    if (pressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) navigate(NEXT_TAB);
    const int dir = b & XINPUT_GAMEPAD_DPAD_UP    ? UP
                  : b & XINPUT_GAMEPAD_DPAD_DOWN  ? DOWN
                  : b & XINPUT_GAMEPAD_DPAD_LEFT  ? LEFT
                  : b & XINPUT_GAMEPAD_DPAD_RIGHT ? RIGHT : -1;
    if (dir != held) {
        held = dir;
        repeat = now + 0.4;
        if (dir >= 0)
            navigate((Nav)dir);
    } else if (dir >= 0 && now >= repeat) {
        repeat = now + 0.08;
        navigate((Nav)dir);
    }
}

/* The mouse takes over from the focus when it moves. */
void poll_mouse()
{
    static POINT last = {-1, -1};
    POINT p;
    if (!GetCursorPos(&p) || (p.x == last.x && p.y == last.y))
        return;
    const bool first = last.x == -1;
    last = p;
    if (first || !GetActiveWindow() || (!g_nav && !g_pad))
        return;
    g_nav = false;
    set_pad(false);
    show_focus();
}

/* A dark title bar in the launcher's colour instead of the light default. */
void dark_title_bar()
{
    HWND hwnd = NULL;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND h, LPARAM p) -> BOOL {
        if (!IsWindowVisible(h))
            return TRUE;
        *(HWND *)p = h;
        return FALSE;
    }, (LPARAM)&hwnd);
    if (!hwnd)
        return;
    const BOOL dark = TRUE;
    const COLORREF caption = RGB(0x12, 0x14, 0x18);
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof dark);
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR, Windows 11 */, &caption, sizeof caption);
    /* The backend's class has the stock icon; ours is resource 1 (mm3.rc). */
    if (HICON icon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1))) {
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    }
}

bool list_ready(Rml::Context *ctx)
{
    Rml::Element *list = ctx->GetDocument(0)->GetElementById("list");
    return list && list->GetScrollTop() > 0;
}

bool on_key(Rml::Context *ctx, Rml::Input::KeyIdentifier key, int mods, float, bool priority)
{
    if (!priority)
        return true;
    /* A text field being typed in keeps its keys; Enter, Esc and up/down
     * leave it. */
    Rml::Element *typing = ctx->GetFocusElement();
    if (typing && typing->GetTagName() == "input" && !g_splash && g_capture < 0) {
        if (key != Rml::Input::KI_RETURN && key != Rml::Input::KI_ESCAPE &&
            key != Rml::Input::KI_UP && key != Rml::Input::KI_DOWN)
            return true;
        typing->Blur();
        if (key == Rml::Input::KI_UP || key == Rml::Input::KI_DOWN)
            navigate(key == Rml::Input::KI_UP ? UP : DOWN);
        return false;
    }
    if (g_splash) {   /* any key skips the splash */
        g_splash = 0;
        g_model.DirtyVariable("splash");
        return false;
    }
    if (g_capture >= 0) { /* the prompt owns the keyboard */
        if (key == Rml::Input::KI_ESCAPE)
            end_capture();
        return false;
    }
    set_pad(false);
    switch (key) {
    case Rml::Input::KI_RETURN: play(); break;
    case Rml::Input::KI_ESCAPE: g_result = 0; break;
    case Rml::Input::KI_UP: navigate(UP); break;
    case Rml::Input::KI_DOWN: navigate(DOWN); break;
    case Rml::Input::KI_LEFT: navigate(LEFT); break;
    case Rml::Input::KI_RIGHT: navigate(RIGHT); break;
    case Rml::Input::KI_SPACE: navigate(ACCEPT); break;
    case Rml::Input::KI_TAB: navigate(mods & Rml::Input::KM_SHIFT ? PREV_TAB : NEXT_TAB); break;
    default: return true;
    }
    return false;
}

void load_fonts()
{
    char fonts[MAX_PATH];
    GetWindowsDirectoryA(fonts, MAX_PATH);
    for (const char *name : {"segoeui.ttf", "segoeuib.ttf"}) {
        char path[MAX_PATH];
        snprintf(path, sizeof path, "%s\\Fonts\\%s", fonts, name);
        Rml::LoadFontFace(path);
    }
}

/* The backend's system interface, with RmlUi's log on stderr like the rest
 * of the runtime's diagnostics instead of OutputDebugString. */
struct System : Rml::SystemInterface {
    Rml::SystemInterface *base;
    unsigned errors = 0;
    double GetElapsedTime() override { return base->GetElapsedTime(); }
    void SetMouseCursor(const Rml::String &name) override { base->SetMouseCursor(name); }
    void SetClipboardText(const Rml::String &text) override { base->SetClipboardText(text); }
    void GetClipboardText(Rml::String &text) override { base->GetClipboardText(text); }
    bool LogMessage(Rml::Log::Type type, const Rml::String &message) override
    {
        if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT)
            ++errors;
        fprintf(stderr, "[launcher] %s\n", message.c_str());
        fflush(stderr);
        return true;
    }
} g_system;

bool bind_model(Rml::Context *ctx)
{
    Rml::DataModelConstructor m = ctx->CreateDataModel("launcher");
    if (!m)
        return false;
    if (auto c = m.RegisterStruct<Choice>()) {
        c.RegisterMember("value", &Choice::value);
        c.RegisterMember("label", &Choice::label);
    }
    m.RegisterArray<Rml::Vector<Choice>>();
    if (auto o = m.RegisterStruct<Option>()) {
        o.RegisterMember("header", &Option::header);
        o.RegisterMember("label", &Option::label);
        o.RegisterMember("hint", &Option::hint);
        o.RegisterMember("value", &Option::value);
        o.RegisterMember("choices", &Option::choices);
        o.RegisterMember("page", &Option::page);
        o.RegisterMember("toggle", &Option::toggle);
        o.RegisterMember("fixed", &Option::fixed);
        o.RegisterMember("dev", &Option::dev);
        o.RegisterMember("text", &Option::text);
    }
    m.RegisterArray<Rml::Vector<Option>>();
    if (auto b = m.RegisterStruct<Bind>()) {
        b.RegisterMember("header", &Bind::header);
        b.RegisterMember("label", &Bind::label);
        b.RegisterMember("vk", &Bind::vk);
        b.RegisterMember("key", &Bind::key);
        b.RegisterMember("dev", &Bind::dev);
    }
    m.RegisterArray<Rml::Vector<Bind>>();
    m.Bind("options", &g_options);
    m.Bind("binds", &g_binds);
    m.Bind("missing", &g_missing);
    m.Bind("tab", &g_tab);
    m.Bind("splash", &g_splash);
    m.Bind("capture_label", &g_capture_label);
    m.Bind("fo", &g_fo);
    m.Bind("fb", &g_fb);
    m.Bind("pad", &g_pad);
    m.Bind("dev", &g_dev);
    m.BindFunc("capturing", [](Rml::Variant &v) { v = g_capture >= 0; });
    m.BindFunc("capture_pad", [](Rml::Variant &v) { v = g_capture >= 0 && pad_bind(g_binds[g_capture]); });
    m.BindEventCallback("toggle", [](Rml::DataModelHandle h, Rml::Event &, const Rml::VariantList &args) {
        size_t i = args.empty() ? g_options.size() : args[0].Get<size_t>();
        if (i < g_options.size() && g_options[i].toggle) {
            Option &o = g_options[i];
            o.value = o.value == o.choices[0].value ? o.choices[1].value : o.choices[0].value;
            options_changed();
        }
    });
    m.BindEventCallback("rebind", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args) {
        start_rebind(args.empty() ? g_binds.size() : args[0].Get<size_t>());
    });
    m.BindEventCallback("set", [](Rml::DataModelHandle h, Rml::Event &, const Rml::VariantList &args) {
        size_t i = args.size() == 2 ? args[0].Get<size_t>() : g_options.size();
        if (i < g_options.size() && valid(g_options[i], args[1].Get<Rml::String>())) {
            g_options[i].value = args[1].Get<Rml::String>();
            options_changed();
        }
    });
    m.BindEventCallback("reset", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { reset(); });
    m.BindEventCallback("play", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { play(); });
    m.BindEventCallback("quit", [](Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &) { g_result = 0; });
    g_model = m.GetModelHandle();
    return true;
}

} // namespace

/* 1 = play with the chosen settings exported, 0 = the user quit. */
extern "C" int mm3_launcher(void)
{
    /* Double-clicked: the console only carries diagnostics, drop it. */
    DWORD pids[2];
    if (GetConsoleProcessList(pids, 2) == 1)
        FreeConsole();

    g_missing = !find_game_dir();
    load_settings();
    options_changed();
    /* MM3_LAUNCHER_TAB=0/1/2 opens on Settings, Controls or Mods (screenshots, tests). */
    if (const char *tab = getenv("MM3_LAUNCHER_TAB"))
        g_tab = atoi(tab) >= 0 && atoi(tab) <= 2 ? atoi(tab) : 0;

    /* Not DPI-aware yet, so the work area is in the backend's 96-dpi units. */
    RECT work = {0, 0, 760, 720};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    const int width = Rml::Math::Min(760, (int)(work.right - work.left) * 9 / 10);
    const int height = Rml::Math::Min(720, (int)(work.bottom - work.top) * 9 / 10);
    if (!Backend::Initialize("Midtown Madness 3", width, height, false)) {
        save_and_export(); /* no launcher: play with the saved settings */
        return 1;
    }
    dark_title_bar();
    g_system.base = Backend::GetSystemInterface();
    Rml::SetSystemInterface(&g_system);
    Rml::SetRenderInterface(Backend::GetRenderInterface());
    Rml::Initialise();
    load_fonts();

    Rml::Context *ctx = Rml::CreateContext("launcher", Rml::Vector2i(width, height));
    if (ctx && bind_model(ctx)) {
        if (Rml::ElementDocument *doc = ctx->LoadDocumentFromMemory(kDocument))
            doc->Show();
        g_result = -1;
        int shown_tab = g_tab;
        bool rendered = false;
        /* Waits for messages only in the background: in front, the controller
         * is polled every frame (the backend presents with v-sync). */
        while (g_result < 0 && Backend::ProcessEvents(ctx, on_key, !GetActiveWindow())) {
            const double now = Rml::GetSystemInterface()->GetElapsedTime();
            tick_splash(now);
            poll_capture();
            poll_pad(now);
            poll_mouse();
            ctx->Update();
            Rml::ElementDocument *doc = ctx->GetDocument(0);
            if (g_tab != shown_tab && doc) { /* a new tab starts at its top */
                shown_tab = g_tab;
                g_row = 0;
                show_focus();
                if (Rml::Element *list = doc->GetElementById("list"))
                    list->SetScrollTop(0);
                ctx->Update();
            }
            if (g_type >= 0 && doc) { /* A or Space on a text row: type in it */
                Rml::ElementList fields;
                if (Rml::Element *row = doc->GetElementById("o" + Rml::ToString(g_type)))
                    row->GetElementsByTagName(fields, "input");
                if (!fields.empty())
                    fields[0]->Focus();
                g_type = -1;
            }
            if (g_scroll && doc) {
                g_scroll = false;
                const Rml::String id = (g_fb >= 0 ? "b" : "o") + Rml::ToString(g_fb >= 0 ? g_fb : g_fo);
                if (Rml::Element *row = doc->GetElementById(id))
                    row->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest,
                        Rml::ScrollAlignment::Nearest, Rml::ScrollBehavior::Smooth, Rml::ScrollParentage::Closest));
            }
            /* MM3_LAUNCHER_SCROLL=1 shows the end of the list (screenshots, tests). */
            static bool scrolled;
            if (!scrolled && getenv("MM3_LAUNCHER_SCROLL") && ctx->GetDocument(0)) {
                if (Rml::Element *list = ctx->GetDocument(0)->GetElementById("list"))
                    list->SetScrollTop(list->GetScrollHeight());
                scrolled = list_ready(ctx);
            }
            /* The first WM_SIZE can precede creation of the backend renderer.
             * A hidden startup need not send another one: seed its viewport
             * from the context's actual pixel size before drawing. */
            const Rml::Vector2i dimensions = ctx->GetDimensions();
            static_cast<RenderInterface_DX11 *>(Backend::GetRenderInterface())->SetViewport(
                dimensions.x, dimensions.y);
            Backend::BeginFrame();
            ctx->Render();
            Backend::PresentFrame();
            if (!rendered && !g_system.errors) {
                fprintf(stderr, "[launcher] first frame rendered: %dx%d\n", dimensions.x, dimensions.y);
                fflush(stderr);
                rendered = true;
            }
        }
    }
    Rml::Shutdown();
    Backend::Shutdown();

    if (g_result != 1)
        return 0;
    save_and_export();
    return 1;
}
