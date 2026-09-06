# COLOSSUS SRAM ledger · Phase / Overscan · 2026-09-06

Limit: **524,288 bytes**. Phase owns renderer entries; Overscan enforces the
ELF map and replaces platform reserves with actual symbols. No renderer heap
allocation. Flash const assets are read in place. Sizes are bytes, not decimal
KB. Reservations are ceilings, not claims that anything has been measured.

**Measured column added 2026-09-06 by Overscan**, from
`colossus/build_rp2350/colossus.elf.map` — the renderer build in the tree on
that date — except the scanvideo row, which is measured on the board. The
Bytes column of the platform and synth rows now carries those measurements, as
the paragraph above asks; Phase's renderer rows stay reservations for round
four with the measurement recorded beside them, so Running SRAM is still a
reservation total. The measured totals, and the boot floor, are in the section
after the table; the run that produced them is section 8 of
`briefs/2026-09-06-overscan-platform-reply.md`.

Rows marked *scratch* or *heap* are not part of the 512 KB the heap comes out
of: the two core stacks live in SCRATCH_X and SCRATCH_Y, and scanvideo's
buffers are allocated out of the heap after the static image is placed.

| Buffer / reservation | Bytes | Measured | Owner | Storage | Running SRAM |
|---|---:|---:|---|---|---:|
| Two platform pages, `g_pages`, 2 × 320 × 240 × uint16 | 307200 | 307200 | Overscan | static | 307200 |
| Scanout SRAM hot code `scanout` and video.c state | 428 | 428 | Overscan | static | 307628 |
| `r_depth[240][320]` | 76800 | 76800 | Phase | static | 384428 |
| `r_glow[60][80]` | 4800 | 4800 | Phase | static | 389228 |
| `r_blur[60][80]` | 4800 | 4800 | Phase | static | 394028 |
| `r_shades[4][256]` (slot 2 reused for dawn chrome palette) | 2048 | 2048 | Phase | static | 396076 |
| `body_cache[24]` × 28-byte attributed vertex | 672 | 672 | Phase | static | 396748 |
| `body_joints[5][3]` | 60 | 60 | Phase | static | 396808 |
| Renderer context, stats, bounds, alignment (ceiling; includes environment, dawn and curl scalars) | 256 | 88 | Phase | static | 397064 |
| Synth `g_sin[1024]` | 2048 | 2048 | Phosphor | static | 399112 |
| Synth `g_oct8[12]` | 48 | 48 | Phosphor | static | 399160 |
| Synth `g_dly[8640]` | 17280 | 17280 | Phosphor | static | 416440 |
| Synth `g_rv_c[6][1250]` | 15000 | 15000 | Phosphor | static | 431440 |
| Synth `g_rv_a[2][449]` | 1796 | 1796 | Phosphor | static | 433236 |
| Synth `g_chorus` | 2048 | 2048 | Phosphor | static | 435284 |
| Synth `S` (device sizeof) | 772 | 772 | Phosphor | static | 436056 |
| Synth `g_hash`, `g_mark_{seq,pos,hash}` | 16 | 16 | Phosphor | static | 436072 |
| audio_pwm.c: DMA rings `s_left`/`s_right` (4096), block `s_tmp` (256), hot code (300), state (28) | 4680 | 4680 | Overscan | static | 440752 |
| Renderer SRAM hot code (reservation; measured is `r_bloom` alone) | 8192 | 704 | Phase | copied code | 448944 |
| Synth SRAM hot code `render_block` (5144) and `synth_render` (2704) | 7848 | 7848 | Phosphor | copied code | 456792 |
| SDK, newlib and TinyUSB globals (12391) and inter-section alignment (1145) | 13536 | 13536 | Overscan | static | 470328 |
| Two core stacks, includes all renderer scratch | 8192 | 8192 | Overscan | scratch | — |
| scanvideo runtime allocation, **measured on the device** | 11616 | 11616 | Overscan | heap | — |

## Measured on the device, 2026-09-06 (Overscan)

The reservation total above is **470,328** bytes of the 512 KB region, leaving
53,960 for the heap. What the linker actually produced for the renderer build
in the tree on this date, and what the board did with it:

