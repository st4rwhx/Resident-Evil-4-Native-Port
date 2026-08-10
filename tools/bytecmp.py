#!/usr/bin/env python3
"""Byte-level identity compare of a split object vs our compiled object (DOL or REL unit).

usage: bytecmp.py <unit>              game/foo, lib/foo (DOL) or <mod>/<file> (REL: t_esp/db_widget)
       bytecmp.py <unit> FUNC [...]   word-level diff of the named function(s)
       bytecmp.py --all UNIT...       one summary line per unit
       bytecmp.py --residue UNIT...   one status line per unit (differing words per function, section issues)
       env OBJ=path                   override our object
Verdict IDENTICAL on 961/962 Matching units (2026-09-11; game/sscrn: the 12-byte .text pad of ppcdown's .balign 32).

Per section: size, alignment, bytes with every relocation field masked, and the relocation targets
compared by RESOLVED location, never by name:
  - a strong symbol defined in the object -> the unit's link address (DOL: splits.txt section start +
    value + addend; REL: module section offset), the same space as the resolved names below;
  - an undefined or WEAK symbol (linkonce vtables/functions: the link takes the FIRST copy, which may be
    another unit's) -> resolved through the linked ELF: build/G4BE08/main.elf for the DOL,
    build/G4BE08/<mod>/<mod>.elf then the DOL then the linked modules for a REL;
  - a location inside the unit's own .text -> (function, offset) of the object's own function table.
.text is reported per function (differing words), the function order/sizes are checked, and named-symbol
mismatches at equal positions are listed (placeholder names).  Alignment differences are reported but
do not decide the verdict (dtk gives data sections align 8, GAS 4; the link decides, docs/research/ "DOL sweep
19a"); a size difference that is only zero tail padding up to the alignment is reported as "pad".

Known limitation (REL units, not flipped yet): a WEAK symbol our object DEFINES (a linkonce vtable
copy the module keeps, `_vt.5cUnit`) is still resolved through the linked ELFs, and while the module's
ELF is linked from the split object (where the copy is an anonymous `lbl_<mod>_rodata_X` label) the
name falls through to the DOL's copy. The rows then read `(t_id, .rodata, 0xe20)` vs `(addr, 0x8021d6a8)`
although the object bytes and the linked REL are identical there (t_id/t_id pass 7: `_._6cCoord`,
`_._7ID_DATA`, `_._5cUnit`, `__static_initialization_and_destruction_0`, 2/2/2/4 words).  Judge such rows
with `tools/make_rel.py --verify` on a module ELF linked with our object; after the flip they vanish.
"""
import sys, os, re, json, difflib
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT + '/tools')
from elffile import Elf, Symbol, SHN_UNDEF, SHN_ABS, SHN_COMMON, STB_LOCAL, STB_GLOBAL, STB_WEAK, STT_SECTION

R_ADDR32, R_ADDR16, R_ADDR16_LO, R_ADDR16_HI, R_ADDR16_HA, R_REL24, R_REL14, R_SDA21 = 1, 3, 4, 5, 6, 10, 11, 109
MASK = {R_ADDR32: 0, R_ADDR16: 0xFFFF0000, R_ADDR16_LO: 0xFFFF0000, R_ADDR16_HI: 0xFFFF0000, R_ADDR16_HA: 0xFFFF0000,
        R_REL24: 0xFC000003, R_REL14: 0xFFFF0003, R_SDA21: 0xFFE00000}
CMP_SECTIONS = ('.text', '.ctor', '.dtor', '.ctors', '.dtors', '.rodata', '.data', '.sdata', '.sdata2', '.bss', '.sbss')
BUILD = ROOT + '/build/G4BE08'
NOBITS = 8


def canon(name):
    name = re.sub(r'\.\d+$', '', name)
    m = re.match(r'_GLOBAL_\.([ID])\.(.*)$', name)
    if m:
        return 'global_constructors' if m.group(1) == 'I' else 'global_destructors'
    m = re.match(r'global_(con|de)structors_keyed_to_', name)
    if m:
        return 'global_constructors' if m.group(1) == 'con' else 'global_destructors'
    if name.startswith('__static_initialization'):
        name = re.sub(r'_[0-9A-Fa-f]{8}$', '', name)
    return name


_modules = None


