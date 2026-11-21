"""The test pack's audio checks must flag crackle and holes in a capture (no game needed)."""
import sys, tempfile, unittest, wave
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import numpy as np
from tools.game_test import audio

RATE = 48000


def tone(amp=8000.0, seconds=2):
    t = np.arange(RATE * seconds)
    return (amp * np.sin(2 * np.pi * 440 * t / RATE)).astype(np.int32)


def found(samples):
    return audio.problems(audio.faults(samples, RATE))


class AudioChecks(unittest.TestCase):
    def test_clean_tone_passes(self):
        self.assertEqual(found(tone()), [])

    def test_steps_on_the_frame_grid_are_crackle(self):
        s = tone()
        s[np.arange(100, 200) * audio.FRAME] += 20000
        self.assertIn("crackle", " ".join(f for st, f in found(s) if st == "fail"))

    def test_steps_off_the_grid_are_not_crackle(self):
        s = tone()
        s[np.arange(100, 200) * audio.FRAME + 7] += 20000
        self.assertEqual([f for st, f in found(s) if "crackle" in f], [])

    def test_hole_inside_the_sound_fails_but_silent_ends_do_not(self):
        s = np.concatenate((np.zeros(RATE // 2, np.int32), tone(), np.zeros(RATE // 2, np.int32)))
        self.assertEqual(found(s), [])
        s[RATE: RATE + RATE // 100] = 0
        self.assertEqual([st for st, f in found(s) if "gap" in f], ["fail"])

    def test_near_silence_only_warns(self):
        self.assertEqual([st for st, _ in found(tone(amp=10))], ["warn"])

    def test_read_takes_the_left_channel(self):
        left = tone(seconds=1)
        stereo = np.stack((left, np.zeros_like(left)), 1).astype("<i2")
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "a.wav"
            with wave.open(str(path), "wb") as w:
                w.setnchannels(2), w.setsampwidth(2), w.setframerate(RATE)
                w.writeframes(stereo.tobytes())
            got, rate = audio.read(path)
        self.assertEqual(rate, RATE)
        np.testing.assert_array_equal(got, left)


if __name__ == "__main__":
    unittest.main()
