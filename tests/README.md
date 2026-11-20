# Tests

Each `mm3_*_regression.py` compiles a C contract against the real sources (and, for the
seam tests, the generated code in `src/recomp/gen`) and runs it. Run one with
`python tests/<name>.py`, or all of them from a shell where the compiler is available.

Needs:

- **Windows:** a Visual Studio developer shell (`vcvars64.bat`), so `cl` is on the path, and the
  Visual Studio CMake ahead of any MSYS/devkitPro `cmake` (an MSYS CMake reads `C:/...` as a relative
  path and the seam tests fail with "file failed to open for reading").
- **Python packages:** `unicorn` and `capstone` (the CPU, ABI and tree regressions emulate x86).
  A throwaway environment is enough: `python -m venv .venv-tests && .venv-tests/Scripts/pip install unicorn capstone`.
- **Generated code:** `python scripts/setup.py --iso ...` must have produced `src/recomp/gen` first.

The builder's own tests (`tests/test_build_gui.py`) need nothing else and are the only tests CI runs.
`tools/game_test/` holds the runtime harness (needs the built game and extracted assets).

`tools/run_regressions.ps1` runs every regression and `test_*.py` in the VS environment and prints a summary (`-Filter` narrows it).
`tools/run_regressions.sh [--ci] [--filter x]` does the same on Linux with GCC (`PYTHON=` picks the interpreter; run under `xvfb-run` for the car editor test); CI runs it as `contracts-linux`.

`tests/menu_mouse/` (the native menu-mouse contract) and `tests/host_sdl3_hooks/` need SDL3 and are not in CI.
`host_sdl3_hooks` also holds the renderer tests (`renderer_bindings`, `renderer_fog`, `renderer_lamps`): white-box GPU
checks of the shipping SDL_GPU renderer (draw bindings, production distance fog, lamp depth), run with
`RECOMP_SDL_GPU_DRIVER=vulkan xvfb-run -a ctest` (Mesa lavapipe is enough). They replace the old D3D11 fog/lamp tests.
The mouse contract drives real SDL cursors, so it needs a display (`xvfb-run` works; `SDL_VIDEO_DRIVER=dummy`
has no cursors). `host_sdl3_hooks` builds SDL3 and DXC on first configure (long) and uses an SDL_GPU device when
there is one (Mesa's lavapipe under Xvfb is enough).
