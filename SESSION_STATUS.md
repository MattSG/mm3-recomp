# MM3 session status — 2026-09-27

## Current checkpoint

- Root `master` before this note: `251b432`; nested toolkit: `cd10b9f` (parent `eddd03e` enables guest framebuffer scanout by default). Root and nested worktrees were clean. Historical checkout `I:\repos\midtown-madness-3-recomp-startuptga-display` untouched.
- Executable: `build-msvc-tailfix\RelWithDebInfo\mm3_recomp.exe`, SHA-256 `691DE743A3BF5F7A2E60577DD9226F5430FFD2E35F7D60557DAD2060FFD614CC`. Build: `& 'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-msvc-tailfix --config RelWithDebInfo --target mm3_recomp`. Full generated C is in ignored `conformance_tmp/mm3_full_regen_clean3_20260927_gen`; XBE SHA-256 `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.
- This remains a diagnostic build: ABI check off, no strict runner or `MM3_STRICT` option; CTest has zero tests. No two cold strict runs. Actual window screenshots/video, menu, 3D city, controls, audio, ten-minute drive and pause/resume/menu remain unverified.

## Latest runtime evidence

- Default scanout opens a 640x480 guest framebuffer window. An isolated cold native run stayed on authentic `LOADING` panel with animated dots for eight minutes; no menu transition. Inspected framebuffer capture: `conformance_tmp/native_default_scanout_probe_20260927_long/frame.bmp`, SHA-256 `492B1FEFBEC108E2AC475709F33BC1CBE3CCF40B56EF8F42984389119037327A`. This is guest framebuffer data, not an app-window screenshot. Run stderr SHA-256 `EEDCBDB36C3516A0F8E89C09224034781007C65120382925F89B288A85F83B37`.
- Native opens `Data_hd.zip` and city assets including `Data\Shared\Paris\city.ai`. CDB sampled guest worker stack in `sub_001E7B8F_gen`, `sub_00081F13`, and `KeDelayExecutionThread`; one sample only, not proof of a deadlock. Trace SHA-256 `66590D45D37B8AD8237C43B79BE601C5DC0D622042DE5C1E1CD85F72C8BF68D5`. Probe process stopped; its two isolated save trees were removed (9 files / 5,278,032,850 logical bytes each).
- Existing xemu PID `32416` remains running and resumed after bounded GDB samples. Its `-snapshot` base HDD clone SHA-256 stayed `609BB38B721C5826F0EF8156AAF2A948EA6A5AAD442D1B6D6DE163B3F02DBA59`. Read-only FATX parse of base image Z/Partition3 found five sound files and no `mmxLock.lck`; native initially reports the lock missing, later opens it, so this miss is not the current cause. `-snapshot` overlay state is not represented by offline base-image parse. Partition log SHA-256 `CF6A78E9A798CDEB25CE913335E35C1BB71B3E67B2E2A73C45281D6B419C05E5`.
- Current xemu GDB sample still has guest `0x00431654 = 0x00431838`; sample log SHA-256 `F8D4059FFF4CE7AADEDBDD694C5C4CC509969634E27B2D884154D4725C8112FB`. Previously paired native/xemu trace found `sub_00214EDD` returns 1 native vs 0 xemu, leaving that field unset natively. Callback-level cause and first divergence in the current eight-minute loader state remain unresolved; no same-PC/thread pair yet.
- Renderer ceiling: current pushbuffer executor handles clears and limited untextured screen-space geometry; texturing, depth and vertex programs remain unsupported. Even after loader is fixed, this cannot meet authentic city rendering.

## Upstream and next action

- Live upstream checked at `sp00nznet/xboxrecomp`, default `main`: requested PRs #117, #92, #93, #97, #112–116 and #129 are merged; #128 remains open. Local toolkit contains the earlier integration set; #129 is absent locally and has no demonstrated MM3 relevance. Do not merge without runtime evidence.
- Next: reproduce loader frontier, capture native and xemu at same guest thread/PC around `sub_00214EDD` and worker delay path, compare callback result plus `0x00431654`; fix proven cause, rebuild, inspect guest pixels, then address full D3D rendering. After that, establish strict launch and complete two cold runs with controls, sound, ten minutes, pause/resume/menu and actual window capture.
- Latest source commits: root `41e8424` scanout/save-root support; toolkit `eddd03e` default scanout; root status checkpoints `9410e66` and `251b432`; toolkit note `cd10b9f`. Drive I: had 23.78 GB free after cleanup.
