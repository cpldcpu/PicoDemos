"""Capture contact sheet and individual stills from the actual C renderer."""
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parents[2]
shots=[('opening',7),('plain',30),('arrival',46),('unfolding',60),('solar',73),('corona',92),('orbits',119),('eclipse',137),('endcard',151)]
sheet=Image.new('RGB',(960,3*264),(6,15,25))
draw=ImageDraw.Draw(sheet)
with tempfile.TemporaryDirectory(prefix='helion-gallery-') as tmp:
    for n,(name,t) in enumerate(shots):
        p=Path(tmp)/f'{name}.ppm'
        subprocess.run([str(root/'helion/build_host/helion.exe'),'--start',str(t),'--shot',str(p)],check=True)
        im=Image.open(p);im.save(root/'media'/f'{name}.png')
        xy=((n%3)*320,(n//3)*264);sheet.paste(im,xy)
        draw.text((xy[0]+8,xy[1]+243),f'{t:05.1f}s / {name.upper()}',fill=(178,206,215))
sheet.save(root/'media'/'gallery.png')
