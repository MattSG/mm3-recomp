# XboxRecomp ownership migration

Goal: merge SDL3 into main, then keep all MM3-specific code and policy in this
repository. Broader XboxRecomp refactoring is out of scope. No GitHub changes
have been published.

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
