# DAWN - Original Amiga Source Code

This folder contains the original 68000 assembly source code for the **DAWN** 4k intro by AZURE/ARTWORK, released at Party V on November 25, 1995.

## Source

The original source code and binary are available from:
- **Amycoders Archive**: https://amycoders.org/
- **Pouet**: https://www.pouet.net/prod.php?which=9213
- **Source File**: `dawn_final.s` (1,874 lines of 68020 assembly)
- **Binary Size**: 4096 bytes (exactly 4KB)

## About the Original Demo

DAWN is a legendary Amiga 1200 4K intro that showcases:
- Environment-mapped rotating torus with morphing
- Voxel landscape rendering
- Bitmap text effects with blur
- Procedural audio (4-channel Paula synthesis)

In 1995, fitting all of this—including geometry generation, texture synthesis, and audio—into **exactly 4096 bytes** of 68000 assembly was a technical tour de force. Every instruction was hand-optimized, every algorithm carefully chosen.

### Technical Constraints

**Hardware:**
- **CPU**: Motorola 68020 @ 14 MHz (no FPU)
- **Chipset**: AGA (Advanced Graphics Architecture)
- **Chip RAM**: 2 MB
- **Audio**: Paula 4-channel 8-bit DMA
- **Display**: PAL 50 Hz

**Demo Specifications:**
- **Resolution**: 160×128 pixels (chunky buffer)
- **Colors**: 64 colors (6-bit palette indices)
- **Bitplanes**: 6-plane AGA display (converted from chunky)
- **Framerate**: 50 Hz (PAL VBlank timing)
- **Triple buffering**: For smooth animation

### Why This Matters

DAWN represents the pinnacle of "doing more with less." The demo uses:
- **No pre-baked assets** - All graphics, geometry, and audio generated at runtime
- **Fixed-point math** - Fast integer operations (no slow 68000 FPU)
- **Lookup tables** - Trading memory for speed (sine, division, blur, normalization)
- **Chunky-to-planar conversion** - Bridging CPU rendering and AGA bitplanes
- **Size optimization** - Every byte counts to fit in 4KB

## Assembly → TypeScript: Translation Guide

The browser port preserves the original algorithms, transliterating them from assembly to TypeScript while maintaining the computational approach.

### Fixed-Point Arithmetic

The Amiga 68000 has no hardware FPU. DAWN uses 16.16 fixed-point throughout:

**Original Assembly** (dawn_final.s:1662):
```assembly
muls    d5,d0      ; cos * x (32-bit result in d0)
swap    d0         ; Get high word (divide by 65536)
asl.l   #1,d0      ; Scale by 2
```

**TypeScript Port** (vector3d.ts:53):
```typescript
const cosX = (cos * x) >> 16;  // Extract high word
const newX = (cosX - sinZ) << 1;  // Scale by 2
```

### Division Tables

Division is extremely slow on the 68000 (70+ cycles). DAWN pre-generates a 256×256 lookup table:

**Original Assembly** (dawn_final.s:1388-1404):
```assembly
make_div:
    moveq   #0,d0
.loop:
    moveq   #0,d1
.inner:
    ext.l   d0
    divu    d1,d0          ; Expensive!
    move    d0,(a0)+       ; Store quotient in table
    addq    #1,d1
    cmp.w   #256,d1
    bne.s   .inner
    addq    #1,d0
    cmp.w   #256,d0
    bne.s   .loop
```

**TypeScript Port** (mathTables.ts:40-54):
```typescript
for (let dividend = 0; dividend < 256; dividend++) {
    for (let divisor = 0; divisor < 256; divisor++) {
        const index = (dividend << 8) | divisor;
        if (divisor === 0) {
            this.divTable[index] = 0;
        } else {
            this.divTable[index] = Math.floor((dividend / divisor) * 256);
        }
    }
}
```

### Perspective Projection

The original uses clever fixed-point projection with zoom animation:

