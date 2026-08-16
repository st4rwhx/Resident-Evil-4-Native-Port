"""Phase N7 (64 bits) : inventaire des champs pointeurs des structures du jeu, depuis les types DWARF (build PORT_TYPES).

Pour chaque structure / classe nommée définie dans les en-têtes du jeu (include/, src/) : nombre de champs pointeurs de
données, de pointeurs de fonction, et présence d'un vptr (classe virtuelle). En 64 bits, chacun grandit de 4 octets et
décale la disposition GameCube.
  python inventaire_64.py <dossier d'objets PORT_TYPES> [sortie.tsv]
"""
import glob, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verite_dwarf as vd


def main():
    objs = sorted(glob.glob(os.path.join(sys.argv[1], '*.o')))
    out = sys.argv[2] if len(sys.argv) > 2 else None
    res = {}
    for o in objs:
        dies, children = vd.parse(o)

        def strip(t):
            while t in dies and dies[t]['tag'] in ('typedef', 'const_type', 'volatile_type'):
                t = dies[t].get('type')
            return t
        for off, d in dies.items():
            if d['tag'] not in vd.AGG or 'name' not in d or 'decl' in d:
                continue
            f = (d.get('file') or '').replace('\\', '/')
            if '/include/' not in f and '/src/' not in f:
                continue
            if '/src/lib/' in f or '/include/dolphin' in f:
                continue
            name = d['name']
            if name in res:
                continue
            ptr = fptr = 0
            vptr = False
            for c in children.get(off, ()):
                m = dies[c]
                if m['tag'] != 'member':
                    continue
                if m.get('name', '').startswith('_vptr'):
                    vptr = True
                    continue
                t = strip(m.get('type'))
                if t in dies and dies[t]['tag'] == 'pointer_type':
                    pt = strip(dies[t].get('type'))
                    if pt in dies and dies[pt]['tag'] == 'subroutine_type':
                        fptr += 1
                    else:
                        ptr += 1
                elif t in dies and dies[t]['tag'] == 'array_type':
                    et = strip(dies[t].get('type'))
                    if et in dies and dies[et]['tag'] == 'pointer_type':
                        ptr += 1
            res[name] = (ptr, fptr, vptr, os.path.basename(f))
    touch = {k: v for k, v in res.items() if v[0] or v[1] or v[2]}
    print('structures du jeu : %d ; touchées en 64 bits : %d' % (len(res), len(touch)))
    print('  champs pointeurs de données : %d ; pointeurs de fonction : %d ; classes virtuelles : %d' % (
        sum(v[0] for v in touch.values()), sum(v[1] for v in touch.values()), sum(1 for v in touch.values() if v[2])))
    for k, v in sorted(touch.items(), key=lambda kv: -(kv[1][0] + kv[1][1]))[:25]:
        print('  %-28s ptr %3d  fn %2d  vptr %s  (%s)' % (k, v[0], v[1], 'oui' if v[2] else 'non', v[3]))
    if out:
        with open(out, 'w') as fh:
            fh.write('structure\tpointeurs\tfonctions\tvptr\tfichier\n')
            for k, v in sorted(touch.items()):
                fh.write('%s\t%d\t%d\t%d\t%s\n' % (k, v[0], v[1], int(v[2]), v[3]))


if __name__ == '__main__':
    main()
