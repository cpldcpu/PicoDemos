"""Audit concrete release artifacts; records no unmeasured hardware claims."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import tempfile
import wave
import array
import math
import os

root=Path(__file__).resolve().parents[2]
build=root/'vesper/build_rp2350'
exe=root/'vesper/build_host/vesper.exe'
uf2=(root/'vesper_vga_rp2350.uf2').read_bytes()
binary=(build/'vesper.bin').read_bytes()
assert len(binary)<=65536, 'Flash budget exceeded'
assert len(uf2)%512==0
families=set()
image_blocks=[]
for i in range(0,len(uf2),512):
    block=uf2[i:i+512]
    m0,m1,flags,address,length,number,count,family=struct.unpack_from('<8I',block)
    assert (m0,m1)==(0x0a324655,0x9e5d5157)
    assert struct.unpack_from('<I',block,508)[0]==0x0ab16f30
    assert flags&0x2000 and length<=476
    families.add(family)
    if family==0xe48bff59:
        image_blocks.append((number,count,address,block[32:32+length]))
assert 0xe48bff59 in families, 'Missing RP2350 ARM secure image'
image_blocks.sort()
for i,(number,count,address,payload) in enumerate(image_blocks):
    assert number==i and count==len(image_blocks)
    assert address==0x10000000+i*256
payload=b''.join(b[3] for b in image_blocks)
assert payload[:len(binary)]==binary, 'UF2 and flash binary differ'
assert len(payload)-len(binary)<256 and not any(payload[len(binary):]), 'Unexpected UF2 tail padding'
mapping=(build/'vesper.elf.map').read_text()
end=int(re.search(r'0x([0-9a-f]+)\s+__bss_end__',mapping)[1],16)
remaining=0x20080000-end
assert remaining>=24576, 'Insufficient heap allowance for scanvideo'
with tempfile.TemporaryDirectory(prefix='vesper-audit-') as tmp:
    wav=Path(tmp)/'music.wav'
    subprocess.run([str(exe),'--wav',str(wav)],check=True)
    with wave.open(str(wav),'rb') as f:
        assert (f.getframerate(),f.getnchannels(),f.getsampwidth(),f.getnframes())==(24000,2,2,2880000)
        samples=array.array('h',f.readframes(f.getnframes()))
    peak=max(abs(v) for v in samples)
    rms=math.sqrt(sum(v*v for v in samples)/len(samples))
    stereo=math.sqrt(sum((samples[i]-samples[i+1])**2 for i in range(0,len(samples),2))/(len(samples)//2))
    dc=sum(samples)/len(samples)
    assert 2000<rms<18000 and 10000<peak<32767 and stereo>100 and abs(dc)<200
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','stream=codec_type,width,height,r_frame_rate,sample_rate,channels,duration','-of','json',str(root/'media/vesper.mp4')]))
video=next(s for s in probe['streams'] if s['codec_type']=='video')
audio=next(s for s in probe['streams'] if s['codec_type']=='audio')
assert (video['width'],video['height'],video['r_frame_rate'])==(960,720,'60/1')
assert abs(float(video['duration'])-120)<.02 and abs(float(audio['duration'])-120)<.02
assert audio['channels']==2 and audio['sample_rate']=='24000'
host_check_output=subprocess.check_output([str(exe.with_name('vesper_check.exe'))],text=True)
dummy_env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
subprocess.run([str(exe),'--frames','60'],env=dummy_env,check=True,capture_output=True)
report={
    'production':'VESPER / LATENT / 2026',
    'credits':{'scene_handle':'Phase','model':'GPT-6 Astra','critic':'Azure'},
    'score':'Canticle (approved revision)',
    'hardware_tested':False,
    'flash_image_bytes':len(binary),
    'uf2_bytes':len(uf2),
    'uf2_sha256':hashlib.sha256(uf2).hexdigest(),
    'main_sram_through_static_data_bytes':end-0x20000000,
    'main_sram_remaining_for_heap_bytes':remaining,
    'stack_bytes_in_each_scratch_bank':4096,
    'audio':{'frames':2880000,'sample_rate':24000,'channels':2,'peak':peak,'rms':round(rms,2),'dc':round(dc,2),'stereo_difference_rms':round(stereo,2)},
    'host_tests':['7201 guarded frames including endpoint','DAC pixel format','nonblank interior frames','black endpoint','visual seek independence','exact chapter boundaries','complete stereo block independence: 1 versus 997 frames','silence after the score','signed integer overflow trapping','SDL video and audio using dummy drivers'],
    'host_check_output':host_check_output.strip().splitlines(),
    'capture':probe['streams'],
    'unverified':['physical VGA and PWM output','device render frame rate','runtime heap high-water mark','audio ring minimum on device','host/device audio equivalence']
}
(root/'media/validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
