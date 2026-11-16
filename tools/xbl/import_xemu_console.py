"""Make the recomp sign in to Xbox Live (Insignia) as your xemu console.

EXPERIMENTAL: the import is verified, but sign-in with a real Insignia account
is not yet tested.

    python tools/xbl/import_xemu_console.py [--save-dir DIR] [--no-keys]

Xbox Live identifies the console, not just the player, so the recomp borrows
your xemu console's whole identity. It copies three things into the save dir:

  eeprom.bin        xemu's EEPROM: the serial number, MAC and online key the
                    kernel serves as factory settings
  Partition0.img    sectors 8..0x3FF of xemu's hard disk: the Xbox Live
                    accounts (make yours with Insignia's setup in xemu first)
  console_keys.txt  the HD key, plus this game's signature and LAN keys, read
                    from the kernel's exports in a running xemu over its gdb
                    stub. xemu is started with the game disc from xemu.toml
                    and closed once Midtown Madness 3 has loaded.

Saves are signed with a key the console derives, so the recomp's existing
saves are re-signed with the new one (a timestamped backup of the title's save
folder is kept beside it). Paths default to xemu.toml (%APPDATA%/xemu/xemu/xemu.toml). The keys are
secret to your console: they stay in the save dir and are never printed.
"""
import argparse
import os
import shutil
import socket
import struct
import subprocess
import sys
import time
import tomllib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools/saves"))
import resign_save  # noqa: E402
FIRST, END = 8 * 512, 0x400 * 512           # the HDD's config sectors
KERNEL = 0x80010000
XBE_BASE = 0x10000
# Kernel export ordinals: XboxHDKey, XboxSignatureKey, XboxLANKey.
KEYS = {"RECOMP_XBOX_HDKEY": 323, "RECOMP_XBOX_SIGKEY": 325, "RECOMP_XBOX_LANKEY": 353}


def default_save_dir():
    if os.environ.get("MM3_SAVE_DIR"):
        return Path(os.environ["MM3_SAVE_DIR"])
    if os.name == "nt":
        return Path(os.environ["LOCALAPPDATA"]) / "xboxrecomp"
    return Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")) / "xboxrecomp"


def xemu_config():
    root = Path(os.environ.get("APPDATA", Path.home() / ".local/share")) / "xemu/xemu/xemu.toml"
    return tomllib.loads(root.read_text()) if root.is_file() else {}


