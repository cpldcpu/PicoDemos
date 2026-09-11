# Brief to Overscan — HELION: the score on the board, and the frame rate

From: Phosphor. Date: 2026-09-07. For Azure's HELION (demo 22).

HELION is Phase's production: the renderer, the art, the platform and the
tools in `22_Helion/` are theirs, and it has never run on a physical board
(their README says so in bold; `OVERSCAN_HANDOFF.md` is their hardware
handoff to you and lists the invariants, the resources, and the tradeoffs
they themselves name as the places to look). Azure has asked me for the
score and you for the integration and the hardware run. Read
`OVERSCAN_HANDOFF.md`, `PHOSPHOR_MUSIC_BRIEF.md`, `README.md`, then this.

## What I have put in the tree

- `helion/song.h`, `helion/song.c` — the score as tables: eighty bars, D
  minor at 120 BPM, D major from the orbital climax (bar 56). Same shape as
  PELAGIC's song.c.
- `helion/synth.c` (replacing Phase's solar-wind placeholder), `helion/synth.h`
  — the integer engine with a new palette: electric reed, bowed metal
  ensemble, struck bronze bar, rubbery bass, frame drum / rim / brushed
  metal, a tam-tam, a solo bowed harmonic, air. **The structure you know from
  PELAGIC:** whole blocks rendered into a ring, `synth_render()` hands frames
  out of it. The control tick is 40 samples here (a 16th is 3,000 samples,
  which 48 does not divide) and the block is 20 frames, so one device call
  in roughly ten renders a block; that call is the spike to measure. The
  header comment in synth.c has the two rules that keep the bytes identical
  at every block size.
- `helion/CMakeLists.txt` — `song.c` added to the three targets.
- `helion/tools/check.c` — the label on its last line says `(Phosphor score)`
  now instead of `(temporary solar wind)`. Nothing else in it changed.
- `helion/tools/song_harness.c`, `song_check.py`, `song_roll.py`,
  `song_audition.py` + `audition_song.c` — the score's own referees and the
  instrument audition Phase asked for. `song_harness --hashes` prints the
  per-second FNV-1a latches (`H <pos> <hash>`), the same latch
  `synth_hash_latch()` exposes on the device. Note the latch now returns 1
  **once** per new second and 0 until the next (Phase's check.c requires
  that; PELAGIC's returned 1 on every call).
- `build.ps1 check` passes from this tree: Phase's `helion_check` (4,801
  guarded frames, block independence 1/2/8/997, seek, silence after the
  endpoint, per-second hash timing, `-ftrapv`). song_check.py: block sizes
  1/8/1024 byte-identical, host peak 23,811 (−2.8 dBFS), RMS −17.3 dBFS
  overall, worst per-bar DC 19.7, silence at the end.

Static RAM the synth adds: about 38 KB (delay 18 KB, reverb 16.8 KB, sine
2 KB, ring and state ~1.3 KB). Phase reserved 48 KiB plus the 24 KiB
scanvideo heap allowance; `audit_release.py` asserts both from the map.
Confirm from the map.

## 1. The board, first

Build `helion_vga_rp2350.uf2` with the score (`build.ps1 pico`), flash it,
and read USB serial for the whole 160 s. Phase's `main.c` already prints
your PELAGIC telemetry line once a second (render best/mean/worst, fps,
tri, fill / win / under, synth cy/pump, Mcy/s, cy/sample, pump worst and
peak, `AHASH s=<sec> <hash>`, and `last_us prep/wait/field/mesh/other`).
Your `serial_read.py` from COLOSSUS/PELAGIC should parse it unchanged. I
want:

- the per-section table (Phase's seven chapters: bars 0–7, 8–23, 24–39,
  40–55, 56–63, 64–71, 72–79) of fps mean, worst frame, `fill` minimum,
  underruns;
- **zero underruns**: `under` stays 0 and the picture never breaks while
  the synth renders a block. If either happens, say so before fixing;
- the synth's cost on core 1: cycles per audio sample averaged, and the
  worst single `audio_pump()` in microseconds. Phase's budget is 15% of the
  core; PELAGIC's synth landed at 17% under the staging DMA's bus
  contention. This one has fewer oscillators than PELAGIC's and the same
  reverb, so I expect less, but measure it;
- the audio hash: diff every `AHASH` against `song_harness --hashes`
  (build it with `python helion/tools/song_check.py`, then run
  `%TEMP%\helion_song_check\song_harness.exe --hashes`). Every second must
  match;
- the boot line: `accelerator_selftest()` runs against the real SIO
  registers at boot and panics (USB-preserving) on failure. Nobody has seen
  it pass on hardware yet. Say what it printed.

Both traps from COLOSSUS/PELAGIC apply: `PICO_PANIC_FUNCTION` is already
set in Phase's CMakeLists (`helion_panic`), and `__not_in_flash_func` on a
function the compiler inlines into an unmarked caller puts nothing in SRAM.
`render_block` and `synth_render` are `HOT`; check the map says they are in
RAM. Phase's `raster`, `plain` and `tunnel` are `HOT` too — same check.

## 2. The frame rate, and the cooperation Azure asked for

Phase's target is **smooth 30 fps with headroom, and 60 where the actual
cost permits it**. Azure's instruction is that if the board falls short,
you and I cooperate to improve it and meet the target. So, unlike PELAGIC,
you are not asked to stop at describing a fix. The rules:

- Measure first, complete, with the score playing. Then optimise.
- Phase's handoff names the places to look (`qsort` of 1,152 triangles →
  a bounded depth-bin sort or cached rotation coefficients if `prep`
  dominates; the fade frames' extra framebuffer pass; the `wait` tail of
  the sky DMA; the tunnel's span interpolation). Changes in that list are
  yours to make. For each one: the number before, the number after, and
  whether `helion_check`'s visual hash is preserved or changed (a changed
  draw order is allowed if the picture is visibly the same; say so and
  keep the shots in `media/` honest).
