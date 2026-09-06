# COLOSSUS — Overscan's platform report

To: Phosphor (director). From: Overscan (Claude Opus 5). Date: 2026-09-06.

Every number below says where it was measured. **HOST** means this desktop
(MinGW gcc 15.2, Windows) or the linker map. **DEVICE** means the Pico 2 on
COM10.

**There are no DEVICE numbers in this report, and that is the headline.** The
platform is written, all three firmware variants link, the host side is
finished and cross-checked — and the board is locked up and cannot be
recovered from software. It needs one physical action, described in §7. What
locked it up was my own instrument, and the fix is in the tree.

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
**8 × 324 × 4 = 10,368 bytes** — read out of the SDK source, HOST, and to be
confirmed on the device by the difference between `heap_free_after_init` and
`heap_free_after_video`. LEDGER.md reserves 21,504 for this, which is right for
sixteen buffers and about twice what this build takes. Predicted heap after
`video_init()` on the renderer build: 61,616 − 10,368 = **51,248 B**.

---

## 4. What I did NOT measure

Everything the brief asks for from the device, because the board is locked up:

- cycles per scanline copy, both scanout modes
- synth cost per second on core 1
- free heap at boot and after `video_init()`
- the boot floor by ballast bisection
- hash match over the full score
- frame telemetry with the stub, and the per-material worst case

The firmware for all of it is built and sitting in the tree
(`colossus_stub_rp2350.uf2`, `colossus_stub640_rp2350.uf2`,
`colossus_vga_rp2350.uf2`), and `build.ps1 run` / `build.ps1 floor` do the runs
in one command each. I will produce the numbers the moment the board answers.

I have not written estimates in their place, and `tools/ledger_check.py` does
not pretend either: its `DEFAULT_FLOOR` is PERSISTENCE's measured 79 KiB,
labelled in the source and in the report line as *inherited, not measured on
this project*. Against that inherited floor the renderer build **fails** the
heap check at 61,616 B — which is the same warning Phase already wrote into
LEDGER.md, now coming from a tool instead of from prose. It is probably
pessimistic: PERSISTENCE ran sixteen scanline buffers to my eight.

---

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

## 6. Requests I cannot make myself

**To Phase, for `LEDGER.md`.** `ledger_check.py` reads it as prose — any
identifier it mentions counts as declared — and these are the ones it wants,
with the measured sizes (HOST, from the map of the renderer build):

- `video.c` and `g_pages` — 307,628 B. The "Two platform pages" row is right,
  it just does not name the file or the symbol the map does.
- `audio_pwm.c`, `s_left`, `s_right` — 4,680 B against the 4,096 reserved. The
  rings are exactly 4,096; the extra 584 is the 256-byte synth block and the
  channel state. Either the row goes to 4,680 or I shrink the block.
- `render_block` (5,144 B) and `synth_render` (2,704 B) are `synth.c` **SRAM
  code**, 7,848 B of it, from `CV_HOT`. The ledger's 8,192 "Renderer SRAM hot
  code" row is Phase's; the synth's hot code has no row and nearly fills that
  one on its own.
- `r_bloom` — 704 B in `render.c`, not listed.
- `synth.c` totals **46,856 B**, not the ~30,700 the ledger's synth rows sum
  to: `g_rv_c` is 15,000 not 10,008, and `g_chorus` (2,048) is new. This is the
  main reason the heap came out at 61,616 rather than the ledger's 71,460.
- scanvideo's runtime allocation is **10,368 B** at eight buffers, not 21,504.

**To you, for `song.c`.** `song_section_name()` still returns `"the forge"` for
the chapter PLANNING revision 2 renamed to **IV · LOAD**. It is only a
telemetry string today, but it is the string the device prints for those bars
and the one a reviewer will read.

**The contract I assumed for `render.cmake`,** since I had to write
`CMakeLists.txt` before it existed: it sets `COLOSSUS_RENDER_SOURCES` (a list
of `.c` files; `RENDER_SOURCES` is accepted as an alias) and may set
`COLOSSUS_RENDER_INCLUDES` and `COLOSSUS_RENDER_DEFINES`. Phase's file matches,
so nothing needs to change — recording it so it stays true.

---

## 7. The board

**The Pico 2 on COM10 needs to be unplugged and plugged back in while the
BOOTSEL button is held.** A plain replug is not enough: the flashed image is
the one with the malloc probe in it, and it will panic and lock up again about
a second and a half after power-up, before anything can catch it. Held BOOTSEL
puts the bootrom in charge instead.

The moment it is in BOOTSEL, this is the whole run:

```
cd d:\Toyprojects\PicoDemos\20_Colossus
picotool load -x colossus_stub_rp2350.uf2
.\build.ps1 run -Stub -Seconds 330
```

The fixed firmware waits up to five seconds for the CDC to be opened before it
does anything that could panic, so a panic message can actually reach the wire
this time; and if it does panic, `cv_panic()` keeps USB alive and the board
stays flashable. There is a watch running here that will notice the second the
board answers, and I will take the numbers then.

I am sorry it cost a hand. The instrument that broke it is gone, the failure
mode it exposed is now impossible to repeat, and the platform is better for
having hit it — but the brief asked for a first hardware run and I owe you one.

— **Overscan** (Claude Opus 5)
