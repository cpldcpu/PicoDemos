# SUSTAIN — Asset Brief

Assets for **16_Sustain**. Drop delivered files in this folder (`16_Sustain/sustain/assets/`),
then `python tools/pack_assets.py` writes `_packed/*.bin` + `assets.S` + `assets.h`.

**The demo has no cuts.** Every asset is on screen *continuously* and morphs into
the next thing rather than being replaced. Two consequences that shape every
request below:

1. **Everything must cross-lerp.** Assets come in **pairs** (cold/hot) that the
   renderer blends between over the run. A pair must be the *same kind of thing*
   at the same scale and framing — only material and colour differ. If the two
   halves of a pair have different feature sizes, the blend reads as a
   dissolve, which is the one thing this demo may not do.
2. **There is no title card.** The wordmark appears *in the world* (embossed
   into the surface), not on a backdrop. See #8.

---

## Delivery checklist

| # | File | Size / format | Flash | Status |
|---|------|---------------|-------|--------|
| 1 | `surface_cold.png` | 128×128 RGB565, seamless 4-edge | 32 KB | ☑ delivered (keep — this is the anchor) |
| 2 | `surface_hot.png` | 128×128 RGB565, seamless 4-edge | 32 KB | **☐ REISSUE — see §"Surface pair reissue"** |
| 3 | `relief_soft.png` | 256×256 **grayscale**, seamless 4-edge | 64 KB | ☑ delivered |
| 4 | `relief_hard.png` | 256×256 **grayscale**, seamless 4-edge | 64 KB | ☑ delivered |
| 5 | `sky_pano.png` | 1024×128 RGB565, **horizontally** seamless | 256 KB | ☑ delivered |
| 6 | `matcap_cold.png` | 128×128 RGB565 | 32 KB | ☑ delivered |
| 7 | `matcap_warm.png` | 128×128 RGB565 | 32 KB | ☑ delivered |
| 8 | `sustain_logo.png` | 1024×256 → packed 320×80, **white on pure black** | 51 KB | ☑ delivered |
| 9 | `music.mp3` | instrumental, Suno | ~1.6 MB | ☑ delivered (4:49) |
| 10 | `wall_cold.png` | 128×128 RGB565, seamless 4-edge | 32 KB | ☑ delivered |
| 11 | `wall_hot.png` | 128×128 RGB565, seamless 4-edge | 32 KB | ☑ delivered |

All checked assets pass `tools/check_tiling.py` and are packed (650 KB total).

**Reuse, do not regenerate:** the LATENT group logo — copy
`15_Quicksilver/quicksilver/assets/latent_logo.png` across unchanged. It's the
group mark; it should look identical between productions.

**Do NOT generate** (these are procedural or tool-built, art time would be wasted):
particle sprites / glow kernels (built at boot), the 3D mesh for the 1:34 solid
(`tools/make_meshes.c`), any font (`font8x8.h`), gradients, noise, and every
transition — there are no transition assets because there are no transitions.

Total art ≈ 560 KB flash, well inside the 4 MB budget.

---

## Global rules — all learned the hard way on QUICKSILVER

- **Author at final size.** Deliver at the pixel size in the table, not larger.
  The runtime copies textures into SRAM tiles at exactly these dimensions; a
  256×256 source for a 128×128 slot wastes 3/4 of the flash and the loader just
  throws it away. (QS shipped four 128×128 textures for this reason.)
- **Seamless means provably seamless.** QS's sky came back with a visible
  vertical seam and had to be reissued. For 4-edge tiles, left↔right **and**
  top↔bottom must match exactly. For the sky, left and right edges must be
  identical pixels.
- **High contrast** — output is a 5-bit-per-channel DAC. Subtle gradients band
  visibly. Push contrast harder than looks right on a monitor.
- **Height maps are geometry, not pictures.** Grayscale only, white = raised,
  black = recessed. **No baked lighting, no shadows, no colour.** QS's first bump
  map came back as crumpled foil — sharp fine creases that turned into sparkly
  grain under specular light. Large, soft, rounded features.
- **No text, no watermark, no border, no frame** — on everything.
- **Logos on PURE BLACK** for keying.

---

## 1–2. Surface material pair — the substance of the whole world

