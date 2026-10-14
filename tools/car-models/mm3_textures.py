"""Encode ordinary DDS textures in MM3's CDDS container.

The Huffman codebooks follow Daniel Stien's decdds research (see the bundled
codebook provenance). CDDS is a bit stream over little-endian 32-bit words;
the header and pixels use separate codebooks without an alignment boundary.
"""
from __future__ import annotations

import argparse
import io
import json
import struct
from pathlib import Path

from PIL import Image


class Bits:
    def __init__(self):
        self.words = bytearray()
        self.value = 0
        self.count = 0

    def write(self, value: int, count: int):
        self.value = (self.value << count) | value
        self.count += count
        while self.count >= 32:
            self.count -= 32
            self.words.extend(struct.pack('<I', self.value >> self.count))
            self.value &= (1 << self.count) - 1

    def finish(self) -> bytes:
        if self.count:
            self.words.extend(struct.pack('<I', self.value << (32 - self.count)))
        # The decoder looks ahead ten bits, and reads whole words.
        return bytes(self.words) + bytes(8)


def _books():
    doc = json.loads(Path(__file__).with_name('cdds_codebooks.json').read_text())
    return {name: {int(k): tuple(v) for k, v in book.items()}
            for name, book in doc['books'].items()}


def _encode_block(data: bytes, book: dict, bits: Bits):
    pos = 0
    while pos < len(data):
        best_length, best_distance = 0, 0
        for distance in range(1, min(pos, 32) + 1):
            length = 0
            while (length < 9 and pos + length < len(data)
                   and data[pos + length] == data[pos + length - distance]):
                length += 1
            if length > best_length:
                best_length, best_distance = length, distance
        if best_length >= 2:
            # Retail's decoder counts the final literal differently from a
            # back-reference. Do not strand one byte after a repeated suffix.
            if pos + best_length == len(data) - 1:
                best_length -= 1
        if best_length >= 2:
            symbol = 0x100 + (best_distance - 1) * 8 + best_length - 2
            pos += best_length
        else:
            symbol = data[pos]
            pos += 1
        bits.write(*book[symbol])


