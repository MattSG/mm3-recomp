> **AI-generated code warning:** Nearly all project code was written with AI assistance.
> Expect bugs and rough edges. Reviews, fixes and improvements are welcome.

# Midtown Madness 3 recompilation

An experimental static recompilation of the Xbox game to native x86-64 Windows,
using [xboxrecomp](tools/xboxrecomp) and Direct3D 11. No game files are
included: you build it from your own copy of the game.

## Quick start

You need:
- Windows 10/11 x64;
- Visual Studio 2022 or newer with **Desktop development with C++**, which includes CMake;
- Python 3.12 (3.11.4 or newer works);
- git and PowerShell 7 (`pwsh`);
- a dump of your own Midtown Madness 3 disc (see [Which disc](#which-disc)).

The first configure downloads FreeType and RmlUi for the launcher, so it needs
network access.

```powershell
git clone --recurse-submodules https://github.com/MattSG/mm3-recomp.git
cd mm3-recomp
pwsh scripts\setup.ps1 -Iso "<path to your disc image>"
.\build\RelWithDebInfo\mm3_recomp.exe
```

`setup.ps1` does each of these steps:
1. Checks the toolchain and makes `tools\venv`.
2. Verifies your disc and extracts it to `game_files\`.
3. Translates the game code to C in `src\recomp\gen\` (about six minutes).
4. Builds the executable.

Re-running it skips finished steps, and `-Force` redoes them. Nothing from your
disc is ever committed.

The executable opens a launcher with **Settings**, **Controls** and **Mods**
tabs. Press **Play** (or Enter) to start the game.

### Which disc

Any dump of the retail game works: a redump `.iso` (video and game partitions)
or a bare XISO. USA and Europe/Australia share the same code. Setup checks the
game code's hash and refuses anything else. These images were tested end to end:

| SHA-1 | Image |
|---|---|
| `52cd998731eb67477b44c49c4152f56c0fc58a56` | Midtown Madness 3 (USA): redump `.iso`, 7.8 GB |
| `8ed003adaf504883afc980d2888df6125496d044` | Midtown Madness 3 (Europe, Australia) (En,Fr,De,Es,It): XISO, 3.6 GB |

## Playing

- **Launcher:** arrows, Space and Tab navigate; Enter plays and Esc quits. A
  controller works too: D-pad to move, A to change, LB/RB for tabs, Start to play.
- **Keyboard:** turn keyboard and mouse on under Controls, where every key can be
  rebound. With the **Keyboard driving** mod (on by default), these keys apply:
  - arrows accelerate, brake and steer;
  - R and F shift up and down in manual;
  - Space is the handbrake;
  - C changes the camera;
  - Esc goes back;
  - Ctrl enlarges the race map.
- **Menu mouse:** turn menu keys and mouse on under Controls. Point to select,
  then click to confirm; right-click goes back.
- **Fullscreen:** F11 or Alt+Enter.

To start straight into the game without the launcher:

```powershell
pwsh scripts\play.ps1 -Keyboard -Profile Balanced -Fov 100 -FpsLimit 60 -Fullscreen
```

The options are:
- `-Profile`: Balanced, Quality or LowPower.
- `-Fov`: the horizontal driving field of view, from 60 to 110 degrees.

The world updates at a fixed rate whatever the frame rate: 30 a second (the
Xbox's), 60 (the default) or 90, under Settings > Tick rate. Handling and the
AI are the same at any frame rate. If performance is poor, lower supersampling
or the internal resolution.

**Audio** runs the Xbox's sound DSP: reverb, 3D sound and the game's own mix.
The first time you play, the game saves its DSP programs to `game_files\dsp`.
The next build compiles them to native code; until then an interpreter runs
them. If the sound stutters (`[XA2] underrun` in the log), set Audio > Sound
processor to Off. The voices are then mixed directly, without those effects.

**Saves.** `MM3_SAVE_DIR` picks a separate save folder. Back up your saves
before trying a new build.

**Unlocking cars.** `MM3_UNLOCK_ALL_CARS=1` makes every installed car selectable
without changing your profile.

### Multiplayer

System Link works between copies on one PC or on a LAN. For two copies on one PC,
run each from its own PowerShell window with its own save folder:

```powershell
$env:MM3_SAVE_DIR = 'C:\MM3\player-one'   # player-two in the other window
pwsh scripts\play.ps1 -AllowMultipleInstances
```

For copies on other networks, set `RECOMP_NET_PEERS=<ip>:<port>` and open that
UDP port.

Xbox Live names resolve to [Insignia](https://insignia.live)'s servers, but
signing in is not yet verified. To try it:
1. Make an account in xemu.
2. Import it with `py scripts\import_xemu_accounts.py <xbox_hdd.qcow2> <save dir>`.
3. Give the game that console's keys in `RECOMP_XBOX_HDKEY`, `RECOMP_XBOX_SIGKEY`
   and `RECOMP_XBOX_LANKEY` (32 hex digits each).

Keep accounts and keys local.

## Mods

Each change this project makes to how the game plays or looks is a mod, and the
launcher's Mods tab turns it on or off. The mods include:
- widescreen;
- extended draw distance, with native fog;
- a smooth skyline;
- field of view;
- menu mouse ([details](MENU_MOUSE.md));
- keyboard driving;
- lamp flare depth;
- intro skipping;
- console-rate simulation;
- offline network;
- the developer tools and a teleport debugger.

Extended draw distance also replaces the Xbox fog texture with a native fog
pass. Nearby scenery stays clear, distant geometry fades into the sky colour,
and the low-resolution horizon panorama is left out. Turning the mod off
restores the original fog and panorama.

The collision-cell and disc-check repairs are required and always on. See the
[mod registry](src/mods/mods.c) for each mod's switch and default.

## Extra cars

The optional [extra-car pack](tools/car-mods/README.md) adds nine cars beside
the original 35:
- Panoz GTR-1
- McLaren F1
- Formula 1
- Ford GT
- McLaren Senna
- McLaren Senna GTR
- Ferrari FXX K
- Koenigsegg Regera
- Lamborghini Aventador

Each car has its own name, paint, menu ratings and driving setup. Close the game
and run `tools\car-mods\editor.cmd` to edit them; **Save & Apply** installs your
changes. The [car-mods guide](tools/car-mods/README.md) covers these:
- every physics, gearing and speedometer setting;
- command-line installation;
- restoring the original data (`prepare.py --restore`).

The [model workflow](tools/car-models/README.md) adds imported bodies, and also
lists each model's source and credits. Models are not distributed here.

## Status

This is a work in progress, and rendering, audio, input and gameplay can
regress. A successful build doesn't mean every feature works. The
[integration record](docs/integration-2026-10-13.md) lists what has been verified
and what hasn't.

## Development

- [Setup](scripts/setup.ps1) and [generator](scripts/generate_mm3.py): extraction, translation and build. To regenerate the translated code, run `python scripts\generate_mm3.py` and then `pwsh scripts\setup.ps1`. The generator is pinned to toolkit commit `967ec30`, separately from the runtime submodule.
- [Launcher](src/mm3_launcher.cpp) and [mod registry](src/mods/mods.c): settings, defaults and environment variables.
- [Runtime](tools/xboxrecomp): our fork of [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp). It stays game-agnostic: MM3-specific behaviour belongs in this repository, mostly as mods. Generic fixes go upstream as focused PRs.
- [Developer tools](tools/debug-features/README.md), [teleport debugger](DEBUG_TELEPORT.md) and [ImGui debug tools](IMGUI_DEBUG_TOOLS.md).
- [Fork reference](docs/fork-reference-2026-10-13.md): profiles, measurements and diagnostic flags.

PRs should address one problem and say which checks were run; see
[Contributing](CONTRIBUTING.md). AI-assisted contributions are welcome. Review
and understand your changes before you submit them.

## Credits

This project combines [Matt Graham's project](https://github.com/MattSG/mm3-recomp)
with the car tools, gameplay repairs and runtime work from
[lakshya16157's PC Native fork](https://github.com/lakshya16157/midtown-madness-3-PC-Native),
which was prepared with OpenAI Codex. It is an unofficial community project with
no affiliation with Microsoft, Xbox or Digital Illusions CE (DICE).

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
