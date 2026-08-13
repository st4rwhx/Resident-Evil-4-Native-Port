# afs_iso.py <iso> list|get <name...> <outdir> : lit BIO4DAT.AFS directement dans l'ISO PS2 (sans l'extraire)
import struct, sys, os

def iso_find(f, path):
    f.seek(16 * 2048); pvd = f.read(2048)
    root = pvd[156:156 + 34]
    lba, size = struct.unpack('<I', root[2:6])[0], struct.unpack('<I', root[10:14])[0]
    for part in path.split('/'):
        f.seek(lba * 2048); d = f.read(size); o = 0; found = None
        while o < len(d):
            l = d[o]
            if l == 0:
                o = (o // 2048 + 1) * 2048; continue
            nl = d[o + 32]; nm = d[o + 33:o + 33 + nl].decode('latin1').split(';')[0]
            if nm.upper() == part.upper():
                found = (struct.unpack('<I', d[o + 2:o + 6])[0], struct.unpack('<I', d[o + 10:o + 14])[0]); break
            o += l
        if not found: raise SystemExit('absent: ' + part)
        lba, size = found
    return lba * 2048, size

def afs_entries(f, base):
    f.seek(base); h = f.read(8)
    assert h[:3] == b'AFS', h
    n = struct.unpack('<I', h[4:8])[0]
    tab = f.read(n * 8 + 8)
    ents = [struct.unpack('<II', tab[i * 8:i * 8 + 8]) for i in range(n)]
    noff, nsz = struct.unpack('<II', tab[n * 8:n * 8 + 8])
    names = []
    if noff:
        f.seek(base + noff); nt = f.read(n * 0x30)
        names = [nt[i * 0x30:i * 0x30 + 32].split(b'\0')[0].decode('latin1') for i in range(n)]
    return [(names[i] if names else str(i), o, s) for i, (o, s) in enumerate(ents)]

iso = sys.argv[1]
with open(iso, 'rb') as f:
    base, _ = iso_find(f, 'DATA/BIO4DAT.AFS')
    ents = afs_entries(f, base)
    if sys.argv[2] == 'list':
        for n, o, s in ents: print(n, s)
    else:
        out = sys.argv[-1]; os.makedirs(out, exist_ok=True)
        want = set(x.lower() for x in sys.argv[3:-1])
        for n, o, s in ents:
            if n.lower() in want:
                f.seek(base + o); open(os.path.join(out, n), 'wb').write(f.read(s)); print('ok', n, s)
