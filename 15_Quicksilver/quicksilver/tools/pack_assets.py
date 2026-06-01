#!/usr/bin/env python3
"""
pack_assets.py — converts AI-generated PNGs in ../assets/ into the raw
formats the RP2350 engine consumes, and emits assets.S + assets.h that
the CMake build incbins.

Run from WSL:
    wsl -e bash -c "cd /mnt/d/Toyprojects/20_TheDemo/thedemo/tools && python3 pack_assets.py"

Output goes to ../assets/_packed/ — that directory is incbinned via a
generated assets.S that lives there too.

Formats:
  '8bpp'   — palette-indexed 256-color, Floyd-Steinberg dithering.
             Emits <name>.bin (W*H bytes, row-major) and <name>.pal
             (256 entries of RGB565 little-endian = 512 bytes).
             For MODE_320 chunky scenes.
  'rgb565' — packed 16-bit color, little-endian. Emits <name>.bin only.
             For MODE_160 truecolor scenes and the env-map.
  'gray8'  — 8-bit grayscale, no palette. Emits <name>.bin only.
             For heightmaps.
  '1bpp'   — packed 1-bit monochrome, 8 pixels per byte MSB-first.
             Emits <name>.bin only. For shape masks.

Some assets are derived from others (e.g. voxel_height from voxel_color)
rather than loaded from a standalone PNG.
"""

import os
import sys
import struct
from pathlib import Path

from PIL import Image, ImageOps

# ---- config -------------------------------------------------------------

HERE         = Path(__file__).resolve().parent
ASSETS_DIR   = HERE.parent / "assets"
OUT_DIR      = ASSETS_DIR / "_packed"

# (name, source_png_or_None, format, target_w, target_h, dither, derivation, [reserved])
# derivation is one of: None | "heightmap_from:<other_name>"
# reserved is an optional int (default 0) for 8bpp assets: quantize to
# (256 - reserved) colors, leaving the top slots free for runtime use
# (sky gradient, fade ramps, etc.).
ASSETS = [
    # name        source          fmt       W    H    dither  derivation  reserved
    # Title backdrop — 8bpp, blitted (palette in title_bg.pal) by effects/title.c.
    ("title_bg",  "title_bg.png", "8bpp",   320, 240, True,   None,       0),
    # Rubber rotozoomer texture — seamless chrome filigree (pow2 = free wrap).
    ("roto",      "roto.png",     "rgb565", 256, 256, False,  None),
    # Mode-7 mercury ground tile — seamless.
    ("ground",    "ground.png",   "rgb565", 256, 256, False,  None),
    # Chrome matcap sphere-map — sampled by view-space normals in envmap3d.c.
    ("envmap",    "envmap.png",   "rgb565", 256, 256, False,  None),
    # Equirectangular dusk sky panorama (pow2 width = free yaw wrap).
    ("sky",       "sky_pano.png", "rgb565", 512, 128, False,  None),
    # Chrome Wordmark Logo (320x80)
    ("title_logo","title_logo.png", "rgb565", 320, 80, False,  None),
    # Seamless tunnel wall (256x256)
    ("tunnel",    "tunnel.png",   "rgb565", 256, 256, False,  None),
]


# ---- helpers ------------------------------------------------------------

def rgb_to_565(r, g, b):
    """8-8-8 RGB to packed 16-bit, PIO-native bit layout matching the
    default pico_scanvideo_dpi pin mapping on the Pimoroni VGA Demo
    Base (B at MSB, gap bit at 5, R at LSB, 5 bits per channel — the
    hardware has no GPIO pin for the 6th green bit). See
    thedemo/rgb565.h for the layout diagram. Effects unpack the same
    layout when blending or palette-fading."""
    return ((b & 0xF8) << 8) | ((g & 0xF8) << 3) | (r >> 3)


def pack_8bpp(img, out_bin, out_pal, dither, reserved=0):
    """Quantize to (256 - reserved) colors with optional FS dither, dump
    indices + RGB565 palette. Reserved upper slots are zero-filled and
    free for runtime use (gradients, fades). Image indices stay in
    [0, 256 - reserved)."""
    img = img.convert("RGB")
    pal_method = Image.Quantize.MEDIANCUT
    dither_flag = Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE
    q = img.quantize(colors=256 - reserved, method=pal_method, dither=dither_flag)
    # Index bytes:
    indices = q.tobytes()
    out_bin.write_bytes(indices)
    # Palette: PIL gives us a flat [r0,g0,b0, r1,g1,b1, ...] list of length 768.
    pal = q.getpalette()
    if pal is None or len(pal) < 768:
        # Pad if quantizer used fewer colors.
        pal = (pal or []) + [0] * (768 - len(pal or []))
    pal_bytes = bytearray()
    for i in range(256):
        r, g, b = pal[i*3], pal[i*3+1], pal[i*3+2]
        pal_bytes += struct.pack("<H", rgb_to_565(r, g, b))
    out_pal.write_bytes(bytes(pal_bytes))


def pack_rgb565(img, out_bin):
    img = img.convert("RGB")
    px = img.load()
    w, h = img.size
    buf = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            buf += struct.pack("<H", rgb_to_565(r, g, b))
    out_bin.write_bytes(bytes(buf))


def pack_gray8(img, out_bin):
    img = img.convert("L")
    out_bin.write_bytes(img.tobytes())


