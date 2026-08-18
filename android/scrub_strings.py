#!/usr/bin/env python3
"""Build de diffusion : efface d'une bibliotheque les noms de fichiers source que la bibliotheque
standard C++ du NDK (libc++abi, liee en statique) garde dans ses messages d'abandon
(cxa_demangle.cpp, ...). Ils ne disent rien du portage, mais tools/verifier_diffusion.py refuse tout
nom de fichier source. Chaque nom, entoure d'octets nuls ou de '/', est remplace par des 'x' de meme
longueur : les messages restent valides, rien d'autre ne bouge.

  python scrub_strings.py <libmain.so>
"""
import re
import sys

NOMS = rb"(?<=[\x00/])\w+\.(?:cpp|hpp)(?=\x00)"


def main():
    # mmap : le fichier fait plus de 100 Mo avec symboles et la machine peut manquer de memoire virtuelle (aucune copie en memoire)
    import mmap
    p = sys.argv[1]
    vus = []
    with open(p, 'r+b') as f, mmap.mmap(f.fileno(), 0) as d:
        for m in re.finditer(NOMS, d):
            vus.append(m.group().decode())
            d[m.start():m.end()] = b'x' * (m.end() - m.start())
        cle = b'C:/re4n/nightjar-gen'
        i = d.find(cle)
        while i >= 0:
            d[i:i + len(cle)] = b'.' * len(cle)
            i = d.find(cle, i + len(cle))
        d.flush()
    print('%s : %d nom(s) efface(s) %s' % (p, len(vus), ' '.join(sorted(set(vus)))))


if __name__ == '__main__':
    main()
