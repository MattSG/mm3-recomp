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
