# -*- coding: utf-8 -*-
"""Juge du tir (Studio S5, 16/11) : un ennemi amene devant Leon perd-il de la vie quand on lui tire dessus ?

    python -I juge_tir.py <appareil> <id_hex> [distance] [hauteur_visee]

L'ennemi (premier vivant de cet identifiant) est place `distance` unites devant Leon (position ET oldPos, comme
cModel::setPos, jeu en pause), a la hauteur du sol de Leon ; Leon vise (R) et tire (R+A) trois fois. Verdict TOUCHE si
la vie baisse ou si l'ennemi meurt, sinon RATE (avec la vie, le statut et la position relevees).
"""
import json
import math
import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

POS, OLDPOS, ROT = 0x94, 0x110, 0xA0
dev, eid = sys.argv[1], sys.argv[2]
dist = float(sys.argv[3]) if len(sys.argv) > 3 else 1500.0
s, f = studio.connect(dev)


def trouver():
    for e in json.loads(studio.ask(f, 'ennemis')[0])['ennemis']:
        if e['id'] == eid:
            return e
    return None


def adresse_ennemi(n):
    # EmMgr : tableau de travaux de taille fixe ; on retrouve l'adresse en cherchant la position annoncee
    return None


e0 = trouver()
if not e0:
    raise SystemExit('ennemi %s absent' % eid)
pl = int(json.loads(studio.ask(f, 'base')[0])['pPL'], 16)
lx, ly, lz = struct.unpack('<3f', studio.lire(f, pl + POS, 12))
cap = struct.unpack('<f', studio.lire(f, pl + ROT + 4, 4))[0]
tx, tz = lx + dist * math.sin(cap), lz + dist * math.cos(cap)
# Adresse du travail de l'ennemi : Leon est le travail 0 de EmMgr (la liste du Studio commence a n=1). La taille d'un
# travail se retrouve en cherchant, apres Leon, la position annoncee de l'ennemi n=1 (sans symboles).
lst = json.loads(studio.ask(f, 'ennemis')[0])['ennemis']
e1 = next(x for x in lst if x['n'] == 1)
size = None
blob = b''.join(studio.lire(f, pl + o, 2048) for o in range(0, 0x8000, 2048))
for off in range(0x200, 0x8000 - 12, 4):
    x, y, z = struct.unpack_from('<3f', blob, off + POS)
    if abs(x - e1['pos'][0]) < 2 and abs(z - e1['pos'][2]) < 2 and abs(y - e1['pos'][1]) < 2:
        size = off
        break
if not size:
    raise SystemExit('taille de travail introuvable')
arr = pl
ea = arr + size * e0['n']
v = struct.pack('<3f', tx, ly, tz).hex()
studio.ask(f, 'pause'); time.sleep(0.15)
studio.ask(f, 'ecrire %x %s' % (ea + POS, v))
studio.ask(f, 'ecrire %x %s' % (ea + OLDPOS, v))
studio.ask(f, 'reprendre')
time.sleep(0.6)
print('ennemi %s n%d place devant Leon : %s' % (eid, e0['n'], trouver()), flush=True)
verdict = 'RATE'
for k in range(3):
    studio.ask(f, 'pad 20 0 0 0 0 40'); time.sleep(1.3)
    studio.ask(f, 'pad 120 0 0 0 0 6'); time.sleep(1.0)
    e = trouver()
    print('tir %d : %s' % (k + 1, e), flush=True)
    if e is None or e['vie'] < e0['vie']:
        verdict = 'TOUCHE'
        break
studio.ask(f, 'capture')
print('VERDICT', verdict, flush=True)
s.close()