**Original Assembly** (dawn_final.s:1676-1686):
```assembly
add.w   ro_trans,d2    ; z += translation
add.w   #1000,d2       ; z += zoom factor (1000)
move    d2,d3          ; Save z for division
muls    #1000,d0       ; x *= zoom
divs    d3,d0          ; x /= (z + zoom + trans)
add.w   #80,d0         ; Center X (screen width/2)
```

**TypeScript Port** (vector3d.ts:82-88):
```typescript
const denom = z2 + this.translation + this.zoom;  // z + 1000 + trans
const scale = this.zoom / (denom !== 0 ? denom : 1);

const screenX = Math.round(x2 * scale + 80);  // Project & center
const screenY = Math.round(y1 * scale + 64);
```

### Sine Table Generation

DAWN generates sine tables using Taylor series expansion (no math library in 4KB intro!):

**Original Assembly** (dawn_final.s:1413-1455):
```assembly
make_sine:
    ; Taylor series: sin(x) ≈ x - x³/6 + x⁵/120
    ; For 1024 entries, 0-2π
    moveq   #0,d0
.loop:
    ; Calculate Taylor terms...
    move.w  d7,(a0)+       ; Store sine value
    addq    #1,d0
    cmp.w   #1024,d0
    bne.s   .loop
```

**TypeScript Port** (mathTables.ts:18-30):
```typescript
for (let i = 0; i < 1024; i++) {
    const angle = (i * 2 * Math.PI) / 1024;

    // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120
    const x = angle;
    const x3 = x * x * x;
    const x5 = x3 * x * x;
    const sine = x - x3 / 6 + x5 / 120;

    this.sineTable[i] = Math.round(sine * 65536);  // 16.16 fixed-point
}
```

### Texture Mapping

The inner loop is the most performance-critical section:

**Original Assembly** (dawn_final.s:1288-1340):
```assembly
.scanline:
    move.w  d3,d5          ; V coordinate (high byte)
    move.b  d0,d5          ; U coordinate (low byte)
    move.b  (a4,d5.w),(a2) ; Lookup texel from env map
    add.l   d4,d3          ; Step V
    addx    d2,d0          ; Step U with carry
    addq    #1,a2          ; Next pixel
    dbf     d7,.scanline   ; Loop for scanline width
```

**TypeScript Port** (textureMap.ts:220-235):
```typescript
for (let x = startX; x <= endX; x++) {
    const u = (uCurrent >> 16) & 0xFF;
    const v = (vCurrent >> 16) & 0xFF;
    const texelIndex = (v << 8) | u;

    buffer[offset] = envMap[texelIndex];  // Palette index lookup

    uCurrent += uStep;
    vCurrent += vStep;
    offset++;
}
```

## Code Cross-Reference

Every algorithm in the browser port is transliterated from specific assembly sections:

| Module | TypeScript Function | Assembly Lines | Algorithm |
|--------|---------------------|----------------|-----------|
| **mathTables.ts** | `generateSineTable()` | 1413-1455 | Taylor series sine generation (1024 entries) |
| **mathTables.ts** | `generateDivTable()` | 1388-1404 | Fast division lookup table (256×256) |
| **mathTables.ts** | `generateNormTable()` | 1457-1478 | Vector normalization (length² → multiplier) |
| **palette.ts** | `generatePalette()` | 1518-1562 | Procedural RGB from 5 color schemes |
| **palette.ts** | `createBlurTable()` | 1564-1598 | Blur pair lookup table (64×64) |
| **vector3d.ts** | `rotateY()` | 1632-1657 | Y-axis rotation matrix (2D) |
| **vector3d.ts** | `rotateX()` | 1658-1683 | X-axis rotation matrix (2D) |
| **vector3d.ts** | `project()` | 1676-1686 | Perspective projection with zoom |
| **torus.ts** | `generateTorus()` | 735-789 | Parametric torus surface (8×32 segments) |
| **torus.ts** | `calculateNormals()` | 797-849 | Face & vertex normal calculation |
| **textureMap.ts** | `generateEnvMap()` | 1239-1259 | Spherical environment map (256×256) |
| **textureMap.ts** | `drawPolygon()` | 1163-1351 | Scanline-based texture mapper |
| **voxel.ts** | `generateHeightmap()` | 1494-1545 | Plasma heightmap synthesis |
| **voxel.ts** | `render()` | 517-575 | Column-based voxel raycasting |
| **effects.ts** | `renderText()` | 582-647 | Bitmap font rendering (additive blend) |
| **effects.ts** | `blurVertical()` | 653-670 | 2-tap vertical blur filter |
| **sequencer.ts** | `update()` | 159-271 | Demo scene timeline & transitions |
| **audio.ts** | `init()` | 1092-1106 | Paula 4-channel setup (8-byte waveform) |

