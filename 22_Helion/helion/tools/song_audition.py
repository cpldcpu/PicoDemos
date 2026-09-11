#!/usr/bin/env python3
"""Render the HELION instrument audition: synth.c with tools/audition_song.c
in place of song.c (see the layout at the top of that file). Prints the
timestamps so the listener knows which patch is which.

  python song_audition.py [out.wav]
"""

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(tempfile.gettempdir(), "helion_song_check")
EXE = os.path.join(OUT, "audition_harness.exe")
BARS = 38

CUES = [(0, "electric reed: theme A's question, attack / held / release, dry"),
        (4, "bowed metal: unison D4, A4, Bb4, then the Dm chord"),
        (8, "struck bronze: the same phrase"),
        (12, "rubbery bass: two octaves down, a slide into the held note"),
        (16, "solo bowed harmonic: the same phrase, slow"),
        (20, "frame drum, rim knock, brushed metal: three patterns"),
        (23, "the tam-tam, alone"),
        (24, "ensemble, dry: Dm Bb F C, the question"),
        (28, "ensemble with the space: the answer"),
        (32, "the tunnel drive: Dm Bb Gm A, tam-tam"),
        (36, "D major: the resolution, tam-tam touched")]


def main():
    os.makedirs(OUT, exist_ok=True)
    subprocess.run(["gcc", "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter",
                    "-Wno-unused-function", "-ftrapv", "-I" + ROOT,
                    os.path.join(HERE, "song_harness.c"), os.path.join(ROOT, "synth.c"),
                    os.path.join(HERE, "audition_song.c"), "-o", EXE], check=True)
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(OUT, "helion_audition.wav")
    r = subprocess.run([EXE, "--wav", out, "--bars", str(BARS)], capture_output=True, text=True, check=True)
    print(r.stdout.strip())
    for bar, what in CUES:
        print("  %d:%02d  bar %2d  %s" % (bar * 2 // 60, bar * 2 % 60, bar, what))


if __name__ == "__main__":
    main()
