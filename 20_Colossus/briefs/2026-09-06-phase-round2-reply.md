# Phase → Phosphor · round two · 2026-09-06

Phase (GPT-6 Astra), LATENT. Working to PLANNING revision 2 and 125 BPM.

## 1. Silhouette ready for review

Review [native contact sheet](sketches/silhouette-contact.png) and [nearest 3×](sketches/silhouette-contact-3x.png). Separate native/3× plates are `sketches/silhouette-{whole,hand,eye,crown}[-3x].png`. These come from the same dimension table in `colossus/assets/body_layout.json`, generated with `assets/silhouette.py`; that script also emits the C body layout. The body stands 174 pixels high in its native plate.

The silhouette reads as a planted load-bearing machine: the short separated legs, long open hands, low left shoulder and unequal split crown survive without surface lighting. The three finger gaps are the fragile feature: at reveal size they need at least one uninterrupted pixel column, while the opposed thumb needs a wider opening. I retained those spaces explicitly rather than thinning the whole hand. The absent right chest cross-rib makes the central opening asymmetric without adding another outline vocabulary. The eye is an aperture, not a face. This passes my construction gate for surface work; director approval of the pixels remains open. The block proportions are deliberately severe; depth and a small forward hood offset will supply the stoop.

Further implementation and measured host checks follow below when complete.

## Silhouette second pass — director notes received during this turn

I found `2026-09-06-phase-silhouette-notes.md` after the first engine/asset
pass had already run. I paused that review and revised the shared construction
before continuing. The linked silhouette plates now show this second pass.
`assets/body_parameters.json` makes the low shoulder **2.6 world units / 22
native pixels** lower, narrows the torso to **2.9 units** while deepening it to
**3.8**, and turns/advances the near foot by **−24° / −0.85 units**. The near
hand is now roughly **36 native pixels high**, with broad fingers, two open
inter-finger channels and a separate thumb opening. Its fingers reach well
below the pelvis; its paired forearm slabs are the heaviest limb masses.
The eye detail has a dark interior and pale negative rim under an overhang.
The same shoulder-drop parameter drives the reveal lift; it is not a hand edit
of the PNG. This is the silhouette submitted for review, superseding my first
construction judgment. Director acceptance is still pending.

## 2. Ledger and integration finding

[LEDGER.md](../colossus/LEDGER.md) was written before renderer allocation and
updated against the current synth object. It names the two pages, depth, both
glow fields, shade tables, component cache, joints, all synth buffers, stacks,
hot code, platform/audio reserves and scanvideo's invisible heap allocation.
Renderer: no malloc, no decompressed image surface, no particle array.

**The provisional device budget does not pass.** Reservations total 470,460 B
before scanvideo, leaving **53,828 B free**, versus the conservative **80,896 B
(79 KiB)** boot floor. After the estimated 21,504 B scanvideo allocation, the
remaining allowance is 32,324 B. The shortfall against that boot floor is
**27,068 B**. These include explicit hot-code reservations for both renderer
and synth. They are planning numbers, not board measurements. The synth grew
concurrently during the turn (larger reverb plus chorus); its current mutable
symbols total 41,056 B before alignment. I did not edit it.

Overscan: please replace audio/SDK/hot-code reserves with the ARM map, measure
this project's boot floor and enforce it. Phosphor: if that does not recover
the margin, the page/depth/synth allocation decision needs your direction.
I have not traded away finger gaps or silently reduced the depth surface.

## 3. Engine skeleton and hand pipeline

The source list is `colossus/render.cmake`, variable
`COLOSSUS_RENDER_SOURCES`. Include it in the platform build and link `m`.
`demo.h` is untouched. `demo_init`, `demo_render` and `demo_stats` implement
the existing contract. `render_host.c` and `render_checks.c` are separate
host-only executables and are deliberately absent from that source list.

Implemented:

- Shared body dimensions, parent shoulder-to-wrist displacement, periodic hand
  tension, and the bar-128 shoulder lift, reconstructed from absolute sample.
  The low shoulder reaches the high shoulder over two bars. No accumulated pose.
- A 24-vertex reusable transformed-component cache. Close LOD subdivides the
  selected component faces into four quads; body LOD keeps complete box faces;
  whole LOD culls away-facing faces. Eye ring uses 24/12/8 segments. Recognition
  parts remain in every LOD. Cache capacity stays constant.
- VESPER-derived near clipping and spans, extended to far clipping and all
  interpolated attributes. Separate flat, Gouraud, painted matcap and indexed
  texture span loops; fixed-point inner-loop steps; 8-bit reciprocal depth
  mapped to 1–255 with zero reserved for the background. Per-chapter near/far
  windows are explicit. Gouraud lighting stays in world space; matcap normals
  move with the camera.
- Dedicated level-floor path, no floor depth usage. Eight-pixel correction
  intervals are exact along a row of this level plane (constant row depth).
  This does not approve an interval for a tilted or displaced production floor.
  Bronze currently uses the conspicuous diagnostic tile; final wear art waits.
- Two-field separable bloom: intended sources only, bounded low-resolution
  blur and occupied 4×4 composite rectangles. Bright chrome does not seed it.
  Pass contract is opaque occluders, emissive sources, embers, bloom; this is
  documented in `render.h`. It is not a general arbitrary-order emissive pass.
- Stateless particle lifetimes from id, epoch and sample, 64–127 in the normal
  skeleton and 256 in the test. Depth-tested 1/3-pixel marks with softer edges.
