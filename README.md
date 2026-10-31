# Midtown Madness 3 recompilation

A static recompilation of the Xbox game to native Windows x64, using
[xboxrecomp](https://github.com/MattSG/xboxrecomp) and Direct3D 11 or SDL3 GPU
(Vulkan / Direct3D 12). No game files are included: you build it from your own disc.

> **Largely AI-written.** Nearly all of the code was written with AI assistance
> (Claude Code and OpenAI Codex) and tested in play. It is
> experimental, so expect bugs. Fixes and reviews are welcome.

## Quick start

You need a dump of your own Midtown Madness 3 disc, Python 3.12, git, CMake,
and an x86-64 Windows or Linux PC. Windows also needs Visual Studio 2022 or
newer with **Desktop development with C++** and **C++ CMake tools for
Windows**. Linux needs a C/C++ compiler, the dependencies listed in the
[Linux section](#linux-sdl3-gpu--vulkan), and Tkinter (for example,
`python3-tk` on Ubuntu).

1. Clone the repository and its submodules:

   ```sh
   git clone --recurse-submodules https://github.com/MattSG/mm3-recomp.git
   cd mm3-recomp
   ```

2. Download the standalone builder for your OS from the repository releases
   and run it, or run `python scripts/build_gui.py` on Windows /
   `python3 scripts/build_gui.py` on Linux. If prompted, select the checkout
   folder created in step 1. On Linux, make the download executable with
   `chmod +x mm3-builder-linux` before running it. The standalone app can
   check a disc without Python; building the game still needs Python and the
   listed build tools.
3. Choose your disc image and select **Check SHA** to verify its image SHA-1
   and game-code SHA-256. The result shows whether the image matches a tested
   dump; **Show log** has the details.
4. Choose a graphics API and select **Build**. The disc is checked again before
   building. Open **Show log** for progress and any errors.
5. When the build finishes, select **Launch game**. The chosen graphics API is
   used for that launch.

The builder reuses existing extracted game files and keeps separate Windows
and Linux renderer builds under `build/gui-*`. Direct3D 11 and Direct3D 12
are available on Windows; Vulkan is available on Windows and Linux. macOS game
builds are not supported yet.

For command-line setup instead, run:

```powershell
python scripts/setup.py --iso "<path to your disc image>"
.\build\RelWithDebInfo\mm3_recomp.exe
```

### Windows: SDL3 GPU (Vulkan and Direct3D 12)

Include Visual Studio's **C++ CMake tools for Windows** component. From the
repository root, build once for both backends:

```powershell
python scripts/setup.py --iso "<path to your disc image>" --renderer SDL3_GPU
```

Already set up? Omit `--iso`. CMake downloads SDL3 and the shader compiler;
no Vulkan SDK is needed. Vulkan requires a compatible GPU driver.

Select the backend in PowerShell, closing the game before switching:

```powershell
# Vulkan
$env:RECOMP_SDL_GPU_DRIVER = "vulkan"
.\build\RelWithDebInfo\mm3_recomp.exe

# Direct3D 12
$env:RECOMP_SDL_GPU_DRIVER = "direct3d12"
.\build\RelWithDebInfo\mm3_recomp.exe

# Restore automatic selection
Remove-Item Env:RECOMP_SDL_GPU_DRIVER -ErrorAction SilentlyContinue
```

The startup log confirms `[SDLGPU] device: vulkan` or `direct3d12`.
For source-only rebuilds, use **Developer PowerShell for Visual Studio**:

```powershell
cmake --build build --config RelWithDebInfo --target mm3_recomp --parallel
```

To return to Direct3D 11, run `python scripts/setup.py --renderer D3D11`;
that build ignores `RECOMP_SDL_GPU_DRIVER`. For a Release build, pass
`--config Release` to setup and launch `build\Release\mm3_recomp.exe`.

### Linux: SDL3 GPU / Vulkan

On Linux x86-64 (tested on Ubuntu 22.04), install Python, CMake, Ninja, a C/C++
compiler, OpenSSL, libepoxy, SDL3's build dependencies and a Vulkan GPU driver.

```sh
python3 scripts/setup.py --iso "<path to your disc image>" --renderer SDL3_GPU
RECOMP_SDL_GPU_DRIVER=vulkan ./build/mm3_recomp
```

Subsequent builds can omit `--iso`. Direct3D backends require Windows.

Setup verifies your disc, extracts it, translates the game code (about six
minutes) and builds. It then runs the game for about ten seconds, hidden and
muted, to capture its audio DSP programs, and rebuilds with them compiled in. The game opens a launcher with **Settings**, **Controls**
and **Mods** tabs; press **Play**.

### Supported discs

Setup checks the game code's hash, so any retail dump with the same code works.
These images were tested:

| SHA-1 | Image |
|---|---|
| `52cd998731eb67477b44c49c4152f56c0fc58a56` | Midtown Madness 3 (USA): redump `.iso`, 7.8 GB |
| `8ed003adaf504883afc980d2888df6125496d044` | Midtown Madness 3 (Europe, Australia) (En,Fr,De,Es,It): XISO, 3.6 GB |

## What's improved

- **Rendering:** Direct3D 11 or SDL3 GPU (Vulkan / Direct3D 12), with render scale,
  MSAA, anisotropic filtering, any aspect ratio and a frame-rate limit. Performance profiles (Balanced,
  Quality, LowPower) set these for you.
- **Audio:** the Xbox's sound DSP (reverb, 3D sound and the game's own mix),
  captured during setup and compiled to native code.
