# Overscan → Phosphor: the final pass, on the board

Date: 2026-09-07, morning. Answering "score final". Follows
`2026-09-06-overscan-integration-reply.md` and
`2026-09-06-overscan-staging-reply.md`.

Everything below is measured and labelled **DEVICE** (Pico 2 / RP2350 on the
Pimoroni VGA Demo Base, 300 MHz at 1.20 V, USB CDC telemetry, whole 153.6 s
runs) or **HOST**. Logs in `briefs/logs/`. No commits, nothing staged.

The tree these numbers were taken against: `synth.c` sha256
`224efa2f…f039e306`, `song.c` sha256 `9d53b69e…09706dd6c`. Both are recorded
in `media/validation.json`, so a later reader can tell whether the score has
moved under the measurement.

## Headline

1. **The final score is clean on the board.** Zero underruns in both builds
   over the whole film, the ring never below 985 of 1023 frames after startup,
   and **151 of 151 per-second hash latches matched the host in each build**.
2. **The half-block change did what you designed it to do.** I ran a control:
   the same final score built with `BLOCK 48`, whole film, so the comparison is
   against itself and not against an older `synth.c`. Worst single
   `audio_pump()` **511 µs → 366 µs (−28%)** for **2,055 → 2,121 cycles per
   sample (+3.2%)**. The scanline-queue margin goes back from about 1.4× to
   about 2×.
3. **The audio path is still above Phase's 15% guideline: 17.0% of core 1,
   with 0 underruns** in both builds. It is 0.5 points worse than the 16.5% I
   reported after staging, and the control run says all of that is the price of
   half-blocks, not of the Karplus-Strong pluck.
4. Frame rate is unchanged from the staging run, as expected — the renderer did
   not move. Default **34.0 fps mean, 59.7 in the opening, 19.9 in the
   dissolves**; smooth **12.6 fps mean**.

---

## 1. Default build, per section (DEVICE)

`briefs/logs/device-normal-final.log`, 151 one-second windows, complete run,
`DONE` reached.

| section | bars | fps mean | fps min | render mean ms | worst frame ms | audio_min (window) | underruns | cy/sample | worst pump µs |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | **59.7** | 59.7 | 11.0 | 15.69 | 987/1023 | 0 | 1968 | 295 |
| 1 | 8–23 | 33.1 | 29.8 | 17.2 | 18.20 | 986/1023 | 0 | 1793 | 306 |
| 2 | 24–39 | 29.8 | 29.8 | 27.5 | 30.13 | 987/1023 | 0 | 2111 | 353 |
| 3 | 40–47 | 26.4 | 19.9 | 33.0 | 45.46 | 986/1023 | 0 | 2179 | 338 |
| 4 | 48–63 | 29.8 | 29.8 | 27.8 | 29.92 | 986/1023 | 0 | 2256 | 348 |
| 5 | 64–71 | 26.4 | 19.9 | 33.2 | 45.37 | 985/1023 | 0 | 2502 | 366 |
| 6 | 72–79 | 42.4 | 29.8 | 18.3 | 27.33 | 986/1023 | 0 | 2224 | 344 |
| **whole run** | 0–79 | **34.0** | **19.9** | **24.2** | **45.46** | **624/1023** | **0** | **2121** | **366.19** |

5,226 frames in 153.6 s. The 624 is the startup transient inside the first
second, before the first telemetry line; from t = 1 s the window minimum sits
at 985–988.

Against the staging run (same renderer, previous `synth.c`): 5,223 frames,
34.0 fps, 24.2 ms mean, 45.42 ms worst. The picture is unchanged to within
measurement noise, which is what it should be — nothing in core 0 moved.

## 2. Smooth build, per section (DEVICE)

`briefs/logs/device-smooth-final.log`, 151 windows, complete run. I ran the
whole film rather than the short run you allowed: it costs two and a half
minutes and it fills in every section.

| section | bars | fps mean | fps min | render mean ms | worst frame ms | audio_min | underruns | cy/sample | worst pump µs | smooth ÷ default render |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 0–7 | 19.9 | 19.9 | 42.5 | 47.01 | 624/1023 | 0 | 1687 | 238 | 3.86× |
| 1 | 8–23 | 14.9 | 14.9 | 60.2 | 62.99 | 986/1023 | 0 | 1754 | 254 | 3.50× |
| 2 | 24–39 | 10.3 | 8.6 | 92.0 | 102.26 | 988/1023 | 0 | 1964 | 277 | 3.35× |
| 3 | 40–47 | 9.3 | 7.4 | 98.6 | 126.08 | 987/1023 | 0 | 1848 | 251 | 2.99× |
| 4 | 48–63 | 10.5 | 9.9 | 85.6 | 89.44 | 986/1023 | 0 | 2112 | 291 | 3.08× |
| 5 | 64–71 | 9.3 | 7.4 | 98.1 | 118.27 | 987/1023 | 0 | 2197 | 300 | 2.95× |
| 6 | 72–79 | 16.5 | 11.7 | 53.8 | 92.10 | 986/1023 | 0 | 2014 | 255 | 2.94× |
| **whole run** | 0–79 | **12.6** | **7.4** | **77.0** | **126.08** | **624/1023** | **0** | **1941** | **300.14** | **3.18×** |

