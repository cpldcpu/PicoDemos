#!/usr/bin/env python3
"""Does every chapter change substitute a matched shape, or just cut?

PLANNING's rule is that chapters change by matching an outgoing shape to an
incoming one under a local veil, never by a hard cut. The obvious test is to
take the last frame before a downbeat and the first frame after it, reduce
each to "structure or sky", and measure the overlap in the region the veil
exchanges in.

That test does not work, and this tool exists partly to say so. Almost every
frame in this demo has bronze structure through the middle of the screen, so
overlap in a fixed central region is high whether the shapes were registered
or not. The control below proves it: frames from unrelated boundaries score
as well as real ones. Two different measures were tried -- intersection over
union, then the column-occupancy profile -- and the control tracked both.

So the threshold is the control itself. A boundary counts as matched only if
it beats the best unrelated pair; anything at or below that is coincidence,
however good the raw number looks. On the current build that means one of the
nine passes, which is the truth: only bar 24 has a shape built to register.

This reports; it does not gate. PLANNING allows the glow alone where no real
shape exists, and the director has accepted that for the boundaries listed at
the end, so a non-matching boundary is a note for the next round rather than a
broken build. It exits zero deliberately, and `build.ps1 check` runs it for
the report.

    python tools/transition_check.py
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zlib

CV_RATE, CV_BAR = 24000, 46080
BOUNDARIES = [8, 24, 40, 56, 72, 88, 112, 128, 144]
NAME = {8: "overture -> plain", 24: "plain -> hand", 40: "hand -> heart",
        56: "heart -> eye", 72: "eye -> load", 88: "load -> spine",
        112: "spine -> crown", 128: "crown -> colossus", 144: "colossus -> coda"}
# The cells the veil exchanges in (render.c, r_transition).
REGION = (128, 56, 184, 182)


def read_png(path):
    data = open(path, "rb").read()
    i, idat, w, h = 8, b"", 0, 0
    while i < len(data):
        ln = struct.unpack(">I", data[i:i + 4])[0]
        kind = data[i + 4:i + 8]
        body = data[i + 8:i + 8 + ln]
        if kind == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif kind == b"IDAT":
            idat += body
        i += 12 + ln
    raw = zlib.decompress(idat)
    stride = 1 + w * 3
    return w, h, [raw[y * stride + 1:(y + 1) * stride] for y in range(h)]


def structure(w, h, rows):
    """True where the pixel is structure rather than sky or ground haze."""
    m = bytearray(w * h)
    for y in range(h):
        row = rows[y]
        for x in range(w):
            r, g, b = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]
            m[y * w + x] = 0 if (b > r + 16 and b > 40) else 1
    return m


def iou(a, b, w, h, box=None):
    x0, y0, x1, y1 = box or (0, 0, w, h)
    inter = union = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            p, q = a[y * w + x], b[y * w + x]
            if p or q:
                union += 1
                if p and q:
                    inter += 1
    return 100.0 * inter / union if union else 0.0


def profile(m, w, box):
    """Per-column count of structure rows inside the region.

    Overlap alone cannot tell registration from coincidence -- most frames
    have bronze somewhere in the middle of the screen, and the control below
    proves it scores as well as a real match. What registration actually
    means is that the outgoing and incoming silhouettes have their edges in
    the same columns, so the thing to compare is the shape of the column
    profile, not the area of the intersection.
    """
    x0, y0, x1, y1 = box
    return [sum(m[y * w + x] for y in range(y0, y1)) for x in range(x0, x1)]


def match(a, b, w, box):
    """0..100. One minus the normalised L1 distance between column profiles."""
    pa, pb = profile(a, w, box), profile(b, w, box)
    total = sum(pa) + sum(pb)
    if not total:
        return 0.0
    diff = sum(abs(u - v) for u, v in zip(pa, pb))
    return 100.0 * max(0.0, 1.0 - diff / total)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    exe = os.path.join(root, "build_host", "capture.exe")
    if not os.path.exists(exe):
        exe = os.path.join(root, "build_host", "capture")
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=exe)
    ap.add_argument("--gap", type=float, default=0.04,
                    help="seconds either side of the downbeat")
    args = ap.parse_args()

    samples, tags = [], []
    for bar in BOUNDARIES:
        at = bar * CV_BAR
        d = int(args.gap * CV_RATE)
        samples += [at - d, at + d]
        tags += [(bar, "before"), (bar, "after")]

    with tempfile.TemporaryDirectory(prefix="cv-trans-") as tmp:
        subprocess.run([args.exe, "--out", tmp, "--samples",
                        ",".join(str(s) for s in samples), "--quiet"], check=True)
        masks = {}
        for s, tag in zip(samples, tags):
            p = os.path.join(tmp, "bar%03d_s%08d.png" % (s // CV_BAR, s))
            w, h, rows = read_png(p)
            masks[tag] = structure(w, h, rows)

        # The control comes first, because it sets the bar. These are pairs
        # that were never meant to match: the frame before one boundary
        # against the frame after a different one. A boundary that does not
        # beat the best of them is coincidence, not registration, however
        # good its own number looks.
        ctrl = []
        for i, bar in enumerate(BOUNDARIES):
            other = BOUNDARIES[(i + 4) % len(BOUNDARIES)]
            ctrl.append(match(masks[(bar, "before")], masks[(other, "after")], w, REGION))
        threshold = max(ctrl)
        print("control, unrelated pairs: min %.1f%%  mean %.1f%%  max %.1f%%"
              % (min(ctrl), sum(ctrl) / len(ctrl), threshold))
        print("a boundary counts as matched only if it beats %.1f%%\n" % threshold)

        print("%-22s %10s %10s   %s" % ("boundary", "frame IoU", "match", "verdict"))
        weak = []
        for bar in BOUNDARIES:
            a, b = masks[(bar, "before")], masks[(bar, "after")]
            f = iou(a, b, w, h)
            r = match(a, b, w, REGION)
            ok = r > threshold
            if not ok:
                weak.append((bar, r))
            print("%-22s %9.1f%% %9.1f%%   %s"
                  % (NAME[bar], f, r, "matched" if ok else "not distinguishable"))

    if weak:
        print("\nBoundaries with no shape registering better than chance:")
        for bar, r in weak:
            print("  bar %-4d %-24s %.1f%%" % (bar, NAME[bar], r))
        print("\nOnly bar 24 has an outgoing shape built to register with the")
        print("incoming one (scene_chapters.c, scene_outgoing). The rest change")
        print("under the glow alone, and are the next round's work.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
