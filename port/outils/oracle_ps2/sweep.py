# sweep.py <dossier_salles_gc> <iso_ps2> : pour chaque salle GC (rXXX.das) : decompression, notre conversion, puis
#  1. etiquettes NON converties, fichiers sans convertisseur, messages « inattendu »
#  2. comparaison a trois avec le rXXX.dat du port PS2 de Capcom : mots que Capcom a retournes et pas nous, par etiquette
# Les FCV (valeurs lues en gros-boutiste par FccF32/FccS16, voulu) et DRA (outil de debug) sont comptes a part.
import os, sys, struct, subprocess, re, glob, collections
H = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, H)
from cmp_arc import arc
src, iso = sys.argv[1], sys.argv[2]
work = os.path.join(H, 'sweep'); os.makedirs(work, exist_ok=True)
rooms = sorted(glob.glob(os.path.join(src, '*', 'r???.das')))
names = [os.path.basename(r)[:-4] + '.dat' for r in rooms]
subprocess.run([sys.executable, '-I', os.path.join(H, 'afs_iso.py'), iso, 'get'] + names + [os.path.join(work, 'ps2')],
               stdout=subprocess.DEVNULL)
notconv = collections.Counter(); noconv = collections.Counter(); odd = collections.Counter()
oublis = collections.defaultdict(list)
for r in rooms:
    nm = os.path.basename(r)[:-4]
    d = open(r, 'rb').read()
    size, off = struct.unpack('>I', d[0x24:0x28])[0], struct.unpack('>I', d[0x2c:0x30])[0]
    b0 = os.path.join(work, nm + '_b0.bin'); dec = os.path.join(work, nm + '_dec.bin'); ours = os.path.join(work, nm + '_ours.bin')
    open(b0, 'wb').write(d[off:off + size])
    subprocess.run([os.path.join(H, 'unyz2.exe'), b0, dec], stdout=subprocess.DEVNULL)
    tr = subprocess.run([os.path.join(H, 'harness.exe'), 'room', dec, ours], capture_output=True, text=True,
                        env=dict(os.environ, RE4_TRACE_ENDIAN='1')).stdout
    tags = {}
    for l in tr.splitlines():
        m = re.match(r'\[ARC\] entree (\d+) : (\S+) a .*? (NON converti|converti)$', l)
        if m:
            tags[int(m.group(1))] = m.group(2)
            if m.group(3) == 'NON converti': notconv[m.group(2)] += 1
        elif 'sans convertisseur' in l:
            noconv[re.sub(r"'[^']*?(\.[a-z0-9]+)'", r'\1', re.sub(r'\(\d+ octets\)', '', l))] += 1
        elif 'inattendu' in l or 'deborde' in l or 'hors de' in l:
            odd[re.sub(r'0x[0-9a-f]+', 'X', l)] += 1
    p = os.path.join(work, 'ps2', nm + '.dat')
    if not os.path.exists(p): continue
    g = open(dec, 'rb').read(); o = open(ours, 'rb').read(); q = open(p, 'rb').read()
    try:
        ag, ao, ap = arc(g, '>'), arc(o, '<'), arc(q, '<')
    except Exception:
        continue
    for i in range(min(len(ag), len(ap))):
        A, B, C = ag[i], ao[i], ap[i]
        if not A or not C or A[1] != C[1]: continue
        G, O, P = g[A[0]:A[0]+A[1]], o[B[0]:B[0]+B[1]], q[C[0]:C[0]+C[1]]
        n = 0
        for k in range(0, len(G) - 3, 4):
            gw, ow, pw = G[k:k+4], O[k:k+4], P[k:k+4]
            if pw != gw and (pw == gw[::-1] or pw == gw[1::-1] + gw[3:1:-1]) and ow == gw: n += 1
        if n: oublis[tags.get(i, '?')].append((nm, i, n))
print('== etiquettes NON converties (nombre de salles)'); [print('  ', t, c) for t, c in notconv.most_common()]
print('== fichiers sans convertisseur'); [print('  ', c, t) for t, c in noconv.most_common(20)]
print('== messages inattendus'); [print('  ', c, t) for t, c in odd.most_common(20)]
print('== oublis contre Capcom (par etiquette : salles, mots)')
for t, l in sorted(oublis.items(), key=lambda x: -sum(n for _, _, n in x[1])):
    print('  %-4s %3d salles %7d mots   ex. %s' % (t, len(l), sum(n for _, _, n in l), ' '.join('%s#%d:%d' % x for x in l[:4])))
