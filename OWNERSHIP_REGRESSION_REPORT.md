# Repository ownership and regression check

2026-10-07. Root baseline: `e93442c`. Toolkit baseline: `e3674f3`.
Toolkit ownership commit: `cc9388f`. Regression fixes: `6d55457`.

## Ownership

- MM3 keeps its entry repairs, guest addresses, compatibility overrides,
  generation manifests, asset viewer and menu/race benchmark driver.
- The maintained MM3 runtime header is now `src/mm3_recomp_types.h`, outside
  the ignored generated tree. Generation and the bootstrap check use it.
- The previously ignored CPU helpers are tracked in xboxrecomp's
  `include/recomp_cpu.h`; generation copies that source. They contain Xbox
  CPUID and flag semantics, with a standalone native regression check.
- Generic texture-pack upscaling, frame-time analysis and the PVIDEO check
  live under `tools/xboxrecomp/tools/`. The MM3 benchmark references the shared
  analyzer. Runtime comments describe shared behavior rather than MM3 assets,
  addresses or measured title-specific performance.
- No runtime behavior changed in this cleanup. Moved headers match their
  previous contents after line-ending normalization. The PVIDEO harness now
  includes the real register definitions and stdlib so it compiles against
  the current runtime.
- Both Git inventories were checked, including ignored source extensions.
  Remaining ignored source is generated guest code, dependencies, installed
  tools or investigation scratch. The two loose scripts inside the generation
  virtual environment are one-off string analysis and staging scratch, not
  build inputs. Game assets, binaries and test output remain excluded.

## Results

| Check | Result |
| --- | --- |
| MM3 `RelWithDebInfo` build, existing Visual Studio tree | Passed |
| xboxrecomp Python tools, MSVC x64 environment | 599 passed, 31 skipped; 57 subtests passed |
| 23 Windows native CMake regression projects | 32 tests passed, including four D3D8 smoke checks |
| Linux memory regressions and two POSIX projects | 6 tests passed |
| Shared CPU helpers | Passed, included in native total |
| Texture upscaler and frame-time analyzer self-checks | Both passed |
| MM3 worker bootstrap and generated memmove tails | Both passed |
| PVIDEO register check | Passed: both banks stop, command reads zero, restart works |
| MM3 runtime | Profile, menu, mission loading and controller-driven Washington gameplay verified |

Native projects: `kernel_regressions` (including the slow wrap check),
`memory_regressions`, `kernel_directory`, `kernel_events`, `nv2a_combiner`,
`nv2a_vsh`, `fp_precision`, `mmx_integer`, `mmx_rounding`, `fist`, `cpu_helpers`,
`adpcm_decode`, `apu_mixdown`, `idex_channel`, `kernel_audio_setting`,
`kernel_bridge`, `kernel_dispatch`, `kernel_irql_abi`, `kernel_object_paths`,
`kernel_physical_address`, `wma_decoder`, `xaudio2`.

All executed regressions pass. Python skips cover other unavailable optional
toolchains and platform paths; this is not coverage of every supported host.
Native differential conformance now runs with the installed Visual Studio 18
toolchain. Linux checks used Ubuntu 22.04 in WSL, with the runtime's existing
OpenSSL, SDL2 and epoxy development dependencies installed.

### Failures resolved

1. Default contiguous allocation intentionally reuses freed blocks. Corrected
   the stale assertion; kept that behavior.
2. Restored opt-in `RECOMP_HEAP_RECLAIM`, split reused heap blocks, and compacted
   coalesced entries so later frees still find their neighbors.
3. Extended Windows VMA now replaces the layout's owned placeholder without
   releasing it. Shutdown returns the VMA to placeholder ownership, coalesces,
   then releases the entire reservation. All four memory modes check teardown.
4. XAudio2's failure was in a stale mock: the newer callback registration had
   no mock, and the test assumed three buffers instead of 64. Updated failure
   injection and outstanding-buffer/wrap checks. No physical audio device is
   involved in this mocked test.
