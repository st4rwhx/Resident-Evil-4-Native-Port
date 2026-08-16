# -*- coding: utf-8 -*-
"""Audit des declarations locales contre les definitions (16/11).

    python -I port/outils/audit_prototypes.py [racine_du_depot]

Le portage relie les fonctions du jeu par leur nom seul (symboles non decores) : une declaration locale dont les
parametres different de la definition se relie sans erreur. Sur PowerPC un flottant deplace ne change rien (registres
flottants a part) ; sur x86 (pile) et ARM softfp (registres entiers) tous les arguments suivants arrivent decales.
Pour chaque fonction libre, on compare la suite des classes de parametres (I entier/pointeur, F f32, D f64, L 64 bits)
de chaque declaration avec celle de la definition. Seules les differences d'ordre ou de classe sur la partie commune
sont signalees (un parametre en trop ou en moins a la fin est sans effet sur x86 et ARM).
"""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
DIRS = ('src', 'include')
RX = re.compile(r'(?m)^[ \t]*(?:extern\s+"C"\s+)?(?:static\s+|inline\s+|extern\s+)*'
                r'([A-Za-z_][\w:<>\* \t&]*?[ \t\*&])([A-Za-z_]\w*)[ \t]*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*(?:asm\([^)]*\)\s*)?([;{])')
KW = {'if', 'else', 'do', 'goto', 'case', 'while', 'for', 'switch', 'return', 'sizeof', 'else', 'case', 'do', 'new', 'delete'}


def classe(p):
    p = re.sub(r'/\*.*?\*/', '', p).strip()
    if not p or p == 'void' or p == '...':
        return None
    if '*' in p or '&' in p or '[' in p or '(' in p:
        return 'I'
    t = re.sub(r'\b(const|volatile|register|unsigned|signed|struct|class|enum)\b', ' ', p).split()
    if not t:
        return 'I'
    base = ' '.join(t[:-1]) if len(t) > 1 else t[0]
    if re.search(r'\b(f32|float)\b', base):
        return 'F'
    if re.search(r'\b(f64|double)\b', base):
        return 'D'
    if re.search(r'\b(s64|u64|long\s+long)\b', base):
        return 'L'
    if re.match(r'^(f32|float)$', t[0]):
        return 'F'
    if re.match(r'^(f64|double)$', t[0]):
        return 'D'
    if re.match(r'^(s64|u64)$', t[0]):
        return 'L'
    return 'I'


def sig(params):
    parts, depth, cur = [], 0, ''
    for c in params:
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        if c == ',' and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += c
    parts.append(cur)
    out = [classe(re.sub(r'=.*$', '', x)) for x in parts]
    return ''.join(x for x in out if x)


defs, decls = {}, {}
for d in DIRS:
    for root, _, files in os.walk(os.path.join(ROOT, d)):
        for fn in files:
            if not fn.endswith(('.cpp', '.c', '.h', '.inc')):
                continue
            p = os.path.join(root, fn)
            src = open(p, encoding='utf-8', errors='replace').read()
            src = re.sub(r'//[^\n]*', '', src)
            src = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), src, flags=re.S)
            for m in RX.finditer(src):
                ret, name, params, end = m.groups()
                if name in KW or '::' in ret or 'return' in ret or ret.strip() in ('else', 'return', 'new'):
                    continue
                line = src.count('\n', 0, m.start()) + 1
                rel = os.path.relpath(p, ROOT).replace('\\', '/')
                s = sig(params)
                (defs if end == '{' else decls).setdefault(name, []).append((s, rel, line))

n = 0
for name, lst in sorted(decls.items()):
    if name not in defs or len({s for s, _, _ in defs[name]}) != 1:
        continue
    ds, dfile, dline = defs[name][0]
    for s, f, l in lst:
        k = min(len(s), len(ds))
        if s[:k] != ds[:k] and ('F' in s + ds or 'D' in s + ds or 'L' in s + ds):
            print('%-32s def %-10s %s:%d | decl %-10s %s:%d' % (name, ds, dfile, dline, s, f, l))
            n += 1
print('%d declaration(s) en desaccord' % n)
