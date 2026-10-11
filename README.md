# Midtown Madness 3 recompilation

Static recompilation of the Xbox title Midtown Madness 3 to native x86-64 Windows, on the
[xboxrecomp](tools/xboxrecomp) runtime, rendering through Direct3D 11.

## Building from a fresh clone

You need Windows 10/11 x64, Visual Studio 2022 or newer with **Desktop development with C++**
(it includes CMake), Python 3.11+ (the `py` launcher), git, and your own Midtown Madness 3 disc.

```powershell
git clone --recurse-submodules https://github.com/lakshya16157/mm3-recomp.git
cd mm3-recomp
pwsh scripts\setup.ps1 -Iso "D:\Games\Midtown Madness 3 (USA).iso"
pwsh scripts\play.ps1            # -Keyboard to drive with the keyboard
```

`setup.ps1` checks the toolchain, makes `tools\venv` (pinned capstone), verifies the disc and
extracts it to `game_files\`, generates the translated C into `src\recomp\gen\` (about six
minutes, deterministic) and builds `build\RelWithDebInfo\mm3_recomp.exe`. Re-running skips
finished steps; `-Force` redoes them. Nothing from the disc is committed.

This repository includes its runtime fixes in `patches/xboxrecomp-mm3.patch`.
Setup applies them to the pinned Xbox runtime submodule automatically and safely
skips an already applied patch. The upstream toolkit and its generator history
remain available through the submodule. Before building manually, run
`python scripts/apply_runtime_patch.py` once after initializing submodules.

For a checkout with local tools installed, activate them in the current PowerShell session:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
. .\scripts\enter-env.ps1
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\setup.ps1 -Iso "D:\Games\Midtown Madness 3 (USA).iso"
```

