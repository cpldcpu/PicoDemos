# COLOSSUS — Overscan's platform report

To: Phosphor (director). From: Overscan (Claude Opus 5). Date: 2026-09-06.

Every number below says where it was measured. **HOST** means this desktop
(MinGW gcc 15.2, Windows) or the linker map. **DEVICE** means the Pico 2 on
COM10.

The board locked up partway through this work and Azure replugged it with
BOOTSEL held; **§5** says what locked it up, **§7** how it came back, and
**§8** is the whole hardware run that followed — the stub over the full 5:07,
both scanout modes, the synth's cost on core 1, the heap and the measured boot
floor, referee 2's hash diff, Phase's renderer per phrase, and the material
ceiling. Sections 1–3 were written before the board came back and are HOST
measurements; every DEVICE number is in §8.

---

## 1. What I built

Mine, all under `d:\Toyprojects\PicoDemos\20_Colossus\`:

| File | What it is |
|---|---|
| `build.ps1` | both targets, flash, run, capture, video, the heap-floor run |
| `colossus/CMakeLists.txt` | host + RP2350; `include(render.cmake)` for Phase's list |
| `colossus/platform.h` | my side of `demo.h`: video, audio, telemetry, heap |
| `colossus/main.c` | core 0, the clock, the telemetry, `cv_panic()` |
| `colossus/video.c` | core 1 scanout, page handshake, the doubling copy |
| `colossus/audio_pwm.c` | 24 kHz stereo PWM through DMA, pumped from core 1 |
| `colossus/host/stub_demo.c` | the platform's stand-in renderer |
| `colossus/host/stub_font.h` | 8x8 font for it (copied from PERSISTENCE) |
| `colossus/host/img_write.h` | PNG (stored-deflate) and PPM, no image library |
| `colossus/host/player.c` | SDL2 player: audio, seek by phrase, pause, screenshot |
| `colossus/host/Makefile` | the WSL path: plain `gcc`, no cmake, no SDL |
| `colossus/tools/capture.c` | whole-run capture: WAV, hashes, stills, raw, bench |
| `colossus/tools/ledger_check.py` | LEDGER.md against the ELF map and the device |
| `colossus/tools/serial_read.py` | telemetry + referee 2, with a `--replay` mode |
| `colossus/tools/serial_probe.py` | raw dump, for when there is nothing to parse |

I have not touched `render*`, `body*`, `scene_*`, `assets/`, `song.c`,
`synth.c`, `demo.h` or `LEDGER.md`. Phase's `render.cmake` landed while I was
writing and my `CMakeLists.txt` picked it up with no edit, as intended.

### Core split

VESPER's, with the full 240 rows. Core 0 asks the DMA what sample is playing,
calls `demo_render(page, sample)`, then `video_present()`, which publishes the
page and blocks until core 1 latches it at a scanline zero. Core 1 does
nothing but generate scanlines and call `audio_pump()` in the gap after handing
each buffer back.

Two things there are not VESPER's:

**The mode is `vga_mode_320x240_60`, not `vga_mode_640x480_60`.** Its `yscale`
is 2, and `scanvideo.c` repeats a generated scanline buffer for the second
physical line by re-triggering the same DMA (`y_repeat_target`, line 816). So
core 1 generates **240** buffers a frame instead of 480 for exactly the same
picture. `xscale` is informational in the DPI backend — the buffer still has to
carry 640 pixel clocks — so the horizontal doubling is still ours. This should
halve the scanout cost; `-DCOLOSSUS_SCANOUT_640=ON` builds VESPER's
arrangement so the two can be measured against each other on the board, and
that image is built and waiting (`colossus_stub640_rp2350.uf2`).

**The doubling writes 32-bit words.** Display pixel *x* takes `src[x>>1]`, and
because the `RAW_RUN` header puts pixel 0 in the token's argument, `out[4]`
onward is 4-byte aligned and every source pixel from 1 to 319 is one
`v | (v<<16)` store: 319 stores per line, not 639.

### Audio

Exactly 24,000 Hz: 300,000,000 / 24,000 = 12,500 sys cycles a sample, no
remainder, from one DMA timer feeding two channels on two slices (GP28 slice 6A,
GP27 slice 5B) started by one mask write, GP26 held low. Finite transfer count
(`CV_TOTAL_SAMPLES + CV_RATE = 7,396,800`), because all-ones selects ENDLESS on
RP2350 and that counter is the clock the picture follows.

PERSISTENCE had to pull its rate 0.4% flat to keep an integer number of samples
per video frame. COLOSSUS does not need that and must not do it: here the
picture is a function of the sample the DAC is playing, so the video may run at
whatever 59.75 Hz it likes while the audio stays at the rate the host renders
at — which is what lets the host WAV and the device output be the same bytes.

The ring is 512 frames a side (21.3 ms, 4,096 B for the pair, which is exactly
what LEDGER.md reserved). A pump happens once per generated scanline, about
14,340 times a second, so the ring only needs 1.7 frames of work each time;
rather than call `synth_render()` for one or two frames at that rate, the pump
waits for 16 frames of room and fills up to 64. Underruns are counted as
"the DAC has played a sample the synth never wrote", because a ring does not
fault when that happens, it repeats 21 ms of the past.

### Telemetry

One line a second (`T ...`) and one at each phrase boundary (`PHRASE ...`),
plus `BOOT ...` lines and a final `DONE ...`. Each carries: min/avg/**max**
render time, frames per second, the worst page **hold** in refreshes, `miss`
(held 3+ refreshes = under the 30 fps floor), `late` (held 2+ = under 60), the
worst displayed-frame **gap** in ms, audio underruns and ring fill, **cycles
per scanline copy** and Mcycles/s for it, **cycles per pump**, Mcycles/s and
cycles per audio sample for the synth on core 1, Phase's `demo_stats()`, and
the synth hash latch.

---

## 2. How to build and run

```
.\build.ps1 host              SDL player + capture tool (renderer)
.\build.ps1 host  -Stub       the same against the platform's stub
.\build.ps1 pico              colossus_vga_rp2350.uf2
.\build.ps1 pico  -Stub       colossus_stub_rp2350.uf2
.\build.ps1 pico  -Stub -Scanout640     VESPER's 480-copy scanout
.\build.ps1 flash             picotool reboot -f -u, then load -x
.\build.ps1 run   -Seconds 330          flash, then read the whole score
.\build.ps1 floor -Stub -Ballast N      the heap-floor experiment
.\build.ps1 capture           WAV, per-second hashes, one still per phrase
.\build.ps1 video             the whole run to media/colossus.mp4
```

Stub and renderer have separate build trees and separate `.uf2` names; they are
two different programs and flipping one cmake cache between them wastes minutes
and eventually lies about what was built.

**WSL, for Phase:** `make -C colossus/host` builds the capture tool with plain
`gcc`, no cmake, no SDL and no image library (`make player` adds SDL if
`sdl2-config` is on PATH; `make stub=1` forces the stub). The renderer source
list is read out of `render.cmake` with `sed` rather than globbed, so
`render_host.c` and `render_checks.c` stay out of it as `render.cmake` asks.
Verified here by running that exact Makefile under MSYS `bash` + `cc`: it built
`capture` and wrote twenty phrase stills.

**Capture:**

```
capture --wav score.wav                 the whole 5:07 at 24 kHz stereo
capture --hashes hashes.txt             the per-second FNV table (referee 2)
capture --out DIR --phrases             one still at each phrase downbeat
capture --out DIR --bars 0,24,40,128    or --seconds, or --samples, or --every N
capture --bench 2000                    host cost per frame, writes nothing
capture --raw --fps 60 | ffmpeg ...     the whole run for the MP4
```

PNG is written directly: a real zlib stream of stored deflate blocks plus
CRC32, about sixty lines, so the tool needs nothing but libc.

**Player** (`build_host/player.exe`): space pauses, left/right seek a phrase,
`,`/`.` a bar, digits 1–9 and 0 jump to phrases 1–10 (shift for 11–20), `s`
writes a native-size PNG, `n`/`g` resize, `f` fullscreen. Seeking is the test
of `demo.h`'s purity: `synth_seek()` renders and discards to the target so the
music is bit-identical to a straight play-through, and `demo_render()` is
handed the new sample with nothing else changed. If a scene looks different
after a seek than it did on the way past, the renderer is keeping state it
promised not to keep.

---

## 3. What I measured — HOST

**Audio, and referee 2's host half.**

- The whole score renders to WAV in **1.03 s** (HOST) for 5:07.2 of 24 kHz
  stereo — about 300x realtime.
- `media/colossus.wav` from my `capture --wav` is **byte-identical** to your
  `media/colossus_score_v2.wav` (SHA-256 `51D47013…956E5D`, HOST). The tool
  renders your synth, not an approximation of it.
- **307** per-second hash latches over the 7,372,800 frames. The table is
  **identical** whether the executable was linked against Phase's renderer or
  against my stub (HOST) — the synth does not depend on the picture, which is
  the assumption the whole referee rests on.
- `tools/song_check.py` still passes end to end: block-size independence, no
  clipping, ends in silence, overall peak 29,914 (91.3%).

**The referee can fail.** I fed `serial_read.py --replay` two synthetic device
logs built from the host table, one clean and one with a single bit flipped in
one of the 307 hashes. Clean: `307 checked, 0 wrong`, exit 0. Corrupted:
`307 checked, 1 wrong … sample 3624000: device 70a52060 host 70a52061`, exit 1.
An unfalsifiable referee is not a referee, and I would rather have shown that
before the run than after it.

**Render cost, HOST, for scale only** (a desktop core is not a Cortex-M33 and
these do not predict the device):

- the stub: **0.079–0.083 ms** per frame over 3,000 frames
- Phase's renderer at this commit: **1.190 ms** per frame over 2,000 frames

**Static memory, from the ELF map (HOST).**

| | stub build | renderer build |
|---|---:|---:|
| main SRAM, ours | 359,932 | 449,136 |
| main SRAM, SDK + newlib + TinyUSB | 14,479 | 14,439 |
| SCRATCH_X/Y (two 4 KB core stacks) | 8,192 | 8,192 |
| **heap region** (`__StackLimit − __end__`) | **150,880** | **61,616** |

Ours, by source, in the renderer build: `video.c` 307,628 (the two pages plus
372 bytes of SRAM scanout code), `render.c` 89,236, `synth.c` 46,856,
`audio_pwm.c` 4,680, `body.c` 736.

Symbols at or above 1 KB: `g_pages` 307,200 · `r_depth` 76,800 · `g_dly`
17,280 · `g_rv_c` 15,000 · `render_block` 5,144 · `r_blur` 4,800 · `r_glow`
4,800 · `synth_render` 2,704 · `s_left` 2,048 · `s_right` 2,048 · `g_chorus`
2,048 · `g_sin` 2,048 · `r_shades` 2,048 · `g_rv_a` 1,796.

**The allocation the linker cannot see.** `scanvideo_setup()` allocates
`PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT` buffers of
`PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS` words each
(`scanvideo.c` line 1345). I build with 8 and 324, so it is
**8 × 324 × 4 = 10,368 bytes** of buffer — read out of the SDK source, HOST,
and predicting a heap of 61,616 − 10,368 = 51,248 after `video_init()` on the
renderer build. The device says **49,152**: newlib's malloc took 2,096 bytes
more than the buffers themselves, in chunk headers and a top pad. Close enough
to be worth predicting, wrong enough to be worth measuring — §8.5. LEDGER.md
reserved 21,504, which is right for sixteen buffers, not this build's eight.

---

## 4. What §3 could not measure

Section 3 was written while the board was locked up. Everything it lists as
missing was measured afterwards and is in **§8**: cycles per scanline copy in
both scanout modes, the synth's cost on core 1, free heap at boot and after
`video_init()`, the boot floor by ballast bisection, the hash diff over the
full score, the frame telemetry with the stub, and the renderer and the
material ceiling on the device.

## 5. What surprised me

**The heap probe panicked the board, with the exact panic it was written to
prevent.** `main.c` originally measured free heap by malloc'ing bigger and
bigger blocks until one failed. `PICO_MALLOC_PANIC` defaults to 1: the SDK
treats a failed allocation as fatal. So the instrument called
`panic("Out of memory")` — PERSISTENCE's postmortem message, produced
deliberately, by me, on purpose, as the measurement. It now asks `_sbrk(0)`
where the break is, which costs nothing, cannot fail, and measures the
unallocated tail, which is the number that decides whether scanvideo's malloc
succeeds anyway.

**The stock panic makes the board unrecoverable, and that is the more important
bug.** `panic()` ends in `bkpt #0`. With no debugger attached that escalates to
a HardFault, whose handler spins at priority −1 with every other interrupt
masked, USB included. The board is still enumerated — Windows lists COM10 as
OK — but it answers nothing: opening the port blocks in the kernel until the
process is unkillable, and `picotool reboot -f -u` prints "the device was asked
to reboot" and nothing happens, because picotool prints that unconditionally
(a successful reboot cannot be ACKed). I confirmed the CPU is dead rather than
the host being wedged: after a reboot request the USB device node never
disappears for even 200 ms out of five seconds.

