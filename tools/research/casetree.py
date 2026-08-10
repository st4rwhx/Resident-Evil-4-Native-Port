#!/usr/bin/env python3
"""Model of the GCC 2.95.3 (SN ProDG) switch expander: stmt.c expand_end_case ->
group_case_nodes / balance_case_nodes / emit_case_nodes with the node_has_low_bound /
node_has_high_bound pruning and the estimate_case_costs cost mode, followed by the jump.c
clean-ups (jump1 and jump2) that reshape the emitted compare tree.

SN's stmt.c never emits a jump table (the CASE_VALUES_THRESHOLD test is `if (1)`), so every
switch is a compare tree.  Everything else in the case-tree code is stock 2.95.3 (diffed against
the NGC_GNU_SRC drop).  Every rule below was checked against cc1plus output (/tmp probes) and
against 836 switches of the Matching REL modules (tools/casetree.py final() == target run).

Case list
---------
`cases` is a list of (value, label) or ((lo, hi), label).  Label 'default' (alias 'D') means the
value is grouped with `default:` (it is still a node: it shapes the balance, its compares fold
into the default jump later).  Values NOT listed are handled by the out-of-tree default jump and
do not shape anything.  Consecutive values with the same label merge into one range node
(group_case_nodes merges on the label's first real insn: two labels whose bodies are both a
plain `break` (`b EXIT`) ARE merged, a `default:` whose body is a jump too; a label with a real
body is never merged with a neighbour of another label even if the bodies are identical --
`case 7: body; case 8: same body;` written as two arms gives two nodes).

Index type (`index=`)
---------------------
Sets TYPE_MIN_VALUE/TYPE_MAX_VALUE (bound pruning) and the compare mnemonic:
  'int'  (default)  -2^31..2^31-1, cmpwi.  Also what a u8/u16 member or any enum gives: the C++
                    front end promotes them to int and only keeps a narrow type when its
                    signedness equals int's (cp/typeck.c c_expand_start_case/get_unwidened).
  's8' / 's16' / 'char' (char IS signed on this target): -128..127 / -32768..32767, cmpwi; a
                    leaf with high 127 / low -128 needs no bound test (verified: `switch (s8)`
                    with case 126,127 has no `cmpwi 127; bgt`).
  'unsigned' / 'u32': 0..2^32-1; a leaf with low 0 has a low bound (no `cmplwi 0; blt`).  The
                    EQ compares of an unsigned index are still `cmpwi` (SELECT_CC_MODE gives
                    CCmode for EQ/NE, CCUNSmode only for the ordered codes), so an unsigned tree
                    reads `cmpwi 5; beq; cmplwi 5; ble` -- the two compares of one node are NOT
                    merged by cse (different modes).
  'enum': int bounds, cost mode OFF (expand_end_case tests TREE_TYPE (orig_index), the
                    un-promoted enum; verified: an all-printable enum switch is balanced plain).

Cost mode (`cost=`)
-------------------
None (default) = decide like stmt.c: use_cost_table is on when the index type is not an enum AND
estimate_case_costs accepts the grouped list: every value in -1..127 and none of them a control
character other than \\0 \\b \\t \\n \\v \\f (0x1..0x7, 0xE..0x1F, 0x7F disable it -- every damage
switch with 0xD or 1..7 is plain).  In cost mode balance_case_nodes bisects the summed
cost_table weights (alnum 16, punct/space 8, \\0 and \\t 4, \\n 2, \\b \\v \\f 1, -1 -> 0) instead
of 1 per node / 2 per range, and a list whose FIRST node already reaches half the cost is left
LOPSIDED: root = first node, no left child, right chain LINEAR (no recursion) -- verified with
`case 'A'..'D': / case 'a': / case 'x':` -> `cmpwi 65 blt; cmpwi 68 ble; cmpwi 97 beq; cmpwi 120
beq`.  True/False force it.  A game switch is in cost mode when all its values are 0, 8..0xC or
0x20..0x7E (e.g. `switch (no)` over 0x13,0x16,0x17,0x19,0x1F,0x20 is NOT: 0x13/0x16/0x17/0x19/
0x1F are control chars).

Balance (plain mode), for reference: n nodes, r ranges, root = the node where a countdown from
(n + r + 1) / 2 (2 per range, 1 per single) reaches <= 0; exactly 3 nodes -> middle; 1-2 nodes
stay a linear right chain.  A list of >= 4 nodes can never make its FIRST node the root
((n+r+1)/2 > 2 whenever n >= 4), so a right-only range node with a balanced right subtree
(em31DmCk's `[18-28] -> [2b-2c]{[29-2a],[2d]}`) is not a stmt.c shape in either mode.

Output
------
tree(cases, ...)     raw stmt.c emission: [(value, cond, label) | ('jmp', label)], cond in
                     EQ GT LT GE LE (conds as written by emit_case_nodes, before jump.c).
compares(cases, ...) [(value, cond)] of the raw emission (old interface, unchanged).
final(cases, layout=[...], ...)  the branch sequence AFTER jump1+jump2: list of
                     ('cmpwi'|'cmplwi', value) and ('beq'|'bne'|'bgt'|'ble'|'blt'|'bge'|'b', target)
                     with targets resolved to arm names ('EXIT' for default/break arms); a
                     ('cmpwi', v) with no branch after it is a dead compare (see jump2 below).
                     Stops at the first arm body.
   layout      = the arm labels in source order (the order their bodies follow the tree); the
                 range swap and the jump-over-jump inversions depend on it.  Arms not listed are
                 appended in first-use order.  'EXIT' inside the layout means "the code after the
                 switch comes here"; arms listed after it are laid out behind it (bodies ending in
                 `return` that jump.c/loop.c relocated, em24DmCk) and end with a jump to the
                 function end.
   empty       = labels whose body is only `break` (default: {'default'}).
   fallthrough = labels whose body falls into the next arm.
   goto        = {label: other} arms whose body is only `goto other;` (`default: goto normal;`
                 written FIRST puts the default label right behind the tree: em36BloodSet).
   merge       = {label: survivor} arms whose identical bodies jump2 cross-jumps into one copy
                 (the survivor is normally the LAST identical arm in layout order; 'X+' = the
                 copies jump into X's body behind a private first insn of X, e.g. its own dead
                 compare -- em28DmCk `b .L_240` past A3's dead `fcmpu`).
rtl(cases, ...)      the same but as the insn list (for debugging: --rtl).
match(target, seq)   True when the target branch list equals seq up to label renaming.
extract_target(asm_path, func, reg=None, start=None)  parse a dtk .s: the longest run of
                     `cmpwi/cmplwi rN, K` + branches (start = '.L_xxx' or a hex offset selects
                     the run; up to 6 scheduled insns between a compare and its branch are
                     skipped).  extract_all(asm_path, func) = every run.  extract_ours(unit, func,
                     ..) = the same from our object via objdiff-cli (like tools/fdiff.py).
search(fixed, blocks, labels, target, ...)  enumerate label assignments for value blocks and print
                     those whose tree (raw compares subsequence, or final() equality with
                     `layout`) matches the target.

jump.c effects modelled (tools/sn-gcc/src/gcc/jump.c jump_optimize, in its order):
  jump1 (before flow; a deleted conditional jump takes its compare with it):
  - jump to the following insn deleted (`b L; L:`),
  - conditional jump to the same place as the immediately following unconditional jump deleted
    (`beq D; b D`) -- this is what folds the compares of default-labelled and break-labelled
    leaves, and of a `default: goto X` label's leaves,
  - conditional jump over an unconditional jump inverted (`bgt L1; b L2; L1:` -> `ble L2`),
  - jump to a jump threaded (`default: break;` labels, `case X: break;` arms -> EXIT),
  - the `if (foo) bar; else break;` range swap (not on the first round): `bcc L1; R1; b L2; L1:
    R2; b X; L2:` with L1 used once -> `b!cc L1; R2; b X; L1: R1; b L2` -- the "right child laid
    out before the left" shape (em29DmCk's 0x29 subtree, em31's high half).  L1 can be a case
    label too (`beq A; ..; b B; A: body; b END; B:` -> `bne A'; body; b END; A': ..; B:`).
  jump2 (after reload, with -fschedule-insns2): delete_computation deletes only the jump, so a
  conditional jump that becomes redundant HERE leaves its compare behind: `cmpwi 0; b D`
  (em28DmCk), `lbz dmWep; cmpwi 0x21` (em29DmCk hp>0 arm, the "em30 family").  Redundancy
  comes from cross-jumping: identical arm bodies collapse into `b survivor`, then `beq X; b X`
  and `bne Q; b Q` lose their branch.  Also modelled: find_cross_jump matching only the final
  branch of two nodes (`cmpwi 1; beq A; b EXIT; T: cmpwi 7; beq A` -> `cmpwi 1; b L; T: cmpwi 7;
  L: beq A`, em24DmCk with the arm body moved away).  NOT modelled: which identical copy
  survives and partial tail merges (COMPILER-DIFF 6 territory; give them via `merge`).

CLI:
  python3 tools/casetree.py 0-4:D 5:X 7-8:Y 0xd:X 0x17:H  [--index u8] [--cost auto|on|off]
      [--layout X,Y,H] [--empty D] [--fallthrough X] [--goto D=Y] [--merge X=Y,Z=Y+] [--raw] [--rtl]
      [--target build/G4BE08/<mod>/asm/<mod>/<unit>.s:<func>[:rN[:START]]]
      [--ours <mod>/<unit>:<func>[:rN[:START]]]
  ('D' as a label is an alias of 'default'.)
"""
import itertools
import re
import sys

