"""Render the actual synth, retain a complete PCM reference and measure the mix."""
import argparse, hashlib, json, subprocess, wave, re
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=Path, default=ROOT/'tessera/build_host/tessera.exe')
    a = p.parse_args()
    path = ROOT/'media/one_small_thing.wav'
    subprocess.run([str(a.exe), '--wav', str(path)], check=True)
    with wave.open(str(path)) as w:
        assert (w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()) == (24000,2,2,3686400)
        raw = w.readframes(w.getnframes())
    pcm = np.frombuffer(raw, dtype='<i2').reshape(-1,2)
    h = 2166136261; hashes = {}
    for i, v in enumerate(pcm.view('<u2').reshape(-1)):
        h = ((h ^ int(v)) * 16777619) & 0xffffffff
        if (i+1) % 48000 == 0:
            hashes[str((i+1)//2)] = f'{h:08x}'
    x = pcm.astype(float)
    result = dict(pcm_sha256=hashlib.sha256(raw).hexdigest(), sample_rate=24000,
                  frames=len(pcm), hashes=hashes, final_hash=f'{h:08x}',
                  peak=int(abs(x).max()), dc=x.mean(axis=0).tolist(),
                  rms_dbfs=float(20*np.log10(np.sqrt((x*x).mean())/32768)),
                  clipped_samples=int(np.count_nonzero(abs(x)>=32767)), sections=[])
    picture=subprocess.run([str(a.exe),'--vhash'],capture_output=True,text=True,check=True).stdout
    result['visual_hashes']=dict(re.findall(r'VHASH s=(\d+) (\w+)',picture))
    for i in range(6):
        a=x[i*614400:(i+1)*614400]
        result['sections'].append(dict(section=i,peak=int(abs(a).max()),rms_dbfs=float(20*np.log10(np.sqrt((a*a).mean())/32768))))
    assert result['clipped_samples']==0 and result['peak']>16000
    assert max(abs(v) for v in result['dc'])<50
    assert np.max(abs(x[-240:]))<200, 'ending must settle to silence'
    (ROOT/'validation/audio_reference.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='hashes'},indent=2))

if __name__=='__main__': main()
