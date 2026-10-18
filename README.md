# Midtown Madness 3 recompilation

A static recompilation of the Xbox game to native Windows x64, using
[xboxrecomp](https://github.com/MattSG/xboxrecomp) and Direct3D 11. No game
files are included: you build it from your own disc.

> **Largely AI-written.** Nearly all of the code was written with AI assistance
> (Claude Code and OpenAI Codex) and tested in play. It is
> experimental, so expect bugs. Fixes and reviews are welcome.

## Quick start

You need:
- Windows 10/11 x64;
- Visual Studio 2022 or newer with **Desktop development with C++**;
- Python 3.12;
- git;
- PowerShell 7;
- a dump of your own Midtown Madness 3 disc.

```powershell
git clone --recurse-submodules https://github.com/MattSG/mm3-recomp.git
cd mm3-recomp
pwsh scripts\setup.ps1 -Iso "<path to your disc image>"
.\build\RelWithDebInfo\mm3_recomp.exe
```

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

- **Rendering:** Direct3D 11, with render scale, MSAA, anisotropic filtering,
  any aspect ratio and a frame-rate limit. Performance profiles (Balanced,
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
- **Skipping the launcher:** run `pwsh scripts\play.ps1 -Keyboard -Profile Balanced`.
- **Saves:** back them up before you try a new build.

## Contributing

PRs are welcome, including AI-assisted ones. Review and understand what you
submit. Keep each PR to one problem and say which checks you ran; see
[CONTRIBUTING.md](CONTRIBUTING.md).

Changes to how MM3 plays or looks belong here, as mods. Fixes to the runtime
belong in [xboxrecomp](https://github.com/MattSG/xboxrecomp), which must stay
game-agnostic. Generic runtime fixes also go upstream to
[sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp).

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