INT_MIN, INT_MAX = -2**31, 2**31 - 1

INDEX_TYPES = {
    'int': (INT_MIN, INT_MAX, False),
    's32': (INT_MIN, INT_MAX, False),
    'u8': (INT_MIN, INT_MAX, False),      # promoted to int by the front end
    'u16': (INT_MIN, INT_MAX, False),
    'enum': (INT_MIN, INT_MAX, False),
    's8': (-128, 127, False),
    'char': (-128, 127, False),
    's16': (-32768, 32767, False),
    'unsigned': (0, 2**32 - 1, True),
    'u32': (0, 2**32 - 1, True),
}


def _cost_table():
    t = {-1: 0}
    for i in range(128):
        c = chr(i)
        if c.isalnum():
            t[i] = 16
        elif 0x21 <= i <= 0x7E and not c.isalnum():
            t[i] = 8          # ISPUNCT
        elif i < 0x20 or i == 0x7F:
            t[i] = -1         # ISCNTRL
        else:
            t[i] = 0
    t[0x20] = 8
    t[0x09] = 4
    t[0x00] = 4
    t[0x0A] = 2
    t[0x0C] = 1
    t[0x0B] = 1
    t[0x08] = 1
    return t


COST_TABLE = _cost_table()


class Node:
    def __init__(self, lo, hi, label):
        self.low, self.high, self.label = lo, hi, label
        self.left = self.right = self.parent = None

    def __repr__(self):
        r = hex(self.low) if self.low == self.high else '%s-%s' % (hex(self.low), hex(self.high))
        return '[%s:%s]' % (r, self.label)


def _norm_label(l):
    return 'default' if l in ('D', 'default', 'DEFAULT') else l


