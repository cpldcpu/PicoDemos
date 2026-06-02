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

### What the past tracks got wrong
Three vocal-led attempts in a row were **raw, monotone, and the sections didn't
line up with the action**. The fix is a hard reset of the brief:

1. **Fully instrumental.** Sung words flatten into a loop and fight the visuals.
   A demo wants a *tune you remember*, carried by leads, not a voice.
2. **A real MELODY, with a theme that recurs.** State a memorable lead motif in
   the intro, pay it off at the drops, bring it back transformed in the finale.
   This is what makes the track feel composed instead of "raw".
3. **Demoscene craft, not generic synthwave.** Fast major-key arpeggios, glassy
   FM-bell and supersaw leads, call-and-response between two leads, a key change
   into the final drop — the Future Crew / Purple Motion / Skaven lineage.
4. **Hard sectional contrast** so the cuts can land on real boundaries: quiet
   glass intro → build → bright drop → soaring second lead → filtered breakdown
   → euphoric key-up finale → reflective fade.

5. **GRAVITAS.** The synth version was *better but too light* — it needs weight
   and grandeur. Fuse the demoscene leads with **cinematic orchestral muscle**:
   a low choir, brass swells, deep sub-bass, timpani/taiko booms and orchestral
   hits, and a **noble minor / Dorian** tonality (not bright major). Slow,
   monumental chords sit *under* the fast arps so the energy rides on a heavy
   foundation. Slightly slower tempo so every hit lands hard.

The demo is **re-timed to the delivered track** (scene cuts snap to its drops),
so Suno's exact lengths don't have to match — but giving it clear sections makes
that re-time clean. Target ~**3:00**, **~128 BPM**, **noble minor/Dorian** key.

### Demo arc to score (what each scene wants from the music)
| scene | musical job |
|---|---|
| **title** (brand reveal) | glassy intro: airy pad + a single wistful statement of the MAIN THEME on a bell lead; no drums; anticipation |
| **rotozoomer** | DROP 1: four-on-the-floor kick, punchy bass, the main theme in full on a bright supersaw — uplifting demoscene energy |
| **mercury plain** | a new SOARING counter-melody over the driving groove; wide, cruising, a touch of melancholy |
| **chrome objects (A)** | the HOOK — the catchiest lead, call-and-response between bell and saw, layered harmonies |
| **liquid metal** | BREAKDOWN: strip to filtered pad + sub bass, a quiet reprise of the intro theme, reverb tails, tension |
| **chrome reprise (B)** | DROP 2 / PEAK: double-time, KEY CHANGE UP, the theme returns triumphant and stacked, arpeggio fireworks |
| **credits tunnel** | OUTRO: resolve, the theme one last time on a lone glassy lead, slow filter close, reflective fade to silence |

### Two gotchas to avoid (learned the hard way)
- **It sang the descriptions.** Suno *sings any text in the Lyrics box that is
  not inside `[brackets]`.* So prose like "the catchiest lead" gets vocalised.
  Fix: **turn ON Suno's "Instrumental" switch**, and keep the Lyrics box to
  **bracket-only** section tags — the rich arrangement detail goes in the
  **Style** field, which is never sung.
- **It ran 8 minutes.** Too many/too-verbose sections make Suno sprawl. Keep it
  to ~7 short tags, say the length in the Style field, and **don't hit Extend** —
  pick the ~3-minute take. If a take still runs long, delete the `[Lead]` or
  `[Hook]` tag and regenerate.

### Style Prompt (paste into Suno's *Style* field — carries all the detail)
> Epic cinematic demoscene instrumental, about 3 minutes, ~128 BPM, noble
> minor/Dorian key. Glassy FM-bell and supersaw leads with a strong recurring
> melody and fast arpeggios, riding a HEAVY orchestral foundation: low choir,
> brass swells, deep sub-bass, timpani and taiko booms, orchestral hits.
> Monumental and reflective, huge dynamics — solemn intro, weighty drop on the
> main theme, soaring minor counter-melody, filtered breakdown reprise, then a
> double-time key-change-up finale and a slow reflective fade. Grand, with
> gravitas; Second Reality / Purple Motion grandeur meets hybrid film score.
> Strictly instrumental, no vocals, no vocal chops.

### Structure Prompt (paste into the *Lyrics* field — bracket-only, nothing sung)
*(Every line is a `[bracketed]` tag, so Suno reads it as a section direction and
sings nothing. Short cues inside the brackets are safe; full prose is not.
Belt-and-braces: also flip the **Instrumental** toggle on.)*

```text
[Instrumental]
[Intro: solemn, low choir, lone bell states the theme, deep sub, no drums]
[Build: timpani roll, rising arpeggio, brass swell into the drop]
[Drop: taiko + four-on-the-floor, sub-bass, theme on supersaw doubled by brass]
[Lead: soaring minor counter-melody, wide strings underneath]
[Hook: bell and saw call-and-response over orchestral hits and choir]
[Breakdown: filtered pad + lone choir, quiet reprise of the theme]
[Drop 2: double-time, key change UP, full brass + choir, biggest moment]
[Outro: lone lead over a slow choir chord, filter close, fade to silence]
```

*(If a vocal still sneaks in: set the "exclude styles" box to
`vocals, lyrics, singing, vocal chops` and regenerate.)*

### Music Conversion
Convert the output audio using:
1. `ffmpeg -i input.mp3 -ar 22050 -ac 1 -c:a pcm_s16le music.wav`
2. `tools/qoaconv_s16.exe music.wav music.qoa`
3. Link into firmware via `music_qoa.S`.
