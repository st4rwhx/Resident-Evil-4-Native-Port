#!/usr/bin/env python3
"""Populate orig/<ver>/ from the disc: the DOL, Bio4.sym, and for every REL module of config.yml its
original REL (loose files/Rel/<mod>.rel, or extracted from files/em/<mod>.drs with tools/drs.py) and
debug symbol file files/Bio4.<mod>.sym.

usage: extract_orig.py <config.yml> <disc> [--force]
       <disc> is a GameCube image (.iso/.gcm, read directly) or a directory extracted from one
       (`dtk disc extract <iso> <dir>`). Existing files are kept unless --force.

configure.py runs this when a configured module's orig REL is missing and orig/<ver>/ holds an image.
"""
import os
import struct
import sys

import yaml

sys.path.insert(0, os.path.dirname(__file__))
import dol_sections  # noqa: E402
from drs import Drs  # noqa: E402

DOL_SPLIT_ADDR = 0x8021C9E0  # .ctors/.dtors share one DOL section; dtk needs two (README)


class Gcm:
    """Minimal GameCube disc image reader. Paths follow dtk's extracted layout: files/<fst path>,
    sys/main.dol."""

    def __init__(self, path):
        self.f = open(path, 'rb')
        self.f.seek(0x420)
        self.dol_off, fst_off, fst_size = struct.unpack('>3I', self.f.read(12))
        self.f.seek(fst_off)
        fst = self.f.read(fst_size)
        count = struct.unpack('>I', fst[8:12])[0]
        names = fst[12 * count:]
        self.files = {}  # lower-case path -> (path, offset, size)
        stack = [('', count)]
        i = 1
        while i < count:
            while i >= stack[-1][1]:
                stack.pop()
            flags_name, a, b = struct.unpack('>3I', fst[12 * i:12 * i + 12])
            name = names[flags_name & 0xFFFFFF:].split(b'\0', 1)[0].decode('shift_jis')
            path = f'{stack[-1][0]}/{name}' if stack[-1][0] else name
            if flags_name >> 24:
                stack.append((path, b))
            else:
                self.files[path.lower()] = (path, a, b)
            i += 1

    @staticmethod
    def _fst_path(path):
        assert path == 'files' or path.startswith('files/'), path
        return path[6:].lower()

    def listdir(self, d):
        d = self._fst_path(d)
        d = d + '/' if d else ''
        return [p[len(d):] for p, _, _ in self.files.values() if p.lower().startswith(d) and '/' not in p[len(d):]]

    def exists(self, path):
        return path == 'sys/main.dol' or self._fst_path(path) in self.files

    def read(self, path):
        if path == 'sys/main.dol':
            self.f.seek(self.dol_off)
            hdr = self.f.read(0x100)
            offs = struct.unpack('>18I', hdr[:0x48])
            sizes = struct.unpack('>18I', hdr[0x90:0xD8])
            size = max(o + s for o, s in zip(offs, sizes))
            self.f.seek(self.dol_off)
            return self.f.read(size)
        _, off, size = self.files[self._fst_path(path)]
        self.f.seek(off)
        return self.f.read(size)


class DiscDir:
    def __init__(self, root):
        self.root = root

    def _find(self, path):
        d = self.root
        for part in path.split('/'):
            m = [e for e in os.listdir(d) if e.lower() == part.lower()]
            if not m:
                raise KeyError(path)
            d = os.path.join(d, m[0])
        return d

    def listdir(self, d):
        return os.listdir(self._find(d))

    def exists(self, path):
        try:
            self._find(path)
            return True
        except KeyError:
            return False

    def read(self, path):
        return open(self._find(path), 'rb').read()


def open_disc(path):
    return DiscDir(path) if os.path.isdir(path) else Gcm(path)


def main():
    args = [a for a in sys.argv[1:] if a != '--force']
    force = '--force' in sys.argv
    cfg_path, disc_path = args
    cfg = yaml.safe_load(open(cfg_path))
    base = cfg['object_base']
    disc = open_disc(disc_path)
    files = {e.lower(): e for e in disc.listdir('files')}

    def want(rel):
        out = os.path.join(base, rel)
        if os.path.exists(out) and not force:
            return None
        os.makedirs(os.path.dirname(out), exist_ok=True)
        return out

    out = want('sys/main.dol')
    if out:
        open(out, 'wb').write(disc.read('sys/main.dol'))
        print(out)
    out = want('sys/main_split.dol')
    if out:
        dol_sections.split(os.path.join(base, 'sys/main.dol'), out, DOL_SPLIT_ADDR)
    out = want('files/Bio4.sym')
    if out:
        open(out, 'wb').write(disc.read('files/Bio4.sym'))
        print(out)
    for m in cfg.get('modules', []):
        name = m['name']
        out = want(m['object'])
        if out:
            d, fn = m['object'].split('/')[1:]
            if d == 'Rel':
                if not disc.exists(m['object']):
                    print(f'{out}: not on this disc (st3_* are on disc 2)')
                    continue
                data = disc.read(m['object'])
            else:
                a = Drs(disc.read(f'files/{d}/{name}.drs'))
                assert a.rel is not None, f'{name}.drs has no REL'
                data = a.rel
            open(out, 'wb').write(data)
            print(f'{out} ({len(data):#x} bytes)')
        sym = files.get(f'bio4.{name}.sym'.lower())
        if sym:
            out = want(f'files/{sym}')
            if out:
                open(out, 'wb').write(disc.read(f'files/{sym}'))
                print(out)


if __name__ == '__main__':
    main()
