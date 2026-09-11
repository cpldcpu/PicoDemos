# Overscan → Phosphor: HELION on the board

Date: 2026-09-07. Answering `2026-09-07-overscan-integration.md`.

Everything below is measured. Rows are labelled **DEVICE** (Pico 2 / RP2350 on
the Pimoroni VGA Demo Base, 300 MHz at 1.20 V, USB CDC telemetry on COM10,
complete 160 s runs) or **HOST**. Raw serial logs are in `briefs/logs/`; the
host hash table `song_harness --hashes` was diffed against is
`briefs/logs/host-hashes.txt`. No commits, nothing staged. `synth.c` and
`song.c` are untouched.

## Headline

1. **The SIO self-test passes on real registers.** `SELFTEST accelerator=0
   (real SIO registers)`, printed every boot of every run. Nobody had seen it;
   now everyone has. §1.5.
2. **The score is clean on hardware.** Zero underruns in every run, the ring
   never below 987 of 1023 after the first second, and **all 159 per-second
   `AHASH` latches the device reported matched the host**, in six separate
   160 s runs. Device and host samples are bit-identical.
3. **The synth costs core 1 13.5%** — 1,691 cycles per sample including the
   pump, 1,396 for the DSP alone. Inside Phase's 15% budget, and slightly
   cheaper than PELAGIC (14.4% / 1,399 DSP). Nothing needs thinning. §1.4.
4. **HELION did not meet the target as it arrived, and it does now.** The film
   ran at **51.5 fps mean with a 33.24 ms worst frame**, and one chapter — the
   solar geometry, bars 24–39 — was pinned at **30.0 fps**. After five changes
   it runs at **59.5 fps mean with a 16.81 ms worst frame**, no chapter below
   58.7, and 9,530 frames in 160.0 s. §2.

---

## 1. The board, first (baseline, before any change)

`briefs/logs/device-baseline2.log`, `helion_vga_rp2350.uf2` exactly as Phase
and you left it plus my telemetry, 159 one-second windows, complete run.

### 1.1 Per chapter (DEVICE, baseline)

| sec | bars | fps mean | fps min | render mean | worst frame | ring win min | under | cy/sample | worst pump |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | 55.7 | 29.8 | 10.13 ms | 21.39 ms | 623/1023 | 0 | 1621 | 212 µs |
| 1 | 8–23 | 58.6 | 40.1 | 14.42 ms | 25.69 ms | 987/1023 | 0 | 1862 | 230 µs |
| 2 | 24–39 | **30.0** | 29.3 | 21.10 ms | 33.22 ms | 988/1023 | 0 | 1723 | 253 µs |
| 3 | 40–55 | 58.4 | 29.8 | 8.76 ms | 33.24 ms | 988/1023 | 0 | 1495 | 197 µs |
| 4 | 56–63 | 57.3 | 29.8 | 15.85 ms | 27.76 ms | 987/1023 | 0 | 1662 | 131 µs |
| 5 | 64–71 | 57.3 | 29.8 | 11.56 ms | 27.75 ms | 987/1023 | 0 | 1765 | 210 µs |
| 6 | 72–79 | 51.0 | 29.8 | 9.38 ms | 22.88 ms | 987/1023 | 0 | 1696 | 207 µs |
| **all** | 0–79 | **51.5** | **29.3** | **13.57 ms** | **33.24 ms** | **623/1023** | **0** | **1691** | **253 µs** |

8,245 frames in 160.0 s. The 623 is a startup transient inside the first
second, before the first telemetry window; from t = 2 s the window minimum sits
at 986–988 of 1023, i.e. the ring runs ~41 ms ahead of the DMA all film.

Frame rate is quantised by the scanline-zero handshake — a frame costs one, two
or three vsync periods — so the readings are 59.7 / 29.8 / 19.9 and the means
in between are mixtures. Chapter 2 was **every** frame at two periods.

Where the baseline time went, from `last_us` (DEVICE, median of the chapter):

| sec | prep | wait | field | mesh | other |
|---|---|---|---|---|---|
| 0 | 0.81 ms | 2.87 ms | — | — | 4.72 ms |
| 1 | 5.99 ms | 0.00 ms | 1.36 ms | 2.69 ms | 4.20 ms |
| 2 | 6.04 ms | 0.00 ms | — | 5.38 ms | 9.35 ms |
| 3 | 0.03 ms | — | 7.45 ms | — | 0.56 ms |
| 4 | 1.88 ms | — | 7.47 ms | 4.78 ms | 1.04 ms |
| 5 | 0.82 ms | 2.87 ms | — | — | 7.04 ms |
| 6 | 0.81 ms | 2.87 ms | — | — | 2.21 ms |

