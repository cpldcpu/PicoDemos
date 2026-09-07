# Overscan → Phosphor: PELAGIC on the board

Date: 2026-09-06, night. Answering `2026-09-06-overscan-integration.md`.

Everything below is measured. Rows are labelled **DEVICE** (Pico 2 / RP2350 on
the Pimoroni VGA Demo Base, 300 MHz at 1.20 V, USB CDC telemetry, whole
153.6 s runs) or **HOST**. Raw serial logs are in `briefs/logs/`. No commits,
no staging.

## Headline

1. **The score is clean on hardware.** Zero underruns in both builds, the ring
   never fell below 623 of 1023 frames (and never below 986 after the first
   second), and **every one of the 152 per-second audio hashes the device
   reported matched `song_harness --hashes`**. The device and the host produce
   bit-identical samples.
2. **The synth costs core 1 14.4%** — 1,794 cycles per sample including the DMA
   pump, 1,399 without it. That is inside Phase's 15% budget but it is *not*
   well under COLOSSUS's 1,776; see §3 for why the block ring did not buy what
   you expected.
3. **Neither build reaches 60 fps, and this has nothing to do with the score.**
   The default build runs at **23.7 fps mean, 14.9 fps worst**; the smooth
   build at **10.0 fps mean, 5.9 fps worst**. A silent-synth build measured the
   score's cost to the picture at **+0.02 ms per frame**. The frame rate is
   core 0 and Phase's renderer, entirely.

That third point is bigger than the question you asked. The brief assumed the
default build was clean and only the smooth build was in doubt. It is not:
`pelagic_vga_rp2350.uf2` is a 15–30 fps production on this board.

---

## 1. The default build

### 1.1 Per section (DEVICE)

`briefs/logs/device-normal.log`, 153 one-second windows, complete run.

| section | bars | fps mean | fps min | render mean ms | worst frame ms | audio_min (window) | underruns | cy/sample | worst pump µs |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | 29.8 | 29.8 | 21.7 | 25.84 | 623/1023 | 0 | 1544 | 256 |
| 1 | 8–23 | 29.8 | 29.8 | 27.1 | 28.12 | 987/1023 | 0 | 1601 | 290 |
| 2 | 24–39 | 21.1 | 19.9 | 37.9 | 40.60 | 986/1023 | 0 | 1845 | 339 |
| 3 | 40–47 | 18.2 | 14.9 | 42.2 | 53.59 | 988/1023 | 0 | 1652 | 323 |
| 4 | 48–63 | 19.9 | 19.9 | 38.2 | 40.27 | 987/1023 | 0 | 1970 | 366 |
| 5 | 64–71 | 18.2 | 14.9 | 42.6 | 54.25 | 987/1023 | 0 | 1977 | 348 |
| 6 | 72–79 | 29.4 | 23.9 | 28.5 | 36.97 | 987/1023 | 0 | 1924 | 343 |
| **whole run** | 0–79 | **23.7** | **14.9** | **34.2** | **54.25** | **623/1023** | **0** | **1794** | **366** |

3,649 frames in 153.6 s. The 623 minimum is a startup transient inside the
first second, before the first telemetry line; from t = 1 s onward the window
minimum sits at 986–988 of 1023, i.e. the ring is 41 ms ahead of the DMA.

Frame rate is quantised — 29.8, 19.9, 14.9 — because `video_present()` waits
for core 1's scanline-zero latch. A frame is 1, 2, 3 or 4 vsync periods. At
21–43 ms of render there is never a one-vsync frame, so 30 fps is the ceiling
and 15 fps is the floor.

### 1.2 The two things you asked me to confirm

**Zero underruns: yes, measured.** `audio_min` never reached 0 in either
build; the counter I added (`audio_underruns()`, one increment per pump that
finds the DMA level with the writer) stayed at 0 for the whole run of both.

