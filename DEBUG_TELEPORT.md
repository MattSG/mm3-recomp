# MM3 ground teleport debugger

Opt in with `MM3_DEBUG_TELEPORT=1`. The native panel shows live X/Y/Z and
accepts only destination X/Z. MM3 is Y-up; its native ground-placement reset
calculates Y, applies car clearance, aligns to the slope while retaining
heading, clears motion and updates the cached transform and reset flags.
No usable ground leaves the car unchanged. Guest changes run in the primary
player update, with GPR/x87/XMM state restored afterward. Disabled is default.

The same operation uses the local pipe `\\.\pipe\MM3Teleport-<PID>`:
`position` or `teleport X Z`, returning JSON with the actual resulting Y.
Requests that cannot reach the player update within two seconds are cancelled.
The panel distinguishes ground misses, stalled updates and unavailable cars.

```powershell
tools/powershell/Send-MM3Teleport.ps1 -ProcessId 12345
tools/powershell/Send-MM3Teleport.ps1 -ProcessId 12345 -X -512.25 -Z 1001.125
```

These example coordinates are fixture values, not a verified destination.
For standalone live runs enable `RECOMP_FB_WINDOW=1`, `RECOMP_PB_EXEC=1`,
`RECOMP_USB=1`, optionally `RECOMP_KEYBOARD=1`, and use separate game/save data.
This runtime's stubbed audio DSP also needs its existing passthrough mailbox
acknowledgement `RECOMP_APU_DSP_ACK=0x80458810` to pass MM3's loading wait.
That option does not emulate the DSP program.

## PAL evidence

XBE SHA-256: `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.

- Primary player update: `0x002203E5`, ECX=player. Primary player: `[[0x003C5CDC]+0x54]+0xD8`.
- Player +0x19C = vehicle interface, vtable `0x00383768`; vehicle +8 = car.
  Car +0x3C = simulation; simulation +4 = body. Translation: body +0xC8/+0xCC/+0xD0.
- Ground reset `0x0021ADF3` takes player, position, search mode and heading.
  Mode zero uses `0x0021848C` to raycast through the world's full Y bounds,
  ignoring supplied Y. The heading matches the native heading getter's matrix convention.
- It calls virtual +0xAC to clear motion, +0x34 for car clearance and +0x28
  to set transform/quaternion; caches at player +0x48C, clears +0x1B0, sets +0x218.

## Validation

RelWithDebInfo builds. Original-PAL and stub offline fixtures pass automatic
height/car clearance, ignored supplied Y, heading/quaternion, cleared motion,
cache/register preservation, ground misses, cancellation, inactive/disabled
paths and the actual hidden X/Z edit controls/button handler. The original-PAL
fixture uses synthetic collision geometry and registry services; it is not a
live terrain test. Its PowerShell pipe sequence returns X=-512.25, Y=6.5,
Z=1001.125 without a Y argument.

Generate and compile from the repository, using the toolkit's Python environment
and the same generated headers as the game build:

```powershell
python tests/generate_native_teleport_test.py --xbe game_files/default.xbe --functions tools/xboxrecomp/tools/disasm/output/functions.json
cl.exe /nologo /W4 /DMM3_TELEPORT_NATIVE_TEST /I conformance_tmp/overlay-gen /Fe:conformance_tmp/teleport_native_test.exe /Fo:conformance_tmp/ tests/mm3_teleport_contract.c conformance_tmp/native_teleport_functions.c user32.lib
conformance_tmp/teleport_native_test.exe
```

Generated outputs, data, logs and executables remain outside git. Generated
bodies retain ordinary unused-label/local warnings. `--serve` runs the native
fixture pipe for ten seconds.

Authorized isolated live testing exposed a CPU-scanout startup bug: renderer
initialization waited for a GPU flip, while movies preceded one. The toolkit
tick now initializes it. Captures show the loading screen and intact intro,
then the live car/city/HUD at approximately 60 FPS. USB input transfers and
live coordinates are verified. The user confirmed the debugger working after
trying the live panel. Testing never controlled the separate clean-worktree
instance. Initial implementation/testing stayed in `feature/imgui-teleport`;
source integration on `main` is a separate commit.
