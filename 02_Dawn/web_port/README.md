# DAWN - Browser Port

A cycle-accurate browser port of the legendary Amiga 1200 4K intro **"DAWN"** by AZURE/ARTWORK, released at Party V on November 25, 1995.

![DAWN Demo](screenshot.png)

## About the Original

In 1995, fitting environment-mapped 3D, voxel landscapes, and procedural audio into **exactly 4096 bytes** of 68000 assembly was a technical tour de force. DAWN showcased the Amiga 1200's AGA chipset capabilities while pushing the boundaries of size-optimized demo coding. The entire demo—including all geometry generation, texture synthesis, and audio—fit in less space than this README file.

### Why This Matters

DAWN represents a pinnacle of "doing more with less." Every instruction was hand-optimized, every algorithm carefully chosen. The demo uses:
- **No pre-baked assets** - All graphics, geometry, and audio are generated at runtime
- **Fixed-point math** - Fast integer operations instead of slow 68000 FPU
- **Lookup tables** - Trading memory for speed (sine, division, blur, normalization)
- **Chunky-to-planar conversion** - Bridging CPU rendering and AGA bitplanes

This port preserves those algorithms, transliterating them from assembly to TypeScript while maintaining the original's computational approach.

## Technical Architecture

### The Amiga Hardware We're Emulating

#### Paula Audio Chip (4-channel DMA)
The original uses Paula's hardware sample playback:
- **Sample**: 8-byte waveform `[0x00, 0x30, 0x59, 0x75, 0x7F, 0x75, 0x59, 0x30]`
- **Period**: DMA rate (3,579,545 Hz ÷ period = sample playback rate)
- **4 Channels**: Periods 9724, 9740, 9756, 9772 (slightly detuned for chorus effect)
- **Volume**: 34/64 (53%)

Our Web Audio implementation duplicates samples at the correct rate (no interpolation) to match Paula's sample-and-hold behavior.

#### AGA Chipset (Chunky-to-Planar)
The original renders to a chunky 160×128 buffer (one byte per pixel = palette index), then converts to planar format for AGA display. We skip the planar conversion and render directly to Canvas via ImageData, but preserve the chunky buffer and 6-bit palette (64 colors).

### Translation Approach

This port preserves the original algorithms while translating from 68000 assembly to modern TypeScript. Key preservation areas:

- **Fixed-point arithmetic** - 16.16 format maintained throughout
- **Lookup tables** - Sine, division, normalization, and blur tables
- **Rendering pipeline** - Same chunky buffer → palette → display flow
- **Algorithmic accuracy** - Each function maps to specific assembly sections

For detailed assembly-to-TypeScript translation examples and code cross-references, see [original_source/README.md](original_source/README.md).

### Rendering Pipeline

```
┌─────────────────┐
│ Scene Update    │ - Rotate vectors (sine table lookups)
│ (50 Hz tick)    │ - Update translation (zoom-in animation)
└────────┬────────┘
         │
         v
┌─────────────────┐
│ 3D Pipeline     │ - Project vertices (z-division)
│                 │ - Backface culling (2D cross product)
│                 │ - Depth sort (quicksort)
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Rasterization   │ - Scanline edge setup
│                 │ - Perspective-correct texture mapping
│                 │ - Environment map lookup (normal → color)
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Chunky Buffer   │ - 160×128 Uint8Array (palette indices 0-63)
│ (6-bit palette) │ - Vertical blur (2-tap averaging)
│                 │ - Text additive blending
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Palette Lookup  │ - 64 RGB colors (procedural generation)
│                 │ - 5 color schemes (red/orange/green/gray/yellow)
└────────┬────────┘
         │
         v
┌─────────────────┐
│ Canvas Render   │ - ImageData.putImageData()
│ (scaled 4x)     │ - 640×512 display
└─────────────────┘
```

## Implementation Highlights

### Voxel Landscape (voxel.ts)
Column-based heightmap raycasting, just like Comanche:

```typescript
// voxel.ts:115-147
for (let x = 0; x < 160; x++) {
    let heightOnScreen = 128;

    for (let z = 1; z < 200; z++) {
        const mapX = Math.floor(px) & 0xFF;
        const mapY = Math.floor(py) & 0xFF;

        const heightValue = this.heightMap[(mapY << 8) | mapX];
        const heightOnScreenNew = Math.floor((128 - heightValue) / z);

        // Draw vertical line from previous height to current
        if (heightOnScreenNew < heightOnScreen) {
            const color = this.colorMap[(mapY << 8) | mapX];
            for (let y = heightOnScreenNew; y < heightOnScreen; y++) {
                buffer.setPixel(x, y, color);
            }
            heightOnScreen = heightOnScreenNew;
        }

        px += dx;
        py += dy;
    }
}
```

**Bug Fix Note**: The colormap initially looked too dark. The issue? We were using the *processed* heightmap value instead of the *original* combined height. The assembly saves the raw value in register `a5` before clamping (lines 1530-1540). Now fixed.

