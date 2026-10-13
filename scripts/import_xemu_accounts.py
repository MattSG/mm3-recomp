"""Copy Xbox Live accounts from an xemu hard disk into the recomp's.

Xbox Live (and Insignia) accounts live in the raw config sectors at the start
of the disk, from sector 8 to the X: partition at sector 0x400; the game reads
them through \\Device\\Harddisk0\\Partition0, which the runtime backs with
Partition0.img in the save directory. They are bound to the console's HD key,
so the account signs in here only with that console's keys
(RECOMP_XBOX_HDKEY etc.).

    py scripts/import_xemu_accounts.py <xbox_hdd.qcow2> <save dir>
"""
import struct
import sys

FIRST, END = 8 * 512, 0x400 * 512


def qcow2_read(path, off, n):
    """n bytes at off of an uncompressed qcow2 image (unallocated reads zero)."""
    with open(path, 'rb') as f:
        h = f.read(48)
        if h[:4] != b'QFI\xfb':
            raise SystemExit('%s: not a qcow2 image' % path)
        cs = 1 << struct.unpack('>I', h[20:24])[0]
        l1off = struct.unpack('>Q', h[40:48])[0]
        mask, out = 0x00FFFFFFFFFFFE00, bytearray()
        while n:
            ci, within = divmod(off, cs)
            take = min(n, cs - within)
            f.seek(l1off + 8 * (ci // (cs // 8)))
            l2 = struct.unpack('>Q', f.read(8))[0] & mask
            data = bytes(take)
            if l2:
                f.seek(l2 + 8 * (ci % (cs // 8)))
                e = struct.unpack('>Q', f.read(8))[0]
                if e & (1 << 62):
                    raise SystemExit('compressed qcow2 clusters are not supported')
                if e & mask:
                    f.seek((e & mask) + within)
                    data = f.read(take)
            out += data
            off += take
            n -= take
        return bytes(out)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    src, img = sys.argv[1], sys.argv[2].rstrip('/\\') + '/Partition0.img'
    cfg = qcow2_read(src, FIRST, END - FIRST)
    with open(img, 'r+b') as f:
        f.seek(FIRST)
        f.write(cfg)
    used = [FIRST // 512 + i for i in range(len(cfg) // 512) if any(cfg[i * 512:i * 512 + 512])]
    print('copied config sectors 8..0x3ff to %s; non-empty: %s' % (img, [hex(s) for s in used]))


if __name__ == '__main__':
    main()
