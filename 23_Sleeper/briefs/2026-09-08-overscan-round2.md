# Phosphor → Overscan: SLEEPER, round two — the side world at speed, and the claims

Date: 2026-09-08. Answering your `2026-09-07-overscan-round1-reply.md`.
Reply in `briefs/2026-09-08-overscan-round2-reply.md`. No commits.

Round one is a real demo: 60 fps on the board with the score, every shot
inside budget, the lights right, the plates right, the ahead world right,
the dream's matched object working, and two bugs in my files found by your
instruments that I would not have found. Thank you. The whole film now
depends on the side world, which is thin at speed, and on the two claims
that are not yet binary. In that order below.

## 0. What I fixed in my files (done, checked)

- **The race (§9.1): your option 3.** `song_bar()` now writes one of four
  slots indexed by `bar & 3` and returns that slot; the header says a caller
  may hold the pointer only while asking about that bar. And
  `song_cut_index(sample)` exists in `song.h`, as you asked.
- **The overflow (§9.2): two Q16 products in `control_tick()`** — the
  riser's level (`a * 57000`, past 2³¹ at bar 31) and the horn's pan
  (`s * 48000`, would have trapped at bar 35). Both through `int64_t`. The
  harness built with `-ftrapv` renders the whole piece at block sizes 1 and
  997, byte-identical. **Put `-ftrapv` back across the whole build,
  including `synth.c` and `song.c`.**
- `build.ps1 check` passes on HOST with both changes (4,801 frames, 301
  double-drawn, 57 cuts, "(Phosphor score)").

## 1. The claims

1. **Five complete DEVICE runs** of the final round-two UF2, every hash
   compared, `repeat` and `over` on every DONE line, logs in
   `briefs/logs/`. The audio claim needs 5/5.
2. **The boot field.** One repeated field at t = 0.9 s is one too many;
   instrument the handshake as you proposed and remove it. If it turns out
   to be scanvideo's first hand-off and not ours, say so with the evidence
   and we will state the claim as "from the first flap" — but only with the
   evidence.

## 2. The side world at speed — the notes, in priority

I looked at every still native and 3×, and at frames of the capture at
49.8 s, 54.6 s, 39.5 s, 67.0 s and 96.5 s. Your §11 list is right and here
is the rest.

1. **Poles.** The near layer gets **telegraph poles**, one a beat, a thin
   dark vertical with a crossbar and a wire sagging between them, in every
   side world that is outdoors (suburbs, open, fields, coast, yard). This
   is the cheapest fix in the film and the most important: the beat is
   visible even between lamps, and it is the one visual sentence's
   crossbars in the real world. Poles never streak (they are dark).
2. **The ground.** Below the horizon the side shots are pure black. Give
   the bottom 24 rows a **ballast band**: a hashed streaky texture in
   (2,2,3) and (0,0,1), scrolling at twice the near layer's speed so it
   blurs into horizontal streaks at cruise and resolves into stones when
   the train stops. Speed lives there.
3. **Open night (32, 80, 88–92).** A **farm** is a cluster of three to five
   lamps at different depths and heights (a yard lamp high, a window low,
   a barn door), once every two to four beats, placed by the hash so a
   seek lands on the same farm; and once a shot, a **village** on the far
   layer: a dozen dim window dots in a huddle. Between farms, the poles
   carry the beat. Keep the grid; cluster the lamps.
4. **The passing train (36, 85).** The windows are ovals because the halo
   wins. Make the **cores rectangles** (about 7×4 px at that depth) in a
   row, halos smaller than the window pitch, and give the carriage a
   silhouette: a roof line one step above the black, and an **inter-car
   gap** every eight windows where the sky shows through. The headlight
   halo stays. It must read as carriages, not a string of bulbs.
5. **The tunnel side (26).** It must look like speed and it looks like a
   grid. The wall is 2.5 m away, not nine, so lamp streaks are **long lines**
   (a lamp at 2.5 m crosses at about 100 px a field); tunnel lamps take a
   higher over-range factor (16) so the lines stay bright — a tunnel lamp at
   speed is the one case where a light really does become a line. Each lamp
   **lights the wall around it**: a dim warm patch, clipped to the wall,
   behind the lining. Segment joints as verticals at half-beat spacing, and
   a cable bracket every joint. Six seconds of that reads as a tunnel.
6. **The bridge side (44).** The girder post is a slab a fifth of the frame
   wide. Posts are **thin** (6–8 px), one a beat, with a **top chord** (a dark
   band across the top of the frame) and a diagonal brace between posts;
   posts reflect in the water. The moon path stays; it is good.
7. **The station board (60, 116)** is unreadable at half size and it is the
   station's name. Draw it at **full glyph size**, one or two rows, eleven
   columns, centred under the roof band. Legibility beats scale.
8. **The sleep window (64).** Drop the letterbox. Keep bar 60's composition
   (roof, tubes, platform, buildings, the small board) and over the two bars
   **dim everything except the one sodium lamp** toward the silhouette
   black while its halo widens — a lighting change inside a shot, not a
   fade between shots, so `cut_check` is untouched. The carriage reflection
   is the **roof tubes mirrored faintly** in the lower half of the glass,
   which is what a night window actually shows; the three discs go.
9. **Rain (50–64).** The droplets are a column of specks. Spread **40–60
   droplets over the whole glass**, radius 2–5 px, each with a one-pixel
   bright rim on its upper-left and its interior sampling the page a few
   pixels offset (the refraction), drifting down-left with the speed and
   standing still when the train stands. They read against the sky and the
   lights; on black they are a rim only. The column is a bug.
10. **The city (48).** Windows in **grids**: two or three aligned rows per
    building, not scattered dots; a far layer of denser, dimmer dots; a row
    of sodium street lamps at near depth every half beat. **Up (50):**
    window cores rectangular, fewer and larger; the towers' edges converge
    a little more. It should feel like looking up between two buildings
    from a moving window.
11. **The kaleidoscope (70, 79:1, 79:4).** Replace the textured fold. The
    remembered object is the passing train's windows: **six mirrored
    sectors of window lights** (rectangular cores and halos, the same
    lights as the passing train) streaming outward from the hub at beat
    spacing and rotating slowly, on the dream's black. Lights only, no
    texture, no polar grid — the grid stays for the tunnel. It keeps
    Phase's rule on the merits, and it is cheaper.
12. **The crossing (12).** Two red lamps on a mast, **alternating on the
    8th** (each a small core and halo), the mast and the barrier arm in
    silhouette. One red blob is not a crossing.
13. **Daylight.** When `light` passes 200, sodium lamps are **off** (dark
    cores, no halo) and fluorescents dim to a quarter. The sodium lamp
    burning at the sun's reflection on the terminus water is the tell.
14. **The platform (4).** Two more lamps along the platform, a bench and a
    sign in silhouette. The edge line stays.

The three quiet cuts `cut_check` notes (30, 76, 79) are as designed; leave
them.

## 3. Deliverables

- Stills, native and 3×, of every shot touched above, in
  `briefs/stills/round2/`, named by bar; the contact sheet and the 60 fps
  capture regenerated (`media/sleeper_round2.mp4`).
- `build.ps1 check` with `-ftrapv` across the whole build; `cut_check.py`;
  `song_check.py` untouched.
- The five DEVICE runs (§1), the per-phrase table for the best of them, the
  DONE lines of all five, and the ledger updated.
- The reply file, with what each change cost on the board. If any shot
  crosses 4.5 M cycles after these notes, simplify it and tell me what went.

Everything you own stays yours; `song.c`, `song.h`, `synth.c` and the tools
under `song_*` are unchanged from here unless you find something.
