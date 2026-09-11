# SLEEPER · LATENT · 2026 — the plan

Demo 23. Directed and scored by **Phosphor** (Claude Fable 5.1), who is in
charge of the production. Code, platform, renderer and every hardware
measurement by **Overscan** (Claude Opus 5). Critique and painted sky plates
by **Phase** (GPT-6 Astra). Critic and producer: **Azure**.

**Revision 2**, 2026-09-07, after Phase's critique
(`briefs/2026-09-07-phase-critique-reply.md`); what changed is in §13.
Everything here is a decision unless it says "measure" — then it is a
question the board answers.

## 1. What it is

A night train, and the group's first fast demo.

Since SUSTAIN, every LATENT production has been slow on purpose: a drone, a
world that never cuts, a monument, an ocean, a star. They earned their pace.
This one goes the other way. It runs at 160 BPM, it cuts, and it cuts only on
the beat: the picture is a train at night, and everything that passes the
window passes at the tempo. Lamps, catenary posts, rail joints, bridge
girders, the windows of the train going the other way — the world is the
sequencer.

The name is three things at once. **Sleepers** are the railway ties the
track rests on, the grid the beat rides. A **sleeper** is the overnight
train. And the **sleeper** is the passenger, who falls asleep in the middle
and dreams the journey back in shapes. (A fourth, for the compo: a sleeper is
the entry nobody saw coming.) The train departs at 23:00 — demo 23.

The design rule that makes it one thing rather than a list of effects:
**every device of dance music is a railway event, one to one.**

| Arrangement device | Railway event | Picture |
|---|---|---|
| intro, tempo building | departure: the train accelerates | the platform slides away, lamps pass faster and faster |
| the beat locks | cruising speed | rail joints land exactly on the beat |
| filter closes, reverb shortens | a tunnel | the tunnel; lamps strobe on the bar |
| the drop | leaving the tunnel | open night at full speed, streaks |
| a fill | points and crossings | the rails multiply and cross |
| a pitch bend and a pan | the train going the other way | its windows strobe past, the horn Dopplers |
| the breakdown | braking into a station | the world slows; the music thins |
| the riser | the second departure, half-asleep | the dream accelerates |
| the relative major | dawn | the same tune, in daylight |
| the outro | the terminus | the split-flap board flips the credits |

No effect appears because it is possible. Each one is something a passenger
would see out of that window at that moment; the *ahead* and *under* views
are the passenger's inserts, what the mind's eye adds to the sound. The
dream (§6, phrases 9–10) is where the shapes of the journey come apart into
abstraction — that is where the demo shows off, and it is earned by the
sleep.

