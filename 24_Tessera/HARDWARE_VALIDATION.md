# TESSERA hardware validation

Phase / GPT-6 Astra, 2026-09-11.

Two complete shipping-UF2 runs: **59.7 fps in every timing window**, **12.97 ms worst render**, **zero repeated film fields, zero missing scanlines and zero audio underruns**. All **306/306** per-second PCM hashes, both complete-score hashes and **12/12** fixed-frame image hashes match the desktop.

RP2350 A2, COM10, pico-sdk 2.2.0, Cortex-M33 at 300 MHz / 1.20 V.
Every run programs and verifies flash with `picotool load -F -v` before
rebooting. The connected board reports 16 MiB installed flash; the release
audit separately enforces the production's **4 MiB** budget.

UF2 SHA-256: `63aa9098f07ccbbfdbc47086638288b0c4af265f2349cd17c648335fd7417622`.

| Run | Worst ms | Min fps | Repeats | Over 16 ms | Missing lines | Underruns | Second hashes |
|---|---:|---:|---:|---:|---:|---:|---:|
| [development_01](validation/development_01.log) | 20.46 | 29.8 | 262 | 418 | 0 | 0 | 152 |
| [development_02](validation/development_02.log) | 10.42 | 59.7 | 0 | 0 | 0 | 0 | 153 |
| [development_art_01](validation/development_art_01.log) | 15.06 | 59.7 | 0 | 0 | 0 | 0 | 153 |
| [release_01](validation/release_01.log) | 12.96 | 59.7 | 0 | 0 | 0 | 0 | 153 |
| [release_02](validation/release_02.log) | 12.97 | 59.7 | 0 | 0 | 0 | 0 | 153 |
| [reference_01](validation/reference_01.log) | 16.53 | 58.7 | 1 | 61 | 0 | 0 | 153 |

Development failures are retained as evidence, not counted as release
passes. The earlier pre-art release runs are in `validation/pre_art/`.
The previous card-layout release is archived in `validation/before_typography/`.

The optional software reference uses CPU background copies and software
texture addressing. Its complete run matches all six image checks, all
153 second hashes and the final PCM hash. It reaches **16.53 ms**,
with **1 repeated field**; its strict production gate
result is **FAIL**. It is a diagnostic fallback.
The accelerated UF2 is the compo build: **12.97 ms** worst, with no
repeated fields in either run.

The current title and credits cards measure visible glyph bounds to center
each line and size the background with 12-pixel horizontal and 8-pixel
vertical padding. The credits fit above the footer. Pixel measurements are
in [typography_review.json](validation/typography_review.json); the complete
device runs above validate this revised renderer and its updated image hashes.

## Fixes driven by the board

1. The initial rasterizer reached **20.46 ms** and repeated **262 fields**.
   It tested three edges and divided on every triangle row. Sorted edge
   walkers calculate slopes once. Settled surfaces avoid redundant morph
   evaluation; each tile shares its centre between four vertices. Shadow
   spans no longer calculate texture coordinates.
2. The ending enlarged 384 almost coincident tiles. It now gathers first,
   then moves closer to the surviving tile. This fixes overdraw through
   choreography without dropping geometry from the main scenes.
3. Ordinary DMA reads of the paintings churned the XIP cache: **15.06 ms**
   worst render and **181.32 us** worst synth pump. Projection moved to SRAM,
   and DMA now reads the dedicated **XIP streaming FIFO via XIP_AUX**, paced
   by `DREQ_XIP_STREAM`. The before/after image checkpoints are identical.
4. A **9,600-byte shadow union mask** blends the painted floor once per
   covered pixel. Overlapping shadows neither erase the artwork nor darken
   it repeatedly.
5. Windows sometimes purged BOOT while opening CDC. A bounded 300 ms
   settling delay preserves the complete startup evidence. The abandoned
   partial capture remains in `development_stream_boot.log`.
6. The first log omitted the final per-second hash. An explicit tail latch
   now records it before DONE. A separate hash covers the final partial
   second too. The old received hashes were correct; coverage was incomplete.

## Scope of the proof

The shipping runs draw 9,179 and 9,178 frames across 153.6 s. The VGA
mode is nominally 60 Hz; serial windows report 59.7. One repeated **boot**
field is reported separately. Film repeats start after the audio clock and
the first armed page latch. Missing lines are counted where scanvideo's
IRQ actually chooses its fallback, not inferred from render time. CMake
instruments a build-local SDK copy and leaves the installed SDK untouched.

The largest shipping synth pump is **92.96 us**. Synth plus pump
costs **901 cycles per stereo frame** over the full run,
about **7.2% of core 1**. Hashes cover the integer
PCM handed to the two PWM DMA rings: all 3,686,400 stereo frames, plus
checkpoints every second. The master peaks at 22,845 with zero clipped
samples; whole-piece RMS is −19.09 dBFS and residual DC is under 4 units.

Image checks are at 0, 30, 60, 90, 120 and 150 seconds; the first is black.
These compare complete RGB555 pages on the real hardware against the
desktop address model. Q15 trig source data and disabled FP contraction
make them reproducible. They are sampled checks, not a claim that every
hardware frame was captured. There was no analogue waveform or external
VGA capture measurement; the movie is explicitly a host capture.

## Release budget and checks

| Item | Bytes |
|---|---:|
| Flash image, including both paintings | 366,088 |
| UF2 container | 733,184 |
| Static main SRAM | 440,564 |
| Heap available before scanvideo allocations | 83,724 |
| Two stacks in separate scratch banks | 8,192 |
| 60 fps host movie | 47,558,316 |

The desktop checks all 9,216 frames plus endpoint, framebuffer guards,
selected frames re-rendered on different fills, deterministic seeking,
complete audio at arbitrary pull sizes, the final fade and signed overflow
traps. The movie contains 9,216 frames at 640×480, 60 fps and stereo 24 kHz.

`release_audit.py` binds both runs to the shipping UF2 and checks the flash
limit, heap headroom, media format and ten negative controls: truncated logs,
wrong PCM hashes, wrong image hashes, missed lines and repeated fields must
all fail. Source, movie and firmware hashes are in
[release.json](validation/release.json). Reproduction commands are in
[README.md](README.md). JSON results come from logs and are not hand-edited.
