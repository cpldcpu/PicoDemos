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
> is the only one still open. #7/#8/#9/#10 all delivered.

> **Authoring note for ALL textures below — deliver them PRE-DOWNSAMPLED at the
> size the demo actually samples (128×128 for walls/matcaps, NOT 256×256).** The
> runtime copies every wall/matcap into a 128×128 SRAM tile anyway (the 256
> source is downsampled on the fly and otherwise wastes 3/4 of the flash). Authoring
> at 128 makes the flash asset 1/4 the size and lets the loader do a straight copy.
> If you deliver 128 px, set the pack entry width/height to 128 (e.g.
> `("conduit","conduit.png","rgb565",128,128,...)`) — the code reads `ASSET_*_W`,
> so the downsample step becomes a no-op automatically.

#### 7. Credits Tunnel Wall — *delivered (`tunnel.png`, chrome flutes)*
Used by the finale; keep as-is.

#### 9. Mid-demo Conduit Wall — *DELIVERED (`conduit.png`, 128×128 chrome cell grid)*
Scene 3 (0:27, the "Chrome Conduit" tunnel) now flies `conduit.png` (a chrome
cell grid with cyan nodes), wired in `effects/tunnel.c` via `asset_conduit_data`.
Replaced the old rotozoom-filigree borrow. Original request kept below for ref:
> **Prompt:**
> Seamless tileable texture of a liquid-chrome HEXAGONAL honeycomb / mercury cell
> grid, flowing molten metal in the channels, cool silver with cyan and a touch
> of magenta iridescence, soft specular highlights, smooth (NOT busy), bold
> medium-scale cells (about 6 across), tiles perfectly on all four edges
> (left↔right AND top↔bottom), no seams, no text, no border, high contrast.

- **Source File:** `assets/conduit.png`  →  **Output:** `assets/_packed/conduit.bin`
  (rgb565, **128×128** per the authoring note above).
- **Pack entry:** `("conduit","conduit.png","rgb565",128,128,False,None)`.
- Once delivered, point `effects/tunnel.c` at `asset_conduit_data` /
  `ASSET_CONDUIT_W` (it falls back to `asset_roto_data` until then).

#### 11. Conduit Height / Bump Map — *REQUESTED (`conduit_bump.png`, 256×256 grayscale)*
The tunnel is now **bump-mapped with moving light rings** (`qs_tunnel_render_lit`):
bright lights sweep forward along the tube and the wall's surface relief catches
them — ridges flash as a light rolls past. The relief currently derives from the
*colour* luma, which is soft (big smooth chrome panels), so the bump reads gentle.
A **dedicated grayscale height map** with crisp, high-frequency detail is the
single biggest upgrade — the lights will rake across real grooves and seams.
Target look: a classic demoscene bump-scroller backdrop — **smooth flowing grey
liquid metal** with glossy specular highlights (NOT crumpled foil / sharp creases,
NOT industrial panels). The shader (`qs_tunnel_render_lit`) already does the grey
desaturation, specular gloss and emboss; it just needs a source with **large, soft,
rounded** relief.
> **Prompt:**
> Seamless tileable GRAYSCALE height map of a SMOOTH FLOWING LIQUID MERCURY / molten
> chrome surface — large soft rounded blobs, bulges and valleys like the surface of
> liquid metal, smooth organic curves. NO sharp creases, NO fine wrinkles, NO noise,
> NO grain. White = raised bulge, black = recessed. Medium-to-large features (about
> 8–12 across the image), smooth high contrast, soft rounded gradients. Pure
> grayscale height field, NO baked lighting or shadows, NO colour, no text, no
> border. Tiles perfectly on all four edges (left↔right AND top↔bottom).

> *(First attempt `conduit_bump.png` came back as crumpled foil — sharp fine creases
> → grainy/sparkly under specular. The build-time blur in `qs_tunnel_build_grad_g8`
> (`smooth`/`baseline`) can round it a little, but the real fix is a source with the
> large rounded blobs above.)*

- **Source File:** `assets/conduit_bump.png` (256×256 grayscale)
  →  **Output:** `assets/_packed/conduit_bump.bin` (gray8, 256×256, 64 KB flash).
- **Pack entry (already added):** `("conduit_bump","conduit_bump.png","gray8",256,256,False,None)`.
- **Already wired:** `effects/tunnel.c` auto-switches to `asset_conduit_bump_data`
  the moment `ASSET_CONDUIT_BUMP_W` is defined (point-downsampled to the 128 grid
  by `qs_tunnel_build_grad_g8`); until then it derives the bump from colour luma.
  So the *only* step is: drop the PNG in `assets/`, run `pack_assets.py`, rebuild.