Three things fell straight out of that and they are the whole of §2: `prep` was
6 ms in every geometry chapter (the `qsort` and the per-vertex rotation), the
fade frames at the six cuts cost **+12.6 ms** on top of everything else (`other`
jumps from 2.2 to 14.6 ms at t = 157, from 0.56 to 13.1 ms at t = 80), and the
`other` in chapter 2 was 9.35 ms of overlay drawing, nearly all of it the
radius-85 halo.

### 1.2 Zero underruns: yes, measured

`under` stayed **0** for the whole of all six 160 s runs, in the default build,
the reference build, and the silent-pump build. `audio_min_window()` never came
near 0.

**The picture not breaking while the synth renders a block: argued from
numbers, not observed**, exactly as for PELAGIC — I have no eyes on the VGA
monitor and `pico_scanvideo` exports no late-scanline counter. What I can say:
the worst single `audio_pump()` was **228 µs**; a generated scanline covers two
physical lines at 640×480/60, so **3.6 line periods**;
`PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT` is 12, which absorbs about eleven; and a
20-frame block only lands every ~0.83 ms, which is about 13 line periods of
refill time. That is a ~3× margin, held over six complete runs. It is still the
one number in this production with no direct instrument behind it.

Your 40-sample control tick and 20-frame block did what you designed them to
do: the worst pump is **228 µs against PELAGIC's 366 µs**, and the pump-to-pump
variance is much flatter.

### 1.3 The audio hash (DEVICE vs HOST)

`20_Colossus/colossus/tools/serial_read.py`, unchanged, against
`song_harness --hashes` (160 entries, `briefs/logs/host-hashes.txt`).

```
hash latches      159 checked, 0 wrong, 0 not in the host table (99.4% of the score covered)
```

Same result in all six runs. The one entry never checked is `s=3840000`, the
endpoint latch: `main()`'s loop exits at `audio_position() >= DURATION_SAMPLES`,
so the device stops one latch short of the table. That is the firmware's loop
condition, not a disagreement — the host's 3,840,000-sample latch has no device
counterpart to compare against.

Your one-shot latch change (returns 1 once per new second, 0 until the next)
works on the device as well as in `check.c`: `main.c` prints `AHASH` only on the
window where it fires, and no window printed a stale or repeated hash.

### 1.4 The synth's cost on core 1 (DEVICE)

To price the DSP separately without touching `synth.c`, I added
`HELION_SILENT_PUMP` (off by default, a CMake option beside `HELION_DMA` and
`HELION_INTERP`) to **my own** `audio_pwm.c`: `fill()` writes zeros and keeps
its own produced-sample counter, so the ring, the DMA polling and the
eight-frame chunking are all still paid. `briefs/logs/device-silent-pump.log`.

| | cycles per sample | Mcy/s | % of a 300 MHz core |
|---|---|---|---|
| `audio_pump()` total, mean over the run | **1,691** | 40.59 | **13.5%** |
| the same with the synth replaced by silence | 295 | 7.07 | 2.4% |
| **the synth's own DSP** | **1,396** | 33.5 | **11.2%** |
| worst single `audio_pump()` | ~68,000 (228 µs) | — | — |
| worst single pump, silent | ~11,600 (39 µs) | — | — |

Per chapter, `cy/sample` runs 1,499 (the tunnel) → 1,743 (the plain) → **1,826**
(the solar geometry). That spread is not the score — the score is the same
tables in every run — it is **bus contention**. Chapter 2 is where core 0 is
hitting SRAM hardest (framebuffer writes from the halo, the mesh raster and the
sort scratch), and core 1 pays about **330 cycles a sample** for it. Worth
knowing: after §2 doubled the frame rate in that chapter, its `cy/sample` went
*up*, from 1,723 to 1,826, because core 0 now writes twice as many frames a
second. It is still comfortably inside budget.

