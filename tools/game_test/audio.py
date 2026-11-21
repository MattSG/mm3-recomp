"""Faults in a RECOMP_AUDIO_WAV capture that a listener hears.

grid_jumps: sample-to-sample steps over JUMP that fall on the voice processor's 32-sample frame grid.
Music and effects step that far now and then, anywhere; a voice whose position slips between frames
steps on the grid, again and again (heard as crackle: per-frame APU pacing with the fork's DSP thread,
about 150 in 44 s of menus). gaps: runs of digital silence of GAP_MS or more inside the sound (holes
from skipped periods), not counting silence at either end of the capture.
"""
import wave

import numpy as np

FRAME = 32          # samples per voice-processor frame
JUMP = 12000        # 16-bit step taken as a discontinuity
GAP_MS = 2
GRID_LIMIT = 5      # more on-grid jumps than this fail a check


def read(path):
    """Left channel as int32, and the sample rate."""
    with wave.open(str(path)) as w:
        if w.getsampwidth() != 2:
            raise ValueError(f"{path}: 16-bit PCM expected")
        data = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
        return data[:: w.getnchannels()].astype(np.int32), w.getframerate()


def faults(samples, rate):
    step = np.abs(np.diff(samples))
    at = np.nonzero(step > JUMP)[0] + 1          # index of the sample after the step
    on_grid = int(np.count_nonzero(at % FRAME == 0))
    sound = np.nonzero(samples)[0]
    gaps = 0
    if sound.size:
        inner = samples[sound[0]: sound[-1] + 1] == 0
        edges = np.diff(np.concatenate(([0], inner.astype(np.int8), [0])))
        lengths = np.nonzero(edges == -1)[0] - np.nonzero(edges == 1)[0]
        gaps = int(np.count_nonzero(lengths >= rate * GAP_MS // 1000))
    return {"seconds": round(len(samples) / rate, 1), "jumps": int(at.size), "grid_jumps": on_grid,
            "gaps": gaps, "rms": round(float(np.sqrt(np.mean(samples.astype(np.float64) ** 2))), 1) if len(samples) else 0.0}


def problems(stats):
    """(status, fault) pairs for a check: crackle and holes fail; a capture with next to no sound in it
    only warns (a quiet screen can be right)."""
    out = []
    if stats["grid_jumps"] > GRID_LIMIT:
        out.append(("fail", f"{stats['grid_jumps']} jumps on the 32-sample frame grid (crackle)"))
    if stats["gaps"]:
        out.append(("fail", f"{stats['gaps']} silent gaps of {GAP_MS} ms or more"))
    if stats["rms"] < 50:
        out.append(("warn", f"next to no sound (rms {stats['rms']})"))
    return out