def module_names():
    global _modules
    if _modules is None:
        yml = open(ROOT + '/config/G4BE08/config.yml').read()
        _modules = re.findall(r"^\s+name:\s*(\S+)\s*$", yml, re.M)
    return _modules


def split_bases(splits_path, unit):
    """{section: link start} of a unit from a dtk splits.txt (first entry per section)."""
    txt = open(splits_path).read()
    m = re.search(r'^' + re.escape(unit) + r'\.(?:cpp|c):\n((?:\t.*\n)*)', txt, re.M)
    if not m:
        raise SystemExit(f'{unit} not in {splits_path}')
    out = {}
    for ln in m.group(1).splitlines():
        f = ln.split()
        out.setdefault(f[0], int(f[1].split(':')[1], 16))
    return out


class Resolver:
    """Name -> location through the linked ELFs (what the linker actually used)."""

    def __init__(self, mod):
        self.mod = mod
        self.tables = []   # (tag, {name: loc}): linked ELF (our names + what the link used), then the dtk
        # symbols.txt of the same image (the split objects' names for data no compiled unit carries)
        if mod:
            self.tables.append((mod, self._load(f'{BUILD}/{mod}/{mod}.elf', mod)))
            self.tables.append((mod, self._load_symtxt(f'{ROOT}/config/G4BE08/modules/{mod}/symbols.txt', mod)))
        self.tables.append(('dol', self._load(f'{BUILD}/main.elf', None)))
        self.tables.append(('dol', self._load_symtxt(f'{ROOT}/config/G4BE08/symbols.txt', None)))
        if mod:
            rel = json.load(open(f'{ROOT}/config/G4BE08/modules/{mod}/rel.json'))
            ids = {m: json.load(open(f'{ROOT}/config/G4BE08/modules/{m}/rel.json'))['module_id'] for m in module_names()}
            for m in sorted(rel['links'], key=lambda m: ids[m]):
                self.tables.append((m, self._load(f'{BUILD}/{m}/{m}.elf', m)))
                self.tables.append((m, self._load_symtxt(f'{ROOT}/config/G4BE08/modules/{m}/symbols.txt', m)))
        self.cache = {}

    @staticmethod
    def _load_symtxt(path, mod):
        out = {}
        for m in re.finditer(r'^(\S+) = (\S+):0x([0-9A-Fa-f]+);', open(path).read(), re.M):
            name, sec, v = m.group(1), m.group(2), int(m.group(3), 16)
            out.setdefault(name, ('addr', v) if mod is None else (mod, sec, v))
        return out

    @staticmethod
    def _load(path, mod):
        e = Elf(path)
        out = {}
        for s in e.symbols:
            if s.bind in (STB_GLOBAL, STB_WEAK) and s.shndx != SHN_UNDEF and s.name:
                if s.shndx == SHN_ABS:
                    loc = ('abs', s.value)
                elif s.shndx == SHN_COMMON:
                    loc = ('COMMON', s.name)
                elif mod is None:
                    loc = ('addr', s.value)
                else:
                    loc = (mod, e.sections[s.shndx].name, s.value)
                out.setdefault(s.name, loc)
        return out

    def resolve(self, name):
        if name not in self.cache:
            loc = ('UNRESOLVED', name)
            for tag, tbl in self.tables:
                if name in tbl:
                    loc = tbl[name]
                    break
            else:
                # dtk names a cross-unit reference to a scope:local DOL symbol `<name>_<address>`
                m = re.match(r'^.*_([0-9A-F]{8})$', name)
                if m and self.mod is None:
                    loc = ('addr', int(m.group(1), 16))
            self.cache[name] = loc
        return self.cache[name]


def add(loc, addend):
    if loc[0] in ('COMMON', 'UNRESOLVED'):
        return loc + (addend,)
    return loc[:-1] + (loc[-1] + addend,)