So `PICO_PANIC_FUNCTION` now points at `cv_panic()` in `main.c`, which prints
the message, prints the heap numbers, and then spins in `sleep_ms()` with
interrupts on and the LED blinking fast. USB stays alive, the message is
readable, and `picotool reboot -f -u` still works. **A panic you can still talk
to** is what makes the ballast bisection safe to run at all: the whole
experiment is "add dead .bss until it stops booting", and with the stock panic
the first failing arm costs a human.

**`vga_mode_320x240_60` halves the scanout work and nobody here was using it.**
Both earlier demos scanned out through the 640x480 mode and shifted the row
index, generating 480 line buffers a frame. The 320x240 mode's `yscale=2`
makes scanvideo re-trigger the same buffer for the second physical line, so 240
buffers produce the identical picture. It cost one line of code to switch and I
kept the old path behind a flag so the saving can be measured rather than
asserted.

**I got the `RAW_RUN` length wrong in a way that read as correct.** VESPER
writes `n = 638` for a 640-pixel line, which looks off by two, so I "fixed" it
to 637. The PIO's comment is `| jmp raw_run | colour | n | n+2 colours |`, and
the loop is `jmp x--`, which tests before decrementing and therefore runs
*n+1* times: the run emits n+3 pixels. But `COMPOSABLE_EOL_ALIGN` has to land
on an **odd** halfword index — the `||` in `scanvideo.pio`'s comments is a word
boundary and the token is what aligns to it — and 640 pixels puts it on an even
one. VESPER's 638 emits 641 pixels, the last one black and spent in the
16-pixel front porch, precisely so the end-of-line token stays word-aligned.
My 637 would have handed the state machine a pixel as a jump target. It is back
to 638 with the reasoning written down, because I will not remember this in a
week either.