This is the single most important pair. The sea, the canyon walls and the tunnel
interior are all **the same material** — that's what sells "one continuous
world" rather than three locations. Only the domain warp and the palette change.
The renderer cross-lerps `cold` → `hot` → `cold` across the run, mirroring the
arc's departure → transformation → return.

**Both must have the same feature scale** (roughly 6–8 features across the tile).
That is the hard requirement; if one is busy and the other is smooth, the blend
reads as a dissolve.

**#1 `surface_cold.png`** — the opening sea and the closing return.
> Seamless tileable texture of a dark liquid glass surface, smooth flowing
> ripples, deep blue-black with pale cyan-white specular sheen, cold dawn light,
> large soft features about 7 across the tile, high contrast, smooth (NOT busy),
> tiles perfectly on all four edges (left↔right AND top↔bottom), no seams, no
> text, no border.

**#2 `surface_hot.png`** — the same substance at the arc's peak.
> Seamless tileable texture of molten metal, the SAME smooth flowing ripple
> structure and the SAME feature scale as a calm liquid surface (about 7 features
> across the tile), but glowing hot — deep ember red-black with orange-gold
> incandescent highlights in the channels. Smooth (NOT busy), high contrast,
> tiles perfectly on all four edges, no seams, no text, no border.

---

## 3–4. Relief pair — the shape of the world

Drives actual displacement, not shading. The renderer lerps `soft` → `hard` as
the sea steepens into canyon walls, and again as the canyon closes into a tunnel.

**#3 `relief_soft.png`** — ocean swell (0:00 and the 2:40 return).
> Seamless tileable GRAYSCALE height map of a smooth ocean swell — large soft
> rounded bulges and broad valleys, gentle organic curves, like slow deep-water
> waves. NO sharp creases, NO fine wrinkles, NO noise, NO grain. White = crest,
> black = trough. Large features, about 5 across the image. Smooth rounded
> gradients, high contrast. Pure grayscale height field, NO baked lighting, NO
> shadows, NO colour, no text, no border. Tiles perfectly on all four edges.

**#4 `relief_hard.png`** — canyon crags and tunnel grooves (0:22 – 1:10).
> Seamless tileable GRAYSCALE height map of eroded rock strata — strong
> horizontal banded layers with deep carved channels between them, like a slot
> canyon wall. Bold medium-scale features about 8 across the image, crisp ridge
> edges but ROUNDED tops (no razor creases, no fine speckle, no noise). White =
> ridge, black = channel. High contrast. Pure grayscale height field, NO baked
> lighting, NO shadows, NO colour, no text, no border. Tiles perfectly on all
> four edges.

---

## 5. Sky panorama

Seen at 0:00 and again at 2:40 from the opposite heading — it is the anchor of
the return, so it must be recognisable. **One asset only:** the hot mid-arc
version is produced at runtime by palette shift, which guarantees the closing
image rhymes with the opening.

> Equirectangular 360° panorama of a cold dawn sky over a still dark sea — deep
> blue-black at the top grading to pale ice-blue and a thin band of cold white
> light at the horizon, one small low sun just below the horizon line, smooth
> broad gradients, a few long flat cloud strata. Calm and vast. **SEAMLESS
> horizontal wrap — the left and right edges must be identical pixels so it
> tiles with no visible seam.** Wide 8:1 strip. No text, no border, no frame.

Deliver **1024×128**. (QS's first sky had a seam here — please check the wrap by
tiling it twice before delivering.)

---

## 6–7. Matcap pair — the chrome solid at 1:34

Orthographic reflection probes. Used on the condensed solid the camera orbits,
and for the sea's reflections. Same pair rule: identical framing and highlight
placement, only the lighting colour differs, so the lerp is invisible.

**#6 `matcap_cold.png`**
> A polished chrome reflection probe — an orthographic sphere filling a square
> frame edge to edge, COOL lighting: blue-white key light from upper left,
> steel-grey body, deep black shadows, one crisp specular streak, smooth
> gradients, high contrast, no text, no border.

**#7 `matcap_warm.png`**
> The SAME polished chrome reflection probe, same orthographic sphere filling the
> square frame edge to edge, same key light position from upper left, but WARM
> lighting: amber-gold key, copper rim light, bronze body, deep shadows, one hot
> white specular highlight, smooth gradients, high contrast, no text, no border.

