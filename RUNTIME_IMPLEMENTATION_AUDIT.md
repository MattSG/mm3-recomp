# Implementation audit, 2026-10-17

Work in progress. This inventory distinguishes source defects from paths reached
by the bounded game run. A successful drive does not establish complete audio,
networking, translator or renderer correctness.

## Fresh runtime evidence

`tools/game_test/run.py` owns its process, input pipe and private saves. The
fresh-profile scenario adds the profile confirmation to `driving.json`; it
reaches Mathilda's Paris mission, not the saved-profile Cruise route.

- Baseline: `build/audit-20261017/runs/run-ej7nf63t`, passed in 44.266 s.
- Diagnostics/frame-limit fix: `build/audit-20261017/verified-runs/run-a77egagu`,
  passed in 44.203 s with explicit `RECOMP_FPS_LIMIT=0`, `RECOMP_VSYNC=0`.
  Startup reports `fps=0`. Motion checks pass; baseline capture was inspected.
- No unknown untranslated instruction, unresolved stub or taken dead-branch
  diagnostic appeared in the baseline. Four known hardware replacements did.
- `NtFsControlFile(0x90020)` was rejected. This is GET_COMPRESSION; unsupported
  per-stream compression on FATX can correctly fail. Do not invent a successful
  compression result merely to remove a log line.
- Harness defaults include the DSP ACK override. These runs do not establish
  native DSP/audio fidelity. Frame CSV data is not external performance proof.

Build evidence is local and ignored; it is not committed source. Repeat the
documented commands after final integration rather than treating these runs as
proof of subsequent changes.

## Corrections and checks

| Area | Change | Evidence |
|---|---|---|
| CPU diagnostics | Unknown untranslated instructions stop with the exact site rather than silently continuing after a global logging budget. Known hardware replacements remain identified. | Repaired `mm3_cpu_diagnostics_regression.py`: 32 original-XBE/Unicorn comparisons plus known-site, mismatched operand/instruction and trap checks pass under MSVC. |
| Indirect-call regression | Repaired another stale fixture that omitted the real suppression-accounting helpers. | Original-XBE call decoding, safe diagnostic reads, guest-state preservation and exact suppressed-event totals pass under MSVC. |
| Pacing | Explicit zero is unpaced; positive integer rates above 90 remain usable. Overflow, negative and malformed input fall back to 60. | `mm3_frame_limit_contract.c` passes under MSVC; uncapped driving run above passes. |
| Offline readiness | Removed the hook that forced readiness and its obsolete toggle/test. The original guest path reaches driving; input must wait until initialization completes. | `offline-original/run-25fe4cj6` passed in 63.922 s with an initial wait. `offline-original-ack/run-q8jik056` passed in 64.422 s using actual acknowledgement polling. Rebuilt hook-free default run `integrated-default/run-7neh3vjb` passed in 62.750 s. All 11 harness regressions pass, including delayed acknowledgement. |
| Replay snapshot | Allocation failures are checked before any copy, partial buffers freed and subsequent requests can retry. | Real executor test injects failure at each of six allocations, then verifies snapshot/restore, texture addressing and replay semaphore isolation. |
| Native frame | Existing SDL game and launcher request decorated OS windows. Custom RmlUi controls are a separate concern. | Main hidden-render/close smoke passes. SDL Windows launcher style `0x16ca0000` and resizable game style `0x16cf0000` verified on actual owned HWNDs, with game capture. SDL proof is committed on `sdl3-gpu-linux` as `SDL_NATIVE_WINDOW_PROOF.md`. Linux compositor appearance remains unverified. |

The rebuilt main game also passed `MM3_RENDER_EVERY_FRAME=1` in 53.063 s
(`integrated-every-frame/run-74r9ik9u`) and `MM3_SMOOTH=1` in 64.375 s
(`integrated-smooth/run-mzdstqnj`). All three captures were inspected. These
bounded drives do not prove the queue heuristic correct across every scene.
Nine isolated launcher settings cases and the native-frame smoke also pass.

The first isolated generator candidate passed syntax checks on all 51 C files
but added 103,092 lines and increased unresolved flag branches/stubs. It was
not promoted; the default pin remains `967ec30f`. A refined shared translator
passes 478 tests, 17 skips and 69 subtests; full generation validation continues.

Generic runtime findings and crypto vector sources are tracked in
`tools/xboxrecomp/docs/technical/runtime-audit-20261017.md`.

## Open work

- Recover legitimate overlapping x86 branch targets in the shared translator;
  preserve both streams and explicit successors. Regenerate and validate MM3
  after the generic fix, including unreachable-tail diagnostics.
  The current baseline has 5,923 legacy no-op markers and zero calls to
  `RECOMP_DEAD_BRANCH`; merely replacing its header does not guard those sites.
  A dedicated pin backport preserves the proven lifter byte-for-byte. Native
  before/after tests demonstrate the old pin takes the wrong path while the
  corrected pin matches Unicorn on both streams.
- Unresolved generated functions still guess stack cleanup while omitting their
  bodies. Recover actual reachable functions rather than claiming success from
  a stub. Failed translations must not emit silently empty functions.
- Shared flag provenance and generation failure handling: the MM3 flag-join
  postprocessor still permits unresolved `_flags` branches. Repair their actual
  semantics and replace tests which merely bless emitted text.
- `MM3_RENDER_EVERY_FRAME` restores changed words from a broad global range
  based on pointer-looking values. Identify and preserve actual queue ownership;
  cover pause, menus, race transitions and driving. The bounded snapshot arrays
  now use static storage, removing their unchecked allocation paths.
- Prove CLI/STI clock-sampling semantics against the runtime's interrupt/thread
  model; host interrupt masking alone is not an implementation of guest IRQs.
- Finish crypto, settings, MMIO aliases, I/O dispatch, POSIX timers/waits/input,
  device getters/reset and the remaining runtime inventory. Do not conflate
  explicit unsupported errors with fabricated successful implementations.
- Validate the SDL branch, restore a clean working tree through reviewed commits,
  and remove temporary audit caches after preserving the final evidence report.

Primary compression contract:
[Microsoft FSCTL_GET_COMPRESSION](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fsa/80c21e17-6ad4-48b3-b287-a63c3e7edb94).
