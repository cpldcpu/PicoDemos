"""Capture the actual C renderer and synth, never a separate approximation."""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--exe',type=Path,default=Path(__file__).resolve().parents[1]/'build_host'/'pelagic.exe')
    parser.add_argument('--out',type=Path,default=Path(__file__).resolve().parents[2]/'media'/'pelagic.mp4')
    parser.add_argument('--fps',type=int,default=30)
    args=parser.parse_args()
    args.out.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='pelagic-') as tmp:
        wav=Path(tmp)/'score.wav'
        subprocess.run([str(args.exe),'--wav',str(wav)],check=True)
        render=subprocess.Popen([str(args.exe),'--raw','--fps',str(args.fps)],stdout=subprocess.PIPE)
        command=['ffmpeg','-y','-v','warning','-f','rawvideo','-pixel_format','rgb24','-video_size','320x240','-framerate',str(args.fps),'-i','pipe:0','-i',str(wav),'-vf','scale=640:480:flags=neighbor','-c:v','libx264','-preset','slow','-crf','21','-maxrate','3M','-bufsize','6M','-pix_fmt','yuv420p','-c:a','aac','-b:a','128k','-movflags','+faststart','-shortest',str(args.out)]
        try:
            encoder=subprocess.Popen(command,stdin=render.stdout)
            render.stdout.close()
            encoder_status=encoder.wait()
            if encoder_status and render.poll() is None:
                render.terminate()
            render_status=render.wait()
            if encoder_status or render_status:
                raise RuntimeError(f'capture failed: renderer={render_status}, ffmpeg={encoder_status}')
        finally:
            if render.poll() is None:
                render.terminate()
                render.wait()
    print(args.out)

if __name__=='__main__':
    main()
