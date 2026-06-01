# QUICKSILVER — implementation notes

## 1. The interpolator as hardware hero

The RP2350 has two SIO interpolators per core (INTERP0/INTERP1). Each lane
**right-rotates** its accumulator by `SHIFT`, masks bits `[MASK_LSB..MASK_MSB]`,
optionally sign-extends, and adds a base; the **FULL** result is
`base2 + lane0_masked + lane1_masked`. INTERP0 also has a **BLEND** unit
(`lane1 = base0 + (base1-base0)*alpha/256`) and INTERP1 a **CLAMP** unit. The
classic use is affine texture-address generation at ~1–2 reads/pixel.

### Dual-target via one header
Effect code includes **`interp_compat.h`** and uses **only** the SDK function
API (never raw `interp->peek[]`). On device that header pulls in
`hardware/interp.h` (raw silicon). On the SDL host (`HOST_BUILD`) it pulls in
**`interp_emu.h`** — our own `interp_hw_t` and `interp_*` functions that
*compute* the datapath inside `peek`/`pop` (a memory-mapped register can't be
modelled by a plain struct). `interp_emu.c` is compiled **only** by the host
Makefile, never by CMake. `tools/interp_selftest.c` proves the emulator
bit-exact against hand-computed vectors for shift/rotate, mask, sign-extend,
cross-input, add-raw, FULL, BLEND, CLAMP and POP (run it: it prints `ALL PASS`).

### Base-relative addressing (portability)
`interp_peek_full_result()` returns a `uint32_t`. Device pointers are 32-bit so
it could carry a real texel address, but **host pointers are 64-bit**. So
`qs_texmap_setup()` (in `interp_compat.h`) sets `base2 = 0` and the FULL result
is a **byte offset** into the texture, with the interpolator doing the
`col*bpp + row*stride + wrap` math; the effect adds its own base pointer. One
code path, both targets; the RP2350 still does the real interpolator work. The
mask gives free power-of-two wrap (tiled textures cost nothing).

## 2. Per-scene interpolator configuration

- **Rotozoom / Mode-7 / credits floor** — `qs_texmap_setup(interp0, log2bpp=1,
  log2w=8, log2h=8)` for a 256×256 RGB565 texture. Per scanline: load
  `accum0=u0, accum1=v0` (16.16), `du,dv`; inner loop is
  `qs_tap_bilerp()` = `peek_full` (texel offset) + 3 neighbour taps + bilinear,
  then `add_accumulator(du/dv)`. Mode-7 derives `(u0,v0,du,dv)` from the row's
  perspective depth; bilinear weights come from the accumulator low bits.
- **Mercury Plain haze** — `interp1` in **CLAMP** maps each row's "near-ness"
  into a `[0,255]` fog weight in hardware.
- **Chrome (`envmap3d.c`)** — per vertex, the normal is rotated into view space
  and mapped to matcap UV; UVs are Gouraud-interpolated across each triangle and
  the span loop is the same `qs_tap_bilerp` on the matcap. interp0 = address-gen.
- **Liquid (`liquid.c`)** — `interp0` in **BLEND**; a coarse plasma grid is
  bilinearly upscaled with three hardware BLEND ops per pixel (`hw_blend`).

So across the demo the interpolator's **affine address-gen, BLEND and CLAMP**
are all exercised.

## 3. Chrome rasteriser (`envmap3d.c`)

Matcap (sphere-map) shading: rotate vertex + normal, perspective-project to
`int16` screen coords, map view-normal `(x,y)` → matcap UV (16.16). Triangles
are back-face culled by screen winding (all meshes are wound **CCW-outward** —
the knot/torus generators were flipped to match the sphere), painter-sorted by
rotated-centroid z (far first), and filled with a scanline rasteriser that
interpolates UV per scanline and taps the matcap per pixel via interp0. Per-frame
working set is file-static (`int16` coords, 16.16 UVs, `MAXV=2048`, `MAXT=4096`).
Objects baked by `tools/make_meshes.c`: icosphere(642v), trefoil & (3,5) knots,
spike-ball, twisted torus(1920v), rounded cube.

## 4. Memory & performance

- 320×240 RGB565 double-buffered framebuffer arena: 307 KB (shared, from
  `vga.c`). `g_scratch` union ≤ 76.8 KB for animated geometry. envmap3d
  file-static ≈ 60 KB. **BSS ≈ 386 KB / 520 KB.**
- Flash: code+meshes ≈ 0.9 MB, music QOA 2.38 MB, textures 0.5 MB → **text ≈
  3.3 MB / 4 MB.**
- All per-pixel scenes render to the back buffer on core 0 at 60 fps (the
  per-pixel cost is dominated by the bilinear lerp; the interpolator collapses
  the address math). Cycle-count the hot span loops with
  `arm-none-eabi-objdump -d build_rp2350/quicksilver.elf` and mark them
  `__not_in_flash_func` if a scene runs long. A beam-raced (no-framebuffer)
  rotozoom variant is a documented stretch goal (see PLANNING.md tier-2).

## 4b. Full VGA 640×480 — beam-raced (`vga640/`, `quicksilver_vga640.uf2`)

A standalone firmware does **true 640×480@60 with no framebuffer** (614 KB > 520 KB
SRAM, so it's impossible to store one): core 1 generates each scanline live via
the interpolator POP loop straight into the scanvideo buffer. Verified on real
RP2350 hardware.

Hitting the per-line deadline (~9 500 cy/line at 300 MHz for 640 px) required
strict beam-racing discipline — without it you get a few good lines then a blank
(underrun) screen:
- **texture in SRAM** — XIP-flash random reads (rotozoom access) miss the cache
  and blow the budget; copied once in `qs_race_setup()`;
- **hot code in SRAM** — generator + core-1 loop are `__not_in_flash_func`
  (confirmed at `0x2000xxxx`);
- **no transcendentals on core 1** — core 0 precomputes the affine basis +
  per-row flex each frame.
Net ~3–4 cy/px (POP + SRAM load + store), comfortably inside budget. DMA cannot
help generate (the interpolator is in SIO, CPU-only) — DMA only streams the
finished lines to the PIO.

## 5. Transitions

`effects/transition.c` is applied by the scene runner (`scene.c`) after every
MODE_HIRES `frame()`, washing the buffer toward chrome-white over the first/last
~420 ms of each scene, so adjacent scenes meet on a white frame — a uniform
liquid-chrome glint crossfade with no per-scene wiring and no hard cuts.

## 6. Banding

Smooth procedural gradients (chrome backdrop, credits sky, liquid colourise) are
**ordered-dithered** (`qs_dither`, 4×4 Bayer) before the 5-bit `rgb565_pack`
truncation, to avoid banding on the VGA DAC.

## 7. File map

```
interp_emu.{h,c}      software interpolator (host only)
interp_compat.h       target switch + qs_texmap_setup()
effects/qs_fx.h       bilinear interpolator tap, lerp, dither
effects/envmap3d.{c,h} matcap chrome rasteriser  | meshes.h baked objects
effects/{rotozoom,mode7,chrome,liquid,title,credits,transition}.c   scenes
tools/{make_meshes.c, make_textures.c, make_logo.sh, interp_selftest.c}
assets/_packed/{roto,ground,envmap,sky,title_bg,logo}.bin + assets.S/logo.S
```
Engine (`vga.c`, `scene.c`, `audio_qoa.c`, `main.c`, `rgb565.h`, host backends)
is reused verbatim from 13_Singularity.
