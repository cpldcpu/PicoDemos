# COLOSSUS · LATENT · 2026

**A monument.** Five minutes and seven seconds of one subject — a colossal
working machine standing on a plain, seen one part at a time and then, at
last, whole — with a wall-of-voices score synthesised on the chip.

The group's twentieth production, and the first made by three models in
their own roles: direction and music **Phosphor** (Claude Fable 5.1), the
body, the look and the painted art **Phase** (GPT-6 Astra), code, platform
and hardware **Overscan** (Claude Opus 5). Critic and producer: **Azure**.

[Watch the complete demo](media/colossus.mp4) · [RP2350 firmware](colossus_vga_rp2350.uf2) · [Windows launcher](Run%20Colossus.cmd) · [The plan](PLANNING.md) · [The briefs](briefs/)

![COLOSSUS](briefs/sketches/round6/chapter-8-colossus.png)

## Watch it

Double-click **Run Colossus.cmd**. The desktop player uses the same C
renderer, score and synth as the firmware; the launcher builds it if needed.
**Space** pauses, **left/right** seek a phrase, **,** and **.** a bar, **s**
saves a native-size still, **f** is fullscreen. Seeking is the test of the
renderer's one promise: every frame is a pure function of the audio sample
being played, so a seek lands on exactly the picture a straight play-through
would have shown.

The [MP4](media/colossus.mp4) is the whole run from the host renderer at
60 fps with the synth's output; the audio is byte-identical to what the
device plays (see *Referees*). It is not a recording of the Pico.

## The piece

Azure asked for something in the spirit of *Dope* (Complex, 1995), and what
the scene remembers about that demo is not an effect: the music carries it,
the pacing is slow and confident, it is an object show with a through-line,
and it never hard-cuts. COLOSSUS takes those four rules and one subject.

The machine has always been working. Over five minutes we come to
understand what moves and why. Every part is built on one construction
principle — paired bronze load-bearing ribs enclosing a pale chrome tendon,
with one rib deliberately missing — so the finger, the chest opening, the
vertebra and the split crown read as one anatomy. One shoulder sits low
under load the whole way, and at the reveal it takes the weight and rises.
The body stays cold at the end; the sky warms. The machine does not get its
dawn. We do.

| Bars | Time | Chapter | What happens |
|---|---|---|---|
| 0–7 | 0:00 | Overture | The wordmark out of black; one bronze edge catches light. The one tone. |
| 8–23 | 0:15 | The plain | Slabs, ground haze, part of a shoulder. The whole body is withheld. |
| 24–39 | 0:37 | I · HAND | Three fingers and an opposed thumb, sky in the gaps, slowly taking tension. Theme A. |
| 40–55 | 1:07 | II · HEART | One eccentric and two strokes behind the sternum, warm light from inside. |
| 56–71 | 1:38 | III · EYE | Still, for theme B alone. Then the camera passes through the aperture. |
| 72–87 | 2:09 | IV · LOAD | The transmission chamber: counterweights down, tendon up, furnace light below. |
| 88–111 | 2:40 | V · SPINE | One continuous ascent: enclosed, alongside a shoulder, into open sky. |
| 112–127 | 3:26 | VI · CROWN | The split crown in profile against cold sky, the eye lit from within. Up a tone. |
| 128–143 | 3:56 | COLOSSUS | Pull back until it stands whole. The shoulder rises. Dawn behind it. |
| 144–159 | 4:26 | Coda | The same world held under the credits. The tone is left alone. |

Chapters change on phrase downbeats through a local, lit veil — dense
embers and a warm glow around the part being exchanged, the outgoing
structure legible through it — never through black and never with a hard
cut. Each chapter carries an inscription in a drawn serif, the group's
answer to *Dope*'s routine boxes.

## The music

D minor at 125 BPM, a hundred and sixty bars in twenty phrases of eight,
written note by note in tables (`colossus/song.c`) and played by an integer
synth on core 1 (`colossus/synth.c`). Theme A climbs the chord and falls a
step, and its second half climbs higher and peaks on D6; theme B is a long
arch that rises to C6 and falls to the leading tone; the two stack from the
spine on. A drone on D opens the demo alone and closes it alone.

After Azure heard the first version and asked for *the many-voice organ and
chorus from Dope*, the synth gained a drawbar organ (six harmonics per note
from one phase accumulator, since phase × k is exactly the k-th harmonic), a
formant choir (detuned saws through three band-passes at the formants of an
open "ah"), a stereo chorus on the wide bus, a hall reverb, the lead doubled
at the octave and a boom under every crash. The tempo is 125 and not 128
because a 16th has to be an integer number of samples and control ticks:
2,880 samples, sixty ticks of 48.

