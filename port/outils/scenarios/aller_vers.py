# -*- coding: utf-8 -*-
"""Aller vers un point en marchant (Studio, 16/11) : guidage en boucle fermee, sans teleportation.

RE4 se pilote « en char » : stick haut = avancer dans la direction de Leon. On ecrit sa direction (rot.y, qui tient)
vers la cible, on pousse le stick quelques images, on relit la position ; s'il n'avance plus (mur, objet), on essaie
de contourner (+/-45, +/-90 degres) avant de reprendre le cap. Utilisable seul ou importe (aller(f, x, z)).

    python -I aller_vers.py <appareil> <x> <z> [rayon]
"""
import json
import math
import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

ROT_Y = 0xA0 + 4   # cModel::rot.y (include/model.h)


def leon(f):
    l = studio.etat(f)['leon']
    return l['x'], l['z']


def regarder(f, pl, cap):
    studio.ask(f, 'ecrire %x %s' % (pl + ROT_Y, struct.pack('<f', cap).hex()))


def aller(f, tx, tz, rayon=1200.0, limite_s=180.0, journal=print):
    pl = int(json.loads(studio.ask(f, 'base')[0])['pPL'], 16)
    t0 = time.time()
    x, z = leon(f)
    bloque = 0
    while time.time() - t0 < limite_s:
        d = math.hypot(tx - x, tz - z)
        if d < rayon:
            studio.ask(f, 'pad 0 0 0 0 0 1')
            journal('arrive a %.0f de la cible (%.0f, %.0f)' % (d, x, z))
            return True
        cap = math.atan2(tx - x, tz - z)
        if bloque:
            cap += (math.pi / 4, -math.pi / 4, math.pi / 2, -math.pi / 2)[(bloque - 1) % 4]
        regarder(f, pl, cap)
        studio.ask(f, 'pad 0 0 90 0 0 24')
        time.sleep(0.9)
        nx, nz = leon(f)
        avance = math.hypot(nx - x, nz - z)
        bloque = bloque + 1 if avance < 60 else 0
        x, z = nx, nz
    studio.ask(f, 'pad 0 0 0 0 0 1')
    journal('pas arrive : encore %.0f (%.0f, %.0f)' % (math.hypot(tx - x, tz - z), x, z))
    return False


if __name__ == '__main__':
    s, f = studio.connect(sys.argv[1])
    ok = aller(f, float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]) if len(sys.argv) > 4 else 1200.0)
    s.close()
    sys.exit(0 if ok else 1)
