# ImGui debug tools

Enable **ImGui debug tools** under the launcher's Mods tab. Controls contains the
show/hide keyboard, controller-combination and optional mouse bindings. They save
with the launcher's other settings when Play is pressed.

Defaults: **F4** or **Back + left-stick click**; mouse toggle is unbound. Inside
ImGui, use mouse, Tab/arrows and Enter/Space, or controller D-pad/stick and A.
The overlay closes on focus loss. Gameplay continues while it is open.

For an isolated test run after building and preparing private data:

```powershell
python tools/debug-features/run.py --config RelWithDebInfo --debug-overlay --capture-display
```

This script uses its own worktree's data and `out/debug-features/save`. It bypasses
launcher settings; binding overrides can be supplied as environment variables:
`MM3_OVERLAY_KEY` (hex VK), `MM3_OVERLAY_PAD` (hex physical XInput button mask),
and `MM3_OVERLAY_MOUSE` (0 off, 4 middle, 5/6 side buttons). Do not launch a new
foreground game window during someone else's focus-sensitive live test.

HUD/native-menu commands need `tools/debug-features/prepare.py`, run only against
backed-up private data while its game is stopped. Native teleport, recovery and
repair do not require the prepared scripts. The ImGui controls do not expose profile unlock/save commands.

The Developer, Teleport, Vehicle and Diagnostics tabs reuse the game's native
backends. Opening the native debug menu closes ImGui so the game can receive
input. Experimental rendering/debug commands can disrupt gameplay; loader
success alone does not establish that a command works correctly.

See [native teleport](DEBUG_TELEPORT.md) and the
[developer probe guide](../tools/debug-features/README.md) for command usage.
