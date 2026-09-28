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
- Isolated xemu GDB now works through Rider GDB. It reached XBE entry `0x00083C55` and the first D620 call. That call consumed the `setup.lua` stream beginning `-- Description: Configure the game settings per user.` and returned `0x2D`, matching the first native D620 value; the second xemu event was not captured, so the inputs are not yet fully aligned. Trace SHA-256 `559ACCA2025C5FD8503E5B7F1E0E2BE4E966C7C0199348431FF3C9AF6BC1F8EC`.
- An isolated xemu candidate reached the authentic MM3 title screen (“Press START to play”). Screenshot: `conformance_tmp/xemu_ce99_d620_20260928_window.png`, SHA-256 `6E44A357DF4BE9B401283B682AB6030C5649E5184CE638430263AC13F30059CE`. This is xemu reference output, not native acceptance evidence.
- The paused reference xemu remains PID `32416` on port `1235`; all candidates created for this capture were stopped. No native strict runs have passed.

## Next blocker and acceptance

- Align the second CE99/D620 stream and the native failing event at `0x001F37E2`; identify the first differing producer, then fix at the shared source layer and rebuild from canonical generated output.
- Completion still requires two cold strict native runs with recognizable menu, city/car/HUD, working controls and audio, 10 minutes of driving, pause/resume/menu, and actual-window captures at menu, loading, and driving.