"""macho_lien.py : edition de liens du coeur natif pour iOS (Mach-O), sans `ld -r` ni `--wrap` (18/11, I5).

Sur Android, port/build_rel.py fusionne chaque module REL par `ld -r`, renomme ses points d'entree et rend local le
reste (llvm-objcopy --localize-symbols), et le lien du coeur utilise --allow-multiple-definition et --wrap (couche gxn
port/gxn_wrap.rsp, memwrap : malloc/calloc/memset). En Mach-O, ld64.lld n'a ni `-r` ni `--wrap`, et llvm-objcopy ne
sait pas `--localize-symbol` ; il sait `--redefine-sym`. Ce script obtient le meme resultat par renommage :

  1. modules : pour chaque module (objets rel/obj/<mod>__*.o), symboles partages calcules comme build_rel (definis par un
     module et attendus ailleurs) ; points d'entree _prolog/_epilog/_unresolved -> <mod>__prolog... ; tout autre symbole
     defini par le module -> <mod>__L<nom> dans tous ses objets ; un symbole defini deux fois dans le meme module garde la
     premiere definition, les suivantes sont renommees (equivalent de --allow-multiple-definition) ;
  2. --wrap : dans chaque objet du coeur qui APPELLE X (X non defini), X -> __wrap_X ; dans les objets enveloppes (gxn,
     memwrap), __real_X -> X ;
  3. registre des modules (rel_registry.cpp, meme format que build_rel) compile pour iOS ;
  4. archive libnjnatif.a (llvm-ar), a lier par -force_load dans l'appli (ios/CMakeLists.txt, NJ_NATIVE_CORE).

Usage (sur le portable, apres build_game.py et build_rel.py avec PORT_ABI=ios) :
  python port/outils/macho_lien.py [--build _port_build_ios]
"""
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
NDKBIN = os.path.join(os.environ.get('NDK', 'C:/android/android-ndk-r26d'), 'toolchains', 'llvm', 'prebuilt', 'windows-x86_64', 'bin')
NM = os.path.join(NDKBIN, 'llvm-nm.exe')
OBJCOPY = os.path.join(NDKBIN, 'llvm-objcopy.exe')
AR = os.path.join(NDKBIN, 'llvm-ar.exe')
CLANG = os.path.join(os.environ.get('PORT_IOS_CLANG', 'C:/re4n/llvm/build-win64/bin'), 'clang++.exe')
SDK = os.environ.get('PORT_IOS_SDK', 'C:/re4n/ios_sdk/iPhoneOS.sdk')
ENTRY = ('_prolog', '_epilog', '_unresolved')   # noms C ; en Mach-O, un « _ » de plus devant


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def nm(objs, flag):
    """{objet: set(noms Mach-O)} ; flag '--defined-only' ou '-u'."""
    out = {}
    for k in range(0, len(objs), 60):
        chunk = objs[k:k + 60]
        r = run([NM, flag, '-g', '--format=just-symbols' if flag == '-u' else '--format=posix'] + chunk)
        cur = chunk[0] if len(chunk) == 1 else None
        for line in r.stdout.splitlines():
            line = line.rstrip()
            if not line:
                continue
            if line.endswith(':') and (line[:-1] in chunk):
                cur = line[:-1]
                out.setdefault(cur, set())
                continue
            if cur is None:
                continue
            name = line.split()[0]
            out.setdefault(cur, set()).add(name)
        for o in chunk:
            out.setdefault(o, set())
    return out


def cname(sym):
    return sym[1:] if sym.startswith('_') else sym


def msym(c):
    return '_' + c


