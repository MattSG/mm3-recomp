# XboxRecomp ownership migration

Goal: XboxRecomp contains useful, reusable runtime/lifter mechanisms only.
Title-specific behavior, policy, fixtures and tooling belong in this game
repository; add a generic extension boundary where a direct move is impossible.
Audit the entire current fork, including inherited upstream code. Earlier
milestones were published; the renewed whole-fork audit below is incomplete.

## Baseline and merge

- Root main: 722facc -> 50a042f (fast-forward of sdl3-gpu-linux).
- Fork main: 9214c046 -> 8c01af0 (fast-forward of sdl3-gpu-linux).
- The previously uncommitted volume/Y16/lamp-depth fixes are preserved in fork
  8c01af0 and root pin 50a042f. Before the ownership move, 34 focused GPU tests
  passed, with actual night driving and lamp captures retained separately.
- Work proceeds in I:/repos/mm3-ownership-main. The original checkout and the
  user's running night session remain separate. Both repositories have their
  own commits and validation boundaries.

## Ownership rules

MM3 owns guest addresses, game-specific classifiers, authored asset/shader
selection, rendering enhancements and their policy, private command protocols,
gameplay/timing/HUD hooks, and game-specific fixtures or tooling.

XboxRecomp owns documented Xbox hardware/XDK behavior and reusable host
mechanisms. An MM3-derived regression can remain only when it tests a generic
hardware/ABI contract, without a title activation path. MMX/XMM register names
such as g_mm3 are architectural state, not title coupling.

## Audit ledger (not complete)

| Area | Evidence / required action | Status |
| --- | --- | --- |
| SDL3 integration | Main ancestry in root and fork, preserved lighting changes | Merged locally |
| Private fog stream | Fixed tag/header and parser moved from runtime into src/mods/render_commands.*; executor has an optional ordered NOP hook | Moved and validated |
| Panorama selection | MM3 render_policy.c selects its authored 256x256 DXT5 horizon; backends only execute an optional sampling hook | Moved and validated |
| Lamp selection | MM3 render_policy.c owns Flares.psh recognition and diagnostic dumps; backends only execute an optional depth hook | Moved and validated |
| Extended-distance shaders | Retired remap/sky-feedback branches removed; active fog shader, parameters and sky composition moved to src/mods/render_fog* | Moved and validated on both backends |
| Overlay placement | Existing NvOverlayHooks already delegates HUD/sanitization to MM3 | Verify remaining renderer assumptions and tests |
| Runtime / platform | Audit all fork-vs-upstream runtime changes for hidden guest addresses/defaults and title-only behavior | In progress |
| Tools / templates / tests / docs | Review tracked artifacts and build dependencies, including captured game-specific fixtures | In progress |
| Inventory coverage | Complete fork commit tree captured: 622 tracked files, including all 153 src paths; modes/blob IDs and working-tree hashes checked | Covered; ownership decisions remain scoped |
| Build / runtime proof | Same behavior after each ownership boundary; D3D11 and SDL GPU builds, focused tests, bounded gameplay | In progress |
| Final inventory | Both main branches, root submodule pin, clean intended source state, worktrees, no unresolved ownership entries | Not yet satisfied |

## First boundary: private fog packet

MM3 retains the exact wire format, float/ARGB/flag payload, stream ordering and
partial-packet reset behavior. The runtime does not recognize its tag or word
count. The generic hook can consume extension NOP words; unrecognized NOPs
retain native notification behavior. This also removes the former unconditional
subchannel-zero early return, which swallowed unrelated NOP notifications.

Validation so far:

- MM3 packet decoder contract: complete payload, unknown words, another
  subchannel and interrupted packets pass.
- Existing MM3 mod initialization and facade-fade contracts pass.
- Real executor texture/replay/first-frame tests pass, with added hook routing,
  native NOP notification and interrupted-run checks.
- D3D11/SDL GPU binding suite: 20/20 passed, including fog, fog MSAA,
  pipeline binding/reset, failures, resize and overlay upload. Its SDL recorder
  still expected nine bindings after the volume work introduced thirteen;
  updating that fixture restored two failing binding checks.
- Full isolated D3D11 game build passes. Bounded night Cruise run
  conformance_tmp/game_test/run-5en73gvu passed: settings/gameplay captures
  visually inspected, 34.817 units forward, zero backward travel, 29 samples.
  The harness closed only its owned child after the successful run.
- Full ownership cleanup remains incomplete; continue through the ledger.


## Second boundary: authored rendering selectors

- `src/mods/render_policy.c` owns horizon texture/mip/filter/blend selection and
  Flares.psh's three-mask-tap classification. Existing mod gates and the optional
  lamp trace/vertex-program dump are now game-owned. Both backend implementations
  use the same MM3 policy, supplied through `NvOverlayHooks`.
- The runtime retains cubic filtering and non-writing scene-depth tests as
  generic mechanisms. No hook means no change to guest sampling/depth. The
  cubic hook cannot select a cube or volume resource as a 2D image.
- Captured MM3 lamp vertex/combiner data and its pass decision table moved out of
  the fork to `tests/mm3_renderer_lamps`. Those fixtures compile the production
  MM3 policy. Generic GPU sampling tests now verify the callback boundary and
  opt-in behavior; game-specific selector cases live in MM3 CPU contracts.
- Validation: MM3 policy, initialization and overlay contracts pass; 12 generic
  graphics cases pass; cube/volume rejection gate checks pass on both backends;
  all 8 moved lamp GPU cases pass, including with RECOMP_FLARE_TRACE=1.
- Full D3D11 game build passes. Night Cruise run
  `conformance_tmp/game_test/run-putssbyj` passed: 35.947 units forward, zero
  backward travel, 29 samples. Settings/gameplay captures were inspected; player
  and NPC tail glows and road illumination remain present.
- Remaining audit entries above are still required. In particular, investigate
  the depth-view-remap and dependent shader branches rather than assuming that
  a parameterized API makes every authored enhancement generic. An initial
  search found no MM3 caller of the old remap setter, but that is not yet a
  complete removal proof. Both renderer builds and final inventory are pending.


## Third boundary: retired distance remap

- Removed the uncalled `nv2a_d3d_set_depth_view_remap` API, its per-frame haze
  state, nonlinear Y16 remap and post-haze foliage coverage shader branch.
  The previous distance implementation has no active caller; the current MM3
  distance mod submits its own fog command instead. Reintroducing this retired
  enhancement in the generic toolkit would retain title policy with no user.
- Native Y16 word extraction, quarter-guest-pixel alignment and W-depth
  quantization/polygon offset remain. Existing actual-GPU Y16/depth checks are
  the regression boundary; no rendering enhancement is enabled by this removal.
- Both backends' 12 graphics-quality cases pass, including Y16 and W-depth.
  Full D3D11 game build passes. Night Cruise run `run-8l09mkp3` passed with
  39.846 units forward, zero backward, 29 samples; night settings and gameplay
  captures were inspected. Player/NPC rear glows appear in its final capture.
- This is behavior-preservation evidence only. The user's original live build
  separately has dark rear lamps in `run-996jw7qq/capture-035.bmp`, with the road
  ahead illuminated. Temporal lamp/reflection flicker and sky motion remain
  unresolved; individual lit captures do not establish visual fidelity.
- Full SDL GPU game build has been configured and is running. Remaining
  ownership audit is incomplete, including the dedicated fog shader and dead
  panorama/framebuffer mixing branch. The fog curve and destination-alpha sky
  mask must be reviewed as authored enhancement policy, not assumed generic
  solely because its inputs are parameters.


## Fourth boundary: unused panorama feedback

- Both backend PS flag producers set only bits 0..2. Removed the unactivated
  bits-8..15 panorama sky-colour mixing branch, its shader resources and its
  extra scene copies. Native render-to-texture feedback copies remain; texture
  binding indices stay stable (slot 8 reserved, volumes in 9..12).
- All 12 actual-GPU graphics cases pass after removal. Full D3D11 and SDL GPU
  game rebuilds pass after removal.
- The first full SDL GPU build of the previous depth-cleanup milestone passed
  Night Cruise run `run-dizlb5s6`: 34.074 units forward, zero backward, 29 samples.
  Inspected night settings and final gameplay captures; geometry, HUD, road
  illumination and player/NPC rear glows are present in those frames. The
  harness closed its own child. This does not resolve the separate temporal
  visual defects reported in the user's original running build.
- Dedicated fog enhancement ownership and the remaining runtime/artifact audit
  are still open; this milestone does not claim the fork is fully generic.


## Fifth boundary: active fog enhancement

- `src/mods/render_fog.c`, backend drawing headers and
  `src/mods/shaders/distance_fog.hlsl` now own the entire active enhancement:
  range validation, depth-to-distance conversion, exponential curve, sky fill,
  destination-alpha mask and camera-plane fade. MM3 builds its own embedded
  D3D11 shader and SDL GPU SPIR-V/DXIL. The fork no longer compiles a fog shader,
  has no dedicated fog API/flags, and has no fog-only pipeline blend branch.
- The generic `nv2a_d3d_host_draw` callback borrows current colour/depth handles,
  device and context/command buffer. The runtime flushes prior draws, restores
  its retained bindings after native client drawing and tracks colour writes.
  This follows the existing host-overlay interoperability boundary without
  giving the toolkit knowledge of MM3's fog parameters or private stream.
- Moved the three authored GPU cases to `tests/mm3_renderer_fog`. They reuse
  generic fixture setup/readback without making the fork depend on MM3.
  All 3 pass, including D3D11 MSAA and SDL GPU. The fork's independent suite
  passes 19/19, including new synthetic host-draw ordering/rejection/recovery
  checks. MM3 command, facade and initialization CPU contracts pass.
- Full D3D11 and SDL GPU game rebuilds are running after the public header
  change. Their final build and bounded gameplay proof remain outstanding.
  The overall runtime/artifact ownership audit is also still incomplete.


### Fifth-boundary full game proof

- Both full game builds passed. D3D11 night Cruise `run-jk8gwj39` passed:
  33.704 units forward, zero backward, 29 samples. SDL GPU night Cruise
  `run-14qffiay` passed: 39.440 units forward, zero backward, 29 samples.
  Inspected both night settings and final gameplay captures. Both logs confirm
  `[MM3_FOG] pass 853x480 depth-bits=24 samples=1`; the port-owned shader runs
  during real driving. Each harness closed only its child.
- These checks prove the moved path is playable and retains its tested pixel
  contracts; they do not establish a fix for the reported temporal lamp/sky
  defects. The wider ownership audit remains required.

## Sixth boundary: VP monitor calibration

- Found a hard-coded 0.316 multiplier in the generic VP-only monitor, justified
  by MM3 race measurements (GP -20.7 versus VP -10.7 dBFS, 0.4 percent clipped).
  MM3 now supplies that calibration in its CMake configuration. The generic
  VP monitor defaults to unity; voice/bin hardware mixing is unchanged.
- Existing production-VP attenuation and SGE checks pass. Generated APU build
  definitions on both backends confirm MM3 still supplies 0.316f. No claim is
  made here about full audio fidelity or measured output levels after the move.


## Seventh boundary: title-derived runtime fixtures

- Inline-event registration now uses synthetic PCs, event metadata and request
  sentinels. MM3's actual allow-list remains in `src/main.c`, outside the fork.
  Allocator churn tests use synthetic non-page-aligned block sizes instead of
  captured car-model sizes. APU physical-memory checks use synthetic addresses.
  The timer deadline case remains a generic deferred-work/rearm contract.
- Moved title-specific provenance below; runtime comments describe reusable
  mechanisms and limits. Standalone pacing instructions no longer depend on
  MM3's developer environment. Removed a no-op MM3_BUILDING_TRACE test setting.
- Fixed two existing standalone-test build gaps exposed by these checks:
  kernel_dispatch now enables the C11 dialect its included headers require;
  apu_phys supplies the physical resolver it previously failed to link. The
  resolver fixture also checks accepted synthetic runtime IDs and rejection of
  IDs outside the 28-bit bus before they can alias a cached contiguous page.
- Validation: 9 inline-event caller cases, 9 timer lifecycle/deadline cases,
  all 7 memory regressions, APU physical mapping and kernel-bridge tests pass.
  These are standalone fork builds with no MM3 source or assets linked.

### Title-derived regression provenance retained by MM3

- PAL inline-event callers: setter 0x0035C348, wait 0x0035D553, cancellation wait
  0x0035D56C and auto-reset wait 0x0035C7F1. The original request callback was
  0x0035C33A; 0x00343D57 was an unrelated caller. Return PCs are still configured
  by the MM3 entry point; generic tests need only distinct registered/unrelated
  identifiers, not these addresses or the captured request/header words.
- Guest packet ABI evidence came from default.xbe SHA-256
  2b04b66c43e7f37bbcebbfb5b72ccb96a2aa99cc2c60c53eb7d3530bcc2a3d79:
  0x35B2B3/0x35B2B6 read stack+0x0c/+0x04, 0x35B2D3 reads +0x10,
  0x35B2F4 tests flags+2, 0x35B2FA reads buffer+8, and 0x35B1DE reads
  IoControlCode+0x10. This is static evidence of driver expectations, not proof
  of runtime reachability, hardware conformance or backing-device transfers.
- MM3 audio callback sub_0027B554 -> sub_0027B389 motivated the future absolute
  timer deadline case. The test executes a synthetic callback, not guest code.
- Original race/menu/model-churn allocation sizes were 2441264, 2818096,
  1638448, 1720368, 1261616, 1458224, 1635472 and 1342608 bytes. The generic
  contract concerns release/reuse, zeroed allocations, alignment and intact
  neighbours; its synthetic replacement preserves those stress conditions.
- 0x01191EA0 was an observed streamed-voice heap pointer. Identity mapping
  below/above 64 MB and unmapped-address rejection are generic bus contracts;
  their fixture no longer retains that title pointer.


## Eighth boundary: async wait diagnostics

- A deeper address audit found three literal MM3 return PCs still embedded in
  `bridge_async_wait_log`. They selected ESI for MM3's auto-submit helper and
  EBX for its explicit wait/cancel helper, then decoded request words by field.
  The logger now records guarded raw 12-word snapshots from both registers.
  The generic wait implementation no longer selects a title layout or caller.
- MM3 interpretation: caller 0x0035C7F1 retains its request in ESI; callers
  0x0035D553 and 0x0035D56C retain it in EBX. Request dwords 1/2/3 were status,
  callback and context. This knowledge stays here and in MM3 diagnostics,
  including `src/recomp_manual.c`, rather than the reusable wait bridge.
- The generic trace regression now uses a synthetic return PC and raw sentinel
  words. Trace-off, trace-on and inline-event cases pass, covering both readable
  and unreadable register pointers, real wait outcomes, preserved guest state
  and LastError, and separate bounded success/failure log budgets.
- `request_*` fields in ASYNC_WAIT logs are replaced by `esi`, `esi_read`,
  `esi_words`, `ebx`, `ebx_read`, `ebx_words`. No runtime behavior opt-in changed.


## Ninth boundary: sharing diagnostics and save-path fixtures

- The opt-in file ledger hard-coded `Data_hd.zip`, and sharing-failure evidence
  was further limited to `Cache/Data/Data_hd.zip`. Removed both title filters:
  `RECOMP_FILE_DIAGNOSTICS` now records shared-file owners for any normalized
  file path. Access, share modes, retries and handle ownership remain unchanged.
- The path fixture no longer embeds title ID 4d53002a, observed save-slot IDs,
  `Data/Shared/Cars/VRace/body.cmp` or `Data/Data_hd.zip`. It uses synthetic
  01234567 title data and example assets while retaining drive routing, path
  case handling, save isolation and non-creation contracts.
- Sharing and diagnostics cases pass, including a real conflict on synthetic
  `sharing.bin` and guest-owner evidence. The standalone save-path case passes.
  Both full D3D11 and SDL GPU game rebuilds pass after this boundary.
- This audit found real policy beyond literal MM3-name matches. Continue the
  remaining runtime/tooling review and final repository inventory; the goal
  is not yet marked complete.

## Night rendering validation caveat (2026-10-20)

- MM3 fba38ce and XboxRecomp a18f4eb restore the native kick acknowledgement
  and resolve constant render-target taps at guest pixel scale. All 13 graphics
  checks passed; the flush handshake regression passed. SDL GPU D3D12 and
  Vulkan builds and bounded night-driving checks completed.
- Distant-light flicker remains unresolved, with small forward/back movement
  at the reported location providing a live reproduction. Sky jumping and
  rooftop bloom blur have not been demonstrated resolved. These commits do
  not establish complete visual fidelity on either D3D12 or Vulkan.
- The live flare-depth override was an investigative A/B change only; it is
  not a committed default change or evidence that flicker is fixed.
- Ordinary ambient traffic assets lack the separate road-light projector
  present on the player car. Their glowing headlights without ground beams
  appear consistent with original-disc asset design; original-console runtime,
  police and race-opponent behavior remain unverified.


## Black-sky root cause and fix (2026-10-21)

- User confirms the building-window shimmer also occurs in xemu; it is no
  longer a renderer-fix acceptance condition. Vehicle-lamp queue exhaustion
  is addressed separately below.
- A normal-settings D3D12 diagnostic run (`run-sbx4439m`) captured seven brief
  whole-sky blackouts. Every detected flash followed a logged omission of the
  custom fog packet because the native push cursor had reached its soft limit
  or fewer than 48 bytes remained. No host-fog rejection was logged. The
  missing packet also omitted the destination-alpha sky mask.
- `sub_000F8DCE` now uses native D3D rollover (`sub_00344770`, the same block
  sizes as the native setters) before writing the packet. It preserves guest
  registers and stack arguments. No renderer-specific workaround is needed.
- `mm3_draw_distance_contract` fails before the fix and passes afterward. It
  covers exact fit, insufficient space, cursor past limit, guest ABI and the
  disabled-mod path. Full SDL GPU and D3D11 builds and both contract binaries
  pass. Temporary diagnostics were removed from production source.
- Normal-settings night Cruise checks passed on D3D12 (`run-rhh2u9d0`) and
  Vulkan (`run-asiqn_dq`): 120-second Windows Graphics Capture videos each,
  including short forward/reverse movement, teleport settling and idle. The
  same whole-sky temporal check found zero flashes in 7,199 normalized output
  frames per backend; darkest frames were inspected and retain the sky.
  This is bounded proof for the captured blackout, not full rendering parity.
- Evidence: `conformance_tmp/sky-fog-correlation.json`, `sky-trace-live.mp4`,
  `sky-fixed-d3d12.mp4`, `sky-fixed-vulkan.mp4` and their `.sky-check.json`
  reports. The earlier sampling experiments were not published.


## Vehicle-lamp queue exhaustion (2026-10-21)

- Native flare submission (`sub_001A831B`) rejects records when the scene's
  256-entry queue fills. A baseline river-scene diagnostic captured rejected
  red rear-lamp and white headlamp records at the player's lamp positions.
  Enhanced visibility exhausted this shared budget before vehicle submission.
- Scale only the scene's original 256-entry constructor request by the larger
  draw/view multiplier. The native allocator grows the record array and all
  three buffered vertex allocations together. Mod-off, stock-distance and
  unrelated constructor calls retain their original budgets and guest ABI.
- Full SDL GPU and D3D11 builds pass. Eight contract runs cover both builds,
  stock/enhanced settings, other callers/capacities and guest ABI preservation.
- D3D12 and Vulkan night Cruise captures include repeated forward/reverse
  movement with close chase view. Both rear lamps remain lit in the inspected
  unobstructed views, including all 1,350 normalized frames in each capture's
  first 45 seconds. D3D12 reached 261 queued flares, exceeding the old limit.
  Default saved settings allocated 512 records and three 229,376-byte VBs;
  Vulkan maximum-distance settings allocated 1,024 and three 458,752-byte VBs.
  Vulkan's sampled traffic did not exceed 256; this remains bounded gameplay
  validation, not proof for every scene. Native occlusion remains enabled.