#### 10. Extra Chrome Matcaps — *DELIVERED (`envmap2.png` gold, `envmap3.png` violet)*
The two chrome scenes now use authored matcaps instead of the old code re-grade:
block A (drop 1) = `envmap3` (violet iridescent), block B (climax) = `envmap2`
(warm gold), copied straight to SRAM in `effects/chrome.c` (both 128×128). The
original neutral `envmap.png` is no longer referenced. Original request kept below:
> **Prompt A (blue steel):** A polished liquid-chrome reflection probe, orthographic
> sphere filling a square frame, COOL studio lighting — blue-white key, steel-grey
> body, deep blacks, crisp specular streaks, smooth gradients, no text, no border.
> **Prompt B (warm gold):** …same sphere, WARM lighting — amber/gold key, copper
> rim, bronze body, one hot white highlight, deep shadows, smooth, no text, no border.
> **Prompt C (violet iridescent):** …same sphere, IRIDESCENT oil-slick lighting —
> magenta-to-cyan gradients across the chrome, violet rim, dark, smooth, no text, no border.

- **Source Files:** `assets/envmap2.png` (gold), `assets/envmap3.png` (violet), …
- **Output:** `assets/_packed/envmapN.bin` (rgb565, **128×128**).
- **Pack entries:** `("envmap2","envmap2.png","rgb565",128,128,False,None)` etc.
- Once delivered, replace the `env_cool`/`env_warm` re-grades in `effects/chrome.c`
  with the real maps (and add a third for a per-object swap if desired).

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

## Music (Suno 5.5 Pro) Prompts

### What changed with v5.5 — and how we use it
Researched the v5.5 / Studio 1.2 feature set (June 2026). Three things matter
for a *synced demo soundtrack* and they change our whole approach:

1. **Parameterised section metatags.** v5.5 honours arrangement detail placed
   *inside* a section tag, e.g. `[Drop: taiko, sub-bass, theme on full brass]`.
   This overrides the Style field per-section, so we can score each scene
   individually instead of hoping one global description shapes the whole track.
   This is the single biggest control upgrade — use it heavily.
2. **Negative constraints are now mandatory.** v5.5 rewards 2–3 explicit `no`
   tags (`no vocals, no reverb wash, no lo-fi`). Vague 6-word styles no longer
   land; *modular, layered* descriptors do, and subtle descriptors finally bite.
3. **Warp Markers + Quantize (Suno Studio 1.2) = the sync tool.** Post-generation
   you can drag transients onto a grid (non-destructive, like Ableton Warp /
   Logic Flex Time) and quantise to lock drops tight. It is for *subtle* timing
   correction, **not** heavy stretching — for big timing changes, regenerate.
   Generation itself still does **not** condition on exact second-timings.

**Strategic consequence — co-design, don't retrofit.** Every prior attempt
retimed the *demo* to whatever Suno produced, which is why the cuts feel "off".
We now do the opposite: the storyboard below fixes the section lengths, we ask
Suno for a track with those sections, then use **Warp Markers** to nudge its
drops onto our exact cut points (33s/…). Co-designed, not retrofitted.

### Target shape
- **~2:30** total (a fast demo; 2–3 min is the sweet spot). `[End]` hard-stops it.
- **~140 BPM**, **minor / Dorian** key — uplifting, driving, energetic.
- **Single pass through the effects** — each effect is seen *once*; only CHROME
  (the centrepiece) returns, on the second drop, with fresh objects. So the
  music needs exactly **two** big drops, not a stream of equal hits.
- Fully **instrumental** (toggle ON + `[Instrumental]`): sung words flatten into
  a loop and fight the visuals. A demo wants a *tune you remember* on leads.
- A **recurring main theme**: stated solo in the intro, paid off on drop 1, and
  returned transformed (key-up) on the final drop. This is what makes it read as
  *composed* rather than raw/monotone — the recurring complaint.
- **This must sound like a DEMO TUNE, not a movie.** The previous track read as
  an *end-credits scroller* — because earlier briefs chased "gravitas" with an
  orchestra (choir, brass, timpani, "hybrid film score"). That is the culprit.
  Get the weight the **demoscene** way instead: stacked supersaws + hoover stabs
  + deep sub under **fast arpeggios**, a punchy four-on-floor / breakbeat, glassy
  FM-bell leads, and big synth drops — the Future Crew *Second Reality* / Purple
  Motion / Skaven / Necros tracker lineage. Energetic and uplifting, **not slow,
  not cinematic, no orchestra**.

### Demo arc to score — 7 scenes, single pass (~2:30)
**Important — Suno takes no timing input.** There is no seconds field; BPM is
approximate and section lengths are *emergent*, not specified. So the lengths
below are **demo-side targets**, NOT something the prompt forces Suno to hit.
What the prompt *does* control is the **count, order and character** of sections
(the two-drop shape). The only lever that nudges length is **bar counts**, which
the model honours far better than seconds (at ~140 BPM a 4/4 bar ≈ 1.7 s) — so
the `bars` column is a *hint*, still approximate. **Reconcile after generation:**
run `analyze_music.py`, then fit by retiming `timeline.c` to the real boundaries
(± subtle Warp-Marker nudges / a small trim). Co-design just shrinks that fit.