**`scanvideo`'s buffer count is 8 by default and both earlier demos raised it.**
VESPER used 12, PERSISTENCE 16. At 240 generated lines a frame each buffer
covers 63.5 µs of display, so eight is about 440 µs of slack for core 1 —
comfortably more than the ~17 µs a 16-frame synth burst costs — and it hands
11 KB back to a heap that is down to 61,616 B.

---

## 6. The ledger, and one thing left for Phase

**Done, with your authorisation.** `LEDGER.md`'s platform and synth rows now
carry device-linker sizes instead of reservations, and it has a *Measured*
column and a measured-totals section under the table. `ledger_check.py` reads
**OK** against it. What changed:

- the platform pages row now names `video.c` and `g_pages`, and a new row
  carries the 428 B of scanout hot code and state that go with them;
- the audio row is **4,680** rather than 4,096, itemised: rings 4,096, synth
  block `s_tmp` 256, hot code 300, state 28;
- the "platform globals, USB, SDK, alignment" reservation of 4,096 becomes the
  measured **13,536** — 12,391 of SDK, newlib and TinyUSB globals plus 1,145
  of inter-section alignment, which is real and was not accounted for anywhere;
- `g_chorus` is **2,048**, not the 4,096 the row assumed;
- "Synth SRAM hot code (reservation) 8192" becomes the measured **7,848**:
  `render_block` 5,144 and `synth_render` 2,704, both from `CV_HOT`;
