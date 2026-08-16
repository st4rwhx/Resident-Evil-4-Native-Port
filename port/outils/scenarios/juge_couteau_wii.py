# -*- coding: utf-8 -*-
"""Juge des commandes Wii du couteau (W3, 17/11) : visée automatique + coup par secousse.

    python -I juge_couteau_wii.py <appareil> <binaire_non_epure>

1. Verrouillage : Ganado de la maison (id 12) à ~1500 devant Leon, couteau sorti (L tenu, action 11) avec le réglage
   « wii » à 0 puis 1 ; cPlayer.pLockEm (+0x508) attendu 0 puis le Ganado.
2. Secousse : Ganado à ~450, couteau sorti, Studio « secousse 25 4 » (2,5 g pendant 4 images -> action 48) ; attendu : coup
   (pl.xFE = 2) avec le réglage, rien sans ; vie du Ganado relevée.
Verdict : OK / ECHEC / A_LIRE.
"""
import json, math, os, struct, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev, binaire = sys.argv[1], sys.argv[2]
POS, ROT, FL3C8, LOCK, HP = 0x94, 0xA0, 0x3C8, 0x508, 0x320
SCRATCH = 0x81F00000
LX, LZ = -82910.0, -38480.0
s, f = studio.connect(dev)


def appeler(nom, *args):
    r = json.loads(studio.ask(f, 'appeler %x %s' % (studio.adresse(f, nom, binaire), ' '.join('0x%x' % x for x in args)))[0])
    if 'resultat' not in r:
        raise SystemExit('appel %s : %s' % (nom, r))
    return int(r['resultat'], 16)


def poser(model, x, y, z, angy):
    studio.ask(f, 'ecrire %x %s' % (SCRATCH, struct.pack('<3f', x, y, z).hex()))
    appeler('cModel::setPos', model, SCRATCH)
    studio.ask(f, 'ecrire %x %s' % (model + ROT + 4, struct.pack('<f', angy).hex()))


def u32(a):
    return struct.unpack('<I', studio.lire(f, a, 4))[0]


def placer(dist):
    studio.ask(f, 'pad 0 0 0 0 0 20'); time.sleep(0.8)
    studio.ask(f, 'pause'); time.sleep(0.2)
    poser(pl, LX, 860.0, LZ, 1.5708)                  # Leon tourné vers +x
    poser(em, LX + dist, 860.0, LZ, -1.5708)          # Ganado en face
    studio.ask(f, 'reprendre')
    time.sleep(0.3)
    pg = int(json.loads(studio.ask(f, 'base')[0])['pG'], 16)
    studio.ask(f, 'ecrire %x %s' % (pg + 0x4FA4, struct.pack('<H', 1200).hex()))


lst = json.loads(studio.ask(f, 'ennemis')[0])['ennemis']
g = next((x for x in lst if x['id'] == '12'), None)
if not g:
    raise SystemExit('VERDICT INVALIDE (Ganado de la maison absent)')
pl = int(json.loads(studio.ask(f, 'base')[0])['pPL'], 16)
e1 = next(x for x in lst if x['n'] == 1)
blob = b''.join(studio.lire(f, pl + o, 2048) for o in range(0, 0x8000, 2048))
size = next(off for off in range(0x200, 0x8000 - 0x100, 4)
            if all(abs(a - b) < 2 for a, b in zip(struct.unpack_from('<3f', blob, off + POS), e1['pos'])))
em = pl + size * g['n']
print('Ganado de la maison : %x' % em, flush=True)
studio.ask(f, 'ecrire %x %s' % (em + FL3C8, struct.pack('<I', u32(em + FL3C8) | 1).hex()))   # activé (statut 5)
studio.ask(f, 'drapeau 64 2000000 1')   # ennemis sans attaque (outil Capcom) : Leon reste en garde au couteau

# 1. verrouillage
lockres = {}
for reg in (0, 1):
    studio.ask(f, 'reglage wii %d' % reg)
    placer(1500.0)
    studio.ask(f, 'ecrire %x %s' % (pl + LOCK, '00000000'))
    studio.ask(f, 'pad 40 0 0 0 0 120')
    lock = 0
    for _ in range(8):
        time.sleep(0.25)
        lock = lock or u32(pl + LOCK)
    lockres[reg] = lock
    print('verrouillage wii=%d : pLockEm %08x' % (reg, lock), flush=True)

# 2. secousse
coupres = {}
for reg in (0, 1):
    studio.ask(f, 'reglage wii %d' % reg)
    placer(450.0)
    hp0 = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
    studio.ask(f, 'pad 40 0 0 0 0 150'); time.sleep(1.0)        # couteau sorti et tenu
    hpa = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
    studio.ask(f, 'secousse 25 4')
    time.sleep(1.5)
    hp1 = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
    coupres[reg] = hp1 < hpa
    print('secousse wii=%d : vie Ganado au placement %d, avant secousse %d, apres %d -> %s' % (
        reg, hp0, hpa, hp1, 'TOUCHE' if hp1 < hpa else 'rien'), flush=True)
    studio.ask(f, 'pad 0 0 0 0 0 30'); time.sleep(1.2)
studio.ask(f, 'pad 0 0 0 0 0 5')
studio.ask(f, 'reglage wii 0')
studio.ask(f, 'drapeau 64 2000000 0')
ok1 = lockres[1] == em and lockres[0] != em
ok2 = coupres[1] and not coupres[0]
print('VERDICT %s (verrouillage %s, coup par secousse %s)' % ('OK' if ok1 and ok2 else 'A_LIRE', ok1, ok2), flush=True)
s.close()
