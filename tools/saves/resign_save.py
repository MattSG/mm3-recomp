"""Re-sign Midtown Madness 3 saves copied from an Xbox so this port loads them.

The game signs every save file (profile, score) with a 20-byte HMAC-SHA1 at its
end, keyed by the kernel's XboxSignatureKey export, and calls the save damaged
when it does not match. A console's key is its own; this port's is 16 zero
bytes, or the 32 hex digits in RECOMP_XBOX_SIGKEY (src/main.c). So saves from
a console (often a Datel download with datelinfo.xbx beside them) load as
damaged until they are signed again with the port's key. Their data is fine.

The save folder name is no key: XAPI names it after the save's name,
h = (h * 65536 + UTF-16 code unit) mod 0xFFFFFFFFFFC5 as 12 hex digits
(sub_000808F2), so a folder from a console is already right.

    python tools/saves/resign_save.py <UDATA, its 4d53002a folder or one save folder> <new folder>

The input is never changed: the output folder gets a copy, every signed file in
it is re-signed and each folder name is checked against its SaveMeta.xbx.
Set RECOMP_XBOX_SIGKEY as you do for the game, or pass --key.
"""
import argparse
import hashlib
import hmac
import os
import shutil
from pathlib import Path

SIGNATURE = 20
TITLE_ID = "4d53002a"


def parse_key(text):
    """The port's signature key: 16 zero bytes, or 32 hex digits as src/main.c reads them."""
    if not text:
        return bytes(16)
    if len(text) != 32:
        raise ValueError("a signature key is 32 hex digits")
    return bytes.fromhex(text)


def folder_name(save_name):
    h = 0
    data = save_name.encode("utf-16-le")
    for i in range(0, len(data), 2):
        h = (h * 65536 + int.from_bytes(data[i:i + 2], "little")) % 0xFFFFFFFFFFC5
    return f"{h:012X}"


def save_name(meta):
    text = meta.read_bytes().decode("utf-16")
    for line in text.splitlines():
        if line.startswith("Name="):
            return line[5:]
    return None


def signature(key, data):
    return hmac.new(key, data[:-SIGNATURE], hashlib.sha1).digest()


def resign_folder(folder, key):
    """Re-sign every signed file in one save folder; returns a report line."""
    meta = folder / "SaveMeta.xbx"
    name = save_name(meta) if meta.is_file() else None
    notes = []
    if name is not None and folder_name(name) != folder.name.upper():
        notes.append(f"folder should be {folder_name(name)} for '{name}'")
    for path in sorted(folder.iterdir()):
        if not path.is_file() or path.suffix.lower() == ".xbx" or path.stat().st_size <= SIGNATURE:
            continue
        data = path.read_bytes()
        if data[-SIGNATURE:] == signature(key, data):
            notes.append(f"{path.name} already signed for the port")
            continue
        path.write_bytes(data[:-SIGNATURE] + signature(key, data))
        notes.append(f"{path.name} re-signed")
    return f"{folder.name} ({name or 'no SaveMeta'}): " + ", ".join(notes or ["nothing to sign"])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("--key", default=os.environ.get("RECOMP_XBOX_SIGKEY", ""),
                        help="the port's signature key (default: RECOMP_XBOX_SIGKEY, else zeros)")
    args = parser.parse_args(argv)
    try:
        key = parse_key(args.key)
    except ValueError as error:
        parser.error(f"--key: {error}")
    if args.target.exists():
        parser.error(f"{args.target} exists; pick a new output folder")
    shutil.copytree(args.source, args.target)
    # A UDATA folder, the title folder inside it, or one save folder.
    title = args.target / TITLE_ID if (args.target / TITLE_ID).is_dir() else args.target
    folders = [f for f in title.iterdir() if f.is_dir()] if title.name.lower() == TITLE_ID else [args.target]
    for folder in sorted(folders):
        print(resign_folder(folder, key))


if __name__ == "__main__":
    main()