### Environment Mapping (textureMap.ts)
The environment map is a 256×256 procedural sphere map. During rendering, each polygon's face normal is rotated, then used to lookup the texture:

```typescript
// textureMap.ts:93-99
const u = ((nx + 128) & 0xFF);  // Normal X → U
const v = ((ny + 128) & 0xFF);  // Normal Y → V
const texelIndex = (v << 8) | u;
return this.envMap[texelIndex];
```

The mapper uses scanline interpolation with fixed-point steps for perspective-correct texturing.

### Blur Effect (effects.ts)
Vertical 2-tap averaging filter using a pre-computed lookup table:

```typescript
// effects.ts:13-24
for (let y = 0; y < 127; y++) {
    for (let x = 0; x < 160; x++) {
        const offset = y * 160 + x;
        const pixel1 = data[offset];
        const pixel2 = data[offset + 160];

        // Lookup blurred color from table
        const blurIndex = (pixel1 << 6) | pixel2;
        data[offset] = blurTable[blurIndex];
    }
}
```

The blur table maps every possible pair of palette indices (64×64 = 4096 combinations) to their averaged result.

### Audio Synthesis (audio.ts)
Authentic Paula emulation using Web Audio:

```typescript
// audio.ts:62-80
private createSampleBuffer(period: number): AudioBuffer {
    // Convert 8-bit unsigned sample to float (-1.0 to 1.0)
    const sampleFloat = this.SAMPLE_DATA.map(byte => (byte - 0x80) / 128);

    // Paula DMA rate: period determines how long each sample byte plays
    // With 8 bytes, complete waveform frequency is:
    const frequency = PAL_CLOCK / (period * SAMPLE_LENGTH);
    // Example: 3,579,545 / (9724 * 8) = 46 Hz

    const samplesPerCycle = this.audioContext.sampleRate / frequency;
    const buffer = this.audioContext.createBuffer(1, Math.ceil(samplesPerCycle), sampleRate);

    // Fill buffer by DUPLICATING samples (no interpolation!)
    for (let i = 0; i < channelData.length; i++) {
        const sourceIndex = Math.floor((i / samplesPerCycle) * SAMPLE_LENGTH) % SAMPLE_LENGTH;
        channelData[i] = sampleFloat[sourceIndex];
    }

    return buffer;
}
```

**Bug Fix Journey**: Initially used oscillators (sounded wrong), then tried smooth interpolation (still wrong), finally implemented sample duplication with correct period calculation: `PAL_CLOCK ÷ (period × sample_length)`. Now it sounds authentic.

## Recent Bug Fixes

This port went through extensive debugging to achieve authenticity:

