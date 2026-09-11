# TESSERA · LATENT · 2026

**One small thing can become a world.**

Phase / GPT-6 Astra: direction, renderer, score, synth and device validation.
Azure: critic and producer. The platform carries Overscan's existing work
from SLEEPER and HELION; neither Overscan nor Phosphor worked on this entry.

The material is a ceramic tessera: a tiny square with a bevel, a glaze, a
slight surface irregularity and its own colour. All 384 pieces persist
through the film. They assemble into a sheet, curl into a world, hang as
a ribbon, twist into a tower and open as a flower. Before each change,
the seams open. The ending gathers everything back into one small tile.

The presentation borrows from a printed exhibition catalogue: warm paper,
cobalt, vermilion, ochre, two painted ceramic gardens and generous margins. Hard projected
shadows place the objects on the painted stage. The tower enters moonlight as the
score drops to its quieter instrumentation. No prerecorded frames or audio.

| Bars (zero based) | Time | Picture | Music |
|---|---|---|---|
| 0–15 | 0:00–0:25.6 | GATHER: pieces arrive, then a moving tiled sheet | Wooden mallet introduces the motif; bass and kick enter at bar 8 |
| 16–31 | 0:25.6–0:51.2 | WORLD: the sheet closes into a ceramic globe | Brass question over the full broken beat |
| 32–47 | 0:51.2–1:16.8 | SUSPEND: globe unfurls into a twisting canopy | Brass answer, longer notes and extra shaker subdivisions |
| 48–63 | 1:16.8–1:42.4 | ASCEND: dark paper and a rising spiral tower | Mallet and soft accordion chords; restrained percussion returns halfway |
| 64–79 | 1:42.4–2:08 | FLOURISH: a five-lobed tiled bloom | F-major emphasis, full beat, question then answer |
| 80–95 | 2:08–2:33.6 | RETURN: a world, then one tessera and credits | Last refrain, instruments fall away, F–Bb–C–Dm cadence |

The score, **One Small Thing**, is 96 bars at exactly 150 BPM. A sixteenth is
2,400 stereo frames at 24 kHz. Every picture is a function of that audio
sample position. Lead onsets drive the traveling glaze accent; phrase
positions drive the seams. The score tables are immutable and callable from
either core, avoiding a shared mutable score cache.

The tune is written note by note, with deliberate rests and an eight-bar
question/answer structure. Harmonic movement is Dm–Bb–F–C in two-bar units;
the bloom rotates both tune and harmony to F–C–Dm–Bb. The last four bars
have their own cadence and end on D, instead of fading an unresolved loop.

Instrument design:

- Rounded octave bass: fundamental plus second harmonic, short gated phrases.
- Muted brass: four harmonic partials, a softened attack, restrained register.
- Soft accordion triads: three independently panned voices; no long pad wash.
- Wooden mallet: decaying 2:1 phase modulation, sparse phrasing.
- Membrane kick, dry snare, high-passed shaker and a short pitched rim.
- Quiet dotted-eighth/eighth stereo taps and a small room; the beat stays dry.

The implementation uses core 0's FPU for the surfaces and projection,
INTERP1 for affine glaze addresses, INTERP0 BLEND for the glaze palettes,
and XIP FIFO streaming into DMA channel 8 for the painted background copy while geometry is prepared. Glaze textures,
palettes, geometry and hot raster code are in SRAM. Core 1 generates VGA
scanlines and pumps the integer synth into two timer-paced PWM DMA rings.

Validation is part of the production. The desktop draws every frame at
60 fps with guards, re-renders selected frames on a different initial fill,
checks seeking, and compares PCM at arbitrary block sizes. The device logs
render cost, repeated fields, actual scanvideo missing-line fallback calls,
audio underruns and PCM hashes. Complete device runs must reach the final
sample with every hash matching. The measured results live in
`validation/`; goals in this document are not substitutes for those results.