The local environment uses `tools\venv`, portable PowerShell at `tools\bin\powershell`,
and MSVC/Windows SDK at `tools\bin\msvc` (via `setup_x64.bat`). With local MSVC,
install CMake and Ninja in the venv (`python -m pip install "cmake>=3.20,<4" ninja`);
setup uses Ninja Multi-Config and keeps the same `build\<Config>` output paths.
Run these commands from the repository directory, not its parent folder.

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
toolkit commit `967ec30` (extracted to `tools\.generator\`): its output is the translation the
game was proven on, and regenerating reproduces it function for function. The submodule's
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

The game enforces a maximum of **60 FPS** for presentation and emulated vblank.
`RECOMP_FPS_LIMIT` may select a lower integer limit from 1 to 60; unset, invalid,
uncapped (`0`), or higher settings use 60, including when launching the executable directly.

F11 or Alt+Enter toggles fullscreen. The game renders at 2304×1440 by default with a
16:10 aspect, matching a 3840×2400 display, and scales the image to the window or
fullscreen size. The internal height stays fixed when you maximize the window.
Set `RECOMP_RENDER_SCALE=auto` to follow the window resolution instead;
`RECOMP_SUPERSAMPLE` applies in that mode. Set `RECOMP_ASPECT=auto` to follow the
window's aspect, or select another aspect explicitly.

With `scripts\play.ps1 -Keyboard`, hold **Up** to accelerate, **Down** to brake/reverse,
and **Left/Right** to steer. Arrows also navigate menus. **Z** confirms, **X** goes back,
and **Enter/Esc** pauses/resumes. **C** changes the camera view while driving. The original
**3** accelerator, **1** brake and numpad **4/6** steering keys still work. Keyboard input
releases when the game loses focus.
Choose **Manual** transmission in car setup, then tap **R** to shift up and **F** to shift down
(the default Xbox B/X gear controls). These aliases operate only while driving.
Set `RECOMP_KEYBOARD_LAYOUT=legacy` to use only the original runtime bindings.

All 35 car models included on the supported disc are selectable, including the hidden bonus
cars. Event-specific vehicle classes still apply. Bonus paints and career statistics continue
to follow profile progress. Set `MM3_UNLOCK_ALL_CARS=0` to restore the original car locks;
the availability change does not write unlock keys to your save.

### Options

| Variable | Default | Effect |
|---|---|---|
| `RECOMP_PB_EXEC` | off | Execute the push buffer on the D3D11 renderer. **Required** for the game to draw. |
| `RECOMP_FB_WINDOW` | off | Open the game window. **Required.** |
| `RECOMP_USB` | off | Emulate the USB controller ports so pads (XInput) reach the game. **Required** for input. |
| `RECOMP_APU_DSP_ACK` | unset | DSP acknowledgement address(es) the audio needs; use `0x80458810`. |
| `RECOMP_KEYBOARD` | off | `1`: keyboard as a pad (arrows drive/navigate, Z X A S = A B X Y, Enter = Start, Backspace = Back). |
| `RECOMP_KEYBOARD_LAYOUT` | `mm3` | Arrow driving controls; `legacy` restores the original D-pad-only arrows and 1/3 trigger keys. |
| `RECOMP_FPS_LIMIT` | `60` | Presentation and emulated-vblank cap, 1–60. Uncapped, invalid, or higher values use 60. |
| `RECOMP_VSYNC` | `0` | Present sync interval passed to DXGI. |
| `RECOMP_FULLSCREEN` | off | Start fullscreen. |
| `RECOMP_WINDOW_SIZE` | auto | Initial window client size, `WxH`. |
| `RECOMP_ASPECT` | `16:10` | Pin the display aspect: `16:10`, `16:9`, `4:3`, or a number. `auto` follows the window. Widescreen UI follows it. |
| `RECOMP_SUPERSAMPLE` | `2` | In auto resolution mode, render N×N pixels per window pixel (1–4). |
| `RECOMP_RENDER_SCALE` | `3.0` | Internal height as a multiple of 480 lines (0.5–8): default 1440, `2.25` = 1080, `1.5` = 720. `auto` follows the window and uses supersampling. |
| `RECOMP_MSAA` | `4` | MSAA sample count (1/2/4/8). |
| `RECOMP_ANISO` | `16` | Anisotropic filtering (1–16). |
| `RECOMP_DRAW_DISTANCE` | `2` | View/LOD distance multiplier (1 = Xbox, up to 4). |
| `RECOMP_SHARP_POINT` | `1` | `0`: plain point filtering for fonts and menu art instead of sharp outlines. |
| `RECOMP_PS_SPEC` | `1` | `0`: skip specialised pixel shaders (slow; for debugging). |
| `RECOMP_PS_PREWARM` | `1` | Prepare previously used pixel shader states during startup; `0` disables learning and prewarming. |
| `RECOMP_SHADER_CACHE` | `1` | Save compiled shaders between launches; `0` disables the cache. |
| `RECOMP_SHADER_CACHE_DIR` | built-in | Shader cache folder; defaults to `%LOCALAPPDATA%\xboxrecomp\ShaderCache`. |
| `MM3_SAVE_DIR` | built-in | Folder holding the HDD/save partition images. |
| `MM3_UNLOCK_ALL_CARS` | `1` | Make every installed car selectable; `0` restores earned car availability. |
| `MM3_DEBUG_TELEPORT` | off | Enable the teleport pipe (`tools\powershell\Send-MM3Teleport.ps1`). |

### Automation and measurement

| Variable | Effect |
|---|---|
| `RECOMP_PAD_LIVE=<file>` | Read pad input appended to a file, one `button:hold_ms` per line (e.g. `a:150`). |
| `RECOMP_PAD_SCRIPT=<spec>` | Timed scripted pad input (format in `src/usb/usb_gamepad.c`). |
| `RECOMP_PAD_PRESS=<mask>[,period,hold]` | Pulse digital buttons (`0x10` = Start) periodically. |
| `RECOMP_FRAME_CSV=<file>` | One row per flip: QPC time, frame time, pacing time. Summarise with `tools\xboxrecomp\tools\bench\frames.py`. |
| `RECOMP_D3D_PROFILE` | Print per-second fps, worst frame and renderer stage costs to stderr. |
| `RECOMP_KERNEL_SUMMARY` | `1`: collect and print periodic kernel-call totals (off during normal play). |
| `RECOMP_CS_COUNTERS` | `1`: count critical-section entries/exits for contention diagnostics (off during normal play). |
| `RECOMP_HITCH_TRACE` | `1`: report slow command submissions, shader compilation, GPU waits, and file reads with QPC timestamps (off during normal play). |
| `RECOMP_PB_CAPTURE=<prefix>` | Diagnose the first bounded-parser limit by saving fetched command words, parser state, and guest RAM; disabled during normal play. |
| `RECOMP_PRESENT_CAPTURE=<prefix>` | With `<prefix>.flag` containing N, save the next N frames as `<prefix>-*.bmp`. |
| `RECOMP_CAPTURE_SURFACE` | Capture at internal resolution instead of the window. |
| `RECOMP_XA2_PCM_DUMP=<file>` | Dump the mixed audio (s16 stereo 48 kHz). |
| `RECOMP_WATCHDOG_SECS=<n>` | After n seconds of running, report where the game thread is (for hangs). |

`tools\bench\run.ps1 -Exe <exe> -Save <save template>` runs the repeatable menu + race
benchmark (frame-time percentiles, lows, jitter, executor time, GPU use).
`tools\powershell\Test-MM3AiStart.ps1 -Exe <exe> -Save <save template> [-FpsLimit 60]` takes the
same path into the first race and checks every AI car leaves the line forwards (exit 0 = pass).

Compiled shaders are cached automatically. New specialised gameplay shaders
compile in the background. The runtime remembers up to 512 exact shader states
and prepares them during later startups using the existing two background
workers; shaders requested by current draws take priority. The first visit to
a new state still uses the generic shader while compilation finishes. Changes
to shader source or compiler settings invalidate the matching cache entries.
Disabling the shader cache also disables state learning and prewarming.

The renderer selects the high-performance GPU on laptops with integrated and
dedicated graphics. Guest workers and shader compilation run on separate host
threads; rendering keeps the ordering that the game expects. Texture conversion
uses contiguous tile/block copies, and repeated guest locks reuse a thread-local
lookup. Audio DMA also reuses successful address lookups per thread, avoiding
allocator locks while preserving gaps and newly allocated ranges. Optimized
MSVC builds allow automatic inlining in the DSP interpreter; positional-audio
filtering reuses ring indices while retaining the same floating-point operations.
`MM3_DSP_AUTO_INLINE=OFF` restores the compiler's default inlining setting for
build comparisons. Normal play skips shared diagnostic counters and redundant
GPU flushes.
Pixel shader constants reuse their last successful upload only when all bytes,
including dynamic lighting, fog, alpha and texture scales, are identical.
GPU readbacks use one completion query per copy batch before mapping the pixels;
query failures retain the original per-surface polling path. Pushbuffer packets
and called command lists pause at the published PUT position and resume with
their packet and return state intact. A bounded parser pass cannot acknowledge
commands it has not consumed.
MM3 fences complete from executed semaphore-release commands after pending
readbacks finish. Advancing the game's next-fence counter does not complete
an unsubmitted command.
Both queue-pointer views follow actual command consumption. GPU notifications
become visible after their matching consumed position and readbacks, so a ring
wrap cannot wake the producer with a read pointer from the previous lap.
Native waits retain the game's final queue and GPU-idle checks.
Collision-call fallbacks preserve the argument block reused by the following
helper, preventing a second stack cleanup from consuming saved registers.
Supported DXT textures upload their original compressed blocks for GPU decoding,
reducing upload traffic and texture memory. Short mip chains retain CPU conversion
and generated mip levels. Native BC decoding follows Direct3D's precision rules
and can differ slightly from the integer CPU decoder.
GPU completion wakes waiting guest threads instead of keeping a CPU core busy;
the same command, notification, and fence completion checks still apply.
These reduce CPU work without lowering resolution, filtering, or draw distance.
Frame-time benchmarks exclude screenshot capture, which adds a GPU readback and
disk writes that can create spikes of its own.

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
- **Audio** (`apu_*.c`): `RECOMP_APU_TRACE`, `RECOMP_APU_MIXDOWN_ALL`, `RECOMP_DSP_ACK`, `RECOMP_AC97_READY`
- **USB/input** (`ohci.c`, `usb_gamepad.c`): `RECOMP_USB_HC`, `RECOMP_USB_NDP`, `RECOMP_USB_PADS`,
  `RECOMP_USB_PORT`, `RECOMP_USB_STATS`, `RECOMP_USB_TRACE`, `RECOMP_INPUT_DIAG`
- **Kernel/memory/threads** (`kernel_*.c`, `xbox_memory_layout.c`): `XBOX_LOG_LEVEL`,
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
