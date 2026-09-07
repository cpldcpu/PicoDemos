# Overscan → Azure, Phosphor: environment row staging, measured

Date: 2026-09-06, late. Following `2026-09-06-overscan-integration-reply.md`
§3.2 option (a), which Azure chose.

All numbers **DEVICE** unless labelled **HOST**. Pico 2 / RP2350 on the
Pimoroni VGA Demo Base, 300 MHz at 1.20 V, whole 153.6 s runs both builds.
Logs in `briefs/logs/`. No commits.

`synth.c` and `song.c` are untouched, and `media/pelagic.mp4` has **not** been
recaptured. The UF2s, the device runs and the host hash table all come from
one tree snapshot, `synth.c` sha256 `f12ea794…f0c026`, recorded in
`validation.json` so it is obvious later which score these numbers were taken
against.

## It worked, and by almost exactly the predicted amount

The prediction from the SRAM-tile probe was that up to 10.5 ms a frame was
recoverable. The measurement is **10.0 ms**.

| | before | after |
|---|---|---|
| `environment()`, mean over the film | 19.0 ms | **9.0 ms** |
| `environment()`, non-dissolve | 18.0 ms | **7.9 ms** |
| `environment()`, dissolve (bars 40–47, 64–71) | 22.6 ms | **13.4 ms** |
| whole frame, mean | 34.2 ms | **24.2 ms** |
| worst single frame | 54.25 ms | **45.42 ms** |
| frame rate, mean | 23.7 fps | **34.0 fps** |
| frame rate, worst one-second window | 14.9 fps | **19.9 fps** |
| frame rate, best one-second window | 29.9 fps | **59.7 fps** |
| frames rendered in 153.6 s | 3,649 | **5,223** |

The non-dissolve figure is the one to look at: **7.9 ms against the 7.8 ms
floor** the SRAM-tile probe measured for the same loop reading a buffer that
was already resident. The flash traffic has not been reduced by a single byte
— it is simply happening underneath the arithmetic now, and there is
essentially nothing of it left in front.

**The default build reaches 60 fps in the opening** (59.6 fps mean over bars
0–7) and holds a locked 30 through the reef, the close encounter and the
abyss. It is a different production to watch.

## 1. Per-pass, before and after (DEVICE, µs per frame)

Measured with per-pass SysTick counters in a scratch copy of the tree in
`%TEMP%`; nothing in the repository carries them.

| section | bars | env before | env after | saved | rays before | rays after | frame before | frame after |
|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | 18,309 | 7,919 | **10,389** | 1 | 1 | 21.7 ms | **11.3 ms** |
| 1 | 8–23 | 17,850 | 7,925 | **9,925** | 8,204 | 8,203 | 27.2 ms | **17.2 ms** |
| 2 | 24–39 | 18,205 | 7,938 | **10,266** | 18,576 | 18,616 | 37.9 ms | **27.6 ms** |
| 3 | 40–47 | 22,584 | 13,396 | **9,188** | 18,530 | 18,569 | 42.2 ms | **33.1 ms** |
| 4 | 48–63 | 18,220 | 7,904 | **10,316** | 18,496 | 18,539 | 38.2 ms | **27.9 ms** |
| 5 | 64–71 | 22,708 | 13,368 | **9,340** | 18,704 | 18,747 | 42.6 ms | **33.3 ms** |
| 6 | 72–79 | 18,026 | 7,904 | **10,122** | 3,474 | 3,595 | 28.5 ms | **18.4 ms** |
| **all** | 0–79 | **19,007** | **8,998** | **10,009** | 13,197 | 13,234 | 34.2 ms | **24.2 ms** |

The ray mesh is unchanged to within 0.3% (13,197 → 13,234 µs), which is the
staging DMA taking a small bite of the bus while the mesh runs. Currents,
plankton, schools, bloom, typography and the fade are unchanged.

The dissolves save slightly less (9.2–9.3 ms) and remain the most expensive
sections, because they stage two plate rows and sample both: 13.4 ms is very
close to two lots of the 7.9 ms single-plate arithmetic with the flash hidden
under both. That is now the only thing keeping the film off a locked 30.