class Obj:
    def __init__(self, path, resolver, bases):
        self.path = path
        self.e = Elf(path)
        self.R = resolver
        self.mod = resolver.mod
        self.bases = bases
        self.text = self.e.section('.text')
        ti = self.text.index if self.text else -1
        self.funcs = sorted((s for s in self.e.symbols if s.shndx == ti and s.type == 2 and s.size), key=lambda s: s.value)
        self.relas = {si: {r.offset: r for r in lst} for si, lst in self.e.relas.items()}
        self.text_base = bases.get('.text', 0)
        self.fnkey = 'name'
        self.commons = set()
        self.headpad = {}

    def cover_gaps(self, other=None):
        """Nameless .text regions (linkonce copies the fold appended without a symbol) become pseudo functions,
        cut at the other object's function boundaries when given."""
        if self.text is None:
            return
        self.funcs = [f for f in self.funcs if not f.name.startswith('<nameless')]
        cuts = sorted({s.value for s in other.funcs} | {s.value + s.size for s in other.funcs}) if other else []
        pos = 0
        extra = []
        for f in list(self.funcs) + [Symbol(-1, '', self.text.size, 0, 0, 2, self.text.index)]:
            while pos < f.value:
                end = min([c for c in cuts if c > pos] + [f.value])
                extra.append(Symbol(-1, f'<nameless@{pos:#x}>', pos, end - pos, STB_LOCAL, 2, self.text.index))
                pos = end
            pos = max(pos, f.value + f.size)
        self.funcs = sorted(self.funcs + extra, key=lambda s: s.value)
        self.fnindex = {s.value: i for i, s in enumerate(self.funcs)}

    def secname(self, shndx):
        return self.e.sections[shndx].name

    def textpos(self, off):
        for i, s in enumerate(self.funcs):
            if s.value <= off < s.value + s.size:
                return ('.text', i if self.fnkey == 'index' else canon(s.name), off - s.value)
        if 0 <= off < self.text.size:
            return ('.text', '?', off)
        # a branch the original link resolved into another unit of the same section (split kept it raw)
        return ('addr', self.text_base + off) if self.mod is None else (self.mod, '.text', self.text_base + off)

    def key(self, r):
        """Location of a relocation's target, independent of symbol spelling."""
        s = self.e.symbols[r.sym]
        if s.shndx == SHN_COMMON or (s.shndx == SHN_UNDEF and self.R.resolve(s.name)[0] == 'COMMON'):
            self.commons.add(s.name)
            return ('COMMON', r.addend)
        if s.shndx == SHN_UNDEF or (s.bind == STB_WEAK and self.R.resolve(s.name)[0] != 'UNRESOLVED'):
            loc = add(self.R.resolve(s.name), r.addend)
        elif s.shndx == SHN_ABS:
            loc = ('abs', s.value + r.addend)
        else:
            sec = self.secname(s.shndx)
            v = s.value + r.addend
            if sec not in self.bases:
                return ('nobase', sec, v)
            v += self.headpad.get(sec, 0)
            loc = ('addr', self.bases[sec] + v) if self.mod is None else (self.mod, sec, self.bases[sec] + v)
        # inside this unit's own .text -> function-relative
        if self.text is not None:
            if self.mod is None and loc[0] == 'addr':
                off = loc[1] - self.text_base
            elif self.mod is not None and loc[0] == self.mod and loc[1] == '.text':
                off = loc[2] - self.text_base
            else:
                return loc
            if 0 <= off < self.text.size:
                return self.textpos(off)
        return loc

    @staticmethod
    def fold_abs(w, typ, k):
        """A linker absolute (GXWGFifo) is a plain constant in the target: fold it into the word."""
        if k is None or k[0] != 'abs':
            return w, k
        a = k[1]
        if typ == R_ADDR16_HA:
            return w | (((a + 0x8000) >> 16) & 0xFFFF), None
        if typ == R_ADDR16_HI:
            return w | ((a >> 16) & 0xFFFF), None
        if typ in (R_ADDR16_LO, R_ADDR16):
            return w | (a & 0xFFFF), None
        if typ == R_ADDR32:
            return a & 0xFFFFFFFF, None
        return w, k

    def words(self, fn):
        """[(masked word, key)] of a .text function; unrelocated branches keyed by their target."""
        out = []
        data = self.text.data
        relas = self.relas.get(self.text.index, {})
        for i in range(0, fn.size, 4):
            off = fn.value + i
            w = int.from_bytes(data[off:off + 4], 'big')
            r = relas.get(off) or relas.get(off + 2)
            k = None
            if r is not None:
                m = MASK.get(r.type)
                if m is None:
                    k = ('RELOC', r.type, self.key(r))
                else:
                    w &= m
                    w, k2 = self.fold_abs(w, r.type, self.key(r))
                    k = None if k2 is None else (r.type, k2)
            else:
                op = w >> 26
                if op == 18 and not (w & 2):
                    d = w & 0x03FFFFFC
                    if d & 0x02000000:
                        d -= 0x04000000
                    k = (R_REL24, self.textpos(off + d))
                    w &= 0xFC000003
                elif op == 16 and not (w & 2):
                    d = w & 0xFFFC
                    if d & 0x8000:
                        d -= 0x10000
                    k = (R_REL14, self.textpos(off + d))
                    w &= 0xFFFF0003
            out.append((w, k))
        return out

    def datasec(self, sec):
        """(masked bytes, {offset: (type, key)}) of a non-text section."""
        s = self.e.section(sec)
        data = bytearray(s.data)
        rl = {}
        for r in self.e.relas.get(s.index, []):
            rl[r.offset] = (r.type, self.key(r))
            n = 2 if r.type in (R_ADDR16, R_ADDR16_LO, R_ADDR16_HI, R_ADDR16_HA) else 4
            data[r.offset:r.offset + n] = bytes(n)
        return bytes(data), rl

    def named(self, sec):
        """Defined, named non-section symbols of a section: [(value, size, name, bind)]."""
        s = self.e.section(sec)
        return sorted((q.value, q.size, q.name, q.bind) for q in self.e.symbols
                      if q.shndx == s.index and q.type != STT_SECTION and q.name)


