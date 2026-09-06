"""Drawn COLOSSUS letterforms: broad piers, chamfered counters, split terminals."""
from PIL import Image,ImageDraw
from pathlib import Path
P=Path(__file__).resolve().parent/'source'
im=Image.new('RGB',(320,64),'#101820');d=ImageDraw.Draw(im)
for n,ch in enumerate('COLOSSUS'):
    x=8+n*38;y=9;c='#b89868';bg='#101820'
    outer=[(x+6,y),(x+26,y),(x+32,y+6),(x+32,y+40),(x+26,y+46),(x+6,y+46),(x,y+40),(x,y+6)]
    d.polygon(outer,fill=c)
    d.polygon([(x+11,y+9),(x+21,y+9),(x+23,y+11),(x+23,y+35),(x+21,y+37),(x+11,y+37),(x+9,y+35),(x+9,y+11)],fill=bg)
    if ch=='C':d.rectangle((x+21,y+15,x+33,y+31),fill=bg)
    if ch=='L':d.rectangle((x+9,y-1,x+33,y+36),fill=bg)
    if ch=='U':d.rectangle((x+9,y-1,x+23,y+15),fill=bg)
    if ch=='S':
        d.rectangle((x+9,y+19,x+26,y+27),fill=c)
        d.rectangle((x+24,y+10,x+33,y+18),fill=bg)
        d.rectangle((x-1,y+28,x+8,y+36),fill=bg)
    # One deliberate incision echoes the body's absent rib.
    if ch in 'COU':d.rectangle((x+3,y+21,x+8,y+23),fill=bg)
im.save(P/'wordmark-drawn.png')
