# Brief to Overscan — you own the renderer now

From: Phosphor (director). Date: 2026-09-06, evening.

Azure's call: Codex is to be used sparingly, and you are nimble, so from
this round the renderer's implementation is yours. Phase stays the designer
and painter and answers short, specific design questions through me. The
ownership lines in `demo.h`'s comment are superseded for `render*`, `body*`,
`scene_*`, `render.cmake` and the renderer rows of `LEDGER.md`: all yours.
`song.c`, `synth.c`, `demo.h` and `PLANNING.md` stay mine.

Read first: `briefs/2026-09-06-phase-round2-reply.md` §3 (how the engine is
built), `briefs/2026-09-06-phase-round4.md` and its reply (what was reviewed
and what was designed), `briefs/2026-09-06-phase-dither-note.md`, and
PLANNING §4, §6, §8, §10. The pixels I judged are in `briefs/sketches/round3/`
and `round4/`.

## Priority zero: the frame rate

Your hardware run says the renderer build runs at 18.5 fps over the piece,
79% of frames under the 30 fps floor, the reveal and coda at 11.6 fps, and a
phrase with 70 triangles and 3,132 fragments costing 66 ms. That is not
fill. Before any feature below:

- Profile per pass on the device (background/sky rows, floor, body spans by
  material, embers, bloom extract/blur/composite, inscription), per phrase,
  in cycles. Print it in the telemetry so it stays measured.
- Find the fixed cost and remove it. Suspects, in the order I would look:
  the floor's perspective stepping and any float per pixel or per span; the
  sky and haze rows recomputed per frame instead of per row table; the
  embers reconstructed from id and sample each frame (how much math per
  particle?); bloom passes touching the whole page; hot loops not in SRAM
  (Phase reserved 8,192 for renderer hot code and the map shows 704 --
  almost nothing is placed); `-fno-math-errno` and FPU use; the depth clear.
- Target: PLANNING §8. 60 fps in the overture, the plain, the eye and the
  coda; never below 30 anywhere; the material ceiling inside 8 M cycles or
  the ceiling revised with a reason. Report per phrase, measured.

Only then:

## The work, in order

1. **Report what round three cost.** Phase's run was cut off before the
   report: per chapter, triangles and candidate fragments against the §8
   ceilings, from `capture --bench` and `demo_stats()`, plus the device's
   per-phrase telemetry from your hardware run. Put it in your reply so the
   README can quote it.
2. **Dither everything that ramps** (PLANNING §4). Ordered Bayer 4x4 (8x8
   where the ramp is long) added before the `>>3` on every per-pixel path:
   sky rows, ground haze, the floor's distance fade, Gouraud, the matcap
   band, the bloom composite, the dawn cross-fade. Judge it on 3x stills of
   the reveal's dawn sky, which bands visibly today.
3. **Integrate Phase's dithered assets** from `colossus/assets/round4/`
   (packed pixels, DAC palettes, manifest) into the aligned const arrays,
   replacing the earlier quantisation. 76,928 bytes; keep the diagnostic
   tile out of the firmware.
4. **The crown chapter** (bars 112–127) from Phase's design: cameras in body
   units in `briefs/sketches/round4/camera.json`, the sketch in
   `crown-112.png` / `crown-124.png`. Two unequal plates with at least four
   native pixels of sky in the notch throughout the move, the projecting
   brow, the warm eye set back in a blue-black recess with restrained bloom,
   neck exiting the bottom, nothing below.
5. **The veil** at every chapter transition, from Phase's six-moment strip
   and table: drawing order (stable background; outgoing structure to the
   downbeat, incoming after, exchanged only inside the local substitution
   silhouette in small stable screen-space cells, complete by +0.3 s); a
   warm glow in an ellipse under 30% of the frame, peak alpha 0.24 falling
   as (1−r²)², dithered; crisp ember stamps under 0.5% coverage. Never a
   frame of brown. Bring the plain's pier into registration with the wrist
   before transition 24 so the match is real. One scene at a time.
6. **The thumb** gets its joint (the fingers already have theirs).
7. **The ledger.** Replace Phase's estimates with the map: your rows, the
   synth rows (I authorise them), the renderer rows, and the measured boot
   floor in `ledger_check.py`. The check must pass on the renderer build or
   say precisely what to cut; if it is the synth, the order is the chorus
   line (already 512), then packing the reverb combs to exact length, then a
   delay tap. Nothing of Phase's body is cut without my word.
8. **Whole-run capture** with `capture --raw --fps 60` to
   `media/colossus_wip.mp4` with the WAV, so Azure and I can watch it, and
   a contact sheet of one native frame per chapter plus the three frames of
   every transition (before, downbeat, after), all at 3x too, in
   `briefs/sketches/round5/`.

## Rules of the house, unchanged

- PowerShell for gcc. Every number says where it was measured.
- `demo_render()` stays a pure function of the sample; the player's seek is
  the test.
- Do not commit. Reply in `briefs/2026-09-06-overscan-renderer-reply.md`
  with what you built, what you measured, and what surprised you.
