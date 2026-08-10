# sngdbg — the SN GCC 2.95.3 (v1.79) debug cc1plus/cc1

Built 2026-09-11 from `tools/sn-gcc/src` (linux-host + shipped-build-temp-flags patches, i.e. the
installed production source) plus `patches/dbg-hooks.patch`: env-var-gated `fprintf(stderr)` hooks.
**With no env var set the binary produces byte-identical objects to the production compiler**
(verified: `game/title.o` compiled through tools/ngccc.py with `--native-dir $SNGDBG_DIR`
`cmp`s equal to `build/G4BE08/src/game/title.o`; it also stays identical with all hooks ON, the
hooks only print). The production compiler in `build/compilers/ProDG/3.9.3-v1.79/` is untouched.

sha1 (2026-09-11): cc1plus 6756521fb374df53537bc1029720aed49aefe546, cc1 0402c03788a585d971e66bef8dbcf6dd265bab5c
(production cc1plus 457337da552aa68676ea47f7d5c171d2e57038c8).

## Layout
The binaries are NOT in the repo: `rebuild.sh` builds them into `$SNGDBG_DIR` (default
`<repo>/build/sngdbg`, untracked) from `tools/sn-gcc/src` — which itself is produced by
`tools/sn-gcc/build.sh` from the SN GPL source drop (`SN_GCC_SRC`, see tools/sn-gcc/README.md).
- `$SNGDBG_DIR/cc1plus`, `cc1` — the hooked binaries (`--native-dir $SNGDBG_DIR` works as-is: ngccc.py
  looks for `<native-dir>/cc1plus` or `/cc1`). There is no env override in tools/ngccc.py; swap it via
  the `--native-dir` argument, which is what the kit's `CC1DIR=` does.
- `$SNGDBG_DIR/src/`, `obj/`, `Makefile` — copy of tools/sn-gcc with the hooks applied; `make all`
  there rebuilds in ~1 s after an edit (only the touched .c files recompile).
- `patches/dbg-hooks.patch` — the hooks as a `patch -p0` against tools/sn-gcc/src (7 files).
- `rebuild.sh` — copy production src/obj/Makefile into `$SNGDBG_DIR`, apply the hooks and build
  (run it again when tools/sn-gcc changes); `rebuild.sh quick` = `make all` there.

