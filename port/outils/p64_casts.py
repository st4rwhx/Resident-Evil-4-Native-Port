"""Phase N7 (17/11) : corrige automatiquement les conversions pointeur <-> entier 32 bits signalées par la compilation 64 bits.

Entrée : le journal d'erreurs de build_game.py (PORT_ABI=x86_64, --errlog). Chaque erreur donne fichier:ligne:colonne et,
sous la ligne de code, l'étendue exacte de l'expression (^~~~). Avec une base de fenêtre alignée sur 4 Go (port/ptr32.h) :
  - pointeur -> entier 32 bits : `(u32) X`  -> `(u32)(uintptr_t)(X)`   (les 32 bits bas = décalage = adresse GameCube)
  - entier 32 bits -> pointeur : `(T*) X`   -> `(T*)port_ptr64(X)`     (base | valeur ; 0 reste NULL)
Les erreurs dans une macro sont corrigées à la définition (dernière note « expanded from macro »). Les fichiers sous
#line "D:/Bio4/Prog/x.cpp" sont ramenés aux vraies lignes. En 32 bits, uintptr_t et port_ptr64 ne changent rien.
  python p64_casts.py <erreurs_64.txt> [--appliquer]
"""
import collections, glob, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')).replace('\\', '/')
COPIE = 'c:/re4n/re4port_pc'
INTERDITS = {'src/Tools/t_prim.cpp', 'src/Tools/t_util.cpp', 'src/Tools/tools.cpp', 'src/game/filter.cpp',
             'src/tools/db_light.cpp',
             'port/ptr32.h'}   # le type lui-même : une erreur qui y apparaît vient de l'appelant (instanciation)
RE_DIAG = re.compile(r'^(.*?):(\d+):(\d+): (error|note): (.*)$')

_line_maps = {}


def map_line_directives():
    """{ (chemin #line, numéro) : (fichier réel relatif, ligne réelle) } pour tout src/."""
    if _line_maps:
        return _line_maps
    for p in glob.glob(ROOT + '/src/**/*.cpp', recursive=True) + glob.glob(ROOT + '/src/**/*.c', recursive=True):
        rel = p.replace('\\', '/')[len(ROOT) + 1:]
        try:
            L = open(p, encoding='utf-8', errors='replace').read().split('\n')
        except OSError:
            continue
        cur = None
        for i, l in enumerate(L):
            m = re.match(r'\s*#line\s+(\d+)(?:\s+"([^"]+)")?', l)
            if m:
                if m.group(2):
                    cur = [m.group(2).replace('\\', '/').lower(), int(m.group(1))]
                elif cur:   # `#line N` sans nom : même fichier présumé qu'avant
                    cur = [cur[0], int(m.group(1))]
                else:       # `#line N` sans nom dès le début : le compilateur garde le vrai nom du fichier
                    cur = ['@' + rel, int(m.group(1))]
                continue
            if cur:
                _line_maps.setdefault((cur[0], cur[1]), (rel, i + 1))
                cur[1] += 1
    return _line_maps


def vrai(f, line):
    f = f.replace('\\', '/')
    rel = None
    if f.lower().startswith(COPIE + '/'):
        rel = f[len(COPIE) + 1:].replace('src/tools_lc/', 'src/tools/')
    elif f.startswith(ROOT + '/'):
        rel = f[len(ROOT) + 1:]
    if rel:
        m = map_line_directives().get(('@' + rel, line))   # fichier à `#line N` sans nom (motion.cpp)
        return m if m else (rel, line)
    m = map_line_directives().get((f.lower(), line))
    return m if m else (None, None)


