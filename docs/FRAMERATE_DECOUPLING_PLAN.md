# MM3 frame rate, simulation timing, and adjustable tick rate

Audit date: 2026-11-01. Sections 1-4 record the pre-implementation source audit, including the legacy mod.

Implementation update: a fresh whole-loop scheduler and separate saved-image presentation path now exist. See [replacement implementation and validation](FRAMERATE_REPLACEMENT_VALIDATION.md) for current behavior, tests and the remaining interpolation/compatibility work. The full goal is not complete.

## Outcome and evidence boundary

The right first target is **30 world ticks per simulation second, independent of presentation FPS**, with smooth presentation between completed ticks. An adjustable world tick rate is a second compatibility project: changing update cadence can change AI, controller filters, collision ordering, random-number consumption, scripts, and animation even when elapsed seconds are correct.

The user reports that the existing mod is not working well and has chosen a fresh implementation. **Build a replacement timing mod from measured retail behavior, rather than extend `src/mods/framerate.c`.** The existing code remains evidence of previous approaches and regression cases, not the design baseline. Its default repeats the last image on frames without a world tick; optional `MM3_SMOOTH` replays GPU commands with interpolated constants. Neither mechanism establishes complete game-wide decoupling. Keep the old implementation available for controlled comparisons, but ensure only one timing implementation owns the guest hooks in any run.

This report distinguishes:

- **Confirmed source behavior:** read in the current handwritten/generated code or mapped local XBE data.
- **Recorded claim:** existing comments describe earlier measurements, but their underlying captures/traces were not reproduced in this audit.
- **Candidate:** a plausible coupling or unclassified dispatch target requiring tracing. Inclusion does not mean a demonstrated bug.

There is no claim here that every indirect callback or gameplay mode has been resolved. The coverage ledger below includes all major areas that must be closed before making that claim. No current runtime A/B, xemu trace, physical audio check, or multiplayer session was performed. This is a concrete investigation and implementation plan, not a completed unrestricted-FPS mod.

Snapshot:

| Item | Identity |
| --- | --- |
| Root HEAD | `282552ccea02310c5a469fb8d9a0be0678f26158` |
| XboxRecomp HEAD | `0acefa1b642d6362e4a8b0c3b5e7e8c72d9aced4` |
| Local `game_files/default.xbe` SHA-256 | `2b04b66c43e7f37bbcebbfb5b72ccb96a2aa99cc2c60c53eb7d3530bcc2a3d79` |
| `src/recomp/gen/generation.sha256` contents | `5709beb7424e28033dfb39c29f383bc97d1d5140b3444faa297f57e2e0edbe6e` |
| Initial root and nested working trees | Clean |

Guest addresses below belong to this generated target. Check executable/XBE identity before using them on another region/build. Generated line numbers are navigation aids and can move after regeneration. The generation stamp alone is not proof of correspondence to the local XBE.

## 1. Current timing path

Read these first:

| Source | What it establishes |
| --- | --- |
| `src/recomp/gen/recomp_0017.c:27551`, `sub_001E77F3` | Main frame loop and exact ordering |
| `src/recomp/gen/recomp_0017.c:25442`, `sub_001E6C87` | Raw/clamped/averaged timing and special timing branches |
| `src/recomp/gen/recomp_0013.c:85191`, `sub_00170316` | Counter difference, elapsed double, accumulated elapsed double |
| `src/recomp/gen/recomp_0007.c:101715`, `sub_0009F810` | Timer ultimately reads the emulated TSC |
| `src/mods/framerate.c:115` | Fixed-step wrapper scope and temporary timing-field substitution |
| `CMakeLists.txt:188` | Definition-only seams, including same-chunk calls |

The main loop's repeated body is:

