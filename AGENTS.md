# AGENTS.md

## Running the game in xemu

xemu is a per-machine tool, not committed to the repo: `tools/xemu/xemu.exe`
(with `tools/xemu/{bios,mcpx,hdd}`). Its settings live outside the repo at
`%APPDATA%\xemu\xemu\xemu.toml` (Windows: `C:\Users\<user>\AppData\Roaming\xemu\xemu\xemu.toml`).

Key settings:
- `dvd_path` must point at a real, bare XDVDFS XISO, not a redump `.iso`.
  The working disc is already in the repo tree at
  `games/xiso/Midtown Madness 3 (Europe, Australia) (En,Fr,De,Es,It).xiso.iso`
  (3.6GB, single game partition, boots straight to the dashboard/game) — use
  that one, not `games/Midtown Madness 3 (USA).iso` (a two-part redump xemu
  can't read directly) and not a file under `conformance_tmp/` (scratch space
  that gets cleaned between runs).
- `display.vulkan.preferred_physical_device` must match a GPU actually present
  on the machine (check `GL_RENDERER` in xemu's stderr output). A stale/wrong
  device name here doesn't error loudly — it just silently falls back or
  misbehaves.

Do not pass a manual `-set drive.ide1-cd0.file=...` QEMU arg alongside
`dvd_path`/`-drive index=1,media=cdrom,...` — this xemu build no longer
defines a drive with id `ide1-cd0`, so the extra `-set` fails with:
`there is no drive "ide1-cd0" defined`. Just set `dvd_path` in `xemu.toml` (or
pass `-dvd_path=<iso>` on the CLI) and let xemu build the `-drive` args itself.

For a DVD boot, use xemu's selector `-boot d`; `-boot order=d` leaves this
build at the dashboard/insert-disc screen even when the CD drive is present.

The confirmed-working launch (default `%APPDATA%\xemu\xemu\xemu.toml` already
has `bootrom_path`/`flashrom_path`/`hdd_path`/`eeprom_path` set — see above):

```powershell
tools\xemu\xemu.exe -boot d
```

with `dvd_path` in that config set to
`games\xiso\Midtown Madness 3 (Europe, Australia) (En,Fr,De,Es,It).xiso.iso`.

If you only have the USA redump (`games/Midtown Madness 3 (USA).iso`, a
two-part video+game layout) and need to make your own bare XISO from it:
strip the ~387MB DVD-video partition that precedes the actual XDVDFS game
partition (see the `BASE_CANDIDATES` comment in
`tools/xboxrecomp/tools/xiso/xdvdfs.py` — this redump's game partition starts
at offset `0x18300000`). It's a straight byte copy from that offset to EOF.
Verify the result with `xdvdfs.py`'s `Xiso` class before trusting it — it
should report `base=0x0` and list `default.xbe` etc. Don't check the output
into git (7+GB); it's scratch, regenerate if needed.

If `xemu`'s home screen says "Configure machine settings to get started"
even though Settings → System shows all paths filled in: this was a stale
window from before `xemu.toml` got its GPU/DVD paths fixed, not a real
config-loading bug — quit xemu fully and relaunch to pick up the current
`xemu.toml`.

A `-config_path <file>.toml` must contain **all** of `[general]`, `[display]`,
`[display.vulkan]` and `[sys.files]` (not just `[sys.files]`) or xemu silently
fails to read `flashrom_path` and aborts with `Failed to load BIOS '(null)'`
without ever building a `-bios` arg. Easiest fix: copy the real
`xemu.toml` and only change the paths you need, rather than writing a config
from scratch.