def build(cases):
    """Sorted, grouped node list (group_case_nodes). cases: (v, label) or ((lo, hi), label)."""
    items = []
    for v, l in cases:
        if isinstance(v, tuple):
            items.append((v[0], v[1], _norm_label(l)))
        else:
            items.append((v, v, _norm_label(l)))
    items.sort()
    nodes = [Node(lo, hi, l) for lo, hi, l in items]
    out = []
    i = 0
    while i < len(nodes):
        n = nodes[i]
        j = i + 1
        while j < len(nodes) and nodes[j].label == n.label and nodes[j].low == n.high + 1:
            n.high = nodes[j].high
            j += 1
        out.append(n)
        i = j
    return out


def estimate_case_costs(lst):
    for n in lst:
        if n.low < -1 or n.high > 127:
            return False
        for i in range(n.low, n.high + 1):
            if COST_TABLE[i] < 0:
                return False
    return True


def balance(lst, parent, use_cost=False):
    """balance_case_nodes on a python list (the 'right' chain). Returns the root."""
    if not lst:
        return None
    n = len(lst)
    ranges = 0
    cost = 0
    for x in lst:
        if x.low != x.high:
            ranges += 1
            if use_cost:
                cost += COST_TABLE[x.high]
        if use_cost:
            cost += COST_TABLE[x.low]
    if n > 2:
        if use_cost:
            i = (cost + 1) // 2
            k = 0
            while True:
                if lst[k].low != lst[k].high:
                    i -= COST_TABLE[lst[k].high]
                i -= COST_TABLE[lst[k].low]
                if i <= 0:
                    break
                k += 1
            if k == 0:
                # lopsided: root = first node, left empty, right chain linear
                root = lst[0]
                root.parent = parent
                cur = root
                for x in lst[1:]:
                    cur.right = x
                    x.parent = cur
                    cur = x
                return root
        elif n == 3:
            k = 1
        else:
            i = (n + ranges + 1) // 2
            k = 0
            while True:
                if lst[k].low != lst[k].high:
                    i -= 1
                i -= 1
                if i <= 0:
                    break
                k += 1
        root = lst[k]
        root.parent = parent
        root.left = balance(lst[:k], root, use_cost)
        root.right = balance(lst[k + 1:], root, use_cost)
        return root
    root = lst[0]
    root.parent = parent
    cur = root
    for x in lst[1:]:
        cur.right = x
        x.parent = cur
        cur = x
    return root


class Emitter:
    def __init__(self, tmin, tmax, unsigned):
        self.tmin, self.tmax, self.unsigned = tmin, tmax, unsigned
        self.out = []
        self.ntest = 0

    # node_has_low_bound / node_has_high_bound
    def has_low(self, node):
        if node.low == self.tmin:
            return True
        if node.left:
            return False
        lm1 = node.low - 1
        if lm1 < self.tmin:     # subtraction overflowed in the index type
            return False
        p = node.parent
        while p:
            if p.high == lm1:
                return True
            p = p.parent
        return False

    def has_high(self, node):
        if node.high == self.tmax:
            return True
        if node.right:
            return False
        hp1 = node.high + 1
        if hp1 > self.tmax:
            return False
        p = node.parent
        while p:
            if p.low == hp1:
                return True
            p = p.parent
        return False

    def bounded(self, node):
        return self.has_low(node) and self.has_high(node)

    def cmp(self, value, cond, label):
        self.out.append((value, cond, label))

    def jmp(self, label):
        self.out.append(('jmp', label))

    def jmp_if_reachable(self, label):
        if not (self.out and self.out[-1][0] == 'jmp'):
            self.jmp(label)

    def test_label(self):
        self.ntest += 1
        return '.T%d' % self.ntest

    def label(self, name):
        self.out.append(('label', name))

    def emit(self, node):
        D = 'default'
        if self.bounded(node):
            self.jmp(node.label)
        elif node.low == node.high:
            self.cmp(node.low, 'EQ', node.label)
            if node.right and node.left:
                if self.bounded(node.right):
                    self.cmp(node.high, 'GT', node.right.label)
                    self.emit(node.left)
                elif self.bounded(node.left):
                    self.cmp(node.high, 'LT', node.left.label)
                    self.emit(node.right)
                else:
                    t = self.test_label()
                    self.cmp(node.high, 'GT', t)
                    self.emit(node.left)
                    self.jmp_if_reachable(D)
                    self.label(t)
                    self.emit(node.right)
            elif node.right:
                r = node.right
                if r.right or r.left or r.low != r.high:
                    if not self.has_low(node):
                        self.cmp(node.high, 'LT', D)
                    self.emit(r)
                else:
                    self.cmp(r.low, 'EQ', r.label)
            elif node.left:
                l = node.left
                if l.left or l.right or l.low != l.high:
                    if not self.has_high(node):
                        self.cmp(node.high, 'GT', D)
                    self.emit(l)
                else:
                    self.cmp(l.low, 'EQ', l.label)
        else:
            if node.right and node.left:
                t = None
                if self.bounded(node.right):
                    self.cmp(node.high, 'GT', node.right.label)
                else:
                    t = self.test_label()
                    self.cmp(node.high, 'GT', t)
                self.cmp(node.low, 'GE', node.label)
                self.emit(node.left)
                if t:
                    self.jmp_if_reachable(D)
                    self.label(t)
                    self.emit(node.right)
            elif node.right:
                if not self.has_low(node):
                    self.cmp(node.low, 'LT', D)
                self.cmp(node.high, 'LE', node.label)
                self.emit(node.right)
            elif node.left:
                if not self.has_high(node):
                    self.cmp(node.high, 'GT', D)
                self.cmp(node.low, 'GE', node.label)
                self.emit(node.left)
            else:
                if not self.has_high(node):
                    self.cmp(node.high, 'GT', D)
                if not self.has_low(node):
                    self.cmp(node.low, 'LT', D)
                self.jmp(node.label)