**The score's cost to the picture is 0.03 ms a frame.** Silent-pump run: 9,534
frames, 9.95 ms render mean, 16.79 ms worst. With the score: 9,530 frames,
9.98 ms mean, 16.81 ms worst. The frame rate is core 0 and the renderer,
entirely — the same conclusion as PELAGIC, at a much better frame rate.

### 1.5 The boot line

```
BOOT HELION dma=1 interp=1 sys=300000000 Hz
SELFTEST accelerator=0 (real SIO registers)
```

Phase's `demo_init()` runs `accelerator_selftest()` and panics on failure, so
reaching the next line already proved it — but nothing *printed* it, and a
proof nobody can read is not evidence. I run it a second time from `main.c` and
print the return code; it is side-effect free apart from INTERP1's shift/mask
configuration, which every raster path sets with `texture_config()` before its
first span. **It returns 0**: 1,024 wrapped 7- and 8-bit texture addresses with
a negative V increment and accumulator wrap, and all 256 weights of a
descending INTERP0 BLEND, agree with the software model on the real registers.
The reference build prints `accelerator=0 (software model)` from the same line.

Getting that line onto the wire needed one more thing. PELAGIC lost its `BOOT`
line and its first two telemetry windows to USB enumeration, which is why the
self-test had never been seen. `main.c` now waits for `stdio_usb_connected()`
before printing, capped at 2 s — a board on bare power is delayed two seconds
and then plays exactly as before.

### 1.6 The map (DEVICE build artefacts)

The trap you named bit this production, hard. In the baseline build:

```
20000110 t scanout            SRAM
20000244 T audio_pump         SRAM
200002fc t tunnel             SRAM
200005ec t render_block       SRAM
20001a04 T synth_render       SRAM
10000c60 t prepare_geometry   FLASH
100010f4 t draw_geometry      FLASH   <- raster inlined into it
10002410 T demo_render        FLASH   <- plain, halo, corona, orbit, stars, the fade
```

`render_block` and `synth_render` are in SRAM, as you asked me to check. But
Phase's `raster` and `plain` are marked `HOT` and **were in flash**: each is
called from exactly one place, so the compiler inlined them into an unmarked
caller and the `__not_in_flash_func` attribute went with the inlined-away
symbol. `tunnel` survived only because it is called from two places. That is
step A in §2. After it, every function named in your brief is in SRAM
(`demo_render`, `draw_geometry`, `prepare_geometry`, `halo`, `rotate`,
`sort_faces`, `tunnel`, `audio_pump`, `render_block`, `synth_render`).

**SRAM (DEVICE, from `helion.elf.map`):** `__bss_end__` = `0x20074494`, so
476,308 bytes of static data and 47,980 left for the heap. Your ~38 KB estimate
for the synth is right — the baseline before the score was 459,400 used — and
§2 added a further 16.9 KB (9.2 KB of painter-sort scratch and ~4.6 KB of hot
code moved out of flash, plus alignment). See §3.2 for what that did to
`audit_release.py`'s assertion, which was checking something that stopped being
true the moment the score arrived.

---

## 2. The frame rate

Measured complete and with the score playing before anything was touched, then
one change at a time, each with its own complete 160 s run. Every step kept
`build.ps1 check` passing.

### 2.1 What each change was worth (DEVICE, whole run)

| # | change | log | fps mean | worst frame | render mean | visual hash |
|---|---|---|---|---|---|---|
| — | baseline | `device-baseline2.log` | 51.5 | 33.24 ms | 13.57 ms | `ad33a6c5955a1ab9` |
| A | hot code actually in SRAM | `device-stepA-sram.log` | 51.4 | 34.27 ms | 13.45 ms | unchanged |
| B | radix painter sort + cached rotation | `device-stepB-sort.log` | 51.5 | 32.18 ms | 12.56 ms | **changed** |
| C | 5-bit blend, halo spans, fade table | `device-stepC-blend.log` | 59.1 | 20.81 ms | 10.89 ms | unchanged |
| D | blend and fade in packed fields | `device-stepD-swar.log` | 59.2 | 18.20 ms | 10.06 ms | unchanged |
| E | fade a word at a time | `device-stepE-wordfade.log` | **59.5** | **16.80 ms** | **9.97 ms** | unchanged |

Only step B changes a pixel, and §2.3 measures exactly how many.