def lire_blocs(path):
    L = open(path, encoding='utf-8', errors='replace').read().split('\n')
    blocs, cur = [], None
    i = 0
    while i < len(L):
        m = RE_DIAG.match(L[i])
        if m:
            src = L[i + 1] if i + 1 < len(L) else ''
            car = L[i + 2] if i + 2 < len(L) else ''
            d = dict(f=m.group(1), line=int(m.group(2)), col=int(m.group(3)), kind=m.group(4), msg=m.group(5),
                     src=src, car=car)
            if d['kind'] == 'error':
                cur = dict(err=d, notes=[])
                blocs.append(cur)
            elif cur is not None:
                cur['notes'].append(d)
            i += 1
            continue
        i += 1
    return blocs


def genre(msg):
    if 'cast from pointer to smaller type' in msg:
        return 'enc'
    if 'from smaller integer type' in msg:
        return 'dec'
    return None


def cible(b):
    """(fichier, ligne, colonne, longueur) où se trouve vraiment le cast."""
    d = b['err']
    macros = [n for n in b['notes'] if 'expanded from macro' in n['msg']]
    if macros:
        d = macros[-1]
    car = d['car']
    k = car.find('^')
    if k < 0:
        return None
    m = re.match(r'\^~*', car[k:])
    return d['f'], d['line'], d['col'], len(m.group(0)), d['src']


def fin_cast(seg):
    """Indice juste après la parenthèse fermante du type `( ... )` au début de seg, ou -1."""
    if not seg.startswith('('):
        return -1
    n = 0
    for i, c in enumerate(seg):
        if c == '(':
            n += 1
        elif c == ')':
            n -= 1
            if n == 0:
                return i + 1
    return -1


def main():
    errlog = sys.argv[1]
    appliquer = '--appliquer' in sys.argv
    edits = collections.defaultdict(dict)   # rel -> {(ligne, col): (longueur, genre)}
    stats = collections.Counter()
    for b in lire_blocs(errlog):
        g = genre(b['err']['msg'])
        if not g:
            continue
        c = cible(b)
        if not c:
            stats['sans_etendue'] += 1
            continue
        f, line, col, n, src = c
        rel, rl = vrai(f, line)
        if not rel:
            stats['fichier_inconnu'] += 1
            continue
        if rel in INTERDITS:
            stats['interdit'] += 1
            continue
        edits[rel][(rl, col)] = (n, g, src)
    for rel, E in sorted(edits.items()):
        p = os.path.join(ROOT, rel)
        data = open(p, 'rb').read().decode('utf-8', errors='surrogateescape')
        L = data.split('\n')
        par_ligne = collections.defaultdict(list)
        for (rl, col), (n, g, src) in E.items():
            if rl - 1 >= len(L) or L[rl - 1].strip() != src.strip():
                stats['ligne_differente'] += 1   # le texte de la ligne doit être celui du journal, sinon on n'y touche pas
                continue
            par_ligne[rl].append((col, n, g))
        change = False
        for rl, lst in par_ligne.items():
            s = L[rl - 1]
            faits = []   # (début, delta) des modifs déjà faites sur la ligne (à droite)
            for col, n, g in sorted(lst, key=lambda t: -t[0]):
                a = col - 1
                e = a + n
                for (pa, dl) in faits:
                    if a <= pa < e:
                        e += dl
                seg = s[a:e]
                k = fin_cast(seg)
                if k < 0 or e > len(s):
                    stats['forme'] += 1
                    continue
                typ, op = seg[:k], seg[k:].strip()
                if not op:
                    stats['forme'] += 1
                    continue
                if g == 'enc':
                    nouv = '%s(uintptr_t)(%s)' % (typ, op)
                else:
                    nouv = '%sport_ptr64(%s)' % (typ, op)
                s = s[:a] + nouv + s[e:]
                faits.append((a, len(nouv) - len(seg)))
                stats['ok_' + g] += 1
                change = True
            L[rl - 1] = s
        if change and appliquer:
            open(p, 'wb').write('\n'.join(L).encode('utf-8', errors='surrogateescape'))
            stats['fichiers'] += 1
    print(dict(stats))


if __name__ == '__main__':
    main()
