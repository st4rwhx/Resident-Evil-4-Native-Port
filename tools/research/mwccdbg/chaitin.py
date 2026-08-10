#!/usr/bin/env python3
"""chaitin.py DIR [--pass N] [--check] [--verbose] [--nb] [--costs]

Python model of the MWCC 2.4.7 GPR Chaitin allocator, fed from an mwcc-debugger dump directory
(ra.py output).  Reads `regalloc-gpr-pass-N-all.txt` (interference graph, names, coalesced nodes) and
`backend-NN-before-regalloc.txt` (PCode with LOOPWEIGHT per block, for the spill costs) and prints the
predicted colouring ORDER and colours.  `--check` compares against the compiler's own
`regalloc-gpr-pass-N-assigned.txt` (order, colour, degree at removal, cost where the compiler reports
one) and lists every divergence.

Model (docs/research/cri.md "CRI sfd_mpv/mwsfdcre pass 2/3"; IDENTICAL to the compiler on ExecServerSub, ChkBufSiz,
DecodePicAtr (10 spill picks), CreateSfd pass 1 and pass 2, CreateSofdec):
  * nodes = virtual registers r32.. in vid order (params -> own locals reverse-declared -> inlined
    helper locals at their inlining point -> frontend @N temps -> backend temps in IR order).
    Physical registers r0..r31 are pre-coloured and never removed.  A node flagged `fCoalesced` was
    merged into its `-> target`: into a physical register (argument move) or into another vreg (a
    copy); it stays in every neighbour list as a never-removed "ghost" carrying its target's colour.
    A `fCoalescedInto` leader's neighbours are the union of its members' neighbours minus the members.
  * simplification: scan vids upward, remove every node whose CURRENT degree is < 29 (push on the
    stack, decrement its neighbours); repeat the scan until nothing is removable;
  * when stuck: remove the node with the minimum  cost / current_degree, ties -> highest vid; rescan.
    cost = sum over the class's instructions (intra-class `mr` copies excluded) of
    LOOPWEIGHT * (2*uses + defs); the base of an update-form load/store is a use and a def; a node
    with a SINGLE def that is a `li`/`lis` (rematerialisable) costs sum(LOOPWEIGHT of its uses) -
    LOOPWEIGHT(def) (a multi-def constant variable is charged normally).
  * colouring: pop the stack; a node takes the lowest-numbered free register among r0, r3..r12 and the
    callee-saved registers already handed out in this function, else a NEW callee-saved register
    from r31 downward; nothing free -> SPILL (the compiler then inserts spill code and reruns as pass 2).

Usage (cwd = re4 root; ra.py runs must be sequential):
  python3 tools/research/mwccdbg/ra.py lib/sfd_mpv sfmpv_ChkBufSiz --quiet
  python3 tools/research/mwccdbg/chaitin.py /tmp/mwccdbg.$USER/out/sfmpv_ChkBufSiz --check
  python3 tools/research/mwccdbg/chaitin.py /tmp/mwccdbg.$USER/out/mwsfcre_CreateSfd --pass 2 --check
As a library: `import chaitin; g = chaitin.load(DIR); pred = chaitin.allocate(g)`; edit `g.adj` (add /
remove an interference), `g.cost`, or `g.order` (the vid scan order) before `allocate` to test "what
if this value had one more neighbour / a loop-weighted use / a lower id" without touching the source.
The -all.txt of pass 2 has no PCode of its own: costs for --pass 2 are taken from the compiler's
`cost:` lines of the pass-2 -assigned.txt when present (they are carried over from pass 1).
"""
import os, re, sys, glob

K = 29                                   # r0, r3..r12, r14..r31
VOLATILE = [0] + list(range(3, 13))      # colouring candidates in this order
STORES = {'stw', 'stwu', 'stb', 'stbu', 'sth', 'sthu', 'stwx', 'stbx', 'sthx', 'stwux', 'stfd', 'stfs',
          'stfdu', 'stfsu', 'stfdx', 'stfsx', 'stwbrx', 'sthbrx', 'stmw', 'stswi', 'dcbz', 'dcbf', 'dcbst', 'icbi'}
NODEF = {'cmp', 'cmpi', 'cmpl', 'cmpli', 'fcmpu', 'fcmpo', 'mtctr', 'mtlr', 'mtcrf', 'mtfsf', 'mtfsb0', 'mtfsb1',
         'b', 'bt', 'bf', 'bl', 'blr', 'bctr', 'bctrl', 'bdnz', 'bdz', 'sync', 'isync', 'eieio', 'nop', 'trap'}
UPDATE = {'lwzu', 'lbzu', 'lhzu', 'lhau', 'lfdu', 'lfsu', 'stwu', 'stbu', 'sthu', 'stfdu', 'stfsu', 'lwzux', 'stwux'}
REMAT = {'li', 'lis'}


