"""Capture PERSISTENCE to a video file.

    python tools/capture.py --out ../media/persistence.mp4
    python tools/capture.py --from 53 --secs 14 --out tunnel.mkv --lossless

Runs the host build headless, takes its raw 640x480 BGRA frames and feeds them
to ffmpeg, muxing the synth's WAV alongside. Unlike demo 17, this production
CAN seek -- it stores nothing, so every frame is a function of the frame index
-- and --from therefore costs nothing: the host is simply told to start there.

WHY A PYTHON BRIDGE and not a shell pipe: PowerShell's pipeline is an object
pipeline and re-encodes anything sent through it, which silently corrupts
binary. subprocess pipes are bytes. (Inherited from 17_Hysteresis, where this
cost an evening.)

The content here is close to a worst case for a video codec -- native 640x480
with hard one-pixel edges everywhere, a dithered floor, and a plasma with no
flat areas at all -- so the x264 settings protect the detail rather than hit a
bitrate.
"""

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.join(HERE, "..", "host")
EXE = os.path.join(HOST, "persistence.exe")

W, H, FPS = 640, 480, 60
FRAME = W * H * 4
TOTAL = 9000

X264 = [
    "-c:v", "libx264", "-preset", "slow", "-pix_fmt", "yuv420p",
    "-x264-params",
    "aq-mode=3:aq-strength=1.0:psy-rd=1.0,0.2:no-dct-decimate=1:"
    "deadzone-inter=0:deadzone-intra=0:ref=6:bframes=8",
    "-movflags", "+faststart",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="persistence.mp4")
    ap.add_argument("--from", dest="start", type=float, default=0.0,
                    help="seconds to start at (free: the demo seeks)")
    ap.add_argument("--secs", type=float, default=0.0, help="0 = to the end")
    ap.add_argument("--crf", type=int, default=16)
    ap.add_argument("--lossless", action="store_true")
    ap.add_argument("--no-audio", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(EXE):
        sys.exit("build the host player first (cd host && make)")

    first = int(args.start * FPS)
    count = int(args.secs * FPS) if args.secs else (TOTAL - first)

    wav = None
    if not args.no_audio:
        wav = os.path.join(HOST, "_capture.wav")
        subprocess.run([EXE, "--wav", wav], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    vin = ["-f", "rawvideo", "-pixel_format", "bgra",
           "-video_size", f"{W}x{H}", "-framerate", str(FPS), "-i", "-"]
    ain = ["-ss", str(args.start), "-i", wav] if wav else []
    vcodec = ["-c:v", "ffv1", "-level", "3"] if args.lossless else X264 + ["-crf", str(args.crf)]
    acodec = ["-c:a", "aac", "-b:a", "192k"] if wav else []
    amap = ["-map", "0:v", "-map", "1:a", "-shortest"] if wav else []

    cmd = (["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
           + vin + ain + amap + vcodec + acodec + [args.out])

    print(f"rendering {count} frames ({count / FPS:.1f} s) from {args.start:.1f}s "
          f"-> {args.out}", file=sys.stderr)

    sim = subprocess.Popen([EXE, "--headless", "--rawpipe",
                            "--start", str(args.start), "--frames", str(count)],
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)
    enc = subprocess.Popen(cmd, stdin=subprocess.PIPE, bufsize=0)

    try:
        n = 0
        while n < count:
            buf = sim.stdout.read(FRAME)
            if len(buf) < FRAME:
                break
            enc.stdin.write(buf)
            n += 1
            if n % 600 == 0:
                print(f"  {n}/{count}  {n / FPS:6.1f}s", file=sys.stderr)
    finally:
        try:
            enc.stdin.close()
        except OSError:
            pass
        enc.wait()
        sim.stdout.close()
        sim.wait()
        if wav and os.path.exists(wav):
            os.remove(wav)

    if os.path.exists(args.out):
        mb = os.path.getsize(args.out) / 1048576
        print(f"wrote {args.out}: {mb:.1f} MB", file=sys.stderr)


if __name__ == "__main__":
    main()
