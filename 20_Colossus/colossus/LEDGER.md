# COLOSSUS SRAM ledger · Phase / Overscan · 2026-09-06

Recorded before renderer allocation. Limit: **524,288 bytes**. Phase owns
renderer entries; Overscan enforces the ELF map and replaces platform reserves
with actual symbols. No renderer heap allocation. Flash const assets are read
in place. Sizes below are bytes, not decimal KB. Reservations are ceilings,
not claims that the platform has implemented or measured them.

| Buffer / reservation | Bytes | Owner | Storage | Running SRAM |
|---|---:|---|---|---:|
| Two platform pages, 2 × 320 × 240 × uint16 | 307200 | Overscan | static, reserved | 307200 |
| `r_depth[240][320]` | 76800 | Phase | static | 384000 |
| `r_glow[60][80]` | 4800 | Phase | static | 388800 |
| `r_blur[60][80]` | 4800 | Phase | static | 393600 |
| `r_shades[4][256]` | 2048 | Phase | static | 395648 |
| `body_cache[24]` × 28-byte attributed vertex | 672 | Phase | static | 396320 |
| `body_joints[5][3]` | 60 | Phase | static | 396380 |
| Renderer context, stats, bounds, alignment (ceiling) | 256 | Phase | static | 396636 |
| Synth `g_sin[1024]` | 2048 | Phosphor | static | 398684 |
| Synth `g_oct8[12]` | 48 | Phosphor | static | 398732 |
| Synth `g_dly[8640]` | 17280 | Phosphor | static | 416012 |
| Synth `g_rv_c[6][1250]` | 15000 | Phosphor | static | 431012 |
| Synth `g_rv_a[2][449]` | 1796 | Phosphor | static | 432808 |
| Synth `g_chorus[2][1024]` | 4096 | Phosphor | static | 436904 |
| Synth `S` (current host sizeof) | 772 | Phosphor | static | 437676 |
| Synth `g_hash`, `g_mark_{seq,pos,hash}` | 16 | Phosphor | static | 437692 |
| Audio DMA rings and synth block (reservation) | 4096 | Overscan | static | 441788 |
| Two core stacks, includes all renderer scratch | 8192 | Overscan | stack reserve | 449980 |
| Renderer SRAM hot code (reservation) | 8192 | Phase | copied code | 458172 |
| Synth SRAM hot code (reservation) | 8192 | Phosphor | copied code | 466364 |
| Platform globals, USB, SDK, alignment (reservation) | 4096 | Overscan | static | 470460 |
| scanvideo runtime allocation (estimate, not linker-visible) | 21504 | Overscan | heap | 491964 |

This leaves **53,828 bytes before** scanvideo's allocation, **32,324 after**.
PERSISTENCE's 79 KiB free-heap successful boot is the provisional conservative
floor; 48 KiB failed. This reservation does **not** pass the 79 KiB floor
(**27,068 bytes short**). Do not lower that floor or call this
hardware-safe. Overscan must measure this project's floor and map; otherwise
Phosphor must approve a memory change. Both core stacks and both owners' hot
code are counted. No portion of the residual is discretionary asset space.

The synth grew while this round was running: 1250-sample comb storage,
449-sample allpasses, and two chorus lines. The table above supersedes the
initial smaller synth reservation; those are Phosphor's concurrent changes,
not edits by Phase. Host `nm -S` gives 41,056 bytes of synth mutable symbols
before alignment. Device linker output is still required, particularly for
hot code and SDK globals. A successful host build is not a boot-fit verdict.

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
