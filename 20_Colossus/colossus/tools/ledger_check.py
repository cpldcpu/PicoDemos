#!/usr/bin/env python3
"""Enforce colossus/LEDGER.md against the linker map and the device.

PERSISTENCE linked cleanly and panicked at boot, twice, because the linker
only ever checks static sections: 499 KB of static data fits in 520 KB of
SRAM, and what did not fit was pico_scanvideo's runtime malloc of its
scanline buffers -- an allocation no build-time tool accounts for. So the
ledger is the tool. Phase writes colossus/LEDGER.md; this reads the map and
fails if:

  1. a source file of ours puts bytes in SRAM and is not named in the ledger;
  2. one of our symbols is at or above --symbol-threshold bytes and is not
     named in the ledger;
  3. the heap left after all static data (__StackLimit - __end__) is below
     the measured boot floor;
  4. --free-heap, the number main.c prints at boot, is below the floor.

Two deliberate limits on 1 and 2. The SDK, newlib and TinyUSB are not ours to
itemise -- nobody is going to write a ledger row for `libc_a-findfp.c` -- so
their SRAM is totalled and reported as one platform figure, and the heap
check is what keeps it honest. And the two 4 KB core stacks live in SCRATCH_X
and SCRATCH_Y, not in the 512 KB the heap comes out of, so they are counted
and printed separately rather than charged against it.

The floor has to be measured on this project, by linking dead .bss with
-DCOLOSSUS_BALLAST=N and bisecting N until the firmware stops booting
(`build.ps1 floor -Ballast N`). Until that run happens, DEFAULT_FLOOR below
carries PERSISTENCE's number and says so; do not quote it as a COLOSSUS
measurement.

    python tools/ledger_check.py --map build_rp2350/colossus.elf.map
    python tools/ledger_check.py --map ... --free-heap 41728
    python tools/ledger_check.py --map ... --warn-only     (during the build)
"""

import argparse
import os
import re
import sys

# NOT YET MEASURED ON THIS PROJECT. 79 KiB is PERSISTENCE's measured floor
# (79 KB of heap booted, 48 KB did not), inherited here as a placeholder so
# that nothing silently passes against a number nobody has checked. It is
# almost certainly pessimistic for COLOSSUS: PERSISTENCE ran scanvideo with
# PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16 and this build uses 8, which is
# 8 x 324 x 4 = 10,368 bytes of scanline buffers instead of about 21 KB.
# Replace this with the ballast bisection from `build.ps1 floor` -- the
# largest -Ballast N whose BOOT line still reaches heap_free_after_video --
# and say in the commit which run produced it.
DEFAULT_FLOOR = 80896        # 79 KiB, inherited from PERSISTENCE, unverified here

# RP2350: 512 KB of striped main SRAM, then SCRATCH_X and SCRATCH_Y at the top.
MAIN_LO, MAIN_HI = 0x20000000, 0x20080000
SCRATCH_LO, SCRATCH_HI = 0x20080000, 0x20082000

SYMBOL_LINE = re.compile(r"^\s+0x[0-9a-f]+\s+(?P<name>[A-Za-z_.$][\w.$]*)\s*$")
ASSIGN_LINE = re.compile(r"^\s+0x(?P<addr>[0-9a-f]+)\s+(?P<name>__\w+)\s*=")


