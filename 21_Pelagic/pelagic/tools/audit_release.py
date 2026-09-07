"""Audit concrete release artifacts; records no unmeasured hardware claims.

The `device` block below is the exception that proves the rule: every number
in it was read off a Pico 2 on the Pimoroni VGA Demo Base over USB serial on
2026-09-07, against the final score, and the raw logs it summarises are in
briefs/logs/. Nothing here is estimated. If the firmware is rebuilt and not
re-run on the board, the `device` block is stale and must be re-measured, not
adjusted.
"""
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
# The score sources the `device` block below was measured against. Updated by
# hand, and only when the board has actually been re-run against them.
MEASURED_SOURCES={
    'synth.c_sha256':'224efa2f37501f847b24b875afb6b1caa83a9b48165c2f2b8f493135f039e306',
    'song.c_sha256':'9d53b69e841282f2f3f40486cb7c23ca824146f4e5e125f470cbd9409706dd6c'}
LIVE_SOURCES={k:hashlib.sha256((root/'pelagic'/k.split('_')[0]).read_bytes()).hexdigest()
              for k in MEASURED_SOURCES}
build=root/'pelagic/build_rp2350'
exe=root/'pelagic/build_host/pelagic.exe'
uf2=(root/'pelagic_vga_rp2350.uf2').read_bytes()
binary=(build/'pelagic.bin').read_bytes()
assert len(binary)<=4*1024*1024-1536*1024, 'Flash budget exceeded'
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
mapping=(build/'pelagic.elf.map').read_text()
end=int(re.search(r'0x([0-9a-f]+)\s+__bss_end__',mapping)[1],16)
remaining=0x20080000-end
assert remaining>=24576+65536, 'Insufficient heap allowance for scanvideo'
with tempfile.TemporaryDirectory(prefix='pelagic-audit-') as tmp:
    wav=Path(tmp)/'music.wav'
    subprocess.run([str(exe),'--wav',str(wav)],check=True)
    with wave.open(str(wav),'rb') as f:
        assert (f.getframerate(),f.getnchannels(),f.getsampwidth(),f.getnframes())==(24000,2,2,3686400)
        samples=array.array('h',f.readframes(f.getnframes()))
    peak=max(abs(v) for v in samples)
    rms=math.sqrt(sum(v*v for v in samples)/len(samples))
    stereo=math.sqrt(sum((samples[i]-samples[i+1])**2 for i in range(0,len(samples),2))/(len(samples)//2))
    dc=sum(samples)/len(samples)
    assert 0<rms<18000 and 0<peak<32767 and stereo>0 and abs(dc)<200
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','stream=codec_type,width,height,r_frame_rate,sample_rate,channels,duration','-of','json',str(root/'media/pelagic.mp4')]))
video=next(s for s in probe['streams'] if s['codec_type']=='video')
audio=next(s for s in probe['streams'] if s['codec_type']=='audio')
assert (video['width'],video['height'],video['r_frame_rate'])==(640,480,'30/1')
assert (root/'media/pelagic.mp4').stat().st_size < 95*1024*1024, 'Capture exceeds repository file budget'
assert abs(float(video['duration'])-153.6)<.02 and abs(float(audio['duration'])-153.6)<.02
assert audio['channels']==2 and audio['sample_rate']=='24000'
host_check_output=subprocess.check_output([str(exe.with_name('pelagic_check.exe'))],text=True)
dummy_env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
subprocess.run([str(exe),'--frames','60'],env=dummy_env,check=True,capture_output=True)
report={
    'production':'PELAGIC / LATENT / 2026',
    'credits':{'scene_handle':'Phase','model':'GPT-6 Astra','critic':'Azure',
               'music':'Phosphor','integration_and_hardware':'Overscan',
               'models':'GPT-6 Astra + Claude Fable 5.1'},
    'score':'Phosphor score: 80 bars, E major at 125 BPM, up a tone from bar 64',
    'hardware_tested':True,
    'flash_image_bytes':len(binary),
    'flash_capacity_bytes':4*1024*1024,
    'flash_remaining_bytes':4*1024*1024-len(binary),
    'music_reserved_flash_bytes':1536*1024,
    'music_reserved_sram_bytes':65536,
    'uf2_bytes':len(uf2),
    'uf2_sha256':hashlib.sha256(uf2).hexdigest(),
    'main_sram_through_static_data_bytes':end-0x20000000,
    'main_sram_remaining_for_heap_bytes':remaining,
    'stack_bytes_in_each_scratch_bank':4096,
    'audio':{'frames':3686400,'sample_rate':24000,'channels':2,'peak':peak,'rms':round(rms,2),'dc':round(dc,2),'stereo_difference_rms':round(stereo,2)},
    'host_tests':['4609 guarded frames including endpoint','DAC pixel format','nonblank interior frames','black endpoint','visual seek independence','complete stereo block independence: 512 versus alternating 1, 2, 8, 997 frames','silence after the score','signed integer overflow trapping','SDL video and audio using dummy drivers'],
    'host_check_output':host_check_output.strip().splitlines(),
    'capture':probe['streams'],
    'device':{
        'measured':'2026-09-07, Pico 2 (RP2350) on the Pimoroni VGA Demo Base, 300 MHz at 1.20 V, USB CDC telemetry, final score',
        'logs':['briefs/logs/device-normal-final.log','briefs/logs/device-smooth-final.log',
                'briefs/logs/device-normal-block48.log',
                'briefs/logs/device-normal-staged.log','briefs/logs/device-smooth-staged.log',
                'briefs/logs/device-normal.log','briefs/logs/device-smooth.log','briefs/logs/device-silent-synth.log'],
        'run_seconds':153.6,
        'renderer':'environment() stages each source plate row into SRAM by DMA one output row ahead',
        'normal_build':{
            'frames_rendered':5226,
            'fps_mean':34.0,'fps_min_one_second_window':19.9,'fps_max_one_second_window':59.7,
            'render_mean_ms':24.2,'worst_frame_ms':45.46,
            'fps_by_section':{'0-7':59.7,'8-23':33.1,'24-39':29.8,'40-47':26.4,'48-63':29.8,'64-71':26.4,'72-79':42.4},
            'flash_image_bytes':1297040,'sram_remaining_for_heap_bytes':105040},
        'smooth_build':{
            'frames_rendered':1942,
            'fps_mean':12.6,'fps_min_one_second_window':7.4,'fps_max_one_second_window':19.9,
            'render_mean_ms':77.0,'worst_frame_ms':126.08,
            'fps_by_section':{'0-7':19.9,'8-23':14.9,'24-39':10.3,'40-47':9.3,'48-63':10.5,'64-71':9.3,'72-79':16.5},
            'render_cost_versus_normal':'2.9x to 3.9x',
            'flash_image_bytes':1297928,'sram_remaining_for_heap_bytes':97096},
        'render_passes_us_per_frame_mean':{
            'environment_before_staging':19007,'environment_after_staging':8998,
            'environment_sampled_entirely_from_sram':7829,
            'ray_mesh':13234,'everything_else':1900},
        'staging':{
            'saved_ms_per_frame':10.0,
            'sram_bytes':3840,
            'sram_bytes_smooth':7680,
            'visual_hash_unchanged':True,
            'visual_hash_point_sampled':'6e4f5dccac98d710',
            'visual_hash_filtered':'c4a8a3628acd4000',
            'configurations_checked':['SMOOTH=OFF INTERP=OFF','SMOOTH=OFF INTERP=ON','SMOOTH=ON INTERP=OFF','SMOOTH=ON INTERP=ON']},
        'audio':{
            'underruns':0,
            'underruns_smooth_build':0,
            'ring_frames':1024,
            'lowest_ring_fill_frames':624,
            'lowest_ring_fill_after_startup_frames':985,
            'audio_pump_cycles_per_sample_mean':2121,
            'audio_pump_cycles_per_sample_mean_smooth_build':1941,
            'audio_pump_cycles_per_sample_before_staging':1794,
            'audio_pump_cycles_per_sample_without_synth':395,
            'core1_load_percent':17.0,
            'core1_load_percent_smooth_build':15.5,
            'worst_audio_pump_us':366.19,
            'worst_audio_pump_us_smooth_build':300.14,
            'generated_scanline_period_us':63.6,
            'scanline_queue_buffers':12,
            'hash_latches_checked':151,'hash_latches_wrong':0,
            'hash_latches_checked_smooth_build':151,'hash_latches_wrong_smooth_build':0,
            'hash_source':'song_harness --hashes versus synth_hash_latch() on the device',
            'half_block_control':{
                'what':'the same final score built with BLOCK 48 instead of 24, whole run on the board, so the '
                       'half-block change is measured against itself and not against an older synth.c',
                'log':'briefs/logs/device-normal-block48.log',
                'block_48_cycles_per_sample':2055,'block_24_cycles_per_sample':2121,
                'block_48_worst_audio_pump_us':510.77,'block_24_worst_audio_pump_us':366.19,
                'block_48_core1_load_percent':16.4,'block_24_core1_load_percent':17.0,
                'host_output_identical':True,
                'note':'Half-blocks cut the worst single audio_pump() by 28% (511 to 366 us) for 3.2% more mean cost '
                       '(2,055 to 2,121 cycles per sample). The scanline-queue margin goes back from about 1.4x to '
                       'about 2x. Both builds ran the whole film with zero underruns.'},
            'core1_guideline_percent':15,'note':'17.0% is above Phase\'s 15% guideline and is accepted rather than chased. The synth itself did not get slower: the environment staging DMA competes with core 1 for the bus (1,794 cycles per sample before staging, 2,055 after), and half-blocks add a further 66 cycles per sample to halve the worst spike. Zero underruns in either build over the whole film, and the ring never fell below 985 of 1023 frames after startup.'},
        'score_cost_to_the_picture_ms_per_frame':0.02,
        'hot_functions_in_sram':['scanout','audio_pump','environment','triangle','render_block','synth_render'],
        'synth_static_sram_bytes':57464,
        'hot_function_sram_bytes':{'scanout':308,'audio_pump':184,'environment':1764,'triangle':944,
                                   'render_block':5360,'synth_render':3060},
        'score_sources_measured':dict(MEASURED_SOURCES,current_tree=LIVE_SOURCES,
            matches_current_tree=(MEASURED_SOURCES==LIVE_SOURCES),
            note='The device run, the UF2s and the host hash table all came from the MEASURED tree state. '
                 'If matches_current_tree is false the score has moved since the board was last run: the '
                 'audio block and media/pelagic.mp4 below belong to a different synth.c than the one on '
                 'disk, and the device numbers must be re-measured rather than adjusted.'),
        'note':'Frame rate is core 0 alone. A silent-synth build measured the score at +0.02 ms per frame.'},
    'unverified':['VGA picture quality and PWM audio quality judged by ear and eye (telemetry only reports timing)','runtime heap high-water mark']
}
(root/'media/validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
