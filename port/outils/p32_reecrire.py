"""Phase N7 (17/11) : réécrit les champs pointeurs des structures du jeu en P32<T> / FnP32<F> (port/ptr32.h).

Les lignes exactes viennent des types DWARF (build PORT_TYPES : decl_file / decl_line de chaque membre pointeur), donc
aucune recherche approximative dans le texte. En 32 bits P32<T> = T* : la réécriture ne change pas le code machine.
Chaque ligne réécrite garde sa fin de ligne d'origine (LF ou CRLF) et la colonne de son commentaire.
  python p32_reecrire.py <dossiers d'objets...> [--appliquer]   (sans --appliquer : compte et liste ce qui résiste)
"""
import glob, os, re, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verite_dwarf as vd

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')).replace('\\', '/')
COPIES = ['C:/re4n/re4port_pc']   # copies de travail du build (pc_build.sh) : ramenées au dépôt
# Fichiers modifiés par d'autres sessions, non commités : ne jamais y écrire (règle du carnet §1).
INTERDITS = {'src/Tools/t_prim.cpp', 'src/Tools/t_util.cpp', 'src/Tools/tools.cpp', 'src/game/filter.cpp',
             'src/tools/db_light.cpp'}


def chemin_reel(f):
    f = f.replace('\\', '/')
    for c in COPIES:
        if f.lower().startswith(c.lower() + '/'):
            f = ROOT + f[len(c):]
            f = f.replace('/src/tools_lc/', '/src/tools/')
    return f


def du_jeu(f):
    r = f[len(ROOT) + 1:] if f.startswith(ROOT + '/') else None
    if not r:
        return None
    if r.startswith('include/') and r.count('/') == 1:
        return r
    if r.startswith('src/') and not r.startswith('src/lib/'):
        return r
    return None


def membres(objs):
    out = {}   # (fichier relatif, ligne) -> {nom: genre}
    for o in objs:
        dies, children = vd.parse(o)

        def strip(t):
            while t in dies and dies[t]['tag'] in ('typedef', 'const_type', 'volatile_type'):
                t = dies[t].get('type')
            return t
        for off, d in dies.items():
            if d['tag'] != 'member' or 'name' not in d or 'file' not in d or 'line' not in d:
                continue
            if d['name'].startswith('_vptr'):
                continue
            rel = du_jeu(chemin_reel(d['file']))
            if not rel:
                continue
            t = strip(d.get('type'))
            if t in dies and dies[t]['tag'] == 'array_type':
                t = strip(dies[t].get('type'))
            if t not in dies or dies[t]['tag'] != 'pointer_type':
                continue
            pt = strip(dies[t].get('type'))
            genre = 'fn' if pt in dies and dies[pt]['tag'] == 'subroutine_type' else 'ptr'
            out.setdefault((rel, int(d['line'])), {})[d['name']] = genre
    return out


QUAL = r'(?:(?:const|volatile|class|struct|union|unsigned|signed|enum)\s+)*'
TYPE = QUAL + r'[A-Za-z_][\w:]*(?:<[^;()]*?>)?(?:\s+(?:const|volatile))?'


def reecrire(ligne, nom, genre):
    if 'P32<' in ligne or 'DiscPtr<' in ligne or 'DISC_PTR(' in ligne:
        return None, 'deja'
    corps = ligne.rstrip('\r\n')
    fin = ligne[len(corps):]
    code, sep, comm = corps.partition('//')
    if re.match(r'^\s*static\b', code):
        return None, 'statique'   # champ statique : hors de la structure, ne rien changer

    def avec_comm(nouv):
        if not sep:
            return nouv + fin
        return nouv.ljust(max(len(code), len(nouv) + 1)) + sep + comm + fin

    if genre == 'fn':
        mt = re.match(r'^(\s*)([A-Za-z_]\w*)\s+' + re.escape(nom) + r'\s*;\s*$', code)
        if mt:   # pointeur de fonction par type nommé : TaskFunc func;
            return avec_comm('%sFnP32<%s> %s;' % (mt.group(1), mt.group(2), nom)), 'ok'
        m = re.match(r'^(\s*)(.+?)\(\s*\*\s*' + re.escape(nom) + r'\s*\)\s*(\(.*\))\s*;\s*$', code)
        if not m or ',' in m.group(2):
            return None, 'forme'
        return avec_comm('%sFnP32<%s(*)%s> %s;' % (m.group(1), m.group(2).rstrip(), m.group(3), nom)), 'ok'
    m = re.match(r'^(\s*)(' + TYPE + r')\s*(\*+)\s*' + re.escape(nom) + r'\s*((?:\[[^\]]*\])*)\s*;\s*$', code)
    if not m:
        return None, 'forme'
    ind, typ, et, arr = m.groups()
    t = typ.strip()
    for _ in range(len(et)):
        t = 'P32<%s>' % t
    return avec_comm('%s%s %s%s;' % (ind, t, nom, arr)), 'ok'


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    appliquer = '--appliquer' in sys.argv
    objs = []
    for a in args:
        objs += sorted(glob.glob(os.path.join(a, '*.o')))
    mem = membres(objs)
    stats = collections.Counter()
    resiste = []
    par_fichier = collections.defaultdict(dict)
    for (rel, ligne), noms in mem.items():
        if rel in INTERDITS:
            stats['interdit'] += len(noms)
            continue
        if len(noms) > 1:
            stats['plusieurs'] += len(noms)
            resiste.append((rel, ligne, ','.join(noms), 'plusieurs déclarateurs'))
            continue
        par_fichier[rel][ligne] = next(iter(noms.items()))
    for rel, lignes in sorted(par_fichier.items()):
        p = os.path.join(ROOT, rel)
        try:
            data = open(p, 'rb').read().decode('utf-8')
        except (OSError, UnicodeDecodeError):
            stats['illisible'] += len(lignes)
            continue
        L = data.splitlines(True)
        change = False
        for ligne, (nom, genre) in sorted(lignes.items()):
            if ligne - 1 >= len(L):
                stats['hors'] += 1
                continue
            nouv, etat = reecrire(L[ligne - 1], nom, genre)
            stats[etat + ('_' + genre if etat == 'ok' else '')] += 1
            if etat == 'ok':
                L[ligne - 1] = nouv
                change = True
            elif etat == 'forme':
                resiste.append((rel, ligne, nom, L[ligne - 1].strip()[:90]))
        if change and appliquer:
            open(p, 'wb').write(''.join(L).encode('utf-8'))
            stats['fichiers'] += 1
    print('membres pointeurs du jeu trouvés : %d lignes' % len(mem))
    print(dict(stats))
    for r in resiste[:40]:
        print('  RESISTE %s:%d %s : %s' % r)
    if len(resiste) > 40:
        print('  ... et %d autres' % (len(resiste) - 40))


if __name__ == '__main__':
    main()
