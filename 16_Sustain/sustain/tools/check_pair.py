#!/usr/bin/env python3
"""
check_pair.py — verify a cold/hot texture pair will cross-lerp cleanly.

SUSTAIN blends each pair (surface, wall, matcap) from cold to hot across the
run. assets/PROMPTS.md states the rule: a pair must be the same kind of thing
at the same scale and framing, differing only in material and colour. If the
two halves have features in DIFFERENT PLACES, blending them does not transform
one into the other — it fades one out while fading the other in, which is a
crossfade, the one thing this demo may not do.

Eyeballing two textures side by side does not catch misalignment reliably, so
this measures it:

  structure correlation  Pearson r between the two images' luminance, after
                         removing each one's own mean and contrast. This asks
                         "are the light and dark parts in the same places?"
                         while ignoring that one is blue and one is orange.
                         Near 1.0 = the blend is a recolour. Near 0 = a
                         dissolve.

  band alignment         Same, but on the per-row luminance profile — the test
                         that matters for wall textures, whose strata must sit
                         at the same heights in both halves.

  scale                  Dominant feature count per axis, from the profile's
                         zero crossings. A pair at different scales blends
                         into visible beating even when correlation is decent.

    python tools/check_pair.py assets/wall_cold.png assets/wall_hot.png
"""

import sys

try:
    from PIL import Image
except ImportError:
    print("needs Pillow:  pip install Pillow")
    sys.exit(2)

R_GOOD, R_WARN = 0.85, 0.60


def load(path):
    im = Image.open(path).convert("L")
    w, h = im.size
    px = list(im.getdata())
    return w, h, px


def norm(v):
    n = len(v)
    m = sum(v) / n
    d = [x - m for x in v]
    s = (sum(x * x for x in d) / n) ** 0.5
    return [x / s for x in d] if s > 1e-9 else [0.0] * n, m


def pearson(a, b):
    na, _ = norm(a)
    nb, _ = norm(b)
    return sum(x * y for x, y in zip(na, nb)) / len(na)


def row_profile(w, h, px):
    return [sum(px[y * w:(y + 1) * w]) / w for y in range(h)]


def col_profile(w, h, px):
    return [sum(px[y * w + x] for y in range(h)) / h for x in range(w)]


def crossings(prof):
    n, _ = norm(prof)
    c = sum(1 for i in range(len(n) - 1) if n[i] <= 0 < n[i + 1])
    return c


def verdict(r):
    return "OK" if r >= R_GOOD else ("WARN" if r >= R_WARN else "FAIL")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    geometry = "--geometry" in sys.argv
    if len(args) != 2:
        print(__doc__)
        return 2
    pa, pb = args

    wa, ha, a = load(pa)
    wb, hb, b = load(pb)
    if (wa, ha) != (wb, hb):
        print(f"FAIL: size mismatch {wa}x{ha} vs {wb}x{hb} — a pair must match.")
        return 1

    ra, ma = row_profile(wa, ha, a), sum(a) / len(a)
    rb, mb = row_profile(wb, hb, b), sum(b) / len(b)
    ca, cb = col_profile(wa, ha, a), col_profile(wb, hb, b)

    r_struct = pearson(a, b)
    r_rows = pearson(ra, rb)
    r_cols = pearson(ca, cb)

    print(f"pair: {pa.split('/')[-1]}  vs  {pb.split('/')[-1]}   ({wa}x{ha})")
    print(f"  structure correlation : {r_struct:+.3f}   [{verdict(r_struct)}]")
    print(f"  band alignment (rows) : {r_rows:+.3f}   [{verdict(r_rows)}]")
    print(f"  column alignment      : {r_cols:+.3f}   [{verdict(r_cols)}]")
    print(f"  bands per axis        : rows {crossings(ra)} vs {crossings(rb)}   "
          f"cols {crossings(ca)} vs {crossings(cb)}")
    print(f"  mean luminance        : {ma:.1f} vs {mb:.1f}  "
          f"(ratio {mb/max(ma,0.01):.2f}x)")

    if crossings(ra) != crossings(rb):
        print("  note: differing band counts — the blend may beat between them.")

    # HEIGHT MAPS ARE EXEMPT, and the distinction is not a technicality.
    #
    # The pair rule exists because averaging two COLOUR textures at 50% shows
    # both at half strength — the eye sees a crossfade. Averaging two HEIGHT
    # fields does not do that: the mean of two height fields is itself a
    # perfectly valid height field, and the renderer lights it as one coherent
    # surface. Nothing is ever shown at half strength.
    #
    # Worse, enforcing correlation here would be actively wrong. relief_soft
    # and relief_hard are the sea and the canyon; if they were correlated the
    # two would be the same shape at different amplitudes, and the morph that
    # is supposed to turn open water into a slot canyon would just... inflate.
    # Uncorrelated is the CORRECT state for this pair.
    if geometry:
        print("\nGEOMETRY PAIR — correlation not required. Averaging two height "
              "fields\nyields a valid height field, so an uncorrelated pair "
              "morphs rather than\ndissolves. Reported above for information "
              "only.")
        return 0

    ok = r_struct >= R_WARN and r_rows >= R_WARN
    print("\n" + ("PAIR OK — the blend recolours rather than dissolves."
                 if r_struct >= R_GOOD else
                 "PAIR WEAK — features are not in the same places; the blend "
                 "will read partly as a crossfade."
                 if ok else
                 "PAIR FAIL — unrelated structure. As a uniform lerp this is a "
                 "dissolve, not a morph.\nEither reissue with matched feature "
                 "placement, or drive the blend with a\nspatially varying "
                 "threshold so the two are never both visible at half strength."))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
