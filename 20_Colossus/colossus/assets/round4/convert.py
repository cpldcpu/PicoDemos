"""Asset-only round-four converter. Frozen existing palettes; no C/H writes."""
from pathlib import Path
import json, hashlib, struct, argparse
import numpy as np
from PIL import Image
A=Path(__file__).resolve().parents[1]; O=Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def diffuse(rgb,pal,mask=None):
    a=np.array(rgb,dtype=float); h,w,channels=a.shape; p=np.array(pal,dtype=float); out=np.zeros((h,w),dtype=np.uint8)
    for y in range(h):
        for x in range(w):
            if mask is not None and not mask[y,x]:continue
            v=a[y,x]; start=1 if mask is not None else 0
            k=start+int(np.argmin(np.sum((p[start:]-v)**2,axis=1)));out[y,x]=k;e=v-p[k]
            for dx,dy,weight in [(1,0,7/16),(-1,1,3/16),(0,1,5/16),(1,1,1/16)]:
                xx,yy=x+dx,y+dy
                if 0<=xx<w and yy<h and (mask is None or mask[yy,xx]):a[yy,xx]+=e*weight
    return out
old=json.loads((A/'manifest.json').read_text()); entries=[dict(e) for e in old['assets'] if e['name']!='diagnostic_tile']
entries.append(dict(name='tendon_brushed_metal',width=32,height=32,bits=4,source='assets/source/tendon-brushed-metal-master.png',palette=['#%02x%02x%02x'%tuple((int(a+(b-a)*i/15)//8)*8 for a,b in zip((16,24,32),(216,224,224))) for i in range(16)],pixel_bytes=512,palette_bytes=32))
images={};pals={};masks={};outputs={}
for e in entries:
    n=e['name'];src=A.parent/e['source'];im=Image.open(src).convert('RGB');size=(e['width'],e['height'])
    if n=='wordmark':
        crop=im.crop(tuple(e['crop'])).resize((299,47),Image.Resampling.LANCZOS);im=Image.new('RGB',size,(16,24,32));im.paste(crop,(8,9));masks[n]=np.any(np.array(Image.open(A/'source/wordmark-drawn.png').convert('RGB'))!=[16,24,32],axis=2)
    else:im=im.resize(size,Image.Resampling.LANCZOS)
    if n in ('ember_stamps','end_inscription'):masks[n]=np.max(np.array(im),axis=2)>0
    images[n]=im;pals[n]=[tuple(bytes.fromhex(c[1:])) for c in e['palette']];e['source_sha256']=sha(src.read_bytes())
indices={}
for kind in ('sky','matcap'):
    a,b='dusk_'+kind,'dawn_'+kind
    # Six-channel error diffusion preserves one index image for palette animation.
    indices[a]=indices[b]=diffuse(np.concatenate([np.array(images[a]),np.array(images[b])],axis=2),[x+y for x,y in zip(pals[a],pals[b])])
for e in entries:
    n=e['name'];bits=e['bits'];pal=pals[n]
    if bits==1:
        raw=Image.open(A.parent/e['source']).convert('1').tobytes(); preview=Image.frombytes('1',(e['width'],e['height']),raw).convert('RGB');e['quantization']='binary drawn atlas; no tonal ramp'
    else:
        idx=indices[n] if n in indices else diffuse(images[n],pal,masks.get(n));flat=idx.ravel().tolist()
        raw=bytes(flat) if bits==8 else bytes((a<<4)|b for a,b in zip(flat[::2],flat[1::2]));decoded=list(raw) if bits==8 else [k for b in raw for k in (b>>4,b&15)]
        assert decoded==flat
        preview=Image.new('RGB',(e['width'],e['height']));preview.putdata([pal[k] for k in decoded]);e['quantization']='Floyd-Steinberg RGB, left-to-right, deterministic, seed 0 (no RNG); paired six-channel for sky/matcap'
    import io
    buf=io.BytesIO();preview.save(buf,format='PNG');outputs[n+'-quantized.png']=buf.getvalue()
    outputs[n+'.pixels.bin']=raw
    if bits!=1:
        assert all(all(v%8==0 for v in c) for c in pal)
        outputs[n+'.palette.bin']=b''.join(struct.pack('<H',(r>>3)|((g&248)<<3)|((b&248)<<8)) for r,g,b in pal)
    e.update(pixel_bytes=len(raw),pixel_sha256=sha(raw),preview_sha256=sha(buf.getvalue()),roundtrip=True,palette_bytes=0 if bits==1 else len(pal)*2)
    e.pop('palette_mapping',None)
manifest=dict(converter_version='round4-asset-only-1',seed=0,palette_policy='frozen round3 palettes; tendon fixed 16-entry steel ramp',shared_indices=['dusk_sky/dawn_sky','dusk_matcap/dawn_matcap'],assets=entries,flash_bytes=sum(e['pixel_bytes']+e['palette_bytes'] for e in entries),integration='packed binaries for Overscan; existing C/H unchanged')
outputs['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
check=argparse.ArgumentParser();check.add_argument('--check',action='store_true');check=check.parse_args().check
for n,b in outputs.items():
    if check:assert (O/n).read_bytes()==b,n
    else:(O/n).write_bytes(b)
print(('Verified' if check else 'Wrote'),len(entries),'assets;',manifest['flash_bytes'],'flash bytes')
