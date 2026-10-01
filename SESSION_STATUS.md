# MM3 native playability — 2026-10-01

## Acceptance

Not met. Latest native window is black at15sec and shows a dark, distorted vertical image at45/90sec. Menu, textured city/car/HUD, input, audio, ten minutes driving, pause/resume/menu and two full cold acceptance runs remain unproven.

## Current identity and reproducible commands

- Source integration commit `3a5d4cf`; seeds `e9611ef`; toolkit `00d2c2b0c80582305430ddf2ce2e445976f87e08`. This status-only checkpoint follows the integration commit. Toolkit checkout clean.
- Executable `build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe`, SHA256 `60D225B64099C57E1265A9E17069BEEBDD97C1A85DEA18ED20169C3756D477B2`. Runtime-only build completed with exit0; log `conformance_tmp/mm3_pvideo_build_20261001.log`. Toolkit `e1ad75f` adds PVIDEO scanout; precise precommit source hashes in `conformance_tmp/mm3_pvideo_source_identity_20261001.json`. Generated C unchanged.
- XBE `game_files/default.xbe`, SHA256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Build-time HEADs plus exact modified-source and analysis-input hashes: `conformance_tmp/mm3_bink_yuy2_source_identity_20261001.json`. Build used four project seeds plus two separately recorded BINKYUY2 candidates. The committed six-entry project seed list now contains the same union; no executable regenerated merely to change commit metadata.
- Official analysis: from `tools/xboxrecomp`, venv Python `../../conformance_tmp/mm3genvenv/Scripts/python.exe -m tools.disasm ../../game_files/default.xbe --force --seed-functions ../../mm3_runtime_function_seeds.json`, then `-m tools.func_id` and `-m tools.abi_analysis` with the same XBE.
- Official main generation: same Python, `-m tools.recomp ../../game_files/default.xbe --all --split 1000 --gen-dir ../../conformance_tmp/mm3_neg_flags_gen_20261001 --manual-functions ../../conformance_tmp/mm3_neg_flags_gen_20261001/manual_functions.json --skip-binary-check`; copy project `src/recomp/gen_trace/recomp_types.h` afterward. 26,443 functions. Analysis/generation logs prefixed `mm3_bink_yuy2_` in conformance_tmp.
- Full overlay regeneration entrypoint `scripts/Generate-MM3Recomp.ps1`. Current main regenerated officially; existing SEH/ABI/memmove overlays retained and do not use the new CPU instructions. No generated body edited.
- Build: VS18 Enterprise `Launch-VsDevShell.ps1 -Arch amd64 -HostArch amd64 -SkipAutomaticLocation`, then bundled CMake `--build build-msvc-tailfix --config RelWithDebInfo --parallel 1`; ABI_CHECK off.
- Strict runner: `pwsh -NoProfile -File tools/powershell/run_clean_strict.ps1 -RunId <unique> -Seconds 900`. Latest bounded run `strict_pvideo_20261001_01`,45sec,timeout, outer471473a/toolkite1ad75f/exe60D225B6; metadata under `conformance_tmp/clean-strict/`. Controlled save root; no diagnostic/recovery toggles. This is not an acceptance pass.
- Controlled save root `MM3_SAVE_DIR=I:/repos/midtown-madness-3-recomp-clean/conformance_tmp/mm3_ac97_writable_savedata_20261001`. Default-save sandbox run previously failed on denied LocalAppData TitleData writes. External saves preserved. FATX audit found xemu E: empty; paired contentmeta open remains unproven.

## Verified progress

