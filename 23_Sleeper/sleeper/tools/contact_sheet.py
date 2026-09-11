#!/usr/bin/env python3
"""One native-size still per cut, enlarged 2x, in a grid, for the director.

COLOSSUS's contact sheet took one frame a chapter and three a transition,
because COLOSSUS had ten chapters and never cut. SLEEPER has 57 cuts and
nothing but cuts, so the unit is the cut: one frame from each, taken a
little way in so the shot has settled, labelled with its number and bar.

    python tools/contact_sheet.py --out ../media/contact_sheet.png

It calls the real renderer through --samples, so what is on the sheet is
what the film draws, and it prints the HOST render cost of every shot it
took, which is the per-shot table the round-one brief asks for.
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw

W, H, SCALE, COLS = 320, 240, 2, 6
SAMPLE_RATE, BAR = 24000, 36000
PAD, LABEL = 4, 12

SHOT = ["BLACK", "BOARD", "SIDE", "AHEAD", "UNDER", "UP", "TUNNEL", "RAILS", "WHEEL", "KALEIDO"]
WORLD = ["NONE", "PLATFORM", "SUBURBS", "LINE", "TUNNEL", "OPEN", "BRIDGE", "CITY",
         "YARD", "STATION", "FIELDS", "COAST", "TERMINUS", "DREAM"]


def main():
    here = Path(__file__).resolve().parent
    root = here.parent
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=root / "build_host" / "sleeper.exe")
    ap.add_argument("--harness", type=Path, default=root / "build_host" / "song_harness.exe")
    ap.add_argument("--out", type=Path, default=root.parent / "media" / "contact_sheet.png")
    ap.add_argument("--into", type=float, default=1.0, help="seconds into each shot")
    ap.add_argument("--table", type=Path, default=None, help="write the per-shot cost table here")
    args = ap.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)

    rows = [r.split() for r in subprocess.run([str(args.harness), "--cuts"], check=True,
                                              capture_output=True, text=True).stdout.split("\n") if r.strip()]
    shots = []
    for i, r in enumerate(rows):
        at = int(r[0])
        nxt = int(rows[i + 1][0]) if i + 1 < len(rows) else 128 * BAR
        off = min(int(args.into * SAMPLE_RATE), max(0, (nxt - at) // 2))
        shots.append({"i": i, "sample": at + off, "bar": int(r[1]), "beat": int(r[2]),
                      "shot": SHOT[int(r[3])], "world": WORLD[int(r[4])],
                      "flags": int(r[5]), "variant": int(r[6]),
                      "held": (nxt - at) / SAMPLE_RATE})

    with tempfile.TemporaryDirectory(prefix="sleeper-sheet-") as tmp:
        out = subprocess.run([str(args.exe), "--samples", ",".join(str(s["sample"]) for s in shots),
                              "--outdir", tmp], check=True, capture_output=True, text=True)
        cost = {}
        for line in out.stdout.split("\n"):
            if line.strip():
                sample, bar, us = line.split()
                cost[int(sample)] = float(us)
        cells = []
        for s in shots:
            im = Image.open(Path(tmp) / ("s%08d.ppm" % s["sample"])).convert("RGB")
            cells.append(im.resize((W * SCALE, H * SCALE), Image.NEAREST))

    cw, ch = W * SCALE, H * SCALE + LABEL
    n = len(cells)
    rows_n = (n + COLS - 1) // COLS
    sheet = Image.new("RGB", (COLS * (cw + PAD) + PAD, rows_n * (ch + PAD) + PAD), (16, 16, 20))
    draw = ImageDraw.Draw(sheet)
    for k, im in enumerate(cells):
        x = PAD + (k % COLS) * (cw + PAD)
        y = PAD + (k // COLS) * (ch + PAD)
        sheet.paste(im, (x, y))
        s = shots[k]
        draw.text((x + 2, y + H * SCALE + 1),
                  "%02d  bar %d:%d  %s / %s  v%d  %.1fs  %.2fms"
                  % (s["i"], s["bar"], s["beat"] + 1, s["shot"], s["world"], s["variant"],
                     s["held"], cost.get(s["sample"], 0) / 1000),
                  fill=(190, 190, 200))
    sheet.save(args.out)
    print("contact_sheet: %d cuts, native x%d, %s" % (n, SCALE, args.out))

    # the per-shot HOST cost table the brief asks for
    lines = ["| cut | bar:beat | shot | world | v | held | HOST render (ms) |",
             "|---:|---|---|---|---:|---:|---:|"]
    for s in shots:
        lines.append("| %d | %d:%d | %s | %s | %d | %.1f s | %.2f |"
                     % (s["i"], s["bar"], s["beat"] + 1, s["shot"], s["world"],
                        s["variant"], s["held"], cost.get(s["sample"], 0) / 1000))
    text = "\n".join(lines)
    if args.table:
        args.table.write_text(text + "\n", encoding="utf-8")
        print("table: %s" % args.table)
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
