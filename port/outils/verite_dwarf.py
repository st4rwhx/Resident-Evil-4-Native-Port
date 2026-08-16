"""Prototype Vérité 1 : décalages réels (DWARF, -g) contre commentaires `// 0xNN` du décompilé.

Un champ concorde si son commentaire vaut son décalage dans son propre type OU son décalage absolu dans un type qui
l'englobe (les commentaires du décompilé donnent souvent l'offset dans la classe complète : cEm.dmRad 0x334).
usage : python verite_dwarf.py <objet ou binaire non épuré avec -g> [...]
"""
import subprocess, sys, re, os, collections
DUMP = 'C:/android/android-ndk-r26d/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-dwarfdump.exe'
AGG = ('structure_type', 'class_type', 'union_type')

def parse(obj):
    out = subprocess.run([DUMP, '--debug-info', obj], capture_output=True, text=True, errors='replace').stdout
    dies, children, stack = {}, collections.defaultdict(list), []
    cu = 0
    for blk in out.split('\n\n'):
        m = re.search(r'^(0x[0-9a-f]+):(\s+)DW_TAG_(\w+)', blk, re.M)
        if not m:
            continue
        off, depth, tag = int(m.group(1), 16), len(m.group(2)), m.group(3)
        if tag == 'compile_unit':
            cu = off
        while stack and stack[-1][0] >= depth:
            stack.pop()
        a = {'tag': tag}
        for k, pat in (('name', r'DW_AT_name\s+\("([^"]*)"\)'), ('loc', r'DW_AT_data_member_location\s+\((0x[0-9a-f]+)\)'),
                       ('type', r'DW_AT_type\s+\((0x[0-9a-f]+)'), ('file', r'DW_AT_decl_file\s+\("([^"]*)"\)'),
                       ('line', r'DW_AT_decl_line\s+\((\d+)\)'), ('decl', r'DW_AT_declaration\s+\((true)\)')):
            mm = re.search(pat, blk)
            if mm:
                a[k] = mm.group(1)
        if 'type' in a:
            a['type'] = int(a['type'], 16)
        dies[off] = a
        if stack:
            children[stack[-1][1]].append(off)
        stack.append((depth, off))
    return dies, children

def check(objs):
    poss = collections.defaultdict(set)  # (fichier, ligne, champ) -> offsets possibles
    owner = {}
    for obj in objs:
        dies, children = parse(obj)
        def strip(t):
            while t in dies and dies[t]['tag'] in ('typedef', 'const_type', 'volatile_type'):
                t = dies[t].get('type')
            return t
        def walk(t, base, depth, top):
            if depth > 6:
                return
            for c in children.get(t, ()):
                d = dies[c]
                if d['tag'] != 'member':
                    continue
                o = int(d.get('loc', '0x0'), 16)
                if 'file' in d and 'line' in d:
                    k = (d['file'].replace('\\', '/'), int(d['line']), d.get('name', '?'))
                    poss[k].add(o)
                    poss[k].add(base + o)
                    owner.setdefault(k, top)
                st = strip(d.get('type'))
                if st in dies and dies[st]['tag'] in AGG:
                    walk(st, base + o, depth + 1, top)
        for off, d in dies.items():
            if d['tag'] in AGG and 'name' in d and 'decl' not in d:
                walk(off, 0, 0, d['name'])
    lines_cache, ok, bad, nocom = {}, 0, [], 0
    for (p, line, field), offs in sorted(poss.items()):
        if p not in lines_cache:
            try:
                lines_cache[p] = open(p, encoding='utf-8', errors='replace').read().split('\n')
            except OSError:
                lines_cache[p] = []
        L = lines_cache[p]
        if line - 1 >= len(L):
            continue
        c = re.search(r'//\s*(0x[0-9A-Fa-f]+)', L[line - 1])
        if not c:
            nocom += 1
            continue
        want = int(c.group(1), 16)
        if want in offs:
            ok += 1
        else:
            near = min(offs, key=lambda o: abs(o - want))
            bad.append((owner[(p, line, field)], field, near, want, os.path.basename(p), line))
    for b in bad:
        print('ECART %s::%s natif 0x%X commentaire 0x%X (ecart %+d)  %s:%d' % (b[0], b[1], b[2], b[3], b[3] - b[2], b[4], b[5]))
    print('concordants', ok, 'ecarts', len(bad), 'sans commentaire', nocom)

def table_types(objs):
    """{type: {champ ou a.b.c: decalage natif}} pour le Studio (`*pPL+cModel.pos`)."""
    tab = {}
    for obj in objs:
        dies, children = parse(obj)
        def strip(t):
            while t in dies and dies[t]['tag'] in ('typedef', 'const_type', 'volatile_type'):
                t = dies[t].get('type')
            return t
        def walk(t, base, pre, depth, out):
            if depth > 6:
                return
            for c in children.get(t, ()):
                d = dies[c]
                if d['tag'] == 'inheritance':
                    st = strip(d.get('type'))
                    if st in dies:
                        walk(st, base + int(d.get('loc', '0x0'), 16), pre, depth + 1, out)
                    continue
                if d['tag'] != 'member':
                    continue
                o = base + int(d.get('loc', '0x0'), 16)
                nm = d.get('name')
                st = strip(d.get('type'))
                if nm:
                    out.setdefault(pre + nm, o)
                if st in dies and dies[st]['tag'] in AGG:
                    walk(st, o, pre + nm + '.' if nm else pre, depth + 1, out)
        for off, d in dies.items():
            if d['tag'] in AGG and 'name' in d and 'decl' not in d and d['name'] not in tab:
                out = {}
                walk(off, 0, '', 0, out)
                if out:
                    tab[d['name']] = out
    return tab


if __name__ == '__main__':
    check(sys.argv[1:])
