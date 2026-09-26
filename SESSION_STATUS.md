# MM3 Session Status - 2026-09-26

## Current source/build identity
- Outer source checkpoint: b38c895e0df97a1ab249e0be4653c798a78bc6cd (first outer commit; source-only files and toolkit gitlink; game assets, logs, build outputs and generated C excluded).
- Toolkit: 01db629c151a6c00afc9df2b8909e755a8fec908; local tracked edits remain in CMakeLists.txt, src/kernel/kernel_bridge.c, src/kernel/kernel_hal.c, src/kernel/nv2a_pb_exec.c, src/kernel/xbox_memory_layout.c, src/usb/ohci.c.
- Generator input: game_files/default.xbe, SHA-256 2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79; script scripts/Generate-MM3Recomp.ps1; CMake RECOMP_GEN_DIR=conformance_tmp/mm3_targeted_gen (45 C files); CMake cache SHA-256 F3EA6BD4729EF26871D7063C4E7D43F5456E4D500B64616F33EC2FA51467AEB2.
- Build command succeeded: C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe --build build-msvc-frontier --config Release --target mm3_recomp.
- Executable: build-msvc-frontier/Release/mm3_recomp.exe, SHA-256 862EC6FE857A111CAAEC2AB5C7B9D1EE26C8B3219BB80ABB59C0EB91B3862477. PATH CMake 4.0.2 cannot create the VS 18 generator; VS-bundled CMake 4.2.3 succeeds.
- Strict launch command: not established. Current CDB evidence direct-launched the executable; no run_strict.ps1 exists in this checkout. Diagnostic toggles and acceptance mode still need explicit verification.

## Verified visible/runtime result
- No playable milestone. Inspected conformance_tmp/m4_live_desktop_20260926.png (SHA-256 CAF0DFE1262142386510C20070AA3A85ACDA7049DCF36D46B991E137AE79E1D7): desktop capture with no visible game window.
- conformance_tmp/m4_present_20260926.err.log ends in access violation during startup after D3D setup. CDB logs show host mm3_recomp+0x69d154 executing mov [rdx+rax],ecx, with rdx=0x7edb85d8, rax=0x10000; invalid write 0x7edc85d8. Guest trace context includes sub_0016FF04 called from sub_001BFA12. Exact guest write origin/cause remains unproven.
- Requested Run10008/11199/11201/11204 evidence not located by this checkout's current FFF index; do not treat older splash Present evidence as menu/gameplay proof.

## Next action / retained evidence
- Re-run current built executable with diagnostic and recovery toggles explicitly off, identify exact guest PC for the invalid host write, and verify input/data parity against xemu. Keep paired logs and screenshots.
- Xemu HDD vs recomp title data mapping is unresolved. No artifact cleanup yet; preserve current traces and existing build/output trees.
- I: free space at note time: 32.5 GiB.

## 2026-09-26 follow-up
- Native Release build succeeded with VS-bundled CMake 4.2.3 using the command above. The PATH CMake 4.0.2 failure was a generator-version mismatch, not a source failure.
- A direct current-exe launch with no MM3/RECOMP/XBOX environment variables stayed alive at 15 seconds, but stderr grew to 210,247 bytes because the generated targeted C includes RECOMP_TRACE_ENTER calls. Stopped our PID 476 to avoid unbounded diagnostic output. This is not a strict or acceptance run.
- Generation script currently always passes --trace-functions with conformance_tmp/mm3_focus_trace_functions.json. Next: make normal generation omit that option by default, preserve an explicit diagnostic switch, regenerate from the same XBE/toolkit, build, then capture the actual game window.
- Upstream check: sp00nznet/xboxrecomp main is 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b. Local toolkit 01db629 is 204 commits ahead and 0 behind that merge base. PR heads 89, 92, 93, 97, 112-117 and 128 are reachable from local HEAD; #129 and #130 remain open and each is one commit ahead. Their changes touch different files; #128 overlaps four currently dirty runtime files, so do not merge it wholesale.
- Latest direct run log pair: conformance_tmp/native_current_default_20260926.out.log and .err.log; executable hash at launch 862EC6FE857A111CAAEC2AB5C7B9D1EE26C8B3219BB80ABB59C0EB91B3862477.

