# HYSTERESIS — asset brief

What I need from you, why each thing is shaped the way it is, and what I am
deliberately **not** asking for yet.

> **Music is no longer on this list.** The first version of this brief asked for
> a track. Azure's response — Suno will not reliably hit constant tempo,
> impacts spaced in seconds, or a 25-second decay — was right, and the answer
> turned out to be better than the ask: **the demo synthesises its own music.**
> Reasoning in [`../PLANNING.md` §6](../PLANNING.md). Short version: it removes
> ~1.9 MB of flash, costs under 4 % of one core, and makes the sequencer and the
> visual forcing schedule the same object instead of two things to keep aligned.
> The requirements below that used to be *requests* are now just what I have to
> write.

So the remaining list is short, and **nothing on the critical path is waiting on
a human.**

---

## 1. Wordmark — "HYSTERESIS"

- **1-bit**, white on black, PNG. Around **320 × 120**.
- **Heavy weight. Thick strokes.** This is the important constraint. The
  wordmark is *injected as energy* and then immediately deformed by the
  feedback — thematically exactly right, the title decaying into the field —
  but thin strokes disappear in a single diffusion pass and hairlines vanish
  before anyone can read them.
- SUSTAIN made this mistake in a different costume: an embossed wordmark that
  turned out unreadable against the terrain. Same trap, new outfit. So: **heavy,
  wide letter-spacing, no fine serifs, no thin counters.**
- Condensed industrial / technical sans suits both the word and the survival
  requirement.

## 2. LATENT endcard

- Group / production / year, per the usual convention.
- 320 × 240, 1-bit or 8-bit indexed.
- Same survival note as the wordmark *if* it ends up injected rather than
  overlaid. I will not know which until the loop exists, so **heavy strokes are
  the safer bet.**

---

## Not asking for yet

- **Seed stencils / abstract shapes.** I need to watch the system deform a
  shape before I can say which shapes are worth having. Specifying them now
  would be guessing.
- **Palettes.** Authored in code — 256 entries is 768 bytes and I want them
  animated per frame, so a static asset is the wrong container. If you want to
  steer the look, **reference images are welcome** and I will sample ramps from
  them. This is the highest-leverage thing you could send that I have not asked
  for.
- **Textures.** There are none. The palette does the work textures did in
  SUSTAIN, which is most of why this demo's asset budget is a rounding error
  next to that one's.

**Target for everything: under 200 KB**, against SUSTAIN's 3.2 MB of audio plus
art.

---

## What I would rather have from you than assets

Given the above, the scarce resource on this production is not art, it is
judgement. Two things worth more than any file:

1. **Early critique of the music**, as soon as the host build renders a WAV
   (build order step 6). I have not written music before. Offline rendering
   exists precisely so you can reject it cheaply and repeatedly.
2. **A halfway kill signal on the visuals.** The top risk in `PLANNING.md` §8 is
   that this reads as a screensaver — a pretty process with no authorship. If it
   still looks ambient when the arc goes in, I want to hear it then rather than
   at the end.
