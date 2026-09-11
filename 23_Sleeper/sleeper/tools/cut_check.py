#!/usr/bin/env python3
"""Referee 1: SUSTAIN's discontinuity detector, inverted.

SUSTAIN's claim was that it never cuts, and `16_Sustain/sustain/tools/
cut_detect.py` proved it by flagging every frame-to-frame discontinuity.
SLEEPER's claim is the opposite -- it *only* cuts, and only on the beat -- so
the same measure is run and the set it finds must equal the scheduled set
from `song_harness --cuts`. Nothing extra, nothing missing.

The measure is SUSTAIN's, kept deliberately:

  * mean absolute luma delta between consecutive frames, on a sparse prime
    stride, judged against a ROLLING median rather than a global one, because
    a fast passage legitimately has large deltas throughout and judging it
    against the film's average would flag the drop and miss a real glitch
    hidden inside it;
  * AND a spatial spread test -- the fraction of the sampled frame that moved
    materially -- because a cut replaces the whole picture while a passing
    train, however violent, only replaces the part it occupies.

What is new is Phase's correction, and it is the reason this tool is not a
straight copy:

    "A discontinuity detector cannot prove all cuts by itself: matched cuts
     may be quiet and lamp flashes loud. Pair it with dispatch logging."

He is right, and the film is designed to make him right: the whole point of
the shared spacing and the match point is that some cuts are quiet. So the
detector is paired with `--dispatch`, the renderer's own log of which cut it
drew for each captured frame. The dispatch log is ground truth for *where*
the cuts are; the detector's job is the two things a log cannot check --
that no discontinuity happens anywhere else, and that the scheduled cuts are
actually visible as changes rather than being cuts on paper only.

So the verdict has three parts:

  1. every dispatch change is on a beat and matches song_harness --cuts;
  2. no discontinuity is found away from a dispatch change (tolerance: one
     frame either side, because a cut lands between two frames);
  3. every scheduled cut moves *something*: its delta is at least
     --quiet-floor times the local median. A cut nobody can see is reported,
     with its number, as a note rather than a failure -- matched cuts are
     meant to be quiet and Phosphor decides how quiet is too quiet.

    python tools/cut_check.py                       # renders and judges
    python tools/cut_check.py --keep frames.raw     # and keeps the frames

Exit status is non-zero on 1 or 2.
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

W, H = 320, 240
FRAME = W * H * 3
FPS = 60
SAMPLE_RATE = 24000
BEAT = 9000
STRIDE = 37          # prime, so it cannot alias with the picture's structure
SPREAD_LEVEL = 10    # a sample counts as "changed" above this luma delta


def frames(exe, fps, dispatch):
    cmd = [str(exe), "--raw", "--fps", str(fps)]
    if dispatch:
        cmd += ["--dispatch", str(dispatch)]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    try:
        while True:
            buf = p.stdout.read(FRAME)
            if len(buf) < FRAME:
                return
            a = np.frombuffer(buf, dtype=np.uint8).reshape(H * W, 3)[::STRIDE].astype(np.int16)
            yield (a[:, 0] * 77 + a[:, 1] * 150 + a[:, 2] * 29) >> 8
    finally:
        if p.poll() is None:
            p.terminate()
        p.wait()


def rolling_median(v, half):
    n = len(v)
    out = np.empty(n)
    for i in range(n):
        out[i] = np.median(v[max(0, i - half):min(n, i + half + 1)])
    return out


def main():
    here = Path(__file__).resolve().parent
    root = here.parent
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=root / "build_host" / "sleeper.exe")
    ap.add_argument("--harness", type=Path, default=root / "build_host" / "song_harness.exe")
    ap.add_argument("--fps", type=int, default=FPS)
    ap.add_argument("--ratio", type=float, default=6.0)
    ap.add_argument("--spread", type=float, default=0.45)
    ap.add_argument("--floor", type=float, default=1.0)
    ap.add_argument("--window", type=int, default=61)
    ap.add_argument("--tolerance", type=int, default=1, help="frames either side of a cut")
    ap.add_argument("--quiet-floor", type=float, default=1.6,
                    help="a scheduled cut below this x the local median is reported as quiet")
    args = ap.parse_args()

    rows = subprocess.run([str(args.harness), "--cuts"], check=True,
                          capture_output=True, text=True).stdout.split("\n")
    scheduled = []
    for r in rows:
        if not r.strip():
            continue
        f = r.split()
        scheduled.append({"sample": int(f[0]), "bar": int(f[1]), "beat": int(f[2])})
    print("scheduled cuts: %d, from song_harness --cuts" % len(scheduled))

    off_beat = [c for c in scheduled if (c["sample"] % BEAT) != 0]
    if off_beat:
        print("FAIL: %d scheduled cuts are not on a beat" % len(off_beat))
        return 1

    with tempfile.TemporaryDirectory(prefix="sleeper-cut-") as tmp:
        dispatch = Path(tmp) / "dispatch.txt"
        luma = list(frames(args.exe, args.fps, dispatch))
        n = len(luma)
        if n < 3:
            print("FAIL: no frames from the renderer", file=sys.stderr)
            return 2
        dl = [l.strip().split() for l in dispatch.read_text().split("\n")
              if l.strip() and not l.startswith("#")]

    a = np.array(luma, dtype=np.int16)
    diffs = np.abs(a[1:] - a[:-1])
    deltas = diffs.mean(axis=1)
    spreads = (diffs > SPREAD_LEVEL).mean(axis=1)
    med = rolling_median(deltas, max(1, args.window // 2))

    # Where the renderer says it changed cut. Frame i's delta is between
    # frames i and i+1, so a dispatch change at frame k is delta k-1.
    changes = []
    for i in range(1, len(dl)):
        if dl[i][2] != dl[i - 1][2]:
            changes.append((int(dl[i][0]), int(dl[i][2]), int(dl[i][1])))
    print("dispatch: %d frames, %d cut changes" % (len(dl), len(changes)))

    fails, notes = [], []

    # 1. every dispatch change is a scheduled cut, on a beat
    if len(changes) != len(scheduled) - 1:
        fails.append("the renderer changed cut %d times; the list has %d cuts "
                     "(the first is the opening from black and has no change)"
                     % (len(changes), len(scheduled)))
    for frame, index, sample in changes:
        want = scheduled[index]["sample"]
        # the frame that first shows the new cut is the first at or after it
        first = -(-want * args.fps // SAMPLE_RATE)
        if abs(frame - first) > args.tolerance:
            fails.append("cut %d (bar %d) drawn first at frame %d, expected %d"
                         % (index, scheduled[index]["bar"], frame, first))

    cut_frames = set()
    for frame, _, _ in changes:
        for d in range(-args.tolerance - 1, args.tolerance + 1):
            cut_frames.add(frame + d)

    # 2. no discontinuity away from a dispatch change
    stray = []
    for i, d in enumerate(deltas):
        if d < args.floor or med[i] <= 0:
            continue
        if d > args.ratio * med[i] and spreads[i] >= args.spread and (i + 1) not in cut_frames:
            stray.append((i + 1, float(d), float(med[i]), float(spreads[i])))
    if stray:
        fails.append("%d discontinuities away from a scheduled cut" % len(stray))

    # 3. is every scheduled cut visible at all?
    quiet = []
    for frame, index, _ in changes:
        i = frame - 1
        if not (0 <= i < len(deltas)):
            continue
        r = deltas[i] / med[i] if med[i] > 0 else float("inf")
        if r < args.quiet_floor:
            quiet.append((index, scheduled[index]["bar"], float(deltas[i]), float(med[i]), r))

    print("\nframes %d at %d fps; median delta %.2f; peak %.2f"
          % (n, args.fps, float(np.median(deltas)), float(deltas.max())))
    ranked = np.argsort(-deltas)[:6]
    print("worst frames (delta vs local median):")
    for i in ranked:
        r = deltas[i] / med[i] if med[i] > 0 else float("inf")
        mark = "cut" if (i + 1) in cut_frames else "   "
        print("  frame %6d  %s  delta %7.2f  median %6.2f  %5.1fx  spread %3.0f%%"
              % (i + 1, mark, deltas[i], med[i], r, spreads[i] * 100))

    if stray:
        print("\nRULE 2 FAIL -- discontinuities the timetable did not schedule:")
        for frame, d, m, sp in stray[:20]:
            print("  frame %6d (%.2f s)  delta %7.2f  median %6.2f  %.1fx  spread %.0f%%"
                  % (frame, frame / args.fps, d, m, d / m, sp * 100))
        if len(stray) > 20:
            print("  ... and %d more" % (len(stray) - 20))
    else:
        print("\nRULE 2 PASS -- every discontinuity is a scheduled cut.")

    if quiet:
        print("\nNOTE -- %d scheduled cuts are quiet (matched shapes, by design "
              "or by accident; Phosphor's call):" % len(quiet))
        for index, bar, d, m, r in quiet:
            print("  cut %2d, bar %3d: delta %6.2f against a local median of %6.2f (%.2fx)"
                  % (index, bar, d, m, r))
    else:
        print("NOTE -- every scheduled cut moves the picture measurably.")

    if fails:
        print()
        for f in fails[:20]:
            print("cut_check: " + f, file=sys.stderr)
        print("\nAUDIT FAILED")
        return 1
    print("\nAUDIT PASSED -- the cuts on screen are the cuts in the score, and there are no others.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
