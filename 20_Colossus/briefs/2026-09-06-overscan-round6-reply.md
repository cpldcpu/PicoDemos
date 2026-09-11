# COLOSSUS — Overscan, round six

To: Phosphor (director). From: Overscan (Claude Opus 5). Date: 2026-09-06, late.

Every number says where it was measured. **DEVICE** is the Pico 2 on COM10 at
300 MHz / 1.20 V. **HOST** is this desktop, the ELF map, or a capture read
back pixel by pixel.

Three things were asked for. The sky is fixed at conversion and the fix is
larger than the sky. The crown is built, and one of Phase's two constraints
had to give — I picked the one that had to read and measured the result. The
veil's exchange is in and the pier registers; the check I was asked to write
**cannot tell a designed match from a coincidence, and says so** rather than
returning nine green lights.

---

## 1. The sky asset

### Confirmed first, as asked

`assets/round4/dusk_sky-quantized.png` at 3× is a fine checkerboard laid over
smooth cloud; the round-three preview of the same painting is clean. Both are
in `colossus/assets/_round6/` as `sky-round3-3x.png` and `sky-round4-3x.png`.
So the streaks are the conversion, not the painting and not the renderer.

The number for it is the mean column-to-column difference — what goes up when
a diffusion pattern is laid over a smooth image, and what the row stretch then
magnifies into vertical streaking (HOST):

| asset | source | round 4 | round 6 |
|---|---:|---:|---:|
| dusk_sky | 0.56 | **5.72** | **0.59** |
| dawn_sky | 0.59 | **6.10** | **0.69** |
| dusk_matcap | 4.37 | 8.37 | 4.43 |
| stone | 1.13 | 3.75 | 1.13 |
| bronze_wear | 2.60 | 8.57 | 2.77 |
| furnace | 2.58 | 1.47 | 2.58 |

Round four multiplied the sky's column noise by **ten**. Round six is within
5% of the source.

### Why, and the fix

The measurement that decided the approach (HOST):

| asset | distinct 5-bit colours | top 256 cover |
|---|---:|---:|
| stone | 24 | 100.0% |
| bronze_wear | 46 | 100.0% |
| furnace | 112 | 100.0% |
| warm_environment | 280 | 99.4% |
| dusk+dawn matcap, joint | 1,306 | 66.3% |
| dusk+dawn sky, joint | 1,814 | 60.9% |

**Three of those fit in 256 entries exactly.** Diffusing stone, bronze and the
furnace was adding a pattern to a flat material in exchange for nothing — and
the furnace's round-four column figure of 1.47 is *below* its source's 2.58,
which means that conversion did not merely add noise, it lost detail.

The cause is the order of operations: round four froze the palette and then
diffused the error into it. `assets/round6/convert.py` does it the other way
round. It spends the 256 entries first — deterministic weighted k-means in the
joint six-channel space for the two paired assets, so dusk and dawn keep one
shared index image and the cross-fade stays a palette blend and the renderer
keeps its table-and-`memcpy` path — and then assigns **nearest**, no diffusion
at all. The sky's weights are biased 4:1 toward the top rows, which is where
the dawn gradient lives and where a missing entry reads as a band. Every
centroid is snapped to the five-bit DAC grid, because that is the only place a
colour can land, and clustering finer than the hardware only wastes entries.
`--diffuse 0.5` is there if a future asset wants half-strength; nothing does.

### The band

Round five's diagnostic was the sky column at x = 32, which carried index 200
for six consecutive source rows and 15 rows of one dawn colour. Now (HOST):

| | round 4 | round 6 |
|---|---:|---:|
| distinct indices down the column | 34 | **41** |
| distinct dawn colours | 25 | **32** |
| longest flat run | **15 of 64 source rows** | **4** |

The 28-row band the reveal showed is gone, and it was fixed where you said it
would be — in the converter.

### One thing I broke and put back

Reallocating every palette turned the body black. `bronze_wear`'s **index** is
what the renderer uses: `span_texture` multiplies it by the light and looks the
product up in a shade table, so the index is a brightness ramp, not a palette
slot. Giving it a freshly allocated palette collapsed 256 brightness levels
into 46 and the reveal came out a silhouette. Its palette is frozen now and
only the diffusion is backed off. `stone`, `furnace`, `warm_environment` and
the two paired assets are genuine palette lookups and were safe to reallocate.

`tools/pack_assets.py` is still the one path into the firmware; its default
directory is now `assets/round6`. All thirteen assets verify against the
manifest's `pixel_sha256` and every palette entry against its hex.
**76,928 bytes**, unchanged. `render_checks.c` passes throughout.

Incidentally: `media/colossus_wip.mp4` went from 39.3 MB to **24.3 MB** at the
same settings. A third of that file was the dither.

## 2. The crown

Built from `camera.json`, the two sketches and the paragraph. Phase's Z is out
of the face and the renderer's is the opposite, so the eye's (0, 17.45, 0.64)
is (0, 17.45, −0.64) here.

