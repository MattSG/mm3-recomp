"""record_video.py shows, at each display refresh, the newest image presented by then."""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from tools.game_test.record_video import shown_at_refreshes


class ShownAtRefreshes(unittest.TestCase):
    def test_repeats_and_skips_come_out_as_on_screen(self):
        # Presents at 0, 10, 12 and 40 ms on a 60 Hz display (refreshes at 0, 16.7, 33.3, 50 ms):
        # the 10 ms image is never seen, the 12 ms one is seen twice.
        self.assertEqual(shown_at_refreshes([0, 10000, 12000, 40000], 60), [0, 2, 2, 3])

    def test_every_present_seen_when_faster_than_presents(self):
        self.assertEqual(shown_at_refreshes([0, 33333, 66666], 120), [0, 0, 0, 0, 1, 1, 1, 1, 2])


if __name__ == '__main__':
    unittest.main()
