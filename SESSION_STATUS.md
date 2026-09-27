# Current checkpoint — 2026-09-27

- Root HEAD `205d8d1`; nested `tools/xboxrecomp` HEAD `db0413f`; both worktrees are clean after this checkpoint commit.
- Translator fix refreshes derived `SEH_HELPERS` after late helper detection. Regression passed: `python3 -m unittest tools.recomp.test_translator_seh_helpers`. Full generation completed: 26,400/26,437 functions translated, 0 failed. Fresh `0x858F3` and `0x860AA` overlays read back `ebp = g_seh_ebp` after `0x94FC0`.
- Release candidate `build-msvc-ebx/Release/mm3_recomp.exe`, SHA-256 `0FFB3ACFC1E89A261012E5249090AE0ABDB3538800718DF5A30CE984D6B5BEEA`. Matching-symbol RelWithDebInfo build `build-msvc-ebx/RelWithDebInfo/mm3_recomp.exe`, SHA-256 `D1FC5EEF6CDC855AE7E2F310B75FB6F5DAD57C7AEAE241E62E5DB0320203DF67`.
- Temporary trace in the ignored generated file captured `sub_000858F3_gen` allocator scans. At `0x85C3F`, request `0x13D` matched a block of size `0x13D`, node `0x01002C38`, next `0x01001180`. At `0x85C6A`, the new chunk was `0x01002C30`, so its link node `chunk+8` aliased the old node `0x01002C38`; the insertion writes made the node self-linked. The next request `0x13A` sees size `3` and next `0x01002C38` repeatedly. This explains the spin; whether the alias is caused by guest state or translation is unresolved. Temporary instrumentation was removed, the symbol build was refreshed, and our diagnostic PID 45160 was stopped.
- No strict acceptance run, recognizable menu/gameplay, or screenshot has been achieved. Available CUA returned no apps or browsers and reports native APIs disabled. `RECOMP_UNIMPL_TRAP=1` is a debug guard, not strict-mode proof; clean checkout has no historical strict runner.
- Do not touch user xemu PID 32416. PID 52848 is our stopped clone; its earlier allocator call differs from the native caller/size and is not parity evidence.
- Next: trace the `0x85C6A` caller arguments and compare the same free-list transition in xemu, then make an evidenced fix. Disk free was last measured at 22.8 GB.
---
# MM3 Session Status — 2026-09-27

## Build identity
- Root runtime source base: `17134da`; toolkit: `3b52c93`. XBE `game_files/default.xbe`, SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Generator: `scripts/Generate-MM3Recomp.ps1`; default generator dir `conformance_tmp/mm3_targeted_gen`; `-TraceFunctions` is opt-in. Current candidate used `conformance_tmp/mm3_all_sections_gen_20260927_ebx`.
- Build: VS 18 CMake 4.2.3, `cmake --build build-msvc-ebx --config Release --target mm3_recomp`. Candidate `build-msvc-ebx/Release/mm3_recomp.exe`, SHA-256 `A2EF77DC715AA68C5B4C649E3189177CB07E11C434CF27588F52135973977C09`. Pre-trace failure preserved at `conformance_tmp/mm3_recomp_pre_path_map_20260927.exe`, SHA-256 `F7C8E2B13F3CB0EB197687FC3556E7EE8CD50E179A224E0AB214D1F650FE62C5`.
- No strict runner (`tools/powershell/run_strict.ps1` is absent); no acceptance run. Release build and bounded probes are not acceptance proof.

## Current evidence
- No readable recomp menu or gameplay capture. Inspected xemu window capture is black: `conformance_tmp/xemu_reference_fresh_20260927.png`, SHA-256 `DC074CD781A6DAF139BDE9F128AAACBB5AA477D2082561DF70FA0B501E8A82CB`. Run11199 proves four Startup.tga Presents with `draw>0` only. Run10008 and Run11201 remain UNKNOWN for visible output.
- Current HDD clone SHA-256 `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59`: C has `xboxdash.xbe`; XODash is absent; E root/TDATA is empty. Native TDATA folders are also empty.
- Native trace proves `\Device\Harddisk0\Partition1\TDATA` maps to `game_files\TDATA`; path source maps `T:` to `%LOCALAPPDATA%\xboxrecomp\TitleData`. At guest caller `0x86D34`, the bridge receives `OBJECT_ATTRIBUTES=0x40` and returns `C000003A`. Generated code at that caller appears to construct the attributes at `ebp-16` (root 0, name pointer `ebp-44`, attributes `0x40`), so the generated call and bridge argument disagree; cause is not established. Later opens map DVD `dashupdate.xbe` successfully, then map system `xboxdash.xbe` to missing `game_files\xboxdash.xbe` (`C0000034`). Path trace `conformance_tmp/native_path_map_20260927.err.log`, SHA-256 `860711436BD22AA3E3B8B8DC14EA4D5EE9AC0AD6E7E84652C7F8D044E20DC37B`; request trace `conformance_tmp/native_file_request_20260927.err.log`, SHA-256 `DAC43A3F75486DA778B09BE6206A1D57CF05E8984DC1E7FD0C9DBEF82184B783`.
- Xemu’s first captured file path is `T:\$u\contentmeta.xbx` and returns `C000003A`, caller `sub_00296A61` return `0x00296ABA`; capture `conformance_tmp/xemu_multi_file_20260927.gdb.log`, SHA-256 `FDE2C7C08504E0274097B3FC0FF56D300D30A8E11035E72E70F3F414B66BD758`. Same guest thread/call alignment with native null-name request is not proven.
- At xemu dispatch PC `0x87347`, callback slot contains and calls `0x0029713B`; native trace calls `sub_00296E94` from `0x00297138`. Same callback/thread is not established. Capture `conformance_tmp/xemu_callback_table_20260927.gdb.log`, SHA-256 `CF570C7CA61B8399C78EE1D76FE36648136AF14B7F2F40417B108F69688C9A4D`.
- Verified clean toolkit already contains startup-worker EBX preservation (`0x001E7B41`) and immediate-mode vertex drawing. Clean root still lacks historical deferred-Present-after-body fix `c93792a`; revisit after startup reaches guest rendering.

## Disk and next action
- I: free `31.22 GB`; `conformance_tmp` totals `41.53 GB`. Two 8 GB raw HDD snapshots differ by SHA-256 and remain preserved; no scratch cleanup performed this turn.
- Next: trace the generated ICALL stack through `RECOMP_ICALL_SAFE` and compare it with the bridge's `STACK_ARG` base; then align caller/thread with xemu. Fix only the evidenced divergence. Keep tracing opt-in; no runtime cause is established yet.