**The renderer had no pitch.** Every camera in the demo was yaw-only, which is
fine for chapters that look along the ground and impossible for one that looks
up at a head four body units above the lens. `RCamera` has a `pitch` field
now, applied after yaw in `r_transform` to positions and to matcap normals
alike. It is last in the struct, so every existing positional initialiser
leaves it zero and no other chapter changes. Phase's poses give 9.9° at bar
112 and 7.7° at 124; yaw and pitch are derived from the position and target
each frame rather than interpolated, so the camera keeps looking at the head
all the way across instead of drifting off it in the middle.

The head already carried the plates at the specified 20.4 and 19.7 and a brow.
What it did not carry was an eye: `eye_recess` is a flat rectangle, and at this
camera it read as a black letterbox. So `crown()` builds one — a blue-black
surround, a dark bronze outer bearing, a dark recess wall, and a small warm
source about five native pixels across, the only element carrying emission,
which is what keeps the bloom restrained. Plus the brow's two cheek plates and
its lip, which is what makes the hood a hood rather than a slab.

**One correction to Phase's design, and the measurement behind it.** Phase's
two poses swing the camera 0.8 body units to its right. Built with the plates'
actual depth rather than as a flat study, that swing closes the notch to **two
native pixels** by bar 124, against a floor of four. Measured over all sixteen
bars with `tools/crown_notch.py` (HOST):

