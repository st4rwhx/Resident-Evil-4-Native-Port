#!/usr/bin/env python3
"""Remove unreferenced symbols from a compiled Dolphin SDK object the way the game's linker did.

The game was linked with SN ProDG's linker, which dead-strips SDK library objects at symbol
granularity while keeping the remaining symbols in their original order:
  * functions (static or global) nobody references are dropped from .text entirely;
  * global data objects nobody references are removed in 8-byte units (the last size % 8 bytes
    stay, unnamed) and lose their relocations (observed on GXFrameBuf's GXRenderModeObj table,
    where every unreferenced 60-byte object left its last 4 bytes; on dvdFatal's 24-byte Europe[]
    which vanished; and on dvdFatal's Japanese/English pointers which became zeros);
  * local data is kept whole when anything references it or its section is addressed through a
    -O4,p pool symbol (...data.0/...bss.0); local data nobody references at all is stripped like
    global data (axartlfo's unused wave tables).
The DOL therefore contains a subset of each SDK object. This tool reproduces that: it deletes
every symbol that is not listed for the unit in config/<ver>/sym_map.tsv, fixing up section
contents, relocations, symbols and the Metrowerks .comment section.

usage: strip_unused.py --unit lib/OS.c build/G4BE08/src/lib/OS.o
       strip_unused.py --gcc --unit lib/_eh.c build/G4BE08/src/lib/_eh.o   (ProDG objects)
       strip_unused.py --gcc --module st1_1 --unit st1_1/em_wrap.cpp build/G4BE08/src/st1_1/em_wrap.o
           (REL module unit: keep what config/<ver>/modules/<mod>/sym_map.tsv lists for the unit;
           modules.py STRIP_UNUSED names the units, see docs/matching.md "REL modules")
"""
import argparse
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VER = os.environ.get("RE4_VERSION", "G4BE08")

SHT_SYMTAB = 2
SHT_RELA = 4
SHT_NOBITS = 8
STT_OBJECT = 1
STT_FUNC = 2
STT_SECTION = 3
STB_LOCAL = 0
DATA_GRANULE = 8  # the linker strips global data in 8-byte units (the remainder stays)


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sync_symbols import demangle_v2  # noqa: E402


def target_symbols(unit):
    """{section: set(names)} of symbols kept in the DOL for this unit (dtk _ADDR suffixes removed)."""
    names = {}
    seen = False
    path = os.path.join(ROOT, "config", VER, "sym_map.tsv")
    with open(path) as f:
        next(f)
        for line in f:
            addr, size, sec, u, scope, name, dn = line.rstrip("\n").split("\t")
            if u != unit:
                continue
            seen = True
            s = names.setdefault(sec, set())
            s.add(name)
            s.add(re.sub(r"_[0-9A-F]{8}$", "", name))
            # gcc 2.95 function-local statics carry a DECL_UID suffix (`pl_move_func_tbl.1272`) that a
            # recompile cannot reproduce: match them by the base name (player.cpp's unreferenced table)
            if re.search(r"\.\d+$", name):
                s.add(re.sub(r"\.\d+$", ".*", name))
            if dn and dn != ".":
                s.add(dn)
                # gcc 2.95 names them after the first global of the unit (`_GLOBAL_.I.<key>`); the
                # key need not be reproduced, the thunk is a local symbol
                if dn.startswith("global constructors keyed to"):
                    s.add("_GLOBAL_.I.*")
                if dn.startswith("global destructors keyed to"):
                    s.add("_GLOBAL_.D.*")
                # gcc 2.95 vtable symbol for "Class virtual table" (every unit carries weak copies)
                m = re.match(r"^(\w+) virtual table$", dn)
                if m:
                    s.add(f"_vt.{len(m.group(1))}{m.group(1)}")
    return names if seen else None