1. `0x1E7849`: state-machine step `sub_0002545A`.
2. State/exit conditions, including `game+0x48`, `+0x74`, `+0x78`, `+0x7C`.
3. `sub_001E6C87`: measure and prepare frame time.
4. `sub_001E7590`, returning to **`0x1E7895`**: world/update dispatch.
5. `sub_001EC8E6`, returning to **`0x1E78AA`**: render dispatch.
6. `sub_001F373E` and a four-slot loop over `game+0x8C` slots `+0x2C..+0x38`: `sub_001F3065`, `sub_001F34F7`, `sub_001F33CF`.
7. Virtual call at guest callsite `0x1E7920`, game vtable slot `+0x0C`.
8. Conditional increment of `game+0x10`, then repeat.

**The fixed wrapper covers step 4 only, not the whole frame.** Its return-address guard also means other callers of the same function bypass this scheduling. The state-machine wrappers currently just forward to their originals. The post-render dispatch receives restored variable `game+0x54`. `sub_001F373E` demonstrably accumulates that delta into its object `+0x28`; these are real remaining timing paths, although their full gameplay ownership still needs classification.

### Timing fields are not interchangeable

With `esi=game+0x18`, the timer writes `esi+0x18` to **`game+0x30`** (double elapsed delta), and `esi+0x20` to **`game+0x38`** (double cumulative elapsed time). `sub_001E6C87` prepares **`game+0x4C`** (float clamped/modified delta), maintains history under `+0x5C`, and writes **`game+0x54`** (averaged delta). It also has branches involving `+0x50`, `+0x70`, and `+0x80`. One branch explicitly sets `+0x4C` to float bits `0x3D088889` (about 1/30); another scales `+0x70` by 0.001.

The mod temporarily substitutes only `+0x30`, `+0x4C`, and `+0x54`. It does not substitute `+0x38`, the counter accumulators, timing history, or arbitrary callers' copies. Establish consumers before changing more fields: replacing wall-clock state globally could break waits, movies, network timeouts, and loading.

### What the existing scheduler actually does

- Default-on mod `MM3_MOD_FRAMERATE`; settings cached on first use.
- `MM3_SIM_HZ`: accepts **30, 60, 90 only**, otherwise silently uses 30. Not arbitrary-rate or live adjustment.
- Adds raw frame delta to a float accumulator, executes zero or more whole world updates, then restores the timing fields for outer-loop work.
- Catch-up budget is approximately **133 ms**, i.e. 4/8/12 updates at 30/60/90 Hz. More elapsed time is discarded.
- `fixed_step_take` treats nonpositive, NaN, or greater-than-one-second delta as **one tick**, not zero. A long stall therefore has a different policy from a moderate hitch.
- Clock, AI phase, interpolation state and queue snapshots are static. There is no explicit reset API in this mod for new races, loads, pause/resume, or rate changes.
- `s_ticks` is consumed once by the render hook and reset to -1. This deliberately allows render-only briefing/post-race paths to draw.
- No new thread is created: updates and render requests are still driven by the main loop. A blocking render/present can delay the next simulation update until catch-up.

These are confirmed implementation details; whether each causes an observable gameplay defect is untested here.

## 2. Physics and NPC evidence

### Vehicle solver has an inner stepping mechanism

`sub_0021A75E` in `src/recomp/gen/recomp_0018.c` (notably lines 69328-69460) combines incoming dt with object remainder `+0x214`, multiplies by **120** at `0x386198`, converts to an integer through `sub_001F0399`, caps the count at **16**, and divides accumulated time by that count. It subsequently invokes the controller at object `+0x19C`, vtable `+0x10`, in a loop, carrying a remainder. There is also a step-size cap branch using `0x385E1C` and float bits `0x3D4CCCCD` (0.05), plus a callback at vtable `+0xCC` for excess time.

This is stronger evidence than the existing comment that physics always takes four exact 1/120 steps. At an exact 1/30 input and ordinary remainder, four substeps are the expected case. **The code divides total time by its chosen count; it is not enough to assume every rate produces exact 1/120 substeps.** Trace the conversion's rounding and live remainder/controller target before specifying behavior at 90 Hz or arbitrary rates.

