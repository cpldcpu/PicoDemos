# SLEEPER — the memory ledger

Overscan, 2026-09-07, revised 2026-09-08 for round two. Written before the
first allocation and kept true after it, as COLOSSUS did. **Every figure is HOST** — read out of
`build_rp2350/sleeper.elf` with `arm-none-eabi-nm` and `arm-none-eabi-size`
— except the boot floor, which is COLOSSUS's DEVICE bisection and is quoted
as inherited, not re-measured.

The RP2350 on the Pico 2 has **520 KiB** of SRAM: 512 KiB in the main striped
bank at `0x20000000`, plus two 4 KiB scratch banks at `0x20080000` which hold
the two core stacks and nothing else. Everything static plus the runtime heap
must fit in the 512 KiB; the stacks do not compete with them.

## What is in SRAM

| | Bytes | Note |
|---|---:|---|
| Two 320×240 RGB555 pages | 307,200 | `video.c`, aligned 4; the transport needs two and cannot do with one |
| Audio DMA rings, left + right | 8,192 | `audio_pwm.c`, each aligned 4096 for the DMA ring wrap |
| Rail texture, 128×128 indexed | 16,384 | ballast + a sleeper every four texels |
| Tunnel ring texture, 128×128 indexed | 16,384 | concrete, a ring joint every sixteen |
| Depth palettes, cool + warm, 16×256×2 | 16,384 | the plane and the tunnel |
| Polar grid, 41×31 × 8 | 10,168 | the tunnel's, and now only the tunnel's |
| Synth delay line | 13,500 | Phosphor's |
| Synth reverb | 15,000 | Phosphor's |
| Sine table, 1024 floats | 4,096 | `lights.c` |
| Hot code moved out of flash | ~19,000 | see below |
| Everything else (SDK, TinyUSB, newlib, the renderer's small statics, alignment) | ~38,000 | |
| **Total static, `0x20000000` → `__bss_end__` (`0x200716b4`)** | **464,052** | |
| **Heap, `__bss_end__` → `__StackLimit`** | **60,236** | |
| | 524,288 | 464,052 + 60,236, exactly the main bank |
| Core stacks, SCRATCH_X + SCRATCH_Y | 8,192 | outside the main bank |

### The synth's reserve

The brief reserved **48 KiB** for Phosphor's engine. The engine as delivered
uses **31,776 bytes** of that in its two big lines (13,500 delay, 15,000
plate) plus about 3 KiB of voice state — call it **35 KiB**, against an
estimate of ~40 KiB. It is inside its reserve and the 13 KiB it did not
spend is in the heap column, where scanvideo can use it.

### The hot code

About 21 KiB of the renderer is `__not_in_flash_func`, which spends heap to
buy determinism: `demo_render`, `halo`, `light_draw`, `window_light`, `px`,
`mixc`, `hspan`, `hspan_a`, `vspan`, `rect`, `rect_a`, `disc`, `line_a`,
`page_dim`, `band_fill`, `ballast`, `water`, `droplet`, `rain_draw`,
`board_at`, `tile`, `band`, `plane`, `rails`, `polar_pass`, `sky_expand`,
`world_side`, `world_ahead`, `world_under`, `world_up`,
`world_tunnel_draw`, `world_dream`, plus `scanout`, `audio_pump` and
`synth_render`. `poles`, `farms`, `tunnel_wall`, `station_body`,
`passing_train_draw`, `crossing`, `kaleido`, `wheel` and `rails_of_light`
have no symbols of their own: they are inlined into their `HOT` callers and
go to SRAM with them, which is the trap working for us rather than against
us for once.

**The HELION trap, checked and not repeated.** `HOT()` on a helper does
nothing if the compiler inlines it into a flash caller: the attribute goes
with the symbol that no longer exists. The first RP2350 link of SLEEPER had
`demo_render`, `halo`, `light_draw`, `plane`, `polar_pass` and `sky_expand`
in SRAM and *every span primitive and every world function in flash* —
`hspan`, `vspan`, `rect`, `disc`, `line_a`, `mixc`, `band_fill`,
`world_side`, `world_ahead`, `board_at` — because the worlds were unmarked
callers. Marking the callers as well moved 19 KiB and left the flash list
empty. The check is `arm-none-eabi-nm sleeper.elf`, and it is in the
round-one reply.

**And it bit once more in round two**, on exactly one function: `rect_a`,
the alpha-blended rectangle the passing train's window cores and the sleep
window's reflection are drawn with. It is called from two SRAM functions and
was itself in flash, because it was new and unmarked. Marked; the flash list
is empty again. The lesson is not "remember to mark things", it is "read the
map after every link", and that is now what the round reply does.

## What is in flash

| | Bytes |
|---|---:|
| Night sky plate, 320×240 indexed + 256×3 palette | 77,568 |
| Dawn sky plate, same | 77,568 |
| Code and the rest of `.rodata` | ~100,180 |
| **Flash image** | **255,316** |
| Free of the 4 MiB | 3,938,988 |

## The heap, and the floor under it

Nothing in this production calls `malloc` except `pico_scanvideo`, which
allocates its scanline buffers at `video_init()`. At
`PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=12` and
`PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=324` that is about 21 KiB — half
as much again as the ~14 KiB COLOSSUS measured at eight buffers.

COLOSSUS bisected the **boot floor** on the board with dead `.bss` ballast:
**10,568 bytes of heap boots and reaches the main loop; 10,332 does not**
(`20_Colossus/briefs/numbers.md`). That figure is a property of the platform
and is void if either scanvideo constant changes — and SLEEPER changes one of
them, from 8 buffers to 12, so the floor here is higher than COLOSSUS's by
about 7 KiB. Taking it as ~18 KiB to be safe:

| | Bytes |
|---|---:|
| Heap available | 60,236 |
| scanvideo's runtime allocation, 12 buffers | ~21,000 |
| Left after `video_init()` | ~39,000 |
| Boot floor, inherited and adjusted for 12 buffers | ~18,000 |
| **Margin** | **~21,000** |

**What is left, honestly: about 39 KiB after the transport has taken its
share.** Round two added telegraph poles, a ballast band, farms, wall pools,
a window-light kaleidoscope and a full-page dim, and cost 52 bytes of static
SRAM net, because the textured kaleidoscope it removed was code too. That is the budget for anything round two adds. The two sky plates
cost nothing here because they stay in flash and are expanded a row at a
time; putting either one in SRAM as RGB555 would cost 153,600 bytes and
there is nowhere to put it, which is exactly why the sky is indexed and
tinted rather than DMA'd (PLANNING §9, and Phase's note about it).

## What would break it

- **A third texture.** The rail and ring textures are 16 KiB each. A third
  costs a quarter of the remaining heap; the kaleidoscope shares the rail
  texture rather than having its own, and that was the reason.
- **A second polar grid.** One grid, centred on the match point, serves the
  tunnel; round two rebuilt the kaleidoscope out of window lights, so the
  grid now has one customer and could be dropped entirely if 10 KiB were
  ever needed — the tunnel is the only shot that wants it.
- **A depth buffer.** COLOSSUS had one at 76,800 bytes. SLEEPER has no
  hidden-surface problem — every system is painter-ordered by construction —
  and there is no room for one.
- **Raising the scanline buffer count again.** Twelve is already 21 KiB and
  it is what absorbs the worst `audio_pump()`.
