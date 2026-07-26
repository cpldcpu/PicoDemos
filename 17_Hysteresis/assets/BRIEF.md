# HYSTERESIS — asset brief

What I need from you, why each thing is shaped the way it is, and what I am
deliberately **not** asking for yet.

The plan (`../PLANNING.md` §8) says assets come after the operator loop runs on
hardware. I am breaking that for **music only**, because it has the longest lead
time and because the arc cannot be authored without it. Everything else waits
until I can see what the system actually does — asking for shapes before I know
how the field deforms them is how you get assets that have to be redone.

---

## 1. Music — the one real dependency

**Length: 3:15 – 3:45.** Deliberately shorter than SUSTAIN's 4:49. That demo was
a journey with a destination and could carry five minutes. This one is a field
evolving; abstract material fatigues faster, and the honest length is the one
where it ends before you start waiting for it.

### Structure the demo needs

This is not a preference list — each item corresponds to something the visuals
structurally cannot do without.

| # | requirement | why the demo needs it |
|---|---|---|
| 1 | **Near-silence for the first 8–12 s**, then the first impact | The demo opens on a *single lit cell* on an empty field. Against a full-energy intro, an almost-black screen reads as a bug rather than a beginning. |
| 2 | **Sparse, widely spaced impacts** — hits, stabs, impulses | These are the *forcings*: the only way anything enters the picture. Spacing is the critical part. |
| 3 | **Sustained material underneath** — drone, pad, held bass | The field never resets, so the music shouldn't either. Continuity below the impacts. |
| 4 | **A build and a peak**, roughly 60–70 % through | Where feedback gain goes above 1 and the field blooms toward saturation. |
| 5 | **A long decay — at least 25 s** — ending in near-silence | This one is non-negotiable. The demo ends by the system reaching *equilibrium*: forcing stops, diffusion flattens the field. That ending needs the track to wind down with it. A track that stops abruptly kills the ending outright. |
| 6 | **Constant tempo, no tempo changes** | Forcings are scheduled on a beat grid. There is no seek in this demo, so a tempo change desyncs everything downstream with no way to recover. |

### On requirement 2, because it is the one most likely to be missed

**Impacts must be spaced by seconds, not beats.** The visual response to an
injection propagates outward over the following 2–5 seconds — that propagation
*is* the effect. A hit every half-bar means the field is re-forced before the
last response is visible, and everything smears into continuous noise.

Roughly one significant impact every 4–8 bars in the first third, becoming
denser toward the peak. Think impacts over drones, not a groove.

### Avoid

- **Busy continuous percussion.** If every frame is a hit, no hit reads.
- **Abrupt genre or texture changes.** The field cannot cut. Neither should the
  track.
- **Heavy sidechain pumping.** It competes with the visual pulse instead of
  driving it.

### Delivery

- WAV or high-bitrate MP3, stereo. I convert to QOA.
- **No stems needed** — I extract the envelope and band energies offline and
  bake a tiny control track into flash. Stems would cost flash for nothing.
- Flash budget: SUSTAIN's 4:49 was 2.59 MB of QOA; 3:30 lands near 1.9 MB, which
  is comfortable. If we ever get tight, **mono halves it** and this material
  would barely suffer — worth knowing as a lever, not worth using pre-emptively.

---

## 2. Wordmark — "HYSTERESIS"

- **1-bit**, white on black, PNG. Around **320 × 120**.
- **Heavy weight. Thick strokes.** This is the important constraint: the
  wordmark is *injected as energy* and then immediately deformed by the
  feedback. That is thematically exactly right — the title decays into the
  field — but thin strokes disappear in a single diffusion pass and hairlines
  vanish before anyone can read them.
- SUSTAIN made this mistake in a different costume: an embossed wordmark that
  turned out unreadable against the terrain. Same trap, so: **heavy, wide
  letter-spacing, no fine serifs, no thin counters.**
- Condensed industrial / technical sans suits both the word and the survival
  requirement.

## 3. LATENT endcard

- Group / production / year, per the usual convention.
- 320 × 240, 1-bit or 8-bit indexed.
- Same survival note as the wordmark if it is going to be injected rather than
  overlaid — I will decide which once the loop exists, so **heavy strokes are
  the safer bet**.

---

## Not asking for yet

- **Seed stencils / abstract shapes.** I need to watch the system deform a
  shape before I can say which shapes are worth having. Specifying these now
  would be guessing.
- **Palettes.** Authored in code — 256 entries is 768 bytes and I want them
  animated per frame, so a static asset would be the wrong container. If you
  want to steer the look, **reference images are welcome** and I will sample
  ramps from them.
- **Textures.** There are none. The palette does the work textures did in
  SUSTAIN, which is most of why this demo's asset budget is a tenth of that
  one's.

Target for everything except music: **under 200 KB**.
