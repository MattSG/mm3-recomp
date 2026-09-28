# MM3 playability status — 2026-09-28

## Build identity

- Root source checkpoint `41e8424`; clean tree. Toolkit `tools/xboxrecomp` is clean, detached at `6d94cf5a627b077c64524ad101147582f6678ac4`.
- Generated C: ignored `conformance_tmp/mm3_full_regen_clean3_20260927_gen`; XBE SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Build: `& 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-msvc-tailfix --config RelWithDebInfo --target mm3_recomp`
- Executable: `build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe`; SHA-256 `82CE9929D8641927E9852EBE05074685C844FAAD44396A25EA5BFBB738C0BCE0`. Built successfully after making `[RTL_ABI]` tracing opt-in via `RECOMP_TRACE_RTL_ABI`; strict runner/`MM3_STRICT` path remains unestablished.

## Proven boundary

- Cold xemu reaches guest `0x001E770C` with `EAX=1`, then callback callsites `0x001E777C` and `0x001E778D`. Trace: `conformance_tmp/xemu_render_parent_20260928/trace_gate.log`, SHA-256 `BEC7C089EA513DE89D4D59DD65EC304552FA0E760011394F99C86F23DE1422A4`.
- Native reaches `sub_001E7627` and `sub_001E73AF`; the function return and `0x001E770C` result are unknown. The ~35s `gu` capture is inconclusive. CDB log SHA-256 `175B4886079FE9285CD0B0AB4CF3620A8CCBD43484DA149787B51FE62581C406`.
- Paired cold trace: xemu enters `sub_001E6EAD` at +9.366s; return sites `0x001E6ECA/ED0/ED8/EE0` follow by +9.377s and gate `0x001E770C` at +9.600s. `trace.log` SHA-256 `8AE0134A636228BAF5AEBFD62340A2470249046F4FE941A77B507938E25EFCD4`; private HDD base unchanged at `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59` (snapshot boot, copied EEPROM).
- Native CDB stopped at `recomp_lookup(0x80000)` from guest `sub_001F37D2`, caller chain `0x001F38B3 -> 0x001F575F`. At that call, guest EAX was `1`; `sub_001F38AC` sets EAX from ESI. The low-memory chain from EAX=1 yields null and then target `MEM32(0x14)=0x80000`, followed by unhandled AV at `sub_0003B2F2+0x61`. CDB SHA-256 `2C50E79D9FA82EC1948D4AFB1E4D4887D7BC6DBDA80018BF6D8A4099FC51CE4C`; stderr `45E434BB4B93FD2B5284BC5F4A6FF175AB4538EDC55712D826BB4FA5CBF896B7`.
- Paired at the same caller chain: cold xemu `sub_001F37D2` entry has EAX/object `0x005E2BA0`, vtable target `0x001A3E8F`, and guest stack returns `0x001F38B3 -> 0x001F575F`. Trace SHA-256 `E979B3591EF8E3723A082E05D3A5E43CDFDBBFD199D20B54AD4545ADDCA41E61`; the private snapshot left the HDD base unchanged.
- The `ESI` corruption theory is disproved: all 101 direct calls in `sub_001F4825` returned with ESI unchanged. Native reaches `sub_001F373E` from `0x001E78BE` with EBX=`0x004315C8`, ESI=`0x00431610`, and EDI=0; its list reads at `0x2C..0x38` are `[0,0,1,0x707]`, and the `1` becomes EAX/ESI for the bad virtual call. Paired xemu has the same EBX/ESI but EDI=`0x00431838` and valid list objects at `0x00431864..0x00431874`. A fresh xemu hardware watchpoint caught the first field write: `sub_001F33A2` returns `0x00431838`, stored at `[object+0x8C]` by `0x001E771B`. Native later reads that field as zero. Next compare the `sub_001F33A2` return and call inputs.
- Independent renderer blocker: current NV2A path can drop world batches and has no textured world output. Need one authentic city draw through the executor before expanding support.

## Acceptance state

- No visually verified menu, loading screen, city, vehicle/HUD, controls, audio, 10-minute drive, pause/resume/menu, actual-window screenshots/video, or two cold strict runs. Do not claim playability from logs or framebuffer dumps.
- No reliable strict launch command yet. FFF search `strict *.ps1` found no tracked acceptance runner; validate build/runner targets before strict work.
- Original xemu remains PID `32416`; do not alter its HDD/EEPROM. No native process was live at last check. Latest isolated save directories from this turn were removed after checking processes; decisive logs remain in `conformance_tmp`.

## Next action

Compare guest ESI across native/xemu calls in `sub_001F4825`; find the first callee that fails to preserve it.
