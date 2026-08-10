#!/usr/bin/env python3
"""xjump.py -- model of jump.c's jump2 cross-jumping (GCC 2.95.3 / SN cc1plus), the part that decides
which identical arm tail survives (COMPILER-DIFF #6 territory).

Input: the function's RTL layout at jump2 entry (after sched2) as a flat list of items, in insn order:
    ('L', name)            code label
    ('i', text)            an insn; identical text = identical insn (rtx_renumbered_equal_p)
    ('c', text)            a CALL_INSN (text must match too)
    ('u', text)            a USE/CLOBBER insn: must match its counterpart but does not count toward `minimum`
                           ('u0' = flow.c's `(use (const_int 0))` nop after a call that ends a basic block;
                            'use r3' = the return-value use that survives a value-select `return r` join)
    ('b', label)           unconditional jump (simplejump); a barrier follows implicitly
    ('bc', label, cond)    conditional jump (falls through)
    ('ret',)               RETURN insn (leaf functions only)
Rules modelled (jump.c jump_optimize_1 with cross_jump, forward scan, repeated while changed):
  * jump to the following insn deleted; `bcc L1; b L2; L1:` -> `b!cc L2`; `bcc X` directly followed by `b X`
    -> the conditional jump deleted; jump to jump threaded.
  * for a simplejump `b L`: candidate 1 = the code falling into L (find_cross_jump(insn, L, minimum=1)),
    candidate 2.. = the other simplejumps to L in jump_chain order = LATEST in insn order first
    (mark_all_labels prepends; redirect_jump prepends to the new label's chain), minimum 2.
  * find_cross_jump walks both tails backwards: a CODE_LABEL before the scanned tail -> --minimum and stop;
    a mismatch at a conditional jump around the scanned jump (its label right after e1) -> --minimum;
    USE/CLOBBER insns must match but do not count.  Success iff minimum <= 0 and >= 1 real insn matched.
  * do_cross_jump deletes the SCANNED tail and redirects the scanned jump to a label put before the
    candidate's tail (an existing label there is reused); the jump is then re-examined (`next = insn`).
The returned layout shows the survivors: an arm whose tail is gone reads `b NEWLABEL`.

Validated (2026-09-10) against cc1plus -dR/-dJ dumps: db_widget AddPrimitive (ours: the labelled
`li r3,0; b END` copy merges through the chain into the else arm's copy; target/value-select form: the
`use r3` left before END blocks it), DB_NUMERIC2::OnCalcMsg (MIN -> MAX 3-insn chain merge, DEFAULT
protected by the flow nop), pl_class isKamae (`goto ng` form: the first copy survives, later copies
`bne` to it), item use (ours: everything collapses).  The jump2 POLICY itself is the original's: every
policy variant (fall-through minimum 2 / none, no label decrement, oldest-first chain, no jump-around
bonus, no USE move, no range swap, swap in round 1) regresses 400-3800 matched functions in a
whole-tree build and fixes none (see docs/research/compiler.md "COMPILER-DIFF #6 resolved").

Usage:  python3 tools/xjump.py FILE.py     FILE.py defines `items = [...]` (and optionally `trace = False`)
        python3 tools/xjump.py --selftest
"""
import itertools, sys

class Insn:
    __slots__ = ('kind', 'text', 'label', 'cond', 'uid', 'deleted')
    def __init__(self, kind, text=None, label=None, cond=None):
        self.kind, self.text, self.label, self.cond = kind, text, label, cond
        self.deleted = False
    def __repr__(self):
        if self.kind == 'L': return f'{self.text}:'
        if self.kind == 'b': return f'  b {self.label}'
        if self.kind == 'bc': return f'  b{self.cond} {self.label}'
        if self.kind == 'ret': return '  blr'
        return f'  {self.text}'

def parse(items):
    out = []
    for it in items:
        k = it[0]
        if k == 'L': out.append(Insn('L', text=it[1]))
        elif k in ('i', 'c', 'u'): out.append(Insn(k, text=it[1]))
        elif k == 'b': out.append(Insn('b', label=it[1]))
        elif k == 'bc': out.append(Insn('bc', label=it[1], cond=it[2] if len(it) > 2 else 'eq'))
        elif k == 'ret': out.append(Insn('ret'))
        else: raise ValueError(it)
    return out

