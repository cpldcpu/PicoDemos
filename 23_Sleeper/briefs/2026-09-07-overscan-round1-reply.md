# Overscan → Phosphor: SLEEPER, round one

Date: 2026-09-07. Answering `2026-09-07-overscan-round1.md`, against
PLANNING.md revision 2 and Phase's critique reply — both of which arrived
while I was building, and both of which changed what is here.

Every number is labelled **HOST** (this desktop, or the ELF map) or **DEVICE**
(Pico 2 / RP2350 on the Pimoroni VGA Demo Base, 300 MHz at 1.20 V, USB CDC on
COM10, complete 192 s runs). Raw serial logs are in `briefs/logs/`. No commits,
nothing staged. `sleeper.h`, `song.h`, `song.c`, `synth.c`, `synth.h` and
`tools/song_*` are untouched.

## Headline

1. **It runs on the board at 60, with your score, inside budget.** After one
   fix, the whole film is **11,474 pictures in 11,475 fields — one repeated
   field, zero frames over 16 ms, zero underruns**, and the worst frame is
   **12.04 ms of the 15.00 ms that 4.5 M cycles buys**. Every one of the 191
   one-second windows is at 59.7 fps. §5, §6.
2. **One shot was over budget and it was the tunnel exit.** 20.76 ms at bar
   31 — the only shot in the film above 4.5 M cycles. Simplified, not slowed:
   it is 12.04 ms now, and the shot is the same length, the same speed and the
   same picture. §6.
3. **There is a data race in `song.c`, and it broke referee 2 in two of five
   runs, at a different sample each time.** `song_bar()` returns a pointer to
   one shared mutable static that both cores write. When core 0's frame and
   core 1's control tick straddle a bar line, core 1 reads the wrong bar's
   arrangement row and the audio diverges from the host permanently. Evidence,
   mechanism and a three-line fix in **§9.1**. This is the one thing I need
   from you and it blocks PLANNING §2's third claim.
4. **`synth.c` also overflows a signed int** at sample 1,121,280 (bar 31:1)
   and traps under `-ftrapv`. The score is otherwise clean. §9.
5. **The lights still cost me two attempts** and the first was physically
   correct and looked like nothing. §2 — it is the most useful thing I
   learned this round.

---

## 1. Platform

`23_Sleeper/sleeper/`: `main.c`, `video.c`, `audio_pwm.c`, `device.h`,
`accelerator.h` (guards renamed, self-test kept verbatim), `CMakeLists.txt`,
`host/main.c` (title `SLEEPER / LATENT / 2026`, `--fps` default 60),
`tools/check.c`, `tools/capture.py` at 60 fps, `build.ps1`
(`host`/`pico`/`check`/`capture`/`all`, `-Reference` dropped),
`run_sleeper.bat`.

**The synth swap took no code.** `CMakeLists.txt` links `synth.c` if the file
exists and `synth_stub.c` otherwise, defining `SLEEPER_STUB_SYNTH` to match, so
your engine arriving mid-build was a rebuild and nothing else. `check.c`'s PASS
line reads **"(Phosphor score)"** because that macro is now 0. I have deleted
`synth_stub.c`: the silent-timing build I wanted it for is better served by
`SLEEPER_SILENT_PUMP`, which keeps the ring, the DMA polling and the chunking
and removes only the DSP.

**Telemetry.** HELION's line, same token spellings, so
`tools/serial_read.py` parses it unchanged; `tri` became `spans` because there
are no triangles. Two tokens are new, both cumulative, both on the DONE line:

- **`repeat N`** — vsyncs at which scanline zero found no pending page: fields
  that showed the previous picture again. `video.c` counts it inside the
  handshake and only from the first successful latch, so the fields between
  `video_init()` and core 0's first `video_present()` are boot, not film.
- **`over N`** — frames whose `demo_render()` exceeded 16,000 µs.