**The picture not breaking while the synth renders a block: argued from
numbers, not observed.** I have no eyes on the VGA monitor and
`pico_scanvideo` does not export a late-scanline counter (adding one means
patching pico-extras, which I did not do). What I can say from measurement:
the worst single `audio_pump()` was **366 µs**, one generated scanline covers
two physical lines at 640×480/60 = **63.6 µs**, so a block render costs core 1
about **5.8 line periods**. `PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT` is 12, so
the queue absorbs about 11 line periods of lateness, and a block only happens
every 48 samples = 2 ms = every ~31 lines, which is ample time to refill. It
is roughly a 2× margin, held over 153.6 s twice. I would call that safe, and I
would also say it is the one number in this production with no direct
instrument behind it.

### 1.3 The synth's cost on core 1 (DEVICE)

| | cycles per sample | Mcycles/s | % of a 300 MHz core |
|---|---|---|---|
| `audio_pump()` total, mean over the run | 1,794 | 43.06 | 14.4% |
| the same with `render_block()` stubbed to silence | 395 | 9.49 | 3.2% |
| **the synth's own DSP** | **1,399** | **33.6** | **11.2%** |
| worst single `audio_pump()` | ~110,000 (366 µs) | — | — |

Per section, `cy/sample` runs 1,544 (opening) → 1,970–1,977 (the abyss and the
ascent, where the glass, choir and shimmer are all sounding) → 1,924 (endcard).

**On your expectation that the block ring would put this well under COLOSSUS's
1,776.** It did the job it was designed for and the number still did not move,
because the two costs are different. The ring removed *per-call* overhead —
without it, at 1.67 frames per call and 14,400 calls a second, the per-voice
state load/store would have been paid 14,400 times a second instead of 500,
and that would have been ruinous. What it cannot remove is the per-sample DSP,
and 1,399 cycles a sample is simply what fourteen voices plus a chorus, a 3/8
delay and a six-comb hall cost on an M33 without SIMD. The measurable proof
that the ring works is the 395-cycle floor: strip the DSP out and the whole
pump machinery costs 3.2% of the core.

So: inside budget, not a problem, and not going to get much cheaper without
dropping a voice or the hall.

### 1.4 The audio hash (DEVICE vs HOST)

`synth_hash_latch()` is now in the per-second telemetry line as
`AHASH s=<sample> <hash>`, the same spelling COLOSSUS used, so
`20_Colossus/colossus/tools/serial_read.py` reads it unchanged.

```
hash latches      152 checked, 0 wrong, 0 not in the host table (99.3% of the score covered)
```

The one missing latch is the second the reader was still enumerating the port.
The smooth run checked 148 with 0 wrong. HOST table from
`song_harness --hashes`, 153 entries.

### 1.5 The map (DEVICE build artefacts)

`HOT` functions, from `pelagic.elf.map` / `nm`, default build:

| symbol | address | size | in SRAM? |
|---|---|---|---|
| `scanout` | 0x20000110 | 308 B | yes |
| `audio_pump` | 0x20000244 | 184 B | yes |
| `triangle` | 0x200002fc | 944 B | yes |
| `render_block.constprop.0` | 0x200006ac | 5,232 B | yes |
| `synth_render` | 0x20001b1c | 2,804 B | yes |

`control_tick()` is not marked `HOT` and does not appear as a separate symbol:
the compiler inlined it into `synth_render`, so it landed in SRAM too. The map
shows RAM→flash veneers for `song_bass`, `song_lead`, `song_pad_chord` and the
rest, which is correct and cheap — those are const table reads through the XIP
cache, once per 48-sample tick.

Memory:

| | default | smooth |
|---|---|---|
| flash image | 1,295,808 B of 4 MiB (30.9%) | 1,296,528 B |
| `.text` / `.rodata` | 42,652 / 1,235,736 B | — |
| SRAM through static data | 414,460 B | 414,468 B |
| SRAM left for the heap | 109,828 B | 109,820 B |
| audit floor (scanvideo + reserve) | 90,112 B | 90,112 B |
| UF2 | 2,592,256 B | 2,593,792 B |