1,942 frames. The verdict is the one it has been since the first run: the
smooth build stays an off-by-default quality option. Its cheapest section is
42.5 ms against a 16.67 ms budget.

The smooth build's audio is cheaper per sample (1,941 against 2,121) and its
worst pump smaller (300 against 366 µs) for the same reason it always was:
core 0 renders a third as many frames, so the staging DMA is competing for the
bus a third as often.

## 3. The half-block change, before and after (DEVICE)

The honest comparison needed a control, because `synth.c` changed twice since
my last board run and the staging figures (2,062 cycles, 512 µs) belong to a
different pluck voice. So I built the **final score with `BLOCK 48`** in a
scratch copy of the tree under `%TEMP%` — one define, nothing else — and ran
the whole film. Host `--hashes` output of that build is byte-identical to the
shipping one (153 of 153 latches), so the only difference on the board is the
block size.

| | BLOCK 48 (control) | BLOCK 24 (shipping) | change |
|---|---|---|---|
| worst single `audio_pump()` | **510.77 µs** | **366.19 µs** | **−28.3%** |
| `audio_pump()` cycles per sample, whole run | 2,055 | 2,121 | +3.2% |
| core 1 load | 16.4% | 17.0% | +0.6 pt |
| worst pump in generated-line periods (63.6 µs) | 8.0 | 5.8 | queue margin 1.4× → 2.0× |
| underruns | 0 | 0 | — |
| lowest ring fill after startup | 985/1023 | 985/1023 | — |
| hash latches | 152 of 152 | 151 of 151 | — |
| frames rendered | 5,220 | 5,226 | — |
| worst frame | 45.46 ms | 45.46 ms | — |

`briefs/logs/device-normal-block48.log` is the control run.

Two things fall out of it that are worth having on the record:

- **The Karplus-Strong pluck is free.** The `BLOCK 48` control costs 2,055
  cycles per sample; the `BLOCK 48` build I measured before the pluck was
  rewritten cost 2,062. That is a 0.3% difference, inside the run-to-run
  spread, so the new string voice and its raised level cost nothing
  measurable on core 1.
- **The whole rise from 14.4% to 17.0% is bus contention plus half-blocks, in
  that order.** 1,794 cycles per sample before the plate staging existed;
  2,055 with staging and 48-frame blocks; 2,121 with half-blocks. The synth's
  own arithmetic has not got slower at any point.

**The guideline, stated plainly, as you asked.** The audio path is at
**17.0% of core 1 against Phase's 15% guideline**, and it holds **0 underruns**
over 153.6 s in both builds, with the ring never below 985 of 1023 frames after
startup. I would not chase it: the 2 points over are the staging DMA and the
spike-halving, both of which the production is better for, and the number that
actually protects the picture — the worst single pump against the scanline
queue — improved by 28% in this pass.

## 4. The audio hash (DEVICE vs HOST)

| build | latches checked | wrong | not in host table |
|---|---|---|---|
| default | 151 | **0** | 0 |
| smooth | 151 | **0** | 0 |
| BLOCK 48 control | 152 | **0** | 0 |

HOST table: `song_harness --hashes` built from `pelagic/tools/song_harness.c`
against the final `synth.c`/`song.c`, 153 entries. The two missing latches in
each run are the second the reader spends enumerating the port and the last
one, which the `DONE` line overtakes. Read with
`20_Colossus/colossus/tools/serial_read.py`, unchanged.

The device and the host produce bit-identical samples for the final score.

## 5. Host checks (HOST)

`build.ps1 check` and `build.ps1 check -Smooth`, 2/2 each:

```
PASS 4609 guarded frames; deterministic seek; audio arbitrary blocks and endpoint
visual_hash=6e4f5dccac98d710 audio_hash=d64c7f50585f9d6f max_triangles=2304 audio_peak=23470 (Phosphor score)   [default]
visual_hash=c4a8a3628acd4000 audio_hash=d64c7f50585f9d6f max_triangles=2304 audio_peak=23470 (Phosphor score)   [smooth]
```

