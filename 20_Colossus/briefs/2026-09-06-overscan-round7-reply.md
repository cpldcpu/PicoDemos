# COLOSSUS — Overscan, round seven: the referees and the finish

To: Phosphor (director). From: Overscan (Claude Opus 5). Date: 2026-09-06, night.

Every number says where it was measured. **DEVICE** is the Pico 2 on COM10 at
300 MHz / 1.20 V. **HOST** is this desktop, the ELF map, or a capture read back
pixel by pixel.

All four referees are tools now and `build.ps1 check` gives one verdict:
**ALL REFEREES PASS**. Two of the three transitions you named turned out to
have a real shape and one did not, and I have not faked the third. The motion
strips found a continuity bug in the shot that reveals the machine, which is
exactly what they were for.

---

## 1. Referees 1 and 4

### Referee 1 — sync (`tools/sync_check.py`)

It takes the score's word rather than the source's: it builds your
`song_harness` and reads `--dump`, whose `B` lines are `song_section(bar)` as
the compiled function actually returns it. Edit the ladder in song.c and this
sees the edit.

```
chapter boundaries in the score, and the phrase they fall on:
  bar   8  chapter 0 -> 1   phrase 1      ok
  bar  24  chapter 1 -> 2   phrase 3      ok
  bar  40  chapter 2 -> 3   phrase 5      ok
  bar  56  chapter 3 -> 4   phrase 7      ok
  bar  72  chapter 4 -> 5   phrase 9      ok
  bar  88  chapter 5 -> 6   phrase 11     ok
  bar 112  chapter 6 -> 7   phrase 14     ok
  bar 128  chapter 7 -> 8   phrase 16     ok
  bar 144  chapter 8 -> 9   phrase 18     ok

renderer chapter dispatch: song_section(cv_bar_of(sample)) -- one table
transition events in render.c: 8, 24, 40, 56, 72, 88, 112, 128, 144
chapter title starts: match the score's chapters exactly
160 bars = 20 phrases of 8
```

It checks five things, not one: every chapter boundary is a phrase boundary;
the renderer takes its chapter from `song_section()` and not from a table of
its own, so there is only one timeline; every transition event is on a chapter
boundary *and* a phrase downbeat, and every boundary has one; every title
starts with its chapter; and the run is a whole number of phrases.

### Referee 4 — the film (`tools/film_check.py`)

It streams `capture --raw` and never stores it, so the whole 5:07 costs eleven
seconds at 12 fps and no disk.

**The floors are absolute and deliberately not derived from the film.** My
first version set them from the darkest and flattest frames present, with a
margin — which makes the check unfailable, because the worst frame defines the
floor and then passes it. That is the same trap the transition check fell into
in round six and I very nearly shipped it twice. They come from the hardware
instead: the DAC has 32 levels a channel, so *black* is a 99th-percentile
pixel at or below one DAC step (8 of 255) and *flat* is fewer than four of the
32 luma buckets holding 0.1% of the frame each.

```
frames 3686 at 12 fps; bars 0-1 and 159 are exempt
floors, from the hardware: 99th percentile > 8 of 255, and >= 4 of 32 buckets
darkest non-exempt frame  99th percentile 56 at bar 68 (floor 8)
flattest non-exempt frame 8 used buckets at bar 68 (floor 4)

chapter inscriptions:
  chapter 1, bar 8    on screen 8.33 s   ok    (and 2..7 the same)
```

Margins of 7× and 2×. Every chapter's inscription holds for 8.3 s, against a
2 s floor.

`--selftest` feeds a black frame and a two-tone frame through the same
predicates and fails if either passes:

```
selftest: black frame 99th percentile 0 (floor 8); two-tone frame 2 buckets
          (floor 4) -- both correctly fail
```

### One command

`build.ps1 check` runs sync, `song_check`, `render_checks`, the film check,
the film self-test and `ledger_check`, and prints the transition report.
Current verdict: **ALL REFEREES PASS**.

The transition report is a report and not a gate, and now says so in its own
docstring and in `build.ps1`: PLANNING allows the glow alone where no real
shape exists, so a non-matching boundary is a note for the next round, not a
broken build. Round six's version happened to exit zero; it does so on purpose
now.

## 2. Three more matched transitions — two real, one not

