# Interpolation experiment discarded

The user rejected interpolation on 2026-11-06 after broken gameplay. The added replay/recording, transform traces and interpolation-specific tests have been removed. Fixed-tick timing and the controller startup fix remain. Earlier interpolation evidence is archived only in ignored scratch space; it is not acceptance of the current build.

# Fresh timing implementation: scope and validation

2026-11-01. This is the first implemented stage of the [replacement plan](FRAMERATE_DECOUPLING_PLAN.md), not completion of smooth arbitrary-FPS gameplay or game-wide fidelity validation.

## What changed

The replacement schedules the **complete native main-loop iteration** at a default 30 Hz, rather than repeating only `sub_001E7590`. The native state machine, world update, native render, input-related post-render callbacks and end-of-frame virtual dispatch retain their original order and execute once per tick. The native timer still maintains its counter/history; the wrapper publishes the logical tick's raw, clamped, averaged and cumulative elapsed time for the main-loop callbacks.

Between ticks the host presents a saved completed image. Guest render queues and pointers are never rewound. The runtime saves each completed flip before the guest can reuse its surface. In this manual presentation mode the caller owns frame-limit deadlines; the renderer omits its additional limiter wait, so low display caps do not hold the game thread for a whole display interval. Device/VSync waits and actual rendering work can still block.

This makes presentation requests independent of native tick frequency. **It does not yet interpolate motion:** native drawing remains once per logical tick, and intermediate displayed images are repeats. A high presentation cap alone does not produce high-refresh camera, wheel, pedestrian or vehicle motion.

Implementation:

- `src/mods/timing.c`: main-loop boundary, timing publication, lifecycle reset and diagnostic trace. AI dispatch is observed and forwarded; no shared every-N-call gate.
- `src/mm3_tick_clock.c` / `.h`: host monotonic deadlines, bounded catch-up and missed-display handling.
- `tools/xboxrecomp/src/video/nv2a_sdlgpu.c`, `nv2a_d3d11.c`, `nv2a_d3d11.h`: generic manual presentation mode and saved-image handoff. No MM3 addresses or timing policy were added to XboxRecomp.
- `CMakeLists.txt`: exclusive implementation selection. Default uses the replacement. `MM3_LEGACY_TIMING=ON` compiles the previous `src/mods/framerate.c` instead. Both cannot own the hooks in one build.
- Launcher: legacy “Draw every frame” queue experiment is hidden in replacement builds; non-30-Hz rates are described as experimental.

The previous implementation remains intact for comparison. Its `MM3_SMOOTH` and `MM3_RENDER_EVERY_FRAME` settings do not activate legacy behavior in replacement builds.

## Settings and lifecycle policy

| Setting | Behavior |
| --- | --- |
| `MM3_MOD_FRAMERATE=1` | Enable replacement timing; default on |
| `MM3_MOD_FRAMERATE=0` | Native frame-driven timing, for controlled baseline comparisons |
| `MM3_SIM_HZ` | Integer world/native-loop rate from 15 through 120; default/invalid fallback 30. Launcher offers 30, 60 and 90. Every non-30 rate is experimental. |
| `RECOMP_FPS_LIMIT` | Independent presentation request cap; 0 is uncapped; `auto` follows the runtime refresh selection |
| `RECOMP_VSYNC` | Retains the existing host presentation policy |
| `MM3_TIMING_CSV` | Optional writable trace path: tick ID, state/scene identities, pause flag, dt, dropped time, world/AI call counts and presentation-request count |

Deadlines reset on game-owner, native state, scene or pause-flag changes. Catch-up is bounded to approximately 133 ms of outstanding native ticks; excess time is dropped and counted. Simulation time advances only for ticks actually executed. The new implementation does not inherit the old invalid-frame-delta-to-one-tick rule.

That bound is an overload policy, not a promise of real-time simulation under any workload. The game still draws at the tick rate and uses a single game thread; an overloaded machine can fall behind. Live rate switching is not implemented. Direct clock consumers, asynchronous workers and additional lifecycle paths remain audit items.

## Verification

