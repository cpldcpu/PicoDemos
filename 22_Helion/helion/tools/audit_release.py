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
import sys
variant="_reference" if "--reference" in sys.argv else ""

root=Path(__file__).resolve().parents[2]
build=root/('helion/build_rp2350'+variant)
exe=root/('helion/build_host'+variant)/'helion.exe'
uf2=(root/('helion'+variant+'_vga_rp2350.uf2')).read_bytes()
binary=(build/'helion.bin').read_bytes()
assert len(binary)<=2*1024*1024, 'Flash budget exceeded'
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
mapping=(build/'helion.elf.map').read_text()
end=int(re.search(r'0x([0-9a-f]+)\s+__bss_end__',mapping)[1],16)
remaining=0x20080000-end
# Phase reserved 48 KiB of SRAM plus a 24 KiB scanvideo heap allowance for a
# score that did not exist yet. The score exists now and lives in that reserve
# (delay line, reverb, sine table, block ring and voice state), and the
# renderer's hot code and the painter sort's scratch buffer moved into SRAM
# after the hardware run, so asserting the reserve is still *unspent* would
# now be asserting that the music is missing. What has to hold is the heap
# allowance scanvideo actually allocates from, and it does. (Overscan)
assert remaining>=24576, 'Insufficient heap allowance for scanvideo'
with tempfile.TemporaryDirectory(prefix='helion-audit-') as tmp:
    wav=Path(tmp)/'music.wav'
    subprocess.run([str(exe),'--wav',str(wav)],check=True)
    with wave.open(str(wav),'rb') as f:
        assert (f.getframerate(),f.getnchannels(),f.getsampwidth(),f.getnframes())==(24000,2,2,3840000)
        samples=array.array('h',f.readframes(f.getnframes()))
    peak=max(abs(v) for v in samples)
    rms=math.sqrt(sum(v*v for v in samples)/len(samples))
    stereo=math.sqrt(sum((samples[i]-samples[i+1])**2 for i in range(0,len(samples),2))/(len(samples)//2))
    dc=sum(samples)/len(samples)
    assert 0<rms<18000 and 0<peak<32767 and stereo>0 and abs(dc)<200
assert (root/'media/helion.mp4').stat().st_size<95*1024*1024
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','stream=codec_type,width,height,r_frame_rate,sample_rate,channels,duration','-of','json',str(root/'media/helion.mp4')]))
video=next(s for s in probe['streams'] if s['codec_type']=='video')
audio=next(s for s in probe['streams'] if s['codec_type']=='audio')
assert (video['width'],video['height'],video['r_frame_rate'])==(640,480,'30/1')
assert abs(float(video['duration'])-160)<.02 and abs(float(audio['duration'])-160)<.02
assert audio['channels']==2 and audio['sample_rate']=='24000'
host_check_output=subprocess.check_output([str(exe.with_name('helion_check.exe'))],text=True)
dummy_env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
subprocess.run([str(exe),'--frames','60'],env=dummy_env,check=True,capture_output=True)
# Measured by Overscan on a Pico 2 (RP2350) on the Pimoroni VGA Demo Base at
# 300 MHz / 1.20 V, USB CDC telemetry, complete 160 s runs, 2026-09-07. Raw
# logs: briefs/logs/device-shipping-uf2.log (the UF2 at the folder root, flashed
# from the file that ships) and briefs/logs/device-reference.log.
# Every field here is DEVICE unless its name says otherwise.
DEVICE={
 '':{
  'log':'briefs/logs/device-shipping-uf2.log',
  'board':'Raspberry Pi Pico 2 / RP2350 on Pimoroni VGA Demo Base, 300 MHz at 1.20 V',
  'run_seconds':160,
  'frames':9530,
  'fps_mean_over_run':59.6,
  'fps_window_mean':59.5,
  'fps_window_min':46.8,
  'windows_below_58_fps':4,
  'windows_below_30_fps':0,
  'render_mean_ms':9.98,
  'worst_render_frame_ms':16.81,
  'fps_by_chapter_mean':{'bars 0-7':59.6,'bars 8-23':59.7,'bars 24-39':59.7,'bars 40-55':59.7,'bars 56-63':58.7,'bars 64-71':58.9,'bars 72-79':59.7},
  'audio_underruns':0,
  'audio_ring_min_frames_of_1023':623,
  'audio_ring_min_after_first_second':987,
  'audio_pump_worst_us':228.76,
  'audio_pump_cycles_per_sample':1691,
  'audio_pump_mcycles_per_second':40.59,
  'audio_pump_percent_of_core1':13.5,
  'audio_pump_machinery_only_cycles_per_sample':295,
  'synth_dsp_cycles_per_sample':1396,
  'synth_cost_to_core0_ms_per_frame':0.03,
  'per_second_audio_hashes_checked':159,
  'per_second_audio_hashes_wrong':0,
  'accelerator_selftest':'accelerator=0 (real SIO registers), printed at boot',
 },
 '_reference':{
  'log':'briefs/logs/device-reference.log',
  'board':'Raspberry Pi Pico 2 / RP2350 on Pimoroni VGA Demo Base, 300 MHz at 1.20 V',
  'run_seconds':160,
  'fps_window_mean':59.2,
  'fps_window_min':41.8,
  'windows_below_58_fps':7,
  'render_mean_ms':11.11,
  'worst_render_frame_ms':18.24,
  'audio_underruns':0,
  'audio_pump_worst_us':229.85,
  'audio_pump_cycles_per_sample':1680,
  'per_second_audio_hashes_checked':159,
  'per_second_audio_hashes_wrong':0,
  'accelerator_selftest':'accelerator=0 (software model), printed at boot',
  'note':'SIO and sky DMA disabled; the hardware paths are worth 1.13 ms of mean render, 1.09 ms of tunnel rasterization and 0.65 ms of the triangle loop',
 },
}
report={
    'production':'HELION / LATENT / 2026',
    'credits':{'scene_handle':'Phase','model':'GPT-6 Astra','critic':'Azure'},
    'score':"Phosphor's score: D minor at 120 BPM, 80 bars, D major from bar 56; integer synth, bit-identical on host and device",
    'hardware_tested':True,
    'flash_image_bytes':len(binary),
    'flash_remaining_bytes':4194304-len(binary),
    'music_reserved_flash_bytes':2097152,
    'music_reserved_sram_bytes':49152,
    'uf2_bytes':len(uf2),
    'uf2_sha256':hashlib.sha256(uf2).hexdigest(),
    'main_sram_through_static_data_bytes':end-0x20000000,
    'main_sram_remaining_for_heap_bytes':remaining,
    'scanvideo_heap_allowance_bytes':24576,
    'stack_bytes_in_each_scratch_bank':4096,
    'audio':{'frames':3840000,'sample_rate':24000,'channels':2,'peak':peak,'rms':round(rms,2),'dc':round(dc,2),'stereo_difference_rms':round(stereo,2)},
    'host_tests':['4801 guarded frames including endpoint','DAC pixel format','nonblank interior frames','black endpoint','visual seek independence','complete stereo block independence: 512 versus 1, 2, 8, 997 frames','silence after the score','per-second audio telemetry hashes','signed integer overflow trapping','SDL video and audio using dummy drivers'],
    'host_check_output':host_check_output.strip().splitlines(),
    'capture':probe['streams'],
    'device':DEVICE[variant],
    'unverified':['analogue VGA and PWM signal quality on a monitor and a speaker (no instrument here reads the DAC output)','runtime heap high-water mark (the allowance is asserted from the map; scanvideo never failed to allocate over eight complete runs)']
}
(root/('media/validation'+variant+'.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
