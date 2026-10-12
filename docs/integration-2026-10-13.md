# Upstream integration — October 13, 2026

Source integration is complete. The combined MSVC build, runtime reproduction,
scoped native/unit checks, live launcher initialization/normal close and final
Paris direction/acceleration smoke runs passed. Independent real-game traces
verified the default 30 Hz world rate with both requested render limits. Observed
driving was about 60 FPS when 60 was requested and 72 FPS when 90 was requested;
these results do not claim achieved 90 FPS or matched handling parity.
Extended coverage is listed below.

## Source snapshots and reproducibility

- Fork checkpoint: `342c912830e098d72169e243bc43618644a664d3`, saved and pushed
  before integration. Its full user/developer reference is preserved in
  [the historical fork guide](fork-reference-2026-10-13.md).
- Game upstream: `MattSG/mm3-recomp` at
  `c1a6adc26c439aedbd795478c648f570fe75dbce`.
- Public runtime base: `MattSG/xboxrecomp` at
  `12cb46d4eb7676d6fba2ad3b80175886485d4c50`, plus the regenerated
  [fork runtime patch](../patches/xboxrecomp-mm3.patch).
- Translation generator: the recipe in
  [generate_mm3.py](../scripts/generate_mm3.py) retains its separate supported
  generator pin `967ec30f362dee229183b4e112e928cd41830484`, with tested
  Capstone 5.0.7 pinned by setup. Advancing the runtime does not imply changing
  the translated game code's generator.

The runtime remains a public gitlink plus an applicable patch. Do not point the
game repository at an unpublished runtime commit. The final runtime patch is
900,310 bytes across 102 files (15,262 insertions and 971 deletions). Application to an
independent clean public-base checkout passed, a second application was
idempotent, and fresh/current reverse checks passed.

## Feature preservation

