#!/usr/bin/env python3
"""Piano roll of the HELION score, straight from song.c's accessors.

Pitch against bar, one colour per voice, with the drums as a strip along the
bottom and the arrangement's level curves underneath. The tune has to look
like a tune before anyone is asked whether it sounds like one.

  python song_roll.py [out.png]
"""

import os
import subprocess
import sys
import tempfile

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(tempfile.gettempdir(), "helion_song_check")
EXE = os.path.join(OUT, "song_harness.exe")
BARS, STEPS = 80, 16

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
SECTIONS = [(0, "the corona"), (8, "the plain"), (24, "unfolding"), (32, "breath"), (40, "the tunnel"),
            (48, "theme returns"), (56, "orbits: D major"), (64, "eclipse"), (72, "return")]


def build():
    if os.path.exists(EXE):
        return
    os.makedirs(OUT, exist_ok=True)
    subprocess.run(["gcc", "-std=gnu11", "-O2", "-I" + ROOT,
                    os.path.join(HERE, "song_harness.c"), os.path.join(ROOT, "synth.c"),
                    os.path.join(ROOT, "song.c"), "-o", EXE], check=True)


def dump():
    r = subprocess.run([EXE, "--dump"], capture_output=True, text=True, check=True)
    steps, bars = [], []
    for line in r.stdout.splitlines():
        f = line.split()
        if f[0] == "S":
            steps.append([int(x) for x in f[1:]])
        elif f[0] == "B":
            bars.append([int(x) for x in f[1:]])
    return steps, bars


def notes_of(events, ring=None):
    """Turn a row-event column (0 hold, 1 off, n note) into (start, len, note).
    With `ring`, a note lasts that many steps (the bronze decays on its own)."""
    out, cur, start = [], None, 0
    for s, e in enumerate(events):
        if e == 0:
            continue
        if cur is not None:
            out.append((start, (min(s - start, ring) if ring else s - start), cur))
        cur, start = (None if e == 1 else e & 127), s
    if cur is not None:
        out.append((start, len(events) - start, cur))
    return out


def main():
    build()
    steps, bars = dump()
    colour = {"bass": "#3a4fb8", "bronze": "#c9861f", "reed": "#d8342a", "harm": "#e8c060", "bow": "#8b6fd0"}

    fig, (ax, axd, axl) = plt.subplots(3, 1, figsize=(24, 12), sharex=True,
                                       gridspec_kw={"height_ratios": [9, 1.6, 2.0]})

    # the bowed chords, shaded by the gate (a stroke per bar or held across)
    for b in bars:
        bar = b[0]
        for n in b[3:7]:
            if n:
                ax.add_patch(Rectangle((bar, n - 0.5), 1, 1, color=colour["bow"], alpha=0.18, lw=0))

    for name, col, ring in (("bass", 1, None), ("bronze", 2, 2), ("reed", 3, None), ("harm", 4, None)):
        for start, length, note in notes_of([s[col] for s in steps], ring):
            ax.add_patch(Rectangle((start / STEPS, note - 0.4), length / STEPS, 0.8, color=colour[name], lw=0,
                                   alpha=0.95 if name in ("reed", "harm") else 0.8))

    for b0, label in SECTIONS:
        ax.axvline(b0, color="#444", lw=0.8, ls="--")
        ax.text(b0 + 0.15, 88.5, label, fontsize=10, va="top", color="#333")
    for k in range(0, BARS, 4):
        ax.axvline(k, color="#888", lw=0.4)
    ax.axvline(56, color="#c00", lw=2, alpha=0.6)

    ax.set_xlim(0, BARS)
    ax.set_ylim(30, 89)
    yt = [n for n in range(30, 89) if n % 12 in (2, 9, 5)]
    ax.set_yticks(yt)
    ax.set_yticklabels(["%s%d" % (NAMES[n % 12], n // 12 - 1) for n in yt], fontsize=8)
    ax.grid(axis="y", color="#ddd", lw=0.4)
    ax.set_ylabel("pitch")
    ax.set_title("HELION -- piano roll from song.c (D minor at 120 BPM; D major from the orbits, bar 56)")
    for name, col in colour.items():
        ax.plot([], [], color=col, lw=6, label=name)
    ax.legend(loc="upper right", ncol=5, fontsize=9)

    lanes = {1: ("drum", 0), 2: ("ghost", 0), 4: ("rim", 1), 8: ("scrape", 2), 16: ("scrape", 2), 32: ("gong", 3), 64: ("gong", 3)}
    for s in steps:
        d = s[5]
        for bit, (label, lane) in lanes.items():
            if d & bit:
                axd.add_patch(Rectangle((s[0] / STEPS, lane), 1 / STEPS, 0.8,
                                        color="#222" if bit not in (2, 16, 64) else "#999", lw=0))
    axd.set_ylim(0, 4)
    axd.set_yticks([0.4, 1.4, 2.4, 3.4])
    axd.set_yticklabels(["frame drum", "rim", "brushed metal", "tam-tam"], fontsize=8)
    for k in range(0, BARS, 4):
        axd.axvline(k, color="#888", lw=0.4)

    xs = [b[0] for b in bars]
    axl.step(xs, [b[7] for b in bars], where="post", color=colour["reed"], label="reed")
    axl.step(xs, [b[8] for b in bars], where="post", color="#f0a0a0", label="reed push")
    axl.step(xs, [b[9] for b in bars], where="post", color=colour["bow"], label="bow")
    axl.step(xs, [b[11] for b in bars], where="post", color=colour["bronze"], label="bronze")
    axl.step(xs, [b[13] for b in bars], where="post", color=colour["bass"], label="bass")
    axl.step(xs, [b[15] for b in bars], where="post", color=colour["harm"], label="harmonic")
    axl.step(xs, [b[16] for b in bars], where="post", color="#7ab", label="air")
    axl.step(xs, [b[17] for b in bars], where="post", color="#5a5", label="space")
    axl.step(xs, [b[18] for b in bars], where="post", color="#000", lw=2, label="energy")
    axl.set_ylim(0, 260)
    axl.set_xlabel("bar")
    axl.legend(loc="upper left", ncol=9, fontsize=8)
    for k in range(0, BARS, 4):
        axl.axvline(k, color="#888", lw=0.4)
    axl.set_xticks(range(0, BARS + 1, 4))

    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(ROOT), "media", "song_roll.png")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    fig.tight_layout()
    fig.savefig(out, dpi=80)
    print("wrote", out)


if __name__ == "__main__":
    main()
