#!/usr/bin/env python3
"""The per-shot cost table: every frame of the 60 fps capture, grouped by the
cut that drew it.

    python tools/shot_costs.py --out ../briefs/logs/host-per-shot.md

HOST milliseconds, from the same run that produces the film, plus the light
and span counts demo_stats() reports and an estimate of what the shot would
cost the board. The estimate is a scale factor and is labelled as one: the
only DEVICE numbers in this production come off the board, and the column is
here to rank shots, not to stand in for a measurement.
"""
import argparse
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

SHOT = ["BLACK", "BOARD", "SIDE", "AHEAD", "UNDER", "UP", "TUNNEL", "RAILS", "WHEEL", "KALEIDO"]
WORLD = ["NONE", "PLATFORM", "SUBURBS", "LINE", "TUNNEL", "OPEN", "BRIDGE", "CITY",
         "YARD", "STATION", "FIELDS", "COAST", "TERMINUS", "DREAM"]
SAMPLE_RATE, BAR = 24000, 36000
FLAGS = [(1, "RAIN"), (2, "PASSING"), (4, "POINTS"), (8, "EXIT"), (16, "MOUTH"),
         (32, "CROSSING"), (64, "ROOF"), (128, "STOPPED")]


def main():
    here = Path(__file__).resolve().parent
    root = here.parent
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=root / "build_host" / "sleeper.exe")
    ap.add_argument("--harness", type=Path, default=root / "build_host" / "song_harness.exe")
    ap.add_argument("--dispatch", type=Path, default=None)
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--fps", type=int, default=60)
    ap.add_argument("--budget-mcycles", type=float, default=4.5)
    args = ap.parse_args()

    cuts = [r.split() for r in subprocess.run([str(args.harness), "--cuts"], check=True,
                                              capture_output=True, text=True).stdout.split("\n") if r.strip()]

    dispatch = args.dispatch
    if dispatch is None:
        import tempfile
        tmp = tempfile.TemporaryDirectory(prefix="sleeper-cost-")
        dispatch = Path(tmp.name) / "dispatch.txt"
        with open(Path(tmp.name) / "sink", "wb") as sink:
            subprocess.run([str(args.exe), "--raw", "--fps", str(args.fps),
                            "--dispatch", str(dispatch)], check=True, stdout=sink)

    per = defaultdict(list)
    lights = defaultdict(int)
    spans = defaultdict(int)
    for line in open(dispatch, encoding="utf-8"):
        if not line.strip() or line.startswith("#"):
            continue
        f = line.split()
        cut, us, li, sp = int(f[2]), int(f[5]), int(f[6]), int(f[7])
        per[cut].append(us)
        lights[cut] = max(lights[cut], li)
        spans[cut] = max(spans[cut], sp)

    lines = ["| cut | bar:beat | shot | world | v | flags | held | frames | HOST mean ms | HOST worst ms | lights | spans |",
             "|---:|---|---|---|---:|---|---:|---:|---:|---:|---:|---:|"]
    worst_overall = (0.0, -1)
    for i, r in enumerate(cuts):
        at = int(r[0])
        nxt = int(cuts[i + 1][0]) if i + 1 < len(cuts) else 128 * BAR
        v = per.get(i, [])
        if not v:
            continue
        mean, worst = sum(v) / len(v) / 1000, max(v) / 1000
        if worst > worst_overall[0]:
            worst_overall = (worst, i)
        fl = "+".join(n for b, n in FLAGS if int(r[5]) & b) or "-"
        lines.append("| %d | %d:%d | %s | %s | %d | %s | %.1f s | %d | %.3f | %.3f | %d | %d |"
                     % (i, int(r[1]), int(r[2]) + 1, SHOT[int(r[3])], WORLD[int(r[4])],
                        int(r[6]), fl, (nxt - at) / SAMPLE_RATE, len(v), mean, worst,
                        lights.get(i, 0), spans.get(i, 0)))
    text = "\n".join(lines)
    if args.out:
        args.out.write_text(text + "\n", encoding="utf-8")
        print("wrote", args.out)
    else:
        print(text)
    print("worst HOST frame in the film: %.3f ms, in cut %d" % worst_overall)
    return 0


if __name__ == "__main__":
    sys.exit(main())
