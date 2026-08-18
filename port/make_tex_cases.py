# -*- coding: utf-8 -*-
"""Fabrique les cas de test du decodeur de textures, depuis de **vraies** textures du jeu.

L'oracle est `tools/re4tpl.py` du depot RE4444 : un decodeur Python deja passe sur des milliers
d'images de RE4, dont les pieges (tuiles 8x4, RGBA8 en deux moities) ont ete payes une fois.
Le C++ embarque doit en etre la copie exacte ; ce script produit de quoi le **prouver sur
l'appareil**, octet a octet, plutot que de comparer deux images a l'oeil.

Sortie : un seul fichier binaire, petit-boutiste (la cible est ARM) :
    u32 magic 'TXCS', u32 nombre de cas
    par cas : u32 format, u32 largeur, u32 hauteur, u32 taille brute, u32 entrees de palette,
              puis les octets bruts, la palette depliee en RGBA, et le RGBA attendu.

usage : python port/make_tex_cases.py <dossier de TPL> <sortie.bin> [cas par format]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tools import re4tpl  # noqa: E402

# Octets par tuile et taille de tuile, pour savoir combien d'octets bruts recopier.
TILE = {0: (8, 8, 32), 1: (8, 4, 32), 2: (8, 4, 32), 3: (4, 4, 32), 4: (4, 4, 32),
        5: (4, 4, 32), 6: (4, 4, 64), 8: (8, 8, 32), 9: (8, 4, 32), 10: (4, 4, 32),
        14: (8, 8, 32)}


def raw_size(fmt, w, h):
    tw, th, bpt = TILE[fmt]
    return ((w + tw - 1) // tw) * ((h + th - 1) // th) * bpt


def cases_from(path, want, per_fmt):
    d = open(path, 'rb').read()
    if d[:4] != b'\x00\x20\xaf\x30':
        return
    n, tab = struct.unpack_from('>II', d, 4)
    for i in range(n):
        io_, pal_off = struct.unpack_from('>II', d, tab + 8 * i)
        h, w, fmt, off = struct.unpack_from('>HHII', d, io_)
        if fmt not in TILE or w == 0 or h == 0 or w > 1024 or h > 1024:
            continue
        if want.get(fmt, 0) >= per_fmt:
            continue
        need = raw_size(fmt, w, h)
        if off + need > len(d):
            continue                      # image tronquee : elle ne prouverait rien
        pal = re4tpl._palette(d, pal_off) if (fmt in (8, 9, 10) and pal_off) else None
        rgba = bytes(re4tpl._decode(fmt, d, off, w, h, pal))
        flat = b''.join(bytes(c) for c in (pal or []))
        want[fmt] = want.get(fmt, 0) + 1
        yield (fmt, w, h, d[off:off + need], len(pal or []), flat, rgba)


def synth(per_fmt):
    """Cas synthetiques, pour les formats que RE4 GameCube n'emploie pas.

    Le recensement du corpus (227 TPL, 1002 images) est net : le jeu n'utilise que **I4, IA8 et
    CMPR**. Les six autres formats restent donc du code jamais execute si on s'en tient aux
    donnees reelles - et du code jamais execute est du code faux. On le couvre avec du bruit
    reproductible, juge par le meme oracle Python.
    """
    import random
    for fmt in (1, 2, 4, 5, 6, 8, 9, 10):
        for k in range(per_fmt):
            rnd = random.Random(fmt * 100 + k)
            w, h = (8, 8) if k == 0 else ((16, 8) if k == 1 else (12, 20))
            need = raw_size(fmt, w, h)
            raw = bytes(rnd.randrange(256) for _ in range(need))
            pal = None
            if fmt in (8, 9, 10):
                npal = 16 if fmt == 8 else 256
                pal = [(rnd.randrange(256), rnd.randrange(256),
                        rnd.randrange(256), rnd.randrange(256)) for _ in range(npal)]
            rgba = bytes(re4tpl._decode(fmt, raw, 0, w, h, pal))
            flat = b''.join(bytes(c) for c in (pal or []))
            yield (fmt, w, h, raw, len(pal or []), flat, rgba)


def main():
    src, out = sys.argv[1], sys.argv[2]
    per_fmt = int(sys.argv[3]) if len(sys.argv) > 3 else 3

    want, blobs = {}, []
    for root, _, files in os.walk(src):
        for f in sorted(files):
            if not f.lower().endswith('.tpl'):
                continue
            try:
                for c in cases_from(os.path.join(root, f), want, per_fmt):
                    blobs.append(c)
            except Exception:
                continue          # un TPL malforme ne doit pas arreter la fabrication
        if len(want) >= 9 and all(v >= per_fmt for v in want.values()):
            break

    blobs.extend(synth(per_fmt))

    with open(out, 'wb') as fh:
        fh.write(struct.pack('<4sI', b'TXCS', len(blobs)))
        for fmt, w, h, raw, npal, pal, rgba in blobs:
            fh.write(struct.pack('<IIIII', fmt, w, h, len(raw), npal))
            fh.write(raw)
            fh.write(pal)
            fh.write(rgba)

    print('%d cas, %d octets' % (len(blobs), os.path.getsize(out)))
    import collections
    for fmt, k in sorted(collections.Counter(b[0] for b in blobs).items()):
        print('   format %-3d : %d cas%s' % (fmt, k, '' if fmt in want else '  (synthetique)'))


if __name__ == '__main__':
    main()
