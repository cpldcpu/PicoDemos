"""Contact sheet of one still per scene, for the README.

    python tools/gallery.py

Renders the stills through the host player (which is the only place a whole
frame is ever assembled) and tiles them with a caption strip under each.
"""

import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.join(HERE, "..", "host")
EXE = os.path.join(HOST, "persistence.exe")
MEDIA = os.path.join(HERE, "..", "..", "media")

# frame, caption
SHOTS = [
    (560,  "0:09  the beam burns the title in"),
    (1100, "0:18  plasma, 640 wide"),
    (1900, "0:31  kefrens: one line buffer, never cleared"),
    (2700, "0:45  twisters over the copper"),
    (3500, "0:58  the tunnel, computed live"),
    (4600, "1:16  the plane, with a reflection"),
    (6000, "1:40  the raster split: five programs at once"),
    (6900, "1:55  the finale"),
    (8380, "2:19  credits on a sine"),
    (8880, "2:28  the endcard"),
]

COLS = 2
W, H = 640, 480
CAP = 26


def main():
    if not os.path.exists(EXE):
        sys.exit("build the host player first (cd host && make)")
    shotdir = os.path.join(HOST, "gallery")
    args = [EXE, "--shotdir", shotdir]
    for f, _ in SHOTS:
        args += ["--shot", str(f)]
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    rows = (len(SHOTS) + COLS - 1) // COLS
    sheet = Image.new("RGB", (COLS * W, rows * (H + CAP)), (10, 10, 12))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("consola.ttf", 16)
    except OSError:
        font = ImageFont.load_default()

    for i, (f, cap) in enumerate(SHOTS):
        path = os.path.join(shotdir, f"f{f:05d}.bmp")
        if not os.path.exists(path):
            print("missing", path, file=sys.stderr)
            continue
        im = Image.open(path).convert("RGB")
        x = (i % COLS) * W
        y = (i // COLS) * (H + CAP)
        sheet.paste(im, (x, y))
        draw.text((x + 8, y + H + 5), cap, fill=(190, 190, 200), font=font)

    out = os.path.join(MEDIA, "gallery.jpg")
    sheet.save(out, quality=88)
    print("wrote", out, sheet.size)

    # individual PNGs for the README table
    for f, _ in SHOTS:
        src = os.path.join(shotdir, f"f{f:05d}.bmp")
        if os.path.exists(src):
            Image.open(src).save(os.path.join(MEDIA, f"f{f:05d}.png"))


if __name__ == "__main__":
    main()
