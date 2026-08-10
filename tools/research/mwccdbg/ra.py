#!/usr/bin/env python3
"""ra.py lib/unit Func [--src file.c] [--out DIR] [--ver GC/2.6] [--quiet]

Run cadmic's mwcc-debugger on one CRI unit (the unit's real ninja flags, compiler GC/2.6 = 2.4.7 build
107, same codegen as our GC/2.7 build 108) and dump the frontend AST, backend PCode per pass and the
GPR/FPR interference graph + priority list for Func into DIR (default $MWCCDBG_OUT/<Func>,
$MWCCDBG_OUT = /tmp/mwccdbg.$USER/out).  Prints the regalloc-gpr-pass-1-assigned table afterwards.

--src file.c compiles that file instead of src/lib/<unit>.c (probe variants).

Needs two external checkouts that are not in the repo (see README.md "Build"):
  MWCCDBG_RW32      the retrowin32 binary (github.com/encounter/retrowin32, branch gdb-stub, built with
                    `cargo build -p retrowin32 -F x86-unicorn --profile lto`);
                    default <this dir>/retrowin32/target/lto/retrowin32
  MWCCDBG_DEBUGGER  cadmic's mwcc_debugger.py (github.com/cadmic/mwcc-debugger);
                    default <this dir>/mwcc-debugger/mwcc_debugger.py
"""
import os, re, subprocess, sys, shlex

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.environ.get('RE4_ROOT') or subprocess.run(
    ['git', 'rev-parse', '--show-toplevel'], cwd=HERE, capture_output=True, text=True, check=True).stdout.strip()
RW32 = os.environ.get('MWCCDBG_RW32', os.path.join(HERE, 'retrowin32/target/lto/retrowin32'))
DBG = os.environ.get('MWCCDBG_DEBUGGER', os.path.join(HERE, 'mwcc-debugger/mwcc_debugger.py'))
OUTBASE = os.environ.get('MWCCDBG_OUT', f"/tmp/mwccdbg.{os.environ.get('USER', 'user')}/out")

def main():
    a = sys.argv[1:]
    unit = a[0].replace('.c', ''); func = a[1]
    src = f'src/{unit}.c'; out = None; ver = 'GC/2.6'; quiet = False
    i = 2
    while i < len(a):
        if a[i] == '--src': src = a[i+1]; i += 2
        elif a[i] == '--out': out = a[i+1]; i += 2
        elif a[i] == '--ver': ver = a[i+1]; i += 2
        elif a[i] == '--quiet': quiet = True; i += 1
        else: i += 1
    out = out or os.path.join(OUTBASE, func)
    for need, what in ((RW32, 'MWCCDBG_RW32 (retrowin32 binary)'), (DBG, 'MWCCDBG_DEBUGGER (mwcc_debugger.py)')):
        if not os.path.exists(need):
            sys.exit(f'{need} not found: set {what}, see README.md')
    cmd = subprocess.run(['ninja', '-t', 'commands', f'build/G4BE08/src/{unit}.o'], cwd=ROOT,
                         capture_output=True, text=True).stdout
    line = [l for l in cmd.splitlines() if 'mwcceppc' in l][0]
    flags = re.search(r'mwcceppc\.exe (.*) -lang=c -MD -c ', line).group(1)
    # retrowin32 maps the cwd; keep include paths relative to ROOT
    flags = flags.replace(ROOT + '/', '')
    if os.path.isabs(src):
        src_rel = os.path.relpath(src, ROOT)
    else:
        src_rel = src
    args = f'build/compilers/{ver}/mwcceppc.exe {flags} -lang=c -c {src_rel} -o {out}/probe.o'
    os.makedirs(out, exist_ok=True)
    r = subprocess.run([sys.executable, DBG, '-e', RW32, '-a', args, func, out], cwd=ROOT,
                       capture_output=True, text=True)
    if not os.path.exists(os.path.join(out, 'regalloc-gpr-pass-1-assigned.txt')):
        print(r.stdout[-3000:]); print(r.stderr[-3000:]); sys.exit('no regalloc dump (function name? compile error?)')
    if not quiet:
        print(open(os.path.join(out, 'regalloc-gpr-pass-1-assigned.txt')).read())
    print(f'dumps in {out}')

if __name__ == '__main__':
    main()