Build: Visual Studio CMake, `RelWithDebInfo`, SDL3 GPU backend, current root sources and current `tools/xboxrecomp`. Build output:

`build-full-sdl3-validation/RelWithDebInfo/mm3_recomp.exe`

The build directory previously pointed to another runtime checkout. It was explicitly reconfigured to this repository's `tools/xboxrecomp` before testing. A separate new build was stopped after unnecessarily recompiling dependencies; no running user game was stopped. The existing game at `build/RelWithDebInfo/mm3_recomp.exe` was preserved.

Focused tests:

- `tests/mm3_tick_clock_regression.py`: clock independence across host sample rates, overload bound, reset, invalid time and rate parsing.
- `tests/mm3_timing_regression.py`: actual replacement wrappers with deterministic host timing and a guest stack fixture; verifies state/world/AI cadence, stack cleanup, cumulative time, pause reset and disabled-mode timer forwarding.
- `tests/mm3_launcher_settings_regression.py`: nine isolated settings cases, including exclusion of the legacy render option.
- `tools/xboxrecomp/tests/video_backends/sdl_present_pacing_contract.py`: real extracted presentation/flip paths, manual-mode saved-image submission, no duplicate limiter wait, normal pacing retained.
- `tools/xboxrecomp/tests/video_backends/d3d11_present_pacing_contract.py`: equivalent D3D11 control-flow contracts, hidden flip writeback/composition and explicit presentation.
- `python -m unittest discover -s tests -p test_game_harness.py -v`: 12 harness regressions passed, including isolation and owned-process cleanup.

The C tests ran with MSVC via `scripts.devenv.msvc_env()`. Tests and build logs are under `conformance_tmp/timing-replacement/` (`focused-tests.log`, `harness-tests.log`, `build-final.log`). These are focused proofs, not a whole-repository suite or original-Xbox conformance result.

## Runtime evidence and boundaries

Every run used `tools/game_test.run.Session`, explicit extracted assets, a copied quiescent save template, private writable saves, named-pipe input/telemetry and owned-child cleanup. The XBE SHA-256 was:

`2b04b66c43e7f37bbcebbfb5b72ccb96a2aa99cc2c60c53eb7d3530bcc2a3d79`

The short menu sequence entered the saved profile's **box-delivery scenario**. A later capture explicitly displays 'Deliver your boxes before Mathilda'; the previous Capture the Flag label was an unsupported inference from the HUD and is withdrawn. The exact native mode enum has not yet been recorded. These runs must not be described as Cruise, Checkpoint, Capture the Flag, or complete Undercover-mode validation. Three trials teleported to the same Paris straight, held throttle for four seconds, recorded player/NPC tracks and captured the presented image. This is a bounded functional comparison with wall-time samples; it does not enforce a statistical handling-parity tolerance. Trial variation and collisions/obstacles prevent promoting it to a precision acceleration benchmark.

Initial matched binary SHA-256:

`97a17634f7083b31a51c9bfc5f02b5a00757507f691e10419dd8ec0830f76527`

Passed evidence:

- `matched-baseline-30/run-smdu0reb`: replacement disabled, 30 FPS cap; driving and pause/resume.
- `replacement-30-at-15/run-za5nyvlf`: 30 Hz with 15 FPS cap; driving and pause/resume.
- `replacement-30-at-60/run-vhrgtglo`: 30 Hz with 60 FPS cap; driving and pause/resume.
- `replacement-30-at-144/run-_h5zha7o`: 30 Hz with 144 FPS cap; driving and pause/resume.
- `replacement-30-uncapped/run-j2hr2vdt`: 30 Hz, frame cap 0 and VSync 0; driving and pause/resume.
- `replacement-60-at-144-retry/run-0czi8lmn`: experimental 60 Hz; driving, two-second rigid-body transform freeze while paused, resume, confirmed race restart, fresh player motion and capture afterward.

All paths above are beneath `conformance_tmp/timing-replacement/`. The four initial 30 Hz replacement runs contain 4,347 flushed tick records in total: each reports dt 1/30, one world call and one AI dispatch per recorded tick. Those counts establish this loop's cadence, not independent classification or correctness of every NPC subsystem.

