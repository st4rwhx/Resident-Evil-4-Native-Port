#!/usr/bin/env python3
"""rasum.py DIR [--nb]  -> one line per node of regalloc-gpr-pass-1-assigned.txt in priority order:
   #  vid -> reg  name  prev/total   [neighbours]     (coalesced nodes from -all.txt listed at the end)"""
import re, sys, os
d = sys.argv[1]
nb = '--nb' in sys.argv
def parse(path):
    nodes = []
    cur = None
    for line in open(path):
        m = re.match(r'^(r\d+) -> (\S+)\s*(.*)$', line)
        if m:
            cur = {'vid': m.group(1), 'reg': m.group(2), 'name': m.group(3).strip(), 'flags': '', 'prev': '', 'nbs': ''}
            nodes.append(cur); continue
        if cur is None: continue
        m = re.match(r'\s+flags:\s*(.*)', line)
        if m: cur['flags'] = m.group(1).strip(); continue
        m = re.match(r'\s+previous neighbors: (\d+)\s*(.*)', line)
        if m: cur['prev'] = m.group(1); cur['pn'] = m.group(2); continue
        m = re.match(r'\s+neighbors: (\d+)\s*(.*)', line)
        if m: cur['nbs'] = m.group(1); cur['nn'] = m.group(2); continue
    return nodes
A = parse(os.path.join(d, 'regalloc-gpr-pass-1-assigned.txt'))
for i, n in enumerate(A):
    s = f"{i:3d} {n['vid']:>4} -> {n['reg']:<4} {n['name']:<14} {n['prev']:>2}/{n['nbs']:<2}"
    if nb: s += ' ' + n.get('nn', '')
    print(s)
allp = os.path.join(d, 'regalloc-gpr-pass-1-all.txt')
if os.path.exists(allp):
    co = [n for n in parse(allp) if 'Coalesced' in n['flags']]
    if co:
        print('coalesced (ghosts):', ' '.join(f"{n['vid']}->{n['reg']}{('=' + n['name']) if n['name'] else ''}" for n in co))
