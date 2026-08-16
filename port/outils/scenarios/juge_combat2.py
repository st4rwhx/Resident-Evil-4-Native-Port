# -*- coding: utf-8 -*-
"""Juge du combat, version « appeler » (Studio S5, 16/11).

    python -I juge_combat2.py <appareil> <binaire_non_epure> [entree_liste_hex=25] [distance=1400]

1. cree le Ganado de la maison de r100 comme le fait r100_HouseEvent : EmSetFromList2(0x25, 0) ;
2. le place devant Leon avec la fonction du jeu cModel::setPos (vecteur ecrit dans une zone libre de MEM1) ;
3. verifie qu'il est valide (vie > 0, position atteinte) ; sinon verdict INVALIDE ;
4. Leon vise (R) et tire (R+A) jusqu'a 4 fois ; vie relue apres chaque tir (cEm::hp, +0x320).
Verdict : TOUCHE (la vie baisse), RATE (valide mais aucune blessure), INVALIDE (situation non atteinte).
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
entree = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x25
dist = float(sys.argv[4]) if len(sys.argv) > 4 else 1400.0
POS, ROT, HP = 0x94, 0xA0, 0x320
SCRATCH = 0x81F00000   # MEM1 reserve (32 Mo) au-dela de l'arene du jeu (0x81800000) : libre
s, f = studio.connect(dev)


def appeler(nom, *args):
    a = studio.adresse(f, nom, binaire)
    r = json.loads(studio.ask(f, 'appeler %x %s' % (a, ' '.join('0x%x' % x for x in args)))[0])
    if 'resultat' not in r:
        raise SystemExit('appel %s : %s' % (nom, r))
    return int(r['resultat'], 16)


def vec(a):
    return struct.unpack('<3f', studio.lire(f, a, 12))


pl = int(json.loads(studio.ask(f, 'base')[0])['pPL'], 16)
lx, ly, lz = vec(pl + POS)
cap = struct.unpack('<f', studio.lire(f, pl + ROT + 4, 4))[0]
studio.ask(f, 'pause'); time.sleep(0.2)
em = appeler('EmSetFromList2', entree, 0)
print('EmSetFromList2(0x%x) -> %x' % (entree, em), flush=True)
if em == 0 and os.environ.get('JUGE_EM'):   # deja cree (une seule fois par salle) : reutiliser son adresse
    em = int(os.environ['JUGE_EM'], 16)
    print('ennemi existant reutilise : %x' % em, flush=True)
if em < 0x10000000:
    studio.ask(f, 'reprendre')
    print('VERDICT INVALIDE (ennemi non cree)', flush=True)
    sys.exit(2)
tx, tz = lx + dist * math.sin(cap), lz + dist * math.cos(cap)
studio.ask(f, 'ecrire %x %s' % (SCRATCH, struct.pack('<3f', tx, ly, tz).hex()))
appeler('cModel::setPos', em, SCRATCH)
hp0 = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
studio.ask(f, 'reprendre')
time.sleep(0.8)
ex, ey, ez = vec(em + POS)
print('Ganado : vie %d, position (%.0f, %.0f, %.0f), cible (%.0f, %.0f)' % (hp0, ex, ey, ez, tx, tz), flush=True)
if hp0 <= 0 or math.hypot(ex - tx, ez - tz) > 800:
    print('VERDICT INVALIDE (vie ou position)', flush=True)
    sys.exit(2)
verdict = 'RATE'
for k in range(int(os.environ.get('JUGE_TIRS', '4'))):
    lx, ly, lz = vec(pl + POS)
    if os.environ.get('JUGE_REPLACER'):   # le replacer devant Leon avant chaque tir (il fuit apres un impact)
        cap = struct.unpack('<f', studio.lire(f, pl + ROT + 4, 4))[0]
        studio.ask(f, 'pause'); time.sleep(0.1)
        studio.ask(f, 'ecrire %x %s' % (SCRATCH, struct.pack('<3f', lx + dist * math.sin(cap), ly, lz + dist * math.cos(cap)).hex()))
        appeler('cModel::setPos', em, SCRATCH)
        studio.ask(f, 'reprendre'); time.sleep(0.2)
    # recadrer Leon vers l'ennemi
    ex, ey, ez = vec(em + POS)
    studio.ask(f, 'ecrire %x %s' % (pl + ROT + 4, struct.pack('<f', math.atan2(ex - lx, ez - lz)).hex()))
    studio.ask(f, 'pad 20 0 0 0 0 24'); time.sleep(0.7)
    studio.ask(f, 'pad 120 0 0 0 0 6'); time.sleep(0.9)
    hp = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
    print('tir %d : vie %d (distance %.0f)' % (k + 1, hp, math.hypot(ex - lx, ez - lz)), flush=True)
    if hp < hp0:
        verdict = 'TOUCHE'
        if not os.environ.get('JUGE_TOUS'):
            break
    hp0 = hp if os.environ.get('JUGE_TOUS') and hp < hp0 else hp0
studio.ask(f, 'capture')
studio.ask(f, 'pad 0 0 0 0 0 1')
print('VERDICT', verdict, flush=True)
s.close()