| # | scene | bars (~s) | section tag | musical job |
|---|---|---|---|---|
| 1 | **title** | 8 (~14s) | `[Intro]` | sparse: glassy FM bell states the MAIN THEME; soft pad + deep sub; no drums; anticipation |
| 2 | **rotozoomer** | 12 (~21s) | `[Build]` | rising arpeggio + snare roll + filter-opening riser — tension climbing to drop 1 |
| 3 | **chrome A** | 14 (~24s) | `[Drop]` | DROP 1: four-on-floor + punchy bass; theme on bright supersaw; hoover stabs + arp fireworks |
| 4 | **mercury plain** | 12 (~21s) | `[Groove]` | driving breakbeat + funky bass; a SOARING second lead over fast arps |
| 5 | **liquid metal** | 12 (~21s) | `[Breakdown]` | strip to filtered pad + lone-bell reprise of the theme; tension rebuilds late |
| 6 | **chrome B** | 14 (~24s) | `[Final Drop]` | PEAK: KEY CHANGE UP, double-time arps, stacked saw leads + hoovers, biggest moment |
| 7 | **credits** | 8 (~14s) | `[Outro]` | resolve: theme once more on a lone bright lead, filter close, quick fade; `[End]` |

(~80 bars ≈ 2:20 at 140 BPM. Treat as aim, not contract.)

### Two gotchas to avoid (learned the hard way)
- **It sang the descriptions.** Suno *sings any Lyrics-box text not inside
  `[brackets]`*. Keep the Lyrics box **bracket-only**; rich detail goes in the
  **Style** field (never sung). Belt-and-braces: flip the **Instrumental** toggle.
- **It ran 8 minutes.** Too many / too-verbose sections make it sprawl. Keep to
  these **7 tags**, state the length in Style, end with `[End]`, and **don't hit
  Extend** — pick the ~2:30 take. If a take runs long, drop a section tag and
  regenerate.

### Style Prompt (paste into Suno's *Style* field — carries all the detail)
> Energetic oldskool demoscene anthem, instrumental, about 2 minutes 30 seconds,
> ~140 BPM, minor/Dorian key — uplifting and driving. Bright supersaw and PWM
> square leads, glassy FM bells carrying a strong recurring melody, fast
> hands-in-the-air arpeggios, hoover stabs, a funky punchy bassline, four-on-the-
> floor kick with breakbeat fills, gated reverb, chip blips. Builds from a sparse
> intro to euphoric synth drops with arpeggio fireworks, a stripped breakdown,
> then a key-change-up final drop. The weight comes from STACKED SAWS and deep
> sub, not an orchestra. Future Crew Second Reality / Purple Motion / Skaven /
> Necros tracker energy. No vocals, no orchestra, no choir, no brass, no
> cinematic film-score, no slow ballad.

### Structure Prompt (paste into the *Lyrics* field — bracket-only, nothing sung)
*(Every line is a `[bracketed]` tag, so Suno reads it as a section direction and
sings nothing. The detail inside each tag is the v5.5 per-section override —
this is what scores each scene. Also flip the **Instrumental** toggle on.)*

*(The `N bars` hints are the only lever that nudges section length — the model
honours bars far better than seconds, though still approximately.)*

```text
[Instrumental]
[Intro - 8 bars: sparse, glassy FM bell states the theme, soft pad, deep sub, no drums]
[Build - 12 bars: rising arpeggio, snare roll, riser, filter opening into the drop]
[Drop - 14 bars: four-on-the-floor + punchy bass, theme on bright supersaw, hoover stabs, arp fireworks]
[Groove - 12 bars: driving breakbeat, funky bassline, soaring second lead over fast arps]
[Breakdown - 12 bars: strip to filtered pad + lone bell reprise, gentle, tension rebuilds late]
[Final Drop - 14 bars: key change UP, double-time arps, stacked saw leads + hoovers, biggest moment]
[Outro - 8 bars: theme once more on a lone bright lead, filter close, quick fade]
[End]
```

*(If a vocal still sneaks in: set the "exclude styles" box to
`vocals, lyrics, singing, vocal chops` and regenerate.)*

### After generation — lock the sync (Suno Studio)
1. Open the take in **Studio**, auto-set **Warp Markers** on transients.
2. Nudge the two drop transients so they land on tidy bar lines; **Quantise** to
   tighten the groove (fixes the "monotone/drifting" feel without regenerating).
3. Export, then run `tools/analyze_music.py` to read the real drop times and
   snap `timeline.c` to them. Keep edits subtle — big moves = regenerate instead.

### Music Conversion
Convert the output audio using:
1. `ffmpeg -i input.mp3 -ar 22050 -ac 1 -c:a pcm_s16le music.wav`
2. `tools/qoaconv_s16.exe music.wav music.qoa`
3. Link into firmware via `music_qoa.S`.
