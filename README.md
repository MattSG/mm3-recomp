# Midtown Madness 3 recompilation

Static recompilation of the Xbox title Midtown Madness 3 to native x86-64 Windows, on the
[xboxrecomp](tools/xboxrecomp) runtime, rendering through Direct3D 11.

**Credits:** This project builds on the original
[mm3-recomp by Matt Graham (MattSG)](https://github.com/MattSG/mm3-recomp) and the
[xboxrecomp toolkit](https://github.com/MattSG/xboxrecomp). Credit for that foundation
belongs to the original author and upstream contributors.

This fork is maintained by [lakshya16157](https://github.com/lakshya16157), with
assistance from OpenAI Codex on performance and runtime improvements, gameplay
fixes, and the custom-car tools.

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
  uploads. Balanced renders at 1536×960 and Quality at 2304×1440, both at 16:10, with
  configurable aspect, resolution, MSAA, anisotropic filtering and draw distance.
- **Efficiency:** persistent shader caching and startup prewarming, less repeated
  texture/shader lookup work, optimized texture conversion and audio processing,
  and GPU-completion waits that let waiting threads sleep.
- **Timing and runtime:** a 60 FPS maximum, presentation pacing immediately before
  Present, command/fence ordering repairs, safer guest-handle access, and save-root
  handling. Graphics memory is reused across scene loads without treating skipped
  heap addresses as occupied. Extended-distance particle overlap is bounded to
  reduce effects load.
- **Driving and selection:** arrow-key driving, camera and manual-shift controls,
  focus-aware keyboard input, and selection of all 35 supported-disc car models.
- **Car workshop:** fourteen extra tuned variants, optional prepared model
  packages, editable paint and driving stats, validated gearing/RPM settings,
  larger HUD and interior speedometer ranges and numeric
  speed readouts for the sport and Koenigsegg HUDs.

The detailed settings and runtime notes below describe these changes and their
limits. See the [car-pack guide](tools/car-mods/README.md) for further tuning examples.

## Building from a fresh clone

You need Windows 10/11 x64, Visual Studio 2022 or newer with **Desktop development with C++**
(it includes CMake), Python 3.11+ (the `py` launcher), git, and your own Midtown Madness 3 disc.

```powershell
git clone --recurse-submodules https://github.com/lakshya16157/mm3-recomp.git
cd mm3-recomp
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
| Balanced (default) | 960 | 2× | 2× Xbox | 60 |
| Quality | 1440 | 4× | 2× Xbox | 60 |
| LowPower | 720 | Off | Xbox | 30 |

All three retain 16× anisotropic filtering. LowPower reduces resolution, edge
smoothing and view distance and caps motion at 30 FPS. Quality restores the
previous rendering settings. Direct executable launches use the same profiles
through `MM3_PERFORMANCE_PROFILE=Balanced`, `Quality` or `LowPower`; unset or
unrecognized values select Balanced. Individual `RECOMP_*` environment settings
take priority. `-FpsLimit 45` selects a different cap when `RECOMP_FPS_LIMIT` is
unset. The launcher restores environment defaults it added when the game exits.

Matched stationary runs on 2026-10-11 used a Ryzen 9 PRO 7940HS (8 cores,
16 threads), an RTX 2000 Ada Generation Laptop GPU with 8 GB VRAM (8,188 MiB
reported by `nvidia-smi`), and 32 GB system RAM. Each profile used Senna GTR in
Paris at the same position, about 132 draws per frame, and approximately 31.8
seconds:

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
and **Enter/Esc** pauses/resumes. **C** changes the camera view while driving. The original
**3** accelerator, **1** brake and numpad **4/6** steering keys still work. Keyboard input
releases when the game loses focus.
Choose **Manual** transmission in car setup, then tap **R** to shift up and **F** to shift down
(the default Xbox B/X gear controls). These aliases operate only while driving.
Set `RECOMP_KEYBOARD_LAYOUT=legacy` to use only the original runtime bindings.

All 35 car models included on the supported disc are selectable, including the hidden bonus
cars. Event-specific vehicle classes still apply. Bonus paints and career statistics continue
to follow profile progress. Set `MM3_UNLOCK_ALL_CARS=0` to restore the original car locks;
the availability change does not write unlock keys to your save.

## Added cars and editing stats

The optional [extra-car pack](tools/car-mods/README.md) adds fourteen separately
selectable slots alongside the 35 original cars: Mini Rally, Mustang GT, Viper
Race, Panoz GTR-1, McLaren F1, Formula 1, Bugatti Chiron, Bugatti Bolide, Ford GT,
McLaren Senna, McLaren Senna GTR, Ferrari FXX K, Koenigsegg Regera and Lamborghini
Aventador. Each has its own name, paint settings, menu ratings and driving setup.

The supplied defaults use stock donor visuals. Workshop's **Model** tab can
select prepared imported body, wheel, texture and menu-icon packages; reapplying
stats preserves that selection. The [model workflow](tools/car-models/README.md)
documents the verified Panoz, F1 GTR, Ferrari F2007, Ford GT, Senna, Senna GTR,
FXX K, Regera and Aventador conversions. Downloaded models and generated packages
are not distributed in this repository. Imported initial liveries remain fixed,
glass is opaque, and interior, sounds and collision remain based on the donor.
Driving settings are game-tuned interpretations rather than measured specifications.

### Visual editor

1. Close the game, then double-click [tools/car-mods/editor.cmd](tools/car-mods/editor.cmd)
   or run `tools\car-mods\editor.cmd` from the repository root.
2. Select an added car and edit its paint, menu ratings, physics, gearing, RPM
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
| `TorqueScale`, `TorqueScaleFactor` | Engine torque and its multiplier. |
| `Mass` | Physical weight. |
| `BrakeTorque`, `HandBrakeTorque` | Braking and handbrake strength. |
| `LatFrictionFactor` | Cornering grip. |
| `AirKZZ`, `AirKZY` | Aerodynamic coefficients. |
| `WheelDrive` | 0: front wheels; 1: rear wheels; 2: all wheels. |
| `SteeringEndSpeed` | Speed-dependent steering setting. |
| `WheelRadius` | Wheel radius, also used in automatic shift-speed calculations. |
| `GearRatioVector` | Reverse ratio followed by two through six forward ratios. |
| `WheelRatio` | Final drive multiplier; a smaller value makes gears longer. |
| `TimeChangeGear`, `TimeChangeGearFR` | Forward shifting and forward/reverse transition times, in seconds. |
| `MinRPM`, `MinRPMGearChange`, `CutRPM`, `MaxRPM` | Idle, minimum shift, cutoff and redline settings. |
| `MaxTorqueRPM`, `MaxTurboTorqueRPM` | Peak torque RPM used to calculate automatic upshift speeds. |
| `GearStatusDownGearFactor` | Automatic downshift factor. |

For example, changing Mini Rally's `physics.TorqueScale` from `440` to `500`
increases engine torque. Changing `ratings.ACCELERATION` changes only its menu bar.
The installer validates supported value ranges before applying changes.

### Gearing and speedometers

Use a negative reverse ratio followed by positive, decreasing forward ratios,
for example `[-3.3, 2.5, 1.72, 1.28, 1.0, 0.82, 0.7]`. Gear count is derived
from this array; do not add `NGears`. Keep
`MinRPM < MinRPMGearChange < CutRPM <= MaxRPM`, and keep both peak-torque RPM
settings above idle and below cutoff. The supported stock torque curve requires
`MaxTorqueRPM` and `MaxTurboTorqueRPM` to be at most 8000 RPM; Formula 1 therefore
uses stock-compatible RPM values rather than a modern F1 engine's RPM range.

Automatic upshifts depend on road speed calculated from gear ratios, final drive,
wheel radius and peak torque RPM (the turbo peak for a turbo-equipped donor).
Wheelspin can raise RPM before the car reaches its shift speed. Lowering
`MinRPMGearChange` alone does not make automatic upshifts happen earlier.
The pack's ratios and RPM settings address the earlier missed-shift issue, and
the installer rejects conflicting thresholds. Select **Automatic** or **Manual**
in the game's car setup; Manual uses **R/F** on the MM3 keyboard layout or Xbox
**B/X** to shift up/down while driving.

The optional `speedometer.max_kmh` and `speedometer.max_mph` values set the
HUD dial and interior dashboard needle range. Fast variants use **800 km/h / 500 mph**;
Mini Rally uses 500 km/h / 311 mph. Enter both limits, or leave both blank in the
editor to inherit the donor gauge. These limits change the gauge, not engine
power, actual top speed or the `TOPSPEED` menu rating. In the workshop, use
**Original car speedometers…** to change original sport and Koenigsegg HUDs.
Koenigsegg, Viper and Lotus default to 800 km/h / 500 mph. Their original physics
and interior gauges stay unchanged. Blank original limits restore the retail HUD.
Sport and Koenigsegg HUDs also show uncapped numeric speed below the dial.

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
| `RECOMP_KEYBOARD` | off | `1`: keyboard as a pad (arrows drive/navigate, Z X A S = A B X Y, Enter = Start, Backspace = Back). |
| `RECOMP_KEYBOARD_LAYOUT` | `mm3` | Arrow driving controls; `legacy` restores the original D-pad-only arrows and 1/3 trigger keys. |
| `MM3_PERFORMANCE_PROFILE` | `Balanced` | Shared rendering defaults for launcher and direct executable; `Quality` restores the previous settings, `LowPower` uses 720 lines and a 30 FPS cap. Individual settings override it. |
| `RECOMP_FPS_LIMIT` | profile: `60` or `30` | Presentation and emulated-vblank cap, 1–60. Uncapped, invalid, or higher values use 60. |
| `RECOMP_VSYNC` | `0` | Present sync interval passed to DXGI. |
| `RECOMP_FULLSCREEN` | off | Start fullscreen. |
| `RECOMP_WINDOW_SIZE` | auto | Initial window client size, `WxH`. |
| `RECOMP_ASPECT` | `16:10` | Pin the display aspect: `16:10`, `16:9`, `4:3`, or a number. `auto` follows the window. Widescreen UI follows it. |
| `RECOMP_SUPERSAMPLE` | `2` | In auto resolution mode, render N×N pixels per window pixel (1–4). |
| `RECOMP_RENDER_SCALE` | profile: `2.0` | Internal height as a multiple of 480 lines (0.5–8): Balanced 960, Quality 1440, LowPower 720; `2.25` = 1080. `auto` follows the window and uses supersampling. |
| `RECOMP_MSAA` | profile: `2` | MSAA sample count (1/2/4/8); 1 disables it. Quality uses 4; LowPower uses 1. |
| `RECOMP_ANISO` | `16` | Anisotropic filtering (1–16). |
| `RECOMP_DRAW_DISTANCE` | profile: `2` | View/LOD distance multiplier (1 = Xbox, up to 4). LowPower uses 1. |
| `MM3_PARTICLE_MAX_OVERDRAW` | `24` | Particle overlap budget at extended draw distances, 1–255. The Xbox distance retains its original budget of 12. |
| `RECOMP_SHARP_POINT` | `1` | `0`: plain point filtering for fonts and menu art instead of sharp outlines. |
| `RECOMP_PS_SPEC` | `1` | `0`: skip specialised pixel shaders (slow; for debugging). |
| `RECOMP_PS_PREWARM` | `1` | Prepare previously used pixel shader states during startup; `0` disables learning and prewarming. |
| `RECOMP_SHADER_CACHE` | `1` | Save compiled shaders between launches; `0` disables the cache. |
| `RECOMP_SHADER_CACHE_DIR` | built-in | Shader cache folder; defaults to `%LOCALAPPDATA%\xboxrecomp\ShaderCache`. |
| `MM3_SAVE_DIR` | built-in | Explicit save root for UDATA/TDATA, including mounted U:/T: and direct HDD paths. |
| `MM3_UNLOCK_ALL_CARS` | `1` | Make every installed car selectable; `0` restores earned car availability. |
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

`tools\bench\run.ps1 -Exe <exe> -Save <save template>` runs the repeatable menu + race
benchmark (frame-time percentiles, lows, jitter, executor time, GPU use).
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
Concurrent guest workers publish and remove native handle tokens under a short
table lock, with pointer-sized acquire/release access for resolution. Native
file operations and waits run outside that lock.
Guest CRT reservations and ordinary heap blocks now retain exact,
non-overlapping ownership, including addresses above Xbox RAM. Allocations
skip occupied ranges without claiming alignment gaps or overwriting another
allocator's data.

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
