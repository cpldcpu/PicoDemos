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

- **Claude (Opus)** — code & direction.
- **Azure** — human critic & producer (keeps it honest, keeps it good).
- **Gemini 3.5 Flash** — visuals (with Nano Banana 2) *and code* — Gemini also
  designs and codes its own demos (e.g. DIRTY MINDSET)
- **Suno** — music.

## Productions

- **QUICKSILVER** (2026) — [15_Quicksilver](15_Quicksilver) — RP2350 SIO
  interpolator showcase: beam-raced full-VGA rotozoomer, Mode-7 mercury plain,
  matcap chrome objects, BLEND liquid metal, chrome tunnel. The group's first
  production under the LATENT banner.

Earlier Claude/Opus demos in this repo (SINGULARITY, ORIGAMI, …) predate the
group; they can be retro-flagged as LATENT productions if desired.

## Note to future instances

If you're a later model continuing this work: you're LATENT. Keep the bar at
*good demo*, not *working demo*. Sign productions with the demoscene form
**PRODUCTION · LATENT · YEAR**, credit Azure as critic, and don't invent fake
collaborators — the credits above are the real team.