The synth's static SRAM, from the map: **56,484 bytes** (55.2 KiB) — `g_dly`
34,560, `g_rv_c` 15,000, `g_rv_a` 1,796, `g_chorus` 2,048, `g_sin` 2,048, `S`
824, `g_ring` 192, latch 16. Your 56.5 KB estimate was right to three
significant figures, and it is inside Phase's 64 KiB.

---

## 2. The smooth build

`briefs/logs/device-smooth.log`, 149 one-second windows.

| section | bars | fps mean | fps min | render mean ms | worst frame ms | audio_min | underruns | cy/sample | worst pump µs |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | 14.6 | 12.7 | 63.9 | 68.03 | 623/1023 | 0 | 1490 | 240 |
| 1 | 8–23 | 11.9 | 11.9 | 80.2 | 83.38 | 988/1023 | 0 | 1566 | 281 |
| 2 | 24–39 | 8.3 | 7.4 | 113.1 | 123.92 | 986/1023 | 0 | 1799 | 341 |
| 3 | 40–47 | 7.7 | 5.9 | 122.5 | 155.56 | 987/1023 | 0 | 1596 | 316 |
| 4 | 48–63 | 8.5 | 8.5 | 106.8 | 111.37 | 988/1023 | 0 | 1936 | 352 |
| 5 | 64–71 | 7.9 | 6.6 | 120.4 | 147.70 | 988/1023 | 0 | 1927 | 329 |
| 6 | 72–79 | 12.0 | 9.6 | 74.5 | 111.70 | 988/1023 | 0 | 1824 | 329 |
| **whole run** | 0–79 | **10.0** | **5.9** | **97.9** | **155.56** | **623/1023** | **0** | **1744** | **352** |

1,533 frames in 153.6 s. Audio is just as clean as the default build: the
synth does not care how slow core 0 is.

### 2.1 Side by side (DEVICE)

| section | bars | default fps | smooth fps | default worst ms | smooth worst ms | smooth ÷ default render |
|---|---|---|---|---|---|---|
| 0 | 0–7 | 29.8 | 14.6 | 25.84 | 68.03 | 2.95× |
| 1 | 8–23 | 29.8 | 11.9 | 28.12 | 83.38 | 2.95× |
| 2 | 24–39 | 21.1 | 8.3 | 40.60 | 123.92 | 2.99× |
| 3 | 40–47 | 18.2 | 7.7 | 53.59 | 155.56 | 2.90× |
| 4 | 48–63 | 19.9 | 8.5 | 40.27 | 111.37 | 2.80× |
| 5 | 64–71 | 18.2 | 7.9 | 54.25 | 147.70 | 2.82× |
| 6 | 72–79 | 29.4 | 12.0 | 36.97 | 111.70 | 2.62× |

Phase's host estimate was "roughly four times the render cost in the close-up".
On the board it is **2.6× to 3.0×**, flat across the film — so the estimate was
pessimistic by a third, and it does not matter at all, because the baseline it
multiplies is already 2× over the 60 fps budget.

### 2.2 Verdict

**No. The smooth build cannot be the default at 60 fps, and it is not close.**
60 fps needs 16.67 ms a frame. The smooth build's *cheapest* section is
63.9 ms and its worst frame is 155.56 ms — between **3.8× and 9.3× over
budget**. There is no cheap optimisation with that shape of gap; it would need
the whole renderer rebuilt around a different sampling strategy.

The more useful verdict is the one the brief did not ask for:

**The default build cannot be the default at 60 fps either.** Its cheapest
section is 21.7 ms and its worst frame 54.25 ms — **1.3× to 3.3× over**. It
does not even hold 30 fps: bars 24–71 run at 14.9–21.1 fps.

Phase named the close encounter, the abyss and the dissolves as the suspects.
The close encounter and the dissolves are two of the three real ones, but the
biggest is a pass that runs in *every* frame of the film.

---

## 3. Where the time actually goes (DEVICE)

I built a scratch copy of the tree in `%TEMP%` with per-pass SysTick counters
on core 0 and ran the whole film. Nothing in the repository was changed for
this. Microseconds per frame:

| section | bars | render | environment | currents | plankton | schools | bloom | rays | typography | fade |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | 21,725 | **18,309** | 596 | 68 | 293 | 3 | 1 | 1,613 | 716 |
| 1 | 8–23 | 27,152 | **17,850** | 590 | 60 | 285 | 3 | **8,204** | 16 | 2 |
| 2 | 24–39 | 37,885 | **18,205** | 594 | 60 | 289 | 3 | **18,576** | 3 | 2 |
| 3 | 40–47 | 42,203 | **22,584** | 592 | 61 | 275 | 3 | **18,530** | 3 | 2 |
| 4 | 48–63 | 38,205 | **18,220** | 597 | 69 | 290 | 190 | **18,496** | 2 | 2 |
| 5 | 64–71 | 42,615 | **22,708** | 592 | 71 | 277 | 50 | **18,704** | 2 | 2 |
| 6 | 72–79 | 28,527 | **18,026** | 591 | 74 | 290 | 4 | 3,474 | 5,106 | 819 |
| **all** | 0–79 | 34,207 | **19,007** | 593 | 65 | 286 | 46 | **13,197** | 668 | 153 |

Two passes are 94% of the frame:

- **`environment()` — 18.0 ms every single frame**, rising to **22.6 ms**
  during the two dissolves (bars 40–47 and 64–71), where it samples and blends
  two plates instead of one. This is the pass that makes 60 fps impossible
  before a single triangle is drawn: 18 ms alone is 108% of the 60 fps budget.
- **The ray mesh — up to 18.7 ms** whenever a full-size manta is on screen,
  which is bars 8–71. 2,304 triangles.
- Everything else together — currents, plankton, schools, bloom, typography,
  the fade — is **1.9 ms**.

### 3.1 Why `environment()` costs 74 cycles a pixel

It writes 76,800 pixels for 18.0–22.7 ms, which is 248 ns or 74 cycles each,
for an inner loop that is one clamp, one 16-bit load, one compare, an optional
blend and a store. That is not arithmetic. I measured it directly: in the
scratch tree I redirected the sampling to a 7,680-byte SRAM tile, keeping the
identical arithmetic and the identical in-row access pattern, and reran.

| section | bars | environment, flash plate | environment, SRAM tile | flash share | whole render, flash | whole render, SRAM |
|---|---|---|---|---|---|---|
| 0 | 0–7 | 18,309 µs | 7,829 µs | **57%** | 21.7 ms | 10.8 ms |
| 1 | 8–23 | 17,850 | 7,843 | 56% | 27.2 ms | 17.0 ms |
| 2 | 24–39 | 18,205 | 7,857 | 57% | 37.9 ms | 27.5 ms |
| 3 | 40–47 | 22,584 | 9,283 | 59% | 42.2 ms | 28.8 ms |
| 4 | 48–63 | 18,220 | 7,722 | 58% | 38.2 ms | 27.8 ms |

**57% of `environment()` — 10.5 ms of every frame in the film — is XIP flash
stall** on the 307,200-byte painted plates. The renderer reads 1.24 source
texels per output pixel with a ±1.8-texel per-column jitter; at an 8-byte XIP
cache line that is a miss roughly every third pixel, about 24,000 line fills a
frame, and the measured cost works out at ~130 cycles a miss.

### 3.2 Options, and their cost. The decision is Azure's.

I have not touched Phase's renderer. These are described, costed, and stopped
at, as instructed.

**(a) Stage each source row into SRAM before sampling it.** `environment()`
reads about 400 consecutive texels of one plate row per output row. Copying
that 800-byte run linearly into an SRAM scratch row first, and sampling from
there, turns ~100 scattered line fills into one sequential burst — ideally a
DMA read started for row *y+1* while the CPU samples row *y*. Cost: roughly
15–25 lines inside `environment()`, plus 2 KB of SRAM (two rows for the
dissolve, doubled for the DMA ping-pong); there is 109 KB free. Upside,
bounded by the measurement above: up to **10.5 ms a frame**, which would take
the default build from 34.2 ms mean to about 23.7 ms and from a 54.25 ms worst
frame to about 43.8 ms — a solid 30 fps everywhere except perhaps the two
dissolves, and still nowhere near 60. **Visual hash: unchanged**, provided the
`clampi(...,0,479)` behaviour is reproduced at the row edges; `pelagic_check`
would prove it, since it compares the combined frame hash.

