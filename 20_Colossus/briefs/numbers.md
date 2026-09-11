# COLOSSUS — the numbers

Every figure here is measured, and every one says where. **DEVICE** is the
Raspberry Pi Pico 2 (RP2350) on COM10 at 300 MHz and 1.20 V, reported over USB
CDC by `colossus/main.c` and read by `tools/serial_read.py`. **HOST** is the
desktop, the ELF linker map, or a capture read back pixel by pixel. Nothing
below is an estimate; where something is not measured it says so.

Phosphor owns the README's prose. These are its numbers.

---

## The production

| | |
|---|---|
| Length | 5:07.2 — 160 bars, 20 phrases of 8, 7,372,800 stereo frames |
| Tempo | 125 BPM; a 16th is exactly 2,880 samples and 60 control ticks of 48 |
| Picture | 320×240, 15-bit colour, doubled to 640×480 VGA |
| Audio | 24,000 Hz stereo, synthesised on the device, 12,500 sys cycles a sample |
| Firmware | 136,000 bytes of flash (HOST, `colossus.bin`) |
| Art | 76,928 bytes of pixels and palettes in flash, 13 assets (HOST) |

## Frame rate over the whole 5:07 (DEVICE)

The renderer as shipped, against the same measurement of the same demo before
the round-five optimisation work:

| | before | after |
|---|---:|---:|
| Frames drawn in 307.2 s | 5,671 | **16,700** |
| Mean frame rate | 18.5 fps | **54.4 fps** |
| Frames below the 30 fps floor | 4,503 (79%) | **0** |
| Frames below 60 | 5,671 (100%) | 1,656 (9.9%) |
| Worst single frame | 108.05 ms | **24.14 ms** |
| Worst displayed-frame interval | 117.15 ms | 33.47 ms |
| Audio underruns | 0 | **0** |

**Not one frame of the 16,700 was held longer than two display refreshes.**

### Per phrase (DEVICE)

Render time min/mean/max in milliseconds, and the frame rate over the phrase.

| Ph | Bars | Chapter | render ms | fps |
|---:|---|---|---|---:|
| 1 | 0–7 | overture | 3.20 / 3.23 / 3.37 | 59.7 |
| 2 | 8–15 | the plain | 3.22 / 4.06 / 12.41 | 59.7 |
| 3 | 16–23 | the plain | 8.41 / 8.70 / 12.42 | 59.7 |
| 4 | 24–31 | the hand | 8.61 / 9.02 / 20.64 | 59.6 |
| 5 | 32–39 | the hand | 15.70 / 16.22 / 20.69 | 57.9 |
| 6 | 40–47 | the heart | 15.10 / 17.23 / 21.28 | 31.1 |
| 7 | 48–55 | the heart | 11.19 / 11.51 / 15.22 | 59.7 |
| 8 | 56–63 | the eye | 11.34 / 11.71 / 15.91 | 59.7 |
| 9 | 64–71 | the eye | 11.96 / 12.16 / 16.01 | 59.6 |
| 10 | 72–79 | the load | 10.85 / 13.40 / 17.68 | 57.9 |
| 11 | 80–87 | the load | 13.28 / 14.80 / 17.24 | 58.9 |
| 12 | 88–95 | the spine | 10.82 / 15.44 / 18.53 | 58.7 |
| 13 | 96–103 | the spine | 6.05 / 6.65 / 23.82 | 59.6 |
| 14 | 104–111 | the spine | 14.55 / 18.08 / 24.13 | 38.8 |
| 15 | 112–119 | the crown | 9.17 / 11.11 / 18.14 | 59.6 |
| 16 | 120–127 | the crown | 14.10 / 14.34 / 18.31 | 58.8 |
| 17 | 128–135 | the colossus | 14.02 / 14.17 / 23.34 | 58.7 |
| 18 | 136–143 | the colossus | 16.21 / 18.97 / 23.53 | 34.6 |
| 19 | 144–151 | coda | 16.20 / 16.81 / 21.09 | 40.5 |
| 20 | 152–159 | coda | 16.08 / 16.30 / 20.21 | 57.8 |

### Geometry against PLANNING §8's ceilings (DEVICE, `demo_stats()`)

Every chapter is inside both ceilings.

