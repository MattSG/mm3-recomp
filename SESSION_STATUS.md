# MM3 Session Status — 2026-09-27

## Current checkpoint

- Root `master` HEAD: `b73b956` (`docs(mm3): record parser table mismatch`); runtime source: `c6dc5e1` (memmove-tail recovery). Toolkit gitlink and checked-out commit: `db0413f1274814d0f41851fa58c1ec80a6155dbe`. `git submodule status` prefixes it with `-` (uninitialized metadata), although the checkout is present and CMake builds against it. Historical tree `I:\repos\midtown-madness-3-recomp-startuptga-display` remains at `2485eda` with 235 dirty paths; it was not changed.
- XBE: `game_files/default.xbe`, SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Current verified build: `build-msvc-tailfix\RelWithDebInfo\mm3_recomp.exe`, SHA-256 `C7E6E761A0F95E3CF244293E7E35A30155D3420D321BC25415F8FBB8655FA920`. It was rebuilt from generated input `conformance_tmp/mm3_memmove_tail_fix_gen` using VS CMake 4.2.3-msvc3 / MSBuild 18.5.4:

  ```powershell
  & 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-msvc-tailfix --config RelWithDebInfo --target mm3_recomp -- /m:8
  ```

  Build log: `conformance_tmp/build_tailfix_verified_20260927.log`, SHA-256 `AB03C65113BA06C2468513606CCB1E2EB5387E2267B08A448CD6A8D1AD102284`. The generated tail source SHA-256 is `BE1BAE79AF2AAD2E78AD47B714638F4EDD98341DF933D493C5907D7A2BC70A33`. The exact generation command was not retained; do not treat the binary as a strict acceptance build.

## Runtime and acceptance

- The tail recovery reaches `sub_00093B04_gen` and returns without the previous unresolved-indirect fallback. In the tail-fixed paired run, native and xemu both read the `0x97315286` sector magic, count `2`, and title ID `0x4D53002A`; native still returns locals `ebp-12=0x48000001`, `ebp-4=0`, while xemu has `0`, `3`. At the parser return, native global table `0x361FD4` points to `0x007404a0` (count `3`, three empty entry slots); xemu points to `0x80012340` (count `3`, populated records). This is a concrete preexisting table-state mismatch to trace; it does not establish an allocator or startup cause.
- No clean strict runner or strict launch command has been verified in this checkout. The historical runner is unusable as acceptance proof: it assumes absent build paths and enables diagnostics. No strict acceptance run has passed.
- No actual native screenshot/video was captured. Menu, city, vehicle, HUD, input, audio, ten-minute driving, pause/resume/menu, and two cold strict runs remain unverified. An attempted CUA capture was rejected by automatic review with `Computer Use was not approved to use mm3_recomp`; do not retry through another UI-control route.
- Historical Run10008 is diagnostic only: strict flag was set, but the runner timed out at 250 seconds and force-killed the process. Metadata says `hard_stall_confirmed=false`, `false_success=false`; it proves neither a hard stall nor visible output.
- Historical Run11201/Run11204 are diagnostic only. Their 34 `[FILE]` and 17 `[QFILE]` events match by path/status/arguments (ignoring host file handles); the first returned-field difference is the third Data_hd.zip class-34 query (last dword `01000000` vs `00F7FDD4`, same status and EOF). The runs used different binaries and Run11204 lacks Run11201's parent-call trace, so this is an observed divergence, not a causal explanation.

## Data and live-process boundary

- The live user xemu process is PID `32416`, using `conformance_tmp/xemu-gdb.toml`, `conformance_tmp/xbox_hdd-gdb.qcow2`, `-snapshot -boot d -S -gdb tcp::1235`. It remains untouched. The active clone SHA-256 is `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59`, different from default `tools/xemu/hdd/xbox_hdd.qcow2` (`5038F9D993B2EBF5D85BAFB196C2CE74DC4A645AF509BAB9F8F14B56661CC59`).
- Native `xbox_path_init(MM3_GAME_DIR, NULL)` maps `\\Device\\Harddisk0\\Partition1\\` to `game_files` and `T:\\` to `%LOCALAPPDATA%\\xboxrecomp\\TitleData`. The native trace opens `Partition1\\TDATA`, `TDATA\\4d53002a`, `UDATA\\4d53002a`, and its three title files successfully; `game_files\\TDATA\\4d53002a` is empty while UDATA `TitleMeta.xbx` is 60 bytes.
- An owned xemu clone (PID 46324, port 1237; stopped after capture) using the same base HDD with `-snapshot` returned success opening guest `Partition1\\TDATA`, `TDATA\\4d53002a`, and `UDATA\\4d53002a\\TitleMeta.xbx`. `NtQueryInformationFile` class 34 then returned a 60-byte size for TitleMeta, matching the native host file. This supports open/metadata parity; exact file payload parity and the guest E: to host mapping are still unproven.
- Correct E FATX geometry gives cluster size `0x4000`, 16-bit FAT entries, FAT size `0x11950`, and root cluster 1 data offset `0xABE92950`. A read-only full-cluster check against base HDD SHA-256 `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59` found that base root cluster zero-filled and cluster 1 marked EOC in the FAT. Since the clone used `-snapshot`, this base-image check does not describe its guest-visible overlay state. No HDD image was modified.
- Evidence logs: `conformance_tmp/native_path_current_20260927.err.log`, `conformance_tmp/cdb_native_partition_pair_20260927.log`, `conformance_tmp/xemu_data_parity_probe_20260927.gdb.log` (SHA-256 `DAD8894C79DF5F21E31D651210AE20AF7310089D08E8EF6EF35E2E80F2F0B8C4`), `conformance_tmp/xemu_titlemeta_read_20260927.gdb.log` (SHA-256 `C46A942DCD1237A30E7A7CCBDFFC76685FA5E0E3A3271E996D67D531103CE4FD`), and corrected `conformance_tmp/xemu_e_root_cluster_20260927.log` (SHA-256 `2A160E9DED4739DE4EB0F334A024E7BB025D9129FEBD4F906D48AEBFAF3B1424`).

## Next

Trace the first parser table writer/populator at `0x361FD4`, then compare post-tail partition inputs/outputs and guest file payloads; establish whether repeated native critical-section calls indicate progress; then define a clean strict launch. Visual acceptance still requires an actual inspected native capture. Keep changes committed and the worktree clean between checkpoints.
