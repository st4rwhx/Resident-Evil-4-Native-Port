"""N7 (18/11) : `(T*) (i * sizeof(X) + (u32) p)` — sizeof rend l'expression 64 bits (size_t), donc le compilateur ne
signale pas la conversion en pointeur (-Werror=int-to-pointer-cast ne voit qu'un entier plus petit), et le résultat n'est
que le décalage, sans la base de la fenêtre (yz2code.cpp : plantage au chargement de r120).
Réécrit en `(T*) port_ptr64((u32) (expr))` : même arithmétique 32 bits que sur la console ; en 32 bits port_ptr64 est
l'identité.   python p64_sizeof.py [--appliquer]
"""
import glob, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
INTERDITS = {'src/Tools/t_prim.cpp', 'src/Tools/t_util.cpp', 'src/Tools/tools.cpp', 'src/game/filter.cpp',
             'src/tools/db_light.cpp', 'port/ptr32.h'}
STRICT = '--large' not in sys.argv   # --large : aussi sans (u32) explicite (champ u32) ; le compilateur rejette ensuite
                                     # `(u32) pointeur`, et ces lignes-la sont remises (p64_sizeof_annuler)
CAST = re.compile(r'\(\s*(?:const\s+)?[A-Za-z_]\w*(?:\[\d+\])?\s*\*\s*\)\s*\(')


def fin_paren(s, i):
    """s[i] == '(' : indice de la parenthèse fermante correspondante."""
    n = 0
    for k in range(i, len(s)):
        if s[k] == '(':
            n += 1
        elif s[k] == ')':
            n -= 1
            if n == 0:
                return k
        elif s[k] in ';{}':
            return -1
    return -1


def traiter(s):
    out = []
    pos = 0
    n = 0
    for m in CAST.finditer(s):
        a = m.end() - 1
        if a < pos:
            continue
        e = fin_paren(s, a)
        if e < 0:
            continue
        expr = s[a + 1:e]
        if 'sizeof' not in expr or 'port_ptr64' in expr or (STRICT and not re.search(r'\(u32\)', expr)):
            continue
        out.append(s[pos:a])
        out.append('port_ptr64((u32) (' + expr + '))')
        pos = e + 1
        n += 1
    out.append(s[pos:])
    return ''.join(out), n


def main():
    tot = 0
    for p in sorted(glob.glob(os.path.join(ROOT, 'src', '**', '*.cpp'), recursive=True) +
                    glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True)):
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        if rel in INTERDITS or rel.lower() in {x.lower() for x in INTERDITS}:
            continue
        s = open(p, encoding='utf-8', newline='', errors='surrogateescape').read()
        if 'sizeof' not in s:
            continue
        s2, n = traiter(s)
        if n:
            tot += n
            print('%-34s %d' % (rel, n))
            if '--appliquer' in sys.argv:
                open(p, 'w', encoding='utf-8', newline='', errors='surrogateescape').write(s2)
    print('total', tot)


if __name__ == '__main__':
    main()
