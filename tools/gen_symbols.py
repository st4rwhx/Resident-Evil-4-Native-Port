#!/usr/bin/env python3
"""Generate config/<ver>/symbols.txt (functions only, or all) from a RE4 .sym file.
usage: gen_symbols.py Bio4.sym out_symbols.txt [--text-only]"""
import sys, re, collections
sys.path.insert(0, __file__.rsplit('/',1)[0]); from re4sym import parse
symf, out = sys.argv[1], sys.argv[2]; text_only = '--text-only' in sys.argv
names, funcs = parse(symf)
# DOL section layout (from header)
SECS = [('.init',0x80003100,0x800034A0), ('.text',0x800034A0,0x8021C960)]
def sec(a):
    for s,lo,hi in SECS:
        if lo <= a < hi: return s
    return None
REPL = {'::':'__','operator=':'op_assign','operator==':'op_eq','operator!=':'op_ne','operator<':'op_lt','operator>':'op_gt','operator<=':'op_le','operator>=':'op_ge','operator+':'op_add','operator-':'op_sub','operator*':'op_mul','operator/':'op_div','operator[]':'op_idx','operator()':'op_call','operator new':'op_new','operator delete':'op_delete','operator+=':'op_addeq','operator-=':'op_subeq','operator*=':'op_muleq','operator/=':'op_diveq','operator!':'op_not','operator&':'op_and','operator|':'op_or','operator^':'op_xor','operator<<':'op_shl','operator>>':'op_shr','operator->':'op_arrow','operator++':'op_inc','operator--':'op_dec','operator%':'op_mod'}
def sanitize(n):
    if n.startswith('@') or n == '.': return None  # unnamed/local literal, let dtk name it
    for k,v in sorted(REPL.items(), key=lambda kv:-len(kv[0])): n = n.replace(k, v)
    n = n.replace('~','dt_')
    n = re.sub(r'[<>,\s\*&\(\)\[\]]+','_', n).strip('_')
    n = re.sub(r'_+','_', n)
    if not re.match(r'^[A-Za-z_@$.]', n): n = '_' + n
    return n
seen = collections.Counter(); rows = []
prev_end = 0; dropped = 0
size_at = collections.defaultdict(int)
for a, sz, u8, dn in funcs: size_at[a] = max(size_at[a], sz)
for a, sz, u8, dn in sorted(funcs):
    if sz == 0: sz = size_at[a]  # asm labels: take size from the unnamed entry at the same address
    s = sec(a)
    if s is None: continue
    if a < prev_end or sz == 0:  # label inside a previous function / asm label without size - drop
        dropped += 1; continue
    prev_end = a + sz
    obj = names[u8 >> 16]; scope = 'global' if (u8 & 1) else 'local'
    n = sanitize(dn)
    if n is None: continue
    seen[n] += 1
    if seen[n] > 1: n = f"{n}_{a:08X}"
    rows.append(f"{n} = {s}:0x{a:08X}; // type:function size:0x{sz:X} scope:{scope}")
open(out,'w').write('\n'.join(rows)+'\n')
print(len(rows),'function symbols written;',dropped,'inner labels dropped')
# mapping: sanitized -> demangled, obj
with open(out.replace('symbols.txt','sym_map.tsv'),'w') as f:
    seen=collections.Counter()
    f.write('address\tsize\tobject\tscope\tsanitized\tdemangled\n')
    for a, sz, u8, dn in sorted(funcs):
        n=sanitize(dn) or dn
        if n!=dn: seen[n]+=1
        if seen[n]>1: n=f"{n}_{a:08X}"
        f.write(f"0x{a:08X}\t0x{sz:X}\t{names[u8>>16]}\t{'global' if u8&1 else 'local'}\t{n}\t{dn}\n")
