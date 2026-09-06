#!/usr/bin/env python3
"""Referee 4: the film, checked frame by frame over the whole run.

PLANNING section 10 asks that no frame outside the overture and the last bar
is black or flat, that black belongs only to the permitted ending, and -- the
human half -- that a frame containing one ember does not pass for a picture.
The numeric half is this. It streams `capture --raw` and never stores it, so
the whole 5:07 costs a couple of minutes and no disk.

Four things fail it:

  1. a frame darker than the black floor, outside bars 0-1 and bar 159;
  2. a frame flatter than the flat floor -- too few distinct colours, or too
     little spread -- in the same range;
  3. any frame in bars 2..158 that is black at all, because black is the
     ending's and nothing else's;
  4. a chapter whose inscription is not on screen for long enough at its
     entrance.

The floors are absolute, and deliberately not derived from the film. Deriving
them from the darkest frame present would make the check unfailable -- the
worst frame would define the floor and then pass it -- which is the same trap
the transition check fell into in round six. They come from the hardware
instead: the DAC has 32 levels a channel, so

  black  = the 99th-percentile pixel is below one DAC step. Ninety-nine per
           cent of the screen is at the darkest value the hardware has.
  flat   = fewer than four of the 32 luma buckets hold as much as 0.1% of the
           frame. Four tones out of thirty-two is not a picture.

The darkest and flattest frames actually present are printed alongside, so the
margin is visible and a regression shows up as a named bar rather than a
number moving. `--selftest` feeds a black frame and a two-tone frame through
the same predicates and fails if they pass, because a referee that cannot fail
is not a referee.

    python tools/film_check.py                 # whole run at 12 fps
    python tools/film_check.py --fps 30        # finer, slower
    python tools/film_check.py --table         # per-bar table

Exits non-zero on any failure.
"""

import argparse
import os
import subprocess
import sys

import numpy as np

W, H = 320, 240
CV_RATE, CV_BAR, CV_BARS = 24000, 46080, 160
FRAME = W * H * 3

# The inscription: scene_chapters.c draws it at (18, 213) in cv_rgb(184,168,144)
# for bars start+0.65 .. start+5, so it is looked for in the band it occupies.
INK = (184, 168, 144)
TEXT_BOX = (14, 208, 306, 228)
# Absolute, from the hardware, not from the film: one DAC step is 8 of 255,
# and a picture that uses fewer than four of the 32 luma buckets at 0.1% of
# the frame each is not a picture.
BLACK_P99 = 8
FLAT_BUCKETS = 4
BUCKET_SHARE = 0.001

CHAPTER_START = [0, 8, 24, 40, 56, 72, 88, 112, 128, 144]
# Chapters 0, 8 and 9 carry the wordmark and the credits instead of a title.
TITLED = [1, 2, 3, 4, 5, 6, 7]


def frames(exe, fps):
    n = int(CV_BARS * CV_BAR * fps / CV_RATE)
    p = subprocess.Popen([exe, "--raw", "--fps", str(fps)], stdout=subprocess.PIPE)
    try:
        for i in range(n):
            buf = p.stdout.read(FRAME)
            if len(buf) < FRAME:
                break
            yield i, np.frombuffer(buf, dtype=np.uint8).reshape(H, W, 3)
    finally:
        if p.poll() is None:
            p.terminate()
        p.wait()


def measure(a):
    """(mean, spread, used luma buckets, 99th percentile) for one RGB frame."""
    mean = float(a.mean())
    spread = float(a.max() - a.min())
    luma = ((a[:, :, 0] * 77 + a[:, :, 1] * 150 + a[:, :, 2] * 29) >> 8).astype(np.uint8)
    counts = np.bincount(luma.ravel() >> 3, minlength=32)
    used = int(np.count_nonzero(counts >= BUCKET_SHARE * luma.size))
    p99 = int(np.percentile(a, 99))
    return mean, spread, used, p99