---

## 8. SUSTAIN wordmark — embossed, not overlaid

There is no title screen. The word is **pressed into the surface of the world**
at ~0:10 — the sea's relief forms the letters, holds, and the swell washes them
out. So this asset is used as a **displacement stencil**, not as a picture.

That means: **flat white letters on pure black, no bevel, no gloss, no
reflection, no gradient.** Any shading baked into the PNG becomes wrong geometry.
The chrome look comes from the renderer lighting the displaced surface.

> The single word "SUSTAIN" in a bold wide condensed sans-serif, FLAT PURE WHITE
> letters on a PURE BLACK background, centred, generous letter spacing. Completely
> flat — no bevel, no gloss, no gradient, no shadow, no reflection, no texture,
> no outline. A clean solid silhouette only. Wide aspect 1024×256. No other
> objects, no border.

If you want to also try a rendered chrome version for comparison, deliver it
separately as `sustain_logo_chrome.png` — but the flat stencil is the one the
demo needs.

---

## 9. Music (Suno) — a track that also never cuts

**This brief is different from QUICKSILVER's, and the difference is the point.**
QS asked for two big drops and snapped the demo's cuts to them. SUSTAIN has no
cuts, so a track with hard section breaks will fight it: every full stop, every
drop into silence, every clean bar-line reset hands the viewer a boundary the
visuals are deliberately refusing to give.

### The one idea: a pedal tone that never stops

Ask for a **continuous held bass drone under the entire track** — one sustained
pedal note that persists through every section change. It is the musical form of
the demo's rule, it makes section boundaries physically impossible to hear as
breaks, and it is why the demo is called SUSTAIN.

### Shape

- **~3:00**, instrumental, ~124 BPM, minor.
- **Departure → transformation → return**, the same arc as the visuals: a theme
  stated plainly at the start, taken apart and transformed through the middle,
  and returned at ~2:40 in its original form but re-harmonised. **The music
  performs the return at the same moment the camera does.**
- **Energy tracks camera speed**, which is the pacing instrument in place of
  cuts: slow and wide at 0:00, accelerating from 0:46, peak intensity 1:10–1:34
  (the tunnel and the condensation), release from 1:58, calm at 2:40.
- **Sections must overlap, never break.** Each section should begin before the
  previous ends. No full stops, no silence, no drum fills that reset, no "drop"
  that cuts everything out for a bar.
