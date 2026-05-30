# AI prompts for 13_Singularity ("SINGULARITY")

**Cosmic / relativistic aesthetic:** deep indigo-violet voids, accretion gold → white-hot,
relativistic blue→red, one pure-white singularity flash. Photoreal + cinematic + painterly.
**No text in any AI image** — all text is rendered in code on top with `font8x8.h`.

Generate at the listed source resolution (or higher — downscale is cheap, upscale is ugly). Save
as PNG into `13_Singularity/assets/`. The packer (`tools/pack_assets.py`) quantizes/palettizes and
emits C-incbinned blobs. The lensing remap LUT is **not** an image — it is baked by
`tools/make_lens_lut.py` (see §Lensing LUT below).

After generating, batch-normalize from WSL:

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/PicoDemos/13_Singularity/assets && for f in *.png; do convert \"\$f\" -strip -auto-orient \"\$f\"; done"
```

Generator: Nano Banana Pro / GPT Image (repo convention).

---

## Scene 0/5 — Star panorama (title bg + gravitational-lensing background)

### `title_star_pano_640.png` — deep-field backdrop, no text
**Source res:** 2048×1024 (equirectangular) → 512×256 (quality) and 256×128 (baseline)
**Prompt:**
> *Ultra-deep-field view of space: a dense star field with a faint luminous spiral-galaxy core
> off-center, dust lanes glowing dim violet and indigo, scattered warm gold stars, subtle nebular
> haze, cinematic, high dynamic range, no text, no foreground objects, seamless wide panorama.*

**Avoid:** text/letters, any large foreground object, a bright element dead-center (the title and
the black-hole shadow sit there).
**Post — resize, then make the horizontal wrap seamless (✅ done):**
```bash
wsl -e bash -c 'cd /mnt/d/Toyprojects/PicoDemos/13_Singularity/assets && \
  convert title_star_pano_640.png -resize 512x256\! star_pano_512x256.png && \
  # offset-feather-blend the vertical wrap seam (roll by half-width, blend a feathered edge band):
  convert star_pano_512x256.png -roll +256+0 /tmp/proll.png && \
  convert -size 512x256 xc:white -fill black -draw "rectangle 40,0 471,255" -blur 20x0 /tmp/pmask.png && \
  convert star_pano_512x256.png /tmp/proll.png /tmp/pmask.png -composite star_pano_512x256.png && \
  convert star_pano_512x256.png -resize 256x128\! star_pano_256x128.png'
```
The lensing LUT indexes into the equirectangular panorama; runtime yaw rotation is a free
`(u + yaw) & (PANO_W-1)` column wrap, so the wrap **must** be seamless. After the blend the
left/right edge-match RMSE dropped from 0.123 → **0.068** (faint, acceptable in a star field).

---

## Scene 2 — Star ignition (160×120)

### `solar_surface_512.png` — seamless granulation, cut into 3× 128×128 tiles
**Source res:** 512×512 (seamless/tileable)
**Prompt:**
> *Seamless tileable extreme close-up of a star's surface: convection granulation cells, bright
> gold-white plasma with darker intergranular lanes, subtle magnetic filaments, high detail, no
> text.*

**Avoid:** vignettes, a curved limb/horizon (we want flat tileable surface), text.
**Post — slice into 3 tiles, then make each tile 2-D seamless (✅ done):**
```bash
wsl -e bash -c 'cd /mnt/d/Toyprojects/PicoDemos/13_Singularity/assets && \
  convert solar_surface_512.png -resize 384x128\! -crop 128x128 +repage solar_tile_%d.png && \
  convert -size 128x128 xc:white -fill black -draw "rectangle 20,20 107,107" -blur 0x10 /tmp/tmask.png && \
  for t in solar_tile_0 solar_tile_1 solar_tile_2; do \
    convert $t.png -roll +64+64 /tmp/roll.png && \
    convert $t.png /tmp/roll.png /tmp/tmask.png -composite $t.png; \
  done'
