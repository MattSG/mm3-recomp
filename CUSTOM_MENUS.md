# Custom main-menu and pause-menu pages

Read-only investigation, 2026-10-06. **Recommended approach: extend MM3's native
Meme UI graphs**, retaining its rendering, controller focus, sounds and Back
navigation. A separate Win32/ImGui panel would not become a native menu entry.

## Verified integration points

The PAL `Data/Data_hd.zip` contains binary **`MemeFile 2.0`** resources:

| Context | Resource | Proposed change |
|---|---|---|
| Main menu | `Menu/Frontend/main/screenMain` | Add a “Mods” button beside the existing choices. |
| Frontend page template | `Menu/Frontend/options/screenOptions` | Clone its page lifecycle and navigation for `mods/screenMods`. |
| Race pause | `Menu/Pausemenu/menuSingleRacePause` | Add the same entry using the existing centered-button layout. |
| Cruise pause | `Menu/Pausemenu/menuSingleRaceCruisePause` | Add the entry separately; it is a distinct graph. |
| Pause page template | `Menu/Pausemenu/menuOptions` | Clone its transitions, tab stops and Back action. |

`Menu/Frontend/pathMain` is a `StackPathNode`, initially selecting `screenStart`.
The main screen already uses `PushPathAction` with `pathMain` and destinations
such as `options/screenOptions`. Pause navigation starts at
`Menu/Pausemenu/pathMenu`; its options graph uses `PushPathAction` and
`PopPathAction`. Post-race, system-link and online pause screens also exist;
they need explicit coverage if the entry should appear in those modes.

Buttons use `TabstopNode`, `MmxSimpleButtonNode` in the main menu and
`MmxCenteredSimpleButtonNode` in pause. Existing graphs demonstrate
`CallFunctionAction` (`functions/resumeRace`, `functions/movieBonus`) and
variable-backed option controls. `Menu/Debug/DebugMenu` also contains native
toggles and numeric controls; it is a useful reference, **not proof that its
bindings work in either target screen**.

## Smallest implementation path

1. Decode just the required Meme graph records and build a reader/writer that
   round-trips an unchanged resource first. These are binary graphs, not text
   configuration; replacing strings alone does not establish valid references,
   child counts or layout. Clone each context's existing Options page rather
   than introducing new widget classes.
2. Add one button and one small page in each context. Reuse existing styles,
   sounds, transitions and focus ordering. Follow the existing path-stack
   actions for entry/Back. Prefer localized labels through the existing string
   resources; `WstringData`, present in DebugMenu, is a possible initial label
   experiment whose button compatibility still needs checking.
3. Bind only the first required custom action. Register it in the same native
   function/variable namespace as the existing menu actions, separately for
   frontend and pause roots if necessary. A host callback needs a guest-callable
   thunk/dispatch entry, not a raw Windows function pointer. The exact Function
   binding constructor and registration contract still need tracing.
4. Test modified resources in a separate, backed-up game-data copy. Determine
   archive/loose-file precedence before assuming loose overrides work. An
   opt-in, named-resource override before deserialization could later preserve
   original archives, but that interception boundary is not established yet.

## Runtime seams and constraints

- PAL `0x0011935B` creates a menu root and calls loader `0x001C5E11`.
  Pause setup `0x00127284` supplies `PauseMenu` and `+Menu/PauseMenu/`.
- `CallFunctionAction` factory registration is referenced by `0x000C6817`,
  which calls `0x001C4243`. This identifies a type factory, **not yet the custom
  action-binding API**.
- [Frontend tracing](src/mm3_frontend_watch.c) already observes load names,
  factory calls and bindings. [CMakeLists.txt](CMakeLists.txt) exposes
  `MM3_FRONTEND_WATCH`; [its hook](src/mm3_frontend_watch_hook.h) provides call
  observation seams without hand-editing generated bodies.
- Run guest actions on the appropriate title thread and preserve register/stack
  state. Do not call the current blocking
  [teleport request](src/mm3_debug_teleport.c) from a native menu callback:
  it waits for the player-update hook, which may stop while paused. A pause
  teleport should enqueue without blocking and apply after Resume, or use a
  separately proven safe game-thread point.

Acceptance for the first slice: both buttons open their native page; controller
navigation and Back work; pause remains paused until Resume; one custom action
executes once; existing choices and screen transitions still work. Serialization,
binding and live insertion remain unproven. No code, assets, builds or running
instances were changed or controlled for this investigation.
