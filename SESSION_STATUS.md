# MM3 native playability — current checkpoint, 2026-10-01

## Acceptance

**Not met.** Fresh diagnostic frames at 15 and 45 seconds are black. Both cold diagnostic-off runs exit via `HalReturnToFirmware(2)`. Readable menu, textured city/car/HUD, input, audio, ten minutes driving, pause/resume/menu and two full cold acceptance runs remain unproven.

## Reproducible source and build

- Generated source: `ea348b3 fix(build): seed observed Bink callback functions`; audio build uses outer `f86666e` plus toolkit `8d3a09b` (same verified audio source committed after build).
- Toolkit: `8d3a09b fix(audio): initialize attached AC97 codec normally`, clean detached checkout; includes seed parser repair `ed03376`. Includes NEG flag repair `2c1b682`.
- Generator script: `scripts/Generate-MM3Recomp.ps1`; input `game_files/default.xbe`, SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Generated directory: `conformance_tmp/mm3_neg_flags_gen_20261001`, with 26,439 functions after two measured Bink seeds; separate official-generator SEH/ABI and memmove-tail overlays. No generated C was edited.
- Build: `& 'C:/Program Files/Microsoft Visual Studio/18/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' --build build-msvc-tailfix --config RelWithDebInfo --parallel 1`.
- Executable: `build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe`; SHA-256 `074493EC0515D00F7313679CE9B48D5D5462F3184829D7BFF627F1E83F8FBD99`. Preserved copy: `conformance_tmp/mm3_bink_seed_baseline_627e1d69.exe`.
- Generation/build logs: `conformance_tmp/mm3_bink_seed_generation_20261001.log`, `mm3_bink_seed_build_20261001.log`.
- Strict runner: `pwsh -NoProfile -File tools/powershell/run_clean_strict.ps1 -RunId <unique> -Seconds 900`; launches the executable above, clears runtime/diagnostic/recovery toggles, and hashes source/toolkit/executable. Process exit zero is not visual acceptance.

## Verified progress and evidence

1. NEG flag snapshot repair makes the guest allocator return valid memory instead of looping over zero-space free nodes. Decisive trace: `conformance_tmp/mm3_native_allocator_fixed_20261001_03.cdb.log`. Toolkit check: `python -m tools.recomp.check_neg_flags` from a compiler developer shell.
2. Main generation now declares manual wrappers separately, resolves real file-reader tail `0x000252AF`, and detects both guest SEH prologs. Commits: `ffa38c6`, `ceb5199`, `5ade0fc`.
3. Reverse memmove jumped to unregistered epilogue `0x00093B58`, leaking twelve guest stack bytes per copy. Commit `4852200` recovers all four original remainder entries. `tools/powershell/check_memmove_tails.ps1` compiles the generated tails and checks byte copies, stack and saved registers; passes. Native Bink return is now ESP `00FFFE7C -> 00FFFE84`, ESI preserved; trace `conformance_tmp/mm3_bink_wait_fixed_20261001_01.cdb.log`.
4. Runtime wording `Failed resolve VA` was invisible to the log-to-seed tool. Toolkit `ed03376` fixes that parser; its two unittest checks pass. Source `mm3_runtime_function_seeds.json` records measured callback entries `0x002F6AC0` and `0x002F9170`, both valid original Bink functions. The existing 64-instruction return probe missed these longer FPO bodies.
5. Fresh disposable xemu snapshot reached callback `0x002F9170` on thread `01` and returned to `0x002FA4DA` with ESP +4, ESI `0x280`, EBX/EDI zero and EAX equal to argument 1. Fresh native CDB matches this caller, argument structure and return contract. Logs: `conformance_tmp/mm3_bink_xemu_1384_04.probe.log`, `mm3_bink_callback_fixed_20261001_01.cdb.log`. Native guest-thread identity has not been independently mapped to xemu's thread ID; no full machine-state parity claim.

## Current failures and next actions

- Diagnostic `RECOMP_AC97_READY=1` reaches Bink decode; native images `conformance_tmp/mm3_bink_seed_visual_20261001_01_t15.png` / `_t45.png` were inspected and are black.
- First unresolved XPP input startup targets in that run: `0x00359480` and `0x00359CC8`, caller `0x003599FD`. Later null virtual calls occur inside `0x002FD1F9/23B`. Audit original functions and recover measured input entries before interpreting rendering.
- Cold runs `strict_bink_seeds_20261001_01/02` use outer `ea348b3`, toolkit `ed03376`, executable hash above. Both exit before gameplay. Strict startup previously paired native DirectSound `DSERR_NODRIVER` against xemu success: default AC'97 status is zero; the working device model is opt-in. Toolkit `8d3a09b` now initializes codec readiness and reset behavior normally. Compiled actual-helper check `python -m tools.recomp.check_ac97_registers` passes. Fresh diagnostic-off CDB `mm3_ac97_default_fixed_20261001.cdb.log` proves status bit `0x100` and DirectSound EAX zero, versus prior `0x88780078`. Cold run `strict_ac97_default_20261001_01` still exits before menu. Incremental build log: `conformance_tmp/mm3_ac97_default_build_20261001.log`. xemu reports codec-ready bit 8 as read-only on GLOB_STA reads: https://github.com/xemu-project/xemu/blob/master/hw/audio/ac97.c. GP/EP DSP remains stubbed; no forced DSP acknowledgment is used.
- Host audio output remains unavailable: XAudio2 mastering voice `0x80070490`, waveOut error 2; fresh xemu also logs `No default audio device available`. Rendering and guest initialization still have actionable source blockers, so the goal remains active.
- TDATA directory opens succeed natively; `$u/contentmeta.xbx` is absent. Read-only FATX inspection reports an empty xemu E: root. Its same-PC runtime open result remains unpaired and is not a proven cause.

## Boundaries and maintenance

- Older source comparison found vertex/GPU visibility and startup-worker EBX fixes present in clean. Historical Present-after-draw hook at guest `0x00343E60` is absent and has not been shown relevant to this current path. No dirty historical source was copied.
- Public upstream main refreshed to `1409a7d` (PR #164, observed seed trust). MM3's two aligned callback entries do not need that exception. Open PRs: 162/161/160/159/158/157/135/134/133/128. No wholesale merge. Local `upstream/main` is stale at `766ecef`.
- Existing xemu VMs, original HDD/config/EEPROM and game assets are preserved. Own probes are stopped. New xemu probe used `-snapshot` and a copied EEPROM/config.
- I: free space last measured `11,184,201,728` bytes; available RAM after disposing the own xemu probe about 11.4 GiB. One build worker. No disk cleanup this checkpoint. Old detailed chronology remains in Git history (through `31b689a`), and decisive logs/images remain in `conformance_tmp/`.