def fold_addr(w, k):
    """An unpaired @ha/@l the split lost (raw `lis rX, 0x80NN` in the target): fold our resolved DOL address."""
    if k is None or k[1][0] != 'addr':
        return None
    return Obj.fold_abs(w, k[0], ('abs', k[1][1]))[0]


def same_word(a, b):
    if a == b:
        return True
    if a[1] is None and b[1] is not None:
        return fold_addr(b[0], b[1]) == a[0]
    if b[1] is None and a[1] is not None:
        return fold_addr(a[0], a[1]) == b[0]
    return False


def compare_words(tw, ow):
    if len(tw) == len(ow):
        return sum(1 for a, b in zip(tw, ow) if not same_word(a, b)), None
    sm = difflib.SequenceMatcher(None, tw, ow, autojunk=False)
    n = sum(max(i2 - i1, j2 - j1) for tag, i1, i2, j1, j2 in sm.get_opcodes() if tag != 'equal')
    return n, sm


def unit_paths(unit):
    mod = unit.split('/')[0]
    if mod in module_names():
        f = unit.split('/', 1)[1]
        return mod, f'{BUILD}/{mod}/obj/{mod}/{f}.o', f'{BUILD}/src/{mod}/{f}.o', f'{ROOT}/config/G4BE08/modules/{mod}/splits.txt'
    return None, f'{BUILD}/obj/{unit}.o', f'{BUILD}/src/{unit}.o', f'{ROOT}/config/G4BE08/splits.txt'


def fmt(k):
    if k is None:
        return ''
    def f(x):
        if isinstance(x, tuple):
            return '(' + ', '.join(f(y) for y in x) + ')'
        return hex(x) if isinstance(x, int) and x > 9 else str(x)
    return f(k)


def pad_only(a, b, adata, bdata):
    """True when the longer section only adds zero tail bytes (dtk attributes the next unit's alignment padding
    to this unit); NOBITS: a difference below 32 bytes (unverifiable in the object -- the link decides)."""
    if a.size == b.size:
        return False
    long_, short = (a, b) if a.size > b.size else (b, a)
    if long_.type == NOBITS:
        return long_.size - short.size < 32
    ld = adata if a.size > b.size else bdata
    return not any(ld[short.size:long_.size])


class Result:
    def __init__(self):
        self.lines = []
        self.identical = True
        self.func_rows = []
        self.sec_issues = []
        self.pads = []
        self.notes = []

    def out(self, s):
        self.lines.append(s)


