# Midtown Madness 3 recompilation

Static recompilation of the Xbox title Midtown Madness 3 to native x86-64 Windows, on the
[xboxrecomp](tools/xboxrecomp) runtime, rendering through Direct3D 11.

## Building from a fresh clone

You need Windows 10/11 x64, Visual Studio 2022 or newer with **Desktop development with C++**
(it includes CMake), Python 3.11+ (the `py` launcher), git, and your own Midtown Madness 3 disc.

```powershell
git clone --recurse-submodules https://github.com/MattSG/mm3-recomp.git
cd mm3-recomp
pwsh scripts\setup.ps1 -Iso "D:\Games\Midtown Madness 3 (USA).iso"
pwsh scripts\play.ps1            # -Keyboard to drive with the keyboard
```

`setup.ps1` checks the toolchain, makes `tools\venv` (pinned capstone), verifies the disc and
extracts it to `game_files\`, generates the translated C into `src\recomp\gen\` (about six
minutes, deterministic) and builds `build\RelWithDebInfo\mm3_recomp.exe`. Re-running skips
finished steps; `-Force` redoes them. Nothing from the disc is committed.

### Which disc

Any dump whose `default.xbe` carries the retail code: a redump `.iso` (video + game partitions)
or a bare XISO. `scripts\extract_disc.py` gates on a SHA-256 of the XBE's section data
(`9c437324…6bbf8ba4`) with the one byte XISO tools patch to skip the DVD media check
(VA `0x87226`, `jge` → `jmp`) normalised, so region/certificate differences and that patch do
not matter. USA and Europe/Australia share the same code. Extraction applies that same media
patch to `game_files\default.xbe` (the emulated drive can't authenticate a pressed disc), so every
supported disc translates identically. Images tested end to end:

| SHA-1 | Image |
|---|---|
| `52cd998731eb67477b44c49c4152f56c0fc58a56` | Midtown Madness 3 (USA) — redump `.iso`, 7.8 GB |
| `8ed003adaf504883afc980d2888df6125496d044` | Midtown Madness 3 (Europe, Australia) (En,Fr,De,Es,It) — XISO, 3.6 GB |

An untested image with matching code is accepted with a note; anything else is refused with
this list.

### Regenerating

`scripts\generate_mm3.py` is the only generation entry point. It runs the recompiler pinned to
toolkit commit `967ec30` (extracted to `tools\.generator\`) plus one later lifter fix,
`fld tbyte` as an 80-bit load (toolkit `cd491b1`). Without it the CRT `pow()` range check
read 0.0, every `pow()` overflowed, and cars never got past the low gears. The submodule's
newer recompiler (upstream v0.13.x) translates ~3.6k functions differently and has not been
through a play session yet; try it with `MM3_GENERATOR_DIR=tools\xboxrecomp`. The hooks in
`CMakeLists.txt` find each wrapped function's chunk themselves, so any regeneration builds
without edits.

## Running

`scripts\play.ps1` sets the defaults below and runs the build from the repository root (the
game reads `game_files\`). By hand:

```powershell
$env:RECOMP_PB_EXEC='1'; $env:RECOMP_FB_WINDOW='1'; $env:RECOMP_USB='1'
$env:RECOMP_APU_DSP_ACK='0x80458810'
build\RelWithDebInfo\mm3_recomp.exe
```

**Frame rates above 60:** the race AI leaves the line correctly at every rate tested
(`tools\powershell\Test-MM3AiStart.ps1` passes at 60, 120, 240 and ~350 fps, and reports the
rate the race ran at). The car's physics is not fully rate-independent: from a standstill on
a straight road it covers ~5% less ground in 6 s at 240 fps than at 60 fps. 60 stays the default.

F11 or Alt+Enter toggles fullscreen. The window size sets the internal resolution unless pinned.

### Options

| Variable | Default | Effect |
|---|---|---|
| `RECOMP_PB_EXEC` | off | Execute the push buffer on the D3D11 renderer. **Required** for the game to draw. |
| `RECOMP_FB_WINDOW` | off | Open the game window. **Required.** |
| `RECOMP_USB` | off | Emulate the USB controller ports so pads (XInput) reach the game. **Required** for input. |
| `RECOMP_APU_DSP_ACK` | unset | DSP acknowledgement address(es) the audio needs; use `0x80458810`. |
| `RECOMP_KEYBOARD` | off | `1`: keyboard as a pad (arrows, Z X A S = A B X Y, Enter = Start, Backspace = Back). |
| `RECOMP_FPS_LIMIT` | `60` | Frame-rate cap (the emulated vblank). `0` = uncapped. Above 60 the car accelerates slightly slower (~5% at 240 fps); see above. |
| `RECOMP_VSYNC` | `0` | Present sync interval passed to DXGI. |
| `RECOMP_FULLSCREEN` | off | Start fullscreen. |
| `RECOMP_WINDOW_SIZE` | auto | Initial window client size, `WxH`. |
| `RECOMP_ASPECT` | window | Pin the display aspect: `16:9`, `4:3`, or a number. Widescreen UI follows it. |
| `RECOMP_SUPERSAMPLE` | `2` | Render N×N pixels per window pixel (1–4). |
| `RECOMP_RENDER_SCALE` | auto | Pin internal resolution as a multiple of 480 lines (0.5–8); overrides the above. |
| `RECOMP_MSAA` | `4` | MSAA sample count (1/2/4/8). |
| `RECOMP_ANISO` | `16` | Anisotropic filtering (1–16). |
| `RECOMP_DRAW_DISTANCE` | `4` | LOD distance multiplier (1 = Xbox, up to 4): pedestrians, traffic, object detail, blob shadows, particle overdraw cap. Stays inside the Xbox view distance. |
| `RECOMP_VIEW_DISTANCE` | `1` | City view distance and far-plane multiplier (1 = Xbox, up to 4). Above 1 the game draws its low-detail far city: flat untextured blocks and a haze band past ~700 m, and trees standing in front of the haze. Facade detail zones stay at the Xbox distances. |
| `RECOMP_PS_SPEC` | `1` | `0`: skip specialised pixel shaders (slow; for debugging). |
| `MM3_SAVE_DIR` | built-in | Folder holding the HDD/save partition images. |
| `MM3_DEBUG_TELEPORT` | off | Teleport debugger: a panel with live X/Y/Z and heading that places the car on the ground at a typed X/Z and heading (degrees), and the pipe `tools\powershell\Send-MM3Teleport.ps1` drives. See `DEBUG_TELEPORT.md`. |
| `MM3_DEV_HUD` | off | `1`: the retail build's own developer HUD (position, damage, speed/RPM/gear). Needs `tools\debug-features\prepare.py` first. |
| `MM3_DEBUG_MENU` | off | `1`: the retail developer menu (D-pad to select and change). Same prerequisite. |
| `MM3_EXTENDED_CAMERAS` | off | `1`: the developer camera modes. Same prerequisite. |
| `MM3_E3_PRESET` | off | `1`: run the shipped `E3settings.lua`. Same prerequisite. |

### Automation and measurement

| Variable | Effect |
|---|---|
| `RECOMP_PAD_LIVE=<file>` | Read pad input appended to a file, one `button:hold_ms` per line (e.g. `a:150`). |
| `RECOMP_PAD_SCRIPT=<spec>` | Timed scripted pad input (format in `src/usb/usb_gamepad.c`). |
| `RECOMP_PAD_PRESS=<mask>[,period,hold]` | Pulse digital buttons (`0x10` = Start) periodically. |
| `RECOMP_FRAME_CSV=<file>` | One row per flip: QPC time, frame time, pacing time. Summarise with `tools\xboxrecomp\tools\bench\frames.py`. |
| `RECOMP_D3D_PROFILE` | Print per-second fps, worst frame and renderer stage costs to stderr. |
| `RECOMP_PRESENT_CAPTURE=<prefix>` | With `<prefix>.flag` containing N, save the next N frames as `<prefix>-*.bmp`. |
| `RECOMP_CAPTURE_SURFACE` | Capture at internal resolution instead of the window. |
| `RECOMP_SPIKE_CAPTURE=<prefix>[,ms]` | Log every flip slower than `ms` (default 50) as `[SPIKE]` and save the next two frames as `<prefix>-<spike>-<n>.bmp` (at most one capture per 2 s). Each spike is followed by a `[SPIKE]   ms:` line splitting that frame's time by renderer stage (`exec(all)` = push-buffer executor, `present` = swap-chain present) and how many specialised shaders became ready in the last second, then a `game thread` line: the functions the game's render thread was sampled in during the spike (`sub_<guest address>` = recompiled guest function; sampled ~1 kHz, no elevation needed), and the two commonest guest call chains (`[SPIKE]   chain N%: ...`); a wait inside the OS is named by its guest caller. |
| `RECOMP_XA2_PCM_DUMP=<file>` | Dump the mixed audio (s16 stereo 48 kHz). |
| `RECOMP_WATCHDOG_SECS=<n>` | After n seconds of running, report where the game thread is (for hangs). |

`tools\bench\run.ps1 -Exe <exe> -Save <save template>` runs the repeatable menu + race
benchmark (frame-time percentiles, lows, jitter, executor time, GPU use).
`tools\powershell\Test-MM3AiStart.ps1 -Exe <exe> -Save <save template> [-FpsLimit 60]` takes the
same path into the first race and checks every AI car leaves the line forwards (exit 0 = pass).
It also prints the player car's top speed under a held throttle: about 50 units/s in 30 s on
the bench save (19 with the throttle/gear bug).

The developer tools are experimental (see `tools\debug-features\README.md`): `prepare.py` adds
their Lua scripts to `Data_hd.zip` without moving the original entries, and `run.ps1` launches
an isolated instance with `-DevHud`, `-DebugMenu`, `-ExtendedCameras`, `-E3Preset` and
`-GraphicsPreset Retail|Default|Max`.

### Diagnostics

Off unless set; for debugging the runtime, not for play. See the source named for each.

- **Renderer** (`nv2a_d3d11.c`, `nv2a_pb_exec.c`): `RECOMP_D3D_TRACE`, `RECOMP_D3D_DEBUG`,
  `RECOMP_D3D_NO_WRITEBACK`, `RECOMP_D3D_SKIP_CMASK`, `RECOMP_D3D_SKIP_SFACTOR`,
  `RECOMP_PB_SCAN`, `RECOMP_PB_EXEC_VERBOSE`, `RECOMP_PB_REPORT_MS`, `RECOMP_PB_UNHANDLED_ALL`,
  `RECOMP_PB_WRAP_TRACE`, `RECOMP_NV2A_TRACE`, `RECOMP_FRAME_TRACE`, `RECOMP_FRAME_TRACE_METHODS`,
  `RECOMP_FRAME_TRACE_CONSTANTS`, `RECOMP_TEX_DUMP`, `RECOMP_TEX_DUMP_EVERY`, `RECOMP_TEX_STATE`,
  `RECOMP_TEX_GENERIC`, `RECOMP_DXT_VALIDATE`, `RECOMP_FB_DUMP`, `RECOMP_FB_DUMP_FLIPS`, `RECOMP_FB_VA`,
  `RECOMP_FIND_NAN`, `RECOMP_FIND_QUAD`, `RECOMP_VSH_TRACE`, `RECOMP_VSH_DUMP`, `RECOMP_NO_VSH`,
  `RECOMP_NO_COMBINERS`, `RECOMP_RC_TRACE`, `RECOMP_RC_GENERIC`, `RECOMP_BLEND_GENERIC`,
  `RECOMP_SOFT_RASTER`, `RECOMP_RASTER_TEST`, `RECOMP_RASTER_THREADS`, `RECOMP_PEEK`,
  `RECOMP_PEEK_CHAIN`, `RECOMP_POKE`, `RECOMP_VBLANK`, `RECOMP_VIDEO_FLAGS`
- **Video/movies** (`fb_present.c`, `video_pump.c`): `RECOMP_PVIDEO_TRACE`, `RECOMP_PVIDEO_DUMP`,
  `RECOMP_PVIDEO_FRAME_AUDIT`, `RECOMP_FMV_DUMP`, `RECOMP_FMV_HOST`, `RECOMP_KEY_TRACE`
- **Audio** (`apu_*.c`): `RECOMP_APU_TRACE`, `RECOMP_APU_MIXDOWN_ALL`, `RECOMP_DSP_ACK`, `RECOMP_AC97_READY`.
  Always on: `[XA2] underrun after N buffers` in stderr marks each audible gap (output queue ran dry).
- **USB/input** (`ohci.c`, `usb_gamepad.c`): `RECOMP_USB_HC`, `RECOMP_USB_NDP`, `RECOMP_USB_PADS`,
  `RECOMP_USB_PORT`, `RECOMP_USB_STATS`, `RECOMP_USB_TRACE`, `RECOMP_INPUT_DIAG`
- **Kernel/memory/threads** (`kernel_*.c`, `xbox_memory_layout.c`): `RECOMP_IO_TRACE` (per-operation `[PATH]`/`[FILE]`/`[READ]` lines; off by default because a console write per disc read stalled the game), `RECOMP_KERNEL_SUMMARY` (kernel call counts every 2 s), `XBOX_LOG_LEVEL`,
  `RECOMP_KERNEL_WATCH`, `RECOMP_KERNEL_WATCH_ALL`, `RECOMP_KERNEL_LOG_BUDGET`, `RECOMP_IRQL_TRACE`,
  `RECOMP_CS_MODE`, `RECOMP_CS_WATCH`, `RECOMP_CS_TRACE_CRT`, `RECOMP_WORKERS`, `RECOMP_ASYNC_IO`,
  `RECOMP_CMDLINE`, `RECOMP_CONTIG_TRACE`, `RECOMP_HEAP_WATCH`, `RECOMP_HEAP_LARGE_BREAK`,
  `RECOMP_WATCH`, `RECOMP_WATCH_RAW`, `RECOMP_TRAP_NULL`, `RECOMP_FORCE_RETURN`,
  `RECOMP_THREAD_TLS_TRACE`, `RECOMP_TRACE_ARGS`, `RECOMP_TRACE_BUDGET`,
  `RECOMP_TRACE_DEREF`, `RECOMP_TRACE_PROFILE`
- **Game layer** (`src/`): `MM3_THREAD_MODE=inline`, `MM3_LOG_UNBUFFERED`, `MM3_TRACE_BOOT_STATE`,
  `MM3_ASSET_TRACE`, `MM3_STREAM_TRACE`, `MM3_POST_MOVIE_WATCHDOG`, `MM3_FRONTEND_ICALL_TRACE`,
  `MM3_FRONTEND_WRITE_WATCH`, `MM3_HEAP_FRONTIER`, `MM3_HEAP_FREE_TRACE`, `MM3_HEAP_RETURN_TRACE`,
  `RECOMP_HEAP_LINK_BREAK`, `RECOMP_HEAP_RETURN_BREAK`, `RECOMP_BINK_ALLOC_BREAK`,
  `RECOMP_STREAM_COPY_ORIGINAL`, `RECOMP_UNIMPL_TRAP`, `RECOMP_TRACE_EFD85`

### Build options

CMake options default to a clean build. `MM3_FRONTEND_WATCH`, `MM3_MOVIE_INPUT_TRACE`,
`MM3_TRACY` and `MM3_RECOMP_ABI_CHECK` add diagnostics that cost performance (the first
roughly halves menu frame rate); leave them off for play.