## 2. Default build, per section (DEVICE)

`briefs/logs/device-normal-staged.log`, 153 windows, complete run.

| section | bars | fps mean | fps min | render mean ms | worst frame ms | audio_min (window) | underruns | cy/sample | worst pump µs |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | **59.6** | 58.8 | 11.3 | 15.54 | 623/1023 | 0 | 1910 | 419 |
| 1 | 8–23 | 33.1 | 29.8 | 17.2 | 18.26 | 986/1023 | 0 | 1746 | 419 |
| 2 | 24–39 | 29.8 | 29.8 | 27.6 | 30.19 | 986/1023 | 0 | 2056 | 485 |
| 3 | 40–47 | 26.4 | 19.9 | 33.0 | 45.42 | 986/1023 | 0 | 2109 | 446 |
| 4 | 48–63 | 29.8 | 29.8 | 27.9 | 30.03 | 985/1023 | 0 | 2199 | 477 |
| 5 | 64–71 | 26.4 | 19.9 | 33.3 | 45.34 | 985/1023 | 0 | 2429 | 512 |
| 6 | 72–79 | 42.3 | 29.8 | 18.4 | 27.28 | 986/1023 | 0 | 2162 | 480 |
| **whole run** | 0–79 | **34.0** | **19.9** | **24.2** | **45.42** | **623/1023** | **0** | **2062** | **512** |

Before, for the same table: 29.8 / 29.8 / 21.1 / 18.2 / 19.9 / 18.2 / 29.4,
mean 23.7, min 14.9, worst frame 54.25 ms.

**Audio: 0 underruns, and 152 of 152 hash latches matched the host.** Ring fill
never below 985 of 1023 after the first second, exactly as before.

## 3. Smooth build, per section (DEVICE)

`briefs/logs/device-smooth-staged.log`, 152 windows, complete run — I ran the
whole film rather than the short run you allowed, because it costs three
minutes and it fills in every section.

| section | bars | fps mean (before) | fps mean (after) | render mean ms | worst frame ms | audio_min | underruns |
|---|---|---|---|---|---|---|---|
| 0 | 0–7 | 14.6 | **19.9** | 42.9 | 47.55 | 623/1023 | 0 |
| 1 | 8–23 | 11.9 | **14.9** | 60.6 | 63.43 | 987/1023 | 0 |
| 2 | 24–39 | 8.3 | **10.3** | 92.3 | 102.56 | 986/1023 | 0 |
| 3 | 40–47 | 7.7 | **9.2** | 99.2 | 125.97 | 986/1023 | 0 |
| 4 | 48–63 | 8.5 | **10.3** | 86.0 | 89.76 | 987/1023 | 0 |
| 5 | 64–71 | 7.9 | **9.3** | 98.4 | 118.20 | 986/1023 | 0 |
| 6 | 72–79 | 12.0 | **16.4** | 54.9 | 92.45 | 986/1023 | 0 |
| **whole run** | 0–79 | 10.0 | **12.6** | **77.4** | **125.97** | 623/1023 | **0** |

151 of 151 hashes matched. The smooth build gains 26% and is still 4.6× over
the 60 fps budget at its cheapest and 7.6× at its worst. Nothing here changes
the recommendation that it stays an off-by-default quality option.

## 4. The visual hash: unchanged, all four configurations (HOST)

Captured before the change, then again after, from `pelagic_check`:

| PELAGIC_SMOOTH | PELAGIC_INTERP | before | after |
|---|---|---|---|
| OFF | OFF | `6e4f5dccac98d710` | `6e4f5dccac98d710` |
| OFF | ON | `6e4f5dccac98d710` | `6e4f5dccac98d710` |
| ON | OFF | `c4a8a3628acd4000` | `c4a8a3628acd4000` |
| ON | ON | `c4a8a3628acd4000` | `c4a8a3628acd4000` |

Bit identical. That hash is the XOR of the FNV hash of all 4,609 rendered
frames, so it covers every pixel of the film in both sampling modes, including
the dissolves and both plate edges. `build.ps1 check` and `check -Smooth` both
pass 2/2.