| Area | Integration choice and source | Validation status |
|---|---|---|
| Launcher/settings | Keep upstream Settings, Controls and Mods, persisted `mm3_settings.ini`, keyboard/mouse mapping, controller remapping, menu mouse and full-screen controls in [launcher](../src/mm3_launcher.cpp). Add fork 16:10 and 720p choices; apply profile defaults before saved launcher settings. | Nine production settings-helper cases, fresh full build and live hidden-window rendering/normal-close smoke passed. Broader live settings interactions remain extended coverage. |
| Profiles, FPS and simulation | Retain Balanced/Quality at 60 FPS and LowPower at 30 FPS. Limit configured rendering to 1–90 FPS; launcher offers 30/60/90. Keep upstream 30 Hz default or 60 Hz simulation in [framerate mod](../src/mods/framerate.c), independent of rendering. | Actual preprocessed call-routing checks and a negative control passed. Corrected timing/ABI (18) and actual generated-loop (16) cases passed, including a frozen-cadence negative control. Profile/launcher contracts and real-game default 30 Hz clock/direction/acceleration passed. Actual rendering was 59.96/71.52 FPS for requested 60/90; actual 90 FPS and real-race 60 Hz simulation remain extended coverage. |
| Input and camera | Keep fork arrow driving, Space handbrake, C camera, R/F manual shifts, focus handling and live driving/menu probe; integrate saved key/controller mappings. Keep 0.25 m chase adjustment, FOV and upstream teleport heading. | Runtime keymaps/default shoulders, title controls, production launcher exports, primary-car/teleport probes and fresh-generated camera contracts passed. Live remapping and broader camera scenarios remain extended coverage. |
| Widescreen, HUD and menus | Merge fork camera/map hooks into [widescreen mod](../src/mods/widescreen.c); move title HUD/map/pause placement into [HUD callbacks](../src/mods/widescreen_hud.c) using runtime overlay hooks. Keep native donor HUDs for added cars, original-car HUD overrides and hidden numeric speed/top arrows. | Fresh-generated HUD/gauges, generic title-overlay, native pause and mod-off logical 4:3 contracts passed. Normal mission screenshots passed; pause/post-race/reset scenarios remain extended coverage. |
| Race map and guidance | Keep fork speed-dependent map zoom, Ctrl enlargement, checkpoint alignment and road-following course/current leg in `src/mm3_map_zoom.c` and `src/mm3_race_guidance.c`. | Fresh-generated map/concurrency, zoom and route contracts passed. Missed-turn/next-checkpoint/lap gameplay remains extended coverage. |
| Cars and Workshop | Preserve nine identities from eight source packs, donor factory driving defaults, names, retired-car fallback, unlock rules and model selections in [Workshop](../tools/car-mods/README.md). | Car suite: 61 passed, one skipped; factory references: seven passed; fresh-generated native gearing and factory physics passed for all nine cars. Complete in-game catalog/selection coverage remains extended work. |
| Model and selection artwork tools | Preserve near/far conversion, closed shadows, lamp/reflection repairs, filtered previews, lossless alpha icons, RGB565 selection backgrounds and backed-up menu repair in [model tools](../tools/car-models/README.md). | Model/texture/preview/package/menu suite: 53 passed, five skipped. Import-source suite: eight passed. Actual installed-package/showroom scenarios remain extended coverage. |
| Collision and translation correctness | Keep upstream required collision-cell guard, missing-function/translation repairs and mod registry alongside fork guest-call stack, recovered helper, CPUID, callback and car compatibility repairs. | Collision malformed-cell/cache/failure-ABI, CPU, fresh-generated x87/graph, tree contact/fall, visibility and fresh full build passed. Broader city/race acceptance remains extended coverage. |
| Graphics and performance | Keep upstream generic overlay API, skyline/depth/foliage/fade fixes and fork disk shader cache/prewarm, hashing, native BC/mip conversion, sampler/binding/readback/surface synchronization. Use upstream separate LOD/city-distance controls and facade capacity; retain fork detail fade and explicit particle-budget override. | Runtime CTest graphics/cache/surface suites passed within the 198-case runtime run below; proportional facade fade, single fivefold constructor capacity and nine particle-budget cases passed. Whole-game screenshots and matched performance checks remain extended coverage. |
| Kernel, audio and waits | Combine upstream interrupt/audio-priority/cushion updates with fork memory ownership/cache, events/handles/files/timers/DPC/critical sections, command/fence waits, PCM ring, DSP/HRTF/attenuation optimizations. | Runtime kernel/memory/DSP suites passed within the 198-case runtime run. Actual startup/driving smoke passed; audio-quality comparisons remain extended coverage. |
| Network and runtime diagnostics | Keep upstream Ethernet/NAT/tunnel, background pause/loading-spinner and spike sampler. Keep fork opt-in building/tree/texture/wait traces and launch logging. | Trace/default contracts passed; live network behavior remains extended coverage, with no Insignia/system-link acceptance claim. |
| Attribution and distribution | Preserve upstream GPL-3.0-only, third-party notices, runtime licence and creator credits. Keep original/generated game code, assets, DLC, firmware, disc images, private saves and source-model downloads out of version control. | Publication review passed: conflicts resolved, public runtime gitlink, and no added game binaries/assets/ZIPs/saves staged. |

## Validation record

Before integration, the checkpoint's system-Python car/model tests ran 127 tests:
124 passed and three skipped. The car suite ran 62 tests, the MM3 model/texture
suite 58, and factory references seven. Skips concerned unavailable filesystem
symbolic-link creation and two independent texture-decoder checks without a
configured oracle. These are checkpoint results, not results from the merged build.

Merged-source results are added only after execution.

### Car, model, Workshop and import unit tests

System Python with NumPy, SciPy and Pillow ran these integrated-source suites:

| Discovery pattern | Run | Passed | Skipped |
|---|---:|---:|---:|
| `test_car*.py` | 62 | 61 | 1 |
| `test_mm3*.py` | 58 | 53 | 5 |
| `test_factory_specs.py` | 7 | 7 | 0 |
| `test_mm2_source.py` | 8 | 8 | 0 |
| Total | 135 | 129 | 6 |