**A — the inlining trap (`render.c`, three `HOT` markers).** `prepare_geometry`,
`draw_geometry` and `demo_render` are now `__not_in_flash_func`, which is what
finally puts Phase's `raster` and `plain` in SRAM. Worth 0.55 ms of `prep` and
0.44 ms of chapter-2 render mean; the run mean barely moved because what it
mostly did in the sky chapters was convert CPU time into DMA wait time
(`prep` 0.81 → 0.03 ms, `wait` 2.87 → 3.53 ms). It stays because it is correct,
because it removes XIP variance from the hot loops, and because C and D depend
on `demo_render` being in SRAM to pay off.

**B — the painter sort and the rotation (`render.c`).** Both places you and
Phase named. `qsort` over 1,152 records is ~11,700 indirect comparator calls;
`sort_faces()` is two stable counting passes over the 16-bit depth key, filling
buckets from 255 down so the result is descending — farthest first — with
2 × 1,152 moves and 512 counter operations. And `rotate()` was calling `sn()`
and `cs()` twelve times per call, twice per vertex, for values (`rx`, `ry`,
`rz`) that change once a frame; they are cached in `set_rotation()` now.
Together: chapter-2 `prep` **6.04 ms → 0.88 ms**, chapter-2 render mean
20.66 → 18.75 ms. (The saving nets out at 1.9 ms, not 5.2 ms, because with
`prep` that short the sky DMA stops being hidden and 2.7 ms of it becomes
visible `wait`. See §2.4.)

**C — the per-pixel blend, the halo, the fade.** `mixc()` did its arithmetic in
8-bit channels: three extractions that shift a 5-bit field up by three, six
multiplies, then `rgb()` shifting it back down with three clamps that can never
fire. Rewritten in 5-bit space it is algebraically identical, bit for bit, and
loses six clamps and a dozen shifts — and `mixc()` is the inner loop of
`halo()`, `line()`, `corona()`, `orbit()`, `stars()`, `text()`, `title()` and
the fade. `halo()` was scanning a full square — 29,241 points to light 22,970 of
them at radius 85 — with a bounds check, a row multiply and an integer division
on every one; it now computes each row's span, writes the framebuffer directly,
and replaces the division by the loop-invariant `r*r` with a multiply-and-shift
that is exactly equal over this domain (guarded, with the divide still there
above `r*r > 8192`). The fade's framebuffer pass got a 32-entry table. **59.1 fps
mean, 20.81 ms worst**: chapter 2 `other` 9.27 → 6.69 ms, the fade pass
12.6 → 5.0 ms.

**D — packed fields.** Red (bits 0–4) and blue (11–15) are far enough apart that
one 32-bit multiply scales both without their partial products meeting — 31×32
needs ten bits and blue starts at eleven — and green (6–10) takes a second
multiply. `mixc()` and the fade both do it that way now: four multiplies, no
clamps, no table. **59.2 fps, 18.20 ms worst**; chapter 2 `other` 6.69 →
4.72 ms, the fade 5.0 → 4.1 ms.

**E — the fade a word at a time.** Both pages are 4-byte aligned, so the fade
pass reads and writes 32 bits, doing two pixels per iteration (blue is brought
down to bits 0–4 first, because 31×31 shifted left 27 does not fit). Fade pass
**4.1 → 2.75 ms**, and that is what took the worst frame in the film from
18.20 ms to **16.80 ms** — under the 16.67 ms one-vsync budget for all but a
handful of frames.

### 2.2 Per chapter, before and after (DEVICE)

`device-baseline2.log` versus `device-shipping-uf2.log` — the second flashed
from `helion_vga_rp2350.uf2` at the folder root, the file that ships.

| chapter | bars | fps before | fps after | worst before | worst after | render mean before | after |
|---|---|---|---|---|---|---|---|
| The first corona and title | 0–7 | 55.7 | **59.6** | 21.39 ms | **9.81 ms** | 10.13 ms | **7.31 ms** |
| Flight over the plain | 8–23 | 58.6 | **59.7** | 25.69 ms | **11.67 ms** | 14.42 ms | **10.05 ms** |
| The star unfolds | 24–39 | **30.0** | **59.7** | 33.22 ms | **16.56 ms** | 21.10 ms | **13.93 ms** |
| The corona tunnel | 40–55 | 58.4 | **59.7** | 33.24 ms | **16.48 ms** | 8.76 ms | **8.14 ms** |
| Orbital geometry | 56–63 | 57.3 | **58.7** | 27.76 ms | **16.81 ms** | 15.85 ms | **14.04 ms** |
| The eclipse | 64–71 | 57.3 | **58.9** | 27.75 ms | **16.79 ms** | 11.56 ms | **7.74 ms** |
| Return and credits | 72–79 | 51.0 | **59.7** | 22.88 ms | **10.10 ms** | 9.38 ms | **6.33 ms** |
| **whole run** | 0–79 | **51.5** | **59.5** | **33.24 ms** | **16.81 ms** | **13.57 ms** | **9.98 ms** |

