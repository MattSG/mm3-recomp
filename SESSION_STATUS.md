# MM3 Session Status — 2026-09-27

## Source and build

- Runtime source commit `c5f7f9b`; `tools/xboxrecomp` commit `db0413f` (detached, clean). XBE `game_files/default.xbe`, SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Generator: `scripts/Generate-MM3Recomp.ps1`; current candidate `conformance_tmp/mm3_all_sections_gen_20260927_ebx`. Release `build-msvc-ebx/Release/mm3_recomp.exe`, SHA-256 `0FFB3ACFC1E89A261012E5249090AE0ABDB3538800718DF5A30CE984D6B5BEEA`; matching-symbol RelWithDebInfo `build-msvc-ebx/RelWithDebInfo/mm3_recomp.exe`, SHA-256 `D1FC5EEF6CDC855AE7E2F310B75FB6F5DAD57C7AEAE241E62E5DB0320203DF67`.
- Build command: VS 18 CMake 4.2.3, `cmake --build build-msvc-ebx --config RelWithDebInfo --target mm3_recomp -- /m:8`. Clean checkout has no `tools/powershell/run_strict.ps1` or `build_limited.ps1`; no strict acceptance run or valid strict launch command yet.

## Acceptance state

- No readable native menu, city, vehicle, or HUD capture; no verified input/audio, ten-minute drive, pause/resume/return, or two cold strict runs. Run11199 proves four authentic Startup.tga Presents only. The inspected `conformance_tmp/xemu_reference_fresh_20260927.png` is black (`DC074CD781A6DAF139BDE9F128AAACBB5AA477D2082561DF70FA0B501E8A82CB`).
- Earlier data-path boundary remains unaligned: native `T:` maps to `%LOCALAPPDATA%\xboxrecomp\TitleData`; at caller `0x86D34`, native open returns `C000003A`. Xemu’s first captured path was `T:\$u\contentmeta.xbx`, also not found; same-thread/call parity is unproven. Native trace `conformance_tmp/native_path_map_20260927.err.log`, SHA-256 `860711436BD22AA3E3B8B8DC14EA4D5EE9AC0AD6E7E84652C7F8D044E20DC37B`; xemu trace `conformance_tmp/xemu_multi_file_20260927.gdb.log`, SHA-256 `FDE2C7C08504E0274097B3FC0FF56D300D30A8E11035E72E70F3F414B66BD758`.

## Current allocator evidence

- Xemu and native reach guest allocator PC `0x858F3` through `sub_00043E62`, outer return `0x43E7C`, with flags `0` and size `0x18`. Heap args differ (`0x00490000` vs `0x01001000`) because `sub_00083D49` reads global `0x46A154`.
- Both create that global through the same `sub_000854CF` call at `0x871C8`, return `0x87216`, args `(2, 0, 0x100000, 0x1000, 0, out_ptr)`. Returned bases differ by relocation; first 96 bytes of heap metadata match after normalizing pointers, and first node is base `+0x180`. This is not a proven cause.
- After `sub_00094FC0` at guest `0x85902`, ESI/EDI and frame-relative locals match; EDX differs and liveness is unproven. A separate native hit reaches the allocator comparison with request `0x13A`, size-3 node, and caller chain `sub_00257718 → sub_0003FA12 → sub_00043E62`; the generated caller sets global `0x3C5F98` before entering `sub_0003FA12`. The earlier `0x13D` hit at `0x85C6A` is a distinct later state. No matching xemu `0x13A` call/state or proven cause yet; no allocator fix is justified.
- Paired evidence: `xemu_heap_global_init_20260927.log` (`79DC94D972FD950BA0A7C54BAC875ACD6FC6BAF831AF6378893C25F5803ABA80`), `native_heap_create_call_20260927.cdb.log` (`E58E49E1C3E0BE7EDE847D10914172BE6A0816AF709401E9AF8A24BCE7448DC3`), `xemu_after_seh_20260927.log` (`DA704BEC99F71403F64860741FD7907ECFFD6B26C21B11F899FAE8B50DA7C8FA`), `native_matched_entry_20260927.cdb.log` (`98243F8B74E1597EA2FF5008A950E47284305DEBCC66204359FF65FB623D58EE`). Cold xemu launch: `tools/xemu/xemu.exe -config_path conformance_tmp/xemu-reference-ascii-20260927.toml -snapshot -boot d -S -gdb tcp::1237`; owned clone stopped after capture; user PID `32416` untouched.

## Disk and next action

- I: free `24,475,258,880` bytes; `conformance_tmp` `54,988,754,879` bytes; `build-msvc-ebx` `481,892,135` bytes. No cleanup yet; preserve game assets, HDD snapshots, decisive traces, and current failure. Reclaim only identified duplicates or superseded generated output.
- Next: capture the native `0x13A` request’s exact guest caller and free-list state in xemu; identify first non-relocation divergence, then fix/build/strict-run. Find or establish a clean-checkout strict launch path and capture the actual native window before claiming visual progress.
