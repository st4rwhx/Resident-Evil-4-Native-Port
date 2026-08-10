#!/usr/bin/env python3
"""Generate config/<ver>/symbols.txt, splits.txt and objects.py from the RE4 debug-build Bio4.sym.

usage: gen_config.py <Bio4.sym> <main.dol> <config dir>
"""
import collections, re, struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from re4sym import parse
from symnames import REPL, sanitize  # noqa: F401 (shared with gen_rel_config.py)

symf, dolf, cfgdir = sys.argv[1:4]
names, funcs = parse(symf)

# --- DOL section layout -------------------------------------------------------
d = open(dolf, 'rb').read()
offs = struct.unpack('>18I', d[0:72]); addrs = struct.unpack('>18I', d[72:144]); sizes = struct.unpack('>18I', d[144:216])
bss, bsssize = struct.unpack('>2I', d[216:224])
t = [(addrs[i], addrs[i] + sizes[i]) for i in range(7) if sizes[i]]
dd = [(addrs[i], addrs[i] + sizes[i]) for i in range(7, 18) if sizes[i]]
assert len(t) == 2 and len(dd) == 6, (t, dd)
SECS = [
    ('.init', t[0][0], t[0][1], 'code'),
    ('.text', t[1][0], t[1][1], 'code'),
    ('.ctor', dd[0][0], dd[0][1], 'rodata'),  # named .ctor/.dtor: dtk's .ctors handling assumes the MWCC layout
    ('.dtor', dd[1][0], dd[1][1], 'rodata'),
    ('.rodata', dd[2][0], dd[2][1], 'rodata'),
    ('.data', dd[3][0], dd[3][1], 'data'),
    ('.bss', bss, dd[4][0], 'bss'),
    ('.sdata', dd[4][0], dd[4][1], 'data'),
    ('.sbss', dd[4][1], min(dd[5][0], bss + bsssize), 'bss'),
    ('.sdata2', dd[5][0], dd[5][1], 'rodata'),
]
sbss2_lo = dd[5][1]; sbss2_hi = bss + bsssize
if sbss2_hi > sbss2_lo:
    SECS.append(('.sbss2', sbss2_lo, sbss2_hi, 'bss'))
def sec(a):
    for s, lo, hi, _ in SECS:
        if lo <= a < hi: return s
    return None
SEC_END = {s: hi for s, lo, hi, _ in SECS}
SEC_START = {s: lo for s, lo, hi, _ in SECS}

# --- symbol names: sanitize() lives in tools/symnames.py -----------------------
size_at = collections.defaultdict(int)
for a, sz, u8, dn in funcs: size_at[a] = max(size_at[a], sz)

# unit (file) name per object index; handle duplicate object names (e.g. two memset.obj)
def unit_name(objname, first_addr):
    stem = objname.rsplit('.', 1)[0]
    if objname.endswith('.obj'):
        return f"game/{stem}.cpp"
    return f"lib/{stem}.c"

# Collect symbols, drop inner labels / duplicates at same address
by_addr = collections.OrderedDict()
for a, sz, u8, dn in sorted(funcs, key=lambda f: (f[0], -(f[1]))):
    s = sec(a)
    if s is None: continue
    obj = names[u8 >> 16]; scope = 'global' if (u8 & 1) else 'local'
    if sz == 0: sz = size_at[a]
    if a in by_addr:
        # prefer a real name over '.' / '@'
        old = by_addr[a]
        if (old['dn'] in ('.',) or old['dn'].startswith('@')) and not (dn == '.' or dn.startswith('@')):
            by_addr[a] = dict(a=a, sz=max(sz, old['sz']), obj=old['obj'] if old['obj'] != 'global' else obj, scope=scope, dn=dn)
        continue
    by_addr[a] = dict(a=a, sz=sz, obj=obj, scope=scope, dn=dn)

syms = list(by_addr.values())
# drop labels strictly inside a previous symbol, clamp to section end
out = []; prev_end = 0; prev_sec = None; dropped = 0
for e in syms:
    s = sec(e['a'])
    if s == prev_sec and e['a'] < prev_end:
        dropped += 1; continue
    if e['a'] + e['sz'] > SEC_END[s]: e['sz'] = SEC_END[s] - e['a']
    # unnamed all-zero tail padding in code sections is not a function
    if e['dn'] == '.' and s in ('.init', '.text') and e['a'] + e['sz'] == SEC_END[s]:
        blob = d[offs[[x[0] for x in SECS].index(s)] + (e['a'] - SEC_START[s]):][:e['sz']]
        if not any(blob): dropped += 1; continue
    if e['sz'] == 0 and e['dn'] in ('__bss_start', '_f_bss', '__bss_end', '_e_bss'): continue
    if s in ('.ctor', '.dtor'): e['sz'] = 4  # one pointer per object; the null terminator is linker-generated
    e['sec'] = s; out.append(e); prev_end = e['a'] + e['sz']; prev_sec = s
syms = out

# dtk reserves the names _ctors/_dtors for the .ctors/.dtors section labels (MWCC convention) and
# drops object symbols with those names; the game's tors.c pointer pair is renamed to avoid that.
for e in syms:
    if e['dn'] in ('_ctors', '_dtors') and e['sec'] == '.data': e['dn'] = e['dn'] + '_data'
