# gx_inventaire.py <racine du decompile> : inventaire des fonctions GX (src/lib/GX*.c) pour le GX natif (14/11).
# Pour chaque fonction de la bibliotheque : ce qu'elle ecrit (BP = registres RAS/TEV/pixels, XF = transformation/lumieres,
# CP = format des sommets, FIFO = mots bruts, HW = registres materiels PE/CP/PI/MEM), les GX qu'elle appelle, et les
# appels depuis le jeu (hors src/lib/GX*.c). Sortie : tableau Markdown par famille (le fichier source) + totaux.
import os, re, sys, glob, collections

root = sys.argv[1]
lib = sorted(glob.glob(os.path.join(root, 'src', 'lib', 'GX*.c')))
defre = re.compile(r'^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w\s\*]*?\b(_{0,2}GX\w+)\s*\(([^;{]*)\)\s*(?:\{|$)', re.M)


def bodies(text):
    out = []
    for m in defre.finditer(text):
        i = text.find('{', m.end() - 1)
        if i < 0 or text[m.end() - 1:i].strip(): continue
        depth, j = 0, i
        while j < len(text):
            if text[j] == '{': depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0: break
            j += 1
        out.append((m.group(1), text[i:j + 1], text.count('\n', 0, m.start()) + 1))
    return out


funcs = {}
for f in lib:
    t = open(f, encoding='utf-8', errors='replace').read()
    for name, body, line in bodies(t):
        w = []
        if 'GX_WRITE_RAS_REG' in body or 'GX_WRITE_SOME_REG' in body: w.append('BP')
        if 'GX_WRITE_XF_REG' in body or re.search(r'GX_WRITE_U8\(\s*0x10\s*\)', body): w.append('XF')
        if 'GX_WRITE_CP' in body or re.search(r'GX_WRITE_U8\(\s*0x08\s*\)', body): w.append('CP')
        if re.search(r'GX_WRITE_(U8|U16|U32|F32)', body): w.append('FIFO')
        if re.search(r'GX_(SET|GET)_(MEM|CP|PE|PI)_REG|__(pe|cp|pi|mem)Reg', body): w.append('HW')
        if 'SET_REG_FIELD' in body and not w: w.append('etat')
        calls = sorted(set(re.findall(r'\b(_{0,2}GX\w+)\s*\(', body)) - {name})
        funcs[name] = dict(file=os.path.basename(f)[:-2], line=line, writes=w, calls=calls)

# appels depuis le jeu
src = [p for ext in ('c', 'cpp') for p in glob.glob(os.path.join(root, 'src', '**', '*.' + ext), recursive=True)
       if not re.search(r'[\\/]lib[\\/]GX\w*\.c$', p)]
callre = re.compile(r'\b(GX\w+)\s*\(')
uses = collections.Counter(); files = collections.defaultdict(set)
wg = 0; wgfiles = set()
for p in src:
    t = open(p, encoding='utf-8', errors='replace').read()
    for n in callre.findall(t):
        uses[n] += 1; files[n].add(os.path.relpath(p, root).replace('\\', '/'))
    k = t.count('GXWGFifo')
    if k: wg += k; wgfiles.add(os.path.relpath(p, root).replace('\\', '/'))

called = {n for n in uses if n.startswith('GX')}
lib_called = sorted(n for n in called if n in funcs)
outside = sorted(n for n in called if n not in funcs)   # sommets immediats (port/gx_sdk.cpp), macros, types...
fam = collections.defaultdict(list)
for n in lib_called: fam[funcs[n]['file']].append(n)

print('# Inventaire GX pour le GX natif\n')
print('Fonctions definies dans src/lib/GX*.c : %d ; appelees par le jeu : %d ; noms GX appeles hors bibliotheque : %d ;'
      ' acces directs a GXWGFifo : %d dans %d fichiers.\n' % (len(funcs), len(lib_called), len(outside), wg, len(wgfiles)))
tot = collections.Counter()
for fa in sorted(fam, key=lambda x: -sum(uses[n] for n in fam[x])):
    l = sorted(fam[fa], key=lambda n: -uses[n])
    print('## %s : %d fonctions, %d appels\n' % (fa, len(l), sum(uses[n] for n in l)))
    print('| fonction | appels | fichiers | ecrit | appelle |')
    print('|---|---|---|---|---|')
    for n in l:
        d = funcs[n]
        for x in d['writes']: tot[x] += 1
        print('| %s | %d | %d | %s | %s |' % (n, uses[n], len(files[n]), ' '.join(d['writes']) or '-',
                                            ' '.join(c for c in d['calls'] if c in funcs)[:90] or '-'))
    print()
print('## Noms GX appeles par le jeu mais pas definis dans src/lib/GX*.c\n')
print(', '.join('%s (%d)' % (n, uses[n]) for n in sorted(outside, key=lambda n: -uses[n])))
print('\n## Fichiers qui ecrivent directement dans GXWGFifo\n')
print(', '.join(sorted(wgfiles)))
print('\n## Totaux (fonctions appelees par le jeu, par type d ecriture)\n')
print(', '.join('%s %d' % kv for kv in tot.most_common()))
