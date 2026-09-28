# MM3 playability status — 2026-09-28

## Build identity

- Latest tracked root source change `4dc020f16880b00c79996eb49bcf04fc95ca7de9`; later commits are evidence notes. Toolkit `tools/xboxrecomp` is clean at `c09dc79f87e1ffd368f37df0d7e157f70fc7bf6c`, matching the gitlink.
- XBE SHA-256: `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- Current build is diagnostic, not acceptance: `build-msvc-tailfix/RelWithDebInfo/mm3_recomp.exe`, SHA-256 `08B2FC089E3BE22A598C8A8FC6784E99754F78FB637897CC1806A8C955C55D46`. Its CMake cache points at instrumented ignored generation `conformance_tmp/mm3_full_regen_compareflags_20260928_v8_gen`.

## Current frontier

- Native startup still exits with `0xC0000005` at the null indirect-call path, guest caller `0x001F37E5`; the diagnostic log reports unresolved target `0x00080000`. No native menu or city frame is proven.
- The toolkit repeat-CMPS/SCAS flags fix makes equal `onInit` keys compare equal and reaches callback state 5, but does not resolve the later null call. Root cause of the next parser/handler divergence is still open.
- Paired CE99/D620 traces show the first native stream is `Scripts/userSetup.lua` (409 bytes), matching xemu. The second native stream is the valid 24-byte `Scripts/userRun.lua` stub; xemu selects valid `Scripts/modules.lua` (1,029 bytes). Both copies of `Data_hd.zip` are byte-identical (SHA-256 `DDB744668B11EDE578D143E966417123CE0445D492CEF60E464EE6CE13D3D4AF`), so the divergence is runtime file selection, not stale or corrupt cache data. Native byte trace SHA-256 `2E805F87CA634D6429EFB6DC1B13AC993682D96E13202F3F9C82177F4A8F9B4C`; xemu trace SHA-256 `775F0B4086A5A0ADE09A09DE9B01062A46E5EA0C6B1E716E1AC2F4819314545E`.
- Native CE99 entry context has identical wrapper return `0x0008867F` and ECX `0x01055AF0` for both calls, but locals differ (`0x00F7FD00` then `0x00F7FD18`) and point to the two script streams. Context trace SHA-256 `643BAB8D6211AEC357F22EFA4784368ACAAC042536BFB225A1742860EDA59873`.
- A separate native file-thread run opened `D:\Data\Data_hd.zip`, both city AI files, `Z:\Data\Data_hd.zip`, and `D:\Data\Data_dvd.zip`. Reads from `Data_hd.zip` are present; no `Data_dvd.zip` read appears before the crash. Trace SHA-256 `5F9BA2CCF72721E5232BC84043909B7F7719F3F264B8CA15943BC8BB25D2F122`.
- Isolated xemu GDB reached XBE entry `0x00083C55` and captured three D620 events. The first two return `0x2D` and `0x66`, matching native; their streams begin with the setup.lua description and `function onInit()` respectively. Xemu then parses `EnvMapMode(0, 8)` and returns `0x65`; native crashes before an aligned third event. Native logs show the same AE4E stream pointer for its first two calls, while xemu uses distinct pointers. This is a concrete difference, not yet a proven cause. Trace SHA-256 `775F0B4086A5A0ADE09A09DE9B01062A46E5EA0C6B1E716E1AC2F4819314545E`.
- Isolated xemu windows captured the authentic MM3 title screen (`conformance_tmp/xemu_ce99_d620_20260928_window.png`, SHA-256 `6E44A357DF4BE9B401283B682AB6030C5649E5184CE638430263AC13F30059CE`) and an original Paris street scene (`conformance_tmp/xemu_reference_scene_20260928.png`, SHA-256 `A9615F351859988CC37B720D0F287C2962E7319906C59351CD1978C8F2515FC3`). These are xemu references, not native acceptance evidence.
- The paused reference xemu remains PID `32416` on port `1235`; all candidates created for this capture were stopped. No native strict runs have passed.

## Next blocker and acceptance

- Trace the producer/state feeding `sub_00088668` to explain why the second native CE99 call selects `userRun.lua` while xemu selects `modules.lua`; prove the first differing state before changing source. Then rebuild from canonical generated output and retest the null indirect-call path.
- Completion still requires two cold strict native runs with recognizable menu, city/car/HUD, working controls and audio, 10 minutes of driving, pause/resume/menu, and actual-window captures at menu, loading, and driving.
