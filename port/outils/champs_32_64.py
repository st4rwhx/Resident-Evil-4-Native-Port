"""N7 : pour une structure, les membres dont le décalage ou la taille diffère entre 32 et 64 bits (types DWARF).
  python champs_32_64.py <objets 32> <objets 64> Nom [Nom...]
Le premier membre qui « glisse » désigne la cause (pointeur non converti, long, vptr).
"""
import glob, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verite_dwarf as vd


def membres(d, noms):
    out = {}
    for o in sorted(glob.glob(os.path.join(d, '*.o'))):
        txt = subprocess.run([vd.DUMP, '--debug-info', o], capture_output=True, text=True, errors='replace').stdout
        cur, prof = None, None
        for blk in txt.split('\n\n'):
            m = re.search(r'^0x[0-9a-f]+:(\s+)DW_TAG_(\w+)', blk, re.M)
            if not m:
                continue
            p, tag = len(m.group(1)), m.group(2)
            if cur is not None and p <= prof:
                cur = None
            if tag in ('structure_type', 'class_type', 'union_type'):
                n = re.search(r'DW_AT_name\s+\("([^"]*)"\)', blk)
                if n and n.group(1) in noms and n.group(1) not in out and 'DW_AT_declaration' not in blk:
                    cur, prof = n.group(1), p
                    out[cur] = []
                continue
            if cur and tag in ('member', 'inheritance') and p == prof + 2:
                nm = re.search(r'DW_AT_name\s+\("([^"]*)"\)', blk)
                ty = re.search(r'DW_AT_type\s+\(0x[0-9a-f]+ "([^"]*)"\)', blk)
                lo = re.search(r'DW_AT_data_member_location\s+\((0x[0-9a-f]+)\)', blk)
                out[cur].append(((nm.group(1) if nm else '<base>'), int(lo.group(1), 16) if lo else 0, ty.group(1) if ty else '?'))
    return out


def main():
    noms = set(sys.argv[3:])
    a, b = membres(sys.argv[1], noms), membres(sys.argv[2], noms)
    for n in sys.argv[3:]:
        A, B = dict((x[0], x) for x in a.get(n, [])), b.get(n, [])
        print('== %s' % n)
        dec_prec = 0
        for nm, off, ty in B:
            o32 = A.get(nm, (nm, None, '?'))[1]
            if o32 is None:
                continue
            d = off - o32
            if d != dec_prec:
                print('   %-22s 32b 0x%-5X 64b 0x%-5X (glisse de %+d)  type %s' % (nm, o32, off, d - dec_prec, ty[:60]))
                dec_prec = d


if __name__ == '__main__':
    main()
