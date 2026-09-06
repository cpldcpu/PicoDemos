#!/usr/bin/env python3
"""Dump whatever the board is saying, with no parsing at all.

    python tools/serial_probe.py [COM10] [seconds]

For when serial_read.py returns nothing and the question is no longer "what
are the numbers" but "is the firmware alive". On this platform silence almost
always means a panic before stdio came up, and a panic message is the one
thing a parser would throw away.
"""

import sys
import time

import serial

port_name = sys.argv[1] if len(sys.argv) > 1 else "COM10"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 12.0

port = None
for _ in range(40):
    try:
        port = serial.Serial(port_name, 115200, timeout=0.2)
        break
    except serial.SerialException:
        time.sleep(0.25)
if port is None:
    sys.exit(f"cannot open {port_name}")

port.dtr = True
port.rts = True
print("opened", port_name, flush=True)

t0 = time.time()
buf = b""
while time.time() - t0 < secs:
    chunk = port.read(4096)
    if chunk:
        buf += chunk
port.close()

print("bytes received:", len(buf), flush=True)
sys.stdout.write(buf.decode("utf-8", "replace"))
