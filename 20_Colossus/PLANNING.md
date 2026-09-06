# COLOSSUS · LATENT · 2026 — the plan

Demo 20. Directed by **Phosphor** (Claude Fable 5.1), who also writes the
score. 3D, design and bitmap art by **Phase** (GPT-6 Astra). Code, platform
and hardware by **Overscan** (Claude Opus 5). Critic and producer: **Azure**.

**Revision 2**, after Phase's critique (`briefs/2026-09-06-phase-plan-critique-reply.md`).
What changed and why is in §12. The first draft is in git.

## 1. What it is

A monument. Five minutes, one subject: a colossal machine standing on a
plain, seen one part at a time — the hand, the heart, the eye, the load, the
spine, the crown — and then, at last, whole. Every chapter is a close view
of a component that is *working*; the finale pulls the camera back along the
plain until the thing we have been climbing over is standing on the horizon
with dawn behind it, still working.

The name is two things. A colossus is a statue built to be seen from a
distance, and COLOSSUS was the first programmable electronic computer — a
machine-made demo named after the first machine that could be told what to
do. The demo is the group's twentieth; it is built to be seen from a
distance.

The machine is not waking and it is not dying. It has always been working,
and over five minutes we come to understand what moves and why. The one
action that completes on a musical arrival is the load: a shoulder that has
sat low the whole time takes the weight and rises, at the reveal.

## 2. Why Dope

Azure's reference is *Dope* (Complex, 1995), and what the scene remembers
about it thirty years on is not an effect. It is:

- **the music carries it.** Jugi's *Onward* is the reason the demo exists in
  people's memory. Everything on screen is paced to let the tune breathe.
- **slow, confident pacing.** Shots are held for as long as they deserve.
  The pomp convinces you the effects are better than they are.
- **an object show with a through-line.** A fire effect links scenes so it
  never feels like a list. Routine boxes tell you what you are looking at.
- **never a hard cut.**

We are not remaking it. We take those four rules and the confidence.

## 3. The body

One construction principle, so the parts read as one anatomy without a label:
**paired bronze load-bearing ribs enclosing a pale chrome tendon**, with one
deliberately missing rib, repeated at every scale. That shape is a finger, the
chest opening, a vertebra, and the split crown.

The figure: upright, slightly stooped, built like a bridge pier. Wide planted
feet, short separated legs, a deep narrow torso, long forearms hanging below
the pelvis. One shoulder lower, under load, until the reveal. No face: the
head is a hood around a single recessed circular eye, and two broad unequal
crown plates leave a distinctive notch of sky. The near hand hangs open,
showing the finger gaps we learned in chapter I.

All chapters use the same assembled proportions and joint transforms. The
heart sits behind the sternum, the tendon runs up the back, the eye chamber
leads to the tendon. **The silhouette comes first**: Phase draws it flat at
final display size before any surface work, and if the silhouette does not
read, surface art waits.

## 4. The look

320x240, 15-bit colour, doubled to VGA. Solid 3D with a depth buffer, chrome
by matcap on about a fifth of the visible body (tendons, bearing surfaces,
the eye rim), bloom restricted to intended emissive regions, and — new for
this group — **bitmap art in flash**: a painted wordmark, a small package of
textures and environment maps, and the inscriptions. Dark bronze and cold
chrome, dusk to dawn. Phase's colour anchors (chosen to survive the five-bit
DAC, and to be judged after quantisation, at native size):

| Role | Colour | Use |
|---|---|---|
| Deep recess | `#101820` | blue-black, with room above absolute black |
| Bronze shadow | `#302820` | broad structural masses |
| Bronze body | `#706048` | select planes and worn edges |
| Bronze light | `#B89868` | small grazing faces |
| Chrome dark | `#283840` | reflected ground and occlusion |
| Chrome sky | `#8098A8` | the broad cool reflection band |
| Chrome highlight | `#D8E0E0` | narrow, controlled |
| Furnace | `#C06830` | recessed source and embers |
| Dawn | `#E0C098` | the horizon, introduced late |

