#!/usr/bin/env python3
"""Referee 1: one table drives the music and the picture.

PLANNING section 10 asks for a script that asserts every scene boundary in the
timeline is a phrase boundary in the score. This is that script, and it takes
the score's word for it rather than the source's: it builds `song_harness` and
reads `--dump`, whose `B` lines are `song_section(bar)` for all 160 bars as the
compiled function actually returns them. If someone edits the ladder in
song.c, this sees the edit.

What it checks:

  1. every chapter boundary in the score is a phrase boundary -- bar % 8 == 0,
     a phrase being eight bars (colossus.h);
  2. the renderer dispatches chapters through song_section() and not through a
     table of its own, so there is only one timeline;
  3. every transition event in render.c falls on a chapter boundary, and on a
     bar downbeat;
  4. every chapter title in scene_chapters.c starts at its chapter's first
     bar;
  5. the run is a whole number of phrases.

    python tools/sync_check.py

Exits non-zero on any failure, and prints the table either way.
"""

import os
import re
import subprocess
import sys

BARS_PER_PHRASE = 8


def build_and_dump(root, out):
    """Build the director's harness and read the score's own section table."""
    os.makedirs(out, exist_ok=True)
    exe = os.path.join(out, "sync_harness.exe")
    cmd = ["gcc", "-std=gnu11", "-O1", "-w", "-DHOST_BUILD=1", "-I" + root,
           os.path.join(root, "tools", "song_harness.c"),
           os.path.join(root, "song.c"), os.path.join(root, "synth.c"),
           "-o", exe, "-lm"]
    subprocess.run(cmd, check=True)
    text = subprocess.run([exe, "--dump"], check=True, capture_output=True,
                          text=True).stdout
    section = {}
    for line in text.splitlines():
        if line.startswith("B "):
            f = line.split()
            section[int(f[1])] = int(f[2])
    return section


def uint_list(path, name):
    """The `static const unsigned <name>[]={...}` literal from a source file."""
    src = open(path, encoding="utf-8").read()
    m = re.search(r"\b" + name + r"\s*\[\s*\]\s*=\s*\{([^}]*)\}", src)
    if not m:
        return None
    return [int(v) for v in re.findall(r"\d+", m.group(1))]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    out = os.path.join(here, "_sync_check")

    section = build_and_dump(root, out)
    bars = max(section) + 1
    fails = []

    # 1. chapter boundaries against the phrase grid
    boundaries = [b for b in range(1, bars) if section[b] != section[b - 1]]
    print("chapter boundaries in the score, and the phrase they fall on:")
    for b in boundaries:
        phrase = b / BARS_PER_PHRASE
        ok = b % BARS_PER_PHRASE == 0
        print("  bar %3d  chapter %d -> %d   phrase %-5s  %s"
              % (b, section[b - 1], section[b], ("%g" % phrase), "ok" if ok else "OFF GRID"))
        if not ok:
            fails.append("chapter boundary at bar %d is not a phrase boundary" % b)

    # 2. the renderer dispatches through the score
    render = open(os.path.join(root, "render.c"), encoding="utf-8").read()
    if "song_section(cv_bar_of(sample))" not in render:
        fails.append("demo_render() does not take its chapter from song_section();"
                     " there would be two timelines")
    else:
        print("\nrenderer chapter dispatch: song_section(cv_bar_of(sample)) -- one table")

    # 3. transition events
    events = uint_list(os.path.join(root, "render.c"), "boundaries")
    if events is None:
        fails.append("no transition table found in render.c")
    else:
        print("\ntransition events in render.c: %s" % ", ".join(str(e) for e in events))
        for e in events:
            if e not in boundaries:
                fails.append("transition at bar %d is not a chapter boundary" % e)
            if e % BARS_PER_PHRASE:
                fails.append("transition at bar %d is not on a phrase downbeat" % e)
        for b in boundaries:
            if b not in events:
                fails.append("chapter boundary at bar %d has no transition event" % b)

    # 4. titles start with their chapters
    starts = uint_list(os.path.join(root, "scene_chapters.c"), "start")
    if starts is None:
        fails.append("no title table found in scene_chapters.c")
    else:
        want = [0] + boundaries
        if starts != want:
            fails.append("title starts %s do not match the chapters %s" % (starts, want))
        else:
            print("chapter title starts: match the score's chapters exactly")

    # 5. whole phrases
    if bars % BARS_PER_PHRASE:
        fails.append("%d bars is not a whole number of phrases" % bars)
    else:
        print("\n%d bars = %d phrases of %d" % (bars, bars // BARS_PER_PHRASE, BARS_PER_PHRASE))

    if fails:
        print()
        for f in fails:
            print("sync_check: " + f, file=sys.stderr)
        return 1
    print("\nsync_check: OK -- one table, and every boundary on a phrase")
    return 0


if __name__ == "__main__":
    sys.exit(main())