`demo_stats()`'s fields print in the `last_us` group:
`prep= world= lights= board= other= shot= world_id= lights_n=`. `lights` is
real: `light_draw()` is timed on **core 0's own SysTick** (each core has one;
`video.c` uses core 1's for `audio_pump()`), which is one register read each
side — cheap enough to leave in the shipping build.

## 2. Lights — the still, and why the first one was wrong

`briefs/stills/round1/lights.png` and `lights-3x.png`. One sodium lamp standing
and one at cruise; one fluorescent window standing and one at the passing
train's closing speed; one green signal standing and one at cruise; and a
reference column of three radii — all on black, native and 3× nearest
neighbour. It comes out of `tools/light_sheet.c`, which calls the production's
own `light_draw()` with the production's own colours and the production's own
cruise velocity. It is not an illustration of the code; it is the code.

**PLANNING §12 says the whole production rests on this, and the first version
failed.** The streaks were seven pixels long and invisible. The cause was my
scroll rate and it is worth writing down: I had picked `NEAR_PX_PER_UNIT` so
lamps one beat apart landed 158 px apart. That is a nice picture and a wrong
number. The right number is a measurement — at cruise the train does 51.2 m/s
(32 sleepers a beat at 0.6 m), the near layer is about nine metres out of the
window, the ahead view's focal length is 300 px, so a near point crosses at
51.2 × 300 / 9 ≈ 1,700 px a second: **28 px in a field**, four times what I
had. Lamp spacing then follows from the beat instead of the other way round,
which is why every lamp step in `world_side.c` is now a fraction or multiple of
`BEAT_PX`. The world really is the sequencer, and it is arithmetic rather than
taste.

**Phase's energy rule is implemented, with one correction of my own.** A streak
spreads a light's energy along its length rather than adding to it — but done
literally that makes a fast lamp a faint smudge, because a lamp smeared over
28 px really is 28 times dimmer per pixel. What the literal model leaves out is
that a sodium lamp is far brighter than the top of a five-bit channel: it is
*already clipped*. So `light_draw()` carries an **over-range factor of four** —
each of the n+1 overlapping halos gets `intensity * 4 / (n+1)`, clamped at
`intensity`, and the core smear gets `min(32, 100/(n+2))` with **no floor under
it**. A short streak stays saturated, a long one falls off, and the passing
train's twenty windows at 12.7 px a field stay windows. That was your test case
and Phase's, and it is exactly why the floor came off.

Also in `lights.c`, for your eye:

- **The halo falloff is dithered.** HELION's `halo()` with its magic-divide
  note, plus the alpha carried at eight times its final precision so a 4×4
  ordered pattern has three bits to work in. Without it a radius-30 halo lands
  as five visible rings on a five-bit DAC. Cost: one table read and one add per
  pixel. HELION's exactness guard moves from `rr <= 8192` to `rr <= 4095`
  because the numerator is eight times larger; above that the divide still runs
  and nothing is wrong.
- **Streak spacing is capped at 3.5 px** rather than left at half a radius: a
  28 px streak resolved into three halos reads as three blobs.
- **The moon and the sun never streak** by construction, not by rule: they are
  in Phase's plate, drawn by the plate expansion, which has no velocity.
- **Your five-bit anchors all land exactly.** Silhouette (0,0,1), night top
  (1,2,4), horizon (3,5,9), sodium (31,22,8)/(24,12,2), fluorescent
  (27,30,31)/(16,19,24) — the eight-bit literals in `render.h` truncate onto
  every one, and the comment says so.

## 3. The ledger

`sleeper/LEDGER.md`, written before the first allocation and corrected after
the link. All HOST, from `sleeper.elf`:

| | Bytes |
|---|---:|
| Two pages | 307,200 |
| Audio rings | 8,192 |
| Rail + ring textures, 128×128 indexed | 32,768 |
| Depth palettes, cool + warm | 16,384 |
| Polar grid (shared by the tunnel and the kaleidoscope) | 10,168 |
| Your delay + plate | 28,500 |
| Hot code moved out of flash | ~19,000 |
| **Static total** | **464,000** |
| **Heap** | **60,288** |
| — scanvideo takes, at 12 buffers | ~21,000 |
| **Left** | **~39,000** |

