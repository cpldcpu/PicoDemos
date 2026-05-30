# 13_Singularity — "SINGULARITY"

> An original RP2350 demoscene production. **LLM author: Claude Opus 4.8.**
> A relativistic journey into a black hole. Deliberately unlike the prior originals —
> not bio-organic (10 SLOP), not neon-cyber (11 VOLTAGE), but **cosmic, relativistic, immense.**

This file is the full implementation plan: concept, engine reuse, scene-by-scene technical
design, memory budget, the gravitational-lensing pipeline, the asset-generation prompts, and the
Suno 4.5 music prompt. It targets the **same hardware** as the other Pico demos (RP2350 +
Pimoroni VGA Demo Base) and **reuses the proven engine** in `../10_TheDemo/thedemo/` verbatim.

---

## ✅ Implementation status — BUILT & VERIFIED

All eight scenes are implemented, the gravitational-lensing LUT is baked, the soundtrack is
encoded, and the project builds for **both** the desktop preview and the real RP2350:

- **Host preview** (`host/singularity.exe`, MSYS2 UCRT64 + SDL2) — builds clean; all 8 scenes
  captured to `host/screenshots/*.png` and reviewed. The nebula visibly coalesces into a proto-star
  core, the relativistic starfield shows aberration/Doppler colour, the Mode-7 disk shows the
  Doppler hot-spot, and the **lensing scene renders the Einstein ring + shadow** (see
  `tools/lens_preview.png` for the offline sanity render).
- **Device firmware** (`build_rp2350/singularity.uf2`) — builds clean with the Pico SDK 2.x +
  pico-extras, arm-none-eabi-gcc 13. Measured footprint:
  - **Flash:** 3.45 MB / 4 MB (music.qoa 2.95 MB + packed assets 0.44 MB + lens LUT 153 KB + code).
  - **SRAM (BSS):** 402 KB / 520 KB — comfortable headroom for stacks + scanvideo pools.
- **Soundtrack:** `assets/Graviton Choir.mp3` (5:30) → `music.qoa` (mono 22050, 2.95 MB).

### Resolution — full 320×240 truecolor (MODE_HIRES)

The six cosmic per-pixel scenes render at **full native 320×240 RGB565 truecolor** via a new
`MODE_HIRES` added to the engine (`vga.c`), not the half-res 160×120 the sister demos use. The
title and rebirth scenes stay **320×240 8bpp palette** (`MODE_320`) — palettized is ideal for their
single-backdrop-plus-text look and saves flash. A **shared framebuffer arena** lets the 320×240
RGB565 double-buffer (307 KB) and the palette buffers alias the same bytes (only one mode is live at
a time; switches land on a black frame), so truecolor-at-full-res costs no extra SRAM beyond the
largest buffer pair.

