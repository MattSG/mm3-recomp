import runpy
from pathlib import Path
import struct
import unittest

decode = runpy.run_path(str(Path(__file__).resolve().parents[1] /
                           "tools/scripts/decode_mm3_pb_capture.py"))["decode_state"]


class CaptureDecodeTest(unittest.TestCase):
    def test_layout_and_invalid_snapshots(self):
        for version, words in [(1, 24), (2, 27)]:
            data = bytearray(72 + 4 * (words * 4 + 0x2200))
            struct.pack_into("<2I", data, 0, version, len(data))
            struct.pack_into("<I", data, 68, 1)
            struct.pack_into("<4I", data, 72, 0x10000, 0x20000, 1, 0x28)
            struct.pack_into("<I", data, 72 + words * 4 + 0x24, 0x12345678)
            decoded = decode(data)["mirrors"][0]
            self.assertEqual(decoded["ring_low"], 0x12345678)
            self.assertIsNone(decoded["ring_high"])
            self.assertIsNone(decoded["fence_table"])
            for offset, value in [(0, 99), (4, 1), (68, 5), (84, 0x2201)]:
                bad = bytearray(data)
                struct.pack_into("<I", bad, offset, value)
                with self.assertRaises(ValueError):
                    decode(bad)
            with self.assertRaises(ValueError):
                decode(data[:-1])
        with self.assertRaises(ValueError):
            decode(b"")


if __name__ == "__main__":
    unittest.main()
