# Implementation Plan - DIRTY MINDSET Premium Visual Polish & Refinements

This plan builds upon our previous baseline and implements premium refinements based on your latest review feedback:
1. Adding smooth, theme-appropriate transitions to all scenes.
2. Upgrading **Scene 4 (Dirty Logo)** line-drawing to use **Xiaolin Wu Antialiased Lines** via custom palettized intensity gradients.
3. Stabilizing and refining **Scene 5 (Reaction Mind)** using a high-fidelity **9-point isotropic Laplacian solver** and **type-safe/aligned float memory structures** to cure blocky artifacts and produce stunning, winding neural filaments.
4. Fixing and perfecting the **Scene 8 (Outro)** translucent overlay colors by using a pre-calculated Euclidean color-distance lookup table.

---

## User Review Required

> [!IMPORTANT]
> **Palettized Antialiasing.** 
> To achieve sub-pixel fractional antialiasing on an 8bpp palettized buffer without slow read-modify-write blending, we partition 32 color entries into four 8-level brightness gradients:
> - Neon Cyan (indices 32..39)
> - Neon Magenta (indices 40..47)
> - Neon Blue (indices 48..55)
> - Neon Purple (indices 56..63)
> The fractional Wu intensity `w` (0.0 to 1.0) maps directly to `base + (int)(w * 7.99f)`, producing gorgeous, smooth anti-aliased wireframe lines.

> [!IMPORTANT]
> **High-Fidelity 9-Point Laplacian.**
> The blocky diamond artifacts in Scene 5 were caused by the cardinally-biased 5-point discrete Laplacian. Replacing it with a 9-point isotropic discrete Laplacian (Cardinal weights = 0.20, Diagonal weights = 0.05, Center = -1.0) will result in extremely smooth, high-fidelity curves.
> To prevent any alignment/casting crashes on the ARM Cortex-M33 target, we will redefine the shared scratch arrays in `g_scratch.rd` as native 4-byte-aligned `float` arrays instead of casting `uint8_t` arrays.

> [!IMPORTANT]
> **Scene 8 Outro Translucent Overlay Color Fix.**
> In 8bpp palettized mode, direct mathematical division of framebuffer values (e.g., `bg / 3`) acts on the *palette index* rather than the color's RGB components, resulting in garbled neon colors.
> To resolve this elegantly, we precalculate a **darkness shading LUT** `dark_lut[256]` during scene init. For each color index `i`, we extract its original RGB, target a 35% shaded intensity, search the first 253 colors of the endcard palette for the closest matching color using a Euclidean distance search, and map `dark_lut[i] = closest_idx`. During frame rendering, we simply write `fb[pixel] = dark_lut[fb[pixel]]`, yielding a beautiful, dark translucent window with zero distortion.

---

## Proposed Changes

### Component 1: Shared Scratch Pad Aligned Float Buffers

We will update the global scene scratch pad memory to define Scene 5's double buffers as native `float` arrays, guaranteeing 4-byte alignment and type safety on the device.

#### [MODIFY] [scene_scratch.h](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/scene_scratch.h)
- Change `g_scratch.rd` struct arrays (`grid_a`, `grid_b`, `grid_a_next`, `grid_b_next`) from `uint8_t[19200]` to `float[80 * 60]`. This maintains the exact same memory footprint (76,800 bytes total) but guarantees 4-byte alignment and removes any unsafe pointer casting.

---

### Component 2: Scene 2 (Plasma Chip) Transition Polish

#### [MODIFY] [plasma_chip.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/plasma_chip.c)
- Compute a `global_fade` multiplier: linear fade-in from 0.0 to 1.0 in the first 1000ms (`t_ms < 1000`) and linear fade-out to 0.0 in the last 1000ms (`t_ms > duration - 1000`).
- Apply the `global_fade` scale factor to the final RGB values before packing them into the truecolor RGB565 back buffer.

---

### Component 3: Scene 3 (Text Matrix) Glitch & Rain Transition

#### [MODIFY] [text_matrix.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/text_matrix.c)
- Implement a smooth 1000ms palette fade-in at start.
- Implement a **Digital Glitch Scanline Jitter** transition in the final 1000ms:
  - Displace horizontal rows of pixels using random/sinusoidal offsets to create scanline tearing.
  - Introduce random row blackout dropouts and bright white dropout glitch lines to emulate system malfunctions.
  - Fade the palette colors to black over the glitch duration to transition cleanly to the next act.