- Hand-drawn 128×64, MSB-first, 1-bit atlas; 8×12 cells with 10-pixel stems
  and foot serifs. The period glyph is a centered dot, so `I . HAND` renders
  the intended unboxed `I · HAND`. The inscription holds through bars 24–27.

Review [hand native](sketches/hand-engine.png), [hand 3×](sketches/hand-engine-3x.png),
[7.68-second chrome motion](sketches/hand-chrome-motion.gif) and
[exact-color keyframes](sketches/hand-chrome-keyframes.png). The GIF is a motion
convenience with GIF palette limits; judge DAC colors in the PNGs. Also supplied:
[eye chrome](sketches/eye-chrome.png) and [whole-body framing scaffold](sketches/body-reveal-scaffold.png).
The latter is bar 136, after the shoulder has risen; the silhouette plate is
before the lift. It is not evidence of equal starting shoulders.

The hand is rendered through the complete pipeline for its chapter. Other
chapters are explicitly framing scaffolds, not finished shots: no final
heart/load/spine action, matched transitions, camera-through-eye move or credit
sequence is claimed. Chrome on the hand and eye is submitted for judgment,
not declared artistically or device-performance approved. An LOD switch under
occlusion is a later shot-design task; this round chooses LOD per view.

## 4. Worst-case material test and host checks

Call `render_material_test(page, sample)` from `render.h` instead of
`demo_render` for the device benchmark. It has no sticky mode and preserves
normal-render seek purity. Its synthetic overdraw rig submits five layers,
each 15×10 quads across a 300×60 region. Far-to-near ordering makes every
layer shade. All four paths, painted chrome, indexed texture, intended bloom
and 256 embers are on. The triangles/fill figures exclude background and
postprocessing exactly as the existing `demo_stats_t` definitions require;
those passes still run and must be included in device render-time telemetry.

Measured in the WSL host executable at sample **1,198,080** (bar 26):

| Frame | Submitted triangles | Candidate fragments | Embers |
|---|---:|---:|---:|
| Revised hand, close LOD | 816 | 37,281 | 102 |
| Material ceiling | 1,500 | 90,000 | 256 |

[Material ceiling native](sketches/material-ceiling.png) ·
[3×](sketches/material-ceiling-3x.png). Sixteen sample positions assert the
exact ceiling counts. This is a deliberately count-controlled benchmark,
not a claim that the unfinished production shots have this worst-case cost.
No device cycles, frame rate, free heap or underrun measurements are claimed.

WSL GCC builds cleanly with `-DHOST_BUILD=1 -std=c11 -O2 -Wall -Wextra -Werror`.
AddressSanitizer + UndefinedBehaviorSanitizer checks pass: **483 seek comparisons**
(bar starts, midpoints, ends and out-of-run clamping), page guards, separated
surface depth order, wholly near/far-clipped primitives, crossing primitives
in every material, independently specified cut-edge UV/light values compared
pixel-for-pixel, no bloom from non-emissive geometry, hidden emissive sources
submitted after occluders, and exact material-ceiling counts. LeakSanitizer is
disabled because this environment's tracing setup makes it fail; ASan/UBSan
remain enabled. Firmware has no renderer heap allocations.

Reproduce from this folder:

```sh
python3 colossus/assets/review.py
colossus/assets/phase_capture 1198080 briefs/sketches/hand-engine.ppm
colossus/assets/phase_capture 1198080 briefs/sketches/material-ceiling.ppm stress

gcc -DHOST_BUILD=1 -std=c11 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  colossus/render_checks.c colossus/render.c colossus/body.c \
  colossus/scene_hand.c colossus/scene_material_test.c \
  colossus/assets/engine_assets.c colossus/assets/painted_assets.c \
  colossus/song.c -lm -o colossus/assets/phase_checks
ASAN_OPTIONS=detect_leaks=0 colossus/assets/phase_checks
```

The temporary PPM dumper writes the actual DAC channels back to RGB. Overscan
can replace it with the owned `tools/capture.c`; no SDL dependency exists here.

## 5. First assets and converter

The built-in image tool painted the drawn wordmark, dusk matcap and dusk sky.
Masters, exact prompts and the transformation provenance are retained in
`colossus/assets/source/` and `colossus/assets/prompts.md`.

- [Drawn letters](../colossus/assets/source/wordmark-drawn.png) →
  [320×64 / 4-bit painted wordmark](../colossus/assets/wordmark-quantized.png).
  The exact drawn mask restores counters, margins and missing-rib incisions.
- [64×64 / 8-bit dusk matcap](../colossus/assets/dusk_matcap-quantized.png).
- [256×64 / 8-bit dusk sky](../colossus/assets/dusk_sky-quantized.png).
- 128×64 / 1-bit inscription atlas and 64×64 / 8-bit diagnostic intensity tile.

`tools/convert_assets.py` records dimensions, fixed palettes, source SHA-256,
resampling/crop, transparency, packing order and sizes in `assets/manifest.json`.
It emits aligned const C arrays, packs high nibble first for 4-bit data,
round-trips indices and DAC colors, and reproduces its outputs byte-for-byte
with `--check` (passed). No random quantization or runtime decompression.
Five assets occupy **36,896 bytes** in flash including painted palettes;
the three painted assets alone occupy 31,776 bytes. Source masters are not
firmware payload. The wordmark and both dusk assets are consumed by the renderer.

All requested round-two deliverables are present. The remaining gates are
director acceptance of this revised silhouette/chrome and Overscan's measured
memory/performance verdict. I wrote only Phase-owned sources/assets, the
explicitly assigned ledger and review/report files; PLANNING.md, demo.h,
song.c, synth.c and platform sources were not edited by me.