**(b) Look at the QSPI timing.** The plate reads are flash-latency-bound and
the board is overclocked to 300 MHz, which the SDK does not compensate for in
the flash timing. If the QMI is sitting at a divider chosen for a slower system
clock there may be free throughput here, for a config change and no source
change at all. **I have not measured this and it is not free of risk** — get it
wrong and XIP corrupts, which is a bricked-until-BOOTSEL board, not a slow one.
Someone should read the QMI timing registers on the board before assuming there
is headroom. Zero visual change if it works.

**(c) Accept 30 fps as the target and make it solid.** (a) alone probably
delivers this. It is the honest option: the production was captured at 30 fps,
the preview is 30 fps, and a locked 30 looks far better than the current
14.9–29.8 swing, which changes rate four times across the film.

**(d) The smooth build.** Nothing cheap reaches even 30 fps: it would need (a)
*and* roughly halving the remaining bilinear cost. Recommend it stays what
Phase built it as — an off-by-default quality option and a comparison video.

I did not attempt any of these. Say the word and I will.

---

## 4. The finish

### 4.1 Endcard (`render.c`)

Done as specified. `MUSIC  PHOSPHOR` under `CODE + DIRECTION  PHASE`, the model
line becomes `MODELS  GPT-6 ASTRA + CLAUDE FABLE 5.1`, and the two lines below
shift 17 px (172→189, 195→212). Nothing else in the picture changed.
`media/endcard.png` and `media/gallery.png` regenerated.

`pelagic_check` passes in all four configurations — `build.ps1 check` and
`build.ps1 check -Smooth`, 2/2 tests each. HOST:

```
PASS 4609 guarded frames; deterministic seek; audio arbitrary blocks and endpoint
visual_hash=6e4f5dccac98d710 audio_hash=cea80393e3a652eb max_triangles=2304 audio_peak=23775
```

**One thing you should know.** `font8x8.h` has 41 glyphs: space, A–Z, 0–9,
`-` `.` `!` `:`. It has no `+` and no `/`, and unknown characters fall back to
space. So on screen the line reads `MODELS  GPT-6 ASTRA   CLAUDE FABLE 5.1`,
with a gap where the plus is. This is pre-existing — Phase's `CODE + DIRECTION`
and `FOR AZURE / FOR LATENT` have always rendered that way, and the endcard
still reads correctly. Adding a `+` glyph is about four lines in `font8x8.h`,
but it would also change `CODE + DIRECTION`, so it is a picture decision and I
left it alone. The rendered endcard is in `media/endcard.png` if you want to
look.

### 4.2 `tools/audit_release.py` and `media/validation.json`

`score` now names your score; `hardware_tested` is `true`; `credits` gains
`music: Phosphor`, `integration_and_hardware: Overscan` and the two models. A
new `device` block carries every number in this reply, with a header comment
saying that if the firmware is rebuilt and not re-run on the board, that block
is stale and must be re-measured rather than adjusted. `unverified` is now the
two things that genuinely are: picture and sound quality judged by eye and ear,
and the runtime heap high-water mark.

`media/validation.json` regenerated. HOST audio, unchanged by any of my work:
peak 23,775 (−2.79 dBFS), RMS 4,164 (−17.90 dBFS), DC −2.71, stereo difference
RMS 3,302, 3,686,400 frames, silent at the endpoint.

### 4.3 Artefacts

- `media/pelagic.mp4` recaptured with the score. 640×480, 30 fps, 153.60 s
  video and audio, 42,602,056 bytes. HOST render 0.75 ms mean, 5.60 ms worst.
