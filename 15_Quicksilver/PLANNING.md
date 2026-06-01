# 15_Quicksilver — A liquid-chrome demo built on the RP2350 Interpolator

## Context

This repo (`d:\Toyprojects\PicoDemos`) is a series of LLM-assisted demoscene
productions for the RP2350 (Pico 2). Demos 13_Singularity and 14_Origami
established a mature dual-target engine: effects compile **byte-identically**
on the RP2350 firmware and on an SDL host preview, sharing `scene.c`/`vga.c`/
`audio_qoa.c` and differing only in swapped `main`/`vga`/`audio` files.

**The winning angle.** Demo 13's design notes explicitly reserved the RP2350
**SIO interpolator** (INTERP0/INTERP1) for a Mode-7 disk — but it was *never
actually implemented*. `hardware_interp` is even already linked in 13's
CMakeLists with a "reserved" comment. **No demo in this repo has ever used the
interpolator.** Demo 15 makes it the hardware hero: every scene is driven by
the interpolator's affine address-generation, BLEND (hardware bilinear lerp),
and CLAMP units, cycle-counted on bare metal — and, because the host has no
such hardware, we ship a **faithful software emulator** of the interpolator so
the same effect code previews on PC.

**Identity: QUICKSILVER** (codename MERCURY) — liquid chrome / mercury. The
interpolator's signatures (smooth bilinear blending, rotozoom, chrome
environment-mapping) *are* the visual language: reflective, molten, flowing
metal. Full 5–6 scene production with strong interp-powered transitions
between every scene. Music from Suno 4.5, 2D art from nano banana.

Target dir: `15_Quicksilver/quicksilver/` (mirroring `14_Origami/origami/`).

---

## Architecture overview

- **Reuse the engine verbatim** from 13_Singularity: `vga.c/.h`, `rgb565.h`
  (custom PIO-native bit order — always `rgb565_pack()`), `scene.c/.h`,
  `audio_qoa.c`, `audio.h`, `qoa.h`, `font8x8.h`, `main.c`, `pico_sdk_import.cmake`,
  `pico_extras_import.cmake`, and `host/{main_host.c,vga_sdl.c,audio_sdl.c}`.
  Copy `effects/poly3d.{c,h}` from 14_Origami for flat geometry / title bars.
- **All scenes run in MODE_HIRES** (320×240 RGB565 truecolor) for banding-free
  chrome — the same mode `lensing.c` and `poly3d.c` already prove fits 60 fps.
- **Master clock** is `audio_now_ms()`; scene boundaries in a rewritten
  `timeline.c` snapped to beats via `tools/analyze_music.py`.