- The same sky check found zero black flashes in both 60-second captures.
  Evidence: `conformance_tmp/tail-budget-{d3d12,vulkan}.mp4`, matching
  `.lamps-metrics.json`, `.sky-check.json`, native/pressure reports and
  `tail-budget-contract-results.json`. No experimental sampling/shader changes
  are included in this fix.


## Renewed whole-fork audit (2026-10-21, in progress)

The previous ledger is historical evidence, not a completed ownership audit.
Baseline: MM3 `253565f`, fork `a18f4eb`; upstream main fetched at `f0f8a461`.
Work remains isolated in `I:/repos/mm3-ownership-main`. No unrelated running
instance or original checkout was stopped or edited.

### Tenth boundary: loading presentation policy

- Removed the runtime's built-in amber spinner, MM3 timeout, environment
  selection and shader drawing. Both renderers expose an optional idle-present
  policy callback, with time since the last native flip and movie activity.
  No hook requests no extra idle presentations; native CPU/movie scanout stays.
- MM3 owns the 700 ms threshold, enable setting and animation phase in
  `render_policy.c`, and draws the arc through its existing ImGui host overlay.
  Spinner-only registration has no event/input-capture handlers; the debug
  overlay still requires its existing opt-in.
- Full SDL GPU and D3D11 game builds pass. MM3 policy tests cover threshold,
  movie suppression, flip reset and disable; actual SDL tick-body checks cover
  no-hook versus requested redraw and both pacing orders. The debug-overlay
  input contract passes. Generic binding suite: 19/19 pass; graphics suite:
  13/13 pass. Explicit Vulkan reruns pass all 9 binding and 7 graphics cases.
- Build evidence: `conformance_tmp/ownership-spinner-*-build.log`; contracts:
  `conformance_tmp/verify_spinner.py`. Vulkan night Cruise `run-7fc1utu4`
  passed 49 actions including forward/reverse input and fresh position
  telemetry; gameplay capture was inspected. This does not finish the broader
  ownership audit or establish pixel-identical spinner animation.

### Eleventh boundary: dormant captured-menu prototype

- Moved `nv2a_pb_replay.c`, its empty menu/font capture placeholders,
  `nv2a_pb_test.c` and the title-profiled `nv2a_pgraph_d3d11.*` to
  `tools/reference/legacy_nv2a` in MM3. They retain their original bytes for
  provenance and are explicitly not a supported/buildable runtime plugin.
- No MM3 caller initializes that translator or calls the replay/test activation
  functions. Its PGRAPH entry always returned unhandled until initialized.
  Removed its dormant dispatch branch and build dependency. The actual
  pushbuffer executor/host rendering and register-state fallback remain.
- This removes Burnout 3 menu navigation, font address/name mappings, chyron
  animation, external frontend globals and the command-injection test's fixed
  guest pointer defaults from the fork without adding an unused hook API.
- Both full game builds pass after removal. Cumulative night-driving evidence
  is recorded in the next boundary.

### Twelfth boundary: kernel import defaults

- Removed the legacy 0x0036B7C0 thunk-table default and reference-title import
  order. MM3 already supplies its own address/count in `src/main.c`; the generic
  loader also parses each XBE. The old import list is retained only as reference
  in MM3 `tools/reference/legacy_kernel_imports.h`.
- Unconfigured bridge state is genuinely zero, matching the public API
  contract. Bridge initialization returns before touching guest memory when no
  thunk table is configured. The host import table stays empty without an XBE;
  callers needing a canonical export can use `xbox_resolve_ordinal` directly.
- Extended the existing kernel bridge regression: no legacy fallback,
  initialization without mapped memory, empty host table, synthetic XBE import
  order/count and normal bridge dispatch all pass. Both game builds pass.
  Cumulative Vulkan `run-cgs0jo4s` (128.6 s) and D3D12 `run-x3dosfz8`
  (123.7 s) passed 49 actions each, including forward/reverse motion and fresh
  player telemetry. Captures were inspected: scene, sky, HUD and lamps remain
  present. Foreground NPC occlusion in final captures is not a lamp failure.
  This is preservation evidence, not full rendering or runtime conformance.
- Existing D3D11 pacing extraction had fallen behind the flip-present wrapper
  and interpolation path. Updated its fixture to exercise the actual bodies;
  both before/after-present orders pass alongside the SDL pacing checks.

Remaining audit: full runtime/platform defaults and guest-address selectors;
legacy renderer and input behavior; tools, templates, tests and documentation;
usefulness and real consumers of fork-only APIs; both-backend preservation
proof; final fork commit/root pin and source/worktree inventory. Do not mark
this goal complete from the spinner migration or a clean name search alone.


### Thirteenth boundary: online-service DNS policy

- Removed hard-coded Insignia alias names/destinations from the generic NAT.
  `net_backend_set_dns_aliases` accepts a caller-owned table before startup;
  no table means ordinary DNS forwarding. MM3 installs its existing four rules
  in `src/main.c` before hardware/network initialization.
- Generic network tests use `name.test` -> numeric loopback, not a live service.
  They cover no default alias, truncated/unrelated/non-A requests, the actual
  guest UDP DNS answer and clearing the table. Both network/NIC ring cases pass.
  Both full game builds pass after integration. No online login or
  service-availability claim is made by these local tests.


### Additional areas inspected in this continuation

- Input keymap defaults are console button/axis mappings; title-context keyboard
  behavior already uses a game-supplied hook. No fixed MM3 asset/guest address
  was found in the inspected XInput or USB descriptor/report paths.
- `nv2a_ps_manifest.h`, `nv2a_pacing.h` and `nv2a_sampler_cache.h` retain bounded
  generic cache/host timing/resource mechanisms, with no baked title activation
  or authored data in those inspected helpers.
- Lifter/disassembler name matches inspected so far are regression provenance
  and XBE/config-driven discovery, not a sufficient whole-tool audit. Remaining
  runtime/tool/template review and final inventory are still required.


### Fourteenth boundary: analysis-tool image layouts (2026-10-21)

- Removed Burnout 3/Dashboard section, entry-point and thunk defaults from
  disassembly, function identification and recompilation configuration.
  Unconfigured modules contain no image ranges. Installing another image clears
  absent optional ranges and zero entry/thunk values rather than retaining the
  previous image's metadata. Removed authored translation-resource names from
  the recompiler's data-section overrides; conventional section/XDK rules stay.
- Function identification configures the supplied XBE in its public `run()`
  entry, covering direct callers as well as CLI use. The removed reference
  layouts are recorded only in MM3 `tools/reference/legacy_section_layouts.json`.
- `analyze_unresolved.py` now requires the matching XBE and accepts explicit
  functions/symbol/output paths. It classifies resources from parsed code/data
  flags instead of treating every unfamiliar section as a library. No fixed
  title address table remains in that tool.
- Nine focused checks cover empty defaults, sequential different-image layouts,
  direct identification API setup and unresolved reports for a synthetic image
  beyond the old ranges. Full recompiler/disassembler/identifier suite: 535
  passed, 17 skipped, 69 subtests passed under the MSVC environment with a private
  pytest temp directory. The initial unconfigured-environment run failed on
  missing C headers/temp access; that was corrected before collecting results.
- Actual MM3 XBE comparison: both old/new configured recompilation and function
  identification expose the same 35 sections and address ranges, entry and
  thunk data (`conformance_tmp/ownership-tool-layout-parity.json`). This is
  metadata/translation-contract proof, not a full game regeneration or new
  gameplay acceptance run. Previous runtime checks remain separately scoped.

### Fifteenth boundary: obsolete runtime starter implementations

- Moved the old `xbox_memory.h` section-address/allocator blueprint and the
  `kernel_stubs.h` success-stub bodies into MM3 reference storage. Their template
  include paths remain, forwarding to the supported runtime memory/kernel
  headers instead of encouraging duplicate allocators or fake kernel success.
- Updated the runtime guide and template inventory to use the real runtime.
  A standalone MSVC syntax check including both replacement template headers
  passes (`conformance_tmp/check_runtime_template_headers.py`). No new runtime
  implementation or title-specific compatibility API was added.
- Runtime/API usefulness and full artifact inventory audit remain open; the
  goal is not complete from these tooling/template migrations.


### Sixteenth boundary: feedback window follows the loaded image (2026-10-22)

- Removed the 8 MiB indirect-target recorder window derived from a reference
  title. The loader configures the recorder from actual executable bounds.
  Standalone hosts may configure the same generic mechanism before workers
  start. Reconfiguration clears prior target/site observations; invalid ranges
  leave the active window unchanged, and allocation failure disables recording
  for that load without changing guest execution.
- Code/image bounds now reset when a new image's sections are loaded, preventing
  absent executable sections from inheriting a previous image's range.
- Target-dump checks cover addresses beyond the old limit, lower/exclusive upper
  boundaries, overlapping flags, invalid 32-bit ranges, a different second image
  and clearing. Enabled and disabled standalone kernel builds both pass. The
  enabled loader fixture records a synthetic high-code target and clears
  it when loading a second image with no code sections.
- Enabled the existing runtime feedback tests on MSVC rather than skipping them.
  Recompiler/disassembler/identifier validation: 538 passed, 15 skipped,
  69 subtests passed (`ownership-tools-recomp-tests.log`).

### Seventeenth boundary: host movie presentation and exit policy

- Removed the `.wmv` auto-play selector and 60-second movie-completion wait from
  kernel bridges. MM3 owns them in `src/mm3_host_video.c`, using the existing
  optional `RECOMP_FMV_HOST` mode and generic video player. Normal guest opens
  in the fork do not select host playback or delay firmware return.
- Added startup-only successful-host-open and firmware-return observers. File
  observers receive borrowed native paths after result publication, preserving
  the caller's host and translated error state. Firmware notification belongs
  in the shared HAL entry, covering direct and bridged calls with no default
  product policy. Kernel/video linkage remains for GPU/scanout dependencies.
- MM3 policy contract passes with mocked playback/waits: opt-in selection,
  UTF-8 path conversion, active-playback guard, completion and the 60-second
  ceiling on Windows and Linux. Generic sharing tests verify success-only open notification and
  caller error preservation even after an observer's nested failed open.
- Standalone kernel builds with feedback on/off each pass all four bridge/
  firmware tests (default, callback and cleared callback). The full kernel
  dispatch suite passes 61/61 cases. Both full game builds pass. Vulkan
  `run-50818hrf` and D3D12 `run-5d74q20k` each passed 49 night-Cruise actions
  in 123.4 seconds, including movement and fresh player telemetry. Driver logs
  and captures were inspected; visible sky, geometry, HUD and rear lamps remain.
  The Vulkan final view includes normal foreground-NPC occlusion. These are
  bounded preservation checks, not host-movie decode parity. POSIX compilation
  of both changed file/HAL source modules also passes.

### Confirmed next cleanup: retired menu-quad observation

- The native MM3 menu mouse reads only the presented viewport via
  `nv2a_d3d_menu_rects(..., 0, present)`; it does not consume renderer quads.
- The input runtime retains pointer-gated quad collection, but no production
  caller registers that retired pointer callback. Its dormant machinery computes
  CPU vertex colours and stores per-frame rectangles with no current consumer.
  Replace that retired path with a generic presentation-rectangle getter and
  retain native menu hit-testing. Trace both renderers/executor/input and their
  contracts before removing the unused collection machinery.


### Eighteenth boundary: retired renderer-quad menu observation

- Removed dormant quad arrays, per-frame copies and CPU vertex-tint extraction
  from both renderers and the executor. The input backend no longer couples
  its optional pointer callback to renderer quad capture. Retired source
  fragments are retained in MM3 tools/reference/legacy_menu_quads, uncompiled.
- MM3 native menu input reads a generic presentation rectangle directly. The
  getter copies the last composited viewport under the existing backend lock;
  viewport publication and native menu geometry/focus remain unchanged.
- Both complete Windows game builds pass. Production getter checks cover both
  backend bodies, locking, initial/current viewport and independent copies.
  Native mouse/mod initialization (3), input (4), keymap (2), before/after
  presentation pacing and all 61 kernel-dispatch plus 19 binding tests pass.
  Evidence: ownership-menu-{contracts,runtime}.log and both build logs.
- Fresh owned Night Cruise checks passed all 49 actions on Vulkan run-bz7fljh7
  (123.5 seconds) and D3D12 run-nnn5kru1 (123.454 seconds), with backend logs,
  movement telemetry and captures inspected. Vulkan shows both lamps before
  later foreground-traffic occlusion; D3D12 ends in bonnet view, so supplies
  scene/sky/HUD preservation evidence rather than rear-lamp evidence. These
  bounded runs do not establish full temporal fidelity or native mouse UI flow.
- Getter contracts also pass under WSL Linux. Actual input-capture/release
  contracts pass in keyboard/pad and physical-only modes.
- Renderer overlay hooks and the remaining input callback API still require
  usefulness review; this boundary does not complete the whole-fork audit.


### Nineteenth boundary: retired pointer-to-D-pad navigation

- No production code registers xbox_InputSetMenuPointer. Removed that unused
  public callback and its highlight stabilization, row/strip pulse scheduling,
  and focus-wait policy. Retained the old implementation in MM3 reference
  storage beside the retired quad collector, with its original provenance.
- The shared input path retains its active native-click callback and generic
  mouse-to-A fallback. With no pointer callback, the old path immediately
  started the same 80 ms pulse; the simplified path preserves that behavior
  without dormant focus heuristics. Native MM3 geometry/focus is unchanged.
- Removed historical title-specific API call counts and title-named public
  header guard. The header describes the reusable input ABI only.
- Native mouse/mod checks (3), input checks (4), keymap checks (3), viewport and
  before/after pacing checks pass. A deterministic production-input fixture
  verifies consumed clicks, pulse boundaries, held-button suppression and a
  new press; assertions remain enabled in Release builds.
- Both full Windows game targets rebuild successfully. Input-capture contracts
  pass in keyboard/pad and physical-only modes for both builds. Evidence:
  ownership-pointer-contracts.log and ownership-pointer-*-build.log. There is
  no fresh gameplay or Linux input-runtime acceptance claim for this boundary.
- The broader runtime, tools/artifact and final inventory audits remain open.


### Twentieth boundary: generic thread creation defaults to spawning

- Both bridge thread-create APIs previously privileged the first call as the
  title's inline main routine by default. Default to the existing spawned
  execution path instead; ports may explicitly select first-call inline
  compatibility before startup. MM3 already selects SPAWN for normal operation
  and INLINE for its diagnostic environment option, so that policy stays in
  the game. No new runtime selector or title address was added.
- Actual thunk-dispatch tests cover ordinals 254 and 255 in fresh child
  processes. Default calls use a different host thread, preserve the caller's
  registers, receive both start contexts, use a separate guest stack and, with
  the fixture's valid TLS directory, an independent TIB. Explicit inline calls
  retain their documented register/stack/context behavior. Spawned handles
  close after joining the worker, before guest memory is released.
- The two default-mode cases failed against the prior inline default while
  explicit inline cases passed. After the change all 8 bridge tests pass with
  feedback disabled and enabled; all 61 kernel-dispatch tests and both complete
  Windows game builds pass. Logs: ownership-thread-{red,green}-tests.log,
  ownership-thread-bridge-feedback-tests.log, ownership-thread-kernel-tests.log
  and ownership-thread-{sdlgpu,d3d11}-build.log.
- An initial fixture check assumed an exact bridge stack offset and omitted
  a TLS directory. Those fixture assumptions were corrected before accepting
  the results. No new Linux thread-runtime or fresh game-driving acceptance
  claim is made. Existing worker-error/fallback behavior remains a separate
  runtime correctness issue, not proof of full kernel thread semantics.
- Full runtime/API, artifact and final inventory audits remain incomplete.


### Twenty-first boundary: title-specific runtime migration history

- Moved docs/technical/burnout3-reunification.md out of the fork into MM3
  tools/reference/historical_runtime. Its migration phases, title measurements
  and status are historical, not generic toolkit functionality or current
  acceptance evidence. The original bytes match fork f8b1214 exactly (SHA256
  d5bbf7c24c83fe2badebdfd7d2fb4a4f0ace50e7e14d11fadd0587e2dd5e99d7).
- Replaced both documentation-index entries with the current generic runtime
  guide. Removed the remaining header reference to the retired report and
  documented active callback-stack consumers: DPC/timer, network, USB and APU.
  The source call paths confirm the pool is useful generic runtime code.
- Reviewed src/host/host.c and xbox_host.h: hardware fault routing and graphics
  initialization are active reusable mechanisms, with caller-supplied window
  title and hardware/backend selection. No title policy move justified there.
- Verified the archive byte comparison, new documentation target and diff
  whitespace. This documentation/reference move changes no executable code,
  and adds no new game-runtime acceptance claim.
- Next runtime review: the callback stack pool still derives guest addresses
  from XBOX_WORKER_STACK_BASE while spawned-thread stacks use image-aware heap
  allocations. Its active cross-subsystem callers require range/lifetime
  validation before concluding that the fixed pool is safe for other images.
- The full artifact/runtime and final inventory audits remain incomplete.


### Twenty-second boundary: image-aware callback-stack placement

- The active callback pool used the fixed 0x780000 guest range even when an
  image occupied it. A real-loader synthetic image at 0x910000 reproduced
  corruption: filling all leased callback stacks overwrote its code bytes.
- The loader now places the existing pool page-aligned beyond all loaded
  sections, retaining the conventional runtime stack base when it is already
  above the image. Both fresh and reused ordinary heap allocations stay above
  the image/pool ownership floor. No per-title classifier, new allocator or
  per-callback heap churn was introduced; the bounded slot allocator remains.
- A pool that cannot fit in mapped guest RAM is disabled. Shutdown clears its
  placement and leases; the next load computes a fresh range. The startup-only
  reset interface also supports standalone caller-owned guest mappings.
- Regression checks fill all 16 leases and preserve image bytes, check pool
  exhaustion/release, shutdown and image replacement, place an image above the
  conventional heap, exclude that relocated pool from heap allocation, and
  reject pool/heap allocations for an image reaching mapped RAM's upper edge.
  The corruption check failed before the repair and passes after it.
- Validation: both 8-case bridge builds (feedback on/off), 61 kernel cases,
  7 memory regressions, 2 network cases, APU physical mapping and both complete
  Windows game builds pass. Evidence: ownership-callback-*-{build,tests}.log.
  Actual MM3 XBE inspection finds 35 sections ending at 0x489DE0 and the same
  0x780000 callback placement as before (ownership-callback-mm3-layout.json).
  No fresh gameplay or Linux callback-runtime acceptance is claimed here.
- Full artifact/runtime and final inventory audits remain incomplete.


### Current source inventory and retained mechanisms (2026-10-22)

- XBOXRECOMP_SOURCE_INVENTORY.json records 153 current src paths returned by
  FFF, with working-tree SHA256 values at fork 887b747. Sixteen stale indexed
  paths are explicitly excluded. Each recorded path exists and its hash was
  rechecked. This is a discovery snapshot, not a claim that FFF covers every
  tracked artifact or that every line in these files has been audited.
- Source groups in this snapshot: video 35, kernel 32, APU 32, D3D 16,
  NV2A 8, platform 6, audio 6, network 5, USB 5, input 4, host 3 and the
  source README. Build rules, documentation and license files are included.

| Reviewed area | Current ownership decision and evidence | Remaining proof |
| --- | --- | --- |
| Host startup | Retain hardware fault routing and graphics/device initialization; caller supplies the title. host.c, xbox_host.h and CMake target inspected. | Final build/platform inventory |
| Frame replay/interpolation | Retain generic GPU method/state/vertex recording and replay. MM3 framerate.c actively calls repeat/interp APIs; its ticks, queue counters and opt-in policy remain in MM3. | Full recorder/lifetime ownership pass beyond the inspected recording/replay sections |
| Kernel audit tools | Retain import/bridge coverage and name-based ordinal-remap generation: inputs are caller analyses and runtime export data. Tool bodies inspected; no title activation/default data found in those inputs. | Remaining tooling and fixture inventory; coverage is not kernel implementation proof |
| Shader metadata/cache | Inspected PS manifest format/checksum/family validation and texture-hash implementation entry; these are cache mechanisms, not authored title selection. | Remaining cache paths and usage/test cross-check |
| NV2A library target | CMake now builds register/MMIO core only; retired captured menu/prototype renderer is outside the fork. | Final tracked-artifact check against index and build graph |

