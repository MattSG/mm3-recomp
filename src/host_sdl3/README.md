# host_sdl3: mm3's SDL3 host stack

The SDL3 renderer, window, pads, audio and SDL_GPU D3D8 layer that used to live
in the MattSG xboxrecomp fork, as a module of this repository. It builds against
upstream xboxrecomp configured with the host backends left out (see
`docs/technical/host-backends.md` in the toolkit, branch
`hooks/host-extension-points`).

| File | Provides |
|---|---|
| `video/nv2a_sdlgpu.c` | the `nv2a_d3d_*` renderer contract on SDL_GPU |
| `video/fb_window_sdl.[ch]`, `video/fb_present_sdl.c`, `gfx/*` | the framebuffer window contract, event pump, GPU device and shader helpers |
| `d3d/*` | the `xbox_d3d8` target (SDL_GPU) |
| `apu/apu_sdl.c` | the `xa2_*` audio device on an SDL3 stream |
| `input/sdl_pads.c` | the host pad provider; call `mm3_sdl3_install_pads()` before `xbox_InputInit()` |
| `shaders/*`, `Nv2aSdlgpu.cmake` | SDL_GPU shaders compiled with DXC at build time |

Wiring (the order matters: the toolkit checks for `xbox_d3d8` when it is added):

```cmake
set(XBOXRECOMP_RENDERER EXTERNAL)
set(XBOXRECOMP_APU_OUTPUT EXTERNAL)
set(XBOXRECOMP_BUILD_D3D8 OFF)
set(XBOXRECOMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/xboxrecomp)
add_subdirectory(src/host_sdl3)
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)
target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp mm3_host_sdl3)
mm3_copy_runtime_dlls(${PROJECT_NAME})
```

Status: copied unchanged from the fork's `origin/main` (3c2c02a8), plus the pad
provider split out of `xinput_device.c`. mm3's `CMakeLists.txt` builds the
toolkit `EXTERNAL` and links this module; `tools/xboxrecomp` is the upstream
toolkit branch `hooks/host-extension-points`. A scratch project wiring the module
to the toolkit builds, links and runs. mm3 itself configures through the module,
the toolkit and the RmlUi/ImGui fetches, and stops at the check for generated
recompiled sources (`src/recomp/gen`), which need the game files, so the full
mm3 build and a run still have to be done on a machine with the game.