def rename(obj, pairs, dst):
    """Applique les renommages (ancien -> nouveau, noms Mach-O) par fichier de correspondances."""
    if not pairs:
        if obj != dst:
            shutil.copyfile(obj, dst)
        return True
    lst = dst + '.ren'
    with open(lst, 'w') as f:
        for a, b in pairs.items():
            f.write('%s %s\n' % (a, b))
    r = run([OBJCOPY, '--redefine-syms=' + lst, obj, dst])
    if r.returncode != 0:
        print('   objcopy %s : %s' % (os.path.basename(obj), r.stderr.strip()[:200]))
        return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(ROOT, '_port_build_ios'))
    args = ap.parse_args()
    b = args.build
    out = os.path.join(b, 'lien')
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, 'obj'))

    dol = sorted(glob.glob(os.path.join(b, 'obj', '*.o')))
    relobjs = sorted(glob.glob(os.path.join(b, 'rel', 'obj', '*.o')))
    mods = {}
    for o in relobjs:
        m = os.path.basename(o).split('__')[0]
        if m.startswith('t_') or m == 'Tools':   # outils de debug Capcom exclus en 64 bits (build_rel)
            continue
        mods.setdefault(m, []).append(o)
    print('objets : DOL %d, modules %d (%d objets)' % (len(dol), len(mods), sum(len(v) for v in mods.values())))

    allobj = dol + [o for v in mods.values() for o in v]
    DEF = nm(allobj, '--defined-only')
    UND = nm(allobj, '-u')
    dol_def = set().union(*[DEF[o] for o in dol])
    dol_und = set().union(*[UND[o] for o in dol])

    ren = {o: {} for o in allobj}

    # 1. modules
    MD, MU = {}, {}
    for m, objs in mods.items():
        seen = {}
        for o in objs:
            for s in sorted(DEF[o]):
                if s in seen:   # deuxieme definition dans le meme module : renommee dans cet objet
                    ren[o][s] = msym('%s__D%d_%s' % (m, len(seen), cname(s)))
                else:
                    seen[s] = o
        MD[m] = set(seen)
        MU[m] = set().union(*[UND[o] for o in objs]) - MD[m]
    mods = {m: v for m, v in mods.items() if msym('_prolog') in MD[m]}
    need = set(dol_und)
    for m in mods:
        need |= MU[m]
    need -= dol_def
    entry_m = {msym(e) for e in ENTRY}
    exports = {}
    for m in sorted(mods):
        for s in MD[m]:
            if s in need and s not in entry_m and s not in exports:
                exports[s] = m
    print('%d symboles partages entre modules' % len(exports))
    for m, objs in mods.items():
        loc = {}
        for s in MD[m]:
            if s in entry_m:
                loc[s] = msym('%s_%s' % (m, cname(s)))            # _prolog -> <mod>__prolog
            elif exports.get(s) != m:
                loc[s] = msym('%s__L%s' % (m, cname(s)))
        for o in objs:
            for s, n in loc.items():
                if s in DEF[o] or s in UND[o]:
                    ren[o].setdefault(s, n)

    # 2. --wrap
    wraps = ['malloc', 'calloc', 'memset']
    for line in open(os.path.join(ROOT, 'port', 'gxn_wrap.rsp')):
        line = line.strip()
        if line.startswith('-Wl,--wrap='):
            wraps.append(line.split('=', 1)[1])
    wrappers = [o for o in dol if os.path.basename(o).startswith(('port__gxn', 'port__memwrap'))]
    nw = 0
    for w in wraps:
        sw = msym(w)
        for o in allobj:
            if o in wrappers:
                if msym('__real_' + w) in UND[o]:
                    ren[o][msym('__real_' + w)] = sw
                continue
            if sw in UND[o] and sw not in DEF[o]:
                ren[o][sw] = msym('__wrap_' + w)
                nw += 1
    print('%d fonctions enveloppees, %d appels redirigees (objets)' % (len(wraps), nw))

    # 3. renommage des objets
    final = []
    for o in allobj:
        dst = os.path.join(out, 'obj', os.path.basename(o))
        if rename(o, ren[o], dst):
            final.append(dst)

    # 4. registre des modules
    lines = ['// genere par port/outils/macho_lien.py (iOS) - meme format que port/build_rel.py',
             'struct RelEntry { unsigned id; const char *name; void (*prolog)(); void (*epilog)(); void (*unresolved)(); };']
    ids = {}
    for m in sorted(mods):
        try:
            ids[m] = json.load(open(os.path.join(ROOT, 'config', 'G4BE08', 'modules', m, 'rel.json')))['module_id']
        except Exception:
            ids[m] = -1
        for e in ('prolog', 'epilog', 'unresolved'):
            lines.append('extern "C" void %s__%s();' % (m, e))
    lines.append('extern const RelEntry g_rel_table[] = {')
    for m in sorted(mods):
        lines.append('    { %d, "%s", %s__prolog, %s__epilog, %s__unresolved },' % (ids[m], m, m, m, m))
    lines += ['    { 0, 0, 0, 0, 0 }', '};']
    reg = os.path.join(out, 'rel_registry.cpp')
    open(reg, 'w').write('\n'.join(lines) + '\n')
    rego = os.path.join(out, 'obj', 'rel_registry.o')
    r = run([CLANG, '--target=arm64-apple-ios16.0', '-isysroot', SDK, '-miphoneos-version-min=16.0', '-fPIC', '-c', reg, '-o', rego])
    if r.returncode != 0:
        print('registre : %s' % r.stderr[:300])
        return 1
    final.append(rego)
    print('%d modules au registre' % len(mods))

    # 5. archive
    lib = os.path.join(out, 'libnjnatif.a')
    lstf = os.path.join(out, 'objets.txt')
    open(lstf, 'w').write('\n'.join(final) + '\n')
    r = run([AR, 'rcs', lib, '@' + lstf])
    if r.returncode != 0:
        print('archive : %s' % r.stderr[:300])
        return 1
    print('%s : %d objets, %d octets' % (lib, len(final), os.path.getsize(lib)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
