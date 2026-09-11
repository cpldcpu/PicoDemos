"""Gate the artifacts against real, complete runs of the exact release UF2."""
import hashlib,json,re,struct,subprocess
from pathlib import Path
from board_run import audit
ROOT=Path(__file__).resolve().parents[2]
uf2=ROOT/'tessera_vga_rp2350.uf2';raw=uf2.read_bytes();sha=hashlib.sha256(raw).hexdigest()
assert len(raw)%512==0
seen=set();end=0;ignored=0
for offset in range(0,len(raw),512):
    m0,m1,flags,address,length,number,total,family=struct.unpack_from('<8I',raw,offset)
    assert (m0,m1)==(0x0a324655,0x9e5d5157) and struct.unpack_from('<I',raw,offset+508)[0]==0x0ab16f30
    # Pico's converter prepends an ABSOLUTE-family compatibility block
    # explicitly tagged UF2_EXTENSION_RP2_IGNORE_BLOCK (boot/uf2.h). It is
    # not flashed. Bound every actual ARM payload, not this ignored header.
    if family==0xe48bff57:
        assert offset==0 and flags==0xa000 and address==0x10ffff00
        assert length==256 and number==0 and total==2
        assert struct.unpack_from('<I',raw,offset+32+length)[0]==0x9957e304
        ignored+=1;continue
    assert family==0xe48bff59 and flags==0x2000 and length==256
    assert 0x10000000<=address and address+length<=0x10400000, '4 MiB flash budget'
    assert total==len(raw)//512-1 and number not in seen;seen.add(number);end=max(end,address+length)
assert ignored==1 and seen==set(range(len(raw)//512-1))
reference=json.loads((ROOT/'validation/audio_reference.json').read_text(encoding='utf-8'))
runs=[]
for name in ('release_01','release_02'):
    result=json.loads((ROOT/'validation'/f'{name}.json').read_text(encoding='utf-8'))
    lines=(ROOT/'validation'/f'{name}.log').read_text(encoding='utf-8').splitlines()
    checked=audit(lines,reference)
    assert result['uf2_sha256']==sha and result['pcm_sha256']==reference['pcm_sha256']
    assert checked['passed'],checked['failures'];runs.append(checked)
    # Negative controls: a truncated run, a wrong PCM latch, a wrong image,
    # a real missed scanline and a repeated field must all fail the gate.
    mutants=[lines[:-1], [x.replace('5f7858ba','00000000') for x in lines],
             [x.replace(list(reference['visual_hashes'].values())[1],'00000000') for x in lines],
             [x.replace('missed 0','missed 1') for x in lines],
             [x.replace('repeat 0','repeat 1') for x in lines]]
    assert all(not audit(m,reference)['passed'] for m in mutants)
maptext=(ROOT/'tessera/build_rp2350/tessera.elf.map').read_text(encoding='utf-8')
def symbol(name):
    return int(re.search(r'(0x[0-9a-f]+)\s+(?:PROVIDE \()?'+re.escape(name)+r'\b',maptext)[1],16)
bss=symbol('__bss_end__');limit=symbol('__HeapLimit');flash=symbol('__flash_binary_end')-0x10000000
assert limit-bss>=24*1024,'scanvideo allocation and runtime heap headroom'
assert flash<=4*1024*1024
media=ROOT/'media/tessera.mp4'
probe=json.loads(subprocess.run(['ffprobe','-v','error','-show_format','-show_streams','-of','json',str(media)],capture_output=True,text=True,check=True).stdout)
video=next(s for s in probe['streams'] if s['codec_type']=='video');audio=next(s for s in probe['streams'] if s['codec_type']=='audio')
assert (video['width'],video['height'],video['avg_frame_rate'],int(video['nb_frames']))==(640,480,'60/1',9216)
assert audio['channels']==2 and int(audio['sample_rate'])==24000
assert abs(float(probe['format']['duration'])-153.6)<.05 and media.stat().st_size<95*1024*1024
source={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'tessera').rglob('*')) if p.is_file() and not any(part.startswith('build') or part=='__pycache__' for part in p.relative_to(ROOT/'tessera').parts) and p.suffix in ('.c','.h','.py','.txt')}
result=dict(uf2_sha256=sha,flash_image_bytes=flash,uf2_bytes=len(raw),static_main_sram_bytes=bss-0x20000000,
            heap_before_scanvideo_bytes=limit-bss,separate_stack_bytes=8192,
            movie_bytes=media.stat().st_size,movie_sha256=hashlib.sha256(media.read_bytes()).hexdigest(),
            movie_frames=9216,negative_controls_passed=10,runs=runs,source_sha256=source)
if (ROOT/'validation/reference_01.json').exists():
    comparison=json.loads((ROOT/'validation/reference_01.json').read_text(encoding='utf-8'))
    assert comparison['uf2_sha256']==hashlib.sha256((ROOT/'tessera_reference_vga_rp2350.uf2').read_bytes()).hexdigest()
    body=(ROOT/'validation/reference_01.log').read_text(encoding='utf-8')
    assert dict(re.findall(r'AHASH s=(\d+) (\w+)',body))==reference['hashes']
    assert comparison['visual_hashes']==reference['visual_hashes']
    assert f"ENDHASH s={reference['frames']} {reference['final_hash']}" in body
    result['software_reference']=comparison
(ROOT/'validation/release.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ('source_sha256','runs')},indent=2))
