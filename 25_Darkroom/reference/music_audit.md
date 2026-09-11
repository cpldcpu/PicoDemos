# Darkroom music replay audit

The score is Strobo's original 1994 Darkroom MOD for Stellar. The RP2350 replay implementation is by Phase; it does not replace or rearrange the music.

## Source integrity and module flow

`darkroom.mod` is 57,862 bytes with SHA-256 `fdf3ed2fb0184ff66b6647c0dbebc5efa236d901e4603aa036fad5b2018105ce`. The byte array in `darkroom/assets.c` matches it exactly. The played order list is `2,3,4,4,0,0,1,5,6` over seven stored patterns.

The played rows contain only the effects needed by the native replay: `A` (3), `C` (493), `E1` (2), `ED` (2), and `F` (576). Every row has one speed command: `F08` on even rows and `F05` on odd rows, except the final `F00` at order 8, row 63. The two delayed notes are both `ED4`.

All non-trivial sample loops end at the physical sample end in this particular MOD. The replay nevertheless has to remember that Paula switches from the initial full-sample pass to a `loop start + loop length` endpoint; comparing every later pass with the physical length is incorrect for general ProTracker data. It also keeps the selected instrument separate from the sample currently playing so `EDx` does not replace active DMA early.

The native mixer uses the original Amiga channel layout: channels 0 and 3 are hard left, and channels 1 and 2 are hard right. No synthetic crossfeed is added. From 0.5 through 74 seconds, its left/right correlation is 0.0168 and its side/mid RMS ratio is 0.9822; the MP4 reference measures 0.0200 and 0.9818. The earlier one-third crossfeed measured 0.6204 and 0.4912, so it was removed. Per-channel music gain is otherwise unchanged. The capture's left/right balance varies between the AVI and MP4 encodes, so it does not justify altering Strobo's module volumes.

The integer 4.4 kHz one-pole low-pass remains an approximation of the enabled Amiga 500 output filter. It gives the intended softened chip-sample sound and remains bit-identical on host and RP2350, but it is not a transistor-level model of the original analog circuit or capture chain.

## Original clock and endpoints

The original replay is VBlank timed at 49.920409 Hz. At 24 kHz, its deterministic fixed-point tick length is `0x029067A6 * 24000 / 2^31` samples. One 64-row order is 416 ticks and 8.333250 seconds.

| Event | Tick | 24 kHz sample | Time |
|---|---:|---:|---:|
| Order 4 starts | 1664 | 799,993 | 33.333042 s |
| Order 6 starts | 2496 | 1,199,990 | 49.999583 s |
| Order 8 starts | 3328 | 1,599,986 | 66.666083 s |
| Final `F00` | 3739 | 1,797,581 | 74.899208 s |

An idealized 50 Hz replay reaches `F00` at 74.780000 seconds, 119.208 ms early. `ffmpeg`/libopenmpt reports 74.94 seconds for the module and emits a 75.04-second WAV; decoder end and fade policy make that duration unsuitable as the authoritative `F00` boundary.

## Reference capture alignment

The audio in `reference_50hz.mp4` is the audio from `darkr.avi` delayed by 0.900 seconds, with no detectable drift. Cross-correlation of short, high-passed energy windows from the native PAL replay against the MP4 gives the same 0.723-second offset at every tested point from 12 through 63 seconds. The MP4's initial capture material ends and the long black demo opening begins at 0.700 seconds, so the music starts about 23 ms after that transition. Its soundtrack ends near 75.622 seconds in the MP4 (`0.723 + 74.899`); the remaining track is capture noise.

Repeating the comparison with an ideal 50 Hz render produces a steadily growing offset, about 0.751 seconds at module time 12 seconds and 0.826 seconds at 63 seconds. The fitted rate is 49.9207 Hz, independently confirming the PAL VBlank clock.

## Reproduction

Run the structural, embedding, timing, and PCM checks from the project directory:

```powershell
python darkroom/tools/music_audit.py --mod reference/darkroom.mod --assets darkroom/assets.c --wav media/darkroom_soundtrack.wav
```

The audited native capture is 24 kHz stereo signed 16-bit PCM. Its expected whole-file replay hash is `1e1a9967` and its WAV SHA-256 is `61649376ebaf2dfb72bbb6552d333209369b817b2374af28c68cd2a04fcc4bec`; signal above 8 LSB ends at 74.899250 seconds, immediately after `F00` and the output filter's decay.