No new executable behavior was changed by this inventory pass. The prior scoped
runtime tests remain scoped to their corresponding boundaries. Remaining work
includes whole-fork tracked-artifact coverage, the rest of each partially reviewed
source area, final cumulative runtime checks and repository/publication inventory.


### Twenty-third boundary: authored vertex-alignment selection

- Both renderers selected a special 0.53125 vertex offset solely for 640x480
  targets, while other sizes used pixel-zero edge snapping. Moved that fixed
  target-size classifier into MM3 render_policy.c. Its hook is installed with
  the existing MM3 overlay hooks, preserving the original choice in both
  backends and with widescreen disabled.
- XboxRecomp retains the shared shader coordinate operations and an optional
  vertex_alignment callback. Both backends use one shared parameter helper;
  without a hook there is no display-size selector. Default snapping and zero
  offset apply at every target size. No authored shader/texture identity was
  added to the runtime.
- The GPU placement fixture now explicitly exercises default and caller-hooked
  behavior at display and offscreen sizes, rather than retaining the old size
  classifier in its expected behavior. All 13 graphics-quality and 19 binding
  cases pass. Both complete Windows game targets build. MM3 policy and actual
  hook-wiring/overlay contracts pass on Windows; the policy contract also passes
  under WSL Linux. Logs: ownership-alignment-{quality,bindings}-tests.log,
  ownership-alignment-{sdlgpu,d3d11}-build.log and alignment-root-tests.log.
- The first Linux policy check exposed the fixture's pre-existing empty-env
  mismatch with Windows _putenv_s. Its POSIX shim now unsets empty values and
  declares POSIX APIs correctly; the runtime's spinner logic was not changed.
- Fresh owned Night Cruise checks passed all 49 actions on Vulkan run-i08uu7t6
  (123.797 seconds) and D3D12 run-l5g4rqc0 (123.375 seconds). Backend logs,
  movement telemetry and final captures were inspected. Both show rear lamps,
  road lighting, sky, scene geometry and HUD. These are bounded preservation
  checks, not full temporal/visual fidelity or all-menu-flow acceptance.
  The broader source/artifact and final inventory audits remain open.


### Twenty-fourth boundary: starter reuses shared host bring-up

- The generic new-game starter retained a second APU bootstrap/MMIO handler,
  an unhandled GPU-register TODO and no shared USB/NIC bring-up. Removed that
  duplicate hardware implementation; the starter now calls existing shared
  host hardware/graphics interfaces and shuts hardware down before kernel and
  guest-memory teardown. The umbrella header exposes the existing host API.
- The starter explicitly selects first-call INLINE for its minimal guest-driven
  loop, preserving its historical model after the generic runtime default
  changed. A host-driven port chooses SPAWN and owns its live-worker lifetime;
  that policy is documented at the port integration boundary.
- Archived the previous starter source from fork 6994d5d in MM3
  tools/reference/legacy_host_starter (reference only). Its diagnostic guest
  register/trace support remains in the active starter. Corrected stale public
  header startup/API/TLS examples and template library-count comments.
- Four runnable Windows contracts compile the actual starter source against
  runtime headers with mocked runtime boundaries. They verify success and
  memory/hardware/graphics initialization failures, exact startup/cleanup order,
  explicit thread mode, and that failure does not enter guest code. All four
  pass; assertions are enabled in Release. Both complete MM3 Windows game
  targets also rebuild successfully. Logs: ownership-starter-*.log.
- This is starter control-flow/header/build proof, not actual emulated MMIO,
  audio, a newly recompiled title, Linux starter support or new MM3 gameplay.
  Whole-fork source/artifact and final inventory audits remain open.


### Twenty-fifth boundary: remaining DPC fixture identity captures

- Found a DPC regression still carrying two observed vtable addresses and a
  captured return PC. Production code does not match those identities; the
  fixture uses them only as distinct live/dead markers and a stack-write
  pattern. Replaced them with synthetic addresses in its existing arena and
  preserved the original values/provenance in MM3 legacy_section_layouts.json.
- Kept the actual generic failure conditions: completion before destruction,
  intrusive-list lifetime, a request below the caller's ESP, callback stack
  isolation/exhaustion, IRQL state and nonrecursive/concurrent DPC draining.
  No runtime activation, branch or game implementation changed.
- All 61 kernel-dispatch cases pass, including all eight DPC-lowering modes.
  Evidence: ownership-dpc-identities-{build,tests}.log. This fixture-only move
  provides no new game-driving, graphics or Linux acceptance claim.
- Reviewed the kernel identity export defaults: standard console hardware,
  version, boot filename and zeroed HLE keys are generic console configuration,
  with a caller version override; they do not select a game. The settings-store
  implementation beyond those declarations still needs its own ownership pass.
- The inspected memory-regression README now describes synthetic scene-load
  sizes, rather than the stale indexed title-sized description. Retain those
  allocator contracts; discovery-index hits alone cannot establish current
  artifact content. Full source/artifact and final inventory audits stay open.


### Retained area: console settings and DSP capture/replay (2026-10-22)

- Completed the ownership pass of kernel_xbox.c: console identity/version
  exports, eager-section reference counters, 96-byte user settings with
  checksum/locked replacement, MAC selection and virtual PHY entry points.
  Retain these generic HLE mechanisms. Save-root choice is caller-owned;
  host video/network overrides are generic configuration. No title selector
  or game-specific address dispatch was found. Existing HLE limitations are
  not a reason to claim complete hardware fidelity.
- The current source hash matches the recorded inventory snapshot; that file
  now carries an explicit retain decision and review scope. No implementation
  changed. kernel_settings passes on Windows and WSL Linux, with fresh private
  roots: restart persistence, independent roots, concurrent updates/readers,
  aggregate/scalar fields, host override isolation and corrupt-image rejection.
  Evidence: ownership-settings-windows-{build,tests}.log and
  ownership-settings-linux.log. These tests do not use the user's saved settings.
- Inspected DSP capture-program extraction, frame replay, clipped-capture
  fixture and their CMake pipeline, plus the APU fixture's generated-program
  setup. Retain the generic capture/replay tools: inputs are caller paths and
  tests build synthetic opcode programs/captures rather than embedding a game
  program. Byte comparison checks replay state/transfers; it does not imply
  full original-console audio equivalence.
- The existing Windows DSP build and all 19 cases pass, including generated
  recompiler replay. DSP_XEMU_REFERENCE_DIR is empty in this build; no pinned
  external reference comparison was run. Evidence: ownership-dsp-audit-*.log.
  The remaining DSP/runtime code, provenance inventory, all tracked artifacts
  and final cumulative/repository proof still require the broader audit.


### Retained area: DSP runtime capture and program export (2026-10-22)

- Reviewed GP/EP register/memory-window, FIFO/PCM/frame flow and capture,
  bootstrap, program-export and execution scheduling paths in apu_dsp.c and
  dsp/dsp.c. Retain those generic hardware/diagnostic mechanisms. Capture
  prefixes and program output directories come from the caller; exported words
  come from guest program memory, not an embedded title image or name selector.
- Added an actual runtime export check with a synthetic NOP/jump loop, run
  separately for GP and EP. Interpreted execution triggers automatic program
  collection; the exported fixed-size image contains the executed loop, omits
  an untouched nonzero data word, and leaves guest memory intact. Each test
  removes only its exact private build-directory output and refuses stale data
  if removal fails. Frame capture is disabled for that fixture.
- All 21 current Windows DSP cases pass, including both export checks. Both
  new export checks also compile and pass under WSL Linux. Evidence:
  ownership-dsp-export-{build,tests}.log and ownership-dsp-export-linux.log.
  The Linux run covers these two cases only; no external xemu reference was
  configured for the Windows suite.
- Inventory source hashes match the previously recorded runtime files; the
  scoped retain decisions are now attached to those entries. No runtime
  implementation or title policy changed in this pass. Remaining ISA/source
  provenance review, complete audio fidelity and whole-fork artifact/final
  inventory acceptance remain distinct and unproven.


### Retained area: supplied DSP programs and guarded translation

- Reviewed the DSP generator and APU build wiring. The compiler reads caller
  interpreter sources/program directories and emits into the build tree. Its
  dispatch comes from the interpreter opcode tables; each generated word is
  guarded against current program memory. There is no embedded title program
  or game-name/identity selector in this path.
- MM3 CMake already supplies game_files/dsp, and MM3 main chooses its runtime
  capture directory. Retain that per-title path policy in MM3. The toolkit's
  optional XBOXRECOMP_DSP_PROGRAM_DIR remains a generic input, not a default
  reference-image path. Missing supplied programs emit fallback stubs.
- Extended the existing generated-frame replay probe: a modified current opcode
  returns zero without changing the copied DSP state, then the matching program
  still executes generated code. Actual frame replay retains its state/transfer
  comparison. This proves the inspected guard path rather than merely relying
  on the generator comment.
- All 21 Windows DSP cases pass. The generated-replay target also builds and its
  case passes on WSL Linux. Evidence: ownership-dsp-guard-{build,tests}.log and
  ownership-dsp-guard-linux.log. The Linux run is that single case; no external
  xemu reference, full audio or new MM3 gameplay claim is made.
- Recorded the compiler review and the unchanged APU CMake source hash in the
  inventory. Other tooling, remaining runtime/source-provenance areas and the
  whole-fork artifact/final repository proof remain open.


### Retained area: generic network transport and caller routing policy

- Reviewed backend queue/checksum/packet assembly, ARP, DHCP, UDP socket NAT,
  peer framing/receive loop and startup, plus its public adapter/backend header.
  Retain those generic virtual-network mechanisms. No title identity selects
  these paths. Ordinary virtual subnet/router MAC/tunnel defaults are transport
  configuration; service-provider names remain caller-owned policy in MM3.
- The FFF service-name hits refer to an old source snapshot. Current source
  has only an optional caller-owned alias table, empty by default. Tests use
  name.test and numeric loopback, and verify default/clear, unrelated/truncated
  query and non-A cases. No online DNS/service dependency is needed by the test.
- Both network tests pass on Windows and WSL Linux, under a 20-second per-test
  bound. They check DHCP/checksums, ARP, loopback UDP echo, peer Ethernet frames,
  synthetic DNS alias answers and NIC descriptor-ring behavior. Evidence:
  ownership-network-audit-windows-{build,tests}.log and
  ownership-network-audit-linux.log. This is bounded transport/descriptor proof,
  not multiplayer-service acceptance or complete NIC/lifecycle correctness.
- Corrected source comments claiming MAC-derived addresses can never collide:
  the mapping has 200 choices and does not guarantee uniqueness across peers.
  Runtime behavior did not change; no new protocol/configuration was added.
- Refreshed the known 153-source hash snapshot at the current fork head and
  recorded these scoped retain decisions. Discovery completeness is still not
  established for the whole tracked fork. NIC/platform internals beyond the
  inspected boundary, other source/tool artifacts and final proof remain open.

### Retained area: NIC device and socket platform layer

- Reviewed the NIC register/PHY model, descriptor rings, guest DMA resolver,
  transmit/receive, interrupt dispatch/thread and initialization/reporting;
  also the full socket/DNS platform header. Retain generic hardware/transport
  behavior. No title address or section name selects the production paths.
- Moved the captured physical-ring/BINKDATA observation into MM3 reference data.
  Allocation-based contiguous-memory precedence stays in the NIC; the old
  assertion about every driver never using statics is no longer claimed.
- Fixed a real generic integration gap: runtime physical IDs for ordinary RAM
  now resolve before legacy contiguous/guest-VA handling. Both descriptor and
  payload accesses share this path. Existing bounds and allocation precedence
  remain in place; no title identity or new configuration was introduced.
- Windows and WSL Linux network suites pass 2/2, including synthetic RAM-ID
  RX payload, descriptor writeback, ring advancement, TX payload and invalid
  range checks. Both MM3 renderer builds pass. Evidence:
  ownership-nic-physical-windows-{build,tests}.log,
  ownership-nic-physical-linux.log and
  ownership-nic-physical-{sdlgpu,d3d11}-build.log.
- This is scoped ownership/DMA proof, not complete NIC hardware fidelity,
  interrupt/lifecycle correctness or online gameplay acceptance. Whole-fork
  inventory, remaining source/tool artifacts and cumulative final proof stay open.

### USB DMA boundary: shared resolution replaces the private-arena fallback

- Reviewed OHCI guest-pointer validation and all three DMA access callers
  (descriptor reads, descriptor writes and payload pointers), plus the public
  header. Retain the generic runtime RAM-ID and loaded-image precedence rules.
  This is a boundary review, not a full OHCI implementation ownership decision.
- Removed the private first-page DMA fallback. The shared physical resolver
  already maps every backed contiguous page, including unallocated page zero.
  The fallback additionally returned a contiguous VA after memory shutdown,
  when the backing no longer existed. The new regression fails on that old
  branch and passes with it removed; backed first-page and page-edge mappings
  still pass through the real memory layout.
- Moved the DDS9/Burnout 3 DMA addresses and Half-Life 2 bring-up observation
  into MM3 reference data. Updated the stale header that claimed transfer
  descriptor walking was absent; this does not claim complete OHCI fidelity.
- Windows kernel_io_mapping passes, and both MM3 renderer builds pass.
  Evidence: ownership-usb-dma-{baseline-tests,tests,sdlgpu-build,d3d11-build}.log.
  WSL Linux kernel_io_mapping also passes (ownership-usb-dma-linux.log).
  Its first configure required a Git discovery ceiling to avoid interpreting
  the Windows parent worktree metadata while applying the SDL source patch.
- Remaining OHCI transfer/scheduler/IRQ internals, other runtime/tool artifacts,
  whole-fork inventory and cumulative final proof remain open.

### USB protocol mechanisms and captured debugging history

- Reviewed root-hub reset/change ordering, control response chunking, report and
  OUT endpoints, short-IN outcomes, descriptor walking, done publication and
  the service/IRQ paths. Retain these generic mechanisms; no title name selects
  them. Scripted stick directions likewise remain independent of d-pad input.
- Moved the DDS9 reset/enumeration history, Burnout 3 rumble/completion and
  stick-input observations, and X-Men Legends short-descriptor history into
  MM3 reference data. Runtime comments now describe the generic mechanism.
- Removed stale comments claiming descriptor walking/repeated interrupts were
  absent, corrected the four-ms frame-count explanation and stopped describing
  the two-second model holdoff as an 80-ms hardware rule. No scheduler, timing,
  IRQ, transfer or input behavior changed.
- Compared both edited files against fork 9c71042 with strings preserved and
  comments/whitespace excluded: their remaining source is identical. Evidence:
  ownership-usb-history-check.log. No new runtime acceptance is claimed for a
  comment-only migration; preceding Windows/Linux mapping and Windows build
  results remain scoped to their actual checks.
- Four-ms periodic sweeps, bounded walkers, delayed device arrival and forced
  delivery after IRQL holdoff are model approximations, not proven hardware
  fidelity. This ownership decision does not certify complete USB/lifecycle
  correctness. Full gamepad source review, remaining source/tool artifacts,
  whole-fork inventory and cumulative final proof remain open.

### Canonical whole-fork inventory coverage

- Reconciled FFF prefix discovery with the committed fork tree at 3ccf1b2
  (tree 57618620f5d206ebe50d4325685f9ef09949d0c7). The manifest now covers
  all 624 tracked files, rather than only the 153 src files. Existing scoped
  reviews and the DSP compiler tool review are preserved.
- Added committed file modes/blob IDs alongside working-tree SHA256 values.
  These are distinct evidence: Git records committed content and the hashes
  record this checkout's bytes, including its line-ending conversions.
- Coverage: src 153, tests 194, tools 217, docs 33, templates 7, cmake 7,
  include 2, LICENSES 2 and nine top-level files. No tracked path is missing
  from the manifest. FFF has 21 stale paths and omits the three recently added
  DSP-export/starter test paths; those discrepancies are recorded explicitly.
- Verified exact path-set equality, uniqueness, mode/blob identity, every
  working-tree hash, existing tool-review hashes, tree/head identity and counts.
  Evidence: ownership-canonical-inventory-check.log. This is metadata coverage;
  it does not certify the unreviewed files or new gameplay/runtime behavior.
- Remaining ownership decisions across that full tree, cumulative validation
  and final branch/submodule/worktree/publication proof remain open. Earlier
  ledger statements about discovery incompleteness describe their snapshots;
  the canonical coverage check above supersedes that inventory limitation.

### Generic target evaluation replaces title rankings and project-status claims

- Moved the original candidate-game guide and README target/project sections
  into tools/reference/legacy_candidate_games.md, with source commit and an
  explicit historical/unverified status. This preserves the original text,
  including contradictory reference-title playability claims, without treating
  it as current toolkit compatibility evidence.
- The fork guide now asks for actual XBE sections/imports/library stamps,
  recovered functions, reached runtime requirements and separate acceptance
  evidence. Removed named-game difficulty tiers, fixed bytes-per-function
  estimates and unsupported assumptions that engine or release date predicts
  port difficulty. The existing guide path remains stable.
- README introduction/target sections describe the generic mechanism/port
  policy boundary. Removed its named-project status roster and updated the
  index label. Other README/index content is not certified by this scoped edit.
- Verified original guide and README excerpts are preserved in the archive,
  introduced relative links exist, and the documented parser/JSON command
  succeeds on the synthetic conformance XBE. Evidence:
  ownership-candidate-{cli,doc-check}.log and ownership-candidate-analysis.json.
  No runtime code changed; this is not new game compatibility or gameplay proof.
- Refreshed the complete 624-file tree/blob/hash inventory and recorded the
  scoped decisions. Other documentation, remaining source/tool/test ownership,
  cumulative validation and final repository/publication proof remain open.

### Retained generic diagnostics and tool-documentation boundary

- Reviewed full frame CSV summarizer, guest-stack word scanner, MSVC map
  symbolizer and PowerShell PVIDEO register checker. Retain them: each accepts
  caller-supplied data/source or tests runtime register semantics, with no
  built-in game path, menu sequence, title identity or asset fixture.
- Verified frame-summary selftest and inclusive CSV time window; guest-stack
  in-body hit plus entry/end/gap rejection; map CLI/stdin lookup at a non-default
  host image base; and MSVC compilation/execution of actual PVIDEO register
  function bodies for stop/read-zero/restart. Evidence:
  ownership-diagnostic-tools-check.log and ownership-pvideo-tools-check.log.
- These are bounded diagnostic contracts. Stack-word scans are candidate
  return-site hints, map lookups are nearest public symbols, frame CSVs are
  internal diagnostics and the PVIDEO fixture does not prove presentation.
- Moved the old reference-title regeneration notes/loader filename example into
  tools/reference/legacy_tool_regeneration_notes.md. The generic documentation
  now describes source-owned overrides, lifter fixes and regeneration proof.
  Loader executable AST is unchanged excluding its edited function docstring;
  original notes are preserved and the new guide link resolves. Evidence:
  ownership-diagnostic-docs-check.log. No runtime behavior changed.
- Refreshed all 624 committed tree/blob/hash entries, retaining prior scoped
  reviews. Broader loader/tool documentation, other artifact/runtime ownership,
  cumulative validation and final repository/publication proof remain open.

### Reference-title header namespaces and DirectSound capability descriptions

- Replaced six active BURNOUT3 include guards in DirectSound, D3D8 and NV2A
  headers with XBOXRECOMP names. Search results had also named input/video
  headers, but current source already used generic guards; those were left
  unchanged. No consumer of the old guard names was found outside the guards.