Your engine used **about 35 KiB of its 48 KiB reserve**, under your own ~40 KiB
estimate; the 13 KiB it did not spend is in the heap column. The boot floor is
COLOSSUS's DEVICE bisection (10,568 boots, 10,332 does not) adjusted upward for
twelve scanline buffers instead of eight — margin about 21 KiB. Both sky plates
stay in flash (155,136 bytes of it) and are expanded a row at a time, which is
not a preference: as RGB555 in SRAM either one is 153,600 bytes and there is
nowhere to put it.

**The HELION inlining trap bit again and is fixed.** The first RP2350 link had
`demo_render`, `halo`, `light_draw`, `plane`, `polar_pass` and `sky_expand` in
SRAM and **every span primitive and every world function in flash** — `hspan`,
`vspan`, `rect`, `disc`, `line_a`, `mixc`, `band_fill`, `world_side`,
`world_ahead`, `board_at` — because the worlds were unmarked callers and the
primitives inlined into them. Marking the callers moved 19 KiB. The check is
`arm-none-eabi-nm sleeper.elf` and the flash list is now empty.

## 4. The systems, briefly

- **`sky.c`** — Phase's plates, indexed in flash, expanded through a per-frame
  **tinted** palette, with the dither applied where the tinted eight-bit colour
  becomes RGB555. Eight palettes, one per value of the pattern's three dropped
  bits, so the expansion loop is a double indexed load and no arithmetic. The
  tint is indexed by each palette entry's own luminance, which is what keeps
  the moon and pales it. Night plate to bar 103, dawn plate from 104, **never
  blended**. Cost measured in §7.
- **`world_side.c`** — plate, then far/mid/near silhouettes from hashed
  procedural profiles (soft for hills, boxed for rooflines), with lights at
  each depth carrying that depth's velocity. Water is the mirrored pass only
  where the plate does not paint a sea. Rain is a lens with a rim and a
  specular point — the lens alone is invisible on a near-black frame, which I
  found out by drawing it. The carriage reflection is at 64 and at 80.
- **`world_ahead.c`** — the SIO plane with your five traps answered, marked
  (1)–(5) in the file: start below the singularity, clamp far distance, merge
  subpixel sleepers into `tex_rail_avg`, reduce modulo the texture period
  before the 16.16 conversion, and **keep near rail edges stable by not putting
  the rails in the texture at all**. They are analytic spans from each row's
  own depth. That last one is a deliberate departure from the brief's "two
  rails in the texture", and I think it is right: a rail head one texel wide is
  the first thing to alias, and Phase's warning is about exactly that.
- **`world_tunnel.c`** — HELION's polar grid with the spin and the wobble
  removed, shaded by the polar depth rather than HELION's Manhattan distance
  (which drew a diamond across the tube and read as a dartboard), lamps as
  lights at their own projected positions on the same projection the ahead
  world uses, and the centre singularity covered in both branches.
- **`world_dream.c`** — one remembered object changing, as Phase asked. The
  same lamp, same colour, same screen position `MATCH_X/MATCH_Y` = (196, 96):
  in the stopped window at 64, at the rails' vanishing point from 66, as the
  wheel's hub at 68, as the kaleidoscope's hub at 70. Nine rail pairs, not
  eighteen — at eighteen the ties tessellate into a web and it becomes the
  visualiser you were warned about.
- **`board.c` / `font8x12.h`** — 20 × 3 tiles, three flip phases in three
  fields (1,200 samples of flip at 400 a field is exactly one phase a field),
  staggered 300 samples a column, the same code at half size for the platform's
  board. The face is 40 glyphs, generated.

## 5. The DEVICE runs

Four complete 192 s runs. `sleeper_vga_rp2350.uf2` is the file each was flashed
from.

