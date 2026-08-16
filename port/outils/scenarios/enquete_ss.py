# -*- coding: utf-8 -*-
"""Enquete (Studio, 16/11) : la table de messages du sous-menu (MesData.ptr[2]) est-elle valide a l'ouverture ?

    python -I enquete_ss.py <appareil> <binaire_non_epure>

A lancer juste avant l'ouverture d'un sous-menu (ex. apres juge_maison : Leon ramasse l'objet lache). Arret au
changement de MesData.ptr[2] ; releve du decoupage de la zone du sous-menu (SubScreenWk.pBuf...) et de l'en-tete de la
table ; puis surveillance de la table (arret) et nouveau releve quand elle change.
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
ssw = studio.adresse(f, 'SubScreenWk', binaire)
cmes = studio.adresse(f, 'cMes', binaire)
NOMS = 'pBuf aramSize heapOfs relOfs cmmnOfs pzzlOfs relAddr pCmmn pPzzl x1E4 pItem pTerm pOpData pMapCmn pMapArea ' \
       'pFile pExam pShop pPartner pTplBuf x210'.split()


def releve(titre):
    o = json.loads(studio.ask(f, 'ou')[0])
    e = studio.etat(f)
    p = struct.unpack('<4I', studio.lire(f, md + 4, 16))
    v = struct.unpack('<21I', studio.lire(f, ssw + 0x1C0, 0x54))
    print('== %s : image %d apres %s, salle %s mode %d, ptr %s' % (titre, o['image'], o['arret'], e['salle'], e['mode'],
                                                               ' '.join('%x' % x for x in p)), flush=True)
    print('   ' + ' '.join('%s=%x' % (n, x) for n, x in zip(NOMS, v) if x), flush=True)
    print('   tas 12 = %x .. fin zone %x' % (v[0] + v[2], v[0] + 0x300000), flush=True)
    t = p[2]
    if 0x80000000 <= t < 0x83000000:
        print('   table %x : %s' % (t, studio.lire(f, t, 32).hex(' ', 4)), flush=True)
    for i in range(16):   # emplacements de messages actifs (cMes.mes[i] : flags +4, flags2 +0xC, attr +0x80, pMsg +0x98)
        m = studio.lire(f, cmes + 4 + i * 0xEC, 0xEC)
        fl, f2 = struct.unpack_from('<I', m, 4)[0], struct.unpack_from('<I', m, 0xC)[0]
        if fl & 1 or f2 & 1:
            print('   message %d : flags %x flags2 %x attr %08x pMsg %08x' % (
                i, fl, f2, struct.unpack_from('<I', m, 0x80)[0], struct.unpack_from('<I', m, 0x98)[0]), flush=True)
    for l in studio.ask(f, 'evenements')[1:]:
        if 'surveillance' in l or 'erreur' in l or 'MES' in l:
            print('   ', l[:220], flush=True)
    return p[2]


studio.ask(f, 'oublier')
print(studio.ask(f, 'surveiller %x 4 arret' % (md + 12))[0], flush=True)
etape, t0 = 1, time.time()
while time.time() - t0 < 400:
    time.sleep(0.3)
    try:
        o = json.loads(studio.ask(f, 'ou')[0])
    except Exception as ex:   # noqa: BLE001
        print('connexion perdue (plantage ?)', ex, flush=True)
        break
    if not o['pause']:
        continue
    t = releve('arret %d' % etape)
    if etape == 1 and 0x80000000 <= t < 0x83000000:
        studio.ask(f, 'oublier')
        print(studio.ask(f, 'surveiller %x 16 arret' % t)[0], flush=True)
        print(studio.ask(f, 'surveiller %x 16' % (cmes + 4))[0], flush=True)   # mes[0] : saveStop, flags, x8.., flags2
        print(studio.ask(f, 'surveiller %x 4' % (ssw + 0x34))[0], flush=True)  # SubScreenWk.x34

    etape += 1
    if etape > 4:
        break
    studio.ask(f, 'reprendre')
studio.ask(f, 'oublier')
s.close()
