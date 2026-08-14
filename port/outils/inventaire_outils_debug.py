# -*- coding: utf-8 -*-
"""Inventaire des outils de debug de Capcom (version DEBUG G4BE08) dans la decompilation (15/11).

Pour chaque fichier d'outil : fonctions definies, entrees de menu (chaines en majuscules), aides de boutons
("[A]", "STICK", "TRIGGER"...), fichiers lus/ecrits (chemins), et champs du jeu touches (pG->..., pPL->..., CamCtrl...).

    python -I port/outils/inventaire_outils_debug.py [fichier ...]
"""
import os
import re
import sys

SRC = os.path.join(os.path.dirname(__file__), '..', '..', 'src')
STR = re.compile(r'"((?:[^"\\\n]|\\.){3,80})"')
FUNC = re.compile(r'^[A-Za-z_][\w\s\*:<>,&]*?\b([A-Za-z_][\w:~]*)\s*\([^;{)]*\)\s*(?:const\s*)?\{', re.M)
FIELD = re.compile(r'\b(pG|pPL|pSys|CamCtrl|EvtMgr|EventMgr|LightMgr|EspMgr|EmMgr|Block|Cckpt|pRj|DC)\s*(?:->|\.)\s*(\w+)')


def classify(s):
    if s.endswith('.h') or 'asm(' in s or s.startswith('D:/Bio4/Prog') or s.startswith('('):
        return None
    if re.search(r'[/\\]', s) and re.search(r'\.\w{2,4}\b|%s', s):
        return 'fichier'
    if re.search(r'\[(A|B|X|Y|Z|L|R|START|ST)\]|STICK|TRIG|BUTTON|C-STICK|SUB-STICK|JOY', s):
        return 'touche'
    letters = re.sub(r'[^A-Za-z]', '', s)
    if len(letters) >= 3 and letters.upper() == letters:
        return 'menu'
    return None


def main():
    files = sys.argv[1:]
    for f in files:
        p = os.path.join(SRC, f)
        txt = open(p, encoding='utf-8', errors='replace').read()
        funcs = []
        for m in FUNC.finditer(txt):
            n = m.group(1)
            if n not in ('if', 'for', 'while', 'switch', 'return') and n not in funcs:
                funcs.append(n)
        cats = {'fichier': [], 'touche': [], 'menu': []}
        for m in STR.finditer(txt):
            s = m.group(1)
            c = classify(s)
            if c and s not in cats[c]:
                cats[c].append(s)
        fields = sorted({'%s.%s' % (a, b) for a, b in FIELD.findall(txt)})
        print('=== %s (%d lignes, %d fonctions)' % (f, txt.count('\n'), len(funcs)))
        print('  fonctions : ' + ', '.join(funcs[:25]) + (' ...' if len(funcs) > 25 else ''))
        for c in ('menu', 'touche', 'fichier'):
            if cats[c]:
                print('  %s : %s' % (c, ' | '.join(x.strip() for x in cats[c][:40])))
        if fields:
            print('  jeu : ' + ', '.join(fields[:30]))


if __name__ == '__main__':
    main()
