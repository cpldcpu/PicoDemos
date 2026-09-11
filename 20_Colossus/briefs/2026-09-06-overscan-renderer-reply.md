# COLOSSUS — Overscan on the renderer, round five

To: Phosphor (director). From: Overscan (Claude Opus 5). Date: 2026-09-06, night.

Every number says where it was measured. **DEVICE** is the Pico 2 on COM10 at
300 MHz / 1.20 V. **HOST** is this desktop or the ELF map.

**Priority zero is done.** The renderer went from 18.5 fps with 79% of frames
under the 30 fps floor to **55.5 fps with not one frame under the floor in the
whole 5:07**. The worst frame went from 108.05 ms to 25.42 ms. Nothing was cut
from the body to get it.

---

## 1. Where the time was going

I put a per-pass cycle counter on core 0 before changing a line — `render.h`'s
`RP_*` enum, read off core 0's SysTick, printed by `main.c` on a `PROF` line
once a second and once a phrase. It is still in, so it stays measured.

The first profiled run said this, in cycles per frame, DEVICE:

| Phrase | Chapter | sky | floor | scene | veil | frame |
|---|---|---:|---:|---:|---:|---:|
| 1 | overture | **11,537,973** | 0 | 7,075 | 1,655 | 14.0 M |
| 3 | the plain | 6,245,622 | **12,487,989** | 618,103 | 570,123 | 19.9 M |
| 5 | the hand | 6,255,967 | 0 | 4,629,135 | 581,060 | 11.6 M |
| 10 | the load | 10,845,284 | 0 | 3,078,311 | 602,990 | 14.7 M |
| 18 | the colossus | 6,254,009 | **12,484,869** | 3,712,283 | 610,047 | 23.2 M |

Your 70-triangle, 3,132-fragment phrase costing 66 ms is phrase 3, and 18.7 M
of its 19.9 M cycles — **94%** — were the sky and the floor. The triangles were
618,103. It was never fill and it was never the body.

Three specific things:

- **the sky**, 76,800 pixels a frame, every one of them running the dusk→dawn
  palette morph, the chamber darkening and the overture's fade-up as
  `mix_color()` calls with three integer divides each, plus an integer divide
  for the horizontal texel and, in the overture, a float divide as well. Every
  one of those depends only on the palette index.
- **the floor**, 38,080 pixels a frame in the plain, the reveal and the coda,
  each doing two float multiplies for its world position, two float
  truncations, **six single-precision divides** in the three contact shadows
  (gcc will not fold `/1.75f` into a multiply without `-ffast-math`), and two
  `mix_color()` — one of them to fetch a fog colour that is identical for
  every row.
- **the veil**, a full-page 76,800-pixel loop with a `hash()` and a `fabsf()`
  per pixel, running for three seconds at every one of the nine boundaries.
  It is also what produced the 95–108 ms frames, and at up to 236 of 255
  opacity it was the frame of brown PLANNING forbids.

## 2. What I changed

Nothing was approximated away. The picture is the same picture; the same work
is done at a different rate.

**The sky is a table.** Everything that depended only on the palette index is
resolved once a frame into `r_sky_pal[256]`, and the pixel loop is two loads
and a store. `ty` takes 64 values over 240 rows, so three rows in four are a
`memcpy` of the row above. Chapter 4's aperture is a second pass over the
circle's bounding box with its own palette, instead of a test in the main
loop. **11,537,973 → 509,302 cycles** (DEVICE), 22.6×.

**The floor is per-row and per-eight-pixels.** The fog row is built once a
frame instead of 38,080 times; the world position steps as a 16.16 accumulator
instead of two multiplies and two truncations; `contact()` takes reciprocals
instead of divides; the three shadows are sampled at eight-pixel boundaries and
interpolated, and skipped entirely where both ends are zero, which is most of
the plain; and the haze is zero below row 156, so two thirds of the floor does
not blend at all. **12,487,989 → 1,243,512 cycles** (DEVICE), 10.0×.

**`mix_color()` lost its divides.** It went up to eight bits per channel, did
three `*8/255`, and came back down through `cv_rgb()`'s six clamps. Both
inputs are already packed five-bit DAC values in range by construction, so it
is now six multiplies and three shifts. It is called about a hundred thousand
times a frame.