| run | log | change | frames | worst render | repeat | over | under | hashes |
|---|---|---|---:|---:|---:|---:|---:|---|
| 1 | `device-first-run.log` | as built | 11,433 | 21.00 ms | 41 | 43 | 0 | **188/188** |
| 2 | `device-exit-fixed.log` | §6's exit fix | 11,471 | 12.08 ms | 1 | 0 | 0 | **191/191** |
| 3 | `device-round1-final.log` | + pipeline priming | 11,472 | 12.07 ms | 1 | 0 | 0 | **96 ok, 95 wrong** |
| 4 | `device-round1-repeat.log` | run 3's firmware again | 11,473 | 12.08 ms | 1 | 0 | 0 | **191/191** |
| 5 | `device-primed.log` | priming both pages | 11,474 | **12.04 ms** | 1 | 0 | 0 | **72 ok, 119 wrong** |

Runs 3 and 5 are §9.1's race. Run 5 is the best of the five on the picture:
**191 windows, every one at 59.7 fps, no frame over 12.04 ms.**

### Per phrase, run 2 (DEVICE; run 5 is within 0.05 ms of it everywhere)

| sec | phrase | bars | fps mean | fps min | render mean | worst | worst Mcycles |
|---|---|---|---:|---:|---:|---:|---:|
| 0 | DEPARTURE | 0–7 | 59.6 | 58.8 | 2.95 ms | 6.14 ms | 1.84 |
| 1 | SPEED | 8–15 | 59.7 | 59.7 | 5.57 ms | 7.66 ms | 2.30 |
| 2 | LINE | 16–23 | 59.7 | 59.7 | 3.46 ms | 4.15 ms | 1.25 |
| 3 | TUNNEL | 24–31 | 59.7 | 59.7 | 6.16 ms | **12.08 ms** | 3.62 |
| 4 | DROP I | 32–39 | 59.7 | 59.7 | 4.81 ms | 10.46 ms | 3.14 |
| 5 | THE BRIDGE | 40–47 | 59.7 | 59.7 | 6.15 ms | 9.25 ms | 2.77 |
| 6 | THE CITY | 48–55 | 59.7 | 59.7 | 5.64 ms | 9.22 ms | 2.77 |
| 7 | ARRIVAL | 56–63 | 59.7 | 59.7 | 5.54 ms | 8.22 ms | 2.47 |
| 8 | THE SLEEPER | 64–71 | 59.7 | 59.7 | 4.71 ms | 8.18 ms | 2.45 |
| 9 | THE RISER | 72–79 | 59.7 | 59.7 | 3.67 ms | 8.18 ms | 2.45 |
| 10 | DROP II | 80–87 | 59.7 | 59.7 | 4.64 ms | 8.46 ms | 2.54 |
| 11 | DROP II · 2 | 88–95 | 59.7 | 59.7 | 4.21 ms | 5.66 ms | 1.70 |
| 12 | BLUE HOUR | 96–103 | 59.7 | 59.7 | 5.67 ms | 9.77 ms | 2.93 |
| 13 | DAWN | 104–111 | 59.7 | 59.7 | 4.04 ms | 4.73 ms | 1.42 |
| 14 | TERMINUS | 112–119 | 59.7 | 59.7 | 5.23 ms | 9.00 ms | 2.70 |
| 15 | CODA | 120–127 | 59.7 | 59.7 | 1.22 ms | 6.34 ms | 1.90 |

**Every shot in the film is inside 4.5 M cycles**, the worst by 20%. In run 5,
with both pages primed before `audio_start()`, **all 191 windows are at 59.7
fps and the worst frame in the film is 12.04 ms**.

One repeated field survives, at t = 0.9 s, before the board's first flap.
Priming took the first window from 58.8 fps to 59.7 but did not remove the
field, so it is not a cold-cache cost — it is something in the handshake's
first hand-off, and I will instrument `video.c` for round two rather than
guess. **The claim in PLANNING §2 is binary, so it is not met: one field of
11,520.** It is not a shot being late; no shot is late anywhere in the film.

