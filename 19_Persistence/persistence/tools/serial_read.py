"""Read the firmware's USB CDC telemetry for a few seconds.

    python tools/serial_read.py [--port COM10] [--seconds 6]

The device prints one line a second (main.c). The worst-line figure is the
whole verdict on the beam-racing budget, so this is the tool that gets run
after every flash.
"""

import argparse
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM10")
    ap.add_argument("--seconds", type=float, default=6.0)
    ap.add_argument("--out", default=None, help="also append to this file")
    args = ap.parse_args()

    for attempt in range(20):
        try:
            s = serial.Serial(args.port, 115200, timeout=0.2)
            break
        except serial.SerialException:
            time.sleep(0.5)
    else:
        sys.exit(f"cannot open {args.port}")

    out = open(args.out, "a", encoding="utf-8") if args.out else None
    t_end = time.time() + args.seconds
    buf = b""
    while time.time() < t_end:
        chunk = s.read(4096)
        if chunk:
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").rstrip()
                print(text, flush=True)
                if out:
                    out.write(text + "\n")
    s.close()


if __name__ == "__main__":
    main()