- Preserved old guard names, the 40-entry/68-call reference-title audio metrics
  and the RenderWare adapter origin description in MM3 reference data. Those
  historical metrics are not generic API implementation coverage.
- Inspected DirectSound buffer delegation to the shared APU mixer. Replaced
  stale comments claiming every method is a silent no-op and output is wholly
  deferred. Stream/listener/spatial methods remain partial compatibility stubs;
  the startup log now describes the interface without claiming no audio output.
- Verified each header body is identical after guard normalization and comment
  exclusion; the audio body differs only in its startup message. Six standalone
  MSVC double-inclusion checks pass with the new guards defined. Both Windows
  renderer builds pass; WSL Linux runtime build and kernel_io_mapping pass.
  Evidence: ownership-header-namespace-check.log,
  ownership-header-{sdlgpu,d3d11}-build.log and ownership-header-linux.log.
- This preserves declarations/layouts and buildability, not proof that every
  exposed host adapter matches every guest SDK ABI. No new playback or full
  graphics/USB/audio fidelity is claimed. Complete 624-file inventory refreshed;
  remaining ownership decisions and cumulative/final repository proof stay open.

### Retained DirectSound PCM adapter: prove and repair both storage paths

- Reviewed the generic DirectSound host-buffer adapter and its existing
  actual-mixer regression. Retain the implemented PCM creation, playback,
  state/cursor, volume/frequency and cleanup mechanisms; they contain no fixed
  guest address, title selector, authored audio or game-specific input.
- Found a real shared-adapter gap: CreateSoundBuffer allocated storage for the
  public Lock/Unlock path after mixer voice initialization, leaving that voice
  unattached. SetBufferData worked, so existing tests missed the silent path.
  Allocate storage before binding and supply its pointer/byte count to the
  mixer for 16-bit buffers. No title workaround or new configuration was added.
- Added mono/stereo sample, cursor/status and release checks through actual
  CreateSoundBuffer/Lock/Unlock/Play calls. This fails on the old code and
  passes with the fix. Existing replacement/clearing, loop, fractional cursor,
  stop/resume and long-buffer checks pass too.
- The complete extracted-code case passes with WSL GCC and MSVC. The MSVC
  fixture run exposed omitted stdlib declarations; added the explicit includes
  required by the extracted mixer and test before rerunning successfully.
  Both Windows game renderer builds pass. Evidence:
  ownership-audio-buffer-{baseline,linux,windows}.log and
  ownership-audio-buffer-{sdlgpu,d3d11}-build.log.
- Replaced the audio README's unsupported streaming/circular-lock/spatial claims
  with current implemented paths and limitations. Test wakeups/locking remain
  fixture stubs: no audio-device, concurrency, complete SDK ABI or game-audio
  fidelity claim follows from synthetic PCM samples. No new audio hardware or
  deferred API feature was implemented. The full 624-file inventory is refreshed;
  other ownership areas and cumulative/final proof remain open.

### Retained WMA host decoder and synthetic fixture

- Reviewed the complete caller-path decoder/API, buffer growth, negotiated PCM
  metadata/format-change handling, COM/Media Foundation cleanup and unsupported
  host branch, plus audio link wiring and the standalone test/fixture records.
  Retain these reusable mechanisms: no title asset name/path, guest address or
  title activation selects the decoder.
- The fixture is documented as generated CC0 tone data with its FFmpeg command.
  Independent inspection confirms WMA v2, 44.1-kHz stereo and 0.25-s container
  duration; decoded samples have 66 positive crossings in 0.15 seconds (440 Hz).
  This supports the documented synthetic content, without claiming historical
  authorship forensics. Evidence: ownership-wma-fixture-analysis.json.
- Windows standalone regression passes real MF decoding/PCM metadata and an
  injected read-error flag after partial output, requiring cleanup. Linux passes
  only failure/cleared-output/idempotent-free checks; it does not decode WMA.
  Evidence: ownership-wma-windows-{configure,build,tests}.log and
  ownership-wma-linux.log. No audio device or game content was used.
- Clarified the header's platform and result-ownership rules: free prior output
  before reuse; failure clears a non-NULL result. No decoder implementation or
  runtime behavior changed. This is a Windows host-codec helper, not proof of
  Xbox compressed-packet emulation or game-audio fidelity.
- Complete 624-file tree/blob/hash inventory refreshed with these scoped retain
  decisions. Remaining ownership review, cumulative validation and final
  repository/publication proof stay open.

### Generic Fusion address-map scan: remove captured image bounds

- Inspected the shared module parser/map helpers and their caller sites. The
  map scan had a fixed 0x1000..0x400000 guest range, excluding higher-address
  images. Pair lookup and entry-point enumeration duplicated that assumption.
  Moved the captured Crimson symbol-table calibration metrics and old range
  into MM3 reference data; the exported version-1 structure description stays.
- Synthetic maps reproduced the ceiling and a second defect: a guest-first
  pair table beginning one DWORD into .rdata was matched to preceding host
  words. Shared scanning now keeps each nonzero 32-bit guest value with its
  following host word and drops an incomplete trailing record. Entry-point
  enumeration delegates to map_pairs, preserving its max_delta argument.
- The initial six-case regression failed five cases on the old implementation.
  Expanded cases cover low/6-MiB/high-32-bit guest bases, both DWORD phases,
  complete/incomplete tails, exact lookup/miss and max_delta propagation.
  All 41 Fusion cases pass, including the 12 map cases. Evidence:
  ownership-fusion-map-{baseline,tests}.log. Dependencies were installed in an
  isolated build/ownership-fusion-venv, not the user's existing environment.
- This remains a large-monotonic-run analysis heuristic with its existing
  confidence threshold. It does not recover exact table bounds, certify a
  private compiler format or prove foreign-code equivalence. No captured module
  regression or gameplay claim is made; other Fusion corpus/tool areas and
  cumulative/final proof remain open.
- New synthetic regression is tracked: the complete fork inventory now covers
  625 files (153 src, 218 tools); tree/blob/hash coverage was refreshed while
  preserving prior scoped decisions.

### SVOD extraction boundary and captured corpus vectors

- Inspected block mapping, directory parsing, sector-base derivation and hints,
  caller-selected package/base/output interfaces and XBE diagnostics. Retain
  these generic mechanisms: candidate bases are checked against actual root
  and XBE data, not selected by title identity. Format stride/default-sector
  constants remain shared format rules; no captured game package is shipped.
- Moved the original corpus layout/base measurements, derivation/header-hint
  and corrupt-XBE observations, and two original observed-value test snippets
  into tools/reference/legacy_svod_observations.md with source commit/context.
  Replaced the active tests' observed directory/TLS numbers with synthetic
  values while preserving the parser/decoy/corruption cases.
- Production AST is unchanged excluding docstrings. Test AST is unchanged
  except seven explicit integer substitutions; the original observation text
  remains in the archive. All 41 Fusion tests pass. Evidence:
  ownership-svod-source-check.log and ownership-svod-generic-tests.log.
- Generic docs now state that sampled hashes do not authenticate the whole
  hash tree and XBE confidence thresholds are not universal validity rules.
  No captured-package extraction, complete format support, resource lifecycle,
  full XBE validation or gameplay acceptance is claimed by these tests.
- Complete 625-file committed tree/blob/hash inventory refreshed. Other Fusion
  corpus/documentation, remaining source/tool/test ownership and cumulative/
  final repository proof remain open.

### Generic Fusion signatures and comparison aids

- Reviewed signature construction/matching: image bounds come from supplied XBE
  sections; the donor module supplies symbol metadata, not a built-in title
  layout or bundled database. Retain the reusable reader/mask/matcher mechanisms
  and keep generated signature data owned by its caller/port.
- Found and fixed an instruction-offset bug in the shared masker used by both
  build and match. Capstone encoding offsets are instruction-relative; applying
  them at pattern offset zero masked earlier opcodes and left later address
  bytes fixed. Added the instruction's position for immediate/displacement
  fields while preserving the selected-operand masking rules.
- Regression with a prologue followed by rel32 call, absolute load and image
  immediate checks the exact preserved opcode and wildcard operand bytes. It
  fails on the old implementation (six existing cases passed); all 42 Fusion
  cases pass after the fix. Evidence: ownership-signatures-{baseline,tests}.log.
  Regenerate saved signature databases after masker changes; none are bundled.
- Reviewed caller-supplied reference listing/function-start comparison aids.
  Moved captured donor-build/global examples and earlier overbroad oracle claims
  into tools/reference/legacy_fusion_comparison_notes.md. The remaining observed
  global instruction fixture now uses a synthetic address with the same operand
  semantics. Comparator executable ASTs are unchanged; signature code changes
  are confined to masked_pattern. Evidence:
  ownership-fusion-comparison-source-check.log.
- Docs describe candidate naming/manual analysis rather than semantic equivalence
  or exhaustive function coverage. No real donor database, cross-title naming
  precision, private compiler convention coverage or gameplay proof is claimed.
  Complete 625-file inventory refreshed. Remaining corpus/docs/runtime/artifact
  ownership, cumulative validation and final repository proof remain open.

### Captured Fusion corpus/adoption documentation moves out of the toolkit

- Moved five complete original documents (single-title recompiler analysis,
  codegen teardown, four-title corpus, Crimson adoption plan and measured SVOD
  extraction notes) into tools/reference/legacy_fusion_docs. Its README records
  source commit/blob identities and original runtime feedback provenance.
  Historical claims/status/relative references are not asserted current.
- The fork now has one generic fusion-analysis guide derived from current
  interfaces: inputs/dependencies, version-1 metadata reader, heuristic address
  maps, package reads, comparison aids, signature candidates and runtime feedback.
  It contains no captured title tables or measured guest addresses.
- Updated README/index navigation, indirect-call workflow references and the
  known source callers. Checked new/updated links and removed-page references;
  all five archived texts match their source versions. Python executable ASTs
  and C non-comment source are unchanged. Evidence:
  ownership-fusion-doc-migration-check.log. All 42 Fusion tests still pass
  (ownership-fusion-doc-tests.log); no runtime behavior or gameplay was changed.
- The original source texts remain in MM3 rather than being replaced by a
  claim that synthetic tests establish private-format/corpus fidelity. Other
  documentation and runtime/tool/test ownership decisions, cumulative validation
  and final repository/publication proof remain open.
- Five fork pages were removed and one generic guide added: complete tree/blob/
  hash inventory now covers 621 files, including all 153 src paths. Historical
  624/625-file snapshot records above retain their original scope and counts.

### Retained generic build helpers: independent SDL and shader-compiler setup

- Reviewed all seven CMake/toolchain/patch artifacts: compiler override/pinned
  dependency setup, generic renderer shader units/variants, HLSL embedding,
  explicit MinGW roots and SDL's descriptor heap reservation patch. Retain the
  reusable build mechanisms. No title paths, captured shaders or guest-address
  selectors are supplied by these helpers. DXC overrides are already honored.
- Found a real generic integration gap in xbr_nv2a_sdlgpu_target: supplying SDL
  from a parent project skipped DXC initialization as well. Removed the shared
  conditional and rely on the two dependencies' own existing guards, so either
  can be provided independently. No new configuration or abstraction was added.
- A standalone parent-provided SDL target/no-DXC fixture fails configure on the
  old helper and builds with the fix. It runs real DXC: Windows produced 12
  SPIR-V + 12 DXIL headers; Linux produced 12 SPIR-V headers. Used existing
  compiler packages through FetchContent source overrides, without downloading
  or modifying SDL for the fixture. The dummy SDL target proves the build graph,
  not GPU execution. Evidence: ownership-sdl-helper-{baseline,configure,build}.log,
  ownership-sdl-helper-linux.log and ownership-sdl-helper-headers-check.log.
- Normal SDL game build and all 19 binding cases pass. Evidence:
  ownership-sdl-helper-runtime-build.log and
  ownership-sdl-helper-bindings-{build,tests}.log. This is not isolated proof of
  the vendor descriptor patch, every embed encoding, MinGW cross-builds or new
  rendering/gameplay fidelity.
- The new build-only regression is one CMake project with runnable commands.
  Complete inventory now covers 622 tracked files, including all 153 src paths.
  Remaining ownership decisions, cumulative validation and final repository/
  publication proof remain open.

### Retained observed-state shader cache and warmup mechanisms

- Reviewed the full portable shader-key/hash and Windows bytecode cache headers,
  bounded PS manifest format, and standalone cache/warmup fixtures. Retain these
  generic mechanisms: no captured shader assets or title-specific preload list
  is embedded. Manifest families derive from shared shader source, specialization
  layout, variants, compiler flags and schema. The D3D11 integration review covers
  this family/import/worker boundary only, not the whole renderer.
- Windows bytecode-cache checks pass 2/2; Windows warmup checks pass 11/11,
  including malformed metadata, worker scheduling/persistence, bypass/fallback,
  synthetic GPU comparisons and dynamic constants. Linux portable key/hash checks
  pass 1/1. Evidence: ownership-shader-cache-tests.log,
  ownership-shader-cache-linux.log and ownership-ps-prewarm-tests.log. These do
  not establish Linux file-cache/warmup execution, all-device compatibility or
  original-game rendering fidelity.
- Moved the old game-checkout/environment-script build paragraph into
  tools/reference/legacy_ps_prewarm_integration.md and verified its original text.
  The fork now documents commands from its own root with native CMake/CTest and
  the Windows SDK/MSVC environment. No production code changed in this checkpoint.
- Refreshed the complete 622-file committed-tree/blob/hash inventory with ten
  scoped decisions. Remaining fork ownership decisions, cumulative validation
  and final repository/publication proof remain open.


### Captured D3D8LTCG device map moves to MM3

- Preserved the full original device-context page, including captured Burnout 3
  addresses, offsets, per-entry stack cleanup and snapshot/forced-state bypasses,
  in tools/reference/legacy_d3d8ltcg_device_context.md. The archive records source
  commit/blob provenance; its original text was checked exactly.
- Replaced the fork page at its existing path with a generic investigation guide:
  recover image-specific state ownership, queue/fence contracts and guest calling
  conventions; place instruction/call/runtime defects in shared mechanisms and
  discovered application layouts/policy in the port. Counting pushes or escaping
  a spin loop does not establish a valid guest contract or completed rendering.
- Updated both navigation descriptions. Replacement local links resolve and the
  fork diff contains only three documentation files. No executable behavior or
  runtime verification is claimed for this documentation-only checkpoint.
- Refreshed all 622 committed file identities/hashes; preserved prior scoped
  navigation reviews. Other documentation/runtime/artifact ownership, cumulative
  validation and final branch/submodule/worktree/publication proof remain open.


### Runtime setup follows shared APIs; captured bring-up history moves out

- Archived the full original pipeline runtime guide in
  tools/reference/legacy_runtime_setup.md with source commit/blob provenance.
  Its captured title addresses/import/override counts, executable path and old
  memory/MMIO/heap claims are historical, not SDK guarantees. The original text
  matches after line-ending normalization.
- Replaced it with current starter/API guidance: image-derived memory/thunks,
  explicit thread policy, shared host initialization and checked failure paths,
  teardown lifetimes, mapping versus installed RAM, renderer selection, TLS guest
  registers, direct/indirect hook reachability and both halves of ABI checking.
  The Windows starter is explicitly distinguished from a cross-platform port.
- Captured dispatch-performance and expanded-mapping observations in the starter
  and memory header moved to the same MM3 archive. Their replacement comments
  explain the generic contracts. Both source files are identical with block
  comments removed. This does not certify the rest of the memory header: its
  legacy fence description and remaining runtime ownership still need review.
- Replacement links resolve; Windows starter rebuild and all four startup/
  failure-path cases pass (ownership-runtime-guide-{build,tests}.log). No new
  runtime behavior or original-game fidelity is claimed by these comment/docs
  edits. Complete 622-file committed-tree/blob/hash inventory refreshed.
- Broader ownership decisions, cumulative validation and final repository/
  publication proof remain open.


### Remove synthetic GPU completion policy from the shared runtime

- Removed the unused submitted-to-completed field mirror API, wall-clock frame
  counter fallback and snapshot-derived fence publication. The original source
  blocks/header commentary, including captured dashboard/Wreckless/Half-Life 2
  observations, are preserved as normalized text in
  tools/reference/legacy_gpu_counter_policy.md. Ports needing synthetic pacing
  own that compatibility policy; the SDK guide documents this breaking change.
- Retained actual executed non-replay flip counters and command-driven semaphore
  completion. Registering a fence alone cannot publish completion; the port must
  configure its command selector. MM3 already registers command-driven fences
  and has no counter-mirror or frame-clock registrations. Version-1 capture
  layout is retained with the obsolete counter_mirrors field zero.
- Extended the real command-fence regression with flip-counter uint32 wrap,
  missing device chains and inert unconfigured fence registration. Windows
  kernel suite passes 61/61; both Windows renderer game builds pass. Evidence:
  ownership-counter-policy-{build,tests,sdl-build,d3d11-build}.log. Linux focused
  command-fence validation passes 1/1 (ownership-counter-policy-linux.log); the
  full Linux kernel suite was not run.
- Refreshed the complete 622-file committed-tree/blob/hash inventory. This is
  scoped completion-policy ownership evidence, not a full memory-layout audit
  or gameplay/device fidelity proof. CaptureSnapshot still decodes device fields
  at 0x17C0 and 0x4F8; that image-layout boundary needs a separate caller-configured
  migration. Other ownership, cumulative validation and final repository/
  publication proof remain open.


### Capture layouts and device decoding belong to the port

- Replaced implicit device/channel/ring offsets with optional caller-supplied
  XBOX_NV2A_CAPTURE_LAYOUT registration on a fence root. No layout means no
  decoded channel/PFIFO/ring fields; raw device and hardware state remain
  available. Capture reads retain their bounds/overflow checks. MM3 supplies
  its linked-library offsets during startup; they no longer select shared
  capture behavior by default.
- The scanner now uses decoded ring bounds rather than interpreting fixed raw
  device offsets. Full-ring capture, including the possible boundary JUMP word,
  is preserved. Version-2 snapshots add three words per mirror (35,320 bytes);
  generic producer JSON includes these bounds. The SDK format notes explicitly
  require version/size checks rather than silently accepting old readers.
- Removed the shared JSON decoder's cursor/flags/fence-table layout assumptions.
  MM3 owns tools/scripts/decode_mm3_pb_capture.py with the original named fields
  and offsets. It accepts historical version-1 and current version-2 binary
  snapshots, validates sizes/counts, and marks unavailable bytes as null.
  A standard-library regression checks both layouts and malformed inputs.
- Preserved the original device-layout-dependent source blocks as normalized
  text in tools/reference/legacy_capture_layout.md. The old implicit-layout
  baseline fails as expected. Windows kernel/capture checks pass 61/61, including
  actual fixture binary/JSON outputs and full-ring/pre-callback word preservation;
  Linux focused command-fence/hitch checks pass 3/3. Both Windows renderer game
  builds pass. Evidence: ownership-capture-layout-{baseline,build,tests,linux,
  sdl-build,d3d11-build}.log; root test_mm3_pb_capture.py also passes.
- Complete 622-file tree/blob/hash inventory refreshed. This is scoped capture
  ownership/schema evidence, not atomic snapshot, full scanner/runtime or
  original-game fidelity proof. Remaining ownership decisions, cumulative
  validation and final repository/publication proof remain open.


### Retained framebuffer probe: fix generic mapped-span and mode handling

- Traced the optional framebuffer checksum diagnostic through its setter in the
  display-mode bridge and existing mode/range APIs. It has no captured image
  address to migrate, but its base-RAM size check rejected the resolved contiguous
  framebuffer window and its constant 480-line span ignored PAL's final rows.
- Reused xbox_GetDisplayMode for height and matching pitch, and fence_readable
  for either mapped RAM window. The byte product is widened and bounded before
  converting it to the range-check/count types. Unknown/mismatched modes are
  skipped rather than guessed. No new configuration or title selector was added.
