"""Measure consecutive 24-bit BMP captures; candidates still need visual review."""
import argparse
import json
import struct
from pathlib import Path


def samples(path):
    data = path.read_bytes()
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height = struct.unpack_from('<ii', data, 18)
    bits, = struct.unpack_from('<H', data, 28)
    if data[:2] != b'BM' or bits != 24 or width <= 0 or height == 0:
        raise ValueError(f'Unsupported BMP: {path}')
    stride = (width * 3 + 3) & ~3
    # Ignore the outer 10 percent: letterboxing and ordinary HUD counters.
    return [tuple(data[offset + y * stride + x * 3:offset + y * stride + x * 3 + 3])
            for y in range(abs(height) // 10, abs(height) * 9 // 10, max(1, abs(height) // 36))
            for x in range(width // 10, width * 9 // 10, max(1, width // 64))]


def metrics(pixels, previous=None):
    return {
        'mean_rgb': sum(sum(p) for p in pixels) / (len(pixels) * 3),
        'near_black_fraction': sum(max(p) <= 3 for p in pixels) / len(pixels),
        'previous_mean_absolute_change': (sum(abs(a - b) for p, q in zip(pixels, previous)
                                              for a, b in zip(p, q)) / (len(pixels) * 3)
                                          if previous and len(previous) == len(pixels) else None),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, nargs='?')
    parser.add_argument('--start', type=int, default=0)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        black = [(0, 0, 0)] * 10
        white = [(255, 255, 255)] * 10
        assert metrics(black, white)['near_black_fraction'] == 1
        assert metrics(white, black)['previous_mean_absolute_change'] == 255
        assert metrics(white, white)['previous_mean_absolute_change'] == 0
        print('frame metrics self-test passed')
        return
    if not args.directory:
        parser.error('directory required')
    rows, previous = [], None
    for path in sorted(args.directory.glob('capture-*.bmp')):
        if int(path.stem.rsplit('-', 1)[1]) < args.start:
            continue
        pixels = samples(path)
        rows.append({'file': path.name, **metrics(pixels, previous)})
        previous = pixels
    result = {'frames': rows, 'blank_candidates': [r['file'] for r in rows
                                                 if r['near_black_fraction'] > .99]}
    (args.directory / 'frame-analysis.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({'count': len(rows), 'blank_candidates': result['blank_candidates'],
                      'largest_changes': sorted(rows, key=lambda r: r['previous_mean_absolute_change'] or 0,
                                                reverse=True)[:8]}, indent=2))


if __name__ == '__main__':
    main()
