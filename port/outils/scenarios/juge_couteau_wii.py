# -*- coding: utf-8 -*-
"""Juge du couteau à visée automatique Wii (W3, 17/11) : réglage `wii_couteau` 0 puis 1, Ganado de la maison devant Leon.

    python -I juge_couteau_wii.py <appareil> <binaire_non_epure>

Place le Ganado de la maison (id 12) à ~1500 devant Leon et l'active (comme juge_maison), sort le couteau (L tenu) et lit
cPlayer.pLockEm (+0x508) et la visée de la caméra. Attendu : 0 sans réglage, le Ganado avec. Verdict : OK / ECHEC / INVALIDE.
"""
import json, math, os, struct, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev, binaire = sys.argv[1], sys.argv[2]
POS, ROT, FL3C8, LOCK = 0x94, 0xA0, 0x3C8, 0x508
SCRATCH = 0x81F00000
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
studio.ask(f, 'pause'); time.sleep(0.2)
poser(pl, -82910.0, 860.0, -38480.0, 1.6)                      # Leon tourné vers +x
poser(em, -82910.0 + 1500.0, 860.0, -38480.0 + 60.0, -1.57)    # Ganado à 1500 devant
studio.ask(f, 'ecrire %x %s' % (em + FL3C8, struct.pack('<I', u32(em + FL3C8) | 1).hex()))
studio.ask(f, 'reprendre')
time.sleep(1.0)
res = {}
for reg in (0, 1):
    studio.ask(f, 'reglage wii_couteau %d' % reg)
    pg = int(json.loads(studio.ask(f, 'base')[0])['pG'], 16)
    studio.ask(f, 'ecrire %x %s' % (pg + 0x4FA4, struct.pack('<H', 1200).hex()))
    studio.ask(f, 'ecrire %x %s' % (pl + LOCK, '00000000'))
    studio.ask(f, 'pad 40 0 0 0 0 150')                          # L tenu : couteau sorti (action 11)
    lock = 0
    for _ in range(10):                                         # relevé pendant la tenue (routine 2 = couteau)
        time.sleep(0.25)
        lock = lock or u32(pl + LOCK)
    e = studio.etat(f)
    cam = e['camera']['vise']
    ex, ey, ez = struct.unpack('<3f', studio.lire(f, em + POS, 12))
    print('wii_couteau=%d : pLockEm %08x (Ganado %x) mode %s etat_joueur %s vise %s Ganado (%.0f, %.0f, %.0f)' % (
        reg, lock, em, e['mode'], studio.lire(f, pl + 0xFC, 4).hex(), [round(c) for c in cam], ex, ey, ez), flush=True)
    res[reg] = lock
    studio.ask(f, 'pad 0 0 0 0 0 10'); time.sleep(0.5)
studio.ask(f, 'reglage wii_couteau 0')
if res[1] == em and res[0] != em:
    print('VERDICT OK (le couteau Wii verrouille le Ganado ; sans réglage, rien)', flush=True)
elif u32(pl + 0xFC) == 0 and res[1] == 0:
    print('VERDICT ECHEC (pas de cible avec le réglage)', flush=True)
else:
    print('VERDICT A_LIRE (%s)' % res, flush=True)
s.close()