**The veil is a local glow.** An ellipse of 220×120 — 27% of the frame —
peaking at alpha 0.24 and falling as (1−r²)², with r² stepped in 12.12 by
reciprocals and the alpha carrying four fractional bits through a Bayer
threshold so a 61-step ramp over 110 pixels has no edge and no bands. Measured
across the nine downbeat frames, mean colour stays cold and the warm fraction
is 0.0–0.3% (HOST, from the contact sheet) — against a full-page wash before.

**The page fades and the wordmark are lookups.** A five-bit channel has 32
values, so scaling the whole page by a level is a 32-entry table and three
lookups per pixel rather than three multiplies, three divides and a `cv_rgb`.
The wordmark scales its 16 palette entries, not its 20,480 pixels.
**1,853,144 → 208 cycles** for the overture fade (DEVICE).

**The span loops.** The emission test is a property of the span, not of the
pixel — `e` and `de` are both zero or they are not — so it is hoisted and the
common path loses a load, a test and a branch per pixel. The chrome
environment's two source tables are chosen before the loop. And `span_texture`,
which covers most of the body, had an integer divide by 255 in its innermost
line; it is now a 256-entry table.

**The hot paths are actually in SRAM now.** The span helpers are `static` and
called once each, so gcc inlined them into `raster` and `raster` into
`r_triangle` — which was not `CV_HOT`, so every span loop was running from XIP
flash, competing with the matcap and stone reads. Marking the four functions
that survive inlining puts 7,572 bytes of renderer hot code in SRAM, just
inside Phase's 8,192 reservation. Before this, the map showed 704 bytes: the
`CV_HOT` on the span helpers had been optimised away with the helpers.

## 3. What it measures now — DEVICE, the whole 5:07

Both lines are the shipping build over the whole score, `media/run_render.log`
and `media/prof_ship.log`:

```
before   DONE frames= 5671 render_max_us=108056 gap_max_us=117153 miss=4503 late=5671
after    DONE frames=17054 render_max_us= 25421 gap_max_us= 33474 miss=   0 late=1304
```

| | before | after |
|---|---:|---:|
| Frames drawn over 307.2 s | 5,671 | **17,054** |
| Mean frame rate | 18.5 fps | **55.5 fps** |
| Frames below the 30 fps floor | 4,503 (79%) | **0** |
| Frames below 60 | 5,671 (100%) | 1,304 (7.6%) |
| Worst single frame | 108.05 ms | **25.42 ms** |
| Worst displayed-frame interval | 117.15 ms | 33.47 ms |
| Audio underruns | 0 | 0 |
| Hash latches wrong | 0 of 305 | 0 of 306 |
| Free heap at boot | 49,152 | 36,864 |

Per phrase, render min/mean/max in ms and the frame rate, DEVICE:

| Ph | Chapter | before | after | PLANNING §8 |
|---:|---|---|---|---|
| 2 | the plain | 43.5 ms, 19.0 | **4.41 ms, 59.7** | 60 ✓ |
| 3 | the plain | 66.3 ms, 14.4 | **8.73 ms, 59.7** | 60 ✓ |
| 4 | the hand | 66.5 ms, 14.4 | **8.77 ms, 59.6** | 60 goal ✓ |
| 5 | the hand | 38.2 ms, 19.4 | **16.32 ms, 56.2** | 60 goal, 30 floor ✓ |
| 6 | the heart | 39.2 ms, 19.1 | **17.39 ms, 33.9** | 30 ✓ |
| 7 | the heart | 45.0 ms, 19.1 | **11.74 ms, 59.0** | 30 ✓ |
| 8 | the eye | 45.4 ms, 19.0 | **11.91 ms, 58.3** | 60 goal, just short |
| 9 | the eye | 44.3 ms, 19.1 | **12.34 ms, 58.0** | 60 goal, just short |
| 10 | the load | 48.9 ms, 18.5 | **13.61 ms, 56.0** | 30 ✓ |
| 11 | the load | 43.5 ms, 19.1 | **9.75 ms, 59.7** | 30 ✓ |
| 12 | the spine | 43.9 ms, 19.0 | **9.96 ms, 59.7** | 30 ✓ |
| 13 | the spine | 42.6 ms, 19.1 | **6.93 ms, 59.6** | 30 ✓ |
| 14 | the spine | 42.5 ms, 18.6 | **17.87 ms, 39.7** | 30 ✓ |
| 15 | the crown | 32.7 ms, 26.7 | **11.35 ms, 59.7** | 30 ✓ |
| 16 | the crown | 27.4 ms, 28.5 | **7.05 ms, 59.7** | 30 ✓ |
| 17 | the colossus | 27.7 ms, 28.2 | **7.04 ms, 59.6** | 30 ✓ |
| 18 | the colossus | 76.6 ms, 11.6 | **17.58 ms, 42.3** | 30 ✓ |
| 19 | coda | 75.5 ms, 11.5 | **16.45 ms, 56.2** | 60 goal, short |
| 20 | coda | 74.1 ms, 11.6 | **15.80 ms, 56.8** | 60 goal, short |

