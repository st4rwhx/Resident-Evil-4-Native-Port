"""N7 (18/11) : complément de p32_reecrire.py pour les champs que la passe DWARF n'a pas vus (types absents du build de
types : surtout les modules st*, les outils, cSatBlock...). Critère textuel strict : une ligne de membre
`T* nom;` ou `T* nom[N];` suivie d'un commentaire de décalage `// 0xNN` (le décompilé décrit la disposition de la
console). Les doubles pointeurs deviennent P32<P32<T>> (la table pointée a des cases de 4 octets).
  python p32_texte.py [--appliquer]
"""
import glob, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
INTERDITS = {x.lower() for x in ('src/Tools/t_prim.cpp', 'src/Tools/t_util.cpp', 'src/Tools/tools.cpp',
                                  'src/game/filter.cpp', 'src/tools/db_light.cpp', 'port/ptr32.h')}
RX = re.compile(r'^(\s+)((?:const\s+)?(?:struct\s+|class\s+)?[A-Za-z_][A-Za-z_0-9:]*)(\s*)(\*+)(\s*)([A-Za-z_][A-Za-z_0-9]*)'
                r'(\s*(?:\[[^\]]*\])?\s*;)(\s*//\s*0x[0-9A-Fa-f]+.*)$')


def reecrire_ligne(l):
    m = RX.match(l)
    if not m:
        return None
    ind, t, sp1, etoiles, sp2, nom, fin, com = m.groups()
    typ = t
    for _ in range(len(etoiles)):
        typ = 'P32<%s>' % typ
    avant = ind + t + sp1 + etoiles + sp2 + nom + fin
    apres = ind + typ + ' ' + nom + fin
    # garder la colonne du commentaire quand c'est possible
    pad = len(avant) - len(apres)
    if pad > 0:
        apres += ' ' * pad
    elif not com.startswith(' '):
        apres += ' '
    return apres + com


def main():
    tot = 0
    for p in sorted(glob.glob(os.path.join(ROOT, 'src', '**', '*.cpp'), recursive=True) +
                    glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True)):
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        if rel.lower() in INTERDITS:
            continue
        data = open(p, 'rb').read().decode('utf-8', errors='surrogateescape')
        lignes = data.split('\n')
        n = 0
        for i, l in enumerate(lignes):
            cr = l.endswith('\r')
            corps = l[:-1] if cr else l
            if 'P32' in corps or '(' in corps.split('//')[0]:
                continue
            r = reecrire_ligne(corps)
            if r is not None:
                lignes[i] = r + ('\r' if cr else '')
                n += 1
        if n:
            tot += n
            print('%-36s %d' % (rel, n))
            if '--appliquer' in sys.argv:
                open(p, 'wb').write('\n'.join(lignes).encode('utf-8', errors='surrogateescape'))
    print('total', tot)


if __name__ == '__main__':
    main()
