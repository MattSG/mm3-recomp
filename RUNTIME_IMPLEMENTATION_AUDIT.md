# Implementation audit, 2026-10-18 (updated)

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
- `NtFsControlFile(0x90020)` was rejected. The Windows SDK identifies this as
  `FSCTL_DISMOUNT_VOLUME` (filesystem function8), not GET_COMPRESSION
  (function15, `0x9003c`). Volume flush/dismount semantics remain unimplemented;
  returning success without doing that work would be another false result.
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
| Pushbuffer replay state | Capture the complete GPU/VSH/texture state at frame start on the executor thread, after first-use defaults. Replay loads that state and restores the in-progress next-frame state afterward. Incomplete streams are rejected when interpolation starts mid-frame, methods are skipped during disable/expiry and vertex capture expires mid-dispatch. Recorder allocation failures reject replay while normal execution continues. | Real executor tests verify inherited clear color, next-frame restoration, actual `NV097_FLIP_STALL` boundaries, the first initialized frame, stale pairing-mask reset, disable/skip/re-enable and deterministic 250 ms timeout cases. Fault injection covers six snapshot allocations and recorder command/constant/vertex buffers. Focused MSVC CTest passes 4/4. This proves state isolation and rejection paths, not visual correctness across scenes. |
| SDL sampler lifetime | Full-cache eviction preserves sampler objects already acquired by the active draw through the mid-setup flush and subsequent bind. | Focused Windows CTest passes 4/4 including replay carry; Linux selector test builds with `-Wall -Wextra -Werror` and passes, while its legacy eviction control fails on the pinned victim. SDL Linux memory mapping test passes 1/1 after recompiling the shared executor. SDL Release `mm3_recomp` build passes against the same generated C source directory as main. |
| Native frame | Existing SDL game and launcher request decorated OS windows. Custom RmlUi controls are a separate concern. | Main hidden-render/close smoke passes. SDL Windows launcher style `0x16ca0000` and resizable game style `0x16cf0000` verified on actual owned HWNDs, with game capture. SDL proof is committed on `sdl3-gpu-linux` as `SDL_NATIVE_WINDOW_PROOF.md`. Linux compositor appearance remains unverified. |

The rebuilt main game also passed `MM3_RENDER_EVERY_FRAME=1` in 53.063 s
(`integrated-every-frame/run-74r9ik9u`) and `MM3_SMOOTH=1` in 64.375 s
(`integrated-smooth/run-mzdstqnj`). All three captures were inspected. These
bounded drives do not prove the queue heuristic correct across every scene.
Nine isolated launcher settings cases and the native-frame smoke also pass.

The first isolated generator candidate passed syntax checks on all 51 C files
but added 103,092 lines and increased unresolved flag branches/stubs; it was
not promoted. The refined commit `34330f4ae0d2c7dcea1d1173c202f18620bd3291`
is now pinned in main and SDL. It passes 478 translator tests,
17 skips and 69 subtests; generation produced 26 dead-branch guards and 345
unresolved functions, with no baseline unresolved functions removed. Its
replay-fixed smooth+uncapped main run `run-e46_z31k` passed with three normal
captures. The SDL candidate run `run-3y069j9u` also passed with three captures
showing normal geometry and actual driving/AI motion. These bounded runs prove
the tested native gameplay path, not general interpolation correctness. Full
logs and captures are archived at
`C:\Users\Matt\Documents\MM3-Recovery-20261017\Audit-evidence-20261018`.

Generic runtime findings and crypto vector sources are tracked in
`tools/xboxrecomp/docs/technical/runtime-audit-20261017.md`.

## Open work

- Reachable overlapping x86 control-flow stream recovery is validated by the
  refined translator tests, 26 generated dead-branch guards, native before/
  after tests against Unicorn, and the bounded main/SDL gameplay runs above.
  Remaining translator work is unresolved function bodies and flag provenance:
  the candidate still has 345 unresolved functions and removed none from
  baseline. Recover actual reachable bodies and stack cleanup, and repair the
  `_flags` joins; stubs or text-only checks do not establish those semantics.
- `MM3_RENDER_EVERY_FRAME` restores changed words from a broad global range
  based on pointer-looking values. Identify and preserve actual queue ownership;
  cover pause, menus, race transitions and driving. The bounded snapshot arrays
  now use static storage, removing their unchecked allocation paths.
- Prove CLI/STI clock-sampling semantics against the runtime's interrupt/thread
  model; host interrupt masking alone is not an implementation of guest IRQs.
- Finish crypto, settings, MMIO aliases, I/O dispatch, POSIX timers/waits/input,
  device getters/reset and the remaining runtime inventory. Do not conflate
  explicit unsupported errors with fabricated successful implementations.
- Implement the reached `FSCTL_DISMOUNT_VOLUME` request against emulated volume
  ownership and host-file caching; never dismount the host filesystem itself.
- Preserve the final evidence report and restore the reviewed working trees;
  audit caches remain in use.

Primary dismount contract:
[Microsoft FSCTL_DISMOUNT_VOLUME](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_dismount_volume).
Numeric code verified against Windows SDK10.0.26100.0 `um/winioctl.h`.