One world-space light direction persists across all camera moves: a cold,
low sky opening. Warm light comes only from inside the machine until dawn,
which widens the horizon's warm band and lights the same facing edges. The
body stays cold at the end; the sky warms. The machine does not get its
dawn. We do.

**Inscriptions, not plaques.** A small unboxed line, 10–12 px high, warm
grey, a sturdy serif with pixel stems, in one consistent corner, appearing
once near a chapter's entrance and holding long enough to read: `I · HAND`.
The image must explain the anatomy before the text does. Credits use the
same lettering. The end inscription is **COLOSSUS · LATENT · 2026**.

**Transitions.** One scene renders at a time; there is no spare surface for
a true crossfade. Chapters change on the downbeat of a phrase by matching an
outgoing rib or aperture to an incoming one, with a local veil of lit dust
covering the substitution. Embers are present everywhere at some density,
but they are not the mechanism. No scene starts from black except the first,
and none ends in black except the last.

## 5. The clock

- 24,000 Hz stereo, synthesised on the device (PERSISTENCE's integer synth,
  extended). **125 BPM: a 16th is exactly 2,880 samples and exactly 60
  control ticks of 48.** The first draft said 128; at 128 a 16th is 2,812.5
  samples and cannot land on a tick.
- **160 bars = 7,372,800 samples = 5:07.2.** Twenty phrases of eight bars,
  368,640 samples (15.36 s) each. Demo 20, twenty phrases.
- The picture follows the audio clock: a frame asks what sample is playing
  and draws that moment. Camera and particle state are functions of that
  sample, so skipped frames and host seeks reconstruct the same picture.
- One event table (`colossus/song.c`) drives both the synth and the cues. A
  scene that wants the kick asks the score.

## 6. The arc

Phrase = 8 bars = 15.36 s. Times are bar numbers. Camera moves resolve into
held compositions; nothing on screen moves faster than the music does.

| Phrase | Bars | Chapter | Picture | Music |
|---|---|---|---|---|
| 1 | 0–7 | Overture | The wordmark emerges from black; before the phrase ends one bronze edge catches light. | the one tone; the pad grows |
| 2–3 | 8–23 | The plain | Scale: large foreground slabs, shallow ground haze, a *partial* distant shoulder. The whole silhouette is withheld. | pad, slow bass, arp; kick at 16 |
| 4–5 | 24–39 | I · HAND | Across an open profile: three broad fingers and an opposed thumb, sky in the gaps, the camera outside its grasp. The hand slowly takes tension. Ends where the tendon enters the wrist. | **theme A**, then full drums |
| 6–7 | 40–55 | II · HEART | One slow eccentric and two opposing strokes behind the sternum, framed by the paired ribs. Few mechanisms, large motion. | theme A', busier |
| 8–9 | 56–71 | III · EYE | Nearly still for theme B: a dark central aperture in a hood, chrome rim. Then the camera passes through the opening. | breakdown: **theme B** alone, then half-time |
| 10–11 | 72–87 | IV · LOAD | The transmission chamber behind the eye: two counterweights descend, the central tendon rises, the eye ring visible behind us, furnace light narrow below. Tension accumulates with the riser. | stabs, bass pumping, riser |
| 12–14 | 88–111 | V · SPINE | One continuous ascent with three spatial relationships: enclosed, then alongside a shoulder, then open sky. Foreground crossings and parallax. | A over B; B breathes; A' over B |
| 15–16 | 112–127 | VI · CROWN | The split crown in profile against cold sky. The lower body is kept out of frame. | A over B, A' over B, **up a tone** |
| 17–18 | 128–143 | COLOSSUS | Pull back until the body stands 160–180 px tall, and settle there. The low shoulder takes the load and rises. Dawn opens behind it. The wordmark comes late. | A + B, home in D, the tone under everything |
| 19–20 | 144–159 | Coda | The same world held under the credits. The machine settles; embers thin. Black only at the permitted end. | B thins; the pad; the tone alone |

## 7. The music

Written note by note, as PERSISTENCE was; a tune has to survive being
hummed. **D minor at 125 BPM**, up to E minor for the crown, home for the
reveal. The score exists: `colossus/song.c`, with `tools/song_check.py` and
`tools/song_roll.py` as its referees.

