"""Frame-time summary of a RECOMP_FRAME_CSV log between two qpc_ms marks.

python frames.py frames.csv [start_ms end_ms]     (marks: qpc ms, inclusive)
python frames.py --selftest
"""
import csv
import statistics
import sys


def pct(sorted_v, p):
    i = min(len(sorted_v) - 1, max(0, round(p / 100 * (len(sorted_v) - 1))))
    return sorted_v[i]


def summarize(ft):
    s = sorted(ft)
    n = len(s)
    mean = sum(s) / n
    # 1% / 0.1% low: average fps of the slowest 1% / 0.1% of frames
    def low(frac):
        k = max(1, int(n * frac))
        return 1000.0 / (sum(s[-k:]) / k)
    med = pct(s, 50)
    jitter = sum(abs(a - b) for a, b in zip(ft, ft[1:])) / max(1, n - 1)
    return dict(frames=n, fps=1000.0 / mean, p50=med, p95=pct(s, 95), p99=pct(s, 99),
                p999=pct(s, 99.9), max=s[-1], low1=low(0.01), low01=low(0.001),
                sd=statistics.pstdev(s), jitter=jitter,
                hitch=sum(1 for v in s if v > 2 * med))


def fmt(r):
    return ('{fps:6.1f} fps | p50 {p50:5.2f} p95 {p95:5.2f} p99 {p99:5.2f} p99.9 {p999:5.2f} max {max:6.2f} ms'
            ' | 1% low {low1:5.1f} 0.1% low {low01:5.1f} | sd {sd:4.2f} jitter {jitter:4.2f} ms'
            ' | hitches(>2x p50) {hitch} / {frames}').format(**r)


def load(path, a=None, b=None):
    out = []
    with open(path, newline='') as f:
        for row in csv.DictReader(f):
            t = float(row['qpc_ms'])
            if (a is None or t >= a) and (b is None or t <= b):
                out.append(float(row['frame_ms']))
    return out


def selftest():
    r = summarize([10.0] * 99 + [40.0])
    assert r['frames'] == 100 and r['max'] == 40.0 and r['p50'] == 10.0
    assert abs(r['low1'] - 25.0) < 1e-9          # slowest 1 frame = 40 ms
    assert r['hitch'] == 1
    assert abs(r['fps'] - 1000 / 10.3) < 1e-9
    print('selftest ok')


if __name__ == '__main__':
    if sys.argv[1] == '--selftest':
        selftest()
    else:
        a = float(sys.argv[2]) if len(sys.argv) > 3 else None
        b = float(sys.argv[3]) if len(sys.argv) > 3 else None
        print(fmt(summarize(load(sys.argv[1], a, b))))
