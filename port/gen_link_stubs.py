# -*- coding: utf-8 -*-
"""Genere les souches des symboles que le lien reclame encore.

`gen_stubs.py` couvre les familles du SDK graphique et systeme, reperees par leur prefixe. Il
reste ce qui n'a pas de prefixe reconnaissable : le middleware audio et video de CRI (Sofdec,
ADX), le synthetiseur, le mixeur, quelques compteurs de performance du PowerPC. Plutot que de
tenir une liste a la main, on part de **ce que l'editeur de liens demande**, et on va chercher
le prototype dans les en-tetes du depot.

Quand le prototype est introuvable - c'est le cas du middleware CRI, dont les en-tetes ne sont
pas dans le depot - la souche est declaree `int nom()`. Sur l'ABI ARM cela suffit : une fonction
qui ignore ses arguments et rend zero est appelable depuis n'importe quel prototype, et le
symbole cesse de manquer. C'est volontairement grossier, et chaque souche le dit a l'execution.

usage : python port/gen_link_stubs.py <liste de symboles> [sortie]
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROTO = re.compile(
    r'^\s*(?!typedef|return)([A-Za-z_][\w \t\*&]*?[\w\*&])\s+'
    r'(%s)\s*\(([^;{]*)\)\s*;', re.M)


def headers():
    out = []
    for dp, _, fns in os.walk(os.path.join(ROOT, 'include')):
        for fn in fns:
            if not fn.endswith('.h'):
                continue
            path = os.path.join(dp, fn)
            # Seuls les en-tetes que ce fichier peut **inclure** sont des sources de
            # prototypes. mwply.h, par exemple, decrit bien le middleware video mais depend de
            # cri_xpt.h, absent du depot : en tirer une signature donnerait un fichier qui ne
            # compile pas. Pour ceux-la, la forme minimale suffit.
            rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
            if not (rel.startswith('include/dolphin/') or rel.startswith('include/charPipeline/')):
                continue
            out.append(path)
    # Les en-tetes du SDK d'abord : quand un nom existe des deux cotes, c'est la declaration
    # du SDK qui fait foi, celle du jeu employant parfois ses propres typedefs.
    out.sort(key=lambda p: (0 if (os.sep + 'dolphin' + os.sep) in p else 1, p))
    return out


def main():
    names = [l.strip() for l in open(sys.argv[1]) if l.strip()]
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'port', 'stubs_link.cpp')

    # Un symbole mange par un compilateur (C++ ou ProDG) n'est pas une fonction C : le
    # generateur ne doit pas y toucher, ils demandent une vraie correction a la source.
    plain = [n for n in names if re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', n) and '__' not in n]

    found = {}
    hdrs = headers()
    blob = [(p, open(p, encoding='utf-8', errors='replace').read()) for p in hdrs]
    for n in plain:
        pat = re.compile(PROTO.pattern % re.escape(n), re.M)
        for p, src in blob:
            m = pat.search(src)
            if m:
                ret, _, args = (x.strip() for x in m.groups())
                found[n] = (ret, args, os.path.relpath(p, ROOT).replace(os.sep, '/'))
                break

    lines = [
        '// -*- coding: utf-8 -*-',
        '// GENERE par port/gen_link_stubs.py -- ne pas editer a la main.',
        '//',
        "// Les symboles que le lien reclamait et qu'aucune couche du portage ne fournit encore :",
        '// le middleware audio et video de CRI, le synthetiseur, le mixeur, les compteurs de',
        '// performance du PowerPC. Chacun journalise son premier appel, pour que ce soit',
        "// **l'execution** qui dise lesquels comptent vraiment, et dans quel ordre les ecrire.",
        '#include <dolphin.h>',
        '#include <dolphin/axfx.h>',
        '#include <dolphin/mix.h>',
        '#include <dolphin/syn.h>',
        '#include <dolphin/seq.h>',
        '',
        '// include/card.h et include/dolphin/card.h declarent les memes fonctions avec des',
        '// typedefs differents et se contredisent : on ne peut inclure que le second, et le',
        '// type manquant est redeclare ici a l identique.',
        'typedef void (*CardCallback)(s32 chan, s32 result);',
        '#include <charPipeline/texPalette.h>',
        '',
        '#include "port_log.h"',
        '',
        'extern "C" {',
        '',
    ]
    for n in sorted(plain):
        if n in found:
            ret, args, hdr = found[n]
            body = '{ PORT_TODO("%s"); }' % n if ret == 'void' \
                else '{ PORT_TODO("%s"); return (%s) 0; }' % (n, ret)
            lines.append('%s %s(%s) %s   // %s' % (ret, n, args, body, hdr))
        else:
            # Prototype inconnu : forme minimale, appelable depuis n'importe quelle declaration.
            lines.append('int %s() { PORT_TODO("%s"); return 0; }   // prototype inconnu' % (n, n))
    lines += ['', '}', '']

    with open(out_path, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(lines))
    print('%d souches (%d avec prototype, %d sans) -> %s'
          % (len(plain), len(found), len(plain) - len(found), out_path))
    skipped = [n for n in names if n not in plain]
    if skipped:
        print('%d symboles mangles laisses de cote (correction a la source) :' % len(skipped))
        for n in skipped:
            print('   ' + n)


if __name__ == '__main__':
    main()
