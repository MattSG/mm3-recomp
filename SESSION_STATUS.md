# MM3 Session Status — 2026-09-27

## Build identity
- Root runtime source base: `17134da`; toolkit: `a5d9a30`. XBE `game_files/default.xbe`, SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Generator: `scripts/Generate-MM3Recomp.ps1`; default generator dir `conformance_tmp/mm3_targeted_gen`; `-TraceFunctions` is opt-in. Current candidate used `conformance_tmp/mm3_all_sections_gen_20260927_ebx`.
- Build: VS 18 CMake 4.2.3, `cmake --build build-msvc-ebx --config Release --target mm3_recomp`. Candidate `build-msvc-ebx/Release/mm3_recomp.exe`, SHA-256 `A20DC83CEAB98D67B48EC99042BD137A4E6AE65A38A7F4E3F7012D4968D58614`. Pre-trace failure preserved at `conformance_tmp/mm3_recomp_pre_path_map_20260927.exe`, SHA-256 `F7C8E2B13F3CB0EB197687FC3556E7EE8CD50E179A224E0AB214D1F650FE62C5`.
- No strict runner (`tools/powershell/run_strict.ps1` is absent); no acceptance run. Release build and bounded probes are not acceptance proof.

## Current evidence
- No readable recomp menu or gameplay capture. Inspected xemu window capture is black: `conformance_tmp/xemu_reference_fresh_20260927.png`, SHA-256 `DC074CD781A6DAF139BDE9F128AAACBB5AA477D2082561DF70FA0B501E8A82CB`. Run11199 proves four Startup.tga Presents with `draw>0` only. Run10008 and Run11201 remain UNKNOWN for visible output.
- Current HDD clone SHA-256 `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59`: C has `xboxdash.xbe`; XODash is absent; E root/TDATA is empty. Native TDATA folders are also empty.
- Native trace proves `\Device\Harddisk0\Partition1\TDATA` maps to `game_files\TDATA`; path source maps `T:` to `%LOCALAPPDATA%\xboxrecomp\TitleData`. First native file request has null object name and returns `C000003A` at caller return `0x86D34`. Next opens map DVD `dashupdate.xbe` to `game_files\dashupdate.xbe` successfully, then map system `xboxdash.xbe` to missing `game_files\xboxdash.xbe` (`C0000034`). Trace `conformance_tmp/native_path_map_20260927.err.log`, SHA-256 `860711436BD22AA3E3B8B8DC14EA4D5EE9AC0AD6E7E84652C7F8D044E20DC37B`.
- Xemu’s first captured file path is `T:\$u\contentmeta.xbx` and returns `C000003A`, caller `sub_00296A61` return `0x00296ABA`; capture `conformance_tmp/xemu_multi_file_20260927.gdb.log`, SHA-256 `FDE2C7C08504E0274097B3FC0FF56D300D30A8E11035E72E70F3F414B66BD758`. Same guest thread/call alignment with native null-name request is not proven.
- At xemu dispatch PC `0x87347`, callback slot contains and calls `0x0029713B`; native trace calls `sub_00296E94` from `0x00297138`. Same callback/thread is not established. Capture `conformance_tmp/xemu_callback_table_20260927.gdb.log`, SHA-256 `CF570C7CA61B8399C78EE1D76FE36648136AF14B7F2F40417B108F69688C9A4D`.
- Verified clean toolkit already contains startup-worker EBX preservation (`0x001E7B41`) and immediate-mode vertex drawing. Clean root still lacks historical deferred-Present-after-body fix `c93792a`; revisit after startup reaches guest rendering.

## Disk and next action
- I: free `31.22 GB`; `conformance_tmp` totals `41.53 GB`. Two 8 GB raw HDD snapshots differ by SHA-256 and remain preserved; no scratch cleanup performed this turn.
- Next: capture native `OBJECT_ATTRIBUTES` fields for the null-name request, align its caller/thread with xemu, then fix the first evidenced divergence. Keep all traces opt-in; no runtime cause is established yet.
