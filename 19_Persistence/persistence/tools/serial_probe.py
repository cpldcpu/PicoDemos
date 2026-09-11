"""Dump whatever the board is saying, with no parsing. For when serial_read
returns nothing and the question is whether the firmware is alive at all."""

import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM10"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 12.0

s = None
for _ in range(40):
    try:
        s = serial.Serial(port, 115200, timeout=0.2)
        break
    except serial.SerialException:
        time.sleep(0.25)
if s is None:
    sys.exit(f"cannot open {port}")

s.dtr = True
s.rts = True
print("opened", port, flush=True)

t0 = time.time()
buf = b""
while time.time() - t0 < secs:
    c = s.read(4096)
    if c:
        buf += c
s.close()

print("bytes received:", len(buf), flush=True)
sys.stdout.write(buf.decode("utf-8", "replace"))
