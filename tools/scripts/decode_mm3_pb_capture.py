"""Decode MM3 device fields from a runtime .state-initial/final.bin snapshot.

Usage: python tools/scripts/decode_mm3_pb_capture.py capture.state-initial.bin
The SDK captures raw bytes; these offsets belong to MM3's linked D3D library.
"""
import argparse
import json
from pathlib import Path
import struct


FIELDS = dict(cursor=0, limit=4, flags=8, ring_low=0x24, ring_high=0x28,
              next_fence=0x2C, fence_pointer=0x30, fence_index=0x34,
              fence_mask=0x38, ring_used=0x40, fence_table=0x48)


def decode_state(data):
    if len(data) < 72:
        raise ValueError("truncated snapshot header")
    header = struct.unpack_from("<18I", data)
    metadata_words = {1: 24, 2: 27}.get(header[0])
    if metadata_words is None:
        raise ValueError("unsupported snapshot version")
    stride = metadata_words * 4 + 0x2200
    expected = 72 + 4 * stride
    if header[1] != expected or len(data) != expected or header[17] > 4:
        raise ValueError("invalid snapshot size or mirror count")
    mirrors = []
    for index in range(header[17]):
        offset = 72 + index * stride
        pointer, device, valid, size = struct.unpack_from("<4I", data, offset)
        if size > 0x2200:
            raise ValueError("invalid raw device size")
        raw = offset + metadata_words * 4
        fields = {name: struct.unpack_from("<I", data, raw + field)[0]
                  if valid and field + 4 <= size else None
                  for name, field in FIELDS.items()}
        mirrors.append(dict(device_ptr_va=pointer, device_va=device, **fields))
    return dict(version=header[0], mirrors=mirrors)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    args = parser.parse_args()
    try:
        result = decode_state(args.snapshot.read_bytes())
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))
