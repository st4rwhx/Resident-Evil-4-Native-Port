# kit — the variant harness for re4 matching

Compile a variant of one unit's source with the unit's exact production command and judge it against
the split object, without touching `build/` or the tree. Everything runs from the repo root (the
default; `RE4_ROOT=<dir>` overrides it), writes only under `/tmp` (`/tmp/kit.$USER/<unit>.XXXX/` by
default, or `--out DIR`), and does no ninja build (`ninja -t commands` only reads build.ninja).

Needs a configured tree (`build/build.ninja` from `configure.py`, the split objects and
`build/G4BE08/asm/`), `objdiff-cli` and `build/tools/dtk` (both fetched by the normal build).

## variant.sh — compile a variant of one unit with its production command and judge it
```sh
tools/research/kit/variant.sh <unit> <variant-src> [FUNC...] [--out DIR] [--cc1dir DIR] [--all] [--no-diff] [--dtk]
```
- `unit`: `game/title`, `lib/sfd_tim` (DOL; GCC and MWCC) or `<mod>/<file>` for a REL unit
  (`st2_1/r20e`, `t_esp/db_widget`). `.cpp`/`.c` suffix optional.
- `variant-src`: any file; it is copied to `OUT/src/<original basename>` (the object keeps the unit's
  stem, so ngccc.py's linkonce placement and fold_linkonce/strip_unused work unchanged; the
  original source directory is added as the first include dir so `#include "x.h"` next to the
  original resolves). `__FILE__` would show the OUT path; the game sources use `#line` anyway.
- What runs: the last line of `ninja -t commands build/G4BE08/src/<unit>.o`, tokenised, paths made
  absolute, source/object redirected to OUT; then the `&&`-chained `fold_linkonce.py` /
  `strip_unused.py` steps on the OUT object (dependency-file steps dropped); then
  `OBJ=OUT/<stem>.o python3 tools/bytecmp.py <unit> [FUNC...]`; then a side-by-side per function
  (`FUNC...`, or the functions bytecmp lists as differing, at most 6) from `objdiff-cli diff -1
  <split object> -2 <variant object> FUNC` — the tools/fdiff.py format (`*` = differing line,
  ARG_MISMATCH/INSERT/DELETE/REPLACE), relocations resolved symbolically.
- GCC units go through `tools/ngccc.py` in-process: same cpp defines, same wibo 120 s hang
  timeout with 3 retries, `-DREL_MODULE` module linkonce handling. MWCC units run the wibo +
  sjiswrap + mwcceppc command with the same timeout/retry.
- `CC1DIR=<dir>` (env) or `--cc1dir <dir>`: replace `--native-dir` (the GCC cc1plus/cc1 dir) —
  the hooked compiler built by `tools/research/sngdbg/rebuild.sh`; `GDBG=1`, `LADBG=1`, `NOMALLOC=1`
  pass straight through the environment to it (see tools/research/sngdbg/README.md). MWCC has no
  equivalent: use `tools/research/mwccdbg`.
- `--all`: every line of the side-by-side, not only the differing ones. `--no-diff`: bytecmp only.
- `--dtk`: side-by-side from `build/tools/dtk elf disasm OUT/<stem>.o` against
  `build/G4BE08/asm/<unit>.s` (function-relative offsets/labels) instead of objdiff. dtk fills the
  relocation ADDENDS into the instruction fields of a relocatable object, so branch targets and
  `@sda21`/`@l` immediates are wrong there; use it only for a target-style listing of the bytes.
- Exit is 0 whenever the compile succeeded; read `VERDICT:` and the per-function word counts.

Typical loop (0.5-1 s per variant):
```sh
cd <repo root>
cp src/game/foo.cpp /tmp/kit.$USER/foo.cpp        # edit the copy, never the tree, until it matches
tools/research/kit/variant.sh game/foo /tmp/kit.$USER/foo.cpp fooFunc__FP4Work
tools/research/kit/variant.sh lib/sfd_tim /tmp/kit.$USER/sfd_tim.c sftim_Tc2Time59N
tools/research/kit/variant.sh st2_1/r20e /tmp/kit.$USER/r20e.cpp                     # REL unit, all differing functions
GDBG=1 CC1DIR=<sngdbg dir> tools/research/kit/variant.sh game/foo /tmp/kit.$USER/foo.cpp fooFunc__FP4Work 2> /tmp/kit.$USER/gdbg.log
```
Then apply the winning edit to the tree and confirm with `ninja` + `tools/bytecmp.py` as usual.

Validation (2026-09-11): `variant.sh game/title src/game/title.cpp` and `variant.sh lib/sfd_tim
src/lib/sfd_tim.c` and `variant.sh st2_1/r20e src/st2/r20e.cpp` -> IDENTICAL and `cmp`-equal to the
ninja-built objects; a `asm volatile("" : : : "memory")` variant of titleMain shows 95 words / 3
diff lines, a `frm * 1001` variant of sftim_Tc2Time59N shows 1 word / `mulli ... 0x3e9`.

## rtl.sh — the RTL dumps of a variant (GCC units only)
```sh
tools/research/kit/rtl.sh <unit> <variant-src> [--out DIR] [--cc1dir DIR] [-dX ...]
```
Same compile as variant.sh but the temp dir is OUT, so `OUT/<stem>.i` (the cpp output) and every
dump stay: default flags `-dj -ds -dS -dR -dg -dl -dG -dL -fsched-verbose-9` give `<stem>.i.jump`,
`.cse` (cse1+cse2, `-ds`), `.sched` (sched1 with the haifa verbose log), `.sched2`, `.greg`
(global alloc: `;; N regs to allocate:` order + `Register dispositions`), `.lreg` (local alloc +
flow's REG_N_REFS/live lengths), `.gcse` (hash values / bucket order of the PRE candidates), `.loop`
(movables, givs/bivs, `insn_count`). Pass your own `-dX` set to replace the default. Prints the
dump list; the object is built too (`OUT/<stem>.o`), so `OBJ=OUT/<stem>.o python3 tools/bytecmp.py
<unit>` works on it. For an MWCC unit it refuses and points at tools/research/mwccdbg.

Raw re-run of the compiler proper on the kept `.i` (hooks, other flags), 0.2 s:
```sh
GDBG=1 <sngdbg dir>/cc1plus -O2 -mfast-cast -quiet OUT/foo.i -o /dev/null 2>&1 | grep '^GORDER.*fooFunc'
```

## tools/research/mwccdbg — the MWCC (CRI lib/) side, see its README.md
- `ra.py lib/unit Func [--src file.c] [--out DIR]` — cadmic's mwcc-debugger under retrowin32: frontend
  AST, backend PCode after every pass, the GPR/FPR interference graph and priority list for one
  function (~3 s; GC/2.6, codegen-identical to our GC/2.7).
- `rasum.py DIR [--nb]` — one line per node of the priority list; `rasim.py DIR [--drop A B ..]` —
  simulate the Chaitin removal order; `chaitin.py DIR [--pass N] [--check]` — the validated
  allocator model (predict the colouring after an edit to `g.adj`/`g.cost`/`g.order` BEFORE
  changing the source).

## Files
`variant.py` (all logic; `variant.sh` and `rtl.sh` exec it), this README.

- `KIT_NLINES=N` (env): row limit of the objdiff side-by-side (default 400).
