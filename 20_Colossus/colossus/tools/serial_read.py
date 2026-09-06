#!/usr/bin/env python3
"""Read COLOSSUS's USB CDC telemetry, and be referee 2 while doing it.

    python tools/serial_read.py --seconds 320 --hashes ../media/hashes.txt
    python tools/serial_read.py --replay run.log --hashes ../media/hashes.txt

main.c prints one line a second (``T ...``) and one at every phrase boundary
(``PHRASE ...``), each ending in the synth's FNV hash latch:

    ... | AHASH s=288000 1a2b3c4d

``s`` is the absolute stereo frame the latch was taken at, always a multiple
of 24,000. The host writes the same table with ``capture --hashes``. If every
latch the device reports matches the host's entry for the same sample, the
device and the host produced identical audio -- which is what referee 2
claims, and it is a claim about sample identity only. Underruns are the
separate question of whether those samples came out on time, and the device
counts them; this reports both.

``--replay`` judges a saved log the same way, which is how the referee itself
gets tested: corrupt one hash in a log and this has to fail.

Exit status is non-zero if any hash disagrees, so it can gate a build.
"""

import argparse
import re
import sys
import time

HASH_RE = re.compile(r"AHASH s=(\d+) ([0-9a-fA-F]{8})")
NUM_RE = {
    "render_max": re.compile(r"render ms [\d.]+/[\d.]+/([\d.]+)"),
    "gap_max": re.compile(r"gap ms ([\d.]+)"),
    "miss": re.compile(r"miss (\d+)"),
    "late": re.compile(r"late (\d+)"),
    "under": re.compile(r"under (\d+)"),
    "fps": re.compile(r"fps ([\d.]+)"),
    "copy": re.compile(r"copy (\d+) cy/line"),
    "synth_cy": re.compile(r"synth (\d+) cy/pump ([\d.]+) Mcy/s (\d+) cy/sample"),
}


def load_hashes(path):
    table = {}
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            pos, value = line.split()
            table[int(pos)] = value.lower()
    return table


def lines_from_port(port_name, seconds):
    import serial                                  # only needed for a real run

    port = None
    for _ in range(40):
        try:
            port = serial.Serial(port_name, 115200, timeout=0.2)
            break
        except serial.SerialException:
            time.sleep(0.5)
    if port is None:
        sys.exit(f"cannot open {port_name}")
    try:
        deadline = time.time() + seconds
        buf = b""
        while time.time() < deadline:
            chunk = port.read(4096)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                yield raw.decode("utf-8", "replace").rstrip()
    finally:
        port.close()


def lines_from_file(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            yield line.rstrip("\n").rstrip()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM10")
    ap.add_argument("--seconds", type=float, default=12.0)
    ap.add_argument("--hashes", default=None, help="host table from `capture --hashes`")
    ap.add_argument("--out", default=None, help="also append every line to this file")
    ap.add_argument("--replay", default=None, help="judge a saved log instead of the port")
    ap.add_argument("--quiet", action="store_true", help="only the summary")
    ap.add_argument("--until-done", action="store_true",
                    help="stop early when the firmware prints its DONE line")
    args = ap.parse_args()

    host = load_hashes(args.hashes) if args.hashes else None
    out = open(args.out, "a", encoding="utf-8") if args.out else None

    stats = {"lines": 0, "checked": 0, "bad": 0, "unknown": 0,
             "render_max": 0.0, "gap_max": 0.0, "miss": 0, "late": 0,
             "under": 0, "fps_min": None, "copy": [], "synth": []}
    seen, bad = set(), []

    source = (lines_from_file(args.replay) if args.replay
              else lines_from_port(args.port, args.seconds))

    for text in source:
        if not args.quiet:
            print(text, flush=True)
        if out:
            out.write(text + "\n")
        stats["lines"] += 1

        m = HASH_RE.search(text)
        if m and host is not None:
            pos, value = int(m.group(1)), m.group(2).lower()
            if pos not in seen:
                seen.add(pos)
                if pos not in host:
                    stats["unknown"] += 1
                else:
                    stats["checked"] += 1
                    if host[pos] != value:
                        stats["bad"] += 1
                        bad.append((pos, value, host[pos]))

        for key, rx in NUM_RE.items():
            mm = rx.search(text)
            if not mm:
                continue
            if key in ("render_max", "gap_max"):
                stats[key] = max(stats[key], float(mm.group(1)))
            elif key in ("miss", "late", "under"):
                stats[key] = max(stats[key], int(mm.group(1)))
            elif key == "fps":
                v = float(mm.group(1))
                stats["fps_min"] = v if stats["fps_min"] is None else min(stats["fps_min"], v)
            elif key == "copy":
                stats["copy"].append(int(mm.group(1)))
            elif key == "synth_cy":
                stats["synth"].append(int(mm.group(3)))

        if args.until_done and text.startswith("DONE"):
            break

    if out:
        out.close()

    def mean(xs):
        return sum(xs) / len(xs) if xs else 0.0

    print()
    print(f"SUMMARY  {stats['lines']} lines from {args.replay or args.port}")
    print(f"  render max        {stats['render_max']:.2f} ms")
    print(f"  frame gap max     {stats['gap_max']:.2f} ms")
    print(f"  fps min (1 s win) {stats['fps_min'] if stats['fps_min'] is not None else 0:.1f}")
    print(f"  below 60 / 30 Hz  {stats['late']} / {stats['miss']} (worst window)")
    print(f"  audio underruns   {stats['under']}")
    print(f"  scanout copy      {mean(stats['copy']):.0f} cycles per line (device)")
    print(f"  synth on core 1   {mean(stats['synth']):.0f} cycles per sample "
          f"= {mean(stats['synth']) * 24000 / 1e6:.2f} Mcycles per second (device)")
    if host is not None:
        cover = 100.0 * len(seen) / len(host) if host else 0.0
        print(f"  hash latches      {stats['checked']} checked, {stats['bad']} wrong, "
              f"{stats['unknown']} not in the host table ({cover:.1f}% of the score covered)")
        for pos, got, want in bad[:10]:
            print(f"    sample {pos}: device {got} host {want}")
    if stats["bad"]:
        sys.exit(1)


if __name__ == "__main__":
    main()
