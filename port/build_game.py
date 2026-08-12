# -*- coding: utf-8 -*-
"""Construit le jeu lui-meme pour l'appareil : les unites du DOL + la couche de plateforme.

Ce que ce script ne fait **pas** est aussi important que ce qu'il fait : il ne compile pas
`src/lib/`, le SDK Nintendo decompile. Ce code-la parle au materiel du GameCube (registres,
write-gather pipe, exceptions PowerPC) et n'a aucun sens sur un telephone. C'est precisement
lui que la couche `port/` remplace, fonction par fonction.

La compilation est mise en cache par empreinte du source **et** des options : une session qui
reprend ne recompile que ce qui a change, et un changement d'option invalide tout, ce qui evite
la classe de bugs ou la moitie des objets est compilee avec d'anciens `#define`.

usage : NDK=<racine ndk> python port/build_game.py [--jobs N] [--link] [--quiet]
"""
import argparse
import concurrent.futures
import hashlib
import importlib.util
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# -DDEBUG=1 n'est pas negociable : c'est lui qui rend les fonctions de soumission de sommets
# externes dans GXVert.h (sinon elles ecrivent en inline dans le write-gather pipe du materiel)
# et qui desactive les macros de os.h masquant OSPhysicalToCached et consorts.
COMMON = ['-DRE4_PORT', '-DDEBUG=1', '-DVERSION_G4BE08', '-D__GEKKO__',
          '-DSDK_REVISION=1', '-DBUILD_VERSION=0',
          os.environ.get('PORT_OPT', '-Oz'), '-fno-exceptions', '-fno-rtti', '-w', '-c',
          # Tables de deroulement et pointeur de cadre : sans elles le tombstone Android ne
          # donne que la premiere adresse, souvent fausse (une instruction de retour), et on
          # cherche le bug au mauvais endroit. Le cout en taille est sans importance ici.
          '-funwind-tables', '-fno-omit-frame-pointer', '-g1',
          # Code d'epoque, ecrit pour le compilateur Metrowerks : des fonctions non void se terminent
          # sans return (cSofdec::startApp : a -O2 clang jugeait la suite inatteignable et le code
          # tombait dans les donnees, SIGILL au film d'ouverture), des types se lisent l'un par
          # l'autre, des calculs signes debordent volontairement, et l'image doit rester celle des
          # flottants separes (pas de fusion multiplication-addition).
          '-fno-strict-return', '-fno-strict-aliasing', '-fwrapv', '-ffp-contract=off']
# gnu++11 et pas gnu++17 : gnu++17 supprime `register` et rouvre 44 erreurs. -fdeclspec en
# resout 369 a lui seul.
# -Wno-c++11-narrowing : snd_str4.cpp fait `switch (u32)` sur des constantes DVD_STATE_*
# negatives. C'est legal pour le compilateur d'origine et sans consequence ici - la valeur
# comparee est la meme apres conversion - mais c'est une **erreur** en C++11.
CXXONLY = ['-std=gnu++11', '-fdeclspec', '-Wno-c++11-narrowing']
CONLY = ['-std=gnu99', '-fdeclspec', '-fgnu89-inline']   # extern inline des en-tetes d'epoque (fake_tgmath.h) : pas de definition externe

PORT_UNITS = ['os', 'dvd', 'vi', 'pad', 'gx', 'texture', 'texobj',
              'memfill', 'yz2', 'mtx', 'misc', 'aram', 'thread', 'endian', 'crash', 'prodg_rt', 'port_log',
              'stubs_generated', 'stubs_link', 'main_port', 'arm_hooks', 'card', 'rel', 'rel_missing', 'movie_skip', 'njlink']


