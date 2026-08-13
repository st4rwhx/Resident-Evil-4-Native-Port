# cmp_arc.py <gc.das> <ps2.dat> [entree] : compare l'ArcFile GameCube (bloc 0 du conteneur .das, gros-boutiste)
# a celui du port PS2 de Capcom (petit-boutiste). Par entree : tailles, et pour chaque mot de 4 octets la
# conversion que Capcom a faite : '=' octets identiques, 'L' u32 retourne, 'h' deux u16 retournes, '?' autre.
import struct, sys

def gc_block0(d):
    if d[:4] == bytes.fromhex('cab6be20'):
        size, off = struct.unpack('>I', d[0x24:0x28])[0], struct.unpack('>I', d[0x2c:0x30])[0]
        return d[off:off + size]
    return d

def arc(d, end):
    n = struct.unpack(end + 'I', d[:4])[0]
    offs = list(struct.unpack(end + '%dI' % n, d[0x10:0x10 + 4 * n]))
    nz = sorted(set(o for o in offs if o))
    out = []
    for o in offs:
        if not o: out.append(None); continue
        nxt = [x for x in nz if x > o]
        out.append((o, (nxt[0] if nxt else len(d)) - o))
    return out

def classify(g, p):
    s = []
    for k in range(0, min(len(g), len(p)) - 3, 4):
        a, b = g[k:k + 4], p[k:k + 4]
        if a == b: s.append('=')
        elif a[::-1] == b: s.append('L')
        elif a[1::-1] + a[3:1:-1] == b: s.append('h')
        else: s.append('?')
    return ''.join(s)

if __name__ == "__main__":
  pass
def _main():
    g = gc_block0(open(sys.argv[1], 'rb').read()); p = open(sys.argv[2], 'rb').read()
    ag, ap = arc(g, '>'), arc(p, '<')
    print('entrees GC %d PS2 %d' % (len(ag), len(ap)))
    only = int(sys.argv[3], 0) if len(sys.argv) > 3 else None
    for i in range(max(len(ag), len(ap))):
        if only is not None and i != only: continue
        eg = ag[i] if i < len(ag) else None; ep = ap[i] if i < len(ap) else None
        if not eg or not ep:
            print('%2d (0x%02X) GC %s PS2 %s' % (i, 0x10 + 4 * i, eg and hex(eg[1]), ep and hex(ep[1]))); continue
        c = classify(g[eg[0]:eg[0] + eg[1]], p[ep[0]:ep[0] + ep[1]])
        tag = 'MEME TAILLE' if eg[1] == ep[1] else 'tailles differentes'
        cnt = {x: c.count(x) for x in '=Lh?'}
        print('%2d (0x%02X) GC %7x PS2 %7x %-19s = %d L %d h %d ? %d' % (i, 0x10 + 4 * i, eg[1], ep[1], tag, cnt['='], cnt['L'], cnt['h'], cnt['?']))
        if only is not None: print(c[:2000])
if __name__ == "__main__":
    _main()
