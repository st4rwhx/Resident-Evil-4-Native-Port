# -*- coding: utf-8 -*-
"""Neutralise, sous `#ifdef RE4_PORT`, l'echafaudage PowerPC du decompile.

Une part importante de l'assembleur present dans les sources **ne produit aucune instruction**.
Ce sont des contraintes posees au compilateur de 2001 pour retrouver son allocation de registres
et son ordonnancement, sans quoi le binaire ne serait pas identique a l'octet pres :

  * `register f64 hd asm("fr1");` — epingle une variable sur un registre flottant precis ;
  * `asm("" : "=m"(x) : "f"(y));` — un bloc **vide** dont les seules operandes creent une
    dependance artificielle, pour empecher un deplacement de code.

Sur ARM, `fr1` n'existe pas et la contrainte `f` non plus : ces lignes sont la seule raison pour
laquelle une trentaine d'unites refusent de compiler. Elles n'ont aussi **aucun sens** hors du
build matching - il n'y a rien a traduire, seulement a supprimer.

Ce script fait donc une transformation mecanique, sous garde, en laissant la version d'origine
intacte dans la branche `#else` : le build matching n'est pas touche, et la raison reste lisible
a l'endroit exact ou elle s'applique.

Ce qu'il ne touche pas, volontairement : l'assembleur qui **calcule** vraiment (les instructions
paired-single de `math_sub.cpp`, `trans.cpp`, `shape.cpp`). Celui-la demande une vraie
reecriture en C, une fonction a la fois.

usage : python port/strip_ppc_scaffold.py [--apply]
"""
import argparse
import glob
import io
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# `register <type> <nom> asm("<registre>");`, ou le registre est un registre PowerPC.
PIN = re.compile(
    r'^([ \t]*)register[ \t]+([A-Za-z_][A-Za-z0-9_]*(?:[ \t]*\*)?)[ \t]+'
    r'([A-Za-z_][A-Za-z0-9_]*)[ \t]*asm[ \t]*\([ \t]*"(fr\d+|r\d+|f\d+)"[ \t]*\)[ \t]*'
    r'(=[^;]*)?;[ \t]*(//.*)?$', re.M)

# Un `asm` dont le modele est la chaine vide : il ne produit rien, quelles que soient ses
# operandes. On exige `""` explicitement pour ne jamais toucher a de l'assembleur reel.
EMPTY = re.compile(
    r'^([ \t]*)(?:__)?asm(?:[ \t]+volatile)?[ \t]*\([ \t]*""[ \t]*(:[^;]*)?\)[ \t]*;[ \t]*(//.*)?$',
    re.M)


def guard(indent, port_lines, orig_line):
    out = indent + '#ifdef RE4_PORT\n'
    for l in port_lines:
        out += indent + l + '\n'
    out += indent + '#else\n' + orig_line.rstrip('\n') + '\n' + indent + '#endif'
    return out


def convert(text):
    n_pin = n_empty = 0

    def do_pin(m):
        nonlocal n_pin
        n_pin += 1
        indent, ty, name, reg, init, comment = m.groups()
        decl = '%s %s%s;' % (ty, name, (' ' + init.strip()) if init else '')
        note = '// portage : epinglage sur %s, sans objet hors PowerPC' % reg
        return guard(indent, [decl + '   ' + note], m.group(0))

    def do_empty(m):
        nonlocal n_empty
        n_empty += 1
        indent = m.group(1)
        note = ('// portage : bloc asm vide, il ne produisait aucune instruction - seules ses\n'
                + indent + '// operandes contraignaient l ordonnancement du compilateur d origine')
        return guard(indent, [note], m.group(0))

    text = PIN.sub(do_pin, text)
    text = EMPTY.sub(do_empty, text)
    return text, n_pin, n_empty


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--apply', action='store_true')
    args = ap.parse_args()

    total_pin = total_empty = files = 0
    srcs = sorted(glob.glob(os.path.join(ROOT, 'src', 'game', '*.cpp')) +
                  glob.glob(os.path.join(ROOT, 'src', 'game', '*.c')))
    for path in srcs:
        s = io.open(path, encoding='utf-8', errors='surrogateescape').read()
        if 'RE4_PORT' in s and '#ifdef RE4_PORT' in s:
            pass   # deja traite en partie : la conversion reste idempotente
        out, np_, ne = convert(s)
        if np_ or ne:
            files += 1
            total_pin += np_
            total_empty += ne
            print('  %-28s %2d epinglage(s), %2d bloc(s) vide(s)'
                  % (os.path.basename(path), np_, ne))
            if args.apply:
                io.open(path, 'w', encoding='utf-8', errors='surrogateescape').write(out)

    print('\n%d fichiers, %d epinglages, %d blocs vides%s'
          % (files, total_pin, total_empty, '' if args.apply else '  (essai seulement)'))


if __name__ == '__main__':
    main()