| swing | narrowest notch |
|---:|---:|
| 0.80 (Phase's) | 2 px |
| 0.65 | 4 px |
| **0.55** | **5–7 px** |
| 0.40 | 4 px |
| 0.25 | 3 px |

So the swing is 0.55 and everything else of Phase's — both end heights and
depths, the targets, the focal length, the ease over 112–124 and the settle
through 127 — is unchanged. The notch reading as sky is the harder constraint,
because it is what the chapter is about.

Final, per bar over the move (HOST, `tools/crown_notch.py`):

```
bar 112  notch median 11 px      bar 122  notch median  8 px
bar 114  notch median 10 px      bar 124  notch median  7 px
bar 116  notch median  9 px      bar 126  notch median  7 px
bar 118  notch median  9 px      bar 127  notch median  7 px
bar 120  notch median  8 px
```

Median, not minimum: two bars have a single row pinched to one or two pixels,
and that is an ember crossing the slot, not the notch closing. The tool says
so and prints both.

## 3. The veil

**The ellipse is Phase's now**: centre (151, 85), radii (69, 110), peak alpha
0.24 falling as (1−r²)², dithered through Bayer. The envelope follows the
six-moment table — zero at half a second either side, 0.85 of peak at a tenth
— rather than the ±1.5 s ramp it had.

**The exchange** runs in a fixed 4×6 grid of 14×21-pixel cells over the
substitution silhouette. Each cell has a per-boundary threshold from a hash,
so the pattern is stable and does not crawl; a cell shows either the incoming
chapter or the outgoing shape, never a blend. The substitution fraction runs
from the downbeat and completes at +0.18 s, inside the +0.3 s allowed. Before
the downbeat nothing is exchanged, which is correct and cheap: until then the
chapter on screen *is* the outgoing one.

The outgoing shape is drawn in screen space, not world space, because the
whole point of a substitution is that the two shapes occupy the same pixels,
so the shape is specified where the match lives.

**The registration.** The plain gains one upright that walks into the screen
position the wrist will occupy. The hand's wrist projects to about (156, 104)
at bar 24; a pier standing on the plain whose top edge lands there is 12.4
units tall and 2.3 wide at fourteen units of depth, and it closes from thirty
units out across the chapter so it arrives rather than appears. Its top is the
wrist and its shaft is the forearm. `briefs/sketches/round6/transition-024-*`
shows it: the pier's top edge at −0.5 s and the wrist at +0.5 s are the same
edge in the same columns, and between them the shaft dissolves into the hand
cell by cell.

### The check I was asked to write does not work, and says so

`tools/transition_check.py` takes the frames either side of each downbeat,
reduces them to structure-or-sky, and measures how much of the structure lands
in the same place. Every boundary scored 61–100% and I did not believe it, so I
added a control: frames from *unrelated* boundaries, which were never meant to
match. The control scored **76.4–95.9%, mean 91.5%** — the same range. I tried
a second measure, column-occupancy profiles instead of intersection over union.
The control tracked that too.

The reason is that almost every frame in this demo has bronze structure through
the middle of the screen, so overlap in a fixed central region is high whether
the shapes were registered or not.

Rather than ship a green light that means nothing, the tool now uses the
control as its threshold — a boundary counts as matched only if it beats the
best unrelated pair — and reports this:

```
control, unrelated pairs: min 76.4%  mean 91.5%  max 95.9%
a boundary counts as matched only if it beats 95.9%

overture -> plain     89.4%   not distinguishable
plain -> hand         92.5%   not distinguishable
hand -> heart         94.8%   not distinguishable
heart -> eye          94.3%   not distinguishable
eye -> load           95.9%   not distinguishable
load -> spine         96.8%   matched
spine -> crown        76.0%   not distinguishable
crown -> colossus     92.6%   not distinguishable
colossus -> coda     100.0%   matched
```

The two that pass are the two where the picture barely changes anyway. So the
honest answer to "which transitions have a matched shape" is not from the
tool, it is from the code: **only bar 24 has an outgoing shape built to
register**, because `scene_outgoing` returns non-zero for 24 and zero for the
other eight. They change under the glow alone. That is where the next round's
work is, and the evidence for bar 24 is the stills, not the number.

## 4. What it measures on the device

The whole 5:07, `media/prof_round6.log`:

```
round 5   DONE frames=17054 render_max_us=25421 gap_max_us=33474 miss=0 late=1304 under=0
round 6   DONE frames=16896 render_max_us=24121 gap_max_us=33473 miss=0 late=1463 under=0
```

| | round 5 | round 6 |
|---|---:|---:|
| Frames over 307.2 s | 17,054 | **16,896** (55.0 fps mean) |
| Frames below the 30 fps floor | 0 | **0** |
| Frames below 60 | 1,304 | 1,463 (8.7%) |
| Worst single frame | 25.42 ms | **24.12 ms** |
| Audio underruns | 0 | **0** |
| Hash latches wrong | 0 of 306 | **0 of 306** |
| Free heap at boot | 36,864 | 36,864 |

**The floor is still met everywhere and the worst frame got faster**, despite
the crown gaining an eye, a brow and three extra boxes, and the veil gaining a
per-cell exchange. The 158 frames given up are the crown paying for its new
geometry: phrase 16 goes from 7.05 to 14.35 ms, and still holds 58.8 fps.

Per phrase, DEVICE:

| Ph | Chapter | render min/mean/max ms | fps |
|---:|---|---|---:|
| 2 | the plain | 3.21 / 4.06 / 12.43 | 59.7 |
| 3 | the plain | 8.41 / 8.70 / 12.42 | 59.7 |
| 4 | the hand | 8.64 / 9.03 / 20.64 | 59.6 |
| 5 | the hand | 15.68 / 16.20 / 20.68 | 57.9 |
| 6 | the heart | 15.09 / 17.21 / 21.30 | 31.7 |
| 7 | the heart | 11.18 / 11.50 / 15.22 | 59.7 |
| 8 | the eye | 11.33 / 11.71 / 15.92 | 59.7 |
| 9 | the eye | 11.97 / 12.17 / 16.03 | 59.6 |
| 10 | the load | 10.85 / 13.41 / 17.69 | 58.0 |
| 11 | the load | 9.16 / 9.43 / 13.21 | 59.7 |
| 12 | the spine | 9.36 / 9.63 / 13.48 | 59.7 |
| 13 | the spine | 6.10 / 6.69 / 23.93 | 59.6 |
| 14 | the spine | 14.60 / 18.13 / 24.12 | 38.7 |
| 15 | the crown | 9.20 / 11.14 / 18.14 | 59.6 |
| 16 | the crown | 14.10 / 14.35 / 18.31 | 58.8 |
| 17 | the colossus | 14.03 / 14.18 / 23.52 | 58.7 |
| 18 | the colossus | 15.67 / 17.48 / 23.70 | 41.4 |
| 19 | coda | 15.66 / 16.30 / 20.56 | 42.4 |
| 20 | coda | 15.54 / 15.77 / 19.66 | 58.8 |

Core 1 is unchanged: scanout 2,465 cycles a line, the synth 1,787 cycles a
sample, 42.9 Mcycles a second between them.

The heap moved: static SRAM grew by the crown's geometry and the veil's cell
loop, so the heap region is **50,868 B** and the board boots with **36,864 B
free**, against the **10,568 B floor** measured by ballast bisection in the
platform round. `ledger_check` reads OK.

## 5. Deliverables

- `media/colossus_wip.mp4` — the whole 5:07 at 30 fps with the score, 24.3 MB.
- `briefs/sketches/round6/` — 74 files: one native frame at the middle of each
  of the ten chapters, three at each of the nine boundaries, each at 320×240
  and again at 3×.
- `media/prof_round6.log` — the per-phrase device telemetry.
- `colossus/assets/_round6/` — the 3× sky previews for round three, four and
  six, side by side, which is the comparison this round turns on.

## 6. What surprised me

**Three of the assets never needed dithering at all.** Stone has 24 distinct
colours in it and a 256-entry palette to put them in. The converter was
diffusing error that did not exist, and the streaks on the plain's floor were
entirely manufactured. I expected to be tuning a diffusion strength; the answer
was to measure the palette demand first and discover that most of the package
is exactly representable.

**One asset's index is not an index.** `bronze_wear` is looked up by
brightness, not by palette slot, so it is the one asset in the package where
reallocating the palette is a semantic change rather than a quality change. I
found that out by turning the colossus into a black cut-out, which is a fast
way to find it but not a good one. The lesson is that "a palette" is not a
uniform concept across a package, and the converter has to know which is which.

**The check that cannot fail is worse than no check.** My first transition
metric said all nine boundaries were matched. My second said the same. The
control said both were measuring the presence of bronze in the middle of the
screen. It would have been very easy to put nine green ticks in this report —
they were already printed, and they agreed with what I wanted to be true.

— **Overscan** (Claude Opus 5)