def fully_named_demangled(unit):
    """{section: set(demangled)} of the demangled names whose every sym_map row of this unit already
    carries a real mangled name. A compiled function that only matches such a name through demangling
    is another overload the DOL does not have (cam_ctrl owns cManager<cLight>::create(int) and
    create(), the fold appends the unreferenced create(int, u32) too; the original linker dropped it)."""
    rows = {}
    path = os.path.join(ROOT, "config", VER, "sym_map.tsv")
    with open(path) as f:
        next(f)
        for line in f:
            addr, size, sec, u, scope, name, dn = line.rstrip("\n").split("\t")
            if u != unit or not dn or dn == ".":
                continue
            named = "__" in name and (demangle_v2(name) or name) == dn
            rows.setdefault(sec, {}).setdefault(dn, []).append(named)
    return {sec: {dn for dn, flags in d.items() if all(flags)} for sec, d in rows.items()}


def module_target_symbols(module, unit):
    """{section: set(names)} of the symbols the module's REL keeps for this unit, from
    config/<ver>/modules/<mod>/sym_map.tsv (section, offset, size, unit, scope, name, demangled). The
    stage modules' em_wrap.cpp/cSceObj.cpp were dead-stripped at function level by the original REL
    link like the DOL's SDK objects. Module demangled names carry no parameter list, so an overload is
    told apart by size: `<demangled>#<size>` entries are added and checked first."""
    names = {}
    seen = False
    path = os.path.join(ROOT, "config", VER, "modules", module, "sym_map.tsv")
    with open(path) as f:
        next(f)
        for line in f:
            sec, off, size, u, scope, name, dn = line.rstrip("\n").split("\t")
            if u != unit:
                continue
            seen = True
            s = names.setdefault(sec, set())
            s.add(name)
            if dn and dn != ".":
                s.add(dn)
                s.add(f"{dn}#{int(size, 16):#x}")
                # the `_GLOBAL_.I.<key>` thunk: the key need not be reproduced (see target_symbols)
                if dn.startswith("global constructors keyed to"):
                    s.add("_GLOBAL_.I.*")
                if dn.startswith("global destructors keyed to"):
                    s.add("_GLOBAL_.D.*")
    # a unit with no row at all lost every function (st1_0's em_wrap.cpp: strings only)
    return names


