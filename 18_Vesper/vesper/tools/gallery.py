"""Rebuild the gallery using the shipped C player. Requires Pillow."""
import argparse
from pathlib import Path
import subprocess
import tempfile
from PIL import Image, ImageDraw

parser=argparse.ArgumentParser()
parser.add_argument('--exe',type=Path,default=Path(__file__).resolve().parents[1]/'build_host/vesper.exe')
args=parser.parse_args()
media=Path(__file__).resolve().parents[2]/'media'
scenes=[('opening',6,'00:06 / VESPER'),('nave',23,'00:23 / THE NAVE'),('reliquary',44,'00:44 / THE RELIQUARY'),('iris',68,'01:08 / BLOOM IN THE STATIC'),('swarm',80,'01:20 / DISASSEMBLY'),('organ',89,'01:29 / ALL THINGS RESONATE'),('return',108,'01:48 / THE RETURN'),('ending',114,'01:54 / LATENT')]
sheet=Image.new('RGB',(1280,4*518),(8,12,17))
draw=ImageDraw.Draw(sheet)
with tempfile.TemporaryDirectory(prefix='vesper-stills-') as tmp:
    for i,(name,seconds,label) in enumerate(scenes):
        ppm=Path(tmp)/'frame.ppm'
        subprocess.run([str(args.exe),'--start',str(seconds),'--shot',str(ppm)],check=True)
        with Image.open(ppm) as native:
            im=native.resize((640,480),Image.Resampling.NEAREST)
        im.save(media/f'{name}.png')
        x,y=i%2*640,i//2*518
        sheet.paste(im,(x,y));draw.text((x+36,y+487),label,fill=(155,185,195))
sheet.save(media/'gallery.jpg',quality=94,subsampling=0)
print(media/'gallery.jpg')