- scanvideo's heap allocation is **11,616 measured on the device**, not the
  21,504 estimated for sixteen buffers;
- the core stacks are marked *scratch*, because SCRATCH_X and SCRATCH_Y are
  not part of the 512 KB the heap comes out of and charging them against it
  double-counts 8 KB;
- the boot-floor paragraph is replaced by §8.5's measurement.

**Left for Phase in round four.** Two renderer rows are still ceilings, and
both are generous, so the ledger's total is 7,656 B more pessimistic than the
image: renderer hot code reserves 8,192 and measures **704** (`r_bloom`
alone), and renderer context reserves 256 and measures **88**. `r_bloom` is
now named in the row so the check passes; the reservations are Phase's to
revise. Everything else of Phase's — `r_depth`, `r_glow`, `r_blur`,
`r_shades`, `body_cache`, `body_joints` — measures exactly what it reserved.

**The contract I assumed for `render.cmake`,** since I had to write
`CMakeLists.txt` before it existed: it sets `COLOSSUS_RENDER_SOURCES` (a list
of `.c` files; `RENDER_SOURCES` is accepted as an alias) and may set
`COLOSSUS_RENDER_INCLUDES` and `COLOSSUS_RENDER_DEFINES`. Phase's file
matches, so nothing needs to change — recorded so it stays true.

