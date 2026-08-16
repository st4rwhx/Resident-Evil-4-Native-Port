# -*- coding: utf-8 -*-
"""Amene une nouvelle partie au debut de r100 en jeu (cinematiques passees par START), puis attend N secondes.

    python -I vers_r100.py <appareil> [attente_s=8]
"""
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev = sys.argv[1]
attente = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0
s, f = studio.connect(dev)
t0 = time.time()
saut = False
while time.time() - t0 < 300:
    e = studio.etat(f)
    if e['salle'] == '100' and e['mode'] == 1:
        break
    if e['salle'] == '120' and e['mode'] == 1 and not saut:
        studio.ask(f, 'salle 1 100 0')
        saut = True
        time.sleep(8)
        continue
    if int(e['bits'], 16) & 2:
        studio.ask(f, 'pad 1000 0 0 0 0 4')
    time.sleep(1)
time.sleep(attente)
e = studio.etat(f)
print('salle %s mode %d image %d leon (%.0f, %.0f, %.0f)' % (e['salle'], e['mode'], e['image'], e['leon']['x'], e['leon']['y'],
                                                            e['leon']['z']), flush=True)
s.close()