## Key Assembly Sections

### Memory Layout
- **Lines 91-151**: Buffer allocation (chunky, planar, vertices, normals)
- **Lines 1368-1620**: Lookup table generation (sine, division, normalization, blur)

### Effects
- **Lines 517-575**: Voxel landscape renderer
- **Lines 582-647**: Text rendering with scramble effect
- **Lines 653-670**: Vertical blur filter
- **Lines 1163-1351**: Texture-mapped polygon rasterizer

### 3D Engine
- **Lines 735-789**: Torus geometry generation
- **Lines 797-849**: Normal vector calculation
- **Lines 1632-1728**: Vector rotation (Y and X axis)
- **Lines 1676-1686**: Perspective projection

### Demo Control
- **Lines 159-271**: Main sequencer loop
- **Lines 1092-1106**: Audio setup (Paula DMA)
- **Lines 379-480**: Chunky-to-planar conversion (AGA bitplanes)

### Optimization Techniques
- **Lines 1388-1404**: Division table (avoid slow `divu` instruction)
- **Lines 1413-1455**: Sine table (Taylor series, no transcendental functions)
- **Lines 1564-1598**: Blur lookup (pre-calculate palette pairs)

## Paula Audio

The original uses a simple 8-byte waveform played on 4 channels with slightly different periods to create a chorus/pad effect:

**Waveform** (dawn_final.s:1092):
```assembly
waveform:
    dc.b $00,$30,$59,$75,$7F,$75,$59,$30  ; 8-byte triangle-ish wave
```

**Channel Setup** (dawn_final.s:1098-1106):
```assembly
; Paula registers (custom chip base $DFF000)
; Channel 0: period 9724, volume 34
; Channel 1: period 9740, volume 34
; Channel 2: period 9756, volume 34
; Channel 3: period 9772, volume 34
; Slightly detuned for pad/chorus effect
```

Period calculation: `3,579,545 Hz ÷ period = sample rate`
- Period 9724 → ~368 Hz
- Period 9772 → ~366 Hz

The browser port replicates this using Web Audio oscillators.

## Color Palettes

DAWN includes 5 procedural palette schemes:

| Scheme | Red Multiplier | Green Multiplier | Blue Multiplier | Scene |
|--------|----------------|------------------|-----------------|-------|
| colors1 | 65000 | 60000 | 35000 | Main (warm red/orange) |
| colors2 | 65000 | 50000 | 35000 | "by" text |
| colors3 | 45000 | 50000 | 35000 | "azure" text (greenish) |
| colors4 | 60000 | 60000 | 60000 | Finale (grayscale) |
| colors5 | 60000 | 50000 | 10000 | Alternate (yellow) |

**Generation Formula** (dawn_final.s:1530-1540):
```assembly
; For each palette index i (0-63):
inverted = i XOR $3F        ; Invert (63→0, 0→63)
red   = (inverted² >> 4) * redMul / 65536
green = (inverted³ >> 10) * greenMul / 65536
blue  = (inverted * 4) * blueMul / 65536
```

