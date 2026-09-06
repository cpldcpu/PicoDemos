#!/usr/bin/env python3
"""Motion strips: four native frames across each chapter's mechanical action.

A still tells you a chapter has a composition. It does not tell you the
machine is doing anything -- and "one slow mechanical action completing on a
musical arrival" is the whole premise. So each action gets four frames from
across its span, laid out in one row with a hairline between them, at native
size and again at 3x.

Read them left to right: if the four frames are the same picture with the
camera in a different place, the action is a pose. If something in the machine
has moved between them, it is motion.

    python tools/motion_strip.py --out ../briefs/sketches/round7
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zlib

CV_RATE, CV_BAR = 24000, 46080

# (name, four bar positions across the action)
STRIPS = [
    ("hand-curl", "the fingers take tension", [25.0, 29.0, 34.0, 38.5]),
    ("heart-stroke", "the eccentric and the two strokes", [41.0, 45.0, 49.0, 54.0]),
    ("eye-passage", "through the aperture", [64.5, 66.5, 68.5, 70.5]),
    ("load-counterweights", "counterweights down, tendon up", [73.0, 78.0, 83.0, 87.0]),
    ("spine-ascent", "enclosed, alongside, open sky", [89.0, 96.0, 103.0, 110.0]),
    ("crown-ease", "the ease between Phase's two cameras", [112.5, 117.0, 122.0, 127.0]),
    ("shoulder-lift", "the low shoulder takes the load", [127.5, 128.5, 129.5, 131.0]),
]
GAP = 2
GAP_RGB = (64, 72, 80)


def read_png(path):
    data = open(path, "rb").read()
    i, idat, w, h = 8, b"", 0, 0
    while i < len(data):
        ln = struct.unpack(">I", data[i:i + 4])[0]
        kind, body = data[i + 4:i + 8], data[i + 8:i + 8 + ln]
        if kind == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif kind == b"IDAT":
            idat += body
        i += 12 + ln
    raw = zlib.decompress(idat)
    stride = 1 + w * 3
    return w, h, [raw[y * stride + 1:(y + 1) * stride] for y in range(h)]


def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    z = zlib.compress(raw, 9)

    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" +
                chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", z) + chunk(b"IEND", b""))


def enlarge(w, h, rows, n):
    out = []
    for r in rows:
        wide = bytearray()
        for x in range(w):
            wide += bytes(r[x * 3:x * 3 + 3]) * n
        out.extend([bytes(wide)] * n)
    return w * n, h * n, out


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    exe = os.path.join(root, "build_host", "capture.exe")
    if not os.path.exists(exe):
        exe = os.path.join(root, "build_host", "capture")

    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(root, "..", "briefs", "sketches", "round7"))
    ap.add_argument("--exe", default=exe)
    args = ap.parse_args()
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)

    samples = []
    for _, _, bars in STRIPS:
        samples += [int(b * CV_BAR) for b in bars]

    with tempfile.TemporaryDirectory(prefix="cv-strip-") as tmp:
        subprocess.run([args.exe, "--out", tmp, "--samples",
                        ",".join(str(s) for s in samples), "--quiet"], check=True)
        for name, what, bars in STRIPS:
            frames = []
            for b in bars:
                s = int(b * CV_BAR)
                p = os.path.join(tmp, "bar%03d_s%08d.png" % (s // CV_BAR, s))
                if not os.path.exists(p):
                    print("missing", p, file=sys.stderr)
                    return 1
                frames.append(read_png(p))
            w, h = frames[0][0], frames[0][1]
            total = w * len(frames) + GAP * (len(frames) - 1)
            gap = bytes(GAP_RGB) * GAP
            rows = []
            for y in range(h):
                row = bytearray()
                for i, (_, _, fr) in enumerate(frames):
                    if i:
                        row += gap
                    row += fr[y]
                rows.append(bytes(row))
            write_png(os.path.join(out, name + ".png"), total, h, rows)
            W, H, big = enlarge(total, h, rows, 3)
            write_png(os.path.join(out, name + "-3x.png"), W, H, big)
            print("%-22s %-38s bars %s" % (name, what, ", ".join("%g" % b for b in bars)))
    print("\nmotion_strip: %d strips, native and 3x, in %s" % (len(STRIPS), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