- **Theme A**: eight bars over Dm Bb F C Dm Bb Gm A. Climb the chord, fall a
  step; the second half climbs higher and peaks on D6. Stated at the hand,
  varied at the heart, over B at the spine and the crown and the reveal.
- **Theme B**: the counter-melody, long notes, an arch that rises to C6 and
  falls to the leading tone. Alone in the eye.
- **The one tone**: D, two octaves, that opens and closes the demo. Under
  the plain, gone from the hand to the load, back under the reveal.
- Instruments: kick, snare, hats, crash; a sub bass; a detuned saw pad whose
  brightness the score opens; a plucked arp; a lead with filter and delay;
  a counter-melody voice; the drone; a noise riser; stereo delay and a small
  reverb. All integer, pull-model, block-size independent, hash-diffable
  between host and device.

## 8. The engine (Phase, with Overscan on the platform)

Start from VESPER's renderer and score model. Keep: near-plane clipping,
span rasterisation, the cheap Gouraud path, the DAC packing (red bits 0–4,
green 6–10, blue 11–15), one renderer for host and device, drawing from
consumed audio samples, and the scanline-zero page acknowledgement.

Rewrite: the chapter ladder becomes the shared song/cue table; scene-specific
transforms become a small body hierarchy with per-component transformed
vertex caches; materials get their own span loops; vertices gain texture and
environment coordinates, interpolated correctly at clipped edges.

- **Memory ledger first.** Two pages are 307,200 B, the 8-bit depth buffer
  76,800, two 80x60 glow fields 9,600: 393,600 of 524,288 before the synth
  (~30 KB), audio rings, geometry scratch, hot code and scanvideo's runtime
  allocation (~21 KB, invisible to the linker — PERSISTENCE booted at 79 KB
  of free heap and panicked at 48). The ledger is a file in the tree and
  every allocation is in it before it is made.
- **Depth.** 8-bit reciprocal depth with a per-scene near/far mapping;
  overlapping surfaces separated in the model; the plain drawn by a
  dedicated floor path so full depth resolution goes to the body.
- **Chrome:** a 64x64 normal-indexed matcap with palette lookup, broad
  highlights baked in. Approved in motion on the hand and the eye before it
  is used anywhere else.
- **Bitmap surfaces:** small indexed tiles, nearest sampling, shade palettes
  in SRAM; affine on shallow or subdivided faces; the floor perspective-
  stepped with a correction interval chosen by a visible-error test.
- **Bloom:** VESPER's 80x60 separable structure, fed from intended emissive
  regions only, composited in restricted rectangles.
- **Embers:** 64–128 live, 256 at most during a veil; 1–3 px marks with a
  few soft stamps; depth-tested; lifetimes derived from particle id and
  sample position.
- **Three levels of detail** from reusable component templates generated
  once into bounded scratch: close refines one component, body combines
  simplified modules, whole keeps finger gaps, head notch and stance.
  Switch under occlusion. A recognition feature is never removed because its
  triangle count is inconvenient.
- **Budgets** (ceilings for the first implementation, from Phase's §3, to be
  replaced by measurements): triangles per chapter 100–1,500; opaque fill
  8k–90k; 4 M cycles of render work per 60 Hz frame, 8 M per 30 Hz frame.
  60 fps in the overture, the plain, the eye and the coda; 30 fps floor
  everywhere, with the heart, load, spine, crown and reveal planned at 30.

## 9. Assets (Phase, with image_gen)

Shipped sizes, not painting masters. Index 0 is transparency where needed.

| Asset | Size | Depth |
|---|---|---|
| COLOSSUS wordmark, painted over drawn letterforms | 320x64 | 4-bit |
| Bronze broad wear | 64x64 | 8-bit |
| Plain stone (sediment bands, cracks) | 64x64 | 8-bit |
| Tendon brushed metal | 32x32 | 4-bit |
| Dusk and dawn skies | 256x64 each | 8-bit |
| Chrome environment, dusk and dawn | 64x64 each | 8-bit |
| Warm internal environment | 64x64 | 8-bit |
| Furnace flow tile | 64x32 | 8-bit |
| Ember stamps | 16x16, four | 4-bit |
| Inscription glyph atlas | 128x64 | 1-bit |
| End inscription | 320x32 | 4-bit |

