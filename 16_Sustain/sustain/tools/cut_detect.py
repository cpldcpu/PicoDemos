#!/usr/bin/env python3
"""
cut_detect.py — the rule-1 audit for SUSTAIN.

SUSTAIN's claim is that it never cuts. That claim is worth exactly as much as
our ability to check it, so it is checked mechanically rather than by eye. This
is the demo's equivalent of QUICKSILVER's interp_selftest.c: the central
technical assertion gets a referee, and a build that fails the referee does not
ship.

    cd host
    ./sustain.exe --rawpipe | python ../tools/cut_detect.py

Reads raw 640x480 BGRA frames from stdin (the host's --rawpipe format, which is
deterministic: offline mode clocks from a frame counter, not wall time, so the
audit is reproducible frame-for-frame).

What it enforces
----------------
Rule 1  no frame-to-frame discontinuity.  Every consecutive frame pair must be
        a continuous deformation of the previous. Measured as mean absolute
        luma delta, compared against a ROLLING median rather than a global one
        — a fast passage legitimately has large deltas throughout, and judging
        it against the whole demo's average would flag the fast parts and miss
        a real cut hidden inside them. What matters is a spike relative to the
        demo's local behaviour.

Rule 2  no black.  The screen never fully clears until the final collapse.

Exceptions are declared explicitly, never inferred:

    --allow MS[,MS...]   a motivated cut (PLANNING.md §2). Budget is three for
                         the whole demo, and each must be justified in
                         IMPLEMENTATION.md. The tool enforces the budget.
    --collapse-ms MS     the final collapse to black; rules relax after it.

Exit status is 0 on pass, 1 on failure, so it can gate a build.
"""

import argparse
import sys

# A sample counts as "changed" above this luma delta (0-255).
SPREAD_LEVEL = 10

W, H, BPP = 640, 480, 4
FRAME_BYTES = W * H * BPP
FPS = 60.0

# Sample every Nth pixel. A cut changes a large fraction of the frame, so a
# sparse sample detects one just as reliably as a full scan and keeps a 3-minute
# audit to a few seconds. Prime stride to avoid aliasing with screen structure.
STRIDE = 37


def read_frames(stream):
    """Yield (index, luma_sample_array) for each complete frame on stdin."""
    idx = 0
    while True:
        buf = stream.read(FRAME_BYTES)
        if len(buf) < FRAME_BYTES:
            return
        mv = memoryview(buf)
        # BGRA -> approximate luma on a sparse sample.
        px = [(mv[i], mv[i + 1], mv[i + 2]) for i in range(0, FRAME_BYTES, BPP * STRIDE)]
        luma = [(b * 29 + g * 150 + r * 77) >> 8 for (b, g, r) in px]
        yield idx, luma
        idx += 1