`sub_002199F4` (`recomp_0018.c:66605`) separately computes **`1 / MEMF(0x3B5588)`**, calls `sub_00217E80`, the controller's `+0x10` slot, and `sub_002198BF`. The local XBE initializes `0x3B5588` to 120, but `sub_00212106` writes that global from a parsed value. `0x3B558C` also initializes to 120 and is written by `sub_002120D4`. These are configurable data, not immutable constants. Establish which path runs in player, AI, damage/reset, and paused states.

Keep the solver cadence separate from the world tick setting. Raising world ticks must not automatically multiply solver frequency, retune tire forces, or call the controller twice through overlapping paths.

### AI is cadence sensitive, but the wrapper needs an ownership audit

`src/mods/framerate.c:82` gates **all of `sub_0022DBBF`** at 60/90 world Hz: every second/third call, multiplying its stack dt accordingly. This preserves a nominal 30 Hz cadence only for those integer multiples. Its phase is shared static state, not keyed by AI module identity or race.

Generated `sub_0022DBBF` (`recomp_0019.c:14627`) dispatches object virtual slots `+0x1C` and `+0x20`, gathers data, and ultimately invokes `sub_00157DA1` with dt, count and array. `sub_00157DA1` (`recomp_0013.c:20442`) updates several downstream systems: `sub_001575E7`, `sub_0014EDB6`, `sub_0014E58E`, `sub_0014B2FC`, and `sub_001480EF`. Its `0x3C5768` counter increments on invocation.

Thus, the gate wraps a substantial dispatch, not a proven isolated steering calculation. Map every target and establish whether player controllers, traffic, pedestrians, collision or other jobs share it before changing its policy. Existing comments report AI reversing at high FPS and about 9% extra AI travel at 60 world Hz before gating; these remain **recorded claims**, not new measurements.

### Other hardcoded time assumptions already visible

See [the candidate inventory](FRAMERATE_TIMING_CONSTANT_CANDIDATES.csv). Particularly useful starting points:

| Function / source | Confirmed operation | Interpretation / next check |
| --- | --- | --- |
| `sub_001CDAD3`, `recomp_runtime_entries.c:4297` | Passes fixed 1/30 to `sub_001CB424` | Controller/input path candidate; determine call frequency and native ownership |
| `sub_001E955E`, `recomp_0017.c:33762` | Passes fixed 1/30 to a virtual `+0x58` call | State/controller transition candidate; may be a one-off initialization step |
| `sub_0021DDA1`, `recomp_0018.c:79670` | Multiplies a value by fixed 1/30 | In a large vehicle-related routine; trace operand meaning before retuning |
| `sub_0021A50D`, `recomp_0018.c:68997` | Uses fixed 1/30 in history/filter calculations | Filter response may depend on invocation count |
| `sub_001E9327` / `sub_001E93B3`, `recomp_0017.c:33392` / `33530` | Pass fixed 1/60 into object virtual methods | Resolve targets and whether these are initialization or repeated work |
| `sub_00216436`, `recomp_0018.c:56270` | Accumulator subtracts count * 1/60, with a five-step cap | A second internal scheduler exists; classify it rather than force every subsystem to 30 |
| `sub_000FB9D0`, `recomp_0010.c:25553` | Multiplies input by 1/60 before integer/fraction processing | Could be animation/sample conversion; constant alone is insufficient |
| `sub_001F1EA2`, `recomp_0017.c:58091` | Passes data at `0x3B0AC0`, initialized to 1/30 | Check mutable configuration and call ownership |

The inventory contains **112 matching generated statements**, selected by direct `MEMF(absolute_address)` references whose mapped initial float is approximately 1/15, 1/30, 1/60, 30, 60 or 120. It includes writes and unrelated uses of those numbers. No direct mapped 1/120 float reference matched; the reciprocal-of-120 code above shows why that does not imply absence of 120 Hz physics. It excludes doubles, immediate float bit patterns, indirect/table loads, computed reciprocals and asset/script values. It is a reproducible search frontier, not 112 confirmed timing bugs or an exhaustive dataflow result.