## 2026-09-26 generation frontier
- Nested toolkit commit: ce4076f (`fix(recomp): restore flag snapshot helpers`). This restores REP compare detection and arithmetic-result flag snapshots dropped by the PR #124 merge conflict. A fresh full translation then completed 24,828 of 24,854 bodies with zero translation failures; the later per-function overlay step stopped at missing exact function 0x84020.
- Root commit a8a69b9 makes `--trace-functions` opt-in (`-TraceFunctions`) so ordinary regeneration does not produce high-volume trace C. The nested toolkit gitlink now points to ce4076f.
- Current `default.xbe` disassembly has 24,854 functions and zero xrefs to 0x84020; exact entries 0x84020 and 0x842EA are absent. Older frontier-generated files/manual wrappers still name both. Current and previous XBE hashes were separately analyzed; both fresh disassemblies miss 0x84020. PR #129's focused tests passed (3) but its MM3 disassembly was unchanged and still missed that entry.
- Next: establish the provenance and reachability of the two old overlay entries, reconcile the manual/SEH overlay list with the current XBE, then regenerate into a clean directory and continue the runtime crash investigation. Do not claim playable output until the real window and two clean cold runs are captured.
- Nested toolkit checkpoint advanced to ca20ea2 (`wip(runtime): checkpoint guest memory and NV2A fixes`), including the pre-existing working edits across CMake, kernel memory/IRQL, pushbuffer parsing, and OHCI. These edits are saved but remain runtime-unverified; the earlier build and crash evidence predates this commit. Toolkit tracked status is now clean.

## 2026-09-26 full-section generation
- Correction to the earlier text-only finding: `tools.disasm --text-only` omitted imported executable sections, including XONLINE. Full disassembly of the same XBE produced 26,437 functions; XONLINE contains six direct calls to 0x84020 and four to 0x842EA, and both targets are now present in function data with incoming call xrefs.
- Full parser, function-identification and ABI pipeline completed for the same 2B04...3D79 XBE. `Generate-MM3Recomp.ps1` then generated 26,401/26,437 functions, 0 failed, 3,003,515 C lines, and all requested alternate-SEH/ABI overlays. 36 functions are intentionally project-manual; 369 unresolved stubs and unsupported instructions remain to assess.
- Next: make the generator script run the full-section analysis pipeline itself, configure/build from `conformance_tmp/mm3_all_sections_gen_20260926`, then run with controlled diagnostics and capture the actual game window. This is not yet playable proof.

## 2026-09-27 native boot checkpoint
- Toolkit source HEAD is 97825e0; outer HEAD is 69009a3. Both Git worktrees are clean. The generated files, logs and Xemu install remain local under existing ignore/exclude rules.
- Generator now refreshes all executable XBE sections, function IDs and ABI data before translation; trace C is opt-in. Full-section output: 26,437 total, 26,401 translated, 36 project-manual, 0 translation failures. The compiler fix declares flag snapshots for arithmetic setters. Runtime now provides `recomp_unimpl`; the stale 0x93B00 manual dispatch was removed because the current decoder rejects it as mid-instruction and has no xrefs to it.
- VS 18 Release build succeeded using `RECOMP_GEN_DIR=conformance_tmp/mm3_all_sections_gen_20260926`. Executable SHA-256: 178D8A42449ED6C5C6B88296C29B406B674C5096EB3AB7C895778A2CC625C983.
- Cold native run used `MM3_THREAD_MODE=spawn` and `RECOMP_UNIMPL_TRAP=1`, with no inherited MM3/RECOMP/XBOX variables. It produced no `[UNIMPL]` or `[CRASH]`, then exited via `HalReturnToFirmware(routine=2)`. Before exit it checked `dashupdate.xbe`, failed to find `\Device\Harddisk0\Partition2\xboxdash.xbe` under `game_files`, and did not open a visible game window. Logs: `conformance_tmp/mm3_full_sections_run1.{out,err}.log`.
- Xemu is running PID 46848 with the configured Europe/Australia XISO and RTX 5080. Its stderr confirms the selected GPU and launch args. Native CUA has no app/window APIs in this session, so a live screenshot is unavailable.
- Next: trace the XBE path from its dashboard-update check through the firmware return, then compare with Xemu's boot behavior and correct the native launch/storage model. Playability and two clean cold runs are still unproven.

## 2026-09-27 handoff evidence
- `stackwalk.py` on the native exit dump retains return 0x00296EFE in XONLINE `sub_00296E94`; the translated XONLINE path calls `sub_00296CA6` / `sub_000800EF`, whose body dispatches `HalReturnToFirmware(2)` and then terminates. This is a dashboard/firmware handoff, not an access violation or unsupported-opcode trap.
- The boot path reads `\Device\CdRom0\dashupdate.xbe` successfully, then `\Device\Harddisk0\Partition2\xboxdash.xbe` fails with `STATUS_OBJECT_NAME_NOT_FOUND`. It allocates 4096 bytes at 0x80490000, and the stack contains the UTF-16 label “Xbox Dashboard Updater”. The launch-page field interpretation remains unverified.
- The checkout does not contain `tools/powershell/run_strict.ps1`, `tools/external/xemu/xemu_gdb.py`, or `tools/builds/python-venv`; the active xemu PID 46848 has no GDB listener. The current tool session has no native-app CUA API, so no live screenshot was captured. The prescribed xemu oracle and strict M4 run remain outstanding.
- Next: reproduce the updater/dashboard handoff with a controllable xemu GDB session or an isolated HDD overlay, compare launch-page/path arguments, and then decide whether the native filesystem/launch model needs correction. Do not add a path bypass or force the game past this exit without that evidence.

