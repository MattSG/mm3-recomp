# Midtown Madness 3 recompilation

A native PC build of the Xbox game, made with
[xboxrecomp](https://github.com/MattSG/xboxrecomp). No game files are included;
you need a dump of your own disc.

**Experimental and AI-assisted.** Windows x64 is fully playable. Linux x64 builds
and runs but needs testing. macOS is not supported.

## Build and play

### Windows

Install Python 3.12+, Git, CMake and Visual Studio 2022+ with **Desktop development
with C++** and **C++ CMake tools for Windows**.

```sh
git clone --recurse-submodules https://github.com/MattSG/mm3-recomp.git
cd mm3-recomp
```

Run the builder from [Releases](https://github.com/MattSG/mm3-recomp/releases/latest)
or `python scripts/build_gui.py`:

1. Choose your disc image (**Check SHA** verifies it).
2. Choose **Direct3D 12** or **Vulkan**, then **Build**.
3. **Launch game**, adjust settings, press **Play**.

Command line instead:

```powershell
python scripts/setup.py --iso "<disc image>"
.\build\RelWithDebInfo\mm3_recomp.exe
```

### Linux

Needs an x86-64 system with Python, a C/C++ compiler, CMake, Ninja, pkg-config,
OpenSSL, SDL3's build dependencies, a Vulkan driver, the FFmpeg development
packages and Tkinter (`python3-tk`) for the builder.

```sh
python3 scripts/build_gui.py        # or:
python3 scripts/setup.py --iso "<disc image>"
./build/mm3_recomp
```

A standalone Linux builder is in Releases (`chmod +x mm3-builder-linux`).

### Discs

Tested: the USA redump ISO and the Europe/Australia XISO. Other dumps work if their
game code matches; the builder checks before building.

## Settings

The launcher covers display, graphics, audio, network, controls and mods.
F11 or Alt+Enter toggles fullscreen.

- **Use keyboard and mouse** to drive without a controller (arrows, R/F gears,
  Space handbrake, C camera).
- **Menu keys and mouse** to click through game menus.
- If audio stutters, set **Audio > Sound processor > Off**.

## Xbox saves

Saves copied from an Xbox show as damaged until they are re-signed for this port.
Use the builder's **Import save** button and pick the `UDATA` folder, its `4d53002a`
folder or a single save. It re-signs a copy into your saves; a save with the same
name is moved to `save-backups` first. Terminal: `python tools/saves/resign_save.py --import <folder>`.

## Mods

Visual and gameplay fixes are on by default; cheats and developer features are off.

| Mod | What it does |
|---|---|
| Widescreen | Wider view, HUD at the window edges |
| Extended draw distance | Draw further, with depth fog |
| Smooth skyline | Filter the distant skyline |
| Field of view | Adjust the race camera |
| Console-rate simulation | Keep the game's 30 Hz simulation at any frame rate |
| Frame interpolation | Smooth motion above 30 fps, about 50 ms of latency (off) |
| Keyboard driving / Menu mouse | Keyboard and mouse controls |
| Lamp flare depth | Hide lamp flares behind geometry |
| Skip intro movies | Skip startup logos and movies |
| Collision cell guard | Fix a broken Paris collision cell (required) |
| Rental mission / Old-film fixes | Fix original game bugs |
| Unlock cars, paints, races | Cheats, separately or together (off) |
| Time limits, Opponents' difficulty, Car damage | Tune difficulty (off) |
| Auto-activate Easter egg | Keep the old-film or sketch effect on (off) |
| Hold the race | F2 freezes the race (off; not in System Link) |

[src/mods/mods.c](src/mods/mods.c) lists every switch.

## Known issues

- System Link and Insignia are highly experimental.
- Linux has only been checked on WSL; missions and native GPUs need testers.
- Without Frame interpolation, frame rates above 30 repeat frames.
- "SDL3 patch ... corrupt patch" on build (clones before 2026-11-08): run
  `git rm --cached src/host_sdl3/sdl3-d3d12-descriptors.patch`, delete the file, then
  `git checkout -- src/host_sdl3/sdl3-d3d12-descriptors.patch`.

## More

- [Contributing](CONTRIBUTING.md) · [Docs](docs/) · [Test harness](tools/game_test/README.md)
- Car editor and model tools: [mm3-car-tools](https://github.com/MattSG/mm3-car-tools) (`tools/car-tools`).
- Generic runtime fixes go to the [xboxrecomp fork](https://github.com/MattSG/xboxrecomp) (`tools/xboxrecomp`).

## Legal

Unofficial and unaffiliated with Microsoft, Xbox or DICE. Don't upload game files,
disc images, generated game code, DLC, BIOS files, keys or saves. Project code is
[GPL-3.0-only](LICENSE) (earlier revisions MIT); third-party code keeps its own
licence, see [THIRD_PARTY.md](THIRD_PARTY.md), including work from
[lakshya16157's PC Native fork](https://github.com/lakshya16157/midtown-madness-3-PC-Native).
Midtown Madness 3 belongs to its rights holders.
