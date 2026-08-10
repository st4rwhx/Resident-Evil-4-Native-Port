#!/usr/bin/env python3
"""rasim.py DIR [--drop A B ..] : simulate MWCC's Chaitin removal on regalloc-gpr-pass-1-all.txt; print
the iteration in which each node was removed (level), degree at removal, and the actual assigned reg."""
import re, sys
d = sys.argv[1]
txt = open(f'{d}/regalloc-gpr-pass-1-all.txt').read()
nodes = {}
order = []
for b in re.split(r'\n(?=r\d+ -> )', txt):
    m = re.match(r'(r\d+) -> (r\d+)\s*(\S*)', b)
    if not m: continue
    v = m.group(1)
    nb = re.search(r'\n  neighbors: (\d+) \((.*?)\)', b)
    cost = re.search(r'adjusted cost: ([\d.]+)', b)
    nbs = nb.group(2).split() if nb else []
    phys = [n for n in nbs if int(n[1:]) < 32]
    virt = [n for n in nbs if int(n[1:]) >= 32]
    fl = re.search(r'\n  flags: (.*)', b)
    ghost = bool(fl and 'fCoalesced' in fl.group(1) and 'Into' not in fl.group(1))
    nodes[v] = dict(reg=m.group(2), name=m.group(3), phys=set(phys), virt=set(virt), cost=float(cost.group(1)) if cost else 0, ghost=ghost)
    order.append(v)
# only nodes that have neighbours listed (coalesced ghosts have 'previous neighbors' only)
live = {v for v in nodes if (nodes[v]['virt'] or nodes[v]['phys']) and not nodes[v]['ghost']}
ghosts = {v for v in nodes if nodes[v]['ghost']}
drops = sys.argv[sys.argv.index('--drop')+1:] if '--drop' in sys.argv else []
for v in drops:
    for n in nodes[v]['virt']:
        if n in nodes: nodes[n]['virt'].discard(v)
    live.discard(v)
K = int(sys.argv[sys.argv.index('--K')+1]) if '--K' in sys.argv else 29
deg = {v: len(nodes[v]['phys']) + len(nodes[v]['virt'] & (live | ghosts)) for v in live}
remaining = set(live)
level = {}; rdeg = {}
it = 0
while remaining:
    it += 1
    removed_any = False
    for v in sorted(remaining, key=lambda x: int(x[1:])):
        if deg[v] < K:
            remaining.discard(v); level[v] = it; rdeg[v] = deg[v]; removed_any = True
            for n in nodes[v]['virt']:
                if n in remaining: deg[n] -= 1
    if not removed_any:
        v = min(remaining, key=lambda x: (nodes[x]['cost'], int(x[1:])))
        remaining.discard(v); level[v] = f'{it}S'; rdeg[v] = deg[v]
        for n in nodes[v]['virt']:
            if n in remaining: deg[n] -= 1
maxlvl = max(int(str(l).rstrip('S')) for l in level.values())
for v in sorted(live, key=lambda x: (-int(str(level[x]).rstrip('S')), -int(x[1:]))):
    n = nodes[v]
    if True:
        print(f'{v:>5} lvl {str(level[v]):>3} deg@rm {rdeg[v]:>2} tot {len(n["phys"])+len(n["virt"]):>2} -> {n["reg"]:>4} {n["name"]}')
