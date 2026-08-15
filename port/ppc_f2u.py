# -*- coding: utf-8 -*-
"""Conversions flottant -> entier non signe : comportement du PowerPC sur ARM (15/11).

Le jeu a ete compile pour le PowerPC, ou `(u16) f` passe par fctiwz : conversion SIGNEE puis on garde les bits du bas.
Un flottant negatif fait donc le tour (-1.0 -> 0xFFFF), comme sur x86 (cvttss2si). Sur ARM, clang emet
`vcvt.u32.f32`, qui SATURE : -1.0 -> 0. Le jeu en contient ~1200 (filtres d'ecran, lumieres, ombres, effets, ennemis,
interface) ; un releve Studio (PORT_UBSAN_FLOAT=1) en a vu 5 recevoir des negatifs des la premiere minute (biais de
nettete des textures, tri des objets transparents, couleurs des effets et de l'interface).

Correction : dans les objets du jeu (pas ceux de port/), chaque `vcvt.u32.f32|f64` devient `vcvt.s32.*` - un seul bit
(opc2 100 -> 101 en scalaire, op 11 -> 10 en vectoriel). C'est exactement fctiwz + troncature pour les cibles u8/u16 et
pour les u32 < 2^31. Seul ecart : un u32 >= 2^31 sature a 0x7FFFFFFF (le PowerPC le convertissait juste) ; rare.

    python port/ppc_f2u.py _port_build/game.rsp [objdump]
Idempotent (un objet deja corrige n'a plus de vcvt.u32). Echoue bruyamment si une instruction n'a pas le motif attendu.
"""
import os
import re
import struct
import subprocess
import sys


def sections(data):
    if data[:4] != b'\x7fELF' or data[4] != 1 or data[5] != 1:
        raise ValueError('objet ELF32 petit-boutiste attendu')
    shoff, = struct.unpack_from('<I', data, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 0x2E)
    hdrs = [struct.unpack_from('<IIIIIIIIII', data, shoff + i * shentsize) for i in range(shnum)]
    strtab = hdrs[shstrndx]
    def name(off):
        s = strtab[4] + off
        return data[s:data.index(b'\0', s)].decode()
    out = []
    for i, h in enumerate(hdrs):
        nm, typ, flags, addr, off, size = h[0], h[1], h[2], h[3], h[4], h[5]
        out.append((i, name(nm), typ, flags, off, size))
    return out


def patch_object(path, objdump):
    data = bytearray(open(path, 'rb').read())
    secs = sections(data)
    execs = [s for s in secs if (s[3] & 0x4) and s[2] == 1 and s[5] > 0]   # SHF_EXECINSTR, PROGBITS
    dis = subprocess.run([objdump, '-d', '--no-show-raw-insn', path], capture_output=True, text=True, errors='replace').stdout
    cur = None
    pos = 0
    done = scal = vec = 0
    for line in dis.splitlines():
        m = re.match(r'^Disassembly of section (.+):$', line)
        if m:
            nm = m.group(1)
            while pos < len(execs) and execs[pos][1] != nm:
                pos += 1
            if pos >= len(execs):
                raise RuntimeError('%s : section %s introuvable' % (path, nm))
            cur = execs[pos]
            pos += 1
            continue
        m = re.match(r'^\s*([0-9a-f]+):\s+vcvt\.u32\.f(32|64)\s+(\w+),\s*(\w+)(\s*,\s*#)?', line)
        if not m or cur is None:
            continue
        a = cur[4] + int(m.group(1), 16)
        w = struct.unpack_from('<I', data, a)[0]          # A32 : un mot
        hw1, hw2 = struct.unpack_from('<HH', data, a)     # T32 : deux demi-mots
        vector = m.group(3)[0] in 'qd' and m.group(2) == '32'
        fixed = m.group(5) is not None
        ok = False
        if not vector and not fixed:
            # VFP entier, A32 : cond 1110 1D11 1 opc2 Vd 101 sz op 1 M 0 Vm ; opc2 100 (U32) -> 101 (S32)
            if (w & 0x0FB80E50) == 0x0EB80A40 and ((w >> 16) & 7) == 4:
                w |= 1 << 16; ok = 'a'
            elif (hw1 & 0xFFB8) == 0xEEB8 and (hw1 & 7) == 4 and (hw2 & 0x0E50) == 0x0A40:   # T32
                hw1 |= 1; ok = 't'
        elif not vector and fixed:
            # VFP virgule fixe, A32 : cond 1110 1D11 1 op 1 U Vd 101 sf sx 1 i 0 imm4 ; vers fixe (op=1), U 1 -> 0
            if (w & 0x0FBE0E50) == 0x0EBE0A40 and (w >> 16) & 1:
                w &= ~(1 << 16); ok = 'a'
            elif (hw1 & 0xFFBE) == 0xEEBE and (hw1 & 1) and (hw2 & 0x0E50) == 0x0A40:      # T32
                hw1 &= ~1; ok = 't'
        elif vector and not fixed:
            # Advanced SIMD entier, A32 : 1111 0011 1D11 size 11 Vd 0 11 op Q M 0 Vm ; op 11 (U32) -> 10 (S32)
            if (w & 0xFFB30E10) == 0xF3B30600 and ((w >> 7) & 3) == 3:
                w &= ~0x80; ok = 'a'
            elif (hw1 & 0xFFB3) == 0xFFB3 and (hw2 & 0x0E10) == 0x0600 and ((hw2 >> 7) & 3) == 3:   # T32
                hw2 &= ~0x80; ok = 't'
        else:
            # Advanced SIMD virgule fixe, A32 : 1111 001U 1D imm6 Vd 111 op 0 Q M 1 Vm ; vers fixe (op=1), U 1 -> 0
            if (w & 0xFE800E90) == 0xF2800E10 and (w >> 8) & 1 and (w >> 24) & 1:
                w &= ~(1 << 24); ok = 'a'
            elif (hw1 & 0xEF80) == 0xEF80 and (hw1 >> 12) & 1 and (hw2 & 0x0E90) == 0x0E10 and (hw2 >> 8) & 1:   # T32
                hw1 &= ~(1 << 12); ok = 't'
        if not ok:
            raise RuntimeError('%s +%x : vcvt au motif inattendu %08x (%s)' % (path, a, w, line.strip()))
        if ok == 'a':
            struct.pack_into('<I', data, a, w)
        else:
            struct.pack_into('<HH', data, a, hw1, hw2)
        if vector:
            vec += 1
        else:
            scal += 1
    done = scal + vec
    if done:
        open(path, 'wb').write(data)
        left = subprocess.run([objdump, '-d', '--no-show-raw-insn', path], capture_output=True, text=True, errors='replace').stdout
        if re.search(r'vcvt\.u32\.f(32|64)', left):
            raise RuntimeError('%s : vcvt.u32 restants apres correction' % path)
    return scal, vec


def main():
    rsp = sys.argv[1]
    objdump = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.environ.get('NDK', 'C:/android/android-ndk-r26d'), 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-objdump.exe')
    objs = [t.strip('"') for t in open(rsp).read().split() if t.strip('"').endswith('.o')]
    tot_s = tot_v = nobj = 0
    for o in objs:
        base = os.path.basename(o)
        if base.startswith('port__') or 'njgfx' in base:     # notre code de portage : ecrit pour l'ARM, laisse tel quel
            continue
        s, v = patch_object(o, objdump)
        if s or v:
            nobj += 1
        tot_s += s
        tot_v += v
    print('ppc_f2u : %d conversions scalaires et %d vectorielles rendues signees dans %d objets' % (tot_s, tot_v, nobj))


if __name__ == '__main__':
    main()
