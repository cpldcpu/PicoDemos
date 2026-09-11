#!/usr/bin/env python3
"""Piano roll of the SLEEPER score, from song_harness --dump, to a PNG.

    python song_roll.py [--out media/song_roll.png]

Rows: lead (white), bass (orange), piano (blue), choir (violet), pad (grey),
with the drums below as ticks, the speed profile as a curve under the bars,
and the cut list as marks on the top edge. Bars along the horizontal axis.
"""

import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(tempfile.gettempdir(), "sleeper_song_check")
EXE = os.path.join(OUT, "song_harness.exe")
BARS, STEPS = 128, 16
PNG = os.path.join(os.path.dirname(ROOT), "media", "song_roll.png")
if "--out" in sys.argv:
    PNG = sys.argv[sys.argv.index("--out") + 1]

VOICE_COLOUR = {0: (255, 255, 255), 1: (255, 160, 60), 2: (110, 170, 255), 3: (110, 170, 255),
                4: (110, 170, 255), 5: (110, 170, 255), 6: (200, 140, 255),
                7: (120, 120, 130), 8: (120, 120, 130), 9: (120, 120, 130), 10: (120, 120, 130)}
SECTION = ["DEPARTURE", "SPEED", "LINE", "TUNNEL", "DROP I", "BRIDGE", "CITY", "ARRIVAL",
           "SLEEPER", "RISER", "DROP II", "DROP II", "BLUE HOUR", "DAWN", "TERMINUS", "CODA"]


def build():
    os.makedirs(OUT, exist_ok=True)
    cmd = ["gcc", "-std=gnu11", "-O2", "-w", "-I" + ROOT, os.path.join(HERE, "song_harness.c"),
           os.path.join(ROOT, "synth.c"), os.path.join(ROOT, "song.c"), "-o", EXE, "-lm"]
    subprocess.run(cmd, check=True)


def main():
    build()
    text = subprocess.run([EXE, "--dump"], check=True, capture_output=True, text=True).stdout
    bars, notes, events = {}, [], []
    for line in text.splitlines():
        f = line.split()
        if f[0] == "B":
            bars[int(f[1])] = [int(v) for v in f[2:]]
        elif f[0] == "N":
            notes.append((int(f[1]), int(f[2]), int(f[3]), int(f[4]), int(f[5])))
        elif f[0] == "E":
            events.append((int(f[1]), int(f[2]), int(f[3])))
    cuts = [int(l.split()[0]) for l in subprocess.run([EXE, "--cuts"], check=True, capture_output=True, text=True).stdout.splitlines()]
    tt = [int(l.split()[2]) for l in subprocess.run([EXE, "--timetable"], check=True, capture_output=True, text=True).stdout.splitlines()]

    px_step = 3
    W = BARS * STEPS * px_step + 60
    top, note_h, lo, hi = 40, 3, 28, 92
    H_notes = (hi - lo) * note_h
    H = top + H_notes + 120
    im = Image.new("RGB", (W, H), (16, 16, 24))
    d = ImageDraw.Draw(im)
    x0 = 50

    def x_of(bar, step):
        return x0 + (bar * STEPS + step) * px_step

    def y_of(note):
        return top + (hi - note) * note_h

    # sections and bars
    for b in range(BARS):
        x = x_of(b, 0)
        col = (40, 40, 56) if b % 8 else (90, 90, 120)
        d.line([(x, top), (x, top + H_notes)], fill=col)
        if b % 8 == 0:
            d.text((x + 2, 4), SECTION[bars[b][0]], fill=(200, 200, 220))
            d.text((x + 2, 16), str(b), fill=(120, 120, 140))
    for bar_sample in cuts:
        x = x0 + bar_sample // 2250 * px_step
        d.line([(x, top - 6), (x, top)], fill=(255, 80, 80), width=2)

    # sustain: a note lasts until the next event in its voice
    by_voice = {}
    for bar, step, voice, note, slide in notes:
        by_voice.setdefault(voice, []).append((bar * STEPS + step, note, slide))
    for voice, evs in by_voice.items():
        evs.sort()
        for i, (t, note, slide) in enumerate(evs):
            if note <= 2:
                continue
            t_end = evs[i + 1][0] if i + 1 < len(evs) else t + STEPS
            if voice >= 7:
                t_end = min(t_end, t + STEPS)
            colour = VOICE_COLOUR.get(voice, (200, 200, 200))
            if slide:
                colour = (255, 220, 120)
            d.rectangle([x0 + t * px_step, y_of(note), x0 + t_end * px_step - 1, y_of(note) + note_h - 1], fill=colour)

    # drums and events
    yb = top + H_notes + 8
    for bar, step, e in events:
        x = x_of(bar, step)
        if e & 1:
            d.line([(x, yb), (x, yb + 8)], fill=(255, 255, 255))
        if e & 2:
            d.line([(x, yb + 10), (x, yb + 18)], fill=(255, 200, 200))
        if e & 4:
            d.line([(x, yb + 10), (x, yb + 14)], fill=(160, 120, 120))
        if e & 8:
            d.point((x, yb + 21), fill=(180, 180, 200))
        if e & 16:
            d.line([(x, yb + 20), (x, yb + 24)], fill=(180, 180, 255))
        if e & 32:
            d.point((x, yb + 26), fill=(255, 255, 160))
        if e & 64:
            d.line([(x, yb), (x, yb + 28)], fill=(255, 255, 0))
        if e & (128 | 256 | 512 | 1024 | 2048 | 4096):
            d.line([(x, yb + 30), (x, yb + 38)], fill=(255, 140, 255), width=2)
        if e & 8192:
            d.point((x, yb + 16), fill=(255, 120, 120))

    # speed and light
    ys = yb + 44
    d.text((4, ys), "speed", fill=(150, 150, 170))
    pts = []
    for i, v in enumerate(tt):
        pts.append((x0 + i * 4 * px_step, ys + 40 - v * 40 // 81920))
    d.line(pts, fill=(255, 200, 80), width=2)
    pts = [(x_of(b, 0), ys + 40 - bars[b][2] * 40 // 255) for b in range(BARS)]
    d.line(pts, fill=(120, 160, 255), width=1)
    d.text((4, ys + 46), "light", fill=(120, 160, 255))
    for name, y in (("lead", y_of(80)), ("bass", y_of(40)), ("piano", y_of(66)), ("choir", y_of(85)), ("pad", y_of(52))):
        d.text((4, y), name, fill=VOICE_COLOUR[{"lead": 0, "bass": 1, "piano": 2, "choir": 6, "pad": 7}[name]])
    os.makedirs(os.path.dirname(PNG), exist_ok=True)
    im.save(PNG)
    print(PNG, im.size)


if __name__ == "__main__":
    main()
