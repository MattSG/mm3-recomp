# MM3 Session Status — 2026-09-27

## Current checkpoint

- Root `master` HEAD: `a93b2d1` (`docs(mm3): update tail recovery evidence`); runtime source: `c6dc5e1` (memmove-tail recovery). Toolkit gitlink and checked-out commit: `db0413f1274814d0f41851fa58c1ec80a6155dbe`. `git submodule status` prefixes it with `-` (uninitialized metadata), although the checkout is present and CMake builds against it. Historical tree `I:\repos\midtown-madness-3-recomp-startuptga-display` remains at `2485eda` with 235 dirty paths; it was not changed.
- XBE: `game_files/default.xbe`, SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Current verified build: `build-msvc-tailfix\RelWithDebInfo\mm3_recomp.exe`, SHA-256 `C7E6E761A0F95E3CF244293E7E35A30155D3420D321BC25415F8FBB8655FA920`. It was rebuilt from generated input `conformance_tmp/mm3_memmove_tail_fix_gen` using VS CMake 4.2.3-msvc3 / MSBuild 18.5.4:

  ```powershell
  & 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-msvc-tailfix --config RelWithDebInfo --target mm3_recomp -- /m:8
  ```

  Build log: `conformance_tmp/build_tailfix_verified_20260927.log`, SHA-256 `AB03C65113BA06C2468513606CCB1E2EB5387E2267B08A448CD6A8D1AD102284`. The generated tail source SHA-256 is `BE1BAE79AF2AAD2E78AD47B714638F4EDD98341DF933D493C5907D7A2BC70A33`. The exact generation command was not retained; do not treat the binary as a strict acceptance build.

## Runtime and acceptance

- The tail recovery reaches `sub_00093B04_gen` and returns without the previous unresolved-indirect fallback. It does not yet match xemu's partition parser outputs: at the paired return, native locals are `ebp-12=0x48000001`, `ebp-4=0`; xemu has `0`, `3`. This is the first confirmed unresolved divergence after tail recovery. No allocator or startup cause is established from it.
- No clean strict runner or strict launch command has been verified in this checkout. The historical runner is unusable as acceptance proof: it assumes absent build paths and enables diagnostics. No strict acceptance run has passed.
- No actual native screenshot/video was captured. Menu, city, vehicle, HUD, input, audio, ten-minute driving, pause/resume/menu, and two cold strict runs remain unverified. An attempted CUA capture was rejected by automatic review with `Computer Use was not approved to use mm3_recomp`; do not retry through another UI-control route.
- Historical Run10008 is diagnostic only: strict flag was set, but the runner timed out at 250 seconds and force-killed the process. Metadata says `hard_stall_confirmed=false`, `false_success=false`; it proves neither a hard stall nor visible output.

## Data and live-process boundary

- The live user xemu process is PID `32416`, using `conformance_tmp/xemu-gdb.toml`, `conformance_tmp/xbox_hdd-gdb.qcow2`, `-snapshot -boot d -S -gdb tcp::1235`. It remains untouched. The active clone SHA-256 is `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59`, different from default `tools/xemu/hdd/xbox_hdd.qcow2` (`5038F9D993B2EBF5D85BAFB196C2CE74DC4A645AF509BAB9F8F14B56661CC59`).
- Native `xbox_path_init(MM3_GAME_DIR, NULL)` maps `\\Device\\Harddisk0\\Partition1\\` to `game_files` and `T:\\` to `%LOCALAPPDATA%\\xboxrecomp\\TitleData`. The observed Partition1 TDATA title directory in `game_files` is empty. Read-only checks against the active clone found an E-partition FATX header and zero bytes at two candidate root-table offsets derived from the existing partition map. This supports an empty E root but is not yet a complete FATX directory walk or proof of guest/native path parity.
- Current evidence is in `conformance_tmp/native_path_current_20260927.err.log`, `conformance_tmp/cdb_native_partition_pair_20260927.log`, `conformance_tmp/xemu_partition_scan_20260927.gdb.log`, and `conformance_tmp/xemu_active_hdd_e_root_20260927.log`. No HDD image was modified.

## Next

Pair the post-tail partition inputs/outputs and guest file lookups; establish whether repeated native critical-section calls indicate progress; then define a clean strict launch. Visual acceptance still requires an actual inspected native capture. Keep changes committed and the worktree clean between checkpoints.