- A marker in the final PAL row of a synthetic contiguous framebuffer fails on
  the old probe and produces the expected complete-span checksum with the fix.
  Windows kernel checks pass 61/61, Linux focused regression 1/1, and both Windows
  renderer game builds pass. Evidence:
  ownership-framebuffer-probe-{baseline,build,tests,linux,sdl-build,d3d11-build}.log.
- This is useful generic diagnostic behavior, not presentation/gameplay proof.
  The existing display-mode decoder's accepted formats and HD-mode limitations
  are unchanged. Complete 622-file committed identities/hashes refreshed;
  remaining source/docs/artifact ownership, cumulative validation and final
  repository/publication proof remain open.


### Move inherited application context bootstrap out of the generic loader

- Found two unconditional writes that installed a fixed RenderWare scratch
  pointer chain for every loaded image. Removed those writes/constants from
  xbox_MemoryLayoutInit. Image-derived TLS and guest KPCR/TIB initialization
  remain in the shared implementation; the synthetic image now receives no
  application context chain.
- MM3 startup preserves the exact two inherited writes immediately after memory
  initialization and before mod/kernel/guest execution. They are explicitly
  port-owned compatibility policy; their actual MM3 use remains unaudited, so
  this is not claimed as native application TLS correctness. No generic feature
  was needed: the port already owns access to its mapped guest memory.
- Preserved original bootstrap/log/comment/documentation blocks in
  tools/reference/legacy_application_context.md, including captured executable
  bounds and Wreckless TLS observations. The focused thread-state guide now
  separates image/kernel state from application-owned contexts; the rest of
  that historical memory guide is not certified by this edit.
- Old loader fails the new assertion. Windows memory cases pass 7/7 and kernel
  cases 61/61; seven Linux memory modes pass using the actual fixture compiled
  with existing runtime target flags/link libraries. Both Windows renderer game
  builds pass. Evidence: ownership-context-chain-{baseline,memory-build,
  memory-tests,kernel-build,kernel-tests,linux,sdl-build,d3d11-build}.log.
- Complete 622-file committed identities/hashes refreshed. Other kernel stand-ins,
  memory/source/docs/artifact ownership, cumulative validation and final
  repository/publication proof remain open; no new gameplay fidelity claim.


### Kernel probe mapping is generic; fabricated header bytes belong to MM3

- Removed the fabricated e_lfanew field used to steer library feature probing
  from shared memory initialization. Retained the zeroed kernel probe mapping,
  including the separate fallback allocation and owned teardown. Added the
  small xbox_GetKernelMemoryBase accessor so the port can populate compatibility
  bytes without reimplementing mapping/lifetime ownership.
- MM3 startup writes the inherited e_lfanew value through that view when it is
  available, preserving the former unavailable-page behavior as well. The
  compatibility response is explicitly port-owned; this does not claim a real
  kernel PE implementation or native MM3 feature-probe correctness.
- Archived the full original memory guide and kernel initialization block in
  tools/reference/legacy_memory_layout_guide.md. The replacement guide describes
  current image/RAM/VA/physical-ID APIs, guest/application state boundaries,
  owned probe storage, diagnostics and teardown. Original guide text matches
  after line-ending normalization; replacement local links resolve.
- Old default loader fails the new no-fabricated-offset check. Real mapping
  regressions verify the accessor and its retirement on shutdown. Windows
  memory cases pass 7/7, kernel cases 61/61, and seven Linux memory modes pass
  using actual fixture source with existing runtime flags/libraries. Both
  Windows renderer game builds pass. Evidence:
  ownership-kernel-probe-{baseline,memory-build,memory-tests,kernel-build,
  kernel-tests,linux,sdl-build,d3d11-build}.log.
- Complete 622-file committed tree/blob/hash inventory refreshed. Remaining
  stand-in ownership, other source/docs/artifacts, cumulative validation and
  final repository/publication proof remain open; no new gameplay fidelity claim.


### Generic KPCR consistency replaces captured Prcb scratch storage

- Removed the fixed 0x761000 zeroed scratch page and its cache-probe workaround
  from shared initialization. The main KPCR's Prcb pointer now references its
  own inline PrcbData. Each allocated worker rebases its pointer after copying
  the creator's prefix, instead of sharing that creator's processor-state view.
- This matches the existing inline DPC/IRQL state layout and is corroborated by
  the Cxbx Xbox KPCR/KPRCB declarations (linked in the reference archive).
  The old captured bootstrap is preserved as normalized text in
  tools/reference/legacy_prcb_standin.md. No application selection or callback
  was needed: this is a generic pointer/state consistency defect.
- Both worker layout/state cases fail against the old code. New checks verify
  own-worker pointers, main inline storage, pointer/inline DpcRoutineActive
  agreement and cross-worker isolation. Windows memory cases pass 7/7 and kernel
  cases 61/61. Linux worker cases pass 2/2 and seven actual memory fixture modes
  pass using existing runtime flags/libraries. Both Windows renderer game builds
  pass. Evidence: ownership-prcb-{baseline,memory-build,memory-tests,kernel-build,
  kernel-tests,linux,sdl-build,d3d11-build}.log.
- Complete 622-file committed identities/hashes refreshed. Evidence covers
  consistency in the existing per-host-thread guest model, not full processor/
  scheduler/KPRCB emulation or gameplay fidelity. Remaining ownership decisions,
  cumulative validation and final repository/publication proof remain open.


### Replace fixed main TLS buffers with owned image-safe storage

- Found fixed TLS/thread-data buffers 512 bytes apart in shared startup. A larger
  template overlapped the thread data, and a loaded section at those addresses
  was overwritten before its TLS bytes were copied. Archived the full original
  bootstrap in tools/reference/legacy_fixed_tls_storage.md.
- Main TLS and slot-zero thread data now share one owned heap allocation outside
  loaded image/reservation/stack ranges, using the existing allocator. Directory
  and template ranges and size arithmetic are checked before access/allocation;
  invalid metadata fails initialization with mapping cleanup. Worker cloning
  continues to use the resulting template.
- A synthetic data section at the former scratch range with a template larger
  than 512 bytes fails four checks on old code. The replacement preserves source,
  initialized bytes, zero-fill and separate thread data. Three invalid directory/
  bounds/overflow cases verify rejection and cleanup. Windows memory cases pass
  11/11, kernel cases 61/61 and Linux focused TLS cases 4/4. Both Windows renderer
  game builds pass. Evidence: ownership-tls-layout-{baseline,build,tests}.log,
  ownership-tls-kernel-{build,tests}.log, ownership-tls-linux-focused.log and
  ownership-tls-{sdl,d3d11}-build.log.
- Bounded SDL3/D3D12 night Cruise passed through menus/loading with inspected
  captures and 1.5m measured post-teleport driving displacement. Its real 20-byte
  TLS block was allocated at 0x00F80000. Evidence: ownership-tls-cruise.log and
  conformance_tmp/game_test/run-mm8r30qt. This is bounded gameplay proof, not
  exhaustive fidelity or runtime validation of the other renderer drivers.
- The broader Linux memory attempt failed in ext-vma: host reservation was
  unavailable, allocation fell back to a different address, and the fixture then
  dereferenced the unbacked requested address. ownership-tls-linux.log retains
  the failure. Do not report the full Linux suite green; investigate this path
  and its fixture failure handling next. Heap-header captured CRT rationale,
  remaining ownership and cumulative/final repository proof also remain open.
- Complete 622-file committed identities/hashes refreshed; scope remains the
  full ownership goal, with no claim of complete loader/TLS/kernel emulation.


### Resolve upper-VMA reservation failure with generic ownership handling

- Investigated the Linux failure retained in ownership-tls-linux.log. The POSIX
  path attempted the whole upper range once; a later host mapping disabled even
  its free prefix. Executor graphics allocation could also begin before guest
  upper reservations. An inactive tracker then declined an explicit fixed
  request, permitting heap substitution; the fixture dereferenced the requested
  address despite its failed checks.
- Retain the reservable prefix with startup-only 64 KiB non-replacing backoff.
  Explicitly enabled unavailable ranges now reject fixed allocation/free requests
  and report reserved query state. Disabled opt-in still uses the ordinary path.
  Move executor startup after mapping/reservation initialization. The fixture
  preserves its checks and exits on failure before accessing unbacked memory.
- Added a standalone platform-only real native blocker fixture: partial prefixes
  remain usable, blocked requests keep their address and fail, and shutdown
  preserves foreign mappings. It fails on both old Windows/Linux paths and passes
  with the fix. Its independent Linux link also exposed the platform library's
  undeclared math dependency, now supplied transitively by that generic target.
- Windows kernel checks pass 62/62 and memory cases 11/11. Linux native blocker
  passes 1/1, all eleven actual memory fixture modes pass with existing runtime
  flags/libraries, and five additional independent ext-vma startups pass. The
  prior Linux gap is addressed by these controlled and integration checks; its
  original failure log remains intact. Evidence:
  ownership-vma-{prefix-baseline,prefix-linux-baseline,prefix-tests,
  prefix-linux-final,kernel-tests,memory-tests,linux,repeat}.log.
- Both Windows renderer game builds pass. Bounded D3D11 night Cruise passes
  through menus/loading with inspected captures and 1.09m post-teleport driving
  movement. Evidence: ownership-vma-{sdl-build,d3d11-build,cruise}.log and
  conformance_tmp/game_test/run-ht_15qf6. Host fragmentation can still make an
  address unavailable; this proves truthful handling, not universal availability
  or full NT VM/graphics fidelity.
- New generic fixture increases complete tracked inventory to 623 files (153 src).
  Remaining source/header/docs/artifact ownership, cumulative validation and
  final repository/publication proof remain open. No whole-goal completion claim.


### Honor actual image backing; reject malformed section metadata

- The section loader still used a fixed 64 MiB ceiling even for configured
  128 MiB runs. A synthetic image retaining its text and adding initialized
  data above 64 MiB fails on the old loader. Sections now use the actual backed
  span with widened end arithmetic; wrapped/out-of-range virtual extents fail
  before section writes.
- Used header, section-table and raw-copy bounds are checked with wide sums.
  Invalid/truncated inputs fail with mapping cleanup instead of partially
  initialized images. Diagnostic section names are bounded and certificate
  title reads use widened bounds. The table is bounded by the supplied image
  rather than silently truncated at an arbitrary 64-entry count.
- The stricter header check exposed a bridge fixture lacking the XBE signature;
  it now supplies valid metadata and stops before accessing guest memory if
  initialization fails. Its reserve/ABI assertions remain intact.
- Captured title sizing/CRT stories and obsolete heap/thread/bootstrap claims in
  the memory header moved to tools/reference/legacy_memory_header_notes.md,
  together with the old truncated-section handling. Replacement comments match
  the current mapped-span/image/allocation distinction. Header declarations and
  numeric macro tokens are unchanged with comments/whitespace removed; this
  does not certify the remaining numeric main-stack defaults or late C++ linkage.
- Windows memory cases pass 16/16, kernel cases 62/62 and bridge cases 8/8.
  Sixteen actual Linux memory fixture modes pass with existing runtime flags/
  libraries. Both Windows renderer game builds pass. Evidence:
  ownership-image-range-{baseline,build,tests,linux}.log,
  ownership-image-{kernel,bridge}-{build,tests}.log and
  ownership-image-{sdl,d3d11}-build.log.
- Latest SDL3/Vulkan startup reaches the intro marker and captures a frame
  (ownership-image-boot.log, conformance_tmp/game_test/run-zs43m4op). The early
  capture is black: it is startup-only evidence, not scene or gameplay proof.
  Prior bounded Cruise evidence keeps its recorded earlier revision scope.
- Complete 623-file committed identities/hashes refreshed. Full XBE verification,
  remaining memory/header/source/docs/artifact ownership, cumulative validation
  and final repository/publication proof remain open.


### Restore public C++ memory API linkage; archive captured loader notes

- The memory header closed its `extern "C"` block before nine thread/worker
  declarations. A real C++ consumer linked against the production C runtime
  failed with nine unresolved mangled symbols on the old header. Closing the
  block after all declarations fixes their public ABI without changing the
  declarations, constants or C implementation. Fork checkpoint: `b6977ea`.
- The new `tests/memory_regressions/cpp_linkage.cpp` retains references to all
  nine APIs; its CTest registration makes future C-linkage regressions a build
  failure. Only this test project now requires a C++ compiler. It does not
  claim to verify thread scheduling or allocation behavior.
- Eight captured loader/device comment blocks moved unchanged as normalized
  text to `tools/reference/legacy_loader_device_notes.md`. These include image
  section addresses, debug-thunk observations, title-specific physical/device
  failure stories, obsolete audio readiness explanations and old aliasing
  observations. Replacement comments describe the current mechanism and its
  limits. Executable tokens in `xbox_memory_layout.c` are unchanged.
- Windows memory suite passes 17/17. The actual Linux C++ consumer links/runs
  against the existing runtime libraries and all sixteen Linux memory fixture
  modes pass. Both Windows renderer game builds pass. Evidence:
  `conformance_tmp/ownership-memory-cpp-baseline.log`,
  `ownership-memory-api-{build,tests,linux}.log` and
  `ownership-memory-api-{sdl,d3d11}-build.log`. This batch adds no gameplay or
  device-fidelity acceptance claim.
- Complete committed inventory refreshed to 624 files, including 153 source
  and 197 test files. Main-stack fixed placement/size, the remaining fixed
  thunk-table range and scan bounds, broader source/data/docs/artifact ownership,
  cumulative validation and final repository/publication proof remain open.


### Main-stack placement belongs to the image and port, not a captured map

- The inherited main stack began at `0x780000` with an 8 MiB budget. Callback
  slices began at the same base, so those two stack owners overlapped. A
  synthetic image carrying data at that address also covered the main stack.
  The new `stack-layout` case fails three assertions on the old loader.
