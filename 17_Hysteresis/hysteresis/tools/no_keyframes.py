#!/usr/bin/env python3
"""HYSTERESIS — the referee.

The demo claims that every frame is computed from the previous frame and that
the system therefore has memory. Both halves of that are checkable by machine,
and a build that fails does not ship.

    1. DETERMINISM   Two runs from the same seed must be bit-identical.
                     Catches dependence on wall clock, uninitialised memory,
                     or core scheduling.

    2. PATH-DEPENDENCE  Light ONE extra cell at frame 0 and run both to the end.
                     The divergence must GROW. This is the one that matters:
                     iterated feedback is an iterated function system, and a
                     CONTRACTIVE IFS converges to an attractor regardless of
                     where it started. A contractive HYSTERESIS would have no
                     memory at all, and the demo would genuinely have looked
                     identical if it had been keyframed.

    3. NEGATIVE CONTROL  Run test 2 again with the nonlinearity removed (the
                     identity react curve), where the theory says the system
                     MUST forget. If that passes too, test 2 is measuring
                     nothing.

Test 3 exists because of demo 16. There, a negative control passed when it
should have failed, because the threshold had been guessed rather than
measured, and the audit looked healthy while checking nothing. A referee with
no known-bad input is not a referee.

Usage:
    python tools/no_keyframes.py                 # all three, default length
    python tools/no_keyframes.py --frames 1200
"""

import argparse
import os
import subprocess
import sys

FIELD_W, FIELD_H = 320, 240
FRAME_BYTES = FIELD_W * FIELD_H

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "..", "host", "hysteresis.exe")


def spawn(frames, variant, probe=None):
    cmd = [EXE, "--headless", "--fielddump", "--frames", str(frames),
           "--variant", str(variant)]
    if probe:
        cmd += ["--probe", probe]
    return subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL)


def read_frame(p):
    buf = p.stdout.read(FRAME_BYTES)
    if len(buf) < FRAME_BYTES:
        return None
    return buf


def compare_runs(frames, variant_a, variant_b, probe=None, label=""):
    """Stream both runs in lockstep and return the per-frame divergence."""
    a = spawn(frames, variant_a, probe)
    b = spawn(frames, variant_b, probe)
    series = []
    try:
        for _ in range(frames):
            fa, fb = read_frame(a), read_frame(b)
            if fa is None or fb is None:
                break
            # mean absolute difference over the field, and how much of the
            # field differs at all. Both matter: two runs can differ hugely in
            # a few cells (a moving edge) or slightly everywhere (real
            # divergence), and only the second means the system forgot nothing.
            diff = 0
            spread = 0
            for i in range(0, FRAME_BYTES, 7):     # stride-sample: 11k cells
                d = fa[i] - fb[i]
                if d:
                    diff += d if d > 0 else -d
                    spread += 1
            n = len(range(0, FRAME_BYTES, 7))
            series.append((diff / n, 100.0 * spread / n))
    finally:
        for p in (a, b):
            try:
                p.kill()
            except Exception:
                pass
    return series


def summarise(series):
    if not series:
        return None
    n = len(series)
    early = sum(d for d, _ in series[: max(1, n // 10)]) / max(1, n // 10)
    late = sum(d for d, _ in series[-max(1, n // 10):]) / max(1, n // 10)
    peak = max(d for d, _ in series)
    final_spread = series[-1][1]
    return early, late, peak, final_spread


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--frames", type=int, default=1800)
    ap.add_argument("--probe", default="900,170,258,12,180,200",
                    help="pin the dynamics so the verdict is about the "
                         "system, not about where the arc happens to be")
    args = ap.parse_args()

    # "none" runs the REAL arc rather than pinned parameters. Pinning answers
    # "is this system path-dependent"; the arc answers "is the shipping demo",
    # which is the question that actually matters before release.
    if args.probe.lower() == "none":
        args.probe = None

    if not os.path.exists(EXE):
        print("build the host harness first (cd host && make)", file=sys.stderr)
        return 2

    failures = []

    # ---------------------------------------------------------------- 1 --
    print("1. determinism — two identical runs must agree bit for bit")
    s = compare_runs(args.frames, 0, 0, args.probe)
    if not s:
        print("   FAIL: no frames produced")
        failures.append("determinism")
    else:
        worst = max(d for d, _ in s)
        if worst == 0:
            print(f"   PASS  {len(s)} frames, zero difference")
        else:
            print(f"   FAIL  max per-frame difference {worst:.4f}")
            failures.append("determinism")

    # ---------------------------------------------------------------- 2 --
    print("\n2. path-dependence — one extra lit cell at frame 0 must not heal")
    s = compare_runs(args.frames, 0, 1, args.probe)
    r = summarise(s)
    if r is None:
        print("   FAIL: no frames produced")
        failures.append("path-dependence")
    else:
        early, late, peak, spread = r
        print(f"   early divergence {early:8.3f}")
        print(f"   late  divergence {late:8.3f}   (peak {peak:.3f})")
        print(f"   final spread     {spread:7.1f}% of cells differ")
        if late > early and spread > 5.0:
            print("   PASS  the perturbation grew and persisted")
        else:
            print("   FAIL  the runs re-converged — the system forgets")
            failures.append("path-dependence")

    # ---------------------------------------------------------------- 3 --
    print("\n3. negative control — identity react curve, which MUST forget")
    base = args.probe or "900,170,258,12,180,200,160"
    lin = base.rsplit(",", 4)[0] + ",0,0,0,0"        # identity react curve
    s = compare_runs(args.frames, 0, 1, lin)
    r = summarise(s)
    if r is None:
        print("   FAIL: no frames produced")
        failures.append("negative-control")
    else:
        early, late, peak, spread = r
        print(f"   early divergence {early:8.3f}")
        print(f"   late  divergence {late:8.3f}   (peak {peak:.3f})")
        print(f"   final spread     {spread:7.1f}% of cells differ")
        if late < early or spread < 5.0:
            print("   PASS  the contractive case forgot, as it must")
        else:
            print("   FAIL  even the linear system 'remembers' — test 2 is "
                  "measuring nothing")
            failures.append("negative-control")

    print("\n" + "=" * 60)
    if failures:
        print("REFEREE FAILED:", ", ".join(failures))
        return 1
    print("REFEREE PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
