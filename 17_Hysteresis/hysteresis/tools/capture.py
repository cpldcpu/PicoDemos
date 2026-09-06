"""Capture HYSTERESIS to a video file.

    python tools/capture.py --out ../media/hysteresis.mp4
    python tools/capture.py --from 105 --secs 20 --out peak.mkv --lossless

Runs the host build headless, takes its raw 640x480 BGRA frames and feeds them
to ffmpeg, muxing the synth's WAV alongside.

WHY A PYTHON BRIDGE and not a shell pipe: on this machine PowerShell's pipeline
is an OBJECT pipeline, so it decodes and re-encodes anything sent through it and
silently corrupts binary. It destroyed a field dump earlier in the project and
then killed ffmpeg with garbage input. subprocess pipes are bytes.

THERE IS NO SEEK, so --from does not skip work: the simulation has to be stepped
to that point regardless (sim.h), and this only discards the frames before it
rather than paying to encode them. Expect roughly real time.
"""

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.join(HERE, "..", "host")
EXE = os.path.join(HOST, "hysteresis.exe")
WAV_EXE = os.path.join(HOST, "synthwav.exe")

W, H, FPS = 640, 480, 60
FRAME = W * H * 4

# See the docstring in this file's --help and media/README for the measurements
# behind these. Short version: this content is close to a worst case for a video
# codec, so the settings are chosen to protect the dither and the block edges
# rather than to hit a bitrate.
X264 = [
    "-c:v", "libx264", "-preset", "slow", "-pix_fmt", "yuv420p",
    "-x264-params",
    "aq-mode=3:aq-strength=1.1:psy-rd=1.0,0.3:no-dct-decimate=1:"
    "deadzone-inter=0:deadzone-intra=0:ref=6:bframes=8",
    "-movflags", "+faststart",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="hysteresis.mp4")
    ap.add_argument("--from", dest="start", type=float, default=0.0,
                    help="seconds to discard from the front")
    ap.add_argument("--secs", type=float, default=0.0, help="0 = to the end")
    ap.add_argument("--crf", type=int, default=17)
    ap.add_argument("--lossless", action="store_true")
    ap.add_argument("--no-audio", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(EXE):
        sys.exit("build the host harness first (cd host && make)")

    first = int(args.start * FPS)
    count = int(args.secs * FPS) if args.secs else 0
    total = 12600 if not count else first + count

    wav = None
    if not args.no_audio:
        wav = os.path.join(HOST, "_capture.wav")
        subprocess.run([WAV_EXE, "-o", wav], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    vin = ["-f", "rawvideo", "-pixel_format", "bgra",
           "-video_size", f"{W}x{H}", "-framerate", str(FPS), "-i", "-"]
    ain = ["-ss", str(args.start), "-i", wav] if wav else []

    if args.lossless:
        vcodec = ["-c:v", "ffv1", "-level", "3"]
    else:
        vcodec = X264 + ["-crf", str(args.crf)]

    acodec = ["-c:a", "aac", "-b:a", "192k"] if wav else []
    amap = ["-map", "0:v", "-map", "1:a", "-shortest"] if wav else []

    cmd = (["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
           + vin + ain + amap + vcodec + acodec + [args.out])

    print(f"rendering {total - first} frames "
          f"({(total - first) / FPS:.1f} s) -> {args.out}", file=sys.stderr)

    sim = subprocess.Popen([EXE, "--headless", "--rawpipe",
                            "--frames", str(total)],
                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                           bufsize=0)
    enc = subprocess.Popen(cmd, stdin=subprocess.PIPE, bufsize=0)

    try:
        n = 0
        while n < total:
            buf = sim.stdout.read(FRAME)
            if len(buf) < FRAME:
                break
            n += 1
            if n > first:
                enc.stdin.write(buf)
            if n % 600 == 0:
                print(f"  {n}/{total}  {n / FPS:6.1f}s", file=sys.stderr)
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
