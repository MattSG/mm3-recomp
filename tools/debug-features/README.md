# Retail developer probes

These scripts expose experimental commands from the retail game. Some commands
are absent or incomplete, and debug rendering can corrupt the scene. The native
debug menu does not pause the mission timer.

## Prepare and run

Build Release with the Visual Studio CMake. Run tools/venv/Scripts/python.exe tools/debug-features/prepare.py while the game is stopped, then `python tools/debug-features/run.py`.

While gameplay is active, write one number to the request.txt path in out/debug-features/current-run.json's run directory:

0 baseline; 1 HUD on; 2 debug menu; 3 extended camera modes; 4 facade floors 3; 5 engine profiler; 6 pedestrian count (known failure); 7 E3 preset; 8 reset/menu close; 9 input-bound camera; 10 HUD off; 11 debug menu off; 12 player camera.

`MM3_DEV_HOTKEYS=HUD:70,MENU:71,CAMERAS:72` (virtual keys in hex; the launcher's Controls tab writes it) toggles the HUD, debug menu and extended cameras with a key during gameplay, through probes 1/10, 2/11 and 3/12. Re-run prepare.py once to install probes 10-12.

The hook calls the original Lua loader on the game thread and preserves guest registers. Each request logs begin/end and the loader result. Loader success alone does not establish that a feature works.

The prepare script validates the ZIP and asserts that every original entry keeps its local-header offset. It preserves Data_hd.zip.before-features. Do not repack the original entries: that caused startup failure in this experiment.


Use the [ImGui tools](../../docs/IMGUI_DEBUG_TOOLS.md) for the in-game interface.
The probe numbers above belong to `prepare.py` and `src/mods/dev_tools.c`.