- **Input:** keyboard, mouse and controllers, all rebindable.
- **Multiplayer:** System Link on one PC or a LAN. Xbox Live goes to
  [Insignia](https://insignia.live), but sign-in is not yet verified.
- **Extra cars:** nine extra cars, with an editor for their paint, ratings,
  physics and gearing, in [mm3-car-tools](https://github.com/MattSG/mm3-car-tools).

## Mods

Each change to how the game plays or looks is a mod, switched in the launcher's
Mods tab. The mods marked "always" are needed for the game to run.

| Mod | Default | What it does |
|---|---|---|
| Widescreen | on | The 3D view fills the window and the HUD moves to its edges. |
| Extended draw distance | on | Traffic, objects and the city drawn further, with native depth fog. |
| Smooth skyline | on | The distant skyline is filtered smoothly instead of blocky. |
| Field of view | on | A wider race camera, set under Graphics. |
| Console-rate simulation | on | The world updates at the Xbox's rate, so handling and AI match the console at any frame rate. |
| Keyboard driving | on | Arrows drive, R/F shift gear, Space is the handbrake, C changes the camera. |
| Menu mouse | on | Point at a menu item to select it and click to confirm. |
| Lamp flare depth | on | Street-lamp flares stay hidden behind buildings and cars. |
| Skip intro movies | on | Skips the logo and intro movies at start-up. |
| Collision cell guard | always | Repairs a broken Paris collision cell that can freeze the game. |
| Developer tools | off | The retail debug HUD, menu and cameras. |
| ImGui debug tools | off | An in-game developer interface, with teleport. |

[src/mods/mods.c](src/mods/mods.c) lists every mod and its environment variable.

## Playing

- **Keyboard:** turn it on under Controls.
- **Fullscreen:** F11 or Alt+Enter.
- **Sound stutters:** set Audio > Sound processor to Off.
- **Skipping the launcher:** run `python scripts/play.py --keyboard --profile Balanced`.
- **Saves:** back them up before you try a new build.
- **Saves from an Xbox** load as damaged, because the console signed them with its own key.
  `python tools/saves/resign_save.py <UDATA folder> <new folder>` makes a copy signed for
  this port; put its `4d53002a` folder in your saves' `UDATA`.

## More documentation

In [docs/](docs/): [debug tools](docs/IMGUI_DEBUG_TOOLS.md), [teleport pipe](docs/DEBUG_TELEPORT.md),
[custom menus](docs/CUSTOM_MENUS.md), [menu mouse](docs/MENU_MOUSE.md),
[who owns what between this repo and the xboxrecomp fork](docs/XBOXRECOMP_OWNERSHIP.md), and the
[XboxXBERecomp comparison](docs/xboxxberecomp-evaluation.md). Old audits are in [docs/history/](docs/history/).
The runtime test harness is in [tools/game_test/README.md](tools/game_test/README.md).

## Contributing

PRs are welcome, including AI-assisted ones. Review and understand what you
submit. Keep each PR to one problem and say which checks you ran; see
[CONTRIBUTING.md](CONTRIBUTING.md).

Changes to how MM3 plays or looks belong here, as mods. Fixes to the runtime
belong in our [xboxrecomp fork](https://github.com/MattSG/xboxrecomp): upstream
[sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp) plus small,
game-agnostic commits, each meant to be offered upstream.

Never commit, upload or link any of these:
- disc images or game files;
- generated or decompiled game code;
- BIOS files, keys or saves.

## Legal

This is an unofficial project with no affiliation with Microsoft, Xbox or DICE.
It includes car tools and fixes from
[lakshya16157's PC Native fork](https://github.com/lakshya16157/midtown-madness-3-PC-Native).
Its code is [GPL-3.0-only](LICENSE), and earlier MIT releases stay MIT. See
also [THIRD_PARTY.md](THIRD_PARTY.md). Midtown Madness 3 and its assets belong
to their rights holders. Use only your own lawfully obtained copy.