**CPU feasibility (the reason 160 was the sister-demos' default):** at 320×240 every per-pixel scene
is 4× the work. The Cortex-M33 has a hardware single-precision FPU (`VSQRT`/`VDIV` ≈ 14 cy), so the
sqrt/divide-bearing scenes (star, disk) stay within the ~4.2 M-cycle/frame budget (~10–13 ms). The
one genuine blocker was the nebula's per-pixel `expf` (software, ~200 cy) — replaced with a
hardware-divide falloff. Lensing/spaghetti/starfield are cheap (no per-pixel transcendentals). The
two heaviest (disk, star) have the least margin; the engine drops to 30 fps gracefully (no tearing)
if a frame ever runs long. Real-silicon fps wasn't measured (no hardware in the loop) — the estimate
is by op-count, calibrated against the transcendental-free lensing scene.

Source layout: **all code lives in [`singularity/`](singularity/)** (the demo-folder root keeps only
`README.md`, this `IMPLEMENTATION.md`, `media/`, and the release `.uf2`). Engine reused verbatim
(`scene.c/.h`, `vga.c/.h`, `audio_qoa.c`, `main.c`, `rgb565.h`, `font8x8.h`, `qoa.h`); new work in
`singularity/effects/*.c` (+ `fx_common.h`), `scene_scratch.h`, `timeline.c`,
`tools/make_lens_lut.py`, and the `host/` SDL harness. Build/asset commands below run from inside
`singularity/`.

---

## 1. Logline & aesthetic

A single unbroken fall. Cosmic dust gathers and **ignites a star**; the star burns and dies; we
**plunge through its accretion disk**, past the **Einstein ring**, across the **event horizon**,
are **stretched into the singularity** — and emerge, reborn, into a new universe.

- **Palette:** deep cosmic indigo/violet voids · accretion **gold → white-hot** · relativistic
  **blue (approaching) → red (receding)** · one pure-white singularity flash.
- **Feel:** Interstellar-scale awe; slow gravitational build to an overwhelming climax, then calm.
- **Why it wins:** the gravitational-lensing Einstein-ring shot is a real, physics-accurate "money
  shot" no demo here has attempted, and the demo debuts a **genuine RP2350 hardware first** in this
  codebase: the **SIO hardware interpolators** (used for the Mode-7 accretion disk).

---

## 2. Engine reuse — no engine redesign

We build on `../10_TheDemo/thedemo/`. The engine, display backends, audio, and asset pipeline are
proven; we only add new `effects/*.c`, timeline rows, scratch members, and baked LUTs.

- **Effect interface** (`scene.h`):
  ```c
  typedef struct effect {
      const char   *name;
      screen_mode_t mode;     /* MODE_320 | MODE_160 | MODE_SPLIT_160_OVER_320 (+640 beam-raced) */
      void (*init)(void);
      void (*frame)(uint32_t t_ms_into_scene, uint32_t t_ms_global);
      void (*done)(void);
  } effect_t;
  ```
  The runner (`scene.c`) reads the audio playhead, finds the active `timeline[]` row, and on a
  boundary calls `done()` → `vga_set_mode()` → `init()`, then `frame()` each vblank. Mode switches
  happen only during the brief fade-to-black between scenes (see `CLAUDE.md` watch-outs).
- **Display modes / surfaces** (`vga.h`):
  - `MODE_320`: 320×240 chunky **8bpp** + 256-entry RGB555 palette (`vga_320_back_buffer()`,
    `vga_320_palette_set()`, `vga_320_present()`).
  - `MODE_160`: 160×120 **RGB565** truecolor (`vga_160_back_buffer()`, `vga_160_present()`).
    **RGB565 here is a PIO-native bit order — always build pixels with `rgb565_pack()` from
    `rgb565.h`; never hand-pack.**
  - `MODE_SPLIT_160_OVER_320`: RGB565 upper rows over 8bpp lower rows (copper-style raster split).
- **Registering an effect:** add `effects/<name>.c` defining `const effect_t fx_<name>_real = {…}`;
  add the `.c` to `add_executable(thedemo …)` in `CMakeLists.txt`; add `extern const effect_t
  fx_<name>_real;` + a `timeline[]` row in `timeline.c`; add the object to `host/Makefile` for the
  SDL desktop preview.
- **Baked binary LUTs** (lensing remap, etc.) follow the existing `music_qoa.S` /
  `assets/_packed/assets.S` **incbin** pattern (`.section .rodata` · `.balign 4` · `.global` ·
  `.incbin`), live in **flash**, and are wired with `OBJECT_DEPENDS` in CMake.
- **Closest reference effects to study/reuse:**
  - `effects/spheres.c` — equirectangular **panorama sampling** and the `yaw_offset_q4` runtime
    rotate trick. This is the direct analog for both the lensing background and the Doppler shading.
  - `effects/tunnel_particles.c` — per-pixel **LUT sampling**, particle splat, and the half-res +
    line-double budgeting trick.
  - `title.c` — copper-bar intro + `font8x8.h` text; reuse for the title and endcard scenes.

---

## 3. Budget facts (hard constraints)

- **CPU:** overclock to **300 MHz** → ~5.0 M cycles/frame; ~4.2 M usable after audio + scanout.
  Keep `frame()` under **~14 ms** (QOA decode runs on core0 too). 60 fps target; the engine drops
  gracefully to 30 fps on overrun — so when a scene is heavy, **drop resolution before frame rate**
  (half-vertical-res + line-double, the tunnel trick).
- **SRAM:** 520 KB; BSS ≈ **449 KB**; **~70 KB headroom**. Big per-scene buffers go **only** in the
  `scene_scratch` union (current largest member 76 800 B). Keep every new member **≤ 76 800 B** and
  the union size is unchanged → **zero added BSS**. Never add a static `uint8_t buf[W*H]` per scene.
- **Flash:** ~3 MB free. All read-only **LUTs, panoramas, and textures live in flash**, not SRAM.

---

## 4. Scene timeline & per-scene technical design

**Soundtrack (final, analyzed):** `assets/Graviton Choir.mp3` — **5:30 (330.3 s), 78.3 BPM, 4/4**
(bar = 3.07 s; ≈ the 80 BPM target). Source is stereo 48 kHz → downmix to **mono 22050 Hz** for QOA
(see §7). Boundaries below are **snapped to the `analyze_music.py` beat/onset/segment analysis** of
this exact track. Two anchor hits drive the cut points: the **biggest early onset at 1:14 (74.05 s,
strength 14.4) = the star-ignition flash**, and the **final cadence at 5:25 (325.8 s, strength 13.9)
= the endcard slam.** Structural boundaries land at 0:50, 1:51 and a long energetic body to 4:28; the
loud finale (4:28–5:08) carries the rebirth burst. QOA @ 22050 mono ≈ 5.5 KB/s → **~1.8 MB flash**
for the full track (well within the 4 MB part).

| # | Time | Scene | Mode | Core technique | Est. cost |
|---|------|-------|------|----------------|-----------|
| 0 | 0:00–0:18 | **Title** | 320 | Reuse `title.c` copper-bars + `font8x8.h`; "SINGULARITY" over the star panorama, slow fade-in | trivial |
| 1 | 0:18–0:50 | **Curl-noise nebula** | hires | ~6000 particles advected by an analytic curl-noise flow field + inward pull; soft additive 3×3 splats over a growing warm proto-star core; dust coalesces (ends on the 0:50 structural boundary) | ~3 ms |
| 2 | 0:50–1:14 | **Star ignition** | hires | 3 seamless granulation tiles (128²) affine-sampled (scroll + zoom) summed through a blackbody RGB565 ramp; r²-based limb darkening; zoom into the surface, **ignition flare on the 1:14 hit** | ~10 ms |
| 3 | 1:14–1:51 | **Relativistic starfield warp** | hires | 3D starfield with **aberration** `cosθ'=(cosθ−β)/(1−β·cosθ)` (stars bunch toward travel) + **Doppler** `D=1/(γ(1−β·cosθ))`, brightness ∝ D³, blue→red colour; β ramps 0→0.985 | ~2 ms |
| 4 | 1:51–2:43 | **Doppler-beamed accretion disk** | hires | **Mode-7 affine** sample of the top-down (Cartesian) disk texture as a tilted plane; per-texel Doppler `×D³` + colour shift → approaching side bright/blue, receding dim/red; black hole + photon ring. **Showcases the SIO interpolators** for address gen | ~13 ms |
| 5 | 2:43–3:50 | **GRAVITATIONAL LENSING (climax)** | hires | Per-pixel precomputed **remap LUT** → equirectangular panorama; the **Einstein ring** + black **event-horizon shadow** emerge from offline geodesic integration; runtime yaw add `(u+yaw)&(PW−1)` rotates the background; ends on the 3:50 plunge onset | ~3.8 ms |
| 6 | 3:50–4:37 | **Event-horizon crossing / spaghettification** | hires | Time-varying radial/tidal **warp** of the lensed view (re-sampled through the lens LUT at distorted coords); vertical stretch + chromatic smear intensifying through the loud onsets, ending on a pure-white singularity flash | ~10 ms |
| 7 | 4:37–5:30 | **Rebirth (white-hole burst) + endcard** | 320 | White→color palette fade into the new-universe endcard over the loud finale (triumphant, not calm); scrolling credits + greetz via `font8x8.h`; **final 5:25 hit = endcard slam**, then a short tail to 5:30 | trivial |

### 4.5 Gravitational lensing — the make-or-break (validated, ~1 ms/frame)

The expensive physics is done **offline**; the device only does a table lookup + one texture tap
per pixel. (Now baked at full **320×240 = 76 800** pixels for MODE_HIRES — LUT = 153.6 KB flash,
runtime ≈ **3.8 ms/frame**, still far from compute-bound.)

- **Offline (host Python, `tools/make_lens_lut.py`):** for each of the 320×240 = **76 800** screen
  pixels, trace a photon **backward** from the camera. Integrate the equatorial null geodesic
  `d²u/dφ² + u = 3·M·u²` (with `u = 1/r`, `G=c=1`, `M=r_s/2`) by RK4 until either:
  - `r < r_s` → photon captured → write the **shadow sentinel** (black), or
  - `r → ∞` → photon escapes → record the asymptotic outgoing direction → map to panorama `(u,v)`.

  The photon-sphere critical impact parameter `b = 3√3·M` makes the **shadow radius and Einstein-ring
  location emerge automatically** — nothing is hand-placed. Output `lens_lut.bin` + the generator,
  both checked in, so a fresh clone builds **without** running Python.
- **Device (per pixel):** 1 LUT read · sentinel test · yaw add · 1 panorama tap · store ≈ 12–18 cy.
  19 200 × ~15 cy ≈ **288 K cy ≈ 1.0 ms at 300 MHz.** Background rotation is the free yaw add; slow
  zoom is a couple of muls toward center. Lensing is **not** compute-bound.
- **Sizing options:**
  - **Baseline:** 256×128 panorama → pack `(u,v)` in one `uint16` → LUT = 19 200×2 = **38.4 KB
    flash**; panorama 256×128 RGB565 = **64 KB flash**. ~1.0 ms.
  - **Quality:** 512×256 panorama → `uint32` LUT (76.8 KB) + bilinear panorama tap (~2.6 ms) — still
    comfortably 60 fps.
  - **"Falling-in" depth:** bake 4–6 LUTs at decreasing `r_cam` (~38 KB each) and cross-fade for a
    sense of plunging closer.
- **Offline self-check:** render `lens_lut.bin` against the panorama inside the Python generator and
  eyeball the ring before baking.

### 4.6 SIO hardware interpolators — the technical novelty

Confirmed: **no prior effect in this codebase uses INTERP0/INTERP1.** Using them is a genuine first.

- **Scene 4 (accretion disk, Mode-7):** textbook fit and the recommended showcase. INTERP0 lane0 =
  `u` accumulator, lane1 = `v` accumulator; reading the combined lane returns
  `base + (v<<log2W) + u` directly, collapsing the inner-loop address math from ~5–6 ALU ops to ~2
  register reads (~20→~10 cy/px) and freeing the CPU for the per-texel Doppler color math.
- **Scene 6 (spaghettification):** interpolator **blend mode** does the coarse-grid bilinear upscale
  cheaply.
- API: `#include "hardware/interp.h"`; `interp_default_config()`, `interp_config_set_shift/_mask/
  _add_raw()`, `interp_set_config(interp0, lane, &c)`; write `interp0->accum[]/base[]`, read
  `peek[]/pop[]`. **Interp state is per-core and not saved across IRQs.** The audio timer ISR runs
  on core0 but never touches interp, so `frame()` owns interp0/1 uncontended — **document this so
  nobody adds interp use in an ISR.**

---

## 5. `scene_scratch` union additions

All members **overlap** (one scene active at a time). Keep the largest member ≤ the current
76 800 B so the union size — and therefore BSS — is **unchanged**. Read-only LUTs/panoramas/textures
go in **flash**, never here.

```c
union scene_scratch_u {
    uint8_t bg_cache[VGA_320_W * VGA_320_H];                 /* 76 800 — existing, keeps union size */

    struct { struct { int16_t x, y; uint8_t hue, life; } parts[8000];   /* 48 000 */
             int16_t flow[40*30*2]; } nebula;                            /*  4 800  → ~52.8 KB */

    struct { uint8_t scratch[128*128]; } star;               /* 16 384 (3 noise tiles live in flash) */

    struct { int16_t dir[4000][3]; uint8_t mag[4000]; } stars;          /* ~28 KB */

    struct { int32_t row_u0[120], row_v0[120], row_du[120], row_dv[120]; } disk; /* 1 920 (tex in flash) */

    struct { uint16_t captured[VGA_160_W*VGA_160_H]; } lens;  /* 38 400 (LUT + panorama in flash) */

    struct { uint16_t src[VGA_160_W*VGA_160_H];               /* 38 400 */
             int16_t  warp_grid[(20+1)*(15+1)*2]; } spag;     /*  1 344  → ~39.7 KB */
};
```

**Scene 5 → 6 handoff:** scene 5 captures its final image into `lens.captured`; scene 6 reads the
same bytes as `spag.src` (a deliberate, documented cross-scene alias). Alternatively scene 6 can
re-render the lensing still in its `init()` to avoid the alias dependency.

---

## 6. Risks & fallbacks

| Scene | Biggest risk | Fallback |
|-------|--------------|----------|
| 1 Nebula | analytic curl-noise too slow | precomputed coarse flow grid + bilinear (the baseline); fewer particles |
| 2 Star | 4-octave per-pixel float noise ~7.7 ms (tight) | precomputed noise tiles + affine sample (~1.6 ms); or 30 fps |
| 3 Starfield | none major | cap star count; precompute aberration as a 1D cosθ LUT |
| 4 Disk | Doppler + Mode-7 over budget without interp | SIO interp for address gen; Doppler ramp LUT; half-vert-res + line-double |
| 5 **Lensing** | not compute (~1 ms) — only flash footprint / 160-res quality | single LUT + runtime yaw/zoom; or 4 B/px LUT + bilinear panorama; or 80×60 deflection LUT upscaled (interp blend) for a 320 variant |
| 6 Spaghettify | per-pixel sqrt distortion if pushed to 320 | coarse 1/8-res remap grid + bilinear (interp blend); keep at 160 |
| 7 Endcard | none (reuse proven 320 path) | — |
| Global | scene > 16.67 ms → 30 Hz hitch | engine degrades gracefully; drop res before fps |
| Global | scratch member > 76 KB eats headroom | keep every working set ≤ 76 KB; all read-only data in flash |

---

## 7. Build & beat-sync (reuses the existing pipeline)

**Desktop preview (MSYS2 UCRT64 + SDL2)** — the fast iteration loop:
```bash
# from an MSYS2 UCRT64 shell (gcc, SDL2, make)
cd 13_Singularity/host && make            # → host/singularity.exe
./singularity.exe                          # interactive, with audio
# instant scene capture (headless ok with SDL_VIDEODRIVER=dummy):
./singularity.exe --start-ms 195000 --screenshot-at 195000 --exit-after 195100
```

**Device firmware (RP2350 UF2)** — verified build:
```bash
export PICO_SDK_PATH=/d/Pico/pico-sdk PICO_EXTRAS_PATH=/d/Pico/pico-extras
cmake -B build_rp2350 -G "MinGW Makefiles" -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s
cmake --build build_rp2350 -j8
# → build_rp2350/singularity.uf2   (3.34 MB flash, 327 KB BSS)
```

**Regenerating the baked artifacts** (only when assets/music/LUT change):
```bash
python3 tools/pack_assets.py        # PNGs → assets/_packed/{assets.S,assets.h,*.bin,*.pal}
python3 tools/make_lens_lut.py      # geodesics → assets/_packed/lens_lut.{bin,h,S} + preview
ffmpeg -i "assets/Graviton Choir.mp3" -ac 1 -ar 22050 -f s16le music.raw
./tools/qoaconv_s16.exe music.raw music.qoa 22050 1
```

- **Music → QOA** (source is **stereo 48 kHz**; `-ac 1 -ar 22050` downmixes to the demo's mono rate):
  `ffmpeg -i "assets/Graviton Choir.mp3" -ac 1 -ar 22050 -f s16le music.raw` → `qoaconv_s16
  music.raw music.qoa 22050 1` → incbin via `music_qoa.S`. (~1.8 MB QOA for the 330 s track.)
- **Beat-sync:** already run — `analyze_music.py "assets/Graviton Choir.mp3" -k 8` reports **78.3 BPM,
  330.3 s**, with the segment/onset boundaries used in the §4 timeline. Re-run if the track changes.
- **Image assets:** add rows to `tools/pack_assets.py` (`rgb565` for the panorama and disk texture;
  `8bpp` for the title/endcard backgrounds) → regenerates `assets/_packed/assets.S` + `assets.h`.
- **Lensing LUT:** `python tools/make_lens_lut.py` writes `lens_lut.bin`; wire `lens_lut.S` incbin
  with `OBJECT_DEPENDS` in CMake.
- **Iterate via the host preview first:** build the SDL desktop binary in `host/` and use
  `--screenshot-at N` to tune each scene without flashing hardware, then build the `.uf2`.

---

## 8. Image-asset generation prompts

Generator: **Nano Banana Pro / GPT Image** (repo convention). Deep-space, cinematic, photoreal +
painterly. **No text in any AI image** — all text is rendered in code on top. Generate at the source
resolution (or higher; downscale is cheap, upscale is ugly), save PNG into `assets/`. See
[assets/PROMPTS.md](singularity/assets/PROMPTS.md) for the same prompts in the repo's per-scene format with
post-processing recipes.

1. **`title_star_pano_640.png`** — title backdrop **and** the lensing panorama source.
   > *Ultra-deep-field view of space: a dense star field with a faint luminous spiral-galaxy core
   > off-center, dust lanes glowing dim violet and indigo, scattered warm gold stars, subtle nebular
   > haze, cinematic, high dynamic range, no text, no foreground objects, seamless wide panorama.*

   Post: resize to 512×256 (quality) and 256×128 (baseline) **equirectangular**, then make the
   horizontal wrap seamless via an offset-feather-blend (see [assets/PROMPTS.md](singularity/assets/PROMPTS.md)).
   *(Done — `star_pano_512x256.png` / `star_pano_256x128.png`, edge RMSE 0.068.)*
2. **`accretion_disk_256.png`** — top-down (Cartesian) disk texture (scene 4), sampled as a tilted
   plane by the Mode-7 transform (NOT an unwrapped angle×radius strip).
   > *Top-down circular accretion disk of glowing plasma around a black core, concentric turbulent
   > bands from a white-hot inner edge through gold and amber to a deep-red outer rim, fine filament
   > turbulence, roughly uniform brightness (no baked-in directional asymmetry — Doppler is added in
   > code), black center hole, no text.*

   Post: 256×256. *(Done — `accretion_disk_256.png` exists; `accretion_disk_512.png` is the 1024²
   master.)*
3. **`solar_surface_512.png`** — star granulation, cut into 3× 128×128 tiles then made 2-D seamless
   (offset-feather-blend; see [assets/PROMPTS.md](singularity/assets/PROMPTS.md)). *(Done — `solar_tile_0..2.png`.)*
   > *Seamless tileable extreme close-up of a star's surface: convection granulation cells, bright
   > gold-white plasma with darker intergranular lanes, subtle magnetic filaments, high detail, no
   > text.*
4. **`endcard_640.png`** — rebirth endcard (scene 7).
   > *A serene newborn universe after a black hole: a soft glowing nebula in violet and teal with a
   > single bright new star, calm and hopeful, lots of dark empty space in the bottom third for text,
   > no text, cinematic.*

   Post: 640×480, 8bpp-quantized; "SINGULARITY" + credits drawn in code with `font8x8.h`.

---

## 9. Suno 4.5 music prompt

> **Format rules (this is how Suno actually parses input):**
> - **All descriptive sound-design language goes in the Style field.** The **structure box must
>   contain ONLY bracketed meta tags** — any prose or `(parenthetical)` notes placed there gets
>   **sung / narrated** (this was the bug in the first draft).
> - **Turn the Instrumental toggle ON** — it clears the lyric panel and suppresses vocals. Reinforce
>   "instrumental, no vocals, no spoken word" inside the Style field too (redundancy survives
>   generation). A `[Break]` or instrument-solo tag also interrupts any vocal hallucination.
> - **Duration is not set by a text instruction.** Suno 4.5 can generate up to ~8 min in one shot,
>   but length follows the *structure*; if a take comes out under ~4:00, use **Extend** and then
>   **"Get Whole Song"** to stitch to full length.
> - Style field: **front-load genre/mood**, 4–7 descriptors, up to **1000 characters**.

**Style field** (paste verbatim):

> *Cinematic cosmic orchestral-electronic score, instrumental, dark and awe-inspiring; a slow
> gravitational build to an overwhelming climax, then a serene resolution. Deep sub-bass drones, a low
> pulsing analog-synth heartbeat, swelling string ostinato, a vast church-organ chord, shimmering high
> pads, taiko-like impacts at the structural hits. ~80 BPM, 4/4, minor key, immense — Interstellar
> meets demoscene. No vocals, no spoken word. Clean, crisp transients for tight visual beat-sync.*

**Instrumental toggle: ON.**

**Structure box** (paste **only** these bracketed tags, one per line — no prose, no scene notes):

```
[Intro]
[Build-Up]
[Bridge]
[Percussion Breakdown]
[Climax]
[Break]
[Outro]
[Fade Out]
```

Optional instrument cues you can drop on their own bracketed lines to steer the sound at a point
(still tags only): `[String Swell]` `[Church Organ]` `[Taiko Drums]` `[Sub Bass Drop]`.

**Scene ↔ section map — OUR reference only, do NOT paste into Suno:**

| Section tag | Scene |
|-------------|-------|
| `[Intro]` | title + nebula |
| `[Build-Up]` | star ignition |
| `[Bridge]` | relativistic starfield |
| `[Percussion Breakdown]` | accretion disk |
| `[Climax]` | gravitational lensing |
| `[Break]` | event horizon / spaghettification |
| `[Outro]` / `[Fade Out]` | rebirth + endcard |

**Then:** export MP3 → QOA + beat-sync pipeline (§7). Re-roll until the **`[Climax]`** lands on a clear
downbeat so the Einstein-ring reveal cuts exactly to it; use **Extend → "Get Whole Song"** if the take
is under ~4:00.

---

## 10. Verification

- Build the SDL **host preview** (`host/`) first and snapshot each scene (`--screenshot-at N`) to
  iterate fast without flashing.
- Build the RP2350 `.uf2`, flash a Pico 2 on the Pimoroni VGA Demo Base, and confirm on a real VGA
  monitor: each scene holds **60 fps** (USB-serial `printf` frame timing), the **Einstein ring** and
  **Doppler asymmetry** read correctly, audio is in sync, and scene transitions land on the beat.
- Validate `lens_lut.bin` offline (render it against the panorama in the generator) before baking.

---

*Status: SINGULARITY is the active build. A second demo, **ORIGAMI** (folded-paper, flat-shaded
low-poly 3D), is staged in [`../14_Origami/IMPLEMENTATION.md`](../14_Origami/IMPLEMENTATION.md) as
the next production.*
