# -*- coding: utf-8 -*-
"""Juge du combat de la maison de r100 (Studio S5, 16/11) : le Ganado qu'on doit tuer pour avancer.

    python -I juge_maison.py <appareil> <binaire_non_epure> [tirs=8]

Reproduit la fin de r100_Sce_look (st1/r100.cpp) avec les fonctions du jeu, sans parcourir la carte :
  - le Ganado de la maison (id 12, routine em10_R1_R100WalkStay) et Leon places par cModel::setPos aux positions du code ;
  - le Ganado active comme le fait la scene (flags_3C8 |= 1) -> EmRoutineSet vers le combat ;
puis Leon vise et tire ; la vie (cEm+0x320), la routine (stat) et la distance sont relevees apres chaque tir. Le Ganado
s'approche pour attaquer : Leon est recadre sur lui avant chaque tir. Verdict : TUE / BLESSE / RATE / INVALIDE.
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
tirs = int(sys.argv[3]) if len(sys.argv) > 3 else 8
POS, ROT, HP, FL3C8, STAT = 0x94, 0xA0, 0x320, 0x3C8, 0xFC
SCRATCH = 0x81F00000
s, f = studio.connect(dev)


def appeler(nom, *args):
    r = json.loads(studio.ask(f, 'appeler %x %s' % (studio.adresse(f, nom, binaire), ' '.join('0x%x' % x for x in args)))[0])
    if 'resultat' not in r:
        raise SystemExit('appel %s : %s' % (nom, r))
    return int(r['resultat'], 16)


def vec(a):
    return struct.unpack('<3f', studio.lire(f, a, 12))


def boites(em):
    # liste EmHitInfo (cEm+0x33C, 0x34 octets, suivant en +0x30) : drapeaux de chaque boite (bit0 = active)
    out, p = [], em + 0x33C
    while p and len(out) < 24:
        b = studio.lire(f, p, 0x34)
        out.append('%04x' % struct.unpack_from('<H', b, 0x24)[0])
        p = struct.unpack_from('<I', b, 0x30)[0]
    return ' '.join(out)


def poser(model, x, y, z, angy):
    studio.ask(f, 'ecrire %x %s' % (SCRATCH, struct.pack('<3f', x, y, z).hex()))
    appeler('cModel::setPos', model, SCRATCH)
    studio.ask(f, 'ecrire %x %s' % (model + ROT + 4, struct.pack('<f', angy).hex()))


lst = json.loads(studio.ask(f, 'ennemis')[0])['ennemis']
g = next((x for x in lst if x['id'] == '12'), None)
if not g:
    raise SystemExit('VERDICT INVALIDE (Ganado de la maison absent)')
pl = int(json.loads(studio.ask(f, 'base')[0])['pPL'], 16)
# adresse du travail : Leon = travail 0 de EmMgr ; taille trouvee par la position de l'ennemi n=1
e1 = next(x for x in lst if x['n'] == 1)
blob = b''.join(studio.lire(f, pl + o, 2048) for o in range(0, 0x8000, 2048))
size = next(off for off in range(0x200, 0x8000 - 0x100, 4)
            if all(abs(a - b) < 2 for a, b in zip(struct.unpack_from('<3f', blob, off + POS), e1['pos'])))
em = pl + size * g['n']
print('Ganado de la maison : n%d %x vie %d stat %s' % (g['n'], em, g['vie'], g['stat']), flush=True)
studio.ask(f, 'pause'); time.sleep(0.2)
poser(em, -79116.0, 860.0, -38890.0, -1.39)
poser(pl, -82910.0, 860.0, -38480.0, 1.75)
fl = struct.unpack('<I', studio.lire(f, em + FL3C8, 4))[0]
studio.ask(f, 'ecrire %x %s' % (em + FL3C8, struct.pack('<I', fl | 1).hex()))
studio.ask(f, 'reprendre')
time.sleep(1.5)
hp0 = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
ex, ey, ez = vec(em + POS)
lx, ly, lz = vec(pl + POS)
st = studio.lire(f, em + STAT, 4).hex()
print('boites :', boites(em), flush=True)
print('apres activation : vie %d stat %s Ganado (%.0f, %.0f) Leon (%.0f, %.0f)' % (hp0, st, ex, ez, lx, lz), flush=True)
if math.hypot(lx + 82910, lz + 38480) > 600:
    print('VERDICT INVALIDE (Leon pas a sa place)', flush=True)
    sys.exit(2)
verdict = 'RATE'
hp_prev = hp0
for k in range(tirs):
    pg = int(json.loads(studio.ask(f, 'base')[0])['pG'], 16)
    studio.ask(f, 'ecrire %x %s' % (pg + 0x4FA4, struct.pack('<H', 1200).hex()))   # Leon garde sa vie (essai)
    ex, ey, ez = vec(em + POS)
    lx, ly, lz = vec(pl + POS)
    studio.ask(f, 'ecrire %x %s' % (pl + ROT + 4, struct.pack('<f', math.atan2(ex - lx, ez - lz)).hex()))
    studio.ask(f, 'pad 20 0 0 0 0 24'); time.sleep(0.7)
    studio.ask(f, 'pad 120 0 0 0 0 6'); time.sleep(0.9)
    hp = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
    st = studio.lire(f, em + STAT, 4).hex()
    dmg = studio.lire(f, em + 0x324, 4).hex()
    print('tir %d : vie %d stat %s dmg %s distance %.0f vie_leon %d' % (k + 1, hp, st, dmg, math.hypot(ex - lx, ez - lz),
                                                                       studio.etat(f)['leon']['vie']), flush=True)
    print('   boites :', boites(em), flush=True)
    if hp < hp_prev:
        verdict = 'BLESSE'
    hp_prev = hp
    if hp <= 0:
        verdict = 'TUE'
        break
studio.ask(f, 'capture')
studio.ask(f, 'pad 0 0 0 0 0 1')
print('VERDICT', verdict, flush=True)
s.close()
