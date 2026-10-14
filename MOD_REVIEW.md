# Mod review (2026-10-13)

Reviewed committed `main` at `c1a6adc` in the isolated `review/mod-correctness`
worktree. Excludes `src/mods/framerate.c`, its simulation hooks/settings, and
Claude's dirty runtime-entry/toolkit work. The draft ImGui branch is separate.
This is a source review with focused regression checks, not gameplay certification
or a measured performance comparison.

## Changes

- **Collision cell guard:** the dimension product could overflow to a small
  accepted value (`0x80000001 * 2 * 1` became two cells). Pointer checks covered
  only the start of headers/lists/grids. Bound multiplication before computing
  the product and validate complete guest-address ranges before accessing them.
  Existing face replacement, range clamping, and cache-hit behavior remain.
  These checks bound the sanitizer's access; they do not repair arbitrary corrupt
  headers, prove host-page commitment, or fix the underlying decompression issue.
- **Offline network:** the default-on mod forced readiness even with
  `RECOMP_NET=nat`. Delegate to the original method when networking is enabled,
  matching the launcher description. Cache the decision once, like other mod
  settings; use the same unset/empty/`0` definition of off as the pinned NIC.
  This restores the real readiness wait; it does not establish working online
  play, and network-enabled startup still needs an in-game smoke test.
- **README:** generic improvements should be cleaned up, tested, and proposed to
  upstream XboxRecomp; title-specific hooks stay here.

## Coverage and decisions

| Mod | Source / integration reviewed | Decision |
| --- | --- | --- |
| Widescreen | `widescreen.c`, projection caller/stack handling, HUD hooks | Keep native projection wrapper and caller-owned argument restoration. No speculative camera rewrite. |
| Field of view | `widescreen.c`, launcher values | Retain cached scale and race-camera restriction. No per-frame environment parsing added. |
| Draw distance | `draw_distance.c`, CMake definition-only renames | Caller restrictions preserve temporary pass save/restore. Facade pools and particle budget increase workload intentionally; no default/LOD changes without matched captures and timings. |
| Smooth skyline | `mods.c`, pinned runtime B-spline setup/shader/feedback path | Filtering can require feedback copy/resolve. No shader or quality change without GPU evidence. |
| Menu mouse | `menu_mouse.c`, renderer rectangle API | Bounded rectangle/row/texture scans. Focus uncertainty returns unknown rather than inventing navigation. Keep heuristics until a reproducible layout failure. |
| Developer tools | `dev_tools.c`, launcher bindings, prepared-script documentation | Guest-thread execution and register restoration retained. Startup flags/hotkeys report intended state, not confirmed Lua effects; no guessed state synchronization added. |
| Teleport debugger | `teleport.c`, existing contract, panel/pipe paths | Native reset/register handling retained. Shared mapping-cache and request identity concerns below; avoid competing changes with the draft overlay PR. |
| Offline network | `offline.c`, launcher network option, pinned `nvnet.c` | Fixed bypass incorrectly applying to enabled networking. |
| Collision cell guard | `collision_fix.c`, CMake hook | Fixed overflowing dimensions and incomplete address-range checks. Sanitization still runs only on newly decoded successful cells. |
| Disc check bypass | Required registry entry, generator recipe | Always-on compatibility entry; not a meaningful user-toggle optimization. No bypass behavior changed. |
| Skip intro | `skip_intro.c`, movie hook renames | Boot-movie filtering and original skip predicate retained; story/attract movies remain outside the override. No loader or movie-loop rewrite. |
| Translation corrections | `mm3_asset_trace.c`, `mm3_stream_compat.c`, `recomp_manual.c`, `mm3_memory_tails.c`, `mm3_runtime_entries.c`, CMake/generator ownership | Preserve recovered entries and ABI boundaries. The diagnostic switch is partial, as its description states. No broad deduplication of native ABI repairs or push-buffer synchronization changes. |

## Deferred risks

- `teleport.c:mapped` caches the last mapped host range in process-wide statics,
  but both game and pipe threads call it. The cache is unsynchronized and can
  become stale after a protection/allocation change. A focused follow-up should
  establish allocation lifetime and remove cross-thread cache sharing. The draft
  overlay also changes teleport requests, so consolidate that work there.
- `teleport.c:request_teleport` on main does not bind requests to a car/world
  identity. The draft overlay PR adds identity and expiry guards. Do not duplicate
  those changes here; verify transitions in that PR.
- `widescreen_hud.c` uses quadratic piece clustering and shares HUD setup state
  between guest and renderer threads. No new failure or timing regression was
  reproduced here. Validate level transitions and profile representative HUD
  batches before changing batching or introducing synchronization.
- The offline readiness bypass is deliberately narrow. Enabled networking may
  now expose an existing readiness stall that the unconditional bypass hid.
  Test network-off startup and network-enabled initialization before merging.

## Reproduction and validation

The collision overflow assertion failed against the original implementation.
The offline `nat` test also failed against the original implementation. Both
pass after the fixes. Tests compile the actual mod sources and canonical project
runtime header; the original game readiness function is stubbed. No game dump,
generated game code, assets, or running game are needed.

With Visual Studio's bundled CMake/CTest on PATH:

```powershell
cmake -S tests/mod_contracts -B build-mod-contracts
cmake --build build-mod-contracts --config Release
ctest --test-dir build-mod-contracts -C Release --output-on-failure
```

MSVC Release: **6/6 pass**, assertions enabled. Coverage includes overflowing
dimensions, header/list/grid ranges crossing into an uncommitted guard region,
face replacement/range clamping, cache-hit behavior, and network unset/`0`/`nat`/
`1` plus mod-off delegation. `git diff --check` passes.

No full-game build, live gameplay, network connection, or performance benchmark
was run for this review. Normal offline/online startup and Paris collision-cell
loading/driving remain pre-merge smoke checks. Main files/processes/saves were
not modified; no assets were copied or changed.
