from pathlib import Path
p=Path(__file__).resolve().parents[1]/'tools/convert_assets.py';s=p.read_text().replace('converter v1','converter v2').replace("'converter_version':1","'converter_version':2")
s=s.replace("    if kind=='sky':", """    if kind=='bronze':return [(30+i*154//255&248,31+i*121//255&248,32+i*72//255&248) for i in range(256)]
    if kind=='stone':return ramp((24,28,32),(104,96,80),256)
    if kind=='warm':return ramp((16,24,32),(224,104,32),128)+ramp((32,32,32),(216,200,168),128)
    if kind=='ember':return [(0,0,0)]+ramp((48,24,8),(248,208,128),15)
    if kind=='end':return [(0,0,0)]+ramp((48,48,48),(184,168,144),15)
    if kind=='sky':""")
s=s.replace('    outputs={};decl=', '    shared={};outputs={};decl=')
s=s.replace("('dusk_sky',(256,64),8,'sky')]:", """('dusk_sky',(256,64),8,'sky'),
        ('bronze_wear',(64,64),8,'bronze'),('stone',(64,64),8,'stone'),
        ('dawn_sky',(256,64),8,'sky'),('dawn_matcap',(64,64),8,'matcap'),
        ('warm_environment',(64,64),8,'warm'),('furnace',(64,32),8,'warm'),
        ('ember_stamps',(16,64),4,'ember'),('end_inscription',(320,32),4,'end')]:""")
s=s.replace("path=SRC/('wordmark-painted-master.png' if name=='wordmark' else name.replace('_','-')+'-master.png');source=path.read_bytes();im=Image.open(path).convert('RGB')", """filename={'wordmark':'wordmark-painted-master.png','dusk_matcap':'dusk-matcap-v3-master.png','warm_environment':'warm-environment-v2-master.png'}.get(name,name.replace('_','-')+'-master.png')
        path=SRC/filename;source=path.read_bytes();im=Image.open(path).convert('RGB')""")
s=s.replace("        packed=bytes(indices) if bits==8", """        mapping=None
        if name.startswith('dawn_'):
            # Palette-only crossfade: average the new painting within each fixed
            # dusk index region. Keep the approved dusk sky's spatial topology.
            ref='dusk_'+name[5:];indices=list(shared[ref][0]);counts=[0]*256;sums=[[0,0,0] for _ in range(256)]
            for idx,rgb in zip(indices,im.getdata()):
                counts[idx]+=1
                for k in range(3):sums[idx][k]+=rgb[k]
            pal=[tuple((sums[i][k]//counts[i])&248 for k in range(3)) if counts[i] else shared[ref][1][i] for i in range(256)]
            mapping={'index_reference':ref,'method':'mean source RGB per dusk index, floor to 5-bit DAC','reference_pixel_sha256':sha(bytes(indices))}
        shared[name]=(indices,pal)
        packed=bytes(indices) if bits==8""")
s=s.replace("transparency_index=0 if name=='wordmark' else None", "transparency_index=0 if bits==4 else None")
s=s.replace("nibble_order='high first' if bits==4 else None,roundtrip=True", "nibble_order='high first' if bits==4 else None,palette_mapping=mapping,roundtrip=True")
s=s.replace("    outputs[A/'manifest.json']", "    assert manifest['flash_bytes']<=128*1024,'asset allowance exceeded'\n    outputs[A/'manifest.json']")
s=s.replace('f" 5 assets:', 'f" {len(manifest[\'assets\'])} assets:')
p.write_text(s)
# End inscription is drawn from the approved atlas, not a replacement typeface.
from PIL import Image
A=p.parents[1]/'assets';atlas=Image.open(A/'source/inscription-atlas.png').convert('1');im=Image.new('RGB',(320,32))
text='COLOSSUS . LATENT . 2026';x=(320-len(text)*8)//2
for ch in text:
 n=ord(ch)-32;mask=atlas.crop((n%16*8,n//16*12,n%16*8+8,n//16*12+12));im.paste((184,168,144),(x,10,x+8,22),mask);x+=8
im.save(A/'source/end-inscription-master.png')
