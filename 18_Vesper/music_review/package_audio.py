"""Render review listening files and measure the specific score defects."""
from pathlib import Path
import array
import hashlib
import json
import math
import subprocess
import sys
import wave

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent
RATE=24000
def read(path):
    with wave.open(str(path),'rb') as f:
        assert (f.getnchannels(),f.getsampwidth(),f.getframerate())==(2,2,RATE)
        samples=array.array('h',f.readframes(f.getnframes()))
        if sys.byteorder!='little': samples.byteswap()
        return samples

def write(path,samples):
    with wave.open(str(path),'wb') as f:
        f.setnchannels(2);f.setsampwidth(2);f.setframerate(RATE)
        if sys.byteorder!='little':samples= samples[:];samples.byteswap()
        f.writeframes(samples.tobytes())

def rms(a):return math.sqrt(sum(v*v for v in a)/len(a))
def encode(source,destination):
    subprocess.run(['ffmpeg','-y','-v','error','-i',str(source),'-ar','48000','-c:a','libmp3lame','-b:a','192k',str(destination)],check=True)

original=read(HERE/'original.wav')
candidate=read(HERE/'canticle_proposal.wav')
assert len(original)==len(candidate)==120*RATE*2
for a in (original,candidate):assert max(abs(v) for v in a)<32767 and rms(a)>1000

# Both excerpts cover score time 7.5–30 s: B-flat, F, C and their transitions.
start=int(7.5*RATE)*2;end=30*RATE*2
excerpts=[original[start:end],candidate[start:end]]
levels=[rms(a) for a in excerpts];target=min(levels)
gains=[target/level for level in levels]
comparison=array.array('h')
for k,(a,gain) in enumerate(zip(excerpts,gains)):
    n=len(a)//2;fade=240 # Ten milliseconds at the edit edges, to avoid clicks.
    for i in range(n):
        envelope=min(1,i/fade,(n-1-i)/fade)
        comparison.extend((round(a[i*2]*gain*envelope),round(a[i*2+1]*gain*envelope)))
    if k==0:comparison.extend([0]*(RATE*2))
write(HERE/'comparison.wav',comparison)
encode(HERE/'comparison.wav',HERE/'01_original_then_canticle.mp3')
encode(HERE/'canticle_proposal.wav',HERE/'02_canticle_full_proposal.mp3')

# Derive actual original events, including the even-step indexing and transposition.
note_names=['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B']
natural_minor={0,2,4,5,7,9,10}
old_table=[12,19,15,22,12,24,19,15,10,17,14,22,10,24,17,14]
roots=[38,38,34,34,41,41,36,36]
events=[]
for bar in range(16):
    root=roots[(bar//2)%8]
    for s in range(0,16,2):
        degree=old_table[(s+(bar%4)*2)%16]
        if root!=38 and degree in (14,15):degree+=1
        if root==34 and degree==22:degree=21
        note=root+24+degree
        events.append({'seconds':bar*1.875+s*.1171875,'chord_root':note_names[root%12],
                       'note':note_names[note%12]+str(note//12-1),'midi':note,
                       'outside_d_natural_minor':note%12 not in natural_minor})

# Check the explicitly composed notes against the claimed key and range.
source=(HERE/'synth_proposal.c').read_text()
import re
phrase=source.split('static const uint8_t melody[4][4][8]={')[1].split('};',1)[0]
phrase=re.sub(r'/\*.*?\*/','',phrase,flags=re.S)
new_notes=[int(n) for n in re.findall(r'\b\d+\b',phrase)]
assert len(new_notes)==128
assert all(n==0 or n%12 in natural_minor for n in new_notes)
assert min(n for n in new_notes if n)==64 and max(new_notes)==77

hashes=json.loads((HERE/'production_hashes.json').read_text())
integrated=(HERE/'integration.json').exists()
snapshot_matches=[]
for relative,expected in hashes.items():
    matches=hashlib.sha256((ROOT/relative).read_bytes()).hexdigest()==expected
    if matches:snapshot_matches.append(relative)
    if not integrated:assert matches, f'Production changed before integration: {relative}'
report={
    'status':'archived review; Canticle integrated' if integrated else 'proposal only; not integrated',
    'comparison':{'source_seconds':[7.5,30], 'original_seconds':[0,22.5],
                  'silence_seconds':[22.5,23.5], 'proposal_seconds':[23.5,46],
                  'rms_matching_gains':gains},
    'original':{'peak':max(abs(v) for v in original),'rms':rms(original)},
    'proposal':{'peak':max(abs(v) for v in candidate),'rms':rms(candidate),
                'key_note_check':'all non-rest melody notes in D natural minor',
                'melody_range':'E4 to F5','rest_slots_in_16_bar_phrase':new_notes.count(0)},
    'original_events_first_16_bars':events,
    'production_files_matching_original_snapshot':snapshot_matches
}
(HERE/'review_metrics.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='original_events_first_16_bars'},indent=2))
