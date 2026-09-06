from pathlib import Path
p=Path(__file__).resolve().parents[1]/'tools/convert_assets.py';s=p.read_text();a=s.index('def convert(')
s=s[:a]+'''def paired_palette(dusk_path,dawn_path,size):
    """Fixed deterministic six-channel median partition, no random seed/state.
    Shared indices encode BOTH paintings so only palettes change at runtime.
    """
    dusk=Image.open(dusk_path).convert('RGB').resize(size,Image.Resampling.LANCZOS)
    dawn=Image.open(dawn_path).convert('RGB').resize(size,Image.Resampling.LANCZOS)
    pixels=[tuple(v&248 for v in a+b) for a,b in zip(dusk.getdata(),dawn.getdata())]
    hist={}
    for p in pixels:hist[p]=hist.get(p,0)+1
    groups=[sorted(hist)]
    while len(groups)<256:
        choices=[]
        for i,g in enumerate(groups):
            if len(g)<2:continue
            ranges=[max(p[k] for p in g)-min(p[k] for p in g) for k in range(6)]
            axis=max(range(6),key=lambda k:ranges[k]);choices.append((ranges[axis]*sum(hist[p] for p in g),i,axis))
        if not choices:break
        _,i,axis=max(choices);g=sorted(groups.pop(i),key=lambda p:(p[axis],p));half=sum(hist[p] for p in g)/2;acc=0;cut=1
        for j,pixel in enumerate(g[:-1]):
            acc+=hist[pixel];cut=j+1
            if acc>=half:break
        groups.extend([g[:cut],g[cut:]])
    lookup={};pa=[];pb=[]
    for i,g in enumerate(groups):
        count=sum(hist[p] for p in g);mean=tuple((sum(p[k]*hist[p] for p in g)//count)&248 for k in range(6))
        pa.append(mean[:3]);pb.append(mean[3:])
        for pixel in g:lookup[pixel]=i
    while len(pa)<256:pa.append(pa[-1]);pb.append(pb[-1])
    return [lookup[p] for p in pixels],pa,pb

'''+s[a:];s=s.replace('    shared={};outputs={};decl=', '''    pairs={}
    for kind,size in [('sky',(256,64)),('matcap',(64,64))]:
        d=SRC/('dusk-sky-master.png' if kind=='sky' else 'dusk-matcap-v3-master.png')
        a=SRC/('dawn-'+kind+'-master.png')
        pairs[kind]=paired_palette(d,a,size)
    shared={};outputs={};decl=''')
a=s.index('        mapping=None');b=s.index('        shared[name]=',a)
s=s[:a]+'''        mapping=None
        if name in ('dusk_sky','dawn_sky','dusk_matcap','dawn_matcap'):
            kind=name.split('_')[1];indices,dp,ap=pairs[kind];pal=ap if name.startswith('dawn') else dp
            mapping={'shared_indices':'dusk/dawn '+kind,'method':'deterministic paired RGB median partition, weighted means, floor to 5-bit DAC','dawn_source_sha256':sha((SRC/('dawn-'+kind+'-master.png')).read_bytes()),'dusk_source_sha256':sha((SRC/('dusk-sky-master.png' if kind=='sky' else 'dusk-matcap-v3-master.png')).read_bytes())}
        if name=='ember_stamps':
            # Preserve painted shape, normalize per-cell luminance for runtime
            # brightness classes, and use index 0 for the black surround.
            rgb=list(im.getdata());indices=[]
            for cell in range(4):
                lum=[max(p) for p in rgb[cell*256:(cell+1)*256]];peak=max(lum) or 1
                indices.extend(min(15,v*15//peak) for v in lum)
''' +s[b:]
p.write_text(s)
