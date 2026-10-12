# Midtown Madness 3 PC Native

**Midtown Madness 3 PC Native** brings the Xbox arcade racing game to Windows
10 and Windows 11 x64 through static recompilation. This native MM3 PC port
uses the [xboxrecomp](tools/xboxrecomp) runtime and a Direct3D 11 renderer.

An improved and modded PC version with keyboard controls, widescreen graphics,
custom-car tools, gameplay repairs and performance work. Build it from your own
supported game disc with the instructions below. See the
[upstream comparison](#upstream-comparison-2026-10-12) for which recent MattSG
fixes are present in this fork and which still need integration.

**Credits:** This project builds on the original
[mm3-recomp by Matt Graham (MattSG)](https://github.com/MattSG/mm3-recomp) and the
[xboxrecomp toolkit](https://github.com/MattSG/xboxrecomp). Credit for that foundation
belongs to the original author and upstream contributors.

This fork is maintained by [lakshya16157](https://github.com/lakshya16157), with
assistance from OpenAI Codex on performance and runtime improvements, gameplay
fixes, and the custom-car tools.

## Project keywords

Terms that describe this project and help players find the right repository:

- **Game and project:** Midtown Madness 3, Midtown Madness 3 PC Native,
  Midtown Madness 3 on PC, Midtown Madness 3 PC port, Midtown Madness 3 for
  Windows, MM3 PC, MM3 native, MM3 recompilation, mm3-recomp.
- **Platform and technology:** Xbox to PC, native PC gaming, Windows 10,
  Windows 11, x64, x86-64, static recompilation, Xbox recompilation,
  xboxrecomp, Direct3D 11, D3D11.
- **Mods and improvements:** Midtown Madness 3 mods, MM3 car mods, custom cars,
  car workshop, car model conversion, keyboard controls, widescreen,
  graphics fixes, gameplay bug fixes, performance improvements,
  shader caching, frame pacing, improved draw distance.
- **Gameplay:** arcade racing, driving game, Paris, Washington DC,
  free-roam driving, Blitz races, Checkpoint races.

## How this fork was developed

The maintainer built and tested this fork from a Midtown Madness 3 `.iso`/XISO,
using the [xemu Xbox emulator](https://xemu.app/) to run the original Xbox game
as a reference during development. The native build extracts the game files
from a supported disc image and statically recompiles its Xbox CPU code into
a Windows executable; it then runs on the xboxrecomp runtime with Direct3D 11.
Launching that native executable does not require xemu.

Bring your own game disc or supported image. Game data, disc images, xemu BIOS
files and emulator hard disks are not distributed in this repository. The
[disc notes below](#which-disc) explain accepted native-build images; xemu's
reference workflow uses a bare XDVDFS XISO rather than a two-part redump image.

## Improvements in this fork

- **Visuals:** fixed refreshing of streamed textures and CPU-written swizzled
  surfaces, and prevented stale vertex programs/constants after failed GPU
  uploads. Building-detail fades scale with extended draw distance, and the car
  converter builds closed body and animated-wheel shadow proxies. Widescreen
  pause menus keep button caps, labels and controller prompts joined in one layout.
  Race maps and their checkpoint icons share one placement while expanding or turning.
  Yellow race guidance follows the cities' road curves and junctions, and updates
  the path to the next checkpoint as the car moves.
  The top HUD direction arrows and numeric speed readout are hidden; the
  analog speedometer, minimap, route and checkpoint markers remain available.
  Cubic B-spline sampling smooths the distant skyline, and corrected frame/depth
  alignment removes the sampling offsets that caused bright silhouette rims
  and uncovered top/left frame strips.
  Native GPU coverage for these changes lives in
  `tools/xboxrecomp/tests/nv2a_graphics_quality`: independent cubic RGBA samples,
  panorama selection exclusions, packed depth at four scales with/without MSAA,
  and actual vertex output for display/menu/offscreen placement.
  Balanced renders
  at 1536×960 and Quality at 2304×1440, both at 16:10, with
  configurable aspect, resolution, MSAA, anisotropic filtering and draw distance.
- **Efficiency:** persistent shader caching and startup prewarming, less repeated
  texture/shader lookup work, optimized texture conversion and audio processing,
  and GPU-completion waits that let waiting threads sleep.
- **Timing and runtime:** a 60 FPS maximum, presentation pacing immediately before
  Present, command/fence ordering repairs, safer guest-handle access, and save-root
  handling. Graphics memory is reused across scene loads without treating skipped
  heap addresses as occupied. Extended-distance particle overlap is bounded to
  reduce effects load. CPU-drawn loading prompts continue updating after a race,
  so choosing Next Race displays the A/Start prompt instead of a black screen.
- **Driving and selection:** arrow-key driving, camera and manual-shift controls,
  focus-aware keyboard input, and selection of all 35 supported-disc car models.
- **Car workshop:** ten extra tuned variants, optional prepared model
  packages, editable paint and driving stats, sourced specification references
  and a **Restore default driving** action. The presets retain the stock donor
  engine, gearbox and handling, adjusting only weight and power. Each extra
  car has its own HUD/interior speedometer
  range, with analog HUD gauges.

The detailed settings and runtime notes below describe these changes and their
limits. See the [car-pack guide](tools/car-mods/README.md) for further tuning examples.

## Upstream comparison (2026-10-12)

Compared the actual local sources and generated Xbox C with
[MattSG/mm3-recomp at `a24b737`](https://github.com/MattSG/mm3-recomp/commit/a24b737)
and its runtime, [MattSG/xboxrecomp at `b666568`](https://github.com/MattSG/xboxrecomp/commit/b666568).
Upstream had 33 game-repository commits outside this fork's ancestry at audit time.
That count does not mean 33 missing fixes: some commits update documentation or
the runtime pointer, and some repairs already have independent implementations here.

**The latest upstream improvements have not been merged wholesale into this
fork.** Its toolkit remains pinned to `2a521ff`, with this fork's reproducible
runtime patch on top. Fetching upstream updates the comparison refs; it does
not change the translated game, the pinned toolkit or the working executable.
This publication preserves the current tested fork and records the remaining
integration work below.

The detailed future implementation checklist is saved in
[Next fixes to do.txt](Next%20fixes%20to%20do.txt), including root causes,
source locations, dependencies, integration steps and acceptance tests.

### Fixes still missing or only partly covered

| Upstream fix | Status in this fork | Code comparison and remaining work |
|---|---|---|
| [Collision-cell bounds](https://github.com/MattSG/mm3-recomp/commit/9ad1762): freezes, crashes and falling through roads | Missing | No grid/index sanitizer around `sub_0015A929`. Our collision-call stack repair fixes a separate fault. |
| [Two unresolved functions](https://github.com/MattSG/mm3-recomp/commit/c91d8f1): corrupted guest registers | Partial | The graph epilogue at `0x144ED9` is repaired. The camera-related setter at `0x2534FF` is still an unresolved stub referenced by six generated thunk-tail sites. Direct unresolved-stub reporting is also absent. |
| [Bounded push-buffer run-ahead](https://github.com/MattSG/mm3-recomp/commit/fdc33ea): one-frame dark/tunnel-lighting flicker | Missing | Our WBC flush only uses `_mm_sfence()`; it lacks upstream's 256 KiB producer-distance bound. Native command/fence waits do not implement that bound. |
| [White rims on silhouettes](https://github.com/MattSG/xboxrecomp/commit/d62ae29) | Implemented after this audit | Display passes use aligned translation; Y16 depth taps compensate their quarter-pixel offset. |
| [Bright top/left frame strip](https://github.com/MattSG/xboxrecomp/commit/4d9d178) | Implemented after this audit | Display-sized passes remove the Xbox screen offset while offscreen targets retain pixel-zero snapping. |
| [Extended-distance haze](https://github.com/MattSG/xboxrecomp/commit/d448358) and [distant-tree fade](https://github.com/MattSG/xboxrecomp/commit/060122c) | Missing | The old linear depth remap remains. There is no post-haze foliage coverage fade or final ordered coverage pattern that removes the earlier visible white flicker at default supersampling. |
| [Smooth horizon panorama](https://github.com/MattSG/xboxrecomp/commit/0e63cf0) | Implemented after this audit | Four bilinear taps reconstruct the skyline with cubic B-spline weights; MM3 opts in its panorama blend and texture signature. |
| [Reset fade keeps the damage bar on the gauge](https://github.com/MattSG/xboxrecomp/commit/76192ee) | Missing | The HUD-texture exclusion and fullscreen-fade grouping correction are absent. |
| [Pause/post-race button caps remain joined](https://github.com/MattSG/xboxrecomp/commit/7367dca) | Partial | Our pause-state callback preserves the pause layout. Upstream also detects missing minimap/start rectangles across current and previous frames; equivalent post-race coverage is not established here. |
| [Audio queue cushion](https://github.com/MattSG/xboxrecomp/commit/9b9a37c) and [audio producer priority](https://github.com/MattSG/xboxrecomp/commit/a4c2800) | Missing | `apu_xaudio2.c` lacks the silent cushion, and `apu_core.c` lacks the new time-critical frame-thread priority. Our PCM queue optimization is a separate change. |
| [Device interrupts preempt DISPATCH](https://github.com/MattSG/xboxrecomp/commit/d359ee0): half-second freezes | Missing | The APU still holds off delivery while DISPATCH exclusion is owned. Our DPC ordering and interrupt-synchronization repairs do not implement this preemption change. |
| [Facade slabs and capacity](https://github.com/MattSG/mm3-recomp/commit/31451f6) | Partial | Our allocation-time repair enlarges three capacities by `ceil(draw_distance^2)`, capped at 4x, with failure rollback. Upstream enlarges four fields by 5x and caps facade-zone distance at 2x. Local zones can still scale to 4x; these are different implementations. |
| [Quiet hot-path logging](https://github.com/MattSG/xboxrecomp/commit/4f4dbc6) | Partial | Summary, path, successful-read and push-buffer logs are already reduced or opt-in here. Expected missing-file failures remain unbounded, and upstream's spike-stage split is absent. Focused building traces add work; disable them for performance comparisons. |

The main implementation evidence is in `src/recomp_manual.c`,
`src/mm3_widescreen.c`, `src/mm3_facade_capacity.c`,
`src/mm3_render_distance.c`, `mm3_runtime_entry_repairs.json` and the patched
runtime's `src/video/nv2a_d3d11.c`, `src/apu/apu_core.c`,
`src/apu/apu_xaudio2.c` and `src/kernel/kernel_hal.c`. Generated Xbox C is
rebuilt locally from the supported disc rather than bundled in this repository.

### Upstream fixes already covered independently

- **[80-bit `fld` throttle/gearing repair](https://github.com/MattSG/mm3-recomp/commit/fd112c6):**
  the affected math helper is recovered with `MEMFP80`, and same-chunk calls
  route to it. The functional fix is present, although upstream's generic
  lifter patch is absent and the bypassed original generated function still
  contains the old load. See `scripts/Generate-MM3RuntimeEntries.py`, the repair JSON
  and CMake's recovered-helper routing.
- **[DSP inlining](https://github.com/MattSG/xboxrecomp/commit/7d47dc8) and
  [idle interrupt-check/fetch optimization](https://github.com/MattSG/xboxrecomp/commit/9d8716e):**
  our optimized Windows build already applies `/Ob2` to the hot interpreter,
  forces instruction-fetch inlining and skips idle interrupt work. Upstream's
  explicit GNU `always_inline` path is not present.
- **Developer tools off by default:** local developer features already require
  opt-in flags. The public play script also leaves focused building traces off
  by default; use `-TraceBuildings` only for a diagnostic capture.

### Other upstream improvements still absent

- Separate `RECOMP_VIEW_DISTANCE` from detail/LOD distance, with upstream's
  updated default distances. This fork currently uses one draw-distance control.
- The RmlUi settings launcher, saved key/mouse bindings, menu hover/click control
  and switchable mod registry introduced in upstream `9118262`/runtime `b666568`.
  Our performance profiles, car workshop and driving controls are separate features.
- Spike screenshots, game-thread sampling, guest call chains and the new timing
  stage reports. Local hitch tracing and frame captures do not supply those features.
- Heading display/input in the teleport GUI. The pipe already accepts an
  optional heading in radians; the GUI still only edits position.
- The 240 FPS launch option and achieved-FPS race-test reporting. Upstream's
  `8cff2f2`/`965f6d0` report successful higher-rate tests and expand the launch
  range; they do not add a new AI algorithm. This fork retains its 60 FPS cap
  and has not reproduced those higher-rate tests.

Earlier publication validation: the `RelWithDebInfo` game build passes, along with 126
Python tests (two optional cases skipped), 25 standalone native regressions,
four additional factory-physics/gearing variants and 195 CTest cases across 19
suites. The runtime patch applies to a clean export of the pinned toolkit and
reproduces all 96 modified or added runtime files after Git newline normalization.
These checks verify the published changes; they do not establish that the
missing upstream fixes above are unnecessary or that every gameplay bug is fixed.

The subsequent visual upgrades pass four new native GPU cases and 38 existing
renderer cases, plus HUD/map/menu and profile/launcher contracts. Their runtime
patch reconstructs all 99 changed or added runtime files from the pinned
toolkit, with fresh apply/reverse checks. The internal resolutions remain the
same; the stronger MSAA settings have not been compared in a matched performance run.

## Building from a fresh clone

You need Windows 10/11 x64, Visual Studio 2022 or newer with **Desktop development with C++**
(it includes CMake), Python 3.12+ recommended (3.11.4+ supported), git,
and your own Midtown Madness 3 disc.
Most examples use PowerShell 7 (`pwsh`); the pinned-version commands below use
Windows' built-in `powershell` and also work with `pwsh`.

```powershell
git clone --recurse-submodules https://github.com/lakshya16157/midtown-madness-3-PC-Native.git
cd midtown-madness-3-PC-Native
pwsh scripts\setup.ps1 -Iso "D:\Games\Midtown Madness 3 (USA).iso"
pwsh scripts\play.ps1            # -Keyboard to drive with the keyboard
```

`setup.ps1` checks the toolchain, makes `tools\venv` (pinned capstone), verifies the disc and
extracts it to `game_files\`, generates the translated C into `src\recomp\gen\` (about six
minutes, deterministic) and builds `build\RelWithDebInfo\mm3_recomp.exe`. Re-running skips
finished steps; `-Force` redoes them. Nothing from the disc is committed.

This repository includes its runtime fixes in `patches/xboxrecomp-mm3.patch`.
Setup applies them to the pinned Xbox runtime submodule automatically and safely
skips an already applied patch. The upstream toolkit and its generator history
remain available through the submodule. Before building manually, run
`python scripts/apply_runtime_patch.py` once after initializing submodules.

For a checkout with local tools installed, activate them in the current PowerShell session:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\scripts\enter-env.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup.ps1 -Iso "D:\Games\Midtown Madness 3 (USA).iso"
```

The local environment uses `tools\venv`, portable PowerShell at `tools\bin\powershell`,
and MSVC/Windows SDK at `tools\bin\msvc` (via `setup_x64.bat`). With local MSVC,
install CMake and Ninja in the venv (`python -m pip install "cmake>=3.20,<4" ninja`);
setup uses Ninja Multi-Config and keeps the same `build\<Config>` output paths.
Run these commands from the repository directory, not its parent folder.

### Reproduce the improved version (`v2026.10.12.3`)

The tag **[v2026.10.12.3](https://github.com/lakshya16157/midtown-madness-3-PC-Native/tree/v2026.10.12.3)**
pins this fork's gameplay improvements at that snapshot: race/menu loading and heap fixes, building
detail fades, visibility and rendering work, corrected imported-car shadow
generation, and the ten researched car presets with their gearing and gauges.
It also includes the joined pause-menu buttons, Space handbrake, the closest
chase camera's 0.25 m adjustment, adjustable driving FOV, aligned race-map
checkpoints and model credits. This snapshot adds map zoom and race guidance,
facade-capacity scaling, building/tree diagnostics, car light and reflection
repair tools, and restored factory driving defaults. The earlier `v2026.10.12`,
`v2026.10.12.1` and `v2026.10.12.2` tags remain available as previous snapshots.
Use this tag to reproduce that source and settings version after `main` moves on.
The minimal-logging launcher default and detailed future-fix checklist were
added later on `main`.

If you have already extracted or recompiled the game with upstream, the generated
Xbox C alone does not contain this fork's native hooks, runtime patch or car data.
Build this checkout in a separate directory, using your own supported disc image:

```powershell
git clone --branch v2026.10.12.3 --recurse-submodules https://github.com/lakshya16157/midtown-madness-3-PC-Native.git mm3-improved
cd mm3-improved
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\setup.ps1 -Iso "D:\Games\Midtown Madness 3 (USA).iso"
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --config tools\car-mods\cars.json --apply
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\play.ps1 -Profile Balanced -Keyboard
```

Setup applies the committed runtime patch to the pinned submodule, generates the
supported translation, configures all native hooks and builds the game. Installing
the car preset is a separate step: setup alone does not install the ten extras.
The explicit `--config` selects the published settings even if a machine has a
different `cars.local.json`. `git describe --tags --exact-match` should report
`v2026.10.12.3`; `git submodule status` should show the pinned toolkit, without a
leading `+`. A dirty runtime submodule is expected after applying this fork's patch.

**To reproduce the imported car bodies and repaired shadows**, obtain the nine
original model archives listed in the [model guide](tools/car-models/README.md),
install its conversion dependencies and follow its import/preview/package recipes.
Then close the game and select the portable imported-model preset:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --config tools\car-mods\cars.imported.json --apply
tools\car-mods\editor.cmd --config tools\car-mods\cars.imported.json
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\play.ps1 -Profile Balanced -Keyboard
```

The preset points at `out/car-models/packages/<alias>` for each imported car and
uses the same tuning as `cars.json`. Mini Rally keeps its native Mini body. Game
files, source-model downloads and prepared packages are not bundled in the tag;
without building those packages, use `cars.json` for the same car slots and tuning
with donor visuals. Keep the source authors' readmes and applicable usage terms.

For an existing checkout of **this tag** with extracted game files, run setup
again without `-Iso` to rebuild, then reapply your chosen preset. Setup always
reconfigures CMake; manual rebuilds should also configure before building:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\setup.ps1
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --config tools\car-mods\cars.json --apply
```

To regenerate just the translated code without re-extracting your game data,
run these commands in the existing build environment, then reapply the preset:

```powershell
. .\scripts\enter-env.ps1
python scripts\generate_mm3.py
cmake -S . -B build
cmake --build build --config RelWithDebInfo --target mm3_recomp --parallel
```

Use a fresh checkout when moving from an older fork version whose runtime patch
is already applied. A plain pull does not replace that dirty runtime with the
new patch, and the patch helper intentionally rejects mixed versions. Keep old
local work and saves in the previous directory. `-Force -Iso <path>` re-extracts
and regenerates the game; reapply the car preset afterward.

The tag reproduces the published source, presets and conversion recipes. It does
not promise a byte-identical executable across compiler versions, identical
historical model-package packing, saved progression or identical FPS on another
computer. Unpublished race-car values and MM3's hybrid/handling approximations
remain identified in Workshop's **Car specs** tab and the specification catalog.

### Which disc

Any dump whose `default.xbe` carries the retail code: a redump `.iso` (video + game partitions)
or a bare XISO. `scripts\extract_disc.py` gates on a SHA-256 of the XBE's section data
(`9c437324…6bbf8ba4`) with the one byte XISO tools patch to skip the DVD media check
(VA `0x87226`, `jge` → `jmp`) normalised, so region/certificate differences and that patch do
not matter. USA and Europe/Australia share the same code. Extraction applies that same media
patch to `game_files\default.xbe` (the emulated drive can't authenticate a pressed disc), so every
supported disc translates identically. Images tested end to end:

| SHA-1 | Image |
|---|---|
| `52cd998731eb67477b44c49c4152f56c0fc58a56` | Midtown Madness 3 (USA) — redump `.iso`, 7.8 GB |
| `8ed003adaf504883afc980d2888df6125496d044` | Midtown Madness 3 (Europe, Australia) (En,Fr,De,Es,It) — XISO, 3.6 GB |

An untested image with matching code is accepted with a note; anything else is refused with
this list.

### Regenerating

`scripts\generate_mm3.py` is the only generation entry point. It runs the recompiler pinned to
toolkit commit `967ec30` (extracted to `tools\.generator\`): its output is the translation the
game was proven on, and regenerating reproduces it function for function. The submodule's
newer recompiler (upstream v0.13.x) translates ~3.6k functions differently and has not been
through a play session yet; try it with `MM3_GENERATOR_DIR=tools\xboxrecomp`. The hooks in
`CMakeLists.txt` find each wrapped function's chunk themselves, so any regeneration builds
without edits.

## Running

`scripts\play.ps1` sets the defaults below and runs the build from the repository root (the
game reads `game_files\`). By hand:

```powershell
$env:RECOMP_PB_EXEC='1'; $env:RECOMP_FB_WINDOW='1'; $env:RECOMP_USB='1'
$env:RECOMP_APU_DSP_ACK='0x80458810'
build\RelWithDebInfo\mm3_recomp.exe
```

The game enforces a maximum of **60 FPS** for presentation and emulated vblank.
`RECOMP_FPS_LIMIT` may select an integer limit from 1 to 60. When unset, it uses
the selected profile's cap: 60 for Balanced/Quality or 30 for LowPower. Invalid,
uncapped (`0`), or higher values use 60, including when launching the executable directly.

F11 or Alt+Enter toggles fullscreen. The default **Balanced** profile renders at
1536×960 with a 16:10 aspect and scales the image to the window or fullscreen
size. The internal height stays fixed when you maximize the window. Choose a
profile when launching:

```powershell
pwsh scripts\play.ps1 -Profile Balanced
pwsh scripts\play.ps1 -Profile Quality
pwsh scripts\play.ps1 -Profile LowPower
```

| Profile | Internal height | MSAA | Draw distance | FPS cap |
|---|---|---|---|---|
| Balanced (default) | 960 | 4× | 2× Xbox | 60 |
| Quality | 1440 | 8× | 2× Xbox | 60 |
| LowPower | 720 | Off | Xbox | 30 |

All three retain 16× anisotropic filtering. LowPower reduces resolution, edge
smoothing and view distance and caps motion at 30 FPS. Balanced and Quality
now increase edge and cutout smoothing without increasing their resolution.
Unsupported MSAA counts fall back to the highest supported count.
Direct executable launches use the same profiles
through `MM3_PERFORMANCE_PROFILE=Balanced`, `Quality` or `LowPower`; unset or
unrecognized values select Balanced. Individual `RECOMP_*` environment settings
take priority. `-FpsLimit 45` selects a different cap when `RECOMP_FPS_LIMIT` is
unset. The launcher restores environment defaults it added when the game exits.

Matched stationary runs on 2026-10-11 used a Ryzen 9 PRO 7940HS (8 cores,
16 threads), an RTX 2000 Ada Generation Laptop GPU with 8 GB VRAM (8,188 MiB
reported by `nvidia-smi`), and 32 GB system RAM. Each profile used Senna GTR in
Paris at the same position, about 132 draws per frame, and approximately 31.8
seconds. These historical runs used the earlier Balanced 2× / Quality 4× MSAA
defaults; they do not measure the current stronger MSAA settings:

| Profile | Mean FPS | 99th-percentile frame interval | Process CPU | NVIDIA GPU | GPU power |
|---|---|---|---|---|---|
| Quality | 59.97 | 19.62 ms | 11.39% | 43.37% | 28.18 W |
| Balanced | 60.00 | 17.34 ms | 11.20% | 31.30% | 14.37 W |
| LowPower | 30.00 | 33.85 ms | 8.70% | 18.73% | 10.56 W |

CPU percentages use all 16 logical processors as 100%. GPU utilization and
power are device-wide samples, so they include other activity. These short,
stationary measurements support Balanced's lower GPU demand while retaining
60 FPS; they do not establish a renderer CPU improvement or predict busy
driving scenes on other machines. No recorded frame interval exceeded 50 ms.
LowPower's approximately 33.33 ms intervals follow its 30 FPS cap.

Four native scene loads also completed with no detected guest-heap overlap.
That checks the repaired allocation path within those runs; it does not prove
that every possible crash or menu stall is fixed.

A separate 45-second Balanced driving run averaged 57.80 FPS, with a
99th-percentile interval of 22.86 ms and one 506.39 ms hitch. Its busier scene
is not comparable with the stationary table. An intermittent intro-video
decoder crash was also observed; additional instrumented runs did not
reproduce it. These two issues remain unresolved.

Set `RECOMP_RENDER_SCALE=auto` to follow the window resolution instead;
`RECOMP_SUPERSAMPLE` applies in that mode. Set `RECOMP_ASPECT=auto` to follow the
window's aspect, or select another aspect explicitly.

With `scripts\play.ps1 -Keyboard`, hold **Up** to accelerate, **Down** to brake/reverse,
and **Left/Right** to steer. Arrows also navigate menus. **Z** confirms, **X** goes back,
and **Escape** also goes back. **Enter** pauses/resumes. **C** changes the camera
view while driving. Hold **Space** for the handbrake (the game's default Xbox A
control); **Z** still works for A. Space is active only while driving with the
MM3 keyboard layout. The original
**3** accelerator, **1** brake and numpad **4/6** steering keys still work. Keyboard input
releases when the game loses focus.
Close the focused game with **Alt+F4**, or click the window's **X**. Closing
the display ends the process and releases the game archives. The launcher
captures output without attaching the game to the terminal, so terminal
**Ctrl+C** is not the game-close shortcut.
Hold **Ctrl** to enlarge the race map; its checkpoint icons stay aligned with the roads.
Both map sizes smoothly show more road as speed rises above 20 km/h, reaching
25% more visible distance at 120 km/h, then ease back as you slow down.
Their initial framing stays stable as you approach checkpoints.
Both map sizes draw the ordered checkpoint course along actual streets with
an amber path. The brighter leg updates from the car to the next checkpoint,
including after a wrong turn. Passing checkpoints keeps the full course visible. Checkpoint
icons shrink with the map's speed zoom, reaching 80% of their original width and
height at maximum zoom-out, while retaining their colors and pulsing. World hoops
retain their original size.
Blitz courses stay open; checkpoint races include the closing lap segment.
Choose **Manual** transmission in car setup, then tap **R** to shift up and **F** to shift down
(the default Xbox B/X gear controls). These aliases operate only while driving.
Set `RECOMP_KEYBOARD_LAYOUT=legacy` to use only the original runtime bindings.

The closest rear chase camera sits 0.25 m farther back. Its original smoothing
and collision adjustment still apply; the other camera views retain their
original distances.

Choose a wider driving field of view when launching, for example:

```powershell
pwsh scripts\play.ps1 -Profile Balanced -Keyboard -Fullscreen -Fov 100
```

`-Fov` accepts 60–110 degrees of horizontal FOV at your display aspect, including
decimal values. Omit it to retain the existing widescreen FOV. The setting
applies to driving views, with matching city visibility; menus and mirrors use
their original projection. Restart the game to change it. An existing
`MM3_FOV_DEGREES` environment setting takes priority over the launch argument.

The 35 original cars follow the game's earned availability rules, including hidden bonus
cars. Optional custom cars remain available as configured by their installer.
Set `MM3_UNLOCK_ALL_CARS=1` to make every installed car selectable; unset, `0` and
other values preserve the original rules. Event-specific vehicle classes, bonus paints
and career statistics still follow the game and profile. The override does not write
unlock keys to your save.

## Added cars and editing stats

The optional [extra-car pack](tools/car-mods/README.md) adds ten separately
selectable slots alongside the 35 original cars: Mini Rally,
Panoz GTR-1, McLaren F1, Formula 1, Ford GT,
McLaren Senna, McLaren Senna GTR, Ferrari FXX K, Koenigsegg Regera and Lamborghini
Aventador. Each has its own name, paint settings, menu ratings and driving setup.
The [model names, download sources and creator credits](tools/car-models/README.md#extra-car-names-and-model-credits)
identify each imported exterior and its original conversion pack.
The remaining slots retain their IDs. Profiles that remember a removed custom
car use its original donor vehicle, without restoring the removed menu entry.

The supplied defaults use stock donor visuals. Workshop's **Model** tab can
select prepared imported body, wheel, texture and menu-icon packages; reapplying
stats preserves that selection. The [model workflow](tools/car-models/README.md)
documents the verified Panoz, F1 GTR, Ferrari F2007, Ford GT, Senna, Senna GTR,
FXX K, Regera and Aventador conversions. Downloaded models and generated packages
are not distributed in this repository. Imported initial liveries remain fixed,
glass is opaque, and interior, sounds and collision remain based on the donor.
Roadside trees fall in the impact direction at or above 18 km/h, retaining
native impact sounds and effects while clearing the struck tree's collision.
Defaults retain the donor's engine curve, automatic shifting, gearbox and
handling. Only mass and native torque scale are tuned for arcade driving.
Manufacturer specifications remain available as references in Workshop.

### Visual editor

1. Close the game, then double-click [tools/car-mods/editor.cmd](tools/car-mods/editor.cmd)
   or run `tools\car-mods\editor.cmd` from the repository root.
2. Select an added car. **Car specs** shows its reference variant, source links
   and calibration notes; **Restore default driving** restores the weight/power
   preset while retaining paint, ratings, gauges and model choices. Edit its paint, menu ratings, physics, gearing, RPM
   settings and interior speedometer ranges. Blank optional physics fields inherit
   the stock donor's settings.
3. Choose **Save** to write the selected manifest, or **Save & Apply**
   to save and install the changes into your extracted `game_files` data.
4. Restart the game and select the car to load the changes.

The paint view is a schematic color preview; it does not display or edit the 3D
car mesh. The editor is a small Python/Tkinter desktop app with no web server or
extra GUI packages. Its source is [editor.py](tools/car-mods/editor.py).

### JSON and command-line editing

The editor and installer select `tools/car-mods/cars.local.json` when present,
otherwise [tools/car-mods/cars.json](tools/car-mods/cars.json). The local file is
ignored by Git and keeps machine-specific model selections and tuning. Copy the
tracked defaults to `cars.local.json` to start local settings on a fresh checkout.
You can edit the selected file directly with a text editor. From the repository
root, run the preview first, then apply with
the game closed:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\prepare.py
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --apply
pwsh scripts\play.ps1 -Keyboard
```

Both the editor and installer accept `--config <path>` and `--game-dir <path>`
for another manifest or extracted game directory. For example:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\editor.py --config tools\car-mods\cars.json --game-dir game_files
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --config tools\car-mods\cars.json --game-dir game_files --apply
```

Keep `id`, `alias`, `donor` and `display_name` unchanged: these identities are
registered by the runtime. Adding a new identity or donor body requires code
changes as well as a manifest entry. The supplied editor and manifest tune the
added variants; they do not provide a general editor for every original car.

### What each setting changes

Menu ratings are integers from 0 to 9: `TOPSPEED`, `ACCELERATION`, `MASS`,
`HANDLING` and `DURABILITY`. They control the menu bars. Actual driving behavior
comes from the separate `physics` settings, and `colors` contains one to three
paint colors in `#rrggbb` format.

| Physics setting | Effect |
|---|---|
| `TorqueScale`, `TorqueScaleFactor` | Peak torque in Nm with a factory profile, or donor torque scale; its multiplier. |
| `Mass` | Physical weight. |
| `BrakeTorque`, `HandBrakeTorque` | Braking and handbrake strength. |
| `LatFrictionFactor` | Cornering grip. |
| `AirKZZ`, `AirKZY` | Aerodynamic coefficients. |
| `WheelDrive` | 0: front wheels; 1: rear wheels; 2: all wheels. |
| `SteeringEndSpeed` | Speed-dependent steering setting. |
| `WheelRadius` | Wheel radius, also used in automatic shift-speed calculations. |
| `GearRatioVector` | Reverse ratio followed by one through seven forward ratios. |
| `WheelRatio` | Final drive multiplier; a smaller value makes gears longer. |
| `TimeChangeGear`, `TimeChangeGearFR` | Forward shifting and forward/reverse transition times, in seconds. |
| `MinRPM`, `MinRPMGearChange`, `CutRPM`, `MaxRPM` | Idle, minimum shift, cutoff and redline settings. |
| `MaxTorqueRPM`, `MaxTurboTorqueRPM` | Peak torque RPM references; automatic shift target in donor mode. |
| `GearStatusDownGearFactor` | Automatic downshift factor. |

For example, changing Mini Rally's `physics.TorqueScale` from `380` to `400`
increases engine torque. Changing `ratings.ACCELERATION` changes only its menu bar.
The installer validates supported value ranges before applying changes.

### Gearing and speedometers

Use a negative reverse ratio followed by positive, decreasing forward ratios,
for example `[-3.3, 2.5, 1.72, 1.28, 1.0, 0.82, 0.7]`. Gear count is derived
from this array; do not add `NGears`. Keep
`MinRPM < MinRPMGearChange < CutRPM <= MaxRPM`, and keep both peak-torque RPM
settings above idle and below cutoff. Donor mode retains the stock torque curve's
8,000 RPM peak limit. The supplied presets leave the donor's gear ratios,
final drive, RPM limits and shift times unchanged. Formula 1 uses the Lotus
five-speed gearbox; McLaren F1 and Regera use the Koenigsegg six-speed gearbox.
The optional advanced `engine_profile: "factory"` mode remains available for
experiments with researched curves and transmissions; it is disabled in the
default and imported presets.

Automatic upshifts depend on road speed calculated from gear ratios, final drive,
wheel radius and the factory profile's shift target, or peak torque RPM in donor
mode (the turbo peak for a turbo-equipped donor).
Wheelspin can raise RPM before the car reaches its shift speed. Lowering
`MinRPMGearChange` alone does not make automatic upshifts happen earlier.
The default presets restore the stock shifting behavior, and the installer
rejects conflicting thresholds in custom edits. Select **Automatic** or **Manual**
in the game's car setup; Manual uses **R/F** on the MM3 keyboard layout or Xbox
**B/X** to shift up/down while driving.

The optional `speedometer.max_kmh` and `speedometer.max_mph` values set the
HUD dial and interior dashboard needle range. Extra-car ranges provide rounded
headroom for each preset: **220 km/h / 140 mph** for Mini Rally through
**440 km/h / 280 mph** for Regera. Enter both limits, or leave both blank in the
editor to inherit the donor gauge. These limits change the gauge, not engine
power, actual top speed or the `TOPSPEED` menu rating. In the workshop, use
**Original car speedometers…** to change original sport and Koenigsegg HUDs.
Koenigsegg, Viper and Lotus default to 800 km/h / 500 mph. Their original physics
and interior gauges stay unchanged. Blank original limits restore the retail HUD.
The numeric speed readout is hidden in every HUD layout; analog dials and
their configured ranges remain active.

The installer backs up affected game archives and reuses those originals when
reapplying, so edits do not accumulate. It does not edit saved profiles. To
remove the pack and restore the original game data, close the game and run:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --restore
```

## Options

| Variable | Default | Effect |
|---|---|---|
| `RECOMP_PB_EXEC` | off | Execute the push buffer on the D3D11 renderer. **Required** for the game to draw. |
| `RECOMP_FB_WINDOW` | off | Open the game window. **Required.** |
| `RECOMP_USB` | off | Emulate the USB controller ports so pads (XInput) reach the game. **Required** for input. |
| `RECOMP_APU_DSP_ACK` | unset | DSP acknowledgement address(es) the audio needs; use `0x80458810`. |
| `RECOMP_KEYBOARD` | off | `1`: keyboard as a pad (arrows drive/navigate, Z X A S = A B X Y, Enter = Start, Backspace = Back; MM3 layout also maps Escape to B/menu back). |
| `RECOMP_KEYBOARD_LAYOUT` | `mm3` | Arrow driving controls; `legacy` restores the original D-pad-only arrows and 1/3 trigger keys. |
| `MM3_PERFORMANCE_PROFILE` | `Balanced` | Shared rendering defaults for launcher and direct executable; `Quality` uses 1440 lines and 8× MSAA, `LowPower` uses 720 lines and a 30 FPS cap. Individual settings override it. |
| `RECOMP_FPS_LIMIT` | profile: `60` or `30` | Presentation and emulated-vblank cap, 1–60. Uncapped, invalid, or higher values use 60. |
| `RECOMP_VSYNC` | `0` | Present sync interval passed to DXGI. |
| `RECOMP_FULLSCREEN` | off | Start fullscreen. |
| `RECOMP_WINDOW_SIZE` | auto | Initial window client size, `WxH`. |
| `RECOMP_ASPECT` | `16:10` | Pin the display aspect: `16:10`, `16:9`, `4:3`, or a number. `auto` follows the window. Widescreen UI follows it. |
| `MM3_FOV_DEGREES` | unset | Driving horizontal FOV at the display aspect, 60–110 degrees; unset or invalid values retain the existing widescreen FOV. Set with `scripts\play.ps1 -Fov 100`. |
| `RECOMP_SUPERSAMPLE` | `2` | In auto resolution mode, render N×N pixels per window pixel (1–4). |
| `RECOMP_RENDER_SCALE` | profile: `2.0` | Internal height as a multiple of 480 lines (0.5–8): Balanced 960, Quality 1440, LowPower 720; `2.25` = 1080. `auto` follows the window and uses supersampling. |
| `RECOMP_MSAA` | profile: `4` | MSAA sample count (1/2/4/8); 1 disables it. Quality uses 8; LowPower uses 1. Unsupported counts fall back. |
| `RECOMP_ANISO` | `16` | Anisotropic filtering (1–16). |
| `RECOMP_DRAW_DISTANCE` | profile: `2` | View/LOD distance multiplier (1 = Xbox, up to 4). LowPower uses 1. |
| `MM3_PARTICLE_MAX_OVERDRAW` | `24` | Particle overlap budget at extended draw distances, 1–255. The Xbox distance retains its original budget of 12. |
| `RECOMP_SHARP_POINT` | `1` | `0`: plain point filtering for fonts and menu art instead of sharp outlines. |
| `RECOMP_PS_SPEC` | `1` | `0`: skip specialised pixel shaders (slow; for debugging). |
| `RECOMP_PS_PREWARM` | `1` | Prepare previously used pixel shader states during startup; `0` disables learning and prewarming. |
| `RECOMP_SHADER_CACHE` | `1` | Save compiled shaders between launches; `0` disables the cache. |
| `RECOMP_SHADER_CACHE_DIR` | built-in | Shader cache folder; defaults to `%LOCALAPPDATA%\xboxrecomp\ShaderCache`. |
| `MM3_SAVE_DIR` | built-in | Explicit save root for UDATA/TDATA, including mounted U:/T: and direct HDD paths. |
| `MM3_UNLOCK_ALL_CARS` | off | Exactly `1` makes every installed car selectable; otherwise earned availability applies to original cars. |
| `MM3_DEBUG_TELEPORT` | off | Enable the teleport pipe (`tools\powershell\Send-MM3Teleport.ps1`). |

### Automation and measurement

| Variable | Effect |
|---|---|
| `RECOMP_PAD_LIVE=<file>` | Read pad input appended to a file, one `button:hold_ms` per line (e.g. `a:150`). |
| `RECOMP_PAD_SCRIPT=<spec>` | Timed scripted pad input (format in `src/usb/usb_gamepad.c`). |
| `RECOMP_PAD_PRESS=<mask>[,period,hold]` | Pulse digital buttons (`0x10` = Start) periodically. |
| `RECOMP_FRAME_CSV=<file>` | One row per flip: QPC time, frame time, pacing time, and successful DXGI Present return time/interval. Summarise flip intervals with `tools\xboxrecomp\tools\bench\frames.py`. |
| `RECOMP_PACE_AFTER_PRESENT` | `1`: restore the previous wait-after-submission order for timing comparisons. |
| `RECOMP_D3D_PROFILE` | Print per-second fps, worst frame and renderer stage costs to stderr. |
| `RECOMP_KERNEL_SUMMARY` | `1`: collect and print periodic kernel-call totals (off during normal play). |
| `RECOMP_CS_COUNTERS` | `1`: count critical-section entries/exits for contention diagnostics (off during normal play). |
| `RECOMP_HITCH_TRACE` | `1`: report slow command submissions, shader compilation, GPU waits, and file reads with QPC timestamps (off during normal play). |
| `RECOMP_PB_CAPTURE=<prefix>` | Diagnose the first bounded-parser limit by saving fetched command words, parser state, and guest RAM; disabled during normal play. |
| `RECOMP_PRESENT_CAPTURE=<prefix>` | With `<prefix>.flag` containing N, save the next N frames as `<prefix>-*.bmp`. |
| `RECOMP_CAPTURE_SURFACE` | Capture at internal resolution instead of the window. |
| `RECOMP_XA2_PCM_DUMP=<file>` | Dump the mixed audio (s16 stereo 48 kHz). |
| `RECOMP_WATCHDOG_SECS=<n>` | After n seconds of running, report where the game thread is (for hangs). |

`tools\bench\run.ps1 -Exe <exe> -Save <save template> -Profile Balanced -Tag <unique-name>`
runs a frontend and stationary gameplay benchmark. It copies the save, pins the
profile settings, creates a separate shader cache, and records the executable
hash, effective settings, player positions, frame intervals and process CPU.
`tools\bench\summarize.py` reports guest-flip and host Present submission intervals
separately; neither measures when a frame reaches the display. The run rejects
startup/scene failures and waits for complete CSV persistence before stopping.
Omit `-Exe` to use the current build. Optional `-X <x> -Z <z>` requests a verified
teleport; otherwise it measures the saved scene's spawn. Use the same save,
settings and coordinates for comparisons. `-ShaderCacheSeed <directory>` copies
an identical cache snapshot into each run; without it, each cache starts empty.
The frontend scene is selected by timed input and is not verified as a specific
menu. Traffic, scene timing and other system activity still affect comparisons.
`tools\powershell\Test-MM3AiStart.ps1 -Exe <exe> -Save <save template> [-FpsLimit 60]` takes the
same path into the first race and checks every AI car leaves the line forwards (exit 0 = pass).

Compiled shaders are cached automatically. New specialised gameplay shaders
compile in the background. The runtime remembers up to 512 exact shader states
and prepares them during later startups using the existing two background
workers; shaders requested by current draws take priority. The first visit to
a new state still uses the generic shader while compilation finishes. Changes
to shader source or compiler settings invalidate the matching cache entries.
Disabling the shader cache also disables state learning and prewarming.

The renderer selects the high-performance GPU on laptops with integrated and
dedicated graphics. Guest workers and shader compilation run on separate host
threads; rendering keeps the ordering that the game expects. Texture conversion
uses contiguous tile/block copies, and repeated guest locks reuse a thread-local
lookup. Audio DMA also reuses successful address lookups per thread, avoiding
allocator locks while preserving gaps and newly allocated ranges. Optimized
MSVC builds allow automatic inlining in the DSP interpreter; positional-audio
filtering reuses ring indices while retaining the same floating-point operations.
Audio processing skips idle interrupt handling and selects the fractional-delay
tap loop once per channel, preserving exact filter state and output samples.
Optimized MSVC builds request inlining for the checked DSP instruction fetch helper; debug
builds retain its function boundary and memory checks.
`MM3_DSP_AUTO_INLINE=OFF` restores the compiler's default inlining setting for
build comparisons. Normal play skips shared diagnostic counters and redundant
GPU flushes.
Draws retain fixed vertex resources and update vertex shader bindings only when
the pass changes. Repeated texture stages reuse validated sampler-table entries
and use the mip count already computed during texture lookup.
Pending native PCM uses a bounded circular queue instead of shifting the backlog
on every consumption. Its storage remains capped at 64 KiB; geometric growth
preserves queued frames through wrapping and allocation failures. Custom HUD
dials cache their fixed background/tick directions per guest thread and reuse
the needle's trigonometric results without changing geometry or guest state.
Large texture-content buffers use native vector integer arithmetic when both
the CPU and Windows support it. Hash values and byte coverage match the scalar
implementation exactly; small shader keys retain their scalar path.
Specialized shader lookup reuses the previous exact key and hashes changing keys
in four lanes; full key comparisons still resolve collisions and pending shaders
keep their compilation priority. Vertex program and constant versions advance
only after successful GPU uploads. Failed uploads skip the draw and retry, so
unchanged versions cannot leave stale shader data in subsequent frames.
Pixel shader constants reuse their last successful upload only when all bytes,
including dynamic lighting, fog, alpha and texture scales, are identical.
GPU readbacks use one completion query per copy batch before mapping the pixels;
query failures retain the original per-surface polling path. Pushbuffer packets
and called command lists pause at the published PUT position and resume with
their packet and return state intact. A bounded parser pass cannot acknowledge
commands it has not consumed.
MM3 fences complete from executed semaphore-release commands after pending
readbacks finish. Advancing the game's next-fence counter does not complete
an unsubmitted command.
Both queue-pointer views follow actual command consumption. GPU notifications
become visible after their matching consumed position and readbacks, so a ring
wrap cannot wake the producer with a read pointer from the previous lap.
Native waits retain the game's final queue and GPU-idle checks.
Deferred callbacks and due timers acquire the runtime's DISPATCH exclusion
before being removed from their queues. Cancellation while a guest thread
holds DISPATCH can therefore still find the pending work; the worker cannot
retain a canceled callback while waiting for permission to execute it.
Interrupt synchronization callbacks in the kernel bridge use the same modeled
ISR exclusion and IRQL as interrupt delivery, restoring the caller's IRQL afterward.
Concurrent guest workers publish and remove native handle tokens under a short
table lock, with pointer-sized acquire/release access for resolution. Native
file operations and waits run outside that lock.
Guest CRT reservations and ordinary heap blocks now retain exact,
non-overlapping ownership, including addresses above Xbox RAM. Allocations
skip occupied ranges without claiming alignment gaps or overwriting another
allocator's data.
MM3 enables NT heap reclamation before startup so scene destruction returns
large car-model buffers to the guest heap. Repeated race/menu changes reuse
those buffers instead of exhausting the heap. Facade detail fades also retain
their retail proportions when draw distance is extended.
Facade vertex buffers grow at allocation time with extended draw distance,
using the same increased counts for their real storage and allocation limits.
The default 2x distance receives four times the original capacity; growth is
capped at four times to preserve the guest's 16-bit vertex offsets. The Xbox
distance and custom buffer capacities retain their original sizes.
The hot city visibility test uses native scratch variables while preserving
the original floating-point operations and guest state. Its differential
regression compares 200,000 complete state and memory cases with the generated
Xbox function: `python tests/mm3_visibility_regression.py` after entering the
build environment. The facade fade contract is checked with
`python tests/mm3_facade_fade_regression.py`.
The buffer creation and allocation contract is checked with
`python tests/mm3_facade_capacity_regression.py`.

At startup, the game cache helper compares `Data_hd.zip` contents and retail
creation-time metadata with the disc source. A mismatch invalidates the cached
archive so the game rebuilds it, including a byte-identical copy whose creation
time fails the retail check. Fully matching archives are retained. The helper
does not rewrite archive timestamps. Native file opens preserve the guest's
access and sharing flags and report failures without retrying or closing
another owner's handle. Optional file diagnostics can record sharing-conflict
evidence; an external lock can still cause a disc-error screen.

Collision-call fallbacks preserve the argument block reused by the following
helper, preventing a second stack cleanup from consuming saved registers.
Supported DXT textures upload their original compressed blocks for GPU decoding,
reducing upload traffic and texture memory. Short mip chains retain CPU conversion
and generated mip levels. Native BC decoding follows Direct3D's precision rules
and can differ slightly from the integer CPU decoder.
GPU completion wakes waiting guest threads instead of keeping a CPU core busy;
the same command, notification, and fence completion checks still apply.
Display pacing keeps its one-millisecond spin tail. If the high-resolution
timer fails, it falls back to an ordinary timer or short sleeps; these fallbacks
can wake late on a coarse-resolution system, but avoid a full-frame busy spin.
These reduce CPU work without lowering resolution, filtering, or draw distance.
Frame-time benchmarks exclude screenshot capture, which adds a GPU readback and
disk writes that can create spikes of its own.
Extended draw distance uses a particle overlap budget of 24, instead of raising
the Xbox's 12 to the maximum 255. This bounds smoke, spray and collision effects;
the title's explicit particle settings still take priority. Use
`MM3_PARTICLE_MAX_OVERDRAW=255` to restore the previous effects budget.
Frame pacing waits after composition and immediately before DXGI Present, so
variable game and rendering work does not shift each submission within the
frame interval. Each guest flip waits once, including missing-surface and failed
presentation paths; CPU scanout and movie-only ticks remain unpaced.
CSV `present_ms` records intervals between successful DXGI Present returns for
guest flips, excluding movie-only ticks and failed submissions.
Compare it with `frame_ms` when investigating uneven presentation. Neither
column measures when the monitor actually displays a frame.

### Performance review (2026-10-11)

The first code-review assessment in this audit was **7/10 for implementation, 7/10 for
efficiency and 6/10 for maintainability**. These are engineering judgments,
not benchmark-derived scores. Native recompilation, bounded caches, asynchronous
shader compilation, sleeping GPU waits and focused guest-ABI tests are strengths.
The large compatibility/renderer modules and unresolved gameplay hitches limit
the assessment; this pass does not establish a broad FPS increase.

| Improvement | Verified result | Scope of evidence |
|---|---|---|
| Circular pending PCM queue | 64 to 7 growth allocations; no backlog shifts | 100,000 saturated consume/refill operations: median 61.689 to 3.820 ms over five runs; maximum queue remains 64 KiB |
| Cached custom-dial geometry | 198 to 2 trigonometric calls per warm dial | Bit-identical 2,260,900-byte quad streams across 82 scenarios; guest state preserved |
| Deferred-work claim ordering | Canceled DPCs/timers cannot escape a raised-IRQL owner's cancellation | Both tests fail before and pass after the fix; uncanceled callback arguments and stack recovery preserved |
| Interrupt synchronization | Callback excludes modeled ISRs and restores IRQL | Both regressions fail before and pass after; callback context, Boolean result and guest stack preserved |
| Benchmark runner | Isolated saves/caches, pinned settings, validated gameplay and complete CSV | Launch-failure environment restoration, safe copy boundaries and summary interval checks |

Validation passed 32 renderer tests, 16 DSP tests, 22 kernel-dispatch tests,
four benchmark tests, and the custom-dial/readout regressions. The production
RelWithDebInfo build succeeds. The packaged runtime patch applies to the pinned
toolkit and reproduces the working source after Git newline normalization.

The delivered build completed a 30-second Balanced stationary gameplay window
at the same saved spawn, with isolated empty shader caches in both runs:

| Build | FPS | Frame p99 | Maximum interval | Process CPU | Hitches over 2x median |
|---|---|---|---|---|---|
| Preserved baseline | 59.97 | 17.41 ms | 20.65 ms | 13.66% | 0 |
| Delivered build | 60.00 | 17.39 ms | 21.59 ms | 13.41% | 0 |

These are single capped windows with changing traffic, not proof of a general
FPS or CPU improvement. Three additional baseline startup checks and three
checks with the PCM/HUD/DPC changes, without the graphics experiment, completed
successfully. The delivered build also includes the interrupt synchronization
fix. This limited validation cannot establish that every startup failure is fixed.

A fixed graphics-state retention experiment passed pixel/state regressions and
reduced unchanged-draw calls from 12.59 to 3.32 ns in a microbenchmark, only about
0.0074 ms per 800 draws. Builds containing it repeatedly failed startup in deferred
DirectSound voice work, despite one completed gameplay run. No cache defect was
identified, and its connection to the failure remains uncertain. The experiment
is excluded from the delivered source and build while that issue is investigated.
Its completed gameplay run averaged 59.71 FPS, with a 160.82 ms hitch; the initial
matched baseline averaged 59.97 FPS without that hitch. This is not evidence of
a whole-game performance increase.

The synchronization races above are independently proven; neither establishes
the cause of the DirectSound voice-lifecycle failure. Startup reliability and
gameplay hitches remain priorities. Further performance work should measure draw
setup, uploads and submission separately, then examine surface synchronization,
texture hashing and the GPU vertex-program interpreter in busy driving scenes.
Texture eviction rescans and quadratic HUD grouping remain lower-priority
candidates until profiles show a cost. The follow-up below addresses temporary
mip buffers and the surface synchronization upload-failure path.

Local measurements, preserved before sources/executable, geometry streams and
regression logs are retained under `conformance_tmp/performance-audit-20261011`;
full-game run evidence is under `conformance_tmp/bench`. These generated artifacts
are excluded from source control.

### Performance follow-up (2026-10-11)

The revised code-review assessment is **7.5/10 for implementation, 7.5/10 for
code efficiency and 6/10 for maintainability**. These provisional judgments
credit the verified local changes and failure handling; they are not a measured
whole-game speedup.

This pass preserves the earlier changes and improves three specific paths:

| Weak point | Change | Verified result |
|---|---|---|
| Repeated audio volume exponentiation | Reuse a 4,096-entry attenuation table per APU; rebuild if the audio thread's rounding mode changes | 8,388,608 conversions: median 53.435 to 5.627 ms over five trials (9.5x for this operation); exact float and PCM comparisons for all 65,536 input encodings under all four rounding modes |
| Temporary generated-mip buffers | Reduce each mip in place in the existing decode buffer | Two temporary allocations and a copy removed per applicable upload; 1,400 to zero temporary allocations across 700 updates; isolated 128x128 mip generation 0.0603 to 0.0296 ms |
| Surface shadow committed before upload success | Stage guest bytes and commit them only after successful submission; retry failed resources and constants | 38 surface regressions cover exact pixels, injected failures, same-epoch retry, writes during submission, multiple surface sizes and release, and 1x/4x MSAA |

The audio table adds 16 KiB per APU. Surface synchronization retains one reusable
raw snapshot for changed surfaces, with extra copying on that path; unchanged
surfaces allocate, copy and upload nothing. This is a correctness improvement,
not a claimed synchronization speedup. Full texture-upload timings overlapped
between builds, so the mip microbenchmark does not establish a general upload
or FPS gain. Ten surface failure regressions fail against the preserved original
renderer and pass with the fix.

The benchmark runner now accepts `-Scenario Driving`, which holds the throttle
for the measurement window and verifies that the car moved. It retains the input
script, scenario, executable hash, copied-save identity and start/end positions.
The default remains `Stationary`. Driving uses repeatable inputs, but traffic
and collisions can change the route; compare its results separately from the
stationary window.

Validation passes 72 renderer tests, 17 DSP tests and four benchmark tests. The
RelWithDebInfo game build succeeds, and the refreshed runtime patch reproduces
the working runtime from its pinned toolkit revision. Local source snapshots,
microbenchmarks and test evidence are under
`conformance_tmp/performance-followup-20261011`; gameplay runs use the
`followup-*` directories under `conformance_tmp/bench`.

Matched 30-second Balanced stationary windows used the same saved spawn and
isolated empty shader caches:

| Build | FPS | Frame p99 | Maximum interval | Process CPU | Hitches over 2x median |
|---|---|---|---|---|---|
| Preserved follow-up baseline | 60.00 | 17.53 ms | 17.94 ms | 13.22% | 0 |
| Optimized build | 60.00 | 17.41 ms | 20.70 ms | 13.32% | 0 |

Separate 30-second driving windows used the same save, spawn, profile and
throttle input, again with isolated empty shader caches:

| Build | FPS | Frame p99 | Maximum interval | Process CPU | Hitches over 2x median |
|---|---|---|---|---|---|
| Preserved follow-up baseline | 60.00 | 17.29 ms | 20.28 ms | 12.74% | 0 |
| Optimized build | 59.99 | 17.51 ms | 23.94 ms | 12.77% | 0 |

These single capped windows do not establish an overall FPS, CPU or frame-pacing
improvement. The remaining priorities are the intermittent deferred-voice
startup crash, reproducible long driving hitches, and profiling of draw setup,
surface synchronization, texture hashing and vertex interpretation in busier
scenes. Large renderer and compatibility modules still limit maintainability.
Texture eviction restructuring is deferred until measurements show that the
768 MiB texture budget is exceeded; avoiding scans would otherwise add
bookkeeping to ordinary cache hits without demonstrated benefit.

### Stutter and crash investigation (2026-10-11)

This pass prioritizes deferred callbacks, timer scheduling and thread memory.
These are correctness faults that can create extra work or corrupt state; they
are more urgent than small renderer optimizations.

| Weak point | Repair | Evidence |
|---|---|---|
| Pending DPC completions could outlive the protected object's cleanup | Drain queued completions when a guest lowers IRQL below DISPATCH, while retaining processor exclusion | Lifetime, ordering, nested-callback and boundary regressions fail before and pass after |
| Positive absolute timer deadlines fired immediately | Convert against guest system time; round relative deadlines up safely | A synthetic audio rearm scenario falls from 200 callbacks to one for the same action over two seconds |
| Timer signaling happened after callbacks | Publish the signal before the callback; clear it before publishing a rearmed timer | Rearm and retired-storage regressions pass |
| Canceled DPC entries kept consuming queue slots | Compact the pending FIFO when removing work | 1,000 insert/cancel cycles preserve capacity and callback order |
| The 0x40-byte thread header overlapped guest TLS | Reserve the full 0x280-byte KPCR and initialize each thread's IRQL/DPC state independently | Layout and inherited-state regressions fail before and pass after |

Synchronous callback execution preserves general registers, x87/MMX/XMM state,
direction flags and the host floating-point environment. It leases an independent
guest stack for each batch, with an owned allocation if the worker pool is full.
An initial implementation reused the interrupted stack and caused two startup
timeouts in three trials. A trace identified a USB request callback overwritten
by the return address `0035FBD3`; the regression reproduces that overwrite, and
the isolated-stack version passes three out of three startup trials. Concurrent
pool-exhaustion coverage also rejects sharing a fallback stack between batches.
The normal path performs no guest heap allocation, and an empty queue skips the
stack lease and state snapshot.

The historical DirectSound failure is consistent with completion work observing
a voice after its destruction changed its vtable. The repaired ordering prevents
that tested lifecycle pattern, but the exact intermittent crash was not reproduced
in this pass. Existing skipped USB/null indirect-call warnings occur in both
baseline and repaired runs; their original cause remains unresolved. This is
still a multithreaded approximation of the guest processor, not a complete
single-CPU scheduler.

The final kernel suite passes **43/43** tests; the memory suite passes **5/5**.
The RelWithDebInfo game builds successfully. Evidence, preserved executable/PDB
pairs and unsuccessful diagnostic variants are under
`conformance_tmp/stability-20261011`, `conformance_tmp/dpc-lower-20261011`,
`conformance_tmp/kernel-lifecycle-20261011` and `conformance_tmp/bench`.

Both 90-second Balanced driving runs completed startup and loading from the
same copied save and spawn, using isolated empty shader caches and the same
throttle input:

| Build | FPS | Frame p99 | Maximum interval | Process CPU | Hitches over 2x median |
|---|---|---|---|---|---|
| Preserved stability baseline | 59.80 | 17.76 ms | 68.26 ms | 12.10% | 4 |
| Repaired build | 59.95 | 17.69 ms | 30.79 ms | 15.16% | 0 |

The final run had fewer large spikes, but this single comparison does not
establish causation. Traffic/collisions produced different routes: the baseline
ended around 212 draws per frame and the repaired run around 1,435. The higher
CPU figure therefore cannot isolate callback overhead or establish a CPU
efficiency improvement. The baseline's longest gap included 41 ms between
readback polls, without a correspondingly slow individual readback call.
The repaired run's longest frame includes 16.48 ms inside readback processing;
the existing probe does not separate query, map, pixel copying and unmap costs.
A 56.17 ms menu-to-race transition gap also remains outside the driving window.
These residual stalls need more precise attribution before changing GPU
completion behavior.

A second final-build run, with extra hitch tracing disabled, completed startup,
loading and 120 seconds of driving without an observed crash: **59.96 FPS**,
**17.53 ms p99**, **26.76 ms maximum**, and **zero intervals over twice the
median**, at 14.29% process CPU. Both final-build runs verified movement of the
live player car. These are bounded checks of one saved scene, not exhaustive
coverage of every vehicle, map, mode or intermittent failure.

`tools/bench/run.ps1 -HitchTrace` enables optional renderer, readback, submission,
ACK-worker and DXGI Present timing. Normal runs leave these extra timing probes
disabled. Background shader compilation and a pending readback's elapsed time
must not be mistaken for time actively spent blocking the render thread.

### File and graph-call repairs

The original graph-edge lookup ends through shared tail `0x00144ED9` with
`POP EDI; POP ESI; RET 4`. Its unresolved fallback omitted both pops, which
corrupted the outer route helper's saved registers even though `LEAVE` restored
the final stack pointer. `mm3_runtime_entry_repairs.json` now recovers this exact
tail through the pinned translator. The graph regression compares the real
lifted lookup and its caller with the original XBE instructions.

Non-code indirect-call diagnostics now decode the actual call operand and
record its object/register state. A null target previously matched the first
zero in an unrelated startup table, falsely naming `0x00392834` as its slot.
Diagnostic reads tolerate inaccessible memory, and normal logging remains
limited to 50 reports.

File diagnostics distinguish `NOT_FOUND` and `EOF` from other errors. Host
open errors belong to the calling thread and successful opens clear old errors.
The guest still receives the original NTSTATUS and byte counts. The car cache
and platform-directory misses in the supplied log precede the Shared fallback;
missing `rats.dps` and `ScoreImage.xbx` were also absent from the retail disc.
These probes do not require replacement game assets.

After entering `scripts/enter-env.ps1`, run
`python tests/mm3_graph_edge_regression.py` and
`python tests/mm3_icall_diagnostics_regression.py`. The toolkit's
`kernel-file-sharing` regression checks concurrent error provenance, malformed
open requests, genuine absence and EOF.

### Race map guidance

The yellow route follows the native Paris and Washington street splines and
their connected junctions. The brighter leg leads from the car's current road
position to the next checkpoint and recalculates after moving four metres or
changing checkpoints, including after a missed turn. Height helps distinguish
bridges and roads below them. Checkpoint races keep their closing lap, while
Blitz courses end at their final checkpoint.

Road geometry is copied after native city loading and cleared on unload.
Unavailable or disconnected paths are omitted. The line uses the same map
projection, rotation, zoom and viewport clipping as the checkpoint markers.
Drawing retains bends within a quarter pixel while combining straight samples,
so expanded maps avoid a native draw submission for every six metres of road.
After entering `scripts/enter-env.ps1`, run
`python tests/mm3_race_guidance_regression.py` for routing, city-asset, native
spline, projection and submission checks.

### Tree contact response

Tree contacts use the original solid-contact and penetration correction path
for the initial impact. At or above 18 km/h, native object removal clears the
struck tree's collision and cached contacts in the striking car. Breaking it
also removes a little forward car speed once (up to 8%, capped at 1.5 m/s),
preserving the native lateral and vertical response. A render-only override
keeps that instance's original model visible: it tips in the impact direction
over 0.55–0.9 seconds depending on impact speed, slides 0.25–1.25 metres,
settles after a small landing rebound, then sinks away between
12 and 14 seconds. Slower contacts remain solid. Animation freezes in the pause
menu; city changes, cell reloads and reused instances invalidate the effect.
The retail upward-hop response is bypassed for trees. Native metadata identifies
the selected tree contact; other roadside props retain their original response.
Leaf effects and impact sounds still run. Falling uses the existing tree model
and pivots at its base, so it also works without separate destruction assets.
Falling instances use the full mesh throughout their lifetime: the native tree
sprite ignores tilt and can otherwise leave an upright copy beside the fallen
model during detail transitions. Other trees retain their normal detail levels.

The build applies a checked seam at the selected-contact decision in `0x001D137B`;
regeneration that changes or duplicates that boundary fails configuration.
After entering `scripts/enter-env.ps1`, run
`python tests/mm3_tree_contact_regression.py` for native/original-XBE contact
and object-removal comparisons, and
`python tests/mm3_tree_diagnostics_regression.py` for guarded trace contracts.
`python tests/mm3_tree_fall_regression.py` checks speed-scaled directional falling,
bounded travel with a grounded base, render-only visibility, matrix restoration
and scene lifetime guards.
It also executes the native renderer with both tree mesh queues and reproduces
the upright sprite overlap as a negative control.
`MM3_TREE_TRACE=1` adds bounded object-render, collision-event and tree-contact
reports. Tree-contact reports have their own budget so frequent rendering cannot
hide a physical impact.
`MM3_TREE_TRACE=contacts` skips object-render tracing while keeping physical
contact and collision-event reports for lighter driving captures.

### Building pop-in capture

Every `scripts/play.ps1` launch now writes a unique `logs/play-*.log` and a
matching `.json`. Both output streams are captured concurrently with UTC line
timestamps, and the log stays readable while the game runs. The JSON records
the executable hash, launch options, runtime environment, profile defaults,
installed car configuration and exit status. Existing environment overrides
still take priority.

Normal launches use minimal logging: focused building, texture-stream and
asynchronous-wait tracing is off by default. Use `-TraceBuildings` to opt in
when investigating pop-in; `-NoTraceBuildings` remains available to explicitly
disable that switch. Existing environment overrides still take priority.
The launcher refuses a second game instance because
both copies would open the same cache exclusively; that conflict otherwise
appears as the title's dirty/damaged-disc screen. Close all game windows before
starting a new run.

For the repeatable upper-building gap in Paris, Blitz **Sacré Leap**, approaching
**Boulevard Malesherbes**, close the current game when convenient and launch:

```powershell
tools\bin\powershell\pwsh.exe -NoProfile -ExecutionPolicy Bypass -File scripts\play.ps1 -TraceBuildings -Exe out\building-diagnostics\RelWithDebInfo\mm3_recomp.exe
```

`Play with building logs.cmd` prefers this separate diagnostic build when present
and otherwise uses the normal setup build. It uses local portable PowerShell when
installed, with Windows PowerShell as a fallback for a fresh checkout.
Add `-Keyboard` when using the keyboard. Reproduce the approach, drive back and
approach again, then quit normally and keep both files from that run. This build
adds evidence; the cause of this specific gap has not yet been confirmed.

`-TraceBuildings` sets three switches only where they are unset, and prints
their actual values at launch:

- `MM3_BUILDING_TRACE=1`: actual near/mid/far render paths, early visibility and
  auxiliary rejection, camera and facade coordinates, closest distance, floor
  cap, per-cell visibility rejection, and geometry queue/counter changes.
  Both 28-byte vertex streams report their native head, tail and capacity,
  before/after read validity, and middle-detail/upper-floor allocation attempts,
  successes, failures and requested quad totals. Checked guest reads are sampled at
  most ten times per second per facade, with 64 change events per second per
  render thread and summaries each second. Allocation failures have a separate
  eight-event-per-second budget and suppressed-failure counts. Counter deltas do not prove that
  an asset is loaded or absent.
- `RECOMP_TEXTURE_STREAM_TRACE=1`: real uploads, changed texture content, upload
  duration, mip counts, cache occupancy, eviction totals and creation failures.
  Cache hits stay quiet; an 80-event-per-second budget bounds output.
- `RECOMP_ASYNC_WAIT_TRACE=1`: wait status, handle resolution, caller, stack and
  relevant USB request snapshots. Each wait kind has separate bounded budgets
  for ordinary results and failures, so early polling cannot hide invalid
  handles. It leaves `RECOMP_TITLE_KEVENTS` unchanged; an explicit trace value
  of `0` disables it even when building tracing is enabled.

Diagnostic summaries flush buffered runtime output each second while active;
wait failures and non-code indirect-call reports flush immediately. The default
launcher keeps normal buffering. Focused tracing adds work and is intended for
these reproduction runs, rather than performance comparisons.

The building and indirect-call contracts run with
`python tests/mm3_building_diagnostics_regression.py` and
`python tests/mm3_icall_diagnostics_regression.py` after entering the local build
environment. Launcher coverage is in `tests/test_play_profiles.py`. The toolkit's
`kernel-async-wait-trace-*` cases verify real HLE wait outcomes and unchanged
guest state, memory and host errors with tracing off/on. Its `nv2a_textures`
suite compares actual D3D11 texture pixels and upload retry behavior with
tracing off/on.

The 2026-10-12 12:00 UTC capture contains a confirmed USB request lifetime
failure: XPP's stack event reaches `KeWaitForSingleObject` as an invalid native
handle, the wait returns immediately, and a later USB completion reads reused
request memory. MM3 registers four verified XPP setter/wait return addresses
for inline event handling. This leaves unrelated title-built GPU events on
their existing default path. The focused native USB event fixture covers both
completion orders, timeout/cancel follow-up, reused event storage and invalid
headers. This startup issue has not been linked to the building gap.

That capture's original `zero_queue_delta` values omit the two 28-byte streams
used by middle detail and the near renderer's upper-floor fallback. They cannot
establish missing geometry. The revised trace measures those streams directly;
invalid reads produce an unknown result. Raw-XBE comparisons of middle-detail
and near/fallback geometry, including capacity exhaustion, match the generated
code.

The 12:26 UTC capture confirms buffer exhaustion behind the upper-building gap.
Facade `04AE20F0` passes all 30 cell visibility tests and renders its bottom ten
cells, while all 20 upper cells fail allocation at the opaque stream's 12,288
vertex limit. Later, the same facade remains in the near path and draws those
20 upper cells when space is available. The capture contains 4,263 sampled
allocation failures, and all 40 priority failure events concern that full
opaque stream. The constructor now increases actual buffer storage and its
matching limits before allocation at extended draw distances. A focused native
regression checks the real guest creation path, byte counts, allocation failure
rollback, unchanged stock/custom sizes and successful allocation of the logged
upper-floor workload. The repaired USB waits also complete successfully in this
capture with intact request callbacks.

### Diagnostics

Off unless set; for debugging the runtime, not for play. See the source named for each.

- **Renderer** (`nv2a_d3d11.c`, `nv2a_pb_exec.c`): `RECOMP_D3D_TRACE`, `RECOMP_D3D_DEBUG`,
  `RECOMP_D3D_NO_WRITEBACK`, `RECOMP_D3D_SKIP_CMASK`, `RECOMP_D3D_SKIP_SFACTOR`,
  `RECOMP_PB_SCAN`, `RECOMP_PB_EXEC_VERBOSE`, `RECOMP_PB_REPORT_MS`, `RECOMP_PB_UNHANDLED_ALL`,
  `RECOMP_PB_WRAP_TRACE`, `RECOMP_NV2A_TRACE`, `RECOMP_FRAME_TRACE`, `RECOMP_FRAME_TRACE_METHODS`,
  `RECOMP_FRAME_TRACE_CONSTANTS`, `RECOMP_TEX_DUMP`, `RECOMP_TEX_DUMP_EVERY`, `RECOMP_TEX_STATE`,
  `RECOMP_TEX_GENERIC`, `RECOMP_DXT_VALIDATE`, `RECOMP_FB_DUMP`, `RECOMP_FB_DUMP_FLIPS`, `RECOMP_FB_VA`,
  `RECOMP_FIND_NAN`, `RECOMP_FIND_QUAD`, `RECOMP_VSH_TRACE`, `RECOMP_VSH_DUMP`, `RECOMP_NO_VSH`,
  `RECOMP_NO_COMBINERS`, `RECOMP_RC_TRACE`, `RECOMP_RC_GENERIC`, `RECOMP_BLEND_GENERIC`,
  `RECOMP_SOFT_RASTER`, `RECOMP_RASTER_TEST`, `RECOMP_RASTER_THREADS`, `RECOMP_PEEK`,
  `RECOMP_PEEK_CHAIN`, `RECOMP_POKE`, `RECOMP_VBLANK`, `RECOMP_VIDEO_FLAGS`
- **Video/movies** (`fb_present.c`, `video_pump.c`): `RECOMP_PVIDEO_TRACE`, `RECOMP_PVIDEO_DUMP`,
  `RECOMP_PVIDEO_FRAME_AUDIT`, `RECOMP_FMV_DUMP`, `RECOMP_FMV_HOST`, `RECOMP_KEY_TRACE`
- **Audio** (`apu_*.c`): `RECOMP_APU_TRACE`, `RECOMP_APU_MIXDOWN_ALL`, `RECOMP_DSP_ACK`, `RECOMP_AC97_READY`
- **USB/input** (`ohci.c`, `usb_gamepad.c`): `RECOMP_USB_HC`, `RECOMP_USB_NDP`, `RECOMP_USB_PADS`,
  `RECOMP_USB_PORT`, `RECOMP_USB_STATS`, `RECOMP_USB_TRACE`, `RECOMP_INPUT_DIAG`
- **Kernel/memory/threads** (`kernel_*.c`, `xbox_memory_layout.c`): `XBOX_LOG_LEVEL`,
  `RECOMP_KERNEL_WATCH`, `RECOMP_KERNEL_WATCH_ALL`, `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_IRQL_TRACE`,
  `RECOMP_CS_MODE`, `RECOMP_CS_WATCH`, `RECOMP_CS_TRACE_CRT`, `RECOMP_WORKERS`, `RECOMP_ASYNC_IO`,
  `RECOMP_CMDLINE`, `RECOMP_CONTIG_TRACE`, `RECOMP_HEAP_WATCH`, `RECOMP_HEAP_LARGE_BREAK`,
  `RECOMP_WATCH`, `RECOMP_WATCH_RAW`, `RECOMP_TRAP_NULL`, `RECOMP_FORCE_RETURN`,
  `RECOMP_THREAD_TLS_TRACE`, `RECOMP_TRACE_ARGS`, `RECOMP_TRACE_BUDGET`,
  `RECOMP_TRACE_DEREF`, `RECOMP_TRACE_PROFILE`
- **Game layer** (`src/`): `MM3_THREAD_MODE=inline`, `MM3_LOG_UNBUFFERED`, `MM3_TRACE_BOOT_STATE`,
  `MM3_ASSET_TRACE`, `MM3_STREAM_TRACE`, `MM3_POST_MOVIE_WATCHDOG`, `MM3_FRONTEND_ICALL_TRACE`,
  `MM3_FRONTEND_WRITE_WATCH`, `MM3_HEAP_FRONTIER`, `MM3_HEAP_FREE_TRACE`, `MM3_HEAP_RETURN_TRACE`,
  `RECOMP_HEAP_LINK_BREAK`, `RECOMP_HEAP_RETURN_BREAK`, `RECOMP_BINK_ALLOC_BREAK`,
  `RECOMP_STREAM_COPY_ORIGINAL`, `RECOMP_UNIMPL_TRAP`, `RECOMP_TRACE_EFD85`

### Build options

CMake options default to a clean build. `MM3_FRONTEND_WATCH`, `MM3_MOVIE_INPUT_TRACE`,
`MM3_TRACY` and `MM3_RECOMP_ABI_CHECK` add diagnostics that cost performance (the first
roughly halves menu frame rate); leave them off for play.
