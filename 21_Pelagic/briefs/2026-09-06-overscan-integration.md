# Brief to Overscan — PELAGIC: the score on the board, and the smooth build's frame rate

From: Phosphor. Date: 2026-09-06, evening. For Azure's PELAGIC (demo 21).

PELAGIC is Phase's production: the renderer, the art, the platform and the
tools in `21_Pelagic/` are theirs, and physical playback has never been
tested (their README says so in bold). Azure has asked me for the score and
you for the integration and the hardware run. Read `PHOSPHOR_MUSIC_BRIEF.md`
(Phase's handover to me), `README.md`, and then this.

## What I have put in the tree

- `pelagic/song.h`, `pelagic/song.c` — the score as tables: eighty bars, E
  major at 125 BPM, up a tone from bar 64. Same shape as COLOSSUS's song.c.
- `pelagic/synth.c` (replacing Phase's water bed), `pelagic/synth.h` — the
  COLOSSUS integer synth voiced for water. **One structural change you must
  know about:** the engine renders whole 48-frame blocks into a small ring
  and `synth_render()` hands frames out of it (the header comment in synth.c
  says why). So the device's one-or-two-frame calls between scanlines are a
  memcpy, and roughly one call in thirty renders a block. That one call is
  the spike to measure.
- `pelagic/CMakeLists.txt` — `song.c` added to the three targets.
- `pelagic/tools/song_harness.c`, `song_check.py`, `song_roll.py` — the
  score's own referees (block independence 1/8/1024, per-bar levels, silence
  at the end, the piano roll). `song_harness --hashes` prints the per-second
  FNV-1a latches (`H <pos> <hash>`), the same latch `synth_hash_latch()`
  exposes on the device.
- `build.ps1 check` passes from this tree: Phase's `pelagic_check` (4,609
  guarded frames, block independence 1/2/8/997, seek, silence after the
  endpoint, `-ftrapv`) and `texture_sampling`. Host peak 23,775 (−2.8 dBFS),
  RMS −18 dBFS overall, no DC.

Static RAM the synth adds: about 56.5 KB (delay 34.5 KB, reverb 16.8 KB,
chorus 2 KB, sine 2 KB, state). Phase reserved 64 KiB. Confirm from the map.

## 1. The board, normal build first

Build `pelagic_vga_rp2350.uf2` with the score, flash it (hold BOOTSEL while
connecting, or your COLOSSUS `flash` sequence), and read USB serial for the
whole 153.6 s. `main.c` prints once a second: fps, render mean and worst,
triangles, `audio_min` (the lowest number of frames the synth was ahead of
the DMA). I want:

- the per-section table (Phase's seven chapters: 0–7, 8–23, 24–39, 40–47,
  48–63, 64–71, 72–79 bars) of fps, worst frame, `audio_min`;
- **zero underruns**: `audio_min` never at 0, and the picture never breaking
  while the synth renders a block. If either happens, say so before fixing;
- the synth's cost on core 1: cycles per audio sample averaged, and the
  worst single `audio_pump()` in microseconds. Phase's budget is 15% of the
  core; COLOSSUS's synth was 1,776 cycles a sample with per-call overhead,
  and this one should be well under that because of the block ring;
- the audio hash: add the latch to the per-second print (`synth_hash_latch`
  in `synth.h`) and diff against `song_harness --hashes` — every second must
  match. Your COLOSSUS `serial_read.py` did this; reuse it.

Two traps from COLOSSUS that apply here as they did there: the stock panic
ends in a HardFault with USB masked (a `PICO_PANIC_FUNCTION` that keeps USB
alive saved us hours), and `__not_in_flash_func` on a function the compiler
inlines into an unmarked caller puts nothing in SRAM. `render_block` and
`synth_render` are `HOT`; check the map says they are in RAM.

## 2. The smooth build

Azure's target for the production is **60 fps**. Phase's `-Smooth` build
(bilinear environment and premultiplied-alpha ray, INTERP0 on core 0) is
untested on hardware and Phase's own host estimate is four times the render
cost in the close-up. Build `pelagic_smooth_vga_rp2350.uf2`, flash it, same
telemetry over the whole run, with the score playing.

Report, per section, the frame rate of both builds side by side, and give a
verdict: can the smooth build be the default at 60 fps? If it falls short,
say where (Phase names the close encounter, the abyss and the dissolves as
the suspects) and by how much, and whether there is a cheap, safe
optimisation. Do not rewrite Phase's renderer on your own: if you see a fix
worth more than a few lines, describe it and its cost, and the visual hash it
would or would not preserve, and stop there. The decision is Azure's.

## 3. The finish, if the normal build is clean

- The endcard (`typography()` in `render.c`, bars 72–79) credits Phase and
  the model. Add the music credit in the same style: a line `MUSIC  PHOSPHOR`
  under `CODE + DIRECTION  PHASE`, and the model line becomes
  `MODELS  GPT-6 ASTRA + CLAUDE FABLE 5.1`; shift the lines below by the
  same 17 px so the tagline still clears the bottom. Nothing else in the
  picture changes. `pelagic_check` must still pass.
- `tools/audit_release.py` still says "Temporary water bed; Phosphor score
  pending" and `hardware_tested: False`; make it tell the truth and
  regenerate `media/validation.json`. Add the numbers you measured to it.
- `build.ps1 capture` for a fresh `media/pelagic.mp4` with the score, and
  both UF2s at the folder root.
- The README's "Music is awaiting Phosphor" lines and the music paragraph
  are mine to write; leave the README alone except for the numbers table you
  can add under "On the board", which is yours.

## Rules

As always: measured, and labelled where (HOST or DEVICE). No commits. If the
board locks up, say what the last line printed was. Reply in
`briefs/2026-09-06-overscan-integration-reply.md`, with the serial logs in
`briefs/logs/`.

— Phosphor
