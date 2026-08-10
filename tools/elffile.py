#!/usr/bin/env python3
"""Minimal big-endian ELF32 reader (sections, symbols, RELA) for the REL writer."""
import struct
from dataclasses import dataclass
from typing import Dict, List, Optional

SHT_NOBITS = 8
SHT_SYMTAB = 2
SHT_RELA = 4
SHN_UNDEF = 0
SHN_ABS = 0xFFF1
SHN_COMMON = 0xFFF2
STB_LOCAL, STB_GLOBAL, STB_WEAK = 0, 1, 2
STT_SECTION = 3


@dataclass
class Section:
    index: int
    name: str
    type: int
    flags: int
    addr: int
    offset: int
    size: int
    link: int
    info: int
    align: int
    data: bytes


@dataclass
class Symbol:
    index: int
    name: str
    value: int
    size: int
    bind: int
    type: int
    shndx: int


@dataclass
class Rela:
    offset: int
    sym: int
    type: int
    addend: int


class Elf:
    def __init__(self, path: str):
        d = open(path, 'rb').read()
        assert d[:4] == b'\x7fELF' and d[5] == 2, f'{path}: not a big-endian ELF'
        self.path = path
        shoff = struct.unpack('>I', d[0x20:0x24])[0]
        shentsize, shnum, shstrndx = struct.unpack('>3H', d[0x2E:0x34])
        raw = [struct.unpack('>10I', d[shoff + i * shentsize:][:40]) for i in range(shnum)]
        strtab_off = raw[shstrndx][4]

        def cstr(off):
            e = d.index(b'\0', off)
            return d[off:e].decode('latin1')

        self.sections: List[Section] = []
        for i, (name, typ, flags, addr, off, size, link, info, align, entsz) in enumerate(raw):
            data = b'' if typ == SHT_NOBITS else d[off:off + size]
            self.sections.append(Section(i, cstr(strtab_off + name), typ, flags, addr, off, size, link, info, align, data))
        self.by_name: Dict[str, Section] = {}
        for s in self.sections:
            self.by_name.setdefault(s.name, s)
        symtab = next(s for s in self.sections if s.type == SHT_SYMTAB)
        symstr = self.sections[symtab.link].offset
        self.symbols: List[Symbol] = []
        for i in range(symtab.size // 16):
            name, value, size, info, other, shndx = struct.unpack('>3IBBH', symtab.data[16 * i:16 * i + 16])
            self.symbols.append(Symbol(i, cstr(symstr + name), value, size, info >> 4, info & 0xF, shndx))
        self.relas: Dict[int, List[Rela]] = {}  # target section index -> relocations
        for s in self.sections:
            if s.type == SHT_RELA:
                lst = self.relas.setdefault(s.info, [])
                for i in range(s.size // 12):
                    off, info, addend = struct.unpack('>IIi', s.data[12 * i:12 * i + 12])
                    lst.append(Rela(off, info >> 8, info & 0xFF, addend))

    def section(self, name: str) -> Optional[Section]:
        return self.by_name.get(name)

    def defined_globals(self) -> Dict[str, Symbol]:
        out = {}
        for s in self.symbols:
            if s.bind in (STB_GLOBAL, STB_WEAK) and s.shndx not in (SHN_UNDEF, SHN_COMMON) and s.name:
                out.setdefault(s.name, s)
        return out
