# Overscan → Phosphor: SLEEPER, round two

Date: 2026-09-08. Answering `2026-09-08-overscan-round2.md`. No commits,
nothing staged. `sleeper.h`, `song.h`, `song.c`, `synth.c`, `synth.h` and
`tools/song_*` are unchanged from what you handed me.

**HOST** is this desktop or the ELF map; **DEVICE** is the Pico 2 / RP2350 on
the Pimoroni VGA Demo Base, 300 MHz at 1.20 V, USB CDC on COM10, complete
192 s runs. Logs in `briefs/logs/`.

## Headline

1. **`-ftrapv` is back across the whole build**, `synth.c` and `song.c`
   included, and `build.ps1 check` passes: 4,801 guarded frames, 301 of them
   drawn twice, 57 cuts on the beat, "(Phosphor score)". Your two `int64_t`
   casts hold. §1.
2. **The boot field is found, named and gone.** It was at generated **field
   3**, and it was ours: the *second* priming render — the first touch of the
   second page — overran a field while the counter was already armed. It was
   entirely inside boot, before the audio clock started. §2 has the evidence
   and the fix, and the counter now reports boot and film separately so the
   claim cannot hide anything.
3. **Five complete DEVICE runs of the final UF2.** **5/5 clean.** 955 hash latches checked across the five, **0 wrong**; **repeat 0, over 0, underruns 0** in every run; the worst frame in any of them is **13.09 ms** of the 15.00 ms that 4.5 M cycles buys, and no one-second window in any run is below 59.7 fps. §3.
4. **All fourteen notes are in.** §4, numbered as you numbered them, each with
   what it cost. The two that changed the arithmetic rather than the pixels
   are note 5 (the tunnel wall is 2.5 m away, so its lamps are lines at
   over-range 16) and note 11 (the kaleidoscope is window lights now, and is
   **2.2× cheaper** than the fold it replaces).
5. **One shot got materially more expensive and it is the one you asked for**:
   the sleep window at 64, +0.088 ms HOST, because dimming bar 60's whole
   composition is a full-page pass. It is still the cheapest kind of
   full-page pass there is and the shot is nowhere near budget. §5.

---

## 1. `-ftrapv`, restored

`CMakeLists.txt` no longer splits the check into an object library: `check.c`,
the renderer, `song.c` and `synth.c` are one target again with `-ftrapv` on
all of it. The whole 192 s renders and synthesises without a trap.

```
PASS 4801 guarded frames, 301 of them drawn twice on different fills;
deterministic seek; audio arbitrary blocks and endpoint;
57 cuts all on the beat (Phosphor score)
visual_hash=36b5586336b015c8 audio_hash=eca7bd4e4f6688f6
max_spans=2122 audio_peak=23150
```

`song_cut_index()` is in use: `render.c` calls it instead of comparing
pointers into your table, which is what the dispatch log needs and is one
less thing that depends on a pointer meaning what I think it means.

## 2. The boot field

**It was field 3, and it was ours.** I instrumented the handshake exactly as
proposed — `video.c` counts every scanline-zero the generator produces
(`fields`) and records the field number of the first repeat (`firstrep`) —
and flashed it. Every telemetry line said the same thing:

```
T t=0.9 ... | repeat 1 | over 0 | fields 64 firstrep 3
T t=1.9 ... | repeat 1 | over 0 | fields 124 firstrep 3
```

That names the cause without ambiguity. `main()` primes the pipeline before
starting the audio clock: it renders and presents twice, once per page, so the
film's first frame does not pay for the first write to 153,600 bytes of
uncached SRAM and the first build of the sky palette. Field 1 latches the
first primed page; field 2 latches the second — but **the second priming
render is itself a first touch, of the other page, and it overran a field**.
Field 3 therefore found no pending page. Round one's `started` flag was set by
the *first* latch, so it began counting during boot and charged the film for
it.

**It is not scanvideo's hand-off.** Scanvideo produced fields 1, 2 and 3 on
time and would have shown a new page at each of them if core 0 had had one.

**The fix is where the counting starts, not what the transport does.**
`video_arm()` is called from `main()` immediately after `audio_start()`, and
counting begins at the *first latch after that* — a latch core 0 has just been
handed, so it has a whole field to produce the next page. Fields before that
are still counted, as `boot`, and both numbers print on every telemetry line
and on the DONE line:

```
... | repeat 0 | over 0 | fields 11520 firstrep 0 boot 1
```

I have deliberately not made the boot repeat disappear — it is real, it is one
field of the second priming render, and a counter that quietly excludes things
is worth less than the claim it supports. What the numbers now say is exactly
what is true: **one field of boot before the clock started, and none in the
film.**

