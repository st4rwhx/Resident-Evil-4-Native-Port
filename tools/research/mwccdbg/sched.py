#!/usr/bin/env python3
"""sched.py DUMPDIR [--post] [--block N] [--verbose]

Replay of the MWCC 2.4.7 (GC/2.6 build 107 = GC/2.7 build 108 codegen) PCode list scheduler, read off the
GC/2.6 mwcceppc.exe (scheduler entry 0x507c70, per-block 0x507d80, DAG builder 0x508090, selection 0x507f50,
gekko machine model table 0x5d5b50 / per-opcode bytes at 0x5d5b78).  Input = the raw pcode dumps written by
scheddump.sh lib/unit Func (sched-pre1/post1 = before/after the pre-RA run, pre2/post2 = the post-RA
run; default DIR = $MWCCDBG_OUT/Func = /tmp/mwccdbg.$USER/out/Func).  schedcheck.sh "lib/unit Func" .. = dump + check in one line per function;
schedtrace.sh = the compiler's own (cycle, pcode, height, deadline) picks for a cycle-by-cycle comparison with --verbose.
Prints per block whether the model reproduces the compiler's order; --verbose prints the cycle-by-cycle issue.

ALGORITHM (both runs use the same routine; `postRA` only changes the block gate and one tie-break):
  blocks: scheduled iff pcodeCount > 2 and !(flags & 8) and (postRA or !(flags & 3)); flags |= 8 afterwards.
  DAG (built walking the block BACKWARDS, so every edge goes from an earlier to a later pcode):
    per register operand (kind 0; GPR r2/r13 and the r0 of an indexed base are skipped; rw&2 = write else read):
      write -> edge to every later reader (kind 1 = latency) and every later writer (kind 0 for GPR/FPR/CR, kind 1 for
      SPRs); read -> edge to every later writer (same kind rule).  A read+write operand (rw 3) counts as a write only.  Lists are never cleared (edges are transitively redundant but they COUNT for npreds/freeing).
    memory: load (flags 0x20002) -> edge (kind 1) to every later store that may alias; store (0x40004) -> edge to
      every later load and store that may alias.  May-alias = the pcode alias records (type 0/1 = object [+off,size],
      type 2 = alias-class bitset): same record; obj/obj same object (and for type 1/1 overlapping [off,off+size));
      obj/bitset = the object's index bit is in the set; bitset/bitset = the sets intersect.
    flags 0x80: ordered chain (kind 0) among themselves.
    barrier (flags 0x1000000 | 0x100, or the opcode's model byte 5: every branch, bl, mtctr/mtlr/mfspr, sync):
      kind-0 edge to EVERY later pcode; every pcode gets a kind-0 edge to every later barrier.
    a pcode with no successor gets a kind-0 edge to the block's flag-1 terminator.
    edge latency: kind 1 -> latency(src) (table byte 1, +2 if flags&0x20000000 and !(flags&9), +argc-2 for lmw/stmw);
      kind 0 -> 0.  Duplicate edges keep the max latency and are not counted twice.
    height(n) = max(latency(n), max over edges (lat + height(succ)));  deadline(n) = maxheight - height(n).
  LIST SCHEDULING (top-down, cycle by cycle, at most model.width = 2 picks per cycle):
    candidates = unscheduled pcodes in ORIGINAL order with npreds == 0, readytime <= cycle and model.canIssue().
    best = first candidate; each later candidate replaces it iff, in this order:
      1. it is urgent (deadline <= cycle) and best is not   (best urgent, cand not -> keep);
      2. it frees more successors (successors whose npreds == 1)   (fewer -> keep);
      3. its height is greater   (smaller -> keep);
      4. pre-RA only (global 0x5ea638 = "virtual registers"): its opcodeinfo byte +9 is LOWER
         (0 branch/mr/nop < 1 store/cmp/mtctr < 2 int arith < 3 load < 4 li/lis/mfspr);
      5. otherwise keep best (= earliest in the original order).
    on issue: succ.npreds--, succ.ready = max(succ.ready, cycle + lat); model.issue(); after the cycle model.advance().
  GEKKO MODEL (0x5d5b50: width 2, word +4 = 1 -> WAR/WAW edges have no latency, word +8 = 0 extra into branches):
    per opcode 6 bytes: unit, latency, occupancy1, occupancy2, occupancy3, barrier.  units: 0 BPU, 1 IU1, 2 "IU1 or
    IU2" (IU1 preferred), 3 LSU stage1 -> 4 LSU stage2, 5/6/7 FPU stages, 8 SRU.  A 6-entry in-order completion queue
    (2 retire per cycle) blocks issue when full.  canIssue: unit slot free (for class 2: one IU free, and if only one
    is free the pcode must not touch the GPR written by the other IU's occupant nor by the op that completed in
    IU1/IU2 at the end of the previous cycle); a store cannot issue while LSU stage 2 holds a store.
"""
import os, re, sys, struct

