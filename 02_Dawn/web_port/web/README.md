# DAWN - Browser Port

A TypeScript/Canvas 2D port of the classic Amiga 4k intro "DAWN" by AZURE/ARTWORK (1995).

## Features

- 160×128 resolution with 64-color palette (faithful to the original)
- Environment-mapped rotating torus with morphing
- Voxel landscape rendering
- Bitmap text effects with blur
- Procedural audio (Web Audio API)
- 50Hz timing emulation (original PAL framerate)

## Installation

Install dependencies using npm:

```bash
npm install
```

## Building

Compile TypeScript to JavaScript:

```bash
npm run build
```

This will:
- Compile all TypeScript files in `src/` to JavaScript
- Output compiled files to `dist/`
- Generate source maps for debugging

## Running

After building, open `index.html` in a modern web browser:

```bash
# Option 1: Simple HTTP server (recommended)
npm run serve

# Option 2: Open directly in browser
open index.html  # macOS
start index.html # Windows
xdg-open index.html # Linux
```

**Note**: Click anywhere on the page to enable audio (browser autoplay policy requirement).

## Development

For development with automatic recompilation on file changes:

```bash
npm run watch
```

Then serve the files with a local HTTP server and refresh the browser after changes.

## Project Structure

```
web/
├── index.html          # Main HTML page with canvas
├── src/
│   ├── main.ts         # Entry point & render loop
│   ├── chunkyBuffer.ts # Framebuffer management
│   ├── palette.ts      # 64-color palette system
│   ├── mathTables.ts   # Sine/division/normalization tables
│   ├── vector3d.ts     # 3D rotation & projection
│   ├── torus.ts        # Torus geometry generation
│   ├── textureMap.ts   # Environment mapping & polygon renderer
│   ├── voxel.ts        # Voxel landscape raycaster
│   ├── effects.ts      # Blur & text effects
│   ├── sequencer.ts    # Demo timeline
│   └── audio.ts        # Web Audio synthesis
└── dist/               # Compiled JavaScript (generated)
```

## Browser Compatibility

Requires a modern browser with support for:
- Canvas 2D API
- TypedArrays (Uint8Array, Int16Array)
- Web Audio API
- RequestAnimationFrame

## Technical Notes

This is a faithful port of the original 68000 assembly code, preserving:
- Original resolution and palette limitations
- Fixed-point arithmetic precision
- Algorithmic approach (no WebGL shaders)
- Demo sequencing and timing

See the main [README.md](../README.md) for more details about the original demo and porting process.