# Unites du jeu volontairement **exclues** du portage.
#
# `sn_malloc.c` remplace malloc, free, realloc et calloc par des souches qui impriment une
# erreur et rendent NULL. C'etait du code mort dans le binaire d'origine (ProDG les avait
# elaguees, seuls leurs messages survivent dans le DOL), mais lie ici elles prennent la place de
# celles de la plateforme **pour tout le processus** - y compris pour l'allocateur interne de
# Bionic, qui tombe alors avant meme d'arriver au jeu. Mesure : sans cette exclusion, le premier
# lancement meurt sur "new failed to allocate 4 bytes".
EXCLUDED = {
    # La libc que ProDG liait dans le DOL. Le decompile la reproduit unite par unite, et ces
    # unites **remplacent celles de la plateforme pour tout le processus** : leur memcpy, leur
    # printf, leur fopen deviennent ceux de tout le monde, y compris ceux de Bionic.
    #
    # Ce n'est pas une gene theorique, c'est ce qui bloquait le demarrage :
    #   * `sn_malloc.c` rend NULL et imprime une erreur - l'allocateur interne de Bionic tombe
    #     avant meme d'arriver au jeu ;
    #   * `fopen.c` tient quatre flux et repond "too many open streams" des le premier appel ;
    #   * `sscanf` construit alors un FILE incomplet, et `__srefill` dereference un pointeur
    #     nul juste apres la premiere lecture du disque (trouve par le tombstone Android, pas
    #     par relecture : le symbole fautif etait `__srefill`, a 30 unites de l'appelant).
    #
    # Le portage n'a aucune raison de garder cette libc : celle de la plateforme fait la meme
    # chose, correctement et plus vite. Ces unites ne contiennent pas une ligne de RE4.
    'game/sn_malloc.c', 'game/sn_buf.c', 'game/impure.c',
    'game/fopen.c', 'game/fread.c', 'game/fflush.c', 'game/fwalk.c', 'game/makebuf.c',
    'game/refill.c', 'game/ungetc.c', 'game/stdio.c',
    'game/closer.c', 'game/fstatr.c', 'game/lseekr.c', 'game/readr.c', 'game/writer.c',
    'game/printf.c', 'game/fprintf.c', 'game/vprintf.c', 'game/sprintf.c', 'game/vsprintf.c',
    'game/vfprintf.c', 'game/vfiprintf.c',
    'game/sscanf.c', 'game/vfscanf.c',
    'game/atof.c', 'game/atoi.c', 'game/strtod2.c', 'game/strtol.c', 'game/strtoul.c',
    'game/qsort.c', 'game/exit.c', 'game/errno.c', 'game/locale.c', 'game/mbtowc_r.c',
    'game/math_support.c',
    'game/isdigit.c', 'game/isspace.c', 'game/tolower.c', 'game/ctype_.c',
    'game/memcpy.c', 'game/memset.c', 'game/memcmp.c', 'game/memmove.c', 'game/memchr.c',
    'game/strcat.c', 'game/strchr.c', 'game/strcmp.c', 'game/strcpy.c', 'game/strlen.c',
    'game/strncmp.c', 'game/strncpy.c', 'game/strncat.c', 'game/strpbrk.c', 'game/strrchr.c',
    'game/strspn.c', 'game/strcspn.c', 'game/strstr.c',
    # Assembleur PowerPC brut, remplace par port/memfill.cpp et port/yz2.cpp.
    'game/memset_2.s', 'game/yz2asm.s',
}


# Bibliotheques de la decompilation qui sont du pur logiciel et se compilent telles quelles : le
# middleware CRI (films Sofdec : sfd/mpv/mps/dct/mws/sfx..., son ADX : adx/sj/lsc/rna...) et la
# partie logicielle du son Nintendo (synthetiseur, sequenceur, mixeur, AXART, effets AXFX). Le
# materiel qu'elles pilotent (AX/DSP, GX, DVD, OS) reste fourni par la couche du portage ; les
# souches faibles du portage s'effacent devant ces vraies definitions. RE4_NOLIB=1 : sans elles.
# Phase N1 : le son logiciel Nintendo seulement ; CRI (films, ADX) et la reverberation (asm PowerPC) en phase N4.
LIB_SOFT = re.compile(r'^lib/(mix\.c|axart|syn|seq\.c|delay|axfx)')