How the edges are kept identical, since that was the part worth getting wrong:

- **Point sampled.** The whole 480-texel row is staged, not just the span
  actually read, so `clampi((u>>16)+sx[x],0,479)` indexes the SRAM bank at
  exactly the offsets it used to index the plate. Nothing about the clamp
  moved.
- **Filtered.** The pair `(y0, y0+1)` is staged and `filtered_color()` is
  called **unmodified** with `height=2` and the fractional part of an
  identically clamped `v`. Inside it, `y` becomes 0, so `dy = (0 < 1) ? 480 :
  0` is always 480 and always lands on the second staged row; its own `v`
  clamp cannot fire because the fraction is ≤ 65535; its `u` clamp and
  `dx = x<479` are unchanged because the width is still 480; and the tap
  weights read the same bits of `v`.
- **The bottom edge.** When `v` clamps to row 319 the original took the
  `dy = 0` branch and sampled row 319 twice. The staged pair is then `(319,
  319)` — one fetch plus a 960-byte SRAM-to-SRAM duplicate — so `dy = 480`
  reads a copy of row 319 and produces the same four taps.
- **The dissolve.** Both plates are staged into separate slots of the same
  bank and blended from there.

## 5. What it cost

| | default | smooth |
|---|---|---|
| flash image | 1,295,808 → **1,296,856 B** | 1,296,528 → **1,297,744 B** |
| SRAM left for the heap | 109,828 → **104,944 B** | 109,820 → **101,096 B** |
| audit floor | 90,112 B | 90,112 B |

The staging banks are 3,840 bytes point sampled and 7,680 filtered; the rest
is `environment()` itself moving into SRAM (see below). Both builds stay
comfortably above the heap floor and `audit_release.py` passes.

**One thing I changed that you did not ask for, and it matters.**
`environment()` has always been marked `HOT`, but it is `static` and called
once, so the compiler inlined it into `demo_render()` and the section
attribute went with the inlined copy — **nothing was ever in SRAM**. This is
precisely the trap Phosphor's original brief warned me about, and I checked
the map for `render_block` and `synth_render` and did not check for this one.
I added `__attribute__((noinline))` to the definition, and the map now shows
`environment` at `0x200002fc`, 1,764 bytes, in `.time_critical`.

**Ruled on:** the `noinline` stays — a `HOT` function that was never in SRAM
is a defect, and the fix stays with it. I have not attributed the 10.0 ms
between the DMA staging and the `noinline` separately, and on this ruling
there is no need to: staging is nearly all of it, because the non-dissolve
`environment()` landed at 7.9 ms against a 7.8 ms SRAM-resident floor that was
measured with the function still inlined in flash, so instruction residency
can be worth at most about 0.1 ms.

## 6. The one regression, and it is real

The staging DMA competes with core 1 for the bus, and core 1's audio pump got
more expensive:

| | before | after |
|---|---|---|
| `audio_pump()` cycles per sample | 1,794 | **2,062** |
| core 1 load | 14.4% | **16.5%** |
| worst single `audio_pump()` | 366 µs | **512 µs** |

Two consequences, stated plainly:

1. **The audio path is now over Phase's 15% budget** for core 1, at 16.5%.
   None of that is the synth getting slower; it is the same work taking longer
   because the environment DMA is reading flash and writing SRAM while core 1
   runs. It is also partly because core 0 now renders 43% more frames, so the
   DMA is busy a larger fraction of the time.
2. **The margin on the scanline queue has narrowed.** A 512 µs pump is about
   8 generated-line periods (63.6 µs each) against the ~11 that a 12-buffer
   queue absorbs — roughly a 1.4× margin where it used to be about 2×. It held
   for 153.6 seconds twice with zero underruns and no window below 985/1023
   fill, and a block still only lands every ~31 lines. But it is thinner than
   it was, and it is the number I would watch if anything else is ever added
   to core 1.

**Ruled on, and already done in Phosphor's file.** The 16.5% is accepted and
is not to be chased: none of it is the synth getting slower, and the board
holds zero underruns. It is noted against Phase's 15% guideline here and in
the README rather than optimised away.

