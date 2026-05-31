# 14_Origami — "ORIGAMI"

> An original RP2350 demoscene production. **LLM author: Claude Opus 4.8.**
> A folded-paper world — the deliberate **technical and emotional opposite** of
> [`13_Singularity`](../13_Singularity/IMPLEMENTATION.md): warm, tactile, hand-made, and built on
> **flat-shaded filled-polygon 3D** — a rendering class no prior demo in this repo has done (the
> others did wireframe, env-mapping, raymarching, and per-pixel/LUT effects, but never solid-poly
> rasterisation with depth ordering and per-face shading).

---

## Status: BUILT & VERIFIED

| Metric | Value |
|--------|-------|
| Target | RP2350 (Pico 2), 300 MHz @ 1.20 V, M33 FPU, `pico_scanvideo` 320×240 @ 60 Hz |
| Mode | **MODE_HIRES** throughout — 320×240 **RGB565 truecolor**, **antialiased** polygon & shadow edges |
| Flash (`.text`) | **1.28 MB / 4 MB** (≈ 80 KB code + 1.20 MB `music.qoa`) |
| SRAM (BSS) | **434 KB / 520 KB** (≈ 76 KB headroom; engine working set is ~31 KB; the truecolor framebuffer is free — `vga.c` always sizes its arena for the largest mode) |
| Soundtrack | *"Marimba Seedbox"* (Suno 4.5, prompted by Claude) → `music.qoa`, mono 22050 Hz, 2:14.96 |
| Output | `origami_vga_rp2350.uf2` (prebuilt, checked in) |

Built with the SINGULARITY engine (scene runner, `pico_scanvideo` VGA backend, QOA audio, SDL host)
plus one new module — `effects/poly3d.c`, the flat-shaded polygon renderer.

---

## Logline & aesthetic

Everything is folded paper. A creased sheet opens into a title; a paper plane banks through a
pastel sky of cut-paper clouds; a flat square folds into an origami crane and unfolds again; a page
turns and a paper city pops up; a Miura-ori field ripples in a fold wave on the beat; and the world
bursts into confetti that settles over the credits. **Palette:** warm paper whites and creams, soft
pastels (sky blue, coral, sage, mustard), with soft drop shadows that sell the third dimension.
**Feel:** serene, playful, intimate — the calm after SINGULARITY's cosmic intensity.

**Why it wins:** the headline technique is a real-time **flat-shaded filled-polygon 3D engine** —
Rodrigues hinge **folding** of crease lines, perspective projection, back-face cull, **painter's-
algorithm** depth sort, **per-face Lambert** shade, and **antialiased** convex span fill, all in
320×240 RGB565 truecolor. Paper is modelled as creased planar facets; "folding" animates the dihedral
fold angles. It is a rendering class genuinely new to this repo, and runs in truecolor — so polygon
edges are coverage-antialiased and drop shadows are true alpha-darkening of the ground (smooth, no
banding, no bright fringe). The truecolor framebuffer costs nothing: `vga.c` already allocates its
arena at the largest (hires) size regardless of mode.

---

## The engine — `effects/poly3d.{c,h}`

A self-contained renderer (depends only on `vga.h` + `rgb565.h` + `<math.h>`/`<string.h>`; builds
identically on device and SDL host) that writes the **MODE_HIRES RGB565** back buffer. Its per-frame
working set (~31 KB) lives in file-static BSS, so scenes are free to keep their own animated geometry
in the `g_scratch` union.

- **Models** are static flash tables: `p3_vec3 verts[]`, `p3_face{i0..i3,material,flags}[]` (tri when
  `i3==P3_NO_VERT`, else convex quad; `flags` bit0 = double-sided — paper has two lit sides), and
  `p3_crease{a,b, moves[], angle_src}[]` hinges.
- **Folding** (`fold_model`): each frame copy model verts → world, then apply creases **in array
  order**, each rotating its `moves` vertex set about the *live* world axis `(W[a],W[b])` by
  `fold_angle[angle_src]` via Rodrigues. A child crease listed after its parent (with a subset
  `moves`) folds *within* the already-folded parent → hierarchical folds with no scene graph.
- **Transform** composes the model placement (`yaw/pitch/roll` + offset) with the camera into one 3×3
  + translate; project with `inv = focal/z; sx = cx + x*inv; sy = cy - y*inv`; near-clip `z < 6`.
- **Cull** by whether the face normal faces the camera — `dot(n, view-space centroid) > 0` means it
  points away (works for any orientation, incl. near-horizontal ground planes); double-sided faces are
  exempt. **Lambert** from the camera-facing normal (one `1/sqrtf` per visible face) gives a brightness
  in `[ambient, 1]` that scales the material's RGB → a per-face shaded colour. **Painter** insertion-
  sort on average view-Z (near-O(n) by temporal coherence).
- **Antialiased span fill** (`p3_fill_convex`): per scanline the two convex edge crossings give float
  `[xl,xr]`; interior pixels are written solid, and the fractional left/right boundary pixels are
  **coverage-blended** into the background. Half-pixel sampled. This smooths every non-horizontal
  edge — the diagonal plane wings, crane facets, Miura cells, building sides — at ~2 extra blends per
  scanline.
- **Soft drop shadows** (`p3_render_shadow`): project facets onto a ground plane along the light
  direction, then **alpha-darken** whatever lies beneath (multiply the covered pixels) with a two-pass
  penumbra — a scaled-up soft halo, then a darker core. Because it darkens the real ground rather than
  picking a palette slot, it tints any surface correctly and never shows a bright fringe.

### Materials (set per scene by `og_materials()`)

