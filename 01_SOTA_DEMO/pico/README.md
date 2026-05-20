# SOTA on Raspberry Pi Pico (RP2040 / RP2350)

A port of [nfd/sota](https://github.com/nfd/sota) — the C reimplementation of the legendary 1992 Amiga demo *State Of The Art* by Spaceballs (original release on [pouët.net](https://www.pouet.net/prod.php?which=122)) — to the Raspberry Pi Pico microcontroller. The whole demo wad is baked into the firmware and runs straight from flash.

The same source tree builds for both the **RP2040** (original Pico) and the **RP2350** (Pico 2) — pick the board at configure time with `-DPICO_BOARD=pico` or `-DPICO_BOARD=pico2`. See *Building for RP2350* below for what changes (mostly: the same firmware, with a more generous heap because the chip has ~2× the SRAM).

Three display backends share the same engine, build to different UF2s, and you flash whichever one matches the hardware in front of you:

| UF2 | Display | Color | Resolution | Hardware |
|---|---|---|---|---|
| `sota_pico.uf2` | ST7789 240×240 SPI panel | full color (RGB-565) | 240×240 | 4 wires + GND |
| `sota_composite.uf2` | S-Video Y / composite | 8-level greyscale (Bayer dithered) | 320×256 @ 50 Hz PAL | 4 resistors + a TV |
| `sota_vga.uf2` | VGA monitor + audio | 32K colors (5-5-5), 11 kHz mono audio | 320×240 @ 60 Hz | Pimoroni Pico VGA Demo Base |

A fourth UF2, `sota_composite_test.uf2`, is just a static colorbar test pattern for verifying composite/S-Video sync timing — handy if a TV won't lock onto the main composite build. A fifth, `sota_vga_test.uf2`, paints simple R/G/B horizontal bands and emits a "tick" heartbeat over USB serial — useful for confirming the 250 MHz overclock and USB CDC plumbing work before touching the engine build.

## Build prerequisites

- **MSYS2 UCRT64** environment with these packages:
  - `mingw-w64-ucrt-x86_64-arm-none-eabi-gcc` and `-arm-none-eabi-newlib`
  - `mingw-w64-ucrt-x86_64-picotool`
  - `mingw-w64-ucrt-x86_64-make`, `mingw-w64-ucrt-x86_64-cmake`
- **ffmpeg** in PATH (with `libopenmpt`) — for transcoding `.mod`/`.wav` to QOA when building the VGA wad. The MSYS2 `mingw-w64-ucrt-x86_64-ffmpeg` package works; so does the prebuilt `ffmpeg-full` on Windows as long as it's on PATH.
- **Python 3** in PATH — runs `build_demo.py`.
- **Pico SDK 2.x** at `<path-to-pico-sdk>` (or via `PICO_SDK_PATH`).
  - **`lib/tinyusb` must be populated** (the SDK 2.2.0 release pins a commit that has since been force-pushed off `raspberrypi/tinyusb`, so vanilla `git submodule update --init` may fail; if it does, point the submodule at `hathach/tinyusb` instead — `cd lib/tinyusb && git remote set-url origin https://github.com/hathach/tinyusb && git fetch && git checkout origin/master`). Without populated tinyusb the VGA build silently drops USB CDC stdio and you lose `printf` diagnostics.
- **pico-extras** at `<path-to-pico-extras>` (or via `PICO_EXTRAS_PATH`) — needed for the VGA build (`pico_scanvideo_dpi`).
- A built **`sota.wad`** from `../native/` (see the top-level `README.md`). The ST7789 and composite backends `.incbin` this directly.
- A built **`sota_pico.wad`** in `../native/` for the VGA backend — this is the same demo content but with audio assets transcoded to mono QOA (see *Audio pipeline* below).

## Build

```powershell
$env:PATH             = "<path-to-msys64>\ucrt64\bin;<path-to-msys64>\usr\bin;" + $env:PATH
$env:PICO_SDK_PATH    = "<path-to-pico-sdk>"
$env:PICO_EXTRAS_PATH = "<path-to-pico-extras>"

# 1) Build the desktop-flavoured WAD (ST7789 + composite backends use this).
cd d:\Toyprojects\PicoDemos\01_SOTA_DEMO\native
python build_demo.py

# 2) Build the VGA-flavoured WAD (transcodes .mod/.wav to QOA).
#    First time this runs it caches the QOAs under data/qoa_11025/.
python build_demo.py --audio-format=qoa --output=sota_pico.wad

# 3) Build all four Pico targets — defaults to RP2040 (`pico`).
cd d:\Toyprojects\PicoDemos\01_SOTA_DEMO\pico
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
```

After this, `build/` contains all UF2s side-by-side.

### Building for RP2350 (Pico 2)

Configure a parallel build directory with `-DPICO_BOARD=pico2`:

```powershell
cmake -S . -B build_rp2350 -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DPICO_BOARD=pico2
cmake --build build_rp2350 -j 8
```

The CMake configure step prints `SOTA: RP2350 target, VGA heap = 64 KB` to confirm the chip detection — the RP2040 build reports `VGA heap = 16 KB`. The output UF2s are *not* cross-compatible: each `.uf2` carries the chip's family-ID header and the bootrom refuses to flash one onto the wrong silicon, so you can't accidentally brick a board by dragging the wrong file.

What actually differs between the two builds:
- **Toolchain:** SDK 2.x picks `arm-none-eabi-gcc` with `-mcpu=cortex-m0plus` for RP2040 and `-mcpu=cortex-m33` (plus FPU) for RP2350. Same compiler binary, different flags.
- **Heap:** `HEAP_SIZE_KB` is 16 on RP2040 (squeezed against scanvideo buffers + audio ring in 264 KB SRAM) and 64 on RP2350 (where 520 KB SRAM gives generous headroom).
- **Clock:** both targets run at 250 MHz via `set_sys_clock_khz` in `main.c`. RP2040 needs `VREG = 1.20 V` to be reliable there; RP2350 doesn't really need the voltage bump but the same code is harmless on it (250 MHz is well under what RP2350 will do — 300+ MHz is routine).
- **Audio divisor:** `sound_qoa.c` computes the DMA pacing-timer divisor from the live `clock_get_hz(clk_sys)` rather than hardcoding the 22676 that fits 250 MHz / 11025 Hz exactly, so the same source works at any sys_clk.

The engine is unchanged across the two builds — same bitplane pipeline, same 32K-color palette, same copper. You don't get visibly *more* on the RP2350; you get the same demo running with more breathing room. If you wanted a visible upgrade later, a 640×480 native bitplane variant becomes practical with the extra SRAM (the RP2040 has to stay at 320×240 with scanvideo `xscale=2`).

## Flash

Hold **BOOTSEL**, plug the Pico in via USB → it mounts as `RPI-RP2`. Drag-and-drop whichever UF2 you want, or:

```powershell
picotool load -f -x build\sota_vga.uf2     # auto-resets and runs
```

The VGA UF2 is the biggest (~1.7 MB) because the QOA audio assets are baked into it; ST7789 and composite are similar size with the .mod-format wad.

## Hardware wiring

### ST7789 240×240 SPI (`sota_pico.uf2`)

Tested on a generic GMT130 module. Pinout matches a common MicroPython quick-start:

| Display pin | Pico GPIO | Notes |
|---|---|---|
| VCC | 3V3 | |
| GND | GND | |
| SCK / SCL | GP2 | SPI0 SCK |
| SDA / MOSI | GP3 | SPI0 TX |
| RES | GP5 | active-low reset |
| DC | GP6 | command/data select |
| CS | tie to GND | most 240×240 modules have CS tied internally; if your module has a CS pin, ground it |
| BLK | 3V3 | backlight |

SPI runs at 62.5 MHz (sysclk/2). If your wiring is flying-lead breadboard and you see shimmering, drop the rate in `st7789.c` (look for `ST7789_BAUD_HZ`).

### Composite / S-Video Y (`sota_composite.uf2`)

Three GPIOs through a resistor DAC feed the Y (luma) signal, summed at a single node into the S-Video Y pin or an RCA composite jack. Best-tested wiring (after the resistor-value iterations we did during bring-up):

```
GP12 ─[ 2 kΩ ]──┐   ← video LSB
GP13 ─[ 1 kΩ ]──┤   ← video bit 1
GP14 ─[ 430 Ω ]─┤   ← video MSB
GP15 ─[ 1 kΩ ]──┤   ← sync
                 └─► S-Video Y (DIN-4 pin 3) or RCA composite center
GND ───────────────► both S-Video GND pins (1, 2) or RCA shield
```

That's a 3-bit-per-pixel binary-weighted DAC → 8 grey levels, plus 4×4 Bayer ordered dithering for ~128 perceived greys per row. The output is honest 50 Hz PAL with proper HSYNC/VSYNC; both CRT TVs and most modern AV-input LCDs lock onto it.

Fewer resistors are also fine, the demo just gets simpler greyscale:
- 2 resistors only (drop GP12 and GP13): 1-bit B/W + sync, dithered into ~16 perceived levels.
- 3 resistors (drop GP12): 4 grey levels + dither.

The hello-pattern build `sota_composite_test.uf2` (just black-and-white bars, no engine) needs only the 2-resistor minimum — useful for first-pass sync verification with a scope or TV.

Color composite via S-Video chroma is *possible* on the RP2040 but is a multi-day side project (see end of `../CLAUDE.md` if you ever want to attempt it).

### VGA + audio (`sota_vga.uf2`)

Built for the **[Pimoroni Pico VGA Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base)**. The carrier board has all the resistor DACs (5 bits per channel = 32K colors), the DE-15 connector wired up to the standard RPi Foundation VGA pinout, **plus** an audio jack tied to GP27/GP28:

| Pico pin | Function |
|---|---|
| GP0-GP4 | Red (5 bits, MSB at GP4) |
| GP5 | unused (gap in the layout) |
| GP6-GP10 | Green |
| GP11-GP15 | Blue |
| GP16 | HSYNC |
| GP17 | VSYNC |
| GP26 | I2S DIN — held LOW by firmware so the on-board PCM5100A DAC stays muted (it shares the audio jack with the PWM path) |
| GP27 | PWM audio R (also doubles as I2S BCK when the DAC is active) |
| GP28 | PWM audio L (also doubles as I2S LRCK) |

Plug the Pico into the demo-base socket, connect VGA to your monitor, headphones/speakers to the audio jack. That's it. The same firmware also works on a discrete-resistor breadboard if you wire the same pin layout by hand (six 470 Ω + six 1 kΩ for a 2-bit-per-channel 64-color variant).

The VGA build runs the MCU at **250 MHz with VREG = 1.20 V** — needed on RP2040 to keep the per-scanline render loop inside the budget that yscale=2 gives us, and harmless overhead on RP2350 (which would run there with the default voltage too). The clock is set in `main.c` **before** `stdio_init_all()` so USB CDC enumerates at the final clock and stays connected.

## Audio pipeline

The VGA backend is the only one that plays audio so far. It does not run a tracker — instead the demo's `.mod`/`.wav` assets are pre-rendered and re-encoded as [QOA](https://phoboslab.org/log/2023/02/qoa-time-domain-audio-compression) on the build host.

```
.mod / .wav  ── ffmpeg ──►  11 kHz mono 16-bit PCM  ── qoaconv ──►  .qoa  ──► bake into sota_pico.wad
```

- `build_demo.py --audio-format=qoa` runs this whole pipeline. Outputs cache under `data/qoa_11025/`.
- The host-side encoder `qoaconv.exe` is built from `qoaconv.c` + `qoa.h` in `sota/pico/` (the QOA reference encoder, MIT-licensed, from [phoboslab/qoa](https://github.com/phoboslab/qoa)). It's compiled lazily on first WAD build:
  ```powershell
  cd d:\Toyprojects\PicoDemos\01_SOTA_DEMO\pico
  gcc qoaconv.c -std=gnu99 -lm -O3 -o qoaconv.exe
  ```
- On the Pico, `sound_qoa.c` decodes one QOA frame at a time into a per-stream 10 KB scratch buffer. Two concurrent streams (music + sample) are mixed sample-by-sample into a 2048-entry DMA ring that feeds PWM slice 6 (GP27 + GP28) at 11025 Hz, paced exactly by DMA timer 0.
- Refill is polled from `sound_update()` once per engine frame — half-buffer = ~93 ms, comfortably more than the worst-case ~50 ms engine frame in the 3D scene.

Why 11 kHz mono? At 22 kHz the main MOD encodes to 1.8 MB, which doesn't fit alongside the engine + the rest of the wad in 2 MB of flash. 11 kHz mono encodes the same MOD to ~900 KB, and the perceptual quality is fine for SOTA's tracker music (the original Amiga Paula chip sampled at similar rates anyway). To change the rate edit `--qoa-rate` in `build_demo.py` and `SAMPLE_RATE` in `sound_qoa.c`.

## Source layout

```
sota/pico/
├── CMakeLists.txt              # all four engine targets + vga_test
├── pico_sdk_import.cmake       # SDK boilerplate
├── pico_extras_import.cmake    # extras boilerplate (VGA only)
├── main.c                      # entry point (backend-agnostic; VGA does overclock here)
├── pico_endian.h               # ARM endian shim for the engine's endian_compat.h
├── sota_wad.S                  # .incbin of ../native/sota.wad      (ST7789 + composite)
├── sota_wad_qoa.S              # .incbin of ../native/sota_pico.wad (VGA, QOA-encoded audio)
├── sound_stub.c                # no-op sound.h impl (ST7789 + composite)
├── sound_qoa.c                 # QOA → PWM-DMA audio (VGA)
├── qoa.h                       # QOA codec reference (MIT) — used on Pico for decode
├── qoaconv.c / qoaconv.exe     # host-side WAV ↔ QOA converter (MIT)
│
├── pico_st7789_backend.c       # ─┐
├── bitplane_compose.{c,h}      #  │ ST7789 build:
├── st7789.{c,h}                # ─┘  bitplanes → RGB-565 → SPI panel
│
├── pico_composite_backend.c    # ─┐ composite/S-Video build:
├── composite.pio               # ─┤  PIO bit-bangs sync + 3-bit luma + Bayer
├── composite_test.c            # ─┘  (also drives the sota_composite_test UF2)
│
├── pico_vga_backend.c          # VGA build, uses pico_scanvideo for PIO/DMA/timing
└── vga_test.c                  # standalone color-bands + USB heartbeat test
```

The engine itself (`anim.c`, `choreography.c`, `graphics.c`, `iff.c`, `scene.c`, etc.) lives one directory up in `../native/` and is shared with the desktop build, with **two small upstream patches** required for the MCU:

1. **`graphics.c`** — guard against `num_vertices < 2` in the outline drawer. The original code reads vertex 0 then loops 2..N filling `x1,y1`; with `num_vertices == 1` those stay uninitialized and `planar_line_thick` runs Bresenham toward stack garbage forever. Benign on x86, fatal on Cortex-M0+.
2. **`endian_compat.h`** — branch on `__arm__` / `PICO_BUILD` to include the new `pico_endian.h`. Mingw-w64 ucrt has no `endian.h`, and ARM gcc has no `_byteswap_*`; the shim uses `__builtin_bswap*`.

Both changes are also safe on the desktop build (the polygon guard fixes a latent bug there too).

## Cortex-M0+ alignment gotchas (the ones that cost us a day)

This bit us three times during bring-up and is worth flagging upfront for anyone porting more of the engine to other MCUs:

- **Unaligned 32-bit access hard-faults** on Cortex-M0+ and the default fault handler is a tight infinite loop (no diagnostic). The engine's `planar_line_horizontal` casts row pointers to `uint32_t *`, so **bitplane stride must be a multiple of 4 bytes** — we pad width up to a multiple of 32 pixels.
- **`scene_static2_tick`** offsets a 2×2 plane's `data` pointer by `stride/2` (to expose the four quadrants of a scrolling-buffer effect used by `jump-1`, `iris-vs-glitchy`, `static-dancers`). So `stride/2` must *also* be 4-aligned, i.e. **width must be a multiple of 64 pixels**. Without this, those scenes hard-fault on the first transition.
- **`O_RDONLY` opens binary files in text mode** on Windows ucrt — irrelevant to the Pico build but relevant to the Windows desktop build; see `../native/posix_sdl2_backend.c`.

The Pico backends pad bitplane widths to 64-pixel multiples in `backend_allocate_bitplane`; that single fix is what unlocked most of the demo on the MCU.

## VGA bring-up gotchas (also worth a day of saved time)

- **The on-board PCM5100A I2S DAC fights the PWM path.** GP27/GP28 are shared between PWM_R/L and I2S BCK/LRCK; the DAC will happily decode noise off those pins unless GP26 (I2S DIN) is pinned LOW. Firmware does this in `sound_init`.
- **Audio uses an explicit DMA channel (11), not `dma_claim_unused_channel`.** Scanvideo on core 1 grabs channels 0–2 as part of `scanvideo_setup`, and there's a race window where audio init on core 0 could otherwise steal one of those, after which scanvideo panics, video never starts, and `backend_should_display_next_frame` hangs in `scanvideo_wait_for_vblank` forever (taking the engine — and USB CDC servicing — down with it).
- **scanvideo only accepts pixel clocks that divide sys_clk exactly.** At our 250 MHz overclock, the working pixel clocks are 25 MHz and integer divisions of it. That's why we stay on `vga_mode_320x240_60` / `vga_mode_640x480_60` (25 MHz) and not e.g. `vga_mode_800x600_60` (38.4 MHz — non-integer ratio → panic).
- **Fonts are allocated once and held forever.** SOTA's `iff-font` loader otherwise mallocs ~58 KB on every `loadfont` choreography command. Newlib's malloc fragments under that churn (with audio's `.bss` already in place there's only ~70 KB of heap), and `pico_malloc` *panics* on OOM rather than returning NULL. So `backend_font_load` does a one-shot malloc the first time it's called and reuses the same buffers thereafter. Allocations are also limited to the four planes the font IFF actually uses (`fontmap.iff` is 4-bit), not the full six.
- **Font bitplanes are 256 rows tall.** `fontmap.iff` is 320×256; the engine's `ifffont_load` computes the integer scale as `min(plane.w / iff.w, plane.h / iff.h)`. If you allocate the font planes at the screen's height (240) the scale comes out to 0 and the IFF renders to a 0×0 region — text is invisible. The fix is making the font planes 256-tall regardless of the screen height.

## Stuff that's not done

- **Color composite over S-Video** — only B/W luma today. Full PAL chroma generation via second PIO at 4× the color subcarrier is feasible but ~multi-day of work; see notes in the parent `CLAUDE.md`.
- **ST7789 and composite builds have no audio** — they link `sound_stub.c`. To enable audio there too, swap to `sound_qoa.c` and pack a QOA-flavoured wad like the VGA build does.
- **No two backends share their `.c` files** — there's duplication between `pico_st7789_backend.c`, `pico_composite_backend.c`, and `pico_vga_backend.c` (bitplane allocator, palette code, font wrapping). All three could be factored into a common `pico_backend_common.c` if the project grows.
