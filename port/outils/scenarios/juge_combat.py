# -*- coding: utf-8 -*-
"""Juge du combat (Studio S5, 16/11) : les tirs de Leon blessent-ils un ennemi ?

    python -I juge_combat.py <appareil> <binaire_non_epure> [id_ennemi_hex] [distance]

Sans parcourir la carte : Leon est place a `distance` de l'ennemi dans 8 directions tour a tour, tourne vers lui, vise
(R) et tire une balle (R+A) ; la vie de l'ennemi est relue apres chaque tir. Verdict :
  TOUCHE  : une direction au moins a fait baisser la vie (le combat marche) ;
  RATE    : 8 directions, aucune blessure (detection des tirs ou degats en cause, ou ennemi inactif) ;
Le jeu doit etre en salle, en mode jeu. Les decalages de pos/rot de Leon sont verifies contre `etat`.
"""
import json
import math
import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev, binaire = sys.argv[1], sys.argv[2]
eid = sys.argv[3] if len(sys.argv) > 3 else None
dist = float(sys.argv[4]) if len(sys.argv) > 4 else 1500.0
s, f = studio.connect(dev)


def ennemis():
    return json.loads(studio.ask(f, 'ennemis')[0])['ennemis']


def cible():
    for e in ennemis():
        if (eid is None and 0x10 <= int(e['id'], 16) <= 0x1F) or e['id'] == eid:
            return e
    return None


e0 = cible()
if not e0:
    raise SystemExit('pas d ennemi %s vivant dans la salle' % (eid or 'Ganado (0x10..0x1F)'))
eid = e0['id']
pl = int(json.loads(studio.ask(f, 'base')[0])['pPL'], 16)
leon = studio.etat(f)['leon']
offs = None
for po, ro in ((0x94, 0xA0), (0x48, 0x84)):
    x, y, z = struct.unpack('<3f', studio.lire(f, pl + po, 12))
    if abs(x - leon['x']) < 1 and abs(z - leon['z']) < 1:
        offs = (po, ro)
if not offs:
    raise SystemExit('decalage de la position de Leon introuvable')
ex, ey, ez = e0['pos']
vie0 = e0['vie']
print('cible : id %s vie %d pos %s' % (eid, vie0, e0['pos']), flush=True)
verdict = 'RATE'
for k in range(8):
    a = k * math.pi / 4
    lx, lz = ex + dist * math.sin(a), ez + dist * math.cos(a)
    # Comme cModel::setPos (model.cpp) : position ET ancienne position (oldPos, +0x110), jeu en pause ; sinon le
    # deplacement du joueur (« Player ») reprend l'ancienne (verifie au Studio le 16/11).
    studio.ask(f, 'pause'); time.sleep(0.15)
    v = struct.pack('<3f', lx, ey, lz).hex()
    studio.ask(f, 'ecrire %x %s' % (pl + offs[0], v))
    studio.ask(f, 'ecrire %x %s' % (pl + 0x110, v))
    studio.ask(f, 'reprendre')
    cap = math.atan2(ex - lx, ez - lz)
    studio.ask(f, 'ecrire %x %s' % (pl + offs[1] + 4, struct.pack('<f', cap).hex()))
    time.sleep(0.5)
    studio.ask(f, 'pad 20 0 0 0 0 40'); time.sleep(1.3)
    studio.ask(f, 'pad 120 0 0 0 0 6'); time.sleep(0.9)
    e = next((x for x in ennemis() if x['id'] == eid and abs(x['pos'][0] - ex) < 3000), None)
    vie = e['vie'] if e else None
    print('direction %d : vie %s stat %s' % (k, vie, e and e['stat']), flush=True)
    if vie is None or vie < vie0:
        verdict = 'TOUCHE'
        break
print('VERDICT', verdict, flush=True)
studio.ask(f, 'pad 0 0 0 0 0 1')
s.close()