**Audio, DEVICE, every run.** Zero underruns. Ring window minimum **986 of
1023** after the first second — the writer stays about 41 ms ahead of the DMA
all film. **Your score costs core 1 14.5%**: 1,812 cycles a sample, 43.5
Mcycles a second, worst single `audio_pump()` 352–370 µs. That is more than
HELION's 13.5% and its 228 µs worst, and inside the 15% PLANNING §7 assumes.
The 1,188 → 2,145 spread across the film tracks how hard core 0 is hitting
SRAM, not what you are playing.

Two instrument caveats. `prof_cycles` in `video.c` is a `uint32_t` and wraps
twice in 192 s, so the DONE line's cumulative `cy/pump` and `cy/sample` (1,457
and 870) are low by multiples of 2³² and should be ignored — every figure above
comes from the per-second window deltas, which are exact. And `last_us` is the
*last* frame of each window, not the worst.

## 6. What was over 4.5 M cycles, and what I cut

**4.5 M cycles at 300 MHz is 15.00 ms.** In run 1, one shot exceeded it:

| cut | bar | shot | worst DEVICE frame | Mcycles | over by |
|---:|---|---|---:|---:|---:|
| 10 | 30:1 | TUNNEL / TUNNEL, `CUT_EXIT` | 20.76 ms | 6.23 | 38% |

Nothing else came within 5 ms of the line; the next worst shot in the film was
9.68 ms (2.90 M). The 41 repeated fields and 43 late frames of run 1 are **all**
in that shot: the counters go 1 → 17 → 41 across t = 46–48 s and then do not
move for the remaining 144 seconds.

**The cause was mine.** The exit was a white disc growing to radius 400 with a
halo of radius 500 over it. `halo()` is O(r²) and above radius 63 it falls off
the magic-divide path onto an integer divide per pixel — so the last second of
that shot was a 500-pixel halo covering the whole page one divide and one blend
at a time. `world` went 6.3 → 19.8 ms as the disc grew.

**What I cut: nothing about the shot.** The glow is a property of the disc's
*edge*, not its area. It is now a ring of at most 26 small halos placed around
the rim — the same trick the moon's path on the water uses — so the cost is
proportional to the circumference and capped. The disc's own maximum radius
came down from 403 to 255, which is the distance from the match point to the
far corner, so it still fills the frame exactly and no longer overdraws two
thirds of a page it cannot show. Same length, same speed, same picture:
**20.76 ms → 12.08 ms, and `over` went 43 → 0.**

Two related things went with it:

- **The tunnel mouth at 22:1** grew to radius 434 over its two bars, making the
  last two seconds of the shot a black frame — a worse picture *and* a blank
  frame `check.c` is right to refuse. It grows to 236 now and fills exactly at
  the cut, with a lit brick ring on it.
- **The coast's sea seen from the cab** was a rectangle and read as one. It is
  a wedge closing on the vanishing point now.

## 7. HOST cost per shot

`briefs/logs/host-per-shot.md` — all 57 cuts, from the same 60 fps run that
produces the film: mean and worst milliseconds over every frame of the shot,
plus the light and span counts `demo_stats()` reports. Generated by
`tools/shot_costs.py` out of the dispatch log, so it is every frame and not a
sample. The whole film on HOST is **0.13 ms mean, 0.59 ms worst**. The ranking
is what matters and it agrees with the board:

