#!/usr/bin/env python3
"""
check_tiling.py — prove a texture tiles, instead of squinting at it.

assets/PROMPTS.md says "seamless means provably seamless": QUICKSILVER shipped a
sky with a visible vertical seam and had to reissue it. Eyeballing a wrap is
unreliable, so this measures it.

Method: a texture tiles cleanly when the step across the wrap edge is no larger
than a typical step between adjacent interior lines. So compare
  mean |col[W-1] - col[0]|   against  the median adjacent-column difference,
and likewise for rows. A ratio near 1.0 means the wrap is indistinguishable
from ordinary interior variation. A large ratio is a seam.

Also reports mirror symmetry, because a 2x2 mirrored composite tiles perfectly
but repeats visibly in motion — technically seamless, still wrong.

    python tools/check_tiling.py assets/*.png
"""

import sys

try:
    from PIL import Image
except ImportError:
    print("needs Pillow:  pip install Pillow")
    sys.exit(2)

WRAP_FAIL = 3.0      # ratio above which the wrap counts as a seam
MIRROR_TOL = 2.0     # mean abs diff below which halves count as mirrored


def col(px, w, h, x):
    return [px[x, y] for y in range(h)]


def row(px, w, h, y):
    return [px[x, y] for x in range(w)]


def mad(a, b):
    return sum(abs(p - q) for p, q in zip(a, b)) / len(a)


def median(v):
    s = sorted(v)
    return s[len(s) // 2] if s else 0.0


def check(path, wrap="xy"):
    """wrap: "xy" for a 4-edge tile, "x" for a panorama (horizontal only).

    A sky panorama must NOT wrap top-to-bottom — its top is zenith and its
    bottom is horizon, and they are supposed to differ. Judging it by the
    4-edge rule reports a seam that is actually the sky doing its job.
    """
    im = Image.open(path).convert("L")
    w, h = im.size
    px = im.load()

    interior_x = median([mad(col(px, w, h, x), col(px, w, h, x + 1))
                         for x in range(0, w - 1, max(1, w // 64))])
    interior_y = median([mad(row(px, w, h, y), row(px, w, h, y + 1))
                         for y in range(0, h - 1, max(1, h // 64))])

    wrap_x = mad(col(px, w, h, w - 1), col(px, w, h, 0))
    wrap_y = mad(row(px, w, h, h - 1), row(px, w, h, 0))

    rx = wrap_x / interior_x if interior_x > 0.01 else 0.0
    ry = wrap_y / interior_y if interior_y > 0.01 else 0.0

    # Mirror symmetry: is the right half a flip of the left?
    lh = [px[x, y] for y in range(0, h, 4) for x in range(w // 2)]
    rh = [px[w - 1 - x, y] for y in range(0, h, 4) for x in range(w // 2)]
    mirror_x = mad(lh, rh)
    th = [px[x, y] for y in range(h // 2) for x in range(0, w, 4)]
    bh = [px[x, h - 1 - y] for y in range(h // 2) for x in range(0, w, 4)]
    mirror_y = mad(th, bh)

    flags = []
    if rx > WRAP_FAIL:
        flags.append(f"L/R SEAM ({rx:.1f}x)")
    if wrap == "xy" and ry > WRAP_FAIL:
        flags.append(f"T/B SEAM ({ry:.1f}x)")
    if mirror_x < MIRROR_TOL:
        flags.append("mirrored L/R")
    if mirror_y < MIRROR_TOL:
        flags.append("mirrored T/B")

    status = "FAIL" if any("SEAM" in f for f in flags) else \
             ("warn" if flags else "ok")
    name = path.split("/")[-1].split("\\")[-1]
    tb = f"{ry:5.1f}x" if wrap == "xy" else "  n/a"
    print(f"  [{status:4}] {name:<20} {w}x{h}  "
          f"wrap L/R {rx:5.1f}x   wrap T/B {tb}   "
          f"{'; '.join(flags) if flags else ''}")
    return status != "FAIL"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    print("tiling audit (ratio = wrap-edge step / typical interior step):")
    ok = True
    wrap = "xy"
    for p in sys.argv[1:]:
        # --wrap=x applies to every file after it on the command line.
        if p.startswith("--wrap="):
            wrap = p.split("=", 1)[1]
            continue
        try:
            ok &= check(p, wrap)
        except Exception as e:                       # noqa: BLE001
            print(f"  [err ] {p}: {e}")
            ok = False
    print("\n" + ("all textures tile." if ok else
                  "SEAMS PRESENT — reissue the flagged textures."))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