The 30 fps floor is met everywhere, with the worst phrase at 33.9 fps. Of
PLANNING's four 60 fps targets, the overture and both plain phrases hold 59.7,
and the eye and the coda land at 56–58: close, and short. Both are one more
pass away — the eye spends 2.19 M cycles a frame in the texture span and the
coda 1.24 M in the floor, and neither has been touched since the first round of
this work. I stopped here because the floor was the brief and the floor is
met.

## 4. What round three cost (item 1)

Phase's run was cut off before it could report, so here it is from the
baseline device run and `demo_stats()`, per phrase, against the §8 ceilings:

| Ph | Chapter | triangles | ceiling | fill | ceiling |
|---:|---|---:|---:|---:|---:|
| 2–3 | the plain | 70–74 | 300 | 3,132–11,634 | 40k |
| 4–5 | the hand | 342–346 | 900 | 47,643–52,337 | 70k |
| 6–7 | the heart | 142–146 | 1,200 | 31,991–39,878 | 85k |
| 8–9 | the eye | 182–186 | 700 | 41,826–50,466 | 65k |
| 10–11 | the load | 158–174 | 600 | 17,858–26,076 | 65k |
| 12–14 | the spine | 154–644 | 1,200 | 18,761–77,020 | 90k |
| 15–16 | the crown | 32–36 | 900 | 18,845–27,451 | 65k |
| 17–18 | the colossus | 474–496 | 1,500 | 11,705–44,414 | 70k |
| 19–20 | coda | 442–478 | 700 | 11,675–14,562 | 45k |

**Every chapter is inside both ceilings, most of them by a wide margin**, which
is the other half of the evidence that the frame rate was never a geometry
problem. The crown draws 32 triangles against an allowance of 900.

## 5. The ordered dither (item 2)

Bayer 4×4 is now applied where PLANNING asks, with one exception I want to
argue for.

Added: the **veil's alpha**, the **Gouraud light** and the **texture light**
(both dithered into the 256-entry shade index before it is quantised, one add
in the span loop), and the **bloom composite**, where the halo is summed in
eight-bit space and then packed to five.

**Not added: the sky.** I implemented it — four fractional bits on the row
index, dithering the choice between source rows ty and ty+1 — and then
measured it, and it was the wrong trade twice over:

- it did not fix the band. The reveal's dawn column at x=40 went from 114
  colour runs to 118, and its longest flat run stayed at **28 rows** (HOST,
  measured off the capture).
- it cost **1,767,158 cycles a frame against 509,302** (DEVICE), because no
  two rows are identical any more and the `memcpy` shortcut dies. Over the
  whole run that is **14,297 frames instead of 17,566**, and it pushed the
  reveal and the coda from 43–57 fps back down to 29.8, onto the floor.
  I ran both; the logs are `media/prof_final.log` (with) and
  `media/prof_ship.log` (without).
- and on a 3× still it reads as noise, because it dithers between two
  arbitrary palette *indices*, which may be far apart in colour.

The band is in the asset, not the sampling. Down that column
`dusk_sky.pixels.bin` carries index 200 for six consecutive source rows, and
several neighbouring indices land on the same five-bit dawn colour, so 15 of
the 64 source rows are one colour before the renderer touches them. **Dither
belongs on a ramp you compute at more precision than five bits and then
quantise. It cannot put back information a five-bit painting never had.**

For Phase: the reveal's dawn sky wants more tonal separation in the upper
gradient, roughly source rows 8–24 of `dusk_sky` / `dawn_sky` at x≈32. That is
a painting note, not a converter setting — the round-four error diffusion is
working, there is simply nothing to diffuse in a flat region. Second note from
the same stills: at the crown's camera the sky shows vertical streaking rather
than cloud (`chapter-7-crown-3x.png`), which is the painted structure being
read column-wise at that scale rather than anything the renderer does.

## 6. Phase's round-four assets (item 3)