def qcow2_read(path, off, n):
    """n bytes at off of an uncompressed qcow2 image (unallocated reads zero)."""
    with open(path, "rb") as f:
        h = f.read(48)
        if h[:4] != b"QFI\xfb":
            raise SystemExit(f"{path}: not a qcow2 image")
        cs = 1 << struct.unpack(">I", h[20:24])[0]
        l1off = struct.unpack(">Q", h[40:48])[0]
        mask, out = 0x00FFFFFFFFFFFE00, bytearray()
        while n:
            ci, within = divmod(off, cs)
            take = min(n, cs - within)
            f.seek(l1off + 8 * (ci // (cs // 8)))
            l2 = struct.unpack(">Q", f.read(8))[0] & mask
            data = bytes(take)
            if l2:
                f.seek(l2 + 8 * (ci % (cs // 8)))
                e = struct.unpack(">Q", f.read(8))[0]
                if e & (1 << 62):
                    raise SystemExit("compressed qcow2 clusters are not supported")
                if e & mask:
                    f.seek((e & mask) + within)
                    data = f.read(take)
            out += data
            off += take
            n -= take
        return bytes(out)


def accounts(config_sectors):
    """Gamertags in the config sectors (XC_ONLINE user records), best effort."""
    names = []
    for i in range(0, len(config_sectors), 512):
        sector = config_sectors[i:i + 512]
        for j in range(0, 512 - 16, 4):
            tag = sector[j:j + 16].split(b"\0")[0]
            if 3 <= len(tag) <= 15 and tag.isascii() and tag.replace(b" ", b"").isalnum() \
                    and tag[:1].isalpha() and sector[j + len(tag):j + 16].strip(b"\0") == b"":
                names.append(tag.decode())
                break
    return names


class Gdb:
    """Just enough of the GDB remote protocol to read guest memory."""

    def __init__(self, port, timeout=60):
        deadline = time.monotonic() + timeout
        while True:
            try:
                self.sock = socket.create_connection(("127.0.0.1", port), timeout=10)
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise SystemExit(f"xemu's gdb stub did not open on port {port}")
                time.sleep(0.5)
        self.buf = b""
        self.command("?")                      # QEMU stops the VM on connect

    def _byte(self):
        while not self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise SystemExit("xemu closed the gdb connection")
            self.buf += chunk
        b, self.buf = self.buf[:1], self.buf[1:]
        return b

    def _packet(self):
        while self._byte() != b"$":
            pass
        data = b""
        while (b := self._byte()) != b"#":
            data += b
        self._byte(), self._byte()
        self.sock.sendall(b"+")
        return data.decode()

    def command(self, payload, reply=True):
        self.sock.sendall(b"$%s#%02x" % (payload.encode(), sum(payload.encode()) & 0xFF))
        return self._packet() if reply else None

    def read(self, address, size):
        out = b""
        while len(out) < size:
            n = min(size - len(out), 0x400)
            text = self.command(f"m{address + len(out):x},{n:x}")
            if text.startswith("E") or len(text) != 2 * n:
                raise OSError(f"cannot read {n} bytes at 0x{address + len(out):08X}")
            out += bytes.fromhex(text)
        return out

    def u32(self, address):
        return struct.unpack("<I", self.read(address, 4))[0]

    def resume(self):
        self.command("c", reply=False)

    def pause(self):
        self.sock.sendall(b"\x03")
        self._packet()

    def close(self):
        try:
            self.command("D")
        finally:
            self.sock.close()


def export_address(gdb, ordinal):
    nt = KERNEL + gdb.u32(KERNEL + 0x3C)
    exports = KERNEL + gdb.u32(nt + 0x78)
    base, count = gdb.u32(exports + 0x10), gdb.u32(exports + 0x14)
    if not base <= ordinal < base + count:
        raise OSError(f"kernel has no export {ordinal}")
    return KERNEL + gdb.u32(KERNEL + gdb.u32(exports + 0x1C) + 4 * (ordinal - base))


def certificate(xbe):
    """Title id and the encrypted LAN/signature keys the kernel derives its exports from."""
    cert = struct.unpack_from("<I", xbe, 0x118)[0] - struct.unpack_from("<I", xbe, 0x104)[0]
    return struct.unpack_from("<I", xbe, cert + 8)[0], xbe[cert + 0xB0:cert + 0xD0]


def read_keys(xemu, port, want, timeout):
    """Run xemu with its gdb stub; once the title is running, read its kernel's keys."""
    info = None
    if os.name == "nt":
        info = subprocess.STARTUPINFO()
        info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        info.wShowWindow = 7                    # SW_SHOWMINNOACTIVE: no focus stealing
    proc = subprocess.Popen([str(xemu), "-boot", "d", "-gdb", f"tcp:127.0.0.1:{port}"],
                            cwd=xemu.parent, startupinfo=info,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        gdb = Gdb(port)
        deadline = time.monotonic() + timeout
        try:
            while True:
                try:
                    cert = gdb.u32(XBE_BASE + 0x118)
                    running = (gdb.u32(cert + 8), gdb.read(cert + 0xB0, 0x20)) if cert else (0, b"")
                except OSError:
                    running = (0, b"")
                if running[0] == want[0] and running[1] != want[1]:
                    raise SystemExit("xemu runs a different release of the game than game_files/ "
                                     "(certificate keys differ): set xemu's dvd_path to the disc "
                                     "game_files/ was extracted from")
                if running == want:
                    keys = {name: gdb.read(export_address(gdb, ordinal), 16)
                            for name, ordinal in KEYS.items()}
                    if all(any(k) for k in keys.values()):
                        return keys
                if time.monotonic() > deadline:
                    raise SystemExit(f"Midtown Madness 3 (title {want[0]:08X}) did not start in "
                                     f"xemu within {timeout} s; last title {running[0]:08X}")
                gdb.resume()
                time.sleep(3)
                gdb.pause()
        finally:
            gdb.close()
    finally:
        proc.terminate()
        try:
            proc.wait(10)
        except subprocess.TimeoutExpired:
            proc.kill()


def saved_keys(save):
    path = save / "console_keys.txt"
    if not path.is_file():
        return {}
    pairs = (line.split("=", 1) for line in path.read_text().splitlines() if "=" in line)
    return {name: bytes.fromhex(value) for name, value in pairs}


def save_folders(save, game_dir):
    """The title's save folders where the game reads them: the save dir's UDATA,
    else (RECOMP_SAVE_DIR unset) a UDATA older builds left in the game dir."""
    root = Path(os.environ.get("RECOMP_SAVE_DIR") or save)
    udata = root / "UDATA"
    if not udata.is_dir() and not os.environ.get("RECOMP_SAVE_DIR"):
        udata = game_dir / "UDATA"
    title = udata / resign_save.TITLE_ID
    return sorted(p for p in title.iterdir() if p.is_dir()) if title.is_dir() else []


def resign_saves(folders, old, new):
    """Saves are signed with the title signature key, which the console's kernel
    derives: re-sign those valid under the old key, after backing them up."""
    if old == new or not folders:
        return
    title = folders[0].parent
    backup = title.parent.with_name(f"UDATA.backup-{time.strftime('%Y%m%d-%H%M%S')}") / title.name
    shutil.copytree(title, backup)
    for folder in folders:
        for path in sorted(folder.iterdir()):
            if not path.is_file() or path.suffix.lower() == ".xbx" or path.stat().st_size <= resign_save.SIGNATURE:
                continue
            data = path.read_bytes()
            if data[-resign_save.SIGNATURE:] == resign_save.signature(old, data):
                path.write_bytes(data[:-resign_save.SIGNATURE] + resign_save.signature(new, data))
                print(f"Re-signed {folder.name}/{path.name}")
            elif data[-resign_save.SIGNATURE:] != resign_save.signature(new, data):
                print(f"Left {folder.name}/{path.name}: not signed with the previous key")
    print(f"Saves backed up to {backup}")


def main(argv=None):
    config = xemu_config()
    files = config.get("sys", {}).get("files", {})
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--save-dir", type=Path, default=default_save_dir())
    ap.add_argument("--hdd", type=Path, default=files.get("hdd_path"), help="xemu's xbox_hdd.qcow2")
    ap.add_argument("--eeprom", type=Path, default=files.get("eeprom_path"), help="xemu's eeprom.bin")
    ap.add_argument("--xemu", type=Path, default=REPO / "tools/xemu/xemu.exe")
    ap.add_argument("--xbe", type=Path, default=REPO / "game_files/default.xbe")
    ap.add_argument("--gdb-port", type=int, default=1234)
    ap.add_argument("--timeout", type=int, default=180, help="seconds for xemu to reach the game")
    ap.add_argument("--no-keys", action="store_true", help="copy the EEPROM and accounts only")
    args = ap.parse_args(argv)
    for name in ("hdd", "eeprom"):
        if not getattr(args, name) or not Path(getattr(args, name)).is_file():
            raise SystemExit(f"xemu {name} not found: pass --{name} or set it in xemu.toml")
    eeprom = Path(args.eeprom).read_bytes()
    if len(eeprom) != 256:
        raise SystemExit(f"{args.eeprom}: not a 256-byte EEPROM image")

    save = args.save_dir
    save.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.eeprom, save / "eeprom.bin")
    print(f"EEPROM: serial {eeprom[0x34:0x40].decode(errors='replace')}, "
          f"MAC {eeprom[0x40:0x46].hex(':')}")

    config_sectors = qcow2_read(args.hdd, FIRST, END - FIRST)
    image = save / "Partition0.img"
    with open(image, "r+b" if image.exists() else "w+b") as f:
        f.seek(FIRST)
        f.write(config_sectors)
    tags = accounts(config_sectors)
    print(f"Accounts: {', '.join(tags) if tags else 'none found (create one with Insignia in xemu first)'}")

    if not args.no_keys:
        want = certificate(args.xbe.read_bytes())
        print(f"Starting xemu to read the console keys (waiting for title {want[0]:08X})...")
        keys = read_keys(args.xemu, args.gdb_port, want, args.timeout)
        old = saved_keys(save).get("RECOMP_XBOX_SIGKEY", bytes(16))
        resign_saves(save_folders(save, args.xbe.parent), old, keys["RECOMP_XBOX_SIGKEY"])
        (save / "console_keys.txt").write_text(
            "".join(f"{name}={value.hex().upper()}\n" for name, value in keys.items()))
        print(f"Keys: {len(keys)} read from xemu's kernel")
    print(f"Imported into {save}")


if __name__ == "__main__":
    sys.exit(main())
