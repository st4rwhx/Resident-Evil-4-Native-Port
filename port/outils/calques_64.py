"""N7 (18/11) : recense les « calques » des .cpp : structures locales qui commencent par `u8 pad[0xNN]` pour lire les
champs d'une autre classe à une position fixe (cModelExt sur cModel, cPartsWk sur cCoord, GxWork sur GlobalWork...).
En 64 bits deux choses les cassent : un vrai pointeur dedans (8 octets au lieu de 4, décale la suite) et un décalage
fixe dans une classe à table virtuelle (le vptr fait 8 octets : tout le reste glisse de 4).
  python calques_64.py            -> liste (fichier, structure, pad, pointeurs bruts)
"""
import glob, os, re

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
INTERDITS = {'src/Tools/t_prim.cpp', 'src/Tools/t_util.cpp', 'src/Tools/tools.cpp', 'src/game/filter.cpp',
             'src/tools/db_light.cpp'}
RX = re.compile(r'^(struct|class) (\w+)[^;{]*\{\s*\n\s*u8 (pad\w*)\[(0x[0-9A-Fa-f]+)[^\]]*\];(.*?)^\};', re.M | re.S)
RXP = re.compile(r'^\s*(?!P32|FnP32|static|typedef)[A-Za-z_][\w:<>]*\s*\*+\s*\w+\s*(\[[^\]]*\])?;', re.M)


def main():
    tot = brut = 0
    for p in sorted(glob.glob(os.path.join(ROOT, 'src', '**', '*.cpp'), recursive=True) +
                    glob.glob(os.path.join(ROOT, 'include', '**', '*.h'), recursive=True)):
        rel = os.path.relpath(p, ROOT).replace(os.sep, '/')
        if rel in INTERDITS:
            continue
        s = open(p, encoding='utf-8', errors='replace').read()
        for m in RX.finditer(s):
            n = len(RXP.findall(m.group(5)))
            tot += 1
            brut += n > 0
            vd = 'VDELTA' if 'PORT_VDELTA' in m.group(0) else ''
            print('%-30s %-20s pad %-7s pointeurs bruts %d %s' % (rel, m.group(2), m.group(4), n, vd))
    print('calques', tot, 'dont avec pointeurs bruts', brut)


if __name__ == '__main__':
    main()