- Generic initialization now places the main stack after the loaded image and
  kernel data. It uses page-rounded XBE stack commit with a one-page minimum
  for missing/zero metadata. The field offset `0x130` is corroborated by
  [Cxbx's XBE header](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/common/xbe/Xbe.h#L118).
  This is a static-recomp bootstrap allocation, not complete kernel stack
  reserve/commit, guard-page or growth emulation.
- `xbox_ConfigureMainStack(base,size)` supports checked pre-init port policy.
  Invalid/active-layout changes are refused; image/kernel overlap and backing
  limits fail initialization with cleanup. MM3 now selects its inherited
  `0x780000` / 8 MiB explicitly in `src/main.c`. `XBOX_STACK_*` and the heap
  floor are runtime values, not integer constant expressions.
- Callback slices follow the entire main stack. Standalone hosts get no
  implicit unowned pool and must supply their own range. Heap and above-RAM
  reservations exclude both permanent stack ranges. Quiesced shutdown/reinit
  retires prior allocations; generation checks invalidate each host thread's
  positive contiguous/reservation hints when it next uses a new layout.
- Updated bridge boundary fixture retains successful initialization with
  exactly one page of main-stack space and disabled callbacks, and separately
  rejects an image leaving no stack room. Standalone DPC fixtures explicitly
  own their slices; the contiguous-gap fixture declares its synthetic 64 KiB
  alignment. A broader Linux build exposed a Windows-only observer string copy
  in another fixture; a direct host-character filename assertion removes it.
- Ten old fixed-map source/header/document blocks are preserved unchanged as
  normalized text in `tools/reference/legacy_main_stack_policy.md`. Public
  docs now describe configuration, runtime bounds and lifetime contracts.
- Windows memory 22/22, kernel 62/62 and bridge 8/8 pass. Linux kernel 61/61,
  the actual C++ consumer link/run and all 21 actual memory fixture modes pass.
  Both Windows renderer game builds pass. Evidence:
  `conformance_tmp/ownership-stack-baseline.log`, `ownership-stack-{build,tests}.log`,
  `ownership-stack-{kernel,bridge}-{build,tests}.log`, `ownership-stack-linux.log`
  and `ownership-stack-{sdl,d3d11}-build.log`.
- Bounded SDL3/D3D12 night Cruise passes menus/loading and moving gameplay
  (`ownership-stack-cruise.log`, `conformance_tmp/game_test/run-i3f43p_q`).
  Capture 004 was inspected; post-teleport movement is 1.563m. Logs retain the
  MM3 entry ESP `0x00F7FFF0`; the real 20-byte TLS block is now at `0x01380000`,
  beyond the disjoint callback pool. This is bounded gameplay evidence, not
  all renderer/thread/device fidelity.
- Fork checkpoint `589ceb6`; complete committed inventory stays at 624 files
  (153 src, 197 tests). Fixed kernel-data placement, worker sizing/caps,
  per-thread unwind bounds, thunk-table range/scan bounds, broader source/data/
  docs/artifact ownership, cumulative checks and final publication proof remain
  open. The full goal is not complete. Nothing pushed in this batch.


### Patch only explicitly selected kernel imports

- The bridge automatically scanned sixteen words before the configured import
  table, a Halo-specific recovery policy. A synthetic ordinal-shaped ordinary
  word outside the declared range is overwritten by the old bridge; the new
  regression fails there and passes when only selected imports are patched.
  The original workaround, guessed-count setter and old loader scan are retained
  unchanged as normalized text in `tools/reference/legacy_thunk_range_policy.md`.
- MM3 already selects its exact known range in `src/main.c`. Mapping its original
  XBE section bytes shows no ordinal-marker words in the old 64-byte scan region;
  that title recovery is not active MM3 policy. Other ports can explicitly
  select a known bounded span through the existing setter. Native pointer
  initialization now continues across NULL gaps, while the bridge patches only
  the selected entries. No new pattern-based import discovery was added.
- XBE retail/debug decoding now validates a complete terminated marker run using
  actual mapped backing and wide per-read bounds, rather than a fixed 64 MiB
  ceiling and unchecked reads. Nonzero source ordinals need not be canonical;
  the bridge's port-supplied ordinal remap still works. Malformed/unterminated
  tables fail initialization instead of registering a partial run.
- Zero/absent/empty tables leave no configured imports. Zero address/count clears
  explicit selection; invalid alignment, wrap, backing or slot count clears
  selection rather than guessing an old count or clamping. Initialization and
  shutdown retire prior import metadata and slot state. A failed protection
  change returns before import/data patching. Standalone hosts remain responsible
  for mapping explicitly supplied storage; configuration is startup/quiesced.
- The stricter loader exposed a synthetic bridge fixture whose thunk field
  pointed at its section header. It now correctly declares no image imports;
  its later explicit ABI/bridge setup remains unchanged.
- Windows memory suite passes 34/34, kernel 62/62 and bridge 8/8. Linux kernel
  61/61, actual C++ link/run, all 21 prior memory modes and all 12 new thunk
  modes pass. Both Windows renderer game builds pass. Evidence:
  `ownership-thunk-{range,loader}-baseline.log`,
  `ownership-thunk-{build,tests}.log`,
  `ownership-thunk-{kernel,bridge}-{build,tests}.log`,
  `ownership-thunk-{linux,linux-extra}.log`, and
  `ownership-thunk-{sdl,d3d11}-build.log` in `conformance_tmp`.
- Bounded SDL3/Vulkan night Cruise passes menus/loading/movement, with inspected
  capture 004 and 1.985m post-teleport movement (`ownership-thunk-cruise.log`,
  `conformance_tmp/game_test/run-hurwkm2q`). The loader registers all 151 MM3
  imports at the expected address. This is scene/gameplay evidence, beyond the
  earlier black startup-only Vulkan capture, but still bounded acceptance.
- Complete committed inventory is now 625 files (153 src, 198 tests). Broader
  native-pointer/dispatcher remap paths, fixed kernel-data placement, thread
  sizing/unwind/lifetime, other source/data/docs/artifact ownership, cumulative
  validation and final repository/publication proof remain open. The full goal
  is not complete. Validated checkpoints are now published as they are finished,
  fork first and MM3's matching submodule pointer second.

Fork checkpoint for this batch: `364d9d1`.


### Share explicit ordinal mapping across initialization paths

- Source-to-canonical policy now lives beside canonical resolution. Both native
  pointer initialization and bridge initialization apply it exactly once.
  Direct canonical lookup remains unmapped. Borrowed maps require quiesced
  configuration and valid lifetime; NULL, nonpositive count, zero entries and
  out-of-range source IDs select identity. Port policy persists until replaced.
- The old compiled Linux runtime fails the native-pointer regression for source
  511 mapped to implemented canonical 151. Function and data cases now pass for
  both paths; a second mapping catches accidental recursive translation.
- Captured title/XDK narratives from the bridge, API and generator/tests move
  verbatim as normalized text to `tools/reference/legacy_ordinal_remap_notes.md`.
  All four blocks match fork 364d9d1. The generator remains generic explicit
  name-based policy, reports unresolved imports without claiming ABI agreement,
  and reads/writes UTF-8. Matching parser labels do not certify actual imports.
- Windows memory 35/35, kernel 62/62, bridge 8/8; Linux kernel 61/61, actual C++
  consumer, 21 existing memory modes and 13 thunk modes pass. Generator checks
  pass 5/5 on both systems; bridge ordinal audit and both Windows renderer builds
  pass. Logs are `conformance_tmp/ownership-ordinal-*`. No new gameplay/fidelity
  claim is made for this shared identity-preserving initialization change.
- A first fixture exposed missing native export 103 despite bridge support;
  that separate coverage gap remains open. Fixed kernel-data placement, worker
  stack policy, broader export coverage and whole-fork ownership remain open.

- Publication reconciliation: remote fork 371a553 (APU FIFO) and root 87bebad
  (audio guard wait, including race-entry metadata) advanced during validation.
  They are merged without rewriting history. Inventory hashes include the merge;
  this checkpoint does not certify ownership of the incoming changes. Combined
  Windows memory/kernel/bridge suites and Linux kernel/thunk suites pass again.
  The refreshed runtime-entry metadata required regenerating ignored local bodies
  with MM3's pinned generator before rebuilding the combined game.
- Combined SDL3 GPU and D3D11 game builds pass after pinned runtime-entry regeneration.


### Port-owned launch input instead of captured command-line policy

- The bridge no longer reads RECOMP_CMDLINE or invents title ID 0x45410091.
  Both original source blocks are retained unchanged as normalized reference
  text in `tools/reference/legacy_launch_data_policy.md`, checked against fork
  5a36559. The existing host LaunchDataPage export is sufficient generic input;
  no new configuration API was needed. Absent input is NULL. A supplied page is
  copied in full to a 4096-byte-aligned guest heap allocation, without runtime
  interpretation of type, identity, path, flags or payload. The caller retains
  native pointer storage while in use; guest copies retire with mapped memory.
- MM3 owns the optional RECOMP_CMDLINE interface, truncates/NUL-terminates its
  payload and reads the actual loaded XBE certificate title ID with checked
  bounds. No command-line contents are printed. Synthetic checks compile the
  actual port helper and cover absent input, valid identity/payload, truncation,
  below-base, beyond-buffer and truncated certificate cases.
- The previous compiled Linux runtime fails caller-page byte-copy/identity
  checks because it replaces input with its environment-generated launch page.
  New asset-free supplied/absent cases check all 4096 bytes, native export
  pointer identity, guest alignment and independent copy lifetime. Even with
  RECOMP_CMDLINE present, absent generic input remains NULL.
- Windows memory 37/37, kernel 62/62 and bridge 8/8 pass; Linux kernel 61/61 and all 15 actual thunk
  fixture modes pass. Both Windows renderer builds and bridge ordinal audit
  pass. Evidence is `conformance_tmp/ownership-launch-*`.
- Bounded Vulkan night Cruise `run-y4fgm5ra` passes in 123.968 seconds. Capture 004 was
  inspected as actual night driving; telemetry moves from 1450.21997,-308.053009
  to 1448.13,-307.592, approximately 2.14 metres. With the optional input enabled, the
  port reports loaded MM3 title ID 0x4D53002A and 15 payload bytes. This proves
  bounded boot/menu/loading/movement, not complete launch/reboot semantics.
- Fixed kernel-data placement and remaining hardware/identity stand-ins still
  require review, alongside worker-stack policy and whole-tree ownership.


### Own kernel export storage instead of imposing an application address

- The previous runtime fixes every port's export page at 0x00740000. The old
  compiled Linux runtime overwrites a synthetic loaded-image sentinel there.
  Runtime default placement now follows copied XBE headers and all sections;
  MM3 selects its inherited 0x00740000 explicitly before memory initialization.
- `xbox_ConfigureKernelData` reuses the existing memory-layout configuration
  layer. Page-aligned explicit policy is checked before initialization; invalid
  arguments or active changes preserve policy. Full initialization rejects
  copied-header/section overlap and unbacked storage. A valid image gap remains
  usable for an explicit page. Stack/callback/heap ownership follows the image
  and export page; no generic allocation can consume the selected page.
- Runtime address publication retires on shutdown/failed startup, while policy
  persists for a quiesced retry until explicitly changed. New pages are zeroed.
  Standalone bridge hosts must select/map their page; an unselected bridge
  returns before patching. A configured address alone cannot prove backing for
  a host that bypasses memory-layout initialization.
- Older synthetic fixtures declare their fixed export/scratch geometry rather
  than inheriting it as production policy. New cases cover former-address image
  preservation and relocated data pointers, valid overrides, rejected alignment/
  low/wrapping/active changes, section/header/backing rejection, an image with
  no sections, failed-start retry, policy reset and fresh zeroed storage. The
  C++ consumer links the new configuration API.
- The old shared-header literal and stale README address/offset example are
  archived verbatim as normalized text in
  `tools/reference/legacy_kernel_data_placement.md`; both blocks were checked
  against fork 2638596. The template already includes the canonical runtime
  header, so no duplicated generated-layout policy was added.
- Windows memory 43/43, kernel 62/62 and bridge 8/8 pass. Linux kernel 61/61,
  actual C++ consumer, all 21 existing memory modes and all 21 thunk modes pass.
  Both Windows renderer builds and bridge ordinal audit pass. Evidence:
  `conformance_tmp/ownership-kdata-*` (baseline records old sentinel corruption).
- D3D12 night Cruise `run-6ad4jq_e` passes in 123.593 seconds, with inspected
  capture 004 and movement from 1450.21997,-308.053009 to 1448.75,-307.592
  (1.51153 metres). The final early-failure address-clearing check was added
  after this run; all final suites and both renderer builds pass afterward.
- Kernel-data payloads and hardware/identity stand-ins still need review.
  Broader native export coverage, worker-stack policy and whole-tree ownership
  remain open; this checkpoint establishes storage ownership, not full fidelity.


### Share port-owned kernel identity across export paths

- The existing hardware/version setter and credential exports reached native
  lookup, but the bridge replaced them with different hardware constants,
  fixed version 1.0.5849.0, environment-derived keys or unconditional zero bytes.
  The old compiled Linux runtime fails all seven configured guest-copy checks.
  Bridge initialization now copies existing caller-owned exports; no extra
  configuration API is required. Unconfigured identity is zero and explicitly
  does not certify a console model or an implemented kernel version.
- MM3 owns its inherited guest compatibility values (flags 0, GPU 0xA1, MCP
  0xB1 and reported version 1.0.5849.0). It retains the three credential
  environment interfaces, validates exactly 32 hexadecimal digits, and keeps
  absent/invalid values zero. Invalid-input diagnostics print only the variable
  name, never credential contents. Actual helper checks cover absence, mixed
  case, independent keys, empty/short/long/nonhex/whitespace input.
- All 8 hardware/version bytes, three 16-byte keys, 256 alternate-key bytes and
  284 public-key bytes are compared with native exports by synthetic cases.
  Environment variables alone cannot override generic runtime input. Native
  symbol addresses and the hardware data-import guest pointer are checked.
- Four original normalized source/document blocks are archived at
  `tools/reference/legacy_kernel_identity_policy.md`, checked against fork
  5de8b11. The generic data-export guide no longer carries a captured import
  slot, fixed page or SDK-derived identity example; its event ordinal is 16.
- Windows memory 45/45, kernel 62/62 and bridge 8/8 pass. Linux kernel 61/61
  and all 23 actual thunk modes pass. Both Windows renderer builds, bridge
  ordinal audit and actual MM3 helper checks pass. Evidence:
  `conformance_tmp/ownership-identity-*`.
- Vulkan night Cruise `run-sw1kzk8d` passes in 123.672 seconds, with
  inspected captures 003/004 (004 partly occluded by an NPC) and 1.92740 metres of movement. This is bounded
  menu/loading/driving evidence, not identity, crypto or network fidelity.
- Remaining disk/image-name policy, object-type stand-ins, native export
  coverage, worker-stack policy and whole-tree ownership still require review.


### Marshal port-owned disk and image strings

- Native disk/image STRING exports were configurable, but the bridge replaced
  them with fixed disk labels, serial, image path and partition count. The old
  compiled Linux runtime fails the configured counts/copy/storage/topology
  assertions. The bridge now marshals native export inputs into 8-byte guest
  descriptors (two 16-bit counts and one 32-bit VA), rather than copying the
  host-pointer layout or assuming C strings.
- Each valid string receives owned guest heap capacity MaximumLength; exactly
  Length bytes are copied, unused capacity is zeroed, and counts are preserved.
  No terminator is assumed or appended outside capacity. Invalid length/null
  nonempty input and failed allocation leave an empty descriptor. Host buffers
  remain caller-owned and must back Length bytes; boot copies remain until
  memory-layout shutdown, not synchronized live with host writes.
- Generic strings/topology default to unconfigured zero. MM3 supplies its
  inherited virtual disk model/serial, disc image path and three cache
  partitions through existing exports. Its native capacities include the NUL
  byte to retain inherited guest text behavior. No new configuration API was
  needed. Actual port-helper checks include path text/counts and disk topology,
  alongside existing identity/key parser checks.
- Five original normalized blocks are archived in
  `tools/reference/legacy_kernel_string_policy.md`, verified against fork
  0f55435. Obsolete inline buffer constants/capacity assumptions leave the shared
  header; guest descriptor offsets remain stable. No title path or disk label
  remains as an active generic default.
- Five synthetic modes cover native/guest pointer/count/capacity consistency,
  embedded NUL, exact nonterminated bytes, long paths and all 65535 counted
  bytes, unconfigured and valid empty strings, invalid counts/null data and
  exhausted guest heap. Windows memory 50/50, kernel 62/62 and bridge 8/8 pass;
  Linux kernel 61/61 and all 28 thunk modes pass. Both renderer builds, bridge
  ordinal audit and actual MM3 helper checks pass. Logs:
  `conformance_tmp/ownership-strings-*`.
- D3D12 night Cruise `run-x67u2_e5` passes in 123.578 seconds, with
  inspected gameplay captures and 1.06960 metres of movement. This is
  bounded boot/menu/loading/driving evidence, not full disk/launch fidelity.
- Remaining object-type/channel stand-ins, native export coverage, worker-stack
  policy and whole-tree ownership still require review.


### Correct the generic thread-entry ABI before changing worker policy

- The caller audit exposed ordinal 254 decoding seven arguments, taking its
  routine from argument 6 and using 28-byte stdcall cleanup. The primary nxdk
  kernel header declares five arguments: output handle, optional thread ID,
  start routine, start context and debugger-thread flag. The bridge now reads
  routine/context from arguments 2/3 and uses 20-byte cleanup. Evidence:
  https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h#L2322
- The shared entry helper distinguishes the simple API's one context from
  ordinal 255's two-argument system-routine call. Spawn metadata preserves this
  distinction. On a normal translated return, the helper restores the saved
  caller ESP rather than adding another 12 bytes after callee stdcall cleanup.
  First-call inline entry reuses this helper instead of duplicating frame setup.
- The old runtime fails corrected fixtures for 254 default/inline and 255 inline.
  Fixtures place poison beyond the five simple-API arguments, perform actual
  stdcall-style return cleanup, and check exact parent ESP, delivered contexts
  and independent spawned registers/stack/TIB. The prior fixture mirrored the
  incorrect seven-argument assumption and never adjusted ESP on guest return.
- Four normalized original source blocks (entry helpers, Ex/simple bridges and
  old cleanup metadata) move to `tools/reference/legacy_thread_create_abi.md`,
  verified against fork 0f75ac0. The active bridge loses the captured worker
  recovery narrative in favor of the generic register/stack contract.
- Windows memory 50/50, kernel 62/62 and bridge 8/8 pass. Linux kernel 61/61 and all four actual
  bridge thread modes pass, using the existing runtime flags/libraries. Both
  renderer builds and ordinal route audit pass. Logs are
  `conformance_tmp/ownership-thread-abi-*`; the route audit checks ordinal names,
  while the runtime fixtures establish these argument/cleanup contracts.
- Vulkan night Cruise `run-lbjegf8e` passes in 123.812 seconds, with
  inspected gameplay captures and 1.16132 metres of movement. This is
  bounded boot/menu/loading/driving evidence, not complete concurrency fidelity.
- Further review remains necessary: fixed 512 KiB/eight-worker stack policy,
  allocation-failure inline fallback, native thread ABI/export coverage, optional
  thread-ID/suspended/debugger behavior and optional Ex system-routine handling.
  RtlUnwind still checks main-stack bounds for worker frames. Object references
  ignore requested types and currently manufacture a shared exited-thread object;
  the native IDE channel is a scalar placeholder while the bridge supplies a
  partial empty queue. These are incomplete generic kernel mechanisms, not
  verified object/thread/driver semantics. Whole-fork ownership remains open.


### Own request-sized worker stacks and expose resource failures

- The old allocator fixes every worker at 512 KiB, stops at 8 and frees by
  subtracting that size from any supplied top. The previous compiled Linux
  runtime fails allocations 9-12 and frees a normal heap block from a fabricated
  top. New owned records associate each actual guest heap base/top. Allocation
  and retirement metadata are serialized with the allocator lock; unowned,
  interior, duplicate or zero tops cannot release another heap block.
- `xbox_AllocThreadStackSized` page-rounds explicit requests. Zero/default uses
  XBE StackCommit (minimum one page), with an optional checked pre-init
  `xbox_ConfigureThreadStack` minimum. MM3 explicitly retains 512 KiB minimum
  headroom; this application choice no longer applies to every port. There is
  no separate eight-worker ceiling: guest heap and metadata/resource capacity
  bound allocation. Existing no-argument callers use the same implementation.
- Ex forwards KernelStackSize; the five-argument simple API uses the selected
  default. Exhausted/invalid worker stacks produce STATUS_NO_MEMORY, clear the
  handle output and do not silently execute inline. Explicit inline entry and
  diagnostics remain caller-selected. Failed host start-metadata/CreateThread
  setup returns the owned guest stack and reports INSUFFICIENT_RESOURCES.
  That host failure cleanup is reviewed but has no injected-failure test.
- Quiesced reset retires the host ownership records and active default while
  port minimum policy persists until changed. Stale addresses are invalid
  after shutdown; caller synchronization is still required before reset.
- Tests cover 12 simultaneously live allocations, 12 parallel allocating/freeing
  host threads, forged/interior/duplicate values, minimum and restored image
  default, explicit larger sizes/page rounding, overflowing requests and
  unbacked default policy. Bridge cases exhaust the heap for both APIs and
  verify no inline/spawned execution, plus a multi-megabyte Ex request and an
  overflowing request. The actual C++ consumer links both new APIs.
- Seven normalized original allocator/API/bridge blocks move to
  `tools/reference/legacy_worker_stack_policy.md`, verified against fork 3cabda4.
  Windows memory 54/54, kernel 62/62 and bridge 12/12 pass. Linux kernel 61/61, all 32 actual thunk
  modes, all 8 bridge thread modes and actual C++ consumer pass. Both renderer
  builds and ordinal audit pass. Evidence: `conformance_tmp/ownership-worker-*`.
- D3D12 night Cruise `run-3y9ez78l` passes in 123.875 seconds, with
  inspected gameplay capture and 1.11986 metres of movement. This proves
  bounded menu/loading/driving with MM3's explicit policy, not full thread
  concurrency/teardown fidelity.
- Main-stack-only RtlUnwind bounds, native thread export ABI/coverage, optional
  thread flags/IDs/system routine, object-reference/thread-object stand-ins and
  whole-tree ownership remain open.


### Unwind within the actual owned stack

- The old runtime rejects valid worker and callback registrations by checking
  only the main stack. Both new baseline cases fail before the fix; the main
  case passes. `xbox_GetStackBounds` now reuses ownership for main, allocated
  worker/kernel stacks and retained callback leases. It returns exact exclusive
  bounds or FALSE/zero outputs for unknown/retired addresses. Caller retains
  ownership while using the snapshot; this is not host-thread identity or a
  general memory/backing probe.
- Unwind checks the full argument frame before decoding, scratch budget before
  writing, complete aligned registration pairs, monotonic same-stack links and
  concrete target bounds. Valid handlers are popped before invocation; normal
  return restores the saved caller SP. Unknown/cyclic/truncated/foreign chains
  stop with diagnostics. A missing/unreachable target is never fabricated into
  fs:[0]; the head reflects only actual traversal. Caller-provided record flags
  are preserved. Full context/TargetIp/return-value transfer, collided handler
  dispositions and external record/TIB probing are still incomplete.
- The owner audit also found MmCreateKernelStack using plain heap memory and
  deletion freeing any supplied low pointer. These routes now use the shared
  typed owner registry. Their existing exact requested-size/high-end convention
  is preserved independently of worker minimum policy; matching kind/high/low
  is required to retire a kernel stack. Ordinary heaps/worker allocations cannot
  be released through that API. Zero/oversized requests fail. Native/guard-page/
  debugger semantics remain open. Cxbx's primary implementation separately shows
  guard-page accounting, which is not implemented by this bounded change:
  https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/core/kernel/exports/EmuKrnlMm.cpp#L135
- Three normalized original unwind/create/delete bridge functions are retained
  in `tools/reference/legacy_unwind_stack_policy.md`, checked against fork 1574479.
  New C++ references link the bounds and guest kernel-stack helpers. Memory
  cases verify boundaries, optional/zeroed outputs, lease/stack retirement,
  ordinary heap exclusion, strict allocation kinds/pairs, exact unaligned
  kernel size and independence from configured worker minimum.
- Seventeen bridge cases cover main/worker/callback/kernel stacks, target/exit
  walks, an unreachable or foreign target, foreign/cyclic/unknown chains,
  complete/truncated terminal frame pairs, truncated arguments, insufficient or
  unowned scratch and caller records (including a small valid scratch budget).
  Existing real spawned callbacks also query their retained worker ownership.
- Windows memory 56/56, kernel 62/62 and bridge 29/29 pass. Linux kernel 61/61, 34 actual thunk modes,
  all 8 thread and 17 unwind modes plus actual C++ consumer pass. Both renderer
  builds and ordinal route audit pass. Evidence is
  `conformance_tmp/ownership-unwind-*`; source-route checking alone is not the
  runtime traversal/argument proof.
- Vulkan night Cruise `run-6crgcgq2` passes in 123.656 seconds, with
  inspected gameplay capture and 1.64517 metres of movement. This is
  bounded menu/loading/driving evidence, not complete exception/concurrency
  fidelity. Native thread ABI/coverage, optional flags/IDs/system routine,
  object-reference/thread-object/channel stand-ins and whole-fork ownership
  still require review.


### Keep the shared README generic and move captured history to the port

- README's release history mixes generic fixes with captured title progress,
  timings, function counts, test totals and case-study conclusions. The full
  original 1721-line README is preserved as normalized text in
  `tools/reference/legacy_xboxrecomp_readme.md`, with fork 6292693, original
  README blob identity and SHA-256. Its body exactly matches the normalized
  original; historical relative links retain toolkit-root context.
- Shared README is now 477 lines. It retains pipeline/integration references
  and unchanged licence/contributor/credit text, while removing title success
  estimates, snapshot test totals and unsupported broad compatibility/speed
  claims. Generic fixes/contributors remain available in Git history. This
  does not erase the archived original attribution or evidence.
- Build guidance distinguishes D3D11 from SDL3_GPU configuration and SDL3 GPU's
  runtime Vulkan/Direct3D12 choice. The current CMake selectors, dependency
  modules, environment lookup and existing compiled renderer caches were read;
  obsolete Linux OpenGL/default, six-library and no-dependency claims leave the
  active README. The starter host is explicitly Windows-oriented and needs
  lifecycle adaptation on other hosts.
- All 55 local Markdown path links resolve. Six actual CLI help commands pass:
  xbe_parser, disasm, func_id, abi_analysis, recomp and conformance; the documented
  primary analysis/generation flags appear in their current interfaces. Snapshot
  equality and the unchanged licence/credits block are asserted. Evidence:
  `conformance_tmp/ownership-readme-*-help.log` and
  `ownership-readme-validation.json`. This is documentation/interface evidence,
  not a new clean installation, full regeneration or gameplay acceptance run.
- Source behavior is unchanged by this checkpoint. Linked guides/templates,
  native/thread/object fidelity and whole-fork ownership still require review.


### Native thread callback ABI and optional guest system routine

- Native callbacks now distinguish the one-context start routine from the
  two-argument system routine. Ex supports the optional system callback;
  ordinal 254 has its own five-argument native wrapper. Termination returns
  VOID. Primary signature reference:
  https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h
- The native facade executes host callbacks, preserves actual host suspension,
  returns the actual host thread ID and clears outputs on rejected requests.
  Nonzero thread extension/TLS/debugger requests return NOT_SUPPORTED instead
  of being silently ignored. Zero stack bytes selects the host default.
  Start metadata is freed before callbacks that may terminate without returning.
- Guest Ex with no system routine dispatches its supplied start routine with
  one context; custom system callbacks retain two arguments. Both creation
  routes report invalid/missing dispatch entries instead of fake success.
  Existing log field layout remains compatible with seed consumers.
- Native optional-system baseline and four guest Ex modes fail before their
  respective fixes. Seven native tests use real host threads; six guest modes
  cover optional system entry in inline/spawn modes and missing/unresolved
  routines for both ordinals. Seven original normalized code/guide blocks
  are preserved in tools/reference/legacy_native_thread_abi.md from 760ca88.
- Windows memory 56/56, kernel 62/62 and bridge 42/42 pass. Linux kernel 61/61,
  seven native and six guest modes, eight existing thread and seventeen unwind
  modes and actual C++ consumer pass. Both renderer builds and ordinal route
  audit pass. Evidence: conformance_tmp/ownership-native-thread-*.
- D3D12 night Cruise run-1mq5tm6c passes in 123.328 seconds,
  with inspected gameplay capture and 2.17761 metres of movement.
  This does not establish full thread fidelity: guest suspension/ID output,
  TLS/extensions, native allocation failure injection, object semantics and
  whole-fork ownership remain open. Other sections of the kernel guide have
  not been certified by this scoped correction.


### Generic kernel integration guide

- The kernel guide still embedded a fixed title section/TLS/stack/heap map,
  a title asset directory, obsolete source LOC and bump-allocator API claims,
  and broad replacement/full-synchronization claims unsupported by route tests.
  The original guide from fork a66c66c is preserved verbatim after newline
  normalization in tools/reference/legacy_kernel_integration_guide.md.
- The shared guide now points to current public headers and memory ownership
  guidance. Image-derived layout and port configuration replace edits to shared
  header constants. Native versus guest contracts, initialization/lifetimes,
  address spaces, import/data configuration and partial fidelity are explicit.
  No code behavior changes or new game acceptance claim accompany this edit.
- All nine local links resolve and the archive exactly matches the original.
  Evidence: conformance_tmp/ownership-kernel-guide-validation.json.
  Register-model/template commentary and broader source ownership still need
  review; this checkpoint does not certify the entire fork.


### Generic register-model guide

- Captured title function/SEH/global addresses and function-count/performance
  claims are archived from fork 72f7e88 in tools/reference/legacy_register_model.md.
  The snapshot matches the original after newline normalization.
- The shared guide now describes per-thread register state, local EBP and
  selected shared frame inheritance, runtime stack placement, real guest return
  slots, indirect target capture and operand/flag/FP preservation boundaries.
  It references the actual runtime template and focused generator checks rather
  than recommending a dummy return address or process-wide guest registers.
- All six local links resolve. Six EBP/frame tests and nine return-address checks
  pass using uv with Capstone; the default Python lacks that dependency. Logs:
  conformance_tmp/ownership-register-checks.log, ownership-register-return-checks.log
  and ownership-register-validation.json. Documentation only; no new runtime
  behavior or full CPU fidelity claim. Template commentary and remaining source
  ownership still need review.


### Runtime-template commentary ownership

- Eleven original comment blocks from fork e3bd6bd are retained in
  tools/reference/legacy_runtime_template_comments.md. They mix captured title
  failures/counts/addresses with obsolete process-global/EBP/ABI descriptions.
  The shared header now keeps generic integration requirements and states the
  actual limits of coarse code bounds, local/shared EBP, ABI diagnostics,
  double-backed x87 precision and independent MMX storage. The contradictory
  claim that direct calls bypass RECOMP_ABI_CALL is removed.
- All non-comment tokens match the original, including literals, preprocessor
  directives, declarations and macro bodies. Each archived block matches the
  normalized source. Two existing runtime-helper checks pass. Evidence:
  conformance_tmp/ownership-runtime-comments-validation.json and
  ownership-runtime-comments-tests.log. This is a comment-only migration, not
  new gameplay or complete CPU/SEH correctness evidence. Other source/template
  ownership and incomplete runtime semantics still require review.


### Move fabricated thread objects to the port

- Shared ObReferenceObjectByHandle formerly returned one static, signaled guest
  KTHREAD with zero exit status for every thread handle; other objects were
  returned as tokens. The static guest address also survived memory generations.
  Its normalized original from fork 081a77d is preserved in
  tools/reference/legacy_thread_object_bridge.md. The pre-fix regression confirms
  this fabricated-success behavior. Shared guest references now validate their
  output and report invalid handles or NOT_SUPPORTED with a cleared output.
- xbox_kernel_set_bridge_override installs a port host callback for a supported
  canonical function at initialization. It rejects invalid/data/unrouted IDs,
  preserves policy until changed, supports replacement/NULL clearing, and leaves
  native resolution/data exports alone. Selection occurs after one remap. ESP
  points to arg0 after the dispatcher consumes the return slot; the callback
  preserves that ESP and the original dispatcher performs ABI cleanup.
- MM3 main owns and registers the inherited reference approximation at ordinal
  246, validates its output and clears its cached guest address at startup.
  This preserves application compatibility without advertising fabricated thread
  state as generic kernel semantics. Live per-handle type/object/reference and
  thread exit/wait semantics remain incomplete; native object facades still
  require separate review.
- Windows bridge 44/44 and memory 56/56 pass. Linux kernel 61/61, both new actual
  bridge modes and C++ consumer pass. Cases cover remap-once selection, replacement,
  clearing, invalid/data IDs, native isolation, stdcall cleanup, fastcall register
  arguments and unsupported/invalid/truncated object-reference outputs. Both
  renderer builds and eight route-audit checks pass. An extracted production MM3
  callback check validates its inherited values, reuse, invalid/truncated outputs
  and unchanged ESP against the actual Linux runtime. Evidence:
  conformance_tmp/ownership-object-* and ownership-mm3-object-*.
- Vulkan night Cruise run-mujf75gd passes in 123.438 seconds with an
  inspected capture and 1.27575 metres of movement. This is bounded driving
  evidence, not complete object or concurrency fidelity. Whole-fork ownership
  and remaining native/guest runtime approximations are still open.


### Remove the native IDE scalar stand-in

- Native IdexChannelObject pointed to a four-byte 'IDEX' marker under the
  incorrect assumption callers only pass the identity back. The export is an
  IDE_CHANNEL_OBJECT structure; primary declaration reference:
  https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h#L1486
  The old placeholder and unresolved-import fallback from fork 1bc1dcb are
  archived in tools/reference/legacy_native_ide_channel.md; both blocks match
  normalized source. No port use of the scalar is preserved as generic semantics.
- Native object is now NULL until an embedding supplies host-owned storage.
  Direct resolution and native import initialization retain that supplied
  pointer. An unavailable native IDE data import remains NULL instead of
  exposing executable unresolved-handler bytes as a channel structure. The
  separate fixed-width guest export and its circular empty queue are unchanged.
  This does not implement native IDE hardware or marshal native pointers.
- The new default assertion fails against the old marker. The actual fixture
  now loads a minimal image and explicitly selects its export page, then checks
  unconfigured/supplied native direct lookup and import-slot values, guest queue
  links and independent key storage. Windows and Linux fixture runs pass;
  Windows bridge 44/44 and Linux kernel 61/61 pass. Both renderer builds and
  eight ordinal-route checks pass. Logs: conformance_tmp/ownership-idex-*.
  No new gameplay evidence is claimed for this native-only behavior change.
- Guest asynchronous IDE/IRP scheduling, native object shape/lifetime semantics
  and broader fork ownership remain incomplete and require separate work.


### Source overview and host/sampler ownership

- The src overview retained captured 147-import/LOC/library totals, a Windows-only
  architectural snapshot and unchecked/incomplete initialization examples.
  Its normalized original and the top-level CMake library commentary from fork
  57f6f4c are archived in tools/reference/legacy_runtime_source_overview.md;
  both original blocks match. Shared source guidance now links current build,
  memory/register and module contracts, separates application policy and avoids
  treating a route/library as fidelity proof. All 17 local links resolve.
- Top-level CMake changes are comments only: all nonblank/noncomment lines match.
  Evidence: conformance_tmp/ownership-source-overview-validation.json. No new
  renderer/gameplay/installation validation is claimed by this documentation edit.
- Complete host implementation/header/build definitions were read. Shared code
  uses hardware MMIO ranges, ownership-aware alias resolution, APU/OHCI/NIC/NV2A
  handlers and optional host graphics bring-up. Its default window/device setup
  is generic service behavior; application entry, assets, captured engine
  addresses and service selection remain in the port. Retain these mechanisms.
  This ownership decision does not certify shutdown/reinitialization cleanup,
  all MMIO paths, portability or hardware fidelity.
- Complete spike sampler and video build definition were read. The mechanism
  watches an explicitly selected current host thread, derives executable bounds
  and symbol names at runtime, and reports aggregated samples. MM3's specific
  render-thread registration remains in src/recomp_manual.c; the shared renderer
  consumes the report API. No title function address lives in the sampler.
  Retain it as an opt-in generic diagnostic, with Windows x64/PDB, one-thread
  and process-lifetime resource limits. No new sampling/concurrency/performance
  correctness claim accompanies this source ownership review.
- Other source files/module guides, captured commentary and runtime approximations
  still need review. This checkpoint does not certify whole-fork ownership.


### Host input ownership and integration guide

- Input implementation/header/build and the keymap fixture were inspected.
  Host XInput/SDL state conversion, configurable default mappings, input-capture
  packet handling and explicit keyboard/menu/click callbacks are reusable
  mechanisms. MM3-specific driving buttons/state and native menu handoff remain
  in src/mods/keyboard_driving.c, mods.c and menu_mouse.c. No captured MM3 address
  or action map needs moving from the shared implementation. Retain these APIs.
- The old input guide describes Windows only, records obsolete LOC, omits the
  contextual/click interfaces and dismisses accessory support as unnecessary.
  Its complete normalized version from fork 7089a43 is archived in
  tools/reference/legacy_input_integration_guide.md. Current shared guidance
  documents host backends, configurable maps, callback thread/handoff boundaries,
  startup caching and partial capability/accessory support. Three links resolve.
  Header comment changes remove obsolete Windows-only qualifiers; all runtime
  tokens are unchanged.
- Windows keymap 3/3 passes. The actual Linux fixture compiles with assertions
  enabled and passes default remapping/hooks, click fallback, default pad map
  and two virtual gamepad discovery modes. Evidence:
  conformance_tmp/ownership-input-windows-* and ownership-input-linux-* plus
  ownership-input-validation.json. No new renderer/gameplay claim is made.
  Full input capture, hotplug, concurrency, vibration, accessory behavior and
  broader source ownership remain separate incomplete work.


### Shader ownership and unresolved depth-view policy

- Eleven small shader/binding/SDL compile-unit files were inspected and retained
  as generic resource binding, clear/presentation, varying, colour-Y16 or include
  mechanisms. These source decisions do not claim new shader/GPU correctness.
- A distinct unresolved policy was found in nv2a_y16d.hlsl and both renderers:
  every output uses the high 16 depth bits and a fixed quarter-guest-pixel left
  shift. Low packed depth/stencil words cannot be read through that path. The
  colour-Y16 shader does select low/high words by parity. Mark these specific
  depth-view scopes needs_port_policy instead of certifying generic emulation.
- tools/reference/ownership_depth_view_frontier.md records inspected fork
  2218bff0b2d482a15a300e9530b4d0297d092c06, exact source hashes/shader and current port policy seams. Existing
  surface-sync case lists do not directly prove depth-Y16 parity or stencil.
  Actual GPU readback with distinct high/low bytes and stencil is required for
  a fidelity claim. Migration must preserve required MM3 compatibility
  explicitly and establish a truthful generic view contract; no implementation
  or new gameplay claim accompanies this evidence checkpoint.
- Depth-view migration, remaining shader internals and whole-fork ownership
  remain incomplete.


### Shared source selection for port-owned shaders

- Added XBOXRECOMP_NV2A_SHADER_DIR as a build-time extension configured before
  toolkit/helper inclusion. It must be an existing absolute directory. Matching
  fragment filenames replace shared source; missing fragments retain defaults.
  D3D11 embedding and SDL GPU generated compile units use the same selection.
  Port HLSL/HLSLI files are dependencies; additions/removals trigger selection
  again. Shader ABI, entry points/resources and external include dependencies
  remain the caller's responsibility. No new runtime replacement API is implied.
- MM3 sets its own source directory and selects
  src/mods/shaders/nv2a/nv2a_y16d.hlsl. Its bytes match the prior shared shader,
  preserving current logic while establishing the owner boundary. Both actual
  game renderer builds pass, and SDL compile-unit/D3D11 build inputs select the
  port fragment. The shared fallback/high-word proxy and fixed backend offsets
  have not yet been removed or given complete generic semantics. The depth-view
  frontier is updated to distinguish this checkpoint from completed migration.
- Windows fixtures build actual SPIR-V and DXIL with default and port selection;
  each CTest passes. Linux port fixture builds SPIR-V and passes CTest. The
  fixture verifies selected/fallback sources and embedded port marker. A changed
  port fragment rebuilds D3D embedding plus both Windows shader formats; invalid
  relative source directories fail configuration. Evidence:
  conformance_tmp/ownership-shader-* including ownership-shader-validation.json.
  No new GPU pixel, driving or complete format-fidelity claim is made.
- Inventory now covers 627 committed paths with unchanged 153 src paths.
  Whole-fork ownership and the identified depth policy migration remain open.


### Move depth-view alignment into MM3's selected shader

- Both renderers now supply host-source texels per guest pixel, without the
  captured quarter-pixel alignment factor. The shared shader reads its source
  index without a shift. MM3's selected fragment applies its own 0.25 factor,
  preserving its prior proxy/alignment behavior. Original two constants and
  full shader from fork 528e2a4 are retained in
  tools/reference/legacy_depth_view_alignment.md and match normalized source.
  Float offset equivalence is checked over 48 representative dimension pairs.
- A new actual GPU fixture clears distinct depth regions, converts the view at
  2x scale and reads the 16-bit RGBA texel at the region boundary. The default
  unshifted expectation fails against the old implementation while the shifted
  control passes. Default and MM3-selected fragment expectations now pass on
  D3D11 1x/4x MSAA and SDL GPU using explicit Vulkan and Direct3D12 drivers.
  All 61 surface-sync cases pass. This is alignment proof, not packed depth-word
  parity or stencil-preservation proof. Both renderer game builds pass.
- D3D12 night Cruise run-sgbh4qpa passes in 123.719 seconds with inspected
  gameplay capture and 2.09215 metres of movement. Evidence:
  conformance_tmp/ownership-depth-* and ownership-depth-validation.json.
- The high-word-only shared depth proxy remains unresolved. These rendering
  scopes retain needs_port_policy in the inventory; this checkpoint removes the
  fixed alignment policy but does not claim complete depth-view migration,
  packed low-word/stencil fidelity or whole-fork ownership.


### Complete ownership migration of the depth proxy

- Removed src/video/shaders/nv2a_y16d.hlsl from the shared tree. Its normalized
  05c5233 version matches tools/reference/legacy_shared_depth_proxy.md. MM3's
  existing selected fragment remains the active high-word/alignment policy.
  The shared pipeline no longer supplies an implicit high-word approximation.
- The optional caller fragment nv2a_y16d.hlsl activates converter compilation
  through both shader helpers and the real runtime targets. Without it,
  cached-depth-as-Y16 draws report the missing conversion and return unsupported
  before binding/drawing; no proxy is allocated and no stale RAM fallback is
  used. With it, shared code owns conversion targets, metadata and execution.
  The source-selection fixture uses its own synthetic depth visualisation.
- Native colour/depth converters share a checked target constructor. Texture,
  SRV and RTV creation failures release partial objects and publish no cache;
  subsequent attempts retry. Actual fault/readback tests cover all three native
  failure points for both colour and depth and texture failure on SDL GPU.
- Windows surface 63/63 passes, including unconfigured draw rejection. Configured
  MM3 readback passes D3D11 1x/4x MSAA, Vulkan and Direct3D12; explicit Vulkan and
  Direct3D12 absent-converter checks pass too. Default/synthetic shader CTests
  pass, including disabling/re-enabling in one build directory. Linux shader
  fixture and kernel 61/61 pass. Both game builds pass. Evidence:
  conformance_tmp/ownership-depth-* and ownership-depth-owned-validation.json.
- Vulkan night Cruise run-dz0s8k2b passes in 123.453 seconds with an
  inspected capture and 1.47077 metres of movement. The inherited
  MM3 proxy remains an approximation; full generic packed low-word/stencil
  emulation is unimplemented. This closes this policy migration, not the wider
  ownership or renderer-fidelity audit. Inventory retains 627 paths,
  with 152 src paths after removing the shared shader and adding a synthetic test shader.


### Clarify the retained NV2A register/shader boundary

- Archived the old NV2A guide in tools/reference/legacy_nv2a_integration_guide.md;
  its payload matches fork 1dce749 after newline normalization. The shared guide
  now documents singleton/buffer lifetime, host-owned fault routing, separate
  hook storage, unsupported decode handling and VRAM allocation limitations.
  Removed stale LOC tables, complete-support claims and obsolete renderer advice.
- Read the register core and remaining shared vertex/combiner shader logic.
  Retain these generic guest-state mechanisms with scoped inventory decisions.
  This is not certification of device timing, decoder safety, shader fidelity
  or the whole fork. No executable source changed in this checkpoint.
- All nine guide links resolve; fork diff is documentation only. Inventory
  remains 627 tracked paths and 152 src paths. No new gameplay claim.


### Identify remaining pushbuffer stop-recovery policy

- Current scanner and hitch tests still contain an odd/increment<=128 fence
  recovery heuristic over unparsed payload after malformed commands. Recorded
  exact source/test excerpts and migration requirements in
  tools/reference/ownership_pushbuffer_stop_frontier.md. Both paths now carry
  needs_port_policy rather than a broader implied generic certification.
- Existing Windows hitch quiet/trace tests pass (2/2); those tests explicitly
  require the heuristic, so they do not establish hardware-correct recovery.
  No executable behavior changed. Generic stopped-submission support, MM3-owned
  compatibility recovery and runtime verification remain required.


### Move malformed-stream fence recovery into MM3

- Shared nv2a_pb.h exposes a startup/quiesced synchronous stop handler and
  existing executor primitives. NULL is the default and removes the handler.
  Scanner bounds, continuation and consume-to-PUT behavior remain; it no longer
  searches undecoded payload for semaphore commands. Survey mode never calls
  recovery. The MM3 startup registers src/mm3_pb_recovery.c, which owns the
  inherited odd/increment<=128 compatibility rule and validates its read span.
- Shared tests prove no inferred release by default, exact callback arguments,
  explicit caller work, no callback for valid work/survey, and removal via NULL.
  The separate MM3 fixture covers selection, rejected values, unreadable/wrapped
  and unaligned spans, and a header truncated before PUT. Windows kernel 62/62,
  Linux kernel 61/61, MM3 fixture 1/1 pass; both renderer game builds pass.
- Vulkan night Cruise run-1i7xopyf passes in 124.265 seconds with an inspected
  capture and 1.43926m movement. This run has no malformed stops; branch proof
  comes from synthetic tests, not gameplay. Evidence: conformance_tmp/ownership-stop-*.
  MM3 retains an explicit approximation, not hardware-correct recovery.
  Inventory covers 628 paths / 153 src paths. Wider ownership remains open.


### Archive captured APU device observations

- Moved four Burnout/Wreckless observation blocks from apu_core.c comments
  to tools/reference/legacy_apu_device_observations.md. The shared comments
  explain bus aliases, interrupt vector, register startup and lock scheduling
  without captured guest addresses or title debugging narratives. Copyright
  and license notices remain untouched.
- All four archived payloads match normalized prior source. A comment-aware
  token comparison proves executable tokens unchanged; whitespace checks pass.
  No runtime rebuild/test is needed for this comment-only change and no new
  audio-fidelity claim is made. Inventory scope is these four comments, not
  the full APU implementation; the wider ownership audit remains active.


### Move captured support reports and development retrospective

- Archived the complete legacy gap-analysis and lessons-learned documents in
  tools/reference/legacy_gap_analysis.md and legacy_lessons_learned.md. These
  contain title priorities, captured addresses, development-session history and
  stale DONE claims; they are retained as history, not current support proof.
- Existing shared paths now contain generic integration review criteria and
  support-evidence boundaries, with source/test entry points. Updated README,
  index, shader-guide and contributor references without removing credit.
- Both archive payloads match prior text after newline/trailing-space normalization; all 16
  replacement-guide links resolve. The fork diff changes documentation only;
  no runtime tests or new fidelity claims are warranted. Scope is replacement
  guides/link descriptions, not full review of the referring documents.
  The wider ownership audit remains active.


### Review generic symbol and texture-pack tools

- Read rw_port.py, lib_port.py and texpack/upscale.py in full. These take
  caller-supplied donors/target analysis or textures; no captured assets or
  title addresses drive their implementation. Retain the mechanisms.
- Archived both symbol-tool module docstrings in
  tools/reference/legacy_symbol_port_observations.md and replaced title
  results/unproven cross-version generalizations with usage and matching limits.
  Both archives match normalized prior text; executable Python ASTs are
  unchanged after excluding module documentation.
- Eight synthetic CLI cases pass across both symbol tools: unique match,
  duplicate rejection, absent match and rejection off detected function starts.
  Texture upscaler --selftest passes. Evidence/inputs: ignored
  conformance_tmp/ownership-symbol-check.py and ownership-symbol-inputs/.
  These are bounded tool contracts, not complete COFF/PE validation, symbol
  identity or texture visual parity. Broader ownership remains open.


### Verify synthetic conformance fixture provenance

- Read mkxbe.py and image.py fully. test.xbe exactly equals mkxbe.build():
  4176 bytes, SHA-256 07289d2aa163c828d306f8a72f472245c2eb7b654daf8f0765a9b1c4bc16b0f0.
  It contains five generated arithmetic leaf functions, not captured title data.
  Retain the fixture/generator and generic caller-input PE/MAP extent helpers.
- Added one dependency-free unittest requiring the tracked binary to match its
  generator. It passes. Native MSVC XBE comparison finds five comparable
  functions, zero rejected, and passes all 55 input vectors with zero mismatches.
  Evidence: conformance_tmp/ownership-synthetic-xbe-native.log and its check script.
  The first local invocation used the wrong runtime include path; rerunning
  with the driver's templates/runtime path passed. No runtime code changed.
- This proves synthetic reproducibility and plumbing, not real-title execution,
  full parser safety or full conformance-driver ownership. Inventory now has
  629 tracked paths / 153 src paths. The wider audit remains open.


### Move application keyword classification into explicit MM3 policy

- Removed the implicit racing/application lexicon from shared func_id config.
  tools/mm3_func_id_keywords.json preserves all nine original ordered category
  lists exactly. The shared CLI accepts --game-keywords JSON; run and
  propagate_labels accept a caller mapping. None/empty disables the keyword
  pass; it neither installs global policy nor leaks policy into later calls.
  The scoring/matching algorithm and non-keyword classification stay shared.
- Four unit tests pass: default disabled, explicit policy and call isolation,
  eight malformed policy schemas, and JSON CLI forwarding. Actual full-pipeline
  synthetic CLI checks produce unknown by default, game_space for a caller
  mapping, and game_vehicle for the MM3 mapping on identical input. Evidence:
  conformance_tmp/ownership-func-keywords-check.py and ownership-func-keywords/.
- Updated the shared usage/options and moved its captured output example to
  tools/reference/legacy_function_id_output_example.md. MM3 opt-in usage is in
  tools/reference/mm3_function_keywords.md. No game runtime files changed.
  Scope is this policy boundary, not complete function-identification accuracy
  or full-file ownership certification. Inventory: 630 paths / 153 src paths.


### Review generic ABI analysis and correct output-path handling

- Read the ABI package CLI, analyzer, minimal section reader and package entry
  files. Inputs supply image sections, functions and classifications; no title
  addresses or captured datasets drive these mechanisms. Retain these helpers
  with their heuristic/partial-parser limits, not an accuracy certification.
- The CLI rejected --output abi.json because dirname returned an empty string.
  It now resolves the parent through abspath before mkdir. A synthetic end-to-end
  test fails the basename case on old code and passes basename/nested outputs
  with the fix, checking all five output addresses. No game runtime changed.
- Full tools regression passes under vcvars64 with isolated Unicorn 2.1.4:
  655 passed, 15 skipped, 79 subtests, 34.15s. Initial runs without compiler
  headers and then without Unicorn failed setup-dependent compile checks;
  those limits were resolved in the test environment, not masked in tests.
  Evidence: conformance_tmp/ownership-python-{current,msvc,complete}.log/xml.
  Dependency install is isolated in build/ownership-python-deps; user venv
  was not modified. Inventory: 631 paths / 153 src paths. Audit remains open.


### Replace captured decompilation guide with synthetic workflow

- Archived the old DECOMP.md (Dashboard bytes/addresses, title coverage counts
  and RTTI results) plus captured splitter rationale in
  tools/reference/legacy_decomp_guide.md. The normalized historical guide
  matches prior source. Shared guide uses actual listing/manifest output from
  the five-function synthetic XBE and corrects the explicit --xbe conformance
  invocation; removed claims of guaranteed function discovery/safe execution.
- Read the splitter and CLI. They operate on caller image/analysis and remain
  generic. Source changes affect comments/docstrings only; executable AST
  equality checked after stripping docstrings. Three splitter tests pass.
- Actual parser and split CLIs emit five synthetic functions; reconstructed db
  directives match all five original input ranges. All five guide links resolve.
  Evidence: conformance_tmp/ownership-split-synthetic-check.py and its output
  directory. This is not original-title boundary/ABI accuracy or link parity.
  Wider ownership review remains open.


### Remove reference-version restriction from RenderWare identification

- The RW source-ID parser used a literal RW36Active path and the header
  described a captured count of 67 strings. It now accepts numeric RW version
  tags in the same ID envelope; captured header/layout history is archived with
  the old pipeline guide in tools/reference/legacy_function_identification_guide.md.
  Config executable AST is unchanged; its title-specific history is now in MM3.
- RW37 input fails the old parser's public classifier. Six focused tests now
  pass across versions, malformed/unrelated tags and caller keyword policy.
  Actual full-pipeline CLI checks classify RW36/RW37 as rw_plcore and leave
  RWfoo unknown on synthetic paired image/string inputs. Evidence:
  conformance_tmp/ownership-rw-version-check.py and ownership-rw-version/.
- Replaced unsupported guide flags, stale output schema, captured layout/counts
  and uncalibrated support claims with actual CLI/implementation boundaries.
  Both local guide links resolve; archived guide matches prior normalized text.
  Accepting an ID tag does not prove library ABI compatibility or correctness
  of region heuristics. Inventory: 632 paths / 153 src paths. Audit remains open.


### Derive Ghidra import origin from the caller image

- The flat-image builder already uses the XBE header base, but the runner
  imported every image at literal 0x10000. The runner now reads base_address
  from generated sections.json and forwards it as a quoted headless argument.
  Its old mock invocation fails the relocated-base assertion; corrected one
  passes. Removed captured size/layout assumptions from builder/runner docs.
- Synthetic tests verify byte placement/zero gaps at bases 0x10000 and 0x20000
  and mock headless argv forwarding. Linux 2/2 passes; Windows image test passes
  with two subtests and the POSIX shell fixture is skipped. bash -n passes.
  Evidence includes conformance_tmp/ownership-ghidra-base-check.py.
- Archived previous builder documentation and complete naming guide, including
  captured title counts/results, in tools/reference/legacy_ghidra_import_assumptions.md.
  Historical guide matches normalized old source; 3 replacement-guide links
  resolve. No live Ghidra analysis was run, and IMPORT=0 does not validate or
  rebase an existing program. No game runtime changed. Inventory now covers
  633 paths / 153 src paths. Broader ownership remains open.


### Keep debug-symbol attribution generic and synthetic

- Read source attribution, CLI and tests. The mechanism uses caller function
  ranges/string references; no captured title path drives runtime behavior.
  Archived the old Halo guide/results and source/test examples in
  tools/reference/legacy_debug_symbol_recovery.md. Guide payload matches prior
  text after newline/trailing-space normalization.
- Shared documentation now describes tentative direct votes/interpolation,
  actual JSON wrapper and application behavior. Replaced title-specific test
  paths with neutral synthetic equivalents while retaining path grammar and
  leading-digit cases. Recovery executable AST unchanged excluding docstrings.
- All six self-checks and six pytest cases pass, including section boundaries,
  output metadata, backup creation and preservation of existing names. The
  replacement source link resolves. No original symbol/ABI/runtime fidelity
  claim follows from these heuristic tests. Wider ownership remains open.


### Review RTTI and remove reference-layout assumptions

- Read the RTTI package. It uses caller image records/hierarchies; captured
  Half-Life/corpus counts and class/address examples are now preserved in
  tools/reference/legacy_rtti_recovery.md. Shared docs treat boundaries, seeds
  and owners as proposals, not proof of callable identity. Historical guide
  payload matches prior normalized text; replacement source link resolves.
- Image code ranges now include input executable flags, preserving existing
  conventional/SDK-name fallback. Raw extents are clipped to actual file bytes;
  u32 requires four bytes within one raw section. Shared image helpers fix all
  RTTI callers rather than guarding individual vtable reads.
- Both new synthetic tests fail old code and pass on Windows and Linux: custom
  executable names, legacy fallback, section/file bounds and truncated public
  recovery. Actual CLI checks verify class/name/seed output and the no-records
  no-output case. Evidence: conformance_tmp/ownership-rtti-cli-check.py and
  ownership-rtti-cli/. Full configured Python suite passes: 660 tests, 16 skips,
  89 subtests, 35.02s (ownership-python-rtti.log/xml).
- This is generic ownership and supported-record proof, not complete malformed
  header handling, RTTI/ABI correctness or original-title acceptance. Inventory
  covers 634 paths / 153 src paths. Broader audit remains open.


### Review retained native and renderer helpers

- Read native object/pool adapters, fixed-width guest I/O declarations,
  texture hashing, sampler-cache slot selection and pacing helpers. These
  mechanisms are generic; no title-specific runtime policy was identified
  in those scopes. Native object operations remain partial/no-op adapters,
  not a complete typed namespace or guest reference-counting implementation.
- Corrected object/pool documentation that overstated correctness and moved
  old claims/local I/O evidence pointers to
  tools/reference/legacy_kernel_helper_claims.md. Executable token equality
  proves no object/pool/I/O behavior changed. Guest layout constants and
  assertions remain intact; C11 and C++17 probes both compile.
- Fresh Windows helper builds pass all eight CTests: hash exact/pages/feature
  dispatch (3), sampler lifetime (1), pacing cadence/conversion/failure/native
  wait cases (4). Evidence: conformance_tmp/ownership-helper-*.log and
  build/ownership-helper-*/Testing. These contracts do not establish full
  guest-object/I/O semantics, all CPU paths, performance or gameplay fidelity.
  The wider ownership audit remains open.


### Identify implicit mip-completion visual policy

- Shared host_levels expands authored partial non-volume chains to 1x1 by
  default. A compiled production-helper probe reports guest2/host6 for 32x32.
  Both renderer texture_get paths use it; host format can change too.
  Recorded source and required migration in
  tools/reference/ownership_mip_completion_frontier.md.
- Cache epoch/hash returns precede host-level selection, so the opt-in must be
  startup-fixed or tracked in cache identity/invalidation. Existing generation
  tests need explicit policy selection. These scopes now carry needs_port_policy
  rather than an implied complete generic decision. No executable code changed
  and no new visual/gameplay claim is made. Evidence:
  conformance_tmp/ownership-mip-policy-probe.c/.log. Migration remains required.


### Move mip-completion selection into MM3

- Fork 0e3ec26 preserves authored mip counts by default. Startup-fixed
  RECOMP_COMPLETE_MIPS=1 explicitly selects generic 2D/cube completion. MM3
  selects its inherited behavior before texture creation and honors overrides.
- Windows texture checks pass 24/24, explicit SDL Vulkan and D3D12 each 5/5,
  Linux CPU five checks, and both game builds. Default native BC tests sample
  the GPU; SDL checks compressed byte retention separately from sampling.
- Owned Vulkan night-driving run-gwa36lie passes in 123.484 seconds, with
  about 1.66 metres of movement and inspected capture-004.bmp. This is bounded
  compatibility evidence, not a fix claim for light/sky flicker. Historical
  texture test results now live in tools/reference/legacy_texture_test_guide.md.
  Frontier source/history and migration proof remain in
  tools/reference/ownership_mip_completion_frontier.md. Wider audit stays open.


### Separate DSP provenance from captured review history

- Read all four DSP provenance, source-inventory, SA-frontier and stack-extension
  documents. Preserved their original bytes in tools/reference/legacy_dsp_*.md.
  Removed obsolete local hashes, captured test outcomes and sequential
  unimplemented/implemented narratives from the shared documents.
- Fork retains the unchanged license/provenance header, pinned upstream origin
  mapping, architecture references, adaptation boundaries and validation limits.
  Relative links resolve; changes are Markdown only. No new DSP execution or
  hardware-conformance claim is made. Interpreter ownership/fidelity remains
  subject to the broader audit.


### Identify embedded dashboard path-layout policy

- Full kernel_path.c read and a compiled Windows production-backend probe
  confirm C/Y/Partition2 asset paths select the game root. Existing guest
  symbolic links are constrained by static target rules; the path callback
  only observes. Recorded required host-mount separation and unresolved proof
  in tools/reference/ownership_path_mount_frontier.md.
- Marked this source needs_port_policy. No path behavior changed; this is not
  a completed migration or filesystem-conformance claim. The broader audit
  and the newly identified mount-policy work remain active.


### Replace dashboard path defaults with explicit host mounts

- Fork 365656b adds a borrowed, quiesced XboxPathMount table with structural
  validation, longest-prefix matching and explicit clearing. Guest drive links
  take precedence and may target mounted custom devices; bare partition devices
  retain image handling. Default C/Partition2 files use SystemData and Y uses
  Cache. Extracted dashboard layout is now an explicit application selection.
- Moved eight historical title/path comment blocks and the legacy layout example
  to tools/reference/legacy_path_layout_notes.md. MM3 does not opt into unused
  dashboard aliases. Join capacity checks reject truncation; Windows preserves
  a selected volume root instead of turning C:\ into drive-relative C:.
- Final Windows kernel 62/62 plus mount/save 1/1, Linux kernel 61/61 plus
  mount/save fixture pass. Both final game builds pass. Owned final Vulkan
  run-7008on3d passes 49 steps in 123.422 seconds, with 1.33619m movement and
  inspected night-driving capture-004.bmp. Frontier document records coverage
  and limits. Broader namespace/containment, disk policy and ownership remain
  open; no dashboard runtime or full fidelity claim.


### Move host-video evidence-frame selection into MM3

- Fork 68ad91a removes fixed frame-60/80 capture selection from the shared pump
  and adds a quiesced presentation callback. MM3 registers its existing
  optional environment-driven capture choice in src/mm3_host_video.c.
- Corrected BMP evidence claims: the writer copies decoded CPU memory, not
  presented pixels. Historical source commentary is preserved in
  tools/reference/legacy_host_video_capture.md. Added the missing Linux
  unsupported BMP-dump implementation so the public API links coherently.
- Windows synthetic actual-pump callback CTest 1/1, Linux unsupported API CTest
  1/1, MM3 policy fixtures on both platforms, and both game builds pass. These
  are contract/build checks, not real Media Foundation, GPU or gameplay proof.
- Read fb_present.h and pvideo_scanout.h in full as generic interfaces/helper;
  no executable changes or fresh display-fidelity claim. Decoder internals,
  resource lifecycle and the broader ownership audit remain open.


### Keep APU diagnostics from changing device traffic

- Fork a356fda fixes RECOMP_APU_TRACE performing an extra MMIO read after every
  decode attempt, including writes and unsupported opcodes. Logging now uses
  the value already serviced by the decoder. The actual-hook synthetic test
  fails with tracing before the fix; traced/plain processes then pass 2/2 on
  Windows and Linux. Both game builds pass. This is a useful generic runtime
  correction, not a claim of complete decoder or hardware audio conformance.
- Read audio support headers/shim and retained generic declarations, conversion
  helpers and compatibility mechanisms with explicit partial-proof limits.
  Corrected stale native-rate/WASAPI/test-tone claims; preserved their history
  in tools/reference/legacy_apu_support_claims.md. Copyright/license notices,
  including original project attribution, remain intact. Broader audio and
  ownership review remains open.


### Review generic output backends and retire captured audio guide

- Fork 0e671bf fixes SDL byte-count multiplication occurring before input
  validation. INT_MIN/INT_MAX cases extend the existing output tests. Actual
  SDL dummy output reproduces overflow under UBSan before the fix and passes
  1/1 afterward with halt_on_error; Windows output/monitor tests pass 2/2.
- Read both output branches as generic host mechanisms. Replaced the broad
  bring-up guide with concise interface/ownership/diagnostic documentation,
  preserving the original in tools/reference/legacy_apu_guide.md. Captured
  compiler timing comments moved there too; CMake commands are unchanged and
  local guide links resolve. No source rights notices were removed.
- This establishes scoped source/input/queue evidence, not physical endpoint,
  COM/thread lifetime, timing, arbitrary audio fidelity or new driving proof.
  The broader ownership audit remains open.


### Replace D3D8 snapshot claims with generic contracts

- Fork f55fc29 replaces the old LOC/feature table with a concise interface,
  backend and test guide. Original guide and historical swizzle/menu sampling
  commentary are preserved in tools/reference/legacy_d3d8_guide.md.
- Executable swizzle tokens are identical under string-aware comment stripping;
  new local documentation links resolve. Build definitions and FVF position
  helpers were read as generic mechanisms without source changes.
- Five Windows D3D8 check programs build/run successfully: formats, gamma,
  bound states, alpha/RHW and vertex programs. This is scoped source/check
  evidence, not full format/decoder verification, POSIX GL parity or gameplay.
  The remaining D3D8 source and broader ownership audit stay open.
