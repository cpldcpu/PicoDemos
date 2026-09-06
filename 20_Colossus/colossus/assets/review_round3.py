#!/usr/bin/env python3
"""WSL renderer review; native/3x midpoints, transitions and all 160 bars."""
from pathlib import Path
from PIL import Image
import subprocess,json,re,argparse
R=Path(__file__).resolve().parents[2];C=R/'colossus';S=R/'briefs/sketches/round3';S.mkdir(exist_ok=True)
sources=['render.c','body.c','scene_hand.c','scene_chapters.c','scene_material_test.c','assets/engine_assets.c','assets/painted_assets.c','song.c']
subprocess.run(['gcc','-DHOST_BUILD=1','-std=c11','-O2','-Wall','-Wextra','-Werror',str(C/'render_host.c'),*[str(C/s) for s in sources],'-lm','-o',str(C/'assets/phase_capture')],check=True)
records=[]
def capture(name,sample,scale=True):
 p=S/(name+'.ppm');run=subprocess.run([str(C/'assets/phase_capture'),str(sample),str(p)],check=True,capture_output=True,text=True)
 fields=dict((k,int(v)) for k,v in re.findall(r'(sample|triangles|fill|embers|chapter)=(\d+)',run.stderr));fields['name']=name;records.append(fields)
 im=Image.open(p).convert('RGB');im.save(S/(name+'.png'));p.unlink()
 if scale:im.resize((960,720),Image.Resampling.NEAREST).save(S/(name+'-3x.png'))
 return im
mid=[('overture',4),('plain',16),('hand',32),('heart',48),('eye',64),('load',80),('spine',100),('crown',120),('reveal',136),('coda',152)]
contact=Image.new('RGB',(320*5,240*2))
for i,(name,bar) in enumerate(mid):contact.paste(capture('chapter-'+name,bar*46080),((i%5)*320,(i//5)*240))
contact.save(S/'chapters-contact.png')
for bar in [8,24,40,56,72,88,112,128,144]:
 for dt in [-24000,0,24000]:capture(f'transition-{bar:03d}-'+('before' if dt<0 else 'after' if dt>0 else 'downbeat'),bar*46080+dt)
strip=Image.new('RGB',(40*320,4*240))
for bar in range(160):strip.paste(capture(f'bar-{bar:03d}',bar*46080+23040,False),((bar%40)*320,(bar//40)*240))
strip.save(S/'whole-run-160-bars.png')
for bar in [24.75,31.5,39,42,46,50,54,60,66,68,70,74,78,82,86,90,96,104,110,128.01,129,130,134,140,144.5,147.5,150.5,153.5,157,159.5]:capture('detail-'+str(bar).replace('.','-'),int(bar*46080))
(S/'capture-counts.json').write_text(json.dumps(records,indent=2)+'\n')
for i,(name,_) in enumerate(mid):
 rows=[r for r in records if r['chapter']==i];m=next(r for r in rows if r['name']=='chapter-'+name)
 print(name,'midpoint',m['triangles'],m['fill'],'sampled maxima',max(r['triangles'] for r in rows),max(r['fill'] for r in rows))
print('Captured',len(records),'frames; full run strip is 12800x960, 4 rows of 40 native frames.')