# names
seen = collections.Counter(); at_names = collections.Counter(n for e in syms for n in [e['dn']] if n.startswith('@'))
for e in syms:
    dn = e['dn']; code = SECS[[x[0] for x in SECS].index(e['sec'])][3] == 'code'
    if dn == '.':
        n = f"{'fn' if code else 'lbl'}_{e['a']:08X}"
    elif dn.startswith('@'):
        n = dn if at_names[dn] == 1 else f"{dn}_{e['a']:08X}"
    else:
        n = sanitize(dn)
        seen[n] += 1
        if seen[n] > 1: n = f"{n}_{e['a']:08X}"
    e['name'] = n
    e['type'] = 'function' if code else 'object'

# .init: no symbols in the sym file. dtk's __start analysis expects the MWCC crt0 layout and
# explodes on the ProDG one, so the section is left symbol-less (raw data blob in lib/__start.c);
# the linker script defines __start = 0x80003100 for ENTRY().
init_unit_needed = not any(e['sec'] == '.init' for e in syms)
# GCC crtbegin: the -1 list heads referenced by __do_global_ctors/__do_global_dtors
for nm, sc in (('__CTOR_LIST__', '.ctor'), ('__DTOR_LIST__', '.dtor')):
    syms.append(dict(a=SEC_START[sc], sz=4, obj='crtbegin.o', scope='global', dn=nm, sec=sc, name=nm, type='object'))
syms.sort(key=lambda e: e['a'])

# --- units & splits --------------------------------------------------------------
# assign 'global' (linker/common) symbols to the previous object in the section
units = collections.OrderedDict()  # unit -> {sec: [lo, hi]}
unit_of_obj = {}
prev_unit = None
runs = []  # (unit, sec, lo, hi)
forked = set()
for e in syms:
    obj = e['obj']
    if obj == 'global':
        e['unit'] = prev_unit; e['common'] = True
    else:
        if obj not in unit_of_obj:
            unit_of_obj[obj] = unit_name(obj, e['a'])
        u = unit_of_obj[obj]
        # duplicate object names in .text (two memset.obj): fork a second unit once
        if (e['sec'] == '.text' and runs and runs[-1][1] == '.text' and runs[-1][0] != u
                and any(r[0] == u and r[1] == '.text' for r in runs) and obj not in forked):
            forked.add(obj)
            u = re.sub(r'\.(cpp|c)$', r'_2.\1', u)
            unit_of_obj[obj] = u
        e['unit'] = u; e['common'] = False
        prev_unit = u
    if runs and runs[-1][0] == e['unit'] and runs[-1][1] == e['sec']:
        runs[-1][3] = e['a'] + e['sz']
    else:
        runs.append([e['unit'], e['sec'], e['a'], e['a'] + e['sz']])
# extend each run's end to the next run's start in the same section (padding belongs to previous unit)
by_sec = collections.defaultdict(list)
for r in runs: by_sec[r[1]].append(r)
for s, rs in by_sec.items():
    rs.sort(key=lambda r: r[2])
    for i, r in enumerate(rs):
        r[3] = rs[i + 1][2] if i + 1 < len(rs) else (r[3] if s in ('.ctor', '.dtor') else SEC_END[s])
    # first run starts at the section start
    if rs[0][2] != SEC_START[s]:
        print(f"note: {s} starts at {SEC_START[s]:#x} but first symbol at {rs[0][2]:#x}")
        rs[0][2] = SEC_START[s]

if init_unit_needed:
    runs.insert(0, ['lib/__start.c', '.init', SEC_START['.init'], SEC_END['.init']])
for r in runs: units.setdefault(r[0], []).append(r)

# --- write symbols.txt -------------------------------------------------------------
with open(os.path.join(cfgdir, 'symbols.txt'), 'w') as f:
    for e in syms:
        extra = ''
        f.write(f"{e['name']} = {e['sec']}:0x{e['a']:08X}; // type:{e['type']} size:0x{e['sz']:X} scope:{e['scope']}{extra}\n")
print(len(syms), 'symbols;', dropped, 'inner labels dropped')

# --- write splits.txt ---------------------------------------------------------------
with open(os.path.join(cfgdir, 'splits.txt'), 'w') as f:
    f.write('Sections:\n')
    for s, lo, hi, ty in SECS:
        # dtk maps header entries to DOL sections in order; .ctors/.dtors share DOL section 7
        al = 32 if s == '.sdata2' else 4
        f.write(f"\t{s:<11} type:{ty} align:{al}\n")
    f.write('\n')
    for u, rs in units.items():
        f.write(f"{u}:\n")
        for _, s, lo, hi in sorted(rs, key=lambda r: [x[0] for x in SECS].index(r[1])):
            f.write(f"\t{s:<11} start:0x{lo:08X} end:0x{hi:08X}\n")
        f.write('\n')
print(len(units), 'units')

# --- write objects.py (for configure.py) -------------------------------------------------
with open(os.path.join(cfgdir, 'objects.py'), 'w') as f:
    f.write('# Generated by tools/gen_config.py from Bio4.sym. Edit Matching status by hand.\n')
    f.write('UNITS = [\n')
    for u in units:
        f.write(f'    "{u}",\n')
    f.write(']\n')

# --- write name map -------------------------------------------------------------------
with open(os.path.join(cfgdir, 'sym_map.tsv'), 'w') as f:
    f.write('address\tsize\tsection\tunit\tscope\tname\tdemangled\n')
    for e in syms:
        f.write(f"0x{e['a']:08X}\t0x{e['sz']:X}\t{e['sec']}\t{e['unit']}\t{e['scope']}\t{e['name']}\t{e['dn']}\n")
