# Quicksilver (Mercury) Asset Prompt Guide

This document contains guidance and ready-to-use prompts for generating the 2D art and music assets for the **15_Quicksilver** demo.

---

## 2D Art (nano banana) Prompt Guide

### General Guidance
- **Description structure:** Describe subject + style + lighting + palette + framing.
- **Tiling:** Specify "seamless tileable" for textures (e.g. ground and rotozoom textures).
- **Text & borders:** Explicitly ask for "no text, no watermark, no border, no frame".
- **Contrast:** Request high contrast to make the colors pop beautifully on the RP2350's 5-bit DAC output.
- **Spheremaps:** Ask for a centered fisheye/orthographic sphere that fills the frame completely.

---

### Ready-To-Use Prompts

#### 1. Chrome Spheremap (Centerpiece Reflection)
> **Prompt:**  
> A perfectly polished liquid chrome sphere reflection probe, orthographic, filling a square frame, studio HDR environment reflected in it — cool blue-white highlights, warm amber rim, deep blacks, dramatic specular streaks, smooth gradients, no text, no border, high contrast.

- **Source File:** `assets/envmap.png`
- **Output Target:** `assets/_packed/envmap.bin` (256x256, `rgb565`)

#### 2. Mercury Ground Tile (Mode-7, Seamless)
> **Prompt:**  
> Seamless tileable texture of rippling liquid mercury, top-down, soft caustic reflections, silver and steel-blue, subtle hexagonal flow, high contrast, no seams, no text.

- **Source File:** `assets/ground.png`
- **Output Target:** `assets/_packed/ground.bin` (256x256, `rgb565`)

#### 3. Rotozoom Texture (Seamless)
> **Prompt:**  
> Seamless tileable ornate art-deco chrome filigree on black, radial symmetry, metallic silver with cyan and magenta iridescence, crisp edges, high contrast, no text, no border.

- **Source File:** `assets/roto.png`
- **Output Target:** `assets/_packed/roto.bin` (256x256, `rgb565`)

#### 4. Sky Panorama (Equirectangular Sky) — *needs reissue: seamless wrap*
The current sky shows a vertical SEAM where it wraps (left and right edges don't
match). Please reissue it **horizontally seamless** — the left and right edges
must be identical so it tiles. Double-wide (1024) is ideal so the wrap is rarer.
> **Prompt:**  
> Equirectangular 360° panorama of a chrome-and-violet dusk sky over a mirror-flat liquid-metal sea, soft gradient horizon, distant glowing sun, smooth. SEAMLESS horizontal wrap — the left and right edges must match exactly so it tiles with no visible seam. No text, no border.

- **Source File:** `assets/sky_pano.png`
- **Output Target:** `assets/_packed/sky.bin` (**1024**x128 preferred, or 512x128, `rgb565`)
- If you deliver 1024 wide, change the pack entry to `("sky","sky_pano.png","rgb565",1024,128,...)`; the code reads ASSET_SKY_W so it adapts.

#### 5. Title/Endcard Backdrop
> **Prompt:**  
> Abstract molten silver and quicksilver droplets on a dark gradient, dramatic rim light, cinematic, lots of negative space in the center for a title, no text.

- **Source File:** `assets/title_bg.png`
- **Output Target:** `assets/_packed/title_bg.bin`

#### 6. Chrome Wordmark Logo *(OPTIONAL UPGRADE — requested)*
The title currently renders "QUICKSILVER" from the built-in 8×8 bitmap font as
bevelled chrome. A generated wordmark would look far better. If you generate
this, drop it in as `assets/title_logo.png` and I'll blit it over the backdrop
(black-keyed) instead of the font.
> **Prompt:**
> The single word "QUICKSILVER" as a polished liquid-chrome wordmark, bold
> condensed sans-serif, reflective mercury surface with cyan-white highlights
> and warm amber rim light, centered on a PURE BLACK background, no other
> objects, no border. Wide aspect ~ 1024×256.

- **Source File:** `assets/title_logo.png` (deliver on pure black for keying)
- **Output Target:** `assets/_packed/title_logo.bin` (rgb565, e.g. 320×80)
- **Pack as:** add `("title_logo", "title_logo.png", "rgb565", 320, 80, False, None)`
  to `tools/pack_assets.py`.

> **Open requests to the artist (Gemini / Nano Banana):** #6 (wordmark, optional)
> and #7 below (tunnel wall — would noticeably improve the finale).

