# -*- coding: utf-8 -*-
"""Comparaison au pixel de deux images PPM (P6) : banc A/B du rendu au niveau du modele (G1, 14/11).

    python -I cmp_ppm.py a.ppm b.ppm [diff.png] [--masque x0,y0,x1,y1 ...]

--masque : zones ignorees (bornes incluses). Zone connue : barres de temps du mode debug, x 0-20 y 90-140 (seule
difference entre deux passages identiques au village sur MuMu, 14/11 : ~100 pixels).

Affiche : taille, pixels differents (nombre et %), ecart maximal par canal, et la boite englobante des ecarts.
Avec un troisieme argument, ecrit une carte des ecarts (blanc = identique, rouge = different) si Pillow est present.
"""
import sys


def read_ppm(path):
    data = open(path, 'rb').read()
    parts, pos = [], 0
    while len(parts) < 4:   # P6 largeur hauteur max, separes par des blancs
        while data[pos:pos + 1].isspace():
            pos += 1
        start = pos
        while not data[pos:pos + 1].isspace():
            pos += 1
        parts.append(data[start:pos])
    pos += 1
    if parts[0] != b'P6':
        raise ValueError('%s : pas un PPM P6' % path)
    w, h = int(parts[1]), int(parts[2])
    return w, h, data[pos:pos + w * h * 3]


def main():
    args = sys.argv[1:]
    masks = []
    while '--masque' in args:
        i = args.index('--masque')
        masks.append(tuple(int(v) for v in args[i + 1].split(',')))
        del args[i:i + 2]
    sys.argv = [sys.argv[0]] + args
    a, b = sys.argv[1], sys.argv[2]
    wa, ha, pa = read_ppm(a)
    wb, hb, pb = read_ppm(b)
    if (wa, ha) != (wb, hb):
        print('tailles differentes : %dx%d contre %dx%d' % (wa, ha, wb, hb))
        return 2
    n = wa * ha
    diff = 0
    maxd = 0
    x0, y0, x1, y1 = wa, ha, -1, -1
    mask = bytearray(n)
    for i in range(n):
        o = 3 * i
        d = max(abs(pa[o] - pb[o]), abs(pa[o + 1] - pb[o + 1]), abs(pa[o + 2] - pb[o + 2]))
        if d and masks:
            x, y = i % wa, i // wa
            if any(m[0] <= x <= m[2] and m[1] <= y <= m[3] for m in masks):
                d = 0
        if d:
            diff += 1
            mask[i] = 1
            if d > maxd:
                maxd = d
            x, y = i % wa, i // wa
            x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
    print('%dx%d : %d pixels differents (%.4f %%), ecart max %d%s' % (
        wa, ha, diff, 100.0 * diff / n, maxd, (' ; boite x %d-%d y %d-%d' % (x0, x1, y0, y1)) if diff else ''))
    if len(sys.argv) > 3 and diff:
        try:
            from PIL import Image
            im = Image.new('RGB', (wa, ha), (255, 255, 255))
            px = im.load()
            for i in range(n):
                if mask[i]:
                    px[i % wa, i // wa] = (255, 0, 0)
            im.save(sys.argv[3])
        except ImportError:
            pass
    return 0 if diff == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
