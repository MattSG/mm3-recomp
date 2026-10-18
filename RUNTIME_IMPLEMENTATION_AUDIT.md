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

### Continuation evidence, 2026-10-18

- POSIX wait-all now checks readiness under every participating object's lock
  and consumes signals only when the whole set is ready. Shared readiness and
  acquisition helpers also serve single-object waits. The regression fails on
  the previous source at the lost auto-reset-event signal; the new source passes
  event, semaphore, cross-thread mutex, auto-reset timer, delayed acquisition,
  reversed lock order and wait-any ordering checks. Independent Release build
  on WSL `mm3-gpu-test` passed ten consecutive CTest runs. Luna's Ubuntu run
  also passed with ASan/UBSan. These checks cover host wait acquisition, not
  periodic timers, APC delivery or abandoned mutexes. Windows uses native waits.
- All 11 game-harness regressions pass on Windows. These exercise the harness
  and its fixture process, not a fresh gameplay run of the changed runtime.
- After rebuilding main with the IOSB marshaling fix, fresh-profile uncapped
  driving passed in 62.484 s at
  `build/audit-continuation/driving/run-pa2ntdnh`. Five-second telemetry records
  81.73 m player motion and 80.71 m AI motion; the capture was inspected and
  shows intact city geometry, cars and HUD. This is a bounded Windows gameplay
  check for that build, not proof of later changes or POSIX timer behavior.
- Translator investigation retained a concrete candidate at `0x00093D6D`:
  generated code initializes `_flags` to zero at an overlapping branch entry,
  while the enclosing XBE stream sets flags at `0x00093D68`. The apparent
  vtable was found heuristically; no static reference to that table or direct
  caller establishes live dispatch. Do not count this as a proven reachable
  defect or change its semantics without establishing entry ownership.
- I/O builder investigation confirmed direct casts from guest status blocks
  to host `XBOX_IO_STATUS_BLOCK`: guest storage is 8 bytes, while the x64 host
  struct is 16 bytes. Both builder bridges now use native temporaries and the
  existing two-field guest writer. Actual thunk-dispatch tests pass on MSVC
  Release and Linux Release, covering both fields, adjacent canaries, null
  IOSBs and stack cleanup. Linking the old bridge into the same Linux test
  fails all four field/canary assertions. The older ordinal-173 fixture now
  correctly expects rejection of an uninitialized contiguous alias. This fix
  does not implement IRP construction or driver dispatch. The shared IOSB
  writer still needs complete guest-range/write-permission validation with
  caller-appropriate error handling. Xbox definitions also contradict the
  bridge's `0x28` IRP header assumption and its two-argument `IoAllocateIrp`
  signature; request construction, driver dispatch and completion remain open.
- The allocator/initializer/device ABI slice now uses fixed-width guest
  layouts checked against nxdk commit
  `14d5ee97e73347c973f1f57b68b79ec08c9e77f2` compiled for its actual
  `i386-pc-win32` target: IRP size `0x68`, stack-location pointer at `0x5c`,
  device size `0x48`, device stack count at `0x1e`. Allocation initializes
  real guest IRP fields and stack storage; ordinal 59 consumes one argument.
  Device extensions follow the complete device header. Actual dispatcher
  tests cover initialization, stack cleanup, free accounting, size/canary limits
  and device queue/event fields. This supplies allocation and initialization,
  not request construction, driver callbacks or completion.
- POSIX timers now implement periodic expiration, monotonic relative due
  times, manual/auto-reset signals and APC delivery on the arming thread
  during alertable waits. Cancel preserves an expired signal and removes
  outstanding callbacks; rearm clears the old signal. Native Windows probes
  established that callback timers survive last-handle close until delivery
  or owner rundown. Three registered wait/timer CTests pass independently
  under ASan/UBSan. Cooperative service has a bounded polling delay; power
  resume is unsupported and reported through `ERROR_NOT_SUPPORTED`. Forced
  `TerminateThread` rundown remains unimplemented and is not covered by the
  normal owner-thread exit tests.
- Directory enumeration now prevents fixed-header/name overruns, serializes
  query/close, retains overflowed entries and EOF correctly, and captures the
  search mask once. POSIX metadata errors propagate; names are copied while
  the enumeration owns its `readdir` storage. Native Windows directory and
  renderer controls pass 6/6; Linux directory integration passes 1/1. Minimum
  length `0x48` follows the compiled nxdk structure size. These tests do not
  establish complete FATX behavior on original hardware.
- Final combined Windows build passed extended fresh-profile driving in
  72.797 s (`final-driving/run-81609qm4`). Captures show intact geometry and
  continued motion after two Start presses, but those presses did not visibly
  open pause UI: this is not pause/resume proof. A copied save from that owned
  run then passed saved-profile Cruise with `MM3_RENDER_EVERY_FRAME=1`,
  `RECOMP_FPS_LIMIT=0` and `RECOMP_VSYNC=0` in 56.328 s
  (`final-every-frame/run-jnagam6a`), including 95.05 m player movement and a
  normal inspected capture. Evidence is under `build/audit-continuation`.
  Audio/DSP fidelity and general queue ownership are still unproven.

Local runtime commits: `af7fa42` (I/O guest ABI), `712554a` (waits/timers),
`cb4206a` (directory bounds and ownership). Independent bridge CTest passes
on Windows and Linux; directory tests pass on both; final wait/timer sanitizer
CTest passes 3/3. Old-source controls fail the lost-signal, IOSB canary, IRP
initialization/stack and zero-length directory-output checks.

References: [Win32 wait-all contract](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects),
[nxdk Xbox kernel ABI](https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h).

Reproduce the focused POSIX check:

```sh
cmake -S tools/xboxrecomp/tests/platform_wait_all -B /tmp/mm3-wait-all -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/mm3-wait-all
ctest --test-dir /tmp/mm3-wait-all --output-on-failure
```

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
  Investigation confirms the 76-dword span `[0x3C5680,0x3C57B0)` is not a single
  owned queue: generated functions `sub_0011C742`, `sub_00138FB6` and
  `sub_0014BAE9` write separate state inside it. A direct-call closure of the
  main render does not establish those writers' runtime ownership because
  indirect calls remain. Trace write sites and before/after values during
  ticked and repeated renders before replacing this heuristic; substituting
  host-frame repeat would not implement the requested every-frame rendering.
- Prove CLI/STI clock-sampling semantics against the runtime's interrupt/thread
  model; host interrupt masking alone is not an implementation of guest IRQs.
- Finish crypto, settings, MMIO aliases, I/O dispatch, POSIX timers/waits/input,
  device getters/reset and the remaining runtime inventory. Do not conflate
  explicit unsupported errors with fabricated successful implementations.
- Implement the reached `FSCTL_DISMOUNT_VOLUME` request against emulated volume
  ownership and host-file caching; never dismount the host filesystem itself.
- Final evidence is archived at
  `C:\Users\Matt\Documents\MM3-Recovery-20261017\Audit-evidence-20261018`;
  earlier temporary audit worktrees and build caches were removed. Current
  continuation evidence/builds remain at `build/audit-continuation`; owned
  temporary RED-control worktrees were removed.

Primary dismount contract:
[Microsoft FSCTL_DISMOUNT_VOLUME](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_dismount_volume).
Numeric code verified against Windows SDK10.0.26100.0 `um/winioctl.h`.