| Chapter | triangles | ceiling | fill | ceiling |
|---|---:|---:|---:|---:|
| the plain | 70–74 | 300 | 3,132–11,634 | 40k |
| the hand | 342–346 | 900 | 47,643–52,337 | 70k |
| the heart | 142–146 | 1,200 | 31,991–39,878 | 85k |
| the eye | 182–186 | 700 | 41,826–50,466 | 65k |
| the load | 158–174 | 600 | 17,858–26,076 | 65k |
| the spine | 154–644 | 1,200 | 18,761–77,020 | 90k |
| the crown | 32–36 | 900 | 18,845–27,451 | 65k |
| the colossus | 474–496 | 1,500 | 11,705–44,414 | 70k |
| coda | 442–478 | 700 | 11,675–14,562 | 45k |

### The material ceiling (DEVICE)

PLANNING §8's worst case built exactly — 1,500 triangles, 90,000 candidate
fragments, all four material paths, 256 embers, restricted bloom — drawn in
place of the demo by `build.ps1 pico -MaterialTest`:

| | |
|---|---|
| Render, steady state | 59.76 / 59.80 / 59.97 ms |
| Core-0 render work | 17.9 M cycles at 300 MHz |
| PLANNING §8's allowance for a 30 Hz frame | 8 M cycles |
| | **2.24× over** |

## Where the time goes (DEVICE, per pass, cycles per frame)

Core 0, from `main.c`'s `PROF` line. The two columns are the same demo before
and after the round-five work; the passes are the same passes.

| Pass | before | after |
|---|---:|---:|
| sky | 11,537,973 | **509,302** |
| floor (the plain, reveal and coda only) | 12,487,989 | **1,243,512** |
| overture fade | 1,853,144 | **208** |
| veil, per active frame | ~3,000,000 | ~150,000 |

The 66 ms frame that started that work was phrase 3, where the sky and the
floor were **18.7 M of 19.9 M cycles — 94%** — and the triangles were 618,103.

## Core 1 (DEVICE)

Core 1 does the scanout and the synth and nothing else.

| | |
|---|---|
| Scanline copy | **2,469 cycles a line**, worst 3,246 |
| Scanout cost | **34.9 Mcycles a second** (240 buffers a frame, yscale 2) |
| The same at 640×480, `y>>1` | 2,638 cycles a line, **74.4 Mcycles a second** |
| Synth | **1,776 cycles an audio sample**, 42.6 Mcycles a second |
| Worst single `audio_pump()` | 99,700 cycles = 332 µs |
| Core 1 total | **~25% of one core** |

Scanning out through `vga_mode_320x240_60`, whose yscale is 2, instead of
`vga_mode_640x480_60` with a doubled row index, gives back **39.5 Mcycles a
second — 13.2% of a core** for the same picture. Each copy is also 8% cheaper
when done half as often, because the scanline DMA is reading those buffers out
of the same striped SRAM the copy is writing to.

## Memory

Static, from the ELF map (HOST):

| | Bytes |
|---|---:|
| Static main SRAM, `0x20000000` to `__end__` | **473,420** |
| — ours | 459,272 |
| — SDK, newlib, TinyUSB | 12,447 |
| — inter-section alignment | 1,701 |
| Core stacks, SCRATCH_X + SCRATCH_Y | 8,192 |
| **Heap region**, `__end__` to `__StackLimit` | **50,868** |

473,420 + 50,868 = 524,288 exactly. The largest single allocations are the two
320×240 pages at 307,200 bytes, the depth buffer at 76,800, and the synth's
delay and reverb lines at 32,280.

On the board (DEVICE):

| | Bytes |
|---|---:|
| Free heap at boot | 50,868 |
| After `video_init()` | **36,864** |
| pico_scanvideo's runtime allocation | 14,004 |
| **Measured boot floor** | **10,568** |

### The boot floor

Measured, not inherited: `build.ps1 floor -Ballast N` links N bytes of dead
`.bss`, flashes, and watches (DEVICE).

```
heap 20,880  BOOT      heap 10,568  BOOT
heap 13,380  BOOT      heap 10,332  PANIC "Out of memory"
heap 11,504  BOOT      heap 10,096  PANIC
                       heap  9,628  PANIC
                       heap  5,880  PANIC
```

