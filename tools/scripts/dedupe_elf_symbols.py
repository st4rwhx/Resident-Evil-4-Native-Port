#!/usr/bin/env python3
"""Auto-discovers every symbol NAME that is defined (shndx != SHN_UNDEF) in
more than one of the given ELF32 object files, keeps exactly ONE defined
occurrence (the first one found, in file order = link order), and converts
every other occurrence into an undefined external reference so the linker
resolves callers against the single surviving definition instead of
complaining about multiply-defined symbols. This does not touch the actual
code/data bytes in any section - only symbol-table metadata."""
import struct
import sys
from collections import defaultdict

SHN_UNDEF = 0


class Elf:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            self.data = bytearray(f.read())
        self.endian = ">" if self.data[5] == 2 else "<"
        self.e_shoff = struct.unpack_from(self.endian + "I", self.data, 0x20)[0]
        self.e_shentsize = struct.unpack_from(self.endian + "H", self.data, 0x2E)[0]
        self.e_shnum = struct.unpack_from(self.endian + "H", self.data, 0x30)[0]
        self.sections = []
        for i in range(self.e_shnum):
            off = self.e_shoff + i * self.e_shentsize
            vals = struct.unpack_from(self.endian + "10I", self.data, off)
            (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info,
             sh_addralign, sh_entsize) = vals
            self.sections.append(dict(name=sh_name, type=sh_type, offset=sh_offset,
                                       size=sh_size, link=sh_link, entsize=sh_entsize))
        SHT_SYMTAB = 2
        self.symtab_idx = next((i for i, s in enumerate(self.sections) if s["type"] == SHT_SYMTAB), None)
        if self.symtab_idx is not None:
            symtab = self.sections[self.symtab_idx]
            self.strtab = self.sections[symtab["link"]]
        else:
            self.strtab = None

    def get_str(self, idx):
        base = self.strtab["offset"]
        end = self.data.index(b"\0", base + idx)
        return self.data[base + idx:end].decode("utf-8", "replace")

    def iter_symbols(self):
        if self.symtab_idx is None:
            return
        symtab = self.sections[self.symtab_idx]
        entsize = symtab["entsize"] or 16
        count = symtab["size"] // entsize
        for i in range(count):
            off = symtab["offset"] + i * entsize
            st_name, st_value, st_size, st_info, st_other, st_shndx = \
                struct.unpack_from(self.endian + "3I2BH", self.data, off)
            name = self.get_str(st_name) if st_name else ""
            yield i, off, name, st_value, st_size, st_info, st_shndx

    def undefine_symbol(self, off):
        struct.pack_into(self.endian + "I", self.data, off + 4, 0)
        struct.pack_into(self.endian + "I", self.data, off + 8, 0)
        struct.pack_into(self.endian + "H", self.data, off + 14, SHN_UNDEF)

    def save(self):
        with open(self.path, "wb") as f:
            f.write(self.data)


def main(paths):
    elfs = [Elf(p) for p in paths]

    # Discover every name defined in more than one file (or more than once
    # within a single file), across ALL names, not a hardcoded list.
    def_count = defaultdict(int)
    for elf in elfs:
        for i, off, name, val, size, info, shndx in elf.iter_symbols():
            if name and shndx != SHN_UNDEF:
                def_count[name] += 1

    dup_names = {name for name, c in def_count.items() if c > 1}
    print(f"Found {len(dup_names)} duplicate-defined symbol name(s) across {len(paths)} files")

    kept = {}
    for fi, elf in enumerate(elfs):
        for i, off, name, val, size, info, shndx in elf.iter_symbols():
            if name in dup_names and shndx != SHN_UNDEF and name not in kept:
                kept[name] = (fi, i, val)

    for name, (kfi, ki, val) in sorted(kept.items(), key=lambda kv: kv[1][0]):
        print(f"Keeping {name} = 0x{val:X} as defined in {paths[kfi]} (symbol index {ki})")

    changed_total = 0
    for fi, elf in enumerate(elfs):
        file_changed = 0
        for i, off, name, val, size, info, shndx in elf.iter_symbols():
            if name in dup_names and shndx != SHN_UNDEF:
                if (fi, i) != (kept[name][0], kept[name][1]):
                    elf.undefine_symbol(off)
                    file_changed += 1
        if file_changed:
            elf.save()
            print(f"{paths[fi]}: undefined {file_changed} duplicate occurrence(s)")
            changed_total += file_changed

    print(f"Total: {changed_total} duplicate occurrences converted to external references")


if __name__ == "__main__":
    main(sys.argv[1:])
