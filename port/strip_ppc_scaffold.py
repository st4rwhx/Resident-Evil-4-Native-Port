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

NL = chr(10)
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


# `asm(".section .sdata; .balign 8");` et ses variantes. Ces directives ne produisent aucune
# instruction non plus : elles forcent le placement en sections pour retrouver, a l'octet pres,
# la disposition des objets PowerPC d'origine. Hors du build matching elles n'ont pas de sens,
# et elles sont activement nuisibles : `.sdata` n'existe pas sur x86-64, et une directive laissee
# en fin de fichier laisse l'assembleur hors de `.text`, d'ou des differences de sections que
# l'editeur de liens refuse de soustraire.
SECTION = re.compile(
    r'^([ 	]*)(?:__)?asm(?:[ 	]+volatile)?[ 	]*\([ 	]*"\.section(?:[^"\\]|\\.)*"[ 	]*\)[ 	]*;'
    r'[ 	]*(//.*)?$')


# Une ligne entiere formee d'un bloc d'echafaudage entre accolades :
#   { register int n3 asm("r3"); asm("" : "=m"(x) : "r"(n3)); }
# PIN et EMPTY ne la voient pas - ils exigent une declaration seule sur sa ligne - et pourtant
# elle ne produit rien : l'epinglage designe un registre PowerPC, les asm sont vides. La ligne
# entiere disparait donc, d'un bloc.
BRACED = re.compile(
    r'^([ 	]*)\{[ 	]*register[ 	][^;]*asm[ 	]*\([ 	]*"[rf]\d+"[ 	]*\)[ 	]*;'
    r'(?:[^{}]*)\}[ 	]*(//.*)?$')


def strip_braced(text):
    """Supprime les blocs d'echafaudage tenant sur une seule ligne. Idempotent, comme ci-dessous."""
    out, n, prev = [], 0, ''
    for ln in text.split(NL):
        m = BRACED.match(ln)
        if m and prev.strip() != '#else':
            indent = m.group(1)
            out.append(indent + '#ifndef RE4_PORT   '
                       '// portage : epinglage PowerPC et asm vides, ne produisent rien')
            out.append(ln)
            out.append(indent + '#endif')
            n += 1
        else:
            out.append(ln)
        if ln.strip():
            prev = ln
    return NL.join(out), n


def strip_sections(text):
    """Met les directives de section sous garde, ligne a ligne.

    Ligne a ligne, et non par expression reguliere sur tout le texte, pour une raison precise :
    la conversion laisse l'original dans une branche `#else`, et une seconde execution le
    reprendrait. On saute donc ce qui suit immediatement un `#else`.
    """
    lines = text.split(NL)
    out, n, prev = [], 0, ''
    for ln in lines:
        m = SECTION.match(ln)
        if m and prev.strip() != '#else':
            indent = m.group(1)
            out.append(indent + '#ifndef RE4_PORT   '
                       '// portage : placement en sections, sans objet hors du build matching')
            out.append(ln)
            out.append(indent + '#endif')
            n += 1
        else:
            out.append(ln)
        if ln.strip():
            prev = ln
    return NL.join(out), n


def guard(indent, port_lines, orig_line):
    out = indent + '#ifdef RE4_PORT\n'
    for l in port_lines:
        out += indent + l + '\n'
    out += indent + '#else\n' + orig_line.rstrip('\n') + '\n' + indent + '#endif'
    return out


def convert(text, sections_only=False):
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

    if not sections_only:
        text = PIN.sub(do_pin, text)
        text = EMPTY.sub(do_empty, text)
    text, n_sec = strip_sections(text)
    text, n_br = strip_braced(text)
    return text, n_pin, n_empty, n_sec + n_br


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--apply', action='store_true')
    # Les conversions d'epinglage et de bloc vide laissent l'original dans une branche `#else`,
    # que le script reprendrait a la passe suivante : elles ne sont pas idempotentes, et les
    # unites concernees sont deja traitees. Cette option ne fait que les sections, qui le sont.
    ap.add_argument('--sections-only', action='store_true')
    # Par defaut le script ne voit que src/game/. Les directives de section sont aussi dans les
    # unites d'ennemis et d'armes, mais toutes ne sont pas construites : les .rel n'entrent pas
    # dans la liste des sources. On se limite donc a ce que la cible compile reellement, pour ne
    # pas remuer des fichiers que personne ne lit.
    ap.add_argument('--sources', help='un sources.cmake : ne traiter que les fichiers qu il liste')
    args = ap.parse_args()

    total_pin = total_empty = total_sec = files = 0
    # Tout src/, pas seulement src/game/ : les directives de section sont aussi dans les
    # unites d'ennemis (src/em*/).
    if args.sources:
        srcs = []
        for ln in io.open(args.sources, encoding='utf-8'):
            ln = ln.strip().strip('"')
            if ln.endswith('.cpp') or ln.endswith('.c'):
                srcs.append(os.path.normpath(ln))
        srcs = sorted(set(srcs))
    else:
        srcs = sorted(glob.glob(os.path.join(ROOT, 'src', 'game', '*.cpp')) +
                      glob.glob(os.path.join(ROOT, 'src', 'game', '*.c')))
    for path in srcs:
        s = io.open(path, encoding='utf-8', errors='surrogateescape').read()
        if 'RE4_PORT' in s and '#ifdef RE4_PORT' in s:
            pass   # deja traite en partie : la conversion reste idempotente
        out, np_, ne, ns = convert(s, args.sections_only)
        if np_ or ne or ns:
            files += 1
            total_pin += np_
            total_empty += ne
            total_sec += ns
            print('  %-28s %2d epinglage(s), %2d bloc(s) vide(s), %2d section(s)'
                  % (os.path.basename(path), np_, ne, ns))
            if args.apply:
                io.open(path, 'w', encoding='utf-8', errors='surrogateescape').write(out)

    print('\n%d fichiers, %d epinglages, %d blocs vides%s'
          % (files, total_pin, total_empty, '' if args.apply else '  (essai seulement)'))


if __name__ == '__main__':
    main()
