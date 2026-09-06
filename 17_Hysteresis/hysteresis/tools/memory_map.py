#!/usr/bin/env python3
"""Which parameter regimes actually REMEMBER?

The referee's test 2 failed on the first visually-decent regime: a one-cell
perturbation peaked at a divergence of 51 and then healed back to under 1 % of
cells. Locally sensitive, globally contractive — the system has a single
attractor and forgets which initial condition it came from.

That could mean the demo is impossible, or it could mean I picked the wrong
corner of parameter space. This maps the two properties against each other:

    structure  — sdev and midtone fraction, i.e. is it worth looking at
    memory     — does a one-cell perturbation persist

If the two are mutually exclusive, HYSTERESIS as specified cannot be built and
that is worth knowing now. If there is overlap, that overlap is the demo.
"""

import os
import re
import subprocess
import sys

FRAME_BYTES = 320 * 240
HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "..", "host", "hysteresis.exe")


def run_stats(frames, probe):
    out = subprocess.run([EXE, "--headless", "--frames", str(frames),
                          "--probe", probe, "--stats"],
                         capture_output=True, text=True).stderr
    # the printf pads with %6.2f, so "mean=  0.00" splits into two tokens on
    # whitespace — parse the key=value pairs with a regex instead
    for line in out.splitlines():
        if line.startswith("mean="):
            return {k: float(v)
                    for k, v in re.findall(r"(\w+)=\s*(-?[\d.]+)", line)}
    return None


def divergence(frames, probe):
    ps = [subprocess.Popen([EXE, "--headless", "--fielddump",
                            "--frames", str(frames), "--variant", str(v),
                            "--probe", probe],
                           stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL) for v in (0, 1)]
    idx = range(0, FRAME_BYTES, 7)
    n = len(idx)
    last = (0.0, 0.0)
    peak = 0.0
    try:
        for _ in range(frames):
            fa = ps[0].stdout.read(FRAME_BYTES)
            fb = ps[1].stdout.read(FRAME_BYTES)
            if len(fa) < FRAME_BYTES or len(fb) < FRAME_BYTES:
                break
            diff = spread = 0
            for i in idx:
                d = fa[i] - fb[i]
                if d:
                    diff += d if d > 0 else -d
                    spread += 1
            last = (diff / n, 100.0 * spread / n)
            peak = max(peak, last[0])
    finally:
        for p in ps:
            try:
                p.kill()
            except Exception:
                pass
    return last[0], last[1], peak


def main():
    frames = int(sys.argv[1]) if len(sys.argv) > 1 else 900

    # zoom is pinned near unity: the sweep in the build log showed anything
    # above about +1.4 % collapses the field to a flat colour, because
    # magnification shrinks the sampled domain until the whole screen is a
    # blow-up of the centre.
    cands = []
    for hi in (130, 180, 230):
        for fold in (160, 200, 240):
            for blur in (90, 130, 170):
                cands.append(f"900,{blur},258,12,{hi},{fold}")

    print(f"{'probe':<28} {'sdev':>6} {'mid%':>6} | {'final div':>9} "
          f"{'spread%':>8} {'peak':>7}  verdict")
    print("-" * 84)
    keepers = []
    for probe in cands:
        st = run_stats(frames, probe)
        if not st or st["sdev"] < 8:
            continue                       # flat: not worth testing for memory
        fin, spread, peak = divergence(frames, probe)
        remembers = spread > 5.0 and fin > 0.5
        verdict = "REMEMBERS" if remembers else "forgets"
        print(f"{probe:<28} {st['sdev']:6.1f} {st['mid']:6.1f} | "
              f"{fin:9.3f} {spread:8.1f} {peak:7.1f}  {verdict}")
        if remembers:
            keepers.append((probe, st, fin, spread))

    print("-" * 84)
    if keepers:
        print(f"{len(keepers)} regime(s) both structured and path-dependent:")
        for probe, st, fin, spread in sorted(keepers,
                                             key=lambda k: -k[1]["mid"]):
            print(f"   {probe}   mid={st['mid']:.1f}%  sdev={st['sdev']:.1f}  "
                  f"spread={spread:.1f}%")
    else:
        print("NO regime is both structured and path-dependent at this length.")
        print("That is a concept-level result, not a tuning problem.")


if __name__ == "__main__":
    main()
