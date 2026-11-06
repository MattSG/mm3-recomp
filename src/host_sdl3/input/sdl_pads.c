/**
 * SDL3 gamepads as the runtime's host pad provider (xbox_InputSetHostPadProvider).
 *
 * Port n is the n-th gamepad SDL lists; hot plugging, rumble, RECOMP_PADMAP.
 * Taken from the MattSG fork's src/input/xinput_device.c. The window that
 * pumps SDL events (host_sdl3/video/fb_window_sdl.c) keeps the state current.
 * Call mm3_sdl3_install_pads() before xbox_InputInit().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xinput_xbox.h"
#include <xinput.h>   /* only the XINPUT_STATE types: nothing links against XInput */
#include <SDL3/SDL.h>

/* SDL3 gamepads as XInput ports: port n is the n-th gamepad SDL lists. The
 * framebuffer window's thread pumps SDL events, which keeps the state
 * current; SDL locks its joystick state, so reading it here is safe. */
static SDL_Gamepad *g_sdl_pads[XBOX_MAX_CONTROLLERS];
static XINPUT_GAMEPAD g_sdl_last[XBOX_MAX_CONTROLLERS];
static DWORD g_sdl_packet[XBOX_MAX_CONTROLLERS];
static XBOX_VIBRATION g_sdl_rumble[XBOX_MAX_CONTROLLERS];
static Uint64 g_sdl_rumble_refresh[XBOX_MAX_CONTROLLERS];

/* The pads are read from the title's threads and from launchers and overlays
 * (xbox_InputHostPad), so the open handles and the packet counters sit behind
 * one lock. SDL mutexes are recursive. */
static SDL_InitState s_pad_lock_state;
static SDL_Mutex *s_pad_lock;

static void pad_lock(void)
{
    if (SDL_ShouldInit(&s_pad_lock_state)) {
        s_pad_lock = SDL_CreateMutex();
        SDL_SetInitialized(&s_pad_lock_state, true);
    }
    SDL_LockMutex(s_pad_lock);
}

static void pad_unlock(void)
{
    SDL_UnlockMutex(s_pad_lock);
}

static SDL_Gamepad *sdl_pad(DWORD port)
{
    static Uint64 next_scan[XBOX_MAX_CONTROLLERS];
#if defined(_WIN32) && !defined(XBOXRECOMP_SDL3)
    /* The Direct3D 11 window is a Win32 one, so nothing pumps SDL events:
     * refresh the pad state here, at most once a millisecond. */
    {
        static Uint64 last_update;
        Uint64 now = SDL_GetTicksNS();
        if (now - last_update >= 1000000u) {
            last_update = now;
            SDL_UpdateGamepads();
        }
    }
#endif
    SDL_Gamepad *pad = g_sdl_pads[port];
    if (pad && SDL_GamepadConnected(pad)) {
        /* Xbox output reports latch; SDL effects expire. Renew held speeds
         * while the title polls, including when it sends no new output. */
        if ((g_sdl_rumble[port].wLeftMotorSpeed || g_sdl_rumble[port].wRightMotorSpeed)
            && SDL_GetTicks() >= g_sdl_rumble_refresh[port]) {
            if (SDL_RumbleGamepad(pad, g_sdl_rumble[port].wLeftMotorSpeed,
                                 g_sdl_rumble[port].wRightMotorSpeed, 60000))
                g_sdl_rumble_refresh[port] = SDL_GetTicks() + 30000;
        }
        return pad;
    }
    if (pad) {
        SDL_CloseGamepad(pad);
        g_sdl_pads[port] = NULL;
        memset(&g_sdl_rumble[port], 0, sizeof g_sdl_rumble[port]);
    }
    if (SDL_GetTicks() < next_scan[port])
        return NULL;
    next_scan[port] = SDL_GetTicks() + 1000; /* hot-plug check, not every poll */
    /* Direct-to-game startup may not have visited a launcher or InputInit. */
    if (!SDL_WasInit(SDL_INIT_GAMEPAD) && !SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "[INPUT] SDL gamepad init failed: %s\n", SDL_GetError());
        return NULL;
    }
    {
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        if (ids && (int)port < count)
            g_sdl_pads[port] = SDL_OpenGamepad(ids[port]);
        SDL_free(ids);
    }
    return g_sdl_pads[port];
}