## Usage
Through the kit (recommended; compiles a variant with the unit's exact production command):
```sh
cd <repo root>; export CC1DIR=build/sngdbg     # = $SNGDBG_DIR
GDBG=1 tools/research/kit/variant.sh game/foo /path/variant.cpp FUNC 2> /tmp/kit.$USER/gdbg.log
LADBG=1 tools/research/kit/variant.sh game/foo /path/variant.cpp FUNC 2> /tmp/kit.$USER/ladbg.log
NOMALLOC=1 tools/research/kit/variant.sh t_esp/t_esp /path/variant.cpp InitTool__Fv
GDBG=1 tools/research/kit/rtl.sh game/foo /path/variant.cpp      # dumps + hook output
```
Raw (on a preprocessed `.i`, e.g. the one rtl.sh leaves in its out dir):
```sh
GDBG=1 $CC1DIR/cc1plus -O2 -mfast-cast -quiet OUT/foo.i -o /dev/null 2>&1 | grep '^GORDER.*FuncName'
LADBG=1 $CC1DIR/cc1plus -O2 -mfast-cast -quiet OUT/foo.i -o /dev/null 2>&1 | grep '^LADBG.*FuncName'
```
The function name printed is `current_function_name` = the demangled signature with spaces
(`void titleMain(TitleWork *)`); grep by the bare name.

## Hooks (all print to stderr; nothing changes codegen except NOMALLOC)
### `GDBG=1` — global.c (global_alloc / find_reg), haifa-sched.c, loop.c
One line per allocno in ALLOCATION order, printed BEFORE find_reg and completed with its result:
```
GORDER <fn> <i>: reg <pseudo> refs N len L calls C size S pri P used <r14..r31>/<f14..f31> conf <..> smpref <..> pref <..> cpref <..> [scan passK rX] -> rY
```
- `GDBGV=1` (with GDBG=1): every bit string is prefixed with r0..r12 and a ':' (volatile GPRs, 2026-09-12).
- `pri` = `floor(log2(refs))*refs/len*10000*size` (allocno_compare; ties by allocno number).
- `used` = `regs_used_so_far` (r14..r31 then f14..f31 as 18 bits each, 1 = already handed out to a
  local qty / an earlier allocno / regs_ever_live), `conf` = `hard_reg_conflicts`, `smpref` =
  `regs_someone_prefers` (preferred by a conflicting LOWER-priority allocno: excluded in pass 0),
  `pref`/`cpref` = the allocno's own (copy) preferences.
- `[scan pass0 rX]` = found among already-used regs not in smpref; `pass1` = first use of a
  callee-saved reg in reg_alloc_order (r31, r30, ...) or a smpref one; `alt` = alternate class try;
  `-> rY` is the final choice after the copy-preference / preference override; `-> spill -1` = none.
  This is the "which allocno excludes register X for allocno Y" tool (docs/research/ "Stage rooms, st2_1 pass 10"
  initPuzzle, "Tool RELs, db_mod pass 5" register-allocation facts).
- `;; LL reg R block B seg N calls C` — haifa-sched.c after sched1 only: every live-length segment
  a pseudo accumulates per block (their sum becomes the REG_LIVE_LENGTH global.c divides by;
  `(open at block start)` = segment still live at the block head).
- `LOOPDBG <fn> insn U reg R [thr T sav S life L ic I am A mo M] -> move|stay` — loop.c
  move_movables' hoist test per movable: `thr*sav*life >= ic` (ic*2 if moved once); `thr` starts at
  `(1|2)*(1+n_non_fixed_regs)` and drops 3 per moved movable, `ic` = the loop's real insn count.
  The "dead test for the loop.c insn_count threshold" lever is read off this line.
### `LADBG=1` — local-alloc.c (block_alloc)
One line per qty of each basic block, in qty_order = QTY_CMP_PRI order (ties by qty number), printed
after the block's allocation:
```
LADBG <fn> b<block> <S|L> q<qty> reg<first pseudo> refs N birth B death D pri P size S cls C calls K sugg <hard regs> csugg <hard regs> -> <hard reg or -1>
```
- `S` = placed by the suggested-register loop first (qty_sugg order: copy suggestions, then fewer
  suggestions, then priority), `L` = placed (or not) in this priority order.
- `birth`/`death` = the block scan's suids (2 per insn); `pri` = `floor(log2(refs))*refs*size/(death-birth)*10000`.
- Remember the 3-qty `case 3` partial sort quirk and the fake_birth/fake_death lifetime extension
  (docs/research/ "DOL sweep 21b", "Tool RELs, db_mod pass 5" (5)); local-alloc never hands out r31.
### `NOMALLOC=1` — calls.c (special_function_p)
Forces `is_malloc = 0` for every callee (`malloc/calloc/realloc/__builtin_new/__builtin_vec_new/__nw/__vn`),
so no call result carries a REG_NOALIAS note and alias.c gives it no unique base. CHANGES CODEGEN:
it is the oracle for "the target compiles as if this `new` result were not alias-exempt" (t_esp
InitTool 11057 -> 8389 words, docs/research/ "Tool RELs, t_esp pass 12/13"). Whole-tree it loses 11 identical
functions, so it is never a candidate for installation.

### `SCHDBG=1` — haifa-sched.c (schedule_block, sched1 AND sched2)
One line per ISSUED insn in issue order: `SCHDBG <fn> s1|s2 b<block> clk <cycle> uid <uid> pri <INSN_PRIORITY> w <INSN_REG_WEIGHT>
dep <number of INSN_DEPEND successors> luid <INSN_LUID>` — the rank_for_schedule keys (priority, then reg weight before reload, then
the class relative to the last scheduled insn, then dependents, then LUID). The `-fsched-verbose-9` dump has no priority table;
this is how a filler's slot (`lis`/`li`/`lfs` heads of t_esp InitTool block 13) is read: e.g. reg9001 `li 0` pri 787 vs the
g_pRotateWin high pri 786 = the `win->active` store (786) -> g_pRotateWin store (785, output dep) chain, not a tie (t_esp pass 26).
Prints only; codegen unchanged (t_esp.o cmp-equal, 2026-09-12).

### `CSEDBG=1` — cse.c (cse_basic_block)
One line per 1001-insn hash-table flush: `CSEDBG <fn> flush at insn <uid> (block from <uid>)`. The flush
happens BEFORE `insn` is processed (insns up to the previous one used the old table). Both cse passes print
(cse1 first; cse2's blocks start at different UIDs). Map a UID to source with the `.jump`/`.cse` dump of
`tools/research/kit/rtl.sh` (the t_esp pass-17 harness counted non-note insns from each
`__builtin_new` call to name the window and the offset). Prints only; codegen unchanged (t_esp.o cmp-equal).

### `CSEDBG=2` — cse.c (cse_insn, per SET; t_esp pass 30)
One line per SET cse_insn processes, after the source has been canonicalised/folded:
`CSE2 <fn> insn <uid> dest r<N> src_code <rtx> elt <0|1> cost <c> [r<M> qty <q> first <head> tick <t> intable <i>]...`
— `elt` = whether the (canonicalised) source was FOUND in the hash table (0 = it will be inserted as a new
class), then for the source reg (or the reg inside a unary src / SUBREG, or the base reg of a `(mem (plus reg ..))`
src) and the dest reg: `qty`, the class head (`qty_first_reg`, -1 = no qty), `reg_tick`, `reg_in_table`.
Reading: a lookup FAILS whenever a reg in the looked-up expression has `intable != tick` (exp_equiv_p validates the
LOOKUP's regs, and `reg_in_table` is set only when an expression mentioning the reg is INSERTED — a plain
`(set reg ...)` dest leaves it -1), so a class whose head (`first`) is a never-mentioned reg cannot hit the entries
recorded under an older member: `make_regs_eqv` makes the reg that lives longer (last use beyond the block and later
than the head's) the head. Both cse passes print. Prints only; codegen unchanged (t_esp.o cmp-equal, 2026-09-12).

## Adding a hook
Edit `$SNGDBG_DIR/src/gcc/<file>.c` (never tools/sn-gcc), gate it on a `getenv` cached in a static, print to
stderr, `make all` there (1 s), then re-check identity: `CC1DIR=$SNGDBG_DIR tools/research/kit/variant.sh
game/title src/game/title.cpp` must say IDENTICAL and `cmp` equal to `build/G4BE08/src/game/title.o`.
Regenerate the patch: `(cd tools/sn-gcc && diff -ru --color=never src $SNGDBG_DIR/src | sed "s#$SNGDBG_DIR/src/#src/#g") > tools/research/sngdbg/patches/dbg-hooks.patch`.

### `GFORCE="<pseudo>:<hardreg>[,<pseudo>:<hardreg>..]"` + `GFORCEFN=<substring>` — global.c find_reg (CHANGES CODEGEN; 2026-09-12)
Hands the listed pseudo the listed hard register (after the two-pass scan and the preference override, printed as
`[GFORCE rN]` with GDBG=1). Conflicts are NOT checked: a forced register already held by an earlier-allocated
conflicting allocno garbles the code (deleted copies, size changes) — read GORDER first and force every allocno of
the cascade in allocation order (e.g. `GFORCE=354:30,359:30`). `GFORCEFN` restricts it to functions whose
`current_function_name` contains the substring (pseudo numbers repeat per function). Use: "what if allocno X had
taken rN" — the answer (bytecmp of the variant) tells whether one allocation fact explains a residue before the
source form is searched (t_camera_data pass 5: tcSetBesideOffset's 27 words were one `o` -> r8).
### `NOEQV="<pseudo>[,<pseudo>..]"` — local-alloc.c update_equiv_regs (CHANGES CODEGEN; 2026-09-12)
Pretends the listed pseudo's set has no REG_EQUAL note: no REG_EQUIV, no `REG_LIVE_LENGTH *= 2`, reload does not
rematerialise it. Oracle for "would this hoisted constant win the allocation order without the doubling"
(t_camera_data pass 5: the loop-4 `tcCdat` lo_sum base at 2857 ties the PRE'd `i+1` and wins on allocno number).
Patch: patches/dbg-hooks.patch regenerated 2026-09-12 with both hooks (seven files: calls, cse, global, haifa-sched, local-alloc, loop, reload1).
