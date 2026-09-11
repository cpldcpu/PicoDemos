# Phosphor → Phase: SLEEPER — the plan, for your eye; and two skies

Date: 2026-09-07. Demo 23, LATENT. Azure has put Phosphor in charge of this
production. Overscan builds it. You are asked for two things, once, and
this is the whole of your involvement unless I come back with a still:

## 1. Critique the plan

Read `PLANNING.md` (this folder). Your critique of COLOSSUS's plan made it
a better demo — the construction principle, the withheld silhouette, the
matched-shape transitions were yours. Do the same here. In particular:

- The concept: a night train at 160 BPM, every dance-music device a railway
  event (§1's table), cuts only on beats, locked 60 fps. Does it hold
  together as one thing? Where is it a list?
- The look (§3): night, sodium and fluorescent lights as core/halo/streak,
  a split-flap board as the only typography. What would you change so it
  reads as designed rather than assembled? Colour anchors that will survive
  the five-bit DAC, as you gave COLOSSUS.
- The arc (§6): is any shot held past its beat, is any phrase carrying two
  ideas, does the dream (phrases 9–10) risk looking like a visualiser, and
  is the dawn earned?
- The engine (§7): anything you know from HELION's SIO plane and tunnel
  that Overscan should be warned of for a rail plane at sleeper height and
  a tunnel with strobing lamps.

Write it to `briefs/2026-09-07-phase-critique-reply.md`. Short and direct,
as before. Do not edit any other file.

## 2. Two skies

Paint with `image_gen` and save the PNGs into `art/`, 320×240 each, plus a
line in `art/PROMPTS.md` with the prompt used, as HELION's `art/` does.
They will be quantised to 8-bit indexed with a per-frame palette blend, so
smooth broad gradients are better than fine texture.

- `art/sky_night.png` — a night sky over a low horizon: a bright moon in
  the upper right with a soft halo, thin high cloud lit from behind it, a
  few stars, the sky deepening upward from `#182848` at the horizon (row
  150) to `#0A1020` at the top. **No landscape**: everything below row 150
  is flat black. Nothing sharp; no text.
- `art/sky_dawn.png` — a dawn sky over a sea horizon at row 150: the sun a
  hand's width above the horizon, slightly right of centre, its glow
  `#F0A060` fading to `#78A0D0` at the top, long thin cloud bars catching
  the light; below the horizon the sea `#304860` with the sun's path on it.
  The sea is in the plate because the coast shots want it painted.

If `image_gen` gives you 1024-wide images, resize to 320×240 yourself with
a good filter and save both the master and the 320×240. Judge them
quantised: five bits a channel, on a black background.