---

### Component 4: Scene 4 (Dirty Logo) Xiaolin Wu Lines & Vector Melt

#### [MODIFY] [dirty_logo.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/dirty_logo.c)
- **Palette Setup**: Pre-calculate four 8-level gradients inside `fx_dirty_logo_init()` interpolating from the background color (r=8, g=4, b=20) to Neon Cyan (r=0, g=255, b=255), Neon Magenta (r=255, g=0, b=255), Neon Blue (r=0, g=100, b=255), and Neon Purple (r=160, g=0, b=255).
- **Wu Line Drawer**: Implement fractional AA line drawing helper functions (`fpart`, `rfpart`, `draw_pixel_aa`, `draw_line_aa`).
- **Wireframes & Grid**: Use `draw_line_aa` to draw the receding longitudinal floor grid lines with Neon Purple (base 56) and the tesseract edges with dynamic AA bases (Cyan, Magenta, or Blue).
- **Vector Melt Transition (last 1000ms)**:
  - Accelerate rotations quadratically using a `melt_progress`-derived speed multiplier.
  - Scale down the vertex coordinates size to 0.
  - Collapse the perspective grid vertically towards the vanishing point (y=100) by scaling height offsets.
  - Fade the entire neon palette to black.

---

### Component 5: Scene 5 (Reaction Mind) Isotropic Laplacian & Transitions

#### [MODIFY] [reaction_mind.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/reaction_mind.c)
- **Memory Access**: Use the updated type-safe native `float` arrays from `g_scratch.rd` directly, eliminating `uint8_t *` to `float *` casting.
- **9-Point Laplacian Solver**:
  - Implement the isotropic 9-point Laplacian discrete stencil equation using cardinally-adjacent (0.20) and diagonally-adjacent (0.05) neighbors.
  - Change model constants to standard stable parameters (`diff_a=1.0f`, `diff_b=0.5f`, `feed=0.0545f`, `kill=0.062f`) to support winding labyrinth filaments.
- **Transitions**: Implement smooth 1000ms linear fade-in and 1000ms linear fade-out transitions.

---

### Component 6: Scene 8 (Outro) Euclidean Color-Distance Overlay shading

#### [MODIFY] [outro.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/outro.c)
- **LUT Pre-calculation**: Precalculate a `dark_lut[256]` mapping in `fx_outro_init()`. For each color index, compute a Euclidean distance search within the endcard palette against a 35% shaded target brightness to find the closest darkened shade.
- **Color Overlay Shading**: Replace the index division `bg / 3` inside the rendering loop with `fb[y * VGA_320_W + x] = dark_lut[bg]`.

---

## Verification Plan

### Automated Tests
1. **Compilation**: Run `make` in `thedemo/host` to ensure the host simulator compiles cleanly with zero warnings/errors.
2. **Visual Snapshotting**: Capture simulator screenshots at key timestamps using the `--screenshot-at` CLI flag:
   - `t = 20000` (Scene 2 Plasma Chip fade-in)
   - `t = 48500` (Scene 3 Text Matrix digital glitch transition)
   - `t = 60000` (Scene 4 upgraded Antialiased Tesseract and perspectivic floor grid)
   - `t = 71500` (Scene 4 Vector Melt collapsing to the vanishing point)
   - `t = 82000` (Scene 5 Reaction Mind smooth organic winding labyrinth filaments)
   - `t = 160000` (Scene 8 Outro translucent overlay with perfect shaded colors)
3. **Pico Hardware Build**: Compile the RP2350 VGA target via CMake in `thedemo/build_rp2350/` to guarantee clean target builds.

### Manual Verification
- Execute `./thedemo.exe` and visually verify:
  - Smooth 1000ms fading of the Plasma Chip.
  - Cybernetic scanline tearing and digital glitching at the end of the Matrix rain.
  - Perfectly anti-aliased sub-pixel lines in the synthwave arena.
  - Fluid shrinking, spinning, and collapsing vector melt of the hypercube.
  - Organic winding curve filaments in the Gray-Scott Reaction Mind.
  - Gorgeous, rich-colored dark translucent credits overlay in Scene 8 with zero pixel noise or color corruption.
