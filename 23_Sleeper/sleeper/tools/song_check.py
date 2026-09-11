#!/usr/bin/env python3
"""Level and determinism checks for the SLEEPER score (referee 2).

Builds tools/song_harness.c against ../synth.c and ../song.c, then:

  1. renders the whole piece with block sizes 1, 8 and 1024 and asserts the
     three WAVs are byte-identical (the pull-model contract in sleeper.h);
  2. renders each voice solo and reports, per bar, peak, RMS and which voices
     are sounding;
  3. asserts the full mix stays under -2 dBFS, carries no DC, ends in
     silence, and is never silent where the arrangement says a voice plays;
  4. asserts every cut in the timetable sits on a beat and the list is sorted,
     and that the boards are sorted.

Run from anywhere:  python song_check.py [--keep] [--out DIR] [--quick]
The WAVs go to DIR (default: the scratch folder in TEMP), never the repo.
"""

import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(tempfile.gettempdir(), "sleeper_song_check")
if "--out" in sys.argv:
    OUT = sys.argv[sys.argv.index("--out") + 1]
EXE = os.path.join(OUT, "song_harness.exe")
QUICK = "--quick" in sys.argv

RATE, BARS, BAR_SAMPLES = 24000, 128, 36000
SOLO = {"drums": 1, "clack": 2, "bass": 4, "piano": 8, "pad": 16, "lead": 32, "choir": 64,
        "fx": 128, "horn": 256, "bells": 512, "brake": 1024}


def build():
    os.makedirs(OUT, exist_ok=True)
    cmd = ["gcc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-I" + ROOT,
           os.path.join(HERE, "song_harness.c"), os.path.join(ROOT, "synth.c"),
           os.path.join(ROOT, "song.c"), "-o", EXE, "-lm"]
    subprocess.run(cmd, check=True)


def render(name, chunk=512, solo=None, bars=None):
    path = os.path.join(OUT, name + ".wav")
    cmd = [EXE, "--wav", path, "--chunk", str(chunk)]
    if solo is not None:
        cmd += ["--solo", str(solo)]
    if bars is not None:
        cmd += ["--bars", str(bars)]
    subprocess.run(cmd, check=True, capture_output=True)
    return path


def load(path):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 2 and w.getsampwidth() == 2 and w.getframerate() == RATE
        data = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.int32)
    return data.reshape(-1, 2)


def dbfs(x):
    return 20 * np.log10(max(x, 1) / 32768.0)


def per_bar(x, bars):
    rows = []
    for b in range(bars):
        seg = x[b * BAR_SAMPLES:(b + 1) * BAR_SAMPLES]
        peak = int(np.abs(seg).max()) if len(seg) else 0
        rms = float(np.sqrt(np.mean(seg.astype(np.float64) ** 2))) if len(seg) else 0.0
        dc = float(np.mean(seg)) if len(seg) else 0.0
        rows.append((peak, rms, dc))
    return rows


def dump():
    text = subprocess.run([EXE, "--dump"], check=True, capture_output=True, text=True).stdout
    bars, notes = {}, {}
    for line in text.splitlines():
        f = line.split()
        if f[0] == "B":
            bars[int(f[1])] = [int(v) for v in f[2:]]
        elif f[0] == "N":
            notes.setdefault(int(f[1]), []).append((int(f[2]), int(f[3]), int(f[4])))
    return bars, notes


def check_timetable():
    cuts = subprocess.run([EXE, "--cuts"], check=True, capture_output=True, text=True).stdout.splitlines()
    last = -1
    for line in cuts:
        s = int(line.split()[0])
        assert s % 9000 == 0, "cut off the beat at sample %d" % s
        assert s > last, "cut list not sorted at %d" % s
        last = s
    boards = subprocess.run([EXE, "--boards"], check=True, capture_output=True, text=True).stdout.splitlines()
    last = -1
    for line in boards:
        s = int(line.split()[0])
        assert s > last, "boards not sorted at %d" % s
        last = s
        for row in line.split("	")[1:]:
            assert len(row) <= 20, "board row over 20 columns: %r" % row
    print("timetable: %d cuts on beats, %d boards" % (len(cuts), len(boards)))


def main():
    build()
    check_timetable()
    bars_n = 24 if QUICK else BARS

    a = load(render("full_1", 1, bars=bars_n))
    b = load(render("full_8", 8, bars=bars_n))
    c = load(render("full_1024", 1024, bars=bars_n))
    assert np.array_equal(a, b) and np.array_equal(a, c), "block-size dependence"
    print("block sizes 1/8/1024: identical (%d frames)" % len(a))

    full = load(render("full", 512))
    mono = (full[:, 0] + full[:, 1]) // 2
    rows = per_bar(mono, BARS)
    peak = int(np.abs(full).max())
    print("full mix peak %d (%.1f dBFS)" % (peak, dbfs(peak)))
    assert peak <= 26100, "over -2 dBFS"
    worst_dc = max(abs(r[2]) for r in rows)
    print("worst per-bar DC %.1f" % worst_dc)
    assert worst_dc < 60, "DC offset"
    tail = full[-1000:]
    assert np.abs(tail).max() == 0, "does not end in silence"

    bars, notes = dump()
    solos = {}
    for name, mask in SOLO.items():
        solos[name] = per_bar((lambda x: (x[:, 0] + x[:, 1]) // 2)(load(render("solo_" + name, 512, mask))), BARS)

    print("bar  sec  peak   rms  | " + " ".join("%6s" % n for n in SOLO))
    for bb in range(BARS):
        pk, rms, dc = rows[bb]
        line = "%3d  %2d  %5d %5.0f  | " % (bb, bars[bb][0], pk, rms)
        line += " ".join("%6.0f" % solos[n][bb][1] for n in SOLO)
        print(line)

    # arranged but silent?
    for bb in range(BARS):
        row = bars[bb]
        chord, light, energy, drums, bass, lead, piano, pad, choir, filt, space, clack = row[1:]
        if drums and solos["drums"][bb][1] < 30:
            raise SystemExit("drums arranged but silent at bar %d" % bb)
        if lead and solos["lead"][bb][1] < 30 and any(v == 0 and n > 2 for v, n, _ in notes.get(bb, [])):
            raise SystemExit("lead arranged but silent at bar %d" % bb)
        if bass and solos["bass"][bb][1] < 30:
            raise SystemExit("bass arranged but silent at bar %d" % bb)
        if pad and solos["pad"][bb][1] < 10:
            raise SystemExit("pad arranged but silent at bar %d" % bb)
    print("song_check: PASS")
    if "--keep" not in sys.argv:
        for n in ("full_1", "full_8", "full_1024"):
            try:
                os.remove(os.path.join(OUT, n + ".wav"))
            except OSError:
                pass


if __name__ == "__main__":
    main()