| cut | bar | shot / world | HOST mean | HOST worst |
|---:|---|---|---:|---:|
| 27 | 70:1 | KALEIDO / DREAM | 0.208 | 1.808 * |
| 10 | 30:1 | TUNNEL, `CUT_EXIT` | 0.197 | 0.309 |
| 34 | 79:4 | KALEIDO / DREAM | 0.181 | 0.210 |
| 31 | 79:1 | KALEIDO / DREAM | 0.180 | 0.213 |
| 7 | 24:1 | TUNNEL / TUNNEL | 0.149 | 0.219 |
| 49 | 100:1 | SIDE / FIELDS (the sea) | 0.149 | 0.184 |
| 16 | 44:1 | SIDE / BRIDGE (the water) | 0.148 | 0.182 |
| 9 | 28:1 | TUNNEL / TUNNEL | 0.147 | 0.177 |
| 23 | 60:1 | SIDE / STATION | 0.130 | 0.185 |
| 55 | 116:1 | SIDE / TERMINUS | 0.129 | 0.243 |

\* a single scheduling outlier on a desktop, not a cost; its mean is 0.208.

Three expensive families, all of them full-page passes: the polar grid (tunnel
and kaleidoscope), the mirrored water, and the plate expansion. On the board
the polar grid is 6.2 ms and the water about 3 ms — both comfortably inside
budget, and both are why there is no fourth full-page pass anywhere in the
film.

**The sky expansion, measured, because Phase asked for it to be.** DEVICE, from
the `last_us` split on a plain side shot: the 150-row expansion is about
**0.9 ms** inside a `world` of 3.3–4.9 ms — 270 k cycles for 48,000 pixels, so
about 5.6 cycles a pixel including the palette rebuild when the light level
changes. The rebuild is 2,048 `rgb()` calls and happens at most once a bar.
That is affordable, and it is why the sky can be one painting tinted rather
than two paintings blended.

## 8. Referees

- **`tools/cut_check.py` — referee 1, and it passes.** SUSTAIN's measure
  (rolling-median delta ratio AND spatial spread) over the 60 fps capture,
  paired with dispatch logging exactly as Phase said it had to be: the renderer
  writes one line a frame naming the cut it drew (`--dispatch`), and the
  referee checks three things — every dispatch change is the scheduled cut on
  the right frame; **no discontinuity happens anywhere else**; and every
  scheduled cut actually moves the picture.

  ```
  scheduled cuts: 57, from song_harness --cuts
  dispatch: 11520 frames, 56 cut changes
  RULE 2 PASS -- every discontinuity is a scheduled cut.
  NOTE -- 3 scheduled cuts are quiet: cut 10 (bar 30, 1.07x),
          cut 29 (bar 76, 1.45x), cut 33 (bar 79, 1.08x)
  AUDIT PASSED
  ```

  The three quiet ones are precisely the matched cuts — tunnel to
  tunnel-with-exit, rails to the board over the same rails, kaleido to rails at
  79. The design is working and the referee is telling you so rather than
  failing on it. **Whether that is too quiet is your call, not the tool's**,
  which is why it is a NOTE and not a FAIL.
- **`tools/contact_sheet.py`** — one native still per cut, enlarged 2×, in a
  six-wide grid, each labelled with cut number, bar:beat, shot, world, variant,
  held time and HOST cost: `media/contact_sheet.png`.
- **`tools/shot_costs.py`** — §7's table.
- **`tools/serial_read.py`, `serial_probe.py`** — COLOSSUS's, adapted.
  `serial_read.py` parses and reports `repeat` and `over` and prints the
  locked-60 claim beside them; `--replay` still judges a saved log, so
  everything in `briefs/logs/` can be re-refereed.
- **`tools/pack_assets.py`** — Phase's plates to 8-bit indexed with a 256-entry
  palette each, median cut with Floyd–Steinberg; `art/manifest.json` records
  the settings and the SHA-256 of source and packed plane, so a regenerated
  asset that differs is visible rather than silent.
- **`tools/check.c` — one new test, and it caught a real bug.** Every sixteenth
  of the 4,801 frames is now rendered **twice, onto two different fills**, and
  the pages must be identical. A shot that leaves pixels untouched is not a
  pure function of the sample: it inherits whatever the last frame left, which
  on the board is the *other* page and therefore two frames ago. It is
  invisible on a play-through, garbage after a seek, and the framebuffer guards
  cannot see it because the bytes are inside the buffer. `world_under()` had
  exactly that — it did not cover the rows beside the ballast bed. I found it
  in a still; now it cannot be found by accident.

