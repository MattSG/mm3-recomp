> **AI-generated code warning:** This project is essentially vibe coded—nearly all
> project code was written with AI assistance. Expect bugs, rough edges, questionable
> decisions, and bad code. PRs, bug reports, reviews, and improvements are welcome.

# Midtown Madness 3 recompilation

Experimental static recompilation of the Xbox game to native x86-64 Windows,
using [xboxrecomp](tools/xboxrecomp) and Direct3D 11. You need your own game disc
image; game assets are not included.

This is an unofficial community project, not affiliated with or endorsed by
Microsoft, Xbox, Digital Illusions CE (DICE), or the game's other rights holders.

## Build and play

Requirements: Windows 10/11 x64, Visual Studio 2022 or newer with **Desktop
development with C++** and CMake tools, Python 3.11+ with the `py` launcher,
PowerShell 7 (`pwsh`), and Git.

```powershell
git clone --recurse-submodules https://github.com/MattSG/mm3-recomp.git
cd mm3-recomp
pwsh scripts/setup.ps1 -Iso "D:/Games/Midtown Madness 3 (USA).iso"
./build/RelWithDebInfo/mm3_recomp.exe
```

Setup extracts the disc into `game_files/`, generates the translated code, and
builds the executable. Re-running skips extraction and generation when their
outputs exist; `-Force` repeats them.

USA redump ISO and Europe/Australia XISO images are recognised. Other images
must contain matching retail game code; the [extractor](scripts/extract_disc.py)
checks this before extraction.

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
pwsh scripts/play.ps1 -Keyboard    # optional: -Fullscreen -FpsLimit 120
```

Use 60 FPS as the default; higher rates can change physics behaviour. Lower
supersampling or internal resolution in Settings if performance is poor.

## Features and limits

The launcher exposes widescreen, extended draw distance, skyline filtering,
field of view, menu mouse support, intro skipping, and offline-network bypass.
Optional developer tools and a teleport debugger are also available. The
[mod registry](src/mods/mods.c) lists their switches and defaults.

This is a work in progress. Rendering, audio, input, and gameplay can have
regressions; a successful build does not establish that every feature works.
Network support is experimental: Insignia sign-in and full system-link matches
are not verified, and the system-link browser has remained empty in testing.

**Back up your saves before testing new builds.** This experimental software is
provided as is, without warranty. Use it at your own risk, including the risk
of crashes, corrupted saves, or lost progress.

## Development

- [Setup script](scripts/setup.ps1): toolchain checks, extraction, generation, and build.
- [Generator](scripts/generate_mm3.py): the pinned translation recipe; use it for regeneration.
- [Launcher source](src/mm3_launcher.cpp): settings and environment-variable mappings.
- [Runtime](tools/xboxrecomp): graphics, audio, input, networking, and runtime diagnostics.
- [Developer tools](tools/debug-features/README.md) and [teleport debugger](DEBUG_TELEPORT.md).
- [Third-party licences](THIRD_PARTY.md).

PRs and cleanups are welcome, including fixes to AI-written code and incorrect
documentation. For bugs, include the commit, settings, reproduction steps, and
relevant logs. AI-assisted PRs are welcome, but contributors must understand
their changes and describe their testing. Distinguish implemented behaviour
from behaviour verified in actual gameplay, and identify what remains untested.

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
