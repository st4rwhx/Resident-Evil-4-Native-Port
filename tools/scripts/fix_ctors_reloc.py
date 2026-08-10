#!/usr/bin/env python3
"""Redirects the two _ctors@ha/_ctors@l relocations in __init_cpp (within
auto_01_800771E4_text_000.o) away from the undefined external symbol
'_ctors' (which mwldeppc always resolves to its own internally-synthesized,
near-empty linker-generated ctors table) to a new undefined-external symbol
named 'gap_02_8026D9D8_rodata' - the real name of the correctly-extracted
data at the real original address (0x8026D9D8, verified against the retail
DOL bytes: an empty/null-terminated ctors table living in normal rodata) -
which is GLOBALLY DEFINED in auto_02_8021C960_rodata_merged.o and will
resolve correctly at link time.

Does this by appending a new symtab+strtab (old content + one new entry)
at the end of the file and repointing the section headers + the two
relocation entries, leaving old symtab/strtab bytes as harmless padding.
"""
import struct
import sys

TARGET_PATH = "build/RE4DBG/obj/obj/auto_01_800771E4_text_000.o"
NEW_SYM_NAME = "gap_02_8026D9D8_rodata"
RELOC_OFFSETS = {0x2CB6, 0x2CBA}  # section-relative offsets of the two _ctors relocations


def main():
    with open(TARGET_PATH, "rb") as f:
        data = bytearray(f.read())

    endian = ">" if data[5] == 2 else "<"
    e_shoff = struct.unpack_from(endian + "I", data, 0x20)[0]
    e_shentsize = struct.unpack_from(endian + "H", data, 0x2E)[0]
    e_shnum = struct.unpack_from(endian + "H", data, 0x30)[0]

    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        vals = list(struct.unpack_from(endian + "10I", data, off))
        sections.append(vals)  # [name, type, flags, addr, offset, size, link, info, addralign, entsize]

    RELA_IDX, SYMTAB_IDX, STRTAB_IDX = 2, 3, 4
    rela = sections[RELA_IDX]
    symtab = sections[SYMTAB_IDX]
    strtab = sections[STRTAB_IDX]

    old_strtab_bytes = bytes(data[strtab[4]:strtab[4] + strtab[5]])
    old_symtab_bytes = bytes(data[symtab[4]:symtab[4] + symtab[5]])
    sym_entsize = symtab[9] or 16
    old_sym_count = len(old_symtab_bytes) // sym_entsize
    new_sym_index = old_sym_count

    new_name_offset = len(old_strtab_bytes)
    new_strtab_bytes = old_strtab_bytes + NEW_SYM_NAME.encode("utf-8") + b"\0"

    # STB_GLOBAL(1)<<4 | STT_NOTYPE(0), st_other=0, st_shndx=SHN_UNDEF(0)
    new_sym_entry = struct.pack(endian + "3I2BH", new_name_offset, 0, 0, (1 << 4) | 0, 0, 0)
    new_symtab_bytes = old_symtab_bytes + new_sym_entry

    # Append new strtab then new symtab at end of file
    strtab_new_offset = len(data)
    data.extend(new_strtab_bytes)
    symtab_new_offset = len(data)
    data.extend(new_symtab_bytes)

    strtab[4] = strtab_new_offset
    strtab[5] = len(new_strtab_bytes)
    symtab[4] = symtab_new_offset
    symtab[5] = len(new_symtab_bytes)

    for i, vals in enumerate([strtab, symtab] if False else []):
        pass  # no-op, placeholder to keep structure clear

    def write_section_header(index, vals):
        off = e_shoff + index * e_shentsize
        struct.pack_into(endian + "10I", data, off, *vals)

    write_section_header(STRTAB_IDX, strtab)
    write_section_header(SYMTAB_IDX, symtab)

    # Redirect the two relocations
    rela_entsize = rela[9] or 12
    rela_count = rela[5] // rela_entsize
    changed = 0
    for i in range(rela_count):
        off = rela[4] + i * rela_entsize
        r_offset, r_info, r_addend = struct.unpack_from(endian + "3I", data, off)
        if r_offset in RELOC_OFFSETS:
            r_type = r_info & 0xFF
            new_info = (new_sym_index << 8) | r_type
            struct.pack_into(endian + "I", data, off + 4, new_info)
            changed += 1
            print(f"Redirected relocation at r_offset=0x{r_offset:X} to symbol idx {new_sym_index} ({NEW_SYM_NAME})")

    if changed != len(RELOC_OFFSETS):
        print(f"WARNING: expected to change {len(RELOC_OFFSETS)} relocations, changed {changed}")
        sys.exit(1)

    with open(TARGET_PATH, "wb") as f:
        f.write(data)
    print(f"Saved {TARGET_PATH} (new symbol idx {new_sym_index}, name offset {new_name_offset})")


if __name__ == "__main__":
    main()