# --- compatibility with the first version (int index, no jump.c): has_low/has_high/bounded/emit
_INT_EMITTER = Emitter(INT_MIN, INT_MAX, False)


def has_low(node):
    return _INT_EMITTER.has_low(node)


def has_high(node):
    return _INT_EMITTER.has_high(node)


def bounded(node):
    return _INT_EMITTER.bounded(node)


def emit(node, out):
    em = Emitter(INT_MIN, INT_MAX, False)
    em.emit(node)
    out.extend(e for e in em.out if e[0] != 'label')


def _params(index, cost, lst):
    tmin, tmax, unsigned = INDEX_TYPES[index]
    if cost is None:
        use_cost = index != 'enum' and estimate_case_costs(lst)
    else:
        use_cost = bool(cost)
    return tmin, tmax, unsigned, use_cost


def root_node(cases, index='int', cost=None):
    lst = build(cases)
    tmin, tmax, unsigned, use_cost = _params(index, cost, lst)
    return balance(lst, None, use_cost), use_cost


def emit_raw(cases, index='int', cost=None):
    """stmt.c output including ('label', T) entries for the test labels."""
    lst = build(cases)
    tmin, tmax, unsigned, use_cost = _params(index, cost, lst)
    root = balance(lst, None, use_cost)
    em = Emitter(tmin, tmax, unsigned)
    if root:
        em.emit(root)
    em.jmp_if_reachable('default')
    return em.out, unsigned


def tree(cases, index='int', cost=None):
    """Raw emission without the label markers (the historical interface)."""
    out, _ = emit_raw(cases, index, cost)
    return [e for e in out if e[0] != 'label']


def compares(cases, index='int', cost=None):
    return [(v, c) for v, c, *_ in tree(cases, index, cost) if v != 'jmp']


def dump_tree(cases, index='int', cost=None):
    root, use_cost = root_node(cases, index, cost)
    lines = ['cost mode: %s' % ('on' if use_cost else 'off')]

    def rec(n, depth):
        if not n:
            return
        rec(n.left, depth + 1)
        lines.append('  ' * depth + repr(n))
        rec(n.right, depth + 1)
    rec(root, 0)
    return '\n'.join(lines)


# ---------------------------------------------------------------- jump.c simulation

INVERT = {'EQ': 'NE', 'NE': 'EQ', 'GT': 'LE', 'LE': 'GT', 'LT': 'GE', 'GE': 'LT'}
MNEMONIC = {'EQ': 'beq', 'NE': 'bne', 'GT': 'bgt', 'LE': 'ble', 'LT': 'blt', 'GE': 'bge'}


class Insn:
    __slots__ = ('kind', 'cond', 'value', 'label', 'name', 'deleted')

    def __init__(self, kind, cond=None, value=None, label=None, name=None):
        self.kind, self.cond, self.value, self.label, self.name = kind, cond, value, label, name
        self.deleted = False

    def active(self):
        return self.kind in ('cjmp', 'jmp', 'body', 'deadcmp', 'br')

    def __repr__(self):
        if self.kind == 'cjmp':
            return 'cmp %s %s -> %s' % (hex(self.value), self.cond, self.label)
        if self.kind == 'jmp':
            return 'b %s' % self.label
        if self.kind == 'label':
            return '%s:' % self.name
        if self.kind == 'body':
            return '<%s>' % self.name
        if self.kind == 'deadcmp':
            return 'cmp %s (dead)' % hex(self.value)
        if self.kind == 'br':
            return '%s -> %s' % (self.cond, self.label)
        return self.kind


def layout_rtl(raw, layout, empty=('default',), fallthrough=(), goto=None):
    """Tree insns followed by the arm bodies in `layout` order, EXIT last.
    goto = {label: other_label} for arms whose body is only `goto other;` (e.g. `default: goto
    normal;` written first: the default label then sits right after the tree)."""
    L = []
    for e in raw:
        if e[0] == 'label':
            L.append(Insn('label', name=e[1]))
        elif e[0] == 'jmp':
            L.append(Insn('jmp', label=e[1]))
            L.append(Insn('barrier'))
        else:
            L.append(Insn('cjmp', cond=e[1], value=e[0], label=e[2]))
    empty = set(_norm_label(x) for x in empty)
    fallthrough = set(_norm_label(x) for x in fallthrough)
    goto = {_norm_label(a): _norm_label(b) for a, b in (goto or {}).items()}
    layout = [_norm_label(x) for x in layout]
    for e in raw:      # arms not named in layout follow the named ones, in first-use order
        lab = e[1] if e[0] == 'jmp' else (e[2] if e[0] != 'label' else None)
        if lab and lab != 'default' and not lab.startswith('.T') and lab not in layout:
            layout.append(lab)
    far = []
    if 'EXIT' in layout:
        far = layout[layout.index('EXIT') + 1:]
        layout = layout[:layout.index('EXIT')]
    for name in layout:
        L.append(Insn('label', name=name))
        if name in goto:
            L.append(Insn('jmp', label=goto[name]))
            L.append(Insn('barrier'))
            continue
        if name not in empty:
            L.append(Insn('body', name=name))
        if name not in fallthrough:
            L.append(Insn('jmp', label='EXIT'))
            L.append(Insn('barrier'))
    if 'default' not in layout and 'default' not in far:
        L.append(Insn('label', name='default'))
    L.append(Insn('label', name='EXIT'))
    L.append(Insn('body', name='EXIT'))
    if far:
        # arms moved behind the code that follows the switch (bodies ending in `return`
        # that loop.c/jump.c relocated): they end with a jump to the function end
        L.append(Insn('jmp', label='END'))
        L.append(Insn('barrier'))
        for name in far:
            L.append(Insn('label', name=name))
            if name not in empty:
                L.append(Insn('body', name=name))
            L.append(Insn('jmp', label='END'))
            L.append(Insn('barrier'))
        L.append(Insn('label', name='END'))
        L.append(Insn('body', name='END'))
    return L


