# ImGui debug tools work log

Branch: `feature/imgui-debug-tools`, starting at `c1a6adc`.
Private worktree: `I:/repos/mm3-imgui-debug-tools`.

## Existing backends

| Facility | Existing implementation | Evidence / limit |
| --- | --- | --- |
| Developer commands | `src/mods/dev_tools.c` runs numbered prepared Lua scripts after player update, preserving guest registers | HUD and native menu were visually verified in the existing debug experiment; loader success alone does not prove other effects |
| Teleport | `src/mods/teleport.c` queues requests to primary player update and calls native ground reset `0021ADF3` | Existing fixtures cover ABI, ground miss and cancellation; `DEBUG_TELEPORT.md` records user-confirmed live operation |
| Position / heading | Teleport backend samples active car body under an SRW lock | Availability expires after 500 ms; UI must never read guest objects directly |
| Input / settings | Runtime framebuffer tracks keys and mouse; launcher persists environment settings | No host UI capture seam exists yet; gamepad reads must also be suppressed during capture |
| ImGui | No integration found in current tracked project/runtime sources | Needs pinned Dear ImGui dependency, DX11 presentation callback and synchronized Win32 event delivery |

## Initial scope

Reuse prepared developer commands and native teleport. Expose HUD, native-menu
open/close, and diagnostics first. Other commands must retain their experimental
status until their observable effects are verified. No save unlocks, speculative
repair functions, invented destinations, or framerate changes.

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
- In-game Developer, Teleport and Diagnostics tabs. HUD commands reuse prepared
  scripts. Native-menu launch closes ImGui to hand input to the game.
- Teleport reuses the native backend, with nonblocking requests, finite coordinate
  checks, expiry, active-car identity checks and a session position bookmark.
- Win32 events are queued to the render thread. Focus loss closes the overlay;
  captured gameplay input is neutralized while physical ImGui navigation remains live.
- Private runtime changes are a generic optional host presentation/message/capture
  callback and input neutralization. No title addresses or ImGui dependency in runtime.
- Dear ImGui 1.91.9b is pinned by archive SHA-256; MIT notice in THIRD_PARTY.md.

## Checks performed

MSVC RelWithDebInfo game build and all three focused contract executables passed.
The checks exercise actual project source, with native game calls stubbed:

```powershell
cmake --build build --config RelWithDebInfo --target mm3_overlay_contract mm3_dev_tools_contract mm3_teleport_contract
./build/RelWithDebInfo/mm3_overlay_contract.exe
./build/RelWithDebInfo/mm3_dev_tools_contract.exe
./build/RelWithDebInfo/mm3_teleport_contract.exe
```

Coverage: keyboard/mouse toggle edges, key repeat, input ownership decisions,
focus-loss close, binding parsing, developer command bounds/busy/expiry and guest
register preservation, teleport queue/rejection/expiry plus existing reset fixtures.
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
own setting. No invulnerability or repair control is exposed yet.

## Remaining acceptance

Live rendering, physical controller navigation/toggles, actual input restoration,
teleport/debug effects, loading transitions and settings across restart remain
unverified. Existing cheat/debug controls still need further native-state research
and live proof before inclusion. The Windows Computer Use tool currently reports
its native pipe unavailable (os error 2). No new game process has been launched;
launching a foreground window could disturb Claude's ongoing test.

This is an implementation checkpoint, not a completed goal.
