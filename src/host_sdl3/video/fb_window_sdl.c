/**
 * The framebuffer window through SDL3, for the SDL3 GPU renderer: creation,
 * events, keys, fullscreen. The display state it feeds is fb_present.c.
 *
 * SDL wants a window's swapchain acquired on the thread that created it, so
 * the renderer opens the window (fbw_sdl_open) and pumps its events
 * (fbw_sdl_pump) on its own thread -- the pushbuffer thread, which also runs
 * the loading-screen ticks, so the window keeps answering while the title
 * loads. xbox_FramebufferWindowStart only records that a window is wanted.
 *
 * Off unless RECOMP_FB_WINDOW is set.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fb_window.h"
#include "fb_window_sdl.h"

static SDL_Window *s_window;
static int s_wanted = -1;
static int s_running;
static XboxCursor s_cursor_shown;
static int s_cursor_hidden;

SDL_Window *xbox_FramebufferSdlWindow(void)
{
    return s_window;
}

void *fbw_native_window(void)
{
    return s_window;
}

void xbox_FramebufferWindowStart(void)
{
    s_wanted = getenv("RECOMP_FB_WINDOW") != NULL;
}

int fbw_sdl_wanted(void)
{
    if (s_wanted < 0) s_wanted = getenv("RECOMP_FB_WINDOW") != NULL;
    return s_wanted;
}

/* Windows virtual-key codes, which the pad stand-in and RECOMP_KEYMAP use on
 * every host, from SDL keys. Letters, digits and punctuation follow the
 * keyboard layout as Windows' virtual keys do. */
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

static void set_mouse(float x, float y)
{
    int w = 0, h = 0;
    if (SDL_GetWindowSize(s_window, &w, &h) && w > 0 && h > 0) {
        fbw_mouse_x = x / (float)w;
        fbw_mouse_y = y / (float)h;
    }
}

static void title_tick(void)
{
    uint16_t title[128];
    if (fbw_title_due(title, 128)) {
        size_t n = 0;
        char *utf8;
        while (n < 127 && title[n]) n++;
        utf8 = SDL_iconv_string("UTF-8", "UTF-16LE", (const char *)title, (n + 1) * 2);
        if (utf8) {
            SDL_SetWindowTitle(s_window, utf8);
            SDL_free(utf8);
        }
    }
}

static void cursor_tick(void)
{
    XboxCursor c;
    if (!fbw_cursor_handler) return;
    c = fbw_cursor_handler();
    if (!c) {
        if (!s_cursor_hidden) { SDL_HideCursor(); s_cursor_hidden = 1; }
        return;
    }
    if (s_cursor_hidden) { SDL_ShowCursor(); s_cursor_hidden = 0; }
    if (c != s_cursor_shown) { SDL_SetCursor(c); s_cursor_shown = c; }
}

/* Set a default Linux driver preference before SDL video initialization.
 * Caller environment or higher-priority SDL hints can override it. */
void xbox_FramebufferSdlHints(void)
{
#if defined(__linux__)
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "wayland,x11", SDL_HINT_DEFAULT);
#endif
}

/* A Win32 window's first ShowWindow takes the parent's STARTUPINFO show state,
 * so a script can start the game hidden; SDL shows its windows itself. */
int xbox_FramebufferStartHidden(void)
{
#if defined(_WIN32)
    STARTUPINFOW si;
    GetStartupInfoW(&si);
    return (si.dwFlags & STARTF_USESHOWWINDOW) && si.wShowWindow == SW_HIDE;
#else
    return 0;
#endif
}

SDL_Window *fbw_sdl_open(SDL_WindowFlags extra_flags)
{
    SDL_Rect usable;
    int cw, ch;
    if (s_window) return s_window;
    if (!fbw_sdl_wanted()) return NULL;
    xbox_FramebufferSdlHints();
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "  [FBWIN] SDL video: %s\n", SDL_GetError());
        return NULL;
    }
    memset(&usable, 0, sizeof usable);
    SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable);
    fbw_initial_size(usable.w, usable.h, &cw, &ch);
    s_window = SDL_CreateWindow("Xbox Recomp - Framebuffer", cw, ch,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                (xbox_FramebufferStartHidden() ? SDL_WINDOW_HIDDEN : 0) |
                                extra_flags);
    if (!s_window) {
        fprintf(stderr, "  [FBWIN] SDL_CreateWindow: %s\n", SDL_GetError());
        return NULL;
    }
    if (getenv("RECOMP_FULLSCREEN"))
        SDL_SetWindowFullscreen(s_window, true);
    s_running = 1;
    fprintf(stderr, "  [FBWIN] framebuffer window open (%dx%d client, SDL %s)\n", cw, ch,
            SDL_GetCurrentVideoDriver());
    return s_window;
}

void fbw_sdl_pump(void)
{
    SDL_Event e;
    if (!s_window) return;
    while (SDL_PollEvent(&e)) {
        int consumed = fbw_host_overlay.event && fbw_host_overlay.event(&e);
        if (consumed && e.type != SDL_EVENT_WINDOW_FOCUS_LOST &&
            e.type != SDL_EVENT_WINDOW_FOCUS_GAINED &&
            e.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.type != SDL_EVENT_QUIT) {
            xbox_InputKeysClear();
            continue;
        }
        switch (e.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (s_running && fbw_close_handler)
                fbw_close_handler();
            s_running = 0;
            break;
        case SDL_EVENT_KEY_DOWN: {
            int vk;
            if (((e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT)) || e.key.key == SDLK_F11)) {
                if (!e.key.repeat)
                    SDL_SetWindowFullscreen(s_window,
                        !(SDL_GetWindowFlags(s_window) & SDL_WINDOW_FULLSCREEN));
                break;
            }
            vk = vk_from_sdl(e.key.key, e.key.scancode);
            if (vk) xbox_InputKeySet(vk, 1);
            /* RECOMP_KEY_TRACE: each key as it arrives, edge-triggered. */
            if (vk && getenv("RECOMP_KEY_TRACE")) {
                static unsigned n;
                if (n++ < 40) {
                    fprintf(stderr, "  [KEY] down vk=0x%02X\n", (unsigned)vk);
                    fflush(stderr);
                }
            }
            break;
        }
        case SDL_EVENT_KEY_UP: {
            int vk = vk_from_sdl(e.key.key, e.key.scancode);
            if (vk) xbox_InputKeySet(vk, 0);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            set_mouse(e.motion.x, e.motion.y);
            /* SDL reports a motion with no relative change when the window
             * appears under a resting pointer; only a real move counts. */
            if (e.motion.xrel != 0.0f || e.motion.yrel != 0.0f)
                InterlockedIncrement(&fbw_mouse_moves);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            /* Mouse buttons are virtual keys too, so RECOMP_KEYMAP can bind them. */
            static const unsigned char vk[6] = { 0, 0x01, 0x04, 0x02, 0x05, 0x06 };
            set_mouse(e.button.x, e.button.y);
            if (e.button.button == SDL_BUTTON_LEFT && down && fbw_mouse_press_handler &&
                fbw_mouse_press_handler(fbw_mouse_x, fbw_mouse_y))
                break;
            if (e.button.button >= 1 && e.button.button <= 5)
                xbox_InputKeySet(vk[e.button.button], down);
            break;
        }
        /* Switching away with a key held would leave it held for ever. */
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            xbox_InputKeysClear();
            InterlockedExchange(&fbw_inactive, 1);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            InterlockedExchange(&fbw_inactive, 0);
            break;
        default:
            break;
        }
    }
    cursor_tick();
    title_tick();
}

int fbw_sdl_running(void)
{
    return s_window && s_running;
}