- Toolkit `2c1b682`: NEG snapshots fix allocator traversal; `tools.recomp.check_neg_flags`; native trace `mm3_native_allocator_fixed_20261001_03.cdb.log`.
- Outer `ffa38c6/ceb5199/5ade0fc/4852200`: manual declaration separation, file-reader tail252AF, both SEH prologs and reverse memmove epilogues. Compiled tail check passes; `mm3_cpu_tail_check_20261001.log`.
- Outer `ea348b3`, toolkit `ed03376`: original Bink callbacks2F6AC0/2F9170 recovered; paired native/xemu callback contracts match. Traces `mm3_bink_xemu_1384_04.probe.log`, `mm3_bink_callback_fixed_20261001_01.cdb.log`.
- Toolkit `8d3a09b`, outer `7a98640`: AC97 ready/reset behavior is default. Native DirectSound return changes88780078->0; `mm3_ac97_default_fixed_20261001.cdb.log`.
- Outer `e9611ef`: XPP359480/359CC8 and original BINKYUY2 converters307AE0/307C70 seeded from observed runtime targets. Both converters pass compiled two-row YUY2 output and stack/register checks at widths8/16/32/64; `mm3_bink_yuy2_check_20261001.log`.
- Toolkit `00d2c2b`: CPUID and EFLAGS handling execute original93770 probe. Before, no-op PUSHFD/POPFD/CPUID left capability46E580=0 and dispatch3BFFC0/3BFFD8 null. After, native probe EAX1, EDX383F9FD, cache393004=1/capability1; setup stores307AE0/307C70. Native traces `mm3_cpu_native_probe_20261001_02.cdb.log`, `mm3_cpu_bink_setup_20261001.cdb.log`.
- Measured original xemu CPUID at9379E, thread01: leaf1 EAX68A/EDX383F9FD. Leaf0/2 and invalid-leaf clamping measured; hypervisor40000000/1 zero,40000002 leaf2. Device-query register state restored; no game cache forced. Logs `mm3_cpu_xemu_1385.probe.log`, `.leaves.log`, `.hypervisor.log`.
- Original-probe compiled check and 24 carry/POPF checks pass: `mm3_cpu_probe_check_20261001_02.log`, `mm3_cpu_flag_unit_20261001_02.log`. PUSHFD preserves live RF/VM while clearing its image/reserved bits. Known CMP/TEST/logical/POPF provenance supported; other provenance remains explicitly untranslated.
- Native307C70 matches original guest call2FD23B/countA0, both sampled output rows10801080 repeated, saved registers and ESP+8. Native ESP FFFD6C->FFFD74; xemu D00B4B54->D00B4B5C. Logs `mm3_bink_yuy2_native_20261001.cdb.log`, `mm3_bink_yuy2_xemu_1385.probe.log`. Native guest-thread mapping remains unproven.

## First current unresolved boundary