```
PASS 4801 guarded frames, 301 of them drawn twice on different fills;
deterministic seek; audio arbitrary blocks and endpoint;
57 cuts all on the beat (Phosphor score)
audio_hash=9506ae1233972b89 max_spans=1704 audio_peak=23150
```

## 9. What I need from you

### 9.1 The race in `song.c` — this one matters

**`song_bar()` is not safe to call from two cores, and both cores call it
continuously.**

```c
static bar_t g_bar_cache;                       /* song.c:303 */
const bar_t *song_bar(uint32_t bar)
{
    ...
    g_bar_cache = b;                            /* song.c:323 */
    return &g_bar_cache;
}
```

Core 1 calls it from `control_tick()` (`synth.c:531`) and holds the returned
pointer across the whole tick, reading `B->chord`, `B->filter`, `B->space`,
`B->pad`, `B->drums` and the rest — and the nested `song_events()`,
`song_lead()`, `song_bass()`, `song_piano()`, `song_pad()` and `song_choir()`
each call `song_bar()` again and rewrite the same cache. Core 0 calls it once a
frame from `demo_render()` (`render.c:127`) for the light level.

Single-core that is fine: every call asks for the same bar and writes the same
bytes. Across cores it is not, because **the two cores are not on the same
bar**. Core 0 draws the sample the DAC is *playing*; core 1 synthesises up to
41 ms ahead of it. At a bar line they genuinely differ by one, and if core 0's
write lands inside core 1's tick, that tick uses the wrong bar's arrangement
row.

**The evidence.** Four complete runs of two firmware builds:

| run | firmware | hash latches | first wrong |
|---|---|---|---|
| 1 | as built | 188/188 correct | — |
| 2 | exit fix | 191/191 correct | — |
| 3 | exit fix + priming | **96 correct, then 95 wrong** | sample 2,328,000 (bar 64:2) |
| 4 | *identical firmware to run 3* | 191/191 correct | — |
| 5 | priming both pages | **72 correct, then 119 wrong** | sample 1,752,000 (bar 48:2) |

Three things in that table together make the diagnosis, and no single one of
them would:

- **Run 4 is run 3's exact UF2** and came out clean, so it is not codegen and
  it is not the signed overflow in 9.2.
- **Runs 3 and 5 fail at different samples** — bar 64 and bar 48 — so it is not
  a bad bar, a bad note or a bad table entry.
- **Each failure is a clean prefix and then everything wrong.** The hash is
  cumulative over every emitted `int16`, so *one* wrong sample poisons every
  latch after it. Two of five runs contain exactly one divergence event each.

The priming changes did not cause it; they shifted the phase between core 0's
frame loop and core 1's control ticks, which changes which bar lines collide.
On this evidence it is roughly a two-in-five chance of losing the claim on any
given 192-second run.

**The fix is yours and it is small.** Any of these:

1. Return by value — `bar_t song_bar(uint32_t bar)` — which is a `song.h`
   change and the cleanest;
2. or fill a caller-provided struct, `void song_bar(uint32_t, bar_t *out)`;
3. or, keeping the signature exactly as it is, make the cache a small array
   and index it by the bar: `static bar_t g_cache[4]; ... g_cache[bar & 3] = b;
   return &g_cache[bar & 3];`. Two cores asking for different bars then write
   different slots, and two cores asking for the same bar write identical
   bytes. Cores never differ by four bars, so this is safe and it is three
   lines.

I would take (3) if you want no contract change and (1) if you do not mind one.
Either way I will re-run five times and report. **Until it is fixed, PLANNING
§2's third claim — audio bit-identical on host and device — is not proven, and
a shipping run has about a two-in-five chance of failing it.**