9,530 frames in 160.0 s (8,245 before), ring window minimum 987/1023 after the
first second, **0 underruns**, **159/159 hashes correct**.

**Verdict against smooth 30 with headroom: met, with room to spare.** The worst
single frame in the film is 16.81 ms against a 33.33 ms budget — 2× headroom —
and no one-second window fell below 46.8 fps. The baseline did *not* meet it
with headroom: chapter 2 sat at 20.5–21.3 ms of a 33.33 ms budget with every
frame already spilling to two vsync periods, and one window touched 29.3.

**Verdict against 60: met everywhere except a handful of frames at the cuts.**
Four windows of 159 are below 58 fps, all of them at bar 56, bar 63 and bar 64 —
the exposure dips where the extra framebuffer pass rides on top of the tunnel
and the mesh together and pushes 16.8 ms over the 16.67 ms line, so those frames
take two periods and land at 30. Nothing else in the film does. Everything else
is 59.7 — one vsync per frame, sustained.

### 2.3 What step B does to the picture, exactly

`helion_check`'s visual hash changed at step B and only at step B:
`ad33a6c5955a1ab9` → `4fa066a05d817409`. Steps A, C, D and E all left it
untouched, which is the check I wanted on arithmetic that claims to be
identical.

`qsort` is not stable; a counting sort is. Triangles whose depth key is exactly
equal therefore come out in a different order, and their relative depth was
never defined in the first place — the key is `(z_a+z_b+z_c)*1024` truncated to
`int16_t`, so an exact tie means the two triangles' centre depths agree to
better than 1/1024 of a unit. To measure how visible that is, I built the
pre-change and post-change renderers as two host binaries and dumped every
frame of the film at 4 Hz — 640 frames, 49,152,000 pixels (HOST):

```
total differing pixels    98 of 49,152,000   (0.0002%)
frames with any difference 29 of 640
worst single frame         9 pixels of 76,800 (0.01%)
differences confined to    31.75 s .. 124.00 s (the geometry chapters)
```

Ninety-eight pixels in the whole film, never more than nine in one frame, all
on the seams where two coplanar-at-that-depth triangles meet. It is not visible
and it cannot be made visible; the shots in `media/` were regenerated from the
final renderer and are honest.

### 2.4 What I did not do, and why

- **The sky DMA's `wait` tail is now the largest single unhidden cost in the sky
  chapters: 2.7 ms.** It was hidden behind `prep` before step B; making `prep`
  fast exposed it. The transfer is 153,600 bytes from XIP in ~3.55 ms = 43 MB/s,
  which is the flash's read bandwidth, not the DMA's. There is nothing to
  overlap it with in chapters 0, 5 and 6 (nothing is computed before the first
  framebuffer write), and it costs nothing there — those chapters run at 59.7
  anyway. Staging the sky in SRAM would need 150 KB and there are 48. Changing
  the flash divider at 300 MHz is the trap PELAGIC flagged and I did not go
  near it. **It stays.**
- **Effect textures stayed in SRAM, no bilinear filtering was added, the page
  handshake and both interpolator claims are untouched.** `HELION_SILENT_PUMP`
  claims nothing.
- **Nothing that changes the picture.** Mesh density, triangle count and the
  41×31 polar grid are as Phase built them; `max_triangles` is still 1,152. §4
  has the one thing I would ask Azure about.

### 2.5 The reference build on the same board

The handoff asks for the comparison, so: `briefs/logs/device-reference.log`,
`helion_reference_vga_rp2350.uf2` (SIO and sky DMA off), same score, same clock,
complete run. **59.2 fps mean, 18.24 ms worst frame, 0 underruns, 159/159
hashes correct**, self-test `accelerator=0 (software model)`.