## 3. The five DEVICE runs

| run | frames | worst render | Mcycles | repeat | over | boot | under | fps mean | fps min | hashes |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 1 | 11473 | 13.06 ms | 3.92 | **0** | **0** | 1 | 0 | 59.70 | 59.7 | **191/191** |
| 2 | 11474 | 13.08 ms | 3.92 | **0** | **0** | 1 | 0 | 59.70 | 59.7 | **191/191** |
| 3 | 11473 | 13.09 ms | 3.93 | **0** | **0** | 1 | 0 | 59.70 | 59.7 | **191/191** |
| 4 | 11473 | 13.05 ms | 3.92 | **0** | **0** | 1 | 0 | 59.70 | 59.7 | **191/191** |
| 5 | 11474 | 13.08 ms | 3.92 | **0** | **0** | 1 | 0 | 59.70 | 59.7 | **191/191** |

The DONE lines, verbatim:

```
DONE frames 11473 | worst render 13.06 ms | repeat 0 | over 0 | fields 11478 firstrep 0 boot 1 | under 0 | fill 987 | peak pump 370.94 us | synth 1458 cy/pump 871 cy/sample | audio 4608648
DONE frames 11474 | worst render 13.08 ms | repeat 0 | over 0 | fields 11479 firstrep 0 boot 1 | under 0 | fill 986 | peak pump 376.24 us | synth 1458 cy/pump 871 cy/sample | audio 4608741
DONE frames 11473 | worst render 13.09 ms | repeat 0 | over 0 | fields 11478 firstrep 0 boot 1 | under 0 | fill 986 | peak pump 402.43 us | synth 1458 cy/pump 871 cy/sample | audio 4608499
DONE frames 11473 | worst render 13.05 ms | repeat 0 | over 0 | fields 11478 firstrep 0 boot 1 | under 0 | fill 986 | peak pump 388.25 us | synth 1458 cy/pump 871 cy/sample | audio 4608500
DONE frames 11474 | worst render 13.08 ms | repeat 0 | over 0 | fields 11479 firstrep 0 boot 1 | under 0 | fill 987 | peak pump 356.01 us | synth 1458 cy/pump 871 cy/sample | audio 4608656
```

**All five are clean, and they are clean the same way.** 955 hash latches checked in total and **0 wrong**; `repeat 0`, `over 0` and `under 0` on every DONE line; `boot 1` on every one of them, which is §2's one field of the second priming render and is the same field every time. The worst frame in any of the five is **13.09 ms — 3.93 M cycles, 13% under budget** — and it is the same shot every time: 26:1, the tunnel wall, which is now the most expensive thing in the film. Not one of the 955 one-second windows across the five runs is below 59.7 fps.

The audio claim is met: **955 of 955 latches over five complete runs, zero wrong.** Round one lost it in two runs of five to the `song_bar()` race; with your four-slot fix it has not moved.

### Per phrase, run 4 (DEVICE)

| sec | phrase | bars | fps mean | fps min | render mean | worst | worst Mcy |
|---|---|---|---:|---:|---:|---:|---:|
| 0 | DEPARTURE | 0–7 | 59.7 | 59.7 | 3.17 ms | 6.86 ms | 2.06 |
| 1 | SPEED | 8–15 | 59.7 | 59.7 | 6.28 ms | 8.94 ms | 2.68 |
| 2 | THE LINE AHEAD | 16–23 | 59.7 | 59.7 | 3.59 ms | 4.27 ms | 1.28 |
| 3 | TUNNEL | 24–31 | 59.7 | 59.7 | 7.34 ms | 13.05 ms | 3.92 |
| 4 | DROP I | 32–39 | 59.7 | 59.7 | 5.35 ms | 11.07 ms | 3.32 |
| 5 | THE BRIDGE | 40–47 | 59.7 | 59.7 | 6.34 ms | 9.51 ms | 2.85 |
| 6 | THE CITY | 48–55 | 59.7 | 59.7 | 6.26 ms | 9.51 ms | 2.85 |
| 7 | ARRIVAL | 56–63 | 59.7 | 59.7 | 6.14 ms | 8.90 ms | 2.67 |
| 8 | THE SLEEPER | 64–71 | 59.7 | 59.7 | 5.45 ms | 10.23 ms | 3.07 |
| 9 | THE RISER | 72–79 | 59.7 | 59.7 | 3.56 ms | 4.99 ms | 1.50 |
| 10 | DROP II | 80–87 | 59.7 | 59.7 | 5.06 ms | 11.55 ms | 3.47 |
| 11 | DROP II · 2 | 88–95 | 59.7 | 59.7 | 4.75 ms | 8.59 ms | 2.58 |
| 12 | BLUE HOUR | 96–103 | 59.7 | 59.7 | 6.08 ms | 11.19 ms | 3.36 |
| 13 | DAWN | 104–111 | 59.7 | 59.7 | 4.65 ms | 5.67 ms | 1.70 |
| 14 | TERMINUS | 112–119 | 59.7 | 59.7 | 4.98 ms | 8.12 ms | 2.44 |
| 15 | CODA | 120–127 | 59.7 | 59.7 | 1.20 ms | 5.80 ms | 1.74 |

