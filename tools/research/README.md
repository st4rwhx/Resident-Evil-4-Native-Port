# tools/research — the compiler-research kit

Scripts and models used to read the two original compilers (SN ProDG GCC 2.95.3 for the game, Metrowerks
CodeWarrior 2.4.7 for the CRI middleware) while matching. None of this is needed to build; `docs/matching.md`
("Tooling kit") says when to reach for what, and `docs/research/` is the log of what was found with it.
Every script takes the repo root from `git rev-parse --show-toplevel` (override: `RE4_ROOT=<dir>`), and all of
them need a configured tree (`configure.py` run, `build/build.ninja`, the split objects and `build/G4BE08/asm/`).

**`kit/`** — `variant.sh <unit> <variant-src> [FUNC]` compiles a variant of one unit's source with the unit's
exact production command (taken from `ninja -t commands`) into `/tmp`, runs `tools/bytecmp.py` on the result and
prints an objdiff side-by-side per differing function; `rtl.sh` does the same compile and keeps the GCC RTL dumps
(`-dj -ds -dS -dR -dg -dl -dG -dL -fsched-verbose-9`). GCC and MWCC units, DOL and REL. Needs only what the
normal build already fetched (`objdiff-cli`, `build/tools/dtk`, wibo and the compilers under `build/compilers/`).

**`sngdbg/`** — the debug build of the SN GCC `cc1plus`/`cc1`: `patches/dbg-hooks.patch` adds env-var-gated
`fprintf(stderr)` hooks (`GDBG=1` global.c allocation order and priorities, `LADBG=1` local-alloc qty order,
`SCHDBG=1` haifa issue order, `CSEDBG=1|2` cse flushes / per-SET lookups, `NOMALLOC=1` / `GFORCE=` / `NOEQV=`
oracles that change codegen) to the production compiler source; with no env var the binary is byte-identical to
production. `rebuild.sh` builds it into `build/sngdbg/` (untracked) from `tools/sn-gcc/src`, which
`tools/sn-gcc/build.sh` produces from SN's GPL source drop (not in the repo; see `tools/sn-gcc/build.sh`).
Use it through the kit with `CC1DIR=build/sngdbg`.

**`mwccdbg/`** — the MWCC side: `ra.py lib/<unit> <Func>` runs cadmic's `mwcc-debugger` under `retrowin32`
(gdb-stub branch) on the unit's real flags and dumps the frontend AST, the backend PCode after every pass and
the register allocator's interference graph; `chaitin.py` is a validated Python replay of the 2.4.7 Chaitin GPR
allocator, `sched.py` of its PCode list scheduler (with `scheddump.sh`/`schedtrace.sh`/`schedcheck.sh` gdb
drivers), `rasum.py`/`rasim.py`/`ghostwhatif.py`/`schedwhatif.py`/`blkflags.py` read and perturb those dumps.
Needs two external checkouts that are not in the repo — `mwcc-debugger` and a built `retrowin32` (Rust, cargo,
cmake) — plus an x86 host `gdb` with Python; the exact commands are in `mwccdbg/README.md`. Their default
location is inside `mwccdbg/` (git-ignored), or point `MWCCDBG_DEBUGGER` / `MWCCDBG_RW32` at them.

**`casetree.py`**, **`xjump.py`** — stand-alone models of GCC 2.95.3's switch compare-tree expansion
(stmt.c/jump.c, validated on 836 switches) and of jump2 cross-jumping (which identical arm tail survives);
no external dependencies. Both have their usage in the module docstring.