The first 60 Hz restart script failed because it confirmed the dialog's default **No** selection. The capture showed the confirmation UI; correcting the script to select **Yes** produced the passing restart run above. The failed run and its result remain under `replacement-60-at-144/run-m6irwjw5`; it is not counted as a pass.

Final presentation-wait fix binary SHA-256:

`16a4c9ed1f3e530da00c36ec6af3b896ba0a9a13e1c433c03326a54e887b7d8a`

Final-build runs passed:

- `final-30-at-3/run-42o05y6a`: 30 Hz native ticks with a 3 FPS presentation cap.
- `final-30-at-144/run-yk45l_ri`: 30 Hz native ticks with a 144 FPS presentation cap.
- `final-75-at-144/run-acsf43j4`: experimental 75 Hz native ticks with a 144 FPS presentation cap, demonstrating a non-integer multiple of the legacy 30 Hz selection.

Each includes three driving samples, unchanged rigid-body transform bytes over a two-second pause, resume, explicit Yes confirmation for race restart, fresh post-restart motion and an inspected gameplay capture. Every recorded tick reports one world call and one AI dispatch; the final 30 Hz traces publish 1/30 and the 75 Hz trace publishes 1/75 seconds. Loading/restart stalls and catch-up limits affect wall-time throughput; a fixed logical dt is not a claim that all loading intervals sustain the target tick frequency.

The local comparison driver is `conformance_tmp/timing-replacement/run_compare.py`; its menu path and teleport pose depend on the copied profile. The evidence directories contain the exact recorded scenarios, environment, executable identity and raw tracks. The driver uses the existing Session API and is an audit artifact, not a general automatic mode-selection tool.

Internal trace and frame CSVs establish logical call counts and submission diagnostics. Ignore incomplete final CSV rows left by owned-process termination (the 60 Hz retry has one); `summary.json` records complete-row counts separately. No PresentMon/ETW or instrumented Tracy capture was collected here, so presentation cap selections are not claims of measured display FPS or performance capacity. D3D11 received contract coverage; current native gameplay validation used SDL3 GPU.

## Still required for the full goal

1. Interpolate identified completed vehicle/camera/animation state for smooth between-tick motion. Do not reactivate blanket GPU-constant blending or guest-global rollback as a shortcut. Translated-op replay now does this for paired draws (see the translated-op section); the recording-induced throttle defect above blocks enabling it.
2. Validate and normalize non-30-Hz gameplay: the 60 Hz trials changed some trajectories. Accepting a rate and running at that dt does not establish unchanged handling, NPC decisions or mission behavior.
3. Add deterministic tick-indexed input/event comparison, precision acceleration/braking/steering, collision/damage, heavy traffic/pedestrians, all race modes, Undercover, split-screen and selectable/playable DLC coverage.
4. Resolve remaining timer/counter/async consumers and lifecycle boundaries, and compare solver/NPC behavior with original-Xbox/xemu traces.
5. Verify physical audio/media, live two-peer networking, sustained overload, background suspension and graceful teardown. Owned harness termination does not prove graceful guest exit.

The fresh scheduler is implemented and has bounded gameplay evidence. The complete smooth arbitrary-frame-rate goal remains open.


## Interpolation investigation update

A subsequent diagnostic build adds a bounded, executor-thread transform capture in `src/mods/render_policy.c`, called from the existing private-vertex-upload and frame hooks in `widescreen_hud.c`. It does not change guest state or enable interpolation. Set `MM3_TRANSFORM_TRACE` to an output path and create `<path>.flag` to capture the next two native frames, capped at 4,096 upload records. With the setting absent it performs no file I/O.

Capture format `MM3TRF1` is eight magic bytes followed by fixed 5,280-byte records: six little-endian uint32 fields (native frame, first vertex, count, stride, uploaded-program start, program version), one uint64 source-buffer identity, 136 four-word program slots, and 192 float4 constant registers. The buffer identity is process-local metadata, never dereferenced by the observer. It is not a stable entity identifier. The callback observes uploaded shader state; fixed-function draws can retain stale uploaded state, so this format alone does not prove which program executed.

