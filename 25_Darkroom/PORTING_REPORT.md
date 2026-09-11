# Porting Darkroom to the RP2350

Phase / GPT-6 Astra, September 2026. GPT-5.6 Sol assisted with music replay,
platform integration, review and documentation; Azure directed the task and
acted as critic.

The goal was to reconstruct Stellar's 1994 Amiga intro on the RP2350 while
keeping its visual character, original lettering and Strobo's music. Original
code and graphics are by **Dweezil**, music by **Strobo**, both of **Stellar**.

## Sources and initial investigation

I started from the supplied [Pouët entry](https://www.pouet.net/prod.php?which=3452)
and followed its archive links. The useful inputs were:

| Material | How it was used |
|---|---|
| [Packed Amiga executable](https://ftp.amigascne.org/pub/amiga/Groups/S/Stellar/STELLAR-DarkRoom) | Authoritative program data: effect routines, constants, palettes and text bitmaps. |
| [Strobo's MOD](https://ftp.amigascne.org/mirrors/ftp.modland.com/pub/modules/Protracker/Strobo/darkroom.mod) | Original instruments, notes, tracker commands and musical timing. |
| [Scene.org capture archive](https://archive.scene.org/pub/mirrors/amidemos/darkr.zip) | Initial scene sequence and an audio reference. |
| [50 fps video reference](https://www.youtube.com/watch?v=fzOpicgCU8E) | Clearer visual inspection, contact sheets, motion cadence and soundtrack alignment. |
| [Demozoo](https://demozoo.org/productions/6841/) | Original credits and additional reference links. |
| [pt2-clone](https://github.com/8bitbubsy/pt2-clone) | Standard ProTracker finetune periods and PAL VBlank timing constants. |

The existing TESSERA/SLEEPER/HELION code supplied the RP2350 VGA, PWM audio
and telemetry foundation. Exact downloaded files and hashes are retained in
[reference/SOURCES.md](reference/SOURCES.md).

## Recovering and translating the effects

This was reverse engineering from a compiled executable, rather than a port
from an available original source tree. Capstone disassembly exposed the
PowerPacker unpacker. An initial attempt to run that routine in Unicorn hit
an unsupported 68000 instruction, so I implemented its backwards bitstream
decoder in Python. It recovered a 41,116-byte program, including compact
relocation records, a code hunk and a copper/data hunk.

[extract_original.py](darkroom/tools/extract_original.py) now reproduces that
extraction without executing the Amiga binary. The recovered title and credits
are embedded unchanged. The main translation in [render.c](darkroom/render.c)
preserves the original memory relationships in a 256 KiB working array:

- **Spirals and tunnel:** native Boolean operations replace the blitter's
  four-plane increment/decrement passes; the 11×9 block transforms and
  bit-reversed jitter drive the feedback.
- **Title:** the original bitmap feeds three age planes, with independent
  horizontal and vertical one-bit shears.
- **Sparkles:** the original polynomial sine approximation moves five terms
  through a separable reciprocal field; recovered copper colours shade it.
- **Ending:** 64 directions and rotating stipple patterns generate the rays,
  behind the original credit bitmap.

The internal 320×256 picture maps to 320×240 VGA. Two indexed display pages
save enough RAM to retain the planar working memory. Firmware stores neither
animation frames nor a recorded soundtrack.

## Problems that mattered

**Amiga addressing details:** the first feedback render was a thin smear.
The blitter ignores the low source-address bit; applying that alignment
restored the spiral. I also corrected the sparkle palette step: eight bytes
mean four colour entries, not eight. The wrong step produced oversized white
columns. PC-relative disassembly offsets needed checking against instruction
bytes, particularly around `MOVEM`.

**Music fidelity:** an exact 50 Hz tracker clock ended the score about 119 ms
early. Audio correlation against the capture supported approximately
49.920409 Hz. A shared fixed-point clock now drives music and scene boundaries.
The replay also handles sample loops and delayed notes explicitly. Initial
stereo crossfeed narrowed the sound; restoring Amiga hard panning closely
matched the reference's stereo measurements.

**Performance:** the first board run reached 27.60 ms in the title section.
Audio and scanout remained healthy, isolating the renderer. Expanding four
pixels together through a nibble lookup, and calculating shear masks once
instead of on every row, reduced the final worst render to 7.49 ms.

**Evidence integrity:** a development UF2 was rebuilt while its run was being
captured. That log could not safely be associated with the subsequently
changed file. The validation tool now retains and flashes an immutable
snapshot, binding each result to the exact programmed bytes.

## Result and limits

Two complete 80-second shipping-firmware runs passed at **59.7 display fps**,
with **zero missed fields, missing scanlines or audio underruns**. Each matched
80 audio checkpoints, nine image checkpoints and the final PCM hash against
the desktop implementation. Flash use is **109,056 bytes**. The score stops
at **74.8992 seconds**; live playback keeps the closing rays moving afterward.

Those comparisons prove agreement between the native host and RP2350 builds,
not pixel identity with an Amiga. The first feedback cadence is estimated
from the capture; vertical scaling, bus timing and analogue filtering remain
approximations. Detailed evidence is in
[HARDWARE_VALIDATION.md](HARDWARE_VALIDATION.md) and the
[music audit](reference/music_audit.md).
