/* The SDL3 framebuffer window, as the SDL3 GPU renderer drives it (see
 * fb_window_sdl.c): opened and pumped on the renderer's thread. */
#ifndef XBOXRECOMP_FB_WINDOW_SDL_H
#define XBOXRECOMP_FB_WINDOW_SDL_H

#include <SDL3/SDL.h>

/* Whether RECOMP_FB_WINDOW asks for a window. */
int fbw_sdl_wanted(void);
/* The window, created on the first call; NULL if none is wanted or it failed. */
SDL_Window *fbw_sdl_open(SDL_WindowFlags extra_flags);
/* Handle pending events: keys, pointer, focus, close, fullscreen; and the
 * cursor and title-bar statistics. */
void fbw_sdl_pump(void);
/* Open and not closed by the user. */
int fbw_sdl_running(void);

#endif