`song.c`'s section name is already "the load"; the `run_stub.log` from the
first whole-score run still prints "the forge" because that image was built
twenty minutes earlier. The renderer and material logs have it right.

---

## 7. The board, and how it came back

Azure replugged the Pico 2 with BOOTSEL held, which is the only thing that
recovers a board whose CPU is sitting in a HardFault. Everything below was
measured after that, on the firmware in the tree now: `cv_panic()` instead of
the stock breakpoint, `_sbrk(0)` instead of the malloc probe, and `n = 638`
back in the scanline.

Three whole-score runs, one 100-second run and nine flash-and-boot arms later
the board is still answering, which is the point of the panic change: the
floor bisection deliberately runs the firmware out of memory five times, and
not one of those arms cost a hand.

---

## 8. What I measured — DEVICE

Everything in this section is measured on the Pico 2 on COM10 at 300 MHz and
1.20 V, over USB CDC, by `tools/serial_read.py`. The logs are in `media/`:
`run_stub.log`, `run_stub640.log`, `run_render.log`, `run_material.log`.

### 8.1 The platform alone, the whole 5:07 (referee 3)

`colossus_stub_rp2350.uf2` — the stub renderer, which writes all 76,800 pixels
every frame, so this is the platform plus an honest page fill.

```
DONE frames=18355 render_max_us=6300 gap_max_us=16768 miss=0 late=1 under=0
     min_fill=464 copy_worst_cy=2978 pump_worst_cy=76551 vsyncs=18359
     heap_free=139264 peak=29914
```

