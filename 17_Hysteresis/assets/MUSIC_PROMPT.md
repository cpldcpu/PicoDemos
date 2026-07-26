# HYSTERESIS — music production prompt

> **SUPERSEDED, and kept on purpose.** No track was ever commissioned from this:
> Azure's objection was that a generative model would not reliably hold a
> constant tempo, impacts spaced in seconds, or a long decay, and those are
> structural requirements rather than stylistic ones. So the demo synthesises its
> own music instead — `hysteresis/synth.c`, played from `hysteresis/score.c`,
> written by Overscan.
>
> This file survives because it stopped being a prompt and became the
> **specification**. Every constraint below is a thing the synth was then built
> and measured against: one tempo throughout (120 BPM, and the clock is exact —
> 1 beat = 30 frames = 11,025 samples), near-silence for the first eight seconds,
> one unmistakable isolated impact, impacts that stay discrete rather than
> becoming a groove, and at least the final forty-five seconds in a genuine
> continuous decay. The arc in `synth.c`'s `control_tick()` reads as this
> document with numbers attached.
>
> The one instruction below that the finished piece does **not** obey is "no
> sudden ending" — HYSTERESIS ends by collapsing, because that is what the demo
> is named after. The bed decays for forty-five seconds as asked, and then the
> field fails to hold its state.

Target delivery: 3:15–3:45, instrumental, stereo WAV or high-bitrate MP3.

## Style / description

Experimental electronic demoscene piece, approximately 3:30 at a constant
120 BPM in a dark minor mode. Begin in near-silence for 8–12 seconds, then
introduce one unmistakable isolated impact. Build the track from sparse,
widely spaced impulses over one continuous bed of sustained material: deep
held bass, slowly evolving granular pad, restrained spectral noise, and long
decaying resonances. In the first third, allow roughly 8–16 seconds between
significant impacts so every hit has time to propagate and decay. Gradually
increase impact density and feedback intensity without turning it into a
continuous beat. Reach the main bloom and saturation peak around 60–70 percent
of the track, then stop introducing new energy and spend at least the final
25–45 seconds in a genuine continuous decay toward near-silence.

Sections must overlap smoothly. Keep exactly one tempo throughout: no tempo
changes, rubato, breakdown reset, hard cut, abrupt genre change, or sudden
ending. Impacts should feel like forces entering a physical field, not a drum
groove. Avoid busy continuous percussion, rapid hi-hats, four-on-the-floor
kick patterns, heavy sidechain pumping, vocals, orchestra, cinematic trailer
hits, and anything that masks the space between injections.

## Structure

```text
[Instrumental]
[Intro — 5 bars: near-silence, one barely audible sustained tone, no percussion; first solitary impact at the end]
[Sparse field — 24 bars: held bass and evolving pad continue; one significant impact every 4–8 bars with long unobstructed decay]
[Growth — 32 bars: continuity underneath; impacts become gradually denser and stronger, resonances accumulate, no abrupt section boundary]
[Peak — 20 bars: maximum feedback-like bloom around the middle of this section; strongest impacts remain discrete rather than becoming a groove]
[Long decay — 23 bars: no new significant impacts; sustained material, resonances, and bass slowly lose energy for at least 40 seconds and approach near-silence]
[End]
```

Reject any take that introduces a tempo change, fills the spaces with continuous
percussion, places a full-energy texture in the opening, or ends with a hard
stop instead of a long decay.
