# AI image prompts for 20_TheDemo

Bio-organic aesthetic: wet, iridescent, alien-vegetal, deep teal + burnt orange + magenta accents on near-black grounds. Microscope/octane render look. No text in the AI images themselves — we render all text in code on top.

Generate at the listed source resolution (or higher — downscale is cheap, upscale is ugly). Save as PNG. Drop into `thedemo/assets/`. The packer script (TBD) will quantize, palettize, and emit C-incbinned blobs.

After generating, batch-process from WSL:

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/20_TheDemo/thedemo/assets && for f in *.png; do convert \"\$f\" -strip -auto-orient \"\$f\"; done"
```

---

## Scene 1 — Title (0:00–0:20, 640×480)

### `title_bg_640.png` — backdrop only, no text
**Source res:** 1920×1440 → downscale to 640×480
**Prompt:**
> *Iridescent biopunk surface, intricate organic geometry, deep teal coral substrate with burnt-orange bioluminescent veins, wet glistening texture, dark vignette around the edges, microscope photograph aesthetic, octane render, 4k, no text, no letters, no logo*

**Avoid:** any text, faces, recognizable objects in the center (we overlay the title there).
**Post:** `convert title_bg_640.png -resize 640x480^ -gravity center -extent 640x480 title_bg_640.png`

---

## Scene 2 — Voxel landscape (0:20–0:50, 320×240)

The voxel renderer needs a **paired heightmap + colormap** at the same resolution. AI generators are bad at producing exactly paired images, so we generate the colormap and *derive* the heightmap from it with imagemagick.

### `voxel_color_1024.png` — top-down colormap
**Source res:** 1024×1024
**Prompt:**
> *Top-down satellite view of an alien coral reef, branching reefs and deep basins, iridescent teal-to-orange gradient by elevation, wet glistening surface, no shadows, no clouds, no horizon, no text, tileable, octane render, 8k, orthographic projection*

**Avoid:** perspective foreshortening (we want pure top-down), text/letters, anything centered.

### `voxel_height_1024.png` — derived heightmap
Derive from the colormap with imagemagick:

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/20_TheDemo/thedemo/assets && \
  convert voxel_color_1024.png \
    -colorspace Gray \
    -blur 0x3 \
    -level 10%,95% \
    voxel_height_1024.png"
```

Tweak the level percents until peaks look like peaks and basins look like basins. If the result is too flat: `-evaluate Multiply 1.4` after the level. If too noisy: bump the blur radius.

The voxel engine consumes both at 1024×1024 then downsamples internally. We don't ship them at this res — packer will resize to whatever the engine wants (probably 512×512 to fit flash budget).

---

## Scene 3 — Fluid portraits (0:50–1:20, 160×120)

Three small high-contrast portraits get stamped as "dye" into the fluid sim and reform out of the chaos.

### `portrait_a_512.png`, `portrait_b_512.png`, `portrait_c_512.png`
**Source res:** 512×512 (downscaled to ~96×96 by the packer)
**Prompts (one per portrait, vary the species):**

A — translucent jellyfish creature:
> *Portrait of a translucent jellyfish-like alien being, glowing inner organs visible through gelatinous skin, single large iridescent eye centered, symmetric front-facing pose, black background, biopunk surrealism, high contrast for silhouette readability, no text*

B — coral-skinned humanoid:
> *Portrait of an alien humanoid with iridescent coral-skinned face, embedded fleshy spines, two glowing teal eyes, symmetric front-facing pose, black background, biopunk surrealism, high contrast, no text*

C — anemone-headed entity:
> *Portrait of an alien being with a head crowned by writhing iridescent anemone tendrils, single magenta eye, symmetric front-facing pose, black background, biopunk surrealism, high contrast, no text*

**Avoid:** complex backgrounds (we want pure black except the subject), text, multiple subjects.
**Post:** force black background and bump contrast so the silhouette is unambiguous:

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/20_TheDemo/thedemo/assets && \
  for f in portrait_a_512.png portrait_b_512.png portrait_c_512.png; do
    convert \"\$f\" -level 10%,90% -modulate 110,120,100 -resize 96x96 \"\${f%.png}_96.png\"
  done"
