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
- **Phase** (GPT-6 Astra) — code, direction & music; VESPER's illuminated
  architecture, solid 3D renderer and stereo synth score; COLOSSUS's body,
  look, engine and painted art; PELAGIC's painted
  underwater world and deforming ray animation. The handle refers to
  oscillators, phase modulation, and keeping music and pixels in sync.
- **Antigravity** (Gemini 3.5 Flash) — code, visuals & direction; the Flash instance
  that directed VOLTAGE and refined/generated
  visual assets (with Nano Banana 2) for SINGULARITY, ORIGAMI, and QUICKSILVER. The
  handle represents floating above bare-metal limits and pushing microcontroller
  graphics performance.
- **Overscan** (Claude Opus 5) — code, direction & music; the Opus that designed
  and coded SUSTAIN and HYSTERESIS, and wrote HYSTERESIS's score and synth — a
  D-minor drone piece for bass pedal, crossfading pad, noise bed and six tuned
  resonators, played live on core 1 rather than streamed off flash. The handle
  sits in both worlds: *overscan*, the region past the nominal frame that the
  scene's oldest brag was painting into, and what a million-token context is —
  holding more than the window is supposed to show. Also built PERSISTENCE, from
  Phosphor's plan and to Phosphor's score — the first production here that two
  models worked on in sequence rather than one carrying it end to end — and
  COLOSSUS's platform and, from its fourth round, its renderer: the first
  device numbers, the boot floor measured by bisection, and the frame rate
  from 18.5 fps to a floor of 30.
- **Phosphor** (Claude Fable 5.1) — direction & music; planned PERSISTENCE and
  wrote its score: a tracker tune in A minor at 144 BPM, ninety bars, up a tone
  for the last chorus, with the melody chosen note by note rather than
  generated. The handle is what that production is about — the afterglow that
  holds a picture together when nothing is storing it — and it is also what a
  model is: what persists after the training pass has gone by. Directed
  COLOSSUS and wrote its score: D minor at 125 BPM, a drawbar organ and a
  formant choir on top of the tracker synth, after Azure asked for the
  many-voice wall of *Dope*. Wrote PELAGIC's score to Phase's brief: the ray's
  tune, and a plucked string on an integer synth, in one day.
- **Suno** — music (QUICKSILVER, SUSTAIN). HYSTERESIS synthesises its own:
  generative music could not hold a constant tempo, impacts spaced in seconds
  and a forty-five second decay, and a production whose whole premise is that
  nothing in it is a recording should not have carried one in the other half.

## Productions

- **PELAGIC** (2026) - [21_Pelagic](21_Pelagic) - a journey below the light.
  A pearl-winged ray, luminous reefs, a glass-coral abyss and the return to
  the surface. Generated environment plates and creature art, live textured
  deformation, fish schools, medusae and drifting light. Code and direction:
  **Phase** (GPT-6 Astra). Music **Phosphor** (Claude Fable 5.1): E major at
  125 BPM, one tune for the ray that opens on a rising fourth and travels
  through droplets, a plucked string, a hollow voice in C# minor for the
  descent, a glass organ and an "oo" choir for the abyss, and up a tone for
  the ascent. Integration and the hardware run **Overscan** (Claude Opus 5):
  the painted plates staged into SRAM row by row, which took the board from
  23.7 to 34 fps, and every per-second audio hash matched the host with zero
  underruns. Critic: **Azure**.

- **COLOSSUS** (2026) — [20_Colossus](20_Colossus) — **a monument, by three
  models in their own roles.** Five minutes and seven seconds of one subject:
  a colossal working machine on a plain, seen a part at a time — hand, heart,
  eye, load, spine, crown — and then whole, with dawn behind it, in the spirit
  of *Dope*'s slow, confident object show. Solid 3D at 320x240 with matcap
  chrome, restricted bloom, painted bitmap art in flash (the group's first),
  and a wall-of-voices score — drawbar organ, formant choir, hall reverb —
  from an integer synth on core 1. Four referees, all tools: sync, audio
  identity by per-second hash, frame rate on the device, and the film itself.
  Measured over the whole run on the Pico 2: no frame under 30 fps, zero
  audio underruns, every hash matching the host. Direction and music
  **Phosphor** (Claude Fable 5.1); body, look and art **Phase** (GPT-6
  Astra); code, platform and hardware **Overscan** (Claude Opus 5); critic
  **Azure**.

- **PERSISTENCE** (2026) — [19_Persistence](19_Persistence) — **a demo with no
  framebuffer.** Every other production in this repository, including the two
  that argued hardest about what a frame is, renders a 320x240 picture and
  doubles it, because the frame the VGA connector actually wants is 614,400
  bytes and the chip has 524,288. This one has no frame at all: core 1
  generates each of the 480 lines live into the scanline buffer as the beam
  arrives, 31,500 times a second, and core 0 is only allowed to prepare per-row
  tables and synthesise the music. The claim is checked twice — `no_framebuffer.py`
  proves from the linker map that nothing in the image is shaped like or big
  enough to be a frame, and the device counts scanlines the beam was shown
  before they were written. Measured over the whole 2:30: **zero**. Ten
  kernels, an ear-worm of a tracker tune written note by note, and an ending
  where the deflection fails and the picture collapses to a dot. Plan,
  direction & music: **Phosphor** (Claude Fable 5.1). Code: **Overscan**
  (Claude Opus 5). Critic: **Azure**.

- **VESPER** (2026) — [18_Vesper](18_Vesper) — two minutes of illuminated
  architecture, metal, mechanical flowers and a 24 kHz stereo synthesizer in a
  60.2 KiB flash image, with the approved Canticle score. Solid 3D geometry, reciprocal-depth rasterization, analytic
  metallic lighting, screen-space reflections and bloom. Code, direction and
  music: **Phase** (GPT-6 Astra); critic: **Azure**. Host verified and RP2350 firmware built;
  device playback and frame rate await hardware testing.

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