## 3. Coverage ledger: every area the mod must account for

The first four columns describe evidence; the last column is the required closure work. Unknown addresses are intentionally left unresolved rather than invented.

| Area | Coupling mechanism / evidence | Current coverage | Confidence | Closure requirement |
| --- | --- | --- | --- | --- |
| World dispatch / scripts | `1E7590` forwards `+0x54` into `20F5FA`, `1EC6C5`, `1E6E69`, `1EC698`; `1E6E69` enters manager dispatch `24C991` virtual `+0x1C` | Fixed dt only through guarded callsite | Confirmed seam; leaf ownership incomplete | Enumerate manager targets, script updates and direct clock reads |
| Player rigid body / wheels / suspension / tires | `21A75E`, `2199F4`, controller `+0x19C`; nested dt and remainder | World wrapper may contain active path; not proven for every mode | Confirmed inner stepping | Count actual substeps and compare trajectories, impulses, RPM, gears and suspension |
| Throttle / steering / brakes / gearbox filters | Fixed 1/30 candidates above; controller callbacks | No demonstrated complete normalization | Candidate | Separate per-tick coefficients from per-second quantities; test manual/automatic, handbrake and reverse |
| Race opponents / police / pursuit / rubber-banding | `22DBBF` gate, virtual decisions and downstream dispatch | Nominal 30 Hz gate at offered higher tick rates | Confirmed gate, recorded behavior claims | Resolve modules and measure decision cadence, routes, starts, recovery, pursuit and difficulty |
| Ambient traffic / spawning / lights / horns | Potential manager/AI children; invocation counts, spawn probabilities, RNG order | Inclusion under the world wrapper alone is insufficient proof | Candidate | Name callbacks, record spawn/despawn and light timing, collision avoidance and density |
| Pedestrians / avoidance / reactions / animation | Manager/AI children and animation scheduler candidates | Not independently established | Candidate | Trace walking, crossing, avoidance, impacts, respawn and animation phase |
| Collisions / impulses / damage / detached parts | Solver substeps and downstream physics dispatch | No broad FPS or tick-rate parity proof | Candidate beyond solver seam | Test car/car, walls, poles, trees, jumps, rollovers, recovery, damage and events once per impact |
| Breakable city objects / native debris | City manager, object callbacks, lifetime counters | Native paths unresolved | Candidate | Trace logical retirement and debris simulation independently of render visibility |
| Added falling-tree behavior | `src/mm3_tree_diagnostics.c:160`, `tree_fall_clock`: host milliseconds, pause flag `0x429DB2`, 100 ms cap, clock called while drawing | Separate pause-aware wall clock; outside world scheduler | Confirmed mod-specific clock | Decide simulation-time ownership for collision retirement and visual-time ownership for fall animation; test stalls/pause |
| Race clocks / countdown / checkpoints / laps / scoring | World scripts plus state and post-render paths | Not proven wholly under world ticks | Candidate | Compare event order and simulated seconds across FPS; no double checkpoint or skipped timeout |
| Undercover missions / timers / triggers / cutscenes | Script dispatch, mode callbacks and movies | Not proven | Candidate | Mission start/fail/complete, timed objectives, scripted NPCs and briefing transitions |
| State machine / restart / game-over / pause | `2545A` remains every outer frame, virtual `+8`; `game+0x10` increments on outer-loop path | Intentionally outside fixed update | Confirmed cadence | Classify logical transitions vs UI work; check count-based waits, resets and stale clock phase |
| Input sampling / edges / hold repeat / rumble | World pre-update `1EC130` calls input queries and controller `+0x54/+0x58`; post-render input-related dispatch also exists | Mixed paths; device sampling can remain fast | Confirmed dispatch, incomplete semantics | Timestamp/buffer edges; consume each edge once at tick boundary; hold analog state across catch-up ticks |
| Camera / shake / FOV / chase / spectator | `1EC06D` invokes `1EBEA8`, `1EA854`, `1E9454` with dt and decrements `+0x1E0`; render receives variable dt | Mixed update/render ownership | Confirmed dt flow; individual effects unresolved | Tick authoritative camera state, interpolate pose; avoid double smoothing and camera/vehicle phase mismatch |
| Skeletal animation / wheels / doors / wipers | 1/60 candidates and `216436` scheduler; exact ownership unresolved | GPU-constant interpolation does not cover all deformed vertices | Candidate | Trace pose advancement, sampling frequency, attachment transforms and CPU-skinned geometry |
| Particles / tire smoke / skid marks / trails | Per-call spawn counts, lifetimes, render-side queue consumption | No complete ownership map | Candidate | Spawn on ticks; age with chosen clock; interpolate visuals; render must not emit twice |
| HUD / menus / fades / text / minimap | Render queue mutation; `1F373E` accumulates variable dt after render | Briefing fallback and experimental queue restore exist | Confirmed split | UI wall time may be intentional; prove gameplay clocks and fade durations don't inherit FPS |
| Audio / Doppler / engine pitch / collisions / speech | Events originate in gameplay; mixer/device has its own sample clock | No physical or per-event parity evidence here | Candidate | Emit events once per tick; measure event count/order, listener velocity, loops and pause behavior; retain sample-rate clock |
| Movies / music / streaming media | Independent timestamp/device clocks and frontend state | Outside fixed world guarantee | Candidate | Preserve playback time and A/V sync; verify story movies and transitions at all presentation modes |
| Network / System Link / online | Wall-clock timeouts vs simulation sequencing; packet cadence may depend on update calls | No live two-game evidence | Candidate | Identify protocol cadence, sequence and timeout owners; constrain peers' tick settings until tested |
| RNG / counters / modulo scheduling | `3C5768` invocation counter; `game+0x10` outer-loop count; other consumers unresolved | No deterministic replay guarantee | Confirmed counters; uses unresolved | Find readers, random draws and frame-modulo jobs; keep gameplay scheduling tick-based |
| Loading / streaming / async completion | Jobs may be budgeted per frame or triggered by visibility | Not proven | Candidate | Keep I/O progressing without world ticks; activate gameplay results at deterministic boundaries where needed |
| Render / culling / shadows / reflections | Guest render mutates queues and allocates; repeated or skipped drawing changes call counts | Three presentation strategies already exist | Confirmed side effects in implementation | Resolve queue ownership, offscreen targets and render-derived gameplay data; validate each backend |
| TSC / kernel timers / vblank / sleeps | Host-backed runtime clocks, GPU pacing and guest waits | Not a single simulation clock | Confirmed runtime mechanisms | Audit gameplay consumers; keep hardware/network/I/O timing out of world-rate scaling |

