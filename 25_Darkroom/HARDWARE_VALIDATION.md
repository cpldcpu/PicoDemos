# DARKROOM hardware validation

Validated on 2026-09-11 with an RP2350 A2 (`pico2`, ARM Cortex-M33), pico-sdk 2.2.0, a 300 MHz system clock at 1.20 V, VGA scanout, stereo PWM audio, and USB CDC telemetry on COM10.

The retained files and the reason for keeping each group are listed in the [validation evidence index](validation/README.md).

Two complete runs of the shipping UF2 passed. Every run flashed and verified a retained immutable UF2 snapshot with `picotool load -F -v`. Both snapshots and the distributed UF2 have SHA-256 `91bbf04832400fc66a9dccc4ab863691eeb07d7d7dd031aed1ef0f1984842706`.

| Run | Frames | Worst render | Minimum fps | Repeats | Over 16 ms | Missing lines | Underruns | PCM hashes | Image hashes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| [release_01](validation/release_01.log) | 4,781 | 7.49 ms | 59.7 | 0 | 0 | 0 | 0 | 80/80 | 9/9 |
| [release_02](validation/release_02.log) | 4,781 | 7.49 ms | 59.7 | 0 | 0 | 0 | 0 | 80/80 | 9/9 |

The per-second PCM hashes cover the complete 1,920,000-frame stereo stream at 24 kHz. Both runs end with PCM hash `1e1a9967`, matching the desktop player. Full RGB555 framebuffer hashes at 0, 10, 20, 30, 40, 50, 60, 70, and 80 seconds also match the current desktop renderer. The device reports one repeated boot field before the measured film is armed; it is kept separate from the zero measured repeats above.

The production preview and measured release window are 80 seconds. The source MOD ends at 74.899 seconds. After `DONE` and `FINAL`, the installed production keeps the closing rays animated from elapsed wall time until the board is reset; this continuing display is outside the fixed release measurement and movie.

## Board-driven performance work

The first development image exposed the cost of direct per-pixel bitplane expansion. Stage 0 alternated between 14.56 ms and 20.44 ms frames and rendered only 46.8–47.8 fps; stage 1 reached 27.60 ms and 29.8 fps. The 40-second log records 624 repeated fields and 624 frames over budget, while audio underruns and missing scanlines remained zero. This isolated the problem to renderer work rather than VGA or audio transport.

The final renderer expands four indexed pixels at a time through a nibble lookup and hoists the warp selection masks out of the 256-row loop. The final board cost is 7.49 ms at the heaviest feedback update, about 2.47 ms in the warp, 5.07 ms in the sparkle, and 2.74 ms in the closing rays. The worst audio pump across the release runs is 19.86 microseconds. The ring fill floor is 986 of 1,024 frames.

Development profiles are retained in `validation/` and are not counted as release passes. In particular, `development_optimized_01` predates the final PAL timing and audio changes, and `development_03` predates the final sparkle palette correction.

## Release bounds and reproducibility

| Item | Bytes |
|---|---:|
| Flash image | 109,056 |
| UF2 container | 218,624 |
| Static main SRAM through `__bss_end__` | 450,840 |
| Heap before scanvideo runtime allocation | 73,448 |
| Two stacks in separate scratch banks | 8,192 |
| 80-second host movie | 17,119,959 |

The release audit parses every UF2 block and enforces an RP2350 ARM image wholly inside a 4 MiB flash budget. It reads linker symbols from the final map and requires at least 64 KiB of main SRAM headroom before scanvideo allocation. The connected board reports 16 MiB installed flash; the smaller 4 MiB bound is the production constraint.

The host movie is 4,800 frames at 640x480, 60 fps, with stereo 24 kHz audio. Its SHA-256 is `d81b733a0a258d69b1fcd177e29a9d248f2ffa2d29d9394fe6818b894827df8a`. It is a host capture, not an analogue VGA capture. The nine framebuffer hashes are sampled comparisons rather than a checksum of every displayed hardware frame, and no external analogue audio measurement was made.

[`release_audit.py`](darkroom/tools/release_audit.py) binds both logs to the exact programmed snapshots, regenerates the current host image and PCM references, checks media properties, enforces flash and SRAM limits, and verifies ten negative controls. Truncated logs, wrong visual hashes, wrong PCM hashes, repeated fields, and missing scanlines must all be rejected. Exact firmware, host, media, and source hashes are recorded in [`release.json`](validation/release.json).