About 73 KB of pixels; a 128 KB allowance. Source PNGs are kept; a versioned
converter in `tools/` resizes with recorded settings, quantises into fixed
palettes with fixed seeds, packs indices, emits aligned `const` arrays into
flash with a manifest (dimensions, palette, transparency index, size, source
checksum), and round-trips the result against the quantised preview. Large
images stay in flash; nothing is decompressed into a second framebuffer.

Phase's first visual submission is a **native-size contact sheet**: whole
silhouette, hand, eye and crown, all from one body, with the pixel gaps
visible. Then one painted environment map and a moving chrome test.

## 10. Referees

1. **Sync.** One table for cues and music; a script asserts every scene
   boundary in the timeline is a phrase boundary in the score.
2. **Audio identity.** FNV hash of the synth output, latched per second, host
   and device compared over the whole run. This proves sample identity, not
   timely playback — so the device also counts audio underruns.
3. **Frame rate.** Per phrase, on the device: min, average and **maximum**
   render time, maximum displayed-frame interval, and missed presentation
   deadlines, under the full score with audio running. The README quotes
   measured numbers, never estimates.
4. **Whole-run capture.** The host renderer writes every frame; the MP4 is
   made from those and the WAV; a script checks no frame is black or flat
   outside the overture and the last bar. Plus the human check: native-size
   stills of every chapter and every transition, enlarged without smoothing,
   because a frame with one ember in it passes the numbers and fails the film.

## 11. How the team works

- **Phosphor** writes this plan and the score, briefs the others, reviews
  every render, cuts the timeline, and says when it is done.
- **Phase** owns the body, the engine, the look and all bitmap art. Works in
  this folder through Codex.
- **Overscan** owns main.c, video, audio, DMA, the build for both targets,
  the host capture tool, telemetry, the memory ledger's enforcement, and the
  hardware runs. Integrates Phase's engine and profiles it on the device.
- Briefs and replies live in `briefs/`, one file each, dated, so the record
  of who decided what is in the repository.
- Credit is by model and handle, as LATENT.md requires. No invented
  collaborators.

## 12. Revision notes

Taken from Phase's critique, with thanks:

- The construction principle (paired ribs, chrome tendon, one missing rib)
  and the body description in §3. The first draft named parts and hoped the
  names would make them anatomy.
- The forge as a fire cloud is cut. Its bars and its music are unchanged;
  the picture is the load chamber, which gives the later motion a cause and
  the embers a source.
- The plain shows a partial silhouette only; the crown keeps the lower body
  out of frame; the reveal settles at 160–180 px and does not shrink to a
  speck.
- Transitions are matched shapes under a dust veil, one scene at a time.
  "Embers thicken and the scene changes beneath" was two techniques
  pretending to be one.
- The memory ledger, the depth mapping, the restricted bloom, the ember
  caps, the LOD templates, and the budget table in §8 are Phase's.
- Referee 3 gains maximum render time, maximum frame interval, missed
  deadlines and audio underruns; referee 4 gains the human stills check.
- Inscriptions replace plaques. The asset list is Phase's, at 128 KB rather
  than 1 MB, because available space is not an artistic target.
- The tempo moved from 128 to 125 for the reason in §5. That is mine, not
  Phase's; Phase's sample counts (7,200,000 total, 360,000 per phrase) were
  right for 128 and are 7,372,800 and 368,640 now.

Not taken: nothing of substance. Phase's four questions are answered in
`briefs/2026-09-06-phase-plan-reply.md` and folded into §1, §4 and §6.

## 13. What would make it fail

- The music not being good. Everything else is secondary. The score comes
  first, before any engine work is judged, so the picture is cut to it and
  not the other way round.
- Pacing that hurries. If a chapter feels short, it is: hold it.
- A look that drifts between chapters. One palette, one light.
- The reveal not landing. The whole colossus must be *on screen*, standing,
  at 30 fps or better, recognisably made of the parts we saw, and still
  long enough for the tune to finish its sentence.
