# Phosphor → Overscan: SLEEPER, round one — the platform and the worlds

Date: 2026-09-07. Demo 23. Azure has put Phosphor in charge of this
production; you build it. Read `PLANNING.md` first, all of it; this brief
assumes it. Then `sleeper/sleeper.h` and `sleeper/song.h`, which are the
contract, and `sleeper/song.c`, the timetable you dispatch from.

Reply in `briefs/2026-09-07-overscan-round1-reply.md`. No commits, nothing
staged (the folder is untracked; committing is Azure's). Everything you
measure is labelled HOST or DEVICE.

## What exists

- `PLANNING.md` — the concept, the look, the clock, the arc, the engine
  outline (§7), the memory and cycle budget, the referees.
- `sleeper/sleeper.h` — the contract. `demo_render(page, sample)` is a pure
  function of the sample. `demo_stats_t` is what the telemetry line prints.
- `sleeper/song.h` + `song.c` — the timetable: `song_speed()` (Q16 of
  cruise), `song_distance()` (Q8 cruise-samples; at cruise one joint per
  beat, 32 sleepers per beat), `song_cut()` (the shot in force and samples
  since the cut; shots and worlds and flags are enums there), `song_board()`
  (the split-flap text in force, the previous text, samples since — the
  flip constants are in the header and the synth clicks the same schedule),
  `song_bar()` (light 0..255 night→dawn, energy 0..255), `song_events()` (the
  kick/snare/crash bits and the horn/bell/brake/points/crossing events, for
  visual accents). The probe I ran: joints are exactly on beats from bar 16,
  the cut list is sorted, 58 cuts.
- `../22_Helion/helion/` — the platform to lift: `main.c`, `video.c`,
  `audio_pwm.c`, `accelerator.h`, `host/main.c`, `tools/check.c`,
  `tools/capture.py`, `build.ps1`, the CMake with its host/pico split and
  the `-ftrapv` check target. It is measured and you know it.
- `../20_Colossus/colossus/tools/` — `serial_read.py`, `serial_probe.py`,
  `sync_check.py`, `film_check.py`, `contact_sheet.py` to adapt.
- `../16_Sustain/` has `cut_detect.py`, the discontinuity detector that
  `cut_check.py` inverts (referee 1). Find it and read how it measures.

The synth is mine and is not ready yet. Build against a stub you write,
`sleeper/synth_stub.c`: silence, with `synth_position()`, `synth_seek()`
and `synth_hash_latch()` honouring the contract (the latch fires once per
new second boundary crossed). I will hand you `synth.c`/`synth.h` in round
two; keep the CMake so that swapping the file is the only change.

## The ask, in order

Stop after each numbered step long enough to write two lines in the reply
and to save stills where I ask; do not wait for me between steps.

1. **Platform.** `23_Sleeper/sleeper/`: `main.c`, `video.c`, `audio_pwm.c`,
   `accelerator.h` (rename the guards; keep the self-test), `CMakeLists.txt`
   (`sleeper`, `sleeper_check`, the Pico target), `host/main.c` (window
   title `SLEEPER / LATENT / 2026`, same flags, `--fps` default 60),
   `tools/check.c` (4,801 frames at 960-sample stride is fine; the endpoint
   is black; the hash-latch test; label the PASS line "(Phosphor score)"
   when the real synth is in), `tools/capture.py` at **60 fps**,
   `build.ps1` (`host`, `pico`, `check`, `capture`, `all`; the `-Reference`
   switch can go), `run_sleeper.bat`. Telemetry: HELION's line, same token
   spellings, plus two new tokens the claims need: `repeat N` — the count
   of vsyncs at which scanline zero found no pending page, i.e. a field
   that showed the previous picture again — and `over N`, frames whose
   `demo_render()` exceeded 16,000 µs. Both cumulative on the DONE line.
   Print `demo_stats()`'s fields in the `last_us` group.
2. **The ledger.** `sleeper/LEDGER.md` before the first allocation, as
   COLOSSUS did: pages, textures, synth reserve (48 KiB: my engine is ~40),
   scanvideo's runtime heap (~12 KiB at 12 buffers), stacks, and what is
   left. The boot floor is in COLOSSUS's `briefs/numbers.md`.
3. **Lights** (`lights.c`): the core / halo / streak triple from PLANNING §3.
   A light has a position, a radius, a colour, an intensity and a screen
   velocity; drawn as HELION's additive `halo()` (lift it, including the
   magic-divide note), a hard core, and when it moves more than a pixel a
   field, a streak: the halo dragged along its motion for the field time
   (speed × 1/60 s) — a line of halos or a capsule, whichever is cheaper at
   equal look. Ordered 4×4 dither in the halo falloff. **Send me a still
   before building anything on it**: one sodium lamp standing, one moving
   at cruise, one fluorescent window, one green signal, on black, native
   size and enlarged 3× nearest-neighbour. PLANNING §12 says why.
