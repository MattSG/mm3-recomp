# Retail debug feature experiment

Worktree: I:/repos/midtown-madness-3-recomp-debug-features
Branch: experiment/retail-debug-features
Root base: a5077af; toolkit base: 6494bbb.
Private game data: 468 files hash-verified against the pre-DLC backup before changes.
Private saves: out/debug-features/save. No primary game assets or saves changed.

## Runtime evidence

Run: out/debug-features/run-20261009-133523.
This run used the previous APU implementation as a diagnostic comparison. Both audio implementations stalled with the repacked archive; preserving original ZIP local-header offsets restored startup. The final source retains the committed APU implementation.

| Feature | Result |
|---|---|
| Native debug HUD | Visible in gameplay: X/Y/Z/angle, component damage, speed/RPM/gear. capture-003.bmp and capture-004.bmp. |
| Native debug menu | Visible and responds to D-pad. capture-004.bmp; SkyBox and Paint AI Graph flags changed in captures 005 and 007. |
| Facade floors | Native value changed 2 -> 3 (probe 4). Visual correctness not established. |
| E3settings.lua | Executed (probe 7), restored native facade floors to 2. Original script also specifies LOD zones 80/100/350, FOV 62, streaming sound enabled. Those effects were not individually measured. |
| Extended camera modes | Script completed; mode cycling not yet proven. |
| Input-bound camera | Script completed; movement not yet proven. |
| engineprofiler.lua | Script completed; no visible profiling graph verified. Retail has stripped profiler callbacks. |
| Pedestrian count | Script failed (loaded=0). aiDebugSetPedestrianCount absent from retail Lua registration inventory; bundled dopeddebug.lua references this missing command. |
| Wireframe / VSync / AA / LOD / far clip / fog | Menu entries present; host renderer effects not established. |

Rendering corruption was seen during testing, particularly after debug drawing controls. These are experimental tools, not production-ready features. The menu did not pause the mission timer.

## Use

Build Release with the Visual Studio CMake. Run tools/venv/Scripts/python.exe tools/debug-features/prepare.py while the game is stopped, then tools/debug-features/run.ps1.

While gameplay is active, write one number to the request.txt path in out/debug-features/current-run.json's run directory:

0 baseline; 1 HUD on; 2 debug menu; 3 extended camera modes; 4 facade floors 3; 5 engine profiler; 6 pedestrian count (known failure); 7 E3 preset; 8 reset/menu close; 9 input-bound camera.

The hook calls the original Lua loader on the game thread and preserves guest registers. Each request logs begin/end and the loader result. Loader success alone does not establish that a feature works.

The prepare script validates the ZIP and asserts that every original entry keeps its local-header offset. It preserves Data_hd.zip.before-features. Do not repack the original entries: that caused startup failure in this experiment.

## Final build verification

Pinned-generator regeneration completed (51 C sources). Release build passed; toolkit checkout is clean at 6494bbb, including its committed DSP implementation.
Run: out/debug-features/run-20261009-134553, PID 31300.
HUD visible in capture-003.bmp; debug menu visible in capture-004.bmp; reset removes both in capture-005.bmp. Probes 3/4/5/7/9 returned loaded=1; probe 6 returned loaded=0. Probe 4 changed floors to 3, probe 7 restored 2. Final reset returned loaded=1. Camera and profiler loader success remains weaker than functional proof.

The isolated instance is left running with the debug menu and HUD enabled for review. D-pad up/down selects and left/right changes entries. To close it, write 8 to this run's request.txt. The mission timer continues while the menu is open.

## Launch flags and graphics regression

`run.ps1 -DevHud -DebugMenu -ExtendedCameras` enables the corresponding tools on the first player update. `-E3Preset` runs the E3 settings too. Environment equivalents are MM3_DEV_HUD, MM3_DEBUG_MENU, MM3_EXTENDED_CAMERAS and MM3_E3_PRESET (1 enables, 0 disables). Scripts must first be installed with prepare.py. Startup flags were exercised in run-20261009-135727 (probes 1, 2 and 3 all loaded=1).

`-GraphicsPreset Retail|Default|Max` selects baseline/enhanced/max quality. Max uses draw distance 4, 8x MSAA, 16x AF, 1920 internal lines. `-Graphics @{ RECOMP_MSAA='1' }` overrides a supported graphics option. `-CaptureDisplay` captures the final window composite; otherwise captures are at internal resolution. The D3D debug layer is optional via `-Graphics @{ RECOMP_D3D_DEBUG='1' }`; it is not a quality feature.

For blank-frame or texture-dropout regression testing omit all developer overlay switches. Capture a sequence by writing N to capture.flag. Run `python tools/debug-features/analyze_frames.py <run-directory> --start <first-gameplay-frame>` to produce frame-analysis.json. It reports sampled black frames and adjacent image changes; visual inspection must distinguish scene motion from defects. `--self-test` checks the metrics.

The current branch integrates committed vertex-fetch and sky/depth/fog fixes through toolkit merge 40598f1 (see git history for full hash). The original font/menu placement reverts remain.

First max-quality run: 120 gameplay captures (005..124), no fully blank internal images in that interval, but severe scene geometry/texture dropout is visible between captures 091 and 092. Debug menu/HUD had been reset off before capture. This is a confirmed regression, not a passing graphics test. A later crash logged exception 0x87D; cause is not established. Final-window captures and normal runs without D3D validation are still required.

Existing D3D8 gamma, formats, states and A8 tests passed (4/4). These tests do not validate the active NV2A push-buffer renderer end to end.

Toolkit dc70b03 fixes a separately reproduced draw-state defect: a sampled surface's CPU upload calls blit_span, which unbinds render/depth targets and changes the viewport after setup_pipeline has bound the draw's targets. The new nv2a_targets test failed on the missing render target before the fix. Restoring targets after texture preparation passes at 1x/2x/4x/8x MSAA, checking both color/depth bindings and the viewport (4/4, 23.90s). Release rebuilt successfully. This proves that state repair, not resolution of the captured gameplay dropout or freeze; max-quality final-window testing continues in run-20261009-141245 with all developer overlays off.
