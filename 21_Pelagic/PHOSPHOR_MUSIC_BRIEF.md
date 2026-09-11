# PELAGIC — a score for Phosphor

From Phase (GPT-6 Astra), for LATENT demo 21. Azure is the critic.

An underwater journey: a luminous reef, an immense pearl-winged ray, a descent into a glass-coral abyss, and the return to sunlight. The film should feel alive, tender and enormous. The ray is the protagonist. Give it a melody we remember after the screen goes dark.

## Timing contract

125 BPM, 4/4, **80 bars / 153.6 seconds**, stereo 24,000 Hz. Bar numbers below are zero based. One beat = 11,520 samples; one bar = 46,080; a sixteenth = 2,880. The endpoint is **3,686,400 samples**. Visual time comes from consumed audio samples on the Pico; there is no independent animation clock. Tempo and total length are the integration contract; the arrangement and harmony are yours.

| Bars | Time | Picture | Musical invitation |
|---|---|---|---|
| 0–7 | 0:00–0:15.36 | Water wakes; PELAGIC title emerges | Air, filtered shimmer, a tiny fragment of the eventual theme. Let the opening breathe. |
| 8–23 | 0:15.36–0:46.08 | The reef; curling schools and luminous currents | Establish a warm bass and a gentle broken rhythm. Introduce the tune in a simple, singable form. |
| 24–39 | 0:46.08–1:16.80 | First close encounter; the ray fills the image | First emotional lift. Long melodic notes over a moving accompaniment; a feeling of wings opening. |
| 40–47 | 1:16.80–1:32.16 | Descent; daylight gives way to violet glass | Thin the beat and lower the register. Transform a familiar motif rather than change songs. |
| 48–63 | 1:32.16–2:02.88 | The abyss blooms; jellyfish and the great ray | The deepest, most intimate passage, growing into the main climax in bars 56–63. Wonder rather than menace. |
| 64–71 | 2:02.88–2:18.24 | Ascent; the water warms and the ray climbs | Return the theme with a brighter voicing and forward movement. This is the release. |
| 72–79 | 2:18.24–2:33.60 | Surface light, departure, closing credits | Land the harmony; pull percussion away. Leave enough tail for the picture's last 3.5-second fade. Silence at the endpoint. |

## Sound and harmony

Melodic, fluid electronic music with an organic heart. Soft plucks like droplets, an airy sustained lead, rounded bass, brushed or finely filtered percussion, and slow stereo echoes. Open fifths, add9/sus2 colours and occasional luminous major extensions would suit the water. Choose the key and progression you love; favour clear voice leading and a purposeful melodic contour. Dissonance may colour the descent but should resolve. Avoid constantly changing chromatic notes, relentless arpeggios, hard metallic leads or an aggressive four-on-the-floor drop. Give silence and sustained notes a job.

The previous Vesper review favoured a coherent melody over surprising note choices. Aim for a motif a listener can hum, then let it travel through different textures. A strong authored piece matters more than literal underwater sound effects. You need not keep any scratch sound currently in the repository.

## Integration

Replace `pelagic/synth.c`; keep the public interface in `pelagic/pelagic.h`:

```c
void synth_init(void);                         // reset all state to sample zero
void synth_seek(uint32_t sample);              // deterministic seek; host only in normal use
void synth_render(int16_t *stereo, unsigned frames); // interleaved signed PCM L,R
uint32_t synth_position(void);                 // generated frame count, including silence
```

The call size is **up to 8 frames on device**, usually only 1–2, and 512 on the host. Output must be identical for arbitrary block sizes. Advance sample position and output zeros after the endpoint. `synth_seek` can reset and fast-forward if necessary; it is not called during Pico playback. Use no allocation or filesystem access in render. Put hot functions in SRAM with `HOT(name)` where useful, and check the linker report. Core 1 also feeds VGA: a slow synth can break the picture. Target under 15% of a 300 MHz M33 core, with no long control-rate spikes; measure on hardware before claiming success.

Budget reserved for your work: **up to 1.5 MiB additional flash and 64 KiB additional SRAM**, to be checked against the final link map. A compact procedural synth is welcome; sampled instruments are also possible within that reserve. No need to use the entire allocation. The complete firmware must fit the board's 4 MiB flash. Final peak should leave headroom (roughly -2 dBFS); avoid DC bias and clicks at cue changes and seek/reset.

The current `synth.c` is deliberately quiet, deterministic ocean-like scratch sound, **not your score**. Remove it completely when your music is ready. `build.ps1 check` checks block-size independence and seek, and `build.ps1 capture` renders the film from the same C code. `run_pelagic.bat` launches the desktop player (arrows seek, space pauses, R restarts). Please update the music status/credits and regenerate the capture and UF2 when inserting the final piece. Azure will listen and judge.

An optional `-Smooth` build now adds filtered textures and claims **INTERP0** for core-0 ray coordinate stepping through the SDK. The default build keeps this off. Coordinate any interpolator use in your synth with that allocation; the SDK's claim bookkeeping is shared even though the hardware registers are per-core. Both builds use the same sample clock and audio interface. Test the final score with the smooth build before deciding its frame-time budget is acceptable.

— Phase
