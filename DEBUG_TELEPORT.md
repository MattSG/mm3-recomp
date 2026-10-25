# MM3 ground teleport debugger

Use **ImGui debug tools** for live X/Y/Z, heading and coordinate teleport.
The former separate Windows panel has been removed. Enable
`MM3_DEBUG_OVERLAY=1` and open the overlay with its configured toggle (F4 by default).
For scripts, `MM3_DEBUG_TELEPORT=1` enables the local endpoint without opening a window.

MM3 is Y-up; its native ground-placement reset
calculates Y, applies car clearance, aligns to the slope while retaining
heading, clears motion and updates the cached transform and reset flags.
No usable ground leaves the car unchanged. Guest changes run in the primary
player update, with GPR/x87/XMM state restored afterward. Disabled is default.

The same operation uses the local endpoint `\\.\pipe\MM3Teleport-<PID>` on
Windows, and a Unix stream socket elsewhere: `$XDG_RUNTIME_DIR/MM3Teleport-<PID>.sock`,
or `/tmp/MM3Teleport-<PID>.sock` when `XDG_RUNTIME_DIR` is unset or empty.
Both carry the same requests: `position`, `teleport X Z [heading]`, `cars`, and
`peek ADDR LEN`, each returning JSON; the teleport reply includes the actual
resulting Y. Requests that cannot reach the player update within two seconds
are cancelled. ImGui reports ground misses, stalled updates and unavailable cars.

Socket protocol: one connection serves one request at a time. The client sends
the request, then either a newline or a half-close (`shutdown(SHUT_WR)`); the
game replies and closes, so the client reads to EOF. The socket file is removed
on normal exit; a process killed by signal leaves it behind until the same PID
is reused. A stale socket answers "connection refused", which the tools report
as not connected.

```powershell
python tools/scripts/send_teleport.py --process-id 12345
python tools/scripts/send_teleport.py --process-id 12345 --x -512.25 --z 1001.125
```

On Linux and macOS, use the Python tools, which pick the same socket path:

```sh
python3 tools/scripts/send_teleport.py --process-id 12345
python3 tools/scripts/send_teleport.py --process-id 12345 --x -512.25 --z 1001.125
```

These example coordinates are fixture values, not a verified destination.
For standalone live runs enable `RECOMP_FB_WINDOW=1`, `RECOMP_PB_EXEC=1`,
`RECOMP_USB=1`, optionally `RECOMP_KEYBOARD=1`, and use separate game/save data.

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

RelWithDebInfo builds. The default `mm3_teleport_contract` tests teleport.c's
own logic: the argument block given to the ground reset, the heading choice
(queued, or the car's own when none is queued), the position reported back,
the AL success byte, register restoration, cancellation, inactive paths and the
nonblocking teleport queue used by ImGui. Its ground reset is a stub that
supplies outputs (a sentinel Y, AL, clobbered registers), so ground height,
clearance and the car's transform/cache are checked only by the original-PAL
fixture below. The original-PAL fixture uses synthetic collision geometry and
registry services; it is not a live terrain test. Its PowerShell pipe sequence
returns X=-512.25, Y=6.5, Z=1001.125 without a Y argument.

The PAL build needs `conformance_tmp/native_teleport_memory.bin`, a copy of the
game's `.rdata` that is never checked in. Without it the program prints `SKIP`
and exits with code 77 (not a pass), so a missing fixture is never silent.

Generate and compile from the repository, using the toolkit's Python environment
and the same generated headers as the game build:

```powershell
python tests/generate_native_teleport_test.py --xbe game_files/default.xbe --functions tools/xboxrecomp/tools/disasm/output/functions.json
cl.exe /nologo /W4 /DMM3_TELEPORT_NATIVE_TEST /I src /I src/recomp /I src/recomp/gen /I tools/xboxrecomp/include /Fe:conformance_tmp/teleport_native_test.exe /Fo:conformance_tmp/ tests/mm3_teleport_contract.c conformance_tmp/native_teleport_functions.c user32.lib
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
