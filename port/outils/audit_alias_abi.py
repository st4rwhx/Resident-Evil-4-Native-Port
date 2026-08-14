# -*- coding: utf-8 -*-
"""Audit des alias asm("...") du decompile (15/11) : classe d'hypotheses cachees « convention d'appel ».

Le decompile declare parfois une fonction sous un autre nom et avec d'autres parametres, liee au vrai symbole par
asm("Symbole"), pour reproduire l'allocation de registres de l'original. Sur PowerPC les entiers/pointeurs (r3..r10)
et les flottants (f1..f8) sont alloues SEPAREMENT : changer l'ordre relatif d'un flottant et d'un pointeur ne change
rien. Sur x86 (pile) et ARM softfp (r0-r3 puis pile), c'est l'ordre absolu qui compte : le pointeur recoit le
flottant (plantage em10 AtSphereCapsuleCk, 0x437a0000 = 250.0f).

Ce script compare, pour chaque alias, la suite des classes de parametres (F = flottant, I = le reste) a celle du vrai
symbole : nom decore CodeWarrior (decode ici) ou declaration C trouvee dans include/ et src/.
Sortie : les alias dangereux (ordre F/I different, ou type F <-> I change). Un parametre en trop ou en moins en fin
de liste est signale a part (benin en general en cdecl).

    python -I port/outils/audit_alias_abi.py [racine]
"""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..')
ALIAS = re.compile(r'(?:extern\s+"C"\s+)?([\w\s\*:&<>,]+?)\b(\w+)\s*\(([^()]*)\)\s*asm\("([\w@$.]+)"\)\s*;')
FLOATS = {'f32', 'f64', 'float', 'double', 'Mtx', }


def classify(params):
    params = params.strip()
    if params in ('', 'void'):
        return []
    out = []
    for p in params.split(','):
        p = p.strip()
        if '*' in p or '&' in p or '[' in p:
            out.append('I')
            continue
        toks = re.findall(r'\w+', p)
        toks = [t for t in toks if t not in ('const', 'volatile', 'unsigned', 'signed', 'struct', 'class')]
        out.append('F' if toks and toks[0] in ('f32', 'f64', 'float', 'double') else 'I')
    return out


def cw_params(sym):
    """Classes des parametres d'un nom decore CodeWarrior (foo__FPii, method__6cClassPv...). None si non decore."""
    m = re.match(r'^(.+?)__(.*)$', sym)
    if not m:
        return None
    rest = m.group(2)
    is_method = False
    i = 0
    # classe proprietaire (methode) : <n><nom> ou Q<k>...
    if rest and rest[0].isdigit():
        n = re.match(r'(\d+)', rest).group(1)
        i = len(n) + int(n)
        is_method = True
        if i < len(rest) and rest[i] == 'C':   # methode const
            i += 1
    if i < len(rest) and rest[i] == 'F':
        i += 1
    types = []

    def one(j):
        """lit un type a partir de j ; renvoie (classe, j suivant)"""
        while j < len(rest) and rest[j] in 'CVU S'.replace(' ', ''):
            if rest[j] in 'US':
                j += 1
                continue
            j += 1
        c = rest[j]
        if c in 'PR':
            _, j2 = one(j + 1)
            return 'I', j2
        if c == 'A':   # tableau A<n>_type
            k = rest.index('_', j)
            _, j2 = one(k + 1)
            return 'I', j2
        if c.isdigit():
            n = re.match(r'(\d+)', rest[j:]).group(1)
            return 'I', j + len(n) + int(n)
        if c == 'Q':
            k = int(rest[j + 1])
            j += 2
            for _ in range(k):
                n = re.match(r'(\d+)', rest[j:]).group(1)
                j += len(n) + int(n)
            return 'I', j
        if c == 'F':   # pointeur de fonction F...._ret
            depth = 0
            j += 1
            while j < len(rest) and rest[j] != '_':
                _, j = one(j)
            _, j = one(j + 1)
            return 'I', j
        if c in 'fd':
            return 'F', j + 1
        if c == 'e':   # ...
            return 'E', j + 1
        return 'I', j + 1

    while i < len(rest):
        c = rest[i]
        if c == 'T':   # Tn : comme le parametre n
            n = int(rest[i + 1])
            types.append(types[n - 1] if n - 1 < len(types) else 'I')
            i += 2
            continue
        if c == 'N':   # Nkn : k fois le parametre n
            k, n = int(rest[i + 1]), int(rest[i + 2])
            for _ in range(k):
                types.append(types[n - 1] if n - 1 < len(types) else 'I')
            i += 3
            continue
        if c == 'v' and not types:
            i += 1
            continue
        try:
            t, i = one(i)
        except (IndexError, ValueError, AttributeError):
            return None
        if t != 'E':
            types.append(t)
    if is_method:
        types = ['I'] + types
    return types