5. PVIDEO STOP now clears both banks, matching
   [xemu's implementation](https://raw.githubusercontent.com/xemu-project/xemu/master/hw/xbox/nv2a/pvideo.c).
6. Linux builds exposed unguarded Windows intrinsics/placeholder variables,
   missing framebuffer type visibility and missing D3D11 fallback symbols.
   Restored platform guards and POSIX atomics. Reserved enough host span for
   contiguous memory and extended VMA, and prevented 128 MB mirrors from
   replacing contiguous storage. The memory regression checks distinct RAM
   versus contiguous storage and its tiled alias in all four modes.

Windows placeholder ownership follows the documented
[VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2)
and [VirtualFree](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree)
replacement/preservation rules.

## Game evidence

Own processes only; isolated saves under
`conformance_tmp/movie_skip_20261007_180122_093/save`. Original Profile 1 and
game assets were not changed. Runtime used D3D11, USB input, render scale 2,
1280x720 window and a 60 FPS limit, with the existing offline-XNet build option
and DSP mailbox acknowledgement (`RECOMP_APU_DSP_ACK=0x80458810`). Networking
and DSP effects remain incomplete; DLC playability was not established here.

`conformance_tmp/regression_game_20261007/` contains input commands, stderr,
frame CSV and captures. The initial `capture-006.bmp` and `capture-007.bmp`
show profile selection/main menu. `final-002.bmp` shows Standard Delivery
loading; `final-004.bmp` shows the player stationary in Washington;
`final-005.bmp` shows movement and second gear after right-trigger input;
`final-007.bmp` shows the changed heading/location after steering. These are
live rendered gameplay, not attract movies. No crash occurred in this run.
The final 20-second gameplay sample had 1,201 frames, 60.0 FPS, p95/p99
16.67 ms, maximum 16.71 ms, and zero intervals above twice the median. This
short capped sample does not establish performance across every scene.

After the final platform changes, rebuilt and relaunched the executable with
SHA-256 `1B110F37250E114DB995858C1E4ADFD50513716536146641A9562AC421EF3158`.
The `verified-*` evidence records that final binary loading Standard Delivery,
responding to pause/resume, and accelerating under right-trigger input.

## Reproduce

Use the Visual Studio 2026 Developer PowerShell and its bundled CMake/CTest.
The local Python environment needs pytest, capstone, pefile, numpy and Pillow.

```powershell
Push-Location tools/xboxrecomp
..\.mm3-gen-venv\Scripts\python.exe -m pytest tools -q --basetemp=build/ownership-pytest-msvc --junitxml=build/ownership-regression-msvc-python.xml
Pop-Location

cmake -S tools/xboxrecomp/tests/<project> -B build/ownership-regression/<project> -A x64
cmake --build build/ownership-regression/<project> --config Release --parallel 4
ctest --test-dir build/ownership-regression/<project> -C Release --output-on-failure
# Add -DXBOX_SLOW_TESTS=ON when configuring kernel_regressions.

tools/.mm3-gen-venv/Scripts/python.exe tools/xboxrecomp/tools/texpack/upscale.py --selftest
python tools/xboxrecomp/tools/bench/frames.py --selftest
tools/powershell/check_worker_bootstrap.ps1
tools/powershell/check_memmove_tails.ps1 -GeneratorDir conformance_tmp/mm3_seeded_gen_20261004
tools/xboxrecomp/tools/powershell/check_pvideo_registers.ps1
cmake --build build-msvc-tailfix --config RelWithDebInfo --target mm3_recomp --parallel 4
```

Evidence: `tools/xboxrecomp/build/regression-msvc-python.xml`,
`build/ownership-regression/<project>/Testing/Temporary/LastTest.log` and
`build/regression-mm3-build.log`, and Linux `build/ownership-linux-*` test logs.
These are local ignored outputs.