- Actual captures `mm3_bink_yuy2_visual_20261001_t15.png`, `_t45.png`, `_t90.png` inspected: all black. Run identity `mm3_bink_yuy2_visual_20261001.run.json`; diagnostic/recovery toggles cleared.
- Video copy2F5DE0 returns0 ->1F29C3 ->1F2A71 ->1DB6E8. Corrected return-chain probe clears recurring breakpoint; enclosing1DB6E8 does not return within30 seconds. `mm3_bink_yuy2_return_chain_20261001_02.cdb.log`; earlier chain's last label is a recurring breakpoint, not a return.
- Real movie1F2A71 entries/returns advance handle+0C 1->2->3, +10 0->1->2; width280,height1E0,total90,rate1E. `mm3_movie_frame_state_20261001.cdb.log`. This proves early movie progression, not visible presentation or eventual exit.
- Real original342F20 returns after programming PVIDEO: BUFFER700=1, STOP704=0, OFFSET920=D2A000, SIZE_IN928=SIZE_OUT950=1E00280, POINT_IN/OUT0, DS_DX/DT_DY100000, FORMAT958=10500 (YUY2,pitch500). Trace mm3_pvideo_register_state_20261001.cdb.log.
- GPU aperture is plain RAM (xbox_memory_layout.c:85,2307); pvideo_write breakpoint does not fire in20sec, consistent with mapped stores bypassing that model. fb_present.c reads primary framebuffer only. Existing YUY2 texture converters do not compose this hardware overlay.
- Toolkit `e1ad75f` scans actual PVIDEO YUY2 over the primary framebuffer. Reuses contiguous arena address classification; bounds/crop/scale/color-key/STOP checks; no BUFFER/STOP/IRQ writes. Compiled check `tests/pvideo_scanout.c` passes. Nearest sampling is documented; hardware register interrupt semantics remain unimplemented.
- Real bounds probe `mm3_pvideo_bounds_20261001_02.cdb.log`: BASE0,LIMIT07FFFFFF,LUMINANCE/CHROMINANCE1000; allocated physical range490000..DC0000 contains the full D2A000+96000 buffer. Earlier20sec probe timed out;45sec probe reached original342F20 return.
- Latest cold visual run `mm3_pvideo_visual_20261001.run.json`, PID31356 stopped after90sec, diagnostic/recovery toggles cleared, controlled save root. Actual `_t15.png` black; `_t45.png` and `_t90.png` inspected: dark red/gray vertical strip, no recognizable menu or movie. This is a scanout improvement, not acceptance. Next compare full YUY2 frame and movie progression with xemu at matching guest PC/thread; source decode, texture/primary drawing and eventual movie exit remain unresolved.
- Original xemu hardware reference: https://raw.githubusercontent.com/xemu-project/xemu/master/hw/xbox/nv2a/pvideo.c and pgraph/gl/display.c. Owned snapshot capture `mm3_pvideo_reference_20261001.png` is black at its debug stop, so not a running visual reference.
- Resumed owned xemu without changing guest registers: `mm3_pvideo_xemu_resume_20261001_03.log`; inspected `mm3_pvideo_reference_running_20261001.png` shows recognizable building/sky/city movie. Subsequent real1F2AE7/thread01 snapshot `mm3_pvideo_xemu_frame_20261001.log`: later movie totalC2A/frame1BA, full actual YUY2 saved. It is not paired with native first-movie frame10. Two initial RSP attempts rejected checksums because imported helper omitted packet payload; corrected in disposable probe.
- Native original342F20 return at first-movie frame10 verified, actual OFFSETC94000, full buffer saved: `mm3_pvideo_frame10_20261001_02.cdb.log`, `mm3_pvideo_native_frame10_20261001.yuy2`. Native35sec attach after normal execution: `mm3_pvideo_late_state_20261001.cdb.log`; first movie total90, frame8F, previous8E, BUFFER1/STOP0, sameC94000. This narrows the next investigation to final decode/progression/exit; does not prove a specific stalled call.
- Conditional342F20 frame143 probe did not reach target in50sec (`mm3_pvideo_movie_end_20261001.cdb.log`). Conditional2F5E20 final-frame probe also requires interpretation; Luna read-only audit of2F5E20/2F6430 underway. Avoid claiming their enclosing calls returned. Next get deep late worker stacks and pair the relevant original branch.
- Host audio endpoint absent: XAudio2 80070490/waveOut2; xemu also lacks a default endpoint. APU DSP remains stubbed; no forced DSP acknowledgement. Rendering blocker remains actionable, goal active.

## Maintenance

- Upstream refreshed after CPU milestone: sp00nznet/xboxrecomp main1409a7d; open PRs159/162/158/128/161/160/157/135/134/133. No wholesale merge. Existing vertex and startup-worker EBX fixes confirmed present.
- Source-only commits exclude assets/XBE/QCOW2/logs/generated outputs. Git checkpoint requests3280/3335 cancelled and completed through normal Git commands; no pending Git approval. No owned native probe or build remains live.
- Existing VMs/assets/original HDD/config/EEPROM preserved. Owned snapshot PID31064, copied EEPROM/config, port1385, `-snapshot`, still live; stop approval cell3320 pending. Do not terminate other xemu instances.
- I: last free11,035,631,616 bytes. One build worker used; no disk cleanup. Reviewed CPU baseline preserved at `conformance_tmp/mm3_cpu_reviewed_baseline_4833928d.exe`; decisive traces retained.