def real(i): return i.kind in ('i', 'c', 'u', 'b', 'bc', 'ret')
def active(i): return i.kind in ('i', 'c', 'b', 'bc', 'ret')   # after reload USE/CLOBBER are not active

class Fn:
    def __init__(self, insns):
        self.insns = [i for i in insns]
        self.nlabel = 0
        self.chain = {}   # label name -> list of jumps, head first
        self.trace = []
    # ---- list helpers
    def idx(self, i): return self.insns.index(i)
    def prev_nonnote(self, i):
        k = self.idx(i) - 1
        return self.insns[k] if k >= 0 else None
    def prev_real(self, i):
        k = self.idx(i) - 1
        while k >= 0 and not real(self.insns[k]): k -= 1
        return self.insns[k] if k >= 0 else None
    def prev_active(self, i):
        k = self.idx(i) - 1
        while k >= 0 and not active(self.insns[k]): k -= 1
        return self.insns[k] if k >= 0 else None
    def next_active(self, i):
        k = self.idx(i) + 1
        while k < len(self.insns) and not active(self.insns[k]): k += 1
        return self.insns[k] if k < len(self.insns) else None
    def next_real(self, i):
        k = self.idx(i) + 1
        while k < len(self.insns) and not real(self.insns[k]): k += 1
        return self.insns[k] if k < len(self.insns) else None
    def next_label(self, i):
        k = self.idx(i) + 1
        while k < len(self.insns) and self.insns[k].kind != 'L': k += 1
        return self.insns[k] if k < len(self.insns) else None
    def label(self, name):
        for i in self.insns:
            if i.kind == 'L' and i.text == name: return i
        raise KeyError(name)
    def uses(self, name):
        return sum(1 for i in self.insns if i.kind in ('b', 'bc') and i.label == name)
    def delete(self, i):
        self.insns.remove(i); i.deleted = True
    def barrier_after(self, i):
        # a simplejump is followed by a barrier: code up to the next label is dead (delete_barrier_successors)
        k = self.idx(i) + 1
        while k < len(self.insns) and self.insns[k].kind != 'L':
            self.insns[k].deleted = True; del self.insns[k]
    def mark_all_labels(self):
        self.chain = {}
        for i in self.insns:
            if i.kind == 'b': self.chain.setdefault(i.label, []).insert(0, i)
        self.chain['<ret>'] = [i for i in reversed(self.insns) if i.kind == 'ret']
    def redirect(self, jump, newlabel):
        old = jump.label
        if old in self.chain and jump in self.chain[old]: self.chain[old].remove(jump)
        jump.label = newlabel
        if jump.kind == 'b': self.chain.setdefault(newlabel, []).insert(0, jump)
        if old and self.uses(old) == 0:
            try: self.delete(self.label(old))
            except KeyError: pass
    def get_label_before(self, i):
        p = self.prev_nonnote(i)
        if p is not None and p.kind == 'L': return p.text
        self.nlabel += 1
        name = f'X{self.nlabel}'
        self.insns.insert(self.idx(i), Insn('L', text=name))
        return name
    # ---- find_cross_jump
    def find_cross_jump(self, e1, e2, minimum):
        """e1: the scanned jump; e2: a label (fall-through candidate) or another jump. Returns (f1, f2) or None."""
        i1, i2 = e1, e2
        last1 = last2 = None
        while True:
            # i1 = prev_nonnote_insn (i1): notes are not modelled -> previous item
            k1 = self.idx(i1) - 1
            i1 = self.insns[k1] if k1 >= 0 else None
            k2 = self.idx(i2) - 1
            while k2 >= 0 and self.insns[k2].kind == 'L': k2 -= 1
            i2 = self.insns[k2] if k2 >= 0 else None
            if i1 is None: break
            if i2 is e1 or i1 is e2: break
            if i1.kind == 'L':
                minimum -= 1
                break
            if i2 is None or (i1.kind == 'b') != (i2.kind == 'b') or (i1.kind == 'c') != (i2.kind == 'c') \
               or (i1.kind in ('bc', 'ret')) != (i2.kind in ('bc', 'ret')):
                break
            same = (i1.kind == i2.kind and i1.text == i2.text and i1.label == i2.label and i1.cond == i2.cond) \
                   or (i1.kind == 'u' and i2.kind == 'u' and i1.text == i2.text) \
                   or (i1.kind in ('i',) and i2.kind in ('i',) and i1.text == i2.text)
            if i1.kind == 'u' and i2.kind != 'u' or i2.kind == 'u' and i1.kind != 'u':
                same = False   # USE vs real insn: same GET_CODE (INSN) but different patterns
            if not same:
                # jump-around-jump: i1 is a condjump whose label is right after e1
                if i1.kind == 'bc':
                    nxt = self.next_real(e1)   # prev_real_insn (JUMP_LABEL (i1)) == e1 <=> label is right after e1
                    lab = self.label(i1.label) if any(x.kind == 'L' and x.text == i1.label for x in self.insns) else None
                    if lab is not None and self.prev_real(lab) is e1:
                        minimum -= 1
                break
            if i1.kind != 'u':
                last1, last2 = i1, i2
                minimum -= 1
        if minimum <= 0 and last1 is not None and last1 is not e1:
            return last1, last2
        return None
    def do_cross_jump(self, insn, f1, f2, why):
        label = self.get_label_before(f2)
        n = 0
        p = f1
        while p is not insn:
            q = self.next_real(p)
            self.delete(p); n += 1
            p = q
        old = insn.label
        self.trace.append(f'{why}: tail of `b {old}` at #{self.idx(insn)} ({n} insns) -> b {label}')
        if insn.kind == 'ret':
            insn.kind = 'b'
            self.chain['<ret>'].remove(insn)
            insn.label = label
            self.chain.setdefault(label, []).insert(0, insn)
        else:
            self.redirect(insn, label)
    # ---- main loop
    def run(self, cross_jump=True):
        self.mark_all_labels()
        # delete unreferenced labels
        for i in list(self.insns):
            if i.kind == 'L' and self.uses(i.text) == 0 and not i.text.startswith('KEEP'):
                self.delete(i)
        changed = True
        first = True
        while changed:
            changed = False
            pos = 0
            while pos < len(self.insns):
                insn = self.insns[pos]
                pos += 1
                if insn.kind not in ('b', 'bc', 'ret'): continue
                if insn.kind == 'ret':
                    if cross_jump:
                        for target in list(self.chain.get('<ret>', [])):
                            if target is insn or target.deleted: continue
                            r = self.find_cross_jump(insn, target, 2)
                            if r:
                                self.do_cross_jump(insn, r[0], r[1], 'return'); changed = True
                                pos = self.idx(insn); break
                    continue
                lab = self.label(insn.label)
                # jump to following insn
                if self.prev_active(lab) is insn:
                    self.trace.append(f'delete jump-to-following `b{insn.cond or ""} {insn.label}` at #{self.idx(insn)}')
                    if insn.kind == 'b':
                        self.redirect(insn, None); self.delete(insn)
                    else:
                        self.delete(insn)
                    if self.uses(lab.text) == 0: self.delete(lab)
                    changed = True; pos = min(pos, len(self.insns)); pos -= 1 if pos > 0 else 0
                    continue
                if insn.kind == 'bc':
                    # condjump to the same place as the immediately following simplejump
                    nxt = self.next_active(insn)
                    if nxt is not None and nxt.kind == 'b' and self.next_active(lab) is self.next_active(self.label(nxt.label)):
                        self.trace.append(f'delete `b{insn.cond} {insn.label}` before `b {nxt.label}` (same place)')
                        self.delete(insn); changed = True; pos -= 1; continue
                    # condjump over an unconditional jump: bcc L1; b L2; L1:  ->  b!cc L2
                    rlp = self.prev_active(lab)
                    if rlp is not None and rlp.kind == 'b' and self.prev_active(rlp) is insn:
                        self.trace.append(f'invert `b{insn.cond} {insn.label}; b {rlp.label}; {insn.label}:` -> b!{insn.cond} {rlp.label}')
                        insn.cond = {'eq':'ne','ne':'eq','gt':'le','le':'gt','lt':'ge','ge':'lt'}.get(insn.cond, 'n'+insn.cond)
                        old = insn.label
                        self.redirect(insn, rlp.label)
                        self.redirect(rlp, None); self.delete(rlp)
                        changed = True; pos = self.idx(insn); continue
                # jump to jump
                tgt = self.next_active(lab)
                seen = set()
                while tgt is not None and tgt.kind == 'b' and tgt.label not in seen:
                    seen.add(tgt.label); tgt = self.next_active(self.label(tgt.label))
                if seen:
                    newl = list(seen)[-1] if len(seen) == 1 else None
                    # follow_jumps: final label
                    l2 = lab
                    while True:
                        t = self.next_active(l2)
                        if t is not None and t.kind == 'b' and self.label(t.label) is not l2: l2 = self.label(t.label)
                        else: break
                    if l2 is not lab:
                        self.trace.append(f'thread `b{insn.cond or ""} {insn.label}` -> {l2.text}')
                        self.redirect(insn, l2.text); changed = True; pos = self.idx(insn); continue
                if not cross_jump or insn.kind != 'b': continue
                # cross jumping of unconditional jumps
                r = self.find_cross_jump(insn, self.label(insn.label), 1)
                why = 'fall-through'
                # jump.c: `if (INSN_UID (JUMP_LABEL (insn)) < max_uid)` -- the chain is searched only for
                # labels that existed at jump2 entry; a label do_cross_jump created (X<n>) is never chained
                if r is None and not insn.label.startswith('X'):
                    for target in list(self.chain.get(insn.label, [])):
                        if target is insn or target.deleted or target.label != insn.label: continue
                        r = self.find_cross_jump(insn, target, 2)
                        if r: why = f'chain (into `b` at #{self.idx(target)})'; break
                if r:
                    self.do_cross_jump(insn, r[0], r[1], why)
                    changed = True; pos = self.idx(insn)
            first = False
        return self