**10,568 bytes of heap boots and reaches the main loop; 10,332 does not.** The
shipping build has **26,296 bytes of margin** over it. The floor is a property
of the platform, not the renderer, and it is void if
`PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT` (8) or
`PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS` (324) changes.

PERSISTENCE's inherited 79 KiB was never a floor — it was the heap one build
happened to have, with sixteen scanline buffers rather than eight.

## Audio identity (referee 2)

FNV-1a over every emitted `int16`, latched once a second by the synth, printed
by the device and diffed against the host's table from `capture --hashes`.

| | |
|---|---|
| Latches over the run | 307 (one a second) |
| Reported and checked (DEVICE) | **306** |
| **Wrong** | **0** |
| Device peak sample | 29,914 |
| Host peak sample (`song_check.py`) | 29,914 |

The one or two unreported latches are not disagreements: the latch is "most
recent", the device prints once a second, and the two clocks drift, so
occasionally a mark is overwritten between prints. Every mark reported matched,
on every run.

## The referees

`build.ps1 check` runs all of them and gives one verdict.

| Referee | Tool | What it proves |
|---|---|---|
| 1 sync | `tools/sync_check.py` | every chapter boundary is a phrase boundary; one table drives music and picture |
| 2 audio | `tools/song_check.py` | block-size independent, never clips, ends in silence |
| — renderer | `render_checks.c` | 483 seek comparisons, near/far clipping against independently specified geometry, depth order independent of submission, the bloom mask, 16 exact ceiling frames |
| 3 frame rate | `main.c` telemetry | the table above, per phrase, on the device |
| 4 film | `tools/film_check.py` | no black or flat frame outside bars 0–1 and 159; every chapter's inscription readable |
| — ledger | `tools/ledger_check.py` | every SRAM allocation declared, heap above the measured floor |

Referee 4's floors are absolute and come from the hardware, not from the film:
black is a 99th-percentile pixel at or below one DAC step (8 of 255), flat is
fewer than 4 of the 32 luma buckets holding 0.1% of the frame each. The
darkest frame in the film has a 99th percentile of **56** and the flattest uses
**8** buckets, so the margins are 7× and 2×. `--selftest` feeds a black frame
and a two-tone frame through the same predicates and fails if they pass.

## The art

Thirteen assets, 76,928 bytes of flash, converted by
`assets/round6/convert.py` and packed by `tools/pack_assets.py`, which is the
only path into the firmware and verifies every asset against the manifest's
SHA-256 before emitting.

Palettes are allocated before assignment, not frozen and diffused into, which
is what removed the streaking (HOST, mean column-to-column difference — what
rises when a diffusion pattern is laid over a smooth image):

| asset | source | before | after |
|---|---:|---:|---:|
| dusk_sky | 0.56 | 5.72 | **0.59** |
| dawn_sky | 0.59 | 6.10 | **0.69** |
| stone | 1.13 | 3.75 | **1.13** |
| bronze_wear | 2.60 | 8.57 | **2.77** |
| furnace | 2.58 | 1.47 | **2.58** |

Stone needs 24 distinct 5-bit colours, bronze 46 and the furnace 112 — all fit
in 256 entries exactly, so those three are converted with no diffusion at all
and are lossless.

## Building and running

```
.\build.ps1 host       the SDL2 player and the capture tool
.\build.ps1 pico       colossus_vga_rp2350.uf2, 300 MHz at 1.20 V
.\build.ps1 check      all the referees; one command, one verdict
.\build.ps1 flash      picotool reboot -f -u, then load -x
.\build.ps1 run        flash, then read the telemetry
.\build.ps1 ship       the uf2, the MP4 and the WAV
```

The host build also comes up under WSL with `make -C colossus/host`: plain
gcc, no cmake, no SDL and no image library.

On the desktop, `Run Colossus.cmd` builds the player if it is not there and
launches it. On hardware, hold BOOTSEL, plug the Pico 2 in, and copy
`colossus_vga_rp2350.uf2` across. VGA on the standard Pico VGA pinout; audio
on GP28 and GP27.

Toolchain: Pico SDK with pico-extras, arm-none-eabi-gcc 13.3.0, `-O2` and
`-fno-math-errno` — which lets the compiler use the FPU's own square root but
does **not** license the reassociation `-ffast-math` would, because the host
WAV and the device output are diffed by hash.
