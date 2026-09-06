#!/usr/bin/env python3
"""Pack Phase's converted assets into the aligned const C arrays the firmware links.

Phase's `assets/round4/convert.py` does the art: it crops, resizes, quantises
into the frozen palettes with deterministic Floyd-Steinberg error diffusion,
and writes one `<name>.pixels.bin` and `<name>.palette.bin` per asset plus a
manifest. It deliberately writes no C. This does that half, and nothing else:
no resampling, no requantising, no palette decisions. Bytes in, bytes out.

    python tools/pack_assets.py                     # from assets/round6
    python tools/pack_assets.py --dir assets/round4 # or any other round

It checks each pixel file against the manifest's `pixel_sha256` before
emitting, so a half-written or mismatched bin cannot silently reach flash, and
it refuses to emit anything the manifest does not describe. The diagnostic
tile is excluded by name: it is a converter self-test, not firmware.

Palettes are already in the VGA DAC's packing (red bits 0-4, green 6-10, blue
11-15) as little-endian uint16, which the script verifies against the
manifest's hex colours rather than trusting.
"""

import argparse
import hashlib
import json
import os
import struct
import sys

EXCLUDE = {"diagnostic_tile"}
# The atlas is one bit deep and has no palette; it lives in engine_assets.
ENGINE = {"inscription_atlas"}


def dac(rgb):
    r, g, b = rgb
    return (r >> 3) | ((g & 0xF8) << 3) | ((b & 0xF8) << 8)


def emit_array(out, ctype, name, count, values, per_line):
    out.append("_Alignas(4) const %s %s[%d] = {" % (ctype, name, count))
    fmt = "%d" if ctype == "uint8_t" else "0x%04x"
    for i in range(0, len(values), per_line):
        out.append(",".join(fmt % v for v in values[i:i + per_line]) + ",")
    out.append("};")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=os.path.join(root, "assets", "round6"))
    ap.add_argument("--check", action="store_true",
                    help="verify only; write nothing")
    args = ap.parse_args()

    manifest = json.load(open(os.path.join(args.dir, "manifest.json"), encoding="utf-8"))
    painted_c, painted_h, engine_c, engine_h = [], [], [], []
    painted_h.append("#ifndef COLOSSUS_PAINTED_ASSETS_H")
    painted_h.append("#define COLOSSUS_PAINTED_ASSETS_H")
    painted_h.append("#include <stdint.h>")
    engine_h.append("#include <stdint.h>")

    total, packed, problems = 0, [], []
    for entry in manifest["assets"]:
        name = entry["name"]
        if name in EXCLUDE:
            continue
        px_path = os.path.join(args.dir, name + ".pixels.bin")
        if not os.path.exists(px_path):
            problems.append("%s: no pixels.bin" % name)
            continue
        px = open(px_path, "rb").read()
        if len(px) != entry["pixel_bytes"]:
            problems.append("%s: %d pixel bytes, manifest says %d"
                            % (name, len(px), entry["pixel_bytes"]))
            continue
        want = entry.get("pixel_sha256")
        got = hashlib.sha256(px).hexdigest()
        if want and want != got:
            problems.append("%s: pixel sha256 %s, manifest says %s" % (name, got, want))
            continue

        if name in ENGINE:
            emit_array(engine_c, "uint8_t", name, len(px), list(px), 24)
            engine_h.append("extern const uint8_t %s[%d];" % (name, len(px)))
            total += len(px)
            packed.append((name, len(px), 0))
            continue

        emit_array(painted_c, "uint8_t", name + "_pixels", len(px), list(px), 24)
        painted_h.append("extern const uint8_t %s_pixels[%d];" % (name, len(px)))
        pal_bytes = open(os.path.join(args.dir, name + ".palette.bin"), "rb").read()
        n = len(pal_bytes) // 2
        pal = list(struct.unpack("<%dH" % n, pal_bytes))
        # The manifest's hex is the authority; the bin must already be packed.
        for i, hexcol in enumerate(entry["palette"]):
            rgb = tuple(bytes.fromhex(hexcol[1:]))
            if dac(rgb) != pal[i]:
                problems.append("%s: palette[%d] is %#06x, %s packs to %#06x"
                                % (name, i, pal[i], hexcol, dac(rgb)))
                break
        emit_array(painted_c, "uint16_t", name + "_palette", n, pal, 12)
        painted_h.append("extern const uint16_t %s_palette[%d];" % (name, n))
        total += len(px) + len(pal_bytes)
        packed.append((name, len(px), len(pal_bytes)))

    painted_h.append("#endif")

    for name, px, pal in packed:
        print("  %-22s %7d px %5d pal" % (name, px, pal))
    print("  %-22s %7d bytes of flash" % ("TOTAL", total))
    if "flash_bytes" in manifest and manifest["flash_bytes"] != total:
        print("  note: manifest says %d, which will differ if an asset was excluded"
              % manifest["flash_bytes"])
    if problems:
        for p in problems:
            print("pack_assets: " + p, file=sys.stderr)
        return 1
    if args.check:
        print("pack_assets: verified, nothing written")
        return 0

    head = "/* Generated by tools/pack_assets.py from %s. Do not edit.\n" \
           " * Flash only, no decompression, no SRAM copy. */\n" % os.path.basename(args.dir)
    with open(os.path.join(root, "assets", "painted_assets.h"), "w", encoding="utf-8") as f:
        f.write(head + "\n".join(painted_h) + "\n")
    with open(os.path.join(root, "assets", "painted_assets.c"), "w", encoding="utf-8") as f:
        f.write(head + '#include "painted_assets.h"\n' + "\n".join(painted_c) + "\n")
    with open(os.path.join(root, "assets", "engine_assets.h"), "w", encoding="utf-8") as f:
        f.write(head + "\n".join(engine_h) + "\n")
    with open(os.path.join(root, "assets", "engine_assets.c"), "w", encoding="utf-8") as f:
        f.write(head + '#include "engine_assets.h"\n' + "\n".join(engine_c) + "\n")
    print("pack_assets: wrote painted_assets.{c,h} and engine_assets.{c,h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