class JumpOpt:
    def __init__(self, insns, phase2=False):
        self.L = [i for i in insns if not i.deleted]
        # jump2 runs after reload with -fschedule-insns2: delete_computation then deletes only
        # the jump, the compare stays as a dead `cmpwi` (em28DmCk `cmpwi 0; b D`, the em30
        # family's dead `lbz; cmpwi 0x21`).  Before flow the dead compare is removed.
        self.phase2 = phase2

    # --- navigation
    def idx_label(self, name):
        for k, i in enumerate(self.L):
            if i.kind == 'label' and i.name == name:
                return k
        return None

    def next_active(self, k):
        k += 1
        while k < len(self.L):
            if self.L[k].active():
                return k
            k += 1
        return None

    def prev_active(self, k):
        k -= 1
        while k >= 0:
            if self.L[k].active():
                return k
            k -= 1
        return None

    def next_label(self, k):
        k += 1
        while k < len(self.L):
            if self.L[k].kind == 'label':
                return k
            k += 1
        return None

    def nuses(self, name):
        return sum(1 for i in self.L if i.kind in ('cjmp', 'jmp') and i.label == name)

    def is_jmp(self, k):
        return k is not None and self.L[k].kind == 'jmp'

    # --- mutation (delete_insn semantics)
    def delete(self, k):
        insn = self.L[k]
        if insn.deleted:
            return
        insn.deleted = True
        if insn.kind == 'cjmp' and self.phase2:
            self.L.insert(k, Insn('deadcmp', value=insn.value))
            k += 1
        if insn.kind == 'jmp':
            n = k + 1
            while n < len(self.L) and self.L[n].deleted:
                n += 1
            if n < len(self.L) and self.L[n].kind == 'barrier':
                self.L[n].deleted = True
        if insn.kind in ('jmp', 'cjmp'):
            lab = insn.label
            if self.nuses_live(lab) == 0:
                li = self.idx_label(lab)
                if li is not None:
                    self.delete(li)
        if insn.kind == 'label':
            # delete following insns if now unreachable (prev non-deleted is a barrier)
            p = k - 1
            while p >= 0 and self.L[p].deleted:
                p -= 1
            if p >= 0 and self.L[p].kind == 'barrier':
                n = k + 1
                while n < len(self.L):
                    x = self.L[n]
                    if x.deleted or (x.kind == 'label' and x.deleted):
                        n += 1
                        continue
                    if x.kind == 'label':
                        break
                    if x.name == 'EXIT' and x.kind == 'body':
                        break
                    self.delete(n)
                    n += 1

    def nuses_live(self, name):
        return sum(1 for i in self.L if not i.deleted and i.kind in ('cjmp', 'jmp') and i.label == name)

    def compact(self):
        self.L = [i for i in self.L if not i.deleted]

    def redirect(self, k, nlabel):
        old = self.L[k].label
        if old == nlabel:
            return False
        self.L[k].label = nlabel
        if self.nuses_live(old) == 0:
            li = self.idx_label(old)
            if li is not None:
                self.delete(li)
        return True

    def follow_jumps(self, name):
        value = name
        for depth in range(10):
            li = self.idx_label(value)
            if li is None:
                return value
            na = self.next_active(li)
            if na is None or not self.is_jmp(na):
                return value
            if self.L[na].label == name:
                return name
            value = self.L[na].label
        return name

    def run(self):
        first = True
        for _round in range(50):
            changed = False
            k = 0
            while k < len(self.L):
                insn = self.L[k]
                if insn.deleted or insn.kind not in ('cjmp', 'jmp'):
                    k += 1
                    continue
                li = self.idx_label(insn.label)
                if li is None:
                    k += 1
                    continue
                reallabelprev = self.prev_active(li)
                # jump to following insn
                if reallabelprev == k:
                    self.delete(k)
                    self.compact()
                    changed = True
                    continue
                if insn.kind == 'cjmp':
                    t = self.next_active(k)
                    # cond jump to the same place as the immediately following uncond jump
                    if self.is_jmp(t) and self.next_active(li) == self.next_active(self.idx_label(self.L[t].label)):
                        self.delete(k)
                        self.compact()
                        changed = True
                        continue
                    # cond jump over an uncond jump
                    if (self.is_jmp(reallabelprev) and self.prev_active(reallabelprev) == k
                            and not any(x.kind == 'label' for x in self.L[k + 1:reallabelprev])):
                        old = insn.label
                        insn.cond = INVERT[insn.cond]
                        insn.label = self.L[reallabelprev].label
                        self.L[reallabelprev].deleted = True
                        n = reallabelprev + 1
                        if n < len(self.L) and self.L[n].kind == 'barrier':
                            self.L[n].deleted = True
                        if self.nuses_live(old) == 0:
                            oi = self.idx_label(old)
                            if oi is not None:
                                self.L[oi].deleted = True
                        self.compact()
                        changed = True
                        continue    # re-examine the inverted insn
                # jump to a jump
                nlabel = self.follow_jumps(insn.label)
                if nlabel != insn.label and self.redirect(k, nlabel):
                    self.compact()
                    changed = True
                # if (foo) bar; else break;  range swap
                if not first and insn.kind == 'cjmp':
                    l1 = self.next_label(k)
                    if l1 is not None and self.L[l1].name == insn.label and self.nuses(insn.label) == 1:
                        r1end = self.prev_active(l1)
                        if self.is_jmp(r1end) and r1end > k:
                            l2 = self.next_label(l1)
                            if l2 is not None:
                                r2end = self.prev_active(l2)
                                if (r2end is not None and r2end != r1end and r2end > l1
                                        and self.L[r1end].label == self.L[l2].name
                                        and self.is_jmp(r2end)):
                                    insn.cond = INVERT[insn.cond]
                                    insn.label = self.L[l1].name
                                    range1 = self.L[k + 1:r1end + 1]
                                    bar1 = self.L[r1end + 1:l1]
                                    range2 = self.L[l1 + 1:r2end + 1]
                                    bar2 = self.L[r2end + 1:l2]
                                    self.L = (self.L[:k + 1] + range2 + bar1 + [self.L[l1]] + range1 + bar2
                                              + self.L[l2:])
                                    changed = True
                                    # jump.c resumes at the insn that followed the branch before
                                    # the swap (range1's first insn, now after label1)
                                    k = k + 1 + len(range2) + len(bar1) + 1
                                    continue
                k += 1
            if not changed and self.phase2 and self.cross_jump_branches():
                changed = True
            if not changed:
                break
            first = False
        return self.L

    def cross_jump_branches(self):
        """jump2 find_cross_jump on a `b X`: the insn before it and the insn before label X
        (labels skipped) are identical conditional branches -> the first is deleted and the
        `b X` retargeted to a label in front of the second (`cmpwi 1; b L; T: cmpwi 7; L: beq A`).
        Only the one-insn (branch) match is modelled (the compares differ by construction)."""
        for k, insn in enumerate(self.L):
            if insn.kind != 'jmp' or insn.deleted:
                continue
            i1 = self.prev_active(k)
            li = self.idx_label(insn.label)
            if i1 is None or li is None or self.L[i1].kind != 'cjmp':
                continue
            i2 = li - 1
            while i2 >= 0 and self.L[i2].kind in ('label', 'barrier'):
                i2 -= 1
            if i2 < 0 or i2 == k or i2 == i1 or self.L[i2].kind != 'cjmp':
                continue
            a, b = self.L[i1], self.L[i2]
            if a.cond != b.cond or a.label != b.label:
                continue
            # i1: keep the compare (dead), drop its branch; i2: split into compare, label, branch
            name = '.X%d' % k
            self.L[i1] = Insn('deadcmp', value=a.value)
            insn.label = name
            self.L[i2:i2 + 1] = [Insn('deadcmp', value=b.value), Insn('label', name=name),
                                 Insn('br', cond=b.cond, label=b.label)]
            return True
        return False