class Graph:
    def __init__(self):
        self.names = {}     # vid -> name
        self.adj = {}       # vid -> set(vid)  (physical 0..31 and ghosts included), leaders merged
        self.alias = {}     # ghost vid -> target (physical reg or vreg)
        self.cost = {}      # vid -> number
        self.uses = {}      # vid -> (weighted uses, weighted defs)
        self.order = []     # scan order of the real nodes (vid ascending)
        self.actual = []    # [(vid, reg, name, prev, total, cost)] from -assigned.txt

    def leader(self, v):
        seen = 0
        while v in self.alias and self.alias[v] >= 32 and seen < 100:
            v = self.alias[v]; seen += 1
        return v


def _reg(s):
    m = re.fullmatch(r'r(\d+)', s)
    return int(m.group(1)) if m else None


def parse_nodes(path):
    """-> {vid: dict(reg, name, flags, prev, nbs, cost)}"""
    nodes = {}
    cur = None
    for line in open(path):
        m = re.match(r'^r(\d+) -> (\S+)\s*(.*)$', line)
        if m:
            cur = {'vid': int(m.group(1)), 'reg': m.group(2), 'name': m.group(3).strip(), 'flags': '',
                   'prev': None, 'nbs': set(), 'cost': None}
            nodes[cur['vid']] = cur
            continue
        if cur is None:
            continue
        m = re.match(r'\s+flags:\s*(.*)', line)
        if m: cur['flags'] = m.group(1); continue
        m = re.match(r'\s+cost: (\d+)', line)
        if m: cur['cost'] = int(m.group(1)); continue
        m = re.match(r'\s+previous neighbors: (\d+)', line)
        if m: cur['prev'] = int(m.group(1)); continue
        m = re.match(r'\s+neighbors: (\d+)\s*\(?([^)]*)\)?', line)
        if m: cur['nbs'] = set(_reg(x) for x in m.group(2).split() if _reg(x) is not None)
    return nodes


def parse_costs(path, g):
    """LOOPWEIGHT-weighted use/def counts per coalesce class from the pre-regalloc PCode."""
    w = 1
    uses, defs, remat, wdef, ndef = {}, {}, {}, {}, {}
    for line in open(path):
        m = re.search(r'LOOPWEIGHT=(\d+)', line)
        if m:
            w = int(m.group(1)); continue
        # `   444  lwz      r4,r32,0x1fc4`, or without a line number (parameter moves, inserted copies)
        m = re.match(r'^\s+(?:\d+\s+)?([a-z][a-z0-9_.]*)\s+(\S.*)$', line)
        if not m:
            continue
        op, rest = m.group(1), m.group(2)
        base = op.rstrip('.')
        if base in ('bl', 'bctrl'):
            continue
        ops = [x.strip() for x in rest.split(',')]
        regs = []
        for i, o in enumerate(ops):
            r = _reg(o.split('(')[0])
            if r is not None:
                regs.append((i, r))
        if not regs:
            continue
        if base == 'mr' and len(regs) == 2 and regs[0][1] >= 32 and regs[1][1] >= 32 \
                and g.leader(regs[0][1]) == g.leader(regs[1][1]):
            continue                                   # coalesced copy: no instruction
        isdef = [False] * len(ops)
        isuse = [True] * len(ops)
        if base not in STORES and base not in NODEF:
            isdef[0] = True; isuse[0] = False
        if base in UPDATE and len(ops) > 1:
            isdef[1] = True
        for i, r in regs:
            if r < 32:
                continue
            v = g.leader(r)
            if isdef[i]:
                defs[v] = defs.get(v, 0) + w
                remat[v] = remat.get(v, True) and base in REMAT
                wdef[v] = wdef.get(v, 0) + w
                ndef[v] = ndef.get(v, 0) + 1
            if isuse[i]:
                uses[v] = uses.get(v, 0) + w
    for v in g.adj:
        u, d = uses.get(v, 0), defs.get(v, 0)
        g.uses[v] = (u, d)
        if d and remat.get(v) and ndef.get(v) == 1:
            g.cost[v] = max(u - wdef[v], 0)
        else:
            g.cost[v] = 2 * u + d


def load(d, rapass=1):
    nodes = parse_nodes(os.path.join(d, f'regalloc-gpr-pass-{rapass}-all.txt'))
    g = Graph()
    for v, n in nodes.items():
        g.names[v] = n['name']
        if re.search(r'\bfCoalesced\b', n['flags']):
            g.alias[v] = _reg(n['reg'])
    members = {}
    for v in nodes:
        if v in g.alias and g.alias[v] >= 32:
            members.setdefault(g.leader(v), set()).add(v)
    for v, n in nodes.items():
        adj = set(n['nbs'])
        for m in members.get(v, ()):
            adj |= nodes[m]['nbs']
        adj -= members.get(v, set())
        adj.discard(v)
        g.adj[v] = adj
    g.order = [v for v in sorted(g.adj) if v not in g.alias]
    ap = os.path.join(d, f'regalloc-gpr-pass-{rapass}-assigned.txt')
    act = parse_nodes(ap) if os.path.exists(ap) else {}
    # the -assigned.txt is in colouring order; parse_nodes keeps insertion order
    g.actual = [(n['vid'], n['reg'], n['name'], n['prev'], len(n['nbs']), n['cost']) for n in act.values()]
    pc = sorted(glob.glob(os.path.join(d, 'backend-*-before-regalloc*.txt')))
    if pc and rapass == 1:
        parse_costs(pc[-1], g)
    else:
        for v in g.adj:
            g.cost[v] = 0
        for v, n in act.items():
            if n['cost'] is not None:
                g.cost[v] = n['cost']
    return g