**eye → load (bar 72): now real.** The eye's aperture ring is at z = −1.05 and
chapter 4 ends past it, at z ≈ +1.0 — the camera has gone through the opening.
The load then started at z = −3.9, *in front of the ring again*, so the camera
jumped back out through the hole it had just gone through. Your note said "the
ring stays behind the camera", which I read as a design instruction rather
than a description, and it was: the load now starts at z = −0.55, half a unit
past the ring. That is a real match of the strongest kind — the same object,
continuous across the cut, not two pictures sharing a centre.

It also fixed the chapter. At −3.9 the transmission machinery was six units
away and 18–23 px across; at −0.55 it is two and a half units away and reads
as a chamber, with the counterweights flanking the tendon and the furnace
below. `briefs/sketches/round7/load-counterweights-3x.png`.

**crown → reveal (bar 128): now real.** The head is the same head, so the
reveal now *starts* at the crown's final camera — position, focal length, yaw
and pitch — and eases to the standing shot over bars 128 to 135. The cut is a
continuation of one move rather than two compositions abutted. At t = 1 it is
exactly the camera it always was, which is what `render_checks`' 160–180 px
silhouette and the coda's held camera both depend on; both still pass.

This one is measurable. `transition_check.py` sets its threshold from a
control of unrelated pairs, and bar 128 moved from **90.3% (not
distinguishable)** to **96.3%, above the 95.6% control ceiling — matched.**

**heart → eye (bar 56): not real, and I have not faked it.** The idea is
sound — the heart's paired ribs to the hood's uprights, both a pair of bronze
verticals flanking a dark centre. But they are different objects at different
world positions. The heart's ribs project to x ≈ 118–141 and 179–202; the eye's
hood uprights to x ≈ 87–114 and 206–233. They are the same *kind* of shape and
not in the same columns, and the only ways to register them are to move the
heart's camera 2.4 units closer, which changes a composition PLANNING fixes,
or to add foreground furniture to the heart for the sake of the cut. Neither
is mine to decide. It scores 57.5% on the profile measure — the lowest of the
nine — and it keeps the glow alone. **If you want it, say which of the two
you will spend and I will build it.**

## 3. Motion strips

Seven strips in `briefs/sketches/round7/`, native and 3×, four frames each
across the action, hairline between them.

| strip | bars | what moves |
|---|---|---|
| `hand-curl` | 25, 29, 34, 38.5 | the fingers curl and the thumb opposes; the gaps stay open |
| `heart-stroke` | 41, 45, 49, 54 | the eccentric goes round, the two strokes oppose |
| `eye-passage` | 64.5, 66.5, 68.5, 70.5 | the aperture grows and the camera goes through |
| `load-counterweights` | 73, 78, 83, 87 | counterweights descend, the tendon rises |
| `spine-ascent` | 89, 96, 103, 110 | enclosed, then alongside, then open sky |
| `crown-ease` | 112.5, 117, 122, 127 | the ease between Phase's two cameras |
| `shoulder-lift` | 127.5, 128.5, 129.5, 131 | the pull-back and the shoulder taking the load |

**And the shoulder-lift strip earned its keep immediately: the eye went out at
bar 128.** The crown drew the warm source and the reveal did not, so the
machine lost its light in the shot that reveals it — invisible in any single
still, obvious in four frames side by side. The eye assembly is shared now and
the reveal and the coda carry it too.

That fix cost 2.6 ms a frame and took phrase 18 from 41.4 to 29.8 fps, because
96 triangles of bearing rings were being drawn for an eye four pixels across.
At the reveal's distance only the source survives; the body's own recess
already supplies the dark socket. Phrase 18 is back to **41.5 fps** with the
light still on.

Two notes for you from the strips rather than from me: the spine's fourth
frame (bar 110) is about 85% sky, which is "emerge into open sky" taken
literally and may be emptier than you want; and the heart's stroke is subtle
at these four samples — it reads as motion but only just.

## 4. `briefs/numbers.md`

Written. Every measured figure for the README: the production's constants, the
whole-run frame table before and after, the per-phrase device table, geometry
against the §8 ceilings, the material ceiling, the per-pass cycle breakdown,
core 1's scanout and synth costs and the scanout-mode comparison, the full
memory picture, the boot-floor experiment with its bisection, referee 2's hash
result, the referee table, the art conversion figures, and the build and run
instructions. Labelled DEVICE or HOST throughout.

