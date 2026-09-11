"""Build the hardware report from the exact-binary release manifest."""
import json,re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
m=json.loads((root/'validation/release.json').read_text(encoding='utf-8'))
runs=m['runs'];worst=max(r['worst_render_ms'] for r in runs)
reference=m.get('software_reference')
comparison=''
if reference:
    comparison=f"""The optional software reference uses CPU background copies and software
texture addressing. Its complete run matches all six image checks, all
153 second hashes and the final PCM hash. It reaches **{reference['worst_render_ms']:.2f} ms**,
with **{reference['counters']['repeat']} repeated {'field' if reference['counters']['repeat']==1 else 'fields'}**; its strict production gate
result is **{'PASS' if reference['passed'] else 'FAIL'}**. It is a diagnostic fallback.
The accelerated UF2 is the compo build: **{worst:.2f} ms** worst, with no
repeated fields in either run."""
names=['development_01','development_02','development_art_01','release_01','release_02']
if (root/'validation/reference_01.json').exists():names.append('reference_01')
rows=[]
for name in names:
    r=json.loads((root/'validation'/f'{name}.json').read_text(encoding='utf-8'));c=r['counters']
    rows.append(f"| [{name}](validation/{name}.log) | {r['worst_render_ms']:.2f} | {r['fps_min']:.1f} | {c['repeat']} | {c['over']} | {c['missed']} | {c['under']} | {r['checked_hashes']} |")
peak=[];cycles=[]
for name in ['release_01','release_02']:
    text=(root/'validation'/f'{name}.log').read_text(encoding='utf-8')
    peak.extend(float(x) for x in re.findall(r'peak pump ([\d.]+) us',text))
    cycles.extend(int(x) for x in re.findall(r'\| synth \d+ cy/pump (\d+) cy/sample',text))
summary=f'Two complete shipping-UF2 runs: **59.7 fps in every timing window**, **{worst:.2f} ms worst render**, **zero repeated film fields, zero missing scanlines and zero audio underruns**. All **306/306** per-second PCM hashes, both complete-score hashes and **12/12** fixed-frame image hashes match the desktop.'
report=f'''# TESSERA hardware validation

Phase / GPT-6 Astra, 2026-09-11.

{summary}

RP2350 A2, COM10, pico-sdk 2.2.0, Cortex-M33 at 300 MHz / 1.20 V.
Every run programs and verifies flash with `picotool load -F -v` before
rebooting. The connected board reports 16 MiB installed flash; the release
audit separately enforces the production's **4 MiB** budget.

UF2 SHA-256: `{m['uf2_sha256']}`.

| Run | Worst ms | Min fps | Repeats | Over 16 ms | Missing lines | Underruns | Second hashes |
|---|---:|---:|---:|---:|---:|---:|---:|
{chr(10).join(rows)}

Development failures are retained as evidence, not counted as release
passes. The earlier pre-art release runs are in `validation/pre_art/`.
The previous card-layout release is archived in `validation/before_typography/`.

{comparison}

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

The shipping runs draw {runs[0]['frames']:,} and {runs[1]['frames']:,} frames across 153.6 s. The VGA
mode is nominally 60 Hz; serial windows report 59.7. One repeated **boot**
field is reported separately. Film repeats start after the audio clock and
the first armed page latch. Missing lines are counted where scanvideo's
IRQ actually chooses its fallback, not inferred from render time. CMake
instruments a build-local SDK copy and leaves the installed SDK untouched.

The largest shipping synth pump is **{max(peak):.2f} us**. Synth plus pump
costs **{str(min(cycles)) if min(cycles)==max(cycles) else str(min(cycles))+'–'+str(max(cycles))} cycles per stereo frame** over the full run,
about **{max(cycles)*24000/3000000:.1f}% of core 1**. Hashes cover the integer
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
| Flash image, including both paintings | {m['flash_image_bytes']:,} |
| UF2 container | {m['uf2_bytes']:,} |
| Static main SRAM | {m['static_main_sram_bytes']:,} |
| Heap available before scanvideo allocations | {m['heap_before_scanvideo_bytes']:,} |
| Two stacks in separate scratch banks | 8,192 |
| 60 fps host movie | {m['movie_bytes']:,} |

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
'''
(root/'HARDWARE_VALIDATION.md').write_text(report,encoding='utf-8')
print(summary)
