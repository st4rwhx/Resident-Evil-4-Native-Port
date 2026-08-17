"""N7 (18/11) : un champ P32 passé à une fonction variadique (printf, log, OSReport…) y arrive en 4 octets bruts (adresse
sans la base). Lit le journal d'une compilation de diagnostic (PORT_CFLAGS="-DPORT_P32_VARARGS -Werror=non-pod-varargs",
P32 rendu non trivial) et entoure chaque argument signalé de `port_p(...)` (en 32 bits port_p rend le pointeur tel quel).
  python p32_varargs.py <journal> [--appliquer]
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import p64_casts as pc


def fin_argument(s, a):
    """Indice de fin de l'argument qui commence en a (jusqu'à la virgule ou parenthèse fermante de premier niveau)."""
    n = 0
    i = a
    while i < len(s):
        c = s[i]
        if c in '([{':
            n += 1
        elif c in ')]}':
            if n == 0:
                return i
            n -= 1
        elif c == ',' and n == 0:
            return i
        elif c == '"':
            j = s.find('"', i + 1)
            i = j if j > 0 else i
        i += 1
    return -1


def main():
    appliquer = '--appliquer' in sys.argv
    sites = collections.defaultdict(set)
    stats = collections.Counter()
    for b in pc.lire_blocs(sys.argv[1]):
        d = b['err']
        if 'variadic' not in d['msg']:
            continue
        rel, rl = pc.vrai(d['f'], d['line'])
        if not rel or rel in pc.INTERDITS:
            stats['ignore'] += 1
            continue
        sites[rel].add((rl, d['col'], d['src'].strip()))
    for rel, S in sorted(sites.items()):
        p = os.path.join(pc.ROOT, rel)
        data = open(p, 'rb').read().decode('utf-8', errors='surrogateescape')
        L = data.split('\n')
        par_ligne = collections.defaultdict(list)
        for rl, col, src in S:
            if rl - 1 >= len(L) or L[rl - 1].strip() != src:
                stats['ligne_differente'] += 1
                continue
            par_ligne[rl].append(col)
        change = False
        for rl, cols in par_ligne.items():
            s = L[rl - 1]
            for col in sorted(set(cols), reverse=True):
                a = col - 1
                if s[a:a + 7] == 'port_p(':
                    continue
                e = fin_argument(s, a)
                if e < 0:
                    stats['forme'] += 1
                    continue
                arg = s[a:e].rstrip()
                s = s[:a] + 'port_p(' + arg + ')' + s[a + len(arg):]
                stats['ok'] += 1
                change = True
            L[rl - 1] = s
        if change and appliquer:
            open(p, 'wb').write('\n'.join(L).encode('utf-8', errors='surrogateescape'))
            stats['fichiers'] += 1
    print(dict(stats))


if __name__ == '__main__':
    main()