static DWORD host_pad_state_locked(DWORD port, XINPUT_STATE *state)
{
    static const struct { SDL_GamepadButton b; WORD bit; } k_buttons[] = {
        { SDL_GAMEPAD_BUTTON_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP },
        { SDL_GAMEPAD_BUTTON_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT },
        { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
        { SDL_GAMEPAD_BUTTON_START, XINPUT_GAMEPAD_START },
        { SDL_GAMEPAD_BUTTON_BACK, XINPUT_GAMEPAD_BACK },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, XINPUT_GAMEPAD_LEFT_THUMB },
        { SDL_GAMEPAD_BUTTON_RIGHT_STICK, XINPUT_GAMEPAD_RIGHT_THUMB },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER },
        { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },
        { SDL_GAMEPAD_BUTTON_SOUTH, XINPUT_GAMEPAD_A },
        { SDL_GAMEPAD_BUTTON_EAST, XINPUT_GAMEPAD_B },
        { SDL_GAMEPAD_BUTTON_WEST, XINPUT_GAMEPAD_X },
        { SDL_GAMEPAD_BUTTON_NORTH, XINPUT_GAMEPAD_Y } };
    SDL_Gamepad *pad = sdl_pad(port);
    XINPUT_GAMEPAD *g = &state->Gamepad;
    size_t i;

    if (!pad)
        return ERROR_DEVICE_NOT_CONNECTED;
    memset(state, 0, sizeof *state);
    for (i = 0; i < sizeof k_buttons / sizeof k_buttons[0]; i++)
        if (SDL_GetGamepadButton(pad, k_buttons[i].b))
            g->wButtons |= k_buttons[i].bit;
    /* Triggers are 0..32767; XInput's are 0..255. */
    g->bLeftTrigger = (BYTE)(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >> 7);
    g->bRightTrigger = (BYTE)(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >> 7);
    /* SDL's Y axes point down, XInput's up; -1 - v keeps -32768 in range. */
    g->sThumbLX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    g->sThumbLY = (SHORT)(-1 - SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
    g->sThumbRX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
    g->sThumbRY = (SHORT)(-1 - SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
    /* XInput's packet number moves only when the state does. */
    if (memcmp(g, &g_sdl_last[port], sizeof *g)) {
        g_sdl_last[port] = *g;
        ++g_sdl_packet[port];
    }
    state->dwPacketNumber = g_sdl_packet[port];
    return ERROR_SUCCESS;
}

static DWORD host_pad_state(DWORD port, XINPUT_STATE *state)
{
    DWORD result;
    pad_lock();
    result = host_pad_state_locked(port, state);
    pad_unlock();
    return result;
}

static int host_pad_present(DWORD port)
{
    int present;
    pad_lock();
    present = sdl_pad(port) != NULL;
    pad_unlock();
    return present;
}

static void host_pad_rumble(DWORD port, WORD left, WORD right)
{
    SDL_Gamepad *pad;
    pad_lock();
    pad = sdl_pad(port);
    if (pad && SDL_RumbleGamepad(pad, left, right, 60000)) {
        g_sdl_rumble[port].wLeftMotorSpeed = left;
        g_sdl_rumble[port].wRightMotorSpeed = right;
        g_sdl_rumble_refresh[port] = SDL_GetTicks() + 30000;
    }
    pad_unlock();
}

/* RECOMP_PADMAP: which controller button drives each Xbox input, as
 * NAME:SOURCE,... (BLACK:RB,WHITE:LB). Xbox inputs and sources share these
 * names, except that the pad's shoulders are LB/RB and the Xbox's are
 * BLACK/WHITE; unnamed inputs keep their own button. A trigger used as a
 * button counts past a third of its travel. */
enum { PAD_SOURCES = 16 };
static const char *const k_pad_sources[PAD_SOURCES] = {
    "A", "B", "X", "Y", "LB", "RB", "START", "BACK", "LS", "RS",
    "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT"};
static const char *const k_pad_inputs[PAD_SOURCES] = {
    "A", "B", "X", "Y", "BLACK", "WHITE", "START", "BACK", "LS", "RS",
    "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT"};
static const WORD k_source_bits[14] = {
    XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_START,
    XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
    XINPUT_GAMEPAD_DPAD_RIGHT};

static const int *pad_map(void)
{
    static int map[PAD_SOURCES] = {-1};
    if (map[0] < 0) {
        const char *env = getenv("RECOMP_PADMAP");
        for (int i = 0; i < PAD_SOURCES; i++)
            map[i] = i;
        /* Match the Xbox Black/White positions and the SDL backend. */
        map[4] = 5; /* Black <- right shoulder */
        map[5] = 4; /* White <- left shoulder */
        while (env && *env) {
            char name[16], src[16];
            if (sscanf(env, "%15[^:]:%15[^,]", name, src) == 2)
                for (int i = 0; i < PAD_SOURCES; i++)
                    if (!_stricmp(name, k_pad_inputs[i]))
                        for (int j = 0; j < PAD_SOURCES; j++)
                            if (!_stricmp(src, k_pad_sources[j]))
                                map[i] = j;
            env = strchr(env, ',');
            env = env ? env + 1 : NULL;
        }
    }
    return map;
}

static BYTE pad_source(const XINPUT_GAMEPAD *g, int source)
{
    if (source == 14) return g->bLeftTrigger;
    if (source == 15) return g->bRightTrigger;
    return (g->wButtons & k_source_bits[source]) ? 255 : 0;
}

/* The Xbox buttons from an XInput pad, through RECOMP_PADMAP. */
static void pad_buttons(const XINPUT_GAMEPAD *g, XBOX_GAMEPAD *out)
{
    static const int analog[8] = {XBOX_BUTTON_A, XBOX_BUTTON_B, XBOX_BUTTON_X, XBOX_BUTTON_Y,
                                  XBOX_BUTTON_BLACK, XBOX_BUTTON_WHITE, -1, -1};
    static const WORD digital[8] = {XBOX_GAMEPAD_START, XBOX_GAMEPAD_BACK, XBOX_GAMEPAD_LEFT_THUMB,
                                    XBOX_GAMEPAD_RIGHT_THUMB, XBOX_GAMEPAD_DPAD_UP, XBOX_GAMEPAD_DPAD_DOWN,
                                    XBOX_GAMEPAD_DPAD_LEFT, XBOX_GAMEPAD_DPAD_RIGHT};
    const int *map = pad_map();
    out->wButtons = 0;
    for (int i = 0; i < 6; i++)
        out->bAnalogButtons[analog[i]] = pad_source(g, map[i]);
    for (int i = 0; i < 8; i++)
        if (pad_source(g, map[6 + i]) > 85)
            out->wButtons |= digital[i];
    out->bAnalogButtons[XBOX_BUTTON_LTRIGGER] = pad_source(g, map[14]);
    out->bAnalogButtons[XBOX_BUTTON_RTRIGGER] = pad_source(g, map[15]);
}


uint32_t xbox_InputHostPad(uint32_t port, uint16_t *buttons,
                           uint8_t *left_trigger, uint8_t *right_trigger,
                           int16_t *lx, int16_t *ly, int16_t *rx, int16_t *ry)
{
    XINPUT_STATE state;
#ifndef XBOX_INPUT_HOST_PAD
    if (!SDL_WasInit(SDL_INIT_GAMEPAD))
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
#endif
    if (port >= XBOX_MAX_CONTROLLERS || host_pad_state(port, &state) != ERROR_SUCCESS)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (buttons) *buttons = state.Gamepad.wButtons;
    if (left_trigger) *left_trigger = state.Gamepad.bLeftTrigger;
    if (right_trigger) *right_trigger = state.Gamepad.bRightTrigger;
    if (lx) *lx = state.Gamepad.sThumbLX;
    if (ly) *ly = state.Gamepad.sThumbLY;
    if (rx) *rx = state.Gamepad.sThumbRX;
    if (ry) *ry = state.Gamepad.sThumbRY;
    return ERROR_SUCCESS;
}

static DWORD provider_get_state(DWORD port, XBOX_INPUT_STATE *s)
{
    XINPUT_STATE xi;
    DWORD r = host_pad_state(port, &xi);
    if (r != ERROR_SUCCESS)
        return r;
    s->dwPacketNumber = xi.dwPacketNumber;
    pad_buttons(&xi.Gamepad, &s->Gamepad);
    s->Gamepad.sThumbLX = xi.Gamepad.sThumbLX;
    s->Gamepad.sThumbLY = xi.Gamepad.sThumbLY;
    s->Gamepad.sThumbRX = xi.Gamepad.sThumbRX;
    s->Gamepad.sThumbRY = xi.Gamepad.sThumbRY;
    return ERROR_SUCCESS;
}

static DWORD provider_set_state(DWORD port, const XBOX_VIBRATION *v)
{
    host_pad_rumble(port, v->wLeftMotorSpeed, v->wRightMotorSpeed);
    return ERROR_SUCCESS;
}

static const XBOX_HOST_PAD_PROVIDER s_provider = { NULL, provider_get_state, provider_set_state };

void mm3_sdl3_install_pads(void)
{
    xbox_InputSetHostPadProvider(&s_provider);
}
