# cmp3.py <gc.das> <notre_sortie> <ps2.dat> [entree] : mots que Capcom a retournes (GC->PS2 u32 ou u16) et que NOUS
# avons laisses tels quels ('O' = oubli). 'E' = nous avons retourne, Capcom non (souvent normal : table relue retournee
# par leur code). Seulement les entrees de meme taille GC/PS2 (meme disposition probable).
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmp_arc import arc, gc_block0
g = gc_block0(open(sys.argv[1], 'rb').read()); o = open(sys.argv[2], 'rb').read(); p = open(sys.argv[3], 'rb').read()
ag, ao, ap = arc(g, '>'), arc(o, '<'), arc(p, '<')
only = int(sys.argv[4], 0) if len(sys.argv) > 4 else None
for i in range(min(len(ag), len(ap))):
    if only is not None and i != only: continue
    A, B, C = ag[i], ao[i], ap[i]
    if not A or not C or A[1] != C[1]: continue
    G, O, P = g[A[0]:A[0]+A[1]], o[B[0]:B[0]+B[1]], p[C[0]:C[0]+C[1]]
    s = []
    for k in range(0, len(G) - 3, 4):
        gw, ow, pw = G[k:k+4], O[k:k+4], P[k:k+4]
        conv = pw != gw and (pw == gw[::-1] or pw == gw[1::-1] + gw[3:1:-1])
        if conv and ow == gw: s.append('O')
        elif not conv and pw == gw and ow != gw: s.append('E')
        elif ow == pw: s.append('.')
        else: s.append('?')
    s = ''.join(s)
    print('%2d (0x%02X) taille %6x : oublis %4d  en_plus %4d  autres %4d' % (i, 0x10+4*i, A[1], s.count('O'), s.count('E'), s.count('?')))
    if only is not None:
        for n in range(0, len(s), 64):
            if any(c in s[n:n+64] for c in 'OE?'): print('+%05x %s' % (n*4, s[n:n+64]))
