#!/usr/bin/env python3
"""schedwhatif.py DUMPDIR BLOCK [--post] [edits...]  -- schedule one block of a sched dump with DAG edits.
edits: mv=<i>:<j> (move raw index i to position j), raw=<i,j,..> (those first, rest after), del=<i>, dup=<i>,
       ins=<i>:<text> (insert a P line before raw index i), reg=<i>:<argidx>:<cls>:<reg>:<rw>
Prints the resulting order, one line per pcode with cycle/height/deadline.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sched

def main():
    a = sys.argv[1:]
    d, blk = a[0], int(a[1]); post = '--post' in a
    edits = [x for x in a[2:] if not x.startswith('--')]
    pre = sched.parse(os.path.join(d, 'sched-pre2.txt' if post else 'sched-pre1.txt'))
    b = [x for x in pre if x.idx == blk][0]
    pc = list(b.pcodes)
    for e in edits:
        k, v = e.split('=', 1)
        if k == 'raw':
            idx = [int(x) for x in v.split(',')]
            rest = [p for i, p in enumerate(pc) if i not in idx]
            pc = [pc[i] for i in idx] + rest
        elif k == 'mv':
            i, j = [int(x) for x in v.split(':')]
            p = pc.pop(i); pc.insert(j, p)
        elif k == 'del':
            del pc[int(v)]
        elif k == 'dup':
            i = int(v); pc.append(pc[i])
        elif k == 'ins':
            i, text = v.split(':', 1)
            tmp = sched.parse_line if hasattr(sched, 'parse_line') else None
            # reuse parse() on a mini file
            import tempfile
            f = tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False)
            f.write('BLOCK 0 flags=0004 count=1 weight=1\n' + text + '\n'); f.close()
            np = sched.parse(f.name)[0].pcodes[0]; os.unlink(f.name)
            pc.insert(int(i), np)
        elif k == 'reg':
            i, ai, cls, reg, rw = [int(x, 0) for x in v.split(':')]
            import copy
            p = copy.copy(pc[i]); p.args = [copy.copy(x) for x in p.args]
            p.args[ai].cls, p.args[ai].reg, p.args[ai].rw = cls, reg, rw
            p.text = p.text + '  [reg edit %d]' % ai
            pc[i] = p
    order, nodes = sched.schedule_block(pc, not post, verbose=True)

main()