def rtl(cases, layout, index='int', cost=None, empty=('default',), fallthrough=(), post=True, merge=None,
        goto=None):
    raw, unsigned = emit_raw(cases, index, cost)
    L = layout_rtl(raw, layout, empty, fallthrough, goto)
    if post:
        L = JumpOpt(L).run()
        # jump2 (after reload): cross-jumping has made the bodies in `merge` one copy, and the
        # jump.c loop runs once more with dead compares left behind
        merge = {_norm_label(a): _norm_label(b) for a, b in (merge or {}).items()}
        if True:
            # survivor 'X+' = the merged copies jump into X's body after a private prefix of X
            # (a dead compare of its own, a different first insn): X: <prefix> X+: <body>
            L2 = []
            k = 0
            plus = set(v for v in merge.values() if v.endswith('+'))
            while k < len(L):
                i = L[k]
                if i.kind == 'body' and i.name + '+' in plus:
                    L2.append(Insn('body', name=i.name + '.prefix'))
                    L2.append(Insn('label', name=i.name + '+'))
                    L2.append(i)
                    k += 1
                    continue
                if i.kind == 'body' and i.name in merge:
                    # the whole body is cross-jumped into the survivor: it becomes `b survivor`
                    L2.append(Insn('jmp', label=merge[i.name]))
                    L2.append(Insn('barrier'))
                    k += 1
                    while k < len(L) and L[k].kind in ('jmp', 'barrier'):
                        k += 1
                    continue
                L2.append(i)
                k += 1
            L = JumpOpt(L2, phase2=True).run()
    return L, unsigned


def _resolve(L, name, empty_names):
    """Final target name of a label: arm name, or EXIT when only labels/nothing sit between it
    and EXIT (default / break arms)."""
    seen = set()
    while name not in seen:
        seen.add(name)
        for k, i in enumerate(L):
            if i.kind == 'label' and i.name == name:
                n = k + 1
                while n < len(L) and L[n].kind == 'label':
                    n += 1
                if n >= len(L):
                    return 'EXIT'
                if L[n].kind == 'body':
                    return L[n].name
                if L[n].kind == 'jmp':
                    name = L[n].label
                    break
                return name
        else:
            return name
    return name


def final(cases, layout, index='int', cost=None, empty=('default',), fallthrough=(), post=True, merge=None,
          goto=None):
    """Branch sequence of the switch head after jump.c, targets resolved to arm names.
    merge = {label: survivor} for arms whose identical bodies jump2 cross-jumps into one copy."""
    L, unsigned = rtl(cases, layout, index, cost, empty, fallthrough, post, merge, goto)
    merge = {_norm_label(a): _norm_label(b) for a, b in (merge or {}).items()}
    out = []
    last = None      # cse merges a repeated compare within the extended basic block
    for i in L:
        if i.kind == 'body':
            break
        if i.kind == 'label':
            last = None
        elif i.kind == 'cjmp':
            # SELECT_CC_MODE: EQ/NE compares are CCmode (cmpwi) even for an unsigned index; the
            # ordered compares of an unsigned index are CCUNSmode (cmplwi) and are not merged
            # with the cmpwi of the same value
            mn = 'cmplwi' if unsigned and i.cond not in ('EQ', 'NE') else 'cmpwi'
            if (mn, i.value) != last:
                out.append((mn, i.value))
                last = (mn, i.value)
            t = _resolve(L, i.label, empty)
            out.append((MNEMONIC[i.cond], merge.get(t, t)))
        elif i.kind == 'jmp':
            t = _resolve(L, i.label, empty)
            out.append(('b', merge.get(t, t)))
        elif i.kind == 'deadcmp':
            out.append(('cmpwi', i.value))
            last = None
        elif i.kind == 'br':
            t = _resolve(L, i.label, empty)
            out.append((MNEMONIC[i.cond], merge.get(t, t)))
    return out