def audit(unit, verbose_funcs=()):
    mod, tpath, opath, splits = unit_paths(unit)
    opath = os.environ.get('OBJ') or opath
    bases = split_bases(splits, unit)
    R = Resolver(mod)
    T = Obj(tpath, R, bases)
    O = Obj(opath, R, bases)
    res = Result()
    res.out(f'{unit}: target {os.path.relpath(tpath, ROOT)}  ours {os.path.relpath(opath, ROOT)}')
    # the split may start a section with the previous unit's alignment padding (st4_0/cSceObj .rodata): when
    # the target is k (< 8) zero bytes longer at the head, our data sits at split start + k
    for sec in CMP_SECTIONS:
        a, b = T.e.section(sec), O.e.section(sec)
        if not (a and b and a.type != NOBITS and 0 < a.size - b.size < 8):
            continue
        k = a.size - b.size
        ta, tr = T.datasec(sec)
        oa, orr = O.datasec(sec)
        if ta[:len(oa)] != oa and not any(ta[:k]) and min(tr, default=k) >= k and ta[k:] == oa:
            O.headpad[sec] = k
            res.pads.append(f'{sec} head {k} zero bytes in target')

    # ---- function order / sizes
    T.cover_gaps(O)
    O.cover_gaps(T)
    tf = [(s.value, s.size, canon(s.name)) for s in T.funcs]
    of = [(s.value, s.size, canon(s.name)) for s in O.funcs]
    same_layout = [x[:2] for x in tf] == [x[:2] for x in of]
    if same_layout:
        T.fnkey = O.fnkey = 'index'
        pairs = list(zip(T.funcs, O.funcs))
        for a, b in pairs:
            if canon(a.name) != canon(b.name) and not (a.name.startswith('<nameless') and b.name.startswith('<nameless')):
                res.notes.append(f'name-only: target {a.name} vs ours {b.name} @.text+{a.value:#x} size {a.size:#x}')
        order = 'OK'
    else:
        # layouts differ: one pseudo function per maximal nameless gap, paired k-th with k-th (target `fn_` blocks)
        T.cover_gaps()
        O.cover_gaps()
        nameless = lambda n: n.startswith('<nameless') or n.startswith('fn_')
        tn = [canon(s.name) for s in T.funcs if not nameless(s.name)]
        on = [canon(s.name) for s in O.funcs if not nameless(s.name)]
        obyname = {canon(s.name): s for s in O.funcs if not nameless(s.name)}
        pairs = [(a, obyname[canon(a.name)]) for a in T.funcs if not nameless(a.name) and canon(a.name) in obyname]
        tnl = [s for s in T.funcs if nameless(s.name)]
        onl = [s for s in O.funcs if nameless(s.name)]
        pairs += list(zip(tnl, onl))
        pairs.sort(key=lambda p: p[0].value)
        if len(tnl) != len(onl):
            res.out(f'    nameless blocks: target {[(s.name, s.size) for s in tnl]} ours {[(hex(s.value), s.size) for s in onl]}')
        if tn == on:
            order = 'OK (sizes differ)'
        else:
            order = 'DIFFERS'
            res.identical = False
            res.sec_issues.append('order')
            for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, tn, on, autojunk=False).get_opcodes():
                if tag != 'equal':
                    res.out(f'    order {tag}: target {tn[i1:i2]} ours {on[j1:j2]}')
        tset, oset = set(tn), set(on)
        for n in tn:
            if n not in oset:
                res.out(f'    missing in ours: {n}')
        for n in on:
            if n not in tset:
                res.out(f'    extra in ours: {n}')

    # ---- .text per function
    nfd = 0
    total_words = 0
    first_text_diff = None
    for a, b in pairs:
        tw, ow = T.words(a), O.words(b)
        n, sm = compare_words(tw, ow)
        total_words += n
        if n:
            nfd += 1
            res.identical = False
            sz = '' if a.size == b.size else f' size {a.size:#x}/{b.size:#x}'
            res.func_rows.append((a.size, a.name, n, sz))
            if first_text_diff is None:
                if sm is None:
                    i = next(i for i, (x, y) in enumerate(zip(tw, ow)) if not same_word(x, y))
                else:
                    i = next(i1 for tag, i1, i2, j1, j2 in sm.get_opcodes() if tag != 'equal')
                first_text_diff = f'{a.value + 4 * i:#x} ({a.name}+{4 * i:#x})'
        if canon(a.name) in verbose_funcs or a.name in verbose_funcs:
            res.out(f'  {a.name}: {n} differing words; size {a.size:#x}/{b.size:#x}')
            if sm is None:
                for i, (x, y) in enumerate(zip(tw, ow)):
                    if not same_word(x, y):
                        res.out(f'    {i*4:5x}: {x[0]:08x} {fmt(x[1]):<56} | {y[0]:08x} {fmt(y[1])}')
            else:
                for tag, i1, i2, j1, j2 in sm.get_opcodes():
                    if tag == 'equal':
                        continue
                    res.out(f'    {tag} tgt[{i1*4:#x}:{i2*4:#x}] ours[{j1*4:#x}:{j2*4:#x}]')
                    for i in range(i1, i2):
                        res.out(f'      T {i*4:5x}: {tw[i][0]:08x} {fmt(tw[i][1])}')
                    for j in range(j1, j2):
                        res.out(f'      O {j*4:5x}: {ow[j][0]:08x} {fmt(ow[j][1])}')

    # ---- sections
    secs = [s for s in CMP_SECTIONS if T.e.section(s) or O.e.section(s)]
    for sec in secs:
        a, b = T.e.section(sec), O.e.section(sec)
        if a is None or b is None:
            side = 'ours' if a is None else 'target'
            sz = (b or a).size
            if sz == 0:
                res.out(f'  {sec:8} OK    only in {side}, empty')
                continue
            if a is not None and (a.type == NOBITS or not any(a.data)):
                res.pads.append(f'{sec} only in target ({sz:#x}{", zero" if a.type != NOBITS else ""})')
                res.out(f'  {sec:8} pad   only in target, size {sz:#x} ({"NOBITS" if a.type == NOBITS else "zero bytes"})')
                continue
            res.identical = False
            res.sec_issues.append(f'{sec} only in {side} ({sz:#x})')
            res.out(f'  {sec:8} DIFF  only in {side}, size {sz:#x}')
            continue
        al = f'align {a.align}/{b.align}'
        if sec == '.text':
            why = ''
            if a.size != b.size:
                why += f' size {a.size:#x}/{b.size:#x}'
                res.identical = False
                res.sec_issues.append(f'.text size {a.size:#x}/{b.size:#x}')
            if first_text_diff is not None:
                why += f' first diff @{first_text_diff}'
            why += f'; {len(pairs) - nfd}/{len(T.funcs)} functions identical, {total_words} differing words, order {order}'
            if not same_layout:
                res.identical = False
            st = 'OK' if (same_layout and nfd == 0) else 'DIFF'
            res.out(f'  {sec:8} {st:5} {al}{why}')
            continue
        if a.type == NOBITS or b.type == NOBITS:
            same = a.size == b.size
            st = 'OK' if same else 'DIFF'
            why = f' size {a.size:#x}/{b.size:#x} (NOBITS)'
            if not same and pad_only(a, b, None, None):
                st = 'pad'
                res.pads.append(f'{sec} {a.size:#x}/{b.size:#x}')
            elif not same:
                res.identical = False
                res.sec_issues.append(f'{sec} size {a.size:#x}/{b.size:#x}')
            res.out(f'  {sec:8} {st:5} {al}{why}')
            continue
        ta, tr = T.datasec(sec)
        oa, orr = O.datasec(sec)
        hp = O.headpad.get(sec, 0)
        if hp:
            ta = ta[hp:]
            tr = {o - hp: k for o, k in tr.items()}
        why = ''
        same = True
        st = 'OK'
        if hp:
            why += f' head pad {hp}'
        elif a.size != b.size:
            if pad_only(a, b, ta, oa):
                st = 'pad'
                res.pads.append(f'{sec} {a.size:#x}/{b.size:#x}')
                why += f' size {a.size:#x}/{b.size:#x} (zero tail padding)'
            else:
                same = False
                why += f' size {a.size:#x}/{b.size:#x}'
        n = min(len(ta), len(oa))
        first = next((i for i in range(n) if ta[i] != oa[i]), None)
        if first is not None:
            same = False
            ndw = sum(1 for i in range(0, n, 4) if ta[i:i+4] != oa[i:i+4])
            why += f' bytes: first diff @{first:#x}, {ndw} words'
        if tr != orr:
            same = False
            offs = sorted(set(tr) | set(orr))
            d = [o for o in offs if tr.get(o) != orr.get(o)]
            if len(tr) != len(orr):
                why += f' relocs: {len(tr)}/{len(orr)} entries'
            o = d[0]
            why += f' reloc targets differ @{o:#x}: {fmt(tr.get(o))} vs {fmt(orr.get(o))} (+{len(d)-1} more)'
        if not same:
            st = 'DIFF'
            res.identical = False
            res.sec_issues.append(f'{sec}{why}')
        res.out(f'  {sec:8} {st:5} {al} size {a.size:#x}{why if st != "OK" else ""}')

    # ---- named data symbols: a target name that ours defines somewhere else (position) or not at all.
    # dtk binds every split symbol GLOBAL, so scopes cannot be read off the split object (REL: make_rel --verify).
    gen = re.compile(r'^(lbl_|fn_|gap_|pad_|@|\.L)|\.\d+$|^__static_initialization|^_GLOBAL_\.|^global_(con|de)structors|_virtual_table_')
    for sec in secs:
        a, b = T.e.section(sec), O.e.section(sec)
        if a is None or b is None or sec == '.text':
            continue
        onames = {}
        for v, s, n, bd in O.named(sec):
            onames.setdefault(canon(n), []).append(v)
        for v, s, n, bd in T.named(sec):
            if gen.search(n):
                continue
            ov = onames.get(canon(n))
            if ov is None:
                res.notes.append(f'name: target {sec}+{v:#x} {n} (size {s:#x}) is not a symbol of ours')
            elif v not in ov:
                res.notes.append(f'position: {n} target {sec}+{v:#x} vs ours {sec}+{ov[0]:#x}')

    if T.commons or O.commons:
        res.notes.append(f'COMMON references: target {sorted(T.commons)} ours {sorted(O.commons)} (compared by addend; make_rel allocates)')
    weak_own = {q.name for o in (T, O) for q in o.e.symbols if q.bind == STB_WEAK and q.shndx != SHN_UNDEF}
    unresolved = sorted(n for (n, loc) in R.cache.items() if loc[0] == 'UNRESOLVED' and n not in weak_own)
    if unresolved:
        res.notes.append(f'unresolved names: {unresolved}')
    for r in sorted(res.func_rows, key=lambda r: (r[2], r[0])):
        res.out(f'    {r[0]:#6x} {r[1]}: {r[2]} words{r[3]}')
    for n in res.notes:
        res.out(f'  note: {n}')
    verdict = 'IDENTICAL' if res.identical else 'DIFF'
    if res.identical and res.pads:
        verdict += f' (pad: {", ".join(res.pads)})'
    res.out(f'  VERDICT: {verdict}')
    res.summary = (unit, res.identical, len(T.funcs), len(pairs) - nfd, total_words, res.sec_issues, res.pads, res.notes)
    return res