## 4. Rendering is a separate acceptance problem

`src/mods/framerate.c` currently offers:

| Mode | Implementation | What it does and does not establish |
| --- | --- | --- |
| Default | Skip guest render when no tick, `nv2a_d3d_repeat_frame` | Safe baseline to investigate simulation cadence; repeated images retain tick-rate motion |
| `MM3_RENDER_EVERY_FRAME=1` | Rerender after heuristically restoring changed small globals in `0x3C5680..0x3C57B0` | Restores counters judged non-pointer-like; not an explicit queue schema or proof of pure rendering |
| `MM3_SMOOTH=1` | Guest renders on tick frames; `nv2a_d3d_interp_frame(alpha,drew)` replays | Potential smooth motion, about one tick of interpolation latency, incomplete semantic coverage |

The queue range overlaps `0x3C5768`, which `sub_00157DA1` increments. This does not by itself prove corruption—the restore tracks only words changed by render—but it reinforces the need to identify each retained word rather than rely on numeric pointer heuristics. Existing comments record text/tail-light faults on repeated guest renders and heap corruption when pointers were restored. Treat these as regression scenarios.

Runtime sources:

- `tools/xboxrecomp/src/video/nv2a_d3d11.c:4224` and `:4240`: repeat/interpolation requests block the caller pending presenter completion.
- `tools/xboxrecomp/src/video/nv2a_sdlgpu.c:3378`: equivalent interpolation API; `need_new` explicitly unused.
- D3D11's request stores `need_new`, but the inspected presenter branch replays the latest completed frame rather than waiting on it. Establish tick/frame identity before assuming alpha belongs to a particular completed tick.
- `tools/xboxrecomp/src/kernel/nv2a_pb_exec.c:4732`: interpolation linearly blends recorded command values in method range `0x0B80..0x0BFF` where prior matches exist. This is scalar constant interpolation, not typed vehicle/camera transform interpolation. Rotations, projection values and unrelated constants can need different treatment; changing draw order/identity needs explicit handling.
- Replay snapshots executor state and vertex data, but also executes recorded methods. For example the semaphore-release branch writes guest memory before a replay guard. Audit replay side effects and completion signals, not only displayed pixels.