The six skips concern unavailable symbolic-link creation, absent prepared
packages, absent extracted source cars, an absent Ferrari F2007 fixture and
two independent decoder checks without `MM3_DECDDS_ORACLE`. Generic synthetic
fixtures still exercise package apply/reapply/restore, source ownership, model
and artwork transactions, texture encoding, preview filtering and donor defaults.
These passes do not validate downloaded models or installed car visuals that
were not present in this integration worktree.

Use the system interpreter with the optional model dependencies installed,
before `enter-env.ps1` selects the smaller build venv:

```powershell
python -m unittest discover -s tests -p "test_car*.py"
python -m unittest discover -s tests -p "test_mm3*.py"
python -m unittest discover -s tests -p "test_factory_specs.py"
python -m unittest discover -s tests -p "test_mm2_source.py"
```

### Runtime native tests

The integrated runtime's full MSVC build passed. Its CTest validation passed
**198 cases across 19 suites**: DSP 17, kernel dispatch 57, save paths 1,
keymap 2, memory 7, networking 1, bindings 9, flare depth 4, graphics quality 4,
hashing 3, pacing 4, shader prewarm 10, readback 11, samplers 4, shader cache 1,
shader lookup 8, surface synchronization 40, targets 4 and textures 11.

Coverage includes the generic overlay-metadata GPU-copy contract and the
interrupt-preemption test that combines high-priority ISR delivery over DISPATCH
with the fork's interrupt-synchronization exclusion. The default physical
RB-to-Black/LB-to-White mapping is preserved alongside configurable remapping.
That patch's reproduction, idempotence and reverse checks passed on the public base.
These runtime results remain current: the launcher texture correction changed
game-side launcher code, not the runtime patch. The fresh whole-game build and
live launcher-initialization smoke also passed. Final real-game scheduler/direction/acceleration verification also passed
as recorded below.

### Other native game-side contracts

Root-run checks passed for `mm3_cpu_diagnostics_regression.py`: 32 native
MMX-capability/flag/register/cache/probe comparisons against original-XBE
behavior, plus bounded diagnostics and opt-in trap/trace handling. The asset
cache contract, tree diagnostics in three modes and native performance-profile
contract also passed. Existing frame-limit CMake and window-close regressions
passed. Fresh-generated x87 checks passed 56 cases plus four overflow cases,
including 48 comparisons with original-XBE behavior. Graph-edge checks passed
12 cases and two negative controls. Tree-contact checks passed 61 original-XBE,
33 retirement, five expiry, 45 resistance and eight classification cases;
tree-fall checks passed 18 fixtures, eight real-render controls and 30
generated-call seams.

Game-side contracts also passed for three title keyboard layouts (1,080 checks
each), production launcher default/saved-map exports (1,080 default and 39 saved
checks), primary-car/teleport probing, collision malformed cells/cache/failure ABI,
generic title-overlay map anchors and pause controls, mixed route/HUD/dimmer
placement and private-UV cleanup, proportional facade fade, the single fivefold
constructor capacity, 100 native pause transitions, public-MMX gauge-state
preservation and donor factory defaults for all nine added cars. Fresh-generated
camera contracts passed 12 cases and mutation controls; map/concurrency, zoom,
route, HUD, gauges, gearing and factory-physics contracts passed. Visibility
passed 200,000 cases. Mod-initialization contracts verified the driving/menu
probe and logical 4:3 with widescreen disabled; nine particle-budget cases passed.
The obsolete per-frame facade-capacity helper was removed; the active constructor
path's single fivefold expansion and proportional detail fade passed their
contracts.

### Direct-launch profile and logging contracts

`python -m unittest discover -s tests -p "test_play_profiles.py"` passed all
**eight tests** against the integrated `scripts/play.ps1`. Temporary harmless
script/native fixtures exercised profile selection, explicit FPS/FOV options,
French-culture decimal FOV, environment precedence/restoration, tracing switches,
single-instance refusal, logs and exit-code persistence, including draining
both output streams before exit. No real game or settings GUI was started.
This validates the script boundary; it does not verify native graphics profile
defaults or launcher UI selections.

### Launcher profile and saved-settings contracts