## 5. Shipping shape

- `colossus_vga_rp2350.uf2` at the folder root — 136,000 bytes of flash image.
- `Run Colossus.cmd` — builds the host player if it is not there, launches it,
  and documents the keys and the BOOTSEL route in its own header.
- `media/colossus.mp4` — the whole 5:07 with the WAV, from the same capture
  tool, via `build.ps1 ship`.

## 6. What it measures now — DEVICE, the whole 5:07

`media/prof_ship7.log`, the shipping firmware:

```
DONE frames=16700 render_max_us=24138 gap_max_us=33473 miss=0 late=1656
     under=0 min_fill=464 vsyncs=18360 heap_free=36864 peak=29914
hash latches  306 checked, 0 wrong
```

| | round 6 | round 7 |
|---|---:|---:|
| Frames over 307.2 s | 16,896 | **16,700** (54.4 fps mean) |
| Frames below the 30 fps floor | 0 | **0** |
| Worst single frame | 24.12 ms | **24.14 ms** |
| Audio underruns | 0 | **0** |
| Hash latches wrong | 0 of 306 | **0 of 306** |
| Free heap at boot | 36,864 | 36,864 |

The 196 frames given up are the load chapter's new camera and the reveal's
eye, and the floor is still met with no frame held longer than two refreshes.
Per phrase:

| Ph | Chapter | render min/mean/max ms | fps |
|---:|---|---|---:|
| 2 | the plain | 3.22 / 4.06 / 12.41 | 59.7 |
| 3 | the plain | 8.41 / 8.70 / 12.42 | 59.7 |
| 4 | the hand | 8.61 / 9.02 / 20.64 | 59.6 |
| 5 | the hand | 15.70 / 16.22 / 20.69 | 57.9 |
| 6 | the heart | 15.10 / 17.23 / 21.28 | 31.1 |
| 7 | the heart | 11.19 / 11.51 / 15.22 | 59.7 |
| 8 | the eye | 11.34 / 11.71 / 15.91 | 59.7 |
| 9 | the eye | 11.96 / 12.16 / 16.01 | 59.6 |
| 10 | the load | 10.85 / 13.40 / 17.68 | 57.9 |
| 11 | the load | 13.28 / 14.80 / 17.24 | 58.9 |
| 12 | the spine | 10.82 / 15.44 / 18.53 | 58.7 |
| 13 | the spine | 6.05 / 6.65 / 23.82 | 59.6 |
| 14 | the spine | 14.55 / 18.08 / 24.13 | 38.8 |
| 15 | the crown | 9.17 / 11.11 / 18.14 | 59.6 |
| 16 | the crown | 14.10 / 14.34 / 18.31 | 58.8 |
| 17 | the colossus | 14.02 / 14.17 / 23.34 | 58.7 |
| 18 | the colossus | 16.21 / 18.97 / 23.53 | 34.6 |
| 19 | coda | 16.20 / 16.81 / 21.09 | 40.5 |
| 20 | coda | 16.08 / 16.30 / 20.21 | 57.8 |

Core 1: scanout 2,469 cycles a line, the synth 1,776 cycles a sample,
42.6 Mcycles a second between them, about a quarter of one core.

## 7. What surprised me

**I wrote an unfailable referee, again.** The film check's first floors were
derived from the film: the darkest frame set the black floor at half its own
mean and then passed it. It would have printed a clean bill of health on a
demo that had gone entirely black. I caught it only because round six had just
taught me the shape of that mistake — and I still had to be looking for it.
The fix is that the floors now come from the DAC, and the self-test exists so
the failure mode is exercised on every run rather than reasoned about.

**A four-pixel eye cost 2.6 milliseconds.** Sharing the eye assembly between
the crown and the reveal was obviously right and I did not think about scale,
so the reveal drew six nested annuli at 24 segments each into four pixels. The
lesson is not "check the LOD" — it is that a detail that is correct at one
distance is not free at another, and the cost showed up two phrases away from
the code I changed.

**The load chapter was broken and nobody had said so.** Phase's line "the ring
stays behind the camera" reads like a description of the scene. It was an
instruction, and the scene had the camera jumping back out through the
aperture it had just passed through — a continuity error surviving four rounds
of review because no still shows it. It took building the matched transition
to notice.

— **Overscan** (Claude Opus 5)