def pack_1bpp(img, out_bin):
    """8 pixels per byte, MSB-first. Threshold at 128."""
    img = img.convert("L").point(lambda v: 255 if v >= 128 else 0).convert("1")
    # PIL '1' mode packs 8 pixels per byte, MSB-first. Just dump.
    out_bin.write_bytes(img.tobytes())


def derive_heightmap(source_png, target_w, target_h):
    """voxel_height is derived from voxel_color: grayscale, blur, level-stretch.
    Matches the imagemagick recipe in PROMPTS.md."""
    img = Image.open(source_png).convert("RGB").resize(
        (target_w, target_h), Image.LANCZOS)
    # Use luminance, then push contrast so peaks/basins separate.
    img = ImageOps.grayscale(img)
    # Heavier blur than the colormap so the projected terrain reads as
    # smooth coral mounds rather than spiky noise. Radius=8 on the 1024-source
    # collapses the finest features that produce per-column "spikes" in the
    # 320-wide voxel render.
    from PIL import ImageFilter
    img = img.filter(ImageFilter.GaussianBlur(radius=16))
    # Level-stretch 10..95% → 0..255 (same numbers as PROMPTS.md).
    lut = [0] * 256
    lo, hi = int(0.10 * 255), int(0.95 * 255)
    for v in range(256):
        if v <= lo:
            lut[v] = 0
        elif v >= hi:
            lut[v] = 255
        else:
            lut[v] = int(255 * (v - lo) / (hi - lo))
    img = img.point(lut)
    return img


# ---- main ---------------------------------------------------------------

def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    manifest = []
    total = 0

    for entry in ASSETS:
        name, src, fmt, w, h, dither, deriv = entry[:7]
        reserved = entry[7] if len(entry) > 7 else 0
        out_bin = OUT_DIR / f"{name}.bin"
        out_pal = OUT_DIR / f"{name}.pal" if fmt == "8bpp" else None

        # Get the source image.
        if deriv is not None and deriv.startswith("heightmap_from:"):
            ref = ASSETS_DIR / deriv.split(":", 1)[1]
            if not ref.exists():
                print(f"  SKIP {name}: heightmap source {ref.name} missing")
                continue
            img = derive_heightmap(ref, w, h)
        else:
            png = ASSETS_DIR / src
            if not png.exists():
                print(f"  SKIP {name}: {src} missing")
                continue
            img = Image.open(png).resize((w, h), Image.LANCZOS)

        # Pack to target format.
        if fmt == "8bpp":
            pack_8bpp(img, out_bin, out_pal, dither, reserved)
        elif fmt == "rgb565":
            pack_rgb565(img, out_bin)
        elif fmt == "gray8":
            pack_gray8(img, out_bin)
        elif fmt == "1bpp":
            pack_1bpp(img, out_bin)
        else:
            print(f"  ERR  {name}: unknown format {fmt}")
            continue

        size = out_bin.stat().st_size
        pal_size = out_pal.stat().st_size if out_pal else 0
        total += size + pal_size
        manifest.append((name, fmt, w, h, size, pal_size))
        rsvd_note = f" reserved={reserved}" if reserved else ""
        print(f"  PACK {name:14s} {fmt:6s} {w}x{h:<4d} -> {size:6d} B"
              + (f" + {pal_size} pal" if pal_size else "")
              + rsvd_note)

    print(f"\nTotal packed: {total} bytes ({total/1024:.1f} KB)")

    # Emit assets.S (incbins) and assets.h (externs + sizes).
    write_assets_s(manifest)
    write_assets_h(manifest)
    print(f"  wrote {OUT_DIR/'assets.S'}")
    print(f"  wrote {OUT_DIR/'assets.h'}")


def write_assets_s(manifest):
    lines = [
        "/* GENERATED by tools/pack_assets.py — do not edit by hand. */",
        "",
        '    .section .rodata.assets, "a"',
        "    .balign 4",
        "",
    ]
    for name, fmt, w, h, sz, palsz in manifest:
        lines += [
            f"    .global asset_{name}_data",
            f"asset_{name}_data:",
            f'    .incbin "{name}.bin"',
            f"    .global asset_{name}_end",
            f"asset_{name}_end:",
            "",
        ]
        if palsz:
            lines += [
                f"    .global asset_{name}_pal",
                f"asset_{name}_pal:",
                f'    .incbin "{name}.pal"',
                "",
            ]
    (OUT_DIR / "assets.S").write_text("\n".join(lines), encoding="utf-8")


def write_assets_h(manifest):
    lines = [
        "/* GENERATED by tools/pack_assets.py — do not edit by hand. */",
        "#ifndef THEDEMO_ASSETS_H",
        "#define THEDEMO_ASSETS_H",
        "#include <stdint.h>",
        "",
    ]
    for name, fmt, w, h, sz, palsz in manifest:
        UN = name.upper()
        lines += [
            f"/* {name}: {fmt} {w}x{h} {sz} bytes" + (f" + {palsz} B palette" if palsz else "") + " */",
            f"extern const uint8_t  asset_{name}_data[];",
            f"extern const uint8_t  asset_{name}_end[];",
            f"#define ASSET_{UN}_W {w}",
            f"#define ASSET_{UN}_H {h}",
            f"#define ASSET_{UN}_SIZE {sz}",
        ]
        if palsz:
            lines += [
                f"extern const uint16_t asset_{name}_pal[256];",
            ]
        lines += [""]
    lines += ["#endif", ""]
    (OUT_DIR / "assets.h").write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
