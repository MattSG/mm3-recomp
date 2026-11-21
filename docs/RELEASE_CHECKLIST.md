# Release checklist

What proves a build is releasable, split into what the repository checks by itself and what only a
person with the game, a GPU and a Windows machine can check. Update it when a gate moves.

## Automated (every push, `.github/workflows/build.yml`)

| Gate | Where | Covers |
|---|---|---|
| Builder tests | Windows, Ubuntu | the standalone builder and its release packaging |
| Asset-free contracts | Windows (MSVC `/W4 /WX`) | the mods, launcher settings, tick clock, renderer policy, input, car tools, harness |
| Asset-free contracts | Linux (GCC) | the same suite plus the POSIX platform layer; `tools/run_regressions.sh --ci` |
| No game assets tracked | both | `tests/test_no_game_assets.py` |

## Local, needs SDL3 and a Vulkan device (Mesa lavapipe under Xvfb is enough)

Not in CI because the first configure builds SDL3 and DXC (about 30 minutes).

```sh
cmake -S tests/host_sdl3_hooks -B build/host-tests -G Ninja
cmake --build build/host-tests
RECOMP_SDL_GPU_DRIVER=vulkan xvfb-run -a ctest --test-dir build/host-tests --output-on-failure
```

Covers the SDL3 hooks (pads, audio, window and kernel registration, device creation), the renderer's
draw bindings (9 cases), the production distance-fog pass and the lamp depth rendering (1x/4x, scale 1/3).
The native menu-mouse contract (`tests/menu_mouse`) needs a display for its SDL cursors.

## Local, toolkit (`tools/xboxrecomp`)

* Lifter: `python -m pytest tools -q` (needs `pefile`, `pyyaml`).
* Differential conformance against real x86: build the oracle image once
  (`docker build --platform linux/386 -t xboxrecomp-gcc-i386 -f Dockerfile.gcc .`), then
  `python -m tools.conformance` (6,460 vectors) and `python -m tools.conformance.fuzz --count 400 --seed N`.
  The MSVC-codegen corpus phase needs the Wine images and was not run.
* Runtime C tests (kernel, platform, devices, trapped MMIO): each `tests/*/CMakeLists.txt` is its own project.
  Off Windows they need a renderer: link the contract stubs or an SDL3 host.

## By hand, before tagging

These need the game files, a GPU or a Windows machine and have not been exercised by an agent.

1. **Windows build** with MSVC from a clean checkout (`python scripts/setup.py --iso ...`), launcher
   opens, game boots to the menus. SDL3 is now the only host stack on Windows: check the window,
   fullscreen toggle, resolution and render-scale settings.
2. **Gamepads** on Windows and Linux: a physical pad in the launcher and in a race (buttons, triggers,
   sticks, rumble, hot-plug). Pads now all go through SDL3 (`mm3_sdl3_install_pads` in `main.c`).
3. **Driving acceptance**: `python tools/game_test/run.py` with `driving.json` and `ai-start.json`;
   inspect the captured frame; boot-only runs prove nothing about gameplay.
4. **Generated-code tests** (`tests/needs_generated.txt`, 18 contracts) via `tools/run_regressions.ps1`
   without `-CI`, after `scripts/setup.py` has produced `src/recomp/gen`.
5. **Linux**: boot to the menus and drive a lap on real hardware (the build, the SDL3 host and the
   trapped hardware registers are verified; running the game is not).
6. **Mods**: toggle each feature in the launcher once (the rendering ones were only tested through
   their contracts: fog, lamps, interpolation, widescreen, draw distance).
7. **Xbox Live (experimental)**: untested against a real Insignia account; keep it labelled as such.

## Known open items

* Windows CI's `test_game_harness` flaked twice (exit code 1 from its synthetic game); it now reports the
  fixture's last output. If it recurs, that message says why.
* ThreadSanitizer reports data races in the waitable-timer and APC code of `win32_compat.c`
  (`platform_wait_all` test). The locking reads consistently and they predate the handle-table change, so they
  are unconfirmed; treat a hang or crash around timers or APCs on Linux as the first suspect.
* `tools/xboxrecomp/tests/{apu_monitor,apu_dsp_ack}` are stale against the restructured APU and were not restored.