## 2026-09-27 dashboard mirror cross-check
- Retained `conformance_tmp/guide_dashboard_mirror_final.err.log` shows the earlier mirrored `xboxdash.xbe` did load (XBE header and image header reads succeeded), but its next `\Device\Harddisk0\Partition2\XODash\xonlinedash.xbe` lookup failed with `ERROR_PATH_NOT_FOUND`; execution then returned through `HalReturnToFirmware(2)`.
- The retained FATX extraction log reports `xboxdash.xbe` at 172,032 bytes, then shows extraction failed on an invalid filename. This supports that the old mirror was incomplete; it does not establish the correct full dashboard contents or boot path.
- Current root and toolkit source remain unchanged and clean. Next evidence should come from comparing the authentic xemu handoff or a complete, validated system-partition extract, not adding another guessed file mapping.

## 2026-09-27 isolated xemu GDB checkpoint
- After closing the original Xemu cleanly, launched a second instance from a copied HDD and EEPROM with all original config sections preserved, `-snapshot`, and a GDB stub on port 1235. The original HDD remains untouched.
- WSL GDB attached through the Windows host gateway. In the visible-display run, after 60 seconds the stopped EIP `0x001A422C` maps via the current full-section function database to `sub_001A4214+0x18`; guest stack return sites map to `sub_001F443D` and `sub_001DB6E8`. This confirms xemu advanced into `default.xbe` game code, but does not prove a menu or playable output.
- A separate headless run remained in a kernel polling loop at `0x800426D4`; treat that as a headless-run observation, not evidence against the documented visible launch.
- Next: capture the authentic launch state at the `default.xbe` entry and compare it with the native direct-entry path. Restore the standard xemu launch on the original HDD after the isolated GDB run.
## 2026-09-27 authentic XBE entry snapshot
- Reset the isolated xemu GDB guest and set a hardware breakpoint at the verified XBE entry `0x00083C55`. It hit, proving the documented optical boot sequence reaches the target entry before the later game-code trace.
- At entry: `EIP=0x00083C55`, `ESP=0xD001CD7C`, `EAX=0x000003FA`, `EBX=0`, `ECX=0`, `EDX=0`, `ESI=0x80000000`, `EDI=0`. The top stack word is return address `0x800158E0` in kernel space; subsequent words include launch/init arguments. The live GDB reads of `0x80490000` are unmapped at this point.
- `src/main.c` directly invokes `xbe_entry_point()` after memory/kernel/path initialization. This confirms the native path does not enter the title through the same observed kernel call frame; exact argument meaning and the smallest faithful launch model remain unverified.
- Next: map the xemu entry stack/register setup to kernel startup code, compare the native entry state, then implement only the evidenced missing initialization. No launch bypass has been added.
## 2026-09-27 xemu kernel-to-title handoff
- A GDB breakpoint at kernel `0x800158DA` catches the indirect call through `[0x00010128]` to the XBE entry `0x00083C55`. Kernel calls at `0x8001E972` and `0x8001E9F5` immediately precede the handoff; the return site is `0x800158E0`.
- Before the call, the guest stack is `ESP=0xD001CD80`; registers include `EAX=0x3FA`, `ESI=0x80000000`, with `EBX/ECX/EDX/EDI=0`. The title entry then sees the call return address at the top of the stack. The two preceding kernel routines are not yet identified, so their effects must not be guessed.
- Next: inspect those kernel routines’ effects and compare them with current runtime initialization. Then restore the documented standard Xemu launch on the original HDD; the isolated GDB run remains on its copied disk.
## 2026-09-27 debugger cleanup
- Disassembly at the handoff shows `0x8001E972` reading the loaded XBE header/section data and iterating aligned ranges; `0x8001E9F5` also runs before entry, but its setup effect is still unresolved.
- Closed the isolated GDB guest with the QEMU monitor. Relaunched the documented `tools\\xemu\\xemu.exe -boot d` against the original configured HDD and Europe/Australia XISO (PID 39016). The copied HDD, config, logs, and entry probes remain in excluded `conformance_tmp/`.
- Both the outer repository and nested toolkit worktree are clean after the checkpoint commits. Native playability, strict runs, and required screenshots remain unproven.