| | Measured |
|---|---|
| Frames drawn / display refreshes | **18,355 / 18,359** over 307.2 s |
| Frame rate, every one-second window | **59.7 fps**, no exceptions |
| Render time, min / mean / max | **6.18 / 6.26 / 6.30 ms** |
| Worst displayed-frame interval | **16.768 ms** (one refresh at 59.7 Hz) |
| Missed deadlines: below 30 fps / below 60 | **0** / **1** |
| Audio underruns | **0** |
| Shallowest the audio ring ever got | 464 of 512 frames, 19.3 ms |

The single sub-60 frame is the first present: page 0 is published before
`audio_start()`, so it waits one extra refresh for the DMA. It is real and I
am not hiding it, but it is a start-up artefact, not a dropped frame.

`peak=29914` is the synth's peak sample on the device. `song_check.py` reports
29,914 on the host.

### 8.2 Referee 2: the audio hash over the whole score

The device latched **306 of the 307** per-second FNV marks and printed them;
`serial_read.py` diffed each against `media/hashes.txt` from
`capture --hashes`:

```
hash latches   306 checked, 0 wrong, 0 not in the host table (99.7% covered)
```

The renderer run independently checked **305, 0 wrong**. The one or two
missing are not disagreements: the latch is "most recent", the device prints
once a second, and the print clock drifts slowly against the 24,000-sample
latch clock, so occasionally a mark is overwritten between prints. Every mark
that was reported matched, over the full 5:07, on two separate runs. Referee 2
passes.

### 8.3 Cycles per scanline copy — and the mode change was worth it

Same firmware, same score, one `#ifdef` apart:

| Scanout | Buffers per frame | Cycles per line | Core 1 cost | Core 0 render |
|---|---:|---:|---:|---:|
| `vga_mode_320x240_60`, yscale 2 | 240 | **2,441** (worst 2,978) | **34.9 Mcy/s** | 6.26 ms |
| `vga_mode_640x480_60`, `y>>1` | 480 | **2,638** (worst 2,902) | **74.4 Mcy/s** | 6.32 ms |

The yscale mode hands back **39.5 Mcycles a second — 13.2% of a core** — for
the same picture. It is not quite half, because the per-line copy is 8%
*slower* when you do it twice as often: the scanline DMA is reading those
buffers out of the same striped SRAM the copy is writing to, and at 28,680
lines a second it wins more of the arbitration. The same contention shows on
the other core, where core 0's identical render goes from 6.26 to 6.32 ms.

2,441 cycles for 319 32-bit stores plus the header is 7.6 cycles a store,
which is what SRAM-to-SRAM costs here once the DMA is in the way.

### 8.4 The synth on core 1

| | Measured |
|---|---|
| Cycles per audio sample | **1,297** (overture) to **1,739** (the spine) |
| Cycles per second at 24 kHz | **31.1 to 41.7 Mcy/s** |
| Cost per `audio_pump()` call | 2,170 to 2,960 cycles, about 14,340 calls a second |
| Worst single pump | **76,551 cycles**, 255 µs |

Core 1's total fixed cost is scanout plus synth — **76.5 Mcycles a second,
25.5% of one core** — and the rest is slack against exactly that worst pump. A
64-frame fill costs 255 µs, eight scanlines' worth of time; the eight queued
scanline buffers absorb it, and `min_fill` never dropped below 464 of 512. If
`PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT` is ever reduced, that 255 µs is the
number the queue has to stay above.

### 8.5 The heap, and the boot floor — measured, not inherited

Free heap at boot, from `_sbrk(0)`, printed by `main.c`:

| Build | Heap region | After the inits | After `video_init()` | scanvideo took |
|---|---:|---:|---:|---:|
| stub | 150,880 | 150,880 | **139,264** | 11,616 |
| renderer | 61,616 | 61,616 | **49,152** | 12,464 |
| material test | 62,352 | 62,352 | **49,152** | 13,200 |

Nothing but pico_scanvideo allocates: the synth, the audio rings and Phase's
renderer are all static, and the break does not move between `main()` and
`video_init()`. The three amounts differ by a kilobyte or two because newlib's
malloc takes a top pad when there is room for one.

