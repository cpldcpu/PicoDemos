#!/usr/bin/env python3
"""Round-six asset conversion: allocate the palette, then assign nearest.

Round four diffused every asset. `tools/pack_assets.py` reads the result and
`briefs/2026-09-06-overscan-round6-reply.md` shows what it looked like at 3x:
the sky came out under a fine checkerboard that the row stretch turns into
vertical streaks on screen, and the dawn gradient lost its distinct indices.

The cause is that the palette was frozen first and the error diffused
afterwards. This converter does it the other way round -- it spends the 256
entries where the picture needs them, and then assigns nearest -- because the
measurement says that is enough:

    asset             distinct 5-bit colours   top 256 cover
    stone                                 24          100.0%
    bronze_wear                           46          100.0%
    furnace                              112          100.0%
    warm_environment                     280           99.4%
    dusk+dawn matcap (joint)           1,306           66.3%
    dusk+dawn sky    (joint)           1,814           60.9%

Three of those are exactly representable in 256 entries. Diffusing them was
adding a pattern to a flat material for nothing. Only the two joint pairs need
a real allocation, and for those this runs a deterministic weighted k-means in
the joint six-channel space -- dusk RGB and dawn RGB together, so dusk and
dawn keep one shared index image and the cross-fade stays a palette blend and
the renderer keeps its table-plus-memcpy path.

The sky's weights are biased toward the top of the frame, where the dawn
gradient lives and where a missing entry reads as a band: row 0 counts four
times row 63.

Every centroid is snapped to the DAC's five-bit grid, because that is the only
place a colour can actually land, and clustering to a precision the hardware
cannot show only wastes entries.

    python assets/round6/convert.py              # convert and write
    python assets/round6/convert.py --diffuse .5 # half-strength, if wanted
    python assets/round6/convert.py --report     # measure only, write nothing

Output is Phase's format exactly -- <name>.pixels.bin, <name>.palette.bin,
<name>-quantized.png and a manifest -- so tools/pack_assets.py remains the one
path into the firmware.
"""

import argparse
import hashlib
import json
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.dirname(HERE)
ROOT = os.path.dirname(ASSETS)

SEED = 6
LLOYD = 30


def sha(b):
    return hashlib.sha256(b).hexdigest()


