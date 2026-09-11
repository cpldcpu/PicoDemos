"""Native-pixel stills from the shipping C renderer."""
from pathlib import Path
import subprocess
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[2]
exe=root/'darkroom/build_host/darkroom.exe'
times=[4,10,20,28,36,44,55,63,70]
sheet=Image.new('RGB',(960,792),(18,22,27));draw=ImageDraw.Draw(sheet)
for i,t in enumerate(times):
    path=root/'media'/f'frame_{t:03d}.ppm'
    subprocess.run([str(exe),'--shot',str(path),'--start',str(t)],check=True,capture_output=True)
    im=Image.open(path).copy();im.save(path.with_suffix('.png'));path.unlink()
    if t==10: im.resize((640,480),Image.Resampling.NEAREST).save(root/'media/poster.png')
    x=i%3*320;y=i//3*264;sheet.paste(im,(x,y));draw.text((x+8,y+245),f'{t:.1f} s',fill=(229,221,197))
sheet.save(root/'media/gallery.png')
