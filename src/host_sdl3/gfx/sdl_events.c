/**
 * Events for the D3D8 compatibility window, using the shared keyboard table.
 * The native NV2A framebuffer keeps its existing window event handling.
 */
#include <SDL3/SDL.h>
#include "../input/keyboard.h"
#include <stdlib.h>
#include <string.h>

static SDL_Mutex     *g_mutex;
static SDL_InitState  g_init;
static int vk_from_sdl(SDL_Keycode k, SDL_Scancode sc)
{
    if (k >= SDLK_A && k <= SDLK_Z) return 'A' + (int)(k - SDLK_A);
    if (k >= SDLK_0 && k <= SDLK_9) return '0' + (int)(k - SDLK_0);
    if (k >= SDLK_F1 && k <= SDLK_F12) return 0x70 + (int)(k - SDLK_F1);
    switch (sc) {
    case SDL_SCANCODE_KP_0: return 0x60;
    case SDL_SCANCODE_KP_1: return 0x61;
    case SDL_SCANCODE_KP_2: return 0x62;
    case SDL_SCANCODE_KP_3: return 0x63;
    case SDL_SCANCODE_KP_4: return 0x64;
    case SDL_SCANCODE_KP_5: return 0x65;
    case SDL_SCANCODE_KP_6: return 0x66;
    case SDL_SCANCODE_KP_7: return 0x67;
    case SDL_SCANCODE_KP_8: return 0x68;
    case SDL_SCANCODE_KP_9: return 0x69;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x6A;
    case SDL_SCANCODE_KP_PLUS: return 0x6B;
    case SDL_SCANCODE_KP_MINUS: return 0x6D;
    case SDL_SCANCODE_KP_PERIOD: return 0x6E;
    case SDL_SCANCODE_KP_DIVIDE: return 0x6F;
    case SDL_SCANCODE_KP_ENTER: return 0x0D;
    default: break;
    }
    switch (k) {
    case SDLK_BACKSPACE: return 0x08;
    case SDLK_TAB: return 0x09;
    case SDLK_RETURN: return 0x0D;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 0x10;
    case SDLK_LCTRL: case SDLK_RCTRL: return 0x11;
    case SDLK_LALT: case SDLK_RALT: return 0x12;
    case SDLK_PAUSE: return 0x13;
    case SDLK_CAPSLOCK: return 0x14;
    case SDLK_ESCAPE: return 0x1B;
    case SDLK_SPACE: return 0x20;
    case SDLK_PAGEUP: return 0x21;
    case SDLK_PAGEDOWN: return 0x22;
    case SDLK_END: return 0x23;
    case SDLK_HOME: return 0x24;
    case SDLK_LEFT: return 0x25;
    case SDLK_UP: return 0x26;
    case SDLK_RIGHT: return 0x27;
    case SDLK_DOWN: return 0x28;
    case SDLK_INSERT: return 0x2D;
    case SDLK_DELETE: return 0x2E;
    case SDLK_SEMICOLON: return 0xBA;
    case SDLK_EQUALS: return 0xBB;
    case SDLK_COMMA: return 0xBC;
    case SDLK_MINUS: return 0xBD;
    case SDLK_PERIOD: return 0xBE;
    case SDLK_SLASH: return 0xBF;
    case SDLK_GRAVE: return 0xC0;
    case SDLK_LEFTBRACKET: return 0xDB;
    case SDLK_BACKSLASH: return 0xDC;
    case SDLK_RIGHTBRACKET: return 0xDD;
    case SDLK_APOSTROPHE: return 0xDE;
    default: return 0;
    }
}



static void ensure_mutex(void)
{
    if (SDL_ShouldInit(&g_init)) {
        g_mutex = SDL_CreateMutex();
        SDL_SetInitialized(&g_init, true);
    }
}

void xbox_PumpEvents(void)
{
    SDL_Event e;

    ensure_mutex();
    SDL_LockMutex(g_mutex);
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            SDL_UnlockMutex(g_mutex);
            exit(0);
        }
        if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) &&
            true) {
            int vk = vk_from_sdl(e.key.key, e.key.scancode);
            if (vk) xbox_InputKeySet(vk, e.type == SDL_EVENT_KEY_DOWN);
            /* RECOMP_KEY_TRACE: each key as it arrives, edge-triggered. A
             * sampled "which keys are held" cannot tell a key that was never
             * pressed from one that was tapped; this answers "did it arrive". */
            if (e.type == SDL_EVENT_KEY_DOWN && getenv("RECOMP_KEY_TRACE")) {
                static unsigned n;
                if (n++ < 40)
                    fprintf(stderr, "  [KEY] down scancode=%d\n", (int)e.key.scancode);
            }
        }
        /* Switching away with a key held would leave it held for ever. */
        if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            xbox_InputKeysClear();
    }
    SDL_UnlockMutex(g_mutex);
}