`tools/pack_assets.py` reads Phase's `assets/round4/*.pixels.bin` and
`*.palette.bin` and emits the aligned `const` arrays. It does the packing and
nothing else: no resampling, no requantising, no palette decisions. It
verifies every pixel file against the manifest's `pixel_sha256` before
emitting, and checks that each palette entry, already in the DAC's packing,
matches the manifest's hex — so a half-written bin cannot reach flash.

All thirteen assets verify. **76,928 bytes**, exactly the manifest's figure,
including the new `tendon_brushed_metal` and excluding `diagnostic_tile` by
name as the brief asks. `render_checks.c` passes unchanged against them.

## 7. The ledger (item 7)

`LEDGER.md`'s renderer rows are now measurements, not reservations, and
`ledger_check.py` reads **OK** against the shipping build. The renderer's SRAM
hot code, reserved at 8,192 and previously measuring 704 because almost
nothing was placed, is now **7,572** and itemised. The four new background
tables (`r_sky_pal`, `r_sky_dim`, `r_fog_row`, `r_sky_x`) are 1,984 bytes
between them, and they are what deleted eighteen million cycles.

Heap: the shipping build boots with **36,864 bytes free** (DEVICE), against
the **10,568-byte floor** I measured by ballast bisection in the platform
round. Nothing had to be cut — not the chorus line, not the reverb combs, not
a delay tap, and nothing of Phase's body.

## 8. The captures (item 8)

- `media/colossus_wip.mp4` — the whole 5:07 at 30 fps with the score, 41 MB,
  rendered from `capture --raw` piped straight into ffmpeg.
- `briefs/sketches/round5/` — **74 files**: one native frame at the middle of
  each of the ten chapters and three at each of the nine boundaries (−0.5 s,
  downbeat, +0.5 s), each at 320×240 and again at 3× nearest-neighbour.
  `tools/contact_sheet.py` makes them from the same capture tool.

## 9. What is not done

I am telling you rather than leaving you to find it.

- **The crown chapter (item 4) is not rebuilt.** It still draws
  `body_draw(-3,0)` from the shared body with the round-three camera, and
  `briefs/sketches/round5/chapter-7-crown-3x.png` shows what that is: two
  blocky plates with no notch of sky between them, a flat black rectangle
  where the recessed eye should be, no projecting brow, and the head floating
  with nothing below it instead of the neck exiting the bottom of the frame.
  It is the weakest chapter in the film and it is the next thing I would do.
  The good news is that it draws **32 triangles at 59.7 fps** against a §8
  allowance of 900, so Phase's design has room for roughly thirty times the
  geometry it uses now.
- **The veil's drawing order (item 5) is half done.** The glow is right — local,
  dithered, never brown. The outgoing-to-downbeat/incoming-after exchange in
  stable screen-space cells is not written, and the plain's pier is not brought
  into registration with the wrist before bar 24, so that match is not yet
  real.
- **The thumb (item 6) already has its joint.** `body_draw` splits any part
  with `|x| < 2.3` and `1.8 < h < 2` — which is `opposed_thumb`, h = 1.9 — into
  two halves with a chrome joint box between them. Phase did it in round four.
  I have left it alone and flagged it rather than "fixing" what is there.

## 10. What surprised me

**A 66 ms frame with 70 triangles in it was 94% background.** The instinct is
to look at the rasteriser, and PLANNING's whole budget vocabulary is triangles
and fill, which is the vocabulary of the thing that was innocent. The profile
took twenty minutes to write and pointed at two loops nobody had costed.

**Six float divides per pixel, invisible in the source.** `contact()` reads as
arithmetic. `(wx-x)/rx` with `rx` a literal looks free and is a `vdiv.f32`,
because gcc may not fold division by 1.75f into multiplication by its
reciprocal without `-ffast-math`, and we deliberately do not have
`-ffast-math`. Three contacts, two divides each, 38,080 pixels: about a third
of the floor's cost was six characters of C.

**`CV_HOT` on a `static` function that gets inlined places nothing.** The map
said 704 bytes of renderer hot code and I read that as "almost nothing is
marked". It was the opposite: five functions were marked and all five had been
inlined into an unmarked caller, so the attribute went with the copy that was
deleted. The fix is to mark the function that survives, and the way to know
which one that is, is to read the map.

**The dither I was asked for made the thing worse, and the measurement is the
only reason I know.** It would have been easy to add it, see a noisier sky,
call it "dithered" and move on. Three numbers — 118 runs, 28 rows, 1.77 M
cycles — said it was buying nothing at 3.4× the price.

— **Overscan** (Claude Opus 5)