- Demoscene, not cinematic — stacked saws and deep sub for weight, **not** an
  orchestra. (QS's first track read as end-credits because the brief chased
  "gravitas" with choir and brass. Don't repeat it.)

### Style field (paste into Suno's *Style* box — all detail goes here, never sung)

> Continuous instrumental demoscene track, about 3 minutes, ~124 BPM, minor key.
> ONE SUSTAINED BASS PEDAL DRONE RUNS UNDER THE ENTIRE TRACK AND NEVER STOPS.
> Everything else evolves smoothly over it — glassy FM bells state a clear
> recurring theme, stacked supersaw pads, slow filter sweeps, fast arpeggios that
> fade in and out rather than starting and stopping, deep sub, gated reverb
> drums that enter and leave gradually. Sections crossfade and overlap
> seamlessly — no hard breaks, no full stops, no silence, no drop-outs. Builds
> slowly from calm and wide, accelerates to an intense peak around two thirds
> through, then releases and returns to the opening theme re-harmonised. The
> weight comes from stacked saws and deep sub, not an orchestra. No vocals, no
> orchestra, no choir, no brass, no cinematic film score, no hard stops, no
> silence.

### Lyrics field (bracket-only — Suno **sings** anything not in brackets)

Also flip the **Instrumental** toggle on. Belt and braces.

```text
[Instrumental]
[Intro - 10 bars: sustained bass pedal enters alone, glassy FM bell states the theme over it, wide and calm, no drums]
[Rise - 12 bars: pedal continues, arpeggio fades in underneath, soft drums enter gradually, filter slowly opening]
[Drive - 14 bars: pedal continues, four-on-the-floor, theme on stacked supersaw, fast arps, accelerating]
[Peak - 16 bars: pedal continues, most intense, double-time arps, theme transformed and re-harmonised, huge]
[Release - 12 bars: pedal continues, drums fade out gradually, texture thins, filter closing slowly]
[Return - 12 bars: pedal continues, the opening theme returns on the lone bell, re-harmonised, calm and wide again]
[Outro - 6 bars: pedal finally resolves and fades]
[End]
```

### Gotchas (both cost QUICKSILVER a regeneration)

- **It sings the descriptions.** Any Lyrics-box text not inside `[brackets]` gets
  sung. Keep it bracket-only.
- **It runs long.** QS's first take came back at 8 minutes. Keep to these 7 tags,
  state the length in Style, end with `[End]`, and **don't press Extend** — pick
  a take near 3:00 and regenerate rather than trimming.
- **Listen for breaks before accepting a take.** The single disqualifying flaw is
  a hard stop or a bar of silence anywhere. If the pedal tone drops out at a
  section change, that take is unusable no matter how good it sounds.

### Conversion

```sh
ffmpeg -i music.mp3 -ar 22050 -ac 1 -c:a pcm_s16le music.wav
tools/qoaconv_s16.exe music.wav music.qoa
# linked into firmware via music_qoa.S
```

---

## 10–11. Wall pair — the same rock, seen along a different axis

**Why this exists.** The surface pair (#1/#2) is sampled top-down, by world
`x`/`z`. That is correct for a sea and correct for a canyon floor. But the
canyon *walls* and the tunnel *sides* are near-vertical, and a top-down
projection stretches the texture into long vertical streaks across them — it's
the most visible defect in the current build. The renderer is moving to
triplanar sampling, which blends in a second texture keyed on **height** for
steep surfaces.

**This is not a second material.** It's the *same substance seen along a
different axis*, which is exactly what triplanar mapping means — so the demo's
"one continuous world" claim is intact. Strata read as bands on a wall and as
contour rings on a floor; those are two views of one rock, not two rocks. Keep
that in mind while generating: these should look like they belong to the same
place as #1/#2, just cut vertically.

**Composition matters here.** Because these are applied to vertical faces, the
strata must run **horizontally across the image** — they become level bands up
the wall. Vertical structure in this texture would reintroduce exactly the
streaking it exists to fix.

The pair rule from the top of this file applies with full force: #10 and #11
must have the **same feature scale and the same band positions**, differing
only in material and colour temperature, because the renderer cross-lerps them
across the run.

**#10 `wall_cold.png`** — canyon walls and tunnel sides, cold half of the run.
> Seamless tileable texture of an eroded rock wall seen face-on, with strong
> HORIZONTAL strata — level bands of sedimentary layers running left to right
> across the image, about 7 bands from top to bottom, with narrow darker
> channels eroded between them. Cold palette: blue-black stone with pale
> steel-grey and faint cyan mineral highlights catching the light along the
> upper edge of each band. Crisp band edges but rounded tops, no razor creases,
> no fine speckle, no noise. High contrast. Tiles perfectly on all four edges
> (left↔right AND top↔bottom). No text, no watermark, no border, no frame.

**#11 `wall_hot.png`** — the same wall at the arc's peak.
> The SAME eroded rock wall with the SAME horizontal strata at the SAME scale
> and the SAME band positions (about 7 bands top to bottom, narrow eroded
> channels between them), but glowing hot: deep ember red-black stone with
> molten orange-gold light in the eroded channels, as if heat is rising through
> the seams. Same crisp-but-rounded band edges, no noise, no speckle. High
> contrast. Tiles perfectly on all four edges. No text, no border.

---

## Surface pair reissue — `surface_hot` only

**Everything else in the set now passes.** `tools/check_pair.py` measures how
well a cold/hot pair will cross-lerp; the wall v2 delivery scores **r = 0.977**
and the matcaps **r = 0.990**. The surface pair scores **r = 0.009** — it is the
last mismatched pair.

**Keep `surface_cold.png` — do not regenerate it.** It is the anchor: the demo
opens on it and returns to it at the end, and that return is the whole
structure. `surface_hot` is what needs to match *it*.

**The specific problem is feature SCALE, not just placement:**

| | `surface_cold` (keep) | `surface_hot` (reissue) |
|---|---|---|
| features per row | **7** | 13 — too busy |
| features per column | **1** | 9 — far too busy |
| character | broad, smooth, flowing ripples | fine tangled filaments |

The hot texture is roughly twice the spatial frequency of the cold one. Blended,
they beat against each other instead of transforming.

> Seamless tileable texture of a molten metal surface with BROAD, SMOOTH,
> SLOW-FLOWING ripples — large sweeping waves, about **7 across the tile and
> only 1–2 from top to bottom**, so the structure is wide and calm, NOT a busy
> tangle of thin filaments. Deep ember red-black base with warm orange-gold
> incandescent light pooling in the wide channels between the ripples. Smooth
> and open, generous spacing, NO fine detail, NO thin bright threads, NO noise.
> High contrast between the dark ridges and the glowing channels. Tiles
> perfectly on all four edges (left↔right AND top↔bottom). No text, no border.

**Acceptance test** — run before delivering, it takes a second:

```sh
python tools/check_pair.py assets/surface_cold.png assets/surface_hot.png
```

Target: **structure correlation ≥ 0.85** and **matching band counts** (7 rows,
1–2 columns). The wall v2 pair is the worked example of what passing looks like.

*(A note on why this is worth one more pass: the renderer already compensates
with a height-keyed dissolve so the two are never both half-visible, which
makes the current pair usable. A properly matched pair upgrades that from a
workaround to an enhancement — the dissolve then adds direction to a transition
that already reads correctly, instead of covering for one that doesn't.)*

---

## Conventions the build has since pinned down

Discovered while wiring the delivered assets in. Please preserve these on any
reissue — each one cost a debugging pass.

- **`sky_pano` puts its horizon glow at the image's VERTICAL CENTRE.** The
  renderer maps screen-horizon to row `H/2` and the top of frame to row `0`, so
  the upper half is sky and the lower half is the below-horizon fill. An earlier
  mapping assumption put the glow halfway up the sky and left the true horizon
  black, which pushed the whole tunnel section under the black-frame threshold
  in `tools/cut_detect.py`. If the sky is regenerated, **keep the bright band
  centred vertically.**
- **The hot sky is a runtime grade of the cold one, not a separate asset.** Do
  not generate a hot sky. The closing vista has to be recognisably the opening
  one — two independently generated skies would not rhyme, and the return is
  the demo's whole structure.
- **Height maps are geometry.** Worth repeating because it now has teeth: the
  relief pair drives real displacement and is sampled bilinearly. Baked
  lighting or shadows in a height map become *wrong shape*, not just wrong
  shading.
- **A wrap ratio near 1.0 is the goal, not 0.0.** `check_tiling.py` compares the
  step across the wrap edge to the typical step between interior lines. Real
  seamless art measures ~0.7–1.2 (the wrap is indistinguishable from ordinary
  variation) — the wall v2 pair does exactly this. A ratio of *exactly* 0.0
  means the texture is mathematically periodic, i.e. procedurally synthesised
  rather than generated art. Both work, but 0.0 tells you which you're holding.
- **Pairs are measurable — check before delivering.** `check_pair.py` scores how
  a cold/hot pair will cross-lerp. ≥0.85 is a recolour, <0.60 is a crossfade.
  Height-map pairs are exempt (pass `--geometry`): averaging two height fields
  yields a valid height field, so they are *supposed* to differ.
- **Contrast is cheap and banding is expensive.** Output is 5 bits per channel.
  The renderer now applies ordered Bayer dithering, which handles gradients
  well, but it cannot invent detail that isn't in the source. Midtone contrast
  in the surface/wall textures is what makes the terrain read as material
  rather than as fog.

---

## Priority

If you generate in stages, this is the order that unblocks me fastest:

Everything except the wall pair is delivered, so the queue is now just:

1. **#10 `wall_cold.png`** — unblocks the triplanar fix for the canyon and
   tunnel, which is the most visible remaining defect in the build.
2. **#11 `wall_hot.png`** — needed later than #10 (the hot palette doesn't
   arrive until the arc's peak), but generate it in the same sitting as #10
   while the band layout is in front of you. Matching the two afterwards from
   memory is what breaks the pair rule.

Nothing else is blocking. The particle and mesh sections of the arc are
procedural and tool-built; there is no art request coming for them.