def residue_line(r):
    """one research-log status line: functions with differing words (target/ours sizes) + section/order issues"""
    unit, ident, nf, nok, nw, issues, pads, notes = r.summary
    fns = ', '.join(f'{n} {w}' + (f' ({sz.split()[1]})' if sz else '')
                    for (s, n, w, sz) in sorted(r.func_rows, key=lambda x: -x[2]))
    flags = []
    if 'order' in issues:
        flags.append('ORDER')
    flags += [i for i in issues if i != 'order']
    if pads:
        flags.append('pad ' + ', '.join(pads))
    return f'- {unit} {nok}/{nf} ({nw} w): {fns or "-"}' + (f' | {"; ".join(flags)}' if flags else '')


def main():
    args = sys.argv[1:]
    if args and args[0] == '--residue':
        for u in args[1:]:
            print(residue_line(audit(u)))
        return
    if args and args[0] == '--all':
        for u in args[1:]:
            r = audit(u)
            unit, ident, nf, nok, nw, issues, pads, notes = r.summary
            print(f'{"IDENT" if ident else "DIFF ":5} {unit:28} {nok}/{nf} fn, {nw} words; {"; ".join(issues) or "sections equal"}'
                  + (f'; pad: {", ".join(pads)}' if pads else '') + (f'; notes: {len(notes)}' if notes else ''))
        return
    r = audit(args[0], set(args[1:]))
    print('\n'.join(r.lines))


if __name__ == '__main__':
    main()