For the replacement, design presentation around identified vehicle/camera transforms and animation poses in completed simulation snapshots. Do not inherit GPU-constant blending or render-global restoration as the default architecture. Repeated images can be a diagnostic fallback while simulation ownership is established. Existing generic replay facilities may be evaluated independently, but require evidence before adopting them. Never write interpolated poses into authoritative physics state. Reset interpolation on spawn/despawn, teleport, camera cuts, city load and race restart. UI, text and particles need separate policies.

## 5. Proposed clock contract

| Clock | Policy |
| --- | --- |
| Host monotonic time | Measures elapsed real time and schedules work; unaffected by simulation speed |
| Simulation time | Advances by exactly one configured world step for each completed world tick; pauses with gameplay |
| Physics solver time | Preserves measured retail stepping initially; independent setting only after solver audit |
| AI decision time | Preserve retail cadence initially; independent fractional accumulator if world rate changes |
| Presentation time | Drives interpolation alpha and UI effects with an explicit pause policy |
| Audio/media/network time | Preserve native device/real-time contracts; translate gameplay events at boundaries |

At the 30 Hz compatibility setting, every completed world tick sees 1/30 seconds, whatever the present rate. Fast frames can have zero ticks; slow frames can have several. Rendering observes completed state and does not advance gameplay. Input acquisition can run faster, while gameplay consumes a defined tick input snapshot.

Use a monotonic integer/rational or double accumulator with an explicit overload policy. Implement the replacement clock independently, with explicit lifecycle state and boundary-drift checks; do not inherit the existing static float accumulator or its invalid-delta policy. Record elapsed, accepted and dropped time; do not hide slowdown. Retain bounded catch-up to avoid an unbounded spiral, specifying the difference between a short hitch, suspended process, paused game, and loading. Under sustained overload, “any FPS” cannot promise real-time simulation if the machine cannot execute the required ticks; report the deficit.

World tick-rate changes and time-scale changes must be distinct. Initially allow rate selection at launch or a clean race boundary, with reset of accumulators and interpolation. Arbitrary rates require a time accumulator for AI, not `round(world_hz / 30)` or every-N-call gating. At world rates below 30, determine whether multiple AI decisions per world update are meaningful; otherwise restrict that configuration. At non-divisors of 120, explicitly choose between retail adaptive substeps, a retained 120 Hz solver accumulator, or another proven scheme. That choice can change handling and belongs behind an experimental option.

## 6. Implementation and investigation sequence

### Phase A — instrument without changing behavior

Establish a retail and existing-mod-disabled baseline first. Reuse suitable opt-in diagnostic transport, independently of the old timing policy. Record: frame ID, world tick ID, mode/state, game/module identity, host timestamp, all timing fields above, accumulator, alpha, ticks requested/executed/dropped, physics substep count/dt, AI callback count/dt and present snapshot IDs. Record input edges and one-shot gameplay event IDs. Buffer bounded traces rather than log every object synchronously in performance runs.

