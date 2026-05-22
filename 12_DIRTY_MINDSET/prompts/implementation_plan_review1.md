# Implementation Plan - DIRTY MINDSET Visual Polish & Refinements

This plan addresses all elements of your review feedback for the **DIRTY MINDSET** demo on the RP2350 VGA base. We will resolve specific visual glitches, upgrade scene resolutions, align timings to the track beats, implement clever scene-appropriate transitions, expand font support, and fix the outro credits and rotozoom alignment.

---

## User Review Required

> [!IMPORTANT]
> **Credits Accuracy.** The credits are updated as follows:
> - **Direction:** Opus 4.6 (who designed the plan)
> - **Critic:** Azure (you)
> - **Art:** Nano Banana
> - **Code:** Gemini 3.5 Flash
> - **Music:** Suno 4.5 (prompted by Opus 4.6)

> [!IMPORTANT]
> **Scene 4 Upgrade.** Scene 4 is being upgraded from `MODE_160` (16-bit color, 160x120) to `MODE_320` (8bpp palettized, 320x240). We will draw a rotating 3D tesseract wireframe and overlay a moving 3D vector perspective grid + starfield to fill the background.

---

## Proposed Changes

### 1. Font & Character Set Enhancements

We discovered that the existing custom font in `font8x8.h` does not contain parentheses `(`, `)`, double-quotes `"`, single-quotes `'`, or ampersand `&`. These are required for several texts (CPC boot commands, outro credits). Printing them currently outputs spaces.

#### [MODIFY] [font8x8.h](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/font8x8.h)
- Append new custom 8x8 bitmap glyph entries for `(`, `)`, `"`, `'`, and `&` to `font8x8_data`.
- Update `font8x8_glyph(c)` to correctly map these ASCII symbols to their new indices.

---

### 2. Scene 1: CPC Boot Refinements

Currently, text is rendered at a 2x scale (16x16 pixels per character). On a 320x240 screen, lines like `"Amstrad CPC 6128 - 128K RAM"` require 432 pixels and are truncated. The scene also lacks the beautiful packaged title background bitmap.

#### [MODIFY] [cpc_boot.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/cpc_boot.c)
- Change character rendering to a clean **1x scale** (8x8 pixels) so lines fit perfectly.
- Load `asset_title_bg_data` and its palette `asset_title_bg_pal` as the background.
- Draw a retro CPC-blue terminal window in the center with a double-line border, and type the typewriter commands inside it.
- **Transition effect:** In the final 1000 ms, implement a **CRT Screen Collapse**: squeeze the terminal window vertically to a central line, then horizontally to a single dot, followed by a flash to black exactly as the scene ends.

---

### 3. Scene 3: Text Matrix Resolution

The matrix column positions `column_y[i]` are stored as `uint8_t` variables. When a column rolls off the screen and resets to `-8`, this signed value is written to the `uint8_t` array, wrapping to `248`. Since `248 >= 240` (the screen height), it triggers the reset condition *every single frame*, locking the rain indefinitely and leading to a black/frozen screen.

#### [MODIFY] [scene_scratch.h](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/scene_scratch.h)
- Change `column_y` in the matrix struct from `uint8_t[40]` to `int16_t[40]`.

#### [MODIFY] [text_matrix.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/text_matrix.c)
- Read and write the column positions using `int16_t` values so negative positions are preserved.
- This ensures columns scroll down repeatedly and fluidly without getting stuck.

---

### 4. Scene 4: Evolved 3D Vector Grid & Logo

The current 3D projection has a math bug where outer vertices collapse degenerate lines along the Z-axis, creating a garbled appearance. The background is also completely black, feeling empty.

#### [MODIFY] [dirty_logo.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/dirty_logo.c)
- Switch the scene mode from `MODE_160` to `MODE_320`.
- Update line drawing to `draw_line_320` to work with 8bpp palettized buffers.
- Setup a custom high-tech palette with glowing neon cyan/magenta gradients.
- Implement a **3D perspective floor grid** that scrolls towards the camera in the bottom half of the screen.
- Add a **starfield particle system** where particles fly towards the screen.
- Fix the 3D vertices and edge definitions: draw a beautiful, mathematically sound 3D rotating **Tesseract (hypercube)** in the center of the viewport, scaling dynamically to the beat pulses.

