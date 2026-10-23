# Historical SDL window driver observations

Previous comment from fb_window_sdl.c. These observed runs explain the existing
Linux driver preference; they do not establish current platform-wide behavior.
The generic default hint and caller override mechanism are unchanged.

```c
/* Call before SDL's video subsystem starts. On Linux, native Wayland first and
 * X11 second: through XWayland, SDL's X11 backend aborted the process on an X
 * error (BadRROutput from RRGetOutputInfo) when the display layout changed
 * mid-game, and on Wayland the same runs were clean. SDL itself only prefers
 * Wayland when the compositor has fifo-v1/commit-timing. SDL_VIDEO_DRIVER (or
 * a hint set earlier) still decides. */
```