[mm3_launcher_settings_regression.py](../tests/mm3_launcher_settings_regression.py)
passed **nine isolated cases** using the actual production option/binding tables,
mod registry and environment/reset/INI helpers. Temporary MSVC C11/C++17 builds
passed with `/W3 /WX /Od`; only RmlUi's string/vector types were substituted with
standard-library equivalents. No rendered launcher or game was required.

The cases verified LowPower's 30 FPS/720p/distance/AA settings, Balanced's
`2.0`-to-`2` resolution canonicalization, saved empty Auto choices overriding
profile seeds, invalid inherited values falling back to defaults, and invalid
saved values preserving valid inherited choices. Keyboard, controller and
developer mappings inherited from the environment loaded before saved mappings;
saved entries overrode their specified bindings while preserving unspecified
ones. Required mods stayed fixed and enabled, text bounds rejected multiline
values, and Reset restored every option and binding default.

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\scripts\enter-env.ps1
python tests/mm3_launcher_settings_regression.py
```

These are settings-loader contracts; live GUI interactions and exported settings
on the combined executable remain extended coverage. A separate compiled-process
smoke rendered the hidden 760×720 launcher without RmlUi errors and closed through
normal `WM_CLOSE` with exit code zero, verifying the initial texture-size fix.

### Console-rate simulation and actual-loop contracts

[mm3_simulation_regression.py](../tests/mm3_simulation_regression.py) passed
**18 timing/ABI cases and 16 actual generated-loop cases** after the scheduler
correction. MSVC `/W3 /WX /Od /std:c11` compilation passed. The latter fixture
executes the supported generated main-loop and state functions against fixture
callees, so generated code is required; game assets, a running game and a GPU
are not required.

The independent elapsed-time oracle verifies 30/60 Hz world time, including
regular and irregular rendering, exact simulation steps in all three time fields,
and restoration of caller rendering values. Zero-tick/catch-up frames, bounded
hitches, invalid/large finite input, return addresses, arguments, stack pops and
register state are covered. Alternate callers and mod-off behavior retain their
original contracts; unset/unsupported rate settings select 30 Hz.

The real-loop fixture initializes startup once, executes the actual original
state/nested-transition paths and refreshes the timer before scheduling the
world. It passed 30/60 Hz simulation at synthetic 30/60/90/120/240 render FPS,
plus irregular and mod-off cases. States retain their original per-render
semantics; only world updates are scheduled. A frozen one-tick-per-render mutant
was rejected. Parsed opt-in clock diagnostics independently matched guest
cumulative time to elapsed time and world time to completed ticks, within one
tick; trace fields expose explicit dropped-hitch accounting. Synthetic 120/240 FPS tests
do not expand the supported configured render limit of 90 FPS.

The cumulative guest clock can reset during race loading. Actual-game rate checks
must compare guest/world deltas within one warm gameplay epoch and account for
dropped-hitch time, rather than compare absolute totals across loading. The
opt-in trace explicitly flushes its reports; a buffered-stderr/forced-exit native
check passed so the final gameplay window is retained by the harness.

The initial 16-case fixture had incorrectly called startup every frame. Actual
driving exposed that production initialized it once, leaving the scheduler
frozen while applying a simulation step every render. Its initial timing result
and the associated driving traces are superseded. The corrected actual-loop
fixture now covers this lifecycle and rejects the previous failure mode.

[mm3_simulation_build_regression.py](../tests/mm3_simulation_build_regression.py)
also passed against actual configured/preprocessed generated source. It verified
that main-loop calls target simulation wrappers while only original function
definitions are renamed, rejecting a whole-chunk macro-rename mutant that would
bypass the world wrapper. This independently covers the earlier call-routing
error.

Run after supported generation, from the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\scripts\enter-env.ps1
python tests/mm3_simulation_regression.py
```

Final real-game default-30-Hz clock and direction/acceleration results are
recorded below. Controlled native contracts cover 60 Hz as well; they do not
establish matched handling/gameplay parity.

### Final real-game clock, direction and cadence verification