---

### 5. Scene 5: Reaction Mind Stabilization

The Gray-Scott reaction-diffusion simulation is broken because the boundary borders of `grid_a_next` and `grid_b_next` are never updated in the solver loop. When copying the next grid back to the main grid, these uninitialized border pixels propagate garbage into the center, causing the system to explode into NaNs or frozen noise.

#### [MODIFY] [reaction_mind.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/reaction_mind.c)
- Initialize and explicitly copy the border columns and rows from the current step to the next step buffer at the beginning of each simulation frame.
- This stabilizes the organic Gray-Scott patterns, creating beautiful neural filaments that grow organically without exploding.

---

### 6. Scene 6 & 7: Timing & Transitions

The beat intervals in Scene 4 and Scene 6 were defined as `472` and `923` ms. Since the music tempo is 128 BPM, the actual beat interval is ~464 ms, leading to visual desynchronization. Additionally, we need punchy, on-beat transitions.

#### [MODIFY] [dirty_logo.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/dirty_logo.c)
- Adjust `beat_interval` to `464` ms.

#### [MODIFY] [fractal_zoom.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/fractal_zoom.c)
- Adjust `beat_interval` to `464` ms.
- **Transition effect:** In the last 1000 ms of the scene, accelerate the zoom exponentially, creating a **dive transition** into the black heart of the Mandelbrot set, fading the colors out to black in the final 200 ms exactly on the beat drop at `115310` ms.

#### [MODIFY] [greetings.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/greetings.c)
- **Transition effect:** In the last 1000 ms of the scene, slide the raster split-screen divider row down from `160` to `240` (hiding the metaballs) and fade the text scroller to black, creating a clean sweep into the Outro.

---

### 7. Scene 8: Outro Credits & Rotozoom Center

The Rotozoom's center of rotation is currently anchored at the top-left of the texture, not the center of the image. The text is static and boxy. The credits are incorrect, and the font is missing parenthesis support.

#### [MODIFY] [outro.c](file:///d:/Toyprojects/PicoDemos/12_DIRTY_MINDSET/thedemo/effects/outro.c)
- Fix the rotation center by adding the offset `(160, 120)` to the texture coordinates before modular wrap-around. This centers the rotation.
- Replace the static dark box text with a gorgeous **credits upscroller** that slowly rises from the bottom of the screen.
- Implement a **drifting particle background** (floating stars/dust) to add premium depth behind the scrolling text.
- Update the credits strings to:
  - `DIRECTION BY OPUS 4.6`
  - `CRITIC: AZURE`
  - `ART: NANO BANANA`
  - `CODE: GEMINI 3.5 FLASH`
  - `MUSIC: SUNO 4.5 (PROMPTED BY OPUS 4.6)`
- Implement a global **fade-out to black** in the last 3000 ms of the outro (t_ms > 35323) to finish the demo gracefully.

---

## Verification Plan

### Automated Verification
1. **Host Build**: Run `make` in `thedemo/host/` and verify compilation has zero warnings.
2. **Timing/Screenshot Capture**:
   - Run the simulator to capture screenshots of the transitions and fixes:
     - `./thedemo.exe --screenshot-at 5000` (Scene 1 Boot inside the border)
     - `./thedemo.exe --screenshot-at 43000` (Scene 3 Text Matrix rain)
     - `./thedemo.exe --screenshot-at 60000` (Scene 4 upgraded 3D vector grid & tesseract)
     - `./thedemo.exe --screenshot-at 80000` (Scene 5 Reaction Mind organic pattern)
     - `./thedemo.exe --screenshot-at 155000` (Scene 8 Rotozoom Outro upscroller with parenthesis)
3. **Pico Hardware Build**: Compile the RP2350 VGA target via CMake in `build_rp2350/` to ensure perfect device builds.

### Manual Verification
- Run the demo locally (`./thedemo.exe`) to check the fluid motion of all scenes, the CRT collapse transition, Mandelbrot dive transition, metaball slide, and outro credits fade.
