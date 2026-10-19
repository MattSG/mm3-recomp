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
| Panorama selection | Both renderers identify MM3's 256x256 DXT5 horizon by fixed dimensions/mip/filter/blend criteria | Must move classifier to MM3 |
| Lamp selection | Both renderers identify Flares.psh's three visibility taps and lamp texture; title supplies only stage_program | Must move classifier to MM3 |
| Extended-distance shaders | Depth-view remap, haze coverage, sky fill/fade and skyline filtering contain enhancement policy | Audit and move all title policy; retain only proven reusable primitives |
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
