# -*- coding: utf-8 -*-
"""Genere la couche de plateforme a partir des en-tetes SDK du decompile.

Le portage ne reecrit pas le jeu : il remplace les 276 fonctions du SDK Nintendo que le code
appelle reellement (mesure : 4903 sites d'appel). Leurs **declarations existent deja**, dans
`include/dolphin/`, parce que le decompile embarque le SDK. Il ne manque donc que des
definitions, et les prototypes se recuperent au lieu de se retaper - ce qui evite la classe
d'erreurs la plus penible d'un portage, la signature approximative.

Sortie : `port/stubs_generated.cpp`, une definition par fonction qui journalise son premier
appel et rend une valeur neutre. Cela ne fait rien tourner, mais cela **lie**, et un binaire
qui lie permet de remplacer les fonctions une par une en voyant a chaque fois ce que le jeu
reclame ensuite. C'est l'ordre inverse de "tout implementer puis esperer".
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SDK = os.path.join(ROOT, 'include', 'dolphin')

# Les familles que le jeu appelle (comptees sur src/, hors src/lib).
FAMILIES = ('GX', 'OS', 'DVD', 'AX', 'PAD', 'VI', 'CARD', 'AR', 'ARQ', 'SI', 'DEMO')
PROTO = re.compile(
    r'^\s*(?!typedef)([A-Za-z_][\w \t\*&]*?[\w\*&])\s+'      # type de retour
    r'((?:%s)[A-Za-z0-9_]*)\s*'                              # nom
    r'\(([^;{]*)\)\s*;' % '|'.join(FAMILIES),
    re.M)


def used_symbols():
    """Les fonctions SDK reellement appelees par le code du jeu (src/ sans src/lib)."""
    pat = re.compile(r'\b((?:%s)[A-Z]\w+)\s*\(' % '|'.join(FAMILIES))
    out = set()
    for dp, dn, fns in os.walk(os.path.join(ROOT, 'src')):
        if os.path.basename(dp) == 'lib':
            dn[:] = []
            continue
        for fn in fns:
            if fn.endswith(('.c', '.cpp')):
                with open(os.path.join(dp, fn), encoding='utf-8', errors='replace') as f:
                    out.update(m.group(1) for m in pat.finditer(f.read()))
    return out


def declarations():
    """{nom: (type de retour, parametres, en-tete)} pour tout le SDK."""
    out = {}
    for dp, _, fns in os.walk(SDK):
        for fn in fns:
            if not fn.endswith('.h'):
                continue
            p = os.path.join(dp, fn)
            src = open(p, encoding='utf-8', errors='replace').read()
            src = re.sub(r'/\*.*?\*/', ' ', src, flags=re.S)
            src = re.sub(r'//[^\n]*', '', src)
            for m in PROTO.finditer(src):
                ret, name, args = (x.strip() for x in m.groups())
                out.setdefault(name, (ret, args, os.path.relpath(p, ROOT)))
    return out


def implemented():
    """Les fonctions deja ecrites a la main dans port/*.cpp : le generateur doit les sauter,
    sinon le lien echoue en doublon. On lit les definitions plutot que de tenir une liste, pour
    qu'ajouter une implementation suffise."""
    pat = re.compile(r'^[A-Za-z_][\w \t\*&:]*?\b((?:%s)[A-Za-z0-9_]+)\s*\(' % '|'.join(FAMILIES), re.M)
    out = set()
    for fn in os.listdir(HERE):
        if fn.endswith('.cpp') and fn != 'stubs_generated.cpp':
            out.update(pat.findall(open(os.path.join(HERE, fn), encoding='utf-8').read()))
    return out


NEUTRAL = {'void': None, 'BOOL': '0', 'bool': 'false', 'char': '0', 'u8': '0', 's8': '0',
           'u16': '0', 's16': '0', 'u32': '0', 's32': '0', 'int': '0', 'long': '0',
           'unsigned': '0', 'f32': '0.0f', 'f64': '0.0', 'float': '0.0f', 'double': '0.0',
           'u64': '0', 's64': '0'}


def neutral(ret):
    r = ret.replace('extern', '').strip()
    if r == 'void':
        return None
    if '*' in r:
        return 'nullptr'
    return NEUTRAL.get(r.split()[-1], '{}')


def emit(path):
    used, decl, done = used_symbols(), declarations(), implemented()
    todo = sorted(n for n in used if n in decl and n not in done)
    out = ['// -*- coding: utf-8 -*-',
           '// GENERE par port/gen_stubs.py - ne pas editer a la main.',
           '//',
           '// Une definition par fonction du SDK Nintendo que le jeu appelle reellement. Chacune',
           '// journalise son premier appel et rend une valeur neutre. Cela ne fait rien tourner,',
           '// mais cela **lie** : a partir de la, on remplace les fonctions une par une en voyant',
           '// a chaque execution ce que le jeu reclame ensuite, au lieu de tout implementer a',
           '// l aveugle puis d esperer.',
           '//',
           '// Les prototypes viennent de include/dolphin/, donc ils sont exacts par construction.',
           '#include "port_log.h"',
           '#include <dolphin.h>',
           '// dolphin.h n inclut ni AX ni AXFX ni AXART.',
           '#include <dolphin/ax.h>',
           '#include <dolphin/axfx.h>',
           '#include <dolphin/axart.h>',
           '#include <dolphin/demo.h>',
           '']
    for n in todo:
        ret, args, hdr = decl[n]
        a = args.strip()
        sig_args = '' if a in ('', 'void') else a
        v = neutral(ret)
        body = 'PORT_TODO("%s");' % n + ('' if v is None else ' return %s;' % v)
        out.append('%s %s(%s) { %s }   // %s' % (ret, n, sig_args, body, hdr))
    out.append('')
    open(path, 'w', encoding='utf-8').write('\n'.join(out))
    return len(todo), len(done)


if __name__ == '__main__':
    p = os.path.join(HERE, 'stubs_generated.cpp')
    n, d = emit(p)
    print('%d souches generees, %d fonctions deja implementees a la main -> %s' % (n, d, p))
