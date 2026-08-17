# -*- coding: utf-8 -*-
"""Compile les modules REL du jeu (salles, ennemis, armes, outils) **dans** le binaire ARM.

Sur GameCube chaque module est un fichier .rel que le jeu charge puis relie a la volee
(OSLink). Un telephone ne peut pas executer ce code PowerPC, et on ne veut surtout pas le relier
au demarrage chez le joueur : tout est donc compile a l'avance et lie une fois pour toutes.

Chaque module devient un seul objet relogeable :

  1. ses sources (config/G4BE08/modules.py, sinon `<mod>/<mod>.cpp`) sont compilees avec
     -DREL_MODULE=<mod>, comme dans le build d'origine ;
  2. `ld -r` les fusionne ;
  3. `llvm-objcopy` renomme les trois points d'entree (_prolog/_epilog/_unresolved ->
     <mod>__prolog...) et passe **tout le reste** des symboles definis en local. C'est ce qui rend
     possibles cent modules qui definissent chacun leur `cEmControl`, leur `pRj`, leur `set` : aucun
     ne voit celui du voisin, exactement comme dans des DLL separees ;
  4. un registre (`rel_registry.cpp`) associe l'identifiant de module du fichier .rel
     (config/G4BE08/modules/<mod>/rel.json) a ces trois fonctions : c'est lui que consulte
     OSLink au lieu de relier du PowerPC.

usage : NDK=<racine ndk> python port/build_rel.py [--jobs N] [--only mod,mod] [--out dir]
"""
import argparse
import concurrent.futures
import glob
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_game as bg  # noqa: E402
import gcc2_mangle  # noqa: E402

ROOT = bg.ROOT
CFG = os.path.join(ROOT, 'config', 'G4BE08')


def module_names():
    txt = open(os.path.join(CFG, 'config.yml'), encoding='utf-8').read()
    # seul le bloc `modules:` nomme des modules ; les noms de cles `name:` ailleurs sont rares
    blk = txt.split('\nmodules:', 1)[1]
    return re.findall(r'^\s+name:\s*(\S+)\s*$', blk, re.M)


def load_modules_py():
    spec = importlib.util.spec_from_file_location('modules', os.path.join(CFG, 'modules.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


# Les sources du decompile epinglent des variables sur des registres PowerPC
# (`register f32 z asm("fr0")`) et posent des barrieres d'ordonnancement en assembleur vide
# (`asm volatile("" : : "r"(x))`). Elles n'ont de sens que pour reproduire l'octet pres le code
# de GCC 2.95 ; sur ARM clang refuse les noms de registres. On les retire dans une copie
# temporaire du source - les directives #line gardent les messages d'erreur sur l'original.
RE_REGPIN = re.compile(r'\bregister\s+([A-Za-z_][\w\s\*:<>]*?)\s*(\w+)\s+asm\s*\(\s*"f?r\d+"\s*\)')
RE_ASMEMPTY = re.compile(r'\basm\s*(?:volatile\s*)?\(\s*""[^;\n]*\)\s*;')


# Variables `static` que le decompile designe aussi par un nom d'assembleur (`asm("sceAtCur")`) : sans
# `static` leur symbole existe et le nom d'assembleur les retrouve.
STATIC_VARS = ['em21_trap_cam', 'em2a_rescue_cam', 'sceAtCur', 'sceItemCur', 'scrollWorkPtr']


def _clean(txt):
    new = RE_REGPIN.sub(r'\1 \2', txt)
    new = RE_ASMEMPTY.sub(';', new)
    for nm_ in STATIC_VARS:
        new = re.sub(r'\bstatic\s+((?:const\s+)?[A-Za-z_][\w:<>\*]*\s+\**)(%s)\b' % nm_, r'\1\2', new)
    return new


RE_INC_CPP = re.compile(r'#include\s+"([^"]+\.cpp)"')


def sanitize(path, out_dir, key):
    txt = open(path, encoding='utf-8', errors='replace', newline='').read()
    new = _clean(txt)
    # x86 (banc G0, 14/11) : un .cpp qui en inclut un autre (db_light_v2.cpp -> db_light.cpp) gardait les registres
    # epingles du fichier inclus (« r0 » n'existe pas sur x86). On nettoie aussi l'inclus, copie a cote. Sur ARM, rien
    # ne change (ces noms de registres y existent ; construction du telephone identique).
    incs = []
    if os.environ.get('PORT_ABI') in ('x86', 'x86_64', 'arm64'):   # N7 : noms de registres ARM32 absents en 64 bits aussi
        for inc in RE_INC_CPP.findall(txt):
            ip = os.path.join(os.path.dirname(path), inc)
            if os.path.exists(ip):
                it = open(ip, encoding='utf-8', errors='replace', newline='').read()
                nt = _clean(it)
                if nt != it:
                    incs.append((inc, nt))
    if new == txt and not incs:
        return path
    if incs:
        d = os.path.join(out_dir, key + '.d')
        for inc, nt in incs:
            os.makedirs(os.path.dirname(os.path.join(d, inc)), exist_ok=True)
            with open(os.path.join(d, inc), 'w', encoding='utf-8', newline='') as fh:
                fh.write(nt)
        dst = os.path.join(d, os.path.basename(path))
    else:
        dst = os.path.join(out_dir, key)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, 'w', encoding='utf-8', newline='') as fh:
        fh.write(new)
    return dst


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT, **kw)


