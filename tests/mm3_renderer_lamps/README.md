# MM3 lamp rendering contracts

These fixtures belong to MM3: they select Flares/Flares.psh and replay its
captured vertex/combiner program through the real D3D11 renderer. The game
policy is compiled from src/mods/render_policy.c, not copied into the fixture.
They cover visible and occluded lamps, the authored pass decision table, Z/W
encoding changes between scene and flare, render scale and MSAA requests.

Windows only. From the MM3 root, in a Visual Studio developer shell:

```sh
cmake -S tests/mm3_renderer_lamps -B build/renderer-lamps
cmake --build build/renderer-lamps --config Release
ctest --test-dir build/renderer-lamps -C Release --output-on-failure
```

CPU-only panorama policy checks: python tests/mm3_render_policy_regression.py.
These fixtures do not establish whole-game temporal lighting fidelity.
The SDL3 GPU counterpart was removed when the SDL3 host moved to `src/host_sdl3`;
its test host no longer exists (see git history to restore it).