```
A raw slice from the strip is **not** seamless (the affine sampler scrolls/zooms through the tiles,
so visible seams would drift across the star surface). The offset-feather-blend above removes both
the horizontal and vertical seam — verified by tiling each 2×2.

---

## Scene 4 — Doppler-beamed accretion disk (160×120)

### `accretion_disk_256.png` — top-down (Cartesian) disk texture  ✅ done
**Source res:** 1024×1024 master (`accretion_disk_512.png`) → 256×256 (`accretion_disk_256.png`)
**Prompt:**
> *Top-down circular accretion disk of glowing plasma around a black core, concentric turbulent
> bands from a white-hot inner edge through gold and amber to a deep-red outer rim, fine filament
> turbulence, roughly uniform brightness (no baked-in directional asymmetry — Doppler is added in
> code), black center hole, no text.*

**Avoid:** one side brighter than the other (the relativistic beaming is computed per-frame), text,
stars in the background.
**Post:**
```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/PicoDemos/13_Singularity/assets && \
  convert accretion_disk_512.png -resize 256x256\! accretion_disk_256.png"
```
The Mode-7 sampler treats this as a **flat top-down (Cartesian) plane** seen at a shallow angle — it
is NOT an unwrapped (angle × radius) strip. The central black disk + inner photon glow are drawn in
code, and the SIO interpolators generate the texture addresses.

---

## Scene 7 — Rebirth endcard (320×240)

### `endcard_640.png` — newborn-universe backdrop, no text
**Source res:** 1280×960 → 640×480
**Prompt:**
> *A serene newborn universe after a black hole: a soft glowing nebula in violet and teal with a
> single bright new star, calm and hopeful, lots of dark empty space in the bottom third for text,
> no text, cinematic.*

**Avoid:** text, busy detail in the bottom third (credits scroll there).
**Post:**
```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/PicoDemos/13_Singularity/assets && \
  convert endcard_640.png -resize 640x480^ -gravity center -extent 640x480 endcard_640.png"
```

---

## Lensing LUT (not an image)

`tools/make_lens_lut.py` traces a backward photon through each of the 160×120 screen pixels,
integrating the equatorial null geodesic `d²u/dφ² + u = 3·M·u²` (RK4) until the photon is captured
(`r < r_s` → shadow sentinel) or escapes (→ panorama `(u,v)`), and writes `lens_lut.bin`
(baseline: `uint16` packed `(u,v)` per pixel = 38.4 KB). Wire `lens_lut.S` (`.incbin`) into CMake
with `OBJECT_DEPENDS`. Checked in so a fresh clone builds without Python. Self-check: render the LUT
against the panorama inside the generator and eyeball the Einstein ring before baking.

---

## Music — Suno 4.5

Full prompt + format rules in [`../IMPLEMENTATION.md`](../IMPLEMENTATION.md#9-suno-45-music-prompt).
Key rules: **descriptive language goes in the Style field; the structure box holds ONLY bracketed
tags** (prose there gets sung); **Instrumental toggle ON**; **duration follows the structure, not a
text request** — use **Extend → "Get Whole Song"** if a take is short of ~4:00.

**Style field** (paste verbatim):
> *Cinematic cosmic orchestral-electronic score, instrumental, dark and awe-inspiring; a slow
> gravitational build to an overwhelming climax, then a serene resolution. Deep sub-bass drones, a low
> pulsing analog-synth heartbeat, swelling string ostinato, a vast church-organ chord, shimmering high
> pads, taiko-like impacts at the structural hits. ~80 BPM, 4/4, minor key, immense — Interstellar
> meets demoscene. No vocals, no spoken word. Clean, crisp transients for tight visual beat-sync.*

**Instrumental: ON. Structure box (paste only these tags):**
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
(Scene↔section mapping is in IMPLEMENTATION.md §9 — for our reference, **do not paste it into Suno.**)

Export MP3 → `ffmpeg … -ac 1 -ar 22050 -f s16le` → `qoaconv_s16` → `music.qoa`. Re-roll until the
**`[Climax]`** lands on a clear downbeat for the Einstein-ring cut.