def parse_map(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.readlines()

    entries, symbols = [], {}
    i = 0
    while i < len(lines):
        line = lines[i].rstrip("\n")

        m = ASSIGN_LINE.match(line)
        if m:
            symbols.setdefault(m.group("name"), int(m.group("addr"), 16))

        if line.startswith(" .") and not line.startswith("  "):
            parts = line.strip().split()
            if len(parts) == 1 and i + 1 < len(lines):
                nxt = lines[i + 1].strip().split()
                if len(nxt) >= 3 and nxt[0].startswith("0x"):
                    parts = [parts[0]] + nxt
                    i += 1
            if len(parts) >= 4 and parts[1].startswith("0x") and parts[2].startswith("0x"):
                try:
                    addr, size = int(parts[1], 16), int(parts[2], 16)
                except ValueError:
                    i += 1
                    continue
                if size and MAIN_LO <= addr < SCRATCH_HI:
                    entries.append({"section": parts[0], "addr": addr, "size": size,
                                    "obj": parts[3], "syms": []})
        else:
            m = SYMBOL_LINE.match(line)
            if m and entries:
                entries[-1]["syms"].append(m.group("name"))
        i += 1

    for line in lines:
        for name in ("__StackLimit", "__end__", "__bss_end__", "__HeapLimit"):
            m = re.search(r"0x([0-9a-f]{8,16})\s+" + re.escape(name) + r"(\s|=|$)", line)
            if m:
                symbols.setdefault(name, int(m.group(1), 16))
    return entries, symbols


def symbol_of(entry):
    if entry["syms"]:
        return entry["syms"][0]
    parts = entry["section"].split(".", 2)
    return parts[2] if len(parts) == 3 else entry["section"]


def obj_name(path):
    """A readable source name for a map object path."""
    p = path.replace("\\", "/")
    if "(" in p:                                   # libg.a(libc_a-findfp.o)
        return p.split("(", 1)[1].rstrip(")")
    base = p.rsplit("/", 1)[-1]
    for suffix in (".obj", ".o"):
        if base.endswith(suffix):
            base = base[: -len(suffix)]
            break
    return base


def is_ours(path):
    """True for objects built from sources in colossus/, false for the SDK.

    Everything is compiled into one target, so the SDK's objects also live
    under CMakeFiles/colossus.dir -- but with the source tree's path baked in
    as D_/Pico/..., which is what separates them.
    """
    p = path.replace("\\", "/")
    if "CMakeFiles/colossus.dir/" not in p:
        return False
    tail = p.split("CMakeFiles/colossus.dir/", 1)[1]
    return not tail.startswith("D_/") and "/pico-sdk/" not in tail and "/pico-extras/" not in tail


def load_ledger(path):
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    # The ledger is prose that has to stay true, not a machine format Phase
    # has to learn: any identifier it mentions counts as declared.
    return set(re.findall(r"[A-Za-z_][\w.]*", text))


def declared(names, ledger):
    return any(n in ledger or n.split(".")[0] in ledger for n in names)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)

    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default=os.path.join(root, "build_rp2350", "colossus.elf.map"))
    ap.add_argument("--ledger", default=os.path.join(root, "LEDGER.md"))
    ap.add_argument("--floor", type=int, default=DEFAULT_FLOOR)
    ap.add_argument("--free-heap", type=int, default=None,
                    help="the heap_free= number main.c printed at boot")
    ap.add_argument("--symbol-threshold", type=int, default=1024)
    ap.add_argument("--warn-only", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.map):
        print(f"ledger_check: no map at {args.map}", file=sys.stderr)
        return 0 if args.warn_only else 2

    entries, symbols = parse_map(args.map)
    main_e = [e for e in entries if MAIN_LO <= e["addr"] < MAIN_HI]
    scratch_e = [e for e in entries if SCRATCH_LO <= e["addr"] < SCRATCH_HI]

    ours = [e for e in main_e if is_ours(e["obj"])]
    theirs = [e for e in main_e if not is_ours(e["obj"])]

    by_obj = {}
    for e in ours:
        by_obj[obj_name(e["obj"])] = by_obj.get(obj_name(e["obj"]), 0) + e["size"]
    big = sorted((e for e in ours if e["size"] >= args.symbol_threshold), key=lambda e: -e["size"])

    heap = None
    if "__StackLimit" in symbols and "__end__" in symbols:
        heap = symbols["__StackLimit"] - symbols["__end__"]

    ledger = load_ledger(args.ledger)

    if not args.quiet:
        print(f"main SRAM     {sum(e['size'] for e in main_e):,} B "
              f"({sum(e['size'] for e in ours):,} ours, "
              f"{sum(e['size'] for e in theirs):,} SDK/newlib/TinyUSB)")
        print(f"scratch       {sum(e['size'] for e in scratch_e):,} B "
              f"(core stacks, SCRATCH_X/Y, not from the heap)")
        if heap is not None:
            print(f"heap region   {heap:,} B  (__end__ {symbols['__end__']:#x} .. "
                  f"__StackLimit {symbols['__StackLimit']:#x})")
        provenance = ("inherited from PERSISTENCE, NOT measured on this project"
                      if args.floor == DEFAULT_FLOOR else "measured on the device")
        print(f"boot floor    {args.floor:,} B ({provenance})")
        if args.free_heap is not None:
            print(f"device booted with {args.free_heap:,} B free")
        print()
        print("  our SRAM, by source")
        for name, size in sorted(by_obj.items(), key=lambda kv: -kv[1]):
            mark = " " if ledger is None or declared([name], ledger) else "!"
            print(f"  {mark} {size:9,}  {name}")
        print()
        print(f"  our symbols >= {args.symbol_threshold:,} B")
        for e in big:
            sym = symbol_of(e)
            mark = " " if ledger is None or declared([sym, e["section"]], ledger) else "!"
            print(f"  {mark} {e['size']:9,}  {sym:<28} {obj_name(e['obj'])}")
        print()

    failures = []
    if ledger is None:
        failures.append(f"no ledger at {args.ledger} (Phase writes it; PLANNING.md section 8)")
    else:
        for name, size in sorted(by_obj.items()):
            if not declared([name], ledger):
                failures.append(f"{name} holds {size:,} B of SRAM and is not named in the ledger")
        for e in big:
            sym = symbol_of(e)
            if not declared([sym, e["section"]], ledger):
                failures.append(f"symbol {sym} ({obj_name(e['obj'])}) is {e['size']:,} B "
                                f"of SRAM and is not named in the ledger")

    if heap is not None and heap < args.floor:
        failures.append(f"static data leaves {heap:,} B of heap, below the {args.floor:,} B boot floor")
    if args.free_heap is not None and args.free_heap < args.floor:
        failures.append(f"the device booted with {args.free_heap:,} B free, "
                        f"below the {args.floor:,} B floor")

    if not failures:
        print("ledger_check: OK")
        return 0
    for f in failures:
        print(f"ledger_check: {f}", file=sys.stderr)
    if args.warn_only:
        print("ledger_check: --warn-only, not failing the build", file=sys.stderr)
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
