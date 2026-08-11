# -*- coding: utf-8 -*-
"""Ecrit la liste des sources du portage PC pour CMake.

La liste vient du meme endroit que pour le telephone (`port/build_game.py`) : les unites du
DOL, moins `src/lib/` - le SDK Nintendo decompile, qui parle au materiel du GameCube - et
moins la libc de ProDG, dont les unites remplaceraient celles de la plateforme pour tout le
processus.

Ce qui change par rapport au telephone, c'est la couche de portage : sur PC, aurora fournit
GX, VI, PAD, MTX, les textures et l'OS. On ne garde que ce qu'elle ne couvre pas, et ce qui
est propre a RE4.

usage : python port/gen_pc_sources.py <fichier.cmake>
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'port'))

import build_game  # noqa: E402  (meme EXCLUDED, meme lecture de objects.py)

# Ce que le portage garde sur PC.
#
#   dvd        notre lecteur d'image disque. aurora en a un (via nod), mais le notre connait
#              les particularites de RE4 : le lecteur GameCube rend toujours la longueur
#              demandee, meme au-dela de la fin logique du fichier, et le jeu traite toute
#              lecture courte comme une erreur.
#   thread     les OSThread en coroutines, avec un jeton unique. RE4 s'en sert comme d'un
#              ordonnanceur cooperatif ; du vrai parallelisme introduirait des courses que le
#              jeu n'a jamais eu a survivre.
#   aram       les 16 Mo d'ARAM et les transferts ARQ.
#   memfill,
#   yz2        des routines PowerPC transcrites en C.
#   misc,
#   prodg_rt   ce que la runtime ProDG fournissait.
#   port_log   le journal du reste a faire.
#   endian     les convertisseurs ecrits a la main. Ils restent le temps d'obtenir une premiere
#              image sur PC ; DISC_STRUCT les remplacera ensuite (voir ENDIANNESS.md).
#   stubs_*    les fonctions de plateforme encore absentes.
#   vi_pc      l'horloge d'image, qui pompe la fenetre d'aurora.
#   psmtx_pc   les variantes paired-single de MTX, redirigees vers celles en C.
#   pc_glue    OSReport, DBIsDebuggerPresent, TEXGet.
#   main_pc    le point d'entree, qui initialise aurora.
#
# Retires par rapport au telephone : os, vi, gx, pad, texture, texobj, mtx, crash. aurora les
# fournit, et son interpreteur de listes d'affichage est precisement ce qui manquait pour que
# les modeles s'affichent.
PORT_UNITS = ['dvd', 'memfill', 'yz2', 'misc', 'aram', 'thread', 'endian',
              'prodg_rt', 'port_log', 'vi_pc', 'psmtx_pc', 'pc_glue', 'stubs_generated', 'stubs_link', 'main_pc']


# Les talons ne peuvent pas etre lies tels quels : 66 des fonctions GX qu'ils definissent sont
# aussi definies par aurora, et l'editeur de liens verrait double. Plutot que d'entretenir une
# liste a la main, on filtre sur les symboles reellement exportes par les archives d'aurora
# (`nm -g --defined-only`, releve dans port/aurora_symbols.txt). Chaque talon tient sur une
# ligne - c'est une propriete du generateur, pas un hasard - donc un filtre ligne a ligne
# suffit, et il tombe juste par construction : ce qu'aurora definit, on le laisse a aurora.
FILTERED = ('stubs_generated', 'stubs_link')


def aurora_symbols():
    p = os.path.join(ROOT, 'port', 'aurora_symbols.txt')
    with open(p, encoding='utf-8') as f:
        return set(l.strip() for l in f if l.strip())


def filter_stubs(src, dst, syms):
    """Recopie un fichier de talons sans les definitions qu'aurora fournit deja."""
    kept, dropped = [], 0
    for ln in io.open(src, encoding='utf-8', errors='surrogateescape'):
        m = re.match(r'^[A-Za-z_][^();]*?([A-Za-z_][A-Za-z0-9_]*)\s*\(', ln)
        if m and m.group(1) in syms:
            dropped += 1
            continue
        kept.append(ln)
    io.open(dst, 'w', encoding='utf-8', errors='surrogateescape').write(''.join(kept))
    return dropped


def main():
    out = sys.argv[1]
    files = []
    for u in build_game.units():
        # Les .s sont du PowerPC brut (memset_2.s, yz2asm.s). port/memfill.cpp et port/yz2.cpp
        # les remplacent, transcrits en C ; l'assembleur de la machine hote ne peut rien en
        # faire, et le projet CMake declare le langage ASM pour d'autres raisons.
        if u.endswith('.s'):
            continue
        p = os.path.join(ROOT, 'src', u)
        if os.path.exists(p):
            files.append(p)
    gen = os.path.join(os.path.dirname(os.path.abspath(out)), 'generated')
    if not os.path.isdir(gen):
        os.makedirs(gen)
    syms = aurora_symbols()
    for u in PORT_UNITS:
        p = os.path.join(ROOT, 'port', u + '.cpp')
        if not os.path.exists(p):
            continue
        if u in FILTERED:
            d = os.path.join(gen, u + '.cpp')
            n = filter_stubs(p, d, syms)
            print('%s : %d talons laisses a aurora' % (u, n))
            p = d
        files.append(p)

    with open(out, 'w', encoding='utf-8') as f:
        f.write('# Engendre par port/gen_pc_sources.py - ne pas editer a la main.\n')
        f.write('set(RE4_ROOT "%s")\n' % ROOT.replace('\\', '/'))
        f.write('set(RE4_SOURCES\n')
        for p in files:
            f.write('  "%s"\n' % p.replace('\\', '/'))
        f.write(')\n')
    print('%d sources -> %s' % (len(files), out))


if __name__ == '__main__':
    main()