def allocate(g, trace=None):
    """Return [(vid, reg, name, degree_at_removal, level, spillpick)] in colouring order."""
    nodes = list(g.order)
    deg = {v: len(g.adj[v]) for v in nodes}
    alive = set(nodes)
    stack = []       # (vid, deg_at_removal, level, spillpick)
    level = 0

    def remove(v, level, pick):
        alive.discard(v)
        stack.append((v, deg[v], level, pick))
        for n in g.adj[v]:
            if n in alive:
                deg[n] -= 1

    while alive:
        level += 1
        removed = False
        for v in nodes:
            if v in alive and deg[v] < K:
                remove(v, level, False)
                removed = True
        if removed:
            continue
        best = None
        for v in alive:
            score = g.cost.get(v, 0) / deg[v] if deg[v] else float('inf')
            key = (score, -v)
            if best is None or key < best[0]:
                best = (key, v)
        v = best[1]
        if trace is not None:
            trace.append((v, g.cost.get(v, 0), deg[v], best[0][0]))
        level += 1
        remove(v, level, True)

    colour = {p: p for p in range(32)}
    for v, t in g.alias.items():
        if t < 32:
            colour[v] = t
    handed = []      # callee-saved already used
    nxt = 31
    out = []

    def colour_of(n):
        if n in colour:
            return colour[n]
        if n in g.alias:
            return colour.get(g.leader(n))
        return None

    for v, d, lvl, pick in reversed(stack):
        used = set()
        for n in g.adj[v]:
            c = colour_of(n)
            if c is not None:
                used.add(c)
        reg = None
        for c in VOLATILE + sorted(handed):
            if c not in used:
                reg = c; break
        if reg is None:
            while nxt >= 14 and nxt in used:
                nxt -= 1
            if nxt >= 14:
                reg = nxt
                handed.append(nxt)
                nxt -= 1
        if reg is None:
            out.append((v, 'SPILL', g.names.get(v, ''), d, lvl, pick))
            continue
        colour[v] = reg
        out.append((v, f'r{reg}', g.names.get(v, ''), d, lvl, pick))
    return out


def fmt(g, i, v, r, n, d_, lvl, pick, mark='', nb=False):
    u, df = g.uses.get(v, (0, 0))
    line = (f"{i:3d} L{lvl:<2d}{'S' if pick else ' '} r{v:<4d}-> {r:<5} {n:<16} deg {d_:>2}/{len(g.adj[v]):<3} "
            f"cost {g.cost.get(v, 0):>4} ({u}u {df}d){mark}")
    if nb:
        line += '  [' + ' '.join(f'r{x}' for x in sorted(g.adj[v])) + ']'
    return line


def main():
    a = sys.argv[1:]
    d = a[0]
    rapass = int(a[a.index('--pass') + 1]) if '--pass' in a else 1
    verbose = '--verbose' in a
    nb = '--nb' in a
    check = '--check' in a
    if '--k' in a:
        # CRI pass 47: every physical register named in an `asm { mr rN, x }` pin is taken out of the
        # colour set for the whole function, so the removal threshold is K = 29 - (#pinned registers):
        # `--k 28` for one pin (adx_dcd5 Ste4AsSte), `--k 27` for two.  IDENTICAL on those dumps.
        global K
        K = int(a[a.index('--k') + 1])
    g = load(d, rapass)
    trace = []
    pred = allocate(g, trace)
    actual = {v: (r, i, p, c) for i, (v, r, n, p, t, c) in enumerate(g.actual)}
    bad = 0
    for i, (v, r, n, d_, lvl, pick) in enumerate(pred):
        mark = ''
        if check and g.actual:
            ar, ai, ap, ac = actual.get(v, ('?', -1, None, None))
            if ar == 'none':
                ar = 'SPILL'
            if ar != r or ai != i or (ap is not None and ap != d_):
                mark = f'   <-- compiler: {ar} at #{ai} deg {ap}'
                bad += 1
            if ac and ac != g.cost.get(v, 0):
                mark += f'   <-- compiler cost {ac}'
                bad += 1
        if verbose or mark or i < 40:
            print(fmt(g, i, v, r, n, d_, lvl, pick, mark, nb))
    if trace:
        print('spill picks (vid, cost, degree, score):', ' '.join(f'r{v}:{c}/{dg}={s:.3f}' for v, c, dg, s in trace))
    if g.alias:
        print('ghosts:', ' '.join(f'r{v}->r{t}{("=" + g.names[v]) if g.names.get(v) else ""}' for v, t in sorted(g.alias.items())))
    if check:
        print('CHECK:', 'IDENTICAL to the compiler' if bad == 0 else f'{bad} divergences')


if __name__ == '__main__':
    main()