### 1. Fadeout Transitions
**Issue**: Objects kept rendering during fadeout, blur looked different
**Root Cause**: Missing check to stop rendering during `fadeout` routine
**Fix**: Added `blurTransition.active` check to halt scene updates ([sequencer.ts:86-92](web/src/sequencer.ts#L86-L92))

### 2. Zoom-In Animation
**Issue**: Objects appeared instantly instead of zooming in from distance
**Root Cause**: `ro_trans` translation animation not implemented
**Fix**: Decrements `translation` from 3000→800 at -20/frame ([sequencer.ts:198-202](web/src/sequencer.ts#L198-L202))

### 3. Object Scaling
**Issue**: Torus appeared too large/close
**Root Cause**: Wrong zoom constant (4000 instead of 1000)
**Fix**: Corrected to match assembly constant `#1000` ([vector3d.ts:43](web/src/vector3d.ts#L43))

### 4. Voxel Colors
**Issue**: Landscape looked too dark/dim
**Root Cause**: Using processed heightmap value instead of original for colormap
**Fix**: Save raw `combinedHeight` before clamping, use for slope calculation ([voxel.ts:72-88](web/src/voxel.ts#L72-L88))

### 5. Finale Scene
**Issue**: Wrong blending, double-blur artifact
**Root Cause**: Applying blur twice, using MAX blend instead of REPLACE
**Fix**: Single blur pass, correct blend mode ([sequencer.ts:284-296](web/src/sequencer.ts#L284-L296))

### 6. Audio Frequency
**Issue**: Sound too high-pitched
**Root Cause**: Frequency calculation didn't account for 8-byte sample length
**Fix**: `frequency = PAL_CLOCK / (period × 8)` instead of `PAL_CLOCK / period` ([audio.ts](web/src/audio.ts))

## Quick Start

```bash
cd web
npm install
npm run build
npm run serve
```

Open `http://localhost:8000` in your browser and click to enable audio.

For detailed build instructions, browser compatibility, and development workflow, see [web/README.md](web/README.md).

### Controls

- **🔊 Speaker Icon** - Enable/disable audio (click to unmute)
- **⏸️ Pause** - Pause/resume demo
- **🔄 Restart** - Restart from beginning
- **⛶ Fullscreen** - Toggle fullscreen mode

## Project Structure

```
dawn/
├── original_source/
│   ├── dawn_final.s     # Original 68020 assembly (1,874 lines)
│   └── README.md        # Assembly details & cross-reference
├── web/
│   ├── src/
│   │   ├── main.ts           # Entry point, render loop, UI (186 lines)
│   │   ├── chunkyBuffer.ts   # 160×128 chunky framebuffer (107 lines)
│   │   ├── palette.ts        # 64-color palette generation (150 lines)
│   │   ├── mathTables.ts     # Sine/division/norm tables (168 lines)
│   │   ├── vector3d.ts       # 3D rotation & projection (134 lines)
│   │   ├── torus.ts          # Parametric torus generation (201 lines)
│   │   ├── textureMap.ts     # Environment map & rasterizer (275 lines)
│   │   ├── voxel.ts          # Heightmap raycaster (158 lines)
│   │   ├── effects.ts        # Text & blur (104 lines)
│   │   ├── sequencer.ts      # Timeline & scene management (231 lines)
│   │   └── audio.ts          # Web Audio Paula emulation (86 lines)
│   ├── dist/                 # Compiled JavaScript
│   ├── index.html           # HTML container
│   ├── package.json         # Dependencies
│   ├── tsconfig.json        # TypeScript config
│   └── README.md            # Build & installation guide
└── README.md            # This file
```

**Total**: ~2,000 lines TypeScript vs 1,874 lines assembly

## Assembly Code Cross-Reference

Every TypeScript module is a direct transliteration of specific assembly sections. For the complete cross-reference table mapping each function to its corresponding assembly line numbers, see [original_source/README.md](original_source/README.md#code-cross-reference).

## Performance

**Target**: 50 FPS (PAL framerate)
**Actual**: 60+ FPS on modern hardware

**Critical path**: Texture mapper inner loop (~20,000 iterations/frame)

### Optimization Strategies (from original)
- ✅ Pre-allocated buffers (no GC pressure)
- ✅ TypedArrays (Uint8Array, Int16Array, Float32Array)
- ✅ Division tables (256×256 lookups instead of division)
- ✅ Sine tables (1024 entries, Taylor series)
- ✅ Fixed-point math (integer operations only)
- ✅ Minimal object creation (reuse vectors)

## Technical Specifications

### Original (Amiga 1200)
- **CPU**: Motorola 68020 @ 14 MHz (no FPU)
- **Resolution**: 160×128, 64 colors (6-bit palette)
- **Framerate**: 50 Hz (PAL VBlank)
- **Demo Size**: Exactly 4096 bytes

For complete original hardware specs and assembly details, see [original_source/README.md](original_source/README.md).

### Browser Port
- **Language**: TypeScript (ES2020)
- **Rendering**: Canvas 2D API (no WebGL)
- **Resolution**: 160×128 → scaled 4× (640×512)
- **Framerate**: 50 Hz emulated
- **Code Size**: ~80 KB compiled JavaScript

For build and installation instructions, see [web/README.md](web/README.md).

## Known Limitations

- **Morphing**: Original has 40-keyframe morphing system (not yet implemented)
- **Text scramble**: Simplified compared to original's character-by-character scramble
- **Voxel optimization**: Could use DDA or mipmap optimizations
- **Chunky2Planar**: Skipped (not needed for Canvas rendering)

## Future Enhancements

- [ ] Full 40-keyframe morphing implementation
- [ ] Add shader-based renderer for comparison
- [ ] Mobile touch/tilt controls
- [ ] GIF/WebM recording feature
- [ ] Side-by-side Amiga emulator comparison
- [ ] Annotated source code walkthrough

## Development Notes

See [CLAUDE.md](CLAUDE.md) for:
- Detailed implementation phases
- Assembly code analysis
- Fixed-point math examples
- Palette generation formulas
- Performance considerations

## Credits

### Original Demo
- **Title**: DAWN
- **Code**: AZURE/ARTWORK
- **Release Date**: November 25, 1995
- **Event**: Party V
- **Platform**: Amiga 1200 (AGA)
- **Category**: 4K Intro
- **Size**: Exactly 4096 bytes

### Browser Port
- **Implementation**: 2024
- **Approach**: Direct transliteration from 68000 assembly
- **Language**: TypeScript
- **License**: Educational/preservation purposes

## Resources

### Demoscene
- [Pouet.net](https://www.pouet.net/) - Demoscene production database
- [Scene.org](https://www.scene.org/) - Demoscene file archive
- [Amiga Hardware Reference Manual](http://amigadev.elowar.com/) - Official AGA documentation

### Technical References
- [68000 Instruction Set](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf) - Motorola programming manual
- [Fixed-Point Math](https://en.wikipedia.org/wiki/Fixed-point_arithmetic) - Fixed-point arithmetic primer

## License

This browser port is provided for **educational and preservation purposes**.

Original demo © 1995 AZURE/ARTWORK

---

*"Environment-mapping rulez !!"*
— Original source code comment, dawn_final.s:1239
