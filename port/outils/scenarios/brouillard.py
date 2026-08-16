# -*- coding: utf-8 -*-
"""Releve du brouillard a chaque etage, pour comparer deux appareils (Studio, 16/11).

    python -I brouillard.py <appareil> <binaire_non_epure>

1. le jeu : LightMgr.env.fog (cLightMgr+0x38 env, +0x08 fog : type, debut, fin, couleur) et hokanCnt (+0x190) ;
2. notre couche GX natif : (anonymous namespace)::P, champs fog_* (port/gxn_tev.inc), lus par leur position dans la
   structure : on cherche le motif (fog_start, fog_end) du jeu dans P pour situer les champs sans supposer la disposition.
"""
import json
import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev, binaire = sys.argv[1], sys.argv[2]
s, f = studio.connect(dev)
lm = studio.adresse(f, 'LightMgr', binaire)
pp = studio.adresse(f, '(anonymous namespace)::P', binaire)
studio.ask(f, 'pause')
time.sleep(0.2)
e = studio.etat(f)
t, st, en = struct.unpack('<iff', studio.lire(f, lm + 0x40, 12))
col = studio.lire(f, lm + 0x4C, 4).hex()
hok = studio.lire(f, lm + 0x190, 1)[0]
print('salle %s image %d' % (e['salle'], e['image']), flush=True)
print('jeu   : type %d debut %.1f fin %.1f couleur %s (interpolation restante %d)' % (t, st, en, col, hok), flush=True)
blob = studio.lire(f, pp, 512)
pos = -1
for off in range(0, 512 - 16, 4):
    a, b = struct.unpack_from('<ff', blob, off)
    if t and abs(a - st) < 0.5 and abs(b - en) < 0.5:
        pos = off
        break
if pos >= 0:
    fs, fe, fn, ff = struct.unpack_from('<ffff', blob, pos)
    print('gxn P : fog_set %d type %d debut %.1f fin %.1f near %.1f far %.1f couleur %s (a P+%#x)' % (
        blob[pos - 4], blob[pos - 3], fs, fe, fn, ff, blob[pos + 16:pos + 20].hex(), pos), flush=True)
else:
    print('gxn P : valeurs du jeu introuvables dans P (512 premiers octets)', flush=True)
studio.ask(f, 'reprendre')
print(json.dumps({'type': t, 'debut': st, 'fin': en, 'couleur': col}))
s.close()
