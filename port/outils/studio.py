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
import struct
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
    t = re.sub(r'(?<![A-Za-z])(-?)inf', r'\1Infinity', t)
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


# ---- S2 (16/11) : noms de variables du jeu -> adresses (table des symboles du binaire NON epure, llvm-nm)
NM = os.environ.get('NM', r'C:/android/android-ndk-r26d/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-nm.exe')
_syms = None
_base = None


def symboles(binaire):
    global _syms
    if _syms is None:
        cache = binaire + '.syms.json'
        if os.path.exists(cache) and os.path.getmtime(cache) >= os.path.getmtime(binaire):
            _syms = json.load(open(cache))
        else:
            out = subprocess.run([NM, '-C', '--defined-only', binaire], capture_output=True, text=True, errors='replace').stdout
            _syms = {}
            for ln in out.splitlines():
                parts = ln.split(None, 2)
                if len(parts) == 3:
                    _syms.setdefault(parts[2], int(parts[0], 16))
            json.dump(_syms, open(cache, 'w'))
    return _syms


def base(f):
    global _base
    if _base is None:
        _base = int(json.loads(ask(f, 'base')[0])['base'], 16)
    return _base


def lire(f, a, n):
    r = json.loads(ask(f, 'lire %x %d' % (a, n))[0])
    if 'octets' not in r:
        raise SystemExit('lecture %x : %s' % (a, r))
    return bytes.fromhex(r['octets'])


def adresse(f, expr, binaire):
    """NOM | NOM+0x10 | *NOM+0x10 (pointeur lu puis decalage) | **NOM+8+0x20 | 0x8012abcd"""
    e = expr.strip()
    deref = 0
    while e.startswith('*'):
        deref += 1
        e = e[1:]
    parts = re.split(r'\+', e)
    head, offs = parts[0], [int(x, 0) for x in parts[1:]]
    if re.fullmatch(r'0x[0-9a-fA-F]+|[0-9a-fA-F]{8}', head):
        a = int(head, 16)
    else:
        sy = symboles(binaire)
        if head not in sy:
            cand = [k for k in sy if k.endswith(head)][:5]
            raise SystemExit('symbole inconnu %s (proches : %s)' % (head, cand))
        a = base(f) + sy[head]
    for i in range(deref):
        a = int.from_bytes(lire(f, a, 4), 'little') + (offs[i] if i < len(offs) else 0)
    for o in offs[deref:]:
        a += o
    return a


TYPES = {'f32': ('<f', 4), 'u32': ('<I', 4), 's32': ('<i', 4), 'u16': ('<H', 2), 's16': ('<h', 2), 'u8': ('<B', 1), 's8': ('<b', 1)}


def voir(f, expr, typ, n, binaire):
    a = adresse(f, expr, binaire)
    if typ == 'vec3':
        b = lire(f, a, 12 * n)
        return '%x : %s' % (a, [struct.unpack_from('<3f', b, 12 * i) for i in range(n)])
    if typ in TYPES:
        fmt, sz = TYPES[typ]
        b = lire(f, a, sz * n)
        return '%x : %s' % (a, [struct.unpack_from(fmt, b, sz * i)[0] for i in range(n)])
    return '%x : %s' % (a, lire(f, a, n).hex())


def poser(f, expr, typ, val, binaire):
    a = adresse(f, expr, binaire)
    fmt, sz = TYPES[typ]
    v = float(val) if typ == 'f32' else int(val, 0)
    return ask(f, 'ecrire %x %s' % (a, struct.pack(fmt, v).hex()))[0]


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
    binaire = os.environ.get('STUDIO_BIN', '')
    if '--binaire' in args:
        i = args.index('--binaire')
        binaire = args[i + 1]
        del args[i:i + 2]
    for cmd in args or ['etat']:
        if cmd.startswith('attendre '):   # attendre <secondes> : laisser jouer
            time.sleep(float(cmd.split()[1]))
            continue
        w = cmd.split()
        if w and w[0] == 'voir':          # voir <expr> [type] [nombre]   (type : f32 u32 s16 vec3 ... ; defaut octets)
            print(voir(f, w[1], w[2] if len(w) > 2 else 'x', int(w[3]) if len(w) > 3 else (16 if len(w) <= 2 else 1), binaire), flush=True)
            continue
        if w and w[0] == 'poser':         # poser <expr> <type> <valeur>
            print(poser(f, w[1], w[2], w[3], binaire), flush=True)
            continue
        if w and w[0] == 'guetter':       # guetter <expr> <octets> [arret] : qui modifie cette valeur ?
            print(ask(f, 'surveiller %x %s' % (adresse(f, w[1], binaire), ' '.join(w[2:])))[0], flush=True)
            continue
        if w and w[0] == 'adresse':
            print('%x' % adresse(f, w[1], binaire), flush=True)
            continue
        for line in ask(f, cmd):
            print(line, flush=True)
    s.close()


if __name__ == '__main__':
    main()
