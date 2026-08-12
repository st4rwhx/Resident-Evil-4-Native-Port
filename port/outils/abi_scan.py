"""Cherche les prototypes qui ne s'accordent pas avec la definition sur le passage d'une structure (valeur / adresse).
Sur PowerPC (MWCC) une structure par valeur passe par adresse : les deux formes se melangent sans dommage ; sur ARM
(AAPCS) un Vec ou un GXColor par valeur passe en registres. Les fonctions extern "C" se lient quand meme : rien ne
previent. Sortie : nom, definition, prototype divergent."""
import os
import re
import sys

ROOT = sys.argv[1]
STRUCTS = r'(?:Vec|S16Vec|GXColor|GXColorS10|Quaternion|Point3d|Vec2|cXyz|SVec|Mtx34|GXTexObj|GXLightObj)'
FUNC = re.compile(r'^[ \t]*(?:extern\s+"C"\s+)?(?:static\s+|inline\s+)*[\w:\*\s]+?\b(\w+)\s*\(([^;{}()]*)\)\s*(;|\{|$)', re.M)


def norm_params(p):
    out = []
    for a in p.split(','):
        a = a.strip()
        if not a or a == 'void':
            continue
        a = re.sub(r'/\*.*?\*/', '', a)
        m = re.match(r'(?:const\s+)?(' + STRUCTS + r')\b\s*(\*|&)?', a)
        if m:
            out.append((m.group(1), 'ptr' if m.group(2) else 'val'))
        else:
            out.append(None)
    return out


defs, decls = {}, {}
for dp, dn, fns in os.walk(ROOT):
    if '_port_build' in dp or '.git' in dp:
        continue
    for fn in fns:
        if not fn.endswith(('.cpp', '.c', '.h')):
            continue
        path = os.path.join(dp, fn)
        try:
            txt = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        for m in FUNC.finditer(txt):
            name, params, end = m.group(1), m.group(2), m.group(3)
            if name in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
                continue
            np_ = norm_params(params)
            if not any(np_):
                continue
            rec = (os.path.relpath(path, ROOT), txt.count('\n', 0, m.start()) + 1, np_)
            (defs if end == '{' else decls).setdefault(name, []).append(rec)

n = 0
for name, dl in defs.items():
    for dpath, dline, dp in dl:
        for cpath, cline, cp in decls.get(name, []):
            if len(cp) != len(dp):
                continue
            for i, (a, b) in enumerate(zip(dp, cp)):
                if a and b and a[0] == b[0] and a[1] != b[1]:
                    print(f'{name} : param {i} {a[0]} definition {a[1]} ({dpath}:{dline}) / prototype {b[1]} ({cpath}:{cline})')
                    n += 1
print(n, 'divergences')
