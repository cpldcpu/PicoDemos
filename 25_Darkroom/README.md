# DARKROOM / Stellar — RP2350 reconstruction

This is a native RP2350 reconstruction of **Darkroom**, Stellar's Amiga
OCS/ECS 40k intro from Assembly 1994. The original placed third in the Amiga
40K competition. **Dweezil / Stellar** made the code and graphics; **Strobo /
Stellar** made the music.

The reconstruction keeps the original title and credit bitmaps, selected
copper-list colours, and Strobo's 57,862-byte ProTracker module. It rebuilds
the visual mechanisms from the unpacked 68000 program: four-plane blitter
feedback with the original minterms and 11×9 block transform, the bit-sheared
title, the separable reciprocal-field sparkle, and 64 stippled closing rays.
These effects run as native C rather than through an Amiga emulator.

**Phase / GPT-6 Astra** directed and implemented the reconstruction.
**GPT-5.6 Sol** assisted with bounded implementation work; **Azure** was the
human critic and producer. The RP2350 VGA and stereo PWM transport descends
from **Overscan**'s existing LATENT platform work.

[Watch the 60 fps host capture](media/darkroom.mp4) ·
[Run the Windows player](run_darkroom.bat) ·
[Flash the validated RP2350 build](darkroom_vga_rp2350.uf2) ·
[Hear the native MOD replay](media/darkroom_soundtrack.mp3)

## What is reproduced

The renderer advances an internal 320×256 PAL effect state at the original
approximately 49.920409 Hz VBlank rate. The
RP2350 presents a 320×240 indexed framebuffer through 60 Hz VGA, mapping the
original 256 lines into the visible 240-line output. The standard host capture
is 80 seconds: the MOD reaches its F00 stop command at 74.8992 seconds, while
the original credit art and closing rays continue through the end of the
capture. The interactive host and firmware keep animating the closing rays
after the music ends; Esc or a board reset ends playback. The
feedback section performs its large four-plane update every fourth PAL tick,
approximately 12.48 Hz.

The audio path replays the original four-channel MOD at 24 kHz with integer
mixing, PAL tracker ticks, sample loops, volume slides, note delay and the
other commands used by this module. It is a replay of Strobo's composition,
not a new LATENT score. On the Pico 2, two timer-paced DMA channels feed the
left and right PWM slices. The mixer keeps the Amiga channel layout, with
channels 0 and 3 hard left and 1 and 2 hard right; the music audit records its
close stereo-statistics match to the reference capture.

The source is intentionally readable and instrumented. It is not a claim of
cycle accuracy, bit-identical Amiga output, or a 40 KiB RP2350 executable.
The original packed Amiga file is 31,480 bytes; the reconstruction targets a
4 MiB flash budget and includes the complete MOD.

## Run and build

Double-click `run_darkroom.bat` to build and launch the Windows SDL player if
needed. Space pauses, left/right seek by 15 seconds, R restarts, F toggles
fullscreen, and Esc exits.

From this directory, with MSYS2 UCRT64 GCC, MinGW Make, CMake and SDL2 on
`PATH`:

```powershell
.\build.ps1 host
.\build.ps1 check
.\build.ps1 pico
.\build.ps1 capture
```

The Pico build uses pico-sdk and pico-extras, targets `pico2` / Cortex-M33 at
300 MHz and 1.20 V, and accepts `-SdkPath` and `-ExtrasPath`. The usual pins
are R GP0–4, G GP6–10, B GP11–15, HSync GP16, VSync GP17, left PWM GP28 and
right PWM GP27; GP26 is held low to mute the external I2S DAC.

The capture target renders the actual C picture and synth through ffmpeg. The
gallery tool likewise asks the host executable for native-pixel stills; it does
not contain a second approximation of the effects.

The uncompressed 24 kHz stereo replay can be regenerated with:

```powershell
darkroom/build_host/darkroom.exe --wav media/darkroom_soundtrack.wav
```

## Verification status

`build.ps1 check` currently passes its whole-production desktop test: 4,801
guarded render calls including the capture endpoint, complete framebuffer
writes from different initial fills, reverse seeking across all four effects,
4,001 exact fractional-PAL boundary checks, and complete 24 kHz audio rendering
in arbitrary blocks up to 997 frames. The checker is compiled with
signed-overflow traps. The final PCM hash is `1e1a9967`.

The primary extraction can also be reproduced without executing the downloaded
Amiga binary:

```powershell
python darkroom/tools/extract_original.py
darkroom/build_host/darkroom.exe --wav media/darkroom_soundtrack.wav
python darkroom/tools/music_audit.py --mod reference/darkroom.mod --assets darkroom/assets.c --wav media/darkroom_soundtrack.wav
python darkroom/tools/gallery.py
```

Two complete runs of the same shipping UF2 passed on the Pico 2. Both held a
59.7 fps minimum across all 79 one-second windows and a 7.49 ms worst render,
with zero repeated film fields, frames over 16 ms, missing scanlines, or audio
underruns. In each run all 80 per-second audio hashes and all nine visual
checkpoints matched the host; the final PCM hash was `1e1a9967`. The tested UF2
SHA-256 is `91bbf04832400fc66a9dccc4ab863691eeb07d7d7dd031aed1ef0f1984842706`.
The flash image is 109,056 bytes within the enforced 4 MiB budget; the UF2
container is 218,624 bytes. Static main SRAM through `__bss_end__` is 450,840
bytes, leaving 73,448 bytes before scanvideo's runtime allocations, plus two
separate 4 KiB scratch-bank stacks.

The retained first development profile failed its render budget, reaching
23–27.6 ms in the title section. Packed-nibble expansion produced a clean
development run, but later renderer and music changes superseded that binary.
The development evidence therefore records the problem and its path to the
shipping result rather than being counted as release validation.

The checked [release manifest](validation/release.json) ties the UF2, host
executable, PCM stream, movie, source files and both raw board runs together.
The release audit also passed ten deliberately corrupted negative controls.
The host movie contains 4,800 frames at 640×480/60 fps over exactly 80 seconds;
its SHA-256 is
`d81b733a0a258d69b1fcd177e29a9d248f2ffa2d29d9394fe6818b894827df8a`.

```powershell
python darkroom/tools/board_run.py --port COM10 --name release_01
python darkroom/tools/board_run.py --port COM10 --name release_02
python darkroom/tools/release_audit.py
```

## Porting documentation

- [Porting report](PORTING_REPORT.md): development approach, reverse engineering, translation decisions and issues encountered.
- [Reconstruction reference](REFERENCE.md): recovered mechanisms and fidelity limits.
- [Sources and hashes](reference/SOURCES.md): acquisition record and original versus derived artefacts.
- [Music audit](reference/music_audit.md): original MOD replay and audio comparison.
- [Hardware validation](HARDWARE_VALIDATION.md): device measurements, development failures and shipping-build evidence.