def units():
    p = os.path.join(ROOT, 'config', 'G4BE08', 'objects.py')
    spec = importlib.util.spec_from_file_location('objects', p)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    keep_lib = not os.environ.get('RE4_NOLIB')
    return [u for u in mod.UNITS if u not in EXCLUDED and
            (not u.startswith('lib/') or (keep_lib and LIB_SOFT.match(u)))]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--jobs', type=int, default=max(4, (os.cpu_count() or 4)))
    ap.add_argument('--out', default=os.path.join(ROOT, '_port_build'))
    ap.add_argument('--link', action='store_true')
    ap.add_argument('--quiet', action='store_true')
    ap.add_argument('--maxerr', type=int, default=15)
    ap.add_argument('--errlog', default=None)
    args = ap.parse_args()

    ndk = os.environ.get('NDK')
    if not ndk:
        sys.exit('definir NDK sur la racine de android-ndk-r26d')
    bindir = os.path.join(ndk, 'toolchains', 'llvm', 'prebuilt', 'windows-x86_64', 'bin')
    cxx = os.path.join(bindir, 'armv7a-linux-androideabi28-clang++.cmd')
    cc = os.path.join(bindir, 'armv7a-linux-androideabi28-clang.cmd')
    for t in (cxx, cc):
        if not os.path.exists(t):
            sys.exit('compilateur introuvable : %s' % t)

    objdir = os.path.join(args.out, 'obj')
    os.makedirs(objdir, exist_ok=True)
    stamp_path = os.path.join(args.out, 'stamps.json')
    try:
        stamps = json.load(open(stamp_path))
    except Exception:
        stamps = {}

    inc = ['-I' + os.path.join(ROOT, 'include'), '-I' + ROOT, '-I' + os.path.join(ROOT, 'port'), '-I' + os.path.join(ROOT, 'src'), '-idirafter', os.path.join(ROOT, 'src', 'lib', 'cri'), '-idirafter', os.path.join(ROOT, 'src', 'lib'),
           '-include', os.path.join(ROOT, 'port', 'port_prelude.h')]
    # L'empreinte du cache couvre **aussi les en-tetes**. Sans cela, modifier un `#ifdef
    # RE4_PORT` dans include/ ne recompile rien et le lien echoue sur un symbole qu'on vient
    # pourtant de supprimer - une demi-heure perdue a chercher au mauvais endroit, une fois.
    hdr = hashlib.md5()
    for base in (os.path.join(ROOT, 'include'), os.path.join(ROOT, 'port')):
        for dp, dn, fns in os.walk(base):
            for fn in sorted(fns):
                if fn.endswith(('.h', '.hpp', '.inc')):
                    hdr.update(open(os.path.join(dp, fn), 'rb').read())
    key_opts = hashlib.md5((' '.join(COMMON + CXXONLY + CONLY + inc)
                            + hdr.hexdigest()).encode()).hexdigest()[:8]

    jobs = []
    for u in units():
        src = os.path.join(ROOT, 'src', u)
        if not os.path.exists(src):
            continue
        jobs.append((u, src))
    for u in PORT_UNITS:
        src = os.path.join(ROOT, 'port', u + '.cpp')
        if os.path.exists(src):
            jobs.append(('port/' + u + '.cpp', src))

    def compile_one(job):
        unit, src = job
        obj = os.path.join(objdir, unit.replace('/', '__').replace('\\', '__') + '.o')
        h = hashlib.md5(open(src, 'rb').read()).hexdigest() + key_opts
        if stamps.get(unit) == h and os.path.exists(obj):
            return unit, obj, None, True
        tool = cc if src.endswith('.c') else cxx
        opts = CONLY if src.endswith('.c') else CXXONLY
        cmd = [tool] + opts + COMMON + inc + [src, '-o', obj]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            # Un objet perime doit disparaitre, sinon un lien ulterieur reussirait avec du
            # code d'une version precedente - le genre d'incoherence indetectable a la lecture.
            if os.path.exists(obj):
                os.remove(obj)
            return unit, None, r.stderr, False
        return unit, obj, None, False

    ok, failed, cached = [], [], 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        for unit, obj, err, was_cached in ex.map(compile_one, jobs):
            if obj:
                ok.append(obj)
                if was_cached:
                    cached += 1
                else:
                    stamps[unit] = hashlib.md5(
                        open(os.path.join(ROOT, 'src', unit) if not unit.startswith('port/')
                             else os.path.join(ROOT, unit), 'rb').read()).hexdigest() + key_opts
            else:
                failed.append((unit, err))
                stamps.pop(unit, None)

    json.dump(stamps, open(stamp_path, 'w'))
    if args.errlog:
        with open(args.errlog, 'w', encoding='utf-8', errors='replace') as fh:
            for unit, err in failed:
                fh.write('===== %s\n%s\n' % (unit, err or ''))
    print('%d unites compilees (%d depuis le cache), %d en echec, sur %d'
          % (len(ok), cached, len(failed), len(jobs)))
    if failed and not args.quiet:
        for unit, err in failed[:args.maxerr]:
            first = next((l for l in (err or '').splitlines() if ' error:' in l), '?')
            print('   %-40s %s' % (unit, first[:110]))
        if len(failed) > args.maxerr:
            print('   ... et %d autres' % (len(failed) - args.maxerr))

    if not args.link:
        return

    # Modules REL compiles a l'avance (port/build_rel.py) : un objet par module + le registre.
    rel_objs = []
    reldir = os.path.join(args.out, 'rel')
    reg = os.path.join(reldir, 'rel_registry.cpp')
    if os.path.exists(reg):
        regobj = os.path.join(reldir, 'rel_registry.o')
        rr = subprocess.run([cxx] + CXXONLY + COMMON + ['-o', os.path.relpath(regobj, ROOT).replace(os.sep, '/'),
                             os.path.relpath(reg, ROOT).replace(os.sep, '/')], capture_output=True, text=True, cwd=ROOT)
        if rr.returncode != 0:
            print('registre des modules : ' + rr.stderr[:300])
            sys.exit(1)
        rel_objs = [regobj] + sorted(p for p in (os.path.join(reldir, f) for f in os.listdir(reldir))
                                     if p.endswith('.o') and not p.endswith(('.merged.o', '.merged2.o', '.alias.o')) and p != regobj)

    rsp = os.path.join(args.out, 'game.rsp')
    with open(rsp, 'w') as fh:
        for o in ok + rel_objs:
            fh.write('"%s"\n' % os.path.relpath(o, ROOT).replace('\\', '/'))
    exe = os.path.join(args.out, 're4')
    cmd = [cxx, '@' + rsp, '-o', exe, '-lEGL', '-lGLESv3', '-llog', '-landroid', '-static-libstdc++',
           # sans cela lld s'arrete a 20 erreurs, et on decouvre les symboles manquants
           # vingt par vingt au lieu d'une seule fois.
           '-Wl,--error-limit=0']
    # Le .cmd de clang coupe les arguments aux espaces : chemins relatifs, depuis la racine du depot
    # ("RE4 PROJECT" contient une espace, c'etait la cause du faux "lien en echec").
    cmd[1] = '@' + os.path.relpath(rsp, ROOT).replace(os.sep, '/')
    cmd[cmd.index('-o') + 1] = os.path.relpath(exe, ROOT).replace(os.sep, '/')
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode != 0:
        und = [l for l in r.stderr.splitlines() if 'undefined symbol' in l]
        dup = [l for l in r.stderr.splitlines() if 'duplicate symbol' in l]
        print('lien en echec : %d symboles non definis, %d doublons' % (len(und), len(dup)))
        for l in (und + dup)[:400]:
            print('   ' + l.strip()[:120])
        sys.exit(1)
    print('lie : %s (%d octets)' % (exe, os.path.getsize(exe)))


if __name__ == '__main__':
    main()
