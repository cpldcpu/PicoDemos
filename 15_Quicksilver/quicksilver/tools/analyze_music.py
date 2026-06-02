#!/usr/bin/env python3
"""Offline analysis of the demo soundtrack to find scene-boundary candidates.

Uses librosa for proper beat tracking + onset detection + structural
segmentation. The structural segmentation (recurrence-matrix based) is
the headline feature — it finds where the *character* of the music
changes (verse vs chorus vs breakdown), not just energy jumps.

Usage:
  python analyze_music.py [path/to/music.mp3] [-k N_SEGMENTS]

Needs: librosa, numpy. No ffmpeg required (librosa decodes via audioread
or soundfile).
"""

import sys
import os
import argparse
import numpy as np
import librosa

# Resampling to 22050 Hz mono matches the demo's QOA playback rate and
# keeps the analysis cheap.
SR = 22050

def fmt_mmss(t):
    return f"{int(t)//60}:{int(t)%60:02d}"

def beats_and_tempo(y, sr):
    """librosa's DP-based beat tracker. Returns (tempo_bpm, beat_times)."""
    tempo, beat_frames = librosa.beat.beat_track(y=y, sr=sr, units="frames")
    beat_times = librosa.frames_to_time(beat_frames, sr=sr)
    return float(np.atleast_1d(tempo)[0]), beat_times

def strong_onsets(y, sr, top_n=20, refractory_s=2.5):
    """Strongest superflux onsets, refractory-suppressed."""
    onset_env = librosa.onset.onset_strength(
        y=y, sr=sr, aggregate=np.median
    )
    times = librosa.frames_to_time(np.arange(len(onset_env)), sr=sr)
    # Pick local maxima, then keep top-N by strength with min spacing.
    peaks = librosa.util.peak_pick(
        onset_env, pre_max=10, post_max=10,
        pre_avg=20, post_avg=20, delta=onset_env.std()*0.5, wait=10
    )
    if len(peaks) == 0: return []
    strengths = onset_env[peaks]
    order = np.argsort(strengths)[::-1]
    picked = []
    chosen_times = []
    for idx in order:
        p = peaks[idx]
        t = times[p]
        if any(abs(t - ct) < refractory_s for ct in chosen_times):
            continue
        picked.append((t, float(strengths[idx])))
        chosen_times.append(t)
        if len(picked) >= top_n: break
    picked.sort(key=lambda x: x[0])
    return picked

def structural_segments(y, sr, k=8):
    """Agglomerative clustering of beat-synchronous CQT features.
    Returns the time boundaries between segments — these are the
    structural transitions in the music (verse → chorus → break).
    """
    # Beat-synchronous chroma + MFCC features capture both harmonic
    # and timbral character.
    tempo, beats = librosa.beat.beat_track(y=y, sr=sr, trim=False)
    beat_times = librosa.frames_to_time(beats, sr=sr)
    if len(beats) < k + 2:
        return [], []

    # CQT-based chroma: harmonic content.
    chroma = librosa.feature.chroma_cqt(y=y, sr=sr)
    chroma_sync = librosa.util.sync(chroma, beats, aggregate=np.median)

    # MFCC: timbre.
    mfcc = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=13)
    mfcc_sync = librosa.util.sync(mfcc, beats)

    # Stack and normalise.
    feats = np.vstack([
        librosa.util.normalize(chroma_sync, axis=1),
        librosa.util.normalize(mfcc_sync,   axis=1),
    ])

    # Agglomerative segmentation into k groups.
    bounds = librosa.segment.agglomerative(feats, k=k)
    bound_times = librosa.frames_to_time(beats[bounds], sr=sr)
    # Pair each segment with its mean feature so the report can hint at
    # which segments are similar (likely repeats).
    seg_starts = bound_times
    seg_ends   = np.concatenate([bound_times[1:], [len(y)/sr]])
    return list(zip(seg_starts, seg_ends)), beat_times

def snap_to_beat(t, beats):
    """Return the beat time nearest to t."""
    if len(beats) == 0: return t
    i = int(np.argmin(np.abs(beats - t)))
    return float(beats[i])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path", nargs="?", default="music.mp3")
    ap.add_argument("-k", "--segments", type=int, default=8,
                    help="number of structural segments to find (default 8)")
    args = ap.parse_args()

    if not os.path.exists(args.path):
        print(f"not found: {args.path}", file=sys.stderr)
        return 1

    print(f"loading {args.path} via librosa @ {SR} Hz mono ...")
    y, sr = librosa.load(args.path, sr=SR, mono=True)
    dur = len(y) / sr
    print(f"length: {dur:.2f} s ({fmt_mmss(dur)})")

    print("\nbeat tracking ...")
    tempo, beats = beats_and_tempo(y, sr)
    print(f"  tempo: {tempo:.1f} BPM   (beat = {60000.0/tempo:.0f} ms, "
          f"bar @ 4/4 = {60.0/tempo*4:.2f} s)")
    print(f"  found {len(beats)} beats")

    print(f"\n--- strongest onsets (likely musical hits / drops) ---")
    onsets = strong_onsets(y, sr, top_n=20, refractory_s=2.5)
    for t, strength in onsets:
        print(f"  {fmt_mmss(t)}  ({t:6.2f} s)   strength={strength:.2f}")

    print(f"\n--- structural segments (k={args.segments}, "
          f"beat-synchronous chroma+MFCC clustering) ---")
    segments, _ = structural_segments(y, sr, k=args.segments)
    for i, (s, e) in enumerate(segments):
        print(f"  seg {i}: {fmt_mmss(s):>5} – {fmt_mmss(e):<5}  "
              f"({s:6.2f} – {e:6.2f} s, {e-s:5.2f} s)")

    # Quote the current QUICKSILVER timeline (see ../timeline.c) next to the
    # proposed boundaries so the diff is easy to see. Keep these in sync with
    # timeline.c when scenes are re-cut.
    # NOTE: keep in sync with timeline.c. Cuts laid on the structural segment
    # edges / strongest onsets of "Taiko Dorian Bells".
    current = [
        (  0.0,   "title"),
        ( 33.69,  "rotozoom (DROP 1)"),
        ( 63.48,  "liquid (build)"),
        (100.10,  "chrome A (obj 0-2, peak)"),
        (126.66,  "mode7 (sustained cruise)"),
        (180.56,  "chrome B (obj 3-5, 2nd drop)"),
        (224.68,  "credits (outro)"),
        (249.40,  "<end>"),
    ]
    print(f"\n--- current timeline vs nearest segment boundary ---")
    seg_starts = np.array([s for s, _ in segments])
    for t, name in current:
        marker = " PAST!" if t > dur else ""
        if len(seg_starts):
            nearest = float(seg_starts[np.argmin(np.abs(seg_starts - t))])
            delta   = nearest - t
            beat_t  = snap_to_beat(t, beats)
            print(f"  {fmt_mmss(t):>5} ({t:6.2f}s)  {name:<18}  "
                  f"nearest seg @ {fmt_mmss(nearest):>5} ({delta:+5.2f}s)   "
                  f"nearest beat @ {beat_t:6.2f}s{marker}")
        else:
            print(f"  {fmt_mmss(t):>5} ({t:6.2f}s)  {name}{marker}")
    print(f"\n  music actually ends at {fmt_mmss(dur)} ({dur:.2f} s)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