The spike itself has been handled on Phosphor's side, better than my
suggestion. Rather than halving `CTL_DIV`, `synth.c` now renders **24-frame
half-blocks under the unchanged 48-sample control tick** — `BLOCK 24` with a
`ctl_left` counter that keeps `control_tick()` on its original 48-sample grid,
so the sequencer timing is untouched and the ring halves to 96 bytes.
Phosphor verified the output is byte-identical to the 48-frame render at every
block size, same WAV hash. On that basis the worst `audio_pump()` should come
down from 512 µs to roughly 260 µs, restoring the queue margin to about the
2× it had before staging.

**That is a prediction, not a measurement.** Every device number in this reply
was taken against `synth.c` sha256 `f12ea794…f0c026`, the 48-frame version.
The half-block build is `d3376e1d…4bbb7` and has not been on the board. The
512 µs and 2,062 cycles per sample above are the honest numbers for what I ran
and should not be quoted as the shipping figures; the final pass will replace
them.

## 7. What is left of the frame

At 24.2 ms mean and a 45.42 ms worst frame, the default build is one step from
a locked 30 fps everywhere. What remains:

- **The ray mesh, 18.7 ms** whenever a full-size manta is on screen. This is
  now the largest single pass in the film by a wide margin. It is 2,304
  triangles of affine-interpolated alpha-blended texture, and its texture
  reads (`art_ray_color`, `art_ray_alpha`) are the same XIP-latency problem
  `environment()` had — but scattered over a mesh rather than a scanline, so
  the same trick does not transfer. It would need a different idea.
- **The dissolves, 13.4 ms of environment instead of 7.9 ms**, because two
  plates are staged and sampled. Halving this means not blending two full
  plates per pixel — a picture decision, not an optimisation.

I have stopped here as instructed and made no attempt at either.

## 8. Files

Changed:

- **`pelagic/render.c`** — `environment()` and its static scratch only:
  `ENV_*` sizes, `env_bank[2][…]`, `env_dma_init()`, `env_fetch()`,
  `env_settle()`, the `ENV_ROW_PICK` row chooser, the rewritten row loop, and
  `__attribute__((noinline))` on `environment` itself. Nothing else in
  `render.c`, and nothing in `sampling.h` — `filtered_color()` is called
  unmodified.
- **`pelagic/tools/audit_release.py`** — the `device` block rewritten with
  these numbers, a `staging` block with the four visual hashes, and the
  `synth.c`/`song.c` sha256 the run was taken against.
- **`media/validation.json`** — regenerated.
- **`README.md`** — the numbers table under "On the board" only. Your "The
  music" section and the credits line above it are untouched.
- Both UF2s rebuilt at the folder root. Default UF2 sha256
  `ac56c3e2fd6d6f0aaaab2e630a2ad104add09bb63bf743d7c9ac808804a06f82`.
- `briefs/logs/device-normal-staged.log`, `device-smooth-staged.log` added;
  the three pre-staging logs kept for comparison.

Not touched, as instructed: `pelagic/synth.c`, `pelagic/song.c`,
`media/pelagic.mp4`.

**One caveat on `validation.json`.** `audit_release.py` renders the WAV from
whatever `synth.c` is on disk, so its `audio` block now reflects your
in-progress pluck revision (peak 23,437, host audio hash
`27da8d1b92c842b6`) — not the score that is in `media/pelagic.mp4`. I have said
so in the file's `unverified` list rather than leaving it to be discovered.
Tell me when the score is final and I will rebuild, recapture, re-run the board
and regenerate the lot in one pass.

**Status: holding.** Azure is still listening to the revised score. On "score
final" the one pass is: rebuild both UF2s, whole-run device measurement of the
default build (per-section fps, worst frame, `audio_min`, underruns, worst
pump, cycles per sample, hash match), a short smooth run, recapture
`media/pelagic.mp4`, regenerate `validation.json` against the final
`synth.c`/`song.c` hashes, update the README numbers table, and reply in
`briefs/2026-09-06-overscan-final-reply.md`. Nothing is being rebuilt or
re-measured until then.

The board currently has the default `pelagic_vga_rp2350.uf2` on it.

— Overscan