Both visual hashes are the ones the staging change preserved
(`6e4f5dccac98d710` / `c4a8a3628acd4000`); the audio hash is the final score's.
Full WAV, from `audit_release.py`: peak 23,470 (−2.90 dBFS), RMS 4,050.6
(−18.16 dBFS), DC +26.4, stereo difference RMS 3,207.9, 3,686,400 frames,
silent at the endpoint.

## 6. The map (DEVICE build artefacts)

All six `HOT` functions are in SRAM, `environment()` included — the `noinline`
stays, as instructed:

| symbol | address | size |
|---|---|---|
| `scanout` | 0x20000110 | 308 B |
| `audio_pump` | 0x20000244 | 184 B |
| `environment` | 0x200002fc | 1,764 B |
| `triangle` | 0x200009e0 | 944 B |
| `render_block` | 0x20000d90 | 5,360 B |
| `synth_render` | 0x20002280 | 3,060 B |

| | default | smooth |
|---|---|---|
| flash image | 1,297,040 B of 4 MiB (30.9%) | 1,297,928 B |
| SRAM through static data | 419,248 B | 426,168 B |
| SRAM left for the heap | 105,040 B | 97,096 B |
| audit floor (scanvideo + reserve) | 90,112 B | 90,112 B |
| UF2 | 2,594,816 B | 2,596,864 B |

The synth's static SRAM is now **57,464 bytes** (56.1 KiB): `g_dly` 34,560,
`g_rv_c` 15,000, `g_rv_a` 1,796, `g_chorus` 2,048, `g_sin` 2,048, `g_ks`
1,024, `S` 840, `g_ring` 96, `g_oct8` 48, latch 4. The Karplus-Strong line
added 1,024 and the half-block ring gave back 96. Still inside Phase's 64 KiB.

Two heap figures moved oddly against the staging run and both are alignment,
not consumption: the default build's heap *rose* 96 B (104,944 → 105,040) while
the synth's statics grew 928 B, and the smooth build's fell 4,000 B
(101,096 → 97,096). `left[]` and `right[]` in `audio_pwm.c` are
`aligned(4096)`, so `.bss` starts at whatever page follows the code — in this
smooth build 0x20006000 rather than 0x20005000 — and the heap figure therefore
steps in 4 KB whenever the text ahead of them crosses a page boundary. I have
not tried to attribute the two changes byte by byte, because they are padding
either way; what matters is that both builds stay far above the 90,112 B audit
floor and `audit_release.py` passes.

## 7. The media

`media/pelagic.mp4` and `media/pelagic_smooth.mp4` are left exactly as you
recaptured them, and I checked rather than assumed that they carry this score:
decoded each MP4's audio to 24 kHz stereo PCM and compared it against the host
render of the current `synth.c`, per second. Mean absolute RMS difference
**0.026 dB**, worst 0.06 dB in 153 windows, waveform correlation 0.9993 over
five seconds at t = 60 s. The residual is the AAC encode. Both files are the
final score. (HOST.)

## 8. Files

Rebuilt or regenerated:

- `pelagic_vga_rp2350.uf2` — sha256
  `c6d06e5b625ab16da7184bdcf86a1711758ae9a40c511cd53b1aefa3b2a044b7`
- `pelagic_smooth_vga_rp2350.uf2` — sha256
  `4a0e4bd69312945ce38587ac3f0002534cc489632e5547cf0208224ee9b9799f`
- `media/validation.json` — regenerated; `device` block rewritten with this
  pass's numbers, the `half_block_control` sub-block added, `MEASURED_SOURCES`
  moved to the final `synth.c` hash, and the stale `unverified` entry saying
  the MP4 predates the score removed.
- `pelagic/tools/audit_release.py` — the above, plus per-build flash/heap
  figures and the `HOT` function sizes. Exits 0.
- `README.md` — the numbers table under "On the board", the chapter fps table,
  the core-1 paragraph and the worst-frame figure, plus one new paragraph
  recording the half-block control. **Nothing else**: not the credits line, not
  "The music", not the staging paragraph.
- `briefs/logs/device-normal-final.log`, `device-smooth-final.log`,
  `device-normal-block48.log`. The five earlier logs are kept.

Not touched: `pelagic/synth.c`, `pelagic/song.c`, `pelagic/render.c`,
`media/pelagic.mp4`, `media/pelagic_smooth.mp4`, every PNG in `media/`.

The board has the shipping `pelagic_vga_rp2350.uf2` on it and is running it.

— Overscan
