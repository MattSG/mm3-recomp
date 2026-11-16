# MM3 distance fog GPU regression

Windows only. From the MM3 root, in a Visual Studio developer shell:

```sh
cmake -S tests/mm3_renderer_fog -B build/renderer-fog
cmake --build build/renderer-fog --config Release
ctest --test-dir build/renderer-fog -C Release --output-on-failure
```

The two cases run production MM3 fog (`src/mods/render_fog.c`) over the real
D3D11 renderer, with and without MSAA: near/mid/far depth, destination-alpha
preservation, sky fill, world-up horizon fade, invalid range rejection and the
UI drawing that follows. `nv2a_bindings_fixture.c` is the generic device,
triangle and readback fixture, kept here since the fork dropped it.

The SDL3 GPU case was removed when the SDL3 host moved to `src/host_sdl3`; its
fixture and test host no longer exist (see git history to restore it).
