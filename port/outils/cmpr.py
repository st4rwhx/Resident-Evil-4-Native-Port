"""Decode une texture GameCube CMPR (gros-boutiste, tuiles 8x8 de 4 sous-blocs DXT1) en PNG.
   cmpr.py <fichier.bin> <largeur> <hauteur> <sortie.png> [le]   (le : couleurs 16 bits lues en petit-boutiste)"""
import struct
import sys

from PIL import Image


def c565(v):
    r = (v >> 11) & 31
    g = (v >> 5) & 63
    b = v & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 255)


def decode(data, w, h, le=False):
    im = Image.new('RGBA', (w, h))
    px = im.load()
    off = 0
    fmt = '<HH' if le else '>HH'
    for ty in range(0, h, 8):
        for tx in range(0, w, 8):
            for sb in range(4):
                bx = tx + (sb & 1) * 4
                by = ty + (sb >> 1) * 4
                if off + 8 > len(data):
                    return im
                c0, c1 = struct.unpack_from(fmt, data, off)
                idx = data[off + 4:off + 8]
                off += 8
                p0, p1 = c565(c0), c565(c1)
                if c0 > c1:
                    p2 = tuple((2 * a + b) // 3 for a, b in zip(p0, p1))
                    p3 = tuple((a + 2 * b) // 3 for a, b in zip(p0, p1))
                else:
                    p2 = tuple((a + b) // 2 for a, b in zip(p0, p1))
                    p3 = (0, 0, 0, 0)
                pal = (p0, p1, p2, p3)
                for y in range(4):
                    row = idx[y]
                    for x in range(4):
                        k = (row >> (6 - 2 * x)) & 3
                        if bx + x < w and by + y < h:
                            px[bx + x, by + y] = pal[k]
    return im


if __name__ == '__main__':
    f, w, h, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    le = len(sys.argv) > 5 and sys.argv[5] == 'le'
    decode(open(f, 'rb').read(), w, h, le).save(out)
    print(out)
