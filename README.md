# Midtown Madness 3 recompilation

A static recompilation of the Xbox game to native Windows x64, using
[xboxrecomp](https://github.com/MattSG/xboxrecomp) and Direct3D 11. No game
files are included: you build it from your own disc. It is experimental and
mostly AI-written, so expect bugs.

## Quick start

You need:
- Windows 10/11 x64;
- Visual Studio 2022 or newer with **Desktop development with C++**;
- Python 3.12;
- git;
- PowerShell 7;
- a dump of your own Midtown Madness 3 disc: a USA redump `.iso` or a Europe/Australia XISO.

```powershell
git clone --recurse-submodules https://github.com/MattSG/mm3-recomp.git
cd mm3-recomp
pwsh scripts\setup.ps1 -Iso "<path to your disc image>"
.\build\RelWithDebInfo\mm3_recomp.exe
```

Setup verifies your disc, extracts it, translates the game code (about six
minutes) and builds. The game opens a launcher with **Settings**, **Controls**
and **Mods** tabs; press **Play**.

## Playing

- **Keyboard:** turn it on under Controls.
  - Arrows drive, R/F shift gear, Space is the handbrake and C changes the camera.
  - Esc goes back.
  - All keys can be rebound.
- **Fullscreen:** F11 or Alt+Enter.
- **Sound stutters:** set Audio > Sound processor to Off.
- **Multiplayer:** System Link works on one PC or a LAN.
- **Skipping the launcher:** `pwsh scripts\play.ps1 -Keyboard`.
- **Saves:** back them up before trying a new build.

## More

- **Mods:** each enhancement is a mod you can switch in the launcher. They're listed in [src/mods/mods.c](src/mods/mods.c).
- **Extra cars:** a car editor and model tools are in [mm3-car-tools](https://github.com/MattSG/mm3-car-tools), at `tools\car-tools`.
- **Contributing:** see [CONTRIBUTING.md](CONTRIBUTING.md). PRs are welcome, including AI-assisted ones.

## Legal

This is an unofficial project with no affiliation with Microsoft, Xbox or DICE.
It includes car tools and fixes from
[lakshya16157's PC Native fork](https://github.com/lakshya16157/midtown-madness-3-PC-Native).
Its code is [GPL-3.0-only](LICENSE), and earlier MIT releases stay MIT. See
also [THIRD_PARTY.md](THIRD_PARTY.md).
Never share or commit disc images, game files, generated game code, BIOS or keys.
