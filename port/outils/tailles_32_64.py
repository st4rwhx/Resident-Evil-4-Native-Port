"""N7 : compare la taille de chaque structure du jeu entre la build 32 bits et la build 64 bits (types DWARF PORT_TYPES).

Avec les pointeurs comprimés (P32), une structure doit garder sa taille GameCube. Une taille qui change = un champ
pointeur non converti (ou `long`), ou une classe virtuelle (vptr de 8 octets).
  python tailles_32_64.py <objets 32 bits> <objets 64 bits>
"""
import glob, os, re, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verite_dwarf as vd


def tailles(d):
    out = {}
    vptr = set()
    for o in sorted(glob.glob(os.path.join(d, '*.o'))):
        dies, children = vd.parse(o)
        for off, x in dies.items():
            if x['tag'] in vd.AGG and 'name' in x and 'decl' not in x:
                f = (x.get('file') or '').replace('\\', '/')
                if '/src/lib/' in f or '/include/dolphin' in f or ('/include/' not in f and '/src/' not in f):
                    continue
                blk = None
                m = re.search(r'DW_AT_byte_size\s+\((0x[0-9a-f]+)\)', '')
                out.setdefault(x['name'], None)
        # deuxième passe : lire byte_size dans le texte brut (parse ne la garde pas)
    return out


def tailles_brutes(d):
    import subprocess
    out, vp = {}, set()
    for o in sorted(glob.glob(os.path.join(d, '*.o'))):
        txt = subprocess.run([vd.DUMP, '--debug-info', o], capture_output=True, text=True, errors='replace').stdout
        nom = None
        for blk in txt.split('\n\n'):
            m = re.search(r'DW_TAG_(structure_type|class_type|union_type)', blk)
            if m:
                n = re.search(r'DW_AT_name\s+\("([^"]*)"\)', blk)
                t = re.search(r'DW_AT_byte_size\s+\((0x[0-9a-f]+)\)', blk)
                f = re.search(r'DW_AT_decl_file\s+\("([^"]*)"\)', blk)
                nom = n.group(1) if n else None
                if n and t and f and 'DW_AT_declaration' not in blk:
                    ff = f.group(1).replace('\\', '/')
                    if '/src/lib/' in ff or '/include/dolphin' in ff or ('/include/' not in ff and '/src/' not in ff):
                        nom = None
                        continue
                    out.setdefault(nom, int(t.group(1), 16))
                continue
            if nom and 'DW_TAG_member' in blk and '_vptr' in blk:
                vp.add(nom)
    return out, vp


def main():
    a, va = tailles_brutes(sys.argv[1])
    b, vb = tailles_brutes(sys.argv[2])
    diff = [(k, a[k], b[k]) for k in sorted(a) if k in b and a[k] != b[k]]
    print('structures communes : %d ; tailles différentes en 64 bits : %d' % (len(set(a) & set(b)), len(diff)))
    for k, x, y in diff:
        print('  %-34s 32b 0x%-5X 64b 0x%-5X %s' % (k, x, y, 'vptr' if k in vb else ''))


if __name__ == '__main__':
    main()
