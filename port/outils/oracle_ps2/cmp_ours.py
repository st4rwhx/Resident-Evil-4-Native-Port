# cmp_ours.py <notre_sortie> <ps2.dat> [entree] : notre ArcFile converti (petit-boutiste) contre celui de Capcom.
# par mot : '=' identique, 'L' encore gros-boutiste chez nous (u32 oublie), 'h' u16 oublies, 'x' autre difference.
import struct, sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
from cmp_arc import arc
o = open(sys.argv[1], 'rb').read(); p = open(sys.argv[2], 'rb').read()
ao, ap = arc(o, '<'), arc(p, '<')
only = int(sys.argv[3], 0) if len(sys.argv) > 3 else None
for i in range(min(len(ao), len(ap))):
    if only is not None and i != only: continue
    a, b = ao[i], ap[i]
    if not a or not b: continue
    g, q = o[a[0]:a[0] + a[1]], p[b[0]:b[0] + b[1]]
    s = []
    for k in range(0, min(len(g), len(q)) - 3, 4):
        x, y = g[k:k+4], q[k:k+4]
        s.append('=' if x == y else 'L' if x[::-1] == y else 'h' if x[1::-1] + x[3:1:-1] == y else 'x')
    s = ''.join(s)
    print('%2d (0x%02X) %-6s = %5d L %4d h %4d x %5d' % (i, 0x10 + 4*i, 'meme' if a[1] == b[1] else 'diff', s.count('='), s.count('L'), s.count('h'), s.count('x')))
    if only is not None: print(s[:3000])
