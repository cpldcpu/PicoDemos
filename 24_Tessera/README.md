# TESSERA · LATENT · 2026

**One small thing can become a world.**

384 glazed ceramic tiles gather into a moving sheet, close into a world,
unfurl into a canopy, climb through moonlight and open into a flower.
Finally, everything returns to one tessera. A 2:33.6 production with an
original score and two painted ceramic gardens, running on the RP2350.

**Phase / GPT-6 Astra:** direction, code, music, synth, art direction and
hardware validation. **Azure:** critic and producer. The VGA and PWM DMA
platform is adapted from **Overscan**'s existing SLEEPER / HELION work.
Phosphor was on hiatus; Overscan was unavailable for this production.

[Watch the full 60 fps movie](media/tessera.mp4) ·
[Run the Windows player](run_tessera.bat) ·
[Flash the RP2350](tessera_vga_rp2350.uf2) ·
[Hear the soundtrack](media/one_small_thing.mp3)

![TESSERA](media/gallery.png)

## The film and the score

The [design and cue sheet](DESIGN.md) describe the six chapters. The scene
uses the audio DMA sample position, so music, transformations and glaze
accents share one clock. The camera can seek to any sample without replaying
previous pictures.

**One Small Thing** is an original 150 BPM score in D minor, opening into F
major. Its eight-bar question and answer are written note by note. Rounded
bass, muted brass, soft accordion triads, a wooden FM mallet and dry drums
give it a different ensemble from the previous productions. The ending has
its own F–Bb–C–Dm cadence. [Piano roll](media/score.png),
[editable note export](media/score.csv), [score source](tessera/song.c).

The stereo integer synth runs at 24 kHz in private 24-frame blocks. It has
short stereo delay taps and a small room, with explicit attack/release
envelopes, a DC blocker and a final fade. The uncompressed master is
regenerated with `tessera/build_host/tessera.exe --wav media/one_small_thing.wav`.

## The paintings and the hardware

The dawn and moonlit gardens were made with the built-in image generation
tool. The [original paintings and exact prompts](art/ART.md) are included.
The asset compiler only resizes and packs them into the native DAC format;
both full-resolution originals remain available. The two 320×240 paintings
occupy **307,200 bytes of flash** together.

- **Core 0:** FPU surface deformation and projection, stable depth buckets,
  affine textured tiles and projected shadows. Shadows are combined in a
  9,600-byte bit mask and blended onto the painting once per covered pixel.
- **INTERP1:** advances the two fixed-point glaze coordinates and produces
  indexed texture addresses. **INTERP0 BLEND:** builds material palettes.
- **DMA channel 8:** copies the selected painting through the XIP streaming FIFO and auxiliary bus while
  the CPU prepares geometry. Glaze texels, palettes and hot raster code live
  in SRAM.
- **Core 1:** scanvideo PIO/DMA output and bounded synth pumping. Two separate
  timer-paced DMA rings drive the left and right PWM slices.
- **Presentation:** double-buffered 320×240 native RGB555, displayed at
  640×480 by horizontal duplication and hardware scanline repetition.

Two complete shipping-UF2 runs: **59.7 fps throughout**, **12.97 ms worst render**, zero repeated film fields, missing scanlines or audio underruns. All 306 per-second PCM hashes, both complete-score hashes and 12 fixed-frame image checksums match the host.

The [hardware report](HARDWARE_VALIDATION.md) records complete runs, failures
found during development, the fixes and the shipping binary's measurements.
Raw logs and the [release manifest](validation/release.json) are included.

## Run and build

Double-click `run_tessera.bat`. It builds the SDL player if necessary.
Space pauses; left/right seek 15 seconds; R restarts; F toggles fullscreen;
Esc exits. The runtime DLLs are copied beside the executable during build.

From this directory, with MSYS2 UCRT64 GCC, MinGW Make, CMake and SDL2 on PATH:

```powershell
.\build.ps1 host
.\build.ps1 check
.\build.ps1 pico
.\build.ps1 capture
```

Pico builds use pico-sdk 2.2.0 and pico-extras, `pico2`, Cortex-M33, 300 MHz
and 1.20 V. `-SdkPath` and `-ExtrasPath` override the environment paths;
the script also recognises `D:/Pico/pico-sdk` and `D:/Pico/pico-extras`.
The image occupies **366,088 bytes** of the **4 MiB flash** budget. Static main SRAM is **440,564 bytes**, leaving **83,724 bytes** before scanvideo allocations, plus the two separate 4 KiB stacks.

The optional fallback builds with CPU background copies and software
texture addressing: `build.ps1 pico -Reference` or `build.ps1 check -Reference`.
It produces separate binaries. Its complete board run matches the images and audio, but reaches 16.53 ms and repeats one field; keep it for diagnosis. The default enables XIP/DMA streaming and interpolators and is the compo build.
`TESSERA_SILENT_PUMP` is a measurement-only CMake option, off in all releases.

Pins follow the other VGA demos: R GP0–4, G GP6–10, B GP11–15, HSync GP16,
VSync GP17; left PWM GP28, right PWM GP27; GP26 low mutes the I2S DAC.
Hold BOOTSEL while connecting the board and copy the UF2 to its drive.
Firmware ends on black; reset the board to play again.

## Reproduce the checks

Python tools require Pillow, NumPy and pyserial; movie capture uses ffmpeg, and the release audit uses ffprobe.

```powershell
python tessera/tools/audio_audit.py
python tessera/tools/gallery.py
python tessera/tools/score_review.py
python tessera/tools/board_run.py --port COM10 --name release_01
python tessera/tools/board_run.py --port COM10 --name release_02
python tessera/tools/release_audit.py
python tessera/tools/write_report.py
```

`board_run.py` **flashes, verifies and reboots** the selected board, then
records a whole performance. It rejects partial logs, bad/missing hashes,
repeated fields, missing scanlines, underruns and frames exceeding its
15 ms render budget. `--replay path.log` validates an existing capture.
Use new run names to retain previous logs.

The desktop checker draws all **9,216 frames plus the black endpoint**,
checks framebuffer guards and the DAC's unused bit, re-renders selected
frames on a different fill, verifies seeking, and compares the complete
audio stream at block sizes 1, 2, 8 and 997. Signed-overflow traps are
enabled in that checker. Six fixed frames, including the black opening,
are hashed on the device and compared with the host. Every one-second PCM
hash and the complete 3,686,400-frame score hash must match too.

The missing-line counter instruments the scanvideo IRQ's actual fallback
selection using a build-local copy; it does not modify the installed SDK.
The report distinguishes boot repeats from repeats during the film. These
are device-side digital transport measurements; the movie is a host capture,
and no oscilloscope or analogue audio/video capture is represented as tested.