Current diagnostic executable SHA-256: `fdcfcda68d7b8ee06e4170f78097cf86ea551f435cdd7b6df00eb1d5783559e1`.

Evidence: `conformance_tmp/timing-interpolation/run-0sslaumn/result.json` passed, with an inspected gameplay capture and subsequent measured player/NPC movement. `transforms.bin` contains 2,656 records from native frames 453 and 454 and is exactly 14,023,688 bytes. The native build and render-policy/overlay contracts passed. The capture is not a performance measurement; its synchronous diagnostic writes can perturb timing.

The uploaded-state capture contains 37 active-range bytecode signatures, 13 using relative constant addressing. Registers 96-99 form a common four-row transform candidate across many world-geometry shader paths; registers 58/59 hold the viewport scale/offset in inspected records. Other candidates include three-row transforms and indexed animation palettes. There are two distinct 96-99 matrices within each captured frame, which rules out blindly treating the whole frame as one camera pass.

The local `analyze.py` report is exploratory: register-level dependency propagation can overestimate dependencies, and relative accesses require dynamic index resolution. Do not use its candidate lists as an interpolation allowlist. The next integration work must identify actual shader execution/pass identity, component-level position dependencies, stable entity/pose identity and resource lifetime before enabling between-tick rendering. Blanket scalar blending remains disabled.


### Execution/pass-aware capture and camera interpolation helper

The current observer is a generic read-only `NvOverlayHooks.observe_draw` callback in both host backends. It supplies the actual vertex-program enable flag and target state, avoiding inference from stale uploaded state. MM3's capture writer now emits `MM3TRF2`: the v1 six uint32 fields plus program enable, colour target, clip width/height and position-attribute type/size, followed by the uint64 identity and the same raw programs/constants. Records are 5,304 bytes. The v1 artifact is retained as `transforms-v1.bin`.

The v2 capture `run-6i22ymik` passed native gameplay/movement and produced 2,624 records from native frames 414/415 (`transforms.bin`, 13,917,704 bytes). Its executable SHA-256 is `cc64a6b1ea0ff100f978e69a3ed66d8b1389bc379788acea889f15d8383653e6`. In the main colour target, the large nonzero 96-99 matrix group precedes 25 UI draws with zero matrices in both frames. Uploaded programs are enabled for these draws; zero matrices are not a second usable camera. Other render targets/passes still require explicit classification.

`camera_interpolate_view_projection` in `src/geometry/camera_math.c` now separates a row-major perspective/rigid-view matrix into camera orientation, eye position, projection scales and depth mapping. It interpolates orientation using shortest-path quaternion interpolation and eye position linearly, then rebuilds the matrix. It rejects non-finite, sheared, reflected, degenerate or incompatible-projection inputs without touching output. This avoids shrinking/shearing rotation bases through scalar matrix blending and rejects the observed zero UI matrices.

`tests/mm3_camera_interpolation_regression.py` passed analytic rotation/translation/endpoint/rejection cases and the actual dominant matrix pair from the v2 capture. Render-policy, overlay-hook and timing-wrapper contracts also passed. The helper is **not yet connected to presentation**: stable frame/object identities, safe render-only replay and animated-object pose handling remain required. These tests establish the mathematical component and captured matrix compatibility, not visible smooth gameplay.

### Replay isolation regression work

The production executor regression harness reproduced two additional defects before fixes: `PbState` omitted the `s_reg` register shadow (stencil state failed restoration), and the early direct semaphore target path wrote guest memory even with `s_replaying` set. The snapshot now saves/restores that shadow, and the direct semaphore path returns without publication during replay. Added assertions cover both paths; replay, allocation failure, first-frame, and texture-address cases pass in the compiled production-source fixture. The fixture also gained the missing `xbox_FramebufferWindowSetAA` stub required by the current executor.

These fixes do not establish replay safety. The subsequent replay isolation work below addresses report publication, kickoff markers, and the replay flip path. Remaining review items include extension-hook state, software target writes, query state, and render-target/texture lifetime. Replacement timing still uses repeated intermediate images; semantic camera interpolation remains unconnected to presentation.