- **Interpolator is per-core.** Core-0 effects own `interp0`+`interp1` freely
  (core-1 scanvideo doesn't touch them). The one beam-raced scene runs its
  inner loop on core-1, which then owns its own uncontended interp pair.

### The single source rule for interpolator code
Effects include **one** header, `interp_compat.h`, and use **only** the SDK
*function* API (never raw `interp->peek[]` struct access). That header:

```c
#ifdef HOST_BUILD
  #include "interp_emu.h"      /* our software interp + our interp_* functions */
#else
  #include "hardware/interp.h" /* real RP2350 registers */
#endif
```

This keeps the "no `#ifdef` in effects" convention and lets the host intercept
every interpolator operation. **On device it is raw native hardware** — no
emulation, exactly as required.

---

## Deliverable 1 — Host interpolator emulator (`interp_emu.{h,c}`)

The SDK's `interp_peek_lane_result()` reads `interp->peek[lane]`, a
*side-effecting memory-mapped register* that recomputes the datapath live; a
plain C struct cannot reproduce a load-with-side-effects. So on host we **do
not include the SDK header** — we define our own `interp_hw_t` (`accum[2]`,
`base[3]`, `ctrl[2]`), our own `interp0`/`interp1`, and our own function bodies
that *compute* results inside `peek`/`pop`. The `interp_config_set_*` setters
and the ctrl bit positions are copied verbatim from the SDK header (pure
bit-math) so config words are bit-identical.

**Datapath the emulator must reproduce exactly** (the fidelity-critical part):
- Per lane: input accum = cross_input ? `accum[1-L]` : `accum[L]`.
- **SHIFT is a right ROTATE** on RP2350 (not a logical shift), then mask bits
  `[MASK_LSB..MASK_MSB]`, then sign-extend from `MASK_MSB` if SIGNED.
- Lane result = `base[L] + (add_raw ? input_accum : shift_mask_value)`; ADD_RAW
  bypasses shift+mask for the **lane** result only — **FULL is unaffected** and
  cross_input still applies (mux is before the bypass). FORCE_MSB OR-ed into
  bits 29:28 of the value presented, not the internal datapath.
- **FULL** (`peek[2]`) = `base[2] + lane0_shiftmask + lane1_shiftmask`.
- **BLEND** (interp0 only): `lane1 = base0 + ((base1-base0)*alpha >> 8)`,
  alpha = 8 LSBs of lane1 shift+mask; `lane0` = alpha alone (no base0); FULL
  omits the lane1 term.
- **CLAMP** (interp1 lane0 only): `clamp(base0, shiftmask(accum0), base1)`.
- **POP** returns the value then writes **both** accumulators with the internal
  lane results (CROSS_RESULT routes the *other* lane's result into a lane).

Ship `tools/interp_selftest.c` with hand-computed vectors (and runnable on real
hardware) to diff emulator vs. documented algebra — cheap insurance against the
#1 risk (silent host/device divergence). `interp_emu.c` is compiled **only** by
the host Makefile, never by CMake.

---

## Deliverable 2 — Scenes (each maps to an interpolator feature)

| # | Scene | Interp feature | Notes |
|---|-------|----------------|-------|
| 0 | **Title** — "QUICKSILVER" chrome lettering over a bilinear-zoomed backdrop | address-gen tap | reuses rotozoom inner loop |
| 1 | **Rubber Rotozoomer** — fullscreen bilinear rotozoom with sine "rubber" warp + motionblur | affine address-gen; **beam-raced (no framebuffer)** | the SRAM-saving headline |
| 2 | **Mode-7 Mercury Plain** — infinite reflective ground to a horizon + sky panorama | per-scanline affine | classic interp use; CLAMP for horizon haze |
| 3 | **Chrome (centerpiece)** — multiple high-poly env-mapped objects in sequence | per-pixel spheremap address-gen + bilinear | the 195/95 flex; double-buffered |
| 4 | **Liquid Metal** — plasma + bilinear feedback (mercury droplets/trails) | **BLEND** (interp0) + address-gen (interp1) simultaneously | dual-interp showcase |
| 5 | **Credits** — chrome credits over a reflective floor | address-gen reflection | reuses env-map/rotozoom |

**Per-pixel inner-loop pattern** (scenes 1,2,3,5), texture 256×256 RGB565,
16.16 fixed UV, interp0 configured for Mode-7 address-gen (lane0=u shift16
mask→col, lane1=v shift→`v*W` row term, base2=texbase):
```c
addr = interp_peek_full_result(interp0);   /* &tex[(v>>16)*W + (u>>16)] */
/* 2x2 bilinear tap from addr, addr+2, addr+2W, addr+2W+2; lerp in C */
interp_add_accumulator(interp0, 0, du);
interp_add_accumulator(interp0, 1, dv);
```
256×256 + mask-to-8-bits gives **free wrap** for tiled ground/rotozoom.

### Transitions (explicitly requested — interp-powered, between *every* scene)
- **Chrome melt**: bilinear-blend the outgoing frame downward with increasing
  vertical warp (interp BLEND) — looks like the image liquefying into mercury.
- **Bilinear zoom-punch**: rotozoom the outgoing frame to infinity / incoming
  from zero (reuses scene-1 inner loop) — ties the whole demo together.
- **Mercury wipe**: a moving threshold on a blended noise field reveals the next
  scene with a reflective metallic edge.
Transitions live in a small `effects/transition.c` the scene runner cross-fades
through on boundaries (extend `scene.c`'s fade-to-black hook).

---

## Deliverable 3 — Chrome centerpiece: objects, rasterizer, build tool

- **New `effects/envmap3d.{c,h}`** (do *not* overload flat-shaded `poly3d.c`).
  Span-buffer rasterizer modeled on `02_Dawn/pico/texmap.c`, but interpolating
  **screen-space normals (nx,ny)** per-vertex (Gouraud) across each triangle;
  the reflected normal's xy indexes a real chrome spheremap (vs Dawn's
  procedural `env_sample`). Interp does the spheremap byte-address + bilinear.
  Reuse `poly3d.c`'s transform/project/backface/painter-sort scaffolding.
  Mark the hot span loop `__not_in_flash_func` (run from SRAM, predictable
  cycles). Estimated ~30–45 cy/px → a half-screen object ≈ 0.3 frame at 60 fps.
  **Write in C (-O2) first, measure via `arm-none-eabi-objdump -d`, only drop to
  point-sampling or hand-written ARMv8-M asm if the listing shows spills.**
- **Multiple objects** (per the request) generated by a new
  `tools/make_meshes.py` (follows the `make_lens_lut.py` bake→`.bin`+`.h`+`.S`
  incbin pattern, all checked in so a clone builds without Python):
  - subdivided **icosphere** (chrome ball / morphing blob),
  - **torus knot** (intricate geometric solid),
  - a **morph pair** (same-topology meshes) for a liquid morph,
  - optionally extruded **"MERCURY" logo** geometry.
  Emits `verts[]`, `idx[]`, packed `int8 normals[]`, counts in `.h`.
  The scene cycles through objects with chrome-melt/morph transitions between
  them. Per-vertex working arrays live in the `g_scratch` union (NOT static
  arrays); the spheremap texture (256×256 RGB565 = 128 KB) lives in **flash**
  via incbin (proven: 13 ships a 131 KB `disk_tex.bin`).

---

## Deliverable 4 — Beam-racing (scene 1 only)

Per the user: beam-race only where it makes sense (the hires zoomrotator);
**3D scenes stay double-buffered**.
- **Tier 1 (always built, ship-safe):** scene 1 renders to the framebuffer on
  core-0 like `lensing.c` — proven 60 fps, zero glitch risk.
- **Tier 2 (headline):** add a `MODE_HIRES_RACE` path where `render_scanline_*`
  on core-1 *computes* each pixel via core-1's own `interp0` from the texture —
  **no framebuffer, frees the 307 KB arena.** Core-0 precomputes the per-scanline
  affine basis into a small `race_params[240]` table so core-1 only runs the
  tight inner loop. Budget: ~5.0 M cy/frame ÷ 76 800 px ≈ 65 cy/px; inner loop
  ~25–40 cy/px → fits, with the 16-deep scanline pool (`PICO_SCANVIDEO_
  SCANLINE_BUFFER_COUNT=16`) absorbing transient over-budget lines.
- **Risk:** scanline underrun → tearing. Mitigation: tier-1 is the fallback (flip
  the mode); benchmark before committing tier-2 to the final cut.

---

## Deliverable 5 — 2D art (nano banana) prompt guide

Provide an `assets/PROMPTS.md` with these guidance + ready prompts. Art is
640px-ish PNGs, then `tools/pack_assets.py` → RGB565/8bpp `.bin` + incbin.

**General nano-banana guidance:** describe subject + style + lighting +
palette + framing; specify "seamless tileable" for textures; ask for "no text,
no watermark, no border"; request high contrast for the 5-bit DAC; for
spheremaps ask for a centered fisheye/orthographic sphere filling the frame.

Ready prompts (chrome/mercury identity):
- **Chrome spheremap (centerpiece reflection):** "A perfectly polished liquid
  chrome sphere reflection probe, orthographic, filling a square frame, studio
  HDR environment reflected in it — cool blue-white highlights, warm amber
  rim, deep blacks, dramatic specular streaks, smooth gradients, no text, no
  border, high contrast."
- **Mercury ground tile (Mode-7, seamless):** "Seamless tileable texture of
  rippling liquid mercury, top-down, soft caustic reflections, silver and
  steel-blue, subtle hexagonal flow, high contrast, no seams, no text."
- **Rotozoom texture (seamless):** "Seamless tileable ornate art-deco chrome
  filigree on black, radial symmetry, metallic silver with cyan and magenta
  iridescence, crisp edges, high contrast, no text, no border."
- **Sky panorama (equirectangular, 2:1):** "Equirectangular 360 panorama of a
  chrome-and-violet dusk sky over a mirror-flat liquid-metal sea, soft gradient
  horizon, distant glowing sun, smooth, no text, no border."
- **Title/endcard backdrop:** "Abstract molten silver and quicksilver droplets
  on a dark gradient, dramatic rim light, cinematic, lots of negative space in
  the center for a title, no text."

---

## Deliverable 6 — Music (Suno 4.5) prompts

Per Suno 4.5 guidance: write a **narrative, descriptive style prompt** (genre +
mood + tempo/energy + core instruments + production + progression), keep the
**style field concise** (~200 chars) and put **structure/metatags in the lyrics
field** in `[brackets]`. Document in `assets/PROMPTS.md`.

**Style prompt (primary):**
> "Sleek liquid-chrome synthwave with a driving 4/4 pulse, ~124 BPM. Glassy FM
> bells, metallic arpeggios, deep analog bass, gated reverb drums, shimmering
> reflective pads. Builds from a calm mercury-drip intro to a euphoric chrome
> drop, then a reflective outro."

**Lyrics field — ACTUAL sung lyrics** (metatags only mark structure; everything
else is sung). Chrome/mercury theme, synthwave verse/chorus:
```
[Intro]

[Verse 1]
Cold light running down my arm
Liquid silver, no alarm
I touch the glass and watch it flow
A mirror melts, the colors go

[Pre-Chorus]
Faster now, the edges bend
Reflections of a world that never ends

[Chorus]
Quicksilver, running through my hands
Liquid light I'll never understand
Pour me out and let me shine
Chrome and mercury, forever mine
Quicksilver, falling like the rain
Every drop reflecting me again

[Verse 2]
Spin the world to silver dust
Polished steel, I learn to trust
A thousand faces in the chrome
Every one of them is home

[Pre-Chorus]
Faster now, the edges bend
Reflections of a world that never ends

[Chorus]
Quicksilver, running through my hands
Liquid light I'll never understand
Pour me out and let me shine
Chrome and mercury, forever mine
Quicksilver, falling like the rain
Every drop reflecting me again

[Bridge]
(let it flow, let it flow)
Molten, golden, cold and bright
(let it flow, let it flow)
I dissolve into the light

[Drop / Instrumental]

[Outro]
Quicksilver... running through my hands
Liquid light... I finally understand
```
Then convert: MP3 → mono 22050 Hz s16 (ffmpeg) → `tools/qoaconv_s16.exe` →
`music.qoa` → incbin via `music_qoa.S`.

---

## File / build structure (`15_Quicksilver/quicksilver/`)

**Copy verbatim** (engine): `vga.c/.h`, `rgb565.h`, `scene.c/.h`,
`scene_scratch.c`, `audio_qoa.c`, `audio.h`, `qoa.h`, `font8x8.h`, `main.c`,
`fx_common.h`, `pico_sdk_import.cmake`, `pico_extras_import.cmake`,
`host/{main_host.c,vga_sdl.c,audio_sdl.c}`, and `effects/poly3d.{c,h}`.

**New files:** `interp_compat.h`, `interp_emu.{h,c}` (emu host-only),
`effects/{envmap3d.c/.h, rotozoom.c, mode7.c, chrome.c, liquid.c, qstitle.c,
credits.c, transition.c}`, rewritten `timeline.c` and `scene_scratch.h` (union
sized for env-map vert arrays + feedback buffer), `tools/make_meshes.py` (+
`tools/interp_selftest.c`), `assets/_packed/{meshes,envmap,ground,roto,sky_pano}
.{bin,S,h}` + `assets.S`, `music_qoa.S`, `music.qoa`, `README.md`,
`IMPLEMENTATION.md`, `assets/PROMPTS.md`.

**CMakeLists.txt** (mirror 13's): `project(quicksilver C CXX ASM)`,
`PICO_BOARD pico2`, RP2350-only guard; `add_executable` lists `main.c vga.c
scene.c scene_scratch.c timeline.c audio_qoa.c music_qoa.S
assets/_packed/{assets,meshes}.S` + all `effects/*.c` (incl. `poly3d.c`,
`envmap3d.c`, the scenes, `transition.c`) — **but NOT `interp_emu.c`**;
keep `hardware_interp` linked; keep the scanvideo buffer defines;
`OBJECT_DEPENDS` on the `.bin`s like the `lens_lut.S` block.

**host/Makefile** (mirror 13's): `-DHOST_BUILD=1`; `HOST_C = main_host.c
vga_sdl.c audio_sdl.c interp_emu.c`; add each new `effects/*.c` with its own
`name.o:` rule; add `meshes.o: ../assets/_packed/meshes.S` to `ASM_OBJS`.

---

## Verification

- **Emulator first:** build `tools/interp_selftest.c` on host; diff emulator
  output against hand-computed vectors for every mode (shift/rotate, mask,
  sign, cross, add_raw, FULL, BLEND, CLAMP, POP). Re-run on real hardware once a
  Pico is available and diff. **Do not proceed to visuals until bit-exact.**
- **Per scene:** build host (`cd host && make && ./quicksilver.exe
  --screenshot-at <ms>`), eyeball each scene + transition at key timestamps;
  confirm host and device render identically (the emulator's whole point).
- **Device timing:** build `cmake -B build_rp2350 ...` → flash `.uf2`; for the
  env-map and beam-race scenes, dump `arm-none-eabi-objdump -d` of the hot loop,
  count cycles, and confirm 60 fps (heartbeat LED) with no scanline tearing.
- **Budget:** check `.elf` size after each scene — vert arrays fit `g_scratch`
  union; textures stay in flash; arena unchanged at 307 KB (freed entirely in
  the tier-2 beam-raced rotozoomer).

## Risks & build order

Risks (highest first): (1) emulator fidelity — mitigated by the selftest;
(2) scanline underrun in tier-2 beam-race — mitigated by the tier-1 fallback;
(3) env-map frame-time — mitigated by point-sample fallback + SRAM hot loop +
asm only if needed; (4) SRAM/flash budget — mitigated by `g_scratch` + flash
incbin; (5) dual-interp mode conflicts (BLEND=interp0 only, CLAMP=interp1 only)
— assert in the emulator as the SDK panics.

**Step 0 — store this plan in the demo folder:** create
`15_Quicksilver/` and copy this plan to `15_Quicksilver/PLANNING.md` (matching
the `IMPLEMENTATION.md`/`PLANNING.md` convention of demos 10–13) so the design
lives with the demo; flesh it into `IMPLEMENTATION.md` as the build proceeds.

Build order: **1)** emulator + selftest → **2)** rotozoomer scene (tier-1) →
**3)** Mode-7 plain → **4)** chrome centerpiece + `make_meshes.py` + `envmap3d.c`
→ **5)** liquid-metal feedback → **6)** title/credits + transitions → **7)**
stretch: tier-2 beam-raced rotozoomer if step-2 budget had margin. Generate
music + art in parallel early so beat-synced `timeline.c` can be authored.
