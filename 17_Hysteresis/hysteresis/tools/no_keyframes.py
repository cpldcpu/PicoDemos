#!/usr/bin/env python3
"""HYSTERESIS — the referee.

The demo claims that every frame is computed from the previous frame and that
the system therefore has memory. Both halves of that are checkable by machine,
and a build that fails does not ship.

    1. DETERMINISM   Two runs from the same seed must be bit-identical, in the
                     picture AND in the audio. Catches dependence on wall clock,
                     uninitialised memory, or core scheduling. The audio half
                     also checks that the samples do not depend on the block size
                     they were asked for, which is what lets the device generate
                     them a few per scanline and still match the host.

    2. PATH-DEPENDENCE  Light ONE extra cell at frame 0 and run both to the end.
                     The divergence must GROW. This is the one that matters:
                     iterated feedback is an iterated function system, and a
                     CONTRACTIVE IFS converges to an attractor regardless of
                     where it started. A contractive HYSTERESIS would have no
                     memory at all, and the demo would genuinely have looked
                     identical if it had been keyframed.

    3. NEGATIVE CONTROL  Run test 2 again with the persistence raised past the
                     point where the field stops remembering. Same arc, same
                     react curve, same everything else, and the perturbation
                     visibly peaks and then heals to nothing. If that passes
                     too, test 2 is measuring nothing.

Test 3 exists because of demo 16. There, a negative control passed when it
should have failed, because the threshold had been guessed rather than
measured, and the audit looked healthy while checking nothing. A referee with
no known-bad input is not a referee.

THE CONTRACTION ARGUMENT DOES NOT HOLD, and finding that out is what produced
the control above. PLANNING.md section 8 justified keeping the magnification
above unity by saying a contractive iterated function system converges to an
attractor regardless of where it started, so a zoom-out demo would have no
memory. Run as a negative control, magnification of 0.982 gives a live field
whose divergence keeps GROWING: 99.5% of cells still differ at the end.

The argument is about the wrong map. The step is advect, convolve, apply the
react curve, then blend with the previous value, and the memory lives in the
react curve -- its fold is non-monotone and has slope above one, so value
differences are amplified no matter which way the geometry is pushing pixels
around. Contracting space does not contract value. What does destroy memory is
raising the persistence, because that is the term which damps the expanding mode
directly, and past about 220 the largest exponent goes negative and the field
forgets while still looking exactly like the demo.

So the control is now a knob that provably works rather than an argument that
sounded right, and the demo's real constraint is the persistence ceiling, not
the sign of the zoom.

AND EVERY TEST NOW ASSERTS THE FIELD WAS ALIVE, which is the lesson from this
referee's own worst bug. Two different ways of producing a DEAD field both
passed silently for a long time:

  - --probe built field_params_t from zero and set only the parameters it
    names, so react_out arrived as 0 and the react curve emitted nothing (fixed
    in sim.h / sim_default_params);
  - the negative control asked for the "identity react curve" by zeroing gain,
    which does not linearise the map, it switches off the transport entirely.

In both cases two empty fields agreed perfectly, so test 2 reported "the system
forgets" and test 3 reported "the contractive case forgot, as it must". Nothing
was being measured in either direction. A divergence number computed from two
black screens is not evidence, so the mean level is now checked first and a dead
field is a FAIL rather than a verdict.

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
WAV_EXE = os.path.join(HERE, "..", "host", "synthwav.exe")


def audio_hashes(seconds, chunk=None):
    """Run the synth and return its per-second sample hashes."""
    scratch = os.path.join(HERE, "..", "host", "_referee_audio.wav")
    cmd = [WAV_EXE, "-o", scratch, "--hashes", "--seconds", str(seconds)]
    if chunk:
        cmd += ["--chunk", str(chunk)]
    out = subprocess.run(cmd, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL).stdout.decode()
    try:
        os.remove(scratch)
    except OSError:
        pass
    return [l.strip() for l in out.splitlines() if l.startswith("AHASH")]


def test_audio(failures, seconds=40):
    """PLANNING.md section 7 test 1 covers audio as well as video.

    Two properties, and the second is the one that is easy to get wrong:

      determinism        two runs must produce the same samples.
      chunk independence  the same samples whether asked for one at a time or
                          4096 at a time. The device fills its DMA ring a few
                          samples per scanline (audio_synth.c) and the host
                          renders in big blocks, so if the output depended on the
                          block size at all then the two targets could not agree
                          and the device/host hash comparison would be measuring
                          the buffer size rather than the synth. Everything at
                          control rate is therefore driven off an absolute sample
                          counter and a countdown that survives across calls.
    """
    print("\n1b. audio — deterministic, and independent of the block size")
    if not os.path.exists(WAV_EXE):
        print("   SKIP  synthwav.exe not built")
        return

    a = audio_hashes(seconds)
    if not a:
        print("   FAIL: no hashes produced")
        failures.append("audio")
        return

    b = audio_hashes(seconds)
    one = audio_hashes(seconds, chunk=1)
    big = audio_hashes(seconds, chunk=4096)

    print(f"   {len(a)} one-second hashes, last {a[-1].split()[-1]}")
    ok = True
    for label, other in (("determinism", b), ("chunk=1", one), ("chunk=4096", big)):
        if other == a:
            print(f"   PASS  {label}")
        else:
            n = next((i for i, (x, y) in enumerate(zip(a, other)) if x != y), None)
            print(f"   FAIL  {label} diverges at second {n}")
            ok = False
    if not ok:
        failures.append("audio")


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
            level = 0
            for i in range(0, FRAME_BYTES, 7):     # stride-sample: 11k cells
                d = fa[i] - fb[i]
                level += fa[i]
                if d:
                    diff += d if d > 0 else -d
                    spread += 1
            n = len(range(0, FRAME_BYTES, 7))
            series.append((diff / n, 100.0 * spread / n, level / n))
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
    k = max(1, n // 10)
    early = sum(d for d, _, _ in series[:k]) / k
    late = sum(d for d, _, _ in series[-k:]) / k
    peak = max(d for d, _, _ in series)
    final_spread = series[-1][1]
    # Mean level over the last tenth of the run: whether there was a field there
    # at all to have memory about. See the module docstring.
    level = sum(v for _, _, v in series[-k:]) / k
    return early, late, peak, final_spread, level


# A field whose late mean is under this is not a picture, and any divergence
# measured against it is noise. The shipping arc runs at a mean of about 137.
ALIVE_MIN = 8.0

# The negative control's persistence. Measured, not chosen: at 212 the field
# still remembers (99.7% spread), at 228 it heals to 0.0% with the perturbation
# peaking at 30.1 first, and at 240 it peaks at 14.0 and heals. 240 is far
# enough past the boundary that the control is not itself sitting on a knife
# edge, and the field there still has a mean of 138 and full structure.
DAMPED_PERSIST = 240

# 40 seconds, not 30. The field does not percolate until about 24 s (an actual
# phase transition, PLANNING.md section 8), so a run that stops at 1800 frames
# is judging a field that has only been developed for six seconds -- and a
# parameter sweep run at 1200 frames reports every setting as dead, which cost
# an hour of chasing a bug that was not there.
DEFAULT_FRAMES = 2400


def report_alive(level, failures, tag):
    """Print the aliveness line. Returns False if the field was dead."""
    if level >= ALIVE_MIN:
        print(f"   field mean       {level:8.2f}   (alive)")
        return True
    print(f"   field mean       {level:8.2f}   DEAD — nothing was measured")
    failures.append(tag + "/dead-field")
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--frames", type=int, default=DEFAULT_FRAMES)
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
        worst = max(d for d, _, _ in s)
        report_alive(summarise(s)[4], failures, "determinism")
        if worst == 0:
            print(f"   PASS  {len(s)} frames, zero difference")
        else:
            print(f"   FAIL  max per-frame difference {worst:.4f}")
            failures.append("determinism")

    test_audio(failures)

    # ---------------------------------------------------------------- 2 --
    print("\n2. path-dependence — one extra lit cell at frame 0 must not heal")
    s = compare_runs(args.frames, 0, 1, args.probe)
    r = summarise(s)
    if r is None:
        print("   FAIL: no frames produced")
        failures.append("path-dependence")
    else:
        early, late, peak, spread, level = r
        alive = report_alive(level, failures, "path-dependence")
        print(f"   early divergence {early:8.3f}")
        print(f"   late  divergence {late:8.3f}   (peak {peak:.3f})")
        print(f"   final spread     {spread:7.1f}% of cells differ")
        if not alive:
            print("   FAIL  verdict withheld — see above")
        elif late > early and spread > 5.0:
            print("   PASS  the perturbation grew and persisted")
        else:
            print("   FAIL  the runs re-converged — the system forgets")
            failures.append("path-dependence")

    # ---------------------------------------------------------------- 3 --
    print(f"\n3. negative control — persistence {DAMPED_PERSIST}, which MUST forget")
    # The ONE change is the persistence. Everything else -- zoom, react curve,
    # kernel, flow -- is exactly test 2's, so this isolates the damping term as
    # the cause rather than switching the system off. That distinction is why
    # this test has now been rewritten twice: the first version asked for an
    # "identity react curve" by setting gain to zero, which does not linearise
    # the map, it stops the transport, and two blank screens agree perfectly for
    # reasons that have nothing to do with memory; the second used a contractive
    # zoom, which turns out not to make the system forget at all (see above).
    base = args.probe or "900,170,258,12,180,200"
    parts = base.split(",")
    if len(parts) < 6:
        parts.append("200")                     # fold, the arc's own value
    damped = ",".join(parts[:6] + [str(DAMPED_PERSIST)])
    s = compare_runs(args.frames, 0, 1, damped)
    r = summarise(s)
    if r is None:
        print("   FAIL: no frames produced")
        failures.append("negative-control")
    else:
        early, late, peak, spread, level = r
        alive = report_alive(level, failures, "negative-control")
        print(f"   early divergence {early:8.3f}")
        print(f"   late  divergence {late:8.3f}   (peak {peak:.3f})")
        print(f"   final spread     {spread:7.1f}% of cells differ")
        if not alive:
            print("   FAIL  verdict withheld — a dead field forgets trivially, "
                  "which is not what this test is for")
        elif late < early or spread < 5.0:
            print(f"   PASS  the damped case forgot, as it must "
                  f"(peaked at {peak:.1f}, then healed)")
        else:
            print("   FAIL  even the damped system 'remembers' — test 2 is "
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
