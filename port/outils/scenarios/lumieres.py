# -*- coding: utf-8 -*-
"""Releve des lumieres du jeu (LightMgr) pour comparer deux appareils (Studio, 16/11).

    python -I lumieres.py <appareil> <binaire_non_epure> [sortie.json]

Disposition clang : le pointeur de table virtuelle est en tete, donc cManager a pArray +4, nArray +8, size +0xC
et cUnit a be_flag +4 (sur GameCube : +0, vptr apres les champs). Les champs de cLight a partir de 0x0C sont aux memes
places. Pour chaque lumiere vivante : type (0x0E), couleur de base (0x20), puissance (0x24), direction (0x38), couleur
appliquee (0x13C), position appliquee (0x144). Jeu mis en pause pendant la lecture.
"""
import json
import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev, binaire = sys.argv[1], sys.argv[2]
sortie = sys.argv[3] if len(sys.argv) > 3 else None
s, f = studio.connect(dev)
lm = studio.adresse(f, 'LightMgr', binaire)
studio.ask(f, 'pause')
time.sleep(0.2)
e = studio.etat(f)
arr, n, size = struct.unpack('<3I', studio.lire(f, lm + 4, 12))
print('salle %s image %d : LightMgr %x, %d lumieres de %x octets a %x' % (e['salle'], e['image'], lm, n, size, arr), flush=True)
out = []
for i in range(min(n, 256)):
    b = studio.lire(f, arr + i * size, 0x150)
    flag = struct.unpack_from('<I', b, 4)[0]
    if not flag & 1:
        continue
    d = {
        'i': i, 'be': '%x' % flag, 'type': b[0x0E], 'xD': b[0x0D], 'kind': b[0x29], 'parent': b[0x28],
        'couleur': b[0x20:0x24].hex(), 'puissance': round(struct.unpack_from('<f', b, 0x24)[0], 3),
        'pos': [round(x, 1) for x in struct.unpack_from('<3f', b, 0x10)],
        'dir': [round(x, 3) for x in struct.unpack_from('<3f', b, 0x38)],
        'appliquee': b[0x13C:0x140].hex(),
        'pos_appliquee': [round(x, 1) for x in struct.unpack_from('<3f', b, 0x144)],
    }
    out.append(d)
    print('  %3d type %2d kind %3d couleur %s x%.2f -> appliquee %s pos %s' % (
        i, d['type'], d['kind'], d['couleur'], d['puissance'], d['appliquee'], d['pos_appliquee']), flush=True)
studio.ask(f, 'reprendre')
print('%d lumieres vivantes' % len(out), flush=True)
if sortie:
    json.dump({'salle': e['salle'], 'image': e['image'], 'leon': e['leon'], 'lumieres': out}, open(sortie, 'w'), indent=1)
s.close()
