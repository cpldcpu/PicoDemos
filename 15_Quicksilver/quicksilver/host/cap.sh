#!/usr/bin/env bash
# cap.sh — build the host preview and capture a PNG screenshot at given ms
# timestamps. Usage:  ./cap.sh 5000 [12000 ...]
# PNGs land in screenshots/cap_<ms>.png
set -e
export PATH="/d/msys64/ucrt64/bin:$PATH"
cd "$(dirname "$0")"
make -s
for ms in "$@"; do
    rm -f screenshots/screenshot_000.bmp
    ./quicksilver.exe --start-ms "$ms" --screenshot-at "$ms" --exit-after $((ms+100)) >/dev/null 2>&1 || true
    if [ -f screenshots/screenshot_000.bmp ]; then
        ffmpeg -y -loglevel error -i screenshots/screenshot_000.bmp "screenshots/cap_${ms}.png"
        echo "screenshots/cap_${ms}.png"
    else
        echo "FAILED to capture at ${ms}ms"
    fi
done
