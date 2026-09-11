"""The crown's notch, measured: the strip of sky between the two plates.

Phase's floor is four native pixels through the whole move. A notch is not any
gap -- an ember leaves a one-pixel hole and a hood edge leaves a three-pixel
one. It is a *slot*: a run of sky, enclosed by structure on both sides, that
persists over at least eight consecutive rows at overlapping x. That is what
is measured here, and the number reported is the narrowest row of the tallest
such slot.
"""
import glob, os, struct, sys, zlib


def readpng(p):
    d = open(p, 'rb').read(); i = 8; idat = b''; w = h = 0
    while i < len(d):
        ln = struct.unpack('>I', d[i:i+4])[0]; t = d[i+4:i+8]; b = d[i+8:i+8+ln]
        if t == b'IHDR': w, h = struct.unpack('>II', b[:8])
        elif t == b'IDAT': idat += b
        i += 12 + ln
    raw = zlib.decompress(idat); st = 1 + w * 3
    return w, h, [raw[y*st+1:(y+1)*st] for y in range(h)]


def sky(px):
    r, g, b = px
    return b > r + 16 and b > 40


def enclosed_runs(row, w):
    runs = []; start = None
    for x in range(w):
        if sky((row[x*3], row[x*3+1], row[x*3+2])):
            if start is None: start = x
        else:
            if start is not None: runs.append((start, x)); start = None
    if start is not None: runs.append((start, w))
    return [(a, b) for a, b in runs if a > 0 and b < w and b - a <= 60]


worst = 10 ** 9
for f in sorted(glob.glob(sys.argv[1])):
    if '-3x' in f: continue
    w, h, rows = readpng(f)
    slots = []          # list of [rows, min width, x range]
    for y in range(h):
        for a, b in enclosed_runs(rows[y], w):
            for s in slots:
                if s['last'] == y - 1 and not (b <= s['a'] or a >= s['b']):
                    s['last'] = y; s['n'] += 1
                    s['min'] = min(s['min'], b - a)
                    s['a'], s['b'] = min(s['a'], a), max(s['b'], b)
                    break
            else:
                slots.append({'first': y, 'last': y, 'n': 1, 'min': b - a, 'a': a, 'b': b,
                              'widths': []})
            for s2 in slots:
                if s2['last'] == y and s2['a'] <= a and b <= s2['b']:
                    s2.setdefault('widths', []).append(b - a)
                    break
    tall = [s for s in slots if s['n'] >= 8]
    if tall:
        s = max(tall, key=lambda s: s['n'])
        ws = sorted(s.get('widths') or [s['min']])
        med = ws[len(ws) // 2]
        print("%-30s notch median %2d px, narrowest row %2d px, %d rows (%d..%d)"
              % (os.path.basename(f), med, s['min'], s['n'], s['first'], s['last']))
        worst = min(worst, med)
    else:
        print("%-30s NO SLOT of eight rows or more" % os.path.basename(f))
        worst = 0
print("\nnarrowest over the move: %d native pixels (Phase's floor is 4)" % worst)