#### 7. Tunnel Wall *(requested — would improve the end scene)*
The credits finale is a chrome tunnel; it currently borrows the rotozoom
filigree, which is a *centered mandala* and reads busy/low-res when wrapped onto
a tunnel. A texture authored to **tile seamlessly in BOTH axes** (wraps left↔right
= around the tube, and top↔bottom = depth) would look far better.
> **Prompt:**
> Seamless tileable texture of polished liquid-chrome vertical flutes / fluted
> metal pipes with soft cyan-white highlights and deep shadows between the
> flutes, smooth gradients (NOT busy), tiles perfectly on all four edges, no
> seams, no text, no border, high contrast. Square.

- **Source File:** `assets/tunnel.png`
- **Output Target:** `assets/_packed/tunnel.bin` (rgb565, 256×256)
- The end scene downsamples it to a 128×128 SRAM copy. Until delivered it falls
  back to the roto texture.

#### 8. LATENT group logo *(requested — title & end card)*
We founded the demo group **LATENT**. The title and end card currently render
"LATENT" in the 8×8 font; a proper logo would sell it. Want a small, distinct
mark — NOT another chrome wordmark like QUICKSILVER (it must read as a different
*group* identity), e.g. a minimalist liquid-metal glyph/monogram + the word.
> **Prompt:**
> A minimalist demoscene group logo for "LATENT" — a single liquid-mercury
> droplet/monogram mark beside clean condensed letters, cool silver on PURE
> BLACK, subtle, NOT busy, lots of negative space. Wide strip. No other text.

- **Source File:** `assets/latent_logo.png` (on pure black for keying)
- **Output Target:** `assets/_packed/latent_logo.bin` (rgb565, 256×48)
- Pack entry: `("latent_logo","latent_logo.png","rgb565",256,48,False,None)`.

---

## Music (Suno 4.5) Prompts

### General Guidance
- Write a **narrative, descriptive style prompt** (genre + mood + tempo/energy + core instruments + production + progression).
- Keep the **style field concise** (~200 chars).
- Put **structure/metatags in the lyrics field** in `[brackets]`.

---

### The problem with the first track
It was *too smooth* — a pleasant chrome-synthwave loop with no dynamics and no
climax. A demo soundtrack needs **contrast**: quiet intro → build → a hard DROP
on the centrepiece → breakdown → a bigger second drop → resolve. It should be
**mostly instrumental** (vocals fight the visuals), with only a few sung words
landing on the big moments. Below is a prompt and a sparse lyric authored to the
demo's actual arc.

### Demo arc to score (timestamps the music should hit)
| time | scene | the music should… |
|---|---|---|
| 0:00–0:06 | title | quiet, glassy, a single chrome shimmer |
| 0:06–0:28 | rotozoomer | start the pulse; rising arpeggio build |
| 0:28–0:47 | mercury plain | wide, cruising, melodic — tension building |
| **0:47–1:32** | **chrome objects** | **THE DROP** — gated drums + screaming metallic lead |
| 1:32–1:56 | liquid metal | breakdown: strip to bass + filtered pads |
| **1:56–2:55** | **chrome reprise** | **bigger second drop**, double-time, euphoric |
| 2:55–end | credits tunnel | resolve, reflective, fade out |

### Style Prompt (Primary, ~200 chars)
> **Style:**  
> Cinematic chrome synth-metal, ~128 BPM, DYNAMIC with hard drops. Glassy intro, rising arpeggio build, then a euphoric DROP: gated reverb drums, distorted metallic lead, deep analog bass. Breakdown, bigger second drop, reflective fade. Mostly instrumental, huge contrast.

*(If Suno still flattens it: add `[Build]`, `[Drop]`, `[Breakdown]` as their own
generations, or use the "intensity"/exclude-style controls to force the dynamics.)*

### Lyrics Field — sparse, only on the big moments
*(Brackets are structure tags; keep most sections instrumental. The few sung
words land on the title, the two drops, and the outro.)*

```text
[Intro – ambient pads, no drums]
(whispered) quicksilver

[Build – arpeggio rising, drums enter]

[Drop – full energy]
Liquid chrome
Liquid chrome

[Breakdown – bass and filtered pads only]
(whispered) running through my hands

[Drop 2 – bigger, double-time]
Quicksilver
Shine

[Outro – fade to silence]
(whispered) reflecting me
```

### Music Conversion
Convert the output audio using:
1. `ffmpeg -i input.mp3 -ar 22050 -ac 1 -c:a pcm_s16le music.wav`
2. `tools/qoaconv_s16.exe music.wav music.qoa`
3. Link into firmware via `music_qoa.S`.
