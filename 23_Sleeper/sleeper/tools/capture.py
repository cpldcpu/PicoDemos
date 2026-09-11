"""Capture the actual C renderer and synth, never a separate approximation.

60 fps, not 30: PLANNING §2's first claim is about 60, and a 30 fps film
cannot show a repeated field. The video is doubled to 640x480 with a nearest
neighbour scale, as the VGA DAC does it.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=Path(__file__).resolve().parents[1] / 'build_host' / 'sleeper.exe')
    parser.add_argument('--out', type=Path, default=Path(__file__).resolve().parents[2] / 'media' / 'sleeper.mp4')
    parser.add_argument('--fps', type=int, default=60)
    parser.add_argument('--silent', action='store_true', help='skip the WAV pass; the video has no audio track')
    args = parser.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='sleeper-') as tmp:
        inputs = []
        if not args.silent:
            wav = Path(tmp) / 'score.wav'
            subprocess.run([str(args.exe), '--wav', str(wav)], check=True)
            inputs = ['-i', str(wav)]
        render = subprocess.Popen([str(args.exe), '--raw', '--fps', str(args.fps)], stdout=subprocess.PIPE)
        command = (['ffmpeg', '-y', '-v', 'warning', '-f', 'rawvideo', '-pixel_format', 'rgb24',
                    '-video_size', '320x240', '-framerate', str(args.fps), '-i', 'pipe:0'] + inputs +
                   ['-vf', 'scale=640:480:flags=neighbor', '-c:v', 'libx264', '-preset', 'slow',
                    '-crf', '20', '-maxrate', '6M', '-bufsize', '12M', '-pix_fmt', 'yuv420p'] +
                   (['-c:a', 'aac', '-b:a', '128k'] if inputs else ['-an']) +
                   ['-movflags', '+faststart', '-shortest', str(args.out)])
        try:
            encoder = subprocess.Popen(command, stdin=render.stdout)
            render.stdout.close()
            encoder_status = encoder.wait()
            if encoder_status and render.poll() is None:
                render.terminate()
            render_status = render.wait()
            if encoder_status or render_status:
                raise RuntimeError(f'capture failed: renderer={render_status}, ffmpeg={encoder_status}')
        finally:
            if render.poll() is None:
                render.terminate()
                render.wait()
    print(args.out)

if __name__ == '__main__':
    main()
