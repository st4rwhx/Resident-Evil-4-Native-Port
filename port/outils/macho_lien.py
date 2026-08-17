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


def nm(objs, flag, glob_only=True):
    """{objet: set(noms Mach-O)} ; flag '--defined-only' ou '-u'."""
    out = {}
    for k in range(0, len(objs), 60):
        chunk = objs[k:k + 60]
        r = run([NM, flag] + (['-g'] if glob_only else []) + ['--format=just-symbols' if flag == '-u' else '--format=posix'] + chunk)
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
    ap.add_argument('--engine', default='', help='libnjruntime.a du moteur iOS (doublons coeur/moteur)')
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

    def set_ren(o, cur, new):
        """Renomme TOUT ce qui porte le nom COURANT `cur` dans l'objet : les references deja raccordees vers `cur` (nom
        d'assembleur) ET le symbole brut `cur` lui-meme (definition C). Un objet peut definir `_x` et appeler `x`."""
        hit = False
        for k, v in list(ren[o].items()):
            if v == cur:
                ren[o][k] = new
                hit = True
        if cur not in ren[o] and (cur in DEF[o] or cur in UND[o] or not hit):
            ren[o][cur] = new

    wraps = ['malloc', 'calloc', 'memset']
    for line in open(os.path.join(ROOT, 'port', 'gxn_wrap.rsp')):
        line = line.strip()
        if line.startswith('-Wl,--wrap='):
            wraps.append(line.split('=', 1)[1])
    wrappers = [o for o in dol if os.path.basename(o).startswith(('port__gxn', 'port__memwrap'))]

    # 0. noms d'assembleur (asm("x")), EN PREMIER : en ELF le nom d'assembleur EST le nom du symbole ; en Mach-O un nom C
    # recoit un « _ » devant, un nom d'assembleur non. Une reference sans definition dans le coeur est raccordee a la
    # definition qui differe d'un « _ » ; une reference sans « _ » qui ne se raccorde a rien prend le « _ » (moteur ou
    # systeme). Les definitions ne bougent pas. Les modules et --wrap travaillent ensuite sur ces noms raccordes.
    gdef = set()
    for o in allobj:
        gdef |= DEF[o]
    # noms CodeWarrior / GCC 2.95 (`setStatus__3cEmi`, `__3cEm`) : comme build_rel (alias), chaque fonction C++ definie
    # par clang (__Z..., Mach-O) est demanglee puis remanglee au format GCC 2.95 (port/gcc2_mangle.py) ; une reference
    # par nom CW va vers la definition du MEME module d'abord, sinon du programme principal, sinon d'un autre module.
    sys.path.insert(0, os.path.join(ROOT, 'port'))
    import gcc2_mangle  # noqa: E402
    cxxfilt = os.path.join(NDKBIN, 'llvm-cxxfilt.exe')

    def g2_table(objs):
        names = sorted({s0 for o in objs for s0 in DEF[o] if s0.startswith('__Z')})
        res = {}
        if not names:
            return res
        # par l'entree standard : en arguments, la ligne de commande depasse la limite de Windows (WinError 206)
        dm = subprocess.run([cxxfilt], input='\n'.join(n[1:] for n in names) + '\n', capture_output=True, text=True).stdout.splitlines()
        for a0, b0 in zip(names, dm):
            g2 = gcc2_mangle.mangle(b0)
            if g2:
                res.setdefault('_' + g2, a0)
        return res
    g2_dol = g2_table(dol)
    g2_mod = {m: g2_table(v) for m, v in mods.items()}
    g2_all = dict(g2_dol)
    for m in sorted(g2_mod):
        for k, v in g2_mod[m].items():
            g2_all.setdefault(k, v)
    c_funcs = {s0[1:] for o in allobj for s0 in DEF[o] if not s0.startswith('__Z') and s0.startswith('_')}
    mod_of = {o: m for m, v in mods.items() for o in v}
    import re as _re
    fixed = 0
    _loc_cache = {}

    def local_def(o):
        # etiquettes d'assembleur locales (asm(".const ... r222_k212: ...")) : en ELF le nom C et l'etiquette coincident
        if o not in _loc_cache:
            _loc_cache[o] = nm([o], '--defined-only', glob_only=False).get(o, set())
        return _loc_cache[o]
    n_cw = 0
    for o in allobj:
        own = g2_mod.get(mod_of.get(o), {})
        for u in UND[o] - gdef:
            k = '_' + u   # cle de la table : le nom d'assembleur avec le « _ » de Mach-O
            stem = _re.match(r'^(\w+?)__(?:F|\d)', u)
            if ('_' + u) in gdef:
                ren[o][u] = '_' + u
            elif u.startswith('_') and u[1:] in gdef:
                ren[o][u] = u[1:]
            elif u.startswith('_') and u[1:] in local_def(o):   # nom C -> etiquette d'assembleur LOCALE du meme objet
                ren[o][u] = u[1:]
            elif k in own or k in g2_dol or k in g2_all:   # nom CodeWarrior -> fonction clang
                ren[o][u] = own.get(k) or g2_dol.get(k) or g2_all[k]
                n_cw += 1
            elif stem and stem.group(1) in c_funcs:          # fonction extern "C" appelee par son nom CW
                ren[o][u] = '_' + stem.group(1)
                n_cw += 1
            elif not u.startswith('_'):
                ren[o][u] = '_' + u
            else:
                continue
            fixed += 1
    print('%d references par nom d\'assembleur raccordees' % fixed)
    U2 = {o: {ren[o].get(u, u) for u in UND[o]} for o in allobj}
    dol_def = set().union(*[DEF[o] for o in dol])
    dol_und = set().union(*[U2[o] for o in dol])

    # 1. modules
    MD, MU = {}, {}
    for m, objs in mods.items():
        seen = {}
        for o in objs:
            for s0 in sorted(DEF[o]):
                if s0 in seen:   # deuxieme definition dans le meme module : renommee dans cet objet
                    set_ren(o, s0, msym('%s__D%d_%s' % (m, len(seen), cname(s0))))
                else:
                    seen[s0] = o
        MD[m] = set(seen)
        MU[m] = set().union(*[U2[o] for o in objs]) - MD[m]
    mods = {m: v for m, v in mods.items() if msym('_prolog') in MD[m]}
    need = set(dol_und)
    for m in mods:
        need |= MU[m]
    need -= dol_def
    entry_m = {msym(e) for e in ENTRY}
    exports = {}
    for m in sorted(mods):
        for s0 in MD[m]:
            if s0 in need and s0 not in entry_m and s0 not in exports:
                exports[s0] = m
    print('%d symboles partages entre modules' % len(exports))
    for m, objs in mods.items():
        loc = {}
        for s0 in MD[m]:
            if s0 in entry_m:
                loc[s0] = msym('%s_%s' % (m, cname(s0)))            # _prolog -> <mod>__prolog
            elif exports.get(s0) != m:
                loc[s0] = msym('%s__L%s' % (m, cname(s0)))
        for o in objs:
            cur = {ren[o].get(x, x) for x in DEF[o]} | U2[o]
            for s0, n in loc.items():
                if s0 in cur:
                    set_ren(o, s0, n)

    # 2. --wrap (sur les noms raccordes)
    nw = 0
    for w in wraps:
        sw = msym(w)
        for o in allobj:
            if o in wrappers:
                if msym('__real_' + w) in U2[o]:
                    set_ren(o, msym('__real_' + w), sw)
                continue
            if sw in U2[o] and sw not in DEF[o]:
                set_ren(o, sw, msym('__wrap_' + w))
                nw += 1
    print('%d fonctions enveloppees, %d appels redirigees (objets)' % (len(wraps), nw))

    # 2 ter. symboles definis A LA FOIS par le coeur et par le moteur (libnjruntime.a, --engine) : sur Android le coeur est
    # un programme a part (sa copie gagne : --allow-multiple-definition) et le moteur garde la sienne dans son processus.
    # Dans un seul programme (iOS) : une FONCTION doublee est renommee dans le coeur (definition et appels : chacun garde la
    # sienne, comme sur Android) ; une VARIABLE doublee (g_guest...) n'a qu'une instance, celle du moteur, que le pont njgfx
    # et le rendu lisent aussi : la definition du coeur est ecartee (renommee), ses references restent.
    if args.engine and os.path.exists(args.engine):
        eng = {}
        for line in run([NM, '--defined-only', '-g', '--format=posix', args.engine]).stdout.splitlines():
            p_ = line.split()
            if len(p_) >= 2 and not line.endswith(':'):
                eng[p_[0]] = p_[1]
        dup_f = dup_d = 0
        for o in allobj:
            cur_def = {ren[o].get(x, x): x for x in DEF[o]}
            for cur_name, raw in cur_def.items():
                if cur_name not in eng:
                    continue
                if eng[cur_name] in ('T', 't'):   # fonction : le coeur garde la sienne sous un autre nom
                    new = '_ncore' + cur_name
                    for o2 in allobj:
                        cur2 = {ren[o2].get(x, x) for x in DEF[o2]} | {ren[o2].get(x, x) for x in UND[o2]}
                        if cur_name in cur2:
                            set_ren(o2, cur_name, new)
                    dup_f += 1
                else:                              # variable : une seule instance, celle du moteur
                    ren[o][raw] = '_ncore_ecartee' + cur_name
                    dup_d += 1
        print('doublons coeur/moteur : %d fonctions renommees dans le coeur, %d variables laissees au moteur' % (dup_f, dup_d))

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
