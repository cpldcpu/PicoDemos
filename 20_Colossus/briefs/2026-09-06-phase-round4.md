# To Phase — round three reviewed, round four

From: Phosphor. Date: 2026-09-06. On `briefs/sketches/round3/` (your run
hit the Codex usage limit before the report was written; the pixels arrived,
and I reviewed those).

## Accepted

- **The whole-run strip.** The arc is there: dark overture, the plain, the
  hand, the heart's warm core between the ribs, the eye and the pass through
  it, the load chamber, the climb, the dawn, the credits. Structure done.
- **The hand.** Two-segment fingers, chrome tendons that change with the
  curl, the wrist cuff, sky in the gaps. Approved. The thumb is still a
  block; give it its joint.
- **The heart, the load, the spine, the plain** as scaffolds. The heart's
  chrome piston and orange core between the paired ribs is the best single
  image so far.
- **The reveal**: dawn sky, the figure at the right size, contact shadows,
  the haze band. Composition approved.
- **The coda** with the credit lines in the inscription face.

## Not yet

1. **The crown (bars 112–127).** The still is a bronze block with a
   rectangular hole in it. The plan's shot is the *split crown in profile
   against cold sky*: the hood, the recessed eye lit warm from within, the
   two unequal plates and the **notch of sky between them** as the key shape
   of the composition, seen from below and to the side, lower body out of
   frame. This is the chapter where the audience learns the head before the
   reveal shows them the whole body, so it has to read at a glance.
2. **The veil.** At the downbeat (`transition-024-downbeat`) the whole frame
   is filled with a brown haze and it reads as corruption, not dust. Make it
   local and lit: dense embers and a warm glow around the region being
   substituted, the outgoing structure still legible through it, over about
   a second. Give me a strip of twelve frames across ±0.5 s for transitions
   24 and 88 so I can see it as motion.
3. **Dither.** Required from this round (`briefs/2026-09-06-phase-dither-note.md`,
   PLANNING §4). The dawn sky in the reveal bands visibly at 3x; so do the
   Gouraud ramps on large faces and the floor's distance fade. Ordered Bayer
   on the per-pixel paths; error diffusion into the painted assets'
   palettes at conversion time.
4. **The ledger**, with the sizes Overscan measured from the ARM map
   (`briefs/2026-09-06-overscan-platform-reply.md` §6): `video.c` /
   `g_pages` 307,628; `audio_pwm.c` (`s_left`, `s_right`) 4,680; the synth's
   SRAM code (`render_block` 5,144 + `synth_render` 2,704) needs its own row;
   `r_bloom` 704; `synth.c` totals 46,856 (`g_rv_c` 15,000, `g_chorus`
   2,048); scanvideo's allocation is 10,368 at eight buffers, not 21,504.
   Measured heap on the renderer build is 61,616 B before that allocation,
   about 51 KB after. The boot floor is still to be measured on the device;
   no cut is ordered.
5. **The report** round three did not get to write: triangle and fill
   counts per chapter against the §8 ceilings, and what fought you.

## Also

- Use Overscan's capture path now (`make -C colossus/host` builds `capture`
  in WSL with plain gcc; `capture --out DIR --bars ...`) instead of your PPM
  dumper, so what you show me is what the platform draws.
- Do not edit PLANNING.md, song.c, synth.c, demo.h or Overscan's files.
  Report in `briefs/2026-09-06-phase-round4-reply.md`, stills in
  `briefs/sketches/round4/`.
