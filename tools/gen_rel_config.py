#!/usr/bin/env python3
"""Generate config/<ver>/modules/<mod>/{splits.txt,symbols.txt,sym_map.tsv,rel.json} for every REL module
listed in config/<ver>/config.yml, from the original REL and the debug build's Bio4.<mod>.sym.

usage: gen_rel_config.py <config.yml> <dir with the Bio4.<mod>.sym files>
       (e.g. config/G4BE08/config.yml orig/G4BE08/files)

What the debug .sym gives us for a module: every .text function (offset, size, scope, demangled name);
nothing about data and nothing about which object a function came from (all are "<mod>.preplf").
So:
  * functions come from the .sym (sanitised names, unnamed ones become fn_<mod>_<off>),
  * data symbols are labels at every relocation target inside the module (from its own relocation
    table and from the tables of the other configured modules): lbl_<mod>_<section>_<off>,
  * a module is one unit "<mod>/<mod>.cpp" unless config/<ver>/modules.py UNITS names boundaries; data
    sections are then attributed to units by the relocations coming from each unit's code,
  * rel.json holds the REL header constants (module id, section count, name offset/size, alignments,
    REL section indices, imported modules) that tools/make_rel.py needs to rebuild the file.
"""
import bisect
import collections
import importlib.util
import json
import os
import re
import sys

import yaml

sys.path.insert(0, os.path.dirname(__file__))
import relfile  # noqa: E402
from re4sym import parse as parse_sym  # noqa: E402
from symnames import sanitize  # noqa: E402

DATA_SECTIONS = ['.ctors', '.dtors', '.rodata', '.data', '.bss']
SEC_TYPE = {'.text': 'code', '.ctors': 'rodata', '.dtors': 'rodata', '.rodata': 'rodata', '.data': 'data', '.bss': 'bss'}


def find_sym_file(files_dir, name):
    want = f'bio4.{name}.sym'.lower()
    for f in os.listdir(files_dir):
        if f.lower() == want:
            return os.path.join(files_dir, f)
    return None


def infer_align(prev_end, off, max_align):
    a = 4
    while a < max_align and (prev_end + a - 1) & ~(a - 1) != off:
        a *= 2
    assert (prev_end + a - 1) & ~(a - 1) == off, (hex(prev_end), hex(off))
    return a