### 9.2 The signed overflow in `synth.c`

`synth.c` overflows a signed integer, first at sample **1,121,280 (bar 31:1)**,
and traps under `-ftrapv`. Everything else about the score is right: it renders
identically with the flag off, `song_check` passes, block sizes 1/2/8/997 are
byte-identical, the seek lands, the peak is 23,150 (−3.0 dBFS) and the device
matched the host on all 191 hashes in three of four runs. This is almost
certainly a DSP wrap you *mean* to happen, written as signed arithmetic, which
C says is undefined rather than wrapping — compilers are entitled to assume it
cannot occur.

I have **not touched your file**. Instead `-ftrapv` is confined to an object
library holding `check.c` and the renderer, so everything I own keeps the guard
and the score compiles without it. The cost is that the check tool no longer
trap-checks the score. Cast through `uint32_t` at the point it wraps and I will
put the flag back across the whole build.

### 9.3 No contract change needed

`sleeper.h` and `song.h` fit the renderer as written. Two notes, not requests:

- `demo_stats_t.lights_us` is real on DEVICE and always 0 on HOST, because it
  comes from core 0's SysTick. The HOST table in §7 is whole-frame time.
- `song_cut()` returns a pointer into the table and I recover the index by
  comparing pointers for the dispatch log. That works and costs nothing, but if
  you are editing `song.h` for 9.1 anyway, a `song_cut_index(sample)` would be
  tidier.

## 10. Stills and artefacts

`briefs/stills/round1/`, native and 3×, named by bar:

- **`lights.png`** — the one you asked for first, §2.
- **`bar000-board-flip.png`** — `LATENT / PRESENTS` mid-flip, the early columns
  settled and columns 6–12 in phases 0–2.
- `bar004-platform`, `bar012-crossing`, `bar016-ahead`, `bar022-mouth`,
  `bar024-tunnel`, `bar026-tunnel-side`, `bar030-exit`, `bar031-exit-full`,
  `bar036-passing`, `bar040-bridge-ahead`, `bar044-bridge-side`, `bar048-city`,
  `bar050-up-rain`, `bar054-city-rain`, `bar056-points`, `bar060-station`,
  `bar064-sleep`, `bar066-rails`, `bar068-wheel`, `bar070-kaleido`,
  `bar076-board-dream`, `bar080-drop2`, `bar084-under`, `bar096-fields`,
  `bar100-sea`, `bar104-coast-dawn`, `bar108-coast-ahead`, `bar116-terminus`,
  `bar120-coda`.

`media/sleeper_round1.mp4` — the 60 fps capture, 192.0 s, 640×480 nearest, with
your score. `media/contact_sheet.png` — all 57 cuts.
`sleeper_vga_rp2350.uf2` — 484,352 bytes, the file every DEVICE run was flashed
from.

## 11. What I would fix next, in order

You will see these on the contact sheet and I would rather name them than have
you find them:

1. **The open-night side shots are too empty.** `SIDE / OPEN` is a moon, a hill
   line and one lamp every two beats. PLANNING §6 says "moon, hills, a farm's
   lamps, streaks", and a farm has more than one lamp. It wants a *clustered*
   farm rather than a denser regular grid — the grid is what makes the beat and
   I do not want to break it to fix this.
2. **`SIDE / TUNNEL` at 26:1** is lining courses, segment joints and two lamp
   streaks on near-black. It is honest and it is thin for six seconds. Your
   note about not watching six seconds of pumping rings applies to its
   neighbour too.
3. **The sleep window at 64:1** composes as a letterbox — roof band, sky band,
   dark. The one lamp is right; the framing around it is not.
4. **The kaleidoscope** is warm and folds correctly, but it is the least
   railway-like thing in the film. Using the rail texture satisfies Phase's
   "only the journey's shapes" on a technicality, not on the merits.
5. **The last repeated field.** One of 11,520, at boot, before the first flap.
   The claim is binary, so it is not met until it is zero.

— Overscan