# the GC/2.6 mwcceppc.exe of the configured tree (the machine-model tables are read from it)
_ROOT = os.environ.get('RE4_ROOT') or __import__('subprocess').run(
    ['git', 'rev-parse', '--show-toplevel'], cwd=os.path.dirname(os.path.abspath(__file__)),
    capture_output=True, text=True, check=True).stdout.strip()
EXE = os.path.join(_ROOT, 'build/compilers/GC/2.6/mwcceppc.exe')
OPI, GK, NOPS = 0x5c0fa8, 0x5d5b78, 471

def _load_tables():
    exe = open(EXE, 'rb').read()
    pe = struct.unpack_from('<I', exe, 0x3c)[0]
    nsec = struct.unpack_from('<H', exe, pe + 6)[0]
    opt = struct.unpack_from('<H', exe, pe + 20)[0]
    base = struct.unpack_from('<I', exe, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        s = pe + 24 + opt + i * 40
        vs, va, rs, ro = struct.unpack_from('<IIII', exe, s + 8)
        secs.append((base + va, max(vs, rs), ro))
    def f(va):
        for a, n, ro in secs:
            if a <= va < a + n:
                return ro + va - a
        raise KeyError(hex(va))
    def cstr(va):
        o = f(va); return exe[o:exe.index(b'\0', o)].decode()
    names, b9, gk = {}, {}, {}
    for i in range(NOPS):
        o = f(OPI + i * 0x12)
        nm = cstr(struct.unpack_from('<I', exe, o)[0])
        names[nm] = i
        b9[nm] = exe[o + 9]
        gk[nm] = list(struct.unpack('<6b', exe[f(GK + i * 6):f(GK + i * 6) + 6]))
    return names, b9, gk

OPNUM, B9, GKT = _load_tables()

class Arg:
    __slots__ = ('kind', 'cls', 'reg', 'rw')
    def __init__(s, kind, cls=0, reg=0, rw=0):
        s.kind, s.cls, s.reg, s.rw = kind, cls, reg, rw

class Alias:
    def __init__(s, t, rec, obj=None, objname='', off=0, size=0, idx=0, bits=0):
        s.t, s.rec, s.obj, s.objname, s.off, s.size, s.idx, s.bits = t, rec, obj, objname, off, size, idx, bits

class PCode:
    def __init__(s, addr, op, flags, args, alias, text):
        s.addr, s.op, s.flags, s.args, s.alias, s.text = addr, op, flags, args, alias, text
        s.argc = len(args)

class Block:
    def __init__(s, idx, flags, count, weight):
        s.idx, s.flags, s.count, s.weight, s.pcodes = idx, flags, count, weight, []

def parse(path):
    blocks = []
    for line in open(path):
        if line.startswith('BLOCK'):
            m = re.match(r'BLOCK (\d+) flags=([0-9a-f]+) count=(-?\d+) weight=(-?\d+)', line)
            blocks.append(Block(int(m[1]), int(m[2], 16), int(m[3]), int(m[4])))
        elif line.startswith('P '):
            toks = line.split()
            addr = int(toks[1], 16); op = toks[2]; flags = int(toks[3][6:], 16)
            args, alias = [], None
            for t in toks[4:]:
                if t.startswith('alias='):
                    m = re.match(r'alias=t(\d)@([0-9a-f]+)', t)
                    alias = Alias(int(m[1]), int(m[2], 16))
                elif alias is not None:
                    if t.startswith('obj='):
                        m = re.match(r'obj=([0-9a-f]+)\((.*)\)', t); alias.obj = int(m[1], 16); alias.objname = m[2]
                    elif t.startswith('off='): alias.off = int(t[4:])
                    elif t.startswith('size='): alias.size = int(t[5:])
                    elif t.startswith('idx='): alias.idx = int(t[4:])
                    elif t.startswith('bits='):
                        ws = t[5:].split(',')
                        alias.bits = sum(int(w, 16) << (32 * i) for i, w in enumerate(ws))
                elif t[0] == 'R':
                    c, r, rw = t[1:].split(':'); args.append(Arg(0, int(c), int(r), int(rw, 16)))
                else:
                    args.append(Arg(1))
            blocks[-1].pcodes.append(PCode(addr, op, flags, args, alias, line.strip()))
    return blocks

# ---------------------------------------------------------------- alias
def may_alias(a, b):
    if a is None or b is None:
        return True
    ta, tb = a.t, b.t
    if ta == 0 and tb == 0:
        return a.rec == b.rec
    if (ta, tb) in ((0, 1), (1, 0)):
        return a.obj == b.obj or a.objname == b.objname
    if ta == 1 and tb == 1:
        if not (a.obj == b.obj or a.objname == b.objname):
            return False
        if a.rec == b.rec:
            return True
        return a.off == b.off or (a.off > b.off and a.off < b.off + b.size) or (b.off > a.off and b.off < a.off + a.size)
    if ta in (0, 1) and tb == 2:
        return bool(b.bits >> a.idx & 1)
    if ta == 2 and tb in (0, 1):
        return bool(a.bits >> b.idx & 1)
    if a.rec == b.rec:
        return True
    return bool(a.bits & b.bits)

# ---------------------------------------------------------------- machine model (gekko, 0x5d5b50)
WIDTH = 2
WAW_KIND = 0        # gekko model word +4 = 1 -> WAR/WAW edges of GPR/FPR/CR have latency 0
FDIV_OPS = (168, 169)

def touches_dst_of(p, o):
    """0x507bf0(p, o, GPR): p reads or writes the GPR that o's first operand writes."""
    if o is None or o.argc < 1:
        return False
    a0 = o.args[0]
    if a0.kind != 0 or a0.cls != 4 or not (a0.rw & 2):
        return False
    for a in p.args:
        if a.kind == 0 and a.cls == 4 and (a.rw & 3) and a.reg == a0.reg:
            return True
    return False

class Gekko:
    def reset(s):
        s.slot = [[None, 0] for _ in range(9)]
        s.cq = [[None, 0] for _ in range(6)]
        s.free, s.count, s.head, s.tail = 6, 0, 0, 0
        s.last1 = s.last2 = None

    def can_issue(s, p):
        if s.free == 0:
            return False
        u = GKT[p.op][0]
        if u == 2:
            c1 = s.slot[1][0] is None; c2 = s.slot[2][0] is None
            if not c1 and not c2:
                return False
            if c1 and c2:
                return True
            other = s.slot[2][0] if c1 else s.slot[1][0]
            if touches_dst_of(p, other) or touches_dst_of(p, s.last1) or touches_dst_of(p, s.last2):
                return False
        elif s.slot[u][0] is not None:
            return False
        if (p.flags & 4) and s.slot[4][0] is not None and (s.slot[4][0].flags & 4):
            return False
        return True

    def issue(s, p):
        u, occ = GKT[p.op][0], GKT[p.op][2]
        s.count += 1; s.free -= 1
        s.cq[s.tail] = [p, 0]; s.tail = (s.tail + 1) % 6
        if u == 2 and s.slot[1][0] is None:
            u = 1
        s.slot[u] = [p, occ]

    def _done(s, u):
        p = s.slot[u][0]
        for e in s.cq:
            if e[0] is p:
                e[1] = 1; break
        s.slot[u] = [None, 0]
        return p

    def advance(s):
        s.last1 = s.last2 = None
        for sl in s.slot:
            if sl[0] is not None and sl[1]:
                sl[1] -= 1
        for _ in range(2):
            if s.count and s.cq[s.head][1]:
                s.cq[s.head] = [None, 0]; s.count -= 1; s.free += 1; s.head = (s.head + 1) % 6
            else:
                break
        sl = s.slot
        if sl[1][0] is not None and sl[1][1] == 0: s.last1 = s._done(1)
        if sl[4][0] is not None and sl[4][1] == 0: s._done(4)
        if sl[7][0] is not None and sl[7][1] == 0: s._done(7)
        if sl[8][0] is not None and sl[8][1] == 0: s._done(8)
        if sl[0][0] is not None and sl[0][1] == 0: s._done(0)
        if sl[2][0] is not None and sl[2][1] == 0: s.last2 = s._done(2)
        if sl[5][0] is not None and sl[5][1] == 0 and OPNUM[sl[5][0].op] in FDIV_OPS: s._done(5)
        if sl[6][0] is not None and sl[6][1] == 0 and sl[7][0] is None:
            sl[7] = [sl[6][0], GKT[sl[6][0].op][4]]; sl[6] = [None, 0]
        if sl[5][0] is not None and sl[5][1] == 0 and sl[6][0] is None:
            sl[6] = [sl[5][0], GKT[sl[5][0].op][3]]; sl[5] = [None, 0]
        if sl[3][0] is not None and sl[3][1] == 0 and sl[4][0] is None:
            sl[4] = [sl[3][0], GKT[sl[3][0].op][3]]; sl[3] = [None, 0]

def latency(p):
    lat = GKT[p.op][1]
    if not (p.flags & 9) and (p.flags & 0x20000000):
        lat += 2
    if p.op in ('LMW', 'STMW'):
        lat += p.argc - 2
    return lat

def is_barrier(p):
    return bool(p.flags & 0x1000000) or bool(p.flags & 0x100) or GKT[p.op][5] != 0

# ---------------------------------------------------------------- DAG
class Node:
    def __init__(s, p, i):
        s.p, s.i = p, i
        s.lat = latency(p); s.height = s.lat
        s.succs = []            # [succ node, lat]
        s.npreds = 0; s.ready = 0; s.deadline = 0

def add_edge(n, m, kind):
    if n is m:
        return
    lat = n.lat if kind else 0
    for e in n.succs:
        if e[0] is m:
            if lat > e[1]: e[1] = lat
            n.height = max(n.height, e[1] + m.height)
            return
    e = [m, lat]
    n.succs.insert(0, e)
    m.npreds += 1
    n.height = max(n.height, lat + m.height)

def build_dag(pcodes):
    nodes = [Node(p, i) for i, p in enumerate(pcodes)]
    writers, readers = {}, {}
    stores, loads, ord80, barriers = [], [], [], []
    branch = None; maxh = 0
    later = []
    for n in reversed(nodes):
        p = n.p
        for a in p.args:
            if a.kind != 0:
                continue
            if a.cls == 4 and (a.reg == 2 or a.reg == 13 or (a.reg == 0 and not (a.rw & 3))):
                continue
            key = (a.cls, a.reg)
            k2 = 1 if a.cls == 0 else WAW_KIND     # 0x508390: WAR/WAW carry latency only for SPRs (model word +4 != 0)
            if a.rw & 2:
                for e in readers.get(key, ()): add_edge(n, e, 1)
                for e in writers.get(key, ()): add_edge(n, e, k2)
                writers.setdefault(key, []).append(n)
            else:
                for e in writers.get(key, ()): add_edge(n, e, k2)
                readers.setdefault(key, []).append(n)
        if p.flags & 0x20002:
            for e in stores:
                if may_alias(p.alias, e.p.alias): add_edge(n, e, 1)
            loads.append(n)
        elif p.flags & 0x40004:
            for e in loads:
                if may_alias(p.alias, e.p.alias): add_edge(n, e, 1)
            for e in stores:
                if may_alias(p.alias, e.p.alias): add_edge(n, e, 1)
            stores.append(n)
            if p.flags & 0x40000:
                loads.append(n)
        if p.flags & 0x80:
            for e in ord80: add_edge(n, e, 0)
            ord80.append(n)
        if is_barrier(p):
            for e in later: add_edge(n, e, 0)
            barriers.append(n)
        for e in barriers: add_edge(n, e, 0)
        if not n.succs and branch is not None:
            add_edge(n, branch, 0)
        maxh = max(maxh, n.height)
        if p.flags & 1:
            branch = n
        later.append(n)
    for n in nodes:
        n.deadline = maxh - n.height
    return nodes, maxh

# ---------------------------------------------------------------- list scheduling
def frees(n):
    return sum(1 for e in n.succs if e[0].npreds == 1)

def select(cands, cycle, model, pre_ra):
    best = None
    for n in cands:
        if n.npreds or n.ready > cycle or not model.can_issue(n.p):
            continue
        if best is None:
            best = n; continue
        bu, cu = best.deadline <= cycle, n.deadline <= cycle
        if bu and not cu: continue
        if not bu and cu: best = n; continue
        fb, fc = frees(best), frees(n)
        if fb > fc: continue
        if fb < fc: best = n; continue
        if best.height > n.height: continue
        if best.height < n.height: best = n; continue
        if pre_ra and B9[n.p.op] < B9[best.p.op]:
            best = n
    return best

def schedule_block(pcodes, pre_ra, verbose=False):
    nodes, maxh = build_dag(pcodes)
    model = Gekko(); model.reset()
    todo = list(nodes); out = []; cycle = 0
    while todo:
        for _ in range(WIDTH):
            n = select(todo, cycle, model, pre_ra)
            if n is None:
                break
            for m, lat in n.succs:
                m.npreds -= 1
                m.ready = max(m.ready, cycle + lat)
            out.append(n); model.issue(n.p); todo.remove(n)
            if verbose:
                print('  c%-3d h=%-2d dl=%-2d  %s' % (cycle, n.height, n.deadline, n.p.text[11:80]))
        model.advance(); cycle += 1
    return [n.p for n in out], nodes

def run(dumpdir, post=False, only=None, verbose=False):
    pre = parse(os.path.join(dumpdir, 'sched-pre2.txt' if post else 'sched-pre1.txt'))
    got = parse(os.path.join(dumpdir, 'sched-post2.txt' if post else 'sched-post1.txt'))
    got = {b.idx: [p.addr for p in b.pcodes] for b in got}
    ok = tot = 0
    for b in pre:
        if b.count <= 2 or (b.flags & 8) or (not post and (b.flags & 3)):
            if got.get(b.idx) != [p.addr for p in b.pcodes]:
                print('B%d: NOT scheduled by the model but changed by the compiler?!' % b.idx)
            continue
        if only is not None and b.idx != only:
            continue
        order, nodes = schedule_block(b.pcodes, not post, verbose)
        mine = [p.addr for p in order]
        tot += 1
        same = mine == got[b.idx]
        ok += same
        if not same or verbose:
            gi = {a: i for i, a in enumerate(got[b.idx])}
            first = next((i for i, a in enumerate(mine) if gi[a] != i), None)
            print('B%d (%d pcodes): %s%s' % (b.idx, b.count, 'IDENTICAL' if same else 'DIFF', '' if same else ' first at slot %d' % first))
            if not same:
                byaddr = {p.addr: p for p in b.pcodes}
                for i in range(len(mine)):
                    a, g = mine[i], got[b.idx][i]
                    mark = ' ' if a == g else '*'
                    print('  %2d %s %-40s | %s' % (i, mark, byaddr[a].text[11:50], byaddr[g].text[11:50]))
    print('%s: %d/%d scheduled blocks identical (%s)' % (dumpdir, ok, tot, 'post-RA' if post else 'pre-RA'))
    return ok, tot

if __name__ == '__main__':
    a = sys.argv[1:]
    d = a[0]; post = '--post' in a; verbose = '--verbose' in a
    only = int(a[a.index('--block') + 1]) if '--block' in a else None
    run(d, post, only, verbose)
