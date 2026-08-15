# -*- coding: utf-8 -*-
"""Client du Nightjar Studio (S1, 15/11) : parle au coeur natif (port/studio.cpp) sur 127.0.0.1:7770.

    python -I studio.py [--appareil 127.0.0.1:16384] <commande> [<commande> ...]
    python -I studio.py etat
    python -I studio.py "outil AREA JUMP"
    python -I studio.py "pad 0 0 90 0 0 60" etat          (Leon avance 2 s puis etat)
    python -I studio.py --suivre                           (evenements toutes les secondes, Ctrl+C pour arreter)

Pose `adb forward tcp:7770 tcp:7770` sur l'appareil choisi (MuMu par defaut ; le S5 : --appareil 51b3af08).
"""
import json
import re
import os
import socket
import subprocess
import sys
import time

ADB = os.environ.get('ADB', r'C:\Android\platform-tools\adb.exe')


def connect(dev):
    subprocess.run([ADB, '-s', dev, 'forward', 'tcp:7770', 'tcp:7770'], check=True, capture_output=True)
    s = socket.create_connection(('127.0.0.1', 7770), timeout=5)
    s.settimeout(3)
    f = s.makefile('rwb')
    hello = f.readline()
    if not hello:
        raise SystemExit('le jeu ne repond pas (Studio absent, ou jeu pas encore dans sa boucle)')
    return s, f


def ask(f, cmd):
    f.write((cmd + '\n').encode())
    f.flush()
    out = [f.readline().decode(errors='replace').strip()]
    if cmd == 'evenements':   # une ligne d'en-tete puis une ligne par evenement
        try:
            n = json.loads(out[0]).get('evenements', 0)
        except ValueError:
            n = 0
        for _ in range(n):
            out.append(f.readline().decode(errors='replace').strip())
    return out


def etat(f):
    # Les flottants du jeu peuvent valoir nan/inf (camera degeneree, 15/11) : JSON les refuse, Python les accepte ecrits NaN.
    t = re.sub(r'(?<![A-Za-z])-?nan', 'NaN', ask(f, 'etat')[0])
    t = re.sub(r'(?<![A-Za-z])(-?)inf', r'Infinity', t)
    return json.loads(t)


def scenario(f, nom, sauter):
    """Scenarios pilotes par l'etat du jeu (pas par l'horloge). nouvelle_partie : titre -> NEW GAME -> r100 en jeu."""
    if nom != 'nouvelle_partie':
        raise SystemExit('scenario inconnu : nouvelle_partie')
    t0 = time.time()
    menu_fait = False
    while time.time() - t0 < 600:
        e = etat(f)
        bits = int(e['bits'], 16)
        ec = e['ecran']
        if e['mode'] == 1 and (bits & (1 << 11)):   # NJS_PLAYER
            print('en jeu : salle %s image %d (%.0f s)' % (e['salle'], e['image'], time.time() - t0), flush=True)
            return
        if bits & (1 << 9) and ec['titre_alpha'] > 0.89 and not menu_fait:   # menu du titre : haut (NEW GAME) puis A
            ask(f, 'pad 8 0 0 0 0 4'); time.sleep(0.6)
            ask(f, 'pad 100 0 0 0 0 4')
            menu_fait = True
            print('titre : NEW GAME', flush=True)
        elif e['mode'] == 6 and not (bits & (1 << 9)):   # logos / PRESS START
            ask(f, 'pad 1000 0 0 0 0 4')
        elif sauter and (bits & (1 << 1)):   # NJS_SKIPPABLE : cinematique passee
            ask(f, 'pad 1000 0 0 0 0 4')
        elif bits & (1 << 2):   # texte qui attend
            ask(f, 'pad 100 0 0 0 0 4')
        time.sleep(1.0)
    raise SystemExit('scenario : delai depasse')


def main():
    args = sys.argv[1:]
    dev = '127.0.0.1:16384'
    if '--appareil' in args:
        i = args.index('--appareil')
        dev = args[i + 1]
        del args[i:i + 2]
    s, f = connect(dev)
    if args == ['--suivre']:
        try:
            while True:
                for line in ask(f, 'evenements')[1:]:
                    print(line, flush=True)
                time.sleep(1)
        except KeyboardInterrupt:
            return
    if args and args[0] == '--scenario':
        scenario(f, args[1], '--sauter' in args)
        return
    for cmd in args or ['etat']:
        if cmd.startswith('attendre '):   # attendre <secondes> : laisser jouer
            time.sleep(float(cmd.split()[1]))
            continue
        for line in ask(f, cmd):
            print(line, flush=True)
    s.close()


if __name__ == '__main__':
    main()
