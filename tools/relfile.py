#!/usr/bin/env python3
"""Reader for Nintendo REL (relocatable module) files as produced by SN's snmakerel.

Used by gen_rel_config.py (config generation) and make_rel.py (REL writer / verification).
"""
import struct
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

R_PPC_ADDR32 = 1
R_PPC_ADDR16_LO = 4
R_PPC_ADDR16_HI = 5
R_PPC_ADDR16_HA = 6
R_PPC_REL24 = 10
R_PPC_REL14 = 11
R_DOLPHIN_NOP = 201
R_DOLPHIN_SECTION = 202
R_DOLPHIN_END = 203

# Section names in REL section-table order (only non-empty entries carry a name).
SECTION_NAMES = ['.text', '.ctors', '.dtors', '.rodata', '.data', '.bss']


@dataclass
class Reloc:
    module: int      # target module id (0 = DOL)
    section: int     # source section index (REL index)
    offset: int      # offset in source section
    kind: int        # R_PPC_*
    target_section: int  # target section index (REL index of target module; DOL ELF index for module 0)
    addend: int      # target offset (module 0: absolute address)


@dataclass
class Rel:
    module_id: int
    num_sections: int
    section_info_offset: int
    name_offset: int
    name_size: int
    version: int
    bss_size: int
    rel_offset: int
    imp_offset: int
    imp_size: int
    prolog_section: int
    epilog_section: int
    unresolved_section: int
    bss_section: int
    prolog: int
    epilog: int
    unresolved: int
    align: int
    bss_align: int
    fix_size: int
    # index -> (file offset, exec, size); only entries with offset or size
    sections: Dict[int, Tuple[int, bool, int]] = field(default_factory=dict)
    imports: List[Tuple[int, int]] = field(default_factory=list)  # (module id, reloc table offset)
    relocs: List[Reloc] = field(default_factory=list)
    data: bytes = b''

    def section_name(self, idx: int) -> str:
        used = [i for i in sorted(self.sections)]
        return SECTION_NAMES[used.index(idx)]

    def section_index(self, name: str) -> int:
        used = [i for i in sorted(self.sections)]
        return used[SECTION_NAMES.index(name)]

    def section_data(self, idx: int) -> bytes:
        off, _, size = self.sections[idx]
        if off == 0:
            return b'\0' * size
        return self.data[off:off + size]

    def size(self) -> int:
        return len(self.data)


def parse(path: str) -> Rel:
    d = open(path, 'rb').read()
    f = struct.unpack('>12I', d[:0x30])
    assert f[1] == 0 and f[2] == 0, 'next/prev link fields must be zero on disc'
    ps, es, us, bs = d[0x30:0x34]
    prolog, epilog, unres, align, bss_align, fix_size = struct.unpack('>6I', d[0x34:0x4C])
    r = Rel(f[0], *f[3:], ps, es, us, bs, prolog, epilog, unres, align, bss_align, fix_size)
    assert r.version == 3, r.version
    assert r.section_info_offset == 0x4C
    for i in range(r.num_sections):
        o, s = struct.unpack('>2I', d[r.section_info_offset + 8 * i:][:8])
        if o or s:
            r.sections[i] = (o & ~3, bool(o & 1), s)
    # modules without .bss (bss_size 0) have no entry for it
    assert len(r.sections) == len(SECTION_NAMES) - (r.bss_size == 0), (path, r.sections)
    for i in range(r.imp_size // 8):
        r.imports.append(struct.unpack('>2I', d[r.imp_offset + 8 * i:][:8]))
    end = 0
    for mod, off in r.imports:
        p = off
        sec = None
        addr = 0
        while True:
            o, ty, s, a = struct.unpack('>HBBI', d[p:p + 8])
            p += 8
            if ty == R_DOLPHIN_END:
                break
            if ty == R_DOLPHIN_SECTION:
                sec = s
                addr = 0
                continue
            if ty == R_DOLPHIN_NOP:
                addr += o
                continue
            addr += o
            r.relocs.append(Reloc(mod, sec, addr, ty, s, a))
        end = max(end, p)
    # the REL ends with the last relocation list
    r.data = d[:end]
    assert end == len(d), (path, hex(end), hex(len(d)))
    return r


if __name__ == '__main__':
    import sys
    for p in sys.argv[1:]:
        r = parse(p)
        print(p, 'id', r.module_id, 'nsec', r.num_sections, 'name', hex(r.name_offset), hex(r.name_size),
              'align', r.align, r.bss_align, 'fix', hex(r.fix_size))
        for i, (o, x, s) in sorted(r.sections.items()):
            print(f'  [{i:2}] {r.section_name(i):8} off {o:#8x} size {s:#8x}{" exec" if x else ""}')
        print('  imports', r.imports, len(r.relocs), 'relocs')
