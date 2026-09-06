#!/usr/bin/env python3
"""Level and determinism checks for the PERSISTENCE soundtrack.

Builds tools/song_harness.c against ../synth.c and ../song.c, then:

  1. renders the whole piece with block sizes 1, 32 and 1024 and asserts the
     three WAVs are byte-identical (the pull-model contract in synth.h);
  2. renders each voice solo and reports, per bar, peak, RMS and which voices
     are sounding;
  3. asserts the full mix never clips and is never silent where the
     arrangement says a voice is on.

Run from anywhere:  python song_check.py [--keep]
"""

import os
import subprocess
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(HERE, "_song_check")
EXE = os.path.join(OUT, "song_harness.exe")

RATE, BARS, BAR_SAMPLES = 24000, 90, 40000
SOLO = {"kick": 1, "snare": 2, "hat": 4, "bass": 8, "arp": 16,
        "lead": 32, "lead2": 256, "pad": 64, "fx": 128}
SV = {"kick": 1, "snare": 2, "hat": 4, "bass": 8, "arp": 16,
      "lead": 32, "lead2": 64, "pad": 128, "riser": 256}


def build():
    os.makedirs(OUT, exist_ok=True)
    cmd = ["gcc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter",
           "-Wno-unused-function", "-DHOST_BUILD=1", "-I" + ROOT,
           os.path.join(HERE, "song_harness.c"), os.path.join(ROOT, "synth.c"),
           os.path.join(ROOT, "song.c"), "-o", EXE]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.stdout: print(r.stdout)
    if r.stderr: print(r.stderr)
    if r.returncode: sys.exit("build failed")


def render(name, chunk=1024, solo=511):
    path = os.path.join(OUT, name + ".wav")
    r = subprocess.run([EXE, "--wav", path, "--chunk", str(chunk), "--solo", str(solo)],
                       capture_output=True, text=True, check=True)
    print("  " + r.stdout.strip())
    return path


def load(path):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 2 and w.getsampwidth() == 2 and w.getframerate() == RATE
        data = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
    return data.reshape(-1, 2).astype(np.int32)


def voices_per_bar():
    r = subprocess.run([EXE, "--dump"], capture_output=True, text=True, check=True)
    v = {}
    for line in r.stdout.splitlines():
        if line.startswith("B "):
            f = line.split()
            v[int(f[1])] = int(f[14])
    return v


def per_bar(x):
    n = min(len(x), BARS * BAR_SAMPLES)
    x = x[:n].reshape(-1, BAR_SAMPLES, 2)
    peak = np.abs(x).max(axis=(1, 2))
    rms = np.sqrt((x.astype(np.float64) ** 2).mean(axis=(1, 2)))
    dc = np.abs(x.astype(np.float64).mean(axis=1)).max(axis=1)
    return peak, rms, dc


def main():
    keep = "--keep" in sys.argv
    build()

    print("block independence:")
    paths = {c: render("chunk%d" % c, chunk=c) for c in (1, 32, 1024)}
    ref = open(paths[1024], "rb").read()
    for c in (1, 32):
        assert open(paths[c], "rb").read() == ref, "render differs at chunk %d" % c
    print("  chunk 1 == chunk 32 == chunk 1024: OK (%d bytes)" % len(ref))

    full = load(paths[1024])
    assert len(full) == BARS * BAR_SAMPLES, len(full)
    fpeak, frms, fdc = per_bar(full)

    print("solo renders:")
    solo = {}
    for name, mask in SOLO.items():
        solo[name] = per_bar(load(render("solo_" + name, solo=mask)))

    arranged = voices_per_bar()

    print()
    print("bar  peak   rms   dBFS   sounding")
    problems = []
    for b in range(BARS):
        sounding = [n for n in SOLO if solo[n][1][b] > 60]
        db = 20 * np.log10(max(frms[b], 1) / 32768)
        print("%3d %6d %6.0f %6.1f   %s" % (b, fpeak[b], frms[b], db, " ".join(sounding)))
        if fpeak[b] >= 32767:
            problems.append("bar %d clips (peak %d)" % (b, fpeak[b]))
        if fdc[b] > 64:
            problems.append("bar %d carries DC (mean %.0f)" % (b, fdc[b]))
        if arranged.get(b, 0) and frms[b] < 200:
            problems.append("bar %d is near-silent (rms %.0f) but voices 0x%x are arranged" % (b, frms[b], arranged[b]))
        for n, bit in SV.items():
            key = "fx" if n == "riser" else n
            if arranged.get(b, 0) & bit and solo[key][1][b] < 20:
                problems.append("bar %d: %s is arranged but silent (rms %.1f)" % (b, n, solo[key][1][b]))

    tail = full[89 * BAR_SAMPLES + 39000:]
    print()
    print("overall peak %d (%.1f%% of scale), overall rms %.0f, worst DC %.1f, last 1000 samples peak %d"
          % (fpeak.max(), 100.0 * fpeak.max() / 32767, frms.mean(), fdc.max(), np.abs(tail).max()))
    print("voice peaks (solo):", ", ".join("%s %d" % (n, solo[n][0].max()) for n in SOLO))

    if fpeak.max() >= 32767: problems.append("the mix clips")
    if np.abs(tail).max() > 64: problems.append("does not end in silence")
    if problems:
        print("\nPROBLEMS:")
        for p in problems: print("  " + p)
        sys.exit(1)
    print("\nall checks passed")
    if not keep:
        for f in os.listdir(OUT):
            if f.startswith("solo_") or f.startswith("chunk"):
                os.remove(os.path.join(OUT, f))


if __name__ == "__main__":
    main()