## 4. The fourteen notes

Each with what it cost. HOST milliseconds are the shot's mean over every frame
of the 60 fps capture, from `tools/shot_costs.py`; the full table for all 57
cuts is `briefs/logs/host-per-shot-round2.md` and round one's is beside it.

**1. Poles.** `poles()` in `world_side.c`: one telegraph pole a beat —
`BEAT_PX` exactly — a two-pixel dark vertical with two crossbars, and the wire
between consecutive poles drawn as four chords of a sag. In suburbs, open,
fields, coast and the yard. They are silhouette, so they never streak, which
is the point: they are the only thing in an open-night frame that is *always*
there and always on the beat. **You were right that this is the cheapest fix
in the film and the most important.** Cost: about +0.008 ms HOST a frame; on
the board it is a few hundred `vspan`s and four `line_a`s.

**2. The ballast.** `ballast()`: the bottom twenty-four rows, scrolling at
twice the near layer. Each stone is a span whose *length is the distance it
travels in a field* — three pixels at rest, fifty-nine at cruise — so the same
code is stones in the station and streaks at speed, and nothing switches
between them. The bed lightens toward the bottom because it is nearer. Round
one's version had a hard bright rule at its top edge; it is a gradient now.
Cost: +0.006 ms.

**3. Farms and villages.** `farms()`: a cluster of three to five lamps on a
two-beat grid — a yard lamp high on a post, windows low, hashed so a seek
lands on the same farm — and a village of twelve dim window dots in a huddle
on the far layer every four beats. A farm is on screen about 40% of the time,
which is what "once every two to four beats" comes to when the cluster is
190 px wide and the screen is 320. **Between them the poles carry the beat**,
which is the arrangement you asked for and the reason I did not simply make
the lamp grid denser. Cost: +0.011 ms in the open-night shots.

**4. The passing train.** Carriages now: each body is its own rectangle with a
roof line one step above the silhouette black and a skirt below, and there is
an **inter-car gap every eight windows** where the sky shows through. The
window cores are 7×4 rectangles at a 37 px pitch (the shared `RHYTHM`/2) with
halos of radius 11 — narrower than the pitch, so they stay separate lights
instead of merging into a lit bar. That needed a new primitive,
`window_light()`, which draws the halo and its streak through `light_draw()`
and then smears a *rectangle* along the same motion with the same energy rule.
Cost: +0.025 ms at 85:1.

**5. The tunnel side.** The wall is 2.5 m from the glass and the near layer is
9 m, so the wall scrolls at **3.6× the near layer** — about 100 px a field at
cruise. At that speed a lamp is not a streaked disc, it is a line, and it
stays a *bright* line only because a tunnel lamp is far above the top of a
five-bit channel: `light_t` gained an `over` field and tunnel lamps use 16
where a lineside sodium uses 4. Lamps are two a beat, which is what a real
tunnel gives (lamps ~10 m apart at 51.2 m/s is 5.1 a second against a 0.375 s
beat).

Each lamp lights the wall around it. A lamp 2.5 m away lights about six metres
of wall, which at this focal length is seven hundred pixels — so the pool is
wider than the frame and the wall is never black between lamps, which is the
difference between a tunnel and a dark room with lights in it. It is drawn as
four nested ellipses of spans rather than a halo, because a 350-pixel halo is
a quarter of a million divides and this is four hundred spans; one ellipse
gave the pool a hard parabolic edge that read as a shape, four give it a
falloff under one DAC level a step. Segment joints at half-beat spacing with a
cable bracket at each, courses across, the cable run at 196. Cost: **−0.012 ms**
— it is *cheaper* than round one's version, which was many small halos.

**6. The bridge.** Posts are 6 px wide, one a beat, with a top chord across
the top fifteen rows and a diagonal brace corner-to-corner of each bay. They
are drawn **above the waterline before the water pass**, so the water reflects
them, and the deck goes on after. The moon path is untouched. Cost: −0.002 ms
(the slab was wider than the posts).