### Render-only replay presentation

Production-source fixture checks reproduced kickoff-register overwrite and GET_REPORT guest-memory publication during replay. Replay now skips both publications and does not consume the visibility query through GET_REPORT; native commands still publish the marker, report count, and completion. A replay FLIP_STALL previously reached native framebuffer scanout; a regression stub trapped that call. Replay now exits through `nv2a_d3d_replay_present`, implemented in both SDL and D3D11. It uses an existing cached surface, omits native flip hooks, guest writeback, scanout changes, native flip clocks/statistics, backend flip callbacks and DMA holds. It does not synthesize a target from guest memory. Backend extracted-production-body tests prove manual replay presentation avoids writeback and native flip timestamp/count changes; both before/after pacing policies pass. The full SDL game build passed (`conformance_tmp/timing-interpolation/build-render-only-replay.log`).

These are isolation prerequisites, not proof of complete replay safety or smooth interpolation. In particular replay drawing can still affect cached targets and visibility-query state, and extension hooks require an explicit state contract. Interpolation remains disabled in the replacement scheduler.

Post-change bounded runtime: `conformance_tmp/timing-replacement/replay-isolation-30-at-144/run-8n7e2fha` passed three driving trials with player and NPC movement, raw rigid-body pause equality, resume, restart, and post-restart movement. The final `capture-006.bmp` was visually inspected and shows gameplay in the box-delivery scenario. Executable SHA-256: `4644176f99c9b663355f00976d9b76b779142dbadae1557fde4c8497e3ada545`. Timing rows: {'rows': 1667, 'dt': {'0.033333333': 1667}, 'world_ai': {'1,1': 1667}}. This checks the replacement repeated-image path; it does not exercise enabled replay interpolation or prove external display cadence.

### Visibility-query suspension around replay

Both renderer backends now suspend native visibility counting for the complete replay transaction. Entry collects only already-pending native results and remembers whether counting was active. During replay, enable/clear/read APIs cannot mutate that native query or count; replay geometry is outside the counting interval. Exit restores the prior active/disabled state without clearing its accumulated count. The executor brackets each accepted replay with this API and refuses replay on the software rasterizer because that path writes directly into guest targets.

`tools/xboxrecomp/tests/video_backends/replay_queries_contract.py` compiles the actual enable/clear/read/suspend function bodies from each backend against deterministic GPU-completion mocks. It verifies a native count of 11 plus 7 pending samples survives replay as 18, replay clear/enable/read do not alter it, and subsequent native work contributes another 5 to produce 23. Active and disabled query restoration, duplicate begin, and normal native clears are covered. Both backend cases pass. The executor fixture verifies balanced query suspension/restoration around real recorded-frame replay; replay, allocation, first-frame, and texture cases pass. Existing presentation contracts pass in both pacing modes; full SDL build passed (`conformance_tmp/timing-interpolation/build-replay-queries.log`). These checks do not prove GPU-driver query fidelity or smooth presentation.

The next unresolved extension-state issue is concrete: `src/mods/render_commands.c` keeps a partially assembled fog command in function-static words/next state, while the executor keeps `s_nop_pending` outside `PbState`. Replay can start or end while the next native command is partially assembled. That host parser state needs a snapshot contract, including frame-start state, before enabling replay in the replacement scheduler. Cached render-target/texture lifetime and frame/entity identity remain unresolved too.

### Rendering-command parser snapshots

The production executor fixture reproduced loss of `s_nop_pending` across a GPU-state restore. `PbState` now includes that flag and optional extension-parser bytes. The generic runtime accepts a pure host-state copy callback registered after the NOP hook. Each frame-start and temporary replay snapshot captures that state; restoration rejects snapshots from a different hook-registration generation before mutating GPU or parser state. Allocation failure refuses the snapshot without modifying the current parser.

