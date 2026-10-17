# ImGui debug tools (WIP)

Branch: `feature/imgui-debug-tools`, starting at `c1a6adc`.
Private worktree: `I:/repos/mm3-imgui-debug-tools`.

## Use in this isolated worktree

Enable **ImGui debug tools** under the launcher's Mods tab. Controls contains the
show/hide keyboard, controller-combination and optional mouse bindings. They save
with the launcher's other settings when Play is pressed.

Defaults: **F4** or **Back + left-stick click**; mouse toggle is unbound. Inside
ImGui, use mouse, Tab/arrows and Enter/Space, or controller D-pad/stick and A.
The overlay closes on focus loss. Gameplay continues while it is open.

For a direct isolated test run after building and preparing the private data:

```powershell
python tools/debug-features/run.py --config RelWithDebInfo --debug-overlay --capture-display
```

This script uses its own worktree's data and `out/debug-features/save`. It bypasses
launcher settings; binding overrides can be supplied as environment variables:
`MM3_OVERLAY_KEY` (hex VK), `MM3_OVERLAY_PAD` (hex physical XInput button mask),
and `MM3_OVERLAY_MOUSE` (0 off, 4 middle, 5/6 side buttons). Do not launch a new
foreground game window during someone else's focus-sensitive live test.

HUD/native-menu commands need `tools/debug-features/prepare.py`, run only against
backed-up private data while its game is stopped. Native teleport, recovery and
repair do not require the prepared scripts. The ImGui controls do not expose profile unlock/save commands.

## Existing backends

| Facility | Existing implementation | Evidence / limit |
| --- | --- | --- |
| Developer commands | `src/mods/dev_tools.c` runs numbered prepared Lua scripts after player update, preserving guest registers | HUD and native menu were visually verified in the existing debug experiment; loader success alone does not prove other effects |
| Teleport | `src/mods/teleport.c` queues requests to primary player update and calls native ground reset `0021ADF3` | Existing fixtures cover ABI, ground miss and cancellation; `DEBUG_TELEPORT.md` records user-confirmed live operation |
| Position / heading | Teleport backend samples active car body under an SRW lock | Availability expires after 300 ms; UI never reads guest objects directly |
| Input / settings | Runtime framebuffer tracks keys and mouse; launcher persists environment settings | Optional host capture seam neutralizes gameplay input; physical-device validation pending |
| ImGui | Dear ImGui 1.91.9b with Win32/DX11 backends | Pinned dependency, presentation callback and synchronized Win32 event delivery; live rendering pending |

## Initial scope

Reuse prepared developer commands and native teleport. Expose HUD, native-menu
open/close, and diagnostics first. Other commands must retain their experimental
status until their observable effects are verified. No save unlocks, speculative memory writes, invented destinations, or framerate changes.

The native debug menu can be opened from ImGui, but ImGui must then close to let
the game's menu receive input. No debug-rendering controls are claimed safe.

## Isolation and acceptance

The runtime clone, build, generated outputs, game data and test saves must belong
to this worktree. Main and other live processes are read-only. No runtime launch
until writable paths have been checked. Asset changes require a full hash-verified
private backup first.

## Implemented prototype

- Optional `ImGui debug tools` mod; disabled by default.
- Launcher Controls: keyboard F4/F5/F8/Insert, controller Back+LS/both sticks/LB+RB,
  and optional middle/side mouse button. Existing settings persistence exports these.
- In-game Developer, Teleport, Vehicle and Diagnostics tabs. HUD commands reuse prepared
  scripts. Native-menu launch closes ImGui to hand input to the game.
- Teleport reuses the native backend, with nonblocking requests, finite coordinate
  checks, expiry, active-car identity checks and a session position bookmark.
- Win32 backend input runs on the window thread, serialized with render-thread
  ImGui calls by a recursive mutex (mouse capture can reenter the window procedure).
  Windows requires capture on the window's thread:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setcapture
- Focus loss closes the overlay. Gameplay input is neutralized while physical
  ImGui navigation remains live. Packet numbering preserves capture/release edges.
- Vehicle repair uses the registered native backend `0021CAE8`, with revalidated
  car damage/wheel/drivetrain prerequisites and queued player/car/world identity.
  Recovery uses the existing native ground reset at the current position.
- Bookmarks clear when gameplay becomes unavailable or the world changes.
- Private runtime changes are a generic optional host presentation/message/capture
  callback and input neutralization. No title addresses or ImGui dependency in runtime.
- Dear ImGui 1.91.9b is pinned by archive SHA-256; MIT notice in THIRD_PARTY.md.

