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
| Inventory coverage | Complete fork commit tree captured: 624 tracked files, including all 153 src paths; modes/blob IDs and working-tree hashes checked | Covered; ownership decisions remain scoped |
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
