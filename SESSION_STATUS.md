# MM3 playability status — 2026-09-28

## Build identity

- Latest tracked root source change `4dc020f16880b00c79996eb49bcf04fc95ca7de9`; later commits are evidence notes. Toolkit `tools/xboxrecomp` is clean at `c09dc79f87e1ffd368f37df0d7e157f70fc7bf6c`, matching the gitlink.
- XBE SHA-256: `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Current build is diagnostic, not acceptance: `build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe`, SHA-256 `08B2FC089E3BE22A598C8A8FC6784E99754F78FB637897CC1806A8C955C55D46`. Its CMake cache points at instrumented ignored generation `conformance_tmp/mm3_full_regen_compareflags_20260928_v8_gen`.

## Current frontier

- Native startup still exits with `0xC0000005` at the null indirect-call path, guest caller `0x001F37E5`; the diagnostic log reports unresolved target `0x00080000`. No native menu or city frame is proven.
- The toolkit repeat-CMPS/SCAS flags fix makes equal `onInit` keys compare equal and reaches callback state 5, but does not resolve the later null call. Root cause of the next parser/handler divergence is still open.
- Native CE99 tracing captured two wrappers returning at `0x0008867F`; both AE4E calls returned stream `0x01064630`, while D620 returned `0x2D` then `0x66`. Trace SHA-256 `ADAAE695447EE613B69D133921EE4E1DA4B97E2497B6ECFD73E621500DBDF562`.
- A separate native file-thread run opened `D:\Data\Data_hd.zip`, both city AI files, `Z:\Data\Data_hd.zip`, and `D:\Data\Data_dvd.zip`. Reads from `Data_hd.zip` are present; no `Data_dvd.zip` read appears before the crash. Trace SHA-256 `5F9BA2CCF72721E5232BC84043909B7F7719F3F264B8CA15943BC8BB25D2F122`.
- Isolated xemu GDB reached XBE entry `0x00083C55` and captured three D620 events. The first two return `0x2D` and `0x66`, matching native; their streams begin with the setup.lua description and `function onInit()` respectively. Xemu then parses `EnvMapMode(0, 8)` and returns `0x65`; native crashes before an aligned third event. Native logs show the same AE4E stream pointer for its first two calls, while xemu uses distinct pointers. This is a concrete difference, not yet a proven cause. Trace SHA-256 `775F0B4086A5A0ADE09A09DE9B01062A46E5EA0C6B1E716E1AC2F4819314545E`.
- Isolated xemu windows captured the authentic MM3 title screen (`conformance_tmp/xemu_ce99_d620_20260928_window.png`, SHA-256 `6E44A357DF4BE9B401283B682AB6030C5649E5184CE638430263AC13F30059CE`) and an original Paris street scene (`conformance_tmp/xemu_reference_scene_20260928.png`, SHA-256 `A9615F351859988CC37B720D0F287C2962E7319906C59351CD1978C8F2515FC3`). These are xemu references, not native acceptance evidence.
- The paused reference xemu remains PID `32416` on port `1235`; all candidates created for this capture were stopped. No native strict runs have passed.

## Next blocker and acceptance

- Capture native bytes at the second CE99/D620 event, align them with xemu `function onInit()`, and trace the first differing producer and later null-call target. Then fix at the shared source layer and rebuild from canonical generated output.
- Completion still requires two cold strict native runs with recognizable menu, city/car/HUD, working controls and audio, 10 minutes of driving, pause/resume/menu, and actual-window captures at menu, loading, and driving.