**The floor**, by ballast bisection — `build.ps1 floor -Ballast N` links N
bytes of dead `.bss`, flashes it, and watches:

```
ballast 130000  heap 20880  -> BOOT      ballast 140312  heap 10568  -> BOOT
ballast 137500  heap 13380  -> BOOT      ballast 140546  heap 10332  -> PANIC
ballast 139375  heap 11504  -> BOOT      ballast 140781  heap 10096  -> PANIC
                                         ballast 141250  heap  9628  -> PANIC
                                         ballast 145000  heap  5880  -> PANIC
```

**A heap of 10,568 bytes boots and reaches the main loop. 10,332 panics with
"Out of memory" inside `video_init()`.** The threshold is somewhere in that
236-byte gap; `tools/ledger_check.py` enforces the number that has actually
been seen to work.

PERSISTENCE's inherited 79 KiB was never a floor. It was the heap that one
build happened to have, with sixteen scanline buffers against this build's
eight. Carrying it forward would have condemned the shipping build, which
boots with **49,152 bytes free, 38,584 above the floor**. `ledger_check.py`
now reads `ledger_check: OK` against `LEDGER.md`, which I have updated with
the measured platform and synth rows as you authorised.

### 8.6 Phase's renderer, the whole 5:07 (referee 3)

`colossus_vga_rp2350.uf2`, round three as committed: every chapter,
transitions, credits.

```
DONE frames=5671 render_max_us=108056 gap_max_us=117153 miss=4503 late=5671
     under=0 min_fill=465 vsyncs=18365 heap_free=49152 peak=29914
```

**5,671 frames against 18,365 refreshes: 18.5 fps over the run. 4,503 of them
— 79% — were held for three refreshes or more, which is below the 30 fps
floor. The worst single frame took 108.05 ms.** Per phrase:

| Ph | Bar | Chapter | render min/mean/max ms | fps | below 30 | tri | fill |
|---:|---:|---|---|---:|---:|---:|---:|
| 2 | 8 | the plain | 40.5 / 43.5 / 95.4 | 19.0 | 296 | 74 | 11,634 |
| 3 | 16 | the plain | 64.3 / 66.3 / 95.4 | 14.4 | 222 | 70 | 3,132 |
| 4 | 24 | the hand | 64.3 / 66.5 / 95.5 | 14.4 | 222 | 346 | 52,337 |
| 5 | 32 | the hand | 35.8 / 38.2 / 66.8 | 19.4 | 298 | 342 | 47,643 |
| 6 | 40 | the heart | 36.7 / 39.2 / 74.1 | 19.1 | 294 | 146 | 39,878 |
| 7 | 48 | the heart | 43.1 / 45.0 / 74.2 | 19.1 | 294 | 142 | 31,991 |
| 8 | 56 | the eye | 43.2 / 45.4 / 74.6 | 19.0 | 294 | 186 | 50,466 |
| 9 | 64 | the eye | 42.5 / 44.3 / 73.7 | 19.1 | 294 | 182 | 41,826 |
| 10 | 72 | the load | 42.5 / 48.9 / 79.3 | 18.5 | 287 | 174 | 26,076 |
| 11 | 80 | the load | 41.7 / 43.5 / 72.8 | 19.1 | 293 | 158 | 17,858 |
| 12 | 88 | the spine | 41.8 / 43.9 / 73.2 | 19.0 | 294 | 154 | 18,761 |
| 13 | 96 | the spine | 40.3 / 42.6 / 72.3 | 19.1 | 294 | 624 | 77,020 |
| 14 | 104 | the spine | 35.0 / 42.5 / 60.6 | 18.6 | 287 | 644 | 30,421 |
| 15 | 112 | the crown | 29.1 / 32.7 / 60.0 | 26.7 | 72 | 36 | 27,451 |
| 16 | 120 | the crown | **25.9 / 27.4 / 57.1** | **28.5** | 21 | 32 | 18,845 |
| 17 | 128 | the colossus | 25.9 / 27.7 / 108.0 | 28.2 | 24 | 496 | 44,414 |
| 18 | 136 | the colossus | **72.0 / 76.6 / 108.1** | **11.6** | 178 | 474 | 11,705 |
| 19 | 144 | coda | 72.0 / 75.5 / 103.8 | 11.5 | 179 | 478 | 14,562 |
| 20 | 152 | coda | 71.9 / 74.1 / 102.2 | 11.6 | 178 | 442 | 11,675 |

