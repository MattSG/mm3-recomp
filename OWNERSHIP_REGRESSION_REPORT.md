# Repository ownership and regression check

2026-10-07. Root baseline: `e93442c`. Toolkit baseline: `e3674f3`.
Toolkit ownership commit: `cc9388f`.

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
| xboxrecomp Python tools, MSVC x64 environment | 597 passed, 33 skipped; 57 subtests passed |
| 22 native CMake regression projects | 24 tests passed, 4 failed |
| Shared CPU helpers | Passed, included in native total |
| Texture upscaler and frame-time analyzer self-checks | Both passed |
| MM3 worker bootstrap and generated memmove tails | Both passed |
| PVIDEO register check | Compiled; assertion failed |

Native projects: `kernel_regressions` (including the slow wrap check),
`memory_regressions`, `kernel_directory`, `kernel_events`, `nv2a_combiner`,
`nv2a_vsh`, `fp_precision`, `mmx_integer`, `mmx_rounding`, `fist`, `cpu_helpers`,
`adpcm_decode`, `apu_mixdown`, `idex_channel`, `kernel_audio_setting`,
`kernel_bridge`, `kernel_dispatch`, `kernel_irql_abi`, `kernel_object_paths`,
`kernel_physical_address`, `wma_decoder`, `xaudio2`.

This is not a clean regression pass or gameplay validation. Python skips
include unavailable platform/compiler paths. POSIX-only native projects and
the separate D3D8 smoke executables were not run. No game process or saves
were touched.

### Failures retained

1. `memory_regressions_default`: expects contiguous blocks never to be reused;
   this fork reuses them by default.
2. `memory_regressions_heap_reclaim`: expects the upstream opt-in heap splitting,
   coalescing and decommit behavior. This fork's `xbox_HeapReclaimEnabled()`
   returns zero; six checks fail. Whole-block heap reuse and disabled release
   handling account for these failures.
3. `memory_regressions_ext_vma`: cannot reserve its upper guest range, falls
   back to a different address and then crashes when the harness touches its
   requested address. The new Windows placeholder span includes the gap above
   the RAM mirrors, while `guest_vmem_init()` requires that gap to be free.
4. `xaudio2_test`: `XAudio2Create` returns `0x80004005`; the test then crashes.
   Audio availability in this execution environment was not established.
5. PVIDEO: STOP clears bit 0 of `BUFFER`, leaving bit 4 set for input `0x11`;
   the check expects the entire value to become zero. Hardware semantics need
   verification before changing runtime behavior or the assertion.

The cleanup did not change these runtime functions. Their failures must not
be represented as successful regressions.

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

Evidence: `tools/xboxrecomp/build/ownership-regression-msvc-python.xml`,
`build/ownership-regression/<project>/Testing/Temporary/LastTest.log` and
`build/ownership-mm3-build.log`. These are local ignored outputs.