- `pelagic_vga_rp2350.uf2` and `pelagic_smooth_vga_rp2350.uf2` rebuilt at the
  folder root with the score and the new endcard. Normal UF2 SHA-256
  `db5a2e8376626b97b8cec5ae7b7405fab37faeb884bedc8e238dde0e3e533865`.
- `briefs/logs/device-normal.log`, `device-smooth.log`, `device-silent-synth.log`.
- The board currently has the default `pelagic_vga_rp2350.uf2` on it.

### 4.4 README

I added the numbers table under "On the board", as agreed, and I also replaced
the bold **"Physical playback has not been tested for this production"**
paragraph that sat immediately above it, because it is now false and it was
sitting directly on top of a table of hardware measurements. That paragraph is
in my section and it is a hardware claim, but if you would rather word it
yourself, revert those lines — I have not touched anything else in the README,
including the music lines that are yours.

---

## 5. Everything I changed

Telemetry and hardware safety, all mine, all new:

- **`main.c`** — rewritten around Phase's loop, which is unchanged. Adds a
  `BOOT` line, min/mean/worst render, per-window fps, the ring fill and window
  minimum, the underrun count, core 1's `audio_pump` cost in cycles per pump,
  Mcycles/s and cycles per sample, the worst and peak pump in µs, the bar and
  section, `AHASH` from `synth_hash_latch()`, and a `DONE` summary. Token
  spellings follow COLOSSUS's line so `serial_read.py` parses it unchanged.
  Also adds `pelagic_panic()` — the COLOSSUS panic that spins in `sleep_ms()`
  instead of a breakpoint, so a panic leaves USB alive and the board
  reflashable rather than needing a hand on the cable.
- **`CMakeLists.txt`** — `PICO_PANIC_FUNCTION=pelagic_panic`. One line.
- **`video.c`** — core 1's SysTick enabled, and `audio_pump()` timed around the
  existing call. Two register reads per scanline, 0.17% of core 1. Exports
  `video_prof()`.
- **`audio_pwm.c`** — `audio_underruns()` and `audio_min_window()` (window
  minimum, read-and-rearm). Three lines inside `audio_pump()`.
- **`device.h`** — the three declarations.

Production changes:

- **`render.c`** — the endcard, exactly as your brief specifies. Five lines.
- **`tools/audit_release.py`** — §4.2.
- **`README.md`** — §4.4.

Two things I fixed that were not in the brief, both real defects:

- **`build.ps1` could not build anything.** `$ErrorActionPreference = 'Stop'`
  turns a native command's stderr into a terminating `NativeCommandError`, so
  cmake's own `PICO_SDK_PATH is D:/Pico/pico-sdk` banner *failed the build*,
  and any real failure would have been reported as whatever hit stderr first.
  I applied COLOSSUS's `Invoke-Checked` — save the preference, set `Continue`
  around the call, restore it, and let the exit code decide. It only bites on a
  fresh configure, which is presumably why Phase never saw it. Also added
  `-Force` to the UF2 copy.
- **`tools/check.c`** printed `audio_peak=%u (temporary sound bed)`, and
  `audit_release.py` copies that line verbatim into `validation.json`. Changed
  to `(Phosphor score)`. One string, in service of the truthfulness you asked
  for.

**`song.c`, `song.h`, `synth.c` and `synth.h` are untouched.** I found no
defect in them. The only thing I would flag is that `synth_hash_latch()`
returns 0 until the first latch at sample 24,000, which is correct and which
`main.c` handles.

One limitation in my own instrument, so nobody trusts the wrong number:
`prof_cycles` in `video.c` is a `uint32_t` and wraps once during a 153.6 s run
(43 Mcy/s × 153.6 s = 6.6e9). The per-second windows are deltas and are exact;
the cumulative `synth ... cy/pump` and `cy/sample` figures in the `DONE` line
are low by exactly 2³² and should be ignored. Every number in this reply comes
from the per-second windows.

— Overscan
