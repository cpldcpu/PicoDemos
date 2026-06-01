# 15_Quicksilver — *QUICKSILVER*

A liquid-chrome demoscene production for the **Raspberry Pi Pico 2 (RP2350)**,
beat-loosely-synced to *"Quicksilver Mercy"* (Suno 4.5, ~4:27). Every scene is
driven by the **RP2350 SIO interpolator** (INTERP0/INTERP1) — the demo's
hardware hero — using its affine address-generation, BLEND (hardware bilinear
lerp) and CLAMP units. It is, as far as this repo goes, the **first demo here to
actually use the interpolator** (demo 13 reserved it but never wired it up).

Because the SDL host has no such peripheral, Quicksilver ships a **bit-exact
software emulator** of the interpolator, so the identical effect code previews
on a PC and runs as raw silicon on the RP2350 — host and hardware are pixel-identical.

**640×480 @ 60 Hz scanout.** The fullscreen **rotozoomer** renders at *native*
640×480, **beam-raced with no framebuffer** — core 1 generates each scanline live
via the interpolator (POP self-stepping) as the beam races (a 640×480 truecolour
framebuffer is 614 KB > 520 KB SRAM, so beam-racing is the only way). The other
scenes (title, **Mode-7 mercury plain**, chrome, liquid, credits) render in a
320×240 RGB565 buffer on core 0 and are 2× pixel/line-doubled at scanout — QVGA,
which leaves the per-pixel budget for bilinear filtering, distance fog and the
chrome rasteriser. Pimoroni VGA Demo Base (15-bit DAC).

## The arc

| Scene | What | Interpolator feature |
|-------|------|----------------------|
| **Title** | Chrome "QUICKSILVER" wordmark over molten-mercury droplets | — |
| **Rubber Rotozoomer** | Fullscreen bilinear rotozoom of a chrome filigree, sine "rubber" flex + motion-blur | affine address-gen + bilinear |
| **Mercury Plain** | Infinite reflective Mode-7 ground to a horizon under a chrome dusk sky | per-scanline affine + **CLAMP** haze |
| **Chrome** *(centerpiece)* | Six high-poly solids spinning as polished chrome (sphere, trefoil & (3,5) knots, spike-ball, twisted torus, rounded cube), reflecting a matcap sphere-map | per-pixel matcap address-gen + bilinear |
| **Liquid Metal** | Plasma field computed coarse and **BLEND-bilinear-upscaled** to full res, colourised as flowing mercury | **BLEND** (hardware lerp) |
| **Rotozoom reprise → Credits** | Reprise, then chrome credits scrolling over a reflective mercury floor | affine address-gen |

Every scene boundary gets a uniform **liquid-chrome glint** crossfade.

## Build

### Host preview (SDL2, for iteration)
```sh
cd quicksilver/host
make
./quicksilver.exe                                  # interactive
./quicksilver.exe --screenshot-at 105000 --exit-after 105100   # snapshot a scene
# or: ./cap.sh 6000 60000 105000     # build + capture PNGs of several timestamps
```
Keys: `ESC/Q` quit · `S` screenshot · `SPACE` next scene · `LEFT` prev · `R` restart.

### RP2350 firmware
```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras
cd quicksilver
cmake -B build_rp2350 -G "MinGW Makefiles" -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s
cmake --build build_rp2350 -j
# flash build_rp2350/quicksilver.uf2 (hold BOOTSEL, drag onto RPI-RP2)
```
300 MHz @ 1.20 V. Text ~3.3 MB / 4 MB flash, BSS ~386 KB / 520 KB SRAM.

## Regenerating assets
- **Textures**: nano-banana PNGs in `assets/` → `tools/pack_assets.py` (PIL; run under WSL) → `assets/_packed/*.bin`. A procedural fallback generator is `tools/make_textures.c`.
- **3D objects**: `gcc tools/make_meshes.c -o tools/make_meshes.exe -lm && ./tools/make_meshes.exe > assets/_packed/meshes.h`.
- **Wordmark / tunnel / logos**: delivered PNGs in `assets/` (chrome wordmark, fluted tunnel wall) → `tools/pack_assets.py`. See `assets/PROMPTS.md` for the generation prompts and outstanding requests.
- **Music**: `ffmpeg -i "assets/Quicksilver Mercy.mp3" -ac 1 -ar 22050 -f s16le music.raw && ./tools/qoaconv_s16.exe music.raw music.qoa 22050 1`.
- **Interpolator emulator self-test**: `gcc -DHOST_BUILD=1 -I. tools/interp_selftest.c interp_emu.c -o t && ./t` (must print `ALL PASS`).

## Full VGA (640×480) — `quicksilver_vga640`
A standalone firmware (`quicksilver_vga640.uf2`) renders a **true 640×480@60 VGA**
rotozoom with **no framebuffer at all**: core 1 generates each scanline live via
the interpolator in **POP self-stepping** mode (one `pop_full` per pixel returns
the texel offset *and* advances u,v) straight into the scanvideo line buffer as
the beam races. A 640×480 truecolor framebuffer is 614 KB > 520 KB SRAM, so
beam-racing is the only way to do full VGA on the chip — and the interpolator is
what keeps generation inside the ~16 cy/px budget. Build it from the same CMake;
preview on host with `vga640/quicksilver_vga640.exe`.

## Credits
A **LATENT** production — the demo group for the machine-authored RP2350
productions in this repo.

- **Code & direction** — Claude Opus 4.8
- **Critic** — Azure
- **2D art** — Gemini 3.5 Flash + Nano Banana 2
- **Music** — Suno 4.5 (*"Quicksilver Mercy"*)
- **Hardware hero** — the RP2350 SIO interpolator (affine address-gen, BLEND, CLAMP, POP self-stepping)

See [IMPLEMENTATION.md](quicksilver/IMPLEMENTATION.md) for the technical deep-dive
and [PLANNING.md](PLANNING.md) for the original design.
