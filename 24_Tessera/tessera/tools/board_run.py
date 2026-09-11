"""Flash/verify/reboot, capture a WHOLE performance and reject missing evidence.

No claim from a partial log: BOOT, SIO selftest, all 153 second hashes, final
PCM hash, DONE and FINAL are mandatory. --replay audits an existing log.
"""
import argparse, datetime, hashlib, json, re, subprocess, time
from pathlib import Path
import serial

ROOT = Path(__file__).resolve().parents[2]

def audit(lines, reference, relaxed=False):
    body='\n'.join(lines); failures=[]
    def require(ok, why):
        if not ok: failures.append(why)
    require('BOOT TESSERA' in body, 'missing BOOT')
    require('SELFTEST accelerator=0' in body, 'missing passing SIO selftest')
    done=[s for s in lines if s.startswith('DONE ')]
    final=[s for s in lines if s.startswith('FINAL ')]
    require(len(done)==1 and len(final)==1, 'not exactly one complete film')
    found={p:h.lower() for p,h in re.findall(r'AHASH s=(\d+) ([0-9a-fA-F]{8})', body)}
    require(found==reference['hashes'], 'missing, extra or mismatching per-second PCM hashes')
    visual=dict(re.findall(r'VHASH s=(\d+) (\w+)',body))
    if 'visual_hashes' in reference:
        require(visual==reference['visual_hashes'],'full-frame hardware render checksums differ')
    require(f"ENDHASH s={reference['frames']} {reference['final_hash']}" in body,'whole-score PCM hash mismatch')
    counters={name:max([int(x) for x in re.findall(r'\b'+name+r' (\d+)',body)] or [-1]) for name in ('repeat','over','under','missed')}
    for name,value in counters.items(): require(value==0,f'{name}={value}')
    worst=max([float(x) for x in re.findall(r'worst render ([\d.]+) ms',body)] or [999])
    require(worst<15,'render exceeds 15 ms budget')
    fps=[float(x) for x in re.findall(r'\| fps ([\d.]+)',body)]
    require(len(fps)>=152,'missing timing windows')
    require(bool(fps) and min(fps)>=59.5,'frame-rate window below 59.5')
    frames=int(re.search(r'DONE frames (\d+)',body).group(1)) if done else 0
    require(9150<=frames<=9190,'unexpected whole-film frame count')
    result=dict(passed=not failures, failures=failures, checked_hashes=len(found), worst_render_ms=worst,
                counters=counters, frames=frames, fps_min=min(fps) if fps else None,
                final_hash=reference['final_hash'], visual_hashes=visual, done=done, final=final)
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--port',default='COM10')
    p.add_argument('--uf2',type=Path,default=ROOT/'tessera_vga_rp2350.uf2')
    p.add_argument('--name',default='run_01');p.add_argument('--replay',type=Path)
    p.add_argument('--reference',type=Path,default=ROOT/'validation/audio_reference.json')
    a=p.parse_args();reference=json.loads(a.reference.read_text(encoding='utf-8'))
    stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
    if a.replay: lines=a.replay.read_text(encoding='utf-8').splitlines()
    else:
        # -F enters BOOTSEL without auto-execution; -v verifies flash bytes.
        subprocess.run(['picotool','load','-F','-v',str(a.uf2)],check=True)
        info=subprocess.run(['picotool','info','-a'],capture_output=True,text=True,check=True).stdout
        (ROOT/'validation'/f'{a.name}_device.txt').write_text(info,encoding='utf-8')
        subprocess.run(['picotool','reboot'],check=True)
        deadline=time.monotonic()+180;port=None
        while time.monotonic()<deadline:
            try: port=serial.Serial(a.port,115200,timeout=.2);break
            except serial.SerialException: time.sleep(.1)
        if port is None: raise RuntimeError('CDC did not enumerate')
        lines=[];pending=b''
        with port, (ROOT/'validation'/f'{a.name}.log').open('w',encoding='utf-8') as log:
            while time.monotonic()<deadline:
                pending+=port.read(4096)
                while b'\n' in pending:
                    raw,pending=pending.split(b'\n',1);line=raw.decode('utf-8','replace').strip()
                    if line:
                        lines.append(line);log.write(line+'\n');log.flush();print(line,flush=True)
                if any(line.startswith('FINAL ') for line in lines[-2:]): break
    result=audit(lines,reference)
    result.update(utc=stamp,uf2_sha256=hashlib.sha256(a.uf2.read_bytes()).hexdigest(),
                  pcm_sha256=reference['pcm_sha256'],port=a.port)
    (ROOT/'validation'/f'{a.name}.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2),flush=True)
    if not result['passed']: raise SystemExit(1)

if __name__=='__main__': main()
