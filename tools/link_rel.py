#!/usr/bin/env python3
"""Link a REL module's objects into a relocatable ELF with ProDG's ngcld -r.

usage: link_rel.py <linker> <wrapper> <sn_ngc_path> <ldscript> <out.elf> <rsp>

Split objects (anything under a .../obj/ directory) are first copied next to the output with their
R_PPC_REL14 relocations rewritten to be section-relative: dtk emits them against the containing
function symbol, and ngcld -r patches a relocation against a global symbol with A - P (it only adds
S for local symbols), which is out of the 16-bit range for anything but tiny modules. NgcAs expresses
every conditional branch as `.text + offset`, so the rewrite also makes the split objects look like
assembler output. Compiled objects are linked as they are.

After the link, relocations against absolute symbols (the linker script's GXWGFifo) are applied and
their RELA entries removed: ngcld -r keeps them unresolved, the original RELs carry the final
`lis 0xCC01` / `-0x8000` words and no relocation there (snmakerel resolved them). The other field
difference to the original link (ngcld adds the input-section displacement of a global symbol to
ADDR16/32 fields, the original left the plain addend) is handled by tools/make_rel.py.
"""
import os
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
import elffile  # noqa: E402


def fix_split_object(src: str, dst: str) -> None:
    elf = elffile.Elf(src)
    data = bytearray(open(src, 'rb').read())
    section_sym = {s.shndx: s.index for s in elf.symbols if s.type == elffile.STT_SECTION}
    for rela_sec in elf.sections:
        if rela_sec.type != elffile.SHT_RELA:
            continue
        for i in range(rela_sec.size // 12):
            pos = rela_sec.offset + 12 * i
            off, info, addend = struct.unpack('>IIi', data[pos:pos + 12])
            if info & 0xFF != 11:  # R_PPC_REL14
                continue
            sym = elf.symbols[info >> 8]
            if sym.type == elffile.STT_SECTION or sym.shndx != rela_sec.info:
                continue
            sec_sym = section_sym[sym.shndx]
            data[pos:pos + 12] = struct.pack('>IIi', off, (sec_sym << 8) | 11, sym.value + addend)
    with open(dst, 'wb') as f:
        f.write(data)


R_PPC_ADDR32, R_PPC_ADDR16, R_PPC_ADDR16_LO, R_PPC_ADDR16_HI, R_PPC_ADDR16_HA = 1, 3, 4, 5, 6


def field_bytes(kind: int, value: int) -> bytes:
    if kind == R_PPC_ADDR32:
        return struct.pack('>I', value & 0xFFFFFFFF)
    if kind == R_PPC_ADDR16_HA:
        return struct.pack('>H', ((value + 0x8000) >> 16) & 0xFFFF)
    if kind == R_PPC_ADDR16_HI:
        return struct.pack('>H', (value >> 16) & 0xFFFF)
    return struct.pack('>H', value & 0xFFFF)


def resolve_absolute_relocs(path: str) -> int:
    """Apply and drop every ADDR32/ADDR16* relocation against an SHN_ABS symbol (the script's GXWGFifo).
    Returns the number of relocations resolved."""
    elf = elffile.Elf(path)
    data = bytearray(open(path, 'rb').read())
    changed = 0
    for rela_sec in elf.sections:
        if rela_sec.type != elffile.SHT_RELA:
            continue
        target = elf.sections[rela_sec.info]
        if not (target.flags & 2) or target.type == elffile.SHT_NOBITS:  # SHF_ALLOC
            continue
        kept = bytearray()
        dropped = 0
        for i in range(rela_sec.size // 12):
            entry = rela_sec.data[12 * i:12 * i + 12]
            off, info, addend = struct.unpack('>IIi', entry)
            kind = info & 0xFF
            sym = elf.symbols[info >> 8]
            if kind in (R_PPC_ADDR32, R_PPC_ADDR16, R_PPC_ADDR16_LO, R_PPC_ADDR16_HI, R_PPC_ADDR16_HA) and sym.shndx == elffile.SHN_ABS:
                pos = target.offset + off
                want = field_bytes(kind, sym.value + addend)
                data[pos:pos + len(want)] = want
                changed += 1
                dropped += 1
                continue
            kept += entry
        if dropped:
            # shrink the RELA section in place (the tail bytes stay unused in the file)
            data[rela_sec.offset:rela_sec.offset + len(kept)] = kept
            shoff = struct.unpack('>I', data[0x20:0x24])[0]
            shentsize = struct.unpack('>H', data[0x2E:0x30])[0]
            hdr = shoff + rela_sec.index * shentsize
            struct.pack_into('>I', data, hdr + 20, len(kept))
    if changed:
        with open(path, 'wb') as f:
            f.write(data)
    return changed


def main() -> int:
    if len(sys.argv) != 7:
        print(__doc__, file=sys.stderr)
        return 2
    linker, wrapper, sn_ngc_path, ldscript, out_path, rsp_path = sys.argv[1:]
    with open(rsp_path, 'r', encoding='utf-8') as f:
        inputs = [line.strip() for line in f if line.strip()]

    fix_dir = os.path.join(os.path.dirname(out_path), 'objfix')
    linked = []
    for obj in inputs:
        parts = os.path.normpath(obj).split(os.sep)
        if 'obj' in parts:
            fixed = os.path.join(fix_dir, *parts[parts.index('obj') + 1:])
            os.makedirs(os.path.dirname(fixed), exist_ok=True)
            fix_split_object(obj, fixed)
            linked.append(fixed)
        else:
            linked.append(obj)

    cmd = [wrapper] if wrapper else []
    cmd += [linker, '-r', '-T', ldscript, '-o', out_path, *linked]
    env = os.environ.copy()
    env['SN_NGC_PATH'] = sn_ngc_path
    res = subprocess.run(cmd, env=env, check=False, capture_output=True, text=True)
    # ngcld prints its banner on every run; keep only diagnostics
    for stream in (res.stdout, res.stderr):
        for line in stream.splitlines():
            if line.startswith('ngcld ') and ' @ ' in line:
                continue
            print(line, file=sys.stderr)
    if res.returncode != 0 and os.path.exists(out_path):
        os.remove(out_path)
    if res.returncode == 0:
        resolve_absolute_relocs(out_path)
    return res.returncode


if __name__ == '__main__':
    raise SystemExit(main())
