# MM3 debug coordinates and teleport

Worktree: `I:\repos\midtown-madness-3-recomp-overlay`, branch
`feature/imgui-teleport`. This work does not modify the separate live worktree.

Set `MM3_DEBUG_TELEPORT=1` before starting the isolated executable. A small
Win32 panel appears over its framebuffer window. It shows the primary car's
native X/Y/Z, accepts three numeric coordinates, and has a Teleport button.
The panel avoids a new ImGui/C++ dependency and renderer modifications.
Without the environment variable, no panel or command server is created and
the player update goes directly to the original game routine.

The same queued operation is available through a local, per-process named
pipe, `\\.\pipe\MM3Teleport-<PID>`. Each connection accepts one ASCII message,
`position` or `teleport X Y Z`, and returns one JSON message. The reply reports
whether a car is available or whether a teleport was applied. A request that
cannot reach a player update in two seconds is cancelled.

PowerShell examples (substitute the isolated process ID and an observed destination):

```powershell
tools/powershell/Send-MM3Teleport.ps1 -ProcessId 12345
tools/powershell/Send-MM3Teleport.ps1 -ProcessId 12345 -X -512.25 -Y 6.5 -Z 1001.125
```

Those example coordinates are arbitrary contract-test values, **not** a
verified waterfront location. No coordinate scaling or axis swapping occurs.
Orientation is preserved; the game's native reset clears motion before the
native transform setter updates the rigid body and its quaternion. The cached
player transform and the same flag used by the game's reset path are updated.
All guest mutations execute in the primary player's update, not on a host
panel or pipe thread. Guest GPR/x87 state is restored afterward.

## PAL binary evidence

XBE SHA-256: `2B04B66C43E7F37BBCEBBFB5B72CCB96A2AA99CC2C60C53EB7D3530BCC2A3D79`.

- Player update: `0x002203E5`, incoming `ECX` is the player object.
- Primary player: `[[0x003C5CDC]+0x54]+0xD8`; the update compares this pointer
  with its player object at `0x00220725` through `0x0022073F`.
- Player `+0x19C`: vehicle interface with vtable `0x00383768`; vehicle `+8`
  is the car. Car `+0x3C` points at simulation state; its `+4` is the rigid body.
- Native position getter `0x001D5730` reads body `+0xC8`, `+0xCC`, `+0xD0`.
- Native reset `0x0021F6DE` calls vehicle vtable `+0xAC` and `+0x28`, then
  copies the transform to player `+0x48C`, zeros `+0x1B0`, and sets `+0x218`.
- These vtable methods are `0x001D5F90` (tail jump to `0x001CDDA3`) and
  `0x001D65BC`. The latter copies the 3x4 transform to body `+0xA4` and writes
  its derived quaternion at `+0x134` through `+0x140`.
- The native reset's helper `0x001CD866` clears velocity/integration state.

## Validation status

The full isolated `RelWithDebInfo` executable builds successfully. The new
module compiles with MSVC `/W4`; the PowerShell script parses, and
`git diff --check` passes in the isolated worktree. A synthetic contract test
in `tests/mm3_teleport_contract.c` checks the disabled path, queue completion,
timeout cancellation, invalid vehicle rejection, native XYZ/orientation,
cached transform, reset flag, and guest register preservation. **Its native
game calls are stubbed**, so it does not prove live physics or rendering.
The PowerShell client also passes an end-to-end named-pipe position/teleport/
position check against the synthetic car server.

The executable build and live acceptance are separate gates. Completion
requires reading live coordinates, manually teleporting near water, invoking
the same destination programmatically, and verifying stable rendering and
continued driving. No game instance was launched or controlled during the
initial implementation.