The unchanged final executable passed both isolated Paris smoke runs:
`integration-verified-60-20261013` and `integration-verified-90-20261013`.
Each produced 78 telemetry samples for one AI car and the player. Independent
JSON reconstruction confirmed the following direction/acceleration results:

| Requested render FPS | AI forward/backward world units | Player forward/backward world units | Player one-second peak, world units/s |
|---|---:|---:|---:|
| 60 | 585.45 / 0 | 576.68 / 0 | 45.10 |
| 90 | 601.80 / 0 | 572.86 / 0 | 44.88 |

The last 20 seconds of flip CSV verified the actual observed rendering cadence:

| Requested FPS | Frames | Median frame time | FPS from median | FPS over elapsed window | 95th-percentile frame time |
|---|---:|---:|---:|---:|---:|
| 60 | 1,169 | 16.6789 ms | 59.96 | 58.45 | 19.5664 ms |
| 90 | 1,392 | 13.98285 ms | 71.52 (about 72) | 69.58 | 18.7056 ms |

Stderr confirmed the respective requested limit and enabled framerate mod.
The 90-request run's median pacing wait was 0.0002 ms; 99.71% of waits were
below 1 ms. Its driving window was throughput-limited rather than capped at 60,
and did not achieve 90 FPS.

Independent analysis of the final 20 persisted reports within each warm gameplay
epoch verified the default world-clock rate:

| Requested render FPS | Guest-clock interval | World-clock interval | Completed world ticks | Effective Hz | Maximum reported lag | Dropped time |
|---|---:|---:|---:|---:|---:|---:|
| 60 | 19.140685 s | 19.133335 s | 574 | 29.988477 | 31.056 ms | 0 |
| 90 | 19.152189 s | 19.133335 s | 574 | 29.970464 | 32.580 ms | 0 |

Scheduler elapsed and the independent guest-clock delta matched at printed
precision in both intervals. Lag remained below one 33.333 ms tick; world time
matched completed ticks. This verifies **default 30 Hz** simulation in the actual
game despite different observed render rates. The 60 Hz simulation setting
passed native contracts and has not been separately accepted in a real race.

Both screenshots were visually normal. Both telemetry JSON files parsed
completely per run; no crash/fatal/exception/assertion messages were found, and
both temporary isolated save directories were removed. These checks establish
startup, rendered mission play, direction/acceleration, telemetry, cleanup and
the default world-clock rate. Timed navigation can reach different missions;
these runs do not constitute matched speed/handling or original-console parity
measurements. Trajectory values are game world units without an assumed metre
conversion.

Earlier bypassed and frozen-clock runs are retained as
[superseded diagnostics](integration-timing-diagnostics-2026-10-13.md). Their
passing direction verdicts did not establish a correct world rate. The corrected
actual-loop fixture and frozen-cadence/call-routing negative controls cover the
failures found during integration.

### Scoped acceptance and extended coverage

The scoped acceptance checks passed: the combined build, corrected native
contracts, live launcher rendering/normal close and real-game default-clock,
direction/acceleration runs above. Publication review found no added game
binaries, assets, ZIPs or saves staged, and the runtime gitlink remains public.

Extended coverage remains for every car/imported showroom asset, both cities
and every race transition, live mouse/controller remapping, save/reload,
pause/post-race/reset layout, missed-turn routing, actual 90 FPS driving,
real-race 60 Hz simulation, matched AI/handling comparisons and graphics/audio/
performance measurements. Missing optional fixtures and unrun scenarios remain
visible limits of validation, rather than claims that every combination passed.

## Historical references

The [October 12 comparison and earlier measurements](fork-reference-2026-10-13.md)
and [old backlog](../Next%20fixes%20to%20do.txt) preserve investigation evidence.
Their old missing-fix statuses, runtime pin, launch defaults and timing values
must not be read as current integrated-build results. Additional deep records
remain in [session status](../SESSION_STATUS.md),
[ownership regressions](../OWNERSHIP_REGRESSION_REPORT.md),
[D3D translation audit](../D3D8_D3D11_TRANSLATION_AUDIT.md) and
[video investigation](../VIDEO_ARTIFACT_INVESTIGATION.md).