def rename_targets(seq):
    """Rename branch targets by first use (L0, L1, ..) so two sequences compare structurally."""
    names = {}
    out = []
    for e in seq:
        if e[0].startswith('b'):
            t = e[1]
            if t not in names:
                names[t] = 'L%d' % len(names)
            out.append((e[0], names[t]))
        else:
            out.append(e)
    return out


def match(target, seq):
    return rename_targets(target) == rename_targets(seq)


def fmt(seq):
    return ' '.join('%s %s' % (m, hex(v) if isinstance(v, int) else v) for m, v in seq)


# ---------------------------------------------------------------- target extraction

_RE_CMP = re.compile(r'^\s*(cmpw?i|cmplwi)\s+(?:cr\d,\s*)?(r\d+),\s*(-?0x[0-9a-fA-F]+|-?\d+)')
_RE_BR = re.compile(r'^\s*(beq|bne|bgt|ble|blt|bge|b)\s+(?:cr\d,\s*)?(\S+)')
_RE_LABEL = re.compile(r'^(\.L_[0-9A-Fa-f]+|\S+):')


def extract_target(asm_path, func, reg=None, start=None):
    """Return the longest run of compare/branch insns (on one register) in FUNC of a dtk .s file
    as [('cmpwi', v), ('beq', label), ...].  `reg` (e.g. 'r0') selects the index register when
    several switches exist; `start` = a label ('.L_0000040C') or a hex offset to begin at."""
    lines = open(asm_path).read().split('\n')
    try:
        s = next(i for i, l in enumerate(lines) if l.startswith('.fn ' + func))
    except StopIteration:
        raise SystemExit('no .fn %s in %s' % (func, asm_path))
    body = []
    for l in lines[s + 1:]:
        if l.startswith('.endfn'):
            break
        body.append(l)
    return extract_lines(body, reg, start)


def extract_ours(unit, func, reg=None, start=None, root=None):
    """Same for OUR compiled object: objdiff-cli JSON (like tools/fdiff.py) of unit ('em31/em31')."""
    import json
    import os
    import subprocess
    root = root or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ver = os.environ.get('RE4_VERSION', 'G4BE08')
    mod = unit.split('/', 1)[0]
    ou = '%s/%s' % (mod, unit) if os.path.isdir(os.path.join(root, 'config', ver, 'modules', mod)) else 'main/' + unit
    out = os.path.join(root, 'build', ver, 'casetree.%d.json' % os.getpid())
    subprocess.run([os.path.join(root, 'build/tools/objdiff-cli'), 'diff', '-p', root, '-u', ou, func, '-o', out,
                    '--format', 'json'], capture_output=True, text=True)
    d = json.load(open(out))
    os.remove(out)
    sym = next(s for s in d['right']['symbols'] if s['name'] == func)
    body = []
    for i in sym['instructions']:
        ins = i.get('instruction')
        if ins:
            body.append('/* %08X */\t%s' % (int(ins.get('address', 0)), ins['formatted']))
    return extract_lines(body, reg, start)


def extract_lines(body, reg=None, start=None):
    # tokenise: labels, cmp, branch, other; each token carries the insn offset (or None)
    toks = []
    for l in body:
        m = _RE_LABEL.match(l)
        if m:
            toks.append(('label', m.group(1), m.group(1)))
            continue
        off = None
        mo = re.match(r'^/\* ([0-9A-Fa-f]{8}) ', l)
        if mo:
            off = int(mo.group(1), 16)
        code = l.split('*/')[-1] if '*/' in l else l
        m = _RE_CMP.match(code)
        if m:
            toks.append(('cmp', m.group(1) if m.group(1) != 'cmpi' else 'cmpwi', m.group(2), int(m.group(3), 0), off))
            continue
        m = _RE_BR.match(code)
        if m:
            toks.append(('br', m.group(1), m.group(2), off))
            continue
        if code.strip():
            toks.append(('other', code.strip(), off))
    if start is not None:
        if isinstance(start, str) and start.startswith('.L'):
            k0 = next(i for i, t in enumerate(toks) if t[0] == 'label' and t[1] == start)
        else:
            want = int(start, 16) if isinstance(start, str) else start
            k0 = next(i for i, t in enumerate(toks) if t[-1] == want)
        toks = toks[k0:]
    runs = []
    cur = []
    cur_reg = None
    skip = 0
    for k, t in enumerate(toks):
        if skip:
            skip -= 1
            continue
        if t[0] == 'cmp' and (reg is None or t[2] == reg) and (cur_reg is None or t[2] == cur_reg):
            cur_reg = t[2]
            cur.append(t)
        elif t[0] in ('br', 'label') and cur:
            cur.append(t)
        elif t[0] == 'other' and cur and cur[-1][0] == 'cmp':
            # sched2 may slip unrelated insns between a compare and its branch
            n = 1
            while k + n < len(toks) and toks[k + n][0] == 'other' and n < 8:
                n += 1
            if k + n < len(toks) and toks[k + n][0] == 'br' and n <= 6:
                skip = n - 1
                continue
            runs.append(cur)
            cur, cur_reg = [], None
        else:
            if cur:
                runs.append(cur)
            cur, cur_reg = [], None
    if cur:
        runs.append(cur)
    if not runs:
        raise SystemExit('no compare runs found')
    if start is None and reg is None and _all_runs:
        return [_run_to_seq(r) for r in runs]
    run = runs[0] if start is not None else max(runs, key=lambda r: sum(1 for t in r if t[0] == 'cmp'))
    return _run_to_seq(run)


