"""Procedural design plates, not renderer captures. World layout is read only."""
from pathlib import Path
import json, math, random
import numpy as np
from PIL import Image,ImageDraw
O=Path(__file__).resolve().parent;R=O.parents[2];A=R/'colossus/assets'
params=json.loads((A/'body_parameters.json').read_text());parts=json.loads((A/'body_layout.json').read_text())
bayer=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]])
def sky(plain=False):
    im=Image.new('RGB',(320,240));p=im.load()
    for y in range(240):
        for x in range(320):
            t=y/239;c=np.array([16+35*t,24+43*t,40+48*t]);
            if plain and y>145:c=np.array([57,54,43])*(1-(y-145)/400)
            c=np.clip(np.floor((c+(bayer[y%4,x%4]/16-.5)*8)/8)*8,0,248);p[x,y]=tuple(map(int,c))
    return im
cams={112:([6,16.0,12],[0,18.35,0],510),124:([6.8,16.6,11.8],[0,18.45,0],510)}
for bar,(pos,target,focal) in cams.items():
    eye=np.array(pos);forward=np.array(target)-eye;forward/=np.linalg.norm(forward);right=np.cross(forward,[0,1,0]);right/=np.linalg.norm(right);up=np.cross(right,forward)
    def project(p):
        d=np.array(p)-eye;z=d@forward;return (160+focal*(d@right)/z,120-focal*(d@up)/z)
    faces=[]
    for b in parts:
        if b['name'] not in ('neck','hood','hood_brow','crown_left','crown_right','eye_recess'):continue
        corners=[np.array([b['x']+sx*b['w']/2,b['y']+sy*b['h']/2,b['z']+sz*b['d']/2]) for sx,sy,sz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        for ids,col in [([0,1,2,3],(48,40,32)),([4,5,6,7],(96,80,56)),([1,5,6,2],(56,48,40)),([0,4,7,3],(72,64,48)),([3,2,6,7],(144,120,80)),([0,1,5,4],(32,32,32))]:
            pts=[corners[i] for i in ids];faces.append((sum((p-eye)@forward for p in pts)/4,[project(p) for p in pts],(16,24,32) if b['name']=='eye_recess' else col))
    # Circular eye recessed behind hood front; painter-sort with all faces.
    for radius,col in [(.52,(40,48,48)),(.38,(128,72,32)),(.23,(224,120,48)),(.09,(248,200,120))]:
        pts=[np.array([radius*math.cos(t),17.45+radius*math.sin(t),.64]) for t in np.linspace(0,2*math.pi,33)]
        faces.append(((np.array([0,17.45,.64])-eye)@forward-(.6-radius)*.01,[project(p) for p in pts],col))
    im=sky();d=ImageDraw.Draw(im)
    for _,p,c in sorted(faces,key=lambda v:-v[0]):d.polygon(p,fill=c)
    im.save(O/f'crown-{bar}.png');im.resize((960,720),Image.Resampling.NEAREST).save(O/f'crown-{bar}-3x.png')
# Six native-size blocking studies of a local hand substitution.
times=[-.5,-.3,-.1,.1,.3,.5];frames=[]
for t,strength,coverage in zip(times,[0,.30,.85,.85,.30,0],[0,0,0,.55,1,1]):
    im=sky(True);d=ImageDraw.Draw(im)
    for x in (65,234):d.polygon([(x,0),(x+4,0),(x+2,240),(x-2,240)],fill=(64,56,40))
    for i in range(9):
        x=72+i*18;d.polygon([(x,192),(x+3,184-i%3*3),(x+15,183),(x+14,194)],fill=(48,48,40))
    # Match the far pier to the cuff; substitution only within this silhouette.
    old=Image.new('RGBA',(320,240));q=ImageDraw.Draw(old);q.polygon([(125,0),(155,0),(153,107),(128,115)],fill=(72,64,48,255))
    new=Image.new('RGBA',(320,240));q=ImageDraw.Draw(new)
    q.polygon([(122,0),(162,0),(166,77),(187,93),(181,117),(112,119),(108,95),(123,77)],fill=(80,64,40,255))
    for x,y in [(114,167),(139,176),(164,165)]:
        q.polygon([(x,116),(x+12,116),(x+11,144),(x+7,y),(x-1,y-2),(x+2,141)],fill=(104,88,56,255));q.line([(x+6,119),(x+6,142),(x+3,y-4)],fill=(168,184,184,255),width=2)
    im=im.convert('RGBA');mixed=Image.blend(old,new,coverage);im.alpha_composite(mixed)
    # Analytic local elliptical glow, peak alpha 0.24; never a frame wash.
    glow=Image.new('RGBA',(320,240));pix=glow.load()
    for y in range(200):
        for x in range(82,220):
            r=((x-151)/69)**2+((y-85)/110)**2
            if r<1:pix[x,y]=(232,112,32,int(61*strength*(1-r)**2))
    im.alpha_composite(glow);d=ImageDraw.Draw(im);rng=random.Random(24)
    for i in range(round(80*strength)):
        x=rng.randrange(98,207);y=rng.randrange(8,183)
        d.line([(x,y),(x+1,y-2)],fill=(248,168,64),width=1)
    im=im.convert('RGB');im.save(O/f'veil-{t:+.1f}s.png');frames.append(im)
strip=Image.new('RGB',(960,512),(16,24,32));d=ImageDraw.Draw(strip)
for i,(t,im) in enumerate(zip(times,frames)):
    x=(i%3)*320;y=(i//3)*256;strip.paste(im,(x,y));d.text((x+8,y+242),f'{t:+.1f} s',fill=(216,224,224))
strip.save(O/'veil-six-moments.png');strip.resize((2880,1536),Image.Resampling.NEAREST).save(O/'veil-six-moments-3x.png')
(O/'camera.json').write_text(json.dumps({'units':'body_layout: sole y=0, crown y=20.4; +Y up, +Z eye front','body_parameters':params,'poses':{str(k):{'position':v[0],'target':v[1],'focal_pixels':v[2],'vertical_fov_degrees':2*math.degrees(math.atan(120/v[2]))} for k,v in cams.items()}},indent=2)+'\n')
