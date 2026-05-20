# State Of The Art (SOTA) — Multi-Platform Port (Windows & RP2040/RP2350)

This directory contains a highly optimized, multi-platform port of the legendary Amiga demo **State Of The Art**, originally released by **Spaceballs** at *The Party 1992* (where it won 1st place, see [pouët.net](https://www.pouet.net/prod.php?which=122)).

Our port is built upon the excellent modern C-reimplementation [nfd/sota](https://github.com/nfd/sota) and adapted to compile and run smoothly on both **Windows (desktop simulator)** and **Raspberry Pi Pico microcontrollers (RP2040 and RP2350)** using VGA, SPI TFT, and PAL composite outputs.

---

## 📺 Showcase Media

To see this port running in real-time on real bare-metal hardware, check out the interactive captures and screenshots below:

Direct video files:

- [SOTA_on_tft_screen.mp4](SOTA_on_tft_screen.mp4) — ST7789 SPI TFT capture
- [SOTA_composite.mp4](SOTA_composite.mp4) — full-length composite PAL capture

### 🎬 High-Fidelity Capture Videos & Screenshots

<table>
  <tr>
    <th width="50%">📺 Composite PAL TV Output (Screenshot)</th>
    <th width="50%">📱 ST7789 SPI TFT Screen (Real-time Video)</th>
  </tr>
  <tr>
    <td align="center">
      <img src="SOTA_composite.jpg" width="100%" alt="SOTA Composite Output on TV"/><br/>
      <sub><em>3-bit Luma Resistor DAC with 4x4 Bayer Dithering</em></sub>
    </td>
    <td align="center">
      <video src="SOTA_on_tft_screen.mp4" width="100%" controls></video><br/>
      <sub><em>Real-time Vector Rotoscoping (240x240 display)</em></sub>
    </td>
  </tr>
  <tr>
    <td align="center" colspan="2">
      <video src="SOTA_composite.mp4" width="100%" controls></video><br/>
      <sub><em>Full-length Composite Capture Video</em></sub>
    </td>
  </tr>
</table>

---

## 📁 Directory Structure

We maintain a clean separation between the original upstream source code and our customized multi-platform enhancements:

```text
01_SOTA_DEMO/
├── sota/               # [SUBMODULE] Clean, unmodified upstream nfd/sota git repository
├── native/             # Windows desktop simulator port (SDL2, libmikmod, custom shims)
├── pico/               # Microcontroller backends (ST7789 SPI, Composite PAL, and VGA Pico/Pico2)
└── README.md           # This master overview, architecture map, and build guide
```

### 💡 Why `native/` and `pico/` are separated from the Submodule

The original upstream `nfd/sota` codebase is designed for Linux/macOS and does not compile on Windows or support microcontroller targets out-of-the-box. To preserve all custom enhancements, they are kept outside the submodule:
1.  **Upstream Submodule (`sota/`) remains 100% clean and unmodified.** You can fetch or merge updates from the upstream repository at any time without creating merge conflicts or dirtying the submodule tree.
2.  **Custom wrappers (`native/` & `pico/`) are fully tracked in the parent repository.** This keeps critical Windows desktop simulator patches (such as Windows `O_BINARY` file pacing, event queue polling to prevent window freezes, and memory/endian shims) and microcontroller backends (VGA scanvideo, SPI panels, composite luma) version-controlled under your main project repository.

---

## 🎮 Supported Platforms & Displays

### 1. Windows Desktop Simulator (`native/`)
A high-fidelity desktop wrapper that emulates the Amiga-style bitplane rendering pipeline. It renders in real-time to a window using SDL2 and streams the original `.mod` tracker music using `libmikmod`.
*   **Ideal for:** Rapid choreography debugging, timeline tweaks, and previewing assets without flashing hardware.
*   **Run Commands Reference:**
    *   Skip an early section: `.\sota.exe --scene dance-1` *(scene names are printed at the end of the WAD build)*
    *   Jump to a specific time: `.\sota.exe --ms 60000` *(starts 60 seconds in)*
    *   Run windowed at a given size: `.\sota.exe --width 800 --height 600`
    *   Quiet run: `.\sota.exe --nosound`
    *   Press **Esc** to quit.

### 2. Raspberry Pi Pico & Pico 2 (`pico/`)
A bare-metal, dual-core implementation that compiles for both the **RP2040** and **RP2350**. The entire demo assets (`sota.wad`) are baked directly into the flash memory. Pre-compiled UF2 binaries are included in this repository:

| UF2 Binary Link | Target Display Hardware | Resolution / Color | Audio Output | Hardware Setup |
| :--- | :--- | :--- | :--- | :--- |
| **[sota_vga_rp2040.uf2](sota_vga_rp2040.uf2)** (RP2040)<br>**[sota_vga_rp2350.uf2](sota_vga_rp2350.uf2)** (RP2350) | **Pimoroni Pico VGA Demo Base** | 320×240 @ 60Hz (32K colors) | 11 kHz mono QOA via PWM | VGA Monitor + 3.5mm Headphone Jack |
| **[sota_tft_rp2040.uf2](sota_tft_rp2040.uf2)** (RP2040) | **ST7789 SPI TFT Panel** | 240×240 (16-bit RGB-565) | *Audio disabled* | 4-wire SPI connection |
| **[sota_composite_rp2040.uf2](sota_composite_rp2040.uf2)** (RP2040) | **RCA Composite / S-Video luma** | 320×256 @ 50Hz (8 grey levels) | *Audio disabled* | 4-resistor binary-weighted DAC |

*   **Build & Wiring details:** Detailed pinouts, resistor values, and flashing instructions can be found in [pico/README.md](pico/README.md).

---

## 🛠️ Quick Start Build Guide

### Build Desktop Simulator (Windows MSYS2 UCRT64)
Ensure you have MSYS2 and the required packages installed.
*   **Required pacman packages:** `mingw-w64-ucrt-x86_64-SDL2`, `mingw-w64-ucrt-x86_64-SDL2_mixer`, `mingw-w64-ucrt-x86_64-libmikmod`, `mingw-w64-ucrt-x86_64-pkgconf`, `mingw-w64-ucrt-x86_64-make`, and `mingw-w64-ucrt-x86_64-python`.

```powershell
# Set path and build WAD assets
$env:PATH          = "<path-to-msys64>\ucrt64\bin;<path-to-msys64>\usr\bin;" + $env:PATH
$env:SSL_CERT_FILE = "<path-to-msys64>\usr\ssl\certs\ca-bundle.crt"
cd <your-workspace-path>\01_SOTA_DEMO\native

# Extract assets from ADF and build sota.wad
"`n" | python3 fromadf.py            # extracts anims + ILBMs into data/
python3 split_and_compress.py        # splits anim chunks, zlib-compresses
"`n" | python3 build_demo.py         # downloads MOD/WAV/IFF assets, packs sota.wad

# Compile and run
make
.\sota.exe
```
*Note: Runtime DLLs needed for running `sota.exe` outside the MSYS2 terminal path are `SDL2.dll`, `libmikmod-3.dll`, and `libwinpthread-1.dll`. Copy these next to `sota.exe` along with `sota.wad`.*

### Build Pico Firmware (RP2040 / RP2350)
Requires a pre-configured **Pico SDK 2.x** and **pico-extras** installation.

```powershell
# Pre-compile the VGA-optimized WAD with audio compressed to QOA
cd <your-workspace-path>\01_SOTA_DEMO\native
python build_demo.py --audio-format=qoa --output=sota_pico.wad

# Configure and compile Pico targets (defaults to RP2040)
cd <your-workspace-path>\01_SOTA_DEMO\pico
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8

# Target RP2350 (Pico 2) instead
cmake -S . -B build_rp2350 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=pico2
cmake --build build_rp2350 -j 8
```

---

## 📐 Architecture & Porting Engine

The code is strictly divided into a **platform-agnostic engine** and a **platform-specific backend**. All hooks the engine uses are defined in `native/backend.h` and `native/sound.h`. Porting to a new target environment consists entirely of re-implementing these two headers.

### Conceptual Flow

```text
                main.c
                   │
                   ▼
         choreography.c  ── timeline / "do_frame(ms)" interpreter
              ╱     ╲
             ▼       ▼
        scene.c    anim.c    graphics.c   iff.c   iff-font.c
              ╲     │     ╱        │
               ▼    ▼    ▼         ▼
              backend_bitplane[6]  (Amiga-style 1-bit-per-plane buffers)
                       │
                       ▼
              ===== backend.h =====
              posix_sdl2_backend.c   <-- platform-specific render code
                       │
                       ▼
               SDL2 window / texture / framebuffer (or Pico VGA / SPI DMA)
```

### Core Architecture Concepts
*   **Bitplane rendering, not RGB:** Render routines write to 6 separate 1-bit bitplanes. The backend composites these bitplanes per pixel into a 64-color palette lookup (`palette[]`) during the frame present call.
*   **Six fixed bitplanes:** Allocated from a central pool in the backend. Each scene reset clears this pool. The text font is handled separately inside a parallel `font_bitplane[6]`.
*   **64-entry palette:** 32 standard entries plus 32 EHB (Extra Half-Brite, doubled-bright) entries. A global "copper" callback allows modifying the palette line-by-line during active rendering.
*   **Frame pacing:** Driven at 20 ms per frame (50Hz), with active animation scripts advancing at 25 fps.
*   **WAD I/O:** The entire resource package (`sota.wad`) is loaded/mmapped into RAM once. File reads point directly into this memory space. Choreography command streams also live inside this WAD.
*   **Sound abstraction:** Declares modular sound backends. `sound_mikmod.c` streams trackers for the desktop build, whereas `sound_qoa.c` plays mixed time-domain compressed QOA streams on microcontroller backends.

---

## 💾 Resource Pipeline

All original graphics and audio assets originate directly from the classic Amiga floppy disk:

1.  **`fromadf.py`** downloads `Spaceballs-StateOfTheArt.adf.gz` from `lardcave.net`, byte-grabs binary animation scripts at hardcoded floppy offsets, and converts the 352×283 1-bit static graphics.
2.  **`split_and_compress.py`** splits the animation data stream into tiny, zlib-compressed chunks under `data/*`.
3.  **`build_demo.py`** reads the 1400-line choreography recipe, pulls remote sound tracker modules (`stateldr.mod`, etc.), and packages everything alongside the command streams into the finished `sota.wad` container file.

---

## 🔧 Windows Desktop Port Modifications (Delta from Upstream)

All modifications to the desktop engine are minimally invasive and preserve full portability:
*   **`native/windows_endian.h` / `endian_compat.h`:** Provides `htobe32`/`be32toh` shims via raw `_byteswap_*` intrinsics because MSYS2's UCRT gcc toolchain lacks `endian.h`.
*   **`native/fmemopen.c`:** Backs the POSIX `fmemopen` stream with `tmpfile()` to support `Player_LoadFP` in libmikmod, resolving a missing compatibility function in the Windows headers.
*   **`native/Makefile`:** Auto-detects MINGW/MSYS/CYGWIN build hosts to correctly resolve flags via `pkg-config`, targets `sota.exe`, and applies MINGW-safe optimization settings.
*   **Text/Binary WAD files:** Opens files with `O_BINARY` on Windows to prevent runtime text-mode issues that cause the WAD file reader to falsely detect EOF at the first `0x1A` byte.
*   **Win32 Event Loop Pacing:** Corrected the SDL event loop to drain *all* events per frame using `while(SDL_PollEvent(...))` rather than just a single event, preventing Windows from marking the window as "Not Responding".

---

## 🚀 Key Microcontroller Achievements & Gotchas

Porting SOTA to bare-metal microcontrollers presented several major challenges:

1.  **Cortex-M0+ Alignment Constraints:**
    The Cortex-M0+ processor hard-faults on unaligned 32-bit memory accesses. The engine's rapid bitplane line drawers cast row pointers directly to `uint32_t *`. To bypass this, we implemented custom bitplane allocators in our backends that pad all bitplane widths to multiples of 64 pixels, satisfying alignment requirements and preventing crashes during scene transitions.
2.  **DMA-Driven QOA Audio Engine:**
    Since the Pico lacks standard tracker audio support, we pre-rendered the `.mod` tracker music and compressed it to **Quite OK Audio (QOA)** format at 11 kHz mono (to fit in the tight 2MB flash budget). In `pico/sound_qoa.c`, a custom real-time decoder refills a double-buffered 2048-entry DMA ring, driving the PWM slices on GP27/GP28.
3.  **Bayer-Dithered PAL Composite DAC:**
    For composite TV output, we built a 4-resistor binary-weighted DAC. A PIO state machine bit-bangs precise HSYNC/VSYNC pulses combined with 3-bit luma, utilizing a 4×4 Bayer ordered dither matrix to achieve ~128 perceived grey levels on standard PAL TV screens.
4.  **VGA Scanvideo Memory Preservation:**
    To fit SOTA inside the RP2040's tight 264 KB SRAM budget alongside double-buffered audio and `pico_scanvideo` screen buffers, we limited the font memory footprints (lazy-loading `fontmap.iff` into exactly the 4 required bitplanes rather than 6) and constrained the local scene heap size to 16 KB (expanded to 64 KB on the RP2350).

---

## 🏆 Credits & Acknowledgements

*   **Original Demogroup:** **Spaceballs** (Classic Amiga release, 1992).
*   **Modern Reimplementation:** **nfd** ([nfd/sota](https://github.com/nfd/sota) - C-port and asset extraction logic).
*   **Microcontroller Port & Engineering:** Azure (Tim Boescke).
*   **LLM Porting Assistant:** **Claude Opus 4.7** (assisted in compiling the C port, Windows/Pico platform abstraction, unaligned memory debugging, and DMA-PWM audio plumbing).
