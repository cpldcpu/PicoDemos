# DARKROOM reconstruction notes

## Original production

**Darkroom** is a Stellar Amiga OCS/ECS 40k intro released on 7 August 1994.
It placed third in the Assembly 1994 Amiga 40K Intro competition. Demozoo's
credit record names **Dweezil** for code and graphics and **Strobo** for music.

- [Pouët production page](https://www.pouet.net/prod.php?which=3452)
- [Demozoo production and credits](https://demozoo.org/productions/6841/)

Those are credits for the 1994 Stellar work. LATENT's contribution is the
2026 native reconstruction described in this repository.

## Primary material

The reference directory retains three independently useful primary inputs:

- `STELLAR-DarkRoom`, the 31,480-byte packed Amiga executable from the
  amigascne archive;
- `darkroom.mod`, Strobo's 57,862-byte ProTracker module from the Modland
  mirror at amigascne;
- `darkr.zip`, the scene.org mirror package containing `file_id.diz` and a
  low-quality `darkr.avi` capture.

A separate 50 Hz video download is retained as `reference_50hz.mp4`. It is a
visual and timing reference, not an authoritative dump of Amiga framebuffer
states. Exact URLs and SHA-256 hashes are recorded in
[reference/SOURCES.md](reference/SOURCES.md).

## Static analysis

The original executable was PowerPacker-compressed. The read-only
`darkroom/tools/extract_original.py` script reproducibly produces
`unpacked.bin` (41,116 bytes). Its code hunk is retained as `hunk0.bin`
(38,952 bytes), and the 1,884-byte copper/data hunk as `hunk3.bin`.
`disassembly.txt` and `relocations.json` are analysis derivatives. The hunk
layout declares a 4,096-byte scratch hunk and a 262,144-byte chip-memory hunk
in addition to code and copper/data.

The address comments in `darkroom/render.c` refer to offsets in the unpacked
code hunk. The implementation translates the observed mechanisms rather than
executing 68000 instructions:

- the feedback section uses the original blitter Boolean minterms, alternating
  four-plane saturating increment/decrement, bit-reversed jitter, and the
  11×9 block displacement maps;
- the title section injects the original bitmap one row per PAL tick, then
  applies independent horizontal and vertical one-bit shears through three age
  planes;
- the sparkle section builds the original polynomial sine approximation and a
  separable reciprocal field with five moving terms on each axis;
- the closing section uses the 64 recovered directions and rotating 16-bit
  stipple patterns, then overlays the original credits bitmap.

`assets.c` embeds the recovered 1,280-byte title bitmap, 1,520-byte credits
bitmap, 364 bytes of original copper/data, and the unmodified 57,862-byte MOD.
The renderer maintains a 256 KiB simulated chip-memory array so the bitplane
operations retain their original address relationships.

## Timing and presentation

The visual and tracker clocks use the PAL VBlank duration recorded by
pt2-clone, approximately 49.920409 Hz, represented in 2.31 fixed point. At
24 kHz the order-derived section boundaries are samples 799,993, 1,199,990 and
1,599,986: 33.333042, 49.999583 and 66.666083 seconds. The module's F00 command
falls at sample 1,797,581, or 74.899208 seconds. The 80-second capture then
continues the animated closing rays behind the credits. Interactive and device
playback keep that closing effect running after the capture window. The first
section performs its large four-plane
feedback update every fourth PAL tick, approximately 12.48 Hz; later effect
state advances on every PAL tick.

The original logical picture is 320×256. The release transport is standard
320×240-at-60-Hz VGA, with vertical mapping from the PAL state. Repeated VGA
fields re-present the current effect state between PAL updates.

`darkroom/tools/music_audit.py` independently checks the order timing, F00
location and saved replay outputs. `capture.py` and `gallery.py` invoke the
actual host executable for their pictures and audio rather than maintaining a
separate visual or musical implementation.

## Fidelity boundary

This is a mechanism-level recreation. It preserves original assets and ports
the effect logic that was identified in the executable, but it does not model
Amiga bus contention, exact blitter cycle timing, analogue video, Paula's full
filter response, or every undocumented machine detail. The 24 kHz integer MOD
player implements the commands exercised by this module and applies an
A500-like low-pass. It preserves Amiga hard panning and adds no synthetic
crossfeed.

There are no stored original animation frames and no recorded original audio
stream in the firmware. Visuals are generated live from translated algorithms;
audio is generated live from the original module data.

## Release evidence

The checked [release manifest](validation/release.json) records two complete
runs of the same UF2 on the Pico 2. Each run checked 80 per-second audio hashes
and nine visual hashes against the host, with zero film repeats, frames over
16 ms, missing scanlines or audio underruns. Both reported a 59.7 fps minimum
and 7.49 ms worst render. The manifest also binds the executable, PCM, source,
UF2 and 80-second host movie hashes and records ten rejected negative controls.

The boot priming sequence repeats one field before the timed presentation; the
release logs count it separately as `boot 1`. The zero-repeat claim applies to
the complete 80-second measured presentation.

## Reconstruction credits

- **Phase / GPT-6 Astra:** reconstruction direction and implementation,
  reverse-engineering translation, renderer and MOD replay.
- **GPT-5.6 Sol:** implementation assistance on bounded components.
- **Azure:** human critic and producer.
- **Overscan / Claude Opus 5:** pre-existing LATENT VGA/PWM transport lineage.
