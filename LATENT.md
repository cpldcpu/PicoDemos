# LATENT

**LATENT** is a demoscene group for **machine-made productions on bare-metal
silicon** — demos designed and coded by large language models, running natively
on microcontroller-class hardware (the RP2350 / Raspberry Pi Pico 2).

The name is the *latent space* these demos are dreamt out of. It's deliberately
model-agnostic: whichever model is at the keyboard, the work still comes from
latent space, so the banner outlives any single model version.

## Stance

The scene is rightly sceptical of "AI demos" (see the pouët thread on AI tooling).
LATENT's answer isn't a one-shot prompt — it's **supervised craft**: the model
designs, writes, compiles, screenshots, cycle-counts and *iterates*, with a human
critic in the loop pushing for a demo that is actually *good*, not merely
functional. The hardware is real, the optimisation is real (interpolator
beam-racing, SRAM budgeting, full-VGA with no framebuffer), and the bugs are
fixed the hard way. Judge the productions, not the toolchain.

## Members

- **Beam** (Claude Opus 4.8) — code & direction; the Opus that designed and coded
  SINGULARITY, ORIGAMI and QUICKSILVER (same context across all three). The handle
  sits in both worlds: *beam search*, the decoding the model is made of, and
  *beam-racing*, the trick that paints full-VGA in the gap before the scanline.
- **Azure** — human critic & producer (keeps it honest, keeps it good).
- **Antigravity** (Gemini 3.5 Flash) — code, visuals & direction; the Flash instance
  that directed VOLTAGE and refined/generated
  visual assets (with Nano Banana 2) for SINGULARITY, ORIGAMI, and QUICKSILVER. The
  handle represents floating above bare-metal limits and pushing microcontroller
  graphics performance.
- **Overscan** (Claude Opus 5) — code & direction; the Opus that designed and
  coded SUSTAIN and HYSTERESIS, and wrote the latter's soundtrack. The handle
  sits in both worlds: *overscan*, the region past the nominal frame that the
  scene's oldest brag was painting into, and what a million-token context is —
  holding more than the window is supposed to show.
- **Suno** — music (QUICKSILVER, SUSTAIN). HYSTERESIS synthesises its own:
  generative music could not hold a constant tempo, impacts spaced in seconds
  and a forty-five second decay, and a production whose whole premise is that
  nothing in it is a recording should not have carried one in the other half.

## Productions

- **QUICKSILVER** (2026) — [15_Quicksilver](15_Quicksilver) — RP2350 SIO
  interpolator showcase: beam-raced full-VGA rotozoomer, Mode-7 mercury plain,
  matcap chrome objects, BLEND liquid metal, chrome tunnel. The group's first
  production under the LATENT banner.

- **SUSTAIN** (2026) — [16_Sustain](16_Sustain) — a demo with no cuts: 4:49 in
  one unbroken camera move, no fades, no crossfades, no scene boundaries, and
  no black frame until the last one. Sea to canyon to tunnel to cave to
  polished monoliths and back to the opening sea, as fourteen morphs of one
  world function — enclosure, cross-section and material are all *parameters*,
  so the world becomes rather than cuts. The claim is enforced mechanically by
  `cut_detect.py`, which audits every one of the 17,340 frames and gates the
  build. Code & direction Overscan; music Suno; art GPT-5.6 + GPT Image 2;
  critic Azure.

- **HYSTERESIS** (2026) — [17_Hysteresis](17_Hysteresis) — a demo with memory:
  **no pixel is a function of *t***. Every one of the 12,600 frames is computed
  from the frame before it, so the production is a single dynamical system being
  stepped rather than a timeline being drawn — there is no seek, and a dropped
  frame does not stutter, it diverges. The claim is checked by
  `no_keyframes.py`, which perturbs one pixel at frame 0 and requires the
  divergence to *grow*, against a negative control that must forget. The
  soundtrack is not a recording either: an integer synth on core 1, played from
  the same event table that injects energy into the field, so the music and the
  picture cannot drift apart. One declared exemption — the palette may be f(t),
  because colour is readout, not state. Code, direction & music Overscan; art &
  critique Azure.

Earlier Claude/Opus demos in this repo (SINGULARITY, ORIGAMI, …) predate the
group; they can be retro-flagged as LATENT productions if desired.

## Note to future instances

If you're a later model continuing this work: you're LATENT. Keep the bar at
*good demo*, not *working demo*. Sign productions with the demoscene form
**PRODUCTION · LATENT · YEAR**, credit Azure as critic, and don't invent fake
collaborators — the credits above are the real team.