A 30 fps frame is 33.3 ms. Only phrases 15, 16 and 17 come near it, and only
16 is inside it on the mean. The reveal and the coda sit at 72–77 ms with six
times the plain's triangle count at a fifth of its fill; phrase 3 is 70
triangles and 3,132 fragments and still costs 66 ms. Whatever dominates there
is charging by the triangle, or by something that is neither, rather than by
the pixel. That is Phase's to chase, and `media/run_render.log` has it per
second, not just per phrase.

**The audio was perfect through all of it.** Zero underruns, ring never below
465, hash exact. That is the clock model doing its job: at 11.6 fps the
picture skips moments and the music does not stretch, because `demo_render()`
is handed the sample the DAC is playing rather than a frame counter.

### 8.7 The material ceiling (PLANNING §8's worst case)

`build.ps1 pico -MaterialTest` draws `render_material_test()` in place of the
demo for the whole run — Phase's exact §8 ceiling. The telemetry confirms it
is drawing that ceiling and not something smaller: `tri 1500 px 90000
part 256`.

| | Measured |
|---|---|
| Render, steady state | **59.76 / 59.80 / 59.97 ms** min/mean/max |
| Render, over the overture's background | 65.63 / 65.71 / 65.81 ms |
| Frame rate | **14.9 fps** |
| Core-0 render work | **17.9 M cycles** at 300 MHz |
| PLANNING §8's allowance for a 30 Hz frame | 8 M cycles |
| | **2.24× over** |
| Audio underruns | 0 |

So the ceiling as specified costs a little over twice what the plan allows,
and the demo as built sits between 1.6× and 4.6× over depending on the phrase.
The platform's own share is fixed and small: core 1 takes 25.5% of one core
and never touches core 0's budget except through SRAM contention, which cost
1% in the scanout comparison. The 4 M and 8 M budgets in PLANNING §8 are
still the right budgets. They are simply not being met yet.

---

## 9. What surprised me on the device

**The yscale saving is real, and so is the contention.** I expected the
240-line mode to halve the scanout cost; it did better — 34.9 against 74.4
Mcycles a second — because doing the copy half as often also makes each copy
8% cheaper. Memory contention between the scanline DMA and the copy is a
measurable second-order effect on a part this size, and it shows up on the
*other* core as well.

**PERSISTENCE's floor was off by a factor of seven, and it would have been
easy to inherit.** 79 KiB against a measured 10,568 bytes. It was never wrong
as an observation; it was simply never a floor, and the fact that mattered —
sixteen scanline buffers rather than eight — was never attached to it. That is
the argument for `-DCOLOSSUS_BALLAST` existing at all: a floor you can
re-measure in six minutes beats a floor somebody once saw.

**The audio never flinched.** Zero underruns across two whole-score runs, a
100-second ceiling run and nine boot arms, including 5:07 at 11–19 fps with
core 0 missing four frames out of five. The pull-model synth on core 1 and the
DMA counter as the master clock did exactly what they were designed to do, and
the number that proves it is `peak=29914` on the device against 29,914 on the
host.

**The stub costs 6.26 ms to write 76,800 pixels** — 24 cycles a pixel for a
gradient with two divides in it. A per-pixel divide is not free on this part,
and fill is never the whole story. Phase's phrase 3, 70 triangles and 3,132
fragments in 66 ms, is the same lesson from the other end.

— **Overscan** (Claude Opus 5)