def show(items, trace=True):
    f = Fn(parse(items)).run()
    if trace:
        for t in f.trace: print('   ', t)
    for i in f.insns: print(i)
    return f

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] != '--selftest':
        ns = {}
        exec(open(sys.argv[1]).read(), ns)
        show(ns['items'], ns.get('trace', True))
        sys.exit(0)
    # self-test: AddPrimitive (ours, after the jump1 swap): the labelled `li r3,0; b END` copy merges into
    # the else arm's `li r3,0; b END` through the chain (label rule), the then-arm falls into END.
    END = 'END'
    print('== AddPrimitive, ours (else arm is a jump):')
    show([('i', 'cmpwi x'), ('bc', 'DT'), ('i', 'cmpwi y'), ('bc', 'L2', 'ne'), ('L', 'DT'), ('i', 'li r3,0'), ('b', END),
          ('L', 'L2'), ('c', 'bl ext'), ('i', 'cmpwi t'), ('bc', 'THEN'), ('i', 'li r3,0'), ('b', END),
          ('L', 'THEN'), ('i', 'stwx'), ('i', 'li r3,1'), ('i', 'stw'), ('L', END), ('u', 'use r3')])
    print('== AddPrimitive, target (value-select join leaves `use r3` before END):')
    show([('i', 'cmpwi x'), ('bc', 'DT'), ('i', 'cmpwi y'), ('bc', 'L2', 'ne'), ('L', 'DT'), ('i', 'li r3,0'), ('b', END),
          ('L', 'L2'), ('c', 'bl ext'), ('i', 'cmpwi t'), ('bc', 'ELSE', 'ne'), ('i', 'stwx'), ('i', 'li r3,1'), ('i', 'stw'), ('b', END),
          ('L', 'ELSE'), ('i', 'li r3,0'), ('u', 'use r3'), ('L', END), ('u', 'use r3')])
