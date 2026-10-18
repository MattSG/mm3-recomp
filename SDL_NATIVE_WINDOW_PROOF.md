# SDL native window verification, 2026-10-18

The SDL3 launcher and game already use the Windows native titlebar and frame.
Neither creation path requests a borderless window. The launcher content uses
custom RmlUi controls; native window decorations do not make those native widgets.

## Windows evidence

Built this branch with its SDL runtime after cherry-picking the validated
crypto/settings/replay and overlapping-instruction fixes from main.
Visual Studio 2026, x64, RelWithDebInfo, SDL3_GPU; /FS avoids concurrent SDL
compiler PDB writes. No primary checkout or generated header was overwritten.

- Executable SHA-256:
  `8cbdda1c255aca23946506d68a1d8c90d5017d1a09ac43eb1828e931d6c81491`.
- Launcher: `tests/mm3_launcher_render_smoke.py --visible --exe <SDL executable>`
  passed: 760x720 client, native HWND style `0x16ca0000`, caption, system menu,
  no popup frame, successful first GPU frame and normal WM_CLOSE exit.
- Game: the existing Python Session harness owned a separate child and private
  saves. It observed the `SDL windows` driver, captured a complete 960x540 frame,
  and inspected that child's window: style `0x16cf0000`, caption, system menu,
  thick resizable frame, no popup frame. At the inspection point its client was
  384x216 and outer window was 397x251, including the native non-client area.
- Game evidence remains in the main checkout under
  `build/audit-20261017/sdl-native-game/run-pnrsqcyj/`: `result.json`,
  `native-window-style.json`, logs and capture. This proves the native frame,
  not driving acceptance or pixel correctness.

SDL suspends launcher drawing while hidden. The existing Windows test's hidden
first-frame requirement therefore cannot exercise SDL presentation correctly.
The explicit `--visible` test option preserves the default D3D11 hidden-start
regression while permitting a real SDL launcher frame.

## Regression checks

- Windows SDL fork: crypto CTest passed; settings CTest passed, including
  persistent independent save roots and concurrent reader/writer processes.
- Linux WSL SDL fork: crypto and settings CTests passed.
- SDL fork replay snapshot allocation-failure and restoration test passed.
  The test mock for its presentation boundary fails if unexpectedly called.
- Translator overlapping-branch differential and fallthrough tests: 7 passed.

## Linux appearance

The SDL Wayland backend normally prefers compositor decorations. On desktops
without server-side decorations, native-looking titlebars need libdecor and
its GTK plugin. Disabling libdecor can remove decorations; no redundant SDL
hint or custom titlebar was added. Linux native-frame appearance was researched,
but not visually validated in this Windows run.

References: [SDL Wayland guidance](https://github.com/libsdl-org/SDL/blob/main/docs/README-wayland.md),
[SDL decoration preference](https://wiki.libsdl.org/SDL3/SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR).
