# etm_cmp.py <ours> <ps2> <tag_index> : fichiers nommes de l'ETM, compares par nom (taille, octets identiques ?)
import struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cmp_arc import arc
def files(d, i):
    a = arc(d, '<')[i]; b = d[a[0]:a[0]+a[1]]
    n = struct.unpack('<I', b[:4])[0]; f = 0x20; out = {}
    for _ in range(n):
        sz = struct.unpack('<I', b[f:f+4])[0]; nm = b[f+0x20:f+0x40].split(b'\0')[0].decode('latin1')
        out[nm] = b[f+0x40:f+sz]; f += sz
    return out
o = files(open(sys.argv[1],'rb').read(), int(sys.argv[3])); p = files(open(sys.argv[2],'rb').read(), int(sys.argv[3]))
for nm in sorted(o):
    if nm in p and not nm.endswith(('.bin', '.tpl')):
        A, B = o[nm], p[nm]
        print('%-16s %6d %6d %s' % (nm, len(A), len(B), 'IDENTIQUE' if A == B else 'diff (%d octets)' % sum(x != y for x, y in zip(A, B))))
