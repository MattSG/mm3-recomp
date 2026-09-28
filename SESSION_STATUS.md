# MM3 playability status — 2026-09-28

## Build identity

- Integration: root `1900c07`; source checkpoint `41e8424`; clean tree. Toolkit `tools/xboxrecomp` is clean, detached at `6d94cf5a627b077c64524ad101147582f6678ac4`.
- Generated C: ignored `conformance_tmp/mm3_full_regen_clean3_20260927_gen`; XBE SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Build: `& 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-msvc-tailfix --config RelWithDebInfo --target mm3_recomp`
- Executable: `build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe`; SHA-256 `82CE9929D8641927E9852EBE05074685C844FAAD44396A25EA5BFBB738C0BCE0`. Built successfully after making `[RTL_ABI]` tracing opt-in via `RECOMP_TRACE_RTL_ABI`; strict runner/`MM3_STRICT` path remains unestablished.

## Proven boundary

- Cold xemu reaches guest `0x001E770C` with `EAX=1`, then callback callsites `0x001E777C` and `0x001E778D`. Trace: `conformance_tmp/xemu_render_parent_20260928/trace_gate.log`, SHA-256 `BEC7C089EA513DE89D4D59DD65EC304552FA0E760011394F99C86F23DE1422A4`.
- Native reaches `sub_001E7627` and `sub_001E73AF`; the function return and `0x001E770C` result are unknown. The ~35s `gu` capture is inconclusive. CDB log SHA-256 `175B4886079FE9285CD0B0AB4CF3620A8CCBD43484DA149787B51FE62581C406`.
- Paired cold trace: xemu enters `sub_001E6EAD` at +9.366s; return sites `0x001E6ECA/ED0/ED8/EE0` follow by +9.377s and gate `0x001E770C` at +9.600s. `trace.log` SHA-256 `8AE0134A636228BAF5AEBFD62340A2470249046F4FE941A77B507938E25EFCD4`; private HDD base unchanged at `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59` (snapshot boot, copied EEPROM).
- Fresh native run with application-prefixed environment variables cleared emitted no `[RTL_ABI]` trace, then crashed before the render gate. `[ICALL] Failed resolve VA 0x00080000 caller=0x001F37E5`; unhandled AV at `sub_0003B2F2+0x61`, stack `sub_001A435F -> sub_001F373E -> sub_001E77F3 -> sub_001DDB0A`. CDB log SHA-256 `5C4343A3773257FAF6B05E21D4E39665E94F20AFE50DBD6C794CBC97E7933E2B`; stderr `0C26D75AF9C44A74260D4216830A402028912FECD841F6CCA3A86B50DFF0C0FF`. Earlier trace-heavy native run produced 4,457 `[RTL_ABI]` lines and was diagnostic; it did not establish the cause.
- This narrows the path, not the cause: guest state/data parity is unproven. Next compare xemu and native state at guest indirect callsite `0x001F37E5`: target, object and registers, then identify why native resolves `0x00080000`.
- Independent renderer blocker: current NV2A path can drop world batches and has no textured world output. Need one authentic city draw through the executor before expanding support.

## Acceptance state

- No visually verified menu, loading screen, city, vehicle/HUD, controls, audio, 10-minute drive, pause/resume/menu, actual-window screenshots/video, or two cold strict runs. Do not claim playability from logs or framebuffer dumps.
- No reliable strict launch command yet. FFF search `strict *.ps1` found no tracked acceptance runner; validate build/runner targets before strict work.
- Original xemu remains PID `32416`; do not alter its HDD/EEPROM. No native process was live at last check. Latest isolated save directories from this turn were removed after checking processes; decisive logs remain in `conformance_tmp`.

## Next action

Compare xemu and native state at `0x001F37E5` and trace the unresolved native indirect-call target.
