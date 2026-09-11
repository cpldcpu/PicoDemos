"""Export the actual authored notes and draw a phrase-level piano roll."""
import csv, io, subprocess
from pathlib import Path
from PIL import Image,ImageDraw
root=Path(__file__).resolve().parents[2]
raw=subprocess.run([str(root/'tessera/build_host/tessera_score.exe')],capture_output=True,text=True,check=True).stdout
(root/'media/score.csv').write_text(raw,encoding='utf-8')
rows=list(csv.DictReader(io.StringIO(raw)))
im=Image.new('RGB',(1120,580),(23,31,42));d=ImageDraw.Draw(im)
d.text((24,15),'ONE SMALL THING / PHASE - actual score, bars 16-31 (question then return)',fill=(229,221,197))
colors=[(235,172,52),(220,74,41),(97,130,171),(97,130,171),(97,130,171),(219,217,188)]
for note in range(32,82):
    y=542-(note-32)*10
    if note%12==0:d.text((10,y-4),f'C{note//12-1}',fill=(170,177,182));d.line((48,y,1080,y),fill=(47,57,69))
for bar in range(17):
    x=48+bar*64;d.line((x,42,x,546),fill=(60,70,82));d.text((x+2,553),str(bar+16),fill=(180,186,191))
for row in rows:
    step,voice,note,length=(int(row[k]) for k in ('step','voice','note','length'))
    assert 0<=voice<6 and 24<=note<=84 and 0<length<=32
    if 256<=step<512:
        x=48+(step-256)*4;y=542-(note-32)*10;d.rectangle((x,y-3,min(1071,x+length*4-1),y+3),fill=colors[voice])
im.save(root/'media/score.png')
print(f'{len(rows)} authored note onsets exported; instrument ranges and gate lengths checked')
