# MM3 playability status — 2026-09-28

## Build identity

- Integration: root `f6dcdcb`; source checkpoint `41e8424`; clean tree. Toolkit `tools/xboxrecomp` is clean, detached at `cd10b9f4e7fa229e0f86176775800ab141e1a8c9` (parent pin `eddd03e`).
- Generated C: ignored `conformance_tmp/mm3_full_regen_clean3_20260927_gen`; XBE SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Build: `& 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-msvc-tailfix --config RelWithDebInfo --target mm3_recomp`
- Executable: `build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe`; SHA-256 `691DE743A3BF5F7A2E60577DD9226F5430FFD2E35F7D60557DAD2060FFD614CC`. Diagnostic build only; strict runner/`MM3_STRICT` acceptance path not established.

## Proven boundary

- Cold xemu reaches guest `0x001E770C` with `EAX=1`, then callback callsites `0x001E777C` and `0x001E778D`. Trace: `conformance_tmp/xemu_render_parent_20260928/trace_gate.log`, SHA-256 `BEC7C089EA513DE89D4D59DD65EC304552FA0E760011394F99C86F23DE1422A4`.
- Native reaches `sub_001E7627` and `sub_001E73AF`; the function return and `0x001E770C` result are unknown. The ~35s `gu` capture is inconclusive. CDB log SHA-256 `175B4886079FE9285CD0B0AB4CF3620A8CCBD43484DA149787B51FE62581C406`.
- This locates an unresolved native/xemu control-flow boundary, not its cause. Next capture direct callee entries/returns through `sub_001E73AF`, then compare registers at `0x001E770C`; if equal, capture `sub_0020F7EB` and the branch.
- Independent renderer blocker: current NV2A path can drop world batches and has no textured world output. Need one authentic city draw through the executor before expanding support.

## Acceptance state

- No visually verified menu, loading screen, city, vehicle/HUD, controls, audio, 10-minute drive, pause/resume/menu, actual-window screenshots/video, or two cold strict runs. Do not claim playability from logs or framebuffer dumps.
- No reliable strict launch command yet. FFF search `strict *.ps1` found no tracked acceptance runner; validate build/runner targets before strict work.
- Original xemu remains PID `32416`; do not alter its HDD/EEPROM. No native process was live at last check. Latest isolated save directories from this turn were removed after checking processes; decisive logs remain in `conformance_tmp`.

## Next action

Capture paired native and cold-xemu returns through `sub_001E73AF` and the `0x001E770C` gate. Keep root and nested worktrees clean between steps.