def rel(p):
    return os.path.relpath(p, ROOT).replace('\\', '/')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=6)
    ap.add_argument('--only', default='')
    BASE = {'x86': '_port_build_x86', 'x86_64': '_port_build_x86_64', 'arm64': '_port_build_arm64'}.get(os.environ.get('PORT_ABI'), '_port_build')
    ap.add_argument('--out', default=os.path.join(ROOT, BASE, 'rel'))
    ap.add_argument('--maxerr', type=int, default=20)
    ap.add_argument('--errlog', default='')   # journal complet des erreurs (essais 64 bits, p64_casts.py)
    args = ap.parse_args()

    ndk = os.environ.get('NDK')
    if not ndk:
        sys.exit('definir NDK')
    bindir = os.path.join(ndk, 'toolchains', 'llvm', 'prebuilt', 'windows-x86_64', 'bin')
    # PORT_ABI=x86 : Android x86 32 bits (banc G0, MuMu), voir build_game.py.
    # PORT_ABI=x86_64 / arm64 (N7, 17/11) : mêmes options 64 bits que build_game.py (erreurs entier<->pointeur visibles).
    abi = os.environ.get('PORT_ABI')
    x86 = abi in ('x86', 'x86_64')
    triple = {'x86': 'i686-linux-android28', 'x86_64': 'x86_64-linux-android28',
              'arm64': 'aarch64-linux-android28'}.get(abi, 'armv7a-linux-androideabi28')
    cxx = os.path.join(bindir, triple + '-clang++.cmd')
    if abi in ('x86_64', 'arm64') and '-w' in bg.COMMON:
        bg.COMMON[bg.COMMON.index('-w')] = '-Wno-everything'
        bg.COMMON.extend(['-ferror-limit=0', '-Werror=int-to-pointer-cast', '-Werror=int-to-void-pointer-cast'])
    asm_flags = [] if x86 else (['-march=armv8-a'] if abi == 'arm64' else ['-march=armv7-a'])

    def alias_text(stubs):   # saut vers la vraie fonction : Thumb-2 sur ARM, jmp sur x86
        if x86:
            return ''.join('.globl %s\n.type %s,@function\n%s:\n    jmp %s\n' % (u, u, u, t) for u, t in stubs)
        return '.syntax unified\n.thumb\n' + ''.join(
            '.globl %s\n.type %s,%%function\n.thumb_func\n%s:\n    b.w %s\n' % (u, u, u, t) for u, t in stubs)
    objcopy = os.path.join(bindir, 'llvm-objcopy.exe')
    nm = os.path.join(bindir, 'llvm-nm.exe')

    mods = load_modules_py()
    UNITS = getattr(mods, 'UNITS', {})
    CFLAGS = getattr(mods, 'CFLAGS', {})
    UCFLAGS = getattr(mods, 'UNIT_CFLAGS', {})
    names = module_names()
    only = set(x for x in args.only.split(',') if x)
    if only:
        names = [n for n in names if n in only]
    if abi in ('x86_64', 'arm64') and not os.environ.get('PORT_REL_OUTILS'):
        # N7 (18/11) : 1er jalon 64 bits sans les outils de debug de Capcom (éditeurs t_*, Tools : menu L+START seulement ;
        # db_light.cpp appartient à une autre session). PORT_REL_OUTILS=1 les remet.
        names = [n for n in names if not (n.startswith('t_') or n == 'Tools')]

    os.makedirs(args.out, exist_ok=True)
    objdir = os.path.join(args.out, 'obj')
    os.makedirs(objdir, exist_ok=True)

    inc = ['-I' + rel(os.path.join(ROOT, 'include')), '-I.', '-Iport', '-Isrc',
           '-include', 'port/port_prelude.h']
    hdr = hashlib.md5()
    for base in (os.path.join(ROOT, 'include'), os.path.join(ROOT, 'port')):
        for dp, dn, fns in os.walk(base):
            for fn in sorted(fns):
                if fn.endswith(('.h', '.hpp', '.inc')):
                    hdr.update(open(os.path.join(dp, fn), 'rb').read())
    key_opts = hashlib.md5((' '.join(bg.COMMON + bg.CXXONLY + inc) + hdr.hexdigest()).encode()).hexdigest()[:8]
    stamp_path = os.path.join(args.out, 'stamps.json')
    try:
        stamps = json.load(open(stamp_path))
    except Exception:
        stamps = {}

    # ---- plan : (module, unit, source, objet)
    plan = []
    for mod in names:
        for ent in UNITS.get(mod, [(f'{mod}/{mod}.cpp', None)]):
            unit = ent[0]
            src = ent[2] if len(ent) > 2 and isinstance(ent[2], str) and ent[2] else unit
            path = os.path.join(ROOT, 'src', src)
            if not os.path.exists(path):
                plan.append((mod, unit, None, None))
                continue
            obj = os.path.join(objdir, '%s__%s.o' % (mod, unit.replace('/', '__')))
            plan.append((mod, unit, path, obj))

    def compile_one(job):
        mod, unit, path, obj = job
        if not path:
            return job, None, 'source absente', False
        h = hashlib.md5(open(path, 'rb').read()).hexdigest() + key_opts
        if stamps.get(obj) == h and os.path.exists(obj):
            return job, obj, None, True
        srcdir = os.path.dirname(path)
        path = sanitize(path, os.path.join(args.out, 'src'), mod + '__' + rel(path).replace('/', '__'))
        cmd = [cxx] + bg.CXXONLY + bg.COMMON + inc + ['-I' + rel(srcdir), '-Wno-address-of-temporary', '-DREL_MODULE=' + mod]
        cmd += CFLAGS.get(mod, [])
        cmd += [x for x in UCFLAGS.get(unit, []) if x.startswith('-D')]
        cmd += [rel(path), '-o', rel(obj)]
        r = run(cmd)
        if r.returncode != 0:
            if os.path.exists(obj):
                os.remove(obj)
            return job, None, r.stderr, False
        stamps[obj] = h
        return job, obj, None, False

    ok_objs, failed, cached = {}, [], 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for job, obj, err, was in ex.map(compile_one, plan):
            mod, unit = job[0], job[1]
            if obj:
                ok_objs.setdefault(mod, []).append(obj)
                cached += 1 if was else 0
            else:
                failed.append((mod, unit, err))
    json.dump(stamps, open(stamp_path, 'w'))
    if args.errlog:
        with open(args.errlog, 'w', encoding='utf-8', errors='replace') as fh:
            for mod, unit, err in failed:
                fh.write('===== %s %s\n%s\n' % (mod, unit, err or ''))
    print('%d objets de module (%d depuis le cache), %d en echec' % (sum(len(v) for v in ok_objs.values()), cached, len(failed)))
    for mod, unit, err in failed[:args.maxerr]:
        first = next((l for l in (err or '').splitlines() if ' error:' in l), (err or '?').strip()[:100])
        print('   %-8s %-34s %s' % (mod, unit, first[:120]))

    # ---- fusion
    ids = {}
    merged_ok = []
    for mod in names:
        objs = ok_objs.get(mod)
        if not objs:
            continue
        # un module dont une unite a echoue est ecarte en entier : mieux vaut un module absent
        # (le jeu le signale) qu'un module troue qui plante plus tard
        expected = [p for p in plan if p[0] == mod and p[2]]
        if len(objs) != len(expected):
            print('   module %s ecarte (%d/%d unites)' % (mod, len(objs), len(expected)))
            continue
        merged = os.path.join(args.out, mod + '.merged.o')
        r = run([cxx, '-r', '-nostdlib', '-fuse-ld=lld', '-Wl,--allow-multiple-definition', '-o', rel(merged)]
                + [rel(o) for o in sorted(objs)])
        if r.returncode != 0:
            print('   fusion %s : %s' % (mod, (r.stderr or '').strip()[:150]))
            continue
        merged_ok.append(mod)

    # ---- noms d'epoque GCC 2.95
    # Une fonction declaree `asm("setStatus__3cEmi")` est definie sous son nom clang
    # (`_ZN3cEm9setStatusEi`). Pour chaque symbole que le module reclame sans que personne le
    # definisse, on cherche la fonction dont le nom GCC 2.95 serait celui-la, et on genere un petit
    # relais (`b <fonction>`) compile dans le module.
    cxxfilt = os.path.join(bindir, 'llvm-cxxfilt.exe')
    def defined_funcs(paths):
        res = {}
        for k in range(0, len(paths), 60):
            ch = [rel(x) for x in paths[k:k + 60]]
            names = []
            for l in run([nm, '--defined-only', '-g'] + ch).stdout.splitlines():
                p_ = l.split()
                if len(p_) == 3 and p_[1] in 'TtWw' and p_[2].startswith('_Z'):
                    names.append(p_[2])
            if not names:
                continue
            dm = subprocess.run([cxxfilt], input='\n'.join(names) + '\n', capture_output=True, text=True, cwd=ROOT).stdout.splitlines()
            for a, b in zip(names, dm):
                g2 = gcc2_mangle.mangle(b)
                if g2:
                    res.setdefault(g2, a)
        return res

    dol_objs = sorted(glob.glob(os.path.join(ROOT, BASE, 'obj', '*.o')))
    g2map = defined_funcs(dol_objs)
    # Les fonctions `extern "C"` n'ont pas de signature dans leur nom : on les retrouve par leur nom nu.
    c_funcs = set()
    for k in range(0, len(dol_objs), 60):
        for l in run([nm, '--defined-only', '-g'] + [rel(x) for x in dol_objs[k:k + 60]]).stdout.splitlines():
            p_ = l.split()
            if len(p_) == 3 and p_[1] in 'TtWw' and not p_[2].startswith('_Z'):
                c_funcs.add(p_[2])

    def resolve_label(u, extra=None):
        t = g2map.get(u) or (extra or {}).get(u)
        if t:
            return t
        m_ = re.match(r'^(\w+?)__(?:F|\d)', u)
        if m_ and m_.group(1) in c_funcs:
            return m_.group(1)
        return None
    n_alias = 0
    for mod in list(merged_ok):
        mp = os.path.join(args.out, mod + '.merged.o')
        und = set()
        for l in run([nm, '-u', '-g', rel(mp)]).stdout.splitlines():
            p_ = l.split()
            if len(p_) >= 2:
                und.add(p_[-1])
        own = set()
        for l in run([nm, '--defined-only', '-g', rel(mp)]).stdout.splitlines():
            p_ = l.split()
            if len(p_) == 3:
                own.add(p_[2])
        mod_g2 = None
        stubs = []
        for u in sorted(und):
            if u.startswith('_Z') or u in own:
                continue
            if mod_g2 is None:
                mod_g2 = defined_funcs([os.path.join(args.out, m2 + '.merged.o') for m2 in merged_ok])
            tgt = resolve_label(u, mod_g2)
            if tgt:
                stubs.append((u, tgt))
        if not stubs:
            continue
        sp = os.path.join(args.out, mod + '.alias.s')
        with open(sp, 'w') as fh:
            fh.write(alias_text(stubs))
        so = os.path.join(args.out, mod + '.alias.o')
        r = run([cxx, '-c'] + asm_flags + ['-o', rel(so), rel(sp)])
        if r.returncode != 0:
            print('   alias %s : %s' % (mod, (r.stderr or '').strip()[:200]))
            continue
        tmp = os.path.join(args.out, mod + '.merged2.o')
        r = run([cxx, '-r', '-nostdlib', '-fuse-ld=lld', '-Wl,--allow-multiple-definition', '-o', rel(tmp), rel(mp), rel(so)])
        if r.returncode == 0:
            os.replace(tmp, mp)
            n_alias += len(stubs)
        else:
            print('   fusion alias %s : %s' % (mod, (r.stderr or '').strip()[:150]))
    # le DOL reclame lui aussi des noms d'epoque
    dol_all_def = set()
    dol_all_und = set()
    for k in range(0, len(dol_objs), 60):
        ch = [rel(x) for x in dol_objs[k:k + 60]]
        for l in run([nm, '--defined-only', '-g'] + ch).stdout.splitlines():
            p_ = l.split()
            if len(p_) == 3:
                dol_all_def.add(p_[2])
        for l in run([nm, '-u', '-g'] + ch).stdout.splitlines():
            p_ = l.split()
            if len(p_) >= 2:
                dol_all_und.add(p_[-1])
    dol_stubs = []
    for u in sorted(dol_all_und - dol_all_def):
        if u.startswith('_Z') or not re.search(r'__(F|\d)', u):
            continue
        tgt = resolve_label(u)
        if tgt:
            dol_stubs.append((u, tgt))
    sp = os.path.join(args.out, 'dol_alias.s')
    with open(sp, 'w') as fh:
        fh.write(alias_text(dol_stubs))
    r = run([cxx, '-c'] + asm_flags + ['-o', rel(os.path.join(args.out, 'dol_alias.o')), rel(sp)])
    if r.returncode != 0:
        print('   alias du DOL : %s' % (r.stderr or '').strip()[:200])
    print('%d alias de noms GCC 2.95 (+%d pour le DOL)' % (n_alias, len(dol_stubs)))

    def nm_syms(path, flag):
        out = run([nm, flag, '-g', rel(path)]).stdout
        res = set()
        for l in out.splitlines():
            p = l.split()
            if len(p) >= 2:
                res.add(p[-1])
        return res

    # ---- symboles partages entre modules
    # Dans le jeu d'origine un module appelle parfois une fonction d'un autre (la table des salles
    # de st1_0 designe les salles de st1_1...) : OSLink les relie au chargement. Ici tout est dans
    # le meme binaire, il faut donc garder globaux les symboles que quelqu'un d'autre reclame -
    # et eux seuls, pour que les centaines de `set`, `pRj`, `cEmControl` homonymes restent prives.
    dol_def, dol_und = set(), set()
    dol_objs = sorted(glob.glob(os.path.join(ROOT, BASE, 'obj', '*.o')))
    for k in range(0, len(dol_objs), 80):
        chunk = [rel(o) for o in dol_objs[k:k + 80]]
        for l in run([nm, '--defined-only', '-g'] + chunk).stdout.splitlines():
            p = l.split()
            if len(p) == 3:
                dol_def.add(p[2])
        for l in run([nm, '-u', '-g'] + chunk).stdout.splitlines():
            p = l.split()
            if len(p) >= 2:
                dol_und.add(p[-1])
    D = {m: nm_syms(os.path.join(args.out, m + '.merged.o'), '--defined-only') for m in merged_ok}
    U = {m: nm_syms(os.path.join(args.out, m + '.merged.o'), '-u') for m in merged_ok}
    empty = [m for m in merged_ok if '_prolog' not in D[m]]
    if empty:
        print('   modules sans _prolog (ecartes) : %s' % ' '.join(empty))
    merged_ok = [m for m in merged_ok if '_prolog' in D[m]]
    need = set(dol_und)
    for m in merged_ok:
        need |= U[m]
    need -= dol_def
    entry = {'_prolog', '_epilog', '_unresolved'}
    exports = {}
    for m in merged_ok:
        for sym in D[m]:
            if sym in need and sym not in entry and sym not in exports:
                exports[sym] = m
    print('%d symboles partages entre modules' % len(exports))

    # ---- localisation
    done = []
    for mod in merged_ok:
        merged = os.path.join(args.out, mod + '.merged.o')
        final = os.path.join(args.out, mod + '.o')
        loc = [x for x in D[mod] if x not in entry and exports.get(x) != mod]
        lf = os.path.join(args.out, mod + '.loc')
        open(lf, 'w').write('\n'.join(loc) + '\n')
        cmd = [objcopy]
        for e in sorted(entry):
            cmd += ['--redefine-sym', '%s=%s_%s' % (e, mod, e)]
        cmd += ['--localize-symbols=' + rel(lf), rel(merged), rel(final)]
        r = run(cmd)
        if r.returncode != 0:
            print('   objcopy %s : %s' % (mod, (r.stderr or '').strip()[:150]))
            continue
        try:
            ids[mod] = json.load(open(os.path.join(CFG, 'modules', mod, 'rel.json')))['module_id']
        except Exception:
            ids[mod] = -1
        done.append(mod)

    # ---- registre
    lines = ['// genere par port/build_rel.py - ne pas editer',
             'struct RelEntry { unsigned id; const char *name; void (*prolog)(); void (*epilog)(); void (*unresolved)(); };']
    for m in done:
        for e in ('prolog', 'epilog', 'unresolved'):
            lines.append('extern "C" void %s__%s();' % (m, e))
    lines.append('extern const RelEntry g_rel_table[] = {')
    for m in done:
        lines.append('    { %d, "%s", %s__prolog, %s__epilog, %s__unresolved },' % (ids[m], m, m, m, m))
    lines.append('    { 0, 0, 0, 0, 0 }')
    lines.append('};')
    open(os.path.join(args.out, 'rel_registry.cpp'), 'w').write('\n'.join(lines)+ '\n')
    print('%d modules prets' % len(done))


if __name__ == '__main__':
    main()