The picture follows the audio clock. A frame asks the DMA what sample is
playing and draws that moment; render time that varies drops frames, and
never slows the music or drifts the sync. Zero audio underruns over the
whole run, on the device.

## On the Pico

Target: **Pico 2 / RP2350, Cortex-M33 at 300 MHz and 1.20 V**, on the
Pimoroni VGA Demo Base. Hold BOOTSEL while connecting and copy
`colossus_vga_rp2350.uf2` to the boot drive. Video is 320×240, 15-bit,
doubled to 640×480 VGA; audio is 24 kHz stereo PWM on GP28/GP27.

Core 0 renders into the back page and presents it at scanline zero. Core 1
scans out through `vga_mode_320x240_60`, whose `yscale` of 2 makes
scanvideo repeat each generated line itself — 240 line copies a frame
instead of the 480 every earlier demo here paid for — and pumps the synth
in the gap after each buffer.

Measured on the device over the whole 5:07:

| | |
|---|---|
| Frames presented | 16,700 (mean 54 fps) |
| Frames under the 30 fps floor | **0** |
| Worst frame | 24.1 ms; none held longer than two refreshes |
| Audio underruns | **0** |
| Per-second audio hashes matching the host | **306 of 306** |
| Heap free at boot / measured boot floor | 36,864 B / 10,568 B |
| Painted art in flash | 76,928 B |

*The per-phrase table and the experiments behind these figures are in
[briefs/numbers.md](briefs/numbers.md) and Overscan's replies.*

## How it fits

Solid 3D with an 8-bit reciprocal depth buffer mapped per scene; flat,
Gouraud, matcap chrome and indexed-texture span loops; a dedicated floor
path; bloom from intended emissive sources only, in an 80×60 field
composited in bounded rectangles; embers reconstructed from their id and
the sample, so there is no particle array and no state to carry; a 1-bit
inscription atlas. The body is one parameter file (`assets/body_parameters.json`)
that drives the flat silhouette, every chapter's mesh and the reveal's
shoulder lift alike.

The painted assets — the wordmark, two skies, three environment maps,
bronze, stone, the tendon, the furnace tile, the ember stamps — are
converted by allocating the shared 256-entry palette first (weighted
k-means in the joint dusk/dawn space, snapped to the DAC grid) and then
assigning nearest. The dusk and dawn skies share one index image, so the
dawn is a palette blend and the sky stays a table plus a memcpy.

## Referees

Every claim above has a check, and each of them can fail:

1. **Sync** — `tools/sync_check.py`: every chapter boundary and transition
   event lands on a phrase downbeat of the score.
2. **Audio identity** — the synth hashes its output once a second; the
   device prints the latches and `tools/serial_read.py` diffs them against
   the host table. Shown to catch a single flipped bit before the first run.
3. **Frame rate** — the device prints min/avg/max render time, worst
   displayed-frame interval, missed deadlines and underruns per phrase, with
   the full score playing. The table above quotes it.
4. **The film** — `tools/film_check.py` over the whole-run capture: no black
   or flat frame outside the overture and the last bar, black only at the
   end, the inscription present at each chapter's entrance. Plus the human
   check: native-size stills of every chapter and transition, enlarged
   without smoothing, in `briefs/sketches/`.

`build.ps1 check` runs all of them with the memory ledger and the score's
own checks.

## How it was made

Azure's brief was an all-star team for the twentieth demo. Phosphor wrote
the plan and the score and directed; Phase critiqued the plan (the load
chamber replacing a fire cloud, the construction principle, the memory
ledger before any feature are theirs), designed the body from a flat
silhouette up, built the engine's first rounds and painted the art through
Codex; Overscan built the platform, took the renderer over when Azure asked
for Codex to be used sparingly, and did every hardware run. Every brief and
every reply is a dated file in `briefs/`, so the record of who decided what
is in the repository.

Some of what it cost: a heap probe that panicked the board with the exact
message it was written to prevent, and a stock panic that ends in a
HardFault with USB masked; a boot floor inherited from PERSISTENCE that was
seven times too pessimistic; a renderer at 18.5 fps whose fixed cost was the
sky and the floor, not the triangles; hot code annotated for SRAM that the
compiler had inlined into an unmarked caller; and a sky that banded because
a painting had fewer colours than its gradient, which no dither could put
back.

## Credits

- **Phosphor** (Claude Fable 5.1) — direction, plan, score, synth.
- **Phase** (GPT-6 Astra) — the body, the look, the engine's first rounds, all painted art.
- **Overscan** (Claude Opus 5) — platform, renderer, tools, hardware.
- **Azure** — critic and producer.

*COLOSSUS · LATENT · 2026*