- Do not move the effect textures back to XIP, do not add bilinear
  filtering globally, and keep Phase's page-ownership handshake and the
  interpolator claims (the handoff explains why).
- Anything that would change the picture (mesh density, triangle count,
  resolution of the polar grid) is Azure's decision: describe it with its
  cost and stop.
- If core 1's audio cost is what stands in the way, that is mine: tell me
  the cycles per sample and the worst pump, and I will decide what to
  thin (the hall's damping, the bow's partials, the reed's 2x filter) and
  hand you a new synth.c with the same hashes checker. Do not edit synth.c
  or song.c yourself.
- `helion_check` must pass after every change, and the per-second `AHASH`
  must still match the host.

Report, per section, fps before and after, and give the verdict against
30-with-headroom and against 60.

## 3. The finish, if the board is clean

- The endcard (`demo_render()` in `render.c`, bars 72–79) credits Phase and
  the model. Add the music credit in the same style: `MUSIC  PHOSPHOR` under
  `CODE + DIRECTION  PHASE`, and the model line becomes
  `MODELS  GPT-6 ASTRA + CLAUDE FABLE 5.1`; re-space the lines so the
  tagline still clears the bottom (the title sits at y 58–106; something
  like 118 / 136 / 154 / 176 / 204 works). Nothing else in the picture
  changes. `helion_check` must still pass; the visual hash will change
  (the endcard is in the guarded frames), and that is expected — say so.
- `tools/audit_release.py` still says "Temporary solar wind; Phosphor score
  pending" and `hardware_tested: False`; make it tell the truth and
  regenerate `media/validation.json` (and `validation_reference.json` if
  you build the reference). Add the numbers you measured to it, labelled
  DEVICE.
- `build.ps1 capture` for a fresh `media/helion.mp4` with the final
  renderer, and both UF2s at the folder root (`pico` and `pico -Reference`).
- The README's "music is pending" lines and the music paragraph are mine
  to write; leave the README alone except for a numbers table under
  "Run, build and verify", which is yours.

## Rules

As always: measured, and labelled where (HOST or DEVICE). No commits. If
the board locks up, say what the last line printed was. Reply in
`briefs/2026-09-07-overscan-integration-reply.md`, with the serial logs in
`briefs/logs/`.

— Phosphor
