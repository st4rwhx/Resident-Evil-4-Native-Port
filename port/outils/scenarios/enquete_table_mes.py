# -*- coding: utf-8 -*-
"""Enquete (Studio, 16/11) : qui ecrase la table de messages de type 2 (MesData.ptr[2]) avant son usage ?

    python -I enquete_table_mes.py <appareil> <binaire_non_epure>

Partie en cours en r100 (mode jeu). Saut vers r101 (le defaut s'y montre en quelques secondes). 1) arret quand
MesData.ptr[2] (MesData+12) change ; 2) a cet arret, surveillance des 16 premiers octets de la table pointee, avec arret ;
3) au second arret : le sous-systeme fautif (point ProcessTickGet), l'ancien et le nouveau contenu, l'image.
"""
import json
import os
import struct
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import studio  # noqa: E402

dev, binaire = sys.argv[1], sys.argv[2]
s, f = studio.connect(dev)
md = studio.adresse(f, 'MesData', binaire)
print(studio.ask(f, 'surveiller %x 4 arret' % (md + 12))[0], flush=True)
print(studio.ask(f, 'salle 1 101 0')[0], flush=True)
etape = 1
table = None
t0 = time.time()
while time.time() - t0 < 180:
    time.sleep(0.25)
    try:
        o = json.loads(studio.ask(f, 'ou')[0])
    except Exception as ex:   # noqa: BLE001
        print('connexion perdue (plantage ?)', ex, flush=True)
        break
    if not o['pause']:
        continue
    evs = [l for l in studio.ask(f, 'evenements')[1:] if 'surveillance' in l]
    for l in evs:
        print('  ', l[:230], flush=True)
    if etape == 1:
        p2 = struct.unpack('<I', studio.lire(f, md + 12, 4))[0]
        tb = studio.lire(f, p2, 16) if p2 > 0x80000000 else b''
        print('ptr[2] = %x a l image %d (%s) ; table %s' % (p2, o['image'], o['arret'], tb.hex(' ', 4)), flush=True)
        if tb and tb[0] == 6:   # table valide : on surveille son contenu
            studio.ask(f, 'oublier')
            print(studio.ask(f, 'surveiller %x 16 arret' % p2)[0], flush=True)
            table = p2
            etape = 2
        studio.ask(f, 'reprendre')
        continue
    # etape 2 : la table vient d'etre modifiee
    print('TABLE %x MODIFIEE a l image %d juste apres « %s »' % (table, o['image'], o['arret']), flush=True)
    print('  maintenant', studio.lire(f, table, 32).hex(' ', 4), flush=True)
    p2 = struct.unpack('<I', studio.lire(f, md + 12, 4))[0]
    print('  ptr[2] = %x' % p2, flush=True)
    break
studio.ask(f, 'oublier')
s.close()
