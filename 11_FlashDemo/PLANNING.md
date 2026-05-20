# VOLTAGE: RP2350 Pimoroni VGA Demoscene Production
## Design, Storyboard, and Implementation Plan

**VOLTAGE** is a high-energy, high-voltage cyber-neon demoscene demo designed specifically for the **Raspberry Pi Pico 2 (RP2350)**. It pushes the boundaries of real-time rendering on microcontrollers by leveraging dual-core ARM Cortex-M33 acceleration, the PIO scanvideo hardware, and optimized assembly-level maths.

---

## 1. Theme & Aesthetic Direction

The overall aesthetic is **"Kinetic Neon, High-Voltage Plasma, and Glitch Cybernetics"**. We shift away from organic/wet textures towards high-contrast, glowing vector systems, electric arcs, morphing wireframe 3D geometries, and raymarched neon landscapes.

### Color Palette
- **High-Voltage Cyan**: `RGB(0, 240, 255)` / RGB565 `0x07FF` (Primary energy)
- **Laser Magenta**: `RGB(255, 0, 180)` / RGB565 `0xF816` (Accents)
- **Electric Yellow**: `RGB(255, 230, 0)` / RGB565 `0xFEE0` (Sparks)
- **Deep Void**: `RGB(10, 8, 20)` / RGB565 `0x0842` (Background depth)

---

## 2. Storyboard & Demo Arc

The demo is beat-synced to a high-tempo electronic/synthwave track (approx. 130 BPM). 
Transitions align tightly to musical boundaries (onsets and key beat-drops).

| Section | Duration | Screen Mode | Visual Description | Core Technical Challenge |
| :--- | :---: | :---: | :--- | :--- |
| **01. Spark-Gap (Intro)** | `0:00 - 0:32` | `MODE_320` | Shimmering electric logo fade-in over a pulsating retro-futuristic laser grid. Grid lines wave and bend dynamically with high-speed horizontal scrolling. | Fast 2D perspective projection & raster copper bar effects in 8bpp chunky mode. |
| **02. Plasma Core** | `0:32 - 1:04` | `MODE_160` | Heavy fluid simulation of high-density electric charges. Multi-colored dye particles spiral and feed into a central "plasma arc" that flashes violently in sync with the beat. | Fluid solver advection & particle trace in native RGB565 at 60 FPS. |
| **03. Ray-Volt** | `1:04 - 1:40` | `MODE_160` | Real-time raymarched landscape of glowing metallic cylinders and towers, under a flickering electrical sky. High specular highlights and ground plane reflections. | Optimized Cortex-M33 FPU raymarching, space folding, and fast shading. |
| **04. Vector Strike** | `1:40 - 2:15` | `MODE_SPLIT` | Truecolor morphing 3D wireframe vectors (rotating cyber-stars and tori) in the upper half. Crisp 8bpp horizontal copper scroller in the lower half greeting other scene groups. | Raster split row coordination; 3D perspective projection and high-speed line drawing. |
| **05. Spark-Generator** | `2:15 - 2:48` | `MODE_320` | A high-speed plasma tunnel with branching electric lightning structures snaking down the tunnel walls. Strobe-flickers intensify and flash in perfect sync with the audio track climax. | 2D lookup tables (polar coords) + dynamic bresenham lightning generation. |
| **06. Julia Shockwave** | `2:48 - 3:20` | `MODE_160` | A real-time zooming and morphing Julia Set fractal (`z_next = z^2 + c` where `c` orbits in a chaotic attractor). Smooth, deep palette shifting creates an intense visual vortex. | Complex number math optimization using single-cycle M33 DSP multipliers. |
| **07. Voltage Arc (Outro)** | `3:20 - 4:00` | `MODE_320` | A beautifully rotozoomed credits screen that stretches and twists. A stream of neon sparks rain down over the text, eventually fading to a single flashing electric white spark. | Rotozoomer interpolation with sub-pixel stepping & particle physics overlay. |

---

## 3. Technical Architecture & Constraints

### 3.1 Memory Budget (520 KB SRAM)
We must manage our BSS allocations very carefully. The framebuffers consume:
- `fb_a` + `fb_b` (`320x240` 8bpp): `150 KB`
- `fb160_a` + `fb160_b` (`160x120` RGB565): `75 KB`
Total display buffer: **225 KB**

This leaves **295 KB** of SRAM. We will allocate a shared **Scene Scratchpad Union** (`g_scratch`) of **76.8 KB** to reuse buffers across mutually exclusive effects:
- **Intro Grid**: Scroll offsets and copper buffers.
- **Plasma Core**: Particle coords (`uint16_t x, y` + color for 1024 particles) + `160x120` fluid density field.
- **Ray-Volt**: Light fields or precomputed cosine tables.
- **Vector Strike**: 3D vertices, transformed 2D points, edge lists.
- **Spark Generator**: Precomputed tunnel angle/distance LUTs (`2 * 160x120` bytes = 38.4 KB).
- **Julia Shockwave**: Custom color cycling palettes and coordinate grids.

### 3.2 Dual-Core Strategy
- **Core 0**: Renders the active effect frame, processes audio QOA streaming decode, handles scene timing and input events.
- **Core 1**: Dedicated completely to the VGA scanvideo scanline generation loop. Reads from the published front framebuffer (`fb_front` or `fb160_front`) and generates the exact composable-scanline token stream (DPI timings). This ensures absolutely zero display tearing or jitter.

### 3.3 Audio Engine (QOA)
The soundtrack will be a 22050 Hz Mono QOA-compressed file baked into Flash.
- Decode bandwidth is negligible.
- Sample-accurate timing is achieved by monitoring the DMA playhead position, which maps directly to the active clock in milliseconds.

---

## 4. Immediate Development Plan

We will proceed in an iterative, highly disciplined fashion:
1. **Initialize the timeline and scene scratchpad structure** for the new `VOLTAGE` demo.
2. **Implement the Host Desktop preview makefile and files** under `thedemo/host` to allow immediate previewing.
3. **Build the scenes one by one**:
   - **Scene 1**: Spark-Gap Grid (320x240 palette-based grid scrolling).
   - **Scene 2**: Plasma Core (160x120 high-fidelity particle fluid advection).
   - **Scene 3**: Ray-Volt (optimized raymarching scene).
   - **Scene 4**: Vector Strike (3D wireframe render + split copper scroller).
   - **Scene 5**: Spark Generator (lightning plasma tunnel).
   - **Scene 6**: Julia Shockwave (real-time Julia fractal zoom).
   - **Scene 7**: Voltage Arc (rotozoomer credits).
4. **Compile and test locally** using the MSYS2/GCC host build.
5. **Verify screenshot output** to visually evaluate quality.
6. **Compile for the RP2350 target** to verify memory layout, clock speeds, and flash compliance.

---

Are you ready to kick off this production? I am taking full control of the implementation and will report back as soon as the baseline harness and first scene are running. Let me know if you have any immediate creative adjustments!
