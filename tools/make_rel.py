#!/usr/bin/env python3
"""Write a REL from a module ELF linked with `ngcld -r`, the way SN's snmakerel did for the original.

usage: make_rel.py --config config/<ver>/modules/<mod>/rel.json --dol-symbols config/<ver>/symbols.txt
                   --out build/<ver>/<mod>/<mod>.rel [--link <name>=<elf> ...] [--verify <orig.rel>]
                   build/<ver>/<mod>/<mod>.elf

rel.json (tools/gen_rel_config.py) carries the header constants of the original REL: module id,
section count of the original ELF, name offset/size (into makerel's string table), align, bss_align,
the REL section indices and the size of ngcld's BSS_TAG object at the end of .bss.

Relocations: every RELA entry of .text/.ctors/.dtors/.rodata/.data becomes a REL relocation.
Undefined symbols resolve against the DOL's symbols.txt (module 0, section byte = the original DOL
ELF's section index, addend = absolute address) or against the ELFs of the modules named in rel.json
"links".
snmakerel's rules for what ends up in the file:
  * R_PPC_REL24 to a target in the same section is resolved and dropped from the table; a REL24 to
    another module or the DOL is patched to branch to _unresolved and kept,
  * R_PPC_REL14 (NgcAs emits one for every conditional branch) is resolved and kept,
  * ADDR32/ADDR16_LO/ADDR16_HA fields hold S+A for local symbols (what ngcld -r wrote) and A for
    globals (ngcld 3.9.3 -r also adds the defining input section's displacement there; the original
    linker did not, so the field is rewritten),
  * the relocation lists are ordered: imported modules ascending, then self, then module 0
    (fix_size marks where the self list starts), each list by section then offset.
"""
import argparse
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import elffile  # noqa: E402
import relfile  # noqa: E402

# ELF section indices of the original main.elf (from the section bytes of the module-0 relocations),
# keyed by the DOL config's section names.
DOL_SECTION_INDEX = {'.init': 1, '.text': 2, '.ctor': 3, '.dtor': 4, '.rodata': 5, '.data': 6, '.bss': 7,
                     '.sdata': 8, '.sbss': 9, '.sdata2': 10, '.sbss2': 11}
SYMBOL_RE = re.compile(r'^(\S+) = (\.\w+):0x([0-9A-Fa-f]+);')


def read_dol_symbols(path):
    """config/<ver>/symbols.txt -> name -> (section name, address). The split objects of the modules
    reference DOL symbols by these names; main.elf's symbol table would only have the names the
    compiled DOL units happen to use."""
    out = {}
    for line in open(path):
        m = SYMBOL_RE.match(line)
        if m:
            out.setdefault(m.group(1), (m.group(2), int(m.group(3), 16)))
    return out
KINDS = (relfile.R_PPC_ADDR32, relfile.R_PPC_ADDR16_LO, relfile.R_PPC_ADDR16_HA,
         relfile.R_PPC_REL24, relfile.R_PPC_REL14)