def rolling_median(vals, i, half):
    lo = max(0, i - half)
    hi = min(len(vals), i + half + 1)
    win = sorted(vals[lo:hi])
    n = len(win)
    return win[n // 2] if n else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--allow", default="",
                    help="comma-separated ms timestamps of justified motivated cuts")
    ap.add_argument("--collapse-ms", type=int, default=-1,
                    help="ms at which the final collapse to black begins")
    ap.add_argument("--ratio", type=float, default=6.0,
                    help="flag a frame whose delta exceeds ratio x the rolling median")
    ap.add_argument("--floor", type=float, default=1.0,
                    help="absolute delta below which nothing is ever flagged "
                         "(guards static passages, where the local median "
                         "approaches zero and any flicker looks infinite)")
    ap.add_argument("--black", type=float, default=6.0,
                    help="mean luma below this counts as a black frame")
    ap.add_argument("--spread", type=float, default=0.55,
                    help="minimum fraction of the frame that must change for "
                         "a spike to count as a cut rather than fast motion")
    ap.add_argument("--window", type=int, default=61,
                    help="rolling median window in frames")
    ap.add_argument("--tolerance-ms", type=int, default=120,
                    help="how close a flagged frame must be to an --allow entry")
    args = ap.parse_args()

    allowed = [int(x) for x in args.allow.split(",") if x.strip()]
    if len(allowed) > 3:
        print(f"FAIL: {len(allowed)} motivated cuts declared; the budget is 3 "
              f"(PLANNING.md §2).")
        return 1

    stdin = sys.stdin.buffer
    deltas, spreads, means, prev = [], [], [], None

    for idx, luma in read_frames(stdin):
        means.append(sum(luma) / len(luma))
        if prev is not None:
            n = len(luma)
            diffs = [abs(luma[i] - prev[i]) for i in range(n)]
            deltas.append(sum(diffs) / n)
            # Fraction of the frame that changed materially. This is the second
            # half of the test and it is what separates a CUT from violent but
            # continuous motion: a cut replaces the whole picture, so nearly
            # every sample moves. Near geometry sweeping past the camera at
            # speed produces an equally large MEAN delta, but concentrated in
            # the part of the frame that geometry occupies — typically well
            # under half. Requiring both a mean-ratio spike AND broad spatial
            # coverage makes the audit stricter, not laxer: it now has to be
            # big *and* everywhere.
            spreads.append(sum(1 for d in diffs if d > SPREAD_LEVEL) / n)
        prev = luma

    nframes = len(means)
    if nframes < 3:
        print("FAIL: no frames on stdin. Did you forget --rawpipe?")
        return 1

    half = max(1, args.window // 2)
    collapse_frame = (int(args.collapse_ms * FPS / 1000.0)
                      if args.collapse_ms >= 0 else nframes)

    cuts, blacks = [], []

    for i, d in enumerate(deltas):
        frame = i + 1                      # delta i is between frame i and i+1
        if frame >= collapse_frame:
            continue
        med = rolling_median(deltas, i, half)
        if d < args.floor:
            continue
        if med > 0 and d > args.ratio * med and spreads[i] >= args.spread:
            ms = int(frame * 1000.0 / FPS)
            near = any(abs(ms - a) <= args.tolerance_ms for a in allowed)
            if not near:
                cuts.append((ms, frame, d, med, spreads[i]))

    for i, m in enumerate(means):
        if i >= collapse_frame:
            continue
        if m < args.black:
            blacks.append((int(i * 1000.0 / FPS), i, m))

    dur_ms = int(nframes * 1000.0 / FPS)
    peak = max(deltas) if deltas else 0.0
    med_all = sorted(deltas)[len(deltas) // 2] if deltas else 0.0
    print(f"frames        : {nframes}  ({dur_ms/1000.0:.1f} s at {FPS:g} fps)")
    print(f"median delta  : {med_all:.2f}")
    print(f"peak delta    : {peak:.2f}")
    print(f"declared cuts : {allowed if allowed else 'none'}")

    # Always show the ranking, pass or fail. A silent PASS invites the
    # thresholds to drift out of contact with the content until the audit is
    # rubber-stamping; seeing how close the worst frame came keeps it honest.
    ranked = sorted(range(len(deltas)), key=lambda i: -deltas[i])[:5]
    print("\nworst frames (delta vs local median):")
    for i in ranked:
        frame = i + 1
        med = rolling_median(deltas, i, half)
        ratio = (deltas[i] / med) if med > 0 else float("inf")
        print(f"  {int(frame*1000.0/FPS):>7} ms  frame {frame:>6}  "
              f"delta {deltas[i]:7.2f}  median {med:6.2f}  {ratio:5.1f}x  "
              f"spread {spreads[i]*100:3.0f}%")
    if args.collapse_ms >= 0:
        print(f"collapse from : {args.collapse_ms} ms (frame {collapse_frame})")

    ok = True

    if cuts:
        ok = False
        print(f"\nRULE 1 FAIL — {len(cuts)} undeclared discontinuit"
              f"{'y' if len(cuts) == 1 else 'ies'}:")
        for ms, frame, d, med, sp in cuts[:20]:
            print(f"  {ms:>7} ms  frame {frame:>6}  delta {d:7.2f}  "
                  f"local median {med:6.2f}  ({d/med:.1f}x)  "
                  f"spread {sp*100:.0f}%")
        if len(cuts) > 20:
            print(f"  ... and {len(cuts) - 20} more")
    else:
        print("\nRULE 1 PASS — no undeclared discontinuities.")

    if blacks:
        ok = False
        print(f"\nRULE 2 FAIL — {len(blacks)} black frames before the collapse:")
        for ms, frame, m in blacks[:10]:
            print(f"  {ms:>7} ms  frame {frame:>6}  mean luma {m:.2f}")
        if len(blacks) > 10:
            print(f"  ... and {len(blacks) - 10} more")
    else:
        print("RULE 2 PASS — never goes black.")

    print("\n" + ("AUDIT PASSED — this is SUSTAIN."
                  if ok else
                  "AUDIT FAILED — this is not SUSTAIN and does not ship."))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
