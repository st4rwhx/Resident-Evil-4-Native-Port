# -*- coding: utf-8 -*-
"""Juge de la chaîne complète des commandes Wii au doigt (17/11) : appli Nightjar -> njlink -> coeur.

    python -I juge_glisser_wii.py <appareil> <binaire_non_epure> [x_couteau y_couteau] [--temoin]

Ganado de la maison à ~450 devant Leon (ennemis sans attaque), réglage « wii » ; un tap sur le bouton couteau tactile
(verrouillé), puis glisser rapide au doigt (adb input swipe) : l'appli envoie 2,5 g (touch_source.cpp wii_accel_g), le
coeur pose l'action 48 et Leon frappe. Verdict sur la vie du Ganado : OK (touché) / ECHEC.
"""
import json, os, struct, subprocess, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

TEMOIN = '--temoin' in sys.argv   # réglage « wii » coupé : le glisser ne doit rien faire
args = [a for a in sys.argv[1:] if a != '--temoin']
dev, binaire = args[0], args[1]
KX, KY = (int(args[2]), int(args[3])) if len(args) > 3 else (1488, 660)
POS, ROT, FL3C8, HP = 0x94, 0xA0, 0x3C8, 0x320
SCRATCH = 0x81F00000
LX, LZ = -82910.0, -38480.0
s, f = studio.connect(dev)


def adb(*a):
    subprocess.run(['adb', '-s', dev, 'shell', 'input'] + [str(x) for x in a], capture_output=True)


def appeler(nom, *args):
    r = json.loads(studio.ask(f, 'appeler %x %s' % (studio.adresse(f, nom, binaire), ' '.join('0x%x' % x for x in args)))[0])
    return int(r['resultat'], 16)


def poser(model, x, y, z, angy):
    studio.ask(f, 'ecrire %x %s' % (SCRATCH, struct.pack('<3f', x, y, z).hex()))
    appeler('cModel::setPos', model, SCRATCH)
    studio.ask(f, 'ecrire %x %s' % (model + ROT + 4, struct.pack('<f', angy).hex()))


def u32(a):
    return struct.unpack('<I', studio.lire(f, a, 4))[0]


lst = json.loads(studio.ask(f, 'ennemis')[0])['ennemis']
g = next(x for x in lst if x['id'] == '12')
pl = int(json.loads(studio.ask(f, 'base')[0])['pPL'], 16)
e1 = next(x for x in lst if x['n'] == 1)
blob = b''.join(studio.lire(f, pl + o, 2048) for o in range(0, 0x8000, 2048))
size = next(off for off in range(0x200, 0x8000 - 0x100, 4)
            if all(abs(a - b) < 2 for a, b in zip(struct.unpack_from('<3f', blob, off + POS), e1['pos'])))
em = pl + size * g['n']
studio.ask(f, 'ecrire %x %s' % (em + FL3C8, struct.pack('<I', u32(em + FL3C8) | 1).hex()))
studio.ask(f, 'drapeau 64 2000000 1')
studio.ask(f, 'reglage wii %d' % (0 if TEMOIN else 1))
studio.ask(f, 'pause'); time.sleep(0.2)
poser(pl, LX, 860.0, LZ, 1.5708)
poser(em, LX + 450.0, 860.0, LZ, -1.5708)
studio.ask(f, 'reprendre'); time.sleep(0.5)
adb('tap', 960, 540); time.sleep(0.6)                           # 1er toucher : réveille la couche tactile
for essai in range(3):                                          # un tap bascule le couteau (touch_lock = 0) : jusqu'à routine 2
    if studio.lire(f, pl + 0xFC, 1)[0] == 2:
        break
    adb('tap', KX, KY)
    time.sleep(1.2)
st = studio.lire(f, pl + 0xFC, 4).hex()
hp0 = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
print('couteau tactile : etat joueur %s, vie Ganado %d' % (st, hp0), flush=True)
etats = set()
for k in range(3):                                              # glissers rapides
    adb('swipe', 700, 250, 1350, 250, 40)
    for _ in range(4):
        etats.add(studio.lire(f, pl + 0xFC, 4).hex())
        time.sleep(0.1)
print('etats pendant les glissers :', sorted(etats), flush=True)
time.sleep(0.8)
hp1 = struct.unpack('<h', studio.lire(f, em + HP, 2))[0]
print('apres glisser : vie Ganado %d -> %d' % (hp0, hp1), flush=True)
if studio.lire(f, pl + 0xFC, 1)[0] == 2:
    adb('tap', KX, KY)                                          # deverrouille
studio.ask(f, 'reglage wii 0')
studio.ask(f, 'drapeau 64 2000000 0')
if TEMOIN:
    print('VERDICT TEMOIN %s' % ('OK (rien sans le réglage)' if hp1 == hp0 else 'ECHEC (coup sans le réglage)'), flush=True)
else:
    print('VERDICT %s' % ('OK (glisser -> coup)' if hp1 < hp0 else 'ECHEC'), flush=True)
s.close()