MM3's fog decoder now keeps words/next in an explicit `FogParser`, registers its copy callback, and resets on initialization. The root fog decoder contract passes a partially assembled native packet through an intervening complete replay packet and verifies the resumed packet produces the original values. The executor fixture covers failed allocation/retry, extension byte restoration, stale-generation rejection without current-state mutation, and preservation of next-frame extension state through actual recorded-frame replay. Replay/allocation/first-frame/texture cases, root fog/overlay contracts, both query-isolation contracts, and the full SDL build pass (`conformance_tmp/timing-interpolation/build-extension-state.log`).

This resolves the parser-state prerequisite described above. It does not resolve cached texture/render-target isolation or establish stable entity/frame matching. The replacement scheduler still repeats intermediate images; smooth interpolation remains unconnected.

Post-parser-change runtime: `conformance_tmp/timing-replacement/extension-state-30-at-144/run-29w83yva` passed three player/NPC movement trials, raw body-transform equality while paused, resume, restart, and post-restart movement. The final `capture-006.bmp` was inspected and shows gameplay. Executable SHA-256: `05e3efd0fb393943c8e44ebef20f4bfa40327fa50a6b3f04a41b4da9f7e5ed14`. Timing summary: {'complete_rows': 1662, 'dt': {'0.033333333': 1662}, 'world_ai': {'1,1': 1662}}. This is bounded evidence for normal replacement presentation; replay interpolation is still disabled.

### Isolated replay target/texture caches (SDL backend)

Replay on the SDL3 GPU backend now runs against scratch surface, depth and texture caches (`nv2a_d3d_replay_targets`, bracketed in `nv2a_pb_replay` after the query suspension). The native caches, current render/depth target pointers, texture byte/count accounting, surface sequence, AA state and scale are saved and restored. Scratch resources are released at exit, and scratch slots cannot cancel native readback. Begin is refused while a native writeback is pending, in which case replay is refused and native state is restored. The D3D11 backend returns 0 (replay refused there). This isolates replay clears/draws from images the next native tick uses. It does **not** reproduce the historical contents of render-target-as-texture inputs, so replay fidelity for those remains open. Nothing enables replay in the replacement scheduler yet.

Evidence: full SDL build passed (`conformance_tmp/timing-interpolation/build-replay-targets.log`; build with the VS-bundled CMake). Replay-query and both present-pacing contracts pass; the production-executor replay fixture (`run_replay_test.py`) passes. Gameplay `conformance_tmp/timing-replacement/replay-targets-30-at-144/run-kk1176fo` passed three movement trials, pause equality, resume, restart and post-restart movement; `capture-006` was inspected and shows gameplay. Executable SHA-256: `74634b26ece52f801941838b7b28dcac4dd38928507520e84cc970feee475cab`. Timing: 1,619 rows, all dt 1/30, world/AI 1/1. This run did not exercise the replay path (interpolation is off), so it shows only that the pointer indirection did not regress normal rendering. The fixture stubs the backend call, so the new SDL function itself has no automated test.


## 60 Hz presentation without interpolation

The current SDL build submits retained game images at the requested host presentation cadence while the fixed-tick game loop remains at 30 Hz. No intermediate poses or guest render replay are generated. The previous title-bar `FPS` counted native guest flips only, so it continued to display 30 even when repeated host presentations occurred at 60.

SDL now notifies the framebuffer statistics module after acquiring and submitting a host swapchain image, including repeats. The title reports `Present` (host submissions) separately from `Game` (new guest frames); unsupported backends retain their original FPS display. Neither counter is a physical scanout measurement. The counter fixture verifies 60 host submissions/30 guest flips and 60 host submissions/zero guest flips; the presentation fixture verifies a repeated image increments the host count without native pacing or guest-frame advancement. Both contracts and the full SDL build pass.

Manual run PID 90976, `conformance_tmp/manual-play/run-s_k551kk`, was relaunched at requested 60 FPS presentation/30 Hz simulation with controller support and no interpolation variables. Its live title reports `Present: 60.0 | Game: 30.0`. Complete timing rows show `dt=0.033333333` and one world/AI call per tick. The existing frame CSV records host submissions; this proves the submitted-image cadence, not 60 distinct motion poses or physical monitor scanout. Thirty-hertz motion is intentionally repeated across host refreshes.
