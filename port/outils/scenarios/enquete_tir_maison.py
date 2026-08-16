# -*- coding: utf-8 -*-
"""Enquete : pourquoi le Ganado de la maison n'est blesse qu'au premier tir (Studio, 16/11).

    python -I enquete_tir_maison.py <appareil> <binaire_non_epure> [tirs=6]

A lancer apres juge_maison.py dans la meme partie (Ganado active, deja blesse une fois). Ennemis sans attaque
(DBG_EM_NO_ATK = flags_64 0x02000000), vie de Leon retablie, trajectoires dessinees (flags_60 0x1000). Avant chaque tir :
EatMgr.hitCheck (cSatMgr::hitCheck, masque 0x400000 comme GetWepTargetList2) sur la ligne poitrine de Leon -> poitrine du
Ganado : si le decor coupe la ligne, la distance du point touche et l'attribut sont rendus ; emLineAtCk sur la meme
ligne dit si le Ganado serait touche. Capture apres chaque tir.
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
tirs = int(sys.argv[3]) if len(sys.argv) > 3 else 6
POS, ROT, HP = 0x94, 0xA0, 0x320
S0, S1, SH, SN = 0x81F00000, 0x81F00010, 0x81F00020, 0x81F00030
s, f = studio.connect(dev)


def appeler(nom, *args):
    r = json.loads(studio.ask(f, 'appeler %x %s' % (studio.adresse(f, nom, binaire), ' '.join('0x%x' % (x & 0xFFFFFFFF) for x in args)))[0])
    if 'resultat' not in r:
        raise SystemExit('appel %s : %s' % (nom, r))
    return int(r['resultat'], 16)


def vec(a):
    return struct.unpack('<3f', studio.lire(f, a, 12))


lst = json.loads(studio.ask(f, 'ennemis')[0])['ennemis']
g = next(x for x in lst if x['id'] == '12')
b = json.loads(studio.ask(f, 'base')[0])
pl, pg = int(b['pPL'], 16), int(b['pG'], 16)
e1 = next(x for x in lst if x['n'] == 1)
blob = b''.join(studio.lire(f, pl + o, 2048) for o in range(0, 0x8000, 2048))
size = next(off for off in range(0x200, 0x8000 - 0x100, 4)
            if all(abs(a - c) < 2 for a, c in zip(struct.unpack_from('<3f', blob, off + POS), e1['pos'])))
em = pl + size * g['n']
eat = studio.adresse(f, 'EatMgr', binaire)
print('Ganado n%d %x vie %d ; EatMgr %x' % (g['n'], em, g['vie'], eat), flush=True)
studio.ask(f, 'drapeau 64 2000000 1')
studio.ask(f, 'drapeau 60 1000 1')
studio.ask(f, 'ecrire %x %s' % (pg + 0x4FA4, struct.pack('<H', 1200).hex()))
time.sleep(2.0)
for k in range(tirs):
    studio.ask(f, 'ecrire %x %s' % (pg + 0x4FA4, struct.pack('<H', 1200).hex()))
    L, G = vec(pl + POS), vec(em + POS)
    d = math.hypot(G[0] - L[0], G[2] - L[2])
    studio.ask(f, 'ecrire %x %s' % (pl + ROT + 4, struct.pack('<f', math.atan2(G[0] - L[0], G[2] - L[2])).hex()))
    # ligne d'essai : poitrine de Leon -> poitrine du Ganado, prolongee de 20 %
    p0 = (L[0], L[1] + 1450.0, L[2])
    p1 = (G[0] + (G[0] - L[0]) * 0.2, G[1] + 1200.0, G[2] + (G[2] - L[2]) * 0.2)
    studio.ask(f, 'pause'); time.sleep(0.1)
    studio.ask(f, 'ecrire %x %s' % (S0, struct.pack('<3f', *p0).hex()))
    studio.ask(f, 'ecrire %x %s' % (S1, struct.pack('<3f', *p1).hex()))
    attr = appeler('cSatMgr::hitCheck', eat, S0, S1, SH, SN, 0, 0x400000)
    hit = vec(SH)
    big = struct.unpack('<I', struct.pack('<f', 1e16))[0]
    part = appeler('emLineAtCk', em, S0, S1, big, 0)
    studio.ask(f, 'reprendre')
    dh = math.dist(p0, hit) if attr else -1
    studio.ask(f, 'pad 20 0 0 0 0 30'); time.sleep(0.8)
    studio.ask(f, 'pad 120 0 0 0 0 4'); time.sleep(0.05)
    studio.ask(f, 'capture'); time.sleep(0.8)
    hp = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
    print('tir %d : distance %.0f | decor : attr %x a %.0f (%s) | Ganado sur la ligne : %s | vie %d' % (
        k + 1, d, attr, dh, [round(x) for x in hit] if attr else '-', 'oui %x' % part if part else 'non', hp), flush=True)
studio.ask(f, 'pad 0 0 0 0 0 1')
for l in studio.ask(f, 'evenements')[1:]:
    if 'erreur' in l:
        print('  ', l[:160], flush=True)
s.close()
