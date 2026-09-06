#!/usr/bin/env python3
"""Contact sheets for review: one native frame per chapter, three per transition.

The director reviews from pixels, and the two things worth looking at are the
held composition of each chapter and the three frames either side of every
downbeat where a scene is substituted. So this renders

  - one frame at the middle of each of the ten chapters, and
  - before / downbeat / after (-0.5 s, +0 s, +0.5 s) at each of the nine
    chapter boundaries,

using the real capture tool, and writes each of them at native 320x240 and
again at 3x nearest-neighbour, because a five-bit ramp has to be judged
enlarged without smoothing.

    python tools/contact_sheet.py --out ../briefs/sketches/round5

Needs the host build; it calls build_host/capture.exe (or build/capture under
WSL) rather than reimplementing anything.
"""

import argparse
import os
import struct
import subprocess
import sys
import zlib

CV_RATE, CV_BAR = 24000, 46080
CHAPTER_START = [0, 8, 24, 40, 56, 72, 88, 112, 128, 144]
CHAPTER_END = [8, 24, 40, 56, 72, 88, 112, 128, 144, 160]
CHAPTER_NAME = ["overture", "plain", "hand", "heart", "eye",
                "load", "spine", "crown", "colossus", "coda"]
BOUNDARIES = [8, 24, 40, 56, 72, 88, 112, 128, 144]


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
    rows = [raw[y * stride + 1:(y + 1) * stride] for y in range(h)]
    return w, h, rows


def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + r for r in rows)
    z = zlib.compress(raw, 9)

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
                chunk(b"IDAT", z) + chunk(b"IEND", b""))


def enlarge(w, h, rows, n):
    out = []
    for r in rows:
        wide = bytearray()
        for x in range(w):
            wide += r[x * 3:x * 3 + 3] * n
        out.extend([bytes(wide)] * n)
    return w * n, h * n, out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    exe = os.path.join(root, "build_host", "capture.exe")
    if not os.path.exists(exe):
        exe = os.path.join(root, "build_host", "capture")
    if not os.path.exists(exe):
        exe = os.path.join(root, "host", "build", "capture")

    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(root, "..", "briefs", "sketches", "round5"))
    ap.add_argument("--exe", default=exe)
    args = ap.parse_args()
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)

    shots = []
    for i, name in enumerate(CHAPTER_NAME):
        bar = (CHAPTER_START[i] + CHAPTER_END[i]) / 2.0
        shots.append(("chapter-%d-%s" % (i, name), int(bar * CV_BAR)))
    for bar in BOUNDARIES:
        at = bar * CV_BAR
        for tag, off in (("before", -CV_RATE // 2), ("downbeat", 0), ("after", CV_RATE // 2)):
            shots.append(("transition-%03d-%s" % (bar, tag), max(0, at + off)))

    samples = ",".join(str(s) for _, s in shots)
    tmp = os.path.join(out, "_raw")
    os.makedirs(tmp, exist_ok=True)
    subprocess.run([args.exe, "--out", tmp, "--samples", samples, "--quiet"], check=True)

    made = 0
    for name, sample in shots:
        src = os.path.join(tmp, "bar%03d_s%08d.png" % (sample // CV_BAR, sample))
        if not os.path.exists(src):
            print("missing", src, file=sys.stderr)
            continue
        w, h, rows = read_png(src)
        write_png(os.path.join(out, name + ".png"), w, h, rows)
        w3, h3, rows3 = enlarge(w, h, rows, 3)
        write_png(os.path.join(out, name + "-3x.png"), w3, h3, rows3)
        made += 1
        os.remove(src)
    os.rmdir(tmp)
    print("contact_sheet: %d frames, native and 3x, in %s" % (made, out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
