# XboxRecomp ownership

`tools/xboxrecomp` consumes a published commit from
[MattSG/xboxrecomp](https://github.com/MattSG/xboxrecomp), based on
[sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp). Runtime changes
belong in scoped, game-agnostic fork commits that can be offered upstream.
Update the gitlink to consume them; setup and CMake do not apply patches.

This repository owns MM3 guest addresses, classifiers, rendering policy,
gameplay/timing/HUD hooks, fixtures and tools. It uses the runtime's generic
hooks. Reusable-looking helpers used only by the port also belong here:

- `src/` and `src/mods/`: the port, mod utilities, cache and option policy.
- `src/geometry/`: routing, HUD clustering, camera and drawing math.
- `src/ui/`: the RmlUi launcher host.
- `src/mm3_recomp_types.h`: the title layer over runtime types.
- `scripts/mm3_runtime_unwind.py` and the runtime-entry manifests: MM3 recovery.

Upstream title-specific code may remain inherited. New fork changes must not
introduce MM3 addresses, assets, policy or title-specific defaults. Preserve
third-party licences and provenance, including the xemu DSP core.

## Generation and validation

`scripts/generate_mm3.py` runs analysis and translation with the submodule's
own tools. Recovered MM3 definitions are excluded from the main batch and
emitted by the port's recovery recipe. Setup reuses generated output only
when its fingerprint matches the committed toolkit tools/templates and MM3
recipe/input bytes. Use `--force` for uncommitted toolkit experiments.

The migration was validated with the Windows D3D11 build, native x87
comparisons, port contracts and bounded menu/loading/driving checks with
movement telemetry and inspected captures. The Linux SDL3_GPU runtime build
and focused wait/timer/guest-read contracts also passed. These checks do not
establish complete hardware, rendering or audio fidelity, Linux gameplay, or
external performance measurements.

Use `tools/game_test/run.py` for owned, bounded game sessions and copied save
templates. Keep game assets, generated translations, private saves and local
validation artifacts out of commits. See `tools/game_test/README.md`.