This creates a gradient from bright (index 0) to dark (index 63).

## Register Usage Conventions

The assembly code follows these conventions:

**Data Registers:**
- `d0-d3`: Temporary calculations, coordinates (x, y, z)
- `d4-d6`: Loop counters, accumulated values
- `d7`: Often used for inner loop counters (`dbf d7, .loop`)

**Address Registers:**
- `a0-a2`: Buffer pointers (chunky, planar, vertices)
- `a3-a4`: Lookup table pointers (sine, division, env map)
- `a5`: Object data pointer (preserved across rotations)
- `a6`: Custom chip base ($DFF000 for Paula/Copper)

**Calling Convention:**
- Values often passed in registers (d0-d3 for params)
- `bsr` (Branch SubRoutine) for function calls
- `rts` (ReTurn from Subroutine) to return

## Chunky-to-Planar Conversion

One of the most interesting Amiga-specific techniques is the chunky-to-planar conversion. The demo renders to a **chunky buffer** (one byte per pixel = palette index 0-63), then converts to **planar format** for AGA display (6 bitplanes).

**Why?** AGA stores pixels as separate bitplanes:
- Bitplane 0: Bit 0 of all pixels
- Bitplane 1: Bit 1 of all pixels
- ...
- Bitplane 5: Bit 5 of all pixels

This allows fast scrolling/masking but makes pixel manipulation harder.

**Conversion** (dawn_final.s:379-480):
```assembly
chunky2planar:
    ; For each byte in chunky buffer:
    ;   Extract bits 0-5
    ;   Scatter to 6 planar buffers
    ; Uses clever bit manipulation for speed
```

The browser port **skips this entirely** since Canvas uses RGBA (chunky) format natively!

## How to Read the Assembly

If you're new to 68000 assembly:

1. **Instructions** are in the form: `OPCODE SOURCE,DEST`
   - `move.w d0,d1` - Move word from d0 to d1
   - `add.l d2,d3` - Add long d2 to d3

2. **Suffixes** indicate size:
   - `.b` = Byte (8-bit)
   - `.w` = Word (16-bit)
   - `.l` = Long (32-bit)

3. **Addressing modes**:
   - `d0` - Data register
   - `(a0)` - Memory at address in a0
   - `(a0)+` - Memory at a0, then increment
   - `#100` - Immediate value 100
   - `label` - Address of label

4. **Common instructions**:
   - `move` - Copy data
   - `add/sub` - Arithmetic
   - `muls/mulu` - Signed/unsigned multiply
   - `divs/divu` - Signed/unsigned divide
   - `bsr/jsr` - Call subroutine
   - `rts` - Return from subroutine
   - `dbf` - Decrement and branch if not -1

## Differences from Browser Port

The browser port is **algorithmically faithful** but makes these changes:

**Skipped:**
- Chunky-to-planar conversion (not needed for Canvas)
- Copper list setup (Amiga display hardware)
- Some Amiga-specific optimizations (word alignment, Chip RAM placement)

**Simplified:**
- 40-keyframe morphing system (uses simpler interpolation)
- Text scramble effect (character-by-character vs full scramble)

**Modernized:**
- TypeScript instead of assembly
- Descriptive variable names instead of registers
- Canvas 2D API instead of direct memory writes
- Web Audio API instead of Paula chip
- 60fps instead of 50Hz PAL

**Added:**
- User controls (fullscreen, restart)
- FPS counter
- Responsive scaling

## Further Reading

- **68000 Programmer's Reference**: https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf
- **AGA Chipset Guide**: http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0000.html
- **Amiga Demo Scene**: https://www.pouet.net/
- **Amycoders Archive**: https://amycoders.org/

---

*Original demo by AZURE/ARTWORK, 1995*
*Assembly source preserved for educational and archival purposes*
