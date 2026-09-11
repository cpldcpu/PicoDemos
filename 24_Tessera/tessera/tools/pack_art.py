"""Compile selected paintings to the VGA DAC format; no creative edits."""
from pathlib import Path
import hashlib,json
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[2]
text='/* Compiled from the art/ paintings by tools/pack_art.py. */\n#include "assets.h"\n'
manifest=[]
for filename,symbol in [('ceramic_garden.png','stage_day'),('ceramic_garden_night.png','stage_night')]:
    source=root/'art'/filename
    im=Image.open(source).convert('RGB').resize((320,240),Image.Resampling.LANCZOS)
    a=np.asarray(im,dtype=np.uint16)
    p=((a[:,:,0]>>3)|((a[:,:,1]&248)<<3)|((a[:,:,2]&248)<<8)).reshape(-1)
    text+=f'const uint16_t {symbol}[320*240] __attribute__((aligned(4)))={{\n'
    text+=''.join(' '+','.join(f'0x{int(v):04x}' for v in p[i:i+16])+',\n' for i in range(0,len(p),16))+'};\n'
    im.save(source.with_stem(source.stem+'_320'))
    manifest.append(dict(source=filename,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),width=320,height=240,flash_bytes=153600,format='RGB555: R 0..4, G 6..10, B 11..15; bit 5 clear'))
(root/'tessera/assets.c').write_text(text,encoding='utf-8')
(root/'art/packed.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('Two paintings compiled: 307200 bytes of flash, no extra framebuffer in SRAM')
