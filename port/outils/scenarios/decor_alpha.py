# -*- coding: utf-8 -*-
"""Objets du decor (ObjMgr) qui ont des parties a texture alpha (feuillage) : vivants ? ou ? (Studio, 16/11)

    python -I decor_alpha.py <appareil> <binaire_non_epure>

Disposition clang : cManager pArray +4, nArray +8, size +0xC ; cUnit be_flag +4. cModel : pos +0x94, pInfo +0x15C ;
cModelInfo pData +0x0C ; ModelData nParts +0x1A, pParts +0x1C ; ModelPart : flags +0x0B (bit2 = texture alpha),
alphaTex +0x0E, alphaRef +0x14, size +0x18 (en-tete 0x20 puis le flux).
"""
import collections
import math
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev, binaire = sys.argv[1], sys.argv[2]
s, f = studio.connect(dev)
om = studio.adresse(f, 'ObjMgr', binaire)
arr, n, size = struct.unpack('<3I', studio.lire(f, om + 4, 12))
e = studio.etat(f)
L = (e['leon']['x'], e['leon']['y'], e['leon']['z'])
print('ObjMgr %x : %d travaux de %x octets ; Leon %s' % (om, n, size, [round(x) for x in L]), flush=True)
studio.ask(f, 'pause')
stats = collections.Counter()
lignes = []
for i in range(n):
    o = arr + i * size
    be = struct.unpack('<I', studio.lire(f, o + 4, 4))[0]
    if not be & 1:
        continue
    stats['vivants'] += 1
    pos = struct.unpack('<3f', studio.lire(f, o + 0x94, 12))
    info = struct.unpack('<I', studio.lire(f, o + 0x15C, 4))[0]
    if not 0x10000000 <= info < 0xFFFF0000:
        continue
    data = struct.unpack('<I', studio.lire(f, info + 0x0C, 4))[0]
    if not 0x10000000 <= data < 0xFFFF0000:
        continue
    np_ = struct.unpack('<H', studio.lire(f, data + 0x1A, 2))[0]
    pp = struct.unpack('<I', studio.lire(f, data + 0x1C, 4))[0]
    alpha, refs = 0, set()
    for k in range(min(np_, 64)):
        if not 0x10000000 <= pp < 0xFFFF0000:
            break
        h = studio.lire(f, pp, 0x20)
        if h[0x0B] & 4:
            alpha += 1
            refs.add(h[0x14])
        pp += 0x20 + struct.unpack_from('<I', h, 0x18)[0]
    d = math.dist(pos, L)
    if alpha:
        stats['avec alpha'] += 1
        lignes.append((d, i, be, np_, alpha, sorted(refs), pos))
studio.ask(f, 'reprendre')
print(dict(stats), flush=True)
for d, i, be, np_, alpha, refs, pos in sorted(lignes)[:25]:
    print('  obj %3d dist %7.0f be %08x parties %3d alpha %3d ref %s pos %s' % (i, d, be, np_, alpha, refs,
                                                                                [round(x) for x in pos]), flush=True)
s.close()