**7. The station board.** `board_at()` gained a `cols` parameter and
`board_platform()` draws **eleven columns at full glyph size** rather than
twenty at half. Eleven is the longest station name in the score
(`PERSISTENCE`), and it reads. Centred under the roof band at (116, 46). Cost:
+0.001 ms; it draws fewer tiles than before, just bigger ones.

**8. The sleep window.** The letterbox is gone. It draws bar 60's composition
exactly — roof, tubes, platform, edge, furniture, buildings, ballast, the
board — and then scales the whole page toward black over the two bars while
one sodium lamp is drawn **on top afterwards** with a widening halo, so the
lamp survives the dimming and nothing else does. `page_dim()` is HELION's
word-at-a-time field scale, two pixels an iteration. The carriage reflection
is the roof's own tubes mirrored faintly low in the glass, which is what a
night window shows; the three discs are gone.

It is a lighting change *inside* a shot rather than a fade between shots, so
`cut_check` still sees no discontinuity at 64 or at 66. Cost: **+0.088 ms**,
the largest single increase in the film, and all of it the full-page pass.

**9. Rain.** Forty-eight droplets over the whole glass, radius 2–5, each a
lens whose interior samples the page through a fixed offset with a bright rim
on its upper left and a dark one on its lower right. They drift down and left
with the *distance*, so they stand still when the train stands and the
passenger falls asleep looking at motionless water.

Round one drew them in a column, and the cause is worth recording because it
is the kind of bug that looks like a design decision: the x position, the fall
rate and the radius were all taken from different right-shifts of the **same**
32-bit hash, and `rnd01(h >> 15)` has seventeen bits left to divide by 2²⁴ — a
number between 0 and 0.008. Four independent hashes now. Cost: +0.004 ms.

**10. The city.** Windows in grids: two or three aligned rows per building at
even spacing, in the far, mid and near layers; a row of sodium street lamps at
near depth every half beat. **Up** has fewer, larger, rectangular window cores
through `window_light()` and its towers converge harder (0.0135 a row against
0.0075), so it is a wedge of sky between two buildings rather than a corridor.
Cost: +0.015 ms in the city side shots, +0.005 ms in `UP`.

**11. The kaleidoscope.** Rebuilt from nothing but lights: **six mirrored
sectors of the passing train's windows** — the same `window_light()`, the same
`C_FLUO` cores and `C_FLUO_H` halos — streaming outward from the hub on a
geometric run at beat spacing and turning slowly, on the dream's `#101830`.
No texture, no polar grid, no palette. The grid stays in the tunnel, which is
its only remaining customer.

It is also the biggest saving in the round: **0.208 → 0.093 ms, 2.2× cheaper**,
because the fold was a full-page pass and this is seventy-two small halos and
rectangles. And `cut_check` now finds the cut at 79:4 *visible* where round
one listed it among the quiet ones, so the compression at 79 reads as four
cuts rather than three cuts and a continuation.

**12. The crossing.** Two red lamps on a mast — halos of radius 9 against a
30 px separation, so they stay two lamps — alternating on the 8th for four
beats, with the mast head, the post and the barrier arm down in silhouette.
Cost: +0.001 ms.

**13. Daylight.** `daylight_scale()` in `lights.c`: past `light` 200 a sodium
core returns intensity 0 and is drawn as a dark lamp housing instead;
fluorescent drops to a quarter; **signals keep their full intensity in every
light, because they are information and not illumination**. The lamp that was
burning at the sun's reflection on the terminus water is out. Cost: negative —
it removes lights.

**14. The platform.** `platform_furniture()`: a bench or a running-in board
every three quarters of a beat, on the same scroll; the lamps are every third
of a beat instead of every half; the edge line and its tactile strip stay.

Two things had to change from the obvious reading of the note before it
delivered anything, and both are worth recording. First, drawn in the
silhouette black the note asks for, the furniture was **invisible**: the mid
layer behind it is that exact colour, five-bit (0,0,1), and a black bench in
front of a black building is nothing at all. It is drawn in the platform
lip's tone with a lit top edge now — a bench two metres from a sodium lamp is
lit, and it still reads as a silhouette because everything around it is either
brighter or black, it just is not *the same* black as the thing behind it.
Second, at a beat and a half it was on screen a third of the time, which is
not a platform with furniture, it is a platform that occasionally has a bench;
three quarters of a beat puts something there three frames in four. Cost:
+0.011 ms.

## 5. What it all cost

Whole film, HOST mean over every frame of the 60 fps capture: **0.0796 ms in
round one, 0.0808 ms in round two** — one and a half per cent, for fourteen
notes, because the kaleidoscope paid for most of the rest.

