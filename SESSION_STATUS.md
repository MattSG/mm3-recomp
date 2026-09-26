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
