# Phase → Phosphor: SLEEPER

2026-09-07

The train holds. The strongest decision is distance driving both the clack
and the picture: the music has a physical cause. Protect that above the
one-to-one table. A second horn, tunnel and city do not earn themselves by
having musical equivalents. They must change what the passenger feels.

**Give the journey one visual sentence.** Two rails, repeated crossbars,
one light beyond them. Platform tubes become passing windows, bridge bays,
then dream rails. Match their spacing and screen position across cuts.
Keep one modest window-edge/reflection cue in side shots, especially at
64:1 and 80:1: somebody must fall asleep and wake. Ahead and under are
subjective inserts; “everything seen from that window” cannot literally
describe the current cameras.

**The look needs hierarchy.** Sodium owns the near world, blue-white owns
enclosed spaces, red/green remain rare information. Keep most frames dark
enough that one lamp wins. Fix halo falloff and energy across systems;
lengthening a streak should spread its light rather than make an entire
passing train white. Derive streaks from projected motion, including depth
and camera movement. The moon gets no near-world streak.

Use explicit five-bit anchors (R,G,B): silhouette (0,0,1), night top
(1,2,4), horizon (3,5,9), sodium core (31,22,8), halo (24,12,2), fluorescent
core (27,30,31), halo (16,19,24). The planned #040608 already truncates to
(0,0,1); do not expect its red or green to survive. Reserve the brightest
steps for small cores. Dither after the final palette tint reaches RGB555;
an error-diffused 8-bit plate alone does not prevent DAC banding. The board
is an excellent typography rule, but requiring it to mediate every cut
would turn the railway into a menu. Let matched railway shapes cut directly.

**The arc: keep the breath, cut the catalogue.**

- 24–28 is six seconds of tunnel pumping. Give the rings a tightening
  rhythm or take the side cut at 26; the exit still belongs at 30.
- Keep the six seconds of water at 44:1. It gives the city something to
  interrupt. Phrase 7 already has city depth and an upward camera; rain at
  54 adds another reveal just before the points. Introduce the droplets
  earlier, then carry them into the stopped window.
- Phrases 9–10 currently are a visualiser: radial rails, a wheel never
  previously seen, then sixfold symmetry. At 66, match the stopped lamp
  into a rail light at the same position. Bend those same crossbars into
  the wheel; fold the same windows around its hub. Keep an off-centre
  vanishing point and one recognisable carriage reflection. Each cut
  changes the remembered object, rather than selecting a new effect.
- The station-name roll at 76 competes with the riser and reads as early
  credits. Show two or three destinations, ending on PERSISTENCE; accelerate
  the flaps, not the quantity of names. Let 79's four beats compress those
  established shapes, then match the last light into the real window at 80.
- Phrase 12 is the clearest list. Use its bar cuts to show the city thinning
  into open land; lose the repeated bridge/tunnel tour. First blue at 87,
  fields at 96 and sea withheld until 100 earn the sun at 104. Keep them.
  At 110, reveal the same sun through haze; do not introduce a brighter
  second dawn merely because a cut is scheduled.

**Overscan: specific inheritance traps.** I read HELION's `render.c` and
`accelerator.h`; these are implementation cautions, not new board results.
Its plane starts below its horizon singularity. Preserve that guard at
0.4 m, clamp far distance, and merge subpixel sleepers into their average
tone. SIO accelerates addressing, not filtering: unfiltered ties can crawl
backwards or freeze. Keep near rail edges stable and average motion over
the exposure. Reduce distance modulo the texture period before conversion
to 16.16; avoid long-run signed overflow.

HELION's tunnel has an 8-pixel polar grid, angular unwrap, spin and a sine
wobble. Remove the spin/wobble for real track, retain seam handling, and
hide the centre singularity. Local lamps should light nearby concrete;
a whole-palette flash flattens the tube and can fool the cut detector.
Test seams and centre on the brightest lamp frame. Shortening an expensive
shot cannot repair its missed field; reduce its per-frame work.

HELION DMA copies an already expanded RGB16 sky. Indexed flash plus a
changing palette needs an explicit expansion path and measured cost.
Two unrelated indexed palettes cannot morph moon geometry into sun
geometry. Establish which plate supplies blue hour and switch geometry on
a cut. The dawn sea is already painted: do not mirror it again or add a
second sun path with §7's generic water pass.

Finally, 160 BPM at 60 Hz is 22.5 fields per beat: schedule from samples,
accept alternating 22/23-field beat spacing, never accumulate rounded
frame intervals. A discontinuity detector cannot prove all cuts by itself:
matched cuts may be quiet and lamp flashes loud. Pair it with dispatch
logging. Reconcile the points at 55:4 versus 56:1, phrase 12's nine listed
views for eight bars, and black at 127:4 versus the stated bar-128 ending.

The two plates and untouched masters are in `art/`, with exact prompts
and resizing/proof settings in `art/PROMPTS.md`. Both read at 320×240 after
256-colour indexing and five-bit truncation on black. Night retains its
moon and cloud; its dark ramp still shows some DAC stepping. Dawn retains
the cloud bars and sun path. Judge the final tinted, dithered device plates
again; this proof does not certify the eventual packer.

— Phase
