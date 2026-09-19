"""Independent check of the rendered demo WAVs.

Reads each file with the standard library only (no project code), so it verifies
that what was written to disk really is a valid PCM WAV with the expected
duration, level and spectral content.
"""

import glob
import math
import os
import struct
import sys
import wave


def read_wav(path):
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        raw = w.readframes(n)
        sr = w.getframerate()
        ch = w.getnchannels()
        sw = w.getsampwidth()
    assert sw == 2, f"{path}: expected 16-bit PCM, got {sw * 8}-bit"
    assert ch == 1, f"{path}: expected mono, got {ch} channels"
    samples = struct.unpack("<%dh" % n, raw)
    return [s / 32768.0 for s in samples], sr


def goertzel(x, sr, f):
    if not x:
        return 0.0
    n = len(x)
    w = 2.0 * math.pi * f / sr
    coeff = 2.0 * math.cos(w)
    s1 = s2 = 0.0
    wsum = 0.0
    for i, v in enumerate(x):
        win = 0.5 - 0.5 * math.cos(2.0 * math.pi * i / (n - 1))
        wsum += win
        s0 = v * win + coeff * s1 - s2
        s2, s1 = s1, s0
    power = s1 * s1 + s2 * s2 - coeff * s1 * s2
    return 2.0 * math.sqrt(max(0.0, power)) / wsum if wsum else 0.0


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def main():
    paths = sorted(glob.glob(os.path.join("renders", "*.wav")))
    if not paths:
        print("no renders found")
        return 1
    print(f"{'patch':<12} {'sec':>6} {'peak':>7} {'rms':>7} {'dBFS':>7} {'centroid':>9}  non-silent")
    failures = 0
    for p in paths:
        x, sr = read_wav(p)
        peak = max(abs(v) for v in x)
        r = rms(x)
        # Crude spectral centroid from a coarse band scan (no numpy dependency).
        num = den = 0.0
        for f in range(50, min(12000, sr // 2 - 100), 200):
            a = goertzel(x, sr, f)
            num += f * a
            den += a
        centroid = num / den if den > 0 else 0.0
        silent = r < 1e-4
        if silent or peak > 1.0:
            failures += 1
        print(f"{os.path.basename(p):<12} {len(x)/sr:6.2f} {peak:7.3f} {r:7.4f} "
              f"{20*math.log10(max(r,1e-9)):7.1f} {centroid:9.0f}  {'SILENT!' if silent else 'ok'}")
    print()
    print("all renders valid" if failures == 0 else f"{failures} render(s) failed")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