Resolve dynamic targets at `24C9CD`, `25466`, `1E7920`, `22DCA9`, `22DCDB`, `21A8AB`, `1EC327`, `1EC349`, and `1F34A0`. For each callback, record object/vtable, dt argument, frequency, mode and whether it writes authoritative state. Expand the ledger with actual function/address ownership for traffic, pedestrians, missions, audio and streaming. Classify the CSV candidates using callers and operands; include indirect loads, immediate bit patterns, counters, asset tunables and script timers in the next pass.

Use xemu GDB at the same guest callsites for retail reference: counts per world frame, dt values, conversion rounding, pause behavior, event order and solver callback identity. Use the existing working XISO and full copied xemu configuration as described in AGENTS.md. No emulator-code import is necessary.

**Exit:** callback ownership and clock-domain table for tested modes, stable trace schema, and an original-vs-recomp 30 Hz baseline. Unresolved modes stay open.

### Phase B — implement a fresh 30 Hz scheduler

Build a new MM3-owned scheduler at boundaries established by Phase A. Define one complete logical tick from the authoritative callbacks, including confirmed work outside `1E7590`, while preserving original dependency order. Select the replacement exclusively at build or startup: disable the old scheduler, AI gate and render wrappers together so they cannot double-step or intercept the replacement. Definition-only hooks may remain a mechanical integration technique, but choose their targets from the ownership map rather than assuming the old wrapper boundary is sufficient. Keep menus/device polls/loading responsive. Snapshot gameplay input once per tick; buffer short edges across zero-tick frames and consume once across catch-up ticks. Add explicit lifecycle reset handling, mode-aware pause/load behavior and dropped-time telemetry. Check all entry paths that bypass the guarded return address.

**Exit:** changing presentation FPS changes neither authoritative tick/event counts nor gameplay results beyond measured baseline variation, including restart/pause/briefing/mission transitions.

### Phase C — make high FPS visibly useful

Build presentation snapshots with stable object identity and previous/current completed tick IDs. Interpolate vehicle and camera transforms using suitable position/rotation interpolation; handle animation, particle trails, HUD/text, shadows/reflections and offscreen passes explicitly. Render through identified presentation state without numeric pointer heuristics or restoring an arbitrary range of guest globals. Establish a safe presentation seam first; if native rendering still advances gameplay, separate those side effects before enabling between-tick renders. Keep generic runtime facilities in XboxRecomp; MM3 addresses and scheduling policy stay in the root project.

**Exit:** visually smooth motion at high refresh, no interpolation feedback into simulation, no duplicate gameplay events, backend-specific captures reviewed.

### Phase D — adjustable world tick rate

Expose only validated rates initially. Implement time-based AI cadence rather than copying the old integer gate; bind cadence state to the correct owner and reset it. Audit fixed-per-call damping, smoothing, random probabilities and counters; preserve a 30 Hz compatibility path when necessary. For an identified per-step multiplier `d`, a candidate equivalent over dt is `d^(dt/reference_dt)`; apply only after confirming the quantity is that kind of decay. Do not bulk-replace 30/60/120 constants. Revalidate solver cadence, contact ordering and all gameplay modes per rate. Add live switching only if requested and validated at an explicit safe boundary.

**Exit:** each offered rate has handling/AI/mission evidence and documented intentional differences; unsupported arbitrary values cannot silently masquerade as validated behavior.

## 7. Validation matrix and acceptance

Start with retail and a matched existing-mod-off 30 FPS baseline, then the replacement alone at 30 world Hz at presentation caps **15, 20, 30, 59.94/60, 75, 90, 120, 144, 165, 240, uncapped** as supported. Test actual measured cadence, not just an environment setting. Include irregular frame intervals, 100/250/1000+ ms stalls, sustained overload, alt-tab, pause/resume, race restart, teleport and city transition. Repeat at every offered world rate; add non-divisor rates only when implementing arbitrary-rate support.