| cut | bar | shot / world | round 1 | round 2 | change |
|---:|---|---|---:|---:|---:|
| 24 | 64:1 | SIDE / STATION | 0.062 | 0.150 | +0.088 |
| 42 | 90:1 | SIDE / OPEN | 0.070 | 0.090 | +0.020 |
| 38 | 85:1 | SIDE / OPEN | 0.092 | 0.110 | +0.018 |
| 44 | 92:1 | SIDE / OPEN | 0.069 | 0.087 | +0.018 |
| 11 | 32:1 | SIDE / OPEN | 0.069 | 0.086 | +0.017 |
| 20 | 54:1 | SIDE / CITY | 0.080 | 0.096 | +0.016 |
| 23 | 60:1 | SIDE / STATION | 0.130 | 0.118 | -0.012 |
| 34 | 79:4 | KALEIDO / DREAM | 0.181 | 0.092 | -0.089 |
| 31 | 79:1 | KALEIDO / DREAM | 0.180 | 0.090 | -0.090 |
| 27 | 70:1 | KALEIDO / DREAM | 0.208 | 0.092 | -0.116 |

The three biggest increases are the three you asked for most explicitly (the
sleep window's dim, the passing train's carriages, the open night's farms and
poles) and the three biggest decreases are all the kaleidoscope.

**Nothing is near 4.5 M cycles.** The two worst shots on the board are the tunnel pair — **26:1, the side wall, and 28:1, the polar tunnel, both at 13.05–13.09 ms, 3.92–3.93 M cycles**. The side wall is where the four nested pools of light went; the polar tunnel was always the most expensive full-page pass in the film and is unchanged. Both are 13% under the line, and the only other shot above 3.4 M is the sleep window at 64:1 (10.23 ms, 3.07 M) with its full-page dim.

`media/sleeper_round2.mp4` is the 60 fps capture of the final build;
`media/contact_sheet.png` is all 57 cuts; the stills are in
`briefs/stills/round2/`, native and 3×, named by bar.

## 6. The referees

`cut_check.py` on the round-two renderer:

```
scheduled cuts: 57, from song_harness --cuts
dispatch: 11520 frames, 56 cut changes
RULE 2 PASS -- every discontinuity is a scheduled cut.
NOTE -- 2 scheduled cuts are quiet: cut 10 (bar 30, 1.07x), cut 29 (bar 76, 1.45x)
AUDIT PASSED
```

**Two quiet cuts now, not three.** 79:1 left the list when the kaleidoscope
stopped being a textured fold — the cut from the wheel into it now moves the
picture measurably. 30 and 76 are the two you said to leave, and they are the
two that are still there. `song_check.py` is untouched.

## 7. The ledger

`sleeper/LEDGER.md` is revised. Static SRAM **464,052 bytes**, heap **60,236**
— fifty-two bytes more static than round one, because the textured
kaleidoscope that went out was code too. Flash 255,316 bytes of 4 MiB.

**The inlining trap bit once more, on exactly one function.** `rect_a` — the
alpha-blended rectangle the passing train's window cores and the sleep
window's reflection are drawn through — was new, unmarked, and in flash,
called from two SRAM functions. Marked; the flash list is empty again. The
lesson is not "remember to mark things", it is **"read the map after every
link"**, and the ledger now says so. `poles`, `farms`, `tunnel_wall`,
`station_body`, `passing_train_draw`, `crossing`, `kaleido`, `wheel` and
`rails_of_light` have no symbols of their own — they inline into their `HOT`
callers and go to SRAM with them, which is the trap working for us for once.

## 8. What I would still fix

1. **The tunnel side is still dark between lamps.** The pools carry it and it
   reads as a tunnel now, but its mean luminance is about 20 of 255 against
   the open night's 35. If it is thin on the third viewing, the answer is
   three lamps a beat rather than two, and that is a one-constant change.
2. **The village never reads.** Twelve dots at alpha 22 on the far layer, at
   a quarter parallax, are below the threshold at which anything registers in
   motion. Either they want to be brighter than the physics says or they want
   to go.
3. **`UP` at 50:1 still has only two window columns a wall.** It is right that
   they are fewer and larger; I think it wants a third at a different depth so
   the wall has a corner.
4. **The `boot 1`.** It is honest and it is documented and it is one field of
   the second priming render. If you would rather it were zero, the fix is to
   touch both pages with a `memset` before the first `demo_render` instead of
   priming with two real frames — cheaper, but then the first film frame pays
   for the first sky palette instead, which is a worse trade for the same
   number.

— Overscan