def snap5(a):
    """Every colour the DAC can show is a multiple of eight in each channel."""
    return np.clip((np.asarray(a, dtype=np.int32) + 4) // 8, 0, 31) * 8


def load(rel, size):
    im = Image.open(os.path.join(ROOT, "..", rel) if rel.startswith("assets/")
                    else os.path.join(ASSETS, rel)).convert("RGB")
    if im.size != size:
        im = im.resize(size, Image.Resampling.LANCZOS)
    return snap5(np.asarray(im, dtype=np.int32))


def dac(rgb):
    r, g, b = int(rgb[0]), int(rgb[1]), int(rgb[2])
    return (r >> 3) | ((g & 0xF8) << 3) | ((b & 0xF8) << 8)


def kmeans(data, weights, k, rng):
    """Weighted k-means with deterministic k-means++ seeding.

    data is (n, d) float, weights (n,). Returns (k, d) centroids. Empty
    clusters are refilled from the worst-served point, so the full budget is
    always spent -- an unused entry is a band somewhere.
    """
    n = len(data)
    centres = np.empty((k, data.shape[1]), dtype=np.float64)
    first = rng.choice(n, p=weights / weights.sum())
    centres[0] = data[first]
    closest = ((data - centres[0]) ** 2).sum(1)
    for i in range(1, k):
        p = closest * weights
        total = p.sum()
        centres[i] = data[rng.choice(n, p=p / total)] if total > 0 else data[rng.integers(n)]
        closest = np.minimum(closest, ((data - centres[i]) ** 2).sum(1))

    for _ in range(LLOYD):
        d = ((data[:, None, :] - centres[None, :, :]) ** 2).sum(2)
        lab = d.argmin(1)
        moved = False
        for i in range(k):
            m = lab == i
            w = weights[m]
            if w.sum() > 0:
                new = (data[m] * w[:, None]).sum(0) / w.sum()
            else:
                new = data[(d[np.arange(n), lab] * weights).argmax()]
            if not np.allclose(new, centres[i]):
                moved = True
            centres[i] = new
        if not moved:
            break
    return centres


def build_palette(joint, weights, entries, rng):
    """Palette for a joint (or plain) colour set, on the five-bit grid."""
    uniq, inverse = np.unique(joint, axis=0, return_inverse=True)
    w = np.zeros(len(uniq))
    np.add.at(w, inverse, weights)
    if len(uniq) <= entries:
        # Exactly representable: no clustering, no diffusion, no loss.
        pal = np.zeros((entries, joint.shape[1]), dtype=np.int32)
        pal[:len(uniq)] = uniq
        pal[len(uniq):] = uniq[-1]
        return pal, "exact (%d distinct colours fit in %d entries)" % (len(uniq), entries)
    centres = kmeans(uniq.astype(np.float64), w, entries, rng)
    pal = snap5(np.rint(centres))
    # Snapping can collide; give every duplicate back to the worst-served
    # colour so the whole budget is used.
    seen, spare = {}, []
    for i, row in enumerate(pal):
        key = tuple(row)
        if key in seen:
            spare.append(i)
        else:
            seen[key] = i
    if spare:
        d = ((uniq[:, None, :].astype(np.float64) - pal[None, :, :]) ** 2).sum(2)
        err = d.min(1) * w
        for i in spare:
            j = int(err.argmax())
            pal[i] = uniq[j]
            err[j] = 0
    return pal, "weighted k-means in %d-D, seed %d, %d Lloyd passes, snapped to the 5-bit grid" % (
        joint.shape[1], SEED, LLOYD)


def assign(img, pal, strength):
    """Nearest, or error diffusion at `strength` of the usual weights."""
    h, w, d = img.shape
    work = img.astype(np.float64).copy()
    out = np.zeros((h, w), dtype=np.uint8)
    p = pal.astype(np.float64)
    if strength <= 0:
        flat = work.reshape(-1, d)
        idx = ((flat[:, None, :] - p[None, :, :]) ** 2).sum(2).argmin(1)
        return idx.reshape(h, w).astype(np.uint8)
    for y in range(h):
        for x in range(w):
            v = work[y, x]
            k = int(((p - v) ** 2).sum(1).argmin())
            out[y, x] = k
            e = (v - p[k]) * strength
            for dx, dy, weight in ((1, 0, 7 / 16), (-1, 1, 3 / 16), (0, 1, 5 / 16), (1, 1, 1 / 16)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < w and yy < h:
                    work[yy, xx] += e * weight
    return out


def streakiness(rgb):
    """Mean absolute column-to-column difference: the number that goes up when
    a diffusion pattern is laid over a smooth image, and the one the eye reads
    as vertical streaking once the row stretch magnifies it."""
    a = rgb.astype(np.float64)
    return float(np.abs(np.diff(a, axis=1)).mean())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--diffuse", type=float, default=0.0,
                    help="error diffusion strength, 0 = nearest (default)")
    ap.add_argument("--report", action="store_true", help="measure only, write nothing")
    args = ap.parse_args()

    old = json.load(open(os.path.join(ASSETS, "round4", "manifest.json"), encoding="utf-8"))
    by_name = {e["name"]: e for e in old["assets"]}
    rng = np.random.default_rng(SEED)

    # (name, source, size) for the assets this converter owns. The 4-bit
    # painted assets, the 1-bit atlas and the drawn wordmark keep Phase's
    # round-four conversion: they are lettering and stamps against fixed
    # sixteen-entry ramps, where diffusion is doing real work.
    PAIRS = [("dusk_sky", "dawn_sky", "source/dusk-sky-master.png",
              "source/dawn-sky-master.png", (256, 64), True),
             ("dusk_matcap", "dawn_matcap", "source/dusk-matcap-v3-master.png",
              "source/dawn-matcap-master.png", (64, 64), False)]
    SINGLES = [("stone", "source/stone-master.png", (64, 64), False),
               # bronze_wear's INDEX is the value the renderer uses: span_texture
               # multiplies it by the light and then looks the product up in a
               # shade table, so the index is a brightness ramp, not a palette
               # slot. Reallocating that palette turns the body black -- I did
               # it once and the reveal came out a silhouette. Its palette is
               # therefore frozen and only the diffusion is backed off.
               ("bronze_wear", "source/bronze-wear-master.png", (64, 64), True),
               ("warm_environment", "source/warm-environment-v2-master.png", (64, 64), False),
               ("furnace", "source/furnace-master.png", (64, 32), False)]

    outputs, notes = {}, []

    for a_name, b_name, a_src, b_src, size, top_weighted in PAIRS:
        a, b = load(a_src, size), load(b_src, size)
        joint = np.concatenate([a, b], axis=2)
        h, w = size[1], size[0]
        if top_weighted:
            # Row 0 counts four times row 63: the dawn gradient is up there and
            # a missing entry in it is the band the director saw.
            wcol = 1.0 + 3.0 * (1.0 - np.arange(h) / (h - 1.0)) ** 2
        else:
            wcol = np.ones(h)
        weights = np.repeat(wcol, w)
        pal6, how = build_palette(joint.reshape(-1, 6), weights, 256, rng)
        idx = assign(joint, pal6, args.diffuse)
        for name, half in ((a_name, pal6[:, :3]), (b_name, pal6[:, 3:])):
            outputs[name] = (idx, half, size, 8)
        decoded_a = pal6[:, :3][idx]
        notes.append((a_name, how, streakiness(a), streakiness(decoded_a)))

    for name, src, size, freeze in SINGLES:
        img = load(src, size)
        if freeze:
            pal = np.array([[int(h[1:3], 16), int(h[3:5], 16), int(h[5:7], 16)]
                            for h in by_name[name]["palette"]], dtype=np.int32)
            how = "palette frozen (index is a brightness ramp the renderer reads directly)"
        else:
            weights = np.ones(size[0] * size[1])
            pal, how = build_palette(img.reshape(-1, 3), weights, 256, rng)
        idx = assign(img, pal, args.diffuse)
        outputs[name] = (idx, pal, size, 8)
        notes.append((name, how, streakiness(img), streakiness(pal[idx])))

    print("%-18s %-58s %8s %8s" % ("asset", "palette", "src", "out"))
    for name, how, s_src, s_out in notes:
        print("%-18s %-58s %8.2f %8.2f" % (name, how[:58], s_src, s_out))
    print("\n(src/out are mean column-to-column difference: out well above src")
    print(" is a diffusion pattern laid over a smooth image.)")

    if args.report:
        return 0

    manifest = {"converter_version": "round6-palette-first-1",
                "seed": SEED,
                "palette_policy":
                    "256 entries allocated per asset before assignment; joint six-channel "
                    "k-means for the sky and matcap pairs so dusk and dawn keep one index "
                    "image; sky weights biased 4:1 toward the top rows; every entry snapped "
                    "to the five-bit DAC grid",
                "assignment": "nearest" if args.diffuse <= 0 else
                              "Floyd-Steinberg at %.2f strength" % args.diffuse,
                "shared_indices": ["dusk_sky/dawn_sky", "dusk_matcap/dawn_matcap"],
                "assets": []}

    for entry in old["assets"]:
        name = entry["name"]
        e = dict(entry)
        if name in outputs:
            idx, pal, size, bits = outputs[name]
            raw = bytes(idx.ravel().tolist())
            palette = pal[:256]
            e["palette"] = ["#%02x%02x%02x" % tuple(int(c) for c in row) for row in palette]
            e["pixel_bytes"] = len(raw)
            e["palette_bytes"] = 512
            e["pixel_sha256"] = sha(raw)
            e["quantization"] = next(h for n, h, _, _ in notes if n == name or
                                     (n == "dusk_sky" and name == "dawn_sky") or
                                     (n == "dusk_matcap" and name == "dawn_matcap"))
            e["assignment"] = manifest["assignment"]
            open(os.path.join(HERE, name + ".pixels.bin"), "wb").write(raw)
            open(os.path.join(HERE, name + ".palette.bin"), "wb").write(
                struct.pack("<256H", *[dac(row) for row in palette]))
            Image.fromarray(palette[idx].astype(np.uint8), "RGB").save(
                os.path.join(HERE, name + "-quantized.png"))
        else:
            # Carried over from round four untouched, bytes and all.
            for suffix in (".pixels.bin", ".palette.bin", "-quantized.png"):
                src = os.path.join(ASSETS, "round4", name + suffix)
                if os.path.exists(src):
                    open(os.path.join(HERE, name + suffix), "wb").write(open(src, "rb").read())
            e["carried_from"] = "round4"
        manifest["assets"].append(e)

    manifest["flash_bytes"] = sum(a["pixel_bytes"] + a["palette_bytes"]
                                  for a in manifest["assets"] if a["name"] != "diagnostic_tile")
    json.dump(manifest, open(os.path.join(HERE, "manifest.json"), "w", encoding="utf-8"), indent=1)
    print("\nconvert: wrote %d assets to %s" % (len(manifest["assets"]), HERE))
    return 0


if __name__ == "__main__":
    sys.exit(main())