| Scenario family | Evidence needed |
| --- | --- |
| Straight throttle / brake / steering / gears | Tick-aligned positions/velocities, acceleration times, stopping distance, heading, RPM/gears and input timeline |
| Collisions / jumps / hills / damage | Contact and impulse/event counts, post-impact trajectory, ground penetration, suspension and recovery |
| Race AI / police / traffic / pedestrians | Per-identity tracks and decisions, path progress, reverse recovery, spawns/density and animation phases |
| Cruise / all race types / Undercover / DLC / split-screen | Tick/event counts, timers, mode transitions, completion/failure semantics and inspected captures |
| UI / briefing / post-race / camera / particles | Durations, edge handling, continuity, text/tail lights, pose phase and cuts at high FPS |
| Audio / media | Event order/count, audible pitch/Doppler/loops, speech/movie synchronization and pause behavior |
| System Link / online | Two live peers, packet/sequence timing, stable gameplay and reconnect/timeouts at allowed settings |
| Lifecycle / stalls | Defined accumulator and AI-phase reset, no catch-up bursts after pause/load, dropped-time accounting |

Choose numerical tolerances from repeated matched 30 Hz baseline runs before using them as pass criteria. Prefer deterministic tick-indexed replay if achievable; otherwise preserve raw tracks and report variation. A short acceleration test cannot certify damage, AI, missions or networking.

Reuse `tools/game_test/run.py` with explicit executable, extracted assets and a copied save template; see `tools/game_test/README.md`. `physics_sweep.py` provides acceleration/brake/turn/wall sampling but currently reports rather than enforcing comprehensive parity. `ai-start.json` checks opponents actually move and do not reverse excessively. Do **not** run the older `tools/scripts/test_ai_start.py` unchanged on a shared session: its startup kills every process named `mm3_recomp`; port needed checks to the owned-process harness.

For throughput runs explicitly set `RECOMP_FPS_LIMIT=0` and `RECOMP_VSYNC=0`; test pacing separately. Record executable/XBE hashes, mods, backend, tick/presentation settings, mode, seed/input and captures. Use PresentMon for actual presentation cadence, Tracy for scheduling/callback timing, and RenderDoc for replay/resource/pixel faults. Internal frame CSVs alone cannot distinguish new simulation states from repeated images. Run a normal unprofiled comparison after instrumented runs.

## 8. Concrete next work package

The first work package is a fresh replacement design plus diagnostics and lifecycle/ownership proof, with **30 world Hz as the initial target**. Its deliverables are callback and clock traces for player driving, a populated race, heavy traffic/pedestrians, pause/restart and one Undercover mission; resolved ownership of the post-render timed block; measured solver substeps; and repeated FPS comparisons using owned harness processes. Only then change scheduler boundaries or advertise smooth arbitrary-FPS gameplay. Adjustable world Hz follows that baseline, with solver and AI cadence treated explicitly rather than inferred from a single setting.


## 9. Replacement design and cutover checklist

1. Establish native behavior with the old framerate mod disabled; use the old mod only as an additional comparison, never as the correctness oracle.
2. Resolve tick membership and order before writing the new scheduler. Keep host input/I/O pumping and presentation separate from authoritative tick execution. Start on the existing game thread; add threading only if measured blocking and a proven ownership model require it.
3. Give the new scheduler explicit initialize, pause/resume, load, restart and shutdown boundaries. Specify valid elapsed-time handling, bounded catch-up, dropped-time accounting and reset of input/interpolation history.
4. Preserve the native solver and NPC cadence at the initial 30 Hz world rate. Add independent rate policies only after tracing their actual owners and dependencies.
5. Build a presentation seam with explicit state ownership. Publish completed snapshots; never let render interpolation feed into physics, AI, triggers or event generation.
6. Make old/new selection exclusive. Include active implementation and settings in evidence; prevent old environment variables from silently enabling legacy wrappers during replacement tests.
7. Validate the replacement at 30 Hz across presentation rates, then smooth rendering, then adjustable world rates. Retire old hooks and obsolete settings after replacement acceptance; retain the audit's failure cases as regression coverage.

This revision changes the plan only. It does not remove, disable or repair the current mod, and does not claim its reported failures have been reproduced or diagnosed.
