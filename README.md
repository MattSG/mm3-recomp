# Midtown Madness 3 PC Native

**Midtown Madness 3 PC Native** brings the Xbox arcade racing game to Windows
10 and Windows 11 x64 through static recompilation. This native MM3 PC port
uses the [xboxrecomp](tools/xboxrecomp) runtime and a Direct3D 11 renderer.

An improved and modded PC version with keyboard controls, widescreen graphics,
custom-car tools, gameplay repairs and performance work. Build it from your own
supported game disc with the instructions below. The
[integration record](docs/integration-2026-10-13.md) tracks the combined source,
feature preservation and validation.

**Credits:** This project builds on the original
[mm3-recomp by Matt Graham (MattSG)](https://github.com/MattSG/mm3-recomp) and the
[xboxrecomp toolkit](https://github.com/MattSG/xboxrecomp). Credit for that foundation
belongs to the original author and upstream contributors.

This fork is maintained by [lakshya16157](https://github.com/lakshya16157), with
assistance from OpenAI Codex on performance and runtime improvements, gameplay
fixes, and the custom-car tools.

Upstream reports that nearly all its project code was written with AI assistance.
This fork also uses AI assistance; reviews, fixes and focused pull requests are welcome.
The runtime derives from [MattSG/xboxrecomp](https://github.com/MattSG/xboxrecomp),
a fork of [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp).
This is an unofficial community project with no affiliation with Microsoft,
Xbox or Digital Illusions CE (DICE).

**Source integration is complete.** The combined build, scoped native/unit checks,
live launcher initialization and default-30-Hz driving smoke passed. See the
dated integration record for measured results and extended coverage.

## Features

The combined source retains upstream's Settings, Controls and Mods launcher,
configurable keyboard/mouse and controller bindings, menu mouse navigation,
intro skipping, required collision-cell repair and console-rate simulation.
The [mod registry](src/mods/mods.c) lists its switches and defaults; required
collision and disc-authentication repairs remain enabled.

The fork adds nine extra-car identities and a standalone Car Workshop editor,
MM2/MM1 exterior conversion, source credits, model/shadow/light/reflection/menu
artwork repair, donor-based driving defaults, analog gauge handling, map zoom,
checkpoint alignment and road-following race guidance. Its runtime work covers
shader caches/prewarming, texture and surface synchronization, presentation
pacing, audio processing and guest memory/handle/fence repairs. Integration
checks for these features are tracked separately in the
[preservation matrix](docs/integration-2026-10-13.md#feature-preservation).

Network support remains experimental. Insignia sign-in and complete system-link
matches have not been established by this integration.

## Build and play

You need Windows 10/11 x64, Visual Studio 2022 or newer with **Desktop development with C++**
(it includes CMake), Python 3.12+ recommended (3.11.4+ supported), git,
and your own Midtown Madness 3 disc.
Examples use PowerShell 7 (`pwsh`). Install it separately or use the portable
PowerShell under `tools/bin/powershell` when it is available.
Initial CMake configuration downloads pinned FreeType 2.13.3 and RmlUi 6.3 for
the settings launcher, so a fresh configure requires network access.

```powershell
git clone --recurse-submodules https://github.com/lakshya16157/midtown-madness-3-PC-Native.git
cd midtown-madness-3-PC-Native
pwsh scripts\setup.ps1 -Iso "D:\Games\Midtown Madness 3 (USA).iso"
.\build\RelWithDebInfo\mm3_recomp.exe  # opens Settings, Controls and Mods
```

`setup.ps1` checks the toolchain, makes `tools\venv` with tested Capstone 5.0.7, verifies the disc and
extracts it to `game_files\`, generates the translated C into `src\recomp\gen\` (about six
minutes, deterministic) and builds `build\RelWithDebInfo\mm3_recomp.exe`. Re-running skips
finished steps; `-Force` redoes them. Nothing from the disc is committed.

This fork distributes its additional runtime fixes in `patches/xboxrecomp-mm3.patch`.
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

Generated Xbox translations and game assets stay local. Setup validates and extracts
your own disc; compiled game code and source-model downloads are not bundled.
The runtime commit is pinned by the submodule and the additional patch is
version-specific. See the integration record before moving that pin manually.

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

Use `scripts\generate_mm3.py` for regeneration. The supported recipe uses the
recompiler pinned to toolkit commit `967ec30`, extracted to `tools\.generator\`.
That generator pin is separate from the runtime submodule pin. Changing
`MM3_GENERATOR_DIR` selects a different generator and requires translation and
gameplay validation; the integration does not establish that arbitrary regenerated
output is compatible with the game's native hooks.

An existing pre-integration checkout with old files in `src\recomp\gen\` must
regenerate once for the integrated hooks. Fresh-clone setup generates them
automatically; setup otherwise skips an existing generated directory. Run the
generator directly, then rerun setup to build:

```powershell
. .\scripts\enter-env.ps1
python -m pip install capstone==5.0.7
python scripts\generate_mm3.py
pwsh scripts\setup.ps1
```

This replaces local translated code while keeping the existing `game_files\`
and installed car assets. Use this procedure without `setup.ps1 -Force`, which
also re-extracts the game files. A checkout already populated with the tested
integrated generated files is ready to build without regenerating again.

## Launcher and controls

Starting the executable normally opens the **Settings**, **Controls**, and
**Mods** launcher. Settings save to `mm3_settings.ini` beside `game_files/` when
you press Play. Keyboard and controller bindings are configurable there.

- **Launcher:** Enter plays, Esc quits; arrows, Space, and Tab navigate.
  A controller also works: D-pad/stick to navigate, A to change, LB/RB for tabs,
  Start to play, and Back to quit.
- **Keyboard play:** enable keyboard and mouse under Controls. Defaults include
  arrows for D-pad, Z/X/A/S for A/B/X/Y, Enter for Start, Backspace for Back,
  and numpad 8/2/4/6 for the left stick.
- **Menu mouse:** enable menu keys and mouse under Controls. Point to select,
  click or Space to confirm, and right-click or Esc to go back.
- **Fullscreen:** F11 or Alt+Enter toggles it in game.

For a direct launch that skips the settings window:

```powershell
pwsh scripts/play.ps1 -Keyboard -Profile Balanced
```

Console-rate simulation defaults to **30 Hz**, with **60 Hz** available under
Settings > Simulation. It is separate from the rendering frame limit.
Balanced and Quality profiles default to **60 FPS**; LowPower defaults to
**30 FPS**. The launcher offers 30, 60 and 90 FPS, and `play.ps1 -FpsLimit`
accepts 1–90. Upstream reported race AI reversing at starts above about 110 FPS,
so those rates are excluded. Final driving smoke observed about 60 FPS at a 60 limit and
72 FPS at a 90 limit; it does not establish achieved 90 FPS or matched handling
parity. Lower supersampling or internal resolution if performance is poor.

Profiles are applied before the settings launcher so saved Auto resolution,
aspect and FOV selections can take effect. The launcher includes 16:10 aspect
and 720p internal rendering in addition to upstream's choices.

The launcher saves custom mappings; [input runtime](tools/xboxrecomp/src/input)
implements them. The fork's gameplay aliases and focus handling are preserved
in the integration target, with their precedence against saved mappings
tracked in the validation matrix. To select custom cars and model packages,
use the separate Workshop described below.

## Fork gameplay options

The fork's MM3 keyboard layout provides arrow-key driving, C to change camera,
Space for handbrake and R/F for manual shifting while driving. Ctrl enlarges the
race map; its checkpoint markers remain aligned and the road-following route
updates toward the next checkpoint. Saved launcher bindings can customize input;
use the integration record for verification of remapping and default aliases.

The closest chase camera retains its 0.25 m adjustment. Driving FOV and draw
settings are available through the launcher. Direct launches retain these options:

```powershell
pwsh scripts/play.ps1 -Profile Balanced -Keyboard -Fullscreen -Fov 100 -FpsLimit 60
```

`-Profile` accepts Balanced, Quality or LowPower. `-Fov` accepts 60–110 degrees
of absolute horizontal driving FOV. `-TraceBuildings` enables focused diagnostics;
`-NoTraceBuildings` disables them. Normal play leaves focused tracing off.
The script preserves its logged launch settings and explicit environment overrides.

The original 35 cars keep their earned availability rules. `MM3_UNLOCK_ALL_CARS=1`
makes installed cars selectable without writing unlock keys to a profile.
`MM3_SAVE_DIR` selects an isolated UDATA/TDATA save root for test builds. Close
MM3 before applying Workshop changes and keep a copy of your normal saves when
trying an integrated build.

## Added cars and editing stats

The optional [extra-car pack](tools/car-mods/README.md) adds nine separately
selectable slots alongside the 35 original cars:
Panoz GTR-1, McLaren F1, Formula 1, Ford GT,
McLaren Senna, McLaren Senna GTR, Ferrari FXX K, Koenigsegg Regera and Lamborghini
Aventador. Each has its own name, paint settings, menu ratings and driving setup.
The [model names, download sources and creator credits](tools/car-models/README.md#extra-car-names-and-model-credits)
identify each imported exterior and its original conversion pack.
The remaining slots retain their IDs. Profiles that remember a removed custom
car use its original donor vehicle, without restoring the removed menu entry.

The supplied defaults use stock donor visuals. Workshop's **Model** tab can
select prepared imported body, wheel, texture and selection-artwork packages; reapplying
stats preserves that selection. The [model workflow](tools/car-models/README.md)
documents the verified Panoz, F1 GTR, Ferrari F2007, Ford GT, Senna, Senna GTR,
FXX K, Regera and Aventador conversions. Downloaded models and generated packages
are not distributed in this repository. Imported initial liveries remain fixed,
glass is opaque, and interior, sounds and collision remain based on the donor.
The [menu-artwork repair](tools/car-models/README.md#repair-selection-menu-artwork-in-already-prepared-packages)
regenerates selection icons and blue backgrounds from each package's source
exterior, with backups. The preview and repair tools require NumPy and Pillow;
building new model packages also requires SciPy. Close MM3, repair the selected
packages, then reapply the matching Workshop manifest to install the new artwork.
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

For example, changing Panoz's `physics.TorqueScale` from `750` to `800`
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

The optional `speedometer.max_kmh` and `speedometer.max_mph` values set an added
car's interior dashboard needle range. Its on-screen speedometer keeps the
donor's native font, artwork, gauge style and scale. Interior ranges provide
rounded headroom for each preset: **360 km/h / 240 mph** for Panoz through
**440 km/h / 280 mph** for Regera. Enter both limits, or leave both blank in the
editor to inherit the donor interior gauge. These limits change the interior
needle range, not engine power, actual top speed or the `TOPSPEED` menu rating. In the workshop, use
**Original car speedometers…** to change original sport and Koenigsegg HUDs.
Koenigsegg, Viper and Lotus default to 800 km/h / 500 mph. Their original physics
and interior gauges stay unchanged. Blank original limits restore the retail HUD.
The numeric speed readout is hidden in every HUD layout; analog dials and
their native styles remain active. Original-car HUD overrides remain available.

The installer backs up affected game archives and reuses those originals when
reapplying, so edits do not accumulate. It does not edit saved profiles. To
remove the pack and restore the original game data, close the game and run:

```powershell
tools\venv\Scripts\python.exe tools\car-mods\prepare.py --restore
```

## Development and validation

The integrated source's car/model/factory/import tests ran **135 cases**:
**129 passed and six skipped** because optional local fixtures, decoder oracle
or symbolic-link privileges were unavailable. Corrected simulation tests passed
18 timing/ABI and 16 actual generated-loop cases, including a frozen-cadence
negative control. Runtime validation passed 198 CTest cases across
19 suites and its MSVC build. Eight harmless play-script profile/logging tests,
nine production launcher settings-helper cases and fresh-generated native
camera/map/route/HUD/input/car/collision/translation contracts also passed.
The final combined game build, live launcher rendering/normal close and Paris
smoke runs passed. Independent guest/world-clock traces verified the default
30 Hz world rate at both requested render limits. Actual driving medians were
59.96 FPS with 60 requested and 71.52 FPS (about 72) with 90 requested. These
were direction/acceleration checks; achieved 90 FPS, real-race 60 Hz simulation
and matched handling parity remain extended coverage. See the integration
record for scope and measurements.

- [Integration record](docs/integration-2026-10-13.md): preserved features, source boundaries, tests and remaining acceptance work.
- [Setup](scripts/setup.ps1) and [generator](scripts/generate_mm3.py): supported extraction, translation and build recipe.
- [Launcher](src/mm3_launcher.cpp) and [mod registry](src/mods/mods.c): settings, defaults and environment mappings.
- [Runtime](tools/xboxrecomp) and [runtime patch](patches/xboxrecomp-mm3.patch): renderer, audio, input, threading and fork fixes.
- [Developer tools](tools/debug-features/README.md), [teleport debugger](DEBUG_TELEPORT.md) and [custom-menu investigation](CUSTOM_MENUS.md).
- [Historical fork reference](docs/fork-reference-2026-10-13.md): previous runtime pin, profiles, performance measurements, diagnostic flags and October 12 upstream comparison.
- [Historical integration backlog](Next%20fixes%20to%20do.txt): earlier missing-fix evidence; its statuses are not a current audit of the merged source.
- [Third-party licences](THIRD_PARTY.md) and [contribution guide](CONTRIBUTING.md).

PRs should address one problem and include the checks actually run. AI-assisted
contributions are welcome; review their code and provenance. Runtime changes
belong in the runtime repository, with corresponding game-side integration when
needed. A source merge or test pass does not replace menu, driving, race and
save/reload acceptance on the combined executable.

## Copyright and contributions

Midtown Madness 3 and Xbox software, assets, names, and trademarks belong to
their respective rights holders. This project grants no rights to that material.
Use your own lawfully obtained game dump and comply with applicable law;
owning a copy does not by itself grant permission to redistribute it.

Do not commit, upload, or attach any of the following to this repository,
issues, PRs, or releases:

- Disc images, game executables, DLC, or extracted game assets such as textures,
  models, music, movies, and archives.
- Original or leaked game source, or disassembled, decompiled, or generated
  translations of game code. Generate translated code locally during setup.
- Xbox BIOS/firmware, encryption keys, proprietary SDK files, or private saves
  and account data.
- Links to unauthorised copies of these materials.

Submit only code and materials you created or have permission to share, and
preserve applicable third-party notices. Report copyright concerns through an
issue without reposting the disputed material.

## Licence

Original project code is licensed under the [GNU General Public License,
version 3](LICENSE) (`GPL-3.0-only`). Original contributions submitted to this
project are provided under the same licence.

When distributing covered binaries or modified versions, comply with GPLv3,
including its corresponding-source and licensing requirements. Private
modifications do not have to be published. Earlier revisions released under
MIT remain available under MIT; this change does not revoke those permissions.

Third-party components retain their own licences; see [THIRD_PARTY.md](THIRD_PARTY.md)
and the [runtime's licence](tools/xboxrecomp/LICENSE). Preserve their notices
and comply with their terms when combining or distributing code.

This licence covers only rights the project's contributors can grant. It does
not license Midtown Madness 3, Xbox software, game assets, trademarks, or other
third-party material, or replace the need for appropriate permissions.
