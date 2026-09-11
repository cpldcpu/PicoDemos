#!/usr/bin/env python3
"""Piano roll of the PERSISTENCE score, straight from song.c's accessors.

Pitch against bar, one colour per voice, with the drums as a strip along the
bottom and the arrangement's level curves underneath. This is the structural
review PLANNING.md section 6 promised: the tune has to look like a tune before
anyone is asked whether it sounds like one.

  python song_roll.py [out.png]
"""

import os
import subprocess
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(HERE, "_song_check")
EXE = os.path.join(OUT, "song_harness.exe")
BARS, STEPS = 90, 16

NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
SECTIONS = [(0, "beam"), (8, "plasma"), (16, "kefrens"), (24, "twister"), (32, "tunnel"),
            (40, "plane: B"), (48, "plane: A+B"), (56, "split / riser"), (64, "finale A+B"),
            (72, "finale, key up"), (80, "scroller"), (88, "off")]


def build():
    if os.path.exists(EXE):
        return
    os.makedirs(OUT, exist_ok=True)
    subprocess.run(["gcc", "-std=gnu11", "-O2", "-DHOST_BUILD=1", "-I" + ROOT,
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


def notes_of(events):
    """Turn a row-event column (0 hold, 1 off, n note) into (start, len, note)."""
    out, cur, start = [], None, 0
    for s, e in enumerate(events):
        if e == 0:
            continue
        if cur is not None:
            out.append((start, s - start, cur))
        cur, start = (None if e == 1 else e), s
    if cur is not None:
        out.append((start, len(events) - start, cur))
    return out


def main():
    build()
    steps, bars = dump()
    cols = {"bass": 1, "arp": 2, "lead": 3, "lead2": 4}
    colour = {"bass": "#3a4fb8", "arp": "#4fb37a", "lead": "#e0442e", "lead2": "#f0a020", "pad": "#9b7bd6"}

    fig, (ax, axd, axl) = plt.subplots(3, 1, figsize=(30, 13), sharex=True,
                                       gridspec_kw={"height_ratios": [9, 1.6, 1.8]})

    # pad chords as translucent bands
    for b in bars:
        bar = b[0]
        for n in b[3:7]:
            if n:
                ax.add_patch(Rectangle((bar, n - 0.5), 1, 1, color=colour["pad"], alpha=0.18, lw=0))

    for name, col in cols.items():
        for start, length, note in notes_of([s[col] for s in steps]):
            x = start / STEPS
            w = length / STEPS
            ax.add_patch(Rectangle((x, note - 0.4), w, 0.8, color=colour[name], lw=0,
                                   alpha=0.95 if name in ("lead", "lead2") else 0.8))

    for b0, label in SECTIONS:
        ax.axvline(b0, color="#444", lw=0.8, ls="--")
        ax.text(b0 + 0.15, 96.5, label, fontsize=10, va="top", color="#333")
    for k in range(0, BARS, 8):
        ax.axvline(k, color="#888", lw=0.4)
    ax.axvline(72, color="#c00", lw=2, alpha=0.6)

    ax.set_xlim(0, BARS)
    ax.set_ylim(28, 97)
    yt = list(range(28, 97, 12)) + [n for n in range(28, 97) if n % 12 in (4, 9)]
    ax.set_yticks(sorted(set(yt)))
    ax.set_yticklabels(["%s%d" % (NAMES[n % 12], n // 12 - 1) for n in sorted(set(yt))], fontsize=8)
    ax.grid(axis="y", color="#ddd", lw=0.4)
    ax.set_ylabel("pitch")
    ax.set_title("PERSISTENCE -- piano roll from song.c (A minor, up a tone at bar 72)")
    for name, col in colour.items():
        ax.plot([], [], color=col, lw=6, label=name)
    ax.legend(loc="upper right", ncol=5, fontsize=9)

    # drums
    lanes = {1: ("kick", 0), 2: ("snare", 1), 4: ("hat", 2), 8: ("open hat", 2), 16: ("crash", 3)}
    for s in steps:
        d = s[5]
        for bit, (label, lane) in lanes.items():
            if d & bit:
                axd.add_patch(Rectangle((s[0] / STEPS, lane), 1 / STEPS, 0.8,
                                        color="#222" if bit != 8 else "#999", lw=0))
    axd.set_ylim(0, 4)
    axd.set_yticks([0.4, 1.4, 2.4, 3.4])
    axd.set_yticklabels(["kick", "snare", "hats", "crash"], fontsize=8)
    for k in range(0, BARS, 8):
        axd.axvline(k, color="#888", lw=0.4)

    # arrangement levels
    xs = [b[0] for b in bars]
    axl.step(xs, [b[7] for b in bars], where="post", color=colour["lead"], label="lead level")
    axl.step(xs, [b[8] for b in bars], where="post", color=colour["lead"], ls=":", label="lead cutoff")
    axl.step(xs, [b[9] for b in bars], where="post", color=colour["lead2"], label="lead2 level")
    axl.step(xs, [b[10] for b in bars], where="post", color=colour["pad"], label="pad level")
    axl.step(xs, [b[11] for b in bars], where="post", color="#888", label="riser")
    axl.step(xs, [b[12] for b in bars], where="post", color="#000", lw=2, label="energy")
    axl.set_ylim(0, 260)
    axl.set_xlabel("bar")
    axl.legend(loc="upper left", ncol=6, fontsize=8)
    for k in range(0, BARS, 8):
        axl.axvline(k, color="#888", lw=0.4)
    axl.set_xticks(range(0, BARS + 1, 4))

    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(ROOT), "media", "song_roll.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    fig.tight_layout()
    fig.savefig(out, dpi=80)
    print("wrote", out)


if __name__ == "__main__":
    main()