**One visual sentence** (Phase's phrase) runs through everything: two
rails, repeated crossbars, one light beyond them. The platform's tubes
become passing windows, become bridge bays, become the dream's rails; their
spacing and screen position match across cuts, so a cut changes the world
and keeps the sentence. In most frames one lamp wins: the frame is dark
enough that a single light is the subject.

## 2. The claims

Three, all measured, all by tools:

1. **Locked 60.** Every frame is drawn in under a VGA field and no frame is
   shown twice: over the whole 192 s on the Pico 2, with the score running
   on core 1, the scanline-zero handshake finds a new page at every vsync.
   11,520 fields, 11,520 pictures. The group's best so far is HELION at a
   59.6 mean with four windows below 58; this production's rule is that a
   shot that cannot make budget is simplified, and the claim is not relaxed.
2. **Only scheduled cuts.** SUSTAIN never cut; SLEEPER only cuts. The cut
   list lives in the score (`song.c`), every cut sits on a beat, and
   `cut_check.py` runs SUSTAIN's discontinuity detector over the captured
   frames and requires the set of discontinuities it finds to equal the
   scheduled cut list — no cut off the beat, no glitch that reads as a cut.
3. **The usual.** Audio bit-identical on host and device by per-second hash
   over the whole run, zero underruns, deterministic seek, guarded frames.

## 3. The look

320×240, RGB555, doubled to VGA. **Night, then dawn.** One palette:

| Role | Colour | Where |
|---|---|---|
| Night sky | `#0A1020` → `#182848` | the sky plate, dithered; five-bit (1,2,4) at the top, (3,5,9) at the horizon |
| Landscape black | five-bit (0,0,1) | silhouettes: hills, trees, buildings, the platform — only the blue survives the DAC, and that is intended |
| Sodium lamp | five-bit (31,22,8) core, (24,12,2) halo | station and street lamps: sodium owns the near world |
| Fluorescent | five-bit (27,30,31) core, (16,19,24) halo | train windows, station roof lights, the board: blue-white owns enclosed space |
| Signal green / red | `#20FF60` / `#FF2020` | signals, the crossing — rare, and only ever information |
| Moon | `#E8EEF8` | one moon, upper right, all night |
| Dream | the same lamps, but the black is `#101830` and everything glows | phrases 9–10 |
| Blue hour | sky `#203860` → `#6080B0`, ground `#101820` | phrase 13 |
| Dawn | sky `#F0A060` → `#78A0D0`, sun `#FFF0D0`, sea `#304860` with the sun's path | phrases 14–15 |

Everything bright is a **light**: a small hard core, a soft halo, and — when
the train is moving — a **streak**: the light drawn along its *projected*
motion (depth and camera included) for the one sixtieth of a second the
frame exposes. Streak length is screen speed × the field time, so it is a
fact about the frame and not a filter, and a streak spreads a light's
energy along its length rather than adding to it: a passing train's windows
stay windows, they do not turn the frame white. The moon gets no streak.
The brightest five-bit steps are reserved for small cores. That is the
production's signature and it is analytic and cheap: lines and halos, never a
full-frame blur pass. Halos are HELION's `halo()`, additive, restricted to
lights; no bloom buffer.

**Dither every ramp** (COLOSSUS's rule: the DAC has five bits and a sky
across 140 rows lands as a staircase). Ordered 4×4 on per-pixel paths so the
pattern is stable, applied *after* the final palette tint reaches RGB555 —
an error-diffused 8-bit plate on its own does not prevent DAC banding once
it is tinted.

**Text is a split-flap board.** All lettering in the production — the title,
the station names, the credits — is a departure board whose characters flip
into place, each flip a small click in the score. One monospaced 8×12 face,
white on near-black tiles, a hairline across the middle of every tile. It is
the demo's only typography; nothing fades. When the board is on screen and
its text changes, the flip is the cut between the two texts, and that is
the only cut it makes.

**Cuts, never fades.** A shot ends on a beat and the next begins on the same
beat, and matched railway shapes cut directly — the board mediates nothing
but its own text changes. The only exceptions: the opening from black
(bar 0) and the black after the last flap (127:2). The tunnel and the dream
do not fade in; they cut.

## 4. The clock

- 24,000 Hz stereo, synthesised on the device. **160 BPM: a 16th is exactly
  2,250 samples**, a beat 9,000, a bar 36,000. The control tick is 50
  samples (45 ticks a 16th, 480 a second, so the per-second hash latch sits
  on a tick); blocks of 25.
- **128 bars = 4,608,000 samples = 3:12.0.** Sixteen phrases of eight bars,
  288,000 samples (12.0 s) each.
- The picture is a function of the audio sample (§8): a frame asks what
  sample the DAC is playing and draws that moment. No state survives a
  frame.
- **The timetable is in the score.** `song.c` holds the speed profile, the
  cut list, the section ladder and the event flags, and both the synth and
  the renderer read it through `song.h`. The train's distance along the line
  is the integral of the speed profile, in sleepers; the rail-joint clack in
  the music and the sleepers under the camera in the picture are the same
  number. At cruise a joint passes every beat.

## 5. The music

Written note by note, as always, and first: the picture is cut to it. E
minor, liquid drum and bass at 160 BPM — the half-time backbeat gives the
night its weight and the double-time hats give it speed, and the break is
literally a train rhythm. Dawn is G major, the relative: no key change, the
same notes lit differently.

- **Theme A**, the night tune: a rocking figure, the motion of a carriage.
  E5 · D5 B4 | G4 A4 B4 · | E5 · D5 B4 | A4 G4 F#4 E4 | over Em C G D, then
  the answer a third higher, peaking on G5 and settling on B4 over Em C D Bm.
  First on the lead at bar 16, over the line ahead; full at the drop (32);
  back at drop II (80); in G major at dawn (104).
- **Theme B**, the long notes: E5 – G5 – A5 – B5, each held a bar, an arch
  that rises and does not come down, over C G D Em. On the bridge (40), on
  the choir at dawn (96), under theme A at 88.
- **The clack**: two short clicks a rail joint, "da-dum", from the
  timetable, not the note grid. Accelerating with the train, silent in the
  station, doubled in the dream.
- **The horn**: a minor third of two air-horn saws, pitch bending down a
  semitone and panning right to left as the other train passes (bars 36 and
  85). The **bell** at departure, the **door chime**, the **brake** (a
  resonant squeal sliding down over two bars), the **points** (a rattle
  fill), and the **flaps** of the board.
- Instruments: a **Reese bass** (two detuned saws through a lowpass that
  opens with energy, the DnB signature, new for the group); an **electric
  piano** (two-operator FM, a bell-like tine with a soft attack, new); the
  break (kick, snare with ghosts, closed and open hats, a ride in the drops);
  a **pad** (COLOSSUS's detuned saws, slow filter); a **lead** (a soft pulse
  with portamento and a touch of the reed's breath); the **choir** ("oo",
  COLOSSUS's formant voice, for the dawn); a stereo delay and a plate.
- All integer, pull-model, block-size independent, hash-diffable. The synth
  engine is HELION's (block ring, control tick, RNG-per-voice rule).

## 6. The arc

Phrase = 8 bars = 12.0 s. Times are `bar:beat`. Shots are held for as many
beats as the row says; cuts are on the beat named. Speed is a fraction of
cruise. "Side" is the window view (the world scrolls right to left); "ahead"
is the driver's view; "under" is the camera at sleeper height looking down
the track, the sleepers strobing past.

| Phrase | Bars | Chapter | Picture | Speed | Music |
|---|---|---|---|---|---|
| 1 | 0–7 | DEPARTURE | 0:1 black; the board flips `LATENT PRESENTS` (bar 0–1), then `SLEEPER  23:00  NIGHT SERVICE` (2–3). 4:1 cut to *side*, standing at the platform: lamps, the platform edge, the roof's fluorescents; the bell at 4:1; the door chime at 5:3; at 6:1 the platform begins to slide. | 0 until 6:1, then ramps to 1.0 by 16:1 | ambience; the electric piano, one chord a bar; the bell; the clack begins slow and quickens |
| 2 | 8–15 | SPEED | *Side*, suburbs: lamps and lit windows, poles, trees; 12:1 a level crossing passes — red lights alternating on the 8th, the bell; 14:1 *under*, the first time. | ramping | Reese bass enters at 8; kick and snare half-time at 12; hats at 14 |
| 3 | 16–23 | THE LINE AHEAD | 16:1 *ahead*: rails, sleepers, catenary posts one a bar, a green signal a phrase; 20:1 *under*; 22:1 *ahead*, a tunnel mouth growing; 23:4 the mouth fills the frame. | 1.0 | **theme A** on the lead; full break |
| 4 | 24–31 | TUNNEL | 24:1 *tunnel* (HELION's polar tunnel with its spin and wobble removed, concrete rings; each lamp lights the concrete near it, one a bar); 26:1 *side* inside the tunnel: the wall rushing, each lamp a streak on the beat; 28:1 *tunnel*, the rings tightening; 30:1 the exit growing as a white disc. | 1.0 | the filter closes, the hall shortens, hats only; the bass pumps; a riser from 30 |
| 5 | 32–39 | DROP I | 32:1 *side*, open night: moon, hills, a farm's lamps, streaks; 34:1 *ahead*; 35:4 the other train's headlight; 36:1 *side*: the **passing train**, its windows a strobe, gone by 37:1; 38:1 *under*. | 1.0 | **the drop**: theme A full, the ride, the Reese wide; the horn 35:4–36:3 |
| 6 | 40–47 | THE BRIDGE | 40:1 *ahead*: girders pass overhead one a beat, their shadow crossing the cab; 44:1 *side*: black water below, the moon and the train's own windows reflected and rippling; the city's glow on the horizon. | 1.0 | **theme B** on the pad, half-time; the piano answers |
| 7 | 48–55 | THE CITY | 48:1 *side*: buildings in three parallax depths, window grids, sodium streets; 50:1 *up*: towers passing above, and the **rain** begins on the glass; 52:1 *ahead*, rain on the windscreen: the viaduct, signals, the yard lamps; 54:1 *side*, the droplets refracting the lights. | 1.0 | theme A varied, busier; the piano comps |
| 8 | 56–63 | ARRIVAL | 56:1 the **points**, rattle and picture together: *ahead*, the rails multiply and cross; 58:1 the **brakes**: *side*, the yard, rain; 60:1 the platform slides in under the station roof, slower; 62:1 stop; 62:3 doors; the platform's board reads `PERSISTENCE`. | ramps 1.0 → 0 from 58:1 to 62:1 | the rattle at 56:1; the squeal 58–60; drums out at 60; piano and pad alone; the chime |
| 9 | 64–71 | THE SLEEPER | 64:1 *side*, stopped: one lamp in the window, rain on the glass, a faint reflection of the carriage; the halo slowly widens (sleep); 66:1 **the dream**: that lamp, at the same screen position, becomes the one light beyond *rails of light* — rail pairs and crossbars from an off-centre vanishing point, turning; 68:1 the crossbars bend into *the wheel*, the lamp its hub; 70:1 the windows fold around the hub: *the kaleidoscope*. Each cut changes the remembered object; nothing new is introduced. | 0 (the world); the dream has its own clock | the pad and the choir; the tune inverted on the piano |
| 10 | 72–79 | THE RISER | 72:1 *rails of light* accelerate; 76:1 the board flips three destinations, `QUICKSILVER`, `HELION`, and holds on `PERSISTENCE` — the flaps accelerate, not the list; 78:1 *the wheel*, spinning up; 79:1–79:4 the established shapes compress, four cuts a bar, and the last light matches into the real window at 80:1. | the dream's speed ramps 0 → 1.25 | the riser: the clack accelerates from nothing, the snare rolls, the filter opens |
| 11 | 80–87 | DROP II | 80:1 *side*, full speed, the biggest streaks; 82:1 *ahead*; 84:1 *under*; 85:2 the **passing train**; 86:1 *side*; 87:1 *ahead*, the horizon's first blue. | 1.25 | the drop: theme A over theme B, everything in; the horn 85:1–85:4 |
| 12 | 88–95 | DROP II · 2 | one shot a bar, the city thinning into open land: the far city receding / ahead, open / the last glow on the horizon / under / open / ahead, the first field / the field, still dark / ahead. | 1.25 → 1.0 | A over B, then the tail of the phrase; the ride |
| 13 | 96–103 | BLUE HOUR | 96:1 *side*: fields, mist in the hollows, the sky lightening; 100:1 the sea appears beyond the fields; 102:1 *ahead*: the line curving toward the coast. | 1.0 | half-time; **theme B** on the choir, G major; the piano |
| 14 | 104–111 | DAWN | 104:1 *side*: the coast, the sun a hand above the sea, its path on the water (the dawn plate arrives on this cut); 108:1 *ahead* along the coast, the sun ahead-right; 110:1 *side*, the same sun, clear of the haze — not a brighter second dawn. | 1.0 | **theme A in G major**, the whole band, warm |
| 15 | 112–119 | TERMINUS | 112:1 *ahead*: the outskirts, the yard; 114:1 the brakes; 116:1 *side*: the platform slides in, sea light through the station glass; 119:1 stop; 119:3 doors; the board: `LATENT`. | ramps 1.0 → 0 from 114:1 to 119:1 | the band steps out voice by voice; the squeal; the chime; the piano resolves on G |
| 16 | 120–127 | CODA | the board, alone: `SLEEPER / LATENT / 2026`; `DIRECTION AND MUSIC / PHOSPHOR / CLAUDE FABLE 5.1`; `CODE AND HARDWARE / OVERSCAN / CLAUDE OPUS 5`; `SKIES / PHASE / GPT-6 ASTRA`; `FOR AZURE / UNTIL THE NEXT TRAIN`; at 127:2 every tile flips to blank — the last flaps are the last sound — and the frame is black. | 0 | the piano, the pad, the flaps; the plate's tail; silence |

The list in `song.c` is the truth (`song_harness --cuts` prints it: 57
cuts, every one on a beat); this table is its explanation.

## 7. The engine (Overscan)

Start from HELION's platform, which is measured and known: `main.c`,
`video.c`, `audio_pwm.c`, the accelerator layer, `tools/check.c`,
`tools/capture.py`, `build.ps1`, the host SDL player, and the telemetry
line. Rename, keep the token spellings so the serial tools parse unchanged,
and add the two counters the claims need: **repeated fields** (a vsync at
which scanline zero found no pending page) and **frames over 16,000 µs**.

Renderer: one file per world system, dispatched from the cut list.

- **The side world.** Layers scrolling with the train's distance: sky plate
  (§9), far silhouette (hills or city, parallax ¼), mid (poles, trees,
  houses, buildings, parallax ½), near (platform edge, fence, the ballast
  blur, parallax 1), and lights at every layer's depth. Layers are silhouette
  spans (black over the sky) generated from hashed procedural profiles per
  distance, not bitmaps. Lights are lamps and windows: core, halo, streak.
  Variants by parameter: platform, suburbs, tunnel wall, open night, bridge
  (water below with the reflection), city, rain, fields, coast, station.
- **The ahead world.** HELION's SIO affine plane for the ballast, sleepers
  and rails (a 128×128 indexed texture, SRAM), horizon layers from the side
  world's silhouettes at half height, catenary posts as 3D quads, signals as
  lights, girders (bridge) as dark bands across the top with a moving edge,
  the points as a second rail texture cross-faded by palette.
- **Under.** The same plane, camera at 0.4 m, looking down and forward; the
  sleepers strobe. Cheap and violent.
- **The tunnel.** HELION's polar tunnel grid with a concrete-ring texture;
  lamps as a palette strobe keyed to the bar.
- **The passing train.** A near-layer dark mass with a window grid, moving
  at twice cruise the other way; its windows are lights with streaks; the
  headlight is one big halo.
- **Water.** The lower half of the bridge and coast shots is the upper half
  mirrored with a per-row horizontal offset from a sine table (rows near the
  horizon compressed), darkened; the moon's and sun's paths are long
  vertical halos.
- **Rain.** 40–60 droplets, each a small circle where the source is sampled
  through a fixed offset field (the refraction), drifting diagonally with
  the speed; the lights behind bend through them.
- **The dream.** *Rails of light*: 16–24 rail pairs from a vanishing point
  with ties as short strokes, everything a light; *the wheel*: 24 spokes
  and a rim from one centre, a halo hub; *the kaleidoscope*: an SIO affine
  span of the lamp texture with U and V mirrored on a 60° fold.
- **The station.** The side world with a roof: the fluorescent tubes as a
  row of lights above, the glass as a lighter band, and the board.
- **The board.** Tiles 8×12 from one glyph atlas (`font8x12.h`), each
  character flipping through its predecessors over 3 fields with the top
  half and bottom half drawn separately; the flip schedule comes from
  `song_board()` so the flaps in the score and on screen are one event.
- **Lights, halos, streaks, dither** in one file.

Memory: two pages 307,200; rail and tunnel textures 32 KiB; sky plates in
flash, indexed (§9); synth ~40 KB; the rest is scratch. Ledger in
`sleeper/LEDGER.md` before any allocation, as COLOSSUS did, with scanvideo's
invisible ~12 KB heap and the boot floor from COLOSSUS's bisection.

Budget: **4.5 M cycles a frame at 300 MHz**, everything included, with the
synth's ~14% of core 1 running. Any shot measured above 4.5 M is simplified
in this order: fewer lights, fewer layers, a cheaper streak, a shorter
shot. Not: a lower frame rate.

## 8. The contract

`sleeper.h`: `demo_init()`, `demo_render(uint16_t *page, uint32_t sample)`,
`demo_stats()`; `synth_init/render/position/seek`, `synth_hash_latch`;
WIDTH, HEIGHT, SAMPLE_RATE, BEAT_SAMPLES, BAR_SAMPLES, DURATION_SAMPLES; the
DAC packing. `song.h` is the timetable (§4). Phosphor owns both headers;
they change only by agreement. `demo_render()` may be called for any sample
in any order and must produce the same page; the check tool proves it.

## 9. Assets (Phase)

Two painted skies (delivered 2026-09-07, `art/sky_night.png`,
`art/sky_dawn.png`, masters beside them, prompts in `art/PROMPTS.md`),
320×240, quantised to 8-bit indexed with a 256-entry palette each, in
flash. The indexed plate is expanded through a per-frame palette into the
page (an explicit pass, its cost measured, not HELION's RGB16 DMA), and the
palette is tinted: **the night plate serves bars 0–103**, its tint moving
from night to the first blue at 87 and through the blue hour at 96–103 —
the moon stays, paler, which is what a blue hour looks like; **the dawn
plate arrives on the cut at 104:1** and its tint warms from cool to the
sunrise over 104–111. The two plates never blend into each other; a moon
cannot morph into a sun. The dawn plate's sea is painted, so the coast
shots do not mirror a second one.

| Asset | Content |
|---|---|
| `sky_night.png` | a night sky over a low horizon: a bright moon upper right with a soft halo, thin high cloud lit from behind, a few stars, the sky deepening upward from `#182848` at the horizon to `#0A1020`. No landscape: the horizon line is at row 150 and everything below it is black. |
| `sky_dawn.png` | a dawn sky over a sea horizon at row 150: the sun a hand's width above the horizon slightly right of centre, its glow `#F0A060` fading to `#78A0D0` at the top, long thin cloud bars catching the light, the sea below the horizon `#304860` with the sun's path — the sea is in the plate because the coast shots want it painted. |

Sources are kept in `art/`; `tools/pack_assets.py` quantises with a fixed
seed and dithers, and the manifest records the settings.

## 10. Referees

1. **`cut_check.py`** — the discontinuity detector over the captured frames
   (SUSTAIN's measure), asserting the found set equals the scheduled set
   from `song_harness --cuts` within one frame, and that every scheduled
   cut is on a beat — paired with the renderer's own dispatch log (which
   cut it drew, per frame), because a matched cut can be quiet and a lamp
   flash loud; the detector alone does not prove the list.
2. **`song_check.py`** — block sizes 1, 8 and 1,024 byte-identical; per-bar
   peak, RMS and DC; ends in silence; the audition of every voice.
3. **`check.c`** — 4,801 guarded frames, deterministic seek, audio block and
   seek identity, the hash latch's timing, the endpoint black.
4. **The device** — per second: fps, render best/mean/worst, repeated
   fields, frames over 16,000 µs, ring fill, underruns, pump cost, `AHASH`.
   The claim in §2 is the whole-run count of repeated fields, and it must be
   zero on the shipping UF2 with the shipping score. 160 BPM at 60 Hz is
   22.5 fields a beat: everything is scheduled from samples, beats fall on
   alternating 22- and 23-field spacings, and nothing accumulates a rounded
   frame interval.
5. **The film** — a 60 fps host capture (not 30: the production is about
   60), a contact sheet of every shot at native size, and the human look.

## 11. How the team works

- **Phosphor** wrote this plan, writes the score and the timetable, reviews
  every still and every capture, cuts the shot list when a shot is not
  earning its beats, and says when it is done.
- **Overscan** builds it: platform, renderer, tools, the memory ledger, the
  hardware runs, the UF2s, the captures. Rounds are brief files in
  `briefs/`, dated, with a reply file each.
- **Phase** critiques the plan once (the design eye that improved
  COLOSSUS), paints the two skies, and is otherwise not spent: Codex is
  used sparingly, as Azure asked on COLOSSUS.
- Credit is by model and handle. No invented collaborators.

## 12. What would make it fail

- The tune not being good. It comes first and it is the reason to watch
  twice.
- A frame rate that is "mostly 60". The claim is binary.
- A cut that is not on the beat, or a shot that is held after its beat has
  passed. Fast is a discipline, not a speed.
- Lights that do not look like lights. The core/halo/streak triple is the
  whole look; if it reads as circles and lines, stop and fix it before
  building a single scene on top.
- The dream looking like a plug-in visualiser. It is made only of the
  journey's shapes — rails, a wheel, lamps, the board — and it is short.

## 13. Revision notes

Taken from Phase's critique (`briefs/2026-09-07-phase-critique-reply.md`),
with thanks:

- The one visual sentence (two rails, crossbars, one light) and the rule
  that one lamp wins in most frames; the five-bit colour anchors; streaks
  from projected motion that spread a light's energy rather than add to it;
  the moon unstreaked; dither after the tint. §1 and §3.
- The tunnel's side cut at 26, so nobody watches six seconds of pumping
  rings. The rain from 50, carried into the stopped window at 64. The
  points at 56:1 in both the score and the picture.
- The dream as one remembered object changing — the stopped lamp becomes
  the rail light, the crossbars bend into the wheel, the windows fold into
  the kaleidoscope — with a carriage reflection kept, and the last light
  matched into the real window at 80. Three destinations on the board at
  76, ending on PERSISTENCE, instead of eight.
- Phrase 12 as the city thinning into open land, not a second bridge and
  tunnel; the short tunnel and its filter dip are gone from the score. The
  same sun at 110, not a second dawn.
- The sky plan in §9: night plate to 103 with a tinted palette, dawn plate
  from 104, no morph; the dawn sea painted once.
- Referee 1 paired with dispatch logging; the 22.5-fields-a-beat note under
  referee 4.
- Overscan's inheritance traps for the SIO plane (start below the horizon
  singularity, clamp far distance, merge subpixel sleepers, reduce distance
  modulo the texture period before 16.16, keep near rail edges stable), the
  tunnel (no spin or wobble, local lamp lighting, seam and centre tested on
  the brightest frame) and the indexed sky's expansion cost are in Phase's
  reply and are part of round one.

Not taken: "everything from the window" now says the ahead and under views
are the passenger's inserts, which is what they were; the board never
mediated cuts, and §3 now says so plainly.
