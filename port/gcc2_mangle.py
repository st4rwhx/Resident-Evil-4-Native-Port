# -*- coding: utf-8 -*-
"""Nom GCC 2.95 (celui du compilateur d'origine) d'une fonction C++ a partir de sa signature.

Le decompile declare souvent une fonction avec un nom d'assembleur d'epoque :

    void setStatus(int bit) asm("setStatus__3cEmi");

GCC 2.95 nommait `cEm::setStatus(int)` ainsi ; clang la nomme `_ZN3cEm9setStatusEi`. Le module qui
l'appelle reclame donc un symbole que personne ne definit. Ce module calcule, pour chaque fonction
que clang a compilee, le nom qu'aurait donne GCC 2.95 : il suffit ensuite de relier les deux.

Couvert : types de base, pointeurs, references, `const`, classes, repetitions T/N. Pas couvert
(rend None) : pointeurs de fonction, gabarits, operateurs.
"""
import re

BUILTIN = {
    'int': 'i', 'unsigned int': 'Ul', 'short': 's', 'unsigned short': 'Us', 'char': 'c',
    'unsigned char': 'Uc', 'signed char': 'Sc', 'long': 'l', 'unsigned long': 'Ul',
    'float': 'f', 'double': 'd', 'bool': 'b', 'void': 'v', 'long long': 'x',
    'unsigned long long': 'Ux', 'wchar_t': 'w',
}


def split_args(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '(<':
            depth += 1
        elif ch in ')>':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def mangle_type(t):
    t = t.strip()
    if not t or '(' in t or '<' in t or '::' in t:
        return None
    # suffixes de droite a gauche : *, &, const
    if t.endswith('*'):
        inner = mangle_type(t[:-1])
        return 'P' + inner if inner else None
    if t.endswith('&'):
        inner = mangle_type(t[:-1])
        return 'R' + inner if inner else None
    if t.endswith(' const'):
        inner = mangle_type(t[:-6])
        return 'C' + inner if inner else None
    if t.startswith('const '):
        inner = mangle_type(t[6:])
        return 'C' + inner if inner else None
    if t in BUILTIN:
        return BUILTIN[t]
    if re.fullmatch(r'[A-Za-z_]\w*', t):
        return '%d%s' % (len(t), t)
    return None


def is_builtin_code(code):
    return code in BUILTIN.values()


def mangle_args(args, base=0):
    codes = []
    for a in args:
        m = mangle_type(a)
        if m is None:
            return None
        codes.append(m)
    if codes == ['v']:
        return 'v'
    out = ''
    i = 0
    while i < len(codes):
        c = codes[i]
        if not is_builtin_code(c):
            first = codes.index(c)
            if first < i:
                run = 1
                while i + run < len(codes) and codes[i + run] == c:
                    run += 1
                out += ('T%d' % (first + base)) if run == 1 else ('N%d%d' % (run, first + base))
                i += run
                continue
        out += c
        i += 1
    return out


def mangle(demangled):
    """'cEm::setStatus(int)' -> 'setStatus__3cEmi' ; 'Foo(int, int)' -> 'Foo__Fii'."""
    m = re.fullmatch(r'(?:(?:static )?[\w\s\*&:]*?\s)?([\w:~]+)\((.*)\)(?: const)?', demangled.strip())
    if not m:
        return None
    name, argstr = m.group(1), m.group(2)
    args = mangle_args(split_args(argstr), 1 if '::' in name else 0) if argstr.strip() else 'v'
    if args is None:
        return None
    if '::' in name:
        cls, meth = name.rsplit('::', 1)
        if '::' in cls:
            return None
        cm = '%d%s' % (len(cls), cls)
        if meth == cls:                       # constructeur
            return '__%s%s' % (cm, '' if args == 'v' else args)
        if meth == '~' + cls:                 # destructeur
            return '_$_%s' % cm
        return '%s__%s%s' % (meth, cm, '' if args == 'v' else args)
    return '%s__F%s' % (name, args)