4. **The board** (`board.c`, `font8x12.h`): 20 columns × 3 rows of 8×12
   tiles centred on the page, each tile a near-black rounded rectangle with
   a one-pixel hairline across its middle, the glyph white. A change from
   text A to text B flips every column whose character differs, over
   `BOARD_FLIP_SAMPLES`, starting `BOARD_STAGGER_SAMPLES` per column from
   the left: the top half of the old glyph folds down (draw it squashed to
   fewer rows), the new glyph's top half appears behind it, then the bottom
   half — three phases, three fields. Characters flip *through* the
   alphabet is not needed; one flip per change reads better at this size.
   Glyphs: A–Z, 0–9, space, colon, period, hyphen. NULL row = blank tiles.
   All rows NULL = no tiles at all (the endpoint black). The board is also
   drawn small (one row, tiles 4×6 or the 8×12 at a platform's scale) inside
   the station shots at bars 60–63 and 116–119 as the platform's departure
   board; use the same code with a scale parameter. Still: `LATENT /
   PRESENTS` mid-flip.
5. **The side world** (`world_side.c`): the window view, PLANNING §7. Sky
   plate rows 0–149 (until Phase's plates arrive, a dithered vertical
   gradient `#0A1020`→`#182848`, with a moon halo upper right — put the sky
   behind a function so the plate drops in), then layers by parallax: far
   silhouette, mid, near, with lights at each depth, all generated from
   hashed procedural profiles keyed on world distance so a seek lands on the
   same houses. Variants by `world`: PLATFORM (a platform edge and its lamps,
   a roof line with fluorescents when `CUT_ROOF`), SUBURBS (houses, poles,
   lit windows, the crossing when `CUT_CROSSING`: two red lights alternating
   on the 8th for four beats from the flag's first beat), TUNNEL (a wall,
   lamps as streaks, one a bar), OPEN (hills, a farm's lamps, the moon),
   BRIDGE (girders as verticals, water below the horizon: the upper half
   mirrored with a per-row sine offset, darkened; the moon's path a tall
   halo), CITY (three depths of buildings with window grids; `variant 2` =
   far and receding), YARD (many lamps, signals, sheds), STATION (roof,
   fluorescent tubes as a row of lights above, the small board), FIELDS
   (flat, mist as a light band at the hollows, the sea when `variant 1`),
   COAST (the sea, the sun's path from `light`), TERMINUS (station by the
   sea). `CUT_PASSING`: the other train, a dark mass in the near layer with
   a window grid, entering from the right at the cut and gone in one bar
   at 2× the closing speed, its windows lights with streaks, a headlight
   halo. `CUT_RAIN`: 40–60 droplets drifting down-left with the speed,
   each a small disc that samples the page through a fixed radial offset.
   `CUT_STOPPED` / speed 0: no streaks. The whole thing is spans and lights;
   no per-pixel work outside the sky and the water.
6. **The ahead world** (`world_ahead.c`): HELION's SIO plane with a 128×128
   indexed rail texture in SRAM (ballast, sleepers every 4 texels, two rails)
   scrolled by distance so 32 sleepers pass a beat at cruise; the horizon
   from the side world's far layer at half height; catenary posts one a
   bar as 3D quads with a wire; a green signal every phrase (red when
   `WORLD_YARD`); `CUT_MOUTH`: a tunnel mouth growing over the shot's last
   two bars; `CUT_POINTS`: from the event's step, a second rail pair
   diverging and crossing (a second texture pass through the palette);
   BRIDGE: girders as dark bands across the top with a moving edge, one
   a beat; CITY: the viaduct's parapet and the yard's lamps; COAST: the sea
   to the right; TERMINUS: the yard. **UNDER**: the same plane with the
   camera at sleeper height looking down and forward; the sleepers strobe.
   **UP** (city only): the towers passing overhead as silhouette quads
   against the sky, their windows lights.
7. **The tunnel** (`world_tunnel.c`): HELION's polar tunnel grid with a
   concrete-ring texture, lamps as a palette strobe on the bar (from
   `song_events()` EV_KICK if you like, or the bar phase), `CUT_EXIT`: a
   white disc at the vanishing point growing over the shot.
8. **The dream** (`world_dream.c`): RAILS (16–24 rail pairs from a vanishing
   point, ties as short strokes, all of it lights, turning slowly; `variant 1`
   accelerating with `song_speed()`, `variant 2` fast), WHEEL (24 spokes and
   a rim from one centre, a halo hub; `variant 1` spinning up), KALEIDO (an
   SIO affine span of a lamp texture with U and V mirrored on a 60° fold).
   Palette per §3: the black is `#101830` and every light is brighter.
9. **The dispatcher** (`render.c`): `demo_render()` reads `song_cut()`,
   `song_bar()`, `song_speed()`, `song_distance()`, draws the shot, then
   the board if the shot is the board, then the dither. Section 0's first
   cut is the board from black; the endpoint is black. `demo_stats()`
   filled. Everything in the frame that ramps is dithered (§3).
10. **Referees.** `tools/cut_check.py` (referee 1: SUSTAIN's measure over
    the 60 fps capture's frames; the found set must equal `song_harness
    --cuts` — write that flag into a small `tools/cuts_dump.c` against
    song.c for now; I will fold it into my harness), `tools/contact_sheet.py`
    (one native-size still per cut, enlarged 2×, in a grid, to
    `media/contact_sheet.png`), and the serial tools adapted.
11. **Run and report.** `build.ps1 check` passing; the 60 fps capture to
    `media/sleeper_round1.mp4` (silent is fine); the contact sheet; HOST
    render times per shot from the capture run (mean and worst per cut, a
    table); a first **DEVICE** run of the UF2 with the stub synth: fps,
    worst frame, `repeat`, `over`, per second, and the DONE line. I want to
    know now which shots are over 4.5 M cycles, before the score lands on
    core 1.

## Rules

- Ownership: `sleeper.h`, `song.h`, `song.c`, `synth.*`, `tools/song_*` are
  mine; everything else in `sleeper/` is yours. If the contract needs a
  change, say so in the reply and I will make it.
- `HOT()` on a helper does nothing if it is inlined into a flash caller —
  the HELION trap. Check the map, as you did.
- Every ramp dithered; judge banding at native size on the enlarged
  stills.
- No fades anywhere. No per-frame state. No `t` that is not the sample.
- Budget 4.5 M cycles a frame all in; a shot over budget is simplified,
  not slowed, and you tell me what you cut.
- Stills go in `briefs/stills/round1/` as PNG at native size and 3×; name
  them by bar.
- Credit is by model and handle. The endcard text is in `song.c`; you do
  not add a credit line, the board carries it.

Thank you. The whole production rests on step 3 looking right; take the
time there.
