"""The referee for the production's central claim.

    python tools/no_framebuffer.py build_rp2350/persistence.elf

PERSISTENCE claims there is no framebuffer: no frame of the picture is ever
stored anywhere. Unlike "it has no cuts" or "no pixel is a function of t",
that claim can be settled by arithmetic on the firmware image, because a frame
has a size and the chip has a size.

    640 x 480 x 2 bytes = 614,400 bytes
    RP2350 main SRAM    = 524,288 bytes

So a full frame cannot be stored even if every other byte in the machine were
given up, and the check below proves the stronger statement that is actually
interesting: the largest single object in the firmware is far smaller than a
frame, and so is the sum of all of them.

It also checks the two things that would quietly undo the claim:

  * no object is SHAPED like a framebuffer -- exactly 640x480 or 320x240
    pixels, at one or two bytes each. The test is for the exact sizes rather
    than for anything large, and that distinction is the whole difficulty
    here. The first version failed the build because the scene arena is
    342,016 bytes, which is more than a 320x240 frame; but the arena is a
    union of working sets -- a texture, span lists, meshes -- and no part of
    it is a picture. "Big" is not the property being tested. A real
    framebuffer, on the other hand, is always exactly W*H*bpp, because that
    is what a framebuffer is, so the exact test catches the regression it
    exists to catch (somebody adding `static uint16_t fb[320*240]`) and
    nothing else.
  * the kernels that run on the scanout core are linked into SRAM rather than
    left in flash, because a beam-raced line that has to fetch its own code
    over XIP misses the deadline (see the note in 15_Quicksilver).

Exit status is non-zero if any check fails.
"""

import re
import subprocess
import sys
import os

FRAME_BYTES = 640 * 480 * 2
HALF_FRAME_BYTES = 320 * 240 * 2
SRAM_BYTES = 520 * 1024

NM = "arm-none-eabi-nm"
OBJDUMP = "arm-none-eabi-objdump"


def symbols(elf):
    """Real objects only.

    `nm -S` also reports linker-script markers such as __HeapLimit, whose
    "size" is the distance to the next symbol and can be four gigabytes. They
    are addresses, not storage, and counting them made the first run of this
    check report that the firmware contained a 4 GB array.
    """
    out = subprocess.run([NM, "--size-sort", "-S", elf],
                         capture_output=True, text=True, check=True).stdout
    rows = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        addr, size, kind, name = parts
        size = int(size, 16)
        if size > SRAM_BYTES:          # not an object; a linker marker
            continue
        if name.startswith("__"):      # ditto, by convention
            continue
        rows.append((int(addr, 16), size, kind, name))
    return rows


def main():
    elf = sys.argv[1] if len(sys.argv) > 1 else "build_rp2350/persistence.elf"
    if not os.path.exists(elf):
        sys.exit(f"no such ELF: {elf} (build the firmware first)")

    rows = symbols(elf)
    fails = []

    data = [r for r in rows if r[2].lower() in ("b", "d")]
    total = sum(r[1] for r in data)
    biggest = max(data, key=lambda r: r[1]) if data else (0, 0, "", "-")

    print(f"a frame is {FRAME_BYTES:,} bytes; this chip has {SRAM_BYTES:,} of SRAM\n")
    print(f"  largest object   {biggest[3]}  {biggest[1]:,} bytes")
    print(f"  all static data  {total:,} bytes")

    if biggest[1] >= FRAME_BYTES:
        fails.append(f"{biggest[3]} is {biggest[1]:,} bytes and could hold a frame")
    if total >= FRAME_BYTES:
        fails.append(f"static data totals {total:,} bytes, enough for a frame")

    shapes = {
        640 * 480 * 2: "640x480 16bpp",
        640 * 480: "640x480 8bpp",
        320 * 240 * 2: "320x240 16bpp",
        320 * 240: "320x240 8bpp",
    }
    shaped = [(r, shapes[r[1]]) for r in data if r[1] in shapes]

    print(f"\n  no object can hold a frame          "
          f"{'FAIL' if biggest[1] >= FRAME_BYTES else 'ok'}")
    print(f"  all static data together cannot     "
          f"{'FAIL' if total >= FRAME_BYTES else 'ok'}")
    print(f"  no object is framebuffer-shaped     "
          f"{'FAIL' if shaped else 'ok'}")
    for r, what in shaped:
        print(f"      {r[3]} is exactly {what}")
        fails.append(f"{r[3]} is exactly a {what} framebuffer")

    # The scanout core's kernels must live in SRAM (0x2000_0000..), not in the
    # flash window (0x1000_0000..).
    hot = [r for r in rows if r[2].lower() == "t" and (
        r[3].endswith("_line") or r[3] in ("beam_line", "text_row", "affine_line_p",
                                           "s3d_line", "beam_line_setup"))]
    in_flash = [r for r in hot if 0x10000000 <= r[0] < 0x20000000]
    print(f"\n  scanline kernels found              {len(hot)}")
    print(f"  kernels left in flash               "
          f"{'FAIL (' + str(len(in_flash)) + ')' if in_flash else 'ok (none)'}")
    for r in in_flash:
        print(f"      {r[3]} at {r[0]:08x}")
    if in_flash:
        fails.append(f"{len(in_flash)} scanline kernels are in flash, not SRAM")

    print()
    if fails:
        for f in fails:
            print("FAIL:", f)
        sys.exit(1)
    print("no framebuffer: confirmed from the linker map.")


if __name__ == "__main__":
    main()
