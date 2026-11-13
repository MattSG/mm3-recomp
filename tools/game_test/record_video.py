"""Turn a RECOMP_RECORD file into a video, as a display would have shown it.

  python tools/game_test/record_video.py <in.raw> <out.mp4> [refresh_hz] [playback_fps]

For each refresh of a refresh_hz display (default 60) the newest image presented by then is shown, so repeated
and skipped frames come out as they would on screen. playback_fps below refresh_hz is slow motion (240 and 60:
a quarter speed). Needs ffmpeg on PATH.
"""
import shutil
import struct
import subprocess
import sys


def frames(f):
    while True:
        head = f.read(16)
        if len(head) < 16:
            return
        t, w, h = struct.unpack('<qII', head)
        yield t, w, h, f.read(w * h * 4)


def shown_at_refreshes(times_us, hz):
    """Index of the image on screen at each refresh, from the first present to the last."""
    out, k, refresh = [], 0, times_us[0]
    while True:
        while k + 1 < len(times_us) and times_us[k + 1] <= refresh:
            k += 1
        out.append(k)
        if k + 1 >= len(times_us):
            return out
        refresh += 1e6 / hz


def main():
    src, dst = sys.argv[1], sys.argv[2]
    hz = int(sys.argv[3]) if len(sys.argv) > 3 else 60
    out_fps = int(sys.argv[4]) if len(sys.argv) > 4 else hz
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        sys.exit('ffmpeg is not on PATH')
    with open(src, 'rb') as f:
        times, offsets, size = [], [], None
        pos = 0
        for t, w, h, _ in frames(f):
            times.append(t)
            offsets.append(pos + 16)
            size = (w, h)
            pos += 16 + w * h * 4
        if not times:
            sys.exit('no frames in ' + src)
        w, h = size
        enc = subprocess.Popen([ffmpeg, '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'bgra',
                                '-s', f'{w}x{h}', '-r', str(out_fps), '-i', '-', '-c:v', 'libx264', '-preset', 'slow',
                                '-crf', '12', '-pix_fmt', 'yuv420p', dst], stdin=subprocess.PIPE)
        order = shown_at_refreshes(times, hz)
        for k in order:
            f.seek(offsets[k])
            enc.stdin.write(f.read(w * h * 4))
        enc.stdin.close()
        enc.wait()
    print(f'{dst}: {len(order)} frames, {len(order) / out_fps:.1f} s')


if __name__ == '__main__':
    main()