No palette: each of the 6 paper materials (white, cream, sky-blue, coral, sage, mustard; plus 6 free
accent slots) is just a fully-lit RGB triple set with `p3_set_material(m, r,g,b)`. The engine
multiplies it by the per-face Lambert brightness at fill time, producing a smoothly shaded RGB565
colour — flat per facet, but with no quantisation banding. Backgrounds (sky/endcard gradients) are
drawn in truecolor by the `og_*` helpers in `effects/origami_fx.h`. Captions use a crisp (nearest)
`font8x8` with a soft drop shadow; the big **"ORIGAMI" wordmark** is a vector logotype — angular
geometric strokes thickened into quads and drawn through the engine's antialiased fill (`og_logo`),
so the hero title is a clean cut-paper logo rather than an enlarged bitmap.

---

## Scene timeline (beat-synced to the track)

`tools/analyze_music.py` on `Marimba Seedbox.mp3`: **117.5 BPM**, beat = 511 ms, bar = 2.04 s;
structural segments at 10.4 / 25.8 / 42.8 / 122.8 s; strong onsets at 47.4 / 54.5 / 71.98 / 77.1 /
113.0 / 132.0 s. Boundaries (`timeline.c`) are snapped to these.

| # | Scene | Window | Technique |
|---|-------|--------|-----------|
| 0 | **Title** (`title.c`) | 0:00–0:10.4 | "ORIGAMI" — a 6-panel accordion sheet unfolds (animated creases), then the title stamps on, over a sky gradient |
| 1 | **Paper-plane flight** (`plane.c`) | 0:10.4–0:25.8 | A folded dart banks through cut-paper clouds over a paper ground plane, casting a soft drop shadow |
| 2 | **Crane fold/unfold** (`crane.c`) | 0:25.8–0:54.5 | A flat square folds — wings, neck, beak, tail in a staged crease sequence — into a crane, turns, then unfolds. Centrepiece. |
| 3 | **Pop-up paper city** (`city.c`) | 0:54.5–1:17.1 | 12 facades hinge upright from a page (staggered front-to-back), beat-sway, cast shadows; slow orbit |
| 4 | **Miura-ori wave** (`miura.c`) | 1:17.1–1:53.0 | An 18×11 Miura field ripples in a travelling fold wave; facet bands flip light/shadow; amplitude pulses on the beat. Climax. |
| 5 | **Confetti credits** (`credits.c`) | 1:53.0–2:14.96 | 150 tumbling paper quads (depth-sorted, flat-shaded) rain over a warm endcard while the credits scroll |

All scenes hard-cut on a beat (mode is constant MODE_320, so cuts are glitch-free).

---

## Per-frame budget (300 MHz, ~14 ms/frame for 60 fps)

The densest scene is the Miura field: 228 vertex transforms + 198-face cull/shade/sort + a full-screen
sky gradient + ~80 K filled pixels. Fold (~24 flops/vert), transform (~20 flops/vert), shade (1
`sqrtf`/face), insertion sort (≤256, near-sorted), and the truecolor span fill (16-bit interior writes
+ ~2 coverage blends per scanline edge) all land comfortably inside budget; estimated ~4–6 ms. No
scene needs the 30 fps fallback.

### Memory

`poly3d.c` working set (BSS): `g_world/g_view/g_folded` (640 verts ×3 ×12 B ≈ 23 KB) + screen/flag/
sort arrays ≈ 31 KB total. Per-scene animated geometry (Miura grid, confetti quads + physics) aliases
the 76 800-byte `bg_cache` member of `g_scratch` (`scene_scratch.h`) → zero extra BSS.

---

## Build

**Device (RP2350):**
```powershell
$env:PICO_SDK_PATH    = "C:\path\to\pico-sdk"
$env:PICO_EXTRAS_PATH = "C:\path\to\pico-extras"
cd origami
cmake -B build_rp2350 -G "MinGW Makefiles" -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s
cmake --build build_rp2350 -j        # -> build_rp2350/origami.uf2
```
Flash: hold **BOOTSEL**, plug in the Pico 2, drag `origami.uf2` onto the `RPI-RP2` drive.

**Desktop preview (SDL2, MSYS2 UCRT64):**
```bash
cd origami/host && make && ./origami.exe      # SPACE next scene, LEFT prev, R restart, S screenshot
```

**Soundtrack pipeline** (re-run only if the track changes):
```bash
ffmpeg -i "assets/Marimba Seedbox.mp3" -ac 1 -ar 22050 -f s16le music.raw
./tools/qoaconv_s16.exe music.raw music.qoa 22050 1     # -> music.qoa (incbin'd by music_qoa.S)
python tools/analyze_music.py "assets/Marimba Seedbox.mp3" -k 6   # beat/segment analysis
```

Backgrounds are **procedural** (sky/ground gradients, cast-shadow ramps) for a cohesive flat-paper
look; the provided `assets/*.png` (paper sky, endcard, washi) are reference art and are not blitted.
Models (plane, crane, city, Miura, confetti) are authored as code tables, not images.

---

## Verification

- **Host / headless:** each scene was rendered at 60 fps across its full duration via a headless PPM
  harness (and the SDL host builds and runs) — folding, Lambert shading, painter depth order (no
  faces drawing through each other), cast shadows, beat-sync, confetti physics, and credits scroll
  all confirmed; no degeneration over full scene lengths.
- **Device:** builds clean for `rp2350-arm-s`; `.text` 1.28 MB / 4 MB, BSS 434 KB / 520 KB.
- **Engine unit-check:** a cube renders with correct back-face cull (3 faces), per-face Lambert, and
  crack-free span fill.
