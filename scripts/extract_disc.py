"""Check a Midtown Madness 3 disc image and extract it to game_files/.

    python scripts/extract_disc.py <disc.iso> [--out game_files] [--skip-iso-hash]

Accepts a redump .iso (video + game partition) or a bare XISO. The recompiled
code is tied to the title's code bytes, so the gate is a hash of default.xbe's
section data, not of the image: header/certificate (region) differences and
the common XISO "media patch" do not matter; extraction applies that patch,
since the emulated drive cannot authenticate pressed media. The whole-image
hash is only compared against dumps this project was tested with.
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / 'tools/xboxrecomp'))
from tools.xiso.xdvdfs import Xiso, XisoError  # noqa: E402

# sha256 of default.xbe's sections, media-check branch normalised (see below).
CODE_SHA256 = '9c437324a42a8dbd2d7dd6852969f942ad2da3a6a513196b068db4776bbf8ba4'
# VA 0x87226: jge (0x7D) on pressed discs, jmp (0xEB) after an XISO media patch.
MEDIA_PATCH_VA, MEDIA_PATCH_BYTES = 0x87226, (0x7D, 0xEB)
TESTED_IMAGES = {  # sha1 -> description
    '52cd998731eb67477b44c49c4152f56c0fc58a56': 'Midtown Madness 3 (USA) redump .iso',
    '8ed003adaf504883afc980d2888df6125496d044':
        'Midtown Madness 3 (Europe, Australia) (En,Fr,De,Es,It) XISO',
}


def media_patch_offset(xbe: bytes):
    """File offset of the media-check branch, or None."""
    base = struct.unpack_from('<I', xbe, 0x104)[0]
    count, table = struct.unpack_from('<II', xbe, 0x11C)
    for i in range(count):
        _, va, _, raw, size, _ = struct.unpack_from('<6I', xbe, table - base + i * 0x38)
        if va <= MEDIA_PATCH_VA < va + size:
            return raw + MEDIA_PATCH_VA - va
    return None


def code_sha256(xbe: bytes) -> str:
    base = struct.unpack_from('<I', xbe, 0x104)[0]
    count, table = struct.unpack_from('<II', xbe, 0x11C)
    h = hashlib.sha256()
    for i in range(count):
        _, va, _, raw, size, _ = struct.unpack_from('<6I', xbe, table - base + i * 0x38)
        data = bytearray(xbe[raw:raw + size])
        if va <= MEDIA_PATCH_VA < va + size and data[MEDIA_PATCH_VA - va] in MEDIA_PATCH_BYTES:
            data[MEDIA_PATCH_VA - va] = MEDIA_PATCH_BYTES[0]
        h.update(struct.pack('<II', va, size))
        h.update(data)
    return h.hexdigest()


def file_sha1(path: Path) -> str:
    h = hashlib.sha1()
    with open(path, 'rb') as f:
        while chunk := f.read(1 << 24):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('iso', type=Path)
    ap.add_argument('--out', type=Path, default=REPO / 'game_files')
    ap.add_argument('--skip-iso-hash', action='store_true', help='skip the (slow) whole-image hash')
    args = ap.parse_args()

    try:
        iso = Xiso(str(args.iso))
    except (OSError, XisoError) as e:
        sys.exit(f'{args.iso}: not a readable Xbox disc image ({e})')
    with iso:
        entry = iso.find('default.xbe')
        if entry is None:
            sys.exit(f'{args.iso}: no default.xbe in the game partition')
        xbe = bytearray(iso.read(entry))
        digest = code_sha256(xbe)
        if digest != CODE_SHA256:
            sys.exit(f'{args.iso}: default.xbe code hash {digest}\n'
                     f'  expected {CODE_SHA256}\n'
                     '  This is not a Midtown Madness 3 release this project supports. Tested dumps:\n' +
                     '\n'.join(f'    sha1 {k}  {v}' for k, v in TESTED_IMAGES.items()))
        print(f'default.xbe code matches ({digest[:16]}...)')
        if not args.skip_iso_hash:
            sha1 = file_sha1(args.iso)
            print(f'image sha1 {sha1}: ' + TESTED_IMAGES.get(
                sha1, 'not a dump this project was tested with, but its code matches'))
        args.out.mkdir(parents=True, exist_ok=True)
        files = list(iso.walk())
        for n, (d, e) in enumerate(files, 1):
            dest = args.out.joinpath(*(d.split('/') if d else []), e.name)
            if not d and e.name.lower() == 'default.xbe':
                # Pressed discs ask the drive to authenticate the media, which the
                # emulated drive cannot; apply the standard XISO media patch so
                # every supported disc runs (and translates) identically.
                xbe[media_patch_offset(xbe)] = MEDIA_PATCH_BYTES[1]
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(xbe)
            else:
                iso.extract(e, str(dest))
            if n % 200 == 0 or n == len(files):
                print(f'  extracted {n}/{len(files)}', flush=True)
    print(f'game files ready in {args.out}')


if __name__ == '__main__':
    main()