class Elf:
    def __init__(self, data):
        self.data = bytearray(data)
        (self.shoff,) = struct.unpack(">I", data[0x20:0x24])
        (self.shentsize, self.shnum, self.shstrndx) = struct.unpack(">HHH", data[0x2E:0x34])
        self.sections = []
        for i in range(self.shnum):
            off = self.shoff + i * self.shentsize
            self.sections.append(list(struct.unpack(">IIIIIIIIII", data[off : off + 40])))
        shstr = self.sec_bytes(self.shstrndx)
        self.names = []
        for sh in self.sections:
            end = shstr.index(b"\0", sh[0])
            self.names.append(shstr[sh[0] : end].decode())
        self.contents = [bytearray(self.sec_bytes(i)) for i in range(self.shnum)]

    def sec_bytes(self, i):
        sh = self.sections[i]
        if sh[1] == SHT_NOBITS:
            return b"\0" * sh[5]
        return self.data[sh[4] : sh[4] + sh[5]]

    def index(self, name):
        return self.names.index(name)

    def write(self, path):
        out = bytearray(self.data[:0x34])
        pos = 0x34
        for i in range(1, self.shnum):
            sh = self.sections[i]
            body = self.contents[i]
            align = max(sh[8], 1)
            pos = (pos + align - 1) // align * align
            sh[4] = pos
            sh[5] = len(body)
            if sh[1] != SHT_NOBITS:
                out += b"\0" * (pos - len(out))
                out += body
                pos += len(body)
        pos = (pos + 3) // 4 * 4
        out += b"\0" * (pos - len(out))
        struct.pack_into(">I", out, 0x20, pos)
        for sh in self.sections:
            out += struct.pack(">IIIIIIIIII", *sh)
        with open(path, "wb") as f:
            f.write(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", required=True)
    ap.add_argument(
        "--gcc",
        action="store_true",
        help="ProDG (gcc 2.95) object: .lcomm statics are untyped symbols, data only referenced from "
        "stripped functions is stripped too, and surviving pointers to stripped functions become 0",
    )
    ap.add_argument("--module", help="REL module: keep the symbols of config/<ver>/modules/<mod>/sym_map.tsv for the unit")
    ap.add_argument("object")
    args = ap.parse_args()

    keep = module_target_symbols(args.module, args.unit) if args.module else target_symbols(args.unit)
    if keep is None:
        sys.exit(f"strip_unused: unit {args.unit} not in sym_map.tsv")
    fully_named = fully_named_demangled(args.unit) if args.gcc and not args.module else {}

    def module_kept(nm, size, ks):
        """--module: a function is kept when the module names it (mangled, or demangled base name; an
        overloaded base name only together with the size)."""
        if not args.module:
            return False
        dn = demangle_v2(nm) or nm
        if nm in ks or f"{dn}#{size:#x}" in ks:
            return True
        if (nm.startswith("_GLOBAL_.I.") and "_GLOBAL_.I.*" in ks) or (nm.startswith("_GLOBAL_.D.") and "_GLOBAL_.D.*" in ks):
            return True
        if dn not in ks:
            return False
        # same base name, other size: another overload once the module's row carries a mangled name
        # (setPtr(cEm*, int) vs setPtr(s16, s8, int)); a placeholder row is still ours (unit in progress)
        return not any(k != nm and "__" in k and (demangle_v2(k) or k) == dn for k in ks)

    elf = Elf(open(args.object, "rb").read())
    symtab = elf.index(".symtab")
    strtab = elf.sections[symtab][6]
    syms = [
        list(struct.unpack(">IIIBBH", elf.contents[symtab][o : o + 16]))
        for o in range(0, len(elf.contents[symtab]), 16)
    ]
    strs = elf.contents[strtab]

    def sym_name(s):
        end = strs.index(b"\0", s[0])
        return strs[s[0] : end].decode()

    # Reference counts per symbol and "pooled" sections: with -O4,p MWCC addresses local data through
    # a per-section pool symbol (...data.0 / ...bss.0, NOTYPE at offset 0), so the linker could not
    # attribute those references to individual local objects and kept every local in such a section.
    # Local data with no reference at all (e.g. axartlfo's unused wave tables) was stripped.
    # References from functions that are themselves stripped do not count (gcc's libgcc statics
    # that only the dropped exception runtime used are gone from the DOL).
    dead_funcs = {}
    for s in syms:
        shndx, stype = s[5], s[3] & 0xF
        if shndx == 0 or shndx >= elf.shnum or stype != STT_FUNC:
            continue
        nm = sym_name(s)
        ks = keep.get(elf.names[shndx], ())
        if args.module:
            if elf.names[shndx].startswith(".gnu.linkonce"):
                # ngcld -r kept every object's linkonce copies whether the module names them or not
                # (the nameless cManager<cLight> blocks); fold_linkonce.py --module decides their fate
                continue
            if not module_kept(nm, s[2], ks):
                dead_funcs.setdefault(shndx, []).append((s[1], s[1] + s[2]))
            continue
        if args.gcc and (demangle_v2(nm) or nm) in ks:
            # a demangled-only match against a name whose rows are all mangled already is another
            # overload (a linkonce copy the fold kept because it shares the base name): strip it
            if nm in ks or (demangle_v2(nm) or nm) not in fully_named.get(elf.names[shndx], ()):
                continue
        if nm not in ks and not (nm.startswith("_GLOBAL_.I.") and "_GLOBAL_.I.*" in ks) and not (nm.startswith("_GLOBAL_.D.") and "_GLOBAL_.D.*" in ks):
            dead_funcs.setdefault(shndx, []).append((s[1], s[1] + s[2]))

    def in_dead_func(shndx, off):
        return args.gcc and any(start <= off < end for start, end in dead_funcs.get(shndx, ()))

    if args.module:
        # --module: a function that surviving code still references (NgcAs spells intra-object calls as
        # `.text+off`) cannot have been stripped by the linker; while a unit is being matched a size
        # mismatch of an overload would otherwise strip a called body. Keep them (iterate to a fixpoint).
        module_keep = set()
        changed = True
        while changed:
            changed = False
            for i, sh in enumerate(elf.sections):
                if sh[1] != SHT_RELA or not (elf.sections[sh[7]][2] & 0x4):
                    continue
                for o in range(0, len(elf.contents[i]), 12):
                    r_off, r_info, r_add = struct.unpack(">IIi", elf.contents[i][o : o + 12])
                    if in_dead_func(sh[7], r_off):
                        continue
                    s = syms[r_info >> 8]
                    if (s[3] & 0xF) == STT_SECTION:
                        tsec, toff = s[5], r_add
                    elif (s[3] & 0xF) == STT_FUNC:
                        tsec, toff = s[5], s[1]
                    else:
                        continue
                    for rng in list(dead_funcs.get(tsec, ())):
                        if rng[0] <= toff < rng[1]:
                            dead_funcs[tsec].remove(rng)
                            module_keep.add((tsec, rng[0]))
                            changed = True
    else:
        module_keep = set()

    refcount = [0] * len(syms)
    pooled_sections = set()
    for i, sh in enumerate(elf.sections):
        if sh[1] != SHT_RELA:
            continue
        for o in range(0, len(elf.contents[i]), 12):
            r_off, r_info = struct.unpack(">II", elf.contents[i][o : o + 8])
            sym = r_info >> 8
            if in_dead_func(sh[7], r_off):
                continue
            refcount[sym] += 1
            s = syms[sym]
            if (s[3] & 0xF) in (STT_SECTION, 0) and s[5] != 0:
                pooled_sections.add(s[5])

    # Byte ranges to delete, per section: {shndx: [(start, end), ...]}; symbols to drop; original
    # extents of dropped symbols (relocations inside them are dropped too, e.g. a stripped pointer
    # variable's initializer ends up as zero in the DOL).
    removed = {}
    dead = {}
    removed_idx = set()
    for i, s in enumerate(syms):
        shndx, stype, bind = s[5], s[3] & 0xF, s[3] >> 4
        if shndx == 0 or shndx >= elf.shnum:
            continue
        if args.gcc and stype == 0 and s[2] and not (elf.sections[shndx][2] & 0x4):
            stype = STT_OBJECT  # gcc .lcomm statics are untyped
        if stype not in (STT_FUNC, STT_OBJECT):
            continue
        secname = elf.names[shndx]
        name = sym_name(s)
        if name in keep.get(secname, ()):
            continue
        if args.module and secname.startswith(".gnu.linkonce"):
            continue  # see above: linkonce copies are fold_linkonce's business
        if args.module and stype == STT_FUNC:
            if module_kept(name, s[2], keep.get(secname, ())) or (shndx, s[1]) in module_keep:
                continue
        elif args.gcc and (demangle_v2(name) or name) in keep.get(secname, ()):
            if not (stype == STT_FUNC and (demangle_v2(name) or name) in fully_named.get(secname, ())):
                continue
        if args.gcc and re.search(r"\.\d+$", name) and re.sub(r"\.\d+$", ".*", name) in keep.get(secname, ()):
            continue
        if name.startswith("_GLOBAL_.I.") and "_GLOBAL_.I.*" in keep.get(secname, ()):
            continue
        if name.startswith("_GLOBAL_.D.") and "_GLOBAL_.D.*" in keep.get(secname, ()):
            continue
        if stype == STT_FUNC:
            removed.setdefault(shndx, []).append((s[1], s[1] + s[2]))
        elif args.module:
            # --module (ngcld -r): only functions were stripped; every data object stays, referenced or
            # not (t_camera's t_util.cpp has no function left but all of its .data/.bss)
            continue
        elif bind != STB_LOCAL or (refcount[i] == 0 and shndx not in pooled_sections):
            # unreferenced data: the linker removed it in 8-byte units, leaving the last
            # size % 8 bytes (unnamed, relocations dropped). Exception: an unreferenced *global* in
            # a gcc object is dropped whole (light.cpp's `const f32 cLightMgr::FarDistance` left no
            # .sdata2 behind), while function-local statics of a dead function keep their
            # remainder (filter01's two 4-byte `.sdata` statics survive in the DOL).
            cut = s[2] if (args.gcc and bind != STB_LOCAL) else s[2] - s[2] % DATA_GRANULE
            if cut:
                removed.setdefault(shndx, []).append((s[1], s[1] + cut))
        else:
            continue
        dead.setdefault(shndx, []).append((s[1], s[1] + s[2]))
        removed_idx.add(i)
    if not removed_idx:
        return
    for r in removed.values():
        r.sort()

    def in_dead(shndx, off):
        return any(start <= off < end for start, end in dead.get(shndx, ()))

    def shift(shndx, off):
        """New offset for an old offset in a section (must not be inside a removed range)."""
        d = 0
        for start, end in removed.get(shndx, ()):
            if start <= off < end:
                raise ValueError(f"offset {off:#x} in {elf.names[shndx]} lies inside a removed range")
            if end <= off:
                d += end - start
        return off - d

    # Section contents
    for shndx, ranges in removed.items():
        old = elf.contents[shndx]
        new = bytearray()
        pos = 0
        for start, end in ranges:
            new += old[pos:start]
            pos = end
        new += old[pos:]
        elf.contents[shndx] = new

    # Symbols: drop removed, shift values, renumber.
    remap = {}
    new_syms = []
    for i, s in enumerate(syms):
        if i in removed_idx:
            continue
        if i != 0 and s[5] in removed and (s[3] & 0xF) != STT_SECTION:
            try:
                s[1] = shift(s[5], s[1])
            except ValueError as e:
                if (s[3] & 0xF) in (STT_FUNC, STT_OBJECT):
                    sys.exit(f"strip_unused: symbol {sym_name(s)}: {e}")
                # untyped label (gcc2_compiled., .LC*) inside a stripped function: keep it at
                # the point where the removed range collapsed to
                d = 0
                for start, end in removed[s[5]]:
                    if end <= s[1]:
                        d += end - start
                    elif start <= s[1]:
                        d += s[1] - start
                s[1] -= d
        remap[i] = len(new_syms)
        new_syms.append(s)
    elf.contents[symtab] = bytearray().join(struct.pack(">IIIBBH", *s) for s in new_syms)
    first_global = next((k for k, s in enumerate(new_syms) if (s[3] >> 4) != 0), len(new_syms))
    elf.sections[symtab][7] = first_global

    # Relocations
    for i, sh in enumerate(elf.sections):
        if sh[1] != SHT_RELA:
            continue
        target = sh[7]
        out = bytearray()
        for o in range(0, len(elf.contents[i]), 12):
            r_off, r_info, r_add = struct.unpack(">IIi", elf.contents[i][o : o + 12])
            sym, rtype = r_info >> 8, r_info & 0xFF
            if in_dead(target, r_off):
                continue  # relocation inside a stripped symbol
            if target in removed:
                r_off = shift(target, r_off)
            if sym in removed_idx:
                if not args.gcc or elf.sections[target][2] & 0x4:  # SHF_EXECINSTR
                    sys.exit(
                        f"strip_unused: {elf.names[target]} still references removed symbol "
                        f"{sym_name(syms[sym])} (unit {args.unit})"
                    )
                # a surviving pointer to a stripped function: the linker left it zero
                elf.contents[target][r_off : r_off + 4] = b"\0\0\0\0"
                continue
            s = syms[sym]
            if (s[3] & 0xF) == STT_SECTION and s[5] in removed:
                try:
                    r_add = shift(s[5], r_add)
                except ValueError as e:
                    sys.exit(f"strip_unused: {elf.names[target]} relocation: {e}")
            out += struct.pack(">IIi", r_off, (remap[sym] << 8) | rtype, r_add)
        elf.contents[i] = out

    # Metrowerks .comment: fixed header followed by one 8-byte record per symbol.
    if ".comment" in elf.names:
        ci = elf.index(".comment")
        body = elf.contents[ci]
        hdr = len(body) - 8 * len(syms)
        if hdr >= 0:
            recs = [body[hdr + 8 * k : hdr + 8 * k + 8] for k in range(len(syms))]
            elf.contents[ci] = bytearray(body[:hdr]) + bytearray().join(
                r for k, r in enumerate(recs) if k not in removed_idx
            )

    elf.write(args.object)


if __name__ == "__main__":
    main()