def c_decls(root):
    decl = {}
    pat = re.compile(r'^[\w\s\*]+?\b(\w+)\s*\(([^()]*)\)\s*;', re.M)
    for base in ('include', 'src'):
        for dp, _, fs in os.walk(os.path.join(root, base)):
            for f in fs:
                if not f.endswith(('.h', '.cpp', '.c')):
                    continue
                try:
                    txt = open(os.path.join(dp, f), encoding='utf-8', errors='replace').read()
                except OSError:
                    continue
                for m in pat.finditer(txt):
                    line = m.group(0)
                    if 'asm(' in line or line.lstrip().startswith(('return', 'if', 'while')):
                        continue
                    decl.setdefault(m.group(1), (classify(m.group(2)), os.path.join(dp, f)))
    return decl


def main():
    root = os.path.abspath(ROOT)
    decls = c_decls(root)
    danger, tail, unknown, guarded, ok = [], [], [], [], 0
    for base in ('src', 'include'):
        for dp, _, fs in os.walk(os.path.join(root, base)):
            for f in fs:
                if not f.endswith(('.h', '.cpp', '.c')):
                    continue
                path = os.path.join(dp, f)
                txt = open(path, encoding='utf-8', errors='replace').read()
                for m in ALIAS.finditer(txt):
                    name, params, sym = m.group(2), m.group(3), m.group(4)
                    a = classify(params)
                    real = cw_params(sym)
                    if real is None:
                        d = decls.get(sym)
                        real = d[0] if d else None
                    line = txt.count('\n', 0, m.start()) + 1
                    where = '%s:%d %s -> %s' % (os.path.relpath(path, root).replace('\\', '/'), line, name, sym)
                    # garde : les lignes entre le dernier « #else » et l'alias ne sont que d'autres alias asm(...) ou des
                    # suites de declaration, et ce #else ferme un #ifdef RE4_PORT
                    ls = txt.rfind('\n', 0, m.start(2))
                    el = txt.rfind('#else', 0, ls + 1)
                    between = txt[el + 5:ls] if el >= 0 else 'x'
                    clean = all(('asm(' in l) or not l.strip().endswith(';') for l in between.split('\n'))
                    if el >= 0 and clean and '#endif' not in between and '#ifdef RE4_PORT' in txt[max(0, el - 1500):el]:
                        guarded.append(where)
                        continue
                    if real is None:
                        unknown.append(where)
                        continue
                    n = min(len(a), len(real))
                    if a[:n] != real[:n]:
                        danger.append('%s   alias %s / vrai %s' % (where, ''.join(a), ''.join(real)))
                    elif len(a) != len(real):
                        tail.append('%s   alias %s / vrai %s' % (where, ''.join(a), ''.join(real)))
                    else:
                        ok += 1
    print('alias conformes : %d ; remplaces sous RE4_PORT : %d ; DANGEREUX : %d ; longueur differente : %d ; '
          'vrai symbole introuvable : %d' % (ok, len(guarded), len(danger), len(tail), len(unknown)))
    for d in danger:
        print('DANGER  ' + d)
    for d in tail:
        print('LONGUEUR ' + d)
    for d in unknown:
        print('INCONNU ' + d)


if __name__ == '__main__':
    main()
