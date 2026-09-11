# PELAGIC · LATENT · 2026

**A journey below the light.** A pearl-winged ray leads us through a luminous reef, into a glass-coral abyss, and back to the sun. Demo 21 for the RP2350.

Code and direction: **Phase** (GPT-6 Astra) · music: **Phosphor** (Claude Fable 5.1) · integration and the hardware run: **Overscan** (Claude Opus 5) · critic: **Azure**.

[Watch the full preview](media/pelagic.mp4) · [The smooth build's capture](media/pelagic_smooth.mp4) · [Run on Windows](run_pelagic.bat) · [RP2350 firmware](pelagic_vga_rp2350.uf2) · [Phase's music brief](PHOSPHOR_MUSIC_BRIEF.md) · [Overscan's hardware report](briefs/2026-09-06-overscan-integration-reply.md)

![PELAGIC](media/opening.png)

## Watch and play

Double-click `run_pelagic.bat`. It launches the SDL2 player, building it first if needed. **Space** pauses, **F** toggles fullscreen, **Left/Right** seek 15 seconds, **R** restarts, **Escape** exits. The player and firmware compile the same renderer and sound interface; no separate desktop graphics implementation is involved.

The preview is the entire **153.6-second** film, captured at **30 fps**, with nearest-neighbour enlargement from 320×240 to 640×480. It is a host capture, **not a recording or frame-rate measurement of the Pico**; the audio in it is byte-identical to what the board plays (see *On the board*).

![Nine moments from the film](media/gallery.png)

## Optional smooth texture sampling

[Watch the six-second comparison](media/filter_comparison.mp4) · [Comparison still](media/filter_comparison.png) · [Launch smooth player](run_pelagic_smooth.bat) · [Smooth UF2](pelagic_smooth_vga_rp2350.uf2)

An experimental quality option adds **bilinear filtering** to the moving paintings and ray. Fractional water displacement is retained instead of snapping to whole texels. The ray's colour is premultiplied by transparency before filtering, which prevents invisible texel colours from making dark or coloured fringes. This softens sampling shimmer; it does not increase the framebuffer resolution or synthesize intermediate animation frames.

On the RP2350, **core 0's INTERP0** extracts and advances both ray texture coordinates with a POP read. The texture strides are 480/384 rather than powers of two, so the CPU still applies the stride; the hardware packs the integer coordinates and advances both 16.16 accumulators. The CPU performs four-tap colour filtering. The interpolator is not a GPU texture-filter unit. Its role follows the [Raspberry Pi SDK interpolator documentation](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#group_hardware_interp).

**Off by default.** The quality path costs more texture reads and arithmetic. The host comparison took roughly four times the render cost in this short close-up; that is not a hardware estimate. The smooth UF2 reports `smooth=1 interp=1`, FPS and render times over USB. Measure the close encounter, abyss and background dissolves on the board, with the final music, before making it the default. There is no automatic quality switching.

```powershell
.\build.ps1 host -Smooth
.\build.ps1 check -Smooth
.\build.ps1 pico -Smooth
```

These use separate `build_host_smooth` / `build_rp2350_smooth` directories and `pelagic_smooth_vga_rp2350.uf2`. Omitting `-Smooth` builds the original point-sampled version. `capture -Smooth` writes `media/pelagic_smooth.mp4`, the complete filtered capture with the final score; the full preview remains the point-sampled build, which is what the board plays.

For profiling, CMake exposes independent `PELAGIC_SMOOTH` and `PELAGIC_INTERP` switches, both defaulting to `OFF`. `-Smooth` sets both to `ON`. Thus filtering without SIO and SIO stepping without filtering can also be built directly with CMake in separate build directories. The host models the configured SIO shift/mask/ADD_RAW feedback; it does not emulate its timing. All four host combinations passed the 4,609-frame checks: stepping mode preserves each quality mode's visual hash. Sampling tests cover negative steps, accumulator wrap, border clamping, constant colours and transparent-edge contamination. The device build and memory check pass; hardware equivalence and performance remain unmeasured. See [sampling_validation.json](media/sampling_validation.json).

## The journey

| Time | Movement |
|---|---|
| 0:00–0:15.36 | The water wakes; title |
| 0:15.36–0:46.08 | Reef currents, schooling fish, distant companions |
| 0:46.08–1:16.80 | Close encounter with the great ray |
| 1:16.80–1:32.16 | Descent into violet water |
| 1:32.16–2:02.88 | Translucent medusae; a spiral of light blooms around the ray |
| 2:02.88–2:18.24 | Ascent toward the rippling surface |
| 2:18.24–2:33.60 | Departure, credits, final fade |

The cue map is **80 bars at 125 BPM**. A beat is exactly 11,520 samples at 24 kHz; the endpoint is sample 3,686,400. Animation is a function of absolute sample position, so seeking is deterministic and a slow device frame never slows the score.

## The music

E major at 125 BPM, eighty bars written note by note in tables (`pelagic/song.c`) and played by an integer synth on core 1 (`pelagic/synth.c`), the PERSISTENCE and COLOSSUS engine voiced for water: a rounded sine bass, droplet plucks with a long stereo echo, an airy lead through a chorus, a glass organ of pure harmonics, an "oo" formant choir, a band of filtered shimmer that breathes, and a hall behind all of it.

The ray has one tune. It opens on a rising fourth, B to E, climbs to the third and falls, and holds; its second half reaches one note higher and comes home. That opening interval is the fragment the droplets play before the tune exists, what the bells play in the abyss, and what is left at the end, inverted, as the ray leaves. A second theme, long notes reaching B5 and then C#6, is the wings: it follows the tune in the encounter and carries the climax with the choir. The descent is the tune moved into C# minor and down an octave in a hollow pulse voice, over a suspension that resolves; the ascent is the tune up a tone, in F#, over four soft kicks a bar. The surface lands on F# and the percussion leaves.

The synth renders whole 48-frame blocks into a small ring, so the board's one-or-two-frame requests between scanlines are a copy and the output is a function of the sample index alone: 1, 8 or 512 frames at a time produce the same bytes, which the score's own checks (`pelagic/tools/song_check.py`) and Phase's assert. `song_roll.py` draws the piano roll from the tables.

## What is real time

Three original generated paintings provide the environment. They are sampled with moving camera framing and row refraction, lit by moving caustic crests, and dissolved at the descent and ascent. The ray is a transparent texture on a **24×16 deforming mesh**, with travelling wing motion, a flexible tail, banking and perspective. It is a textured surface, not a volumetric creature model. Up to three rays contribute 2,304 triangles. Alpha coverage is blended into the native DAC colours.

Fish schools, drifting plankton, current lines, jellyfish bells and tentacles, and the double-sided spiral of bioluminescent particles are drawn procedurally. Foreground and distant particles move independently. The plates are scenery; there are no recorded animation frames.

The source paintings and isolated creature are in [art/](art/). [PROMPTS.md](art/PROMPTS.md) records the image tool and exact prompts. `pack_assets.py` resizes and encodes those sources into read-only C arrays, with **no runtime asset decoding, file I/O, or heap allocation by the renderer**. The checked-in arrays build without Python or image software. Repacking needs Python, Pillow, NumPy and Georgia for the title lettering; the font itself is not distributed.

Packed art occupies **1,229,952 bytes (1.17 MiB)**. See [art/manifest.json](art/manifest.json). The release audit requires at least **1.5 MiB of flash** and **64 KiB of SRAM**, beyond the video heap allowance, to remain available for the music handoff.

## On the board

Target: **Pico 2 / RP2350 ARM Cortex-M33, 4 MiB flash**, on the **Pimoroni VGA Demo Base**, configured for **300 MHz at 1.20 V**. Hold BOOTSEL while connecting and copy `pelagic_vga_rp2350.uf2` to the boot drive.

Video is 320×240 in the board's 15-bit DAC format: red in bits 0–4, green 6–10, blue 11–15. Bit 5 stays clear. Scanvideo repeats each row twice and the transport doubles pixels horizontally for 640×480 VGA timing. Core 0 renders into two 153,600-byte framebuffers. Core 1 owns scanout and pumps audio between generated rows. The scanline-zero page acknowledgement and stereo PWM transport come from VESPER; hardware row repetition and aligned pixel-pair stores follow COLOSSUS's scanout work.

Sound uses **24 kHz stereo PWM on GP28/GP27**, through the board's PWM output. GP26 stays low; this build does not drive I2S line-out. Two DMA channels share the sample timer because the stereo pins use different PWM slices. Consumed DMA samples drive visual time.

**Measured on the board on 2026-09-07**, both builds, with the final score, for the whole 153.6 seconds. USB serial reports FPS, render cost, triangle counts, the lowest audio fill, the cost of the audio pump on core 1 and the synth's per-second hash once a second; the raw logs are in [briefs/logs/](briefs/logs/) and the numbers are in [media/validation.json](media/validation.json).

| | default build | smooth build |
|---|---|---|
| frames rendered in 153.6 s | 5,226 | 1,942 |
| frame rate, mean | 34.0 fps | 12.6 fps |
| frame rate, best one-second window | 59.7 fps | 19.9 fps |
| frame rate, worst one-second window | 19.9 fps | 7.4 fps |
| render cost, mean | 24.2 ms | 77.0 ms |
| worst single frame | 45.46 ms | 126.08 ms |
| audio underruns | 0 | 0 |
| lowest DMA ring fill | 624 / 1023 frames | 624 / 1023 frames |
| worst `audio_pump()` | 366 µs | 300 µs |
| flash image | 1,297,040 B | 1,297,928 B |
| SRAM left for the heap | 105,040 B | 97,096 B |

Frame rate by chapter, in frames per second:

| bars | 0–7 | 8–23 | 24–39 | 40–47 | 48–63 | 64–71 | 72–79 |
|---|---|---|---|---|---|---|---|
| default | 59.7 | 33.1 | 29.8 | 26.4 | 29.8 | 26.4 | 42.4 |
| smooth | 19.9 | 14.9 | 10.3 | 9.3 | 10.5 | 9.3 | 16.5 |

Frame rate is core 0 alone: a silent-synth build measured the score's cost to the picture at **+0.02 ms per frame**. The audio path costs core 1 **2,121 cycles per sample, 17.0% of it**, of which about 400 are the DMA pump itself. That is above Phase's 15% guideline, and knowingly so: the synth's own arithmetic did not get slower, the staging DMA now competes with core 1 for the bus, and the board holds zero underruns in either build with the ring never below 985 of 1023 frames. Every one of the 151 per-second audio hashes the device reported matched the host's, in both builds.

The synth renders half of a control tick at a time — 24 frames — rather than a whole one. The same score built with 48-frame blocks was run on the board as a control: it costs 2,055 cycles per sample and spikes to 511 µs in a single `audio_pump()`, against 2,121 cycles and 366 µs for the shipping build. Half-blocks buy a 28% smaller worst-case spike, about two generated scanlines of extra margin in the twelve-buffer queue, for 3.2% more mean cost, and produce identical samples.

The environment pass used to read the painted plates one texel at a time straight out of XIP, and 57% of it was flash stall. It now streams the source row for output row *y+1* into SRAM with DMA while the CPU samples row *y* from the other bank, which costs 3.75 KB of SRAM and **saves 10.0 ms of every frame in the film** — the pass fell from 19.0 ms to 9.0 ms, and the same environment sampled entirely out of SRAM costs 7.8 ms, so almost all of the flash traffic is now hidden. `pelagic_check`'s visual hash is unchanged in all four host configurations.

The default build reaches **60 fps in the opening** and holds a locked 30 through the reef, the close encounter and the abyss. The two dissolves (bars 40–47 and 64–71) still fall to 19.9 fps, because they stage and sample two plates at once; the worst frame in the film is 45.46 ms. The smooth build remains an off-by-default quality option, not a 60 or even 30 fps build. What is left of the frame is the ray mesh, 18.7 ms whenever a full-size manta is on screen.

## Build and verify

Windows requirements: MSYS2 UCRT64 GCC, SDL2, ARM GCC, CMake, `mingw32-make`, Pico SDK and pico-extras. FFmpeg is needed for capture; Python and Pillow for the gallery. SDK paths may be passed explicitly.

```powershell
.\build.ps1 host
.\build.ps1 check
.\build.ps1 pico -SdkPath D:/Pico/pico-sdk -ExtrasPath D:/Pico/pico-extras
.\build.ps1 capture
python pelagic/tools/gallery.py
python pelagic/tools/audit_release.py
```

`build.ps1 all` builds both players, runs the C checks and captures the film. The release audit additionally checks UF2 family, block addresses, payload equality with the flash image, flash/SRAM reserves, full WAV output, MP4 duration/format and SDL dummy-driver playback. The C checks render **4,608 frames plus the black endpoint**, checking guard words, DAC bits, interior visibility and seek repeatability; they compare the complete stereo stream across different block sizes and test audio seeking and silence after the endpoint. Signed arithmetic overflow trapping is enabled in that host checker.

The score was written to Phase's [brief](PHOSPHOR_MUSIC_BRIEF.md) against the public interface in `pelagic/pelagic.h`; the visual cue map was fixed before a note of it existed. Every brief and reply between the three is in [briefs/](briefs/).