def selftest():
    """A referee that cannot fail is not a referee."""
    black = np.zeros((H, W, 3), dtype=np.int16)
    two_tone = np.zeros((H, W, 3), dtype=np.int16)
    two_tone[:120] = 200
    ok = True
    _, _, used, p99 = measure(black)
    if p99 > BLACK_P99:
        print("selftest: a black frame passed the black floor", file=sys.stderr); ok = False
    _, _, used2, _ = measure(two_tone)
    if used2 >= FLAT_BUCKETS:
        print("selftest: a two-tone frame passed the flat floor (%d buckets)" % used2,
              file=sys.stderr); ok = False
    print("selftest: black frame 99th percentile %d (floor %d); two-tone frame %d buckets "
          "(floor %d) -- both correctly fail" % (p99, BLACK_P99, used2, FLAT_BUCKETS))
    return 0 if ok else 1


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    exe = os.path.join(root, "build_host", "capture.exe")
    if not os.path.exists(exe):
        exe = os.path.join(root, "build_host", "capture")

    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=exe)
    ap.add_argument("--fps", type=int, default=12)
    ap.add_argument("--table", action="store_true")
    ap.add_argument("--min-title-seconds", type=float, default=2.0)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()
    if args.selftest:
        return selftest()

    x0, y0, x1, y1 = TEXT_BOX
    ink = np.array(INK, dtype=np.int16)

    rows = []            # (bar, mean, spread, buckets, text pixels, 99th pct)
    for i, f in frames(args.exe, args.fps):
        bar = (i * CV_RATE // args.fps) // CV_BAR
        a = f.astype(np.int16)
        mean, spread, distinct, p99 = measure(a)
        box = a[y0:y1, x0:x1]
        text = int(np.count_nonzero((np.abs(box - ink) <= 24).all(axis=2)))
        rows.append((bar, mean, spread, distinct, text, p99))

    if not rows:
        print("film_check: no frames", file=sys.stderr)
        return 2

    permitted = lambda b: b <= 1 or b >= CV_BARS - 1
    body = [r for r in rows if not permitted(r[0])]

    darkest = min(body, key=lambda r: r[5])
    flattest = min(body, key=lambda r: r[3])

    print("frames %d at %d fps; bars 0-1 and %d are exempt" % (len(rows), args.fps, CV_BARS - 1))
    print("floors, from the hardware: 99th percentile > %d of 255 (one DAC step),"
          " and >= %d of 32 luma buckets" % (BLACK_P99, FLAT_BUCKETS))
    print("darkest non-exempt frame  99th percentile %d at bar %d (floor %d)"
          % (darkest[5], darkest[0], BLACK_P99))
    print("flattest non-exempt frame %d used buckets at bar %d (floor %d)"
          % (flattest[3], flattest[0], FLAT_BUCKETS))

    fails = []
    for bar, mean, spread, distinct, _, p99 in body:
        if p99 <= BLACK_P99:
            fails.append("bar %d: 99th percentile %d, at or below one DAC step -- black"
                         % (bar, p99))
        if distinct < FLAT_BUCKETS:
            fails.append("bar %d: only %d of 32 luma buckets used -- flat" % (bar, distinct))

    # The inscription at each chapter entrance.
    print("\nchapter inscriptions:")
    for c in TITLED:
        start = CHAPTER_START[c]
        seen = [r for r in rows if start <= r[0] < start + 6 and r[4] >= 20]
        seconds = len(seen) / float(args.fps)
        ok = seconds >= args.min_title_seconds
        print("  chapter %d, bar %-4d on screen %.2f s   %s"
              % (c, start, seconds, "ok" if ok else "TOO BRIEF"))
        if not ok:
            fails.append("chapter %d's inscription is on screen %.2f s, under %.2f"
                         % (c, seconds, args.min_title_seconds))

    if args.table:
        print("\n%4s %8s %8s %9s %6s" % ("bar", "mean", "spread", "buckets", "text"))
        by_bar = {}
        for bar, mean, spread, distinct, text in rows:
            b = by_bar.setdefault(bar, [0, 0.0, 0.0, 99, 0])
            b[0] += 1; b[1] += mean; b[2] = max(b[2], spread)
            b[3] = min(b[3], distinct); b[4] = max(b[4], text)
        for bar in sorted(by_bar):
            n, s, sp, di, tx = by_bar[bar]
            print("%4d %8.2f %8.0f %9d %6d" % (bar, s / n, sp, di, tx))

    if fails:
        print()
        for f in fails[:20]:
            print("film_check: " + f, file=sys.stderr)
        if len(fails) > 20:
            print("film_check: ... and %d more" % (len(fails) - 20), file=sys.stderr)
        return 1
    print("\nfilm_check: OK -- no black or flat frame outside the permitted bars,")
    print("and every chapter's inscription holds long enough to read")
    return 0


if __name__ == "__main__":
    sys.exit(main())
