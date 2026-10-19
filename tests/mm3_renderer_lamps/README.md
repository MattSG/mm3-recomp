# MM3 lamp rendering contracts

These fixtures belong to MM3: they select Flares/Flares.psh and replay its
captured vertex/combiner program through both real runtime backends. The game
policy is compiled from src/mods/render_policy.c, not copied into the fixture.
They cover visible and occluded lamps, the authored pass decision table, Z/W
encoding changes between scene and flare, render scale and MSAA requests.

From the MM3 root, with SDL3/DXC available as required by the runtime:

```sh
cmake -S tests/mm3_renderer_lamps -B build/renderer-lamps
cmake --build build/renderer-lamps --config Release
ctest --test-dir build/renderer-lamps -C Release --output-on-failure
```

CPU-only panorama policy checks: python tests/mm3_render_policy_regression.py.
These fixtures do not establish whole-game temporal lighting fidelity.
