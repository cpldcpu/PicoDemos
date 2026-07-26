#!/usr/bin/env python3
"""
analyze_music.py — check the track can carry a demo that never cuts, then
describe its shape so the arc can be authored TO it.

QUICKSILVER's version of this tool found beat onsets to snap scene cuts to.
SUSTAIN has no cuts, so this asks different questions:

  1. DOES THE PEDAL EVER STOP?  assets/PROMPTS.md #9 calls this the single
     disqualifying flaw: "if the pedal tone drops out at a section change,
     that take is unusable no matter how good it sounds." A bar of silence in
     the music is an audible boundary, and this demo's whole claim is that it
     provides none. Measured as bass-band (30-120 Hz) energy over time.

  2. IS THERE A HARD STOP ANYWHERE?  Same argument for full-band energy.

  3. WHAT IS THE ENERGY CURVE?  Camera speed is SUSTAIN's pacing instrument in
     place of cuts (PLANNING.md §6.5), so the camera must accelerate and stall
     with the track. This prints the curve to author against.

  4. WHERE ARE THE STRUCTURAL BOUNDARIES?  Not to cut on — to place morphs on,
     so the world finishes becoming something as the music does.

    python tools/analyze_music.py "assets/Sustained Bass Drone.mp3"
"""

import sys

import librosa
import numpy as np

SR = 22050              # the rate the demo actually plays at
HOP = 512
BASS_LO, BASS_HI = 30, 120

# A window quieter than this fraction of the track's median counts as a drop-out.
PEDAL_FLOOR_FRAC = 0.15
SILENCE_FLOOR_FRAC = 0.10


def sparkline(vals, width=100):
    blocks = " .:-=+*#%@"
    v = np.asarray(vals, dtype=float)
    if len(v) > width:
        idx = np.linspace(0, len(v) - 1, width).astype(int)
        v = v[idx]
    lo, hi = float(v.min()), float(v.max())
    if hi - lo < 1e-9:
        return blocks[0] * len(v)
    norm = (v - lo) / (hi - lo)
    return "".join(blocks[min(len(blocks) - 1, int(n * len(blocks)))] for n in norm)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = sys.argv[1]

    y, sr = librosa.load(path, sr=SR, mono=True)
    dur = len(y) / sr
    t_of = lambda f: f * HOP / sr                                    # noqa: E731

    # ---- full-band and bass-band energy ---------------------------------
    rms = librosa.feature.rms(y=y, hop_length=HOP)[0]

    S = np.abs(librosa.stft(y, n_fft=2048, hop_length=HOP))
    freqs = librosa.fft_frequencies(sr=sr, n_fft=2048)
    band = (freqs >= BASS_LO) & (freqs <= BASS_HI)
    bass = S[band, :].mean(axis=0)

    rms_med, bass_med = float(np.median(rms)), float(np.median(bass))

    print(f"file          : {path}")
    print(f"duration      : {dur:.1f} s  ({int(dur//60)}:{dur%60:04.1f})")
    print(f"sample rate   : {sr} Hz mono (as played)")

    # ---- 1 & 2: the disqualifying checks ---------------------------------
    def dropouts(curve, med, frac, min_ms=250):
        thr = med * frac
        below = curve < thr
        runs, start = [], None
        for i, b in enumerate(below):
            if b and start is None:
                start = i
            elif not b and start is not None:
                if t_of(i) - t_of(start) >= min_ms / 1000.0:
                    runs.append((t_of(start), t_of(i)))
                start = None
        if start is not None and t_of(len(below)) - t_of(start) >= min_ms / 1000.0:
            runs.append((t_of(start), t_of(len(below))))
        return runs

    # Ignore the natural fade at the very end — the outro is allowed to resolve.
    tail_cut = dur - 6.0

    all_gaps = [r for r in dropouts(bass, bass_med, PEDAL_FLOOR_FRAC, min_ms=150)
                if r[0] < tail_cut]
    silences = [r for r in dropouts(rms, rms_med, SILENCE_FLOOR_FRAC)
                if r[0] < tail_cut]

    # A pulsing bassline is not a failing pedal. Short, regularly spaced gaps
    # are the GROOVE — the ear reads them as rhythm, not as a boundary, which
    # is the only thing this demo cares about. What disqualifies a take is a
    # gap long enough to sound like the music stopped. Judging every gap as a
    # failure (the first version of this check did) rejects any track with a
    # rhythmic bass, which is nearly all of them.
    GROOVE_MAX_S = 0.8
    groove = [r for r in all_gaps if r[1] - r[0] < GROOVE_MAX_S]
    structural = [r for r in all_gaps if r[1] - r[0] >= GROOVE_MAX_S]

    print(f"\nbass {BASS_LO}-{BASS_HI} Hz : median {bass_med:.4f}, "
          f"min {float(bass.min()):.4f}")
    if groove:
        gd = [b - a for a, b in groove]
        print(f"  groove    : {len(groove)} short gaps, "
              f"mean {sum(gd)/len(gd):.2f} s, max {max(gd):.2f} s "
              f"— rhythmic articulation, not a boundary.")
    if structural:
        print(f"  PEDAL FAIL — {len(structural)} gap(s) >= {GROOVE_MAX_S}s, "
              f"long enough to hear as a stop:")
        for a, b in structural[:10]:
            print(f"    {a:7.2f} s -> {b:7.2f} s   ({b-a:.2f} s)")
        print("    Place no morph here; keep the camera moving through it.")
    else:
        print(f"  PEDAL OK — no bass gap reaches {GROOVE_MAX_S}s. The track "
              f"can carry a demo with no boundaries.")

    if silences:
        print(f"  HARD STOPS — {len(silences)} near-silent stretch(es):")
        for a, b in silences[:10]:
            print(f"    {a:7.2f} s -> {b:7.2f} s   ({b-a:.2f} s)")
    else:
        print("  NO HARD STOPS — full-band energy never collapses.")

    # ---- 3: the energy curve --------------------------------------------
    win = max(1, int(2.0 * sr / HOP))                 # 2 s smoothing
    smooth = np.convolve(rms, np.ones(win) / win, mode="same")
    print(f"\nenergy curve (0 -> {dur:.0f} s), each cell ~{dur/100:.1f} s:")
    print("  " + sparkline(smooth))

    peak_t = t_of(int(np.argmax(smooth)))
    print(f"  peak energy at {peak_t:.1f} s "
          f"({peak_t/dur*100:.0f}% through)")

    # Where does the outro begin? Last sustained fall below the median.
    half = len(smooth) // 2
    outro_i = None
    for i in range(len(smooth) - 1, half, -1):
        if smooth[i] > np.median(smooth):
            outro_i = i
            break
    if outro_i:
        print(f"  energy last above median at {t_of(outro_i):.1f} s "
              f"-> outro runs {dur - t_of(outro_i):.1f} s")

    # ---- 4: structural boundaries ---------------------------------------
    mfcc = librosa.feature.mfcc(y=y, sr=sr, hop_length=HOP, n_mfcc=13)
    for n in (8, 10):
        bounds = librosa.segment.agglomerative(mfcc, n)
        times = sorted({round(float(t_of(b)), 1) for b in bounds})
        print(f"\n{n} structural boundaries (s): "
              + ", ".join(f"{t:.1f}" for t in times))

    return 1 if (pedal_gaps or silences) else 0


if __name__ == "__main__":
    sys.exit(main())
