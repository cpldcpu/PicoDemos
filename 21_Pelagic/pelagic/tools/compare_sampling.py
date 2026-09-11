"""Six seconds of synchronized native-renderer A/B motion, without audio."""
from pathlib import Path
from PIL import Image,ImageDraw
import subprocess

root=Path(__file__).resolve().parents[2]
frames=180
players=[]
encoder=None
try:
    for variant in ('build_host','build_host_smooth'):
        players.append(subprocess.Popen([str(root/'pelagic'/variant/'pelagic.exe'),
            '--raw','--start','52','--frames',str(frames),'--fps','30'],stdout=subprocess.PIPE))
    encoder=subprocess.Popen(['ffmpeg','-y','-v','error','-f','rawvideo','-pixel_format','rgb24',
        '-video_size','1280x520','-framerate','30','-i','pipe:0','-an','-c:v','libx264',
        '-preset','medium','-crf','20','-pix_fmt','yuv420p','-movflags','+faststart',
        str(root/'media/filter_comparison.mp4')],stdin=subprocess.PIPE)
    for n in range(frames):
        sheet=Image.new('RGB',(640,260),(5,18,28));d=ImageDraw.Draw(sheet)
        d.text((8,5),'DEFAULT / NEAREST',fill='white')
        d.text((328,5),'OPTIONAL / SMOOTH',fill='white')
        for i,player in enumerate(players):
            data=player.stdout.read(320*240*3)
            if len(data)!=320*240*3:raise RuntimeError('Renderer ended before comparison completed')
            sheet.paste(Image.frombytes('RGB',(320,240),data),(i*320,20))
        encoder.stdin.write(sheet.resize((1280,520),Image.Resampling.NEAREST).tobytes())
    encoder.stdin.close()
    if encoder.wait():raise RuntimeError('Encoder failed')
    for player in players:
        if player.wait():raise RuntimeError('Renderer failed')
finally:
    for process in players+([encoder] if encoder else []):
        if process.poll() is None:process.terminate();process.wait()
print(root/'media/filter_comparison.mp4')