```

---

## Scene 4 — Copperbar greetz (1:20–1:40, 640×480)

Mostly procedural — no AI image strictly required. But for shadebob "trails," a tasteful background bitmap is welcome.

### `greetz_bg_640.png` — optional ambient backdrop, gets darkened to ~25% before copperbars overlay
**Source res:** 1920×1440 → 640×480
**Prompt:**
> *Out-of-focus iridescent organic geometry, very dark deep teal and magenta, abstract bokeh, no recognizable forms, painterly, suitable as a moody backdrop, no text*

**Avoid:** anything sharp/in focus, any text. This should be a mood backdrop, not subject art.

---

## Scene 5 — Raytraced spheres (1:40–2:30, 320×240)

The raytracer needs an **environment map** for sphere reflections. Equirectangular projection is what most renderers expect; we'll consume it at 512×256 (RGB565 = 256 KB — fits flash).

### `envmap_equi_2048.png`
**Source res:** 2048×1024 equirectangular → downscaled to 512×256
**Prompt:**
> *Equirectangular 360° panorama, interior of a vast bio-organic alien dome, iridescent wet surfaces in every direction, deep teal floor transitioning to magenta ceiling, soft glowing biolume pockets scattered across the surface, octane render, no horizon line, no text, seamless wrap, 8k*

**Avoid:** any visible seams at the wrap, text, a clearly defined "up" or "down" (the spheres reflect in every direction).
**Post:** confirm 2:1 aspect ratio (`identify envmap_equi_2048.png`), downscale:

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/20_TheDemo/thedemo/assets && \
  convert envmap_equi_2048.png -resize 512x256! envmap_equi_512.png"
```

---

## Scene 6 — Reaction-diffusion logo (2:30–2:50, 160×120)

The RD sim resolves into a "logo" shape. We need a **1-bit mask** of the shape that the sim seeds with for the chemical pattern to converge into.

### `logo_mask_512.png` — 1-bit shape mask
**Source res:** 512×512 → downscaled and binarized to 160×120 1bpp
**Prompt:**
> *Stylized organic logogram, single bold biopunk symbol resembling intertwined coral fronds, pure white solid shape on pure black background, vector-style silhouette, high contrast, symmetric, no text, no letters, suitable as a logo*

**Avoid:** thin strokes (won't survive binarization at 160×120), text/letters, any color other than black/white.
**Post:**

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/20_TheDemo/thedemo/assets && \
  convert logo_mask_512.png -colorspace Gray -threshold 50% -resize 160x120 logo_mask_160.png"
```

---

## Scene 7 — Tunnel + particles (2:50–3:20, 320×240)

### `tunnel_tex_512.png` — inner-wall texture for the tunnel
**Source res:** 512×512 → wraps as a cylinder texture, consumed as 256×256
**Prompt:**
> *Seamless tileable texture of iridescent alien intestine wall, wet glistening pinkish-magenta with teal veins, intricate organic detail, tileable in both directions, octane render, no shadows, no text, 4k*

**Critical:** must be seamlessly tileable left↔right (it wraps around the tunnel). Generate with "seamless tileable" emphasis and verify by tiling 2×2 — no visible seams.
**Post:**

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/20_TheDemo/thedemo/assets && \
  convert tunnel_tex_512.png -resize 256x256 tunnel_tex_256.png"
```

### `endcard_mask_512.png` — 1-bit mask of the endcard text shape
Particles form into this shape at the end of the scene. Could be the demo title.
**Source res:** 512×128 → consumed as ~300×80 1bpp
**Prompt:**
> *Stylized biopunk logogram reading "THE DEMO" in chitinous organic lettering, pure white on pure black background, bold thick strokes, high contrast, no other elements, no decorative flourishes that would dissolve at low resolution*

(Yes this one has text — we want the lettering. Make sure the AI gets the spelling right; if it doesn't, render the text yourself in a vector tool and skip the AI.)

**Avoid:** thin/wispy strokes, italic, decorative serifs that won't binarize.
**Post:** as logo_mask.

---

## Scene 8 — Rotozoom credits (3:20–3:50, 640×480)

### `endcard_640.png` — final logo backdrop
**Source res:** 1920×1440 → 640×480
**Prompt:**
> *Iridescent biopunk logo emerging from deep dark depths, wet reflections, depth-of-field haze in the foreground, dramatic chiaroscuro lighting, bottom 25% darker for credit-text overlay, octane render, 4k, no text*

**Avoid:** text (we render credits on top), bright bottom (overlay zone).

---

## What we're NOT generating with AI

- **Title text and endcard text** — rendered in code with a chunky bitmap font. AI text rendering is unreliable below 256 px height.
- **Greetz scroller text** — rendered in code, scrolled across the bottom of scene 4.
- **Cubemap faces** — too hard to get six consistent faces; we use the equirectangular env-map in scene 5.

## Summary checklist

To kick off scene production, generate these and drop in `thedemo/assets/`:

- [ ] `title_bg_640.png`
- [ ] `voxel_color_1024.png` (heightmap derived)
- [ ] `portrait_a_512.png`, `portrait_b_512.png`, `portrait_c_512.png`
- [ ] `greetz_bg_640.png` *(optional)*
- [ ] `envmap_equi_2048.png`
- [ ] `logo_mask_512.png`
- [ ] `tunnel_tex_512.png`
- [ ] `endcard_mask_512.png` *(or skip AI and hand-render)*
- [ ] `endcard_640.png`

Plus: drop your **`music.mp3`** in `thedemo/` and we'll transcode it to QOA per README.md.
