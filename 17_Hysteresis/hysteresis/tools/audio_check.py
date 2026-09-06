"""Measure the soundtrack instead of trusting it.

    host/synthwav.exe -o host/hysteresis.wav
    host/synthwav.exe -o host/solo_impact.wav --solo impact
    python tools/audio_check.py host/hysteresis.wav --impacts host/solo_impact.wav

Three things this checks, all of which were wrong at least once:

1. TUNING. Each impact in score.c names a MIDI note, and the resonators are
   tuned from baked per-note rotation coefficients (synth_tables.h) precisely
   because deriving them from a shared sine table put D2 a semitone and a half
   flat. This finds the dominant partial shortly after each strike and reports
   the error in cents.

   MEASURE THIS ON A --solo impact RENDER. On the full mix the D2 strikes read
   up to 12 cents flat, and that is the meter and not the synth: the bass pedal
   is D1, so its second harmonic is D2 to the Hertz, and the pedal's detuned
   companion drags the composite FFT peak off the struck pitch. Raising the
   bass saw by 6 dB moved the "error" from -8.9 to -12.1 cents, which is how it
   was identified. With the bed muted the same strikes come out inside a cent.

2. SPECTRAL BALANCE. Energy per band at four moments, in dB relative to the
   loudest band, because percentages are misleading here -- a lowpassed saw
   keeps nearly all its energy in its fundamental, so any honest dark ambient
   mix looks bottom-heavy by percentage even when it is perfectly audible in
   the midrange. What this is really watching for is a band that has dropped
   out entirely, which is what happened at 62 s before the pad got an octave
   oscillator per note.

3. DISCONTINUITIES. Strikes are deliberate impulses, so the largest step is
   always large and that is not a defect. The number worth watching is the
   99.99th percentile, which broadband noise raises legitimately and a
   control-rate parameter jump raises audibly -- clicks are what a 344 Hz
   control rate produces if a level is not smoothed.
"""

import sys
import wave

import numpy as np

RATE = 22050


def load(path):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 1, "mono expected"
        assert w.getsampwidth() == 2, "16-bit expected"
        assert w.getframerate() == RATE, f"{RATE} Hz expected"
        raw = w.readframes(w.getnframes())
    return np.frombuffer(raw, dtype="<i2").astype(np.float64)


def dominant_hz(x, lo, hi):
    """Peak of the magnitude spectrum within [lo, hi], parabolically refined."""
    n = len(x)
    win = np.hanning(n)
    mag = np.abs(np.fft.rfft(x * win))
    freqs = np.fft.rfftfreq(n, 1.0 / RATE)
    band = (freqs >= lo) & (freqs <= hi)
    idx = np.argmax(np.where(band, mag, 0.0))
    # Parabolic interpolation on the log magnitude: the FFT bin is 5.4 Hz wide
    # at this window length, which on its own is 250 cents at D1.
    if 0 < idx < len(mag) - 1:
        a, b, c = (np.log(mag[idx - 1] + 1e-9), np.log(mag[idx] + 1e-9),
                   np.log(mag[idx + 1] + 1e-9))
        off = 0.5 * (a - c) / (a - 2 * b + c)
    else:
        off = 0.0
    return (idx + off) * RATE / n


def midi_hz(n):
    return 440.0 * 2.0 ** ((n - 69) / 12.0)


# (seconds, MIDI note) from score.c. Chosen to span the range actually used:
# the bottom D at 152 s is the one the table's precision was needed for.
STRIKES = [(5, 38), (14, 50), (29, 53), (98, 60), (116, 38), (152, 26)]

BANDS = [("sub 20-60", 20, 60), ("low 60-250", 60, 250),
         ("mid 250-1k", 250, 1000), ("high 1k-4k", 1000, 4000),
         ("air 4k-11k", 4000, 11000)]


def main():
    args = sys.argv[1:]
    path = "host/hysteresis.wav"
    impacts = None
    i = 0
    while i < len(args):
        if args[i] == "--impacts" and i + 1 < len(args):
            impacts = args[i + 1]
            i += 2
        else:
            path = args[i]
            i += 1

    x = load(path)
    print(f"{path}: {len(x)} samples, {len(x)/RATE:.1f} s, "
          f"peak {int(np.max(np.abs(x)))}/32767")

    tune_src = load(impacts) if impacts else x
    print(f"\n-- tuning: dominant partial after each strike "
          f"({'solo impacts' if impacts else 'FULL MIX, see docstring'}) --")
    worst = 0.0
    for sec, note in STRIKES:
        want = midi_hz(note)
        # Start 120 ms in so the thud and the noise burst are gone, and window
        # long enough to resolve 37 Hz.
        a = int((sec + 0.12) * RATE)
        seg = tune_src[a:a + 16384]
        got = dominant_hz(seg, want * 0.72, want * 1.4)
        cents = 1200 * np.log2(got / want)
        worst = max(worst, abs(cents))
        print(f"  t={sec:3d}s  MIDI {note:3d}  want {want:8.2f} Hz  "
              f"got {got:8.2f} Hz  {cents:+6.1f} cents")
    limit = 5 if impacts else 15
    print(f"  worst error: {worst:.1f} cents  "
          f"({'OK' if worst < limit else 'BROKEN'}, limit {limit})")

    print("\n-- spectral balance (dB relative to the loudest band) --")
    print("  t      " + "".join(f"{n:>13}" for n, _, _ in BANDS))
    for sec in (20, 62, 126, 180):
        seg = x[sec * RATE:sec * RATE + 32768]
        mag = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
        freqs = np.fft.rfftfreq(len(seg), 1.0 / RATE)
        e = np.array([mag[(freqs >= lo) & (freqs < hi)].sum()
                      for _, lo, hi in BANDS])
        db = 10 * np.log10(np.maximum(e, 1e-9) / max(e.max(), 1e-9))
        print(f"  {sec:3d}s  " + "".join(f"{v:12.1f}dB" for v in db))

    print("\n-- discontinuities --")
    d = np.abs(np.diff(x))
    j = int(np.argmax(d))
    print(f"  largest step        {int(d[j]):6d} at t={j/RATE:.2f}s (a strike)")
    print(f"  99.99th percentile  {int(np.percentile(d, 99.99)):6d}")
    print(f"  median              {int(np.median(d)):6d}")


if __name__ == "__main__":
    main()