def die(msg):
    sys.exit(f'make_rel: {msg}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--config', required=True)
    ap.add_argument('--dol-symbols', required=True, metavar='SYMBOLS_TXT')
    ap.add_argument('--out', required=True)
    ap.add_argument('--link', action='append', default=[], metavar='NAME=ELF')
    ap.add_argument('--verify', metavar='ORIG_REL')
    ap.add_argument('elf')
    args = ap.parse_args()

    cfg = json.load(open(args.config))
    modules_dir = os.path.dirname(os.path.dirname(os.path.abspath(args.config)))
    elf = elffile.Elf(args.elf)
    mod_id = cfg['module_id']
    sec_index = cfg['sections']  # name -> REL index

    # --- external symbol tables ------------------------------------------------------------------
    dol_syms = read_dol_symbols(args.dol_symbols)
    links = {}
    for spec in args.link:
        name, path = spec.split('=', 1)
        lcfg = json.load(open(os.path.join(modules_dir, name, 'rel.json')))
        lelf = elffile.Elf(path)
        links[name] = (lcfg, lelf, lelf.defined_globals())
    for name in cfg['links']:
        if name not in links:
            die(f'{cfg["name"]} imports module {name}: pass --link {name}=<elf>')

    def resolve_external(name):
        # snmakerel took the first definition in its input order: the DOL, then the modules by
        # ascending module id (t_camera's __builtin_delete goes to the DOL although Tools and t_esp
        # have their own; st1_0's R102Init goes to st1_1 (73), not st1_3 (86)).
        if name in dol_syms:
            secname, addr = dol_syms[name]
            if secname not in DOL_SECTION_INDEX:
                die(f'{name}: DOL section {secname} has no known original section index')
            return 0, DOL_SECTION_INDEX[secname], addr
        for lname, (lcfg, lelf, lsyms) in sorted(links.items(), key=lambda kv: kv[1][0]['module_id']):
            if name in lsyms:
                s = lsyms[name]
                secname = lelf.sections[s.shndx].name
                return lcfg['module_id'], lcfg['sections'][secname], s.value
        die(f'undefined symbol {name}')

    # --- sections ---------------------------------------------------------------------------------
    data = {}
    for name in sec_index:
        s = elf.section(name)
        if s is None:
            if name == '.bss':
                continue
            die(f'ELF has no {name} section')
        data[name] = bytearray(s.data)
    bss = elf.section('.bss')
    bss_size = bss.size if bss else 0
    # COMMON symbols: ngcld -r leaves them unallocated; snmakerel appended them to .bss (after ngcld's
    # zero-size __sn__bss__tag__), in symbol table order.
    common_alloc = {}
    for s in elf.symbols:
        if s.shndx == elffile.SHN_COMMON:
            al = max(s.value, 1)
            bss_size = (bss_size + al - 1) & ~(al - 1)
            common_alloc[s.index] = bss_size
            bss_size += s.size
    if bss_size - (bss.size if bss else 0) != cfg['common_size']:
        die(f'COMMON symbols take {bss_size - (bss.size if bss else 0):#x} bytes, the original had {cfg["common_size"]:#x}')

    def sym_of(name):
        for s in elf.symbols:
            if s.name == name and s.shndx not in (elffile.SHN_UNDEF, elffile.SHN_COMMON):
                return s
        die(f'{name} not defined in the module')

    unresolved = sym_of('_unresolved')
    unresolved_sec = elf.sections[unresolved.shndx].name

    # --- relocations ------------------------------------------------------------------------------
    relocs = []  # (module, src_sec_idx, offset, kind, target_sec_idx, addend)
    for name in sec_index:
        s = elf.section(name)
        if s is None:
            continue
        src_idx = sec_index[name]
        buf = data[name]
        for r in elf.relas.get(s.index, []):
            if r.type not in KINDS:
                die(f'{name}+{r.offset:#x}: unsupported relocation type {r.type}')
            sym = elf.symbols[r.sym]
            if sym.shndx == elffile.SHN_UNDEF:
                tmod, tsec, value = resolve_external(sym.name)
                addend = value + r.addend
                same_section = False
            elif sym.shndx == elffile.SHN_COMMON:
                tmod, tsec = mod_id, sec_index['.bss']
                addend = common_alloc[sym.index] + r.addend
                same_section = False
            elif sym.shndx == elffile.SHN_ABS:
                die(f'{name}+{r.offset:#x}: relocation against absolute symbol {sym.name}')
            else:
                tname = elf.sections[sym.shndx].name
                if tname not in sec_index:
                    die(f'{name}+{r.offset:#x}: relocation against {sym.name} in section {tname}')
                tmod, tsec = mod_id, sec_index[tname]
                addend = sym.value + r.addend
                same_section = tname == name
            addend &= 0xFFFFFFFF
            if r.type == relfile.R_PPC_REL24:
                if same_section:
                    disp = addend - r.offset
                else:
                    if name != unresolved_sec:
                        die(f'{name}+{r.offset:#x}: external branch outside the _unresolved section')
                    disp = unresolved.value - r.offset
                w = struct.unpack('>I', buf[r.offset:r.offset + 4])[0]
                buf[r.offset:r.offset + 4] = struct.pack('>I', (w & ~0x3FFFFFC) | (disp & 0x3FFFFFC))
                if same_section:
                    continue
            elif r.type == relfile.R_PPC_REL14:
                if not same_section:
                    die(f'{name}+{r.offset:#x}: R_PPC_REL14 to another section/module')
                disp = addend - r.offset
                w = struct.unpack('>I', buf[r.offset:r.offset + 4])[0]
                buf[r.offset:r.offset + 4] = struct.pack('>I', (w & ~0xFFFC) | (disp & 0xFFFC))
            elif sym.name == '__sn__bss__tag__':
                # ngcld defines its tag as a local symbol (field = S+A); the original has 0 there
                buf[r.offset:r.offset + 4] = b'\0\0\0\0'
            elif sym.bind != elffile.STB_LOCAL:
                # the original fields hold only A for global symbols; ngcld 3.9.3 -r also adds the
                # displacement of the input section defining the symbol (0 for the first object,
                # so this only shows in multi-object modules: em10's _prolog -> Em10Init)
                a = r.addend & 0xFFFFFFFF
                if r.type == relfile.R_PPC_ADDR32:
                    buf[r.offset:r.offset + 4] = struct.pack('>I', a)
                else:
                    v = a & 0xFFFF if r.type == relfile.R_PPC_ADDR16_LO else ((a + 0x8000) >> 16) & 0xFFFF
                    buf[r.offset:r.offset + 2] = struct.pack('>H', v)
            relocs.append((tmod, src_idx, r.offset, r.type, tsec, addend))

    for ov in cfg.get('field_overrides', []):
        width = 4 if ov['kind'] == relfile.R_PPC_ADDR32 else 2
        buf = data[ov['section']]
        buf[ov['offset']:ov['offset'] + width] = ov['value'].to_bytes(width, 'big')

    def order(rl):
        m = rl[0]
        rank = 2 if m == 0 else 1 if m == mod_id else 0
        return (rank, m if rank == 0 else 0, rl[1], rl[2])

    relocs.sort(key=order)

    # --- layout -------------------------------------------------------------------------------------
    num_sections = cfg['num_sections']
    section_table = 0x4C
    offset = section_table + 8 * num_sections
    layout = {}  # name -> file offset
    for name, idx in sorted(sec_index.items(), key=lambda kv: kv[1]):
        if name == '.bss' or name not in data:
            continue
        al = max(elf.section(name).align, 1)
        offset = (offset + al - 1) & ~(al - 1)
        layout[name] = offset
        offset += len(data[name])
    offset = (offset + 3) & ~3
    imp_modules = []
    for rl in relocs:
        if not imp_modules or imp_modules[-1] != rl[0]:
            imp_modules.append(rl[0])
    imp_offset = offset
    imp_size = 8 * len(imp_modules)
    rel_offset = imp_offset + imp_size

    rel_bytes = bytearray()
    imp_table = []
    fix_size = None
    cur_mod = None
    cur_sec = None
    addr = 0
    for rl in relocs + [None]:
        if rl is None or rl[0] != cur_mod:
            if cur_mod is not None:
                rel_bytes += struct.pack('>HBBI', 0, relfile.R_DOLPHIN_END, 0, 0)
            if rl is None:
                break
            cur_mod = rl[0]
            imp_table.append((cur_mod, rel_offset + len(rel_bytes)))
            if fix_size is None and cur_mod in (0, mod_id):
                fix_size = rel_offset + len(rel_bytes)
            cur_sec = None
        tmod, src, off, kind, tsec, addend = rl
        if src != cur_sec:
            rel_bytes += struct.pack('>HBBI', 0, relfile.R_DOLPHIN_SECTION, src, 0)
            cur_sec = src
            addr = 0
        delta = off - addr
        while delta > 0xFFFF:
            rel_bytes += struct.pack('>HBBI', 0xFFFF, relfile.R_DOLPHIN_NOP, 0, 0)
            delta -= 0xFFFF
        rel_bytes += struct.pack('>HBBI', delta, kind, tsec, addend)
        addr = off
    if fix_size is None:
        fix_size = rel_offset + len(rel_bytes)

    # --- write ----------------------------------------------------------------------------------------
    prolog, epilog = sym_of('_prolog'), sym_of('_epilog')
    out = bytearray()
    out += struct.pack('>12I', mod_id, 0, 0, num_sections, section_table, cfg['name_offset'], cfg['name_size'],
                       3, bss_size, rel_offset, imp_offset, imp_size)
    out += bytes([sec_index[elf.sections[prolog.shndx].name], sec_index[elf.sections[epilog.shndx].name],
                  sec_index[unresolved_sec], 0])
    out += struct.pack('>6I', prolog.value, epilog.value, unresolved.value, cfg['align'], cfg['bss_align'], fix_size)
    assert len(out) == section_table
    for i in range(num_sections):
        name = next((n for n, idx in sec_index.items() if idx == i), None)
        if name is None:
            out += struct.pack('>2I', 0, 0)
        elif name == '.bss':
            out += struct.pack('>2I', 0, bss_size)
        else:
            out += struct.pack('>2I', layout[name] | (1 if name == '.text' else 0), len(data[name]))
    for name, off in sorted(layout.items(), key=lambda kv: kv[1]):
        out += b'\0' * (off - len(out))
        out += data[name]
    out += b'\0' * (imp_offset - len(out))
    for m, o in imp_table:
        out += struct.pack('>2I', m, o)
    assert len(out) == rel_offset
    out += rel_bytes

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, 'wb') as f:
        f.write(out)

    if args.verify:
        orig = open(args.verify, 'rb').read()
        if orig == bytes(out):
            print(f'{args.out}: OK ({len(out)} bytes)')
            return
        regions = [('header', 0), ('section table', section_table)]
        regions += [(n, o) for n, o in sorted(layout.items(), key=lambda kv: kv[1])]
        regions += [('imp table', imp_offset), ('relocations', rel_offset)]
        diffs = [i for i in range(min(len(orig), len(out))) if orig[i] != out[i]]
        print(f'{args.out}: MISMATCH size {len(out):#x} vs {len(orig):#x}, {len(diffs)} differing bytes')
        for i in diffs[:16]:
            region = [n for n, o in regions if o <= i][-1]
            print(f'  {i:#8x} ({region}+{i - dict(regions)[region]:#x}): ours {out[i]:02x} orig {orig[i]:02x}')
        sys.exit(1)


if __name__ == '__main__':
    main()
