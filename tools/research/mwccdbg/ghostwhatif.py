#!/usr/bin/env python3
"""ghostwhatif.py RA_DIR [--k N] [+ghost:REG[:like=VID|:all|:names=a,b,c]]... [--target name=reg,...]
Replay chaitin.py on a dump with extra never-removed ghost nodes aliased to a physical REG.
Adjacency: all (default) = every real node; like=VID = the neighbours of vid VID; names=... = the named nodes."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import chaitin
args = sys.argv[1:]
d = args.pop(0)
g = chaitin.load(d)
target = {}
i = 0
while i < len(args):
    a = args[i]
    if a == '--k':
        chaitin.K = int(args[i + 1]); i += 2; continue
    if a == '--target':
        for kv in args[i + 1].split(','):
            k, v = kv.split('='); target[k] = v
        i += 2; continue
    if a.startswith('+ghost:'):
        parts = a.split(':')
        reg = int(parts[1])
        mode = parts[2] if len(parts) > 2 else 'all'
        gv = 1000 + len(g.alias)
        if mode == 'all':
            nbs = set(g.order)
        elif mode.startswith('like='):
            nbs = set(g.adj[int(mode[5:])])
        elif mode.startswith('phys='):
            p = int(mode[5:])
            nbs = {v for v in g.order if p in g.adj[v]}
        elif mode.startswith('names='):
            want = set(mode[6:].split(','))
            nbs = {v for v in g.order if g.names.get(v) in want}
        g.names[gv] = f'ghost{reg}'
        g.alias[gv] = reg
        g.adj[gv] = set(nbs)
        for v in nbs:
            if v in g.adj:
                g.adj[v].add(gv)
    i += 1
out = chaitin.allocate(g)
named = {}
for v, r, n, dd, lvl, pick in out:
    if n and not n.startswith('@') and n not in named:
        named[n] = r
print(' '.join(f'{v}:{n or "-"}={r}{"S" if pick else ""}' for v, r, n, dd, lvl, pick in out))
if target:
    ok = [k for k in target if named.get(k) == 'r' + target[k].lstrip('r')]
    bad = [f'{k}:{named.get(k)}!={target[k]}' for k in target if named.get(k) != 'r' + target[k].lstrip('r')]
    print(f'score {len(ok)}/{len(target)}', ' '.join(bad))
