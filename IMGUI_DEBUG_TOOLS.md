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

Remaining: overlay implementation, launcher bindings, backend integration, focused
checks, full build, live keyboard/mouse/controller and transition verification.
This document records investigation, not completion.