| | Bytes | How |
|---|---:|---|
| Static main SRAM, `0x20000000` to `__end__` | **462,672** | ELF map |
| — of which ours | 449,136 | ELF map |
| — of which SDK, newlib, TinyUSB | 12,391 | ELF map |
| — of which inter-section alignment | 1,145 | ELF map |
| Core stacks, SCRATCH_X + SCRATCH_Y | 8,192 | ELF map |
| **Heap region**, `__end__` to `__StackLimit` | **61,616** | ELF map |
| scanvideo's runtime allocation at `video_init()` | **11,616** | device |
| **Heap floor: boots** | **10,568** | device |
| Heap floor: panics, "Out of memory" | 10,332 | device |

462,672 + 61,616 = 524,288 exactly. The static image is 7,656 bytes smaller
than the reservation total, all of it in two Phase rows that are still
ceilings: renderer hot code (8192 reserved, 704 measured) and renderer
context (256 reserved, 88 measured). Those stay Phase's to revise.

**The floor is measured, not inherited.** `build.ps1 floor` links dead .bss
and bisects until the firmware stops booting: a heap of 10,568 bytes boots and
reaches the main loop, 10,332 panics inside `video_init()`. PERSISTENCE's
79 KiB was never a floor — it was the heap one build happened to have, with
sixteen scanline buffers instead of this build's eight. Carrying it over would
have condemned a build with **51,048 bytes of margin**. That margin is
`tools/ledger_check.py`'s to defend from now on, and it is void the moment
`PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT` or
`PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS` changes.

None of that residual is discretionary asset space; it is the room the
renderer still has to grow into, and every byte of it must arrive here first.

The synth grew while this round was running: 1250-sample comb storage,
449-sample allpasses, and two chorus lines. The table above supersedes the
initial smaller synth reservation; those are Phosphor's concurrent changes,
not edits by Phase. Host `nm -S` gave 41,056 bytes of synth mutable symbols
before alignment; the device linker gives 46,856 for synth.c in total, of
which 7,848 is SRAM hot code, and those are the numbers in the table now.
A successful host build is still not a boot-fit verdict; the boot is.

Stack scratch: body primitive cached once per component, 24 vertices static;
clipped polygons 2 × 8 × 28 = 448 bytes; raster projected vertices/gradients,
span state and call frames fit within a provisional 2 KiB render stack budget
inside the core-0 4 KiB stack. Embers are reconstructed one at a time (no live
particle array), templates are const flash, and LOD reuses the component cache.
No whole-body transformed cache. Bloom has no third field. Floor uses scalar
perspective stepping. Inscription atlas is 1024 bytes in flash, no unpack buffer.

Host-only review executable (`render_host.c`, optional main): one 153600-byte
page, plus 153600-byte reference page for seek checks, and libc file buffering;
these are excluded from device totals and never linked by render.cmake.
Asset converter and sketch generator are offline Python, not firmware SRAM.

First-asset flash actual: wordmark 10240+32; dusk sky 16384+512;
dusk matcap 4096+512; diagnostic texture 4096; atlas 1024. Total **36896**
bytes including palettes, below 131072. Painted assets alone are 31776 bytes. Generated declarations are aligned
`const`; no decompression or copied asset palettes. Texture shade palette uses
one of the four SRAM shade tables already listed.

Update with exact host symbol sizes and any revised reservations after build;
host pointers differ from the RP2350, and host timings do not certify its budget.

Host GCC `-O2 -fstack-usage`: `r_triangle` 992 B, `body_draw` 512 B,
`demo_render` 112 B; the main nested path is about 1616 B plus scene/call
overhead, within the provisional 2 KiB renderer stack allowance. This is a
host compiler observation, not an ARM stack high-water measurement.

Per-frame automatic buffers: `r_triangle` p/out are 448 B; raster attribute
arrays 3×5 floats, dx/dy 2×5 floats, v[5] ints and Span are 160 B total;
projected vertices are 84 B. Body vertices a/b/c/d are 112 B; scalar and ABI
spill space is covered by the measured frames above. Embers have no array.
Host review/check executables each use two pages (307200 B); `render_checks.c`
adds two 4-byte page guards, all host-only. No other persistent host buffers
are introduced by Phase.
