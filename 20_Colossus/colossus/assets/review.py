#!/usr/bin/env python3
"""Rebuild WSL review executable, capture native PNGs, 3x PNGs and chrome motion."""
from pathlib import Path
from PIL import Image
import subprocess
R=Path(__file__).resolve().parents[2];C=R/'colossus';B=C/'assets';S=R/'briefs/sketches'
sources=['render.c','body.c','scene_hand.c','scene_material_test.c','assets/engine_assets.c','assets/painted_assets.c','song.c']
subprocess.run(['gcc','-DHOST_BUILD=1','-std=c11','-O2','-Wall','-Wextra','-Werror',str(C/'render_host.c'),*[str(C/s) for s in sources],'-lm','-o',str(B/'phase_capture')],check=True)
def capture(name,sample,stress=False):
    p=S/(name+'.ppm');subprocess.run([str(B/'phase_capture'),str(sample),str(p)]+(['stress'] if stress else []),check=True,capture_output=True)
    im=Image.open(p).convert('RGB');im.save(S/(name+'.png'));im.resize((960,720),Image.Resampling.NEAREST).save(S/(name+'-3x.png'));return im
capture('hand-engine',26*46080)
capture('material-ceiling',26*46080,True)
capture('eye-chrome',58*46080)
capture('body-reveal-scaffold',136*46080)
capture('wordmark-in-engine',4*46080)
frames=[]
for i in range(48):
    frames.append(capture('motion-frame',24*46080+i*3840))
frames[0].save(S/'hand-chrome-motion.gif',save_all=True,append_images=frames[1:],duration=160,loop=0,optimize=False)
# A deterministic PNG strip avoids GIF palette changes for exact color review.
strip=Image.new('RGB',(320*4,240))
for i in range(4):strip.paste(frames[i*12],(320*i,0))
strip.save(S/'hand-chrome-keyframes.png')
for ext in ['.ppm','.png','-3x.png']:(S/('motion-frame'+ext)).unlink()
print('Review stills, exact-color keyframes, and 7.68-second hand motion written to briefs/sketches')