def dds_to_cdds(dds: bytes) -> bytes:
    if len(dds) < 128 or dds[:4] != b'DDS ' or struct.unpack_from('<I', dds, 4)[0] != 124:
        raise ValueError('Expected a standard 128-byte DDS header')
    fourcc = dds[84:88]
    name = {b'DXT1': 'dxt1', b'DXT2': 'dxt23', b'DXT3': 'dxt23',
            b'DXT4': 'dxt45', b'DXT5': 'dxt45'}.get(fourcc, 'rgb')
    if name == 'rgb' and struct.unpack_from('<I', dds, 80)[0] & 4:
        raise ValueError('Unsupported DDS FourCC')
    height, width = struct.unpack_from('<2I', dds, 12)
    mips = struct.unpack_from('<I', dds, 28)[0] or 1
    if not (1 <= height <= 4096 and 1 <= width <= 4096 and 1 <= mips <= max(width, height).bit_length()):
        raise ValueError('Invalid DDS dimensions or mip count')
    if struct.unpack_from('<I', dds, 112)[0]:
        raise ValueError('Only 2D DDS textures are supported')
    bpp = struct.unpack_from('<I', dds, 88)[0]
    if name == 'rgb' and bpp not in (8, 16, 24, 32):
        raise ValueError('Unsupported DDS pixel size')
    expected = 0
    for mip in range(mips):
        w, h = max(1, width >> mip), max(1, height >> mip)
        expected += (w * h * bpp // 8 if name == 'rgb' else
                     ((w + 3) // 4) * ((h + 3) // 4) * (8 if name == 'dxt1' else 16))
    if len(dds) != 128 + expected:
        raise ValueError('DDS pixel payload does not match its dimensions and mip count')
    books, bits = _books(), Bits()
    _encode_block(dds[:128], books['hdr'], bits)
    _encode_block(dds[128:], books[name], bits)
    result = bits.finish()
    if result[:4] != struct.pack('<I', 0x990F44C8):
        raise ValueError('DDS header does not produce the required CDDS signature')
    return result


def image_to_dds(path: Path, *, max_size: int = 1024) -> bytes:
    if max_size < 4 or max_size > 4096 or max_size & (max_size - 1):
        raise ValueError('max_size must be a power of two between 4 and 4096')
    with Image.open(path) as source:
        image = source.convert('RGBA')
        image.thumbnail((max_size, max_size), Image.Resampling.LANCZOS)
        # Xbox DXT textures require power-of-two dimensions.
        dimensions = tuple(1 << (max(4, n) - 1).bit_length() for n in image.size)
        if image.size != dimensions:
            image = image.resize(dimensions, Image.Resampling.LANCZOS)
        pixel_format = 'DXT1' if image.getextrema()[3] == (255, 255) else 'DXT5'
        levels, header = [], None
        while True:
            stream = io.BytesIO()
            image.save(stream, format='DDS', pixel_format=pixel_format)
            encoded = stream.getvalue()
            if header is None:
                header = bytearray(encoded[:128])
            levels.append(encoded[128:])
            if image.size == (1, 1):
                break
            image = image.resize(tuple(max(1, n // 2) for n in image.size),
                                 Image.Resampling.LANCZOS)
        struct.pack_into('<I', header, 8, struct.unpack_from('<I', header, 8)[0] | 0x20000)
        struct.pack_into('<I', header, 28, len(levels))
        struct.pack_into('<I', header, 108, 0x401008)
    return bytes(header) + b''.join(levels)


def image_to_cdds(path: Path, *, max_size: int = 1024) -> bytes:
    return dds_to_cdds(image_to_dds(path, max_size=max_size))


def image_to_background_dds(path: Path) -> bytes:
    """Encode a 640x480 car-selection background like the native RGB565 asset.

    These full-screen UI images are linear, opaque and have no mip chain.
    Their non-power-of-two dimensions must not use the car-texture resizing
    path. The size field and caps retain the retail background header values.
    """
    with Image.open(path) as source:
        if source.size != (640, 480):
            raise ValueError('Car-selection backgrounds must be exactly 640x480')
        image = source.convert('RGB')
        pixels = bytearray()
        rgb = image.tobytes()
        for red, green, blue in zip(rgb[0::3], rgb[1::3], rgb[2::3]):
            pixels.extend(struct.pack('<H', (red >> 3) << 11 |
                                      (green >> 2) << 5 | (blue >> 3)))
    header = [0] * 32
    header[:8] = [0x20534444, 124, 0x100F, 480, 640, len(pixels), 0, 0]
    header[19:28] = [32, 0x40, 0, 16, 0xF800, 0x07E0, 0x001F, 0, 0x401008]
    return struct.pack('<32I', *header) + pixels


def image_to_background_cdds(path: Path) -> bytes:
    return dds_to_cdds(image_to_background_dds(path))


def image_to_menu_dds(path: Path) -> bytes:
    """Keep native-size vehicle icon pixels lossless in straight-alpha ARGB32.

    DXT color blocks share only four colors across sixteen pixels, which mixes
    bright body paint into nearby black tires and windows at these small UI
    sizes. MM3's DDS loader accepts A8R8G8B8 and performs any Xbox swizzling;
    ordinary DDS pixels remain linear BGRA bytes without premultiplication.
    """
    with Image.open(path) as source:
        if source.size not in ((128, 64), (64, 64), (256, 64)):
            raise ValueError('Vehicle menu icons must be 128x64, 64x64 or 256x64')
        width, height = source.size
        pixels = source.convert('RGBA').tobytes('raw', 'BGRA')
    header = [0] * 32
    header[:8] = [0x20534444, 124, 0x100F, height, width, width * 4, 0, 0]
    header[19:28] = [32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00,
                     0x000000FF, 0xFF000000, 0x401008]
    return struct.pack('<32I', *header) + pixels


def image_to_menu_cdds(path: Path) -> bytes:
    return dds_to_cdds(image_to_menu_dds(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--max-size', type=int, default=1024)
    args = parser.parse_args()
    result = (dds_to_cdds(args.input.read_bytes()) if args.input.suffix.lower() == '.dds'
              else image_to_cdds(args.input, max_size=args.max_size))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)
    print(f'{len(result)} bytes -> {args.output}')


if __name__ == '__main__':
    main()