_all_runs = False


def extract_all(asm_path, func):
    """Every maximal compare/branch run of FUNC in the target .s (one list per run)."""
    global _all_runs
    _all_runs = True
    try:
        return extract_target(asm_path, func)
    finally:
        _all_runs = False


def _run_to_seq(run):
    # trim trailing labels; drop a trailing plain `b` only if it is not part of the tree
    while run and run[-1][0] == 'label':
        run.pop()
    out = []
    for t in run:
        if t[0] == 'cmp':
            out.append((t[1], t[3]))
        elif t[0] == 'br':
            out.append((t[1], t[2]))
    return out


# ---------------------------------------------------------------- search

def subseq(t, s):
    it = iter(s)
    return all(x in it for x in t)


def search(fixed, blocks, labels, target, index='int', cost=None, layout=None, empty=('default',),
           fallthrough=(), maxprint=40, quiet=False):
    """fixed: {value: label}; blocks: [[values sharing one label], ...]; labels: candidate labels
    per block (None = not an explicit case).  target: [(value, cond)] raw compares (subsequence
    test) when layout is None, else a final()-style branch list compared with match()."""
    res = []
    fixed_cases = [(v, l) for v, l in fixed.items()]
    if layout is not None:
        # jump.c only deletes compares (the range swap reorders them): every target compare value
        # must be emitted at least as often
        from collections import Counter
        tvals = Counter(v for m, v in target if m.startswith('cmp'))
    for assign in itertools.product(labels, repeat=len(blocks)):
        cases = list(fixed_cases)
        for blk, lab in zip(blocks, assign):
            if lab:
                cases += [(v, lab) for v in blk]
        if layout is None:
            cs = compares(cases, index, cost)
            if subseq(target, cs):
                res.append((len(cs) - len(target), assign))
        else:
            cs = Counter(v for v, c in compares(cases, index, cost))
            if not tvals <= cs:
                continue
            seq = final(cases, layout, index, cost, empty, fallthrough)
            if match(target, seq):
                res.append((0, assign))
    res.sort(key=lambda r: r[0])
    if not quiet:
        for extra, assign in res[:maxprint]:
            print(extra, {tuple(hex(v) for v in b): l for b, l in zip(blocks, assign) if l})
        print(len(res), 'candidates')
    return res


# ---------------------------------------------------------------- CLI

def parse_case_arg(s):
    """'0-4:D' / '0xd:X' / '5,6,0xf:D' -> list of (value, label)."""
    vals, label = s.rsplit(':', 1)
    out = []
    for part in vals.split(','):
        if '-' in part.lstrip('-'):
            lo, hi = part.split('-')
            out.append(((int(lo, 0), int(hi, 0)), label))
        else:
            out.append((int(part, 0), label))
    return out


def main(argv):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('cases', nargs='*', help="'lo-hi:LABEL' or 'v1,v2:LABEL' (LABEL D = default)")
    ap.add_argument('--index', default='int', choices=sorted(INDEX_TYPES))
    ap.add_argument('--cost', default='auto', choices=['auto', 'on', 'off'])
    ap.add_argument('--layout', default=None, help='arm labels in source order, comma separated')
    ap.add_argument('--empty', default='default', help='labels whose body is only break')
    ap.add_argument('--fallthrough', default='', help='labels whose body falls into the next arm')
    ap.add_argument('--goto', default='', help='arms whose body is only goto: A=B,C=D')
    ap.add_argument('--merge', default='', help='identical arms cross-jumped into a survivor: A=S,B=S+')
    ap.add_argument('--raw', action='store_true', help='print the stmt.c emission (before jump.c)')
    ap.add_argument('--rtl', action='store_true', help='print the insn list after jump.c')
    ap.add_argument('--target', default=None, help='ASM.s:FUNC[:rN[:START]] to extract and compare')
    ap.add_argument('--ours', default=None, help='MOD/UNIT:FUNC[:rN[:START]] our object (objdiff-cli)')
    a = ap.parse_args(argv)
    cases = []
    for c in a.cases:
        cases += parse_case_arg(c)
    cost = None if a.cost == 'auto' else a.cost == 'on'
    if cases:
        print(dump_tree(cases, a.index, cost))
        if a.raw:
            for e in tree(cases, a.index, cost):
                print('  ', e)
        layout = a.layout.split(',') if a.layout else None
        if layout is not None:
            empty = a.empty.split(',') if a.empty else ()
            ft = a.fallthrough.split(',') if a.fallthrough else ()
            goto = dict(x.split('=') for x in a.goto.split(',')) if a.goto else None
            merge = dict(x.split('=') for x in a.merge.split(',')) if a.merge else None
            if a.rtl:
                L, _ = rtl(cases, layout, a.index, cost, empty, ft, merge=merge, goto=goto)
                for i in L:
                    print('  ', i)
            seq = final(cases, layout, a.index, cost, empty, ft, merge=merge, goto=goto)
            print('final :', fmt(seq))
    if a.target:
        parts = a.target.split(':')
        tgt = extract_target(parts[0], parts[1], parts[2] if len(parts) > 2 else None,
                             parts[3] if len(parts) > 3 else None)
        print('target:', fmt(tgt))
        if cases and layout is not None:
            print('MATCH' if match(tgt, seq) else 'DIFF')
    if a.ours:
        parts = a.ours.split(':')
        ours = extract_ours(parts[0], parts[1], parts[2] if len(parts) > 2 else None,
                            parts[3] if len(parts) > 3 else None)
        print('ours  :', fmt(ours))
        if cases and layout is not None:
            print('MATCH' if match(ours, seq) else 'DIFF')


if __name__ == '__main__':
    main(sys.argv[1:])