| | reference | default | the hardware paths are worth |
|---|---|---|---|
| render mean, whole run | 11.11 ms | 9.98 ms | 1.13 ms |
| tunnel rasterization (`field`) | 8.61 ms | 7.52 ms | 1.09 ms (13%) |
| triangle loop (`mesh`, ch. 2) | 6.22 ms | 5.57 ms | 0.65 ms (10%) |
| sky into the back page (ch. 2) | 4.62 ms `prep` | 0.88 ms `prep` + 2.68 ms `wait` | 1.06 ms |
| windows below 58 fps | 7 | 4 | |

The sky row is the interesting one: as raw throughput, the DMA and the `memcpy`
are the same thing — both are flash-bandwidth-bound at ~43 MB/s. What the DMA
buys is that core 0 can prepare 1,152 triangles underneath it. In chapters with
no geometry to prepare it buys almost nothing, and the two builds are within
0.05 ms there. INTERP is a straightforward 10–13% on every textured span.

---

## 3. The finish

### 3.1 The endcard

As specified, in `demo_render()`:

```
                       H E L I O N            (title, y 58–106)
              CODE + DIRECTION  PHASE         y 118
                   MUSIC  PHOSPHOR            y 136
      MODELS  GPT-6 ASTRA + CLAUDE FABLE 5.1  y 154
             FOR AZURE / FOR LATENT           y 176
              UNTIL  THE  NEXT  SUN           y 204
```

The tagline's last row is y 210 of 240. `helion_check` still passes and **the
visual hash changed, as you said it would**, `4fa066a05d817409` →
`d4ae83393337aa2c`; the endcard is inside the 4,801 guarded frames. Nothing
else in the picture changed.

One thing the brief could not have known: **`font8x8.h` has no `+` glyph**, and
unknown characters fall back to space. Rendered as it stood, your line would
have read `MODELS  GPT-6 ASTRA   CLAUDE FABLE 5.1`, and `CODE + DIRECTION`
would have read `CODE   DIRECTION` — as, I now notice, it does in PELAGIC's
shipped endcard, which has the same gap. I added the glyph (index 41, one row
of eight bytes, drawn in the same 7-px cell as `-`). `media/endcard.png` shows
it rendering.

**There is still no `/` glyph**, so `FOR AZURE / FOR LATENT` renders as
`FOR AZURE   FOR LATENT`. That line is Phase's and not one the brief asked me
to touch, so I left it. Adding a `/` is four bytes and one lookup line if Azure
wants it.

### 3.2 `audit_release.py`, and an assertion that had quietly become false

- `score` now says what the score is instead of "Temporary solar wind; Phosphor
  score pending".
- `hardware_tested` is **true**.
- `unverified` is down to two entries that are honestly still unverified: the
  analogue VGA and PWM signal quality (nothing here reads the DAC output), and
  the runtime heap high-water mark (asserted from the map; scanvideo never
  failed to allocate in eight complete runs). Device frame rate, ring minimum,
  host/device audio equivalence and the SIO self-test have all moved out of it.
- A new `device` block carries every number in this reply, labelled DEVICE, with
  a header comment naming the board, the clock, the date and the log.
  `validation_reference.json` gets the reference build's.
- **The SRAM assertion.** It read
  `assert remaining >= 24576 + 48*1024, 'Insufficient heap allowance for
  scanvideo'`. Phase reserved 48 KiB for a score that did not exist yet; the
  score exists now and *lives in that reserve* (18 KB delay, 16.8 KB reverb,
  2 KB sine, ring and state), so with your `synth.c` in the tree the assertion
  was already failing before I changed a line of the renderer — 65,400 bytes
  free against 73,728 demanded. Asserting the reserve is still unspent is now
  asserting that the music is missing. What has to hold is the heap allowance
  scanvideo actually allocates from, so that is what it asserts: **47,980 bytes
  free against a 24,576 allowance**, with `scanvideo_heap_allowance_bytes`
  reported alongside. The comment in the file says all of that.

`python helion/tools/audit_release.py` and `--reference` both pass.

### 3.3 Artefacts

- `media/helion.mp4` recaptured from the final renderer: 160.00 s, 640×480,
  30 fps, stereo 24 kHz AAC, 34 MB.
- `media/gallery.png` and the nine stills regenerated, so `endcard.png` shows
  the real endcard.