## Checks performed

MSVC RelWithDebInfo game build and all five focused contract executables passed.
The checks compile the actual project source. `mm3_teleport_contract` tests
teleport.c's logic against a stubbed native ground reset; the original-PAL run
is described in DEBUG_TELEPORT.md and skips without its local fixture:

```powershell
cmake --build build --config RelWithDebInfo --target mm3_overlay_contract mm3_dev_tools_contract mm3_teleport_contract mm3_input_capture_contract mm3_launcher_settings_contract
./build/RelWithDebInfo/mm3_overlay_contract.exe
./build/RelWithDebInfo/mm3_dev_tools_contract.exe
./build/RelWithDebInfo/mm3_teleport_contract.exe
./build/RelWithDebInfo/mm3_input_capture_contract.exe
./build/RelWithDebInfo/mm3_input_capture_contract.exe --pad-only
New-Item -ItemType Directory -Force out/settings-contract | Out-Null
Push-Location out/settings-contract
../../build/RelWithDebInfo/mm3_launcher_settings_contract.exe --write
../../build/RelWithDebInfo/mm3_launcher_settings_contract.exe --read
Pop-Location
```

Coverage: keyboard/mouse toggle edges, key repeat, input ownership decisions,
focus-loss close, binding parsing, developer command bounds/busy/expiry and guest
register preservation, teleport queue/rejection/expiry plus existing reset fixtures.
The runtime input check includes the actual `xinput_device.c` with fixture host
inputs. It reproduced a neutral-packet collision before the fix, then passed
merged keyboard/pad and physical-only capture/release transitions after the fix.
These are offline checks, not live input/rendering or physical controller proof.

Private game data has a full 1,066-file SHA-256-verified backup at
`out/original-game-files`, with its manifest beside it. Prepared debug scripts were
installed only in the private game data after backup. Main archive was locked by
another process; that copy attempt was cancelled, with no process touched. Data
came from the earlier isolated menu-mouse worktree instead.

## Cheat / vehicle investigation

Original PAL XBE registers `playerSetDamageFactor` through `001FF465`,
`playerSetDamage` through `001FC074`, and `vehicleRepair` through `002038A8`.
Shipped mission scripts demonstrate the damage-factor command's player-name and
number arguments. This establishes available native script interfaces, not safe
trainer behaviour: restoring a guessed damage factor could overwrite a mission's
own setting. Invulnerability is not exposed. Repair is now wired through `0021CAE8`; its
virtual `+BC` calls `001D5FB7` -> `001CDAD3`, resetting native damage components
and invoking `001CB424` to restore associated wheel/drivetrain parameters. The
queued repair and its prerequisites have offline checks; the live visual effect
has not yet been verified.

The settings check includes the actual launcher source and actual mod registry.
One process saves and exports the overlay mod, F8, both sticks and side mouse 1;
a separate process reloads and exports them from a private INI. This establishes
persistence through the real model functions, not visible launcher interaction.

## Acceptance audit

| Requirement | Current evidence | Status |
| --- | --- | --- |
| Isolated worktree/build/data/saves | Private worktree and runtime clone; verified full data backup; no new game process launched | Established for work so far |
| Tool inventory and native paths | Source and original PAL XBE inspection documented above | Established |
| ImGui shell and bindings | Game build; overlay contract; actual launcher persistence across processes | Offline checks pass; live UI pending |
| Input capture/restoration | Actual runtime input code; reproduced packet regression and passing merged/physical-only cases | Offline checks pass; physical devices pending |
| Teleport/recovery | Existing native ground-reset backend; queue/expiry/identity fixtures | New interface's live effects pending |
| Native repair | Original native function body and virtual path inspected; guarded queue and register fixtures | Live component/visual repair pending |
| HUD/native menu | Existing prepared Lua backend reused | New interface's live effects pending |
| Loading/focus transitions | Focus callback and expiry/identity checks | Normal-run transitions pending |
| No save-changing controls | Exposed actions contain no profile/unlock/save commands | Source inspected |
| Controls and limitations documented | This file and README link | Present |

## Remaining acceptance

Live rendering, physical controller navigation/toggles, actual input restoration,
teleport/debug/repair effects, loading transitions and visible launcher interaction
remain unverified. Settings persistence across processes passes offline. Existing cheat/debug controls still need further native-state research
and live proof before inclusion. The Windows Computer Use tool currently reports
its native pipe unavailable (os error 2). No new game process has been launched;
launching a foreground window could disturb Claude's ongoing test.

This is an implementation checkpoint, not a completed goal.
