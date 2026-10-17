The native fixture includes the production Windows XInput translation,
framebuffer keyboard handler and Xbox USB report encoder. It creates a hidden
window and supplies controlled XInput samples; it does not run MM3 or require
a controller.

Run from the repository root with the local toolchain:

```powershell
python scripts/devenv.py --shell
cmake -S tests/input_contract -B conformance_tmp/input_contract -G Ninja
cmake --build conformance_tmp/input_contract
ctest --test-dir conformance_tmp/input_contract --output-on-failure
```

`python scripts/devenv.py --shell` opens a shell with the MSVC toolchain and
tools/venv on PATH; run the remaining commands in it.

Checks cover independent trigger pressures through the 20-byte USB report,
face buttons, bumpers, signed stick axes, short buffers, legacy and MM3 arrow
bindings in menu and driving contexts, a missing driving probe,
held/released accelerator keys, opposing steering aliases,
R/F gear aliases in driving context with the original X/A bindings retained,
C camera alias in driving context with E and the physical right bumper retained,
Space handbrake alias in driving context with Z and physical A retained,
Escape's menu-back action, Enter's independent pause action, their held/released
state in either context,
keyboard/controller merging and the actual WM_KILLFOCUS handler clearing all
held keys. These checks verify the host input path, not MM3 vehicle physics or
the game's choice of menu actions.

The original Microsoft manual identifies Xbox A as the default handbrake
([control table, printed pages 6–7](https://www.manualslib.com/manual/379152/Games-Microsoft-Xbox-Midtown-Madness-3.html?page=5)).
Space sends that button only in the MM3 driving context, so holding it while
pausing cannot select a menu item. The fixture also verifies that Space is
inactive with the legacy layout or a missing driving probe and that focus loss
releases it.