def main():
    cfg_path, files_dir = sys.argv[1:3]
    cfg_dir = os.path.dirname(cfg_path)
    cfg = yaml.safe_load(open(cfg_path))
    object_base = cfg['object_base']
    spec = importlib.util.spec_from_file_location('modules', os.path.join(cfg_dir, 'modules.py'))
    modcfg = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modcfg)
    unit_overrides = getattr(modcfg, 'UNITS', {})

    # --- load every module --------------------------------------------------------------------
    mods = collections.OrderedDict()  # name -> dict(rel=, syms=)
    by_id = {}
    for m in cfg.get('modules', []):
        name = m.get('name') or os.path.splitext(os.path.basename(m['object']))[0]
        rel = relfile.parse(os.path.join(object_base, m['object']))
        assert rel.module_id not in by_id, f'duplicate module id {rel.module_id}'
        symf = find_sym_file(files_dir, name)
        funcs = []
        if symf:
            names, entries = parse_sym(symf)
            # an unnamed global "." doubles the first function of some modules (the object start)
            named = {(a, sz) for a, sz, u8, dn in entries if dn != '.'}
            for a, sz, u8, dn in entries:
                if dn == '.' and (a, sz) in named:
                    continue
                funcs.append((a, sz, 'global' if u8 & 1 else 'local', dn))
        else:
            print(f'note: no Bio4.{name}.sym, functions of {name} will come from dtk analysis only')
        mods[name] = dict(rel=rel, funcs=sorted(funcs), object=m['object'])
        by_id[rel.module_id] = name

    # --- relocation targets per module -----------------------------------------------------------
    # The relocated field in the file tells how the original object expressed the reference: ngcld
    # writes S+A into the field for local symbols (section-relative, so the field holds the target
    # offset) and only A for global ones (0, or the offset into the global object). Symbol scopes are
    # derived from that so that the relinked fields come out the same.
    targets = collections.defaultdict(lambda: collections.defaultdict(dict))  # id -> sec -> off -> external?
    scope_votes = collections.defaultdict(lambda: collections.defaultdict(set))  # id -> (sec, off) -> {'local','global'}
    inner_only = collections.defaultdict(lambda: collections.defaultdict(set))  # id -> (sec, off) -> {True/False}
    for name, md in mods.items():
        rel = md['rel']
        for r in rel.relocs:
            if r.module == 0:
                continue
            if r.module not in by_id:
                sys.exit(f'{name} imports module {r.module}, which is not in config.yml')
            ext = r.module != rel.module_id
            t = targets[r.module][r.target_section]
            t[r.addend] = t.get(r.addend, False) or ext
            if r.kind not in (relfile.R_PPC_ADDR32, relfile.R_PPC_ADDR16_LO) or r.addend == 0:
                continue
            data = rel.section_data(r.section)
            if r.kind == relfile.R_PPC_ADDR32:
                field = int.from_bytes(data[r.offset:r.offset + 4], 'big')
                same = field == r.addend
            else:
                field = int.from_bytes(data[r.offset:r.offset + 2], 'big')
                same = field == (r.addend & 0xFFFF)
                if field >= 0x8000:
                    field -= 0x10000
            key = (r.target_section, r.addend)
            if same:
                assert not ext, (name, r)
                scope_votes[r.module][key].add('local')
                inner_only[r.module][key].add(False)
            elif field == 0:
                scope_votes[r.module][key].add('global')
                inner_only[r.module][key].add(False)
            else:
                # reference to global symbol at (target - A) with addend A
                base = (r.target_section, r.addend - field)
                scope_votes[r.module][base].add('global')
                inner_only[r.module][base].add(False)
                targets[r.module][r.target_section].setdefault(r.addend - field, ext)
                inner_only[r.module][key].add(True)

    for name, md in mods.items():
        rel = md['rel']
        mod_id = rel.module_id
        out_dir = os.path.join(cfg_dir, 'modules', name)
        os.makedirs(out_dir, exist_ok=True)
        sec_index = {rel.section_name(i): i for i in rel.sections}
        if '.bss' not in sec_index:
            # empty .bss still has a section index (relocations may point at its start)
            extra = {s for s in targets[mod_id] if s not in rel.sections}
            assert len(extra) <= 1, (name, extra)
            if extra:
                sec_index['.bss'] = extra.pop()
        full_size = {s: rel.sections[i][2] if i in rel.sections else 0 for s, i in sec_index.items()}
        sec_size = dict(full_size)  # sizes without the linker-generated tails
        text_size = sec_size['.text']
        # .ctors/.dtors end with a null pointer the linker script generates (LONG(0), as in SN's
        # preplf.ld); dtk trims it from the splits too, so it belongs to no unit.
        for s in ('.ctors', '.dtors'):
            assert rel.section_data(sec_index[s])[-4:] == b'\0\0\0\0', (name, s)
            sec_size[s] -= 4
        # ngcld's BSS_TAG object: the last word of .data (__sn__bss__tag__address__) points at
        # __sn__bss__tag__, a zero-size linker symbol at the end of the objects' .bss. What follows it
        # in the REL (0x34 bytes in every module that has .bss) are the COMMON symbols, which ngcld -r
        # leaves unallocated and snmakerel appended to .bss (Sscrn's code addresses them).
        data_idx = sec_index['.data']
        tag = [r for r in rel.relocs if r.module == mod_id and r.section == data_idx
               and r.offset == full_size['.data'] - 4]
        assert len(tag) == 1 and tag[0].kind == relfile.R_PPC_ADDR32 \
            and tag[0].target_section == sec_index['.bss'], (name, tag)
        common_size = rel.bss_size - tag[0].addend
        assert common_size >= 0 and common_size % 4 == 0, (name, hex(common_size))
        sec_size['.data'] -= 4
        sec_size['.bss'] -= common_size
        skips = [('.data', sec_size['.data'], full_size['.data'])]

        # --- symbols ------------------------------------------------------------------------------
        votes = scope_votes[mod_id]
        scope_notes = collections.Counter()

        def reloc_scope(sec, off, default):
            if targets[mod_id].get(sec, {}).get(off, False):
                return 'global'  # referenced from another module
            v = votes.get((sec, off), set())
            if len(v) > 1:
                scope_notes['conflict'] += 1
                return default
            if v:
                s = next(iter(v))
                if s != default:
                    scope_notes[f'{default}->{s}'] += 1
                return s
            return default

        syms = []  # (sec, off, size, name, type, scope)
        funcs = md['funcs']
        used = collections.Counter()
        text_cover = []
        text_idx = sec_index['.text']
        for a, sz, scope, dn in funcs:
            assert a + sz <= text_size, (name, hex(a), hex(sz))
            if dn == '.':
                n = f'fn_{name}_{a:X}'
            else:
                n = sanitize(dn)
                used[n] += 1
                if used[n] > 1:
                    n = f'{n}_{a:X}'
            syms.append(('.text', a, sz, n, 'function', reloc_scope(text_idx, a, scope)))
            text_cover.append((a, a + sz))
        # labels at relocation targets
        text_gap_labels = 0
        for tsec, offs in sorted(targets[mod_id].items()):
            if tsec not in rel.sections:
                continue  # empty section (BSS_TAG pointer of the em modules)
            sname = next(s for s, i in sec_index.items() if i == tsec)
            for off in sorted(offs):
                if inner_only[mod_id].get((tsec, off)) == {True}:
                    continue  # only ever referenced as global_symbol + offset: no symbol of its own
                if tsec == text_idx:
                    if any(lo <= off < hi for lo, hi in text_cover):
                        continue
                    # code the .sym does not cover (the debug sym generator skipped the cManager<T>
                    # template instantiations and __builtin_saveregs at the end of the tool modules)
                    text_gap_labels += 1
                    syms.append((sname, off, 0, f'lbl_{name}_text_{off:X}', 'label',
                                 reloc_scope(tsec, off, 'global')))
                    continue
                if sname in ('.ctors', '.dtors'):
                    # _prolog/_epilog walk the lists from their start: dtk's linker-generated _ctors/_dtors
                    assert off == 0, (name, sname, hex(off))
                    continue
                if sname == '.bss' and off == sec_size['.bss']:
                    continue  # the BSS_TAG target / the COMMON block (added below)
                assert off < sec_size[sname], (name, sname, hex(off))
                syms.append((sname, off, 0, f'lbl_{name}_{sname[1:]}_{off:X}', 'object',
                             reloc_scope(tsec, off, 'global')))
        if common_size:
            # one COMMON symbol for the whole block until the real variables are known (a `common`
            # split makes dtk emit it as SHN_COMMON; make_rel.py allocates it after .bss like snmakerel)
            syms.append(('.bss', sec_size['.bss'], common_size, f'common_{name}', 'object', 'global'))
        elif sec_index['.bss'] in rel.sections:
            # no COMMON block: the BSS_TAG pointer targets the end of .bss, dtk needs a symbol there
            syms.append(('.bss', sec_size['.bss'], 0, '__sn__bss__tag__', 'label', 'local'))
        if text_gap_labels:
            print(f'note: {name}: {text_gap_labels} .text targets outside every known function (labels)')
        if scope_notes:
            print(f'note: {name}: scopes from relocated fields: {dict(scope_notes)}')
        # Targets referenced both ways (t_id: one object addresses .bss+0x14DAC4 through a global symbol,
        # another through a local one at the same address): one scope cannot reproduce every field, so
        # the odd relocations get their original field content written back by make_rel.py.
        # Likewise for `symbol + A` references dtk cannot express the same way (a negative A, or another
        # label between symbol and target: dtk relocates against the symbol containing the target).
        field_overrides = []
        sym_scope = {(sec_index[s[0]], s[1]): s[5] for s in syms}
        starts = collections.defaultdict(list)
        for s in syms:
            if s[4] != 'label':
                starts[sec_index[s[0]]].append(s[1])
        for v in starts.values():
            v.sort()

        def containing(tsec, t):
            v = starts.get(tsec, [])
            i = bisect.bisect_right(v, t) - 1
            return v[i] if i >= 0 else None

        for r in rel.relocs:
            if r.module != mod_id or r.kind not in (1, 4, 6) or r.target_section not in rel.sections:
                continue
            if r.section == data_idx and r.offset >= sec_size['.data']:
                continue  # BSS_TAG pointer
            tsec, t = r.target_section, r.addend
            data = rel.section_data(r.section)
            width = 4 if r.kind == 1 else 2
            field = int.from_bytes(data[r.offset:r.offset + width], 'big')
            base = containing(tsec, t)
            if base is None or (tsec == text_idx and not any(lo <= t < hi for lo, hi in text_cover)):
                continue  # .text gap labels: no size information, leave to dtk
            a = t - base
            if sym_scope.get((tsec, base)) == 'local':
                a = t  # section-relative: field = S+A
            expect = {1: a, 4: a & 0xFFFF, 6: ((a + 0x8000) >> 16) & 0xFFFF}[r.kind]
            if field != expect:
                field_overrides.append(dict(section=rel.section_name(r.section), offset=r.offset,
                                            kind=r.kind, value=field))
        if field_overrides:
            print(f'note: {name}: {len(field_overrides)} relocated fields written back verbatim (rel.json field_overrides)')
        syms.sort(key=lambda s: (list(sec_index).index(s[0]), s[1]))
        demangled = {a: dn for a, sz, scope, dn in funcs}

        # --- units ----------------------------------------------------------------------------------
        # (unit, first function[, shared source[, {section: data start}]]) -> (unit, first function)
        units = [tuple(u[:2]) for u in unit_overrides.get(name, [(f'{name}/{name}.cpp', None)])]
        data_starts = {u[0]: u[3] for u in unit_overrides.get(name, []) if len(u) > 3}
        starts = []
        for uname, first in units:
            if first is None:
                starts.append(0)
            elif isinstance(first, int):
                # a .text offset, for a first function whose name is not unique in the module (st2_0's
                # r208.cpp and r222.cpp both start with setResetNum)
                assert any(a == first for a, sz, scope, dn in funcs), f'{name}: unit {uname}: no function at {first:#x}'
                starts.append(first)
            else:
                cand = [a for a, sz, scope, dn in funcs if dn == first]
                assert len(cand) == 1, f'{name}: unit {uname}: function {first!r} not unique/found: {cand}'
                starts.append(cand[0])
        assert starts == sorted(starts) and starts[0] == 0, (name, starts)
        text_ranges = [(starts[i], starts[i + 1] if i + 1 < len(starts) else text_size) for i in range(len(starts))]
        ranges = {u: {'.text': r} for (u, _), r in zip(units, text_ranges)}

        def unit_of_text(off):
            for (u, _), (lo, hi) in zip(units, text_ranges):
                if lo <= off < hi:
                    return u
            raise AssertionError(hex(off))

        if len(units) > 1:
            self_relocs = [r for r in rel.relocs if r.module == mod_id]
            for sname in DATA_SECTIONS:
                if sname not in sec_index:
                    continue
                sidx = sec_index[sname]
                if sname in ('.ctors', '.dtors'):
                    # one pointer per unit with static constructors, terminator to the last unit
                    owner = {}
                    for r in self_relocs:
                        if r.section == sidx:
                            assert r.target_section == text_idx and r.kind == relfile.R_PPC_ADDR32
                            owner[r.offset] = unit_of_text(r.addend)
                    order = [owner[o] for o in sorted(owner)]
                    assert order == sorted(order, key=lambda u: [x[0] for x in units].index(u)), (name, sname, order)
                    bounds = {}
                    for o in sorted(owner):
                        bounds.setdefault(owner[o], o)
                else:
                    refs = collections.defaultdict(list)
                    for r in self_relocs:
                        if r.target_section == sidx and r.section == text_idx:
                            refs[unit_of_text(r.offset)].append(r.addend)
                    # a unit's data starts after everything earlier units address; references below
                    # that are to global symbols of an earlier unit (the em10 tails use library data)
                    bounds = {}
                    prev_max = -1
                    # a global of a *later* unit may be addressed too (Sscrn's ss_cap uses a table of
                    # ss_file): a reference above some later unit's lowest local is not the unit's own data
                    min_local = {u: min([a for a in refs.get(u, []) if sym_scope.get((sidx, a)) == 'local'], default=None)
                                 for u, _ in units}
                    for ui, (u, _) in enumerate(units):
                        forced = data_starts.get(u, {}).get(sname)  # modules.py knows better (unreferenced data)
                        if u not in refs and forced is None:
                            continue
                        later_local = [min_local[v] for v, _ in units[ui + 1:] if min_local[v] is not None]
                        later_local += [data_starts[v][sname] for v, _ in units[ui + 1:]
                                        if data_starts.get(v, {}).get(sname) is not None]
                        first_later = min(later_local) if later_local else None
                        for a in refs.get(u, []):
                            if a <= prev_max:
                                assert sym_scope.get((sidx, a)) == 'global' or (sidx, a) not in sym_scope, \
                                    f'{name}: {u} addresses {sname}+{a:#x}, a local of an earlier unit'
                        own = [a for a in refs.get(u, []) if a > prev_max and (first_later is None or a < first_later
                               or sym_scope.get((sidx, a)) != 'global')]
                        if not own and forced is None:
                            continue
                        if forced is not None:
                            assert forced > prev_max and all(a >= forced for a in own), \
                                (name, u, sname, hex(forced), hex(prev_max), [hex(a) for a in own if a < forced])
                            bounds[u] = forced
                        else:
                            # the first unit *with data* owns the section start (unreferenced leading
                            # data; the stage modules' em_wrap.cpp has no .data/.bss at all)
                            bounds[u] = 0 if not bounds else min(own)
                        prev_max = max(own) if own else forced
                ordered = [u for u, _ in units if u in bounds]
                for i, u in enumerate(ordered):
                    lo = bounds[u]
                    hi = bounds[ordered[i + 1]] if i + 1 < len(ordered) else sec_size[sname]
                    if hi > lo:
                        ranges[u][sname] = (lo, hi)
                if not ordered and sec_size[sname]:
                    ranges[units[0][0]][sname] = (0, sec_size[sname])
            # relocations from a unit's data must stay inside the unit
            for r in self_relocs:
                if r.section == text_idx:
                    continue
                src = rel.section_name(r.section)
                if r.offset >= sec_size[src]:
                    continue  # linker-generated tail (BSS_TAG pointer)
                tgt = next(s for s, i in sec_index.items() if i == r.target_section)
                u = next(u for u in ranges if src in ranges[u] and ranges[u][src][0] <= r.offset < ranges[u][src][1])
                if tgt == '.text':
                    continue
                lo, hi = ranges[u].get(tgt, (None, None))
                if lo is None or not lo <= r.addend < hi:
                    sys.exit(f'{name}: {u} {src}+{r.offset:#x} points at {tgt}+{r.addend:#x}, owned by another unit')
        else:
            for sname in DATA_SECTIONS:
                if sname in sec_index and sec_size[sname]:
                    ranges[units[0][0]][sname] = (0, sec_size[sname])

        # The COMMON block is g++ 2.95's .comm output for uninitialised static data members (template
        # statics of cManager<T> & co. in the modules; the DOL's IDSystem::m_scrn_mat sits at the end of
        # its .bss the same way). It is merged across objects, so any unit may carry it in the
        # skeleton: the one whose code addresses it, else the first one with template instantiations.
        common_owner = units[0][0]
        if common_size:
            bss_idx = sec_index['.bss']
            users = [unit_of_text(r.offset) for r in rel.relocs if r.module == mod_id and r.section == text_idx
                     and r.target_section == bss_idx and r.addend >= sec_size['.bss']]
            if users:
                common_owner = users[0]
            else:
                for a, sz, scope, dn in funcs:
                    if '<' in dn:
                        common_owner = unit_of_text(a)
                        break

        # --- symbols.txt ----------------------------------------------------------------------------
        # sizes of labels: up to the next symbol in the section, the section end, or the unit's end
        unit_ends = collections.defaultdict(list)
        for u in ranges:
            for sname, (lo, hi) in ranges[u].items():
                unit_ends[sname].append(hi)
        for i, s in enumerate(syms):
            if s[2] == 0 and s[4] == 'object':
                end = sec_size[s[0]] if s[0] != '.text' else text_size
                for j in range(i + 1, len(syms)):
                    if syms[j][0] == s[0]:
                        end = syms[j][1]
                        break
                    if syms[j][0] != s[0]:
                        break
                if s[0] == '.text':
                    for lo, hi in text_cover:
                        if lo > s[1]:
                            end = min(end, lo)
                            break
                end = min([end] + [e for e in unit_ends[s[0]] if e > s[1]])
                syms[i] = (s[0], s[1], end - s[1], s[3], s[4], s[5])
        # names/scopes already synced from compiled units (sync_rel_symbols.py) survive a regeneration
        sym_path = os.path.join(out_dir, 'symbols.txt')
        existing = {}
        if os.path.exists(sym_path):
            for line in open(sym_path):
                m = re.match(r'^(\S+) = (\.\w+):0x([0-9A-F]+);.*scope:(\w+)', line)
                if m:
                    existing[(m.group(2), int(m.group(3), 16))] = (m.group(1), m.group(4))
        kept = 0
        for i, (sec, off, sz, n, ty, scope) in enumerate(syms):
            if (sec, off) in existing and existing[(sec, off)] != (n, scope):
                syms[i] = (sec, off, sz, *existing[(sec, off)][:1], ty, existing[(sec, off)][1])
                kept += 1
        if kept:
            print(f'note: {name}: kept {kept} synced symbol names/scopes')
        with open(sym_path, 'w') as f:
            for sec, off, sz, n, ty, scope in syms:
                f.write(f'{n} = {sec}:0x{off:08X}; // type:{ty} size:0x{sz:X} scope:{scope}\n')

        # --- splits.txt ----------------------------------------------------------------------------
        aligns = {}
        prev_end = rel.section_info_offset + 8 * rel.num_sections
        for sname, idx in sorted(sec_index.items(), key=lambda kv: kv[1]):
            if sname == '.bss':
                aligns[sname] = rel.bss_align
                continue
            off = rel.sections[idx][0]
            aligns[sname] = infer_align(prev_end, off, rel.align)
            prev_end = off + rel.sections[idx][2]
        with open(os.path.join(out_dir, 'splits.txt'), 'w') as f:
            f.write('Sections:\n')
            # positional: one entry per section dtk loaded from the REL (all non-empty ones)
            for sname, idx in sorted(sec_index.items(), key=lambda kv: kv[1]):
                if idx not in rel.sections:
                    continue
                f.write(f'\t{sname:<11} type:{SEC_TYPE[sname]} align:{aligns[sname]}\n')
            f.write('\n')
            for u, _ in units:
                f.write(f'{u}:\n')
                for sname in ['.text'] + DATA_SECTIONS:
                    if sname in ranges[u]:
                        lo, hi = ranges[u][sname]
                        if hi == lo:
                            continue  # a unit whose every function was dead-stripped (st1_0's em_wrap.cpp)
                        # input-section alignment: the section's (from the file layout) for the first
                        # unit, otherwise the largest power of two the start offset allows (dtk would
                        # default to 8 and pad the link)
                        al = aligns[sname]
                        while lo & (al - 1):
                            al //= 2
                        f.write(f'\t{sname:<11} start:0x{lo:08X} end:0x{hi:08X} align:{al}\n')
                if u == units[-1][0]:
                    # linker-generated BSS_TAG pointer (see above): in no object
                    for sname, lo, hi in skips:
                        f.write(f'\t{sname:<11} start:0x{lo:08X} end:0x{hi:08X} skip\n')
                if common_size and u == common_owner:
                    f.write(f'\t{".bss":<11} start:0x{sec_size[".bss"]:08X} end:0x{full_size[".bss"]:08X} common\n')
                f.write('\n')

        # --- rel.json ------------------------------------------------------------------------------
        links = [by_id[m] for m, _ in rel.imports if m not in (0, mod_id)]
        info = collections.OrderedDict(
            name=name, module_id=mod_id, object=md['object'], num_sections=rel.num_sections,
            name_offset=rel.name_offset, name_size=rel.name_size, align=rel.align, bss_align=rel.bss_align,
            common_size=common_size, sections=sec_index, links=links, field_overrides=field_overrides,
        )
        with open(os.path.join(out_dir, 'rel.json'), 'w') as f:
            json.dump(info, f, indent=2)
            f.write('\n')

        # --- sym_map.tsv (unit_info.py / fdiff.py / sync_rel_symbols.py) ----------------------------
        def unit_at(sname, off):
            if sname == '.bss' and off >= sec_size['.bss']:
                return common_owner
            for u in ranges:
                if sname in ranges[u] and ranges[u][sname][0] <= off < ranges[u][sname][1]:
                    return u
            return units[-1][0]

        with open(os.path.join(out_dir, 'sym_map.tsv'), 'w') as f:
            f.write('section\toffset\tsize\tunit\tscope\tname\tdemangled\n')
            for sec, off, sz, n, ty, scope in syms:
                dn = demangled.get(off, '.') if sec == '.text' and ty == 'function' else n
                f.write(f'{sec}\t0x{off:08X}\t0x{sz:X}\t{unit_at(sec, off)}\t{scope}\t{n}\t{dn}\n')
        nfun = sum(1 for s in syms if s[4] == 'function')
        print(f'{name}: id {mod_id}, {nfun} functions, {len(syms) - nfun} labels, {len(units)} unit(s), links {links}')


if __name__ == '__main__':
    main()