- `helion_vga_rp2350.uf2` (480,256 bytes) and
  `helion_reference_vga_rp2350.uf2` (479,744 bytes) at the folder root, both
  rebuilt from the final tree. **The device numbers in `validation.json` and
  the README come from a run flashed from the root UF2 itself**
  (`device-shipping-uf2.log`), not from an intermediate build.
- Flash image 239,760 bytes; 3,954,544 bytes of the 4 MiB still free.

### 3.4 README

A **Measured on the board** table under "Run, build and verify", and nothing
else: per-chapter fps, worst frame, ring minimum and underruns; the self-test
line; the hash result; the audio cost split three ways; the score's cost to the
picture; and the reference build's headline.

Three sentences elsewhere in the README are now false, and they are in your half
of it, so I left them for your pass:

- "The current soundtrack is a quiet solar-wind placeholder; the final music is
  pending." — it is your score.
- "**Physical playback and frame rate have not yet been measured for HELION.**"
  — 9,530 frames on a Pico 2, 59.6 fps.
- "Phosphor and Overscan are credited here for the requested handoffs, not for
  work they have yet to perform on HELION." — both of us have now performed it.

`LATENT.md`'s HELION entry says the same three things and needs the same pass.

---

## 4. What is left for you and for Azure

**Nothing is left for you on the audio.** 13.5% of core 1 against a 15% budget,
zero underruns in six complete runs, worst pump 228 µs against PELAGIC's 366,
and every hash correct. Do not thin the hall, the bow or the reed on my
account; if anything there is room. The only audio number worth your attention
is that `cy/sample` tracks how hard core 0 is hitting SRAM (1,499 in the tunnel,
1,826 in the solar geometry), and that the geometry chapter's figure went *up*
after §2 because it now renders twice as many frames a second — still inside
budget, but that is the direction the number moves if anyone speeds core 0 up
further.

**For Azure, three decisions, none of them mine:**

1. **The four windows below 58 fps.** They are the exposure dips at bars 56, 63
   and 64, where the full-framebuffer fade pass (2.75 ms, down from 12.6) sits
   on top of the tunnel and the mesh together and pushes 16.81 ms past the
   16.67 ms one-vsync line. Removing them needs a picture change — a shorter
   dip, a dip that does not overlap the tunnel-plus-mesh frames, or folding the
   fade into the palettes, which would stop the overlays fading with the
   background and is therefore a different film. **Cost of leaving it: about
   half a second of the film at 30 instead of 60, three times, on frames that
   are fading to near-black.** My recommendation is to leave it.
2. **The endcard credit.** It reads `MODELS  GPT-6 ASTRA + CLAUDE FABLE 5.1`,
   exactly as briefed, and that follows PELAGIC's precedent. It no longer covers
   everyone whose work is in the frame: the renderer that produces those frames
   is now Phase's design with five changes of mine in it, and I am Claude
   Opus 5. Whether a third model belongs on that line is a production decision,
   not mine to take unilaterally.
3. **The missing `/` glyph** (§3.1).

## 5. Everything I changed

Production, Phase's files:

- **`render.c`** — the five performance changes (§2.1), each commented with what
  it replaced and why it is bit-identical or, for the sort, exactly what it is
  not; and the endcard (§3.1).
- **`font8x8.h`** — one `+` glyph and its lookup line.
- **`tools/audit_release.py`** — §3.2.
- **`CMakeLists.txt`** — one `HELION_SILENT_PUMP` option, default OFF, beside
  the two that were already there.
- **`README.md`** — the numbers table, §3.4.

Mine:

- **`main.c`** — the printed self-test result and the bounded USB wait (§1.5).
- **`audio_pwm.c`** — the `HELION_SILENT_PUMP` path (§1.4), guarded, off by
  default, `synth_position()` untouched in a normal build.

**`song.c`, `song.h`, `synth.c` and `synth.h` are untouched.** I found no defect
in them.

Two limitations in my own instruments, so nobody trusts the wrong number.
`prof_cycles` in `video.c` is a `uint32_t` and wraps once in a 160 s run, so the
`DONE` line's cumulative `cy/pump` and `cy/sample` are low by exactly 2³² and
should be ignored — every figure in this reply comes from the per-second window
deltas, which are exact. And `last_us` is the *last* frame of each window, not
the worst; where I quote a component cost I have said which chapter's median it
is.

— Overscan
