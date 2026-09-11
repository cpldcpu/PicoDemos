#!/usr/bin/env python3
"""Turn the renderer's PPMs into review stills: native size and 3x nearest
neighbour, because a five-bit ramp has to be judged enlarged and without
smoothing, and banding invisible at 320x240 on a monitor is very visible on
a CRT at 640x480.

    python tools/stills.py --exe build_host/sleeper.exe --out ../briefs/stills/round1 \
        --shots bar004:144000 bar064:2304000

A shot is NAME:SAMPLE. With --cuts it takes one still at every scheduled cut
instead, two seconds in or at the shot's midpoint, whichever is sooner.
"""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image


def read_ppm(path):
    return Image.open(path).convert("RGB")


def main():
    here = Path(__file__).resolve().parent
    root = here.parent
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=root / "build_host" / "sleeper.exe")
    ap.add_argument("--harness", type=Path, default=root / "build_host" / "song_harness.exe")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--shots", nargs="*", default=[])
    ap.add_argument("--cuts", action="store_true")
    ap.add_argument("--scale", type=int, default=3)
    ap.add_argument("--into", type=float, default=1.2, help="seconds into each cut")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    shots = []
    for s in args.shots:
        name, _, sample = s.partition(":")
        shots.append((name, int(sample)))
    if args.cuts:
        rows = subprocess.run([str(args.harness), "--cuts"], check=True,
                              capture_output=True, text=True).stdout.split("\n")
        cuts = [r.split() for r in rows if r.strip()]
        for i, r in enumerate(cuts):
            at = int(r[0])
            nxt = int(cuts[i + 1][0]) if i + 1 < len(cuts) else 4608000
            off = min(int(args.into * 24000), max(0, (nxt - at) // 2))
            shots.append(("cut%02d_bar%03d" % (i, int(r[1])), at + off))

    if not shots:
        print("nothing to do", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="sleeper-stills-") as tmp:
        samples = ",".join(str(s) for _, s in shots)
        proc = subprocess.run([str(args.exe), "--samples", samples, "--outdir", tmp],
                              check=True, capture_output=True, text=True)
        costs = {}
        for line in proc.stdout.split("\n"):
            if not line.strip():
                continue
            sample, bar, us = line.split()
            costs[int(sample)] = (int(bar), float(us))
        for name, sample in shots:
            src = Path(tmp) / ("s%08d.ppm" % sample)
            if not src.exists():
                print("missing", src, file=sys.stderr)
                continue
            im = read_ppm(src)
            im.save(args.out / (name + ".png"))
            im.resize((im.width * args.scale, im.height * args.scale), Image.NEAREST) \
              .save(args.out / ("%s-%dx.png" % (name, args.scale)))
        print("stills: %d, native and %dx, in %s" % (len(shots), args.scale, args.out))
        if costs:
            worst = max(costs.items(), key=lambda kv: kv[1][1])
            print("HOST render worst of this set: bar %d, %.2f ms" % (worst[1][0], worst[1][1] / 1000))
    return 0


if __name__ == "__main__":
    sys.exit(main())
