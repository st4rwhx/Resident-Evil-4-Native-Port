# Resident Evil 4 (GameCube, G4BE08 debug build) — matching notes

Goal: C/C++ source that compiles to a byte-identical `main.dol` and 110 byte-identical `.rel`
overlays. The build reproduces the original binaries from split objects; every unit that is matched
replaces one split object with compiled code.

This file is the reference part of the notes written during the matching work: how the compilers
behave, which source shape produces which instruction pattern, and the project conventions. The
pass-by-pass log (the evidence behind every `// COMPILER-DIFF:` tag and every rule below, in
chronological order) is in `docs/research/`; column 5 of the lever catalogue and many source comments
cite its `### ` section titles.

## Conventions

- Build: `ninja <targets>`; never run `configure.py` by hand (ninja runs it when a config file changed).
  Judge with `python3 tools/bytecmp.py <mod>/<unit>` (IDENTICAL or the per-function word list);
  `python3 tools/fdiff.py <mod>/<unit> <sym>` for the side-by-side.
- Zero-code (structural) search first — prototypes vs headers, size/order/rodata/vtable gaps,
  statement/declaration order, inline boundaries, the body census. Only then a tagged form
  (`// COMPILER-DIFF: <item>`): register pins `register T x asm("rN")`, asm launders/anchors/keep-alives,
  a dead statement. MWCC units: `register` locals, codeless pins (`asm { mr rV, x; mr x, rV }` with rV an
  unused volatile register, `asm { mr v, v }`), pragmas. Look the symptom up in the "Lever catalogue" below.
  An asm template that EMITS an instruction (`asm("li %0,K")`, `asm { mr r31, obj; mr sfd, r31 }`, an
  asm-emitted pool load) is not a lever: every such site was replaced by C on 2026-09-17
  (`docs/research/asm-removal.md`, recipes by mechanism) and `python3 tools/asmcheck.py <unit>` must stay at
  0 for every GCC unit outside the hardware kernels. The one remaining emitting block is `lib/dct_ac.c`
  (a documented MWCC pooling difference).
- Never a whole-function asm body, never `.s`. The paired-single kernels (dct_fsri, cftyp422_ppc,
  mpv_umc Bi/OneMakeMb/OutputIntra6blk/SetGqr) stay asm: MWCC 2.4.7 has no paired-single intrinsics, so
  that is the original form. General criterion: a function whose target bytes contain instructions this
  compiler never emits from C (`psq_*`/`ps_*` with quantised GQRs, `mtspr`, `lfdux`/`lwzux`, `stwbrx`
  with the chain unmerged, ...) was inline asm in the vendor's source — keep the asm body for THAT
  function and say so in a comment (mpv_mc `MPVMC08_OneRef1p_TuneC`). Everything else must be C.
- A unit counts as matched when bytecmp says IDENTICAL, (REL) `python3 tools/make_rel.py --verify` passes,
  and after `ninja -k 0` `build/tools/dtk shasum -c config/G4BE08/build.sha1` prints 111 OK. Every unit is
  linked from source; while you work on one, list it in `NON_MATCHING` (`config/G4BE08/objects.py` or
  `modules.py`) so the build links the original object and the SHA-1 check keeps passing.
- Tools: the research kit (see "Tooling kit" below) — `tools/research/kit/variant.sh <unit> <variant-src> [FUNC]`
  for every variant (0.5-1 s, build/ untouched, bytecmp + side-by-side), `tools/research/kit/rtl.sh` for RTL
  dumps, `CC1DIR=<sngdbg build dir>` + `GDBG=1`/`LADBG=1`/`NOMALLOC=1` for the hooked cc1plus,
  `tools/research/mwccdbg/ra.py` for MWCC.

## Lever catalogue (mechanism -> source lever, one line each)

Read the fdiff symptom in column 1, the compiler pass in column 2, try the zero-code levers in column 3, then
apply the tagged lever in column 4 (`// COMPILER-DIFF: <item>`). Column 5 names the section the row is derived
from: a `### ` title in `docs/research/` (`rg -n '^### <name>' docs/research/`) or a section of this file. Verify
every lever with `tools/research/kit/variant.sh`; read the mechanism's numbers with `GDBG=1`/`LADBG=1` (GCC) or
`ra.py`/`chaitin.py` (MWCC) instead of permuting.

### GCC 2.95 (DOL, REL modules: `game/`, `st*/`, `em*/`, `pl*/`, `wep*/`, `Tools/`, `t_*/`, `Sscrn/`)

| symptom in fdiff | mechanism (pass) | zero-code levers | tagged lever | derived in |
|---|---|---|---|---|
| invariant `lis`/`li`/pool load hoisted to the preheader in ours, in the body in the target (or the reverse); second loop pass hoists an inner-loop pseudo | loop.c move_movables: hoist iff `thr*savings*lifetime >= insn_count` (x2 once moved), `thr = (1|2)*(1+n_non_fixed_regs)` minus 3 per moved movable (`GDBG=1` prints `LOOPDBG [thr sav life ic]`) | change the loop's REAL insn count: a dead test / dead statement in the body, a value computed before the loop notes; `do { } while (0)` around the nest (loop.c never hoists out of it, loop depth reweights refs) | two codeless `asm("" : : "r"(i))` at the body top; `int dead = 0;` | "Dead-test lever sweep", "Tool RELs, db_mod pass 4", "Stage rooms, st2_1 pass 10" |
| two loop-hoisted highs / pool constants with swapped callee-saved registers at equal global priority | gcse PRE pseudo numbering = hash-bucket order `(7933 + h(label)) % table_size`, `h = h*129 + c` (bucket.py model); allocated in pseudo order on a priority tie | shift the `.LC` numbering: dead `f32 lcN = N.5f;` pool constants earlier in the TU (deleted at cse1, never emitted); 5-6 dead sets change the table size | tag `candidate (gcse PRE pseudo numbering)` on the dead labels; table SIZE (`(n/2)\|1`, n = insns at gcse entry): a LOOP_END-blinded dead test `{ int z = 0; do { } while (0); if (z > 128) { z = z*77+1; .. } }` = +3 + 2 per arm statement, 0 code, 0 sched1 insns (cse2 folds it), tag `candidate (gcse table size)`; one extra insn in a shared inline header shifts every includer's S (t_esp_area pass 3) | "Stage rooms, st2_1 pass 10", "Matching rules of thumb" (gcse PRE pseudo numbering wrap) |
| fresh `lis sym@ha` where the target reuses a register (or the reverse); a copy reaching a join block | cse: the class head constant (cost 0) beats the pseudo (cost 1), a CODE_LABEL ends the ebb, conditional jumps do not; the AROUND path carries the table into the join | break/extend the ebb: `do { } while (0)`, a dead test whose then-arm ends in a jump, a dead store in the arm (cse2 make_regs_eqv order), the load written twice vs cached | `asm("" : "+r"(x))` launder as the LAST set of the ebb (cprop then has no available set), a pinned variable with a dead `x = 0` before each arm's constant, tag #12 | "COMPILER-DIFF #12 sweep", "Known compiler-build differences" (cse cost side), "db_light, second pass" |
| target reloads `pG`/a work pointer after a store, ours hoists the load above it (or ours reloads where the target does not) | alias.c `true_dependence`: a store through a `MEM_SCALAR_P`/`MEM_IN_STRUCT_P` MEM is disjoint from fixed scalars; a reference/`void*&`/one-member-struct store is not | reference views `BitOn(u32&)`, `ISet(g, g+1)`, `FSet/FSetP`, `PSet(void*&)`, `RefU16(x)`; `WorkPtr.p->field` (one-member struct global, `R10cWorkPtr`); struct-member pointer globals; `(int) pG->flags >= 0` | — (all forms are C) | "Matching rules of thumb" (BitOn/ISet), "Room idioms" (R10fWorkPtr, PSet), "Compiler" (temp-flags patch) |
| `mr r3,rX; mr r4,rY` argument copies deleted in ours (r3 already equal) | reload1.c reload_cse_regs: the register table is forgotten only at a CODE_LABEL, a CALL or a volatile ASM_OPERANDS | find the label: a loop/goto form between the definition and the call | `asm volatile("" : : : "memory")` two-way barrier (a bare `asm volatile("")` is an ASM_INPUT and does not count), tag `candidate (reload_cse register table)` | "DOL sweep 21b" (RouteCkPosToPosDis) |
| same block, two independent insns in the other order (sched1 tie); a store group issued before/after a call | haifa rank_for_schedule: priority = critical path, ties by dependents then LUID (stream order); an output-less asm is volatile = full barrier; a `"=m"` output = one true dependence | statement order (LUID), declaration order of the locals, the chain's innermost assignment (dying-first store), post-increment inside the argument list, `#line` to equalise ASM_OPERANDS hashes | `asm("" : "=m"(anchor) : "r"(x))` codeless anchor (one dependence, no code); never an output-less asm unless a barrier is meant; tag #5 (sched1 tie) | "Tool RELs, t_esp pass 12" (DB_STRING ctor), "COMPILER-DIFF #8 closed", "Tool RELs, db_mod pass 5" (dbmod_p_info) |
| callee-saved register names permuted (`r31` vs `r30`, `r28` vs `r27`) for values that live across calls | global.c: order = `floor(log2 refs)*refs/len*size`, ties by allocno; find_reg pass 0 uses only `regs_used_so_far` minus `regs_someone_prefers`, pass 1 hands out virgin regs in r31,r30,.. order; REG_EQUIV doubles len; local-alloc never gives r31 (`GDBG=1` prints `GORDER .. used conf smpref -> rN`) | change refs/len: an extra use (post-loop use of a giv/biv, a dead test), a second assignment (kills REG_EQUIV), loop depth (`do {} while (0)`), make a block-local value global (a use in another block) or the reverse | `register T x asm("rN")` pin (never r31 for a local-alloc qty), tag #17; when the lengths cannot be re-tied, a codeless `asm("" : "=m"(m) : "r"(p))` in a block with a free sched2 slot = refs 3 -> 4 = the log2 step (pri 29 -> 77; t_esp_area pass 3) | "Tool RELs, db_mod pass 5" (allocation facts 1-5), "Stage rooms, st2_1 pass 10" (initPuzzle), "DOL puzzle final closer" |
| block-local temporaries with swapped scratch registers (r9/r11, r0/r9, f0/f13) | local-alloc.c block_alloc: qty order `refs*size/(death-birth)`, suggestion qtys first, the 3-qty partial sort quirk, fake_birth/fake_death lifetime +-1 insn, hard-reg inputs block `combine_regs` ties (`LADBG=1` prints every qty in allocation order) | statement order (birth/death suids), one variable per value vs one reused variable, an extra ref, an inline helper's own locals vs function-scope locals | `register` pin; an asm that mentions the pseudo twice as inputs = +2 refs for `QTY_CMP_PRI` without code (`asm("" : "=m"(m) : "r"(p), "r"(p))`, esp_app), tag `candidate (local-alloc qty order)` | "DOL sweep 21b" (MemCheckHeapEnd), "Matching rules of thumb" (float local assigned twice), "Tool RELs, t_esp pass 12" |
| a value sits in an argument register (r9/r10) at a variadic call whose format does not use it (`lha r9`, `lhax r9`), and/or a rematerialised `lis` there takes r11 while r9 looks free | calls.c: a surplus argument to a variadic callee is a bare `(set r9 x)` before the call: local-alloc ties `x` to r9, `x` is live at the other argument insns, so reload's per-insn `used_spill_regs` loses r9 and r11 joins the spill set (round robin from then on) | pass the value as an extra argument to the eprintf/printf (`eprintf(.., "[%6s]", dir, no)`); the original did | — | "Tool RELs, db_mod pass 8" |
| target keeps a constant (`li rN,K`, `lis @ha`, pool `lfs`) in a callee-saved register across calls/loops, ours re-materialises it at each use | local-alloc.c update_equiv_regs: a pseudo set once to a constant gets REG_EQUIV and is never allocated (rematerialised by reload) | make the pseudo multi-set (two assignments, `no = ..` reused in another case), a shared variable across cases | (no asm-emitted constants any more, asm-removal.md: a two-set variable, the set inside the loop with the use after it, a fold only combine can do `((u32) e >> 16) & 0xFFFF0000`, three uses so `update_equiv_regs` leaves the `lis` in block 0, `register int z asm("r11")` pin), tag #13 | "COMPILER-DIFF #13 sweep", "Tool RELs, t_esp pass 5", "DOL cam_ctrl final closer" (asm `la` args) |
| `addi rX,r1,N; mr rY,rX` / a hoisted `&local` in ours, a fresh `addi` per use in the target | gcse PRE of `(plus fp N)`; inline argument sets to hard regs are never PRE'd (integrate subst_constants); `high(sym)` PRE to the end of bb 0 | frame-offset-0 local, `&local` passed through an inline taking `Vec*`, `FadeSetW` (the inline's own temp slot), argument expression instead of a variable, `Vec* pa = &ang2` at the block top | `asm("" : "+r"(p))` launder; `&local` passed through a `static inline` wrapper (integrate substitutes it into hard-register argument sets, which PRE never touches); an explicit `else pm = m;` arm to end the cse ebb, tag #3 | "Known compiler-build differences" 3, "DOL sweep 21b" (FadeSetW), "DOL structural pass 5" (view) |
| extra/missing `clrlwi`/`extsh`/`extsb` at a call, a store or a return | combine strips a narrow extension only for a single-set pseudo with known nonzero bits; promoted parameters; `(sign_extend (subreg:QI no))` on a 2-set pseudo | `int c = col;` then `(u8) c` at the use; the launder in the ARM (not the join) when PRE must share it; `u8 num = a; if (num < 60) num = 60;` vs the ternary | int-view / narrow-view asm-labelled alias `T fI(int) asm("mangled")`, tag #2/#4 (check the plain call first: 5 of 51 were unnecessary) | "Known compiler-build differences" 2/4, "db_light, second pass" (COMPILER-DIFF 2), "COMPILER-DIFF tag audit" |
| parameter copy `fmr fN,f1` / `mr rN,r3` ranked as if the incoming register did not die (prologue) | regmove optimize_reg_copy_1 moves the REG_DEAD only when the modes match | read the incoming register in another mode in block 0 | `register f32 x asm("f1")` read as f64 (or s16 for int) inside a codeless `asm("" : "=m"(field) : "f"(x))`, tag #8 | "COMPILER-DIFF #8 closed" |
| argument moves interleaved differently at a mixed int/float call (`fmr f1; li r4; fmr f7; li r5`) | calls.c load_register_parameters in argument order; the FP source dying at the move is weight 0 (rank18 research: no compiler variant fixes it) | try the plain member call first (8 of 26 aliases were unnecessary) | floats-first asm-labelled redeclaration of the callee (`include/atari_init.h`), tag #1 | "Known compiler-build differences" 1, "COMPILER-DIFF tag audit" |
| a shared tail cross-jumped in ours and duplicated in the target (or the reverse); `li r3,0` placement before `beq` | jump2 cross_jump is stock; decided by the RTL at jump2 entry: `(use r3)`, flow's `(use (const_int 0))` after a block-ending CALL, sched2's position of `li r3,K`, jump1's `x = b; if (c) x = a` hoist (tools/research/xjump.py predicts) | duplicate the tail through a non-call statement per arm; arm order; `if (ok) { ..; return 1; } err; return 0;`; `int c; if (..) c = 0; else c = 0x14;` as a statement, not a ternary | `asm volatile("")` (ASM_INPUT) ending an arm, a tied launder, tag #6 | "COMPILER-DIFF #6 resolved", "Matching rules of thumb" (branch arms ending in a call), "db_light, third pass" |
| loop rotated/un-rotated, duplicated exit test, `b test` poll, peeled FP exit test, constants reloaded per iteration | stmt.c expand_end_loop scans ~30 insns for a jump to the loop end; `for(;;){if(c)break;}`/`while(c)` rotate, `while(1)` + deep break and `if(!c){..} else break;` do not; a goto loop has no loop notes | `while (f() != 1) SceSleep(1);`, `do { if (call()==1) break; SceSleep(1); } while (1);`, `goto open; wait: SceSleep(1); open:`, `for (;;) { A; if (c) { x = lim; break; } SceSleep(1); }` | — (all are spellings) | "Don'ts" (loop rotation, room idioms), "COMPILER-DIFF #7/#9 closed" |
| loop counter kept (`cmplwi i,N`) vs pointer compare (`cmplw`), giv `li rY,C` position, `subic./bge`, `addic.; bne` | loop.c strength reduction: a single-use giv is not combined, a giv passed to a call puts its init after the hoisted invariants, `u32` vs `int` decides cmplw/cmpw, a post-loop use keeps the biv | write the giv expression at every use (`C + i*14`), a `y += 14` variable to combine, keep `i` by a use after the loop, `for (i = n-1; i >= 0; i--)`, `while (i--)`, `if (i++ == 9)` | `int dead = 0;` before the copies (extra pseudo), tag #13 | "Matching rules of thumb" (menu loops, strength-reduced index), "db_light, third pass", "Tool RELs, t_event closer" (XmlNodeDataClear) |
| `mr r3,r31; lwz r0,N(r3)` hoisted above a test ("interblock hoist"), or a hoist missing | not haifa: cse rebased the load on the copy; regions form only in functions with no return block (leaf rule) and within 10 blocks / 100 LUIDs | a `return` anywhere kills every region; a shared local / inline wrapper for the copy; `on = 0` instead of `return` | `asm("" : "+r"(pp), "+r"(pn))` before the test, tag #5 | "Known compiler-build differences" 5, "db_light, third pass" (COMPILER-DIFF 5) |
| `.rodata` order (pool constants, strings, vtables, deferred statics), dead pool entries | varasm/finish_file order: inline string literals at parse time, function-local static initialisers after the body, vtables then deferred namespace statics in declaration order | declaration order of `static const` tables, `extern` in a header vs internal linkage, `static const Vec` sources, `#line N` for `__LINE__` | dead `f32 lcN` labels (row 2) | "db_light, second pass", "Matching rules of thumb" (`const T x[]` extern), "DOL structural pass 5" (dead pool) |
| `.text`/`.bss`/`.data` order or size gap, phantom or missing functions, linkonce copies | the original link stripped never-called statics per function; `.gnu.linkonce` placement (ngccc.py); `static int x = 0` .data vs `static f32 v` .bss; empty in-class ctor emits the object at the definition | `STRIP_UNUSED` / `LINKONCE_DROP` in modules.py or objects.py, declaration order, empty `C() {}` | — | "REL modules", "Room idioms" (r229), "DOL sweep 23a", "Matching rules of thumb" (statics) |
| frame slot order / frame size, `addi rX,r1,N` offsets | function.c assign_stack_temp best-fits into freed inline slots, address-taken scalars after arrays, ADDRESSOF forcing, frame = ALIGN8(8 + vars + fpmem + fp/gp) | declare locals after the getter calls / in inner blocks, by-value struct helper for a 4-byte temp, `Vec* pa = &ang2` at the block top | `asm("" : "=m"(unused))` to keep an unused slot | "Matching rules of thumb" (locals whose frame slot, frame size), "Tool RELs, t_esp pass 11" |
| `new` result: ctor stores late, the object pointer in a callee-saved register, spill set of `&local`s | calls.c is_malloc -> REG_NOALIAS -> unique alias base for the pointer; the target's `new` result had none (`NOMALLOC=1` is the oracle) | pass the pointer through a non-inlined boundary / a memory round trip; `CreateEditWindowN(slot)` reading the global inside the helper | class-scope `operator new` bound to the `__builtin_new` symbol, tagged | "Tool RELs, t_esp pass 12/13" |
| `switch` compare tree shape (`cmpwi;beq;bgt` chain, default first, folded case constant) | stmt.c/jump.c case tree (tools/research/casetree.py validated on 836 switches) | case order, `default:` first, empty `case 0: break;`, `no = g->x; switch (no) { case N: num = N; }` | — | "Switch tree model", "Room idioms" |
| inline boundary: `bl` vs inlined body, deferred inline order, template copies | integrate.c: a `void*` destination is not inlined, inline size limits, `static inline` vs macro, `saved_inlines` order | macro vs inline, `void*` parameter, a helper taking `Vec*`, `template <>` declaration order | — | "Inline-vs-macro sweep", "REL modules" (linkonce), "Sscrn" (candidate #8 end-of-file order) |
| setcc (`xori;subfic;adde`) vs branch; `%256` as `rlwinm;subf`; one mask vs two `andi.` | fold/expand: `x = (a == 2)` is a setcc, `!= 2` branches; `(on & A) || (on & B)` folds to one mask | `cursor = (mode == 2)`, inline helpers for separate `andi.` tests, `int skip = 1; if (..) skip = 0;` | — | "db_light" passes, "Room idioms" (r11b) |
| the target re-materialises a symbol high (`lis rX, sym@ha`) per row while a callee-saved register already holds it | reload's `choose_reload_regs` calls `find_equiv_reg(..., NULL_PTR, ...)` so reload-inserted insns are not skipped: the backward scan takes the first `(set hardreg (high sym))`, and if that reg is call-used with a call in between it returns 0 without looking further — one reload `lis rV, sym@ha` into a call-used reg poisons every later unallocated high of that symbol until the next allocated head; such a reload arises from an `addi rX,rX,sym@l` form = `(mem X)` with X ≡ sym unallocated, i.e. a cse1 flush landing between a row's `(set X (lo_sum P sym))` and its `(mem X)` load | move the cse1 flush (pads) so it lands / does not land between a lo_sum set and its load | pads (tagged) | "Tool RELs, t_esp pass 26" |

### MWCC 2.4.7 (CRI `lib/`: adx_*, sfd_*, mpv_*, sfx_*, mps_*, dct_*, gcci, cri_cvfs, ...)

| symptom in fdiff | mechanism (pass) | zero-code levers | tagged lever | derived in |
|---|---|---|---|---|
| two values with swapped registers (r30/r31, r27/r28) | Chaitin RA colours in virtual-id order: parameters first, own locals in REVERSE declaration order, then each inlined helper's parameter+locals in declaration order, helpers cloned breadth-first (FIFO) in call order, frontend @temps by creation order, expression-CSE temps last (`ra.py`/`rasum.py`; `chaitin.py` predicts) | declaration order (first declared = highest id), move code into a `static` helper (its locals outrank own locals and IV temps), two-level helpers (a depth-2 helper called after another depth-1 helper's callees ranks below them), a same-body local (only a local of the SAME inlined body sits between its locals) | `register` local; `asm { mr rN, v }` hard pin (tag M) | "CRI pass 11", "CRI pass 13", "CRI pass 20", "CRI pass 21" |
| parameter copy `mr r31,r3` kept in the target, propagated away in ours (or the reverse) | the RA coalesces only compiler copies (`@ret`, argument moves, `?:` copies); a user copy always leaves its `mr`; a call result assigned to a helper local propagates into `@ret` | `void *obj` parameter + typed kept copy `SFD sfd = obj;`; a plain (non-`register`) copy of the parameter; an arithmetic-defined local survives | codeless K-pin on the copy `asm { mr r11, sfd; mr sfd, r11 }` (K 29 -> 28; both `mr` deleted), or a dead arm reading through the copy (asm-removal.md) (M1) | "CRI pass 12", "CRI pass 20", "CRI pass 10", asm-removal.md |
| `li 0` per arm in the target vs a copy of the entry zero in ours; a dead compare kept | backend CSE rewrites a helper-local @temp `li 0` into a copy of the entry zero; own locals are not @temps; dead compares survive in helper arms | helper-local `ret` (`static Sint32 sub(SFD sfd) { Sint32 ret = 0; ..}`) vs a caller local; the arm as a helper | — | "CRI pass 14b", "CRI pass 15", "CRI pass 13b" |
| `addi` folded into every load/store in the target, kept as `mr`/`addi` in ours (or the reverse); `addi rD,rA,0` turned into `mr` | backend add-propagation folds `addi base,hn,K` into later loads/stores of the SAME block unless a non-foldable use (mr / compare / call argument) survives; a literal `addi rD,rA,0` is constant-propagated to a copy | nested-assignment anchors, word-pointer step `(Uint8 *)(ptr + 1) + n`, argument expressions vs variables, a `mr` base defined by a kept copy, a relocation form for the `addi ..,0` | — | "CRI pass 14", "CRI pass 14b", "CRI pass 15", "CRI pass 17b", "CRI pass 19b" |
| pool base `lis;addi` + `lwz off(base)` vs direct `lis/addi` per table; FPR literal / `frsp` order | `-O4,p` pools per function; a table in a file without deferred data does not pool; `#pragma pool_data off/on` is per function | `extern` declaration first + definition after the function, `static const` layout, the pool base as an OWN local declared first (row 8) | `#pragma pool_data off` .. `on` around ONE function (M2) | "CRI pass 16a", "CRI pass 18b", "CRI pass 19b", "MWCC compiler-build differences" |
| `lfd` folded onto its `lis`, `stwbrx` chain merged/unmerged, post-RA pair folds | peephole pass after RA; a redefinition changes the preheader schedule that the peephole sees | `pcm = sje->pcm[ch]` re-derived before the second loop (a coalesced range-split copy) | `#pragma peephole off` (M4) | "CRI pass 21", "CRI pass 7", "CRI pass 14" |
| a loop body whose target order = ours' PRE-RA schedule (pass 17) but ours is rescheduled post-RA (stw hi/lo pairs, fctiwz chain hoisted) | peephole-forward after the pre-RA schedule sinks every `addi rX,rX,K` to its next reference / the block end (dirties the block -> post-RA reschedule) unless a later STORE's data register NUMBER == X — compared without the register class, so `stfd f43` stops the sink of `addi r43,r43,8` | change the counter's virtual number (own locals in reverse declaration order, then range-split webs): one counter per loop, a pointer form of another loop, until the pass-17 `addi rX,rX,K` register equals a later `stfd`/`stw`/`stb` data number | — (numbering is C) | "CRI pass 41" |
| instruction order within a block (two independent loads/stores swapped) | the scheduler: `#pragma scheduling off/603/604/750/7400/7450` never flips a tie; asm statements are scheduled but a C statement never moves across them; ties follow statement order and @temp creation | statement order, splitting a statement (creates an @temp), IV-temp creation order | `#pragma scheduling off` | "CRI pass 16b", "CRI paired-single kernels pass 2", "CRI pass 19b" |
| a block's pre-RA schedule needs one more node / a different DAG with the final code unchanged (an `addi`/temp issued one cycle too early, a compare chain) | peephole-forward folds mask-then-shift `(x & M) >> k` into one `rlwinm` (record form when compared with 0) and leaves the dead mask def in the block until the RA deletes it: a scheduler node with no consumer; shift-then-mask `(x >> k) & M` is one `rlwinm` + `cmpi` with no leftover; the `& 0xFF` index/value masks of pass 58 are deleted BEFORE scheduling (block-split count only) | spell the test mask-then-shift (sfd_cre AnalyMpv 15 -> 0w) | — | "CRI pass 60", "CRI pass 58" |
| a setup value the target computes BEFORE a pre-loop statement, while ours computes it from a hoisted loop invariant (`slwi` of a stride used only in the loop body) | frontend hoisting appends the loop's invariant @temps after the for-init in creation order; a pre-RA tie between two independent IU ops is input order; `ptr += step` with a single-def `step = E * 16` on a `Uint32 *` is folded into a new hoisted `E << 6` @temp | make the invariant an own-local statement placed before the statement it must precede; declare it (and every other former @temp of the same level, e.g. a byte-scaled `dskip`) FIRST so its colour stays the @temp's (r0: highest vid of the level); keep the pointer step in bytes (cftfx UserTable 2 -> 0w) | — | "CRI pass 61" |
| a pack/expression written straight into a VARIABLE's register in the target while ours keeps `mr own, t` (a temp node that shifts the whole colouring) | the RA coalesces a copy only into a backend temp or an `@N` web (range-split web, CSE temp, INLINED HELPER LOCAL); a copy into an own local never coalesces; a copy FROM an `@N` web into a backend temp (the K6 `mr t, w0` of `__rlwimi(w0, ..)`) does not either; helper-local vids ascend in declaration order (reverse of own locals) | put the body in a `static inline` helper (its locals are `@N` webs) with `#pragma opt_lifetimes off` around the CALLER (around the helper definition it does nothing); `#pragma inline_max_size` if the helper is large | `#pragma opt_lifetimes off` | "CRI SWAR kernels pass 16" |
| a single-use local substituted into its use in ours, kept as a variable in the target (`cmpwi` on a loaded value the target keeps in its own register) | MWCC types `long != int-constant` by retyping the `long` read to `EINDIRECT int`; the frontend's single-use substitution needs the read type to equal the object type | declare the local `Sint32` and compare it with a plain `int` literal (`Uint32`/`int` locals or a `(Sint32)` constant are substituted); also `p + cksz - 4` = `subi; add` vs `p + (cksz - 4)` = `add; addi`; both OR operands masked keeps the rotate-and-mask as the rlwimi base | — | "CRI pass 62" |
| a frontend-CSE'd @temp coloured last (low vid) in ours while the target colours the value with its statement's temps (callee-saved handed out in ascending order) | a cast on a macro argument that the macro duplicates (`SWAP16((Uint16)x)`) stops the frontend CSE: the value becomes a backend temp | cast the macro argument | — | "CRI pass 62" |
| a local assigned from an inlined helper's return ranks with the @temps (coloured above every own local) while the target colours it as an own local | a local that is a PLAIN COPY of an inline's result is replaced by the helper's `@ret` temp (frontend-01 shows `EOBJREF [@N]`, no variable) | write the expression out instead of calling the inline (sfd_tst `tol = mt->unit * cnt / unit`) | — | "CRI pass 64" |
| a counter that must colour after a block's temporaries but before the other own locals (L1, a volatile register) | an inlined helper's locals are `@N` webs numbered in reverse declaration order at the inlining point (vids ascend in declaration order: declared second = coloured first) | move the loop into a `static` helper; order the helper's declarations | — | "CRI pass 64" |
| a pack `rlwinm V; rlwimi V, src, sh, mb, me` written IN PLACE on the variable in the target (same-vreg WAR order: the insert after every read of V, `slwi w1,w1,16` after the fused reads), ours `mr V, t` (coalesced or a node) or a K6 temp | the or->rlwimi peephole (pass 01) fuses the operand equal to the destination if there is one, else the EARLIER-defined rlwinm, and copies the other in as the base; a base rlwinm with rotate 0 whose mask is the complement of the insert is replaced by its source (`mr V, V` -> deleted); a fused rlwinm stays as a dead def (counts for the <= 35 unroll budget, and its WAR edge to a later in-place write of its source gives it height) | `V = base_shift; V = (fused_shift) | (V & complement_mask)` (+1 pcode: the fused def; on a loaded V the mask is a second dead def) or `V = __rlwimi(V, ..)` when the web is not redefined while the K6 temp lives (+1: the K6 `mr`) | — | "CRI SWAR kernels pass 17" |
| induction pointers coloured before a `register`/pool base; loop temporaries in ascending vs descending statement order | frontend range-split IV @temps have ids above every own local, created in statement order; index-form loops put the IV copies in the preheader | write the pointers as OWN locals declared below the base (`p = ip; q = fp; *p = v; p++;`), index form `a[i]` vs `*p++` | `asm { addi ip, bss, 0 }` (becomes `mr`) — use a relocation form | "CRI pass 19b", "CRI pass 20", "CRI SWAR kernels pass 3" |
| an expression computed once (@temp) in the target and twice in ours, or the reverse | the frontend CSEs identical expressions (also across macro uses) into one @temp; casts/`void *` views break the CSE; CSE temps rank below inline temps | write it twice vs cache in a local, `(Uint32)` vs pointer views, `(Uint8*)(p +- k) + n` | — | "CRI pass 13", "CRI pass 20", "CRI pass 14" |
| a hard pin fixes one register and shifts unrelated ones | a hard pin adds a physical neighbour to EVERY node -> colouring levels shift; pins are exclusive with each other | prefer the plain copy (row 2) or one pin of the highest-ranked value; a parameter above the locals = pin at the top | `asm { mr r29, data }` (level-shifter) | "CRI pass 18b", "CRI pass 7", "CRI pass 10" |
| `bl` vs inlined body, .text size gap, block splitting | `-inline auto` thresholds; `#pragma dont_inline on/off` (M3); a helper's size decides | helper size / two-level helpers, `static` vs `static inline` | `#pragma dont_inline on/off` | "CRI pass 16a", "CRI pass 16b" |
| Sint64 halves order, 32-bit views | 32-bit views of Sint64 through a struct/union | struct view instead of shifts | — | "CRI pass 16a" |
| compare folded into the branch, `bf 2,next; b found` found-path, IF/loop branch pairs | backend-folded tests; a deleted coalesced `@ret` copy gives the `bf; b` shape; the frontend's IF shapes read off the branch pairs | condition polarity, `if () {} else` placement, `while`/`for` spelling | — | "CRI pass 13b", "CRI pass 14b" |
| loop invariant recomputed per store in the target | `#pragma opt_loop_invariants off` | store through a `static` | the pragma (tagged) | "CRI pass 9" |
| whole function is `psq_*`/`ps_*`/`mtspr`/`lfdux` | MWCC 2.4.7 has no paired-single intrinsics: vendor inline asm | keep the asm body for THAT function and say so | — | "Conventions", "CRI paired-single kernels pass 2" |
| .text function order, .rodata string order, .bss size | emission = definition order; strings by @N id; `.bss` pads | definition order, `static` table placement, struct padding | — | "CRI pass 5", "CRI pass 16b", "CRI pass 21" |
| the loop's pointers/counters (`stride`, `s0`, `d`, `i`) take r8-r12 in ours and r0/r3-r6 in the target, with block temporaries taking r0/r3-r7 first; `li rN,count` for the ctr in r0 (ours) vs r7 (target) | the last simplification scan removes in vid order, so every surviving temp with a higher vid than the loop variables pops (is coloured) before them; the loop variables pop first only when they form a level of their own (degree >= 29 at the previous scan) | declare the block's value locals (pixels/words/sums) BEFORE the loop variables (their higher vids are visited after the loop variables and keep their degree up); loop variables declared without initialisers, assigned in the target's load order; casts that keep the values own locals (a mask AT the load makes the local a copy of the load temp) | neighbour pin on the 1-2 short loop variables (row below) | "CRI SWAR kernels pass 10" |
| one value X is coloured one Chaitin level too LOW (falls below 29 one scan too early, lands in r0/r3..r12 or a lower callee-saved than the target; a target node "must be level 2" / "needs more neighbours") | the priority list is built by degree-<29 removal scans; a pinned value is coalesced with the asm's copies, which stay as ghosts aliased to the physical register = never-removed neighbours of THAT value only (total degree of everything else unchanged) | a same-body value that overlaps X (a later use of X, a kept copy), a longer web | **codeless K-pin `asm { mr rV, x; mr x, rV }` with rV a VOLATILE register the function never uses (r11, r8, ...)**: corrected 2026-09-17 — it adds NO ghost neighbours; rV leaves the colour set, K 29 -> 28 per distinct rV (`chaitin.py --k 28` reproduces it exactly), so a node with exactly 28 neighbours at its scan survives one level; works on a `register` local or `register` parameter; both `mr`s deleted (size unchanged); prove it by swapping rV. To ADD neighbours use a dead arm instead: `z = 0; if (z != 0) x = x + a->f1 + a->f2;` (+1 per load, deleted post-RA together with `li; cmpi; bt`; arms with a call or store are not deleted; the arm must not sit in the entry block). A callee-saved rV also RESERVES the register (row 10); on a parameter or a call-defined value the `mr` is emitted (a real copy). Tag M1 (neighbour pin) | "CRI pass 40" (cvFsGetFileSize 14 -> 0, the dump reading), "CRI pass 44" |
| range-split webs (`@N`, lifetimes on) of a switch's cases coloured in the wrong order: a value takes the wrong callee-saved / volatile register although the graph is one level | the frontend numbers the `@N` webs by the FIRST DEFINITION of each variable in the function (all of a variable's webs consecutive; within a variable the later definition and the later case get the LOWER @), and the RA colours them in ascending @ before the own locals; a load and the pack that replaces it in the same variable are therefore always pack-then-load | dead initialisers in the declaration (`Uint32 x0 = 0, x1 = 0;`: deleted, no code) move a variable's group first; write a pack INTO another variable (`w2 = (w1 << 8) | w2`) when its load must be coloured before it; keep a single-use load as a variable with the pointer step right after it (`w2 = s0[8]; s0 += stride;`) | — | "CRI SWAR kernels pass 18" |
| a block's target order is reproduced ONLY by the leftover-free DAG (dagx.py: the or-pack's dead fused shift, the intrinsic's K6 copy and an own-local copy each move it) | every C pack spelling of this compiler leaves one pcode until the RA (dead rlwinm / `mr`), and the RA's deletion dirties the block into a post-RA reschedule from the wrong input order | — | `V = base_shift; asm { rlwimi V, src, sh, mb, me }` on `register` locals: an ordinary RLWIMI pcode to the scheduler, tag COMPILER-DIFF (the vendor's TuneC had inline asm there) | "CRI SWAR kernels pass 18" |
| a dead unconditional `b` in the target that ours lacks (a `switch` arm block reduced to `b join`, `beq` already threaded past it; the function 4/8 bytes short, every later branch offset off) | the frontend folds an EMPTY arm onto the default label and deletes every dead def and side-effect-free statement; a statement it keeps (asm, a live def) and a backend pass deletes (CSE-redundant `li` against a dominating def, an RA-coalesced copy, an asm self-copy) leaves an emptied block that keeps its `b` — but only when the block is laid out where its successor is NOT the fall-through (the arm written BEFORE the arm that jumps elsewhere, e.g. before a `return FALSE` arm); an emptied arm laid out just before the join falls into it | arm ORDER in the source (the dead-`b` arm first) | `asm { mr v, v }` on a `register` parameter/local in the arm (codeless: deleted by the RA, no reservation, colours unchanged), tag M1 (codeless arm) | "CRI pass 66" (mwsfdcre IsUseAdxt), "CRI mwsfdcre pass 7" (form A = the CSE-deleted def) |
| two values swapped where one is a `/ 2`, `>> k` or `+ carry` temporary; the target's `srawi`/`addic`/`subfc` (XER writers) come in a different ORDER than ours | the scheduler serialises XER writers (class-0 WAW edges, lat 1) in RAW = statement/evaluation order, pre-RA and post-RA alike: their final order is the vendor's expression order, an oracle for how the source was written | evaluate the sub-expressions in the target's srawi order: `cpos = ofs[0] + ((vx / 2) >> 1) + ((vy / 2) >> 1) * cpitch` (frontend CSE @temps for `vx / 2`, `vy / 2`, shared with the table indices) instead of own locals `cvx = vx / 2; cvy = vy / 2;` (mpv_umc OneReadMb 48 -> 11w; the @temps are coloured before the own locals: cvx r28, and vx dies into fn_y's r25) | — | "CRI pass 68" |
| a block-top temporary (`mbx * 8`) coloured BEFORE the loaded variables it is derived from (takes r10, pushes mby/mbx to r11/r12) while the target colours it after them (r12) | a single-use `x = a * 8` local is substituted into its use and becomes a BACKEND temp (vid above every own local, popped first in L1); a local with two uses stays a variable with its declaration vid | give the temporary a second use (`ofs[1] = mbx8 * 2 + ..`, folded back to `slwi mbx,4` by peephole-forward) and declare it LAST (lowest own-local vid: coloured after mby/mbx) (mpv_umc OneReadMb 11 -> 0w) | — | "CRI pass 68" |
| a later case's value takes a WRONG callee-saved register (r29 vs r31) although valid.py says the target colouring is proper for our graph; a pack written in place in the target while ours keeps `mr` into a variable declared in that case | a callee-saved register is handed out ONCE per function (r31 downward) when a popped node finds no free volatile/handed-out register; later nodes take the LOWEST free handed-out one — so a web's colour is decided by which groups popped before it; copies into or out of an OWN LOCAL never coalesce (own-local packs always keep the `mr`), only temp->`@N` web copies do | make the value a range-split web of a variable first defined in an earlier case (its base copy coalesces), and place it in the pop order by the variable's GROUP: a web popping before the function's first callee-saved hand-out takes r31; two defs of one variable in one case: the EARLIER def has the higher @ and pops right after the later one (mpv_mcy H2 22 -> 0w: pair 1's neighbour = the same variable as pair 3's) | — | "CRI SWAR kernels pass 20" |
| a later case's values take the same callee-saved registers as an earlier case's in the SAME pairs (q1 r30 / A1 r29 in cases 1 and 2 while case 3 has A1 r30 / q1 r29), or a value takes the LOWEST-numbered callee-saved register the function has used so far | the colouring takes the lowest-numbered free register among r0,r3-r12 and the callee-saved registers ALREADY handed out; hand-outs go r31 downward, so the lowest handed = the MOST RECENT hand-out: a later case makes no hand-outs of its own, each of its webs takes the latest hand-out unless a coloured neighbour covers it (chaitin.py, `why.py`) | the hand-out ORDER is set by the earliest case whose webs pop first in each group (case 3's same-variable numbering: pack, then load); the other cases' webs of the same variables pop right after and take the latest hand-out — no per-case numbering trick is needed; check with score.py/why.py which neighbour must cover which register | — | "CRI SWAR kernels pass 21" |
| a load kept in a volatile/low register in a later case while the target keeps it callee-saved, or the reverse; own-local loads turned into unnamed temps | a single-use load is substituted by the frontend into its consumer (a backend temp, popped in L1); the range-split webs of a case are visited LAST in the simplification scan (highest vids of their groups) and can stay >= 29 (L3: coloured first, volatiles) | the pointer step right after the load (`p0 = W(s0,0); s0 += stride;`) keeps it a variable; check levels with chaitin.py --verbose (L3 nodes) and search vid orders with the model before writing C (`vsearch.py`) | — | "CRI SWAR kernels pass 20" |

## Tooling kit

The research kit lives in `tools/research/` (see `tools/research/README.md` for what each part needs
that is not in the repo):

- **`tools/research/kit/`** — `variant.sh <unit> <variant-src> [FUNC]` = the unit's exact production
  command on a variant file with the object under /tmp, `OBJ= tools/bytecmp.py` plus an objdiff side-by-side, GCC and
  MWCC units, DOL and REL; `rtl.sh <unit> <variant-src>` = the `-dj -ds -dS -dR -dg -dl -dG -dL -fsched-verbose-9` dumps
  in one directory; README.md.
- **`tools/research/sngdbg/`** — the SN GCC cc1plus/cc1 built from tools/sn-gcc with env-gated
  hooks (`patches/dbg-hooks.patch`, `rebuild.sh`): `GDBG=1` global.c allocation order / priority / `regs_used_so_far` /
  conflicts / `regs_someone_prefers` per allocno + haifa live-length segments + loop.c movable thresholds, `LADBG=1`
  local-alloc qty order, `NOMALLOC=1` is_malloc off; byte-identical to production with no env var; use it via
  `CC1DIR=<dir with the hooked cc1plus/cc1>`.
- **`tools/research/mwccdbg/`** — cadmic's mwcc-debugger under retrowin32 (both are external checkouts, see its
  README.md): `ra.py lib/unit Func` dumps AST / PCode / interference graph, `rasum.py`, `rasim.py`, `chaitin.py` = the
  validated allocator model, `sched.py` = the scheduler model.

## Facts you need

- Compiler: **SN Systems ProDG (GCC 2.95.x)**, not CodeWarrior. Default toolchain `ProDG/3.9.3`,
  flags `-O2 -mfast-cast` (see `configure.py`; `-mps-float` is wrong: 16-byte FPR save slots). Game code is C++ (old GNU v2 name mangling,
  e.g. `Comeback__13CameraControl`). Some `game/` units are actually newlib C (strlen, atoi, vprintf...).
- Symbols: `config/G4BE08/sym_map.tsv` — address, size, section, unit, scope, current symbol name,
  original demangled name (from the debug build's `Bio4.sym`). Function boundaries and sizes are exact.
- Target asm per unit: `build/G4BE08/asm/<unit>.s` (dtk disassembly with symbolic relocations).
- The 110 REL overlays (rooms, enemies, players, weapons, tools) build byte-identical too; their units are `<mod>/<file>.cpp` with
  their own symbol files under `config/G4BE08/modules/<mod>/` — see "REL modules" below.
- Types: `include/types.h` (u8..f64). Shared class/struct definitions go in `include/<name>.h`;
  check existing headers before adding a type, and only extend, never rewrite, structs other units use.
- Globals seen via `r13`/`r2` (`@sda21`) are small-data; declare them `extern` with the exact symbol name.
- HAZARD: a module source that declares a DOL C-linkage function WITHOUT `extern "C"` (or a DOL
  static it imports) makes `sync_rel_symbols.py` rename the DOL symbol to the mangled/imported name
  in config/G4BE08/symbols.txt, breaking every other module's import (pl0e `Em_R0_Scenario` today).
  Declare DOL imports with the linkage the DOL header uses; after any module sync check
  `git diff config/G4BE08/symbols.txt` for unexpected DOL renames.

## Compiler

The compiler proper is a **native Linux `cc1plus`/`cc1` built from SN's GPL source drop, "2.95.3 SN BUILD
v1.79 for Nintendo Gamecube"** (`build/compilers/ProDG/3.9.3-v1.79/`, untracked build output). SN's
`cpp.exe` and `NgcAs.exe` from the 3.9.3 pack still run through wibo; `tools/ngccc.py` chains the three
exactly like `ngccc.exe -v` shows (same cpp defines, `-G 1024` → `-G1024`, `-D__OPTIMIZE__` only for
-O>0, `LANG=C` for the lexer). `configure.py --prodg-driver native` (default) selects it;
`--prodg-driver ngccc` is the old `ngccc.exe` (cc1plus v1.76) path. Rebuild the binaries with
`tools/sn-gcc/build.sh` with `SN_GCC_SRC` pointing at the extracted NGC_GNU_SRC/NGC source drop (copies the drop, CRLF→LF, `patches/linux-host.patch`
= i386 Linux host config + a dangling-pointer fix in `cp/decl.c`, `make -m32 -static`, installs into
`build/compilers/ProDG/3.9.3-v1.79/`; ~5 s). Why: the pack's five cc1plus builds (3.5/3.5b140 = GCC 2.95.2
SN v1.40, 3.7 = v1.46, 3.8.1 = v1.55, 3.9.3 = 2.95.3 SN v1.76) all share one fpmem-address unspec
between the GQR fast-cast conversions and the classic double-trick ones, so `reload_cse_regs` emits
cross-kind `mr` copies the original never has (see "Dead `mr`" below). v1.79's `rs6000.md` uses unspec 17
for the fast-cast family and 11 for the classic one, which is what the original binary does: on all 224
ProDG units the v1.79 build produces byte-identical objects to v1.76 except the 16 that mix the two
conversion kinds, and there the extra `mr`s disappear (Filter07/09/0bGXDraw, fadeDraw,
Draw_line3d_local_222, Esp11_SetParam, EspStrip_draw_poly, Light02/05/06_Move went to 100%). A remaining
diff is therefore never "compiler version": keep looking for a source form. The DOL has no compiler
string; its GCCI is "Ver.1.09 Build Oct 8 2004".

**Installed compiler patch (2026-09-11): `tools/sn-gcc/patches/shipped-build-temp-flags.patch`**, applied by
build.sh after linux-host.patch (also applied to the live untracked `tools/sn-gcc/src/gcc/function.c`). Six
one-line edits in gcc/function.c: `assign_temp` (line ~1172) and the five `assign_parms` parameter-slot
sites (~4546/4688/4745/4802/5062) set `MEM_IN_STRUCT_P` only for aggregate types and no longer set
`MEM_SCALAR_P` on non-aggregate stack temps / parm slots, so alias.c `fixed_scalar_and_varying_struct_p`
no longer exempts a compiler temp's reload from struct stores through a varying pointer. Evidence (whole-tree
harness, deleted; `h.py build NCCDIR LABEL` = every prodg_cc unit of build.ninja with the given
cc1plus/cc1, `h.py cmp base new` = masked per-function compare against the split objects): 763 units,
19119 functions identical before, **0 regressions and 0 changed objects** with the tree's sources; esp0a with
its four polymorphic copies written plainly (`*base = *esp; *e.p = *base; *base = *this; *p = *base;`) 6/6
and esp0e with `*p = *esp` 8/8 identical (4/6 and 7/8 with the unpatched compiler) -- both units are now pure
C. Widening to `put_reg_into_stack` (1740) regresses item `combine`, so the edit stops at assign_temp +
assign_parms. The tag hunt's "+7 newly identical" (objRobo x3, sce_com SceChapterEnd, t_se_at x3) did NOT
reproduce with the same edit against the committed sources (HEAD objRobo stays 16/19 with either compiler;
39 saved objRobo variants all byte-identical between the two compilers): those were concurrent source edits
to the tree between that run's base and variant builds. sha1 of the installed binaries: cc1plus
95ebed456cf1153736a3b03a4c05db9051c7dd8b -> 457337da552aa68676ea47f7d5c171d2e57038c8, cc1
65dae09ba6f31d245d6e4b45714ce5da34d3d419 -> f1cd07489d77fb41b3722f80c961d0cf756800be (tools/sn-gcc/cc1plus,
cc1 and build/compilers/ProDG/3.9.3-v1.79/ are the same files; `make -C tools/sn-gcc all` rebuilds them from
the patched src/, `./build.sh` from the drop applies both patches). The ninja prodg_cc rule depends on the two
binaries, so a compiler swap rebuilds all 763 units by itself; 111 files OK after the swap. What the patch
does NOT change (representatives tested by removing the workaround with the new compiler, all still needed):
snd SndCall `RefU16(blk)/(no)` (60 words without; address-taken register parms go through put_reg_into_stack,
not assign_parms), objRobo `GRef(pG)`/`FRef` (global scalar MEMs from make_decl_rtl), esp_app EffAreaUpdate
`"=m"(*(u32*) &pos)` filler (user local, expand_decl), trans SelfShadowSetup `ISet` reference stores, shadow
`FRef` unit-static reads. Only the "polymorphic copy vptr temp" family was an assign_temp effect.


### Known compiler-build differences (v1.79 source vs the original build)

Two argument-passing behaviours of the original binary are not produced by any source form with our
cc1plus and are therefore compiler-build differences (the original is a later SN build):
1. Argument-move order at calls with mixed int/float args: the original sometimes issues FP arg moves
   before trailing integer constants (`mr r3; fmr f1; fmr f6; li r4; fmr f7; li r5; li r6` for
   `init(int,int,int,f32 x7)`), GCC 2.95's `load_register_parameters` never does. A whole-tree
   experiment harness exists at ../re4-orig/sn-gcc-argorder/harness/ (10 s per
   run over all game units, with `calls.tsv` = 1209 classified call sites); every candidate rule
   either regresses matched functions or fails SetEmBarred's interleave, so NOTHING is installed.
   Workaround: a floats-first asm-labelled redeclaration (include/atari_init.h) for the affected callee.
   RESEARCH 2026-09-10 (/tmp/rank18 -> /var/tmp/rank18, whole-tree harness with `-D`-hooked haifa-sched.c /
   calls.c / regmove.c and alternative rs6000.md timings; see "COMPILER-DIFF #1 / #8 research" in
   docs/research/compiler.md): SN's haifa-sched.c, calls.c, regmove.c and rs6000_adjust_cost/adjust_priority are stock; every
   rank_for_schedule / weight / arg-emission-order / md-latency variant regresses 4-13558 matched functions
   and fixes at most 2; the residues are decided by whether the FP argument's source pseudo DIES at the arg
   move (weight 0 = issued before the `li`s) -- in the original the FP constants reach the call as dying
   pseudos rematerialised by reload (the #13 constant handling), in ours cse folds the second use to a
   hard-reg copy `fmr f2,f1` (weight +1). Nothing installed; the alias stays. The #8 prologue family
   (`fmr fN,f1` ranked as if f1 did not die: emshield setFall, emwep setThrow, r223 mode, r226 idx, pl_wep
   PlWepAutoTrack mode, em_sub EmYarareContactCk out -- the only 6 such prologues in the whole tree) HAS a
   source lever (2026-09-10, see "COMPILER-DIFF #8 closed" in docs/research/compiler.md): a `register T x
   asm("<incoming reg>")` read in a DIFFERENT machine mode (f64 for an f32 parameter, s16 for an int one)
   inside a codeless `asm("" : "=m"(field) : "f"/"r"(x))` in block 0 keeps the incoming register live past
   the copy, because regmove's optimize_reg_copy_1 only moves the death when the modes match. All six are
   closed (emshield, r223 Matching; emwep setThrow 0 words; pl_wep/em_sub prologues fixed, other residues left).
2. Narrow-argument extension: the original sign/zero-extends narrow values at some call sites and
   entries (`extsh`, `clrlwi 24/16`) where ours treats them as promoted. Workaround: asm-labelled
   alias with the signed/narrow type (id_sys.h `setTimeS`).
3. (candidate) Frame-address PRE: no `addi rX,r1,N; mr rY,rX` pattern exists anywhere in the original
   asm, while our gcse routinely creates a pseudo for `&local` used in several blocks; the original
   also never cross-jumps a single-insn tail (`find_cross_jump` minimum). Under investigation with the
   harness; until then use the `&local` levers (frame-offset-0 local, inline helper taking `Vec*`).
   RESEARCH 2026-09-10 (/tmp/gcse3: copy of the /tmp/sched5 harness, `gcc/` = SN source with `-D`
   hooks in gcse.c/lcm.c/flow.c, `t3.py` = 18-function #3 test set, `dump.sh CFG UNIT -dG` = gcse
   dump of one unit; base 18586/19929 identical). Result: NO configuration reproduces the original;
   nothing is installed.
   - Source facts: SN's gcse.c and lcm.c are byte-for-byte stock 2.95.3 apart from the SN header;
     toplev.c's -O2 defaults are stock (gcse, rerun-cse-after-loop, rerun-loop-opt, strength-reduce,
     expensive-optimizations, regmove, sched1/2, caller-saves, force-mem, thread-jumps,
     cse-follow-jumps/skip-blocks); its only rest_of_compilation difference from stock 2.95.3 is the
     2.95.2 order `cse1; delete_trivially_dead_insns; jump` (stock deletes after jump), which is
     what our build does anyway. 2.95.3 gcse: MAX_PASSES 1, no bb/edge bail-outs, no hoist_code
     (-Os only), `want_to_gcse_p` rejects only REG/SUBREG/CONST_INT/CONST_DOUBLE/CALL (so `(plus fp
     N)`, `(plus reg 1)`, `(high sym)` are all PRE candidates), a CALL_INSN kills only call-used hard
     regs and MEMs (`mem_set_in_block`), never a pseudo expression; `pre_lcm` is Muchnick's
     block-based formulation on antloc/transp only, `delayin/delayout` zero-initialised (an insertion
     is never delayed through a loop header), `optimal & redundant` is empty by construction so
     `pre_insert_copies` is dead code: every partially redundant expression gets a fresh
     recomputation `R = expr` at the end of each optimal block (`insert_insn_end_bb`: before a
     block-ending jump; before the first parameter load if the block ends in a CALL_INSN, which only
     happens inside an EH region; else after the last insn), also in blocks that already compute it.
     The edge-based LCM (`pre_edge_lcm`, insertion on edges, working copies) is mainline 1999-10-17
     (Andrew MacLeod, ChangeLog.2), never on the 2.95 branch; 2.95.3's flow.c already has the edge
     cfg (`split_edge`, `insert_insn_on_edge`, `commit_edge_insertions`).
   - Whole-tree table (regressions = functions identical with the installed compiler that stop being
     identical / newly identical; lists via `python3 h.py cmp base CFG` in /tmp/gcse3):
       -fno-gcse                          4192 / 3 (mes move, t_scroll edit_select_sub, r20d moveWall)
       -fno-rerun-cse-after-loop          cc1plus ICEs in 164 units (unusable)
       -fno-rerun-loop-opt                 433 / 5 (r108 str_check + r203 StreamCheck 18 -> 0, pl0f
                                                   R1_Drop 42 -> 0, t_id DbRandom): the "double EmMgr
                                                   chain" is the SECOND loop pass hoisting the inner
                                                   loop's pseudo, not gcse (#3 tag wrong)
       -fno-strength-reduce               1225 / 0    -fno-thread-jumps          418 / 1
       -fno-expensive-optimizations       2796 / 1    -fno-regmove                 1 / 0
       -fno-schedule-insns               13844 / 0    -fno-move-all-movables,
                                                     -fno-reduce-all-givs          0 / 0 (default off)
       -fno-exceptions                    no change on r120/r213/r226/event (no EH regions anywhere)
     `-D` variants (regressions counted over the 16 test units = 539 identical functions; none makes
     any #3 case identical):
       GCSE3_EDGE (GCC 3.0 edge LCM: comp-aware earliest, ones-initialised LATER, edge insertion,
         working pre_insert_copies, fake exit edges)      179 / 0  (R120Event 114 -> 29, R209Main
                                                                    149, MoveDoor02 41, R226Init 0 -> 105)
       GCSE3_EDGE + no copy-prop in the final cprop       178 / 0
       GCSE3_CALLENDS (calls end blocks, gcse cfg only)    79 / 0; + no parm-load search 73
       GCSE3_CALLKILL_TRANSP (a call kills transparency)  124 / 0  (R120Event 24, initPuzzle 88)
       GCSE3_CALLKILL_LOCAL (call kills antloc/avail)      29 / 0
       GCSE3_COPY_COMP_A/B (copy instead of recomputing
         in blocks with comp set; A after the occurrence,
         B at the block end)                            96 / 0, 64 / 0  (R120Event 24; R213Init 15)
       GCSE3_DELAY_GFP (block LCM, delayin as a greatest
         fixed point = insertions delayed into loops)     36 / 0  (R209Main 124 -> 76 with the
                                                                    target's outer givs; MoveDoor02 38,
                                                                    PartsBombControl 358)
   - Evidence that the original's gcse IS the 2.95.3 block-based PRE: R226Init (matched) has fresh
     `lis r21`/`lis r19` for high(r226_work)/high(pG) at the top of bb 0 although bb 0 already
     computes both (insns 13/49, `lis r11`), plus `addi r27,r1,80; addi r24,r1,16; addi r31,r1,32`
     hoisted there — the recompute-at-earliest-block shape; every variant that delays or copies
     (edge, comp-copy, dgfp) breaks 36-179 of the 539 matched functions in the sample. `high(sym)` is
     CONSTANT_P, so cse never merges two of them and only gcse does — the per-use `lis; addi` pairs
     the target keeps in R120Event's tail (`sym+288@ha`, `sym+308@ha`: CONST offsets folded into the
     reloc) are an address-form difference, not PRE; R120Event's gcse part is exactly 114 -> 24 under
     any variant that stops PREing high(pG) to the end of bb 0.
   - Sharpened #3 (what the original does and does not do): (a) it does PRE `(high sym)` and `(plus
     fp N)` into the end of the earliest block including recomputations in blocks that already
     compute the expression, and into inner-loop preheaders across call-free loops (initPuzzle's
     setLayout loops) — same as ours; (b) it does NOT hoist a single-occurrence loop-latch expression
     (`j+1`, `n = 4`) across an inner loop that contains calls (R209Main, em3c PartsBombControl,
     r209 2ndBattleEmSet, initPuzzle's first loop): with -fno-gcse ours forms the target's outer givs
     (`addi r22,4; addi r23,8`) and GCSE3_DELAY_GFP halves R209Main, but neither "delay through
     loops" nor "calls kill transparency" reproduces it without breaking (a); (c) for an expression
     computed in bb 0 and reused after a call (R213Init `P = fp+24; r3 = P; bl memset`) the target
     defines the reaching reg BETWEEN the argument move and the call (`addi r3,r1,24; li r4,0; mr
     r31,r3` = regmove's optimize_reg_copy_1 on `r3 = P` seeing `R = P` after it), whereas ours
     inserts `R = fp+24` after the block's last insn (after the call, cse2 -> `R = P`, P lives across
     the call); a copy right after the computation (3.0 placement) is undone by cse2's `(set REG0
     REG1)` swap (`R = fp+24; P = R`, 15 words) and copies at the block end, calls-end-blocks and
     no-parm-search insertion do not give it either. (b) and (c) together are not any GCC 2.95/3.0
     gcse variant we could build; treat #3 as a compiler-build difference with unknown mechanism and
     keep using the `&local`/`asm("" : "+r")` levers.
   RESEARCH 2026-09-10, cse cost side (/tmp/highcost: h.py harness on a fresh src/ snapshot, base
   18676/19929 identical; `gcc/` = SN source with `-D` hooks HC_HIGH=n / HC_LOSUM=n in rs6000.h
   CONST_COSTS/RTX_COSTS and HC_REGPREF[_ALL] in cse.c's trial loop; `mk.sh NAME "-Ddefs"`). NEGATIVE:
   every cost/preference variant regresses thousands of functions; nothing is installed.
   - Source facts: SN's cse.c is stock 2.95.2 apart from PROTO removal (it still deletes the dead code
     after a jump cse made unconditional; 2.95.3 leaves that to jump/flow); `CONST_COSTS` (HIGH,
     CONST_INT, CONST, SYMBOL_REF, LABEL_REF, CONST_DOUBLE all `return 0`) and `ADDRESS_COST 0` are
     byte-identical to stock 2.95.3 rs6000.h, so HIGH's cost 0 is NOT SN-specific. `COST(x)`: REG = 0
     for fp/sp/ap/fixed hard regs and hard user vars, 1 for a pseudo, 2 for another hard reg; anything
     else = 2 * rtx_cost(x, SET), so `(high sym)` = 0, `(lo_sum R sym)` = 2*(2+1+0) = 6, `(plus fp N)` = 4.
     `cse_insn`'s trial order is `src_folded <= src <= src_eqv <= src_related <= elt` (all `<=`, i.e.
     src_folded wins ties), but for `(set P (reg R))` there is no tie and no fold: `fold_rtx` returns a
     REG unchanged (src_folded = R, cost 1), gcse's pre_delete adds no REG_EQUAL note, and the trial
     that wins is the hash-table class head `elt->first_same_value` = the `(high sym)` constant (cost
     0, `insert` sorts a class by cost, so a constant always heads its class above the pseudo that
     holds it). The dead-test note above ("cse_insn prefers src_folded on ties") names the wrong rule;
     the effect is the same: cost 0 < 1 re-materialises `lis`.
   - Whole-tree table (regressions = identical with base that stop being identical; test cases = words
     before -> after: r10c SetEmHitAtari 95, R120Event 114, R213Init 14, SubScreenTask 72, t_sce_at
     basic_menu 215, R11bInit 14, r209 SwitchAppearCheck 7 / BridgeAppearCheck 7, R226Init 0):
       HC_HIGH=1 (rtx_cost HIGH 1, COST 2 > REG)      4053 / 0 fixed; r10c 121, r120 15, r213 69, ss 493,
                                                       menu 267, r11b 49, r209 68/61, R226Init 175
       HC_HIGH=2                                       4053 / 0 (identical objects to HC_HIGH=1)
       HC_LOSUM=0 (lo_sum cost 0, no operand sum)      4687 / 1 (emmine R1_Fall 8 -> 0); r10c 287, r120 114
       HC_LOSUM=1                                      4610 / 1 (same fix)
       HC_REGPREF (trial loop skips a HIGH trial when
         the insn's src is a pseudo REG)               2997 / 0; r10c 388, r120 13, r213 69, ss 500,
                                                       menu 265, r11b 42, r209 17/7, R226Init 172
       HC_REGPREF_ALL (skips any CONSTANT_P trial)     3047 / 0
       HC_HIGH=1 + HC_REGPREF                          4051 / 0
     Only R120Event moves toward the target (114 -> 13/15: the x4F8E test becomes `lwz r9,N(r31)`,
     the 13 left are the tail's per-use `sym+288@ha/@l` pairs); every other named case gets worse.
   - What the regressions show: the ORIGINAL re-materialises a copied `high` exactly like stock cse.
     sce_sys SceExecEventCancel (matched) has `lis r9; addi r31,r9; lis r9; addi r9,r9` for one high
     with no holder register (REGPREF gives `lis r28` callee-saved + two `addi rX,r28`, 9 words),
     db_light edit_light_priority has a fresh `lis r9; lwz r9,N(r9)` where REGPREF uses r30, R226Init's
     fresh top-of-bb-0 `lis`es break under every variant (172-175 words, three more callee-saved regs),
     and HC_HIGH additionally merges every second `lis sym@ha` of an ebb into a copy (49 words in
     SceExecEventCancel alone). So the "target uses R directly" family (r10c r27, r120 r31, R213Init,
     SubScreenTask, r11b) is not a cost or source-preference rule: in those functions the original's cse2
     never SAW `R = high` when it reached the redundant copy -- a different extended-basic-block/path
     structure (the #12 taken-branch/AROUND family) or a different PRE placement (#3 proper), not cse's
     choice. Flag probes with the installed compiler: `-fno-cse-follow-jumps` changes nothing on
     r120/r10c/r213; `-fno-cse-skip-blocks` takes R120Event 114 -> 36 (the AROUND path from bb 0 is what
     carries `R = high` into the tail) but regresses 3782 functions whole-tree, so the original's cse does
     skip blocks too. Do not revisit HIGH/LO_SUM costs or REG preference in cse; nothing in /tmp/highcost
     is to be installed.
4. Narrow-argument truncation: the original build does not truncate `int` -> `u16` arguments at call
   sites nor a wider value on a narrow `return`, but masks a u8-returning call assigned to a u16.
   Workaround: asm-labelled int-view / narrow-view declarations (item.h `constructI`, `searchI`).
5. haifa interblock scheduling: our cc1plus's `find_rgns` never marks leaf blocks (only successor =
   EXIT) as reached, so any function ending in a plain return block gets single-block regions and no
   interblock motion; the original formed regions there (shadow `ShadowTrans` `mr r3,r31` hoist over
   a small loop). A leaf-fixed build (/tmp/sngcc-leaf) forms the regions but then moves far more than
   the original, so the original's motion policy differs too. Compiler-build difference, not source.
   RESEARCH 2026-09-10 (/tmp/sched5: harness h.py = full 755-unit ProDG build in 10 s with any
   cc1plus + flags, per-function masked compare vs the split objects; 18543/19929 functions identical
   with the installed compiler). Result: the ORIGINAL's find_rgns is stock 2.95.3 — it does NOT mark
   leaf blocks either. The two sentences above are wrong; nothing is installed.
   - Source facts: SN's haifa-sched.c is byte-for-byte stock 2.95.3 apart from the SN header. The
     leaf-marking (`if (current_edge == 0) dfs_nr[child] = ++count`) is mainline commit "Tue Aug 24
     22:56:35 1999 Jeffrey A Law: (find_rgns): Mark a block found during the DFS search as reachable"
     (gcc/ChangeLog.2), never backported to the 2.95 branch; GCC 3.0 sched-rgn.c has it with the
     comment "temporary until haifa is converted to use rth's new cfg routines". Not a host miscompile:
     an -O0 host build of haifa-sched.c gives identical objects (shadow, em10, cam_ctrl, Espgen42, r226).
   - Stock rule (what ours and the original do): `build_control_flow` creates no edge to EXIT, the DFS
     sets dfs_nr only for blocks with an out edge, so a block whose ONLY successor is EXIT (a return
     block with at least one insn: `mr r3,x`, `li r3,0`, a call before the fall-off) is "unreachable"
     and the WHOLE function gets single-block regions — loops included. A function with no such block
     (void, ends in a loop/if whose exit edge comes from a block that also has a real successor, e.g.
     em10 `setNoSuspend`, r203 `r203_GanadoWandering`, the `for(;;)` task functions) forms regions
     normally: inner loops <= 10 blocks / 100 LUIDs, or the whole function if it is loop-free and within
     the limits. Levers, both directions: a `return` anywhere (even inside the loop) kills every region
     of the function (r203_GanadoWandering with `if (..) return;` instead of `on = 0`: all hoists gone,
     body identical to the target); removing the only leaf enables them.
   - Full-build table (regressions = functions identical with the installed compiler that stop being
     identical; lists in /tmp/sched5/reg_<cfg>.txt):
       leaf fix only                                   5129 regressions, 0 newly identical
       leaf fix, loop regions only (no whole-function) 1721 / 0
       leaf fix + -fno-sched-spec                      2869 / 3 (Espgen42 AddWaterPower 26->0,
                                                        Espgen43 AddSandPower 5->0, r226
                                                        R226EventRoboWalkPassageStart 9->0)
       leaf fix + -fsched-spec-load[-dangerous]        5129 / 0 (= leaf; -fno-sched-spec-load is default)
       leaf fix + MAX_RGN_BLOCKS 1000 / INSNS 100000   12746 / 0;  + -fno-sched-spec 9911 / 4
       -fno-sched-interblock (installed compiler)      100 / 1 (r203_GanadoWandering 27->0)
     So the original does interblock motion exactly where stock does (100 leaf-free functions need it,
     530 of 755 units regress when leaf functions get regions) and never where stock does not. The
     three -fno-sched-spec "fixes" have 0 interblock motions in the dump — region priorities changed
     the intrablock order/regalloc; r226 is really the source form `if (i++ == 0x2C)` /
     `if (i++ == 9)` (PassageStart and BridgeStart 0 words with the installed compiler; not applied).
   - None of the #5-tagged test cases moves under ANY configuration (em10LostHead 59, em3a R1_Fix 7,
     em38 plemEscape 27, em35 R1_Critical 4, shadow make_comn_fit_light 3, cam_extra CameraBinocular
     ctor 71, texture TexRegist 91, r105 execOpenCover 25: identical numbers for base/leaf/nospec;
     bigger regions only make them worse). They are intrablock tie-breaks / register allocation, not
     region formation. em10LostHead's two `cmpwi cr4,a,3` copies cannot be haifa at all: haifa never
     duplicates an insn (update_bbs only feed check_live/update_live), and with a whole-function
     region (bigrgn) the compare still sits once at the join. (RESOLVED 2026-09-10: they are gcse PRE
     insertions on a CFG with a dead `if (a == 3)` test falling through into case 1 -- see the Ganado
     shared library seventh pass; the other #5-tagged residues deserve the same "dead test" check.)
   - The "hoist" workarounds tagged `COMPILER-DIFF: 5` are not haifa either: with the launders removed
     and regions forced, shadow ShadowTrans stays 4 words (`mr r3,r31; lwz r0,N(r3)` — the load is
     REBASED on the copy, which only cse can do, so the copy preceded the test in the original RTL
     before sched1), shadowScrModelRender 6 -> 35, r20c R20cKaigaMoved 9 (one `li r8,0` pseudo shared
     by two conditional stores = one RTL set, not a motion), db_light draw_light_graph 14 -> 76.
     Their origin is an expansion/source-form difference (inline wrapper, a shared local), to be found
     per site; the r203 note "adding ~24 LUIDs makes ours match" is the region-size limit, i.e. the
     original's loop had more RTL, again pre-sched.
   - Do not install anything from /tmp/sched5. Sharpened #5: "stock 2.95.3 haifa in both builds;
     differences appear only through (a) the leaf rule (a return block anywhere disables all
     interblock motion) and (b) the region limits (10 blocks / 100 LUIDs) applied to RTL whose size
     differs from the original's" — both are source-form levers, not compiler-build differences.
6. Cross-jump survivor choice: our jump2 always keeps the *last* identical `li r3,1; b end` copy;
   the original sometimes keeps an earlier arm's copy and cross-jumps later ones into it (pl_class
   `isKamae`), and never merges single-insn tails (item `use`). Compiler-build difference.
   RESOLVED 2026-09-10 (harness /tmp/cd6/h, whole-tree build with a `-D`-hooked jump.c; see "COMPILER-DIFF #6
   resolved" in docs/research/compiler.md): the original's jump2 cross-jump POLICY is stock 2.95.3 = ours. Every
   variant of it -- fall-through candidate with minimum 2 or not tried, no CODE_LABEL `--minimum`, oldest-first
   jump_chain, no jump-around-jump bonus, no USE move, no range swap, swap in round 1 -- regresses 428-3767 of the
   18695 matched functions and fixes none. The two sentences above are wrong: ours keeps the first copy too when
   the first copy's own scan fails (isKamae `goto ng` form), and single-insn tails DO merge (label rule /
   fall-through minimum 1). Every #6 residue is an RTL-at-jump2-entry difference with a source lever: a `(use r3)`
   or flow.c `(use (const_int 0))` insn ending the fall-through arm, sched2's position of `li r3,K`, jump1's layout
   (USE move, range swap), or a same-looking insn with a different RTL mode/register form. Model:
   tools/research/xjump.py (jump2 loop, candidates, minimum rules; validated on -dJ dumps). Not a compiler-build difference.
Do not spend unit time on any of these; use the workarounds and move on. Policy: every workaround
for a compiler-build difference (asm-labelled aliases, `asm("" : "+r"(x))` launders, `register ...
asm("rN")`, dead `p = 0` initialisers used only to shift gcse/loop.c counts) must carry a comment
`// COMPILER-DIFF: <which item>` so they can be removed mechanically if the original build turns up.
Pure C (project decision 2026-09-11): no whole-function `asm` transcription of target machine code anywhere in the
CRI units (SDK units keep Nintendo's own asm functions; CRI's uty_ppc.c GQR save/restore is CRI's own asm); byte
identity must come from C or from identifying the real compiler build. Small tagged pins/levers stay (CRI pass 8).

### Host-dependence of our cc1plus (negative result, do not re-investigate)
The configured cc1plus is a 32-bit static i386 build (HOST_WIDE_INT = int, like Win32). cse.c hashes
SYMBOL_REF/LABEL_REF by host pointer and varasm's const hashes include pointers, but they only select
a bucket; every lookup is exact-match and every ordering decision comes from insertion/call order.
All 610 ProDG units produce byte-identical `.s` under: heap base shifts, heap padding, ASLR on/off,
MALLOC_PERTURB_, pattern/zero stack fills, x87 53-bit precision (Win32 CW), an unstable heapsort as
qsort, cse/varasm/type hashes collapsed to one bucket, an -O0 host build and a clang host build (the
clang build differs only by an extra `.size` from an uninitialised local in SN's toplev.c patch, which
cannot change DOL/REL bytes). The residues described as "which equivalent register", "`lis sym@ha`
pseudo choice", "qty tie", "LUID tie" are v1.79-source-vs-original differences, not host effects.

### Later SN build search (negative, 2026-09-09)
No SN GCC for GameCube newer than cc1plus v1.76 (binary) / v1.79 (GPL source, 2003-06) exists in any
public place (decomp.me/decomp.dev packs, archive.org sn_sys_consoles_2 / prodg-gamecube / GameCubeSDK,
MarioCube, GitHub, Wayback of snsys.com whose support downloads were login-only). RE4's crt0 links
libsn v60 (public pack: v59, 2003-07), so Capcom had post-3.9.3 support patches, presumably including
the compiler build that produces differences #1-#9. If a later drop ever surfaces: build it with
tools/sn-gcc/build.sh into its own dir, smoke-test emwep `setThrow` (#1) and pl_class `isKamae` (#6),
then remove `// COMPILER-DIFF:` workarounds one at a time.

## Per-unit compiler flags

Not every game unit is `-O2`. The sound driver (`snd_iss*/seq*/str*/sub*/main/efx/ram`) is C++ with
`extern "C"` linkage compiled at **-O0** (frame pointer in r31, every local in a stack slot, args reloaded
before every use). `config/G4BE08/objects.py` has `UNIT_CFLAG_OVERRIDES` (flag -> replacement per unit).
If a unit's functions all start with `stwu; mflr; stw r31; mr r31,r1` and reload parameters from the
stack, suspect -O0 before spending time on -O2 forms. snd_drv.h/snd_sdk.h hold the driver types and
the trick for pulling in SDK headers under ProDG.

## Workflow for one unit (`game/foo`)

```sh
python3 tools/unit_info.py game/foo            # functions, sizes, demangled names, current match %
sed -n '/^\.fn NAME/,/^\.endfn/p' build/G4BE08/asm/game/foo.s   # asm for one function
# write src/game/foo.cpp (or .c for the newlib/C units), then:
ninja build/G4BE08/src/game/foo.o              # compile (errors are printed)
python3 tools/sync_symbols.py build/G4BE08/src/game/foo.o   # renames placeholder symbols to the mangled names
ninja                                          # rebuild, refresh report (must still say main.dol OK); ninja reruns configure itself
python3 tools/fdiff.py game/foo <mangled_symbol>   # side-by-side diff of one function (only differing lines; --all for everything)
python3 tools/bytecmp.py game/foo                  # THE judge: every section byte-compared with relocs resolved by address;
                                                   # `... game/foo FUNC` = word diff of one function. objdiff %s are reloc-NAME artefacts.
```

Repeat until `tools/bytecmp.py` says IDENTICAL (unit_info's 100% is neither necessary nor sufficient). Then
remove the unit from `NON_MATCHING` in `config/G4BE08/objects.py`, run `ninja`, and confirm the SHA-1
check still passes. If the link fails after linking a unit, its data/rodata layout differs from the
original: fix the source, do not link it.

## Matching rules of thumb (GCC 2.95)

- ProDG has no header dependency tracking: after editing a header, `touch` the sources that include it.
- `size_t` is `unsigned int` (see `include/newlib_local.h`); `unsigned long` changes register allocation.
- `include/global.h` has `pG` (`GlobalWork`) and the `BitOn(u32&, bit)`/`BitOff` helpers: the original
  sets/clears flag bits through a reference, which makes GCC reload `pG` after the store and keep
  consecutive `|=` separate. Use them where the asm shows that pattern.
- `include/dolphin/*.h` are CodeWarrior-only (SDK units); game code uses `include/vec.h` for Vec/Mtx/PS*.
- Loops that search and set: `for (...) { if (hit) { ...; break; } }`; a `return` inside the loop gives a different tail.
- Zeroed local arrays (`T* a[3] = {NULL, NULL, NULL}`) become a `memset` libcall with `crclr cr1eq`.
- GCC 2.95 puts vtables of classes without a key function and template/inline instantiations in
  `.gnu.linkonce.*` sections; `tools/fold_linkonce.py` (post-build for all `game/` objects) folds them
  back the way the original linker did. Classes are declared in `include/cManager.h` (`cUnit`,
  `cManager<T>`), `model.h` (`cCoord`, `cModel`), `em.h`, `obj.h`, `esp.h`, `light.h`.
- `cUnit::operator delete` takes `unsigned int`, not `u32` (`u32` is `unsigned long`).
- Header-owned strings (`cManager` messages, `__FILE__` from inline range checks via `#line`) appear in
  each unit's `.rodata`; unused inline functions still emit their strings.
- Field names must be agreed across units: a header field may only be renamed if you update every user.
- objdiff's 100% only covers functions present in the target; **also check the compiled object's `.text`
  size equals the split object's** (`dtk elf info build/G4BE08/src/<unit>.o` vs `build/G4BE08/obj/<unit>.o`).
  ProDG emits every in-class member function body out of line (to `.text`, not linkonce), so an inline
  member the original never had grows `.text` and breaks the DOL even though objdiff reports 100%.
- Fast-cast GQR mapping: explicit `(f32)` of a u8/u16/s8/s16 load → `psq_l` with qr2/qr3/qr4/qr5; f32→u8
  stores use `psq_st qr2`; a value already in a register goes through the `0x43300000` double trick.
- `x + -1.0f` gives `fadds -1.0`; `x - 1.0f` gives `fsubs`. `no / per` and `no % per` share one `divwu`.
- Two consecutive `|=` on a u8 field merge into one store unless another store sits between them.
- Stack slot order follows declaration order; float register assignment of same-lifetime locals depends on
  declaration order. `for (i = n - 1; i >= 0; i--)` → `subic.`/`bge` loop.
- Zero-initialised static locals go to `.sdata`, uninitialised to `.bss`; `static const` locals are named
  symbols; anonymous constant-pool aggregates come from non-static constant expressions.
- A block-local `Work* w = &work;` inside the branch vs at function top decides whether the `addi` is hoisted.
- `pLog` is a `cLogPtr` struct wrapper (`include/db_log.h`): loading it as a struct member stops the
  scheduler hoisting the load above stores through `this`.
- GX FIFO: `GXWGFifo` is a linker-provided absolute symbol (`extern volatile WGPipe GXWGFifo[]` in
  `include/gx.h`, `GXWGFifo = 0xCC008000` in `config/G4BE08/ldscript.ld`), so the stores are
  `lis rX,GXWGFifo@ha` + `GXWGFifo@l(rX)` (same bytes as `lis 0xCC01`/`-0x8000`). objdiff shows the
  reloc as ARG_MISMATCH against the split object; the report and the linked DOL are what count. The
  address must be a SYMBOL_REF: with a constant address (`*(volatile WGPipe*)0xCC008000`, a struct
  member at 0xCC000000, or a `const` pointer) the scheduler issues the `lis/lfs` of `Screen` and
  the constant pool before the FIFO `lis`, and `*(volatile WGPipe*)0xCC008000` even gives `lis`/`ori`
  into a base register. An 8-byte `extern volatile WGPipe GXWGFifo;` lands in small data (`@sda21`),
  hence the incomplete array.
- Dead `mr rX,rY` between unrelated GPRs around `-mfast-cast` conversions are copies of the fpmem
  (stack slot) "address" pseudo: every conversion is split before sched1 into `loadaddr` (emits no
  code) + store + load with a scratch register; after reload, `reload_cse_regs` replaces a later
  `loadaddr` with a copy of whichever hard register still holds the previous one (forgotten at a
  code label, at a call, or when that register is overwritten; deleted when both got the same
  register). sched1 always hoists a `loadaddr` (no inputs, long chain through the store/load) to
  the first free integer slot of the block, so two conversions in one block overlap and get
  different scratch registers: a copy is inevitable unless a call, a label or an overwrite lies
  between them. Same-kind copies are in the original too (`mr r9,r11` in Filter07GXDraw, three in
  matched esp4c `move`).
  **Cross-kind copies (fixed by the v1.79 compiler, see "Compiler"):** the original compiler keeps TWO
  address values — one for the
  classic fpmem conversions (`stfd/lwz` fix, `stw/stw/lfd` float: `fix_truncdfsi2`/`floatsidf2`) and
  one for the GQR fast-cast ones (`psq_st`+`lbz/lhz`, `stb/sth`+`psq_l`: `fixuns_truncsfqi2`,
  `floatqisf2`, `floathisf2`, ...). It never copies across the two kinds; the shipped cc1plus v1.76 (all
  five ProDG builds, with `-mfast-cast`, `-mps-nodf`, `-mps-float`, `-msafe-sda`) emits one
  `(unspec [(const_int 0)] 11)` for every conversion type, so `reload_cse_regs` also copies psq↔classic;
  the v1.79 source build uses unspec 17 for the fast-cast family and matches. Evidence: every extra `mr`
  in Filter07/09/0bGXDraw (4 each), fadeDraw (u16 `psq_l` → int magic), esp15 `move`, esp19
  `Draw_line3d_local_222`, esp11 `Esp11_SetParam` is a cross-kind copy, and in esp11/esp19 the target
  shows the two chains directly: `mr r11,r10; mr r8,r10; mr r7,r10` (int→f32) interleaved with
  `mr r3,r5; mr r30,r5; mr r29,r5` (f32→u8) where r5 is a fresh `loadaddr` although r10 held the
  address. No matched unit has a cross-kind copy. Classify with the `.greg` dump: a `movsi` whose
  source register was last used by a `*_store1/_store/_load` of the other kind. Source forms cannot
  change the unspec (statement order, u8/int/u32/s16 at the conversion, `(u8)(f64)`, locals before
  the `if`, u8 helper params, direct FIFO stores — all tried); with the v1.79 build these functions
  match. If you still see a cross-kind copy, check that `build.ninja` uses `tools/ngccc.py` (run
  `python3 configure.py`). `rnd Rnd` (`clrlslwi`/`mr r0,r9`) is a different, plain
  register-allocation diff.
- Loop shapes: `if ((v = x) == 0) { do {...} while ((v = x) == 0); }` duplicates the entry test;
  `for` + `break` gives `cmpwi`/`bgt` without ctr, `return` in the body gives `bdnz`.
  `for (w = wk, i = 0; ...; w++, i++)` vs separate init changes callee-saved register choice.
- A loop-invariant `&Global` held in a register (`addi r31,...,sym@l`) means the source used a pointer
  variable (`MessageControl* m = &cMes`); a two-step `addi rX,rX,sym@l; addi rX,rX,4` comes from an inline
  accessor returning `&this->member`.
- Header-owned strings and initializer templates of unused inline functions are emitted in parse order;
  all-zero aggregates emit nothing; constant pools are emitted at each function's end (`.rodata`
  interleaves them).
- Bio4.sym scopes are unreliable: symbols marked `local` in `sym_map.tsv` are often global (called from
  other units) — check callers' asm before making them `static`.
- Stores through raw pointers/references (non-struct MEMs) make GCC reload `pG` afterwards and keep
  loads in source order; struct-member stores don't (`TOOL_FLAG` raw-offset accessors in `t_util.h`).
- Uninitialised globals are emitted in order of *first declaration*, header externs included, so header
  extern order dictates `.bss`/`.sbss` layout; initialised objects are emitted at their definition.
  Statics/initialised globals ≤ 8 bytes go to `.sdata` (-G 8); 16-byte zero-initialised objects to `.data`.
- `__attribute__((aligned(32)))` buffers create the 0x10 `.bss` holes; a 32-aligned following unit leaves
  `.sdata` padding the split objects don't have (`t_util.cpp` pads with `asm(".section .sdata; .balign 32")`).
- Clamps: `x = x < 0 ? A : (x > B ? 0 : x)` → li/mr chain, one store; `if (v >= 0) { n = v; if (n > M)
  n = M; } else n = 0;` → blt/li-at-end. `if (c) { ...; return X; } rest; return Y;` lays out `rest` first.
- Register args are evaluated left to right and kept live across a nested call in the argument list;
  originals often compute the inner call into a local first.
- `memcpy(dst, "literal")`/`strcpy` with a constant source inlines to word/byte moves; `char s[64] = ""`
  → `lbz` + `memset(s+1, 0, 63)`; `Vec v = {0,0,0}` inside a loop → `memset` per iteration.
- The original never moves a load of a global (`pG`, a static float) above a store made through `this`
  or a member pointer; ProDG does unless the store goes through a scalar reference — `FSet(f32&, f32)`,
  `BitOn16(u16&, u16)` in `include/global.h` reproduce the original order. No compiler flag changes this.
- `fabsf` is a volatile asm (`include/math_sub.h`) and acts as a scheduling barrier.
- Frame layout: `Vec`/`Mtx` locals are 8-byte aligned; function-level locals in declaration order from
  0x8, block-scoped ones after the block's temporaries, freed block slots reused — block scoping matters.
- GCSE: an expression used in both `if/else` arms and after the join must be written inline, not
  pre-computed into a variable. `ret = f(); ...; return ret;` in every branch stops cross-jumping of
  identical call tails.
- `SetFreeWork(EspGenWork*, u32* seed)` is the real cEsp virtual signature (seed in r5).
- GCC 2.95 emits *every* in-class inline member of a class whose vtable is emitted in the TU (key
  function defined here), used or not. Unused inlines of non-polymorphic classes and unused free/static
  inline functions are not emitted, but their string literals and float constants still land in
  `.rodata`. So: stray constants/strings in the target `.rodata` with no body → an unused inline of a
  non-polymorphic class or a free inline, never an extra member of the polymorphic class.
- `switch` on a `u8` with `case 0:` sharing the default body: `cmpwi 2; beq; ble default; cmpwi 3; ...`.
- Chained `a = b = c = 0` shares one zero register across `stw`/`stb`; separate statements get their own.
- `#line N "D:/Bio4/Prog/<unit>.cpp"` before `MEM_ALLOC` reproduces `__FILE__`/`__LINE__` strings.
- ProDG's scheduler ranks ready insns by register pressure before priority; no cross-block hoisting inside
  functions with loops.
- A global pointer loaded twice around a block copy: a plain `T* pT` gets the second load hoisted; reading
  it as a struct member (`((Wrapper*)&pT)->p`, like `cLogPtr`) keeps it below the stores.
- `for (...) { if (hit) { call; return; } }` duplicates the loop entry test; with `break` the loop is
  rotated with a single bottom test. `if (a && b) {reset} else {load}` vs the swapped form controls
  branch layout.
- Unused `static const` scalars are emitted (`.sdata2`) only when an emitted function takes their address;
  zero-initialised statics referenced only by a never-called inline are still emitted. Static locals are
  emitted at their declaration; file-scope uninitialised statics after all function-local ones.
- `s16 mem += (int)(s16)(float)` keeps `lha/extsh/add/sth`; without the `(int)` cast the front end
  narrows to u16 arithmetic. A local `int num = 5` divisor gives `divw` by register, not the magic multiply.
- Struct-member view of a global pointer (`pGS`, `pEffParentWorldS`, `pLog`) keeps its load after a
  preceding store through `this`; but wrapping the global's *declaration* reorders loads in units that
  already match, so use the view macro only where the target shows it.
- Independent stores at a block end are issued in reverse RTL order (`a = b = c = 0` -> reverse;
  separate statements `x; y; z` -> `z, x, y`). CSE reuses the newest register holding a constant.
- A reload of a just-stored member is forwarded as `mr`; a local gives no copy. `if (c < n) x = c+1;
  else x = n;` yields an `mr` before the compare; `x = n; if (...) x = ...` loads into `x` directly.
- `T* p = alloc(); g.p = p; p->init();` gives `mr r0,r3; stw r0`; assigning the call result directly
  stores r3. Loading a wrapped global once into a local avoids per-store reloads.
- `!(flag & 1)` as an `if` condition gives `xori; andi.` when the same test exists in two cross-jumped
  branches; `(flag & 1) == 0` gives a plain `andi.`.
- `*(u32*)(char_ptr + i*4) = 0` keeps the pointer `lwz` inside a `bdnz` loop (may alias) and prevents
  loop reversal; a typed array store gets hoisted and the loop reversed.
- `va_list`: include/va_ppc.h; `va_start` is a struct copy in C++ (g++ 2.95 does not inline
  `__builtin_memcpy`).
- `.sdata` alignment padding a split object contains is reproduced with
  `asm(".section .sdata; .balign 8")`.
- `subfic r0,rX,0; adde r3,r0,rX` is `return x == 0`; `return x != 0` compiles to a branch.
- A `cmpw rCONST,rX` with the constant hoisted into a callee-saved register is a local like
  `int dead = 10;` compared as `dead < x`.
- A `do { ... } while (0)` macro body is a scheduling-region boundary: its stores do not mix with the
  preceding block's stores.
- Hardware registers are struct members at a base (`OS_BUS_CLOCK`: `lis 0x8000; lwz 0xF8(r)`), never
  `*(u32*)0x800000F8`; the GX FIFO is the linker symbol `GXWGFifo` (see gx.h).
- A void-looking function whose last call's r3 is untouched may return that value.
- libm/libc functions (`tanf`, `sqrtf`, `memcpy`...) must be declared `extern "C"` (fdlibm.h / the
  newlib headers). A C++-linkage declaration makes the unit reference `tanf__Ff`; sync_symbols now
  refuses to rename the C symbol and tells you.
- A float variable assigned twice in one block never gets its chain tied by local-alloc (intermediates
  in f0); one variable per chain gives the tied form. A variable used in two loops is globally allocated;
  declare it inside each loop body for separate pseudos.
- Unused `static const` arrays inside a function are still emitted before that function's pool;
  file-scope unused statics are dropped; <=8-byte objects go to `.sdata2`. `int x = 0;` at file scope
  lands in `.sdata`, not `.sbss`.
- cse canonical register: for `(set v x)`, `v` replaces `x` in the extended block only if `v`'s last
  mention is later than `x`'s; otherwise `x` stays canonical and both live (`fmr`). A dead trailing
  reload/copy (`p = pn;` after a loop) changes which one is canonical and thus the copy shapes.
- `while (v < bound) v += step;` recomputes `bound` per iteration; `do/while` or `for` hoists it.
- A `memcpy` whose destination is byte-pointer arithmetic (`(u8*)w + ofs`) keeps a following `.sdata`
  load below the stores; `(u8*)&w->v` casts are stripped by the builtin and behave like a struct copy.
- Identical local aggregate initializers in different functions are merged into one `.rodata`
  template; if the original kept one per function, give each a distinct type.
- Sibling blocks reuse freed stack slots first-fit; a nested block inside a live variable's block does
  not. Reassigning a pointer local after calls forces it into a callee-saved register.
- OPEN (id_sys `setCk`/`dispSw`/`kill`): the target zero-extends one `u8` parameter (`clrlwi rX,rParam,24`)
  before using it as a bit-table index while other u8 params are never masked; no source form found yet
  (u8/int/u32 locals, casts, `& 0xFF`, inline helpers, references, bitfields all tried).
- `if ((p = f()) != 0) {A} if (!p) {HALT}` keeps the compare in cr4 (`mcrf cr4,cr0`); two plain `if`s
  let cse fold the second.
- SN patched out jump tables: 4+ dense cases still give a compare tree; a `case` whose body is only
  `break` still counts as a tree node; writing `default:` first lays the default body out first.
- A local `arc = pG->pPlArc` reused across blocks becomes a global pseudo and ties pG's register; the
  original often re-reads `pG->field` per call. A function-scope `void* p = 0` in a callee-saved register
  gets reused by cse as the constant 0 argument of later calls.
- Store-order rule (sched1): in a block of independent stores, stores whose source register dies
  (last use) are issued first in RTL order, then the non-dying ones in RTL order. A constant shared by
  several stores has one pseudo, so only its last store is a death; a value reused after a branch
  sinks to the end of the block. Derive the source order from the target with this rule instead of
  permuting blindly.
- haifa tie-break: after a non-void call, the next call's arg `li`s outrank the `mr r3,this` copy;
  declaring the callee `void` when the original ignores its result changes the order.
- objdiff scores 100% even when constant-pool *values* differ (relocs compared symbolically): always
  cmp .rodata bytes against the split object before flipping a flag.
- In-class inline members of the class whose vtable the unit owns are emitted after the destructor at
  the end of `.text`.
- Interblock scheduling is on: an independent `i++` in a loop's join block is hoisted into the loop
  header unless `i` is used inside the diamond.
- A store through a plain pointer variable (`*d = v`, no `+` in the address) is assumed to alias
  `static` scalars and forces their reload; `p[i] = v` / `a->p[i].x = v` never aliases a fixed scalar.
- A block ending in a call followed by a label gets a nop that blocks cross-jumping into its tail; a
  dead trailing statement suppresses it and the tails merge one insn deeper.
- Narrow zero stores reuse the nearest wider zero pseudo (HI before SI); the SI zero for pointers
  stays separate.
- OPEN (mes `move`/`WidthCk`): `code = f(x); if (code == 0)` — original keeps `mr r4,r3; cmpwi r4,0`
  where ours combines to `mr. r4,r3`; ~20 forms tried.
- `union { GXColor c; u32 w; } kc; kc.w = 0x6600FF32;` gives the `lis/ori/stw` word store for colour
  constants; a `GXColor k = {..}` initializer gives per-byte `stb`s.
- An 8-byte member anywhere in a class (`u64`) raises the object's `.bss` alignment to 8, creating the
  unnamed 4-byte gaps dtk labels as separate symbols.
- gcse PRE of a struct load across an if/else is blocked by any memory kill in the arm: if the target
  lacks a PRE shape ours produces, the original arm stored to memory somewhere.
- Every store to a `GlobalWork` field followed by a `pG` reload = the original stored through
  reference setters (`U8Set/U16Set/U32Set`, `BitOn16`), not plain member stores. Four byte stores after
  a call through one fresh pointer load = an inline with its own `cPlayer* p = pPL` local.
- A `li rZ,0` in both arms of an if/else whose common tail follows = source-level tail duplication
  (the rest of the function repeated in each arm; jump2 cross-jumps the suffix).
- `x == 2 || x == 3` on a u8 returns as `subi 2; subfic 1; li 0; adde` (unsigned `<= 1` range fold).
- Loop-invariant `cmpwi cr2/cr3/cr4` hoisted before a loop = a `switch (type)` inside the loop body.
- Dead strings: code under `if (0)` or after `return` still emits its string literals into `.rodata`.
- Zeroed aggregate initialisers (`u16 h[3] = {0,0,0}`, `Vec a = {0,0,0}`) become `memset` libcalls
  with `crclr cr1eq`.
- SF constants have a tied GPR/FPR class: when a callee-saved GPR is free, the constant may land in the
  GPR and be stored with `lwz/stw`.
- Row-by-row matrix copy: `while (i--) { dp = *d; sp = *s; for (j<4) *dp++ = *sp++; s++; d++; }`
  gives the `cmpwi -1` reversed outer loop. Byte assembly through a `union { f32 f; u8 b[4]; }` gives
  the `rlwimi` chain.
- An inline `if (c) return 1; return 0;` materialises `li 0; bge; li 1; cmpwi`; `return c` gives
  `mfcr/extrwi`.
- Vtable emission is in reverse class-declaration order; a vtable that needs an uninstantiated
  template member instantiates it in place (strings between vtable groups).
- Deferred inlines (`inline` members, in-class bodies, synthesized dtors) are emitted after
  `__static_initialization_and_destruction_0` in *definition* order (saved_inlines, oldest first): header
  in-class bodies in class order, then the unit's own `inline` definitions in file order, then the
  template instantiations finish_file requests. An implicit (synthesized) derived dtor is saved at the
  end of its class, before its in-class virtuals, and only instantiates the base `~cManager<T>` in
  finish_file (so those land last, in vtable-walk order); a user-written `~cMgr() {}` instantiates it at
  the definition point (game/model: `~cModInfoMgr, memAlloc, memFree, memClear, ~cParts, ~cPartsMgr,
  ...` needs the mgr dtors implicit and memAlloc/memFree/memClear in-class in model.h).
- A `static const` table referenced only by dead-stripped static functions is output by
  wrapup_global_declarations at the end of finish_file's first pass: between the last vtable of the first
  vtable group and a vtable needed only by a deferred inline (model: the 0x10 zero words between the
  cCoord and cUnit vtable copies = `static const s32 ShadowPtNum[4]` used by the dead ShadowModelInit /
  AddShadowModel).
- A byte store `x = 0` makes a QImode zero pseudo that later word stores cannot share; `U8Set(x, 0)`
  (u8& setter, promoted parameter) makes it SImode, and cse's skip-blocks path carries it over a
  one-statement `if` so the word stores after the join reuse the same register (modelInit).
- `Derived() : cUnit(1)` (base initializer) instead of `be_flag = 1;` in the body moves the constant's
  pseudo before the vptr pseudo and gives it r0 (cModelInfo::cModelInfo); the store order is unchanged.
- `for (i = 0; lim = n + 1, i < nArray - lim; i++)`: recomputing `n + 1` in the loop test reproduces
  the entry guard + `mr r7, r0` PRE copy; `u32 lim = n + 1` before the loop gives one register, and
  `nArray - (n + 1)` is reassociated by fold to `(nArray - 1) - n` (createSequential).
- Deferred-inline `inline` functions that other units call out of line (`isTrans__6cModel`) must stay in
  the .cpp: an in-class body would be inlined into those units.
- A dead static function's pool and strings stay in `.rodata` (STRIP_UNUSED drops the body); write it
  with the strings the target shows and reference the globals whose `.sdata`/`.data` slots it owns.
- Weak vtable copies: a later unit's reference binds to the first copy program-wide; symbols.txt must
  name the first copy `_vt.<Class>` and the others `<Class>_virtual_table_<addr>`.
- `extern "C"` functions with function-pointer parameters need `extern "C"` on the definition too.
- Declare each C function in exactly one header (its owning unit's); a second declaration with a
  different signature in another header is a compile error the moment both get included.
- Placeholder names in sym_map.tsv for functions of unmatched units carry no linkage information;
  only already-mangled entries prove C++ linkage. Check a caller's `bl` in the asm when unsure.
- Cross-jumping merges a case body with the fall-through code only as far as it matches, then
  retargets the jump to a new label; jumps to that new label are never merged again. Default-equal
  cases must come first in source order.
- `static const Vec` locals declared mid-function are emitted into `.rodata` at that point and passed
  by address; non-static `const Vec` locals are copied to the stack.
- A switch whose body is a dead local store keeps its compare instructions (branches removed after
  flow deleted the store): `cmpwi ...; b L` sequences with unused compares.
- Narrow values (u8/u16/s16) masked at call sites or after entry (`clrlwi rX,rY,24/16`, `extsh`) where
  ours treats them as already extended: a u8/u16 struct member read *directly* in two places, the first
  in a QI/HI-mode context (range fold `x >= 0xF8 && x <= 0xFD`, a `switch`), stays a QI/HI pseudo and
  every later int use gets an explicit mask; a local `u8 v = p->member` is promoted and never masked.
  Likewise `u32 no = rec->x6; if (no > 0x7F)` keeps `cmplwi 0x7f` where a u8 local folds to `andi.`.
  Try this first on the id_sys/emobj/em_set OPEN cases. NOTE (option `setTime`): the original caller
  sign-extends (`extsh`) an argument whose callee is mangled `Us` (u16) — no cast form gives `extsh`
  for an unsigned parameter, so the original compiler extends narrow arguments at call sites in a way
  ours does not (likely a compiler-build difference in argument promotion, like the FPR/GPR arg-move
  order). Workaround: an asm-labelled signed alias (`setTimeS(IdUnit*, s16) asm("setTime__...Us")`).
- `int susp = !(m->flag & bit); if (susp) return;` gives `xori; andi.; bne`; the direct
  `if (!(x & bit))` gives plain `andi./beq`.
- A single `return ret` reached by `goto ok` from several paths keeps `li r31,1` + `mr r3,r31`;
  separate `return ret` statements are constant-propagated to `li r3,1`.
- A global read after a store through an out-pointer is loaded first only if copied into a local
  before the store. `u32 max = f(); if (w->id < max)` loads `id` after the call; `w->id < f()` keeps
  `id` in a callee-saved register across it.
- Zero-initialised function-pointer table `= { NULL }` lands in `.data`; uninitialised in `.bss`.
- Byte stores alias everything (alias.c: QImode store -> every global reloaded). Word/half stores
  through a varying struct pointer never alias a fixed scalar; if the target reloads a global pointer
  after such a store, the original stored through a scalar reference (`PSet(void*&, void*)`).
- A sum written as two statements (`d = a*b + c*d; d += e*f;`) keeps the intermediate in the
  variable's register instead of a temp tied to a dying operand.
- An unused aggregate local still takes its frame slot.
- `cModel` (include/model.h) is 0x320 bytes; `cEm`, `cObj`, `cMap` start their own fields at 0x320
  (sizeof(cEm) 0xDE0, sizeof(cObj) 0x3D8, sizeof(cPlayer) 0xDE0, sizeof(cMap) 0x324 and all 391
  probed field offsets unchanged by the refactor, verified with an offsetof harness before and after;
  `cMotModel` is now an empty cModel subclass, 0x320 instead of 0x2B4). Layout (after cCoord's 0xF4 bytes):

  | offset | field | notes |
  |---|---|---|
  | 0xF4 | `pParts` / `pPartsHead` | cModel* / cParts* views of the parts chain |
  | 0xF8 | `serial` | |
  | 0xFC | `stat` / `xFC..xFF` | word / byte views |
  | 0x100 | `id`, `type`, `nParts`, `x103` | |
  | 0x104 | `speed`, `oldPos`, `wallNrm` | Vec ×3 |
  | 0x128 | `pFloorNrm`, `x12C..x12F`, `pCldShMd`, `shdCol`, `x135..x13B`, `fixParts`, `fixPos`, `x14C..x14F` | aliased by the obj05 `efmStat/efmSpd/efmRotSpd` view |
  | 0x150 | `x150` / `x150w` | f32 / u32 |
  | 0x154 | `alpha`, `x158`, `pInfo`, `pShMdInfo` | |
  | 0x164 | `lightInfo` | cLightInfo, 0x74 bytes |
  | 0x1D8 | `mot` | MotionWork (0xDC bytes); the cEm/cObj names (`pMotion`, `motFlags`, `motState`, `motFlags2`/`x21C`, `satPos`, `seNo`, `seFlags28B`/`motEvent`, `frame`/`motFrame`, `frameMax`/`motSeqMax`, `motSpeedRate`, `x29D`, `p2A4`, `blendMot`/`motBlend`, `motFlip`, `x2B0`) are an anonymous-struct view of it |
  | 0x2B4 | `atari` | cAtariInfo (0x4C); wrapped in an anonymous struct so no member ctor runs, cModel::cModel calls `AtariInfoConstruct` |
  | 0x300 | `x300`, `x304` | |
  | 0x308 | `pFootShadowTbl` | |
  | 0x30C | `litArea` | EmLightArea (0x10) |
  | 0x31C | `pTexChg` | cTexChg* |
  | 0x2B4 | `sub2B4` | ObjSub2B4: the object units' view of 0x2B4..0x320 |

  `MotionSeqKey/MotionData/MotionWork`, `EmLightArea`, `ObjSub2B4` now live in model.h; `cParts`
  (0x1D8, a cCoord with `pNext`, `bindMat`, `addRot`, `motParts`) and the two managers are declared
  before cModel. `cObj::blk` sits at 0x324 (was `sub2B4.blk`). `p2A4` is `void*` (cam_ctrl casts it).
- Callee return type changes arg-setup order through dependence counts: an `int` result adds an
  output dependence on r3 that pulls `li r3,0`/`mr r3` to the end of the arg block. Declare the callee
  with its real return type (check the callee's own asm); `EstSet`, `MotionSetCore` are `void`.
- Store-block weight rule (generalises the store-order rule): each independent store has weight +1,
  minus 1 per source register that dies there; lowest weight first, ties in source order.
- `x >= C && x <= C` on a float yields the `cror un,eq,gt / bns` pair.
- `for (i = 0; i < 8; i++) Wk[i].flag = 0` compiles to a `mtctr 8` loop stepping the pointer down
  from the last element.
- A loop-invariant `li rX, mask` left inside a small loop = several separate `&= ~bit` statements
  merged by combine after loop opt.
- Fresh block-local pointer copies in a later section (instead of reusing function-level pointers)
  shorten live ranges and re-rank callee-saved register assignment.
- The `u8 pad[0x320 - sizeof(cModel)]` locals some units still carry are zero-length arrays now (a GNU
  extension GCC 2.95 accepts); they take no frame space and can be deleted when the unit is touched.
- `*(u32*)((u8*)p + ofs)` (cast then deref) produces a MEM without `MEM_IN_STRUCT_P`, so it aliases
  scalar globals and forces `pG/pSys/pRK` reloads; `p[i]` / `*(p + i)` does not.
- `if (A || B) return 0;` places the `li r3,0` block after the second test; separate `if`s after the
  first. `!(d >= 0.0f)` and `while (!(x <= 0.0f))` produce the `cror` form; `<`/`>` plain `bge/ble`.
- ngcld does not honour a following unit's 32-byte `.bss` alignment: when the original unit's `.bss` is
  larger than its variables, a zero-initialised static array referenced only by a never-called inline
  reproduces the gap.
- A `goto RESTART` outer loop vs `for(;;)` changes where gcse hoists loop-invariant `lis` (main loop).
- Hand-rolled `goto` loops (label + `if (...) goto loop`) get no loop notes: no invariant hoisting
  and no givs, with an explicit `ofs += N` variable. A `found:` label inside the last loop's if-body
  puts the shared exit block inside that loop.
- A unit-owned global pointer the original reloads after every store through it is reproduced by
  defining the global itself as a one-member struct (`TexRenderMngPtr g_pMgr; g_pMgr.p`).
- Vec by-value parameters are passed by reference under the V4 ABI: callee code identical to `Vec*`.
- A local `lim = 512.0f` shared by an `if` test and a `while` bound keeps one constant register; a
  repeated literal inside the loop is hoisted as a second pseudo and copied (`fmr`).
- Manager work-scan loops whose range check survives only at the loop top come from a guarded
  do-while (`i = 0; if (i < n) do { p = getWork(i); ... } while (++i < n);`) with the inline `getWork`
  reading its fields through a local copy `cMgr* m = this;` (defeats thread_jumps). See map_obj.h.
- In-class inlines of a vtable-owning class are emitted in declaration order, but an explicit
  `virtual ~Mgr() {}` moves the base template's destructor relative to them; the implicit destructor
  gives the DOL order. Manager accessors like `getWork` should be free `static inline XxxMgrWork()`.
- `static int x = 0;` goes to `.sdata` with an explicit zero; unreferenced statics are still emitted.
  `_GLOBAL_.I.<key>` is keyed to the first *initialized* public object or function; `.bss` globals
  don't count.
- Switch tree rule (stmt.c): after merging consecutive same-target cases into ranges, with n nodes
  and r ranges the root is the node where cumulative cost (1 per node, 2 per range) reaches
  (n+r+1)/2; exactly 3 nodes -> middle. Default-equal `case X: break;` labels shape the tree, so
  enumerate every state the original enumerated.
- `if (a != 0 && a >= b) return 1; return 0;` keeps `cmplw; li r3,1; bgelr; li r3,0`;
  `return a != 0 && a >= b` gives the `subfc/adde` store flag. `ret = f(); if (ret != 0) {...}
  return ret;` yields `mr. r3,r3`.
- Inline accessors used as call arguments make precompute_arguments evaluate them before the
  stack-argument stores; plain member reads reload the pointer after each store.
- VLA (`T* tbl[n]`) gives `stwux` plus `mr r25,r1` / `mr r1,r25`; `alloca` has no restore.
- Empty `C() {}` / `~C() {}` produce the empty `__static_initialization_and_destruction_0` and both
  `global constructors/destructors keyed to` functions.
- Case bodies identical to `default` written as separate `case N:` arms survive as explicit
  `cmpwi N; beq default`; cases grouped with `default:` vanish but still count in the tree balance.
- A byte field passed straight to a call whose prototype takes `u8` yields `lbz; clrlwi; mr` copies;
  with an `int` prototype the load folds into the argument register.
- A run of literal zero stores plus one `= zeroVar` store: the `zeroVar` store is emitted first, the
  literals follow in source order.
- A `static const Vec` shared by two functions with one `.rodata` copy lives in an inline helper
  parsed before both.
- Register-argument addresses (`&local`) are precomputed into pseudos and cse merges later `&local`
  uses in the same extended block; only the frame-offset-0 local is set straight into the hard reg and
  recomputed. SOLVED (sscrn/mercenaries FadeSet colour temps, `&pos` for setAng): see "FadeSet colour
  pair" below — the fresh `addi rX,r1,ofs` per call comes from a local of an INLINED helper, whose
  frame address integrate.c substitutes straight into the hard-register argument sets.
- KNOWN DEBT: `cUnit::beginEvent/endEvent` take an `int` in the original (sce_com loops, sscrn); the
  shared declaration in cManager.h is still `()`. Fixing it touches every em*/obj* unit.
- A `&local` passed directly to a call is copied to a pseudo and PRE hoists it into a callee-saved
  register (`mr rX,r4` ... `mr r4,rX`); the same call inside an inlined `static inline` helper taking
  `Vec*` gets the address as a hard-register arg set that gcse never sees, so it is recomputed
  `addi r4,r1,off` at each call. This is the lever for the OPEN `&local` reuse cases.
- `alpha * rate * helper(...)`: the (inlined) call is evaluated first, so `lfs alpha` lands after the
  `bl`; `helper(..., alpha * rate)` precomputes the product before the call.
- sched1 `adjust_priority`: an insn whose dependences resolve is boosted only if its dest register is
  live at block end and set once ("birthing"). A LATER CALL in the same basic block marks the FP arg
  registers live (call-clobber rule), so the `fmr` arg copies of a preceding `init(...)` call get
  boosted above its `li`s. That is why obj20's `cAtariInfo::init` (followed by `setPriority()` in the
  same block) matches while SetTrolley/SetGondola/SetYagura/SetHeliMissile/setScrAtari (no later call
  in the block) don't: the seven `Set*` targets have NO later call in the block (verified), so the boost comes from
  something else there — still OPEN. Note `birthing_insn_p` uses `bb_live_regs` left over from the
  weight scan, which only matters for single-block scheduling regions.
- `(f32)(int) w->u8field` gives the signed double trick instead of `psq_l qr2`; a `u32 x:8` bitfield
  gives the unsigned trick.
- Struct copy from a global pointer with a reload between (`obj->pos = pPL->pos; obj->rot = pPL->rot`
  -> `lwz pPL` twice) = `memcpy((u8*)obj + offsetof(pos), &pPL->pos, sizeof(Vec))`.
- `dx*dx + dy*dy + dz*dz`: the first product is fused into the second (`fmuls dy; fmadds dx`); the
  standalone `fmuls` is the second term.
- A local `class cEmRoom : public cEm` with N declared, undefined virtuals lets a unit call a
  room-module enemy virtual slot without emitting a vtable (key function undefined).
- Call arguments written as locals (`int parts = 0; f32 zero = 0.0f; init(parts, 2, parts, zero, ..)`)
  create pseudos cse keeps across later calls: the float lands in a callee-saved register reused for
  later `= 0.0f` stores, the int becomes the oldest zero pseudo every later `= 0` store reuses. Plain
  constants give hard-register chains (`lfs f1; fmr f2,f1`) and fresh loads later.
- sched1 flushes the pending memory list after 32 entries: in a block of >33 independent memory insns
  the 34th becomes a barrier; within each half stores are grouped by source register in RTL order.
- A float variable assigned in two places is never tied to a call's return register (`fmr f12,f1`
  right after the `bl`).
- Loop pointers must be block-scoped (`T* n = &node[i];` inside each body) to be replaceable givs; a
  function-scope pointer reused across loops leaves an `mr rN,rGIV` copy.
- Spilled `&local` pseudos get their stack slots in gcse hash order (table size = max_uid/4 | 1), so
  slot rotation between ours and the target means the original has more RTL somewhere in the function.
- A local at frame offset 0 is materialised per call; a block-scoped `Vec* pp = &p0;` declared after
  the first use gives "first use direct, later uses from a callee-saved pseudo".
- `p = Vec()` for a POD in g++ 2.95 creates a zeroed temporary plus a block copy, not a `memset`.
- `#line` must precede the *first* `__FILE__` use in the .cpp (dead functions included), otherwise a
  second, shorter file-name string appears in `.rodata`.
- PRE copy signature: `lfs f0; fadds ..,f0; fmr f11,f0` = a member load reused in later blocks by
  gcse (no local); a member cached in a local is used straight with no `fmr`.
- Index-first `lhzx/lfsx/add rD,idx,base` = `(T*)(i * sizeof(T) + (u32)base)` written index first.
- An early `return 0.0f` merged with the final return inserts a label that invalidates reload_cse:
  the following call re-copies a still-valid argument (`mr r4,r31`).
- `fabsf` as volatile asm is a scheduling barrier; where the target loads an `.sdata` constant before
  the `fabs`, use `__builtin_fabsf`.
- Function-scope temporaries reused across several tests become global pseudos that inherit hard-reg
  preferences; inlining the expressions per test gives per-block pseudos.
- Unused `.sdata` globals with no Bio4.sym name must keep the `lbl_XXXXXXXX` symbol name or
  strip_unused removes them.
- Reading a static through a reference (`static inline f32 FRef(f32& v) { return v; }`) gives a MEM
  with neither the struct nor the scalar flag: the load stays below preceding member stores and
  blocks flow.c's dead-store elimination of an earlier store to the same member.
- SN's `BRANCH_COST` is 0, so `&&` is never folded to `&`: a `subfic/adde ... and.` store-flag pair is
  an explicit `&` in the source.
- Byte stores of the literal `0xFF` share one `li rX,0xff`; a `u8`/`int` local `c = 0xFF` yields `li -1`.
- A constant shared by two functions but emitted between them is a public `const Vec x = {..}`
  (declared `extern const` first, defined at that point); `static const` at file scope is deferred.
- `if (x <= 0.0f)` gives `cror un,eq,lt; bso`; `if (!(x > 0.0f))` gives a plain `ble`.
- An address-taken local the original reloads after every store through it is a one-member struct
  local (`struct { cEsp* p; } e; PullEsp(&e.p, id)`).
- `static f32 v = 1.0f / (f32) n;` (function-local, runtime initialiser) is the `_.tmp_0` guard word.
- Brute-force harness note: ninja does not notice sub-second source rewrites; delete the .o before
  each rebuild or the scores are stale.
- Byte stores through `this` are output-dependent on word stores through a work pointer (different
  base pseudos), so they always follow them in a state-store block.
- `return t == 0` with `u32 t = x & MASK` gives `andis.; mfcr; extrwi`; `return (x & MASK) != 0` gives
  the bit-extract; `return t != 0` falls to the `li 1/bnelr/li 0` jump form.
- Search loop `for (s = tbl;; s++) { if (s->id == END) return 0; if (id == s->id) break; } return s;`
  gives the rotation with both tests at the bottom; a `return` inside the loop is not a jump to
  end_label. `continue` keeps the loop label used, so `addi; lhz` at the bottom are not combined
  into `lhzu`.
- Per-loop block-scoped `for (int i ...)` counters change pseudo numbers and hence gcse's hash order
  for PRE-hoisted increments and scratch numbering.
- A small object referenced with the full `lis/addi` form although it sits in `.sdata`: an
  incomplete `extern T x[];` declaration before the definition.
- Constant-pool order via `const f32 x = C;` locals at the function top: the initialiser is expanded
  at the declaration (constant enters the pool first) while every use is folded to the literal.
- Identical case bodies collapse into one only when the body that falls through into the join is one
  of them: `default:` must share the `case 0:` label, not be a separate trailing body.
- Loop invariants assigned inside the body (`range = to;` in the `for`) survive as an `fmr` copy /
  shared double-trick registers hoisted by loop.c; the bound `j < i` with `i = 15` a variable gives
  `cmpwi 0xf; blt`, a literal 15 gives `cmpwi 0xe; ble`.
- The original stores to unit globals/members through scalar references far more often than
  expected: `U16Set`, `VSet(ptr, MEM_ALLOC(..))`, `MSet`, `FSet(m->dir.y, -1.0f)` and reference
  *reads* (`BitChk(pG->flags, bit)`) are what keep following `.sdata`/`pG` loads below the store.
- Inline accessors taking `&m->member`: every such argument is a fresh `(plus m ofs)` that gcse PRE
  turns into an `mr rX,rMember` copy.
- `found = 1` written after a void call is scheduled above the `bl` into the callee-saved register.
- A `goto LABEL` loop keeps the un-rotated body/test/`b` shape; `for(;;)`+`break` gets rotated.
- A value-context `a || b` inside an inline that returns it expands as `x = 0; if (!exp) goto L;
  x = 1;` (`li 0` first); `if (a || b) return 1; return 0;` gives `beq L0; li 1; b; L0: li 0`.
- A `switch` whose cases reassign the switched variable keeps the pre-switch copy (`mr r6,r30`) only
  if the variable is `int`; a promoted `s8` moves the copy to the variable's initialisation.
- Local `u8` array initialisers: 2 bytes -> `sth` immediate; 4 -> `stw 0` + `stb`s; 5+ -> `.rodata`
  template copy.
- Store-block order, refined: dying-source stores first as [last member of D in RTL order] + [rest of
  D in RTL order], then non-dying stores in RTL order; a byte store in the block is a barrier that
  splits it into two such groups.
- A constant kept in a callee-saved register across calls and stored later is a function-scope local
  (`int zero = 0;`, `int type = 2;`) stored through the variable.
- Float box tests with plain `blt/bgt` and a duplicated `blt` to the same label = separate
  `if (v.x < a) continue;` statements, the duplicated test copied verbatim.
- `.rodata` proves the include set: a unit whose `.rodata` lacks `"D:/Bio4/Prog/light.h"` did not
  include light.h; header strings appear in include order.
- Index register class: `add rD,rBase,rOfs` / `lwzx` with the offset in r9/r11 (BASE_REGS) instead of
  r0 means the base pseudo is not pointer-flagged — a `u32 addr` *parameter*; a `u32 addr = (u32)ptr`
  local does not work.
- `u32 trg = Key.trg & MASK` (u64 truncated) tests as 32-bit `andis.`; a `u64 key` exclusive check
  gives the `li 0; mr; rlwinm; or.` word-pair test.
- `mr rLong,rTmp; stb rTmp` (value stored and copied into a long-lived variable) = a reused block
  temp (`u8 c; c = sr[ptn]; mat.r = c; r = c; c = sg[ptn]; ...`); multi-set `c` blocks coalescing.
- An uninitialised `GXColor amb;` passed by value emits `stw rCalleeSaved, slot` (garbage register).
- Argument-move order workaround is per call site: a call may need the floats-first asm-labelled
  redeclaration while another call of the same function in the unit matches with the real one.
- flow.c appends `(use (const_int 0))` after any CALL_INSN that ends a block; jump2's cross-jump then
  fails against the fallthrough, so one of N identical call tails stays unmerged unless the arm does
  not end in the call (repeat a trailing store in every arm to let the tails merge deeper).
- A pointer local assigned in two blocks is not local-allocated: a struct copy through it keeps the
  `addi` untied from r3 and following loads are scheduled after the copy.
- `int atk; w->atkHit = 0; atk = 0;` (assignment after the byte store) keeps the QI and SI zeros in
  separate registers and makes cse pick `atk` as the 0 stack argument of later calls.
- `if (cond ? f() : g())` duplicates `cmpwi r3,0` into both arms; `hit = cond ? f() : g(); if (hit)`
  gives the single shared compare after the join.
- `0xFFu` (unsigned literal) in a ternary makes the following compare `cmplwi` rather than `cmpwi`.
- Odd float constants: `0.05f` in the target is `0x3D4CCCCC`, obtainable only as `0.01f * 5.0f`
  (constant-folded) — when a pool word is off by one ulp, look for a folded product.
- `HALT()` (and any OSReport-style error macro) is a plain `{ }` block in the original, not
  `do{}while(0)`: the loop notes of a do-while are a full sched1 barrier and change argument/`lis`
  ordering around it (read, main_sub, pl_leon, datactrl needed the plain form; nowhere did do-while
  help). Use the plain block everywhere.
- A function with an `if (...) return 1;` and no return at the end keeps r3 = the incoming
  parameter (no `li r3,1` in the tail).
- `pSys->field` inside loops that store through a pointer parameter is hoisted (fixed scalar never
  aliases a varying struct store); reading it through a reference (`SysRef(pSys)->x`) reloads it per
  iteration like the target.
- objdiff REPLACE rows on `lwz/stw off(rN)` with identical bytes = dtk-synthesized `Sym+off` relocs in
  the split object; judge with a raw byte compare (relocated words masked).
- Sprite texture-corner selection (esp_sub/esp08/esp18/esp0f/esp16 Trans): one condition
  `(screen && !f4) || (!screen && f4)`, corners built from a `zero` variable with the flip-s leaves
  adding first (`s0 = zero + z; s1 = zero;`): each leaf is a jump target with no cse knowledge of
  `zero`/`z`, which keeps the `fadds` (nested ifs with literals fold `0 + z`). Fixes the family-wide
  `fadds` vs `fmr` diff.
- Two identical calls in if/else arms are not cross-jumped when the shared argument is a local read
  before the `if`.
- Switch nodes sharing the default target: a leaf whose every exit goes to default vanishes (grouped
  with `default:`), but a default-target node with a right sibling in its chain keeps `cmpwi N; beq
  default`; both count for the tree balance.
- `u8 stat = 1;` gives a QImode pseudo folded into the `stb` (late `li r0,1`); `int one = 1` shared
  between a u16 call argument and a u8 store keeps an SImode pseudo hoisted into a callee-saved reg.
- Read constant pools from the split `.o` bytes, not by hand-parsing the `.s`; objdiff scores pool
  values symbolically and hides wrong constants.
- `INDIRECT_REF(PLUS)` marks a MEM in-struct; the same address through a `(u32)` cast does not:
  `*(u16*)((i << 4) + u32helper(ofs))` gives `sthx base,idx` and a `pG` reload after every store.
- A redundant second assignment (`parent = w->pParent;` again) makes REG_N_SETS=2 and stops regmove
  from merging `lwz r0; mr r29,r0; cmpwi r0`.
- Float `ble/bge` without `cror` = reversed `>`/`<`: write `if (!(a > b))`; `cror un,eq,lt; bso` is
  the real `<=`.
- Work-struct init blocks: int/f32 fields written through reference setters (`ISet/FSet`) while u8
  fields are plain stores gives the target's store schedule where plain int stores never do.
- `insert_bct` refuses known loop counts < 3: a 2-iteration loop only becomes `mtctr/bdnz` if the
  count is not visible to loop.c.
- Routine bytes: the `ff, fd, fc, fe` store order of an `xFC/xFD/xFE/xFF` state change comes from an
  inline `PlRoutineSet(pl, int, int, int, int)` storing `fc, fd, fe, ff`; direct byte stores give
  the dying-first order.
- A static local aggregate with a `_.tmp_0` guard and per-member `stfs 0.0` is a class with a
  constructor (`struct P { f32 x,y,z; P() {..} }`), not a POD initializer.
- `register int r4v asm("r4"); int mode = r4v;` at entry reproduces reading an argument a
  parameterless mangled name does not declare (`beginEvent()` reading r4).
- `void f(...) asm("f__6cBase...");` in a derived class re-exposes a hidden base overload without a
  body; `extern T* alias asm("sym");` gives a second name/type for a conflicting global.
- Interblock scheduling threshold: haifa's `find_rgns` makes a loop one region only if its LUID span
  is <= 100 (notes and deleted insns count). A loop body over that limit shows no speculative
  hoisting (`mcrf cr7,cr0`, arg `li`s before the branch).
- No `clrlwi` for `0x40 + i` passed to a `u8` parameter is only obtainable with an int-parameter
  asm-labelled alias of the callee (`unitPtrI`, `setI` in id_sys.h).
- Stores to a plain `static void*` are freely reordered against `u->member` loads; declaring them as
  one-element arrays (`g_p[1]`) keeps the target's load/store interleave.
- `psq_l f,0(rP),1,qrN` straight from a stepping pointer is inline asm; the compiler always goes
  through a stack slot. Whole skinning kernels (`CalcSk1_x`, `setupGQR6`) are single `asm volatile`
  bodies (NgcAs rejects `subis`; use `addis 0xE000`).
- Locked-cache palette: `PSMTXReorder(m, (f32(*)[3])(0xE0000000 + i*0x30))` gives `mulli; subis`.
- `cond ? A : B` as a call argument folds a common `(x+0x1F)&~0x1F` out of both arms; two if/else
  calls keep the arm-specific masks and cross-jump only the `bl`.
- loop.c `move_movables` needs `threshold * savings * lifetime >= insn_count`: in very large loops
  (>~300 insns) a lo_sum with savings 2 is not hoisted by ours while the original hoists it — the
  original's loop was smaller or its invariant had more uses; count the loop insns before guessing.
- `&local` recomputed per call requires the local to be the first declared frame object (offset 0).
- `lwz r0,X; mr r3,r0; cmpwi r0` on a struct member = the member read directly in several blocks
  (gcse reaching copy); a local gives `lwz r3` directly.
- `ang = w->rotY; ang -= K;` (two statements) loads straight into the variable's register;
  `ang = w->rotY - K` gives a temp-first order.
- A switch whose arms each store to `pG->field` gets the `pG` reload PRE'd into a copy (`mr r11,r9`)
  and the arms never cross-jump; a local temp with one store after the switch gives the merged form.
- A shared scalar temporary across a run of `p = load; store(f(p))` statements reproduces the
  one-scratch-register interleaved schedule; distinct expressions get all loads hoisted.
- Locals declared after a call (`int x = 0;` following `OSReport(..)`) keep their `li` after the `bl`.
- Extern declaration order decides `.sbss`/`.bss` placement of *referenced* externs (first-declaration
  rule): `extern GameSaveData* pSaveData;` must precede `extern cGameSave GameSave;` in game.h.
- A loop pointer initialised by an `mr` copy of the hoisted `&Array` pseudo comes from a dead
  initializer `T* t = g_Arr;` at function scope with `t = &g_Arr[i]` in the body (the extra mention
  keeps the base pseudo live past the giv init).
- A zeroing loop over a word array steps up with `mtctr` only with a `u32` counter; `int` reverses it.
- `>= C` / `< C` compares survive only when the constant is not visible to fold: a plain local
  (`lim = 30; if (x < lim)`) keeps `cmplwi 0x1e; bge`; a literal is canonicalised by combine.
- `if (c) x = a; else { x = b; ... }` hoists `x = a` above the test only when the else arm *starts*
  with `x = b`; an else arm starting with a call keeps it in place.
- A function ending with `return 0;` makes every early `return 0` jump to that final `li r3,0`.
- C frame slots (cc1): BLKmode locals get their slot at declaration, rounded to 8; address-taken
  sub-word scalars are put in the stack at the first `&` in parse order; word-or-larger ones go
  through ADDRESSOF and get slots at purge time, so they land after every sub-word slot.
- `x == 0 && y == 0` on adjacent `short` struct members folds into a single `lwz; cmpwi 0`.
- `int one = 1;` at function scope with a single use in another block: `update_equiv_regs` moves the
  `li` next to the store, making a short qty that takes r0 ahead of a `lwz/rlwinm/stw` chain.
- `if (a && b) {..} else if (c && d) {..}`: the else-if test block has two predecessors, so cse cannot
  reuse cr0 and gcse PREs the shared member load (`lfs f0; fmr f12,f0`); nested ifs share cr0.
- Identical switch bodies written separately are cross-jumped into the *last* copy in source order.
- gcse cprop of a `p = A` copy (A = an inline's parameter pseudo) is blocked by *any* other set of `p`
  on a path; an in-loop `p = &x->m;` re-assignment is such a set, gets hoisted by loop.c and deleted by
  cse2 as a no-op — reproduces a lone `mr rX,rA` copy. gcse runs one pass (MAX_PASSES 1).
- After sched1, REG_LIVE_LENGTH counts only real insns: block notes/braces cannot shift global-alloc
  priorities; only real insns in the range do.
- jump2 cross-jump deletes the tail of the jump scanned *first*: to keep an if/else then-block in
  place and have identical case bodies jump into it, write the case body arm before the if/else arm.
- Early-return tests that branch to a *later test* (not the function end) mean the original nested
  the ifs, not `if (!a) return;` chains.
- Static locals show as `name.NNN` in objdiff; the DECL_UID suffix cannot be reproduced and is ignored by the report.
- Unexplained words in `.rodata` (zero words, stray floats) are usually the constant pool of a function
  the original linker dead-stripped (bodies gone, pools kept, `STRIP_UNUSED` in objects.py): write a
  never-called `static` function whose pool is exactly those constants at the right position (pools are
  emitted per function, right before its body; `x != 0.0` gives a DF zero, `x != 0.0f` an SF zero) and add
  the unit to `STRIP_UNUSED`. Aggregate templates/strings of `static inline` bodies are emitted at parse
  time, so a dead function parsed before a live one can own strings the live one also uses
  (`src/game/geometry.cpp`, `shape.cpp`).
- The compiler never emits `psq_l f,0(rX),1,qrN` straight from a pointer; that is inline asm in the original
  (`asm volatile("psq_l %0,0(%1),1,5" : "=f"(f) : "b"(p))`, store side `psq_st ... : "memory"`), with the
  pointer post-incremented in the operand (`PSQ_L_S16(src++)`) and a `s16 tmp[1]` function-scope array as
  the store target (`shape.cpp CalculateShape_new`). A compiler `(f32)` of a u16/s16 memory operand always
  goes `lhz; sth tmp; psq_l tmp`.
- `add rD, rA, rB` operand order: pointer arithmetic puts the pointer first (`ptr + i*8` → `add tbl, idx`),
  integer arithmetic keeps the written order (`i*8 + (u32) tbl` → `add idx, tbl`).
- `(u8*)d + (n * 2 + 3)` folds the constant into the multiply result first; `p = a + b; p = (p + 3) & ~3`
  keeps the sum in its own register (`add r10; addi r9, r10, 3; clrrwi r10, r9`).
- Parameter order of a function is only visible through prologue copy order (`mr`/`fmr` interleaving):
  `(..., f32 rate, u8* dst)` copies `f31` before `r29`.
- Cross-jumped return tails: `if (x) { if (cond) return 0; ... return fd; } return 0;` shares the outer
  `li r3,0` with the inner early return (`file_open`); `if (call()) return -1; else ret = 0;` inside the
  outer `if` keeps two `li r3,-1` (`file_close`).
- Screen filters (filter01/03/04/07/09/0b): `(u32) Screen.width` gives the `fcmpu 2^31/bso/xoris` unsigned
  conversion; `SCR_W >> 1` then `divwu` is `(SCR_W >> 1) / div`. The `GXSetCopyFilter..GXInvalidateTexAll`
  copy block keeps `&Rmode` in a callee-saved register only with a `GXRenderModeObj* rm = &Rmode` local.
  Static `u8 vfilter[7]` tables are `__attribute__((aligned(32)))` (the 0x1C `.sdata` holes) and each unit
  ends with `asm(".section .sdata; .balign 32")`.
- A variable reassigned through itself (`zv = f(zv)`) keeps its register; a fresh expression in the call
  argument combines with the dying operand (`fmadds f12,f12,...` vs `fmadds f13,...`). `a = x - 1.0f;
  a *= 25.0f;` ties `lfs/fsubs/fmuls` to one register; `a = (x - 1.0f) * 25.0f` gives three.
- `zero = 0.0f; call(); y = zero;` loads the constant before the call (the scheduler moves a pool load
  above a call); with the assignment after the call it stays after. A dead `f32 x = 0.0f` initialiser at
  the top only decides the constant pool order (the load itself is deleted).
- Float-in-u8 round trips: `(f32) a` of a `u8 a = 0x60` local in the same block folds to `96.0f`; the
  original keeps `stb/psq_l qr2` because the assignment is in another block (`a = 0x60` at the loop top,
  use after a `switch`) or the value is an `int` cast down: `(f32)(u8) a`.
- `u32 t = Joy[0].on & 0x400; f(t == 0)` gives `andi.; mfcr; extrwi ..,1,2` (store-flag); the inline
  `(x & 0x400) == 0` gives `xori/extrwi`. `GXColor amb = {0,0,0,0xFF}` declared right before its use
  (C++ mid-block declaration) keeps the `stw 0 / stb` next to the call; at the top it is hoisted.
  `u8 c = 0xFF; amb.r = amb.g = amb.b = amb.a = c;` gives the `li -1` QImode chain.
- A conditional `x12F = a < 250.0f ? 1 : 0` compiles to `mfcr`; the original `if/else` with constant
  stores cross-jumps to `li 0; bge; li 1; stb`. `f32 ratio = a / b; w->a = (f32) c * ratio;` evaluates
  the division before the u8 load; the inline product converts `c` first.
- Stores through `FSet` (scalar references) keep a following `.sdata` load (`lfs f1, static@sda21`) after
  them; plain member stores let ProDG hoist the load. Their emission order still follows the reverse-order
  rule, so permute the statements (`speed.x, speed.y, speed.z, pos.y` in source gives `x, y, pos.y, z`).
- Parameter order only shows in register allocation of the copies; for the filter `GXDraw` helpers the
  order `(f32 x, y, z, u, v, u8 r, g, b, a, f32 scale, int div, int fmt, ...)` reproduces the target.
- Struct field offsets come from the load/store displacements; write real structs, not casts.
- `rlwinm rX,rX,0,MB,ME` with wraparound = `x &= ~bit`; `ori` = `|= bit`.
- Branch shape follows the source: `if/else if` chains vs `switch` produce different compare orders
  (see `AreaCheckOnOff` in `src/game/cam_ctrl.cpp`: the original is a `switch`).
- Return `u8`/`s8` fields: `lbz` alone = unsigned, `lbz`+`extsb` = signed.
- Do not add casts to raw offsets to force codegen. Do not rename globally visible identifiers away
  from the names in `sym_map.tsv`.
- Data (`.rodata`/`.data`/`.bss`) in your unit must also match: define the globals the unit owns
  (see `[.data]`/`[.bss]` in `unit_info.py` output) with the right sizes and initial values.
- A `lwz rX,0(rY); stw rX,0(rY)` no-op pair on a flag word is `volatile u32 flag` with an inlined
  setter whose constant argument folds to `flag |= 0` (dvd `cDvdQueue::setStatus(0)`); a volatile flag
  also explains every reload of the word right after a store to it (`flag &= ~a; flag |= b` → two
  RMW pairs). The `li r9,1; andi.; bne; li r9,0; cmpwi r9,0` chains are an inline
  `int chk(u32 b) { return (flag & b) ? 1 : 0; }`; `if (flag & b) return 1; return 0;` gives the
  reversed `li 0; andi.; beq; li 1` chain (dvd, sofdec.h `isPlay`).
- A store of a register that "happens" to hold a loop counter or a compared value is CSE reusing a
  register known to be a constant on that path: after `if (depth == 0) {...}` the else-branch stores of
  `= 0` use the `depth` register, and `pFilehead[depth]` becomes `lwz 0(rBase)` (index folded to 0)
  (dvd readInit). Likewise `step = 0` right after `switch (step)` in `case 0:` stores the switch register.
- `switch` on a `u32` field gives `cmplwi` in the compare tree (dvd `DvdHeader::type`, `RomFontMessage`'s
  `u32 msg`), `int` gives `cmpwi`; a tree whose root is the lowest case with `ble default` comes from an
  extra empty `case` below it (`case ST_READ: break;` in `readCheckMain`).
- `if (a == 2 || a == 3 || ... )` on the same lvalue is range-folded (`subi; cmplwi`); five separate
  `if (x == k) return 1;` statements keep the compare chain (dvd `SysIsEurope`).
- A peeled first iteration (`lwz n = p->next; cmpwi; beq; cmpw n, q; bne loop` before the loop) with the
  hit block duplicated is `for (p = list; p->next; p = p->next) { if (p->next == q) { ...; ret = 1;
  break; } }` — `return 1` inside the body gives the rotated single-test loop instead (dvd DmaCancel).
- An `if (c) { ret = X; } else if (...)`  chain whose `ret = X` blocks sit *after* the main body means the
  source tested the inverse and put the big block first: `if (req >= 0) { ... } else ret = req;`.
- `cDvd* d = &Dvd; ... d->pList` and plain `Dvd.pList` are not equivalent for GCSE: writing the global
  directly gives the `mr r7,r9` address copy and a reload of `Dvd.pList` inside the loop that a local
  pointer lets the compiler hoist (dvd LinkQueue).
- Two copies of an address (`addi r0,r31,0xa8; mr r3,r0; mr r28,r0`) come from `sprintf(name, ...);
  n = name;` (statement after the call) — `n = name; sprintf(n, ...)` coalesces them.
- `for (i = 0; i < N; i++) a[i] = f(a[i])` over a constant-size array has no entry test and compares the
  pointer to the last element (`cmplw; ble`); `for (p = a; p <= &a[N-1]; p++)` keeps an entry test. A
  do-while over a global array with the base kept in a function-scope pointer (`DvdSndStr* pStr =
  Snd.str; s = pStr; do {...} while (++s <= &pStr[3]);`) hoists the `lis/addi` to the function top.
- `pos = mes_pos[lang][0]; pos += no * 2; f(pos[0], pos[1])` gives `lhzux`; indexing `pos[no*2]`,
  `pos[no*2+1]` gives `lhzx` + `add`. Local array initialisers are copied after the declarations that
  precede them in the source: a `pSys->x` read must be declared before the `u16 tbl[12] = {...}` to be
  loaded first.
- Callers passing an `int` to a `u8` parameter emit `clrlwi r4,r4,24`; if the target has it, the outer
  function's own parameter is `int`, not `u8` (dvd `ReadNblk2Blk(int)`, `ReadCancel(int, int)`).
- `bool ok = f(); if (ok)` materialises `li 1; bne; li 0; cmpwi` from the call result (dvd Watcher).
- `int v = 1; if ((pG->flags & bit) == 0) v = 0;` is how a stored/compared flag test is written when the
  target shows the `li/andis./bne/li` chain for a global; `(pG->flags & bit) ? 1 : 0` becomes `extrwi`.
- File-scope `static const` aggregates are deferred to the end of the unit (they land after the
  cManager template strings); a function-local static is emitted at its declaration, and a constant
  shared by two functions at the front of `.rodata` is a class static member (`const Vec cItemObj::zero`,
  obj19), which is emitted at its definition like any public object.
- Class vtables are `_vt.<len><Class>`; when another unit's split object references the vtable (an
  implicit inline constructor inlined into `cObjMgr::construct`) the symbols.txt entry must carry that
  name or ngcld exits 99 silently. `sync_symbols.py` renames `X_virtual_table` placeholders now, and
  `strip_unused.py` keeps them. A unit in `STRIP_UNUSED` must be synced once *without* the strip (the
  strip deletes every function whose name is not yet in sym_map).
- `switch` tree shapes: `case 0: case 1:` alone gives the linear `cmpwi 0; beq; cmpwi 1; beq; b default`;
  an extra empty `case 2: break;` gives the balanced `cmpwi 1; beq; bgt default; cmpwi 0; bne default`
  (obj26). Case labels that share the default body still shape the tree (`balance_case_nodes` counts
  every node, a range as two) even though jump threading collapses their compares into `b default`:
  obj14's weapon switch needs `case 5..0xA, 0xD, 0xF, 0x10, 0x12..0x18, 0x21, 0x28..0x2A, 0x2C, 0x2D:
  default:` to reproduce the root/branch compares.
- Independent constant stores at a block end come out in an order that is neither source nor reverse
  (`[a,b,c,d]` often as `d,a,b,c`, float and integer stores interleaved by load latency); when several
  fields are initialised, brute-force the statement order with a scripted variant loop (obj14ClothSet,
  SetObj08 took ~10 variants each). Constant registers: an `SImode` zero store followed by a `u8` zero
  store shares one `li 0`; the narrower store first gives two registers. Same for `-1`/`0xFFFF`.
- A struct copy into `pG->member`, `memcpy(&pG->m, ...)`, `(u8*)&pG->m`, `&pG->m.x` or a `Vec&`/`Vec*`
  helper all leave `pG` in its register for the next store; only `memcpy((u8*)pG + offset, ...)`
  (byte-pointer arithmetic destination: not `MEM_IN_STRUCT_P`, alias set 0) makes the next `pG->x = v`
  reload `pG` (obj14 `obj14_R1_Set`, esp1b). A `void*` destination is not inlined at all.
- `stage_no`/`room_no` are also read as one `u16` (`lhz 0x4f9c; cmpwi 4` = room 004): `GlobalWork::room_id`
  union. The four status bytes at cModel+0xFC are also compared as a word (`lwz; clrrwi 16; xoris 0x0100;
  subfic; adde` = `(stat & 0xFFFF0000) == 0x01000000`): `cModel::stat` union (obj14 ckBreak).
- `p ? p->id : 0` as a call argument gives two branches with `b`; `int id = 0; if (p) id = p->id;` gives
  `li 0; cmpwi; beq; lbz` (obj08 SndCall).
- Writing a sub-struct through its own pointer (`c = &w->cloth; c->x = ..`) makes CSE re-base every
  store on `&w->cloth`; `w->cloth.x = ..` keeps the work base with the larger displacements and a
  separate `addi` for the `&w->cloth` argument (obj14ClothSet).
- In a `for (i = 0; i < n; i++)` over an array of pairs, the member used several times (`list[i].part`)
  is strength-reduced into a pointer (`lwz 0(rP); addi rP,8`) while the one used once stays `lwzx`
  indexed off the array base (obj08ToEmHitCk).
- `MotionSetCore` has C++ linkage (`MotionSetCore__FP6cModelPvT1iiii`); `MotionMove`, `EmAtCheck`,
  `SetEmHit`, `YarareInit`, `EmGetDmPos`, `EmDmBloodSet2`, `PenClothSet/Move3`, `EstSet` are C.
- Header inlines that only *reference* a template member still instantiate it: the obj/esp/filter units
  carry the four `cManager<cLight>::create(int)` strings and the `create() failed %s id:%d` one of
  `create(int, u32)` because light.h declares `cLightMgr::createNew()`/`createNo()` (never called in
  the DOL; kept as out-of-class inlines so light.cpp does not get bodies).
- `if (c) n = a; else n = b + i * k;` where the else arm is a single simple set is emitted as
  `n = b + i*k; cmp; bne; n = a;` (jump.c hoists the else move above the branch); the sequential
  `n = b + i*k; if (c) n = a;` gives an `mr A',A` copy of the array base instead (obj00 FallMove).
- Local array element access `s[i].f.x` folds the field offset into the frame base (`frame+0x48 +
  i*stride`, indexed stores); `p = &s[i]; p->f.x` gives the stepping pointer with displacements
  (`stfs 0x18(r7); addi r7,0x2c`). A block-scoped `p` becomes "not always computable" when it is set
  after a conditional inside the loop, which stops biv elimination (`cmpwi i,2` stays instead of the
  pointer compare): set `p = &s[i]` before the `if`.
- `case 7: break;`-style nodes that share the default body still shape the tree (obj18 move needs
  `case 0xB: break;`), and a two-value range `case 2: case 3:` emits `cmplwi hi; bgt; cmplwi lo; blt`
  where `if (x >= 2 && x <= 3)` is range-folded to `subi/cmplwi` (obj01AddSpeed).
- `w->x &= ~2; w->x &= ~0x20;` gives two `rlwinm` on one load/store (combine refuses the non-mask
  constant); `&= ~0x22` loads the constant into a register (obj1d Lost).
- `(u16) w->word` from memory is loaded as `lhz +2` (combine narrows the load); a `(f32)` of an s16
  array element is `lhz; sth; psq_l qr5` (qr5 = s16, qr3 = u16).
- Parts matrix normalisation: `if (v.x == 0 && v.y == 0 && v.z == 0) v.x = 1.0f;` before each
  `VECNormalize` (obj00/obj1d SetOya), the column loads/stores are plain `m[r][c]` scalars.
- An opaque manager class must carry its real size (`u8 pad[0x34]`) or `extern cDmgMgr DmgMgr`
  lands in small data (`li r3, DmgMgr@sda21`).
- NgcAs emits `R_PPC_REL14` relocations for `bc` (conditional) branches to local labels; objdiff
  then shows the loop body as REPLACE lines although the bytes are identical (template copy loops).
- OPEN: independent `li rX,c` argument loads of a call are sometimes scheduled in a different order
  than ours (obj00 `setScrAtari` interleaves int/float arg moves, obj01 `EstSet` calls put `li r3,0`
  first while ours emits it last); the ProDG register-pressure tie-break is not understood.
- (camera units) `(&cam->member)->y` stays pointer arithmetic (GCC 2.x builds `&x->m` as base+offset), so
  matrix/column helpers taking `Vec*` (`getColumn(m, c, &v)`, `setColumns(m, &a, &b, &c, &d)` as static
  inline functions in cam_sys) give `stfs 4(rP)` through the address register with the `.x` store via the
  frame/base (cse picks the zero-offset form); plain `v.y = m[1][c]` gives frame-relative stores.
- PRE (gcse) creates a pseudo set in *both* branches (`mr r27, r5` after the existing `addi`) for an
  inline-parameter address computed in each arm and used after the join; call-argument `addi rN,r1,off`
  are hard-register sets and are never PRE'd (a fresh `addi r4, r1, 8` after the join).
- `w->maxFrame = (f32)(k & 0x3FFF); w->maxFrame += 1.0f;` (two statements) ties the add to the 1.0
  register with the double trick first in the pool; `(f32)x + 1.0f` ties to the converted value.
- `tbl = (u32*)((u32)w->partsNo + w->nParts); tbl = (u32*)(((u32)tbl + 3) & ~3);` keeps the sum in its
  own register (cam_motion ctor) where the single expression reuses it.
- Mtx copy loops (`while (i_--) { for (j...) *dp_++ = *sp_++; }`, MTX_COPY in motion.cpp/camera.cpp):
  declaring/incrementing `d_` before `s_` decides which pointer gets r9/r11 and the `addi` order.
- `for (i = 0; i < 2; i++) memclr_asm(&g_Arr[i], n)` gives a pointer loop with a *signed* `cmpw` end test;
  the do/while pointer form gives `cmplw`. `for (j = 0; j < 1; j++) a[j] = 0` with `u32 j` gives the odd
  `li r0,0; sth; addic. r0,r0,1; beq` one-iteration loop (ctrl12 move).
- A `pLog->warn()` path that falls off the end of a non-void function (`if (!p) { warn; } else { ...
  return idx; }`) leaves r3 = the warn call's r3 (trans_ot AddOt*Radius).
- vtable emission order at finish_file is reverse class-declaration order, and marking a vtable that
  holds a not-yet-instantiated template member (cManager<T>::destroy) instantiates it right there, so its
  strings land between vtable groups: ctrl.h declares cCtrl00/01/10 *after* cCtrlMgr to get
  `_vt.7cCtrl10, 01, 00, [destroy strings], _vt.8cCtrlMgr, cManager, cCtrl, cUnit`.
- An in-class inline `getWork()`/ctor of a class whose vtable the unit owns is emitted out of line (grows
  .text): use a free `static inline` (ctrl.h `CtrlMgrWork`) and no user-declared `cCtrl()` ctor.
- Functions with function-pointer parameters declared inside `extern "C" {}` need `extern "C"` on the
  definition too, or GCC 2.95 treats the definition as a C++ overload (`AddOtDirect__FiPvPFv_v...`).
- Weak vtable copies (`_vt.7cCamera` in cam_extra and cam_motion): the reference in the later unit binds
  to the first copy program-wide, so the first unit's copy must carry the `_vt.` name in symbols.txt and
  the later copy a distinct one (`cCamera_virtual_table_8022C140`), otherwise the DOL differs.
- `#line N` for an inline `MEM_ALLOC` inside a class body counts from the `class` line (ctrl.h: `#line 113`
  puts memAlloc on line 116).
- `-0x602` mask (`and r0, r0, r11`) in every `_._7cCtrlXX` is the inlined `cUnit::~cUnit` (`be_flag &= ~0x601`).
- (read) `bl ReadCheck__4cDvdi` with a `DvdReadInfo*` in r5: `cDvd::ReadCheck(int)` passes its uninitialised
  `info` pointer through to readCheckMain, and read.cpp calls it with three arguments. An asm-labelled
  member declaration reproduces the call (`int ReadCheckInfo(int, DvdReadInfo*) asm("ReadCheck__4cDvdi");`
  in dvd.h); PMF casts give an indirect call.
- `static` functions with unmangled names in sym_map (decodeData, readEm) were declared inside the
  `extern "C" {}` block. "global constructors keyed to X": X is the first *public* function/initialised
  object emitted, so everything before it in `.text` is static.
- `memcpy(dst, src, n)` with typed pointers (`OSModuleHeader*`) expands inline to a libcall with
  `crclr cr1eq`; `void*` operands give a plain prototyped `bl memcpy` (read readEmData).
- A store to a scalar global followed by a struct-member load (`EmInitFunc = m->pInitFunc; return
  m->pArc;`) lets ProDG hoist the load above the store; the original kept the order, so the global is
  stored through a struct view (`((EmInitFuncPtr*)&EmInitFunc)->p`, the pLog trick on the store side).
- A flag test through a `u16&` inline (`BitChk16(PlReadModule.flag, 2)`) materialises `sym+0x82` into a
  register; `&PlReadModule` right after is then `subi r4, rX, 0x82` (cse related-value). `BitOn16` on a
  global struct member gives `lis sym+ofs@ha; lhz/sth sym+ofs@l(r9)`, the plain `|=` after `&sym` is
  known gives `addi sym@l` + displacement.
- `lbzu r0, 4(rP)` at a loop top = `p += 4;` as the first statement of the body (combine merges the add
  into the load); the biv init `p = (u8*)arc + n*4; p += 0xC;` as two statements gives `add rP; addi rP,rP`
  where one expression leaves a temp (`add r9; addi rP, r9`).
- Independent struct stores at a function end: the *last* source statement is issued first, then the rest
  in order (`id, pArc, size, pModule, bssSize` -> `bssSize, id, pArc, size, pModule`).
- `if (f() == 0) { fail; return 0; } success; return x;` lays the success block out as the fallthrough
  (`beq fail`); the if/else form puts the fail block first.
- A constant stored to a global and then assigned to a local (`pG->pPlArc = C; data = (u8*)C;`) is
  cse'd into a copy and re-materialised as `lis/ori` before the preceding call; a local initialised at
  the top shares its register with the store (ReadPlayerData).
- `__attribute__((aligned(32)))` objects of size 0x98 in `.bss` (ReadModule) leave the 8-byte holes;
  the unit ends with `asm(".section .bss; .balign 32")`.
- OPEN (read ReadPlayerData): the PRE'd `type == 0` compare lives in a GPR (`mfcr r29`/`mtcrf 128`) in the
  original, in cr4 in ours; the pass-0 "already used register" choice depends on the GPR allocation order
  (r29 = pArc in the original). readEmData `m`/`newSize` swap r28/r29 (m has 14 refs, one more flips it).
- newlib units: SN's shipped `va-ppc.h` (v393/include) differs from gcc's: `char gpr/fpr` (signed), its
  own `va_arg` (`gpr + size <= 8`, `__va_longlong_p`) — `include/va_ppc.h` carries it. `strtod` (game/strtod2)
  is the Tcl strtod with `float` mantissa/`float powersOf10[]`, `isspace()/isdigit()` unprototyped
  (`crclr`), `if (!isdigit(UCHAR(*p))) { p = pExp; goto done; }` after the exponent sign. vfscanf is the
  stock 1.8.2 source with `MB_CAPABLE` (`__mb_cur_max`, `_mbtowc_r`) and `u_char *__sccl ();` unprototyped.
- (trans_ot/room_jmp/sce_sys) `-fcse-skip-blocks` rewrites a load after a skipped `if` block with the
  address `sym+off(rBase)` it knew before the branch, keeping the base symbol live across a call
  (`lwz r9, 0x88(r24)` + a second callee-saved register). The original avoided it with a different CFG
  in front: `if (zlimit == 0.0f) z = 0.0f; else {...}` instead of `z = 0.0f; if (zlimit != 0.0f) {...}`
  (AddOtWorldPos). Test with `-fno-cse-skip-blocks` on `cc1plus` to confirm the pass.
- `&g_Table[CONST]` folds into `lis/la sym+off`; a `static inline T* tbl(int i) { return &g_Table[i]; }`
  accessor gives the original `addi rX, rSym, off` (trans_ot `otWork(17)`, room_jmp `ofsTbl(p)[i]`,
  sce_sys `eventFlags()[no >> 5]` for `lwzx/stwx` on a pG-relative array).
- Argument order in the C prototype is fixed by the incoming-copy order at function entry: ints and
  floats are assigned registers independently, so `(Vec* pos, f32 radius, u16 kind, f32 zlimit)` and
  `(..., u16 kind, f32 radius, ...)` produce the same call ABI but different `mr`/`fmr` order and
  callee-saved allocation (AddOtWorldPosRadius: kind r27 > func r26 only with radius declared before kind).
- A struct local whose target frame slot is 8 bytes bigger than its declared size means the struct is
  bigger in the original (GeoSphere is 0x18: trans_ot frames), not a compiler temp.
- `for (...) { if (hit) { call(); ...; break; } }` with a *call* in the hit path is not rotated (initial
  `b test`, test at the top); the rotated original (entry test duplicated, `bdnz`-less bottom test) comes
  from `i = 0; if (i < n) { do { ... break; ... i++; } while (i < n); }` (roomdata clear). Without the
  call the plain `for` rotates (roomdata load).
- A search loop whose result is used *after* the loop with the address recomputed inside the loop
  (`mulli; lis/addi Task; add; lbz`) had no loop notes: written with `goto` (sce_sys SceExec slot search).
- `p = start; while ((p = get(p)) != 0) { if (p->x == t) { ...; break; } }` hoists the tail's constants
  (`li r30,0`, `lis sym`) into callee-saved registers above the loop; `return` instead of `break` or a
  `do {} while (p->x != t)` gives the non-hoisted form (sce_sys SceTaskDelete needs `break`).
- A `u16` member written then immediately read back (`x1C = tbl->rel; if (x1C == 0) ...; f(x1C)`) is
  forwarded as `clrlwi rX, rStore, 16` in the original; ours folds it to `mr` (roomdata linkRelData, open).
- `static test test; ... tbl[test.state](&test)` re-forms the address each use; the original kept
  `&test` in r31: `struct test* w = &test;` and use `w` (room_jmp RoomJump).
- `int no; no = w->mode; switch (no) {...}` with the *same* int variable reused for a call result inside a
  case (`no = pRj->checkRoomNo(...); if (no >= 0)`) gives `mr. r30, r3` into the switch register;
  cse's jump equivalence stores that register where a `0` constant is needed in `case 0:` (room_jmp).
- Zero-initialised statics with an explicit `= 0` (`static f32 rdir = 0.0f`) go to `.sdata`, not `.sbss`;
  unreferenced initialised static locals are still emitted (cam_extra: `rnd_gain`, `rnd_on`, `sct_max`).
- Declaring a C callee as returning `int` instead of `void` (cam_extra `IdTexDataLoad`) makes its own
  `mr r3,rX` argument copy precede its `li` argument loads; the preceding call's return type
  (`IdTexRelease`, `void`) did not matter there.
- Passing a struct (not its member) to a varargs `%s` (`pLog->err(..., FileTbl[x1C])`) copies the 8 bytes
  to the stack and passes its address (roomdata linkRelData — a bug in the original kept as is).
- A class with a constructor and an *empty* `~T() {}` is what makes GCC 2.95 emit the
  `global destructors keyed to` function next to the constructor one (roomdata cRoomData); the key is the
  first emitted object, static or not (`St0_data_tbl` is non-static in the original).
- (sscrn) A register argument whose address is a plain `(plus fp N)` / `(addressof reg)` is precomputed
  into a pseudo (`preserve_subexpressions_p()` is 1 at -O2) and cse then merges every later `&local` of the
  same variable in the extended block into that pseudo (callee-saved `mr r4, rN` copies). Only the local at
  frame offset 0 (`(reg vsv)` at expand time) is set straight into the hard register and recomputed per
  call. The bare `&far`/`&ab` (`addi r5, r1, 8` twice) vs `mr r5, r30` (`&ac`) split in sub2 comes from this.
- FadeSet colour pair (sscrn, mercenaries; `include/fade.h` `FadeColorPair`/`FadeSetW`): the recomputed
  `addi r4, r1, 8; addi r5, r1, 0xc` before every `FadeSet` and the fresh `addi r9, r1, 0x68` for
  `setAng(&pos)` after `setPos(&pos)` (OpeSetOpenTerm, `PlSetPosW`) are the locals of an INLINED helper.
  Mechanism (`.rtl`/`.gcse`/`.greg` dumps): the caller's own `&local` is a `(plus vsv N)` that
  `precompute_register_parameters` copies to a pseudo, which cse/gcse then merge and PRE hoist. A local
  of an inlined `static inline` function lives in the inline's frame; integrate.c maps that frame to a
  pseudo P with a `REG_EQUIV (plus vsv N)` and cse substitutes the constant address straight into the
  hard-register arg sets (`(set r4 (plus fp 8))`), which `hash_scan_set` never enters into the gcse table
  (hard-reg dest) — so the address is recomputed at every call and never PRE'd. Rules: (1) the colour pair
  must be ONE local of the inline (`FadeColorPair col` with a user copy-ctor so it is BLKmode and every
  inlined copy shares one 8-byte slot; two `GXColor`/`u32` locals get fixed spilled slots); (2) a store of
  a CONSTANT through P is rejected by recog (no store-immediate on PPC), which cancels that substitution
  group and keeps P (`stw rZ, 4(rP)`, `mr r4, rP`, PRE'd) — so write the constants through a value that
  was set before a label (`black = 0xFF` set once between the two `if`s, both branches of an `if (no &
  0x80000000)` for the 0/0xFF choice), which is why `FadeSetW` looks the way it does; (3) with the local at
  frame offset 0 the frame register itself is substituted and everything is direct. Same lever for any
  "second `&local` is fresh per call while the first is `mr r4, r30`" case: wrap the call in a
  `static inline` helper that takes the address by pointer parameter (integrate substitutes the caller's
  `&pos` for a read-only parameter) or owns the local. penClothAtMake's `&v1`: keep the header
  `PSMTXMultVec(..., &v1)` inside the inline (`penPartsWorldPos`) and make the *case bodies* use one
  `Vec* pv1 = &v1` pseudo declared after the header calls, so gcse has a pseudo to hoist (`addi r26, r1,
  0x18` in the prologue) while the header call stays a fresh `addi r5, r1, 0x18`.
- `switch (lang) { case 1: ... case 2: ... }` with *identical* bodies keeps two tree nodes (bodies
  cross-jumped afterwards): `cmpwi 1; beq; bgt` then `cmpwi 0`; a shared `case 1: case 2:` label makes a
  range node and a different tree (sscrn sscrnSetLanguage).
- `switch (room) { case 0x111..0x113: case 0x118..0x11B: return room - 0x10; } return room;` (u16 in/out)
  gives the `cmpwi/bltlr/ble/bgtlr/bltlr` chain with `subi; clrlwi 16` (sscrn sscrnRoomNo).
- `(u8)(u32 & 0x10000000)` folds to 0 at the tree level; storing through a `u32 t = x & mask; w->b = t;`
  temporary keeps `rlwinm; stb` (sscrn x1B6, a harmless bug in the original).
- `if (size == 0) bss = 0; else bss = alloc(size);` puts the `li r4, 0` between the compare and the
  branch (jump.c moves the *first* arm's simple set above the jump); `void* bss = 0; if (size) ...`
  schedules the `li` before the loads (sscrn DLL_Link).
- `u32* tbl = pG->bits; BitOn(tbl[no >> 5], 0x80000000 >> (no & 0x1F))` (u32 `no`) gives
  `rlwinm 29,3,29` + `lwzx/stwx` off a materialised `pG + 0x82F0`; indexing `pG->bits[...]` directly folds
  the offset into the displacement (sscrn OpeSetMdtNo).
- Two struct-member zero stores in an `if` body come out reversed (`x2AF = 0; x2AE = 0` → `stb 2AE; stb 2AF`);
  three separate `= 0` statements `x; y; z` → `z, x, y` (sscrn GameInit / RoomInit / Miss).
- `Cckpt.getCountDown()->f()` (inline accessor returning `&member`) inside a `for(;;)` task loop is hoisted as
  a loop invariant (`addi r15, r9, Cckpt@l` at the top, `addi r3, r15, 0xb0` at the use); a block-local
  `Cockpit* ck = &Cckpt; ck->countDown.f()` is not (`addi r3, r27, Cckpt@l; addi r3, r3, 0xb0` at the use).
  SubScreenExec uses the first form, SubScreenExit the second.
- The original `cUnit::beginEvent`/`endEvent` take an `int` (sce_com's `cManager<T>::beginEvent(int)` loops
  pass r4, sscrn passes 0); the shared declaration is still `beginEvent()`, so sscrn calls it through a
  vtable-compatible view class (`BEGIN_EVENT`). Changing cUnit means touching every override (obj*/em*).
- A loop-local `u32 addr = *cs++` instead of reusing the function-level `pc` swaps the r28/r31 allocation of
  the two (exception ErrorHandler call-stack loop).
- (cockpit) MEM_IN_STRUCT_P decides which loads survive a store: `*(f32*)((u8*)v + i*4)` (cast then deref, no
  PLUS at the top of the INDIRECT_REF) is not in-struct, so a `static f32 a_ratio` is reloaded after every such
  store while the address still folds to base+index (`lfsx r9,r10`); `v[i]` / `*(v + i)` are in-struct (the
  load is hoisted) and an `f32&` parameter makes the address a general giv (stepping pointer with
  displacements). `FSet(unitPtr()->rot.z, x)` on a call result keeps the following `pG` load below the store.
- A `u8` function result (`u8 f()` declared so) assigned to a `u8` local is never masked (SUBREG_PROMOTED); an
  `int` local holding a u8 result passed to two u8 parameters gets one PRE'd `clrlwi` before both calls
  (BulletInfo::move `wepNo`). `u16 num = f()` with `u16 f()` compares `mr. r30,r3` directly and masks
  `clrlwi 16` only after `num /= 10`.
- A 2-byte `struct { u8 hi, lo; }` local lives in a GPR: `d.hi = v/10; d.lo = v%10` builds it with
  `clrlwi/slwi/or`, a later `d.lo = x` inserts with `rlwinm 0,16,23 | clrlwi 24`, `d.hi` reads as `srwi 8`
  (or `extrwi 8,16` when the hi insert was unmasked). One such variable reused for min/sec/cs shares r28;
  an inline returning the struct by value spills it to the frame.
- fold merges `!(x & A) && !(x & B)` on the same lvalue into one `andis.`; an inline `chk(u32 b) { return
  pG->f & b; }` per test keeps the two `andis.`/`bne` (CountDown::move).
- Extra `psq_l` pool copies in `.rodata` after the last float function = a dead-stripped function with the
  same conversion formula (cockpit `TIME_FRAME` third copy, `STRIP_UNUSED`).
- A `static` local `u8 cnt` / `char xchr[5]` pair (`cnt.NNNN` .sbss, `xchr.NNNN` .sdata) with a `%c` print is
  the spinner `xchr[cnt]; cnt = (cnt + 1) & 3` (debug processBarDisp); config-file flag toggles in
  ConfigSet are BitOn/BitOff (each `|=` reloads `pG`).
- A game unit followed by an SDK library unit carries absolute-address padding the assembler cannot
  reproduce with `.balign`: sscrn ends with `asm(".text\n\t.long 0, 0, 0")` and a never-referenced
  `static u8 pad[0x1C]` (kept alive by an unused inline) for the 0x1C `.bss` gap.
- (datactrl) Array scans over a member array: `for (int i...) u = &unit[i]` gives the pointer loop with a
  *signed* `cmpw` end test, `u32 i` gives `cmplw`; both without an entry test. A pointer loop
  (`for (u = unit; u <= &unit[31]; u++)`) adds an entry test.
- Switch tree shape, exactly: after `group_case_nodes` merges adjacent cases with the same target into
  ranges, `balance_case_nodes` splits a list of n nodes/r ranges at the node where a countdown from
  `(n + r + 1) / 2` (2 per range, 1 per single) reaches 0; lists of 3 split at the middle, lists of 1-2
  stay linear. Empty `case k: break;` labels shape the tree (they are nodes whose target is the default),
  so a root of 4 for values 1..8 means `case 0: break;` exists too; `case 5: case 6: case 7: case 8: break;`
  (a range) moves the root of {0..4} from 2 to 3 (setLoadToMram/Aram).
- A `case` body that falls through into the next case (`case 7: add(); /* fallthrough */ case 4: case 5:
  add(); break;`) is the source of "duplicated tail" case bodies whose second half is not CSE'd with the
  first (no reload merging across the label) (getAramFree).
- loop.c (`find_and_verify_loops`) moves a block that ends in a jump out of the loop (`return X` inside a
  loop, guarded by one conditional jump) to right after the nearest BARRIER outside all loops before the
  return label. `if (n == 0) return 0;` before the loop creates such a barrier and the block lands there;
  `if (n != 0) { loops } return 0;` leaves only the function-top early return, and the found block lands
  after `if (aramSort == 0) return 0;` (checkAramSort). Blocks containing an inner loop are not moved.
- A `default: return 1;` that shares the trailing `li r3,1` with the normal `return 1` after a store
  block: write `default: goto ret;` with `ret: return 1;` after the stores. Without the label sched1
  hoists the `li r3,1` above the stores and jump2 cannot cross-jump the default into it (setClear).
- HALT-style macros as a plain `{ ... }` block instead of `do { } while (0)`: the OSReport stays in the
  same basic block as the preceding `pLog->err`, so its `li r4,0` is issued before the string `addi r6`
  (anti-dependence on the later `lis r4`); the do-while form gives `lis/addi r6` first (setData).
- A constant shared by stores in the loop and after it from one callee-saved register
  (`sth r21, 0xc(rP)` = 0x1F8 in three blocks) is a local assigned once *after* the preceding call
  (`eprintf(...); x = 0x1F8;`); assigned at the function top it is scheduled before the call.
- `Debug_free_h`/`Mem_free_h` (main_mem) are global (datactrl calls them) although Bio4.sym marks
  them local; datactrl's `cDataUnit` empty ctor + `~cDataUnit() {}` reproduce the 32-element ctor/dtor
  loops of `DC`'s static initializer.
- OPEN (datactrl dispDebug): global-alloc order p(r31) > u(r30) > this(r29) in the original; ours gives
  the loop pointer giv r31, this r30, p r29 (same refs and instructions; loop forms, declaration order,
  `unit[i]` indexing, do/while tried).
- Index registers vs base registers (regclass `record_address_regs`): in `lwzx rD, rBase, rOfs` /
  `add rP, rBase, rOfs` the offset pseudo prefers GENERAL_REGS (r0 first in the alloc order) when the
  base pseudo is pointer-flagged (a pointer parameter/local), and BASE_REGS (r9/r11/r10...) when it is
  not — both operands then count half as base. A table offset the target keeps in r9 while ours takes
  r0 means the base was an integer (`u32 addr` parameter, `(EffData*) addr` local), not a pointer
  (eff_sys EspDataLoad).
- `y = C; loop { if (hit) { f(y); y += step; } }`: the target's `li rY, C` issued *last* in the preheader
  (after loop.c's hoisted `lis`/`addi`s) and living in the highest callee-saved register is a
  strength-reduced giv: `n = 0; ... f(C + n * step); n++` (esp EspMove, esp_app EffAreaUpdate).
- update_equiv_regs (local-alloc.c): a pseudo set once to a constant and used once *in another basic
  block* has its `li` moved to right before the use (short range, a caller-saved register such as r9
  right before the `stw`); the same constant written at the store, or a local declared in the same
  block, is hoisted by sched1 into a long-lived callee-saved register. `int repType = 1;` at the
  function top with `mgr->repType = repType;` inside the `if` (esp_app EffEm2d_setTexRender).
- A pointer local assigned in two places (`mgr = pMgr;` twice) has two deaths, is skipped by local-alloc
  and gets a caller-saved register (r12) from global alloc; two distinct locals are local-allocated and
  take r9 / r30 in alloc order.
- Reading a global through a one-member struct *wrapper declaration* (`TexRenderMngPtr g_pMgr; g_pMgr.p`)
  forces the `@sda21` address into a register (`li r8, g@sda21; lwz r9, 0(r8)`) whenever the member is
  read as a value (`mgr = g_pMgr.p`, `this` of a method call) — `expand_expr` calls `memory_address`
  on the BLKmode struct and constant addresses go through a pseudo "to be cse'd". A plain pointer
  global with reference-setter stores (`BitSet(mgr->sx, 0x40)`, `ISet(mgr->repType, v)`) gives the
  same reloads with direct `lwz r9, g@sda21`.
- jump.c hoists a first-arm single set (`if (c) on = 0; else { on = 1; ... }` → `li 0` before the
  branch, inverted test) only when the else arm *starts* with a set of the same variable; reading a
  global into a local first (`u32 f = pG->flags_5010; on = 1; if (f & bit) ...`) keeps the target's
  `beq; li r11,0; b` block (espgen EspgenIsActive).
- local-alloc ties a dying operand to the result of a 3-operand insn (`fmuls fD, fD, fC` with the
  `(f32)d` operand in fD) only when the result pseudo is block-local with one death; a float temp
  assigned in both arms of an `if` (`t = (f32)d * c; ret = 1 - rate * t; ... else t = ...`) is global,
  so nothing ties and d/c/1.0 take f13/f12/f11 after the products in f0 (espgen00/02 Calc_D256).
- cse_around_loop (cse.c): for a `for`/`while` loop whose latch ebb ends at LOOP_END jumping back to the
  header, an expression computed in the header that a REG_LOOP_TEST_P register of the latch also holds
  (`sys + 0x10000` for high-offset members) is rewritten as a copy of that register, with a second copy
  emitted after the matching computation before the loop: `mr r9,r10` before the loop and in the latch,
  the header load using r9 (esp `operator new` loop 3). Goto loops never get it.
- OPEN (esp `operator new`): writing that loop as `for` reproduces the copies but loop.c then moves the
  `PushEsp(esp); goto found;` block behind `found:` (the guarded-exit-block motion above); no form found
  that keeps both. Loops 1/2/4 must stay goto loops (a `for` strength-reduces `&esp->flag`).
- OPEN (esp_app EffAreaUpdate / esp45 HideCheck): a loop.c-hoisted `lis` and an independent `or`/`li`
  in the preheader are issued in the other order; both have equal priority/weight so the tie-break is
  the RTL (LUID) order of the hoisted insns, which no source form changed.
- OPEN (espgen02 espgen02_Update): three `f32 x = 0.0f` locals share one pool load; cse loads it into
  the variable whose last mention is latest in the insn chain (ours colR, target spdR) — the target
  mentions spdR after colR's `colA *= colR` somewhere we do not reproduce.
- CLOSED 2026-09-12 (see "DOL Espgen43 closer 4"; was OPEN) (Espgen43 AddSandPower): the `lis/addi Chk_pos` pair and the z-word temp of the 12-byte struct
  copy swap r10/r11 (local-alloc priority order); memberwise, memcpy, pointer and statement-order forms
  tried. SetSandWork also has an unidentified 8-byte frame slot and one more callee-saved GPR.
- `cAtariInfo::init(int,int,int,f32 x7)` `fmr`/`li` order (SetTrolley/SetGondola/SetYagura/SetHeliMissile/
  SetPillar/SetBox/SetBarrel/SetEmSwitch/setScrAtari, also Espgen44_Destruct's `Filter05SetParam`,
  emobj's `SatMgr.create`): NOT adjust_priority/birthing — SetYagura has no loop, >10 blocks and >100
  insns, so every block is its own region and `bb_live_regs` never holds f1..f7 there. The `.sched`/
  `.sched2` dumps show the seven FPR arg copies and the three `li`s with equal priority (7), equal
  register weight (+1, nothing dies: the `zero` pseudo lives on for `pos = 0`, the copied hard regs f2/f5
  are call args) and equal dependence count, so the ready list falls through to INSN_LUID: the target
  order (`mr r3,this; fmr f1; fmr f6; li r4; fmr f7; li r5; li r6`, fpu one insn/cycle, sched2 keeping
  sched1's order) is exactly what an RTL order "this, FPR args, GPR args" produces, and GCC's
  `load_register_parameters` emits the moves in declaration order (ints first). Proven by an asm-labelled
  redeclaration with the floats first (`include/atari_init.h`: `atariInitF(cAtariInfo*, f32 x7, int x3)
  asm("init__10cAtariInfoiiifffffff")`, ABI-identical since GPR and FPR argument registers are numbered
  independently); with the float constants passed through an inline (`AtariInit`) so they are pseudos
  (cse shares the 0.0 with the `pos = 0` stores through the `if (pos)` branch; the 1000.0 pseudo feeding
  both f2 and f7 gets the longer chain the target loads first). The same "FP arg moves before GPR arg
  moves" appears in every unmatched int-then-float call site checked (embarrel/emBarred/emrock init,
  emobj `create(..., int, int, f32)` with `lwz`/`lfs` args) but not for GPR args that are register copies
  (`PSVECScale(&v, &v, f)` keeps `mr r3; mr r4; fmr f1`): a `calls.c` experiment that always emits arg 0,
  then the FPR args, then the other GPR args flipped 13 functions and regressed 37 (all cases with
  register-copy/`addi`/symbol GPR args after an FP arg), so the exact rule of the original compiler is
  still unknown; use the redeclaration where the target shows the interleave.
- A static function's position in `.text` is its definition position: `objTrolleySatClear` is defined
  after `objTrolley_R0_Break` in the original (forward-declared before `move`), which objdiff's 100%
  does not show — check `.text` symbol offsets against the split object before flipping.
- A store block's weight-0 member is the one where the *work pointer* dies (its last use in the block):
  SetBox's `w->itemNo = -1` is the last `w->` store in source although the target issues it first
  (both `-1` and `w` die there); with it written first, `w` died at `itemNum = 0` and that store jumped
  ahead instead.
- A distance test computed into the function's existing `f32 spd` variable (`spd = dx*dx + dz*dz; if
  (spd < K)`) lands in f1 (global pseudo, two assignments) where the inline expression ties `fmadds` to
  the dying operand's f13 (embarrel emBarrelSetRollSpd).
- An inline helper taking `const Vec* size` evaluates `&size` as the parameter copy before the body's
  `&ofs`, reversing the `lis/addi` pairs of `init2(0, 1, &ofs, &size, 0x10)`; a helper that *returns*
  `&ofs` (shared `.rodata` copy) used as the argument keeps the argument order (embarrel).
- (emrock) `cAtariInfo::init` with *computed* float arguments (`em->scale.x * 1200.0f * 0.5f`): plain
  `atariInitF(&em->atari, ...)` with the constants written inline reproduces both arms (the `fmr f5,f4;
  fmr f6,f5` chain and the `fmuls`-interleaved `li`s); `AtariInit` (pseudo constants) does not. The
  `&em->atari` pseudo PRE'd into r28 for the later `setPriority`/`clrFlag100()` comes from the member
  call form, so use `em->atari.clrFlag100()` (not `em->atari.flags &= ~0x100`, which re-derives the
  address from `em`).
- Any int-then-float call whose target issues the FPR moves before the `li`s can be redeclared with the
  floats first under `asm("<mangled>")` (atari_init.h idiom): `setYarareCubeF(cEmRock*, f32, f32, f32,
  Vec*) asm("setYarareCube__7cEmRockP3Vecfff")` fixes `fmr f3,f1` before `li r4,0` in setFall/setThrow.
- A callee whose mangled name says one parameter but whose body reads r5 (`cGameSave::save(void*)` reads
  an `int` in r5; every caller loads it) is declared through a free asm-labelled function with the real
  parameters *and the real return type*: `int GameSaveSave(cGameSave*, void*, int) asm("save__9cGameSavePv")`.
  Declared `void`, `li r3, GameSave@sda21` is issued before `li r5, -1`; with `int` the r3 output
  dependence puts it last (the "callee return type" rule also applies to asm-labelled aliases).
- `dx*dx + dy*dy + dz*dz` compared with a radius sum: compute `len` into its variable first and
  `r = w->radius + 1000.0f` *after* it (`fadds` then `fmuls f0,f0,f0` tied), then `if (len > r * r)`;
  with `r` computed before `len` the radius load is interleaved into the distance chain (emrock DropHitCk).
- `if (atk) { ...; if (EmAtkHitCk(...)) { ...; return 1; } } return 0;` gives both tests `beq` to one
  `li r3,0` block at the end; two separate `return 0`s put `li r3,0; b end` after the second test.
- `pl->frame / (f32) pl->frameMax` (u16 member, `psq_l qr3`) times a u16 motion count held in a `u32`
  local (`u32 cnt = hdr->maxFrame` → unsigned double trick) converted with `(u32)` gives the
  `fcmpu 2^31/cror/bso` unsigned conversion; write the ratio into its own `f32` first when the target
  computes it before the count (plemRockEscape).
- `(int) pl->x3E0 / 20` on the `u32` cEm field gives the signed `mulhw 0x66666667; srawi 3` divide (obj13
  uses the same `(int)` cast for signed tests; do not change the shared field type).
- A pointer local `Camera* cam = &G;` + `FSet(cam->param.fovy, C)` keeps the store `stfs 0xc0(rCam)`
  and, being a scalar-reference store, gives it a dependence on the following `lwz pG` so it is issued
  before the `addi r5, r1, 8` argument; later `&G.param.at` written on the *global* are cse
  related-values `addi r5, rCam, 0xb0` recomputed per call (a `cam->param.at` pointer form PRE's them
  into callee-saved registers). In a block that follows a branch join the same `&G.param.at` is the
  `lis rH, G+0xb0@ha` / `addi rX, rH, G+0xb0@l` pair with the high part shared (emrock cam functions).
- OPEN (emrock plemRockEscapeCamMove2 / plemRockDropDieCamMove tails): after `PosToPos(..., &G.param.at)`
  calls, the tail's `Vec* cp/ca = &G.param.pos/.at` reuse the calls' high pseudos (`addi r9, r29, G+0xa4@l`)
  and `Camera* cam = &G` is a *fresh* `lis/addi` pair; ours relates `cam` to the newest pointer
  (`subi r30, r9, 0xb0`). When the pointer comes from a struct copy with its own fresh `lis`
  (emRockPushCamMove) the `subi r30, r9, 0xa4` form is what the target has. The cse related-value
  chain needs the lo_sum to fold, which requires the high pseudo to be known in the tail's ebb; no
  source form found that hides it (~10 tried). Also OPEN: SetRock's `w->seAlways[2] = 0` (last QI use of
  the shared zero) is issued in source order by the original although the register dies there.
- A zero word in `.rodata` between two functions' pools that no code references is the pool of a
  dead-stripped `static` function (emrock: `static void emRockSpdClear()` with three `= 0.0f` stores
  between setThrow2 and setYarareCube, unit added to `STRIP_UNUSED`); compare the `.rodata` words of
  the compiled object against the split object directly, objdiff does not see it.
- global.c allocation priority is `floor_log2(refs)*refs/live_length` *truncated to an int* (`*10000`),
  ties broken by pseudo number. REG_LIVE_LENGTH counts every insn *and note/label/barrier* in the range
  (flow.c increments outside the `'i'`-class test), so deleted statements, block notes and jump layout
  shift it. pl_leon setRightHand: `data` (5 refs/20) lost r30 to `info` (5 refs/19) only because the
  do-while(0) HALT let cse rewrite the HALT store's 0 as `info` (a 5th ref); the plain-block HALT gives
  info 4 refs and `data` wins. Hoisted invariants that never die (loop-invariant `lis` in a `for(;;)`)
  all truncate to the same priority and are allocated in pseudo-number order.
- `const f32 name = literal;` locals: the initialiser is expanded (creating the pool entry at that
  point and a 4-byte frame slot) but emits no code, and every use is folded to the literal, so `h +
  name` is computed where it is used while the pool order follows the declarations. pl_push emSandCheck
  needs `const f32 sand = 800.0f; const f32 base = 300.0f;` (pool 800, 300, 0.0 with rot stored first).
- Zero stores come out in pure source order (none first) when no store is the zero's last use: the
  dying one is the *last* zero store in source (`x54 = 0` after `flags = 0`, pl_cloth testDressSetAda).
- Two different QI zero stores separated by a call reuse one pseudo (callee-saved); the original's
  fresh `li r0,0` after the call = the second group used an SImode zero: `u8` fields stored from `int`
  values (an inline `PlRoutineSet(pl, int, int, int, int)`), cse cannot merge SI and QI zeros (pl_dmg).
- `do { } while (0)` macro bodies emit NOTE_INSN_LOOP_BEG/END; haifa makes the first insn after a
  loop note depend on *everything* before it (loop_notes → full barrier), so PRE copies inserted at
  the block end cannot move above the next macro invocation. Written-out blocks with a plain `{ }`
  (pl_cloth LAPEL_MOVE) let the copies schedule right after their `addi`s; re-deriving `pm1 = m1`
  inside the inner block makes the first lapel's later uses go through the PRE copy (`mr r24,r27`).
- PRE-created pseudos are numbered in *hash bucket order* (pre_delete walks `expr_hash_table`), with
  `hash_expr` hashing `high(symbol_ref)` by the symbol *name* (so `.LC<n>` numbering and the table
  size `max_uid/4|1` both matter); the insertions themselves come in bitmap-index (first-occurrence)
  order. path PathGetMatEm: three dead `int x = 0;` initialisers (+3 uids) plus taking `&hpos.x[j]`
  before `&key0` in the loop body give the target's spill slots and copy order. exception ErrorHandler
  (LC64/symbol_err_tbl swap) has no solution with our `.LC` numbering — the original TU numbered its
  constants differently (open).
- gcse 2.95 PRE of a struct load across an if/else: to stop it, kill memory at the *join* (an empty
  `asm volatile("" : : : "memory")` as the first statement after the if/else makes the join's loads
  non-anticipatable; placed inside the arm it leaves the conversion paths jumping to the asm's label).
  A `default:` arm that falls into `case 0:` gives its string `lis` an extra anti-dependence on the
  `err` call (the call's `depend_count` tie-break then issues `lis` before `lwz pLog`); a `default:`
  with its own `wrap = 2; break;` (cross-jumped) keeps `lwz pLog` first (TexRender CopyTexRenderMgr).
- Reusing a dead pointer local for a later block (`p = &tile[0]; ... p = &tile[1]`) extends its live
  range to the function end and gives it the top callee-saved register (datactrl dispDebug p=r31).
- A variable set in the two paths of an unsigned float→int conversion is global-allocated; a second
  such variable in another block must be a *different* local (block-scoped `u32 x0`) or the shared
  pseudo's range covers both (datactrl loop x0 r6 vs over-block x0 r5).
- `int n = 0; int base = 0;` declared inside an `if` block (C++ mid-block) put their `li`s in that
  block next to the calls; at function scope they are hoisted to the prologue (stage subMissionSt1).
- `SatMgr.destroy(p)` on the object is a direct `bl destroy__7cSatMgrP4cSat`; `sat->destroy(p)`
  through a pointer is a vtable call (pl_debug satMakeTest) — check when a manager method is virtual.
- objdiff REPLACE rows with identical text: the split object can carry a synthesized `R_PPC_NONE`
  reloc (path `lfs f12,0(r29)`) or a symbol+addend spelled from a neighbouring symbol
  (`globalCamera+0xe0` = `g_RndMgr-0x38`); compare bytes/addresses, not the row.
- `x == 2 || x == 3 || ... || x == 6` on one lvalue is range-folded by fold_range_test, and five
  separate `if (x == k) return 1;` make the last test a setcc (jump.c store-flag on a single-use
  diamond). The unfolded chain (`cmpwi k; beq L1` x4, `cmpwi 6; bne L0; L1: li 1; b; L0: li 0`) is a
  `||` of inline CALL_EXPRs: `SysRegionIs(2) || SysRegionIs(3) ...` with
  `static inline int SysRegionIs(int r) { return pSys->region == r; }` — operand_equal_p refuses
  expressions with TREE_SIDE_EFFECTS, and the shared true-label sits between the last `bne` and the
  `li 1` so jump.c cannot make a setcc (dvd SysIsEurope, card has the same chain).
- cse-follow-jumps decides `lwzx rD,rIdx,rBase` (index form) vs `lwz rD,0(rSum)` for `*ph` with
  `ph = &pFilehead[depth]`: the sum register is replaced by the index form only inside the extended
  block that knows the equivalence, i.e. blocks entered through a once-used label preceded by a
  barrier (cse follows them TAKEN, then re-runs the fall-through). A block that starts with a label
  entered by a `goto` from elsewhere (`snd_err:` shared by two error paths) is a fresh ebb and keeps
  the sum form; the original duplicated the error block in both `if (r == -1)` arms and let jump2
  cross-jump them (dvd readMain, also flips the r28/r29 allocation of ph vs &pFilehead).
- The `mr; cmpwi` vs `mr.` question (mes) is a combine question: `P = r3; cmp P` right after a call
  always fuses into `mr.` in our RTL; the copy survives unfused only when can_combine_p fails
  (a set of r3 or a volatile insn between them, a label, or a non-REG dest). ~30 forms tried
  (int/u16/s16/u32/volatile/register locals, switch, goto, `if ((code = f()) ..)`, inline wrappers
  give `rlwinm; cmpwi`). Still open.
- A do/while(0) HALT macro is a sched1 barrier (loop notes): with a plain `{ }` HALT the preceding
  `pLog->err`/`OSReport` argument moves are ranked together with HALT's own `lis r4,__FILE__`,
  which pulls `li r4,0`/`lwz r4` before the string `lis/addi r3|r6` (main_sub DLL_Link/DLL_Unlink,
  read decodeData/ReadPlayerData/ReadWepData — the ReadPlayerData `mfcr` OPEN went away with it).
  Units where the do/while form matches (eprintf, sce_com) keep it: try both per unit.
- The `mr rN,rM` copy of a just-loaded global (`lwz r0,g; mr r7,r0; cmplw r0,..`) means the compare
  read the global *before* the local was assigned: `if (mess_keep_ptr >= ..) return; p = mess_keep_ptr;`
  — cse turns the second load into a copy of the first (eprintf EprintfBuffering). Also there:
  `h / 1.3333333f` (7 digits) is 0x3faaaaaa; the original constant is 0x3faaaaab (`1.33333333f`).
- ErrCheck-style `pMes`/`pStr` r20/r21 swaps between two 3-ref invariants: priority = int(30000/L);
  L 390 vs 386 fall in buckets 76/77, so the later-declared one wins; the original's lengths must
  share a bucket (then the lower pseudo wins). Block-scoped declarations and do/while notes do NOT
  change REG_LIVE_LENGTH at global-alloc time (only real insns count there).
- Asm-labelled *definitions* (`int IDSystem::setCkI(int) asm("setCk__8IDSystemUc")`) do not
  assemble: SN's cc1plus emits the function-begin label as `.L_f*setCk__8IDSystemUc_s` (the `*`
  of the asm name) and NgcAs rejects it. So the narrow-parameter masks (`clrlwi rP,rP,24` on a u8
  parameter, id_sys setCk/dispSw/kill) cannot be reproduced by an int-parameter view; an explicit
  `type & 0xFF` is folded away too (nonzero_bits knows the promoted parameter). Only a tool-side
  rewrite of that label (or a compiler flag for argument promotion) would open these.
- gcse PRE copies land at the *end of the block* that computes the expression, and in C++ a block
  ends at every call (EH: flow appends `use (const_int 0)` after CALL_INSNs). An `&member`/`&local`
  address computed in both arms of an if/else and used after the join therefore gets its copy
  `mr r28,r29` right *after* the arm's `bl` (dvd Initialize: `sprintf(name, ..)` in both arms, then
  `name` used directly after the join); a source-level `n = name;` is a cse copy at the statement
  that sched1 hoists above the call. Write the global/member directly after the join, no local.
- `lwz r9,g; <use r9>; mr r11,r9` with later blocks using r11 = the global read directly in a block
  that cse cannot reach (a join with two predecessors after `a && b` / `a || b`, or a loop test):
  gcse PRE re-loads it at the end of bb 0 and cse2 turns that into a copy whose register becomes
  canonical (last use beyond the ebb) for every later block. A local `T* p = g;` merges all reads
  into one register and hides the copy (pl_wep getAngle/getPitch `pPL`, scroll smxInit `pSmx`,
  getWorkNum's loop bound `grp.nGroup` with a guarded do-while + `grp.num[i]` indexing).
- A `li rY,C` loop counter issued *after* loop.c's hoisted `addi`/`lfs` in the preheader is a
  reversed count-up loop (`for (i = 0; i < 3; i++)` with `i` unused in the body); `for (i = 3;
  i != 0; i--)` puts the `li` at the statement position before the hoisted insns (pl_wep
  wepSetWaterShot).
- `(u8*)this + n*4 + 0x14` keeps `add this,idx; addi 0x14`; `(u8*)this + 0x14 + n*4` and
  `&member[n]` fold the constant into the index (`addi idx,0x14; add`) (scroll getWorkPtr).
- `if (a || b) return 0; return p;` shares one `li r3,0` block after the second test; two `if`s
  each returning 0 give two `li r3,0; blr` tails (cManager getPrevWork). `if (!(f & 4)) return NULL;
  return call();` keeps `li r3,0` in its own arm after the call arm; `flag ? call() : NULL` and
  `if (flag) return call(); return NULL;` let jump.c hoist `li r3,0` above the branch because the
  fallthrough arm then starts with a set of r3 (scroll SmdGetGroupNext).
- A struct-member load reloaded after a store to an address-taken stack local (`lwz r9,0x15c(r3)`
  twice around `stw r0,0x28(r1)`): the load was a reference read (`PRef(scr->pInfo)->pTpl`) — a MEM
  with neither struct nor scalar flag conflicts with the fixed-address scalar store, while a plain
  member read (in-struct vs fixed scalar) is hoisted/merged (esp_efm EfmSeqSet `model`/`tpl`).
- OPEN (dvd DiscChange): the 16-byte `game[]` template copy loads words 0,8,c,4; sched2 gives our
  word-4 load priority 9 (its `stw r9,4(r11)` is anti-dependent on the later `lwz r9,pSys`) so it
  goes first; declaration order, `char company[3]`, `const char* const`, separate stores all tried.
- OPEN (read readEmData): `newSize` (6 refs / 54 insns → 2222) beats `m` (14 refs / 198 → 2121);
  `MEM_ALLOC(size)` vs `MEM_ALLOC(newSize)` makes no difference (cse rewrites `size` to the
  later-used `newSize`). Needs `m` ≥ 15 refs or ≥ 4 more insns in newSize's range.
- OPEN (esp_efm EfmSetObj04, emrock emRockRollStartCk, route_ck RouteCkPosToPosDis): the target
  re-reads `w->x79` (`lbz r4`) in the else arm after `stw r24,0x6c(r31)` although cse1 following the
  `beq` merges it in ours (`-fno-cse-follow-jumps` reproduces the reload but is not a per-unit
  option); emrock's `mr r11,r9` pG copy has no third read to PRE; RouteCkPosToPosDis keeps
  `mr r3,r31; mr r4,r29` before `rckLineHitCheck(from, to..)` where reload_cse deletes ours
  (nothing sets r3/r4 or a label between the prologue and the call; goto/flat-if forms tried).
- Sprite corner leaves (esp.h `ESP_SPRITE_CORNERS`): the `!flip-s, flip-t` leaf adds into `s1`
  (`s0 = zero; s1 = s0 + z; t1 = s0; t0 = s1;`) in esp_sub/esp0f, into `t0` in esp08 — read the
  target's `fadds` destination per unit. With the idiom esp_sub Shimmer/Nega and esp0f are 100%.
- `x = 1; if (f() != 2) x = 0;` store-flags to `xori/subfic/adde` (jump.c: `reg_set_last` finds the
  constant 1 across the call, BRANCH_COST 0 case "A is a power of two, B is 0"); the reversed test
  `if (f() == 2) x = 0;` keeps `cmpwi/bne/li`. esp_sub Shimmer's `GetDrawTmpBufType() == 2` was
  simply the real condition (esp18 already had it).
- `EspSeqSet(rec, info, seed, model, mtx, int a, f32 f, cEsp** out, EspSeqOpt* p8, Vec* pos)` is the
  real parameter order (prologue `mr r24,r8; fmr f31,f1; mr r25,r9; mr r28,r10; lwz r23,0xc0(r1)`),
  callers are ABI-identical; its locals are `Vec v; Mtx m; Mtx m2; EspPtr e;` with the one `Vec`
  reused for the RotMatrixZXY input and the PSMTXMultVecSR output (frame 0xb8).
- Two constant-pool `lis` in one store block (0.0 vs 1.0 highs, EspCommonTrans' third arm) are
  issued in the RTL order of the first statement using each constant: `mat[2][2] = 1.0f` written
  after the first `= 0.0f` store puts the 0.0 `lis` first.
- A `lis rX,0x4330` in a callee-saved register far above the int→float conversions that use it
  (esp16 `Esp16_Trans`: `lis r31` after CameraCurrentProjection, `stw r31` in both if/else arms) is
  cse canonicalising the arms' constant pseudos to an OLDER one on the ebb path: a dead conversion
  earlier in the function (`rate = (f32)(int) n;`) whose result flow deletes; signed vs unsigned
  matters because only the SI constant is shared (the DF magics differ). update_equiv_regs moves a
  single-use constant next to its use, so the hoisted `lis` needs ≥ 2 live users (esp12's single
  conversion cannot be reproduced that way, still OPEN together with the `lfs f12; fmr f29,f12`
  copy for `t = 0.0f` in both units).
- Constant pool order via a dead declaration initialiser: `f32 ang = (f32)(int) esp->cnt;` (esp18)
  puts the signed DF magic before the `0.0f` of the next initialiser; flow deletes the load.
- `Filter05SetParam` argument-move order is per call site: all-immediate (Espgen44_Destruct) wants
  the floats-first alias, loaded arguments (Espgen44_SetFreeWork) want the floats between the 5th
  and 6th ints (`(int a,b,c,d,e, f32 x,y,z, int f,g) asm("Filter05SetParam__Fiiiiiiifff")`), the
  only one of 7 interleavings tried that matches — espgen44 is Matching with both.
- espgen02_Update (OPEN, corrected): both ours and the target load the shared 0.0f into spdR and copy
  to scaleR/colR; the diff is only spdR/colR = f24/f23 vs f23/f24 (global-alloc priority, spdR has
  5 refs vs colR's 4 in ours).
- `tools/fdiff.py` can fail with "Invalid control character" on units whose objdiff JSON contains raw
  bytes (esp_sub Shimmer): read with `json.load(..., strict=False)`; concurrent fdiff runs share
  `build/G4BE08/fdiff.json`, so a private copy with its own output path is safer.
- (player) cse's extended block ends at a label with two uses: `if (k & 4) {..} else if (k & 8) {..}`
  (join label used twice) makes the `&Key` address after the following calls a fresh `lis/addi`,
  while two separate `if`s let cse carry it in a callee-saved register (pl_R1_Run). `u64 key =
  Key.on;` tested twice keeps the low word in a register (`rlwinm r10,r12` without a reload);
  `Key.on & bit` twice reloads both words after the store between them.
- jump.c store-flag: every if/else or `x = !(...)` form of `if (c5 && f60 >= 0) moved = 0; else moved
  = 1;` becomes `srwi r26,r0,31` (the hoisted `x = 0` + `if (c) x = 1` diamond, A=0/B=1). The
  original's `li r26,0; cmpwi; bge L; li r26,1` needs a CODE_LABEL between the jump and `x = 1`:
  `if (c5) { moved = 0; if (f60 >= 0) goto ok; } moved = 1; ok:` (the label shared with the outer
  failure path blocks the pattern; cPlayer::move).
- A `u32 frame` local converted with `(u16) frame` at its uses gives one `clrlwi r30,r6,16` after
  the if/else join (Walk/Run motionSet + neck init); a `u16 frame` local gets the extension folded
  per branch (`lbz r30` / `li r30,0` / `clrlwi` inside the float arm).
- Dying-zero-store promotion: the zero store that comes first in the target's block is the *last* in
  source (init1: `satCheckFlag = 0` after `boss0 = 0`); the QI zero of an early `u8 = 0` stays its own
  pseudo (`li r0,0`) only when it precedes every SI zero store.
- Objects with constructors (`cMot3 mot3`, the `m3r` rates) are emitted at their definition, not
  deferred like plain uninitialised globals: define them after the function whose static local
  precedes them in `.bss` (player.cpp: after pl_R1_Turn180's `dd0`). A `f32 x[3]` whose static
  initializer stores one pool 0.0 three times (`stfs 0,m3r@l; stfs 4; stfs 8`) is a class with a
  constructor `r[0] = r[1] = r[2] = 0.0f` (a POD `{0,0,0}` is static data, an inline-call initializer
  gives `stfsu`); other units keep the `extern f32 m3r[3]` view through an asm-labelled alias
  declared at the same header position (`extern cMot3Rate m3rObj asm("m3r")`, .bss order = first
  declaration).
- An inline member of the vtable-owning class defined *out of class* in that unit (`inline void
  cPlayer::subCharLiveCheck() {..}` in pl_class.cpp, declared in-class in player.h) is still emitted
  after the destructor in declaration order, and every other unit calls it out of line (cPlayer::move
  `bl subCharLiveCheck__7cPlayer`); an in-class body would be inlined there.
- Static data members mangle as `_7cPlayer.SPEED_WALK_TURN`; sync_symbols demangles them now
  (renamed the pl_class placeholders). Function-local statics with a DECL_UID suffix
  (`pl_move_func_tbl.1272`) are matched by base name in strip_unused (`--gcc`), so an unreferenced
  static table of a STRIP_UNUSED unit survives like in the DOL.
- `pl->x3E0++; if (pl->x3E0 >= 5 && pl->x3E0 <= 14)` compiles to `subi r11,r9,4; cmplwi 9` on the old
  value (combine folds through the increment) with the store issued before the compare (JumpFall).
- Identical case bodies written twice (`case 0xB:` and `case 0xC:` each with the full body, Crouch)
  keep two tree nodes (`cmpwi 0xc; beq; blt`); a shared `case 0xB: case 0xC:` label is a range
  (`subi; cmplwi 1; ble`). Turn180's `switch (x4FB8)` needs `case 0:` as its own arm (same body as
  default) plus `case 5:` grouped with `default:` for the `cmpwi 2` root.
- (player) `pl->pNeck->motL = 0` followed by a global read (`PlFanceFlag`): the load stays below the
  store only through a `void*&` setter (`PSet`), like the `pG` reloads. `hp = pGS->pl_life` (struct
  view) after `pPL = this` keeps `lwz pG` below the scalar store.
- (pl_npc) `cSubChar` is a cEm whose partner fields overlay the player ones (em.h unions at 0x378,
  0x3E0, 0x3E4..0x400, 0x404..0x524 incl. a second `MotionWorkSub subBackMot` at 0x454, and the
  0x5CC/0x7D4.. tail); `subFlags`/`subFlags2` are `cFlag`s: tests written as
  `((cFlag*) &subFlags2)->check(bit)` reproduce the `lhz; mr rX,r0; clrlwi r0,r0,16` copies gcse PRE
  gives a HImode member load (plain `& mask` tests fold adjacent halfword tests into one word compare
  and never show the mask). `&= ~bit` on the u16 flags is `BitOff16` (`rlwinm`, not `andi.`).
- Routine-byte blocks (`xFC..xFF` + a mode word): `SubRoutineSet(pl, fc, fd, fe, ff)` (int inline)
  followed by the other stores gives the original order when the zero pseudo stays live (`RS(1,0,0,0);
  mode = 2;` -> `fc, ff, stw, fd, fe`; `RS(1,0,0,0); dmHit = 0;` -> `324, fc, fd, fe, ff`); brute-force
  the alternatives with a 6-line test file through tools/ngccc.py + `dtk elf disasm` (seconds).
- A `switch` with a hidden `case N: break;` shifts the compare-tree root (moveFootwork needs
  `case 0x34: break;`); `switch ((u32) f())` gives `cmplwi` range tests; a `default:` written first is
  laid out first.
- (pl_npc, Matching) Judge functions by masked *bytes* plus resolved reloc targets, not objdiff's
  percentage: dtk synthesizes `Sym+off` relocs the compiled object lacks, so 30 byte-identical
  functions showed 97-99%, and one "identical" function (jumpAdjust) had its pool words permuted —
  the masked bytes matched while the constants were assigned to the wrong fields (a = {300,300,0},
  not {0,0,300}). Compare pool *values through the reloc targets* before believing a match.
- Constant-pool order idiom, confirmed on 9 functions: a `const f32 name = literal;` local declared
  before the first use creates the pool entry at the declaration with every use folded, no code
  change (moveMove 400 first, getScrActionPoint 400 before -1000, moveFallWait 1300/0.314 first,
  neckCtrl both speeds first, checkBackEm 4e8/2.5e7/2.618, frontCheck 1500, anaSatInfo 360000,
  jumpAdjust). A dead `f32 x = C;` is dropped at tree level and creates nothing; a const declared
  *after* the stores is dropped too. An unreferenced pool word (the static initialiser's 1000) is
  NOT a kept dead entry: mark_constant_pool drops those in the original as well — it is the pool of
  a dead-stripped function emitted right after (an in-class inline of the vtable-owning class,
  `cSubChar::farCheck`, unit in STRIP_UNUSED).
- Unused `static inline` functions parsed at the END of a unit reorder the vtable/static-init output
  (the static init's pool moved before the vtables): put dead pool-only helpers before the function
  whose pool they precede.
- `__static_initialization_and_destruction_0` is emitted before the out-of-class `inline` members
  (setFace/setHand/initCloth/moveCloth) and the `_GLOBAL_.I` thunk; non-inline definitions of the
  same empty virtuals land before it.
- A switch whose adjacent same-target cases are tested one by one (`cmpwi 7 beq; cmpwi 8 beq` instead
  of a range) had one body per case value (duplicated bodies, cross-jumped later): group_case_nodes
  only merges consecutive values that share a label (moveDamage's subHideMode switches).
- A value computed into a local before the member store (`f32 y = expr; eyeDir.y = y; if (eyeDir.z
  == 0) eyeDir.x = y;`) issues the expression's constant loads before the compare's; the member-store
  form (`eyeDir.y = expr; ... = eyeDir.y`) schedules the compare first. `x = x*z + y*(1-z)` written as
  a member function of the static's class loads z before 1.0 (moveFace).
- `SubRoutineSet(this, 0, md, 0, 0)` with `int md = 1;` declared at the top of the case block: the
  `li r6, 1` is shared by both if/else arms and lets jump2 cross-jump their AtariOn tails (control).
- Byte-store order rule (dmgCheck/control): the emitted order is not the source order; brute-force
  the 3-4 statement permutations with a scripted loop (tools: ngccc.py, ~0.3 s per variant).
- cAtariInfo flag stores through the info's address (`cAtariInfo* at = &atari; AtariOn(at, 0x300)`)
  give `addi rX,this,0x2b4; lhz 0x1a(rX)`; a scalar-reference store on `at->flags` keeps a following
  `lwz pG` below it.
- Two `MotionSetCore` calls that share their tail in the target are `void* m; if (..) m = A; else
  m = B; MotionSetCore(pl, .., m, ..)` (arms compute `arc->ofs[n]`, the `add` after the join); a
  ternary index gives `lwzx`.
- `MotionMove` is called with two arguments by the partner code (`MotionMoveF(m, 0) asm("MotionMove")`).
- Frame size = ALIGN16(8 fixed + ALIGN8(vars) + 8 fpmem + ALIGN8(gp+fp saves)): the "unexplained
  8-byte slot before the fpmem slot" (merchant buyupPrice/sellPrice, option) is usually this 16-byte
  rounding, not a local. A frame that is 0x10 bigger with the *address-taken* locals starting 0x10
  later (`addi r28, r1, 0x18` instead of `0x8`) is an unused aggregate local declared before them
  (option `ChapterEnd::move`: `Vec unused;` — addressof slots are assigned after declared aggregates).
- cse_insn's `(set REG0 REG1)` swap: `tbl = &sym; t = tbl;` gets rewritten to `t = &sym; tbl = t`
  (the lo_sum's dest becomes the copy's dest) when t's REGNO_LAST_UID is later than tbl's and the
  previous non-note insn is tbl's set. Any statement between the two (`ofs = 0;`) blocks it, so the
  `addi` result stays `tbl` and the stepping pointer is the `mr` copy; a second copy taken from `t`
  (`t0 = t`) after that is what cse2 turns into the "lhzx base" register (stage subMissionSt1).
- global.c priority truncation ties (`int(10000*log2(refs)*refs/len)`): OpeSetOpenTerm's params x
  (4 refs/216) and z (2 refs/54) both give 370, so the lower pseudo (x) wins f30; the original
  breaks the tie the other way. REG_LIVE_LENGTH here is `recompute_reg_usage` after sched1 (real
  insns only: block notes and store order changes do not count) — one more real insn in x's range is
  needed, not found yet (OPEN).
- loop.c invariant threshold with a call in the loop is `1 + n_non_fixed_regs` = 71: a `lis` (savings
  1, lifetime 1) is hoisted out of an outer loop only while the loop has <= 71 real insns at loop pass
  2. merchant buyupPrice(ItemWork*) hoists ours (65 insns) but not the original's — its outer loop
  had >= 72 pre-combine insns (OPEN: which extra RTL).
- Dump-script note: the build passes no `-G` to cc1plus (cflags have none); a hand-run cc1plus with
  `-G1024` changes small-data references and callee-saved counts. Use `-O2 -mfast-cast -da` only.
- mercenaries MercSysInitRoom (SOLVED 2026-09-10, COMPILER-DIFF #13 `register int z asm("r11")`): the
  `wk->stage = 0` zero is a reload-materialised pseudo in
  the original (`li r11,0` right before the `stw`, after `lwz pG`), ours a sched1-hoisted `li r0,0`
  (local-alloc'd); and the SndStrReq `lfs f1` pool load is issued last (right before `bl`) although
  its `lis r29` sits at the block top — sched2 in ours hoists it at once. Reference stores (`BitSet`),
  `G_ROOM_ID`, `pGS`, a shared `zero` local, int/local forms of the 0.0f argument all tried.
- Giv final value (loop.c) as a later loop's bound: an `addi r0, base, 0x58` right after an inner
  loop's exit (inside the outer loop's latch) plus `mr rB, r0` in the next loop's preheader is
  loop.c's `final_giv_value` of the inner loop's pointer giv, copied by cse2 into the later loop's
  `&node[2]` bound. record_giv only skips the "replaceable" shortcut when the giv register is
  mentioned *after* the loop (REGNO_LAST_UID beyond loop_end), so the pointer must be ONE
  function-scope variable shared by all the node loops (`EmTreeNode* n; ... n = &node[i];`);
  block-scoped `EmTreeNode* n = &node[i]` per loop gives a fresh `addi rB` instead. The later loop
  also needs the dead `if (i == 2) nx = node; else nx = &node[i + 1];` block: its `i == 2` compare
  makes loop.c place the bound in the preheader (without it `&node[2]` is rematerialised at the
  loop bottom, `where = insn` when threshold < insn_count). emtree/emwep/emshield/emmine R1_Fall.
- Tail-`Normalize` blocks: read the *frame offsets* of the three Vecs from the `lfs` of the mat
  stores, not the names — emshield's tail is emwep's (`Vec b, c, a; Cross(a,b,c); Cross(c,a,b);
  Normalize b, c, a`), not emtree's, and the `#line` numbers follow the real source.
- `u16 flags = p->flags; u32 fl = flags;` (promoted HImode load widened) gives `mr r11, r0`; an
  `int` load copied into a `u16` gives `clrlwi 16` (em_sub EmYarareDisp, still 1 word off: the
  original's copy is not cprop'ed into the later tests while ours propagates one of them).
- A `u8` member passed to two int-parameter inlines (`EmSetDieCk(em->emsetNo)`;
  `EmSetDieOn(em->emsetNo)`) is one QImode load + `mr r6, r9` copy with `clrlwi 24` at each use;
  a `u8 no = em->emsetNo` local is promoted and never masked (em_set EmSetDie).
- Zero-store block order (PenCloth setters): the dying zero store is the *last* zero statement in
  source and is issued first; put that member (`c->x54 = 0` in Em30ClothSet2) after `flags = 0`.
- A distance sum whose result register is the *variable's* callee-saved FPR (not the tied f13 of a
  dying operand) means the variable is assigned in two places: assign the player-distance check to
  the same `d` as the loop's check (emBarred emBarredNearCk).
- OPEN family, likely one compiler-build difference (candidate #8, FPR argument deaths): the
  original ranks the prologue copy `fmr fN, f1` of a float parameter *after* every GPR copy and
  store of the block (emwep setThrow `mr r26,r5; addi w; fmr f30,f1`, emshield setFall
  `mr r31,r4; stw pMotion; fmr f29,f1`, emwep setFall matched only because grav is the last
  parameter), i.e. as if f1 did not die there (weight +1); the same "FP arg register does not die"
  reading explains compiler-build difference 1 (`fmr f1, x` arg moves issued before `li`/`mr`
  int arg moves). No source form changes it; the SatMgrCreateF / atariInitF floats-first aliases
  remain the workaround at call sites (emBarred emBarredEatSet sub[0]/sub[1]).
- Dying-register tie-break not applied by the original (SetRock SOLVED 2026-09-10 with a #13 launder, see
  the sweep section; ShotArrow still OPEN): emrock SetRock's `stb r30, 0x95`
  (last use of the zero pseudo) stays in source order and `li r30, 0` is re-materialised after the
  next label; emwep emWep_R1_ShotArrow's `mr r3, part` (part dies) is issued *last* of the arg
  moves where ours puts a dying copy first. Both look like a spilled/REG_EQUIV pseudo reloaded
  per label region, but a shared `zero` variable is allocated a register (even a 9th callee-saved).
- Byte-compare tool of record: judge functions by masked words with intra-object branches resolved
  by *target symbol* (NgcAs emits REL14 for every conditional branch and REL24 for local `bl`s; a
  size change in an earlier function shifts every later displacement without a real diff), and
  accept the split object's raw `lis rX, 0x8023` words whose `lfs` lives in another block (dtk
  could not pair them; the linked bytes are identical).
- (esp/espgen water) `lwz r3, pLog` issued BEFORE the string `lis r6` in an error block = the block
  ends in a `return` (jump to the function's return label), not a `goto fail`/fallthrough into an
  else: `if (buf == NULL) { pLog->warn(...); return; }` with the rest un-nested (Espgen42/45
  TransSub), and for an `int` function whose fail paths share one `li r3,0`, the body is
  `if (SetWaterWork(...) != NULL) { ...; return 1; } return 0;` with `pLog->err(...); return 0;`
  in the early check (both `return 0`s cross-jump into the final out-of-line `li r3,0`;
  `goto fail; fail: return 0;` keeps the layout but schedules `lis` first). `nx = 0xB8;` written
  BEFORE `pLog->warn(...)` puts its `li r27, 0xb8` ahead of the call's `li r4/r5` (Espgen4x
  SetFreeWork, both 100%).
- Dead literal stores at 8-byte stride (`stfs f13, 0x8; stfs f0, 0x10; ...` never read, in
  Espgen42/45 Move00): four `Vec` locals whose `.x` and `.z` are assigned literals and never used
  (`Vec d0; d0.x = 1.0f; d0.z = 0.0f; ...` — Vec locals are 0x10-rounded, so x/z land 8 apart and
  .y is skipped); `const f32&` reference temporaries of an empty inline emit nothing.
- `addi r9, r1, N; stb r0, N(r1); psq_l fX, 0(r9), 1, qr2` (own 4-byte slot, not the fpmem one) is
  the inline-asm `PSQ_L_U8(&tmp)` on a function-scope `u8 tmp = noise[i];` (Espgen42/45 Move00;
  the compiler's own u8->f32 goes `stb/psq_l` through the shared fpmem slot).
- `sizeof(T) * (p->nx + 1) * (p->ny + 1)` (constant first, nx before ny) gives the target's
  `lhz ny; lhz nx; ...; slwi/mulli (ny+1); mullw (nx+1), that`: combine folds the constant into
  the SECOND factor and the shifted operand is loaded first; `(p->nx + 1) * 2 * p->ny * 12` for
  the display-list size. `u32 n` sized from `p->nx * p->ny` loads nx first only when written
  `p->ny * p->nx` (Espgen45 Move00 `idx`).
- A `k + 1` used twice and then `k++` (`p->nx + (k + 1)` stores, `k++` at the loop end) gives the
  target's `addi r7, r3, 1 ... mr r3, r7` (cse turns the increment into a copy of the shared
  pseudo); `k++` before the uses increments in place (SetWaterWork dl loops).
- Loop counters that must not be hoisted above the preceding calls (`li r28, 0` right before the
  first store, "Don't let it cross a call after scheduling if it doesn't already cross one") are
  variables that cross NO call: give each loop nest its own counter (`int i2`, `i3`, `i4`) — a
  function-level `i` reused in a loop with `fRand1_1()` crosses the call and its `li` floats to the
  block top. Register order then follows global.c pass 0 (`regs_used_so_far`): the loop-1 `i` gets
  r28 only because the disjoint loop-2 counter took r28 first.
- A static read inside a store loop that the target reloads every iteration (`lfs f0, g45_init_y`
  after `lwz p->pos`) is a reference read `FGet(g45_init_y)` (MEM with neither struct nor scalar
  flag stays below the in-struct `stfs`); `int base = p->ny * (p->nx + 1);` hoists the row offset
  into `mulli r11, r8, 0xc` + a stepping giv.
- OPEN (Espgen42/45 Move00 inner loop): the target keeps `k*4` (r31) and `k*12` (r29) as the only
  reduced givs and computes `c = cur + k*4` with an `add` per iteration (`c[-1]`, `c[1]`,
  `subf r9, r21, r7` for `c[-1-nx]`), while ours strength-reduces `&cur[k]` and both neighbours into
  stepping pointers (loop dump: giv 135 `cur + k*4` combined with the `cur[k]`/`c[±1]` DEST_ADDR
  givs and reduced). Pointer/byte-offset/`u32`/label-between forms all reduce; the target's
  `c` is either ignored (`lifetime * threshold * benefit < insn_count`) or not a giv
  (k4 `cant_derive`). Both Move00s are otherwise structurally aligned (registers p=r28 etc.).
- OPEN (esp0a Trans/SetFreeWork, esp0e Trans; candidate compiler-build difference): the implicit
  copy-assignment of a polymorphic object (`*base = *esp`, vptr at 0xF4 saved to a frame temp and
  restored) loads the saved vptr (`lwz r0, 0x8(r1)`) only AFTER the leftover block-copy stores
  in the original, so the copy temp is r0; ours hoists the `mem/f` (scalar) frame temp above the
  in-struct `stw` leftovers, the restore pseudo takes r0 across them and the copy temp gets
  r9/r10/r11 (t4-style `struct A { int x[61]; virtual void f(); }; *a = *b;` reproduces it in
  isolation). Only these two units have the pattern.
- `esp43`'s stray `.rodata` zero word is a dead-stripped static with one `x != 0.0f` compare
  (`Esp43_SetPos`, STRIP_UNUSED); `esp_app`'s 4-byte `.rodata` tail is 8-alignment end padding that the
  linker does NOT re-create (DOL sweep 12: `asm(".section .rodata; .balign 8")` at the end of the file).

## CRI middleware (`lib/adx_*`, `lib/sfd_*`, ... — CodeWarrior 2.4.7)

The CRI ADX/Sofdec libraries, the GCCI/MFCI CVFS interfaces and CRI's `UTY_*` helpers were prebuilt by
CRI with **Metrowerks CodeWarrior 2.4.7** (the `.rodata` build strings say `Append: MW2407
GC20Apr2004Patch1` = compiler 2.4.7 on the Apr 2004 patch 1 SDK; `GC/2.0`, `2.5`, `2.6`, `2.7` are all
2.4.7 and produce identical code on every unit tried; `GC/2.0` is configured). Flags (`cflags_mw_cri` in
configure.py, `CRI_LIBS` lists the units): `-O4,p -inline auto -sdata 0 -sdata2 0 -str readonly
-use_lmw_stmw on -char signed`, no small data at all. Evidence: `stwu r1,-0x10; mflr; stw r0,0x14`
prologue (1.2.5 emits `mflr; stw r0,4(r1); stwu`), `__div2i`/`__mod2i` runtime calls, `mr. r31,r3`,
`stmw/lmw` saves, every 4-byte global addressed `lis/addi`, float constants and strings in `.rodata`.
The ADX group was built Oct 8 2004, Sofdec Sep 22 2004. Headers: `src/lib/cri/` (`cri_xpt.h` types,
`sj.h` stream-joint interface). Workflow is the SDK one (`strip_unused.py --unit` post-build, so dead
functions must be written when their pools/statics survive). Bio4.sym `local` scopes are wrong for
many CRI functions (`SFMEM_ExecServer`, `MPVM2V_Finish`, ...): a `static` that another split object
imports makes ngcld exit 99 silently — bisect by swapping compiled objects for split ones in
`build/G4BE08/main.elf.rsp`.

MWCC idioms seen so far (2.4.7, -O4,p):
- Uninitialised file-scope data (globals and statics) is emitted in order of **first reference** in
  the code, not declaration; `= 0`-initialised scalars still go to `.bss` but at their declaration.
  An original `.bss` order that no live function produces means a dead function referenced them
  first (adx_bahx `ADXB_EntryAhxFunc`, adx_insh `ADXT_GetDmyBuf`).
- Zero-initialised aggregates (`= {0}`) go to `.data`; a zero scalar in `.data` needs
  `#pragma explicit_zero_data on` (gcci_sub). An unreferenced pointer to a dead-stripped function
  is left as a zero word in `.data` (cft_common).
- Statics of one section are addressed through the section pool symbol (`...bss.0`,
  `lis/addi` once, then `lwz off(rBase)`); a global gets `lis/lwz sym@l`. A `volatile` scalar is
  re-read after every store (`lwz` twice); a bare `x;` statement of a volatile is a real load.
- The version string is kept alive by `static const Char8 *const volatile xxx_build = "..."` read
  as a bare statement at the top of the init function (dead `lwz r0,0x3c(rPool)`).
- Float constant pools are per function, emitted in function order; inside one function the order is
  not use order (split constants over dead functions to reproduce a pool).
- `if (x >= 0) return A; return A+1;` folds into `srwi/subi/add` arithmetic; the order of the `-1`
  and the `lis` depends on which value the first branch returns (muldiv).
- `Sint32 sz = sizeof(Sint64); if (sz < 8) for (;;) {}` is NOT folded (`li r0,8; cmpwi r0,8; bge`)
  — CRI's compile-time check idiom (cmptime).
- `ret = 1; else ret = 0; return ret;` keeps the branches; `if (c) return 1; return 0;` becomes
  `neg/subfic` flag arithmetic.
- Loops: `cnt = n + 1; while (--cnt) {...}` gives `addi; b check; body; check: subic.; bne` (the
  UTY_Memcpy/MemsetDword loops); `while (cnt--)` / `for` become `mtctr/bdnz` and are unrolled 8x
  at -O4. `for (i...) { p = &arr[i]; ... }` gives the direct `addi r31,r3,arr@l` induction pointer;
  `p = arr; for (...; p++)` copies it through r0 (`addi r0; mr r31,r0`).
- `if (a == NULL || n <= 0) return;` gives `beq end; cmpwi; bgt body; b end`; a separate
  `if (n > 0)` gives `ble end`.
- 64-bit compares: `x <= y` is `xoris/subfc/subfe/subfe/neg.`; `min = ts->min; if (t < min) min = t;
  ts->min = min;` (if with a local) and `ts->max = (t > ts->max) ? t : ts->max` (ternary) give the
  two branch shapes of sfd_tmr.
- Locals: later declarations get lower frame offsets (sfx_set `inf`/`out`, adx_insh). Callee-saved
  registers go r31 downward in declaration/parameter order; a pointer parameter copied to a local
  (`Uint8 *p = dat`) is allocated after the other parameters (sud_lib).
- 8-byte-aligned structs are copied with `lfd/stfd` pairs, 4-byte-aligned ones with `lwz/stw`
  (mps_get).
- Error returns: `b end` straight after a `bl SFLIB_SetErr` with no `li r3` = `return SFLIB_SetErr(..)`
  (the callee's result is returned); `beqlr` after a NULL test = `return p;` not `return NULL;`.
- A `switch` with one live case and `beq case; bge default; b default` has a second case label that
  shares the default body (`case 2: default:`); `beq case; b default` is a lone case.
- A `mtctr n; loop; ... li r3,0` search with an early exit to the code after it is an inlined
  `static` helper `for (...) { if (p->used == 0) return p; p++; } return NULL;`.
- 32-byte struct copies are unrolled `lwz/stw` pairs, 128-byte ones become a `mtctr 16` loop of
  `lwz/lwzu/stw/stwu` with both pointers pre-decremented by 4 (`subi r5,rDst,4`).
- Division by a constant: `mulhw M; add; srawi s` with a "negative" magic means the unsigned magic
  M gives d = 2^(32+s)/M (mpv_get: 0x91A2B3C5, s=10 → /1800).
- `crclr cr1eq` before a `bl` = the callee is variadic (`MWSFSVM_Error(const Char8 *fmt, ...)`).
- Register order of callee-saved variables is neither declaration nor first-use order (mpv_frm
  `MPV_SkipFrmSj`: original mpv=r31, code=r30, sj=r29, ours r29/r31/r30; sfd_uo `SFUO_Create`
  reuses the `uo` register as the stepping induction pointer; sfx_alp `SFXA_Create` constant
  registers) — OPEN, all statement permutations of SFXA_Create's init block were brute-forced.
  (mpv_frm resolved in CRI pass 5: asm-defined `register` copy of the first parameter.)
- OPEN (mpv_cmc `MPVCMC_InitMcOiRt/InitObj`): the original keeps `addi r5,r3,0x124` as a separate
  base for six `stw off(r5)` stores into a member array while every source form tried (pointer local,
  loops, casts, volatile, inline helper) folds the offsets into `r3`.

### SN libsn / ProDG runtime units (`lib/dummy`, `tealeaf`, `FSasync`, `sndvd`, `fileserver`, `crt0`, ...)

`dummy` (stdio syscall stubs), `sndvd` and `FSasync` are GCC 2.95 -O2 like the game; `tealeaf`
(`__cvt_fp2unsigned`, the MWCC-ABI `__va_arg`, `__div2i`-style aliases that `b` to libgcc) is
hand-written assembly (its libsn.a member carries a `tea151.tmp` FILE symbol like proview/ppcdown,
the C members carry `<name>.c`; `addi r7,r3,0`, `subi/nor` for `~(n-1)`, two zero registers) and is
an `ASM_UNITS` entry. The v393 `libsn.a` in re4-orig/prodg has every libsn member with full symbol
names (sndvd's `NotDvdDsi` label, its `g_hDVD` etc.) (`stwu -8; mflr; stw r0,0xc`, `stmw`,
`first.183` statics, r9/r11 temporaries) but with **no small data** and **no common symbols**
(`LIBSN_UNITS` in configure.py: `cflags_game + -G 0 -fno-common`, `strip_unused.py --gcc`; without
`-fno-common` FSasync's uninitialised globals become COMMON and leave the unit's `.bss`). `proview`,
`ppcdown`, `fileserver` (`addi r31,r4,0` copies, `lis rX,sym@h; ori rX,rX,sym@l`), `eabi`
(`_savefpr_14/_restfpr_14`) and `__start` (`.init`) are hand-written assembly and are built from
**`src/lib/<name>.s`** (configure.py `ASM_UNITS`: the split name stays `lib/<name>.c`, the Object's
`source` is the `.s`, so tools/project.py's `asm_build` uses the template's `as` rule =
`build/binutils/powerpc-eabi-as -mgekko --strip-local-absolute -I include -I build/G4BE08/include
--defsym BUILD_VERSION=0` + `dtk elf fixup`; `macros.inc` provides `.fn/.endfn/.obj`). The sources
are the dtk disassembly with the address comments stripped, `bl`/`b` to global function starts made
symbolic (relocations resolve to the same displacement), `_stack_addr/_SDA_BASE_/_SDA2_BASE_` for the
`.init` register setup, and one fix: dtk prints `ori r0,r0,imm` as `nop`, so proview's
`lis r0,sym@h; nop` was really `ori r0,r0,sym@l` (the DOL check catches it: 2 bytes). `__start` is an
absolute symbol of ldscript.ld, the `.init` code carries the local label `__start_entry`. Branches
into data (`proviewtty`) or into the middle of other units' functions stay raw displacements with a
`# -> 0x8... (sym+off)` comment; objdiff shows ARG_MISMATCH on assembler-resolved local branches, the
linked bytes are identical. `crt0` is only the data half of that assembly (two 32-byte message
buffers, the libsn version words, `LinkFiddle = {__mod2i, 0}`); `crtbegin` is `.ctor/.dtor` `-1`
sentinels; `builtin-delete` is SN's libstdc++ `operator new/delete` warning unit (C++, everything
stripped but the `bad_alloc` type-info name and the four warning strings). The split object's `.data`
alignment (dtk reports 2**3) is what the DOL layout needs: a GCC object with a 4-aligned `.data`
shifts every following `.data` unit by 4 (dummy: `asm(".section .data\n\t.balign 8\n\t.section .text")`).

FSasync (matched) idioms, GCC 2.95 -O2 -G 0:
- Every reload of a global after a store to it (`stw; lwz; cmpwi`) and re-reads inside one expression
  = `volatile` globals (the EXI2 transfer state). A volatile store never moves above a volatile load
  (`cb = g_FSCBFunc; g_nRWasyncPhase = 0; if (cb)` keeps `lwz` before `stw`); a plain load can.
- `g_nBlockCnt--; while (g_nBlockCnt != -1) {...}` gives separate `lis sym@ha` pseudos per block;
  `for (;;) { g_nBlockCnt--; if (g_nBlockCnt == -1) break; ... }` (exit test duplicated by jump.c)
  shares one `lis` between the pre-loop copy and the body and stops loop.c hoisting it out of the
  outer loop — the target's `lis r30,g_nBlockCnt@ha` at the top of each outer iteration.
- `li r0,0x10; slwi r0,r0,8` (an unfolded constant) is a single-use local (`u16 hlen = 0x10;`) set
  before a loop and used after it: cse cannot fold across the loop and update_equiv_regs moves the
  `li` next to the use.
- A struct whose address is taken before a loop (`struct FSResult *res = &g_FsResult;`) keeps
  `lis/addi` in a callee-saved register across the calls; `&g_FsResult` at each use rematerialises
  the `addi`. A DMA target declared `__attribute__((aligned(32)))` gives the 4-byte `.bss` gap before
  it and the 0x20 rounding after it.
- Raw hardware addresses (`*(volatile u32 *)0xCC003000`) are `lis/ori`; a clear-byte loop
  `for (i...) *p++ = 0` is `mtctr; stb; addi; bdnz` (memset would be a libcall).

sndvd (matched) idioms: the DABR write goes through an `"m"` asm operand (`asm volatile("lwz 3,%0; mtspr
1013,3; isync" :: "m"(dabr))`: stack slot + hard r3); the DSI exception entry is one top-level `asm`
block with `.type/.size` (its trailing `li r3..r6,0; bl DSIHandler` is part of the asm); the DI
register copy loop reads `*(volatile u32 *)((0x0C006000 + i * 4) | 0xC0000000)` (physical address
OR'd with the uncached base inside the loop -> `oris` per iteration); `asm(".long 1")` after
`OSReport` is the debugger trap word; `switch (cmd)` with `int cmd` for `cmpw`; store order of six
globals found by brute force (`perm.py` over the statement order); `asm volatile("")` after the
default case's `ForceDvdDeIrq()` blocks the single-insn tail cross-jump (COMPILER-DIFF #6). OPEN:
`ctx` (7 refs / 153 insns) ranks below `pos` (3 refs / 29 insns) in global.c's allocno priority and
gets r28 where the original has r29; a dead `asm volatile("" :: "r"(ctx))` supplies the 8th
reference.

SDK/CRI MWCC register-allocation levers found on reverb_std, svm and ax_rna (MWCC 1.2.5n / 2.4.7):
- reverb_std `ReverbSTDCreate`: `max_length << 2` (not `* 4`) in the inlined `DLcreate` decides whether
  `rv` gets r31 or r23 (same code otherwise); the identical `* 4` form matched in reverb_hi.
- Callee-saved registers of a function's own locals: the *last* declared gets the highest register
  (svm_exec_svr: `p; i; ret` -> ret r28, i r27, p r26); strength-reduced loop pointers normally take
  the registers above the locals (SetOutVol `ptr r31, v r30, i r29`), but a loop living in an
  *inlined static helper* puts the helper's locals above the pointers (AXRNA_ExecHndl: n r30, i r29,
  pointers r28..r26) and the helper's aggregate locals get frame slots in declaration order upward.
- A loop-invariant expression written from a *block-local* copy (`Uint32 f = adj;` inside the `if`)
  is not hoisted out of the loop; written from the function-level variable it is hoisted.
- Two independent `srawi ..,16` of the same value = two locals initialised from the same field
  (`loop = rna->buf[i]; cur = rna->buf[i];`), the loads are CSE'd, the shifts are not.
- `hist[pos++] = v` (post-increment in the index) vs `hist[pos] = v; pos++` swaps the temporaries of
  `pos+1` and `pos*4`; a separate `nbyte = diff * 2` local before a loop moves the loop counter above
  the hoisted product.
- `if ((p->x = f()) == NULL)` tests r3 straight after the call (`cmplwi r3,0; stw r3`); a separate
  `if (p->x == NULL)` reloads. `p->y = f(); if (p->y == NULL)` was used for the SJRBF_Create case.
- Volatile file-scope counters (`svm_lock_level--; if (svm_lock_level == 0)`) reload after the
  store; the error-callback pair `{func, obj}` is a plain struct: `lwz r12,off(rBase)` for `.func`
  but `addi r3,rBase,off; lwz r3,4(r3)` for `.obj` (member at offset 4 of a pooled static).
- An unrolled `stw 0(r6) .. 0x14(r6)` clear through the array's address = `p = arr; for (...) *p++ = 0;`;
  with `arr[i] = 0` the first store folds into the pool base.
- `.bss` first-reference order and `@N` string order need the dead functions written (svm:
  `svm_itoa` with `static Char8 buf[32]`, `SVM_SetCbWaitVsync`, `SVM_SetCbTestAndSet`, `SVM_SetCbLock`,
  `SVM_SetCbGotoSvrBorder`, `SVM_GetNumCbSvr`, `SVM_ExecSvrFuncId`, `SVM_ItoA2`; ax_rna: `AXRNA_DbgDump`
  with a local `const Char8 *sw_str[2] = {"OFF", "ON "}` -> the anonymous `@N` pointer table in
  `.rodata`, and the public getters/setters that are only ever inlined: `AXRNA_GetPlaySw/GetTransSw/
  SetSrcType`). `= 0`-initialised scalars keep declaration order.
- MWCC inlines *public* (non-static) functions defined earlier in the file at -O4 (`AXRNA_SetOutPan`,
  `AXRNA_SetSfreq`, `AXRNA_Destroy` appear inline in `AXRNA_Create`/`AXRNA_Finish` with their
  `rna == NULL` checks kept); `SVM_CallErr1` inlined everywhere but emitted after its users =
  static helper `svm_call_err1` + public wrapper.

- Zero-copy chains `li rA,0; mr rB,rA; mr rC,rA` come from *inlined* code: a zero init inside an
  inlined static helper (or its locals initialised at declaration) copies a zero the caller already
  holds; two zero inits in one non-inlined function give separate `li`.
- A search loop whose index is in r3 and pointer in r4 is an inlined `static Sint32 search(void)`
  returning the index; the direct `for` gives pointer r3 / index r4.
- MWCC never hoists a load above an earlier store to memory, whatever the types: a `lwz` between two
  stores means the source read it into a local at that point.
- Callee-saved registers go r31 downward in *first definition* order; a value defined in an inlined
  helper is allocated after the caller's live locals.
- `-O4` unrolling: `for (i = 0; i < n; i++)` with `i` used in the body -> `subi 8/addi 7/srwi 3` form +
  remainder compare; `while (n-- > 0)` -> `srwi. n,3; mtctr; ...; andi. n,7`.
- Float constant pool entries are emitted before the function's strings; a stray 4-byte zero word in
  `.rodata` between strings is a `0.0f` in a dead function.

- `-fp_contract on` is required in `cflags_mw_cri` (`-fp fmadd` alone does not contract in 2.4.7).
- Callee-saved order = declaration order, first declared -> highest register, after compiler
  temporaries (strength-reduced pointers) which take r31/r30 first.
- `long`/`Sint32` loop counters keep the entry guard and reload constants; `int` counters get it
  folded. Loops <= 32 fully unroll; 64 and 192 don't (a 192-store clear is six macro loops of 32).
- Functions emitted after their inlined uses are separate static helpers at the top plus a public
  wrapper later; a standalone `if (c) return -1; return 0;` becomes `subfic/nor/srawi`, the inlined
  copy keeps branches.
- `.bss` first-reference order includes stripped functions and static helper bodies (dead getters
  are needed to place variables).
- OPEN (MWCC): anonymous float-literal pooling — our 2.4.7 pools >=3 literals of a function through a
  `...rodata.0` base; the original never pools literal-only functions but does pool literals together
  with strings. No flag reproduces it (all GC builds and -O levels tested).

- Right operand of a commutative `|`/`+` is evaluated first (`a | b` -> `b` computed, `a` inserted
  with `rlwimi`; `f(0) + f(1)` calls `f(1)` first).
- `x = LE32(p); x = SWAP32(x);` as two statements gives the `mr` copy before the last `rlwimi`;
  one expression gives no copy. A masked byte-swap stored through a `Uint16*` becomes `sthbrx`,
  the unmasked form stays `srawi/rlwimi`.
- `(Uint8)inbuf[i] << 8` with `Sint8 *inbuf` gives `clrlslwi 24,8`; `Uint8 *` gives plain `slwi`.
- `return f() != 1;` -> `subfic/subi/or/srwi 31`; `return f() == 1` -> `cntlzw/srwi 5`.
- Strings of a local static defined inside an inlined accessor at the top of the file come first in
  `.rodata` and reload the pointer per iteration.
- `#pragma dont_inline on/off` around a static definition stops auto-inlining of it (OPEN: original
  did not inline `mwsfd_ExecSvrHndl`/`MWSFSVR_DecodeServer` while inlining smaller helpers; no
  `-inline` level reproduces it).
- Stack slot order: first-declared aggregate local gets the highest frame offset.

- `-inline auto,deferred` (via `CRI_CFLAG_OVERRIDES`) allows inlining of functions defined later and
  emits functions in reverse source order; units whose accessors inline a later helper (mwsfdset,
  adx_fs) are written in reverse order with the override. `.rodata` string order = codegen order.
- `#pragma dont_inline on/off` around the *caller* stops a big static from being inlined while it
  keeps its own body.
- Same-lifetime temporaries take volatile registers in declaration order (first declared -> lowest);
  brute-forcing declaration permutations is cheap and fixed several sfd_mpvf functions.
- `p = base; p += n; p -= 8;` as three statements keeps `add; subi`; one expression reassociates.
- A pointer local used with both a constant and a variable index materialises a base register with the
  constant use folded into the parent pointer.
- OPEN (MWCC): member-address kept in a callee-saved reg across a call with one use; pooled strings in
  reverse use order; dead `b end` after an empty `case N: break;`; static-function literal placement;
  callee-saved order of parameters not by declaration/lifetime/use count.

- Search loops with an early exit written as `beq next; b found` (a redundant unconditional after the
  last `&&` test) plus `li rX,0` on the fall-through are an inlined `static` helper with
  `for (...) { if (A && B) return id; } return 0;` (sfd_hds `sfhds_SearchStmId`); the direct loop gives
  `bne found`.
- `x = (p[0] << 8) | p[1]; x <<= 8; x |= p[2]; x <<= 8; x |= p[3];` gives `rlwimi` for the first pair
  and `slwi/or` for the rest; one expression or `x = (x << 8) | p[n]` chains give all-`rlwimi`;
  `<<=`/`|=` from the start give all-`slwi/or`.
- A pointer derived in two steps (`SFHDS_FHD *fhd = &sfd->fhd; SFHDS_VID *vid = &fhd->vid;`) keeps
  `addi rX,rBase,ofs` as a live base register; `&sfd->fhd.vid` in one step is folded into the loads.
- Ternary arm order: `f(&tmp) == 0 ? -1 : tmp` gives `bne ok; li -1; b; ok: lwz`; `f(&tmp) ? tmp : -1`
  gives `beq`. `(A && B) ? C : 0` becomes branchless `neg/or/srawi/and`; an if/else into a local keeps
  the branches.
- `if (a <= 0 || p == NULL) return;` gives `bne body; b end`; `else if (a > 0 && p != NULL) {body}`
  gives a plain `beq end`.
- Integer `add` operand order follows the source (`add rD, rLeft, rRight`); pointer arithmetic is
  canonicalised (pointer first) and reassociated (`(buf + ofs) + n` -> `buf + (ofs + n)`). A target
  `add r3,rOfs,rBuf; add r3,rN,r3` is `(Uint32)ofs + (Uint32)buf` then `n + that` as two statements.
- A struct field read once into a local and used across blocks vs. re-read `sj->bsize` at each use
  changes volatile-register numbering (CSE keeps the reload in the same register anyway); when the
  target's temporaries look "reversed", drop the local.
- A `Sint32` function result reused as the return value (`nbyte = 0; ...; return nbyte;`) keeps the
  parameter's register for the result (sj_rbf `SJRBF_IsGetChunk`).
- `#pragma dont_inline on/off` around a public function stops it being inlined into later functions
  (sfd_hds `SFHDS_ProcessHdr`/`sfhds_SetHdrRaw` are called, `SFHDS_IsSfdHeader` is inlined) but
  also stops static helpers being inlined *into* it: use a macro for those.
- MWCC 2.4.7 has no `__dcbi/__dcbz_l/__mfspr` intrinsics (they become calls); `asm { dcbi p, i }`
  with `register` operands in a `for (i = 0; i < N; i += 0x20)` loop is unrolled 6x (156 = 6*26
  iterations for 0x1380 bytes) exactly like the original; `asm { mfspr r0, 920 ; stw r0, hid2 }` gives
  the target's `mfspr/stw/lwz` through a stack slot (mpv_lib).
- A parameter needed in an `asm` block as `register` in the *prologue-copied* form: `register MPV p =
  mpv;` as the first local (all uses through `p`) keeps `lis` of an inlined store above the `mr.`
  copy; `register` on the parameter itself does not (mpv_lib `MPV_Destroy`).
- Counted handle loops (`mtctr nhn ... bdnz`) need the count and base copied to locals before the loop
  (`n = wk->nhn; p = wk->hn;`); indexing the struct members directly reloads them per iteration. A
  second such loop in the same function used fresh block-scoped locals (volatile registers) rather
  than the callee-saved ones (mpv_lib `MPV_Init`).
- A callee that ignores its arguments still receives them: `MPVM2V_SetCond(mpv, id, val)` keeps r3
  live so the `cond` pointer takes r6, not r3.
- Split objects' `.bss` (and `.rodata`) are padded to their alignment by dtk (sj_mem 0x484 vs 0x488,
  mpv_lib 0xAE vs 0xB0): a 4-byte/2-byte trailing `lbl_` gap needs no dummy variable.
- OPEN (sfd_hds `sfhds_DoProcessHdr`/`SFHDS_SetHdr`): the original allocates callee-saved registers as
  params (reverse order, r31 down) then locals (`fhd r31, sfh r30, ver r29`; `result r30, len r29,
  p r28, sfd r27`); ours gives locals first (`ver r31, fhd r30, sfh r29`) or params last. Inlined
  helper values, block scoping, `register`, statement order and a dozen structural variants tried.

- Buffer-table access `sfd->buf[n].field` folds the table base into the displacement; to keep a pointer
  across calls with the folded form use a shifted view type (`struct { Uint8 pad[0x1308]; SFBUF_WORK w; }`).
- A 64-bit result assembled by statements (`hi <<= 32; hi |= pos; return hi;`) frees the argument
  register for the intermediate; the single expression takes a fresh register.
- `static Bool f() { if (a == b) return 1; return 0; }` inlined keeps `subf/cntlzw/srwi.`;
  `return a == b;` inlined folds into `cmplw/bne`.
- MWCC addresses all globals it defines in one .bss via one base (`sym@ha` + offsets).
- A local `Char8 hdr[] = "..."` is a `mtctr` word-copy loop at its declaration point.
- MPEG field extraction: explicit masks `(b >> 4) & 0xF` give `extrwi`/`rlwimi`; unmasked gives `srawi`.
- `for (;;) { if (f()) goto found; ...; if (i >= 3) break; i++; } goto done; found: ...; done:`
  reproduces the found block after the loop with both exits jumping past it.
- OPEN (MWCC, blocking ~60 units): callee-saved/volatile register priority is a computed ranking, not
  declaration or first-use order; brute force of declaration/statement/scope/`register`/types does
  not move it. Also OPEN: `&wk->u.ring` kept in a callee-saved reg; ternary/if-else diamond sunk to its
  use; `adr[8]` kept on the stack; `bne body; b end` after `mr.`.

### MWCC compiler-build differences (sweep result, do not brute-force further)
A full sweep (every GC/1.0..3.0a5.2 and Wii mwcceppc build, every documented and hidden 2.4.7 option
and pragma singly and in 66 pairs, against 7 near-miss functions with identical instruction streams,
plus a 55-function regression set) found NO configuration that moves any of them; all 2.4.x builds
(GC/1.3.2 .. 2.7) emit byte-identical `.text`, and every deviation from `cflags_mw_cri` regresses matched
units. `cflags_mw_cri`/`MWCC_CRI_VERSION` are the unique optimum. Treat these as compiler-build
differences (the original used a 2.4.7 build we do not have) and accept 88-99.9% on the affected
functions instead of more source permutations:
- M1 register ranking: callee-saved order (parameters reverse-order above locals) and the volatile
  temporary preference (e.g. `SFBUF_RingGetRead`: target gives the zero constant the freed r5 and the
  `mulli` result r0; ours zero=r0 and reuses the dying operand in place).
- M2 float-literal pooling: ours pools whenever a function references >=3 distinct `.rodata` objects;
  the original applies that rule to strings but not consistently to float literals.
- M3 auto-inlining decisions for mid-size helpers (mwsfdsvr).
Write the remaining CRI units for source completeness; flag only what matches.
- M4 byte-swap store: our 2.4.7 folds any dead `store(bswap32(x))` into `stwbrx` regardless of
  spelling (only `nopeephole` stops it, which breaks `rlwimi` merging); the Sofdec originals keep
  `rlwinm/rlwimi x3/stw` (ADX originals do use `stwbrx`). Accept 16 bytes/function.
- M5 shift forwarding (Sofdec, mpvabdec): our 2.4.7 forwards a shift definition of a local into
  every rlwinm-foldable use of the same basic block even when the variable is materialised anyway:
  `code <<= 1; n = (code >> 24) & 0xFF` -> `extrwi n,code_old,8,1` + `slwi code,code_old,1`, and
  `x = (Uint32)code >> 22; tbl[x >> 1]; ... x & 1` -> two `rlwinm` from `code` plus `srwi x`. The
  original keeps the materialised register: `slwi r0,r0,1; srwi. r11,r0,24` and `srwi x; clrrwi;
  clrlwi`. No `-O` level, `-opt` keyword or `#pragma` (opt_propagation, peephole, opt_common_subs,
  optimization_level...) reproduces it; ours stops forwarding only when the first use in the block
  is not foldable (a compare/store) or the definition is conditional (`c ? x << 1 : x`). Verified
  by inserting a dummy compare after the shift: the register assignment of the whole prologue then
  matches, so the register residue around such sites is M5-caused, not M1. Costs 1-2 instructions
  and a register renumbering per site (mpvabdec: 6 sites x 3 functions).

- Inlined helper locals are laid out first-declared -> lowest frame offset (reverse of a function's
  own locals); each inlined call gets a block below the previous one.
- `static inline` forces inlining of a helper too large for `-inline auto` (a source-level fix for
  what looks like M3).
- A variable with two definitions gets `mr r0,r3; ...; mr rX,r0` for a call result; single-definition
  variables get a direct `mr`. `y = x` between two live variables stays `mr`; a fresh one folds to `li`.
- Integer add chains reassociate (first addend added last); separate statements keep source order.
- `const T *` parameters let MWCC CSE loads across stores through another pointer; a reloading
  original means non-const parameters.
- Volatile FPRs are assigned in declaration order (first declared -> lowest).
- `lbz` without `extsb` before `cmpwi K` on a `Sint8` field is a single-use `== K` compare.

- `-inline auto,deferred`: `.text` reverse source order, `.bss` reverse declaration order, `.rodata`
  initialised objects in declaration order, float pools in codegen order.
- Table-fill loops: n <= 8 unrolled with no guard; 9..32 unrolled behind a `li 0; cmpwi n; bge` guard;
  > 32 as 32-store `mtctr` loops; a body containing a call-free inner fill loop is not unrolled.
- Constants substituted by the unroller are not refolded (`li r0,4; ori r0,r0,0x700`).
- FP locals get f31 downwards in declaration order; the loop counter declared last takes the lowest
  callee-saved GPR.
- Address-taken `sscanf` outputs are separate scalars with stack slots in declaration order top-down.
- Paired-single inline asm needs the scheduler ON to reproduce swapped adjacent pairs; non-PS asm
  bodies need `#pragma scheduling off`. Inline asm cannot use compiler pool constants: declare
  `static const Float32 x` and use `x@ha`/`x@l`; GQR operands must be numeric.

mpvabdec (`MPVABDEC_NintraBlock/IntraBlock/IntraBlockDc11`, 99.3/99.6/99.6%, residue M5 + M1;
the three 0x400 `.data` tables are 256-entry `switch` jump tables, one per function):
- A dense `switch` (256 cases on `(code >> 24) & 0xFF`) is a `.data` jump table (`@N`, one
  `.rel` per value) with `srwi 24; cmplwi 0xff; bgt after_switch; slwi 2; lwzx; mtctr; bctr`; a
  sparse one (`0, 1, 2-3, 4-7, default`) is a compare tree. Case bodies are emitted in source order
  (the original: coefficient cases 0xFF down to 0x00, then the EOB-terminated cases 0xFE down,
  `default:` first in the sparse switch); `continue` cases branch to the loop head, `break` cases and
  the out-of-range test fall to the code after the loop.
- Struct fields are reloaded after every store (`prm->idx = *++zz; ... prm->iqm[prm->idx]` gives
  `stw; lwz`); several reads in one expression share the reload. `prm->idx = prm->idx0 = v` stores
  `idx0` first.
- `p[1]` then `p += 2` in one block gives `lbz 1(p); lbzu 2(p)` (the increment is deferred into the
  next load); when `p` is dead afterwards the second load is a plain `lbz`.
- `(Float32)i` is the `xoris 0x8000; stw; lfd; fsubs` double trick with the 0x43300000 word stored
  per conversion; two conversions in one block use two stack slots with both high-word stores
  hoisted to the top.
- `Sint32 code` with `(code >> n) & mask` gives the `rlwinm` forms (`srwi`/`extrwi`) and
  `(code << 8) < 0` gives `slwi.; bge`; `(Uint32)code >> n` where no mask is wanted (`srwi x` kept
  as a variable). A `Uint32` look-ahead folds everything into `rlwinm` chains.
- `(prm->level << 1) * prm->qscale` evaluates `qscale` into the first temporary (target
  `lwz r11 qscale; slwi r12 level; mullw r11, r12, r11`); `prm->level * 2` evaluates `level` first.
- `zz = tbl + run` as one expression computes into a temporary and copies (`add r5; lbz; mr r6,r5`);
  `zz = tbl; zz += run;` defines `zz` directly. The operand order of the loop's `add` for
  `zz += prm->run` is NOT spelling dependent (`zz += run`, `zz = run + zz`, `&zz[run]` are
  identical); a pre-loop `zz = tbl; zz += prm->run;` made the loop adds `add zz, run, zz` (target),
  functions without it keep `add zz, zz, run` (OPEN, 5 sites in IntraBlock/Dc11).
- A block clear through a pointer field (`prm->dst[i] = 0.0`) reloads the pointer per store (the
  `Float64` store aliases the field); copy it to a local first.
- Volatile registers of the loop-invariant locals follow declaration order (first -> lowest) but
  a value that is a copy of another variable (`code = bbuf`) is ranked last whatever its position
  (Dc11: target `code r6 ... bbuf r10`, ours `bbuf r6 ... code r10`, M1). The three functions
  needed three declaration orders (Nintra `code, zz, bbuf, nbuf, bitpos, ptr`; Intra
  `bbuf, nbuf, bitpos, ptr, zz, code`; Dc11 `bbuf, nbuf, bitpos, ptr, code, zz`).
- Layouts: `MPV_BLKPRM` 0x00 run, 0x04 level, 0x08 sign, 0x0C len, 0x10 idx0, 0x14 idx (the
  return value: idx, negated when more than one coefficient was stored); `mpv->scale_tbl` is
  `Float32[64]`, `mpv->bitmsk_tbl` is read as `Sint16` (`lhax`), `prm->dctbl` as `Uint8`
  (`(size << 4) | len`); `rl_8` entries are `(len << 16) | (level << 8) | run`, the 11..17-bit
  tables `(level << 8) | run` halfwords (mpv_vlc.c's `RL(len, a, b)` arguments are really
  `(len, level, run)`); D pictures (`picatr.pic_type == 4`) skip the AC loop in IntraBlock only.

#### CRI compiler identification (2026-09-11)
Research pass (pre-pass-6 pure-C sources from commit d996e7c, per-function `.text` byte compare with
relocation fields masked; harness deleted). Findings that supersede parts of the sweep note above:
- Both CRI libraries carry the same compiler tag: `Append: MW2407 GC20Apr2004Patch1` in adx_inis
  (ADX group, `Build:Oct  8 2004 13:3x`), dct_ver/mps_lib/mpv_lib/sfd_lib/mwsfdlib (Sofdec group,
  `Build:Sep 22 2004 10:34-10:35`), i.e. mwcceppc 2.4.7 on the 20Apr2004 patch 1 SDK for both. The
  per-library-compiler hypothesis is refuted: every GC (1.0..3.0a5.2) and Wii (1.0RC1..1.7) build was
  run over 20 ADX units (285 target functions) and 20 Sofdec units (261): GC/1.3.2..2.7 give
  267/285 and 212/261, every other build only loses (GC/1.3: 230/199; GC/3.0a*: 65/61; Wii: 64/56;
  1.0-1.2.5: 56/40); no build gains a single function in either library. 50 flag variants
  (`-O4`/`-O3,p`/`-O4,s`/`-O2,p`, `-opt` keywords, every `-inline` mode, `-proc 750/generic`,
  `-fp fmadd`, `-fp_contract off`, `-rostr`, `-str pool/noreuse`, `-use_lmw_stmw off`, `-common`,
  `-char unsigned`, `-enum min`, `-align powerpc`, `-func_align 16`, `-pool off`, `-schedule`,
  `-sdata 8`, `-lang c++`, ...) likewise gain nothing (`-pool off`: +1 −35, see M2 below). X360 is
  MSVC (cl for PowerPC), not applicable.
- The 2.4.7 builds are NOT all identical: over the 138 CRI units GC/2.5, 2.6 and 2.7 (2.4.7 build
  105/107/108, exe dates Feb 2003/Jul 2003/Jul 2004) differ from GC/2.0 (build 92, Sep 2002) in two
  units, mwsfdcre `mwsfcre_CreateSfd` and sfd_tst `SFTST_Create`: the trailing word of an 8-byte
  struct copy after a `lwz/lwzu/stw/stwu` loop is `lwz r0,4(r4); stw r0,4(r5)` in build >= 105 and
  `lwz r3,4(r4); ...; stw r3,4(r5)` in build 92. The target has the build >= 105 form in both
  (SFTST_Create becomes byte-identical under GC/2.5+, the sweep's "M1: `lwz sftst_debout_buf`
  scheduled above the hdr copy tail" is this build difference), everything else is unchanged
  (ADX 379/410 functions identical either way, Sofdec 587 -> 588/723). The original compiler is a
  2.4.7 build >= 105; recommended `MWCC_CRI_VERSION = "GC/2.7"` (mk-deception uses GC/2.7 for the
  same libraries), strictly non-regressing.
- External evidence: github.com/ShulkMaster/mk-deception (Mortal Kombat: Deception, GQNE5D) ships
  the same libraries one release earlier (`CRI DCT/GC Ver.1.932 Build:Sep  3 2004`, `MPV 1.933`,
  `SFD 1.940`, `MWSFD 3.31`, `ADXT 9.28`, `ADXF 7.17`, same `Append: MW2407 GC20Apr2004Patch1`),
  compiled with `mw_version GC/2.7`, `-O4,p -inline auto -fp hardware -fp_contract on -str reuse
  -align powerpc -enum int -sdata 0 -sdata2 0` plus per-unit `-use_lmw_stmw on` / `-inline noauto` /
  `-str reuse,readonly`; 51 of 135 CRI units Matching. Their symbol map also has no `...rodata.0`
  for dct_ac (same M2 behaviour in a different build of the library). Compiling their sources
  against OUR split objects with `cflags_mw_cri` (GC/2.0) gives byte-identical functions where ours
  had "compiler" residues: mpvabdec 3/3 (ours 0/3: M5 is a source shape — their bit reader is a
  two-word `bit_buffer`/`next_buffer`/`bit_count` model, `peek = bit_buffer | next_buffer >> (32 -
  bit_count)`, `MPV_FINISH_FROM`, not `code <<= 1; n = (code >> 24) & 0xFF`), mwsfdply 10/10
  (`MWSFPLY_SetFlowLimit`: `MWSFD_SetFlowLimit(mwply, (Sint32)(0.8 * n), n)` takes a THIRD argument —
  the "r5..r7 vs r4..r6 M1" was a missed parameter), mpv_cdec `MPVCDEC_IntraBlocks` (the OPEN 192-store
  clear: six calls of a `static inline` helper clearing 32 `Float64` through a `f64 **cursor`
  starting at `&coefficients[3]`), dct_fsri `initSparseTbl`/`DCT_FsriInitScaleTbl`, mwsfdcre
  `MWSFCRE_ResetSfdHn`, sfd_mpv `sfmpv_ChkFatal`/`SFD_SetMpvCond`. Their other units are worse than
  ours (different struct layouts / naming), so take functions, not files. F-Zero GX (rayanht/fzgx)
  has the 2003 Sofdec in a REL built with GC/1.2.5n/1.3 (older library generation, not comparable).
- M2 mechanism (pool-base materialisation, not "pooling"): MWCC merges a unit's local `.rodata`
  objects (and global `const` tables: sfd_mpv `sfmpv_conv_*`, dct_ac's `dctac_i_const` in `.bss`)
  into one pool with a size-0 `...rodata.0`/`...bss.0` label, and a function that references >= 3
  pool members (counted excluding the backend's int->float conversion double, both compilers) loads
  `lis/addi` of the label into a register and addresses members by displacement; with < 3 it emits
  `lis sym@ha; lfd sym@l` per member. Bio4.sym confirms: 32 `...rodata.0` labels, none in adx_dcd,
  dct_ac, sfd_adxt, sfd_mpv, sfx_cnv. In the target the base is used by every function with >= 3
  string/table references (27 functions, e.g. cftyp422_ppc `CFT_MakeArgb8888Alp3211Tbl` with 13 FP
  literals + the version string, cftfx `CFT_MakeYcc422ColAdjTbl` with 7 FP literals of which 3 were
  created by the previous function) but NOT by the five functions whose >= 3 references are all FP
  literals/tables first created by that function (adx_dcd `ADX_GetCoefficient` 10, dct_ac
  `DCT_AcInit` 4, sfd_adxt `SFADXT_SetSpeed` 4, sfd_mpv `sfmpv_Pts2Tc` 3 global tables, cftfx
  `CFT_MakeArgb8888ColAdjTbl` 3 incl. the conversion double = 2, ours agrees on that one). Ours
  counts them. `#pragma pool_data off` before a function switches BOTH pools off for it: adx_dcd
  becomes 10/10 byte-identical (pure C fix, no `.bss` pool there); dct_ac/sfd_adxt/sfd_mpv functions
  also need their `.bss`/global-table pool so the pragma over-shoots (DCT_AcInit 264 vs 256 bytes);
  `-pool off` unit-wide is the same trade (adx_dcd +1, 35 other functions lose their `.bss` pool).
  No build/flag reproduces the FP-literal exclusion.
- M4 (stwbrx): the fold `store(bswap32(x))` -> `stwbrx` happens in every build (GC/1.3..Wii/1.7)
  already at `-O1`, for `volatile` stores, `Sint32` operands, struct-member operands and temp
  spellings (only a chained `v = ..; v |= ..;` form avoids it, with a different instruction mix). The
  ADX group's originals fold (adx_bwav/adx_bau/adx_baif `stwbrx rS, 0, rB`, rA=0 form), the Sofdec
  group's do not (sfh_main x7) although both name the same compiler; mk-deception's sfh_main is also
  NonMatching. Still unexplained by any available build or flag.
- Regression fact for the sweep note: `-O4` (no `,p`) loses 75/261 Sofdec and 84/285 ADX functions,
  `-inline auto` vs `all` are identical on Sofdec and `all` loses 8 on ADX, `-proc 750` == `gekko`.

## REL modules

The game loads its rooms, enemies, weapons and debug tools as Nintendo REL overlays. `ninja` rebuilds the
110 configured RELs byte-identical next to the DOL (`build/G4BE08/<mod>/<mod>.rel`, all covered by the
`build/G4BE08/ok` SHA-1 check: `111 files OK`); a unit you match replaces one split object in one REL,
like the DOL.

### Facts

- Modules: the 20 loose `files/Rel/*.rel` of the disc (Sscrn, Tools, st1_0..st4_0, t_camera, t_emlist,
  t_esp, t_event, t_id, t_light, t_movie, t_sce) and the 90 distinct RELs inside the `files/em/*.drs`
  archives (43 enemies em10..em3e, 8 players pl02..pl14, 39 weapons wep00..wep47). 117 of the 128
  archives carry a REL; 27 of those are byte-identical copies of another archive's REL (costume
  variants em46/56/66 = em16, em4c/5c/6c = em1c, em4d/5d/6d = em1d, em4f/5f/6f = em1f, em50/60/70 = em20,
  pl0b/pl0c = pl02, pl15 = pl11, wep03/18 = wep02, wep20 = wep11, wep21/24 = wep09, wep25 = wep14,
  wep31/32 = wep10, wep46 = wep13; the module is configured once, under the first name), 11 (pl00, pl01,
  pl03..pl05, pl07..pl10, pl12, pl21) have none. Module ids are unique among the distinct RELs.
  Not on disc 1 although a `Bio4.<mod>.sym` exists: em06, em09, emmark, st0, st3_0..st3_3, pl03/10/12.
- Originals: `orig/G4BE08/files/{Rel,em}/<mod>.rel` and `orig/G4BE08/files/Bio4.<mod>.sym` are
  untracked. `python3 tools/extract_orig.py config/G4BE08/config.yml <disc>` writes them (plus
  `sys/main.dol`, `sys/main_split.dol`, `files/Bio4.sym`) from a disc image (.iso/.gcm, read directly)
  or from a directory made by `dtk disc extract`; `configure.py` runs it itself when a configured
  module's REL is missing and an image sits in `orig/G4BE08/`.
- DRS archives (`tools/drs.py list|rel|extract|pack|rebuild|roundtrip`, byte-identical round trip on
  all 128): a 0x20-byte free-text signature ("ハカセのアホーーーーーーー！！！" in Shift-JIS; 11 pl
  archives repeat the half-width "ﾊｶｾ " instead), then 32-byte records `{u32 type, size, 0, offset,
  p0, p1, 0, 0}` ending with type 0xFFFFFFFF, zero to 0x400. Record type 0 is the body at 0x400 (its
  size 0x20-rounded), type 4 the sound bank appended after it (or `{0xFFFFFFFE, 0, 0, file size}` when
  there is none). Body: `u32 count, rel_offset, 0, 0; u32 offsets[count]; char tags[count][4]`, padded
  to 0x20, then the entries (0x20-aligned, tags BIN/TPL/FCV/SEQ/EFF, zero tag = empty entry) — no sizes,
  an entry runs to the next offset and was padded with 0xCD (the packer's uninitialised buffer);
  `rel_offset` (0 = none) is the REL, its own size is the end of its last relocation list, padded with
  0xCD to 0x20. The sound bank is the same container again (records type 1 and 2, exact sizes, zero
  padding, p0/p1 bank parameters). `drs.py rebuild <orig.drs> build/G4BE08/<mod>/<mod>.rel <out.drs>`
  is the archive with a rebuilt REL (identical to the original for every module today).
- Module names are the disc file stems; a module's units are `<mod>/<file>.cpp` (source `src/<mod>/<file>.cpp`,
  or a shared source given in `config/G4BE08/modules.py`), objdiff calls the unit `<mod>/<mod>/<file>`,
  its target asm is `build/G4BE08/<mod>/asm/<mod>/<file>.s`, the split object `build/G4BE08/<mod>/obj/...`.
- Symbols: `config/G4BE08/modules/<mod>/{symbols.txt,splits.txt,sym_map.tsv,rel.json}`, generated by
  `python3 tools/gen_rel_config.py config/G4BE08/config.yml orig/G4BE08/files` (safe to re-run: names and
  scopes already synced from compiled units are kept). The debug `Bio4.<mod>.sym` only lists the `.text`
  functions of the module (offset, size, scope, demangled name; every function is attributed to
  "<mod>.preplf", so there is no per-object information) — data symbols are labels at every relocation
  target (`lbl_<mod>_<section>_<off>`), unnamed functions are `fn_<mod>_<off>`; em3e has no .sym at all
  (labels only). Addresses in the module files are section offsets.
- Units: a module is one unit `<mod>/<mod>.cpp` unless `UNITS` in `config/G4BE08/modules.py` names
  boundaries (`(unit, first function[, shared source[, {section: data start}]])`); the generator
  attributes .rodata/.data/.bss ranges to units by the relocations coming from each unit's code: a
  unit's data starts at its first reference above everything earlier units address (references
  below that must hit a global of an earlier unit), unreferenced data goes to the preceding unit
  unless the 4th element pins the start. The `__static_initialization_and_destruction_0`/`global
  constructors keyed to X` pairs and the `"D:/Bio4/Prog/<file>.cpp"` HALT strings are the best hints
  for the original file boundaries.
- Shared enemy library: the 16 Ganado modules em10..em17, em19..em1f, em20 (.text 0x4402C..0x44444)
  are `em10.cpp` (`"D:/Bio4/Prog/em10.cpp"`, cEm10 and the em10*/em1c*/plem10* helpers: .text
  0..0x43518 = 382 functions + the 0x3B8-byte template gap to 0x438D0, .rodata 0..0x1EC4, all 0x994
  bytes of .data, the 0x34-byte COMMON .bss — byte-identical in all 16, the split object
  `build/G4BE08/<mod>/obj/<mod>/em10.o` is the same in every module) followed by ONE per-enemy
  object, unit `<mod>/<mod>_set.cpp` (src/<mod>/<mod>_set.cpp, all 16 Matching): `_prolog` =
  `OSReport("em10 prolog Ok\n")` in every module + `EmInitFunc = EmXXInit; Em10SetFunc = EmXXSet;`
  (no ctor loop), `_epilog`/`_unresolved` empty, then EmXXInit (`new (em) cEm10()`), EmXXSet,
  EmXXWeaponSet (0x75C..0xB74 bytes). em1d/em1e/em1f/em20 include light.h (+ esp.h for the
  trailing `EspDataLoad((u32) ARC(0x278), 0xCD, 0)`): their `.rodata` is [light.h string][prolog
  string][five cManager<cLight> template strings] with the 0x3B8 cLight linkonce block after
  EmXXWeaponSet - only one TU lays it out that way (a separate entry object would put the template
  strings and the block before EmXXInit), which is why the former `<mod>_prolog.cpp` unit was
  merged. The real file name is not in the binary. The 28 other enemies (em18, em21..em3d,
  em3e; .text 0x10C8..0x17034) start with `_prolog`/`_epilog`/`_unresolved` (same code, 4 `_prolog`
  variants), then their own `"D:/Bio4/Prog/emXX.cpp"`, and share only cUnit's inline
  `beginEvent`/`endEvent`/`~cUnit`/`operator delete` (byte-identical, at the end).
- Compiler flags: `cflags_game` + `-G 0` (no small data in RELs: every DOL global goes through
  `lis/addi`), plus the module's `CFLAGS` entry of config/G4BE08/modules.py (Sscrn:
  `-fno-implement-inlines`, see the Sscrn subsection). Linkonce functions are placed at assembly time by
  `tools/ngccc.py place_linkonce_module` (the REL linkonce rule, Sscrn subsection); post-build
  `fold_linkonce.py --module <mod>` appends the vtables (see "Multi-object modules" below); no
  strip_unused (nothing is dead-stripped in a -r link).
- Toolchain: the original RELs came out of `ngcld -r` followed by SN's `snmakerel` (Nintendo's makerel
  port). We do the same: `tools/link_rel.py` links the units with `ngcld -r -T config/G4BE08/rel_ldscript.ld`
  (SN's preplf.ld layout: every section at 0, `_ctors`/`_dtors` labels and the `LONG(0)` terminators come
  from the script), `tools/make_rel.py` writes the REL from that ELF plus `rel.json` (module id, original
  ELF section count, string-table name offset/size, align/bss_align, REL section indices, imported
  modules). dtk's `rel make` was not usable: it keeps REL14 out of the table, takes REL section indices
  from our ELF and DOL section bytes from our main.elf, and knows nothing about the SN specifics below.
- What snmakerel did, reproduced in make_rel.py (verified byte-for-byte on all 110):
  - imports ordered other modules ascending, then self, then the DOL (fix_size = start of the self list);
    each list by section then offset; NOP entries for gaps > 0xFFFF.
  - REL24 to a same-section target is resolved and dropped; REL24 to another module/the DOL is patched
    to `bl _unresolved` and kept; REL14 (NgcAs emits one for every conditional branch, GAS does not)
    is resolved and kept.
  - ADDR32/ADDR16 fields hold S+A for local symbols and only A for globals, so symbol scopes matter:
    the generator derives them from the original fields; the few targets referenced both ways get
    `field_overrides` in rel.json. Our ngcld 3.9.3 -r also adds the displacement of the input section
    defining a *global* to the field (visible only from the second object on: em10's `_prolog` ->
    `Em10Init`), the original linker did not; make_rel writes A back into every global field. So a
    zero field in the original never tells whether the reference crossed an object boundary.
  - module-0 relocations carry the *original main.elf's* section index in the section byte (.text 2,
    .rodata 5, .data 6, .bss 7, .sdata 8, .sbss 9, .sdata2 10); make_rel resolves DOL names through
    `config/G4BE08/symbols.txt` (not main.elf, whose names follow the compiled DOL units).
  - undefined names resolve to the first definition in DOL, then linked modules by ascending id
    (t_camera's `__builtin_delete` is the DOL's although Tools/t_esp have copies).
  - ngcld 3.9.3 appends its BSS_TAG object: a pointer at the end of .data (`__sn__bss__tag__address__`,
    field 0) to the zero-size `__sn__bss__tag__` at the end of the objects' .bss; both are in every REL,
    so the split of the last unit skips that .data word.
  - g++ 2.95 emits uninitialised static data members as COMMON (template statics of cManager<T> etc.;
    the DOL's `IDSystem::m_scrn_mat` is one). `ngcld -r` leaves them unallocated and snmakerel appended
    them to .bss after the tag: 0x34 bytes in most modules with .bss (0x30 in em21/pl0f, none in the
    weapon/player modules). The skeleton carries them as one `.comm common_<mod>` (a `common` split,
    in the unit that addresses the block or has templates); make_rel allocates COMMON symbols after
    .bss in symbol-table order. Without a COMMON block the tag pointer targets the end of .bss, where
    the generator puts a local `__sn__bss__tag__` label for dtk.
  - dtk needs REL14 relocations against the section symbol (it emits them against the containing
    function, and ngcld's A-P patching then overflows on big modules): link_rel.py rewrites the split
    objects' REL14 entries into `objfix/` copies before linking.
- Proof unit: `src/st2/st2.cpp` (`"D:/Bio4/Prog/st2.cpp"`: `set`, `setTbl`, `_prolog`, `_epilog`,
  `_unresolved`), the same object at the end of every st2_* module; st1_*/st4_0 end with st1.cpp/st4.cpp.
  `_prolog` runs `_ctors`, calls `setTbl` (registers every stage room's Init/Main in the DOL's
  `St2_data_tbl`, cross-module references), `_unresolved` is `HALT()` at line 169.

### Workflow for one module unit (`st2_4/r22c`)

```sh
python3 tools/unit_info.py st2_4/r22c                 # functions, sizes, match % (module sym_map)
sed -n '/^\.fn NAME/,/^\.endfn/p' build/G4BE08/st2_4/asm/st2_4/r22c.s
# write src/st2_4/r22c.cpp, then:
ninja build/G4BE08/src/st2_4/r22c.o
python3 tools/sync_rel_symbols.py build/G4BE08/src/st2_4/r22c.o   # module symbols.txt (+DOL/imported modules for references)
ninja                                                  # must still print `111 files OK` (ninja reruns configure itself)
python3 tools/fdiff.py st2_4/r22c <mangled_symbol>
```

Then remove `st2_4/r22c.cpp` from `NON_MATCHING` in `config/G4BE08/modules.py`, `ninja`.

Data of another (not yet compiled) unit of the same module has no name in the `.sym`, so a reference
like `Em10SetFunc` (em10.cpp's `.data+0`) or `_vt.5cEm10` cannot be matched by demangled name;
`sync_rel_symbols.py` resolves such undefined names through the relocations instead: the unit's split
object (`build/G4BE08/<mod>/obj/<unit>.o`) must reference a `lbl_`/`fn_` placeholder at the same
`.text` offsets with the same relocation types, and that placeholder is renamed. It reports the
names it could not resolve; a name whose relocations do not line up (function order differs from the
target) stays unresolved and `make_rel` then fails with "undefined symbol".

### Adding a module (every REL of disc 1 is configured; this is for another disc/build)

1. Append to `modules:` in `config/G4BE08/config.yml`: `object: files/<Rel|em>/<mod>.rel`, `name`,
   `splits`/`symbols` paths under `config/G4BE08/modules/<mod>/`. The object path says where the REL
   comes from: `files/Rel/` is a loose disc file, `files/em/<mod>.rel` is the REL inside
   `files/em/<mod>.drs`. Name a REL after the first archive that carries it (`tools/drs.py list`).
2. `python3 tools/extract_orig.py config/G4BE08/config.yml <image or extracted disc>` (also copies the
   `Bio4.<mod>.sym`; `configure.py` does this itself when an image is in `orig/G4BE08/`).
3. `python3 tools/gen_rel_config.py config/G4BE08/config.yml orig/G4BE08/files`; every module it
   imports must be configured too (the tool says which id is missing).
4. `sha1sum orig/G4BE08/files/<dir>/<mod>.rel` -> a `<hash>  build/G4BE08/<mod>/<mod>.rel` line in
   `config/G4BE08/build.sha1`; `python3 configure.py && ninja` must report every file OK. A mismatch
   is a new snmakerel/ngcld case for `tools/make_rel.py` (the `--verify` output names the region);
   never patch bytes by hand.

### Multi-object modules (t_emlist, st1_0, the tool modules)

- Unit boundaries from the `.sym`: the `"D:/Bio4/Prog/<file>.cpp"` HALT strings, function-name prefixes
  and the `.rodata` header-string groups (`light.h`/`atari.h`/... appear once per object that includes
  them, at parse time, i.e. before that object's function strings; template strings at the object's end).
  t_emlist = t_emlist.cpp (0x0..0x6388), t_prim.cpp (0x6388), t_util.cpp (0x6B2C), tools.cpp (0x7330);
  st1_0 = r100.cpp, r120.cpp (0x40B0), st1.cpp (0x5D28). Data the code never addresses (header strings,
  `.data` of a dead inline) is assigned with the 4th UNITS element `{".rodata": start, ".data": start}`.
- Shared sources: `src/tools/tools.cpp` (`_prolog` = ctors + `ToolsTask()`, the `DebugMenuSelected`
  switch of 23 Tool* entries, `_unresolved` HALT at line 142; the headers are included *after* the
  functions — its `.rodata` has the entry-point strings before atari.h/light.h/event.h/ctrl.h) is the
  last object of every tool module (byte-identical in t_emlist/t_camera/t_light/t_sce/t_event; in
  t_esp/Tools/t_id the linkonce orphan sections follow it, t_movie has more objects after it).
  `src/st1/st1.cpp` (29 rooms, HALT line 144) ends st1_0..st1_3. `src/tools/t_prim.cpp` / `t_util.cpp`
  are the full versions of the DOL's dead-stripped game/t_prim, game/t_util (different builds per tool
  module: t_light's t_util has only Init/QuitDefault, t_camera's t_prim only 5 functions).
- Linkonce in the module link (`fold_linkonce.py --module`): `ngcld -r` kept *every* object's
  `.gnu.linkonce.t.*` copies, appended to that object's .text in emission order, and the weak names
  resolve to the module's first copy — the `.sym` names only that copy, later copies are nameless
  `fn_<mod>_<off>` blocks (0x3B8 = log/countActiveWork/create(int)/create()/create(int,u32) of
  `cManager<cLight>` for every object that merely includes light.h). The fold keeps the copies the
  module sym_map names for the unit (demangled name + size) or, for a unit with none named, all of
  them as nameless code with weak-undefined symbols. The first copy's body set is *not* what our
  compiler emits for the same include set (t_emlist.cpp has countActiveWork + create(int) only, ours
  gives 5): the fold drops the rest, sizes decide.
- `sync_rel_symbols.py` resolves overloads by size: a mangled name already carried by a DOL or module
  sym_map row (`create__t8cManager1Z6cLighti` = 0x158) only claims a placeholder of that size, and an
  entry that already carries another mangled name is never renamed (message "left alone").
- Field rules seen with displaced sections: a *global* symbol's ADDR16/32 field is A (make_rel writes it
  back), a *local*'s is S+A — so scopes are visible: t_util's `globalCamera` is `static` (field 0x140),
  its five flag backups are globals (field 0), t_prim's Vrect/Orect/FlipMode/`bl` are statics.
- `GXWGFifo` (absolute, `include/gx.h`) is defined in `config/G4BE08/rel_ldscript.ld` too; ngcld -r keeps
  the relocations against it, `tools/link_rel.py` applies and drops them after the link (the RELs have
  the final `lis 0xCC01`/`-0x8000` words and no relocation).
- `.bss` of a compiled module unit is invisible to the split object compare (NOBITS); check sizes with
  readelf, and remember unreferenced `.bss` objects survive the -r link (t_prim's 0x20 behind
  ToolBuffer, dropped by the DOL link).
- t_emlist.cpp idioms (src/t_emlist/t_emlist.cpp, 36/53 functions): the work pointer is a struct member
  (`EmList.wk`, a one-member struct in .data right before the routine table): every store through it
  reloads the pointer, consecutive loads share it. `EmListCtrl* ctl = &EmList;` declared at the top of
  emlist_init puts the `lis EmList@ha` into the prologue (callee-saved) although the only store is
  behind seven calls. `y = 0x50 + i * 0x10;` as the *first* loop statement gives the giv init last in
  the preheader while `mr r4, y` survives in a branch where `i` is known (a plain `y += 0x10` counter
  inits first, an inline expression folds to `li 0xa0`). `int r = p->room; ((step + r) & 0xFF) |
  (r & 0xF00)` puts `step` first in the `add`; the member read directly puts the load first. Tool-style
  raw offsets: `(u32*) (no * 0x20 + (u32) pG + 0x501C)` + `BitOff(tbl[i >> 5], m)` = `slwi; add idx,pG;
  addi 0x501c; lwzx/stwx` with pG reloaded per iteration. `int step = wk->step; switch (step)` keeps the
  switch register and stores it as the constant in `case 1:` (`stw r10`). OPEN: emlist_file_menu_disp
  copies the `i - 11` giv into r8 once before the compare tree and shares one eprintf tail between the
  omake and stage cases; `switch (i - 11)` with `i - 11` in every call reduces the giv (`li r28, -0xb`)
  but cse folds the argument per case.

### Stage (room script) modules st1_0..st1_3, st2_0..st2_4, st4_0

- Layout of every stage REL (config/G4BE08/modules.py UNITS): `em_wrap.cpp` (src/st/em_wrap.cpp,
  include/em_wrap.h: cEmControl/cEmPatrol/cEmGuard + the cEmWrap enemy handle with ~50 members + free
  `setEm`/`SceCkFindPL`; no __FILE__ string, the real file name is unknown), then `cSceObj.cpp`
  (st2_0/st2_3/st4_0 only, src/st/cSceObj.cpp, "D:/Bio4/Prog/cSceObj.cpp"), then one object per room
  (`rNNN.cpp`, sources in src/st1/, src/st2/, src/st4/ — r102/r103/r108 are the same object in st1_1 and
  st1_3, so room sources are keyed by stage, not module), then st1.cpp/st2.cpp/st4.cpp (st4.cpp includes
  map_obj.h/light.h/widget.h/atari.h, hence header strings + a cManager<cLight> block after its code, and
  defines the global `st4_initAdaGame` that r405 calls). Room .text starts are the first function of the
  room (usually `RNNNInit`; r10a.cpp starts at EmSetNormal, r208.cpp at setResetNum (offset, the name is
  also in r222.cpp), r210.cpp at asl_wait, r40a.cpp at r40a_DuraluminCaseOpen, r405.cpp at snd_tbl_set —
  a `bl` to a function of the *previous* unit with an S+A field in the REL means the boundary is wrong;
  make_rel --verify shows it). Room .rodata starts are pinned at the room's first header string (each
  room's group is `[cFlag.set() string][atari.h/event.h/map_obj.h/light.h/widget.h...][HALT][flag_rsf.h]
  [rNNN.cpp]`, header strings are unreferenced); pointer tables referenced only from data need a pinned
  .data start too (st2_3 r21b.cpp: .data 0xA8).
- **The original REL link dead-stripped em_wrap.cpp and cSceObj.cpp at function level** (like the DOL's
  SDK objects: bodies gone, every string and constant pool kept, functions in declaration order): each
  module keeps exactly the members its rooms transitively call (st1_0: none, 0 bytes of .text; st2_4:
  six). `tools/strip_unused.py --gcc --module <mod> --unit <mod>/em_wrap.cpp` reproduces it from the
  module's sym_map.tsv (modules.py `STRIP_UNUSED` names the units; overloads are told apart by size once
  the row carries a mangled name, and anything still referenced from surviving code is kept — NgcAs
  writes intra-object `bl`s as `.text+off`). Room objects were not stripped (st4.cpp keeps the unused
  static helper... which turned out to be referenced from r405). em_wrap.cpp exists in two revisions:
  st2_4/st4_0 have no cEmControl/cEmPatrol/cEmGuard (no "cEmControl::SetPatrol" string, no 0.0/1000/100
  pool: src/st/em_wrap_v2.cpp = `#define EM_WRAP_NO_CONTROL` + include). Members that no module kept
  (isTrans, setMove, ..., get_l_pl) are written from their strings/pools only.
- em_wrap idioms: the getters are `if (isAlive() == 1) return pEm->x; err(...); return 0;` (fail block
  laid out first with `beq`); `checkStatus` is `return pEm->checkStatus(stat) != 0 ? 1 : 0;` inside the
  `if` (the ternary keeps the unmerged `li r3,0; b end`); `setPtr(cEm*, int)` stores `alive = a; no = -1;
  pEm = em; list = -1;` with `int a = (be_flag & 0x201) == 1` (one `li -1` for both narrow stores);
  `setPtr(s16, s8, int)` is `if (p != 0) return setPtr(p, errOn); return setEm(...)`; the patrol wrap is
  `if (next < 0) wrap = nPoint - 1; else wrap = (next > nPoint - 1) ? 0 : next; p->cur = wrap;` (fresh
  variable, one store); SceCkFindPL computes `cEm* p = pArray + size * i` *before* `cEmWrap em;` (the
  ctor call) and needs `Vec v` at frame offset 0 in cEmGuard::TaskMove for the recomputed `addi r4,r1,8`.
  OPEN: cEmWrap::setEm's `add r9, pG, idx` operand order / register choice (ours `add r9, idx, pG`; ~15
  forms of the EM_LIST access tried) — the only non-identical bytes in st1_1..st2_3/st4_0's em_wrap.
- SOLVED (r10d .rodata order): it was a unit-boundary error, not an include-order one. The generator
  gives unreferenced data to the preceding unit, so the `HALT %s(%d)` string that opens r10d.cpp's
  group (`[HALT][flag_rsf.h][r10d.cpp]`, like every room) had been attributed to r10c.cpp (r10c's own
  group ends with the cManager template strings) and r10e's HALT to r10d. Pins moved to 0xE40/0xE80
  in modules.py; both rooms are Matching. Rule: a room's `.rodata` pin is the *first* string of its
  `[cFlag.set()][atari.h ...][HALT][flag_rsf.h][rNNN.cpp]` group — a HALT string right after another
  room's template strings belongs to the next room. Check: dump each split object's `.rodata`
  strings (`readelf -x .rodata build/G4BE08/<mod>/obj/<mod>/rNNN.o`) and make sure no room group
  starts with `D:/Bio4/Prog/flag_rsf.h` or ends with a lone HALT after the cManager strings.
- Room-script idioms (src/st1/r10e, r11a, r109, r107, r102, r10a matched; r108 15 functions, 9 exact):
  - Every room includes `include/st_room.h` after main_mem.h: it defines the module's 0x34-byte COMMON
    placeholder (`asm(".comm common_<REL_MODULE>,52,4")`, REL_MODULE is a per-module `-D` configure.py
    adds to every module unit) so a module whose common-owning room is compiled still links (the split
    skeleton's `common_<mod>` merges with it), plus the scalar reference setters U8Set/U16Set/U32Set/
    IntSet/FAdd/FSub (FSet is global.h's).
  - `include/flag_rsf.h` is the original's shape (RsfSet/RsfClear/RsfCheck with `if (no > 0x1F) HALT`
    at lines 17/21/25; constant `no` folds the check away): `RsfCheck(G_ROOM_ID, 0)` is
    `lwz 4(r3); cmpwi 0; bge/blt` (sign test), higher bits `andis.`. `if (RsfCheck(..) == 0)` and
    `if (RsfCheck(..))` are the two branch polarities.
  - Room work: `static RNNNWork* rNNN_work; rNNN_work = (RNNNWork*) MEM_CALLOC(sizeof, 1, 0xd);` with
    `#line N "D:/Bio4/Prog/rNNN.cpp"` (N from the mem_calloc line argument). Stores into the work after
    a call reload the work pointer: write `rNNN_work->field = call(...)` directly (r109), and a store the
    original reloads *both* the work and the field after is a reference store (`PSet(rNNN_work->evd, ..)`,
    r102). Work pointers of `cSat*`/`cObj*` etc. get their own typed `PSet` per file.
  - `SceExec(0x12, (TaskFunc) fn, 0, 0, 2, 0)` / `SceAtDataSet_exec(no, 0x12, 0, (TaskFunc) fn, 0, 1|2)`
    are the task registrations; `EmSetFromList2(no, 1)`, `setEm(no, -1, 1, 1, 1)`, `SceCkFindPL(0)`,
    `SceCountEmAlive(lo, hi)` the enemy calls; BGM/stream tasks are `for (;;)` loops with an `on` flag
    whose `on = 1` is written *after* the SndRoom* call (the `li` then lands among the call's `li`s).
  - `Vec ang = {a, b, c}` locals with all-constant initialisers are `.rodata` templates copied with
    3 lwz/stw (r109, r102 `pos`); a Vec built from `stfs` of pool constants is memberwise stores, and
    one member stored through a `Vec* pa = &ang` pointer while the others are direct gives the
    `stfs f31, 4(r31)` / `stfs f0, 0x20(r1)` mix (r102 `ang`). A float kept in a callee-saved FPR across
    a call and stored after it is a local `f32 ry = K;` declared before the call. `cPlayer* pl = pPLS`
    (struct view of pPL) keeps the pPL load below the preceding Vec template stores.
  - Vec/aggregate locals are temp slots rounded to 16 bytes (a Vec takes 0x10 of frame); address-taken
    scalars (`cEm* torch; getRoomEtcTorch(0, &torch, 1)`) get their slot after every aggregate.
  - Angle constants: some are `deg * (PI / 180.0f)` folds (r102: -5.4/-3/-1.2 deg), others only
    reproduce as raw float literals (r109's Vecs) — use `numpy.float32(...)` shortest repr literals.
  - `const f32 step = K;` at the function top puts K first in the constant pool while the uses stay
    literal (r102 openCover, r108 openCover); `f32 w = 1000.0f;` (variable) before a float-heavy call
    makes it the first pool entry and the shared `fmr` source (r108 YarareInitCube).
  - Scroll objects: `SmdGetObjPtr(id)->be_flag |= 2` chains (r109 koya_init) are plain stores;
    `BitOn(obj->be_flag, 0x20)` (reference) is needed where the original reloads another global
    pointer after the store (r108 initPuzzle), and `static inline void setObj(cObj*& o, u32 id)
    { o = SmdGetObjPtr(id); BitOn(o->be_flag, 0x20); }` (reference parameters) puts every `lis` of the
    pointer globals into callee-saved registers before the first call.
  - Event flag words `pG->flags_174[i]` in the rooms are `*(u32*) (((no >> 5) << 2) + (u32) &pG->flags_174)`
    (cast-then-deref: the store forces a pG reload per loop iteration, r108).
  - `EmListData d; d.id = ..; d.type = ..; ... EmSetEvent(&d)` blocks: the type byte is stored before
    the halfword/word fields (own QI constant), `hp`, `x1A`, `xB` after `rot` (r10a).
  - `0x150 - cMes.getWork()->fontH - cMes.getWork()->lineSpace - 1` (sce_com's source) compiles to
    `0x150 - lineSpace - fontH` with our cc1plus; the rooms (r108 checkDoor/execPuzzle) need the
    operands written `lineSpace` first to get the target's `lbz 0x19; lbz 0x77` order — sce_com has the
    same mismatch in its own object.
  - Room-local static tables in `.data` (r108 `R108Symbol r108_symbol[8]`, r109's Vec positions) are
    non-const initialised statics; a table only read is `static const` and lands after the template
    strings at the end of `.rodata` (r11a/r107/r10a `AtEffInfo`).
  - cEm27::setWaterHeight (em27 module) is declared in include/em27.h; getRoomEtc*() in etc_model.h;
    EventMgr::NameChange/SetEvt(void*, u32*) in event.h (the second SetEvt overload at 0x8013991C
    takes a name; DOL symbols renamed by hand); EspDataLoad/EspGetEfmTplAddr in esp.h; EmSetEvent in
    em_set.h; EmReadSearch in read.h.
  - OPEN (r108): execShowView issues `lfs f1, 0.0` last (ours second) around the RsfSet store;
    checkEmReset's `int list[11]` end pointer is `addi r29, r31, 0x28` from the array pseudo (ours folds to
    `r1+0x30`; `int* p = list` forms tried); openCover's tail after the `do {} while (1)` loop re-materialises
    the `lis` of both cover globals and the 220.0 constant instead of reusing the loop-hoisted registers;
    switchSymbol / str_check / initChurchBell differ only in callee-saved register choice or one
    load order.

### Ganado per-enemy objects (em10..em20 `<mod>/<mod>_set.cpp`, all 16 Matching, 2026-09)

- The 16 sources were generated from the target asm (the tables differ, the code does not); idioms:
  `Em10Work* w = EM10_WK(em); switch (em->type) {...}` with one arm per model type, each arm a run
  of `w->mot[i] = PL_ARC_PTR(em->subArc, N)` (`lwz subArc; lwz ofs; add; stw`, the subArc reload
  after every store is the natural aliasing of a store through `w` against a load through `em`),
  ending in `Em10SetSeTbl(em, K)`; then `w->x6C5 = K` (or `if (em->type == 6) w->x6C5 = 0; else
  w->x6C5 = 1;` for the `li 1; bne; li 0; stb` shape) and `EmXXWeaponSet(em)`.
- Archive stores are chained (each reload depends on the previous store), so their issue order IS
  the source order (types 6 fill `mot[16]`/`mot[17]` between `mot[4]` and `mot[5]`). Zero stores are
  free: write them after the archive store of the *segment* they are issued in (the run between two
  `lwz subArc`), ascending by index; the dying zero store (`stw r11, 0xd0` = mot[40]) then comes
  out first or mid-run exactly like the target.
- Compare tree: the default arm's `em->type = K` store means `case K:` is a label of the default arm
  (the node is real, its `beq default` is jump-threaded into `ble/blt default`: em11 `case 7:
  default:` gives `cmpwi 8; beq; ble def; cmpwi 9; beq`, em12/em17/em13 `case 0: default:`, em1c
  `case 7:`, em10 `case 0:`). em14 has `case 7:` as the tree root (`cmpwi 7; beq default`).
- Variant types (em1d/em1e/em1f/em20): `case 15: case 19:` arms start with a reloaded `if (em->type
  == 15) {mot[0], mot[1] = A} else {= B}` and share the rest; the default arm `case 14: case 18:
  default: if (em->type == 18) {A} else {B; em->type = 14;}`. em20's `case 18` is a label INSIDE the
  then-arm (`if (em->type == 18) { case 18: A } else {..}`: the switch jumps past the compare).
  em1d's types 17/21 have no compare and share everything from `mot[2]`: `case 17: A; goto common17;
  case 21: B; common17: ...` - two full copies compile to a different zero-store schedule in the
  case-21 block (it would start at `mot[0]`; the target's starts at `mot[2]`), and the cross-jump
  runs after sched1.
- WeaponSet with `lbz type; cmpwi 4; bne` = `if (em->type == 4) { mot[67] = ..; [mot[68] = ..] }
  else {..}` with the identical `add; stw` tail cross-jumped (em10/em15/em16; em16 differs in two
  slots). em20's case 2 also does `em->flags_3C8 |= 0x10000000;` before its `Em10SetSeTbl`.
- `Em10SetFunc` is em10.cpp's `.data+0` (declared in em10.h); `EmInitFunc` is game/em.cpp's
  (`extern void (*EmInitFunc)(cEm*)`); `Em10SetSeTbl` is `extern "C"`.

### Sscrn (sub screen DLL, src/Sscrn/ss_*.cpp, include/ss_main.h)

- The screens are `Widget<SUB_SCREEN>` state-machine nodes (include/widget.h: `num`, `link[]`, `cur`, vptr
  at 0xC; virtuals dtor/init/quit/move; `connect(no, w)`, `transit(no, wk)` with the two `pLog->err`
  strings). `SUB_SCREEN` is the real tag of sscrn.h's work (`SubScreenWork` is a typedef): the module's
  mangled names carry it. The link table is `mem_alloc(4 * num, "widget.h", 89, 1, 13)`; derived widgets
  have NO user constructor (`Widget(int n = 1)` + implicit ctors: an in-class `SsX(int n) : Widget(n) {}`
  is emitted out of line, 0xA4 per class) and are declared in the order their vtables appear reversed
  (`_vt.9CapSelect` at the lowest .rodata address = declared last).
- Each unit's linkonce block is `[cManager<cLight> copies] ~Widget<SUB_SCREEN> [own synthesized dtors +
  in-class inlines in declaration order] Widget::quit, init, move`: the destructor is instantiated by a
  `static inline` `delete w` helper in ss_main.h before any derived class is declared, quit/init/move
  at their first use (transit uses quit then init). Units with named linkonce copies next to nameless
  duplicate blocks (ss_debug's dispWorkNum after the 0x3B8 cManager<cLight> block, ss_file's 0x408 +
  0xC around its dtors) were handled by fold_linkonce's `keep_unnamed` size sum; the module rule is
  now implemented in `tools/ngccc.py place_linkonce_module` (see next item), fold's module heuristics
  only remain for the legacy `--prodg-driver ngccc` path.
- **REL linkonce rule (SOLVED, tools/ngccc.py `place_linkonce_module`, applies to every module unit):**
  the original `ngcld -r` link kept the FIRST object's copy of a linkonce function only if some
  relocation in the module references its symbol, and every LATER object's copies whole as nameless
  code. Evidence in every module: the first unit including light.h names `cManager<cLight>::
  countActiveWork` + `create(int)` only (`bl`'d from the later blocks' `create()`/`create(int,u32)`)
  while `log`, `create()`, `create(int,u32)` (called only through DOL vtables) vanish, and every later
  unit carries the full 0x3B8 block (Sscrn ss_cap vs ss_main.., st1_1 r101 vs r102.., t_emlist vs
  t_util/tools, em10's own partial link: named pair + 0x3B8 block in one object). ss_main's
  `cManager<cMap>::log` is exactly such an unreferenced first copy (dropped), its cLight/Widget copies
  later duplicates (kept). ngccc.py assembles once for the sizes, then rewrites the asm: named copy
  -> `.text` in place; unnamed with an earlier unit naming the class instance -> `.text` in place
  with a local label (nameless duplicate, references resolve to the first copy); unnamed with this
  unit being the class's first instantiator -> deleted (its `.rodata` strings/pool stay, like the
  original); class instance named nowhere -> kept nameless (override: modules.py `LINKONCE_DROP =
  {unit: [mangled names]}`). `.text` in place matters because the original interleaves the template
  bodies with `__static_initialization_and_destruction_0` and the deferred inlines (ss_main: cLight
  block, cModelInfo/cParts/cMap templates, static init, LightSetModel2, ~Widget, dtors, quit/init/move,
  log/~cManager/destroy<cParts,cModelInfo>, keyed ctor/dtor).
- End-of-file output order (ss_main): a global function output AFTER the static-init function
  (LightSetModel2 at 0xD5B4) is a deferred `inline` whose address the code takes; it is output by
  `wrapup_global_declarations` in `saved_inlines` order. COMPILER-DIFF candidate #8: the original
  queues synthesized destructors when they are synthesized (end of file), our cc1plus queues them at
  the class definition (`cons_up_default_function` -> `mark_inline_for_output`), so to come out before
  the widget destructors the inline must be DEFINED before the widget classes are declared:
  ss_main.cpp defines `extern "C" inline void LightSetModel2()` between `#include "sscrn.h"` and
  `#include "ss_main.h"`.
- A group of functions that follows a unit's end-of-file blocks (synthesized dtors, Widget
  quit/init/move) is a separate object: the ss_Draw_tpl/line3d/tile3d helpers after ss_item are
  `Sscrn/ss_item_draw.cpp` (real name unknown; .rodata = three 0.0f pools + 4 pad, no header strings,
  so it includes only gx/tpl/trans/camera headers; `static` `_trans` callbacks inside `extern "C"`;
  `u32 blend` parameters give the `cmpwi 1/beq; cmplwi 1/blt; cmpwi 2; cmpwi 3` tree; the tpl range
  test is `(u32) tpl - 0x80000000 > 0x02FFFFFF`, the later pointer checks two separate `if`s).
- `.rodata` alignment: a unit with a vtable has an 8-aligned `.rodata` (`.align 3` of the vtable
  sections), one without (ss_debug) 4; the split objects are all `align:4`, so a compiled ss_debug
  loses the 4-byte pad before ss_file's `.rodata` until ss_file is compiled too — flip both together.
- Two identical strings are not merged when one comes from a template instantiation and the other from
  a parse-time inline (`"D:/Bio4/Prog/widget.h"` twice): route both through one `static inline const
  char* widgetFileName()` so the inlined copies share the SYMBOL_REF.
- `if (c) return 0; body; return 1;` puts `li r3,0` out of line at the end (jump1 turns the
  `set r3; use r3; jump ret` block into a return sequence); the target's `li r3,0` before the `bne`
  that jumps to the epilogue is `if (c) ret = 0; else { body; ret = 1; } return ret;` (or `goto`).
- `u64 Key.trg & bit` tests: bit 31 of the low word is `0x80000000` (`clrrwi 31`), not 1.
- A `switch` whose two arms come out as `cmpwi 1; beq L1; cmpwi 2; bne END; [case 2]; b END; L1: [case 1]`
  has `case 2:` written before `case 1:` (ss_debug bullet, ss_file SsFileMain::move).
- A shared `state++` tail entered from a `break`-ing case and from a falling case is a `goto NEXT` label
  (`case 0: ...; goto NEXT; case 1: if (..) break; NEXT: state++;`), otherwise cse folds the second
  copy to `li r0, 2` (SsFileInit::move).
- Debug menus: column positions are `int cx = 10;` assigned *after* the header `eprintf` (so the first
  call uses the literal 0x50 while the loop keeps `slwi r3, r23, 3`), the value column is `(cx + 13) * 8`
  (hoisted `addi`, folded to `li 0x17` by cse2), row y is the giv `0x9A + i * 0xE`; the cursor colour
  `int col = i == cursor ? 4 : 0;` is one loop-body local reused by a later case (ssDbgPzzl::move).
  A clamp on a global (`pG->x4F98`) that keeps the pG register across the diamond is
  `GlobalWork* g = pG; int p = g->x; if (p >= 0) { if (p > M) p = M; } else p = 0; g->x = p;`.
- Font sizes are `s16 x[2] = {0, 17}` / `s8 x[4]` statics passed as `x[1]`/`x[3]` to
  `setFontSize(int, s8, s8)` without truncation (COMPILER-DIFF 4: `MessageControlS::setFontSizeS`
  alias in ss_file.cpp); `IdNum.killI(0xFF, 0x40 + i)` likewise (id_sys.h).
- The DLL's model managers are `cSsPartsMgr`/`cSsModInfoMgr` (ss_main.h): constructed by the DOL's
  `__9cPartsMgr`/`__12cModInfoMgr` (asm-labelled ctors) but without a virtual destructor, so the static
  destructor inlines `cManager<T>::~cManager` (stores the cManager vtable) as the target does.
- Status: ss_cap, ss_debug, ss_file, ss_item_draw Matching (the REL is byte-identical with the four
  compiled; ss_model Matching since the sixth pass; ss_main Matching since the eighth pass: SubScreenTask's two `lis
  pG@ha` are a dead test, see the eighth-pass item); ss_item Matching since the ninth pass (34/34: itemSelect and
  itemMakeMove closed with #17 pins, and its eleven .bss objects from item_list on made non-static -- the REL's
  ADDR16 fields hold A only for them, see the ninth-pass item); ss_term (Matching since the fourteenth pass)
  and ss_model (47/47 since the sixth pass: wep09Init is a plain `else if` chain, NOT compiler-build difference 6) are written, see their items; ss_map (src/Sscrn/
  ss_map.cpp, Matching since the tenth pass (105/105: mapPositionCheck closed with a tagged asm copy,
  the eof block reordered by instantiating LightSetModel2 and ~Widget before atari.h; see the
  tenth-pass item); map_room/map_room_num are
  non-static since the ninth pass for the REL fields), .rodata/.data/.bss identical since 2026-09:
  the former 8-byte gap was doorModelInit's missing 2^52 pool entry (`(f32) (int) e->ang` of the u8
  angle, the classic double trick, not a fast-cast psq_l) plus the two file-scope `static const`
  tables in the wrong order (map_cam_entire is defined before mark_model_tbl); see its item) and ss_pzzl (src/Sscrn/ss_pzzl.cpp, Matching since the twelfth pass (64/64:
  PieceSelect::move, pieceFrameDisp, caseModelMove in the twelfth; pieceTblInit, PzzlThinking::move, SsPzzlMain::init in the eleventh; pzzlCursorDisp in the tenth; pieceModelDisp in the ninth; PieceCommand::move and PieceCombine::move since the sixth), .rodata/.data/.bss identical,
  eof block order fixed (in-class `init` bodies, see the eleventh-pass item); the module COMMON block is widened by `asm(".comm _7pzlGrid.size,52,4")` and msg_open/pzzl_cursor/pzzl_sel/pzzl_dbg are non-static for the REL fields, see the twelfth-pass item) are
  written; ss_shop (src/Sscrn/ss_shop.cpp, the merchant screen: Matching since the thirteenth pass (74/74: SellItemNum::move
  closed with a tagged #13 slot filler, see the thirteenth-pass item; levelItemDisp in the eleventh, LvUpItemSelect::move in the tenth, LvUpConfirm::move in the ninth, BuyItemNum::move in the seventh) incl. the 0x980 eof block, .rodata/.data/.bss
  identical; shop_msg / shop_pos_save / shop_msg_buf are non-static for the REL's ADDR16 fields) is written, see its item and the fifth..thirteenth-pass lists.
  Sscrn.rel is byte-identical with all 11 units compiled (ss_term Matching since the fourteenth pass: out-of-line dead AddButton, the ~Widget specialization device, the dead screenPos2terminalPos pool, STRIP_UNUSED; see its item).
- ss_shop idioms (2026-09): include order light.h, map_obj.h, widget.h (the three header strings), then
  "ss_shop.dat" (SsShopInit::move) and the HALT string (mem_alloc lines 0x1BA/0x242). The 13 widgets are
  declared in the order SsShopInit, SsShopMain (ss_main.h), ShopTopMenu(3 links, ctor sets cursor = 1),
  SellMenuSelect(2), SellItemNum(2), SellConfirm(1), BuyMenuSelect(2), BuyItemNum(3, 0x20 bytes),
  BuyConfirm(3), BuyPuzzleEnd(2), LvUpMenuSelect(2), LvUpItemSelect(2), LvUpConfirm(1, 0x20); the link
  count / size of each `new` in SsShopMain::init belongs to the vtable stored AFTER it (the vptr store
  follows the `stw` of the member). SsShopMain::init also creates ss_pzzl's PzzlThinking / PieceSelect /
  CaseChange: their classes moved to include/ss_pzzl.h in the target's declaration order (PiecePopUp,
  PiecePopDown, PzzlThinking, PieceSelect, PieceCommand, PieceCombine, CaseChange = reverse of the
  .rodata vtable order 29E0..2B00; the old ss_pzzl.cpp order was wrong) and the nine pzzl / thirteen
  shop vtable labels were renamed `_vt.<Class>` by hand (the sync tool cannot resolve them).
  SUB_SCREEN gained pShop (0x204), x2B4, pShopWk (0x314, `ShopWork` 0x48) and pMerchant (0x318).
  Idioms: `tbl[(*num)++] = X` (getGreetMsg) keeps the `*num` value in a register across the `tbl[]`
  store (a re-read after the store reloads it); `if (a || b) tbl[(*num)++] = 1; else tbl[(*num)++] = 0`
  gives the `stwx r3` (the zero is levelNew's result) + shared `addi/stw` tail; two early `return 0`
  paths share one `li r3,0` only through `goto NG` to a `NG: return 0` at the end. `cMes.mes[result].flags2`
  (index = the member just zeroed) is what gives `lwzx r0, r9, rZERO` with the zero pseudo; `getMes(0)`
  folds. A function-level `IdUnit* u` reassigned by every `unitPtr()` call gives the `mr r9,r3` copies
  (closeCoat, SsShopMain::init); direct `IdSub.unitPtr(..)->flags` expressions use r3. Separate
  block-local loop counters per loop (SsShopMain::init: r29/r30/r27) keep `this` in r31. `state =
  greetIdx = greetStep = 0` (chain) reproduces `stb 0x10; stw 0x28; stw 0x24` after a call without
  reusing the pre-call QI zero. `if (x) transit(..); else ok = 0;` (then-arm = the call) lets jump2
  merge both `ok = 0; b join` copies out of line; `case 1: default:` shares the default body in the
  `str` switch. dispSellItemList / dispBuyItemList are hand-rolled goto loops (`i = top; goto TEST;
  BODY: ...; i++; TEST: if (i < end) { item = ..; pe = ..; row = i - top; if (pe) goto BODY; }`): no
  loop notes, so the 320/0.8/240 pool constants stay inside the loop and `row + 0x40` etc. are
  recomputed, while dispLvUpItemList is a real `for` (constants hoisted to f29-f31, four givs). In all
  three, the first `for (k = 0; k < 5; k++)` uses its own variable, `i = top` is assigned before the
  dispScrollBar call and `end = top + n` after it, the cursor mark is one `mark = unitPtr(0x3F)` with
  `if (cursor) |= 8 else &= ~8`, the `ItemInfo info` / `Vec pos` temporaries are block-scoped (the
  Sell frame: info 0x8, pos 0x10, second info 0x8, dispPrice's pos 0x8 via combine_temp_slots), the
  message slot is `u8 slot = row + 8`, and `ot`/`otNo` are stored through `U16Set` (ot first).
  The digit displays (dispPrice, stockNumDisp, weaponLevelDisp, levelItemDisp, SellItemNum::move)
  copy the number into a fresh `int n` AFTER the preceding `unitPtr()->flags |= 8` statement (the copy
  lands in r4 after the call: `mr r4, rNUM`); the leading-zero loop is `on = 0; for (i = N-1; i >= 0;
  i--) { if (on == 0) { if (digit[i] == 0 && i != 0) continue; on = 1; } u = unitPtr(base + i); u->flags
  |= 8; u->flags_7F |= 2; u->no = digit[i]; }`. weaponLevelDisp/levelItemDisp digit loop: `if (type ==
  3) { leading-zero skip } else if (type == 0 && i == 2 && digit[2] == 0) hide; else show;` (the
  type == 0 test survives because it is in the other arm); the bar colour is `src = unitPtr(3); if (i <
  lv) { int max = WeaponId2MaxLevel(..); src = colOn; if (lv > max) src = colOff; }` (max in a local
  before the assignments keeps src out of the call's live range, so it stays in r3); `switch (id) {
  case 0x40: .. case 0x34: ..}` (0x40 first) with `case 1:`/`case 2:`/`case 3:` written as separate
  `lv = 1` bodies. The ratio getters take the int level through `getPowerRatioI`-style asm aliases
  (COMPILER-DIFF 4, no `extsb`); `WeaponId2ChargeNumI` keeps the `& 0x1FFF` mask. LvUpConfirm::move
  writes the ItemWork::x6 nibbles through a `TuneLevel` bitfield view (`u16 fire:4, mag:4, speed:4,
  ex:4`) with `(u8) ((s8) sw->lv[0] - 1)` for the top nibble (`lbz +3; extsb; subi; clrlwi 24; slwi 12`)
  and `(s8) sw->lv[i] - 1` for the others; its message branch is `if (msg == 0x17 && (result =
  cMes.getMes(1)->result) != 0) {..} else if (msg == 0x18)` (the result == 0 path joins the second test,
  which therefore reloads msg and Key.trg). Message positions: `int x = (int)(..) + ofs_x;` before the
  MesSet with the y expression inline (loads ofs_x first; both inline loads ofs_y first and cost a
  callee-saved register); `shop_msg[msg]` read twice through the member (not a local copy). Clamps
  against 1 keep `cmpwi 1; blt` only through a variable (`int min = 1`), a literal folds to `<= 0`;
  the 0x54..0x55 test in the shop's itemTexNo is `id <= hi && id >= lo` with `int hi/lo` locals (a
  literal range folds to `subi/cmplwi`). `if (p->num >= left) { for (i = 0; i < left; i++) dump(p);
  break; } left -= p->num; dumpAll(p);` gives the reversed count-down dump loop with the subtract block
  out of line (SellConfirm). setOrientation: `m->rot.x = m->rot.y = m->rot.z = 0.0f; m->scale.x = .. =
  1.0f;` chains give the a0/ac/a8/a4/b4/b0 store order; the place table is indexed (`tbl[i].rot`) for
  the two givs. dispItem clears `be_flag & ~2` (rlwinm 0,31,29). screenPos2worldPos loads `pos.z` into
  a local before the `tan` call (f31). `sw->pShop = (SsArc*) (wk->aramSize + (u32) wk->pBuf)` (offset
  first). `MesData.setPtr(0, ..)` / `setPtr(2, ..)` give the `stwx r0, r9, rZERO` / `stw 8(r9)` pair.
  Open (register allocation / scheduling only unless noted): dispSellItemList & dispBuyItemList
  (`add end` scheduled after the dispScrollBar call and the call passing `top` not `i`; the
  frame/text `unitPtr(row+0x40)`/`unitPtr(row)` calls use a fresh `lis IdSub@ha` (Sell: one
  callee-saved r30 for both; LvUp: rematerialised `lis r9` per call) that gcse never unifies with the
  hoisted copy - a second `extern IDSystem IdSub2 asm("IdSub")` decl splits the expression but ours
  then hoists it), dispLvUpItemList (same `lis` + register names), levelItemDisp (-0x18: the same
  `lis`, `val[cur]` index form, digit/pos slot sharing, frame 0xB8 vs 0xC0), SellItemNum::move
  (`this` r24 vs val r25 swap = val has the higher global-alloc priority in the target; the inner
  `i`/magic-constant pair r28/r29 swapped too), BuyItemNum::move (+8: the `SndCall(0,5)` tail of the
  cancel branch is cross-jumped into case 1's `li r8,0; bl` in the target, compiler-build difference
  6), LvUpItemSelect::move (item r28 / x r30: x outranks item in the target; a do-while around the
  x/y conversions inverts the order but its loop notes stop the `lis cMes` interleave), LvUpConfirm::move
  (-4: the top nibble's `extsb` before `addi -1; clrlwi 24; slwi 12`: combine folds the sign
  extension into the u8 truncation in ours), dispPrice (the num block's `&digit` pseudo is r28 in
  the target (allocated after the loop counter and the digit pointer) and r31 in ours; the price
  block matches).
  Solved in the third pass (2026-09-10): screenPos2worldPos = `out->x; out->y; out->z = 0.0f` (z
  last), stockNumDisp = block-scoped `for (int i ...)` counters (one `int i` shared by the two loops
  gave the digit pointer r29 and the counter r31), BuyConfirm::move = `int act;` initialised AFTER
  the `if (noRoom == 0) dispBuyItemList(..)` diamond, `if (noRoom == 0) act = 1; else act = 3;`
  (jump1 hoists the else-set: the target's `li r29,3` before the compare) and `int min = 1; if
  (!(act < min))`, weaponLevelDisp = `digit[i]` (not `digit[2]`) in the `i == 2` test (one more use
  of the `&digit` pseudo lifts it above numBase in global-alloc priority: floor_log2(8) = 3),
  block-scoped loop counters, `int barBase = 0; int numBase = 0;` declaration order (hoisted zero
  `li`s come out in declaration order) and `if (lv > WeaponId2MaxLevel(id, type)) src = colOff;
  else src = colOn;` (the hoisted else-set lands between the compare and the branch, so `src` can
  share r3 with the call result; an explicit `src = colOn` before the compare schedules above it).
- ss_map idioms (2026-09): include order light.h, map_obj.h, widget.h, atari.h (the cSat/Widget/
  cUnit vtables come out in that reverse order after the widget vtables). The unit defines its own
  `extern "C" inline LightSetModel2` before ss_main.h (the module's second copy, nameless 0x2C at
  the eof before ~cSat / ~Widget). Uninitialised statics are declared BEFORE `#include "ss_main.h"`:
  its externs of ssPlModel/ssWepModel (defined here) would otherwise put them first in .bss
  (first-declaration order). Small local arrays of <= 8 bytes (`int x[2]`, `int x[1]`, `u8 x[4]`)
  have an integer mode, get a pseudo at declaration and only receive a frame slot when their
  address is first taken (put_var_into_stack): their slots follow all BLKmode locals, in
  address-taking order (markGoalPosition: the `int[2]` table lands behind the 3/4/6-element ones
  although declared between them; the `int[1]` singletons follow in switch-case order), while
  their .rodata templates keep declaration order. `static const Vec` locals of a `static inline`
  helper defined between two callers are the single .rodata copy at the helper's position
  (mapModelLight before mapModelInit, after the mapColor tables); file-scope `static const`
  tables (mark_model_tbl, map_cam_entire) come out at the eof after the vtables. getAreaNo is a
  `switch ((u32) room)` with `case a ... b:` ranges (`cmplwi` trees), `case 0: default:` at the
  top and no trailing return (the surviving `return 0` copy is stage 3's inner default). `(x &
  (1 << n))` folds to `sraw/andi.`; `u32 bit = 1 << no;` keeps `slw/and.` (mapModeCheck). The
  8-float `f32 tbl[8]` .data statics are debug camera presets of which only [0] is read.
  Fourth pass (2026-09-10, 88 -> 102/105; harness /tmp/ssw6: try.sh = cc.sh + fdiff3 per function,
  var.py = source-variant loop with bcmp's per-function verdict, dump.sh + fnd.sh for the cc1plus
  dumps; NOTE fdiff3's LEFT column is the split object = target, RIGHT = ours):
  - zoomMove: `CameraParam* from = &m->from; CameraParam* to = &m->to;` pointer locals (a user
    variable pseudo is not combined into the `addi r3` of the call and sched1 hoists it above the
    first call into r30 with `mr r3, r30` uses; the inline `&m->from.pos` is combined into `addi r3,
    m, 4024` after the call) and `memcpy((u8*) pG + 0x138, &up, sizeof(Vec))` for `pG->Cam.up = up`
    (byte-pointer destination: the store may alias pG, which is reloaded for the next call).
  - mapChangeViewport: `static int map_vp_init[1]` (the one-element array of the SsFileInit idiom):
    the in-struct `map_vp_init[0] = 1` store is ordered against the `vp` frame copy's loads and is
    issued between `vp.x` and `vp.y`; a plain scalar store has no dependence and sinks to the end.
  - mapPos2screenPos: `f32 ang = fovy * 0.5f * 0.017453292f;` BEFORE `az = fabsf(out->z)` (the
    puzzlePos2screenPos idiom) and BOTH scale factors into locals (`kx = pMapWk->sw * 0.5f / w; ky =
    pMapWk->sh * 0.5f / h;`) before the `out->x *=` store (the store through `out` forces a `pMapWk`
    reload otherwise); `out->x += cx; out->y += cy` stay member reads (reloaded per statement).
  - The flag-table helpers are written index first: `*(u32*) ((no >> 5) * 4 + tbl) & (0x80000000 >>
    (no & 0x1F))` with `tbl = (u32) &pG->flags_51BC` / `(u32) pG->door_unlock` / `(u32) pG->item_flags`
    (`addi rT, pG, 0x51bc; lwzx r0, rIdx, rT`: index register first, the same shape for all three
    tables so jump2 cross-jumps the flagType 1/2 arms of doorModelDisp; the `pG->door_unlock[i]`
    array form gives `lwzx rT, rIdx` and the cast `((u32*) &pG->flags_51BC)[i]` folds the offset into
    the load displacement).
  - markMerchantPosition: `n = ..; tbl = ..;` (n before tbl in every case -> n r6, tbl r7), `if (idx
    != -1) {..} else return 0;` (the `li r3,0; b END` block out of line after the arm), and BOTH arms
    end with `*pos = p->pos; return 1;` (jump2 cross-jumps arm 1's `bl getPartsPtr` + tail into arm
    2's; with a shared tail after the if/else the else-arm's call ends its block, flow appends `use
    (const_int 0)` and only the tail after the call merges); the else arm is `cModel* mdl =
    m->pMerchant; if (no < mdl->nParts) { p = mdl->getPartsPtr(no); *pos = p->pos; return 1; } return
    0;` (jump2's `x = a; goto l; x = b` rule hoists the `li r3,0` above the compare because the
    fall-through block starts with `mr r3, mdl`).
  - markGoalPosition: the four `int[1]` singletons for areas 8/15/16/17 are `{0}` (the target stores
    the loop counter register, known 0 after the st1 copy loop, into them) and `int none[1] = {0}` is
    declared FIRST (its `li r0,0; stw` precedes the copy loop; the others come after it and reuse
    the counter); `SsMapWork* m = wk->pMapWk` local (one load kept in r3 across the stageFlag loop).
  - markTreasureExist: `u8 st4d[12]` (the 13th 0 byte was .rodata padding; the copy is 3 words).
  - markTreasureDisp / markCoinDisp: block-scoped `for (int i ...)` counters per loop and the
    `u->flags &= ~8; u2->flags &= ~8;` hide statements duplicated into BOTH else arms instead of a
    `continue` to a shared tail (the extra refs at flow time give `u` the top priority (r31) over
    `&scr`, and the longer loop keeps IdNum's `lis` below IdSub's in priority; jump2 cross-jumps the
    copies away).
  - MapModeSelect::init: `pGS->flags_51C0` (struct view) keeps the `lwz pG` below the four timer
    stores. MapModeSelect::move: the Key.trg tests are `if (trg & A) { switch (modeSel) { case 0: ..;
    return; case 1: ..; return; } } if (trg & B) {..} else {..}` (the switch's fall-out reaches the
    second test, so its label has two uses and cse reloads Key.trg there); `mapModeCheck/Change(SUB_
    SCREEN*, s8 no)` (the int `i` is `extsb`'d at the call); the cursor clamp is `m->modeCursor =
    m->modeCursor < 0 ? 3 : (m->modeCursor > 3 ? 0 : m->modeCursor); if (old != m->modeCursor)`
    (the ternary is expanded straight into the QI member store: `mr r9, r11` of the raw byte, `li
    r9, 0/3`, one `stb`, and the compare re-extends the stored register `extsb r0, r9`).
  - SsMapInit::move: `SS_ARC_PTR(wk->pCmmn, 0xC)`, `static int map_wait[1]` with `map_wait[0] = 0`
    (the switch register, known 0 in case 0, is the stored value: `stw r30`) and its own `state++;
    break;` in case 0 (the `lis` gets r11), `FadeSetW(0x80000000, 5, 0, 0)` (fade.h) for the colour
    pair (frame 8/12; `result`/`size` at 16/20).
  - SsMapMain::move: `Widget<SUB_SCREEN>* w = cur; w->move(wk); cur = w->cur;` (cur kept in r30
    across the virtual call), `mapModelDisp(SUB_SCREEN* wk)` (unused parameter, the caller passes
    wk) and the exit body `wk->x34 |= 4; transit(4, wk);` written in BOTH Key.trg arms (type == 2 /
    else) instead of a `goto EXIT` (jump2 merges the copies; the surviving copy is the else arm's).
  - doorModelInit: `e[i].parts` / `e[i].ang` indexed off the table base (no `e++`): the target has
    the `i*5` giv (`lbzx rParts, rGiv, rTbl`) plus a separately reduced pointer for `.ang` (`lbz
    1(rP)`), which the stepping-pointer form folds into one; mapModelLight stores `x135 = 2; x12F = 3`
    in that order (`li r9,2; li r0,3; stb 303; stb 309`).
  - mapModelInit: block-scoped `int n = mapRoomNum(..); int j;` per outer loop (j r28, n r22/r31),
    the floor index `mdl->getPartsPtr(0)->mat[1][3] / 100.0f + 0.5f` (not `pos.y`), `wk->mapFloor =
    (s8) (y + 0.5f)` / `(s8) (y - 0.5f)` per arm (no `y +=`), and `pLog.p->warn(..)` (the member read:
    `lis; lwz` adjacent, so loop.c leaves the `lis` inside the 72+-insn colour loop where the
    `operator->` form's `lis; addi; lwz` has lifetime 3 and is hoisted).
  Open (ss_map): mapModelInit (one register: the hoisted `j + 1` increment is r4 in the target, r11
  in ours - alloc order r0, r9, r11, .. with only 0/1/9 conflicting), mapColor (+0x14: the spill
  slots of the 17 table addresses rotate (gcse hash order) and the target keeps ONE `lis pG@ha`
  pseudo (r29) across the `checkPassed` call for the three stageFlag tests while ours reloads it),
  mapPositionCheck (register allocation only: the target constructs satA (frame 200) fully
  r1-relative (`stw 0,200(1); stw 11,208(1); stb 10,242(1)`) but satB (frame offset 0) with the
  vptr stores through the `this` pseudo r27 and the member stores r1-relative, and keeps that pseudo
  to the destructor (`mr r14, r27`); ours has `this` pseudos for both ctors' `flags = 0` byte stores;
  the `idx = -1` is set before the `if (multi)`, `poly = satB.poly; ... i++, poly++` pointer giv in
  the hit loop, `poly = satB.poly; poly += idx;` for both `&satB.poly[idx]` (a reassigned pointer
  keeps `lhz 0(rP)` where `&tbl[idx]` gives the `lhzx rBase, rOfs` index form), and no explicit
  `be_flag = 1` after the ctors (the ctor already stores it; with the extra stores the value lives
  in a callee-saved r27 and is stored twice)).
- ss_pzzl notes (first pass, 2026-09): pzlBoard+0xC is a Mtx (puzzle.h `mat`, the board -> world
  matrix caseModelMove sets); the grid cell size is a static member of a local class
  (`pzlGrid::size`, the first word of the module's COMMON block, set to 100.0 by caseModelMove);
  the line width statics are TWO `u16` scalars per line kind (`x_ot = 0xF; x_prio = 0`, each
  loaded with its own `lis`; a `u16 x[2]` pair shares one base register); `.data` A44 is the
  message-open flag, AD0 a 20-byte unreferenced table behind an `int = 0`; the second
  `setCommandId` of the module is `static` here.
- ss_pzzl second pass (2026-09, 45/64 byte-identical, .rodata/.data/.bss identical): the Mtx copies
  are the word-copy loop, written as `MTX_COPY` = `MtxPtr d_ = dst; int i_ = 2; MtxPtr s_ = src;
  do { dp_ = *d_; sp_ = *s_; for (j_..4) *dp_++ = *sp_++; d_++; s_++; } while (i_--);` (the
  `li rX,2` of the counter is issued between the `d_` and `s_` inits in every copy of the unit, which
  the motion.cpp `while (i_--)`/`i_ = 3` form gets wrong for a `src` that needs an `addi`).
  caseModelMove: `const f32 size = 100.0f;` at the top (its pool entry precedes the 2^52 double;
  the store is `FSet(pzlGrid::size, size)` so the following `wk->x2B0->caseBoard` load stays below
  it), the `(f32) b->w` conversion is the double trick of an `int w = b->w;` local, two blocks share
  the frame (`{ Mtx tmp; Vec p; Vec ax, ay, az; }` then `{ Vec scr2; Mtx mat; Vec t; Vec p; Mtx
  mat2; }`: the freed block-1 slots are reused best-fit, the 0x10 remainder at 0xA8 stays a hole and
  mat2 gets a fresh slot), the board matrix is rebuilt column by column from three Vec axes
  (`ax.x = m->mat[0][0]; ax.y = m->mat[1][0]; ...; tmp[0][0] = ax.x; ...`) after a copy loop of
  m->mat, and only `m->mat[1][1]` is read through a `f32* col = &m->mat[0][1]` pointer (`lfs
  0x10(r6)` with the `addi r6, r28, 0x10` hoisted before the loop). Residual: one more callee-saved
  register (sw/u/parts allocation). Other idioms: the `y + 1`/`x - 1` neighbours passed to
  `getPiece`/`cellState`/`cell` are `(s8)` casts (`extsb` before the call, as puzzle.cpp does);
  `g = pzlGrid::size` is read after the copy loop and `item = 0` after the early return
  (drawCursor); `a.x = 0; a.y = 0; a.z = 0` in x, y, z order comes out x, z, y (drawGridLine),
  whose colour/ot/prio statics are read into `u32 col; u16 ot, prio;` locals before each loop pair
  (callee-saved, no reload after the calls) and whose board is read as `wk->x2B0->caseBoard->h`
  twice (a `pzlBoard* bd` local reassigned for the space board is a global-alloc pseudo in r3);
  cmpVer walks a `Vec* p = c->v; ... p++` pointer; screenPos2puzzlePos reads `pos.z` (not .y)
  into a local before `tan`; puzzlePos2screenPos computes `ang = fovy * 0.5f * 0.017453292f`
  into a local BEFORE `az = fabsf(out->z)` (the volatile asm is a barrier: constants expanded
  after it stay after it); the shared light set is one `static inline pzzlModelLight(m)` (single
  `static const Vec` pair between pieceModelDisp and pieceModelInit); pieceFrameDisp's
  `VECNormalize:[%s/%d]` uses `__FILE__` under `#line 1158 "D:/Bio4/Prog/ss_pzzl.cpp"` (one FILE
  string shared with SsPzzlInit::move's DVD_READ_N); pzzlEquipDisp has two unused 0x20-byte
  locals (`u8 x[0x20]` after `scr` and after `pos`: frame 0xB0, pos at 0x58, info at 0x88),
  `col = colorRRGGBBAA(..)` in a local before `pieceFrameDisp(arm->model, col, 3)` (a nested call
  argument makes ours precompute `arm->model` into a callee-saved register) and two separate
  `arm = pl->piecePtr(..)` calls in the if/else (cross-jumped); drawCursorInit is `const f32 big =
  1000.0f` (its `lis` kept in r28 across the calls) with the corners written v[0].x, v[0].y, ..
  v[3].y (dying-first puts v[3].y / v[2].x first); `if (c) ret = 0; else { body; ret = 1; }`
  (back2PieceSelect), `case 1:` before `case 0:` (tempSpaceDisp), `if (n != 0) { .. ret = 1/0 }
  else ret = 1` (isTerminable: the `n == 0` arm last); sscrn_pzzl_in/out_init use one reassigned
  `IdUnit* u`; itemCommandType's case 6 keeps an `int id = item->id` with `switch ((u16) id)` and
  calls `itemCombineCheckI` (item.h int view, COMPILER-DIFF 4) on both paths (an asm-labelled
  alias never cross-jumps with the plain declaration: the symbol_ref string differs). numDisp calls
  use the `numDispI` int view (no `clrlwi` of `0x40 + i`).
  Third pass (2026-09-10, 45 -> 54/64):
  - setCommandId: `u8 t = 0x1D; if (lang == 0) t = 0x1C;` (`li 28,29; beq; li 28,28`; the ternary
    hoists the other constant).
  - itemCommandType: case 6 reads `item->id` directly in the inner `switch` and in
    `itemCombineCheckI(item->id)` (no `int id` local), the inner cases in the order `case 8 ... 0xA:
    return 8; case 0x19: case 0x1C: case 0xA8: return 3;` (with the `return 3` group first the last
    compare is inverted into `bne DEFAULT; b RET3`; the target keeps `beq RET3; b DEFAULT`), and both
    combine checks as `if (itemCombineCheckI(..) == 0) return 4; return 5;` (`li r3,4` hoisted, `beq
    END; li r3,5`).
  - pieceTblInit: the loop is `PieceInfo* tbl = piece_info; for (i = 0; tbl[i].id != 0xFFFF; i++)`
    with `tbl[i].id` read at every use (the target has the `i*120` giv `lhzx r0, rGiv, rTbl`, a
    separate store pointer and re-reads the id in the default arms; the `p++` pointer form gets one
    `lhzu` biv plus a gcse PRE copy of the id), the mdl group for 1 is `case 1 ... 2: case 0xE:`
    (not 0xB..0xE: tools/research/casetree.py reproduces the target's compare sequence), and `mdl = mdl * 2 +
    4;` is a statement between the two switches (`add r9,r9,r9; addi r9,r9,4` before the tex
    compares, `slwi 2` at the SS_ARC_PTR use). Left: `lwzx` operand order of both SS_ARC_PTR loads
    (target index first) and the store pointer based at `model[4]` (`stw -4(r6); stw 0(r6)`).
  - pieceModelOrientation: the rotation arms store in the orders case 1/2/3 `z, x, y`, case 4 `x, y,
    z`, case 6 `x, y, z` (brute-forced), and `FSet(m->pos.x, pzlGrid::size * (p->x + 0.5f))` (the
    scalar-reference store forces the reload of the static `size` for `pos.y`).
  - getPieceVertex: `ModelBound* bd = &m->pInfo->bound;` (`addi r9, pInfo, 56; lfs 24(r9)`, one
    pInfo load) and the centre `c.x = m->mat[0][3]; { Vec* pc = &c; pc->y = ..; pc->z = ..;
    PSVECAdd(pc, out, out); }` (first member via the frame, the rest and the argument through the
    pointer pseudo: the r102 `ang` idiom).
  - pieceFrameDisp: the corner loop is `for (i = 0; i < 4; i++) { switch (i) { case 0..3: fabsf
    signs } ...; PSVECAdd(&c, &ofs, &v[i]); }` (biv elimination rewrites the `switch (i)` compares
    into pointer compares against `&v[1]`.. pseudos: the target's `cmpw rP, r21; beq; blt` tree),
    `ModelBound* bd` block as in getPieceVertex, no unused `Vec d` (frame 192), case 4 scales by
    `m->pos.z` and its loop is `for (i = 0; i < 4; i++) PSVECSubtract(&v[i], &c, &v[i]);` (no entry
    test, `cmplw` against `v + 36`). Left: the case-0 loop's `lis` hoists (the target hoists the 0.0f
    pool high (`lis r14`) and the format string, not the `__FILE__` string; ours the two strings).
  - pieceModelInit: `pzlPiece* p = &pl->pieces[i]; p->model = getWork(i + 4); pzlPiece* q =
    &pl->pieces[i]; q->model->be_flag &= ~2;` (two block-local pointers: the destination address is
    computed before the inline call and both `add` are pointer-first).
  - pieceModelDisp: `if (spaceBoard->search(p)) m->x12F = 1; else m->x12F = 3;` (the target's
    polarity: `li r0,3` hoisted, `beq` skips `li r0,1`), the hand piece sets `m->pos.z` (156, not
    scale.z), `pzzlItemInfo(id, &info)` = a `static inline` wrapper around itemInfo (the `&info`
    frame address is recomputed per call instead of PRE'd: COMPILER-DIFF 3 lever), block-scoped loop
    counters. Left (+4): the `item->x8 & 0x1FFF` mask is `clrlwi r4, r4, 19` in BOTH numDisp arms in
    the target (in place on the loaded pseudo) while ours PREs one `clrlwi` above the `>> 13` test
    (in-place `num &= 0x1FFF` per arm, `u32`/`u16` locals and re-reads all get PRE'd).
  - SsPzzlInit::move: `SS_ARC_PTR(wk->pCmmn, 0xC)`, `static int pzzl_wait[1]`, `pzzl_wait[0] = 0;
    state++; break;` in case 0, `if (wk->x266 == 2 && wk->type != 4) { free.. } else { clear }`
    (the && form lays the free arm out as the fall-through with `beq CLEAR`), `LifeMeter* life =
    &Cckpt.life;` declared after `sscrnLightClear` (`lis/addi` below the call, `mr r3` per call),
    `FadeSetW(0x80000000, 5, 0, 0)`.
  - SsPzzlMain::move: `ItemWork* item` set in both arms with `id = item->id` in both (the shared
    pseudo gets r4, the tails cross-jump), `Widget<SUB_SCREEN>* w = cur; w->move(wk); next = w->cur;`,
    `int h = wk->x2B0->caseBoard->h; curY = h - 1;` (the int local keeps the `extsb`; a direct
    `h - 1` is narrowed into the byte store), `int bullets` declared before `u16 armId` (bullets
    r27, armId r26), and `u16 no = WeaponId2WeaponNo(..); u16 type = WeaponId2WeaponType(..);`
    (COMPILER-DIFF 4: the u8 results assigned to u16 locals are `clrlwi 16`'d).
  - SsPzzlMain::quit: `wk->x300 = 0; wk->x40 = 0;` in that order (with x40 first the arm's last insn
    is the else arm's `stw x40` and our jump2 merges the single-insn tail: COMPILER-DIFF 6) and
    `pzzl_dbg.quit(wk)` through a `void quit(SUB_SCREEN*) asm("quit__9ssDbgPzzl")` declaration in
    ss_pzzl's view of ssDbgPzzl (the caller passes wk to the parameterless ss_debug.cpp quit).
  - PieceCommand::init: the member is `inSpace` (1 when the piece is NOT on the case board; the old
    `onCase = search() != 0` had the polarity inverted), written `if (caseBoard->search(pzzl_sel))
    inSpace = 0; else inSpace = 1;` and `if (pc.y > half.y - 0.5f) lower = 1; else lower = 0;` (if/else
    constant stores: `li; beq/ble; li; stb` and the member re-read `lbz r6; extsb` for the
    setCommandId argument; the `= cond` forms give `mfcr` store flags and a callee-saved 0.5f).
  Open (ss_pzzl): PieceCommand::move (-0x98: the nested `command_id` switch tables), PieceCombine::move
  (-0x10), PieceSelect::move (-4: pl/board pointer allocation in the prologue), PzzlThinking::move
  (+8), SsPzzlMain::init (-8), caseModelMove (one callee-saved register), pzzlCursorDisp (+4: case
  2's `lwz r0,0x2b0; mr r3,r0` = the load scheduled above the `mr r30,r3` col copy, so it cannot tie
  to r3 - case 1 of the same code ties; interblock priorities, compiler-build difference 5),
  pieceTblInit / pieceFrameDisp / pieceModelDisp residues above.
- **The module was compiled with `-fno-implement-inlines`** (config/G4BE08/modules.py `CFLAGS`,
  wired through configure.py's `REL_CFLAGS`): SubScreenTask creates every screen's Init/Main widget
  with per-class link counts (`SsFileMain` 5, `SsItemMain`/`SsPzzlMain` 6, `SsMapMain` 5,
  `SsCapMain` 2, `SsExitMain` 0), which needs in-class constructors
  `SsFileMain() : Widget<SUB_SCREEN>(5) {}`, yet no unit of the module has a constructor body (the
  .sym lists none, the nameless blocks are exactly the cManager<cLight> / ~Widget / quit-init-move
  copies). With the default flags g++ 2.95 emits every in-class inline member of a vtable-owning class
  out of line (`__10SsFileMain`, 0xA4); cp/decl2.c `import_export_decl` makes a non-virtual inline
  member external when `!flag_implement_inlines`, so the flag removes exactly those bodies while the
  vtables, synthesized destructors and virtual inlines stay (verified: the three matched units are
  byte-identical with and without the flag). All widget classes are now declared in ss_main.h
  (SubScreenTask needs their sizes and vtables); each unit's own classes keep their relative order
  (vtables are emitted in reverse declaration order per unit: ss_main's are SsExitInit, SsExitMain,
  SsItemExamine — SsItemExamine last).
- `static int file_wait[1]` (one-element array): the in-struct store `file_wait[0] = 0` keeps the
  following `state++` load below it and stops cse from folding case 1's `state++` to `li r0,2`; the
  case-0 block then has two pseudos and the `lis file_wait@ha` gets r11 instead of r9 (SsFileInit::move).
- A loop variable shared by two identical loops (`int i, n` at function scope for both language
  branches) gives both loops the `mr r10,r9; mr r11,r10` shape; per-branch locals lose them in the
  second loop (getTplName).
- Message slot address as one expression on a pointer variable, not the `getMes` inline
  (`SS_MES(pm, no)` = `(no) * sizeof(Message) + (u32) (pm) + sizeof(u32)`, `MessageControl* pm = &cMes`
  block-local): a reference argument built from it (`U16Set(SS_MES(pm, slot)->charSpace, v)`) is
  computed in place into the parameter register (three sets of one `reg/v`), which makes cse lose the
  `slot * 0xEC` product; the next `SS_MES(pm, slot)->lineH = zero` re-multiplies and gcse PRE turns it
  into the `mulli; mr r9,r11; add r11,r11,r3; add r9,r9,r3` pair (dispFileList, mes.cpp setLayout).
  The plain store with a `u16 zero = 0` local keeps `addi r9,r9,4; sth 0x76(r9)` unfolded; the
  reference store folds to `sth 0x7c(r11)`. The zero local declared next to the stores has lifetime
  >= 2 so loop.c hoists it (`li r19,0` in the preheader, threshold 71 x savings x lifetime >= 140
  insns); declared at the body top it is hoisted too but its `li` lands before the `lis cMes@ha`.
- Loop-body `int x, y` (block-local) give the loop its own pseudos: the title's x/y stay r30/r29 and
  the loop's get r28/r29 (dispFileList); with function-level x/y both share registers.
- `if (layout == 1) u->scr = IdSub.unitPtr(0xFD, 0x1E)->scr; else u->scr = IdSub.unitPtr(0xFB, 0x1E)->scr;`
  (the struct copy repeated in both arms) is what lets jump2 merge the two call tails and PRE the
  `lis r28, IdSub@ha` of both arms to the function top (MessageDisplay::init); a ternary index gives
  one call with a `clrlwi`, plain arms leave `li r5; bl` unmerged (the `use` after the call).
- `S16Set(x, ...)` for the member stores before a `pSys->language` read keeps the `lwz pSys` below
  the `sth`s; store order `state = 0; tplState = 0; tplFirst = 1` gives the target's
  `stb 0x12; stb 0x11; stb 0x10` (dying-first rule).
- `u8 page = fw->page` plus direct `fw->page` reads in the same ebb: the local becomes
  `lbz r8; clrlwi r27,r8,24` (the direct reads cse to the QI load pseudo, so `fw->page++` is
  `addi r0,r8,1`); with only the local in use the load is a plain `lbz` (MessageDisplay::move).
- `int* pReq = &file_tpl_req; sprintf(..); ...; *pReq = DVD_READ_N(..)` puts the `lis
  file_tpl_req@ha` in a callee-saved register before the sprintf; `x = call()` expands the call first
  (expr.c expand_assignment CALL_EXPR case) and loads the high part after it.
- ss_file's `.data` is 4 bytes short of the split object: `asm(".section .data; .balign 8")` at the end
  (the next unit's `.data` starts 8-aligned); ss_main the same.
- ss_main: `switch (info.type)` (not if/else-if) for the `cmpwi 1; beq; cmpwi 9; beq; b` chains;
  `&info`/`&size` recomputed per call through `static inline` wrappers (`ssItemInfo`, `ssReadCheck`);
  `PSet(wk->x240, wk->x23C)` keeps the following `lhz exam_id` below the store; `exam.move();
  exam.trans(); ... exam.quit()` (member calls, no local pointer) give the `mr r26,r30` PRE copy;
  the static const Vecs of `cLightInfo::init2` are function-local statics (emitted before the pool);
  numDisp takes `u8 id` (no `clrlwi` at the `unitPtr` calls) and copies `col0[0..3]` byte by byte
  (a struct copy is `lwz/stw`); weaponChangeRequest is `if (x4FB8 == 1) return; switch (x4FB8)
  {case 0: case 2..5:}` (the `cmpwi 1; beqlr` is a separate if); weaponChangeMoveCheck is
  `x250 != 3 && x250 != 4` (`subfic/subfe/neg`); `BitOn(ssPlModel->be_flag, 2)` reloads the model
  pointer for the following `alpha` store; the character switch order is Leon, Ashley, Ada,
  Krauser(4), HUNK(3), Wesker (as playerModelInit). sscrnCameraInit needs `const f32 zero = 0.0f`
  for the pool order (0.0 first).
- SubScreenTask: `SsTermMain* termMain = 0` and the other three null widget pointers are declared
  BEFORE `exitInit = new SsExitInit` (their `li`s are scheduled around the `__builtin_new` call and the
  first ctor's `i = 0` cse's to the first of them), `cur = 0` after `exitInit->connect`, and every
  branch calls `cur->init(wk)` itself (jump2 merges the call tails into the `& 0x40` branch's copy;
  the final else copy stays because of the `use` after the call). The model loops call through a
  function-pointer local (`void (*func)(cModel*) = sscrnModelTrans; for (m = MapMgr.pAlive; ...)
  func(m)` -> `mtlr r31; blrl`). `MotionMoveF(m, 0)` (pl_npc.cpp alias) for the `li r4,0`.
- SOLVED (ss_main sscrnCameraInit): the source order is `pos.z, up.y, at.x, at.y, at.z, pos.x,
  pos.y, up.x, up.z, fovy` — the LAST zero store in the source (`up.z`) carries the zero register's
  death and is issued first among the zero stores (weight rule), the others follow in source order,
  and `fovy` written last has its pool load issued last so `up.z` slips in front of it.
- ss_main SubScreenTask (third pass, 2026-09-10; -8 bytes, only the `lis pG@ha` placement left):
  SOLVED the `cur` r28 / `&MapMgr` r23 allocation, the `cur->init(wk)` tail cross-jumps and the
  digit loop with: `cMapMgr* mgr = &MapMgr;` as the FIRST statement of the `while (wk->x28)` body
  (loop.c hoists `lis`+`addi` as one 2-use movable; declared inside the `if (wk->x44 == 0)` block
  the set is `maybe_never` and used in two blocks, so it is not movable at all; the two direct
  `MapMgr.pAlive` reads give two pseudo pairs whose first one combine folds into `MapMgr+16@ha`),
  the model loops as `m = mgr->pAlive; while (m) { cModel* p = m; m = (cModel*) m->next; func(p); }`
  (the `next` load precedes the `blrl`), `exitInit != cur && exitMain != cur ...` (compare operand
  order `cmpw r20, r28`), the digit loop `for (i = 0; i < 8; i++) { u = IdSub.unitPtr(i + 1, 2); ..;
  u->no = d[i]; }` (giv incremented after the load: `lbz 0,0(r31); addi r31,r31,4`, no `lbzu`),
  and `SsTermMain::TermSub` padded to 0x40 (sizeof 0x8C: `li r3, 0x8c`). With `cur` in r28 the
  fall-through arm's `mr r4,r21; lwz r9,0xc(r28)` order matches and jump2 merges every arm's tail.
  OPEN: the target has three separate `lis pG@ha` for the three `pG` reads in the loop body (two
  in the block before the `ssWepModel2` test = speculative interblock motion of the weapon-switch
  arms' highs, compiler-build difference 5; one fresh in the digit block), ours combines them in
  loop.c (`combine_movables`: `rtx_equal_for_loop_p` compares SYMBOL_REFs by XSTR pointer) into one
  movable (savings 3, life 3 >= 307 insns / threshold 65) hoisted to r24; asm-labelled `pG` aliases
  would defeat the combine but not reproduce the speculative placement. The cManager<cMap>::log copy
  is handled by the linkonce rule above.
- ss_item (src/Sscrn/ss_item.cpp) idioms: cursor state is a 9-byte `ItemScreenWork` (sscrn.h) at
  SUB_SCREEN+0x304 (`col`, `idx[2]`, `sel[2]`, `comb[2]`); the debug item-make state is the tail of
  the 0x34C debug block, addressed as one struct (`SsItemMakeWork`, `addi rX, wk, 0x34c` +
  displacements 0x1C/0x20); `int item_wait[1]` one-element array (the `lis` in r11, SsFileInit
  idiom); font statics `s16 w[2] = {0, 0x12}; s16 h[2] = {0, 0x18}; s8 space[4] = {-1,..}` used as
  `[1]`/`[1]`/`[3]` through the `setFontSizeS` alias; `itemTexNo`'s local `u8 tbl[105]` template
  (the label's 0x6B is padding to the pool); the `.data` `const char*` table and the two `int`s of the
  item-make menu are defined right before it (strings after itemFrameSet's pool); `IdNumN.unitPtrN
  (int, int)` / `IdNum.setI` / `numDispI` int views where the target has no `clrlwi`; a hidden `case
  0: break;` in itemFrameMove's state switch; `off = 0; if (!(flags & 1)) off = 1; if (off) HIDE
  else SHOW` with `goto HIDE` from the other condition (the `li 0; xori; andi.; beq; li 1; cmpwi`
  chain, HIDE laid out first); `d = old - iw->idx[col]` read back right after the store (forwarded
  register + `extsb`, the -1 store after it); `int no = i + 1` inside the unitPtr loop (`mr r31,r30`
  increment); `JOY* joy = &Joy[0]` local in itemMakeMove; `PSet((void*&) wk->x248, item_sel)`
  reloads `item_sel` for the following compare; ItemCommand::move: block-local loop counters per
  `dir |= 0xF` loop (r8, not a callee-saved register), the mode switch written `case 2, 0, 1, 3`
  (layout order), `!(mode > 2)` / `int min = 1; !(mode < min)` nested (a literal 1 folds to `<= 0`).
  Third pass (2026-09-10, 25 -> 30 byte-identical): SsItemInit::move case 0 writes its own
  `state++; break;` (jump2 cross-jumps it into case 1's tail; at allocation time the block has two
  pseudos, so the `lis item_wait@ha` gets r11 - a `goto NEXT` to the shared tail leaves one pseudo
  and r9); SsItemMain::init = `itemCameraInit(wk, &pGS->Cam)` (pG load below the `cur = sel` store)
  and block-scoped `for (int i ...)` counters (a function-scope `i` shared by the 32-loop and the
  2-loop is one pseudo with 4 sets that outranks the IdNum high for r31); ITEM_PTR = `if (idx < 0 ||
  idx > n - 1) goto DUMMY;` with `DUMMY: return &item_dummy;` after the final `return
  ItemMgr.at(no)` and `if (no == 0xFF) return &item_dummy;` as the fall-through (cse follows only
  conditional jumps to single-use labels preceded by a barrier: the two-use DUMMY label starts a
  fresh extended block that recomputes `&item_dummy` (`lis/addi`), while the fall-through reuses
  the flags store's address register); itemFrameSet = `do { m->no = itemTexNo(item->id);
  itemInfo(item->id, &info); } while (0)` (REG_N_REFS loop-depth weight: `item` outranks `col`,
  item r30 / col r29; the do-while around the whole else body moved the block to the end);
  ItemCommand::move = the sub-menu `--`/`++` tests read `Key.trg` (not `Key.rep`: the target
  reuses the trg word register), `mode = 1; subSel = 1;` (SI then QI pseudo), `PSet((void*&)
  wk->x24C, MapMgr.getWork(2))` (the store may alias `item_sel`, so its `lis`/load stay below it),
  case 1 falls through into case 2 (no `break`), `if (subSel == 0) sub[5]->flags |= 8; else
  sub[7]->flags |= 8;` (two block-local pointers in r9, tails cross-jumped; a ternary pointer is a
  global pseudo in r3). itemSelect = `for (i = 0; i < 2;) { int no = i + 1; ..; i = no; }` (`mr
  r31,r30` increment: cse cannot fold `i++` into `no` across the if/else join). itemMakeInit /
  itemMakeMove = the match test written in the `while` condition as a comma expression
  (`itemInfo(id, &info), !(hi == info.type || lo == info.type)`): an inline returning `a || b`
  materialises 0/1 (`li r11,0/1; cmpwi`) where the target branches.
  OPEN: itemSelect (the second loop's counter is r30 in the target: a fresh counter has the top
  priority and takes r31 in ours; `no` shared as the counter inverts i/no instead); itemMakeInit
  (`types`: the target keeps both arms `li r29,1799 / li r29,1292` (no jump1 else-set hoist) and a
  `clrlwi r28,r29,16` u16 view whose `& 0xFF` combine did not narrow to `& 0xF` (nonzero_bits of
  the two constants): the ternary temp's constants were not visible there); itemMakeMove (hi/lo
  r28/r29 in the target vs r23/r24: they rank above mk/joy/d/wk/iw there although their refs and
  live lengths look identical; possibly a live-length halving of the others via REG_EQUIV
  constants); itemMakeDisp (`col` = u8 truncation `clrlwi r30, r5, 24` of an int temp set in both
  arms (`li r5,4 / li r5,0`), `x * 8` recomputed per arm).
- Generic levers confirmed in the Sscrn third pass (2026-09-10; harness /tmp/ssw5: fdiff3.py =
  side-by-side objdump diff with relocs masked and context, LEFT column = split object, RIGHT =
  ours; dump.sh writes the cc1plus `-da` dumps with the module flags, fnd.sh cuts one function out):
  - global-alloc priority halving: local-alloc's `update_equiv_regs` doubles REG_LIVE_LENGTH of a
    pseudo whose set carries a REG_EQUAL/REG_EQUIV note with a function-invariant value, once per
    such set (a variable assigned the same constant in several places is halved several times:
    weaponLevelDisp `lv` 540/270, `numBase` 504/252). loop.c adds that note to every invariant it
    hoists (`move_movables`), gcse PRE insertions carry none: a `&local` pseudo hoisted by PRE
    (anticipatable at the loop entry) keeps its priority, one hoisted by loop.c ranks below the loop
    counters. `.lreg` "used N times across M insns" shows the doubled M.
  - block-scoped `for (int i ...)` counters vs one function-scope `i`: the shared `i` is one pseudo
    with several sets and a long life that outranks or underranks everything else (SsItemMain::init,
    stockNumDisp, weaponLevelDisp, itemSelect); the target's loops mostly have their own counters.
  - cse extended blocks: only CONDITIONAL jumps to a single-use label preceded by a barrier are
    followed (`cse_end_of_basic_block`), a plain `b` ends the block, any code label ends it; a
    `goto ERR` label with two uses is a fresh block that recomputes `&global` (`lis/addi`) while the
    fall-through reuses the register (ITEM_PTR); a shared-tail label after an if/else join is a fresh
    block too, so `i++` there cannot reuse a `no = i + 1` pseudo - write `i = no` (itemSelect).
  - loop.c: `combine_movables` merges equal invariants only if each has n_times_set == 1 and their
    SYMBOL_REFs are the same rtx string pointer (`rtx_equal_for_loop_p`); the moved set's threshold
    is `(1 + n_non_fixed_regs)` with calls in the loop and drops by 3 per moved movable; a set used
    in another basic block is not movable once `maybe_never` is set (after the first conditional
    jump of the loop body): put a pointer that both later loops use (`cMapMgr* mgr = &MapMgr`) at
    the TOP of the loop body to have it hoisted as one 2-use movable (SubScreenTask).
  - jump1's else-set hoist puts `x = b` between the compare and the branch (`if (c) x = a; else x =
    b;`); an explicit `x = b;` before the `if` is a free insn that sched1 moves above the compare
    (weaponLevelDisp `src`); the hoisted form also lets `x` share r3 with a preceding call result.
  - a store through `PSet((void*&) ..)` may alias every scalar, so the following `lis`/`lwz` of a
    global stay below it (sched1 critical path), where a struct-member store lets the `lis` float
    up (ItemCommand::move x24C/item_sel); a do-while barrier gives the same order but different
    pseudo numbering (r9/r11 swapped).
  - a value-context `a || b` inside an inline is 0/1 (`li 0; ..; li 1; cmpwi`); the same test as a
    comma expression in the `while` condition branches directly (itemMakeInit/Move).
  - two identical statements in if/else arms (`sub[5]->flags |= 8` / `sub[7]->..`) give two
    block-local pointer pseudos (both r9) whose tails jump2 merges; a ternary pointer + one store
    is a global pseudo (r3).
- Generic levers confirmed in the Sscrn fourth pass (2026-09-10, ss_map/ss_pzzl; harness /tmp/ssw6):
  - combine folds `p = m + K; r3 = p` into `addi r3, m, K` only for a compiler temporary: a user
    pointer local (`CameraParam* from = &m->from;`) keeps its pseudo, which sched1 then hoists above
    an earlier call into a callee-saved register (`addi r30, m, K` in the prologue, `mr r3, r30` at
    the call). Use it wherever the target has an `mr r3, rX` argument copy of a `this + K` address.
  - jump.c's `if (...) { x = a; goto l; } x = b;` hoist (jump.c:622, also in jump2 after reload) needs
    the insn after the `goto` to be a single set of the same register: an early `return 0` in an
    arm whose fall-through starts with the `mr r3, obj` of the next call gets `li r3,0` hoisted
    above the compare; an arm ending in a call (block-ending CALL_INSN + label) does not.
  - stmt.c narrows `s8 = s8 - 1` to byte arithmetic (no `extsb`); an `int` local holding the member
    keeps the promotion. A ternary assigned straight into a QI member (`m->x = c ? 3 : (d ? 0 : m->x)`)
    is expanded in QImode (`mr r9, r11` raw-byte copy, one `stb`, `extsb` of the stored register at
    the next compare) where an `s8` local is a promoted SImode pseudo.
  - `for (i ...) { switch (i) {..} ...&v[i] }` over a small array: loop.c's biv elimination rewrites
    the `cmpwi i, k` tree into `cmpw rP, &v[k]` pointer compares (pseudos holding `&v[1]`.. in
    callee-saved registers) - a compare tree on addresses in the target is a `switch` on the index.
  - PRE hoists `x & MASK` computed at the top of both if/else arms above the compare; the target
    keeping `clrlwi` in each arm (pieceModelDisp) has no source lever found (in-place `&=`, locals,
    re-reads all PRE'd).
  - Diff tool orientation: /tmp/ssw6/fdiff3.py prints `split | ours` (LEFT = target: `lbl_Sscrn_*`
    reloc names; RIGHT = ours: `.rodata+0x..` names); the "ours N insns, split M insns" header is
    the only thing named the other way round. Check the reloc names before deciding which side
    hoisted what - three of this pass's hypotheses were inverted by misreading the columns.
- Fifth pass (2026-09-10, ss_shop 64 -> 69/74, ss_item 30 -> 31/34, ss_pzzl 54/64 with PieceCommand::move
  -0x98 -> 0 and PieceCombine::move -0x10 -> -4 bytes; harness /tmp/ssw7 = ssw6 copies + `varf.py <unit>
  <func> <variants>` (whole-function variants) + `fdis.py` from /tmp/em10e):
  - **Invalid loop = loop notes without loop.c** (dispSellItemList, dispBuyItemList, both matched): the
    "hand-rolled goto loop" of the item lists is really `i = top; do { end = i + n; } while (0); goto TEST;
    while (1) { BODY; i++; TEST: if (!(i < end)) break; item = ..; row = i - top; pe = ..; if (pe == 0)
    break; }`. The `goto TEST` INTO the loop makes loop.c mark it invalid (no invariant hoisting, no givs:
    the 320/0.8/240 pool loads and the `lis IdSub` stay inside), but the LOOP notes are still there, so
    flow counts every body ref at depth 1 for the global-alloc priorities (that is what puts `sw`/`col`/
    `price`/`m` in the target's registers - `for (;;)` with the same `goto TEST` did NOT reproduce it,
    `while (1)` did) and update_equiv_regs/loop.c see the body as depth 1. The `end = i + n` after the
    dispScrollBar call must stay after it: sched1 hoists the free `add` above the calls (it is one basic
    block), the `do { } while (0)` LOOP_END makes it depend on everything before. `i = top` after the call
    is what keeps the call's first argument `top` (cse canonicalises `top` to `i` only for insns after the
    copy), and `sw` declared before `m` gives the target's `lwz pMerchant` first (equal priority, LUID).
  - `i++` written AFTER the last call of the body (`dispPrice(..); } i++;`) is a free insn that sched1
    hoists to the earliest free slot (Sell: right after the unitPtr call; Buy: the last slot before the
    dispPrice call, because it has no dependent there); `i++` before the call in source gets a
    sched_before_next_call anti-dependence (its pseudo... see haifa `REG_N_CALLS_CROSSED`) and is issued
    two cycles earlier.
  - `n` dying at a call's `mr r4,n` arg move is issued first (weight rule); the target issuing it LAST
    means `n` is still live: `end = top + n` written AFTER the dispScrollBar call (the `add` is hoisted
    above the call anyway, but the death moves to it) - dispLvUpItemList, matched with that plus
    block-scoped `for (int k ...)` counters for the two 5-loops, `Vec pos` block-scoped (frame 152, `info`
    reuses its slot), the `mark = unitPtr(0x3F)` pointer form, `if (m->tunable(item)) col = 0; else col =
    6;`. NOTE the fdiff3 columns again: the dispLvUp loop's fresh `lis r9,IdSub` per unitPtr call
    (documented as "rematerialised") is a hoisted PRE pseudo with a REG_EQUIV `high(IdSub)` that got no
    callee-saved register (r14..r31 all taken); the i/k counters decide who gets r31.
  - levelItemDisp (-0x18 -> -0xC): `Vec pos` block-scoped around each PSVECAdd/x/y block and `int
    digit[3]` block-scoped in the digit block (both at frame 8, first-fit reuse; `val`/`tag` are the
    address-taken small arrays at 40/48 after `pos2`'s 24), `tag[cur]` (a variable index puts the 2-byte
    array in the stack: `sth 0x7174,48(r1)` + `lbz 1(r16)`), and `cur = 1;` placed BEFORE the ratio
    `switch (type)`: set after a conditional jump (maybe_never) and used in another block, loop.c leaves
    it in the type loop (right before the digit loop it was hoisted as `li r14,1`, and `int cur = 0` at
    the body top is deleted by cse as a dead set). Open there: the target's `li 0,1; slwi 7,0,2` is the
    single-use constant moved next to its use by update_equiv_regs, which needs loop depth 0 at the use -
    ours keeps `li r28,1` at the statement (depth 1) although the target's type loop hoists the pool
    constants like a real loop; the `lis cMes`/`addi 14,9,cMes` pair hoisted in the target for the getMes
    stores (ours keeps `lis 10; addi 10` in the loop; a `pm = &cMes` local at the body top merges with the
    setLayout/MesSet `this` highs), the `mr 9,11` loadaddr copy, sw/swk/type register names.
  - dispPrice (matched): ONE function-scope `int n` for both digit blocks (a global pseudo in r4 in both;
    per-block locals gave r4/r5) and block-scoped `for (int i ...)` counters in the price block.
  - itemMakeDisp (matched): `u8 col; int c; if (i == mk->cursor) { eprintf(..">"); c = 4; } else c = 0;
    col = c;` (the int temp is `li r5,4/0` after the call, `clrlwi r30,r5,24` at the join - a `u8 col`
    assigned directly is hoisted above the call as a callee-saved pseudo), `eprintf(x * 8, y++ * 14, ..)`
    post-increments inside the eprintf/MesSet argument lists (the `addi y,1` then precedes the string
    `lis`: it is RTL-before the address setup), and `for (i = 0; i < 3; i++, y++)` (the `y++` giv
    increments follow `i++`'s in the latch).
  - PieceCommand::move (-0x98 -> 0 bytes): (1) `case 5: default:` in the `type` switch; (2) `case 9:`
    written AFTER `default:` - its body (`if (x26C == 0) command_id = 5`) is the fall-through copy before
    the shared `stw command_id`, and because its `li 0,5` precedes the `lis` (the `high` there is the
    PRE pseudo reloaded next to the store), every other `lis; li 0,K; stw` copy cross-jumps only the
    `stw` (the scan stops at the new label) and keeps its own `lis; li` (8 copies of `li 0,6` in the
    target; with the default arm last, its `case 3: command_id = 6` fell through and all `li 0,6` copies
    merged into it); (3) block-scoped `for (int i ...)` counters (r8, not r31); (4) `used =
    ItemMgr.arm(..)` (int, no `!= 0`: `mr. r28,r3`); (5) case 0 is `if (trg & 0x40000000) { ..; break; }
    if (trg & 0x80000000) { ..; if (used == 1) { ..; SndCall(0,8); return; } SndCall(0,7); }` followed by
    the cursor block WITHOUT an else: the target falls from `SndCall(0,7)` into the `Key.rep` cursor code
    (and the Key.trg u64 pair then lands in r11:r12 like the target); (6) `mode = 1; subSel = 0;` in case
    4 (stores come out reversed). Left (0 bytes, ~40 words): the type-2 `case 1: command_id = 5` copy that
    the target cross-jumped 3 insns deep into the case-9 fall-through (`b` to its `li 0,5`; a `goto` into
    the case-9 arm makes the label 2-use and merges more), case 6's separate QI/SI `1` constants and
    `stw; stb` order, `extra`/`extraNum` r30/r31, the `cmpwi`-then-`beq` polarity of two `if (x == K)`
    diamonds (lines 756/766: ours `bt 2; cmpwi; bt 2; b`, target `bf 2` fall-through arms).
  - PieceCombine::move (-0x10 -> -4): no `pzlPlayer* pl` local (`wk->x2B0->` re-read per statement: the
    target reloads `lwz r3,0x2b0(wk)` before every call and keeps the first load only in caller-saved
    r7), `pzlPiece** psel = &pzzl_sel;` declared before the loadCursor call with `*psel = ..ptrPiece(..)`
    (the `lis pzzl_sel@ha` in callee-saved r30 before the calls), `wk->x267 = 2;` BEFORE the `cur`/
    `caseBoard` loads (byte store first: the loads are RTL-after it), `if (b == pl->caseBoard) other =
    pl->spaceBoard; else other = pl->caseBoard;` (the `mr r27,r0` copy of the compared load = the
    hoisted else-set), `int h = b->h; b->curY = h - 1;` (keeps `extsb`, see the s8 narrowing rule), and
    ONE `SndCall(0, se, ..)` after a `switch (info.type) { case 6: se = 0x27; case 2: se = 0x29;
    default: se = 0x28; }` (bodies laid out 6, 2, default; the shared tail then starts at `li r3,0`; an
    if/else-if chain hoists the else-set). Left: wk/b/other = r29/r31/r27 vs r31/r29/r26, and case 4's
    `curX = 0` where ours stores the known-zero `trg & 0x80000000` register (cse skip-blocks carries it
    over the `if (getPieceNum())` diamond) and the target loads `li 0,0` (its cse path was one branch
    longer: PATHLENGTH).
  - Negative results this pass: itemSelect's second loop counter (r30 in the target; 12 variable-role
    permutations, `i` reuse, `no` reuse all give r31 or swap i/no); itemMakeInit's `types` (the target
    neither hoists the else-`li` nor narrows `& 0xFF` to `& 0xF`: switch/u16/init forms, all fold -
    likely the same combine `reg_nonzero_bits` difference as LvUpConfirm's `extsb`, candidate #12:
    ours knows the union of a multi-set pseudo's constant sets, the original does not); itemMakeMove
    hi/lo (r28/r29 vs r23/r24); SellItemNum::move this/val r24/r25 (a `do { } while (0)` around the
    first digit loop fixes this/val and the magic/i pair but swaps wk with the second `lis IdSub`);
    LvUpItemSelect::move item/x (do-while around either MesSet arm does not move it); mapModelInit's
    `j + 1` (r4 = the target picks from the caller-saved order 0,9,11,10,8,7,6,5,4 with r11..r5 all
    busy - `no++` placement, block-scoped j, `int j = 0` forms identical); levelItemDisp's type loop as a
    goto loop (loses every hoist: it IS a real loop in the target).
- Sixth pass (2026-09-10, ss_model Matching (47/47), ss_map 102 -> 104/105, ss_item 31 -> 32/34, ss_pzzl
  54 -> 56/64, ss_main .text size equal, ss_shop -8 -> -4 bytes; harness /tmp/ssw8 = ssw7 copies with
  `varf.py <unit> <func> <variants> [<mangled sym>]` (KEEP=<name> now really keeps that variant)):
  - **Read the target's control flow before believing an "#6" note** (ss_model wep09Init): the "cross-jumped
    call tails" were a plain `else if (type == 2)` chain — with a third arm every earlier arm ends in
    `b JOIN` and jump2 cross-jumps arm 0 into arm 1 through the jump_chain; only the LAST arm falls into
    the join and gets the flow.c `use` nop. The empty-asm tricks are useless here: `asm volatile("")` is an
    ASM_INPUT (find_cross_jump refuses ASM_INPUT and volatile ASM_OPERANDS outright), a non-volatile
    `asm("" : "+r"(x))` after the call is scheduled above the call by sched1 and flow2 re-adds the nop, and
    one with the call result as input merges the whole arm.
  - **Source bug found by the cross-jump depth** (ss_pzzl PieceCommand::move, 41 words -> 0): the target's
    type-2 `case 1:` jumped straight to the case-9 arm's `lis; stw` (skipping its `li 0,5`), i.e. it stores
    the switch register: `command_id = 1` (reload), not 5. Also there: `int one = 1; mode = one; subSel =
    one;` for the shared `li r0,1` of case 6's `stb`/`stw`; the mode-1 `base`/`type` selections are two-case
    `switch`es (`cmpwi 4; beq; cmpwi 6; beq; b` — an `if/else if` puts each arm after its compare); the two
    cursor clamps are ternaries straight into the s8 member (`wk->x26C = wk->x26C < 0 ? num - 1 : (wk->x26C
    > num - 1 ? 0 : wk->x26C)`, `subSel = subSel < 0 ? 0 : (subSel > 1 ? 1 : subSel); if (old != subSel)`:
    the MapModeSelect idiom, raw-byte `mr`, hoisted `li 0`, `extsb` of the stored register); `int extraNum
    = 0; pzlPiece* extra = 0;` declaration order for the `li r31,0; li r30,0` pair.
  - **`expand_preferences` hands a hoisted increment the copy preference of its source** (ss_map
    mapModelInit `j + 1` r4): global.c merges hard-reg preferences between two non-conflicting allocnos when
    one dies in the insn that sets the other. A function-scope `int j` shared by both room loops carries the
    r4 preference of the first loop's `mapBinAddr(&map_room[i], j)` argument into the second loop's `j + 1`
    pseudo (block-scoped `j`s give it the plain order r11). Same mechanism as the `no + 1` -> r5 (create's
    third argument) that already matched.
  - **The function's last statement decides the `return 4` layout** (ss_map mapColor 20 words -> 0): `if
    (p == 0) return 4; ...; if (stageFlag(open) || passed) { if (clear) return 3; if (passed) return 1;
    return 2; } return 4;` — every early `return 4` jumps to the final `li r3,4` block at the end, the clear
    arm's `li r3,3` stays inline, the `!open && !passed` fall-through has no out-of-line block, and with
    that layout the `high pG` / 0x80000000 pseudos of the three stageFlag tests are PRE'd once (callee-saved
    r29/r30 across checkPassed) and the clear test reuses the open test's `pG` load (`addi r10,r9,0x51bc`
    shared). With `if (!open && !passed) return 4;` inline, gcse inserts a fresh `high pG` at the end of the
    hide block, update_equiv_regs moves that single-use pseudo next to the open test's load (fresh `lis`
    after the call) and cse2 re-materialises the clear block's PRE copy. The 17-table spill-slot rotation
    followed from the same change (one fewer pseudo).
  - **do-while(0) around one call argument** (ss_pzzl PieceCombine::move, 4 words -> 0): `do { r =
    wk->x2B0->selPiece(b); } while (0);` counts `b`'s argument ref at loop depth 1 and lifts `b` above `wk`
    in global-alloc priority (b r31, wk r29); wrapping the whole `switch (r)` or the `b`/`other` block
    instead moves too many refs. Plus the dead `do { } while (0);` before `case 4:` for the fresh `li 0,0`
    of `curX = 0` (cse's skip-blocks path from the Key.trg test ended by the LOOP_END note).
  - **COMPILER-DIFF 12 launders applied** (tagged): ss_item itemMakeInit `types` — both arm constants as
    `asm volatile("li %0,%1" : "=r"(types) : "i"(K))` (jump1's else-set hoist needs a plain single set
    after the label; combine's `& 0xFF -> & 0xF` needs reg_nonzero_bits of the constant sets; an asm with a
    "0"-tied constant input gets its `li` hoisted by loop.c and the then-arm hoisted by the second jump.c
    transform); ss_shop LvUpConfirm::move — `int v = (s8) sw->lv[0]; asm volatile("" : "+r"(v)); t->fire =
    (u8) (v - 1);` keeps the `extsb` that our combine strips under the u8 truncation (the u8-parameter inline
    and a `u8 v0` local both still fold). ss_main SubScreenTask — `extern GlobalWork* pG_a/pG_b asm("pG")`
    for the two weapon-switch arms (COMPILER-DIFF 5, partial): defeats loop.c's `combine_movables` so no
    `high pG` is hoisted (.text size equal); the two `lis` stay in their arms where the target scheduled
    them into the join block before the ssWepModel2 test.
  - Analysed, still open: ss_map mapPositionCheck (41 words): the target's `flags = 0` byte stores of both
    cSat locals are frame-direct while ours go through the inlined ctor's `this` pseudo — cse's
    find_best_addr ties `(plus P 42)` against `(plus fp 242)` (both COST 2, rs6000 PLUS returns
    COSTS_N_INSNS(1) without operand costs) and only the offset-0 `(mem P)` is rewritten; `cSat() :
    cUnit(1), flags(0)` changes nothing; the rest is the template-copy register allocation that follows.
    ss_item itemMakeMove (hi/lo r28/r29): the target ranks the two case-arm constants above mk (16 refs)
    / joy / d / wk / iw, which no refs-over-length priority reproduces (refs 4, length ~200 each) — not a
    do-while weight case. ss_item itemSelect (second loop counter r30): the counter shares `no`'s register
    in the target, but `no` as the counter has more refs than `i` and takes r31. ss_shop BuyItemNum::move
    (+8): the target merges only `li r8,0; bl SndCall` of the Key.trg-0x40000000 arm into the else-if
    arm's tail (the else-if arm's block ends in the call -> flow `use` nop in ours); swapping the
    if/else so the else-if arm ends in `b END` merges the whole arm (494 insns). ss_shop levelItemDisp,
    SellItemNum/LvUpItemSelect priorities, ss_pzzl PieceSelect::move (113 words, was 187: block-local `pl`
    for the three board loads after the `x267 = 1` store, re-read `wk->x2B0` per call, if/else `other`;
    left this/mode r31/r30, `state` in callee-saved r27 for case 2's stores, case 1's `mr. r9,r3`; an
    `int st = state` / shared `int n` for the getPieceNum results reorders the cases), PzzlThinking,
    SsPzzlMain::init, caseModelMove, pzzlCursorDisp, pieceTblInit, pieceFrameDisp, pieceModelDisp not
    re-attempted.
- Seventh pass (2026-09-10, ss_shop 69 -> 70/74 (BuyItemNum::move), ss_pzzl pieceFrameDisp 143 -> 140 bytes;
  ss_map/ss_main/ss_item unchanged; harness /tmp/ssw9 = ssw8 copies + `tryx.sh`/`ccx.sh` (a patched cc1plus
  built from a copy of tools/sn-gcc in /tmp/ssw9/sngcc, see below) + `tryl.sh` (the /tmp/sngcc-leaf build)):
  - **Read where a cross-jumped arm LANDS before trusting the source's control flow** (ss_shop
    BuyItemNum::move, +8 -> 0): the target's cancel arm (`Key.trg & 0x40000000`) ends `li r7,0; b .L_28418`
    INTO case 1's `li r8,0; bl SndCall; b .L_284C0`, and `.L_284C0` is `mr r3,r24; bl shopStrStop; b END` —
    so the cancel arm calls `shopStrStop(wk)` after its `SndCall(0, 5)` (the source had `break` there; the
    msg == 0x13 / `Key.trg & 0x80000000` arm and the `result == 0` path really do skip it). With
    `shopStrStop(wk); break;` in the cancel arm, jump2 first merges its `bl shopStrStop; b END` with the
    then-arm's copy (stopping at the two-predecessor join label -> `.L_284C0`), then its `li r8,0; bl
    SndCall` with case 1's `b .L_284C0` tail (2 insns, then `li r7,0` vs `stb` differ). Case 2's own copy
    falls into the join with the flow `use` nop and is never a cross-jump candidate — that is why "swapping
    the if/else" merged 494 insns and why the +8 could not be fixed by arm order alone.
  - **A scalar-reference store delays BOTH the struct-pointer argument loads and the fixed-scalar `pG` load**
    (same function, the last 2 words): `IntSet(state, 1)` (file-local `int&` setter) before `if ((int)
    pG->x4F98 >= m->sellPrice(sw->buyId, sw->count))`. The plain `stw 0,16(this)` only conflicts with the
    `sw->buyId/count` loads (same offset 16, different base pseudos -> true dependence, cost 2) while the
    `lwz pG` (fixed scalar vs struct store) is free and goes first; the reference store has neither MEM
    flag, so all three loads become ready at t=4 and issue by priority: `lhz r4; lwz r5; lwz r30,pG`
    (-fsched-verbose-9 in the `.sched` dump shows the ready lists; the tie is visible as `61` alone at t=3).
  - **`VECNormalize(src, dst)` (math_sub.h) is the real form of the frame-corner normalize** (ss_pzzl
    pieceFrameDisp): its chain `(dst)->x = (dst)->y = (dst)->z = 0.0f` stores x, z, y (RTL z, y, x; the
    dying-source store first); the hand-written `c.x = c.z = c.y` gave y, x, z. Keep the `#line 1158` before
    the macro line. Left (140 bytes): the loop's two REG_EQUIV highs — ours gives the format-string high
    (loop.c pass-2 movable, 5 refs/112 insns) r21 and the 0.0 pool high (gcse-PRE'd from the `size.z = 0.0f`
    store at the top, 4 refs/310) no register (`lis r9` rematerialised in the loop); the target has the pool
    high in r14 and rematerialises the format string (`lis r7; addi r7`) — an allocation-priority order no
    refs/length change reached (k-selection forms, prev/next as statements, `Vec* pc` all 25+ words).
  - **`do { } while (0)` is a "phony" loop for loop.c** (`Loop from N to M is phony` in the `.loop` dump:
    scan_start is not a CODE_LABEL because the unused loop-top label was deleted), so it never hoists
    invariants into its "preheader"; only its notes (flow depth, cse block end, sched barrier) act. A real
    inner loop is needed for a loop.c preheader placement (ss_main SubScreenTask, see next item).
  - ss_main SubScreenTask (unchanged, COMPILER-DIFF 5 stays): the two `lis pG@ha` (r29/r30) sit in ONE
    basic block (`.L_AE80` after `bl MotionMove`, before the `ssWepModel2` test). cse leaves two equal
    `high` sets in a block (a HIGH costs 0 < a pseudo's 1, so the second is not replaced), but nothing in
    ours can place them there: loop.c needs a real inner loop (a do-while(0) around the switch is phony and
    moves the block instead), gcse PRE needs anticipation on the `ssWepModel2 == 0` path (the digit block's
    pG read is conditional and the loop exit computes no `high pG`), and haifa needs a region — the function
    ends in a leaf block (`bl TaskChain` + epilogue) and the enclosing `while (wk->x28)` body is far above
    10 blocks; the /tmp/sngcc-leaf cc1plus does not hoist them either. The two-alias launder stays.
  - ss_map mapPositionCheck (unchanged; mechanism now known, compiler-build family #13): integrate's
    `try_constants` substitutes the ctor's `this` pseudo P (`REG_EQUIV (plus vsv 192)` for satA) into every
    copied insn as ONE validate group, and the `flags = 0` store's group also substitutes the known QI
    constant, giving `(set (mem:QI (plus vsv 234)) (const_int 0))`, which our movqi rejects (no
    store-immediate) — the whole group is cancelled and the store keeps P (`stb r10,42(r26)`); the vptr
    stores (source = a lo_sum pseudo, not a tracked constant) are substituted and come out frame-direct.
    Experiment (/tmp/ssw9/sngcc, rs6000.md movqi/movsi conditions extended with `MEM dest && CONST_INT src`,
    reload then materialises the constant): satA's `stb 9,242(1)` becomes frame-direct with a reload `li`,
    but satB (frame offset 0: integrate's own FIXED_BASE_PLUS_P wants `(plus vreg const)`, a bare vsv gets no
    equivalence) stays `stb 10,42(27)`; adding the bare-vsv equivalence makes satB fully frame-direct
    including its vptr stores (`stw 11,16(1)`), which the target keeps through r27. The store-immediate
    patch alone regresses the module broadly (ss_map 1 -> 12 BAD functions, ss_item 2 -> 13, ss_shop 4 ->
    19), so the original is not simply "accept store-immediates"; nothing installed, no source form exists
    (the flags store cannot be moved out of the shared atari.h ctor: t_atari's static-init loop matches
    with it). Cost check for the cse route: `find_best_addr`'s tie-break `(COST(new)+1)>>1 > best_rtx_cost`
    can never fire for `(plus P c)` vs `(plus fp c')` on rs6000 (PLUS costs COSTS_N_INSNS(1) either way).
  - ss_item itemMakeMove (unchanged): global-alloc priorities from `.lreg` — mk 16 refs/183, joy 9/194,
    d 6/100, wk 6/196, iw 4/185, lo 4/200, hi 4/208 (ours: mk r29 .. iw r25, hi/lo r23/r24); the target
    gives hi/lo r28/r29 ABOVE mk, which needs a live length <= ~25 for 4-ref pseudos or a local-alloc
    allocation (they are set in two arms, so neither is reachable by source). itemSelect not re-tried.
  - ss_pzzl PieceSelect::move: `int st = state` (three placements) 113 -> 161 words, confirmed worse.
    ss_shop levelItemDisp: a `static inline unitScrPos(IdUnit*, Vec*)` wrapper for the two x/y sites gives
    the `&pos` a PRE'd pseudo (`mr r5,r17`) and no `mr r4,r3` — worse; the target's `mr r4,r3; lwz r3,
    0x68(r4); addi r4,r4,0x88` (u not folded into r3) appears only at the two sites whose `pos` is read
    afterwards, not at the two dispPrice sites.
- Eighth pass (2026-09-10, ss_main Matching (58/58, the two `lis pG@ha` closed, the pG_a/pG_b aliases and
  their COMPILER-DIFF 5 tag removed; symbols.txt got `_vt.10SsTermInit/Main`, `_vt.10SsItemInit/Main`,
  `_vt.9SsMapInit/Main` from sync_rel_symbols for the inlined `new` ctors); ss_map mapPositionCheck 93 ->
  69 words (prologue closed through include/atari.h); ss_shop levelItemDisp `digit[j]`; ss_item/ss_pzzl
  analysed, unchanged; harness /tmp/ssw10 = ssw9 copies + `tryf.sh <unit> <src.cpp> <fn>` (compile any
  source as the unit and fdiff3 it) + `atv.sh 'CTOR' 'EXTRA'` (atari.h variant, checks esp + ss_map)):
  - **Two `lis pG@ha` in one block = cse1 canon_reg + gcse PRE around a trivially-dead test** (SubScreenTask
    0 words). `{ int dmy; if (pG->x4FB8 == 7) dmy = 0; }` right before `if (ssWepModel2 && ssWepModel)`:
    (1) cse1 sees the test's `H0 = high pG` in the join block J; the 0x19/0x1F/0x20 arm A is on J's
    extended block (the switch's fall-through path), its own `hA = high pG` joins H0's class and `hA` does
    NOT become the leader (`make_regs_eqv`: NEW leads only if its last use is beyond the block), so
    `canon_reg` rewrites A's load to `pG@l(H0)` and `hA` dies -- cse DOES merge equal `high`s, by register
    canonicalisation, not by cost (that is what "cse leaves two equal high sets" misses); (2) the 0x1C arm
    B is reached by a followed `beq` in a LATER cse path (fresh table), keeps `hB`, and gcse PRE finds it
    redundant with J's occurrence (delayin(J) via the loop body's earlyin chain, S/T blocks delayed,
    `redundant = antloc & ~latein & ~isoout`): insertion `R = high pG` at J's END (before the ssWepModel2
    `beq`), `hB = R`, and cse2 cannot re-materialise the copy because B is not in R's ebb; (3) the store is
    trivially dead (never-read variable) so `delete_trivially_dead_insns` after cse1 removes it and the jump
    pass before gcse folds the `bne` -> J is ONE block at sched time and the highs interleave with the
    ssWepModel2 chain (`lis r9; lis r30; lwz r0; lis r29; cmpwi; beq`); the test's `lwz/lbz/cmpwi` die by
    loop pass 1; (4) loop.c cannot combine/hoist the highs any more: both are used in another basic block
    inside the `maybe_never` region (`!reg_in_basic_block_p && maybe_never` = not movable), so the digit
    block keeps its fresh `lis r9`; (5) a liveness-dead store (`v = 0` with v read later) keeps the `bne`
    to sched2: the two highs land before `lis ssWepModel2` and the 10.0 pool load gets hoisted (f30).
    Rule: two different registers holding the same `high sym` set in one dominating block = one arm
    canonicalised onto a dead test's high, the other PRE'd to that block's end; the dead test must read the
    SAME symbol/field and sit at the block where the target has the pair.
  - **cSat ctor `flags = 0` through a reference** (include/atari.h `cSat() : cUnit(1) { s8& f = flags; f = 0; }`,
    mapPositionCheck prologue = target: `stb r10,242(1)` / `stb r10,50(1)` frame-direct for both sats, satB's
    vptr stores still through r27, params r23/r24/r22, template words r26/r5/r25). The reference makes the
    address a register `f = this + 42`; cse1's `find_best_addr` REG path (`(mem f)`) accepts an equivalent
    with `(p->cost + 1) >> 1 > best_rtx_cost`, and `notreg_cost` doubles rtx_cost, so `(plus fp N)` (cost 4)
    beats the pseudo (cost 1) and the store goes frame-direct; a plain member store is `(mem (plus this 42))`
    and the PLUS path compares `(plus fp N+42)` against `(plus this 42)` (4 vs 4: never). Works for the
    frame-offset-0 satB too (integrate never substituted it, cse does the rewrite). NOT a free function: a
    header-level `static inline SatFlagSet(s8&, s8)` adds DECLs and shifts every DECL_UID in units that
    include atari.h -- game/esp's `EspDispInfo` regressed (`max.238` static-local name -> gcse hashes
    `high(symbol_ref)` by NAME, bucket order changed); the block-local reference variable is the zero-decl
    form (esp still 100%, all 111 files OK, t_atari/rooms unchanged). Left in mapPositionCheck (69 words):
    the second `init` call's `mr r5,r28` before `mr r4,r3` (sched2 tie: the getSat result copy 136 gets r4
    in both, deps 2/2, LUID -> ours first; the target ranks the &zero move first), `i`/`&hit` r29/r28, and
    the debug-draw loop's p/v/col/a/b registers.
  - ss_shop levelItemDisp: `digit[j] == 0` (not `digit[2]`) in the `type == 0 && j == 2` test = the target's
    `lwz r0,8(r19)` through the `&digit` pseudo (cse folds j*4 with j known 2 but keeps the base register;
    `digit[2]` is `(plus fp 16)`), same idiom as weaponLevelDisp. Still -12 bytes: the two `mr r4,r3` sites
    (the unitPtr result U dies at `addi r4,U,136` and is tied to r4 by local-alloc in the target because the
    parent pointer P (`lwz r3,104(U)`) took r3 first; ours allocates U (3 refs/3 insns) before P (2/3) and U
    takes the r3 copy suggestion -- a qty-priority tie), the tail's `addi r30,r30,cMes@l; addi r30,r30,2832;
    sth 34/32(r30)` (an integer `m = (u32)&cMes + 0xC*sizeof(Message)` variable gives the 32/34 offsets but
    folds the base into `lis/addi cMes+0xb10`; `pm` pointer forms fold everything), and type/bar register
    names.
  - ss_item itemSelect (unchanged): a dead test on `i` after the k loop is canonicalised by cse onto `no`
    (`i = no` makes `no` the leader when `i`'s later mention is the dead test... the compares become `cmpwi
    no`), so `i` is not live across the k loop and k still takes r31; a dead test INSIDE the k loop keeps
    `i` live but `no` then outranks `i` for r31 (i r30/no r31/k r31). Two dead tests after the loop give
    i r31/no r30 with the loop test on `no` (5 words). The target needs i r31 live across loop 2 with the
    loop test on i -- not reachable with dead tests.
  - ss_shop LvUpConfirm::move (unchanged, 6 words): the #12 launder's register naming; non-volatile
    `asm("" : "+r"(v))`, tied `"0"` forms, and an `asm volatile("" :: "r"(t))` keep-alive are 6-13 words.
  - ss_pzzl PieceSelect::move (unchanged, 113): read off the target: the r==1 arm stores the getPieceNum
    result register (`mr. r9,r3` -> `stb r9`) as the zero of `x264 = 0; x265 = 0`, the r==2 arm stores `state`
    (r27, loaded once at the top, no reload after the calls) and case 4 stores the Key-test OR result -- all
    three are cse's zero class picking the OLDEST known-zero register on its path: the r==2 arm is reached
    through the followed `beq CASE2` from the top (knows state == 0 from the state switch), the r==1 arm is
    the fall-through ebb restarted after `beq CASE2` (knows only the `mr.` result). Ours knows the Key OR
    result (r27) in both arms -> our path did not follow `beq CASE2` (PATHLENGTH or label shape), and the
    source's `x264 = state` in the r==2 arm reloads the byte (`lbz 0,19`); write both arms as literal zeros
    and shorten/lengthen the cse path so `beq CASE2` is followed -- `int st = state` (161) and an `int n`
    result variable (113, cse substitutes the older zero) do not do it.
- Ninth pass (2026-09-10, ss_item Matching (34/34, module REL still byte-identical); ss_map mapPositionCheck 69 -> 2
  words; ss_shop 70 -> 71/74 (LvUpConfirm::move) with .text size equal (levelItemDisp 98 -> 33 lines, SellItemNum::move
  38 -> 31); ss_pzzl 56 -> 57/64 (pieceModelDisp); harness /tmp/ssw11 = ssw10 copies with the paths rewritten, `v_*.py`
  = per-function variant scripts that rewrite the source from `<unit>.base.cpp`; NOTE `dump.sh` without `-da`: a full
  `-da` dump of ss_shop is ~300 MB and /tmp is a 32 GB tmpfs that was 91% full):
  - **Flip check before the flip: `python3 tools/sync_rel_symbols.py` says "0 symbols changed" and the REL still fails
    on ADDR16 fields** (ss_item): the module's `symbols.txt` had `scope:global` for the unit's .bss objects
    (`lbl_Sscrn_bss_220..274` = item_num, item_total, item_sel, item_frame_on, item_path0/1, item_curve, item_scr,
    item_pos, item_frame_state, and item_list at 0xA0) while the source had them `static` -- 82 differing REL bytes,
    all `@l` fields holding S+A. The em35 rule again; the check is the python snippet of this pass (symbols.txt scope
    per .bss/.data/.rodata offset within the unit's split ranges vs the source's `static`s). ss_map's map_room /
    map_room_num were the same case and are non-static now (ss_map cannot flip yet, see below).
  - **A source bug found by a constant diff** (mapPositionCheck): `Draw_line3d(&hit2, &d, 0xFF00FF, 0)` was
    0xFF0000FF (`lis r5,-256; ori 255`). Read every `*` line of fdiff3 that is not a register name before
    theorising about allocation.
  - **Reusing the function's own pointer variables for the debug loop** (mapPositionCheck 69 -> 4 words): the target's
    p/v registers (r25/r24) are `poly`/`vtx`'s -- the loop was `poly = satA.poly; vtx = satA.vtx; for (..; poly++)`
    over the SAME variables that the following code re-assigns (`poly = satB.poly; poly += idx; vtx = satA.vtx`),
    not block-local `AtPoly* p; Vec* v`. With one pseudo per pointer the whole allocation of the function followed
    (i r29, &hit r28, a r28, b r30, col r27). Then `if (i != idx) col = poly->attr; else col = 0xFFFF0000;` for the
    `cmpw; lis r27,-1; beq` order (jump1's else-set hoist puts the `lis` RTL-after the compare; `u32 col = K; if
    (..) col = ..` has it before and sched2 keeps that LUID order). Left (2 words): the second `init` call's
    `mr r5,r28` before `mr r4,r3` -- read off the sched dumps: in sched1 the result copy `P136 = r3` (204) and
    `r5 = P113` (214) have equal priority (13), equal weight (0: `mr r6,r5` is regmove's optimize_reg_copy of the
    dying `&zero` pseudo in both builds, so P113 dies at 214), both class 3 against the call (call cost 1), 3
    dependents each, and the LUID decides (the call's `copy_to_reg` copy is RTL-first). Only the first call's
    priorities differ (19 vs 18) and match. Statement forms (getSat into a local, `Vec* z = &zero` locals, z for one
    or both args) leave the LUIDs. The original broke the tie the other way; not source-reachable.
  - **#17 pins, what works and what does not** (ss_item itemSelect 5 -> 0, itemMakeMove 27 -> 0):
    (a) `register int pin asm("rN"); asm("" : "=r"(pin) : "r"(x)); asm("" : : "r"(pin));` -- give the producer an
    input that varies in the loop (`"r"(i)`) or loop.c hoists the input-less set out of the loop; the consumer
    has no outputs, so it is volatile: a cse flush + sched barrier at that point (itemSelect's loop top and
    the k-loop's pin are harmless, itemMakeMove's `d = 0` position was not: `mk->cursor` got reloaded). (b) The
    pinned register becomes `regs_ever_live` = used-so-far, so EVERY pass-0 candidate allocated before the
    intended one that does not conflict with the pin grabs it (itemMakeMove: the PRE'd index took r29, the
    second ItemMgr high took r29): the pin range must conflict with all of them or they must have a better
    used-so-far register first. (c) `register int x asm("r31")` is useless: flow never marks the frame pointer
    ever-live before reload and global.c strips r31 (`eliminable_regset`) from every hard_reg_conflicts, so the
    pin neither counts as used-so-far nor conflicts (mk took r31 and the codeless asm "clobbered" it). (d) The
    value-carrying form `register int hi asm("r28"); register int lo asm("r29");` for the two case-arm constants
    is what closed itemMakeMove: the hard registers are live from the arms to the while loop, so mk/joy/d/wk/iw
    (live there) cannot take r28/r29 and fall to r27..r23 in pass 1, while idx/highs (inside hi/lo's range) skip
    them in pass 0 through `regs_someone_prefers`. Tagged `COMPILER-DIFF: candidate #17`.
  - **local-alloc's fake lifetime** (LvUpConfirm::move 6 -> 0; local-alloc.c `fake_birth = birth - 2 + birth % 2`,
    `fake_death = death + 2 - death % 2`, tried first when `-fschedule-insns2`): a qty conflicts with the qtys
    born in the insn right after its death and dying in the insn right before its birth. The four item-pointer
    loads (`lwz r11,36(r29)`) of the four nibble stores alternated r11/r10 in ours because each `lwz` was the
    insn after the previous `sth` in the sched1 order; the target loads the lv byte first in every nibble
    (`int v = (s8) sw->lv[k]; t = &sw->item->x6; t->x = v - 1`), so one insn separates `sth` and the next
    `lwz` and all four take r11. The byte-first LUID also decides nibble 4's `lbz`-before-`lwz` (sched2 tie:
    both loads wait 2 cycles on the previous `sth`, equal priority and dependents). A single reassigned
    `ItemWork* it` pointer is one qty (r11 everywhere) but its reload has an ANTI dependence on the previous
    `sth` (cost 1: `add_dependence` skips the memory link when a register link exists) and goes first (wrong
    for nibble 4). The nibble-1 launder stays the volatile `asm("" : "+r"(v))` AFTER `t = ...`: an
    `asm("extsb %0,%1")` or a non-volatile launder costs the chain a cycle (LINK_COST_FREE applies to the link
    INTO an unrecognizable insn, the asm's own cost is 1), and sched2 then issues `lwz` first (7 dependents vs
    6 with the r11 output dependence) or `clrlwi 0` before `slwi`.
  - **`Vec* scr = &u->scr; asm("" : "+r"(scr)); PSVECAdd(&u->parent->pos, scr, &pos)`** (levelItemDisp, both
    `mr r4,r3` sites, -8 bytes; tagged `COMPILER-DIFF: 3`): local-alloc ties `Q = U + 136` (U dies) and the asm
    output with U into one qty carrying TWO copy suggestions {r3 (call result), r4 (`r4 = Q`)}; `find_free_reg`
    walks REG_ALLOC_ORDER (.. r5, r4, r3), so the qty takes r4 (`mr r4,r3; addi r4,r4,136`) and the parent
    pointer P gets its own suggestion r3 (`lwz r3,104(r4); addi r3,r3,148`). Without the asm, combine folds
    the single-use `Q` into `addi r4,U,136` and U keeps its r3 copy suggestion (ours). A plain `Vec* scr` local
    is folded too (the zoomMove rule needs several uses).
  - **`asm("addi %0,%1,%2" : "=r"(mm) : "b"(a), "i"(0xC * sizeof(Message)))` + direct member stores**
    (levelItemDisp tail `addi r30,r30,cMes@l; addi r30,r30,2832; sth 34/32(r30)`, -4 bytes; tagged
    `COMPILER-DIFF: candidate #12 (address form)`): every C form folds -- `(u32)&cMes + 2832` into the
    relocation (`cMes+2832@l`), an opaque `a` plus `mm = a + 2832` into the displacements (`sth 2866(a)`: cse's
    PLUS associativity `lookup_as_function` folds `(plus mm 28)` to `(plus a 2864)` and the `U16Set` reference
    parameter makes that a SET that `src_folded` wins), a second launder on the sum costs a cycle (`addi 2832`
    after `li 0,6`). The asm-emitted addi is one real insn, and `mm->ot = ..` direct stores keep `(mem (plus mm 32))`
    (find_best_addr needs a strict cost win to rewrite an address).
  - **Function-scope `int x, y` shared by the type loop and the specialTunable tail** (levelItemDisp 87 -> 33
    lines): block-local x/y in the loop body are local-alloc'd pseudos crossing the setLayout call and take
    r28..r30, which makes those registers used-so-far for global pass 0 (i took r30 instead of r31, m r29...);
    the target's y is r31 (never a local-alloc register), i.e. x/y were global pseudos = one declaration for both
    blocks. Left (33): sw/m r29/r30 swap in the prologue and the `cur = 1` -> `li 0,1; slwi 7,0,2` (reload
    rematerialisation of a REG_EQUIV constant next to its use, #13; `asm("li %0,1")` before the digit loop is
    hoisted out of the type loop by loop.c/gcse, `cur = 1` there too).
  - **Per-loop `int i` in SellItemNum::move** (38 -> 31 lines): the two digit loops' counters are separate
    pseudos in the target (r28 loop 1 with the magic constant r29, r29 loop 2); a shared `int i` gave both
    loops one register. Left: this/val r24/r25 (this 27 refs/386 = 0.280 vs val 17/270 = 0.252 in global-alloc's
    order; do-while(0) weights around val's sets or the digit statements swap them but move the schedule or the
    IdSub high; a #17 pin on r25 after the k loop or inside loop 2 costs a register or attracts the k-loop
    pseudos) and the `addi r17,r1,8` slot.
  - **pieceModelDisp 4 -> 0: the earlier note had the fdiff3 columns inverted.** The TARGET computes
    `item->x8 & 0x1FFF` once before the `>> 13` test (`lhz r0; clrlwi r4,r0,19; srwi r0,r0,13`) and ours had it
    per arm: `u32 x8 = item->x8; u32 num = x8 & 0x1FFF; if ((x8 >> 13) == 1) numDispI(id, num, &scr, 3); else
    numDispI(id, num, &scr, 1);`. Re-check the column orientation (`lbl_Sscrn_*` = target) before trusting
    an old residue description.
  - Read and left: pzzlCursorDisp (+4: in ours regmove's optimize_reg_copy_1 folds the `wk->x2B0` load of
    case 2 into `lwz r3` because `col = r3` (the call-result copy) is RTL-first and r3 is dead over the load;
    the target's `lwz r0; mr r3,r0` means r3 was live there, i.e. the load preceded the copy in the original's
    RTL -- no statement order gives that); LvUpItemSelect::move (item/x r28/r30: x must outrank item and
    sw; do-while around the if/else, one arm, `x = ..` or setLayout: 30-97 lines; an r30 pin sends y to r30 and
    item to r31); pieceTblInit, pieceFrameDisp, caseModelMove, SsPzzlMain::init, PzzlThinking::move,
    PieceSelect::move not iterated.
- Tenth pass (2026-09-10, ss_map Matching (105/105, module REL byte-identical, 111 files OK); ss_shop 71 -> 72/74
  (LvUpItemSelect::move; levelItemDisp 33 -> 14 lines, SellItemNum::move 31 -> 4 insns); ss_pzzl 57 -> 58/64
  (pzzlCursorDisp); harness ssw12 = ssw10 copies with the paths rewritten (`tryf.sh <unit> <abs src> <fn>`,
  `varf.py <unit> <base.cpp> <fn> name=variant.py...` = REPL-list variants with a diff-line count, `scopes.py <mod>
  <unit>` = the ADDR16 scope check: module symbols.txt scope per data offset vs our object's bindings; `dump.sh` takes
  explicit `-dX` flags, no `-da`); tools/sn-gcc/src/gcc is the SN cc1plus source for reading passes):
  - **An asm's memory input anchors it after a call without making it a consumer of the call result** (mapPositionCheck
    2 -> 0, unit flipped; tagged `COMPILER-DIFF: 5 (sched1 LUID tie)`): `cSatFile* sb = hdrB->getSat(0); Vec* z;
    asm("mr %0,%1" : "=&r"(z) : "r"(&zero), "m"(zero)); satB.init(sb, z, z);` -- the asm depends on the getSat call
    (memory flush), its chain `asm -> r5 = z -> mr r6,r5 -> call` is one longer than the result copy's `P = r3 -> r4
    = P -> call`, so sched1 ranks it first and sched2 keeps the LUID order (`mr r5,r28; mr r4,r3`). What does not work:
    an asm on the getSat result (`"r"(hdrB->getSat(0))`, the copy stays RTL-first), an asm on `&zero` with the call
    result as an `"r"` input (it becomes a consumer of the result copy and cannot precede it), and a plain `"=r"` output:
    local-alloc ties an asm output to a dying `"r"` input (block_alloc's operand loop, `combine_regs` with
    `may_save_copy` 0) -- `mr r28,r28` -- so a copy asm whose input dies there needs the early-clobber `"=&r"`
    (block_alloc skips `&` outputs). Rule of record for these ties: rank = priority, then INSN_REG_WEIGHT (sched1
    only), then class against the last scheduled insn (anti/output links cost 1 in this haifa: `insn_cost` clamps
    `ADJUST_COST`'s 0 to 1, so every link is class 3), then dependents, then LUID; a sched2 tie between a call-result
    copy and an argument move therefore needs a longer chain or a smaller LUID, never a class difference.
  - **End-of-file order with a synthesized destructor of a class declared before ss_main.h** (ss_map: `_._4cSat` was
    output before LightSetModel2/~Widget, the target has it after them, REL 183 bytes off although every function was
    byte-identical -- bcmp's `pos!` flag). Our cc1plus queues a synthesized dtor at the class definition (atari.h),
    the original at synthesis (candidate #8), so the two inlines that must precede it are instantiated BEFORE
    `#include "atari.h"`: the `extern "C" inline LightSetModel2` definition and `struct SUB_SCREEN; static inline
    void ssMapWidgetDelete(Widget<SUB_SCREEN>* w) { delete w; }`. The .rodata vtable order did not move (Widget's
    vtable still follows SsMapInit's), so the vtable VAR_DECL is not pushed at that instantiation. Check `bcmp.py`'s
    `pos!` column before a flip: fdiff/unit_info do not see function order.
  - **Pre-flip scope check** (`scopes.py`): every `scope:global` data symbol of the unit's split ranges must be a
    non-static (or weak vtable) object in our .o and every `local` a static/anonymous one; ss_map was clean.
  - ss_shop levelItemDisp (33 -> 14 lines): (1) `ShopWork* swk` declared BEFORE `Merchant* m`: `wk` dies at the m
    load, sched1's weight rule issues that load first (`lwz r20,792; mr r29,r4; lwz r30,788`) and the swk load's
    later slot shortens its live length below sw's, so swk takes r30 and sw r29 (global-alloc priority 2 refs/21 vs
    2/22; declaration order after the zero inits changes nothing because sched1 hoists the load anyway); (2) the
    leading-zero flag `on` is the function's `i` (`i = 0; ... i = 1;`): the shared pseudo's refs put it above `j`
    (on r31, j r29); (3) the specialTunable tail loop counts with `i`, not `type` (i r31, &pos r28); (4) tagged
    `COMPILER-DIFF: 13`: `asm("li %0,1" : "=r"(cur) : "m"(tag[0]))` right before the digit block with `tag[1]`
    written literally (the target folds `tag[cur]`; a `"m"(tag[1])` input reorders the hoisted `&tag`/`&digit`
    addis, register/value inputs let loop.c hoist the asm out of the type loop). Left (14): the digit-loop preheader
    -- read off the -dR/-fsched-verbose-6 dump: our asm output r0 dies at `slwi`, so the bct count reload also takes r0
    and inherits an r0 anti chain (`li r0,1` prio 6, `slwi` 5, `li r0,3` 4, `mtctr` 3); the target's count reload is
    r9 (its `cur` was a reload register live across the count init) and its `li r0,1`/`lis` are prio-2 insns issued
    after `li r9,3; lis r6; mtctr r9`. No source form found that keeps r0 busy at the count init without lengthening
    `cur` into a callee-saved register.
  - ss_shop SellItemNum::move (31 -> 4 insns, tagged `COMPILER-DIFF: candidate #17`): `register SellItemNum* self
    asm("r24") = this;` with every member access through `self` (this r24, val r25). The value pin on `val` itself
    (`register int val asm("r25")`) fixes the same allocation but local-alloc then gives the `val % 10` remainder
    the dying hard register (`sub r25,r25,r0; stw r25`) -- pin the OTHER register of a swapped pair when the pinned
    variable is an arithmetic input. Left: `addi r17,r1,8` (the PRE'd `&digit`) issued right after `bl MesSet` in
    ours (a free insn in sched1: the frame pointer is not call-used, so it takes the first free slot), after `li
    r5,31` in the target (ready one cycle after the call, i.e. dependent on it, and LUID-after the argument moves);
    `int digit[]` at function/loop/enclosing-block scope: 2-5 lines. A memory-input asm (`la %0,%1`) depends on the
    call but also becomes a predecessor of the next call with the arguments' priority and a smaller LUID, so it is
    issued before them; not applied.
  - ss_shop LvUpItemSelect::move (30 -> 0, zero code): the frame loop `for (x = 0; x < 5; x++)` counts with the
    MesSet `x` (item r28, x/lv/counter r30, the last loop's `i` r31); block-scoped counters or `x` in the lv loop as
    well: 7-26 lines. Same family as levelItemDisp's `on = i`: when the target gives a short-lived variable a
    register that a longer one should own by refs/length, look for a reused function-scope variable.
  - ss_pzzl pzzlCursorDisp (+4 -> 0, tagged `COMPILER-DIFF: 5 (sched1 tie)`): the ninth-pass note had the columns
    inverted -- the TARGET folds case 1 (`mr r30,r3; lwz r3,0x2b0(r31)`) and keeps case 2 unfolded (`lwz r0; mr
    r30,r3; mr r3,r0`); ours unfolded both. Both arms have identical RTL through regmove (`col = r3; P = mem; r3 =
    P`) and sched1 issues the load (latency 2, prio 8) before the copy (prio 7) in both, so P is born while r3 is
    live and local-alloc gives it r0. Case 1 is closed with `pzlPlayer* pl; asm("lwz %0,%1" : "=r"(pl) :
    "m"(wk->x2B0), "r"(col)); p = pl->ptrPiece(pl->cur);` -- the asm-emitted load consumes `col`, so it follows the
    copy and its output is tied to r3 through the argument move. Why the original's case 1 scheduled the copy first is
    not known (no source difference between the arms).
  - ss_pzzl pieceTblInit (analysed, unchanged): the target materialises `tbl` (`addi r11,r9,piece_info@l; lhz
    r9,0(r11)`, `mr r7,r11` for the loop) and bases the store giv at `model[4]` (`addi r6,r7,84`, `stw -4(r6)/0(r6)`);
    a `void** mp = &tbl[i].model[4]` pointer is re-folded by loop.c into the offset-0 giv (`mr r6,r7`, 80/84), the
    `u32` index-first SS_ARC form folds the second load's +20 into the displacement (worse), a `"+r"(tbl)` launder
    costs 47 lines. pieceFrameDisp: a dead `if (c.x == 1.0f) k = 0;` at the end of the j body stops loop.c's pass-2
    hoist of the FILE-string high (pLog/27E8 then match the target's r20/r21) but the freed register goes to the line
    highs, not to the 0.0 pool high (`lis r14` at the top in the target), and `m` moves to r31: 46 lines. Not applied.
- Eleventh pass (2026-09-10, ss_shop 72 -> 73/74 (levelItemDisp), ss_pzzl 58 -> 61/64 (pieceTblInit, PzzlThinking::move,
  SsPzzlMain::init) + the eof order fixed; neither flipped: SellItemNum::move 4 insns, pieceFrameDisp, caseModelMove,
  PieceSelect::move left; harness ssw13 = ssw12 copies with the paths rewritten + `fvar.py <unit> <base.cpp> <fn>
  <start-marker> <end-marker> name=variant.py..` (REPL applied to ONE function's text only; `sellv.sh`/`tblv.sh`/`thv.sh`/
  `miv.sh`/`psv.sh` wrap it per function), `fn.sh DUMP 'name('` cuts a function out of a dump; NOTE ninja does not track
  header dependencies of the NGCCC rule -- after a header edit `/bin/rm build/G4BE08/src/Sscrn/*.o` before `ninja`):
  - **eof order: an in-class virtual body is queued right after its class's synthesized dtor** (ss_pzzl `pos!` on the
    last 0x300 bytes: the target has `init__10SsPzzlInit` after `_._10SsPzzlInit`, `init__10PiecePopUp` after its dtor, etc.
    -- `state = 0;`/`count = 0;` bodies). Our cc1plus queues the dtor at `finish_struct` (lex.c cons_up_default_function
    -> mark_inline_for_output) and the in-class inline bodies when `do_pending_inlines` compiles them right after the
    class (decl.c finish_function -> mark_inline_for_output), so `virtual void init(SUB_SCREEN* wk) { state = 0; }` in
    the class gives exactly the target's dtor, init, dtor, init interleave; an out-of-line definition is output in
    place. All Sscrn units re-verified (111 files OK) with the ss_main.h change.
  - **levelItemDisp 14 -> 0 (tagged `COMPILER-DIFF: 13`): the digit-loop count reload lands on r9 because of reload's
    round-robin.** `allocate_reload_reg` walks `spill_regs` from `last_spill_reg + 1` across insns (reload1.c); the
    function's spill regs are {r0, r9} (per-insn `used_spill_regs` = all spill regs not used by live pseudos), the two pool
    highs before the loop take r9, r9, and the target's `cur` is a rematerialised REG_EQUIV constant (`li r0,1` before
    `slwi r7,r0,2`) that takes the next slot r0, so the ctr init reload after it gets r9 and sched2 issues `li r9,3; lis
    r6; mtctr r9` first (mtjmpr latency 3 = prio 4/3). Ours had no reload for `cur` (the tenth-pass `li` asm is an
    allocated output) so the ctr reload took r0 and inherited the `slwi` anti chain. Form: `int cur = 1;` declared in the
    block BEFORE the type loop (a single constant set, live across the whole type loop where every callee-saved register
    is taken -> spilled -> rematerialised), its only register use an `asm("slwi %0,%1,2" : "=r"(c4) : "r"(cur))` inside the
    digit loop (loop.c hoists it to the preheader like the target's slwi; with a plain `val[cur]` gcse's cprop folds `cur*4`
    to 4), `*(int*) ((u8*) val + c4)` for the two accesses, and `tag[cur]` written literally (cprop folds it to `lbz 1(r16)`
    like the target). `cur = 1` placed inside the type loop instead is hoisted with the same result but `cur` (pseudo
    127, low number) then wins r14 from the cMes address (equal priorities, allocno order): declare it outside.
  - **pieceTblInit 4 words -> 0 (tagged): the target's `tbl` is #13 again** -- a REG_EQUIV `piece_info` pseudo
    rematerialised as `lis r9; addi r11,r9,@l` at the entry test (`lhz r9,0(r11)`, not folded into the lo_sum) and INHERITED
    (`mr r7,r11`) for the loop base copy; ours allocates it, combine folds the entry load and cse2 rematerialises the loop
    copy as `addi r7,r9,@l`. Applied: `asm("lis %0,piece_info@ha" : "=r"(hi)); asm("addi %0,%1,piece_info@l" : "=&r"(tbl)
    : "b"(hi));` (symbol relocs in asm text assemble fine), `asm("mr %0,%1" : "=&r"(base) : "r"(tbl))` for the copy (the
    early clobber stops local-alloc from tying it to the dying tbl), and the loop rewritten as the target's two bivs: a
    user byte-offset `u32 ofs` (its `ofs = 0` init precedes the hoisted 0xFFFF constant in RTL, which is what puts `li
    r8,0` before `ori r5,r5,65535`; loop.c's own `i*120` giv init comes after the pass-1 movable) with
    `((PieceInfo*) (ofs + (u32) base))->id` loads, and `void** mp = (void**) ((u32) base + 84)` stepping by 30 for the
    `stw -4(r6)/0(r6)` pair (a `&tbl[i]` register giv shared by both stores makes the +0 giv the combine representative).
    The four id re-reads are `*(volatile u16*)`: identical `(mem (plus ofs base))` loads are PRE'd by gcse into one
    register (`mr r11,r0; clrlwi`) -- 2.95 C++ has no alias sets here (struct/u16/s16 views merge), volatile MEMs are
    not gcse candidates and do not perturb the schedule. `base`/`mp` are `register .. asm("r7")`/`asm("r6")` pins (candidate
    #17): as an asm output / opaque sum they carry no REG_EQUAL note, so update_equiv_regs does not double their live
    length and they outrank the biv (the target's copies are REG_EQUIV-doubled). The lwzx operand order is the
    index-first `*(u32*) (idx*4 + (u32) arc)` form; the tex index is laundered (`asm("" : "+r"(tix))`, tagged 12) because
    combine reassociates `(plus (plus tex8 20) arc)` into a displacement in the index-first form only.
  - **PzzlThinking::move 8 -> 0 (zero code): the PieceCombine recipe again** -- `int se; switch (info.type) { case 6:
    se = 0x27; break; case 2: se = 0x29; break; default: se = 0x28; } SndCall(0, se, ..)`: bodies laid out 6, 2, default
    (`beq A; cmpwi 6; bne B; li r4,39; b Lz; A: li r4,41; b Lz; B: li r4,40; Lz: li r3,0`), the shared tail starts at
    `li r3,0`. Three `SndCall` statements give per-arm `li r3,0` + a cross-jumped tail from `li r5`; an if/else-if `se`
    hoists the else-set.
  - **SsPzzlMain::init 8 -> 0 (zero code): four SOURCE bugs read off the diff** (`lwz r4,52/56(r9)` = SS_ARC_PTR indices
    0xD/0xE, not 0xC/0xD; the IdNum.set archive offset 8, not 7; `MesData.setPtr(0, SS_ARC_PTR(pCmmn, 5))` (not 4) and
    `setPtr(2, ..)` (not `ptr[1]`) -- the setPtr form gives `stwx r0,r11,rZERO` with the `state = 0` zero pseudo (r30, live
    across `sscrn_pzzl_in_init`) as the index and `stw 8(r11)`), plus: `ItemWork* last = ItemMgr.pLast; wk->x300 = last;
    append(last)` (`mr r4,r0` instead of a re-read after the store), block-scoped `for (int k ..)` counters per loop (r31,
    r30, r29 with the `tbl[k]` giv in r31 -- the shared function `i` was one high-ref pseudo), `int no = k + 1;` AFTER the
    setCommandId call with `k = no` at the body end (`addi r3,r30,1` after `bl`, `mr r30,r3` at the latch: the itemSelect
    idiom; a pseudo that does not cross the call is anchored to it), and `IdSub.unitPtr(0x80 + (u8) k, type)` -- with a
    plain int `k`, combine narrows the plus under the u8 truncation (`force_to_mode`) and prints `addi -128`; the target's
    `addi r4,r29,128; clrlwi` needs the zero-extended operand (`(u8) k`, `(u32) k` or an `int base = 0x80` variable all work).
  - **SellItemNum::move (4 insns, still open) is the #3(c) shape**: the target defines the PRE'd `&digit` register BETWEEN
    the argument moves and the call (`addi r3,r27,IdSub@l; li r4,0; li r5,31; addi r17,r1,8; bl unitPtr`). Ours: `R = fp+8`
    is loop pass 2's hoist of the k-body's `&digit[n-1]` base, RTL-last in the k preheader, free (REG_N_CALLS_CROSSED 4), so
    sched1 issues it in the MesSet call's own cycle (t=30 ready list `1555 131`: the call takes the bpu, the addi the free
    iu slot). Read from haifa-sched.c: an insn placed after unitPtr in RTL can only depend on MesSet through memory, a
    call-used register or `REG_N_CALLS_CROSSED == 0` -- and every one of those also links it to the LAST call (unitPtr) or
    makes it a predecessor of unitPtr with the arguments' priority (`insn_cost` clamps anti/output links to 1, so the
    asm->call link gives prio 1 + prio(call) = the arg moves'), where the tie falls to LUID and the asm (RTL before the
    arg moves) wins. Tried and rejected (all in the ssw13 harness `v_*.py`): `asm volatile("")` after MesSet (36 lines: the
    prologue's hoisted `lis IdSub`/`li` constants stop at the barrier), memory-anchored `la` asms with `digit` at function
    scope or in an enclosing block (the frame layout is [digit 8..47][info 48..55][fctiwz temp 56]: digit must stay
    block-local in the k body, an enclosing block that starts after the temp keeps it -- `{ int digit[10]; MesSet..; for
    (k..) {} }` is 2 lines, but every asm-produced pointer is alias-opaque and reallocates the loop (57-74 lines)),
    `register` hard-reg argument variables (`register IDSystem* a3 asm("r3") = &IdSub; a4 = 0; a5 = 0x1F; a3->unitPtr(a4,
    a5)` -- the nop `r5 = r5` arg move is folded by cse into a fresh `li r5,31` at the call, so the asm's LUID is still
    smaller). Candidate left: make the asm read r3/r4/r5 (all three hard-reg args) so it is ready one cycle after `li
    r5,31` -- untested because the pointer then has to feed the second digit loop.
  - **PieceSelect::move (113 words, still open)**: literal zeros in the r==2 arm (`x264 = 0; x265 = 0` instead of
    `= state`) make the r==1 and r==2 arms identical RTL and jump2 merges them (161 lines), with or without an `int n`
    getPieceNum result; the target keeps them apart only because cse's zero class differs per arm (r9 = the `mr.` result
    in arm 1, r27 = the `switch (state)` register in arm 2, r25 = the Key OR in case 4). pieceFrameDisp / caseModelMove
    not iterated this pass.
- Sscrn twelfth pass (2026-09, harness ssw14: `sellv.sh`/`psv.sh`/`pfv.sh name=v_x.py` fvar variants over
  `ss_shop.base.cpp`/`ss_pzzl.base.cpp`/`ss_pzzl.cur.cpp`, `dump.sh`, `fn.sh`, `rtlc.py`, `prio.py`, `bcmp.py`, `scopes.py`).
  ss_pzzl closed (64/64, Matching, `Sscrn.rel: OK`); ss_shop still 73/74.
  - **PieceSelect::move 113 -> 0 (one tagged dead store + one tagged dead test, rest zero code)**. The r==1/r==2 arms are
    kept apart by giving each its own zero register: `int st = state; switch (st) { case 0: mode = st; ...` makes `st` the
    zero pseudo of the whole case-0 body (r27 in the target), and the r==2 arm writes `mode = r; x265 = st; x264 = st;`
    (`= st`, not `= 0`: a literal 0 is cse'd to whichever zero heads the class). The r==1 arm keeps its own zero (the
    getPieceNum result `n`, `mr. r9,r3; stb r9`) only if `n`'s REGNO_LAST_UID lies past case 4's Key-zero store: the tagged
    `space = (pzlBoard*) n;` dead store at the end of case 4 (candidate #12, cse2 make_regs_eqv order) does that; without it
    the class head becomes the Key `or.` result (r25) and the arm merges. Case 1's `if (link[3] == 0 && (n =
    space->getPieceNum()) == 0 && wk->type != 4)` puts the getPieceNum call in the condition (`mr.` on the result), the
    else arm is `int h = b->h; b->curY = h - 1;`. Case 3/4 want the store duplicated in both if/else arms (`b->curX = w -
    1;` twice, `b->curX = 0;` twice) — a shared store after the if/else is a different block layout. The 0x80000000 arm
    stores through a pointer local `pzlPiece** psel = &pzzl_sel; *psel = ...ptrPiece(...); transit(0, wk);` (the `lis/stw
    ..@l` split around the call). In `case 1:` of the state switch `msg_open = st;` (the zero again) and arm A ends with the
    tagged dead test `if (st == r) { b = 0; }` placed BEFORE `back2PieceSelect(wk)` (global-alloc priority: extends `st`'s
    live length below `b`'s so b gets r28 and st r27; at the end of the arm the compare is jump-threaded to END and the
    SndCall tails stop cross-jumping). Case 2: `mode &= ~8; switch ((u32) mode) { case 1: break; case 2: ...; case 4:
    transit(3, wk); break; } state = 0;` — the unsigned index plus an explicit `case 1: break;` gives casetree's `cmplwi
    ..; ble default` shape (a default-grouped neighbour value).
  - **pieceFrameDisp -> 0 (tagged: six dead stores, candidate #13; rest zero code)**. The inline `VECNormalize` (through
    `pLog->err`, inline `operator->`) has loop-invariant lifetime 3 and is hoisted in loop pass 1 BEFORE the `&c` copy; the
    target's order needs lifetime 1, so the unit has a `VECNormalizeP(src, dst)` macro that calls `pLog.p->err(...)`
    directly (pass-2 hoist, emitted after the copy). `PSVECAdd(&c, &v[i], &c)` (invariant first) not `(&v[i], &c, &c)`.
    Case 4's tile call has `&v[3]` in r31 and `&v[1]` in r27 only when gcse's expression table has 159 buckets, i.e. six
    more insns at gcse time: `i = 0; i = 1; ... i = 5;` after the corner loop (deleted at flow1, so no bytes). A dead FP
    test (`if (c.x == 1.0f)`) instead adds a pool constant and steals a callee-saved register; a dead test before the
    `switch (type)` steals cr0 from the saved compare (extra `slwi`); a dead test in the corner loop breaks the switch's
    biv elimination.
  - **caseModelMove -> 0 (zero code)**: reuse one `IdUnit* u` for both unitPtr results (`u = IdSub.unitPtr(0, 0x10);
    screenPos2puzzlePos(&u->pos, &q); ... u->scr.y = scr2.y;`), no second pointer local.
  - **REL flip of ss_pzzl**: make_rel refused the module until the COMMON block was 0x34 bytes with `pzlGrid::size` first:
    `f32 pzlGrid::size;` followed by `asm(".comm _7pzlGrid.size,52,4");` (NgcAs accepts the second .comm and widens the
    symbol). The REL shasum then differed in 60 ADDR16 bytes: `msg_open`, `pzzl_cursor`, `pzzl_sel`, `pzzl_dbg` were static
    but scope:global — the target's ADDR16 fields hold A only (scopes.py missed the three .bss ones; it also gives a
    false MISMATCH for `common_Sscrn` and reports vtables as WEAK, both harmless). `pzzl_clear_z`/`pzzl_read_req` stay static.
  - **SellItemNum::move (4 insns, still open) — mechanism confirmed, no accepted form**. `R = fp+8` is loop pass 2's hoist
    (insn 1555, REG_N_CALLS_CROSSED 4), free in sched1, issued at t=30 in MesSet's own cycle; the target needs it after
    `li r5,31`. Every source-level asm gets a smaller LUID than the arg moves and wins the sched2 tie; hoisted insns can
    depend only on the last call. A k-body-top `int* dq; asm("addi %0,1,8" : "=r"(dq) : : "r6");` with `dq[i]` in the
    loop-2 test DOES put the addi in the target's slot (the r6 clobber anchors it to MesSet, loop.c hoists it), but the
    test load no longer carries a `P = fp+8` insn so gcse's r480 (`addi r30,r1,8` in the body) is not inserted at the
    loop-2 header and the byte load becomes an fp+11-based giv with its own callee-saved register (35 lines); `dq[i]`
    in both loop-2 uses drops the recompute (78 lines). Also rejected: dead tests near MesSet (block split, IdSub high not
    shared), do-while barriers, enclosing-block `digit`, and the dq asm plus a dead `dz = digit;` in the test block / before
    the loop to restore gcse's anticipation (v_x3/v_x4: unchanged 35 lines). Candidate left: a form that keeps the test
    load's own `P = fp+8` insn while its giv base is the asm output. (CLOSED in the thirteenth pass, next item.)
- Sscrn thirteenth pass (2026-09-11, harness ssw15 = ssw14 copies with the paths rewritten; ss_shop 73 -> 74/74,
  Matching, `Sscrn.rel: OK`, 111 files OK; 10 of the 11 Sscrn units compiled, ss_term left on its split object):
  - **SellItemNum::move 4 insns -> 0 (tagged `COMPILER-DIFF: #13`): the addi did not need a dependence, it needed to LOSE
    the free slot.** Re-read of the two sched dumps: block 0 (prologue .. the k switch, 5 calls, one basic block) has free
    issue slots at t=8, 22, 23, 24, 30 (MesSet's cycle: the call takes the bpu, an iu slot is free) and t=32 (`li r5,31`
    alone); the prio-1 insns with no dependents (`val = 0`, `n = 0`, `base = 0`, the PRE'd `lis IdSub` 484, and the pass-2
    hoist `R = fp+8`) fill them in LUID order, so the fp+8 hoist -- RTL-last in the k preheader -- lands at t=30. In sched2
    the same insn is `addi r17,r1,8` and r1 IS `call_used_regs` (fixed regs are call-used on rs6000), so it gets the ANTI
    link to MesSet (`sched_analyze_2`, hard-reg use -> `last_function_call`) and unitPtr gets one on it: ready at t=31 with
    the arg moves' priority, and the tie among `addi r3; li r4; li r5; addi r17` (all prio 12, class 3, one dependent) is
    the sched1 LUID order. The target's `addi r17` after `li r5,31` therefore only says that the original's sched1 issued
    it at t=32, i.e. something else took the t=30 slot -- a free insn that left no bytes. Reproduced with a single-use
    constant that update_equiv_regs moves after sched1: `int zero;` at function scope, `zero = 0;` as the FIRST statement
    of the k body (loop pass 1 hoists it to the k preheader, `Insn 193: regno 94 (life 665), global move-insn savings 1
    moved to 1561`, RTL between the gcse insertions and the pass-2 fp+8 hoist 1562 -- pass-1 movables are emitted before
    `loop_start` after gcse's end-of-block insertions, pass-2's after them), and `self->fast = zero;` for the ONE store in
    the `repeat <= 30` arm (`li 0,0; sth 0,18(24)`). sched1 t=30 ready list `1562 1561 132`: MesSet + `zero`; t=32: `li r5`
    + the fp+8 hoist. gcse's cprop cannot fold the `(set (mem:HI) (subreg:HI zero))` store (movhi has no store-immediate),
    the set carries loop.c's REG_EQUAL, REG_N_REFS is 2 and the use is at loop depth 0, so local-alloc's update_equiv_regs
    (`validate_replace_rtx` fails -> `depth == 0` branch) re-emits the `li` right before the `sth` (`.lreg`: insn 1593 with
    REG_EQUIV before 1199, `Register 94 used 2 times across 2 insns in block 50`, r0) -- the same `li r0,0; sth` bytes as the
    literal. Why the store choice matters: a store whose block also sets another constant (`repeat = 30; fast = 1`) is
    reordered in sched1 (the store of the live-in pseudo is free and issues before the other `li`), and the moved `li`
    then has the larger LUID; the `cursor = 1` store after the price MesSet would put the moved `li` behind the
    `addi cMes@l`. Rule of record: **a free hoisted insn in the wrong free slot = count the block's free slots from the
    sched1 ready lists (`-dS -fsched-verbose-6`); one more prio-1 insn with a LUID between the right neighbours shifts
    it by one slot, and a #13 single-use constant (set at a loop-body top, stored once at depth 0) is a free insn that
    vanishes at local-alloc.**
  - ss_shop flip: `scopes.py` flagged `shop_msg` (.data), `shop_pos_save`, `shop_msg_buf` (.bss) as scope:global vs
    static -> made non-static (the vtable WEAK mismatches are the known false positives); bcmp's `.text DIFF at 28 bytes`
    at 0x8128/0x8198.. are the 7 `bl countActiveWork/create` words of the nameless cManager<cLight> block that the split
    object has linker-resolved (no reloc) -- present for every Sscrn unit, harmless; the REL sha1 is the check.
  - fdiff/unit_info still show `REPLACE` lines with identical text and 87-99% for ss_shop's (and ss_pzzl's) functions:
    objdiff's reloc-name comparison against the split's `lbl_Sscrn_*` placeholders, not a code difference.
- The map model globals are named `ssPlModel`/`ssWepModel` (.bss 0x494/0x498, MapMgr works 0/1),
  `ssPlMotion`/`ssWepModel2` (.data 0x978/0x97C), renamed by hand in symbols.txt/sym_map.tsv
  (data labels have no .sym name for the sync tool); the generator attributes them to ss_map.cpp.
- ss_term (src/Sscrn/ss_term.cpp, the codec call screen; Matching since the fourteenth pass, see
  the eof item and the fourteenth-pass item below). Includes in .rodata order: light.h,
  event.h, map_obj.h, widget.h, then `dbg_button.h` — which is now the REAL header (cDbgButtonBase
  / cDbgWindowBase / cDbgButton with their inline virtuals; the 12 menu strings keep their parse
  order, and event.cpp / sscrn.cpp / ss_main stay byte-identical because nothing there constructs
  one). cDbgWindow (the 128-button debug window, key function LocalUpdate) is declared in
  ss_term.cpp *after* MakeCol/DbgDrawBoxFill (its "AddButton(): new failed." string follows
  MakeCol's pool), and SsTermInit/SsTermMain after it — so ss_term includes ss_main.h mid-file,
  after cDbgWindow. Widget<SUB_SCREEN> must be completed before dbg_button.h (a `static inline`
  reading `w->num` at the top) so its vtable comes out last: the .rodata vtable order is the
  reverse declaration order SsTermMain, SsTermInit, cDbgWindow, cDbgButton, cDbgWindowBase,
  cDbgButtonBase, Widget.
  Idioms: `col += (u8)(a * 255.0f) << 24; ...` (MakeCol: `+` chains stay `add`, a single
  expression turns the last one into `or`); cDbgButtonBase's x/y and cDbgWindowBase's x/y are
  `u32` (the LocalDisp int->float conversions have no `xoris`); the box call needs
  `f32 px/py/pw` conversions first, then `f32 ph = 14.0f; f32 bd = 2.0f;` locals and
  `DbgDrawBoxFill(px - bd, py - bd, pw + 0.0f, ph + bd, ...)` (pool 2^52, 14, 2, 0, 0.7, 0.3);
  `(y + 1) + b->y` needs an inline `dbgWindowRow(y)` (fold reassociates the literal otherwise);
  cFileList::init's zero stores are `text, list, cursor, pattern, filter` and `dir(d, f)` passes two
  uninitialised locals (no arg moves); `p = text; num = 0;` (not `num = 0; p = text`) keeps the
  fresh `li r0,0` after a strchr loop whose exit register cse would reuse; `p += strlen(p); p += 2`;
  `if (top + rows > num) end = num; else end = top + rows;` gives the `mr r28,r0` copy; the second
  template array (`char defFilter[12]`) is declared mid-block after the first alloc/strcpy.
  OpeMesTblInit reads the op archive through an inline `opArc(wk)` (pointer reloaded per statement
  because the table stores may alias, and `ofs + (u32) arc` is not reassociated with the +0x400);
  the un-rotated `for (;;) { if (!(s->time > cnt)) { if (!OpeSeqMove(s)) return 1; } else break; }`
  keeps the test at the loop top; `MessageControl* m = &cMes; int i = 0;` declared AFTER the
  preceding call keep `lis cMes` / `li i` below the `bl` (three Delete loops); the `state++` after
  `sscrnMainMenuInit` is `IntSet(x10, 0)` so the following `pSys` load stays below it; the frame
  needs `Vec pos; Vec ang = {0,0,0}; pos = term_pl_pos;` (pos slot first, memset second);
  `modelOn = 0; ended = 0;` come out reversed; `wk->pzzlOfs + (u32) wk->pBuf` (offset first).
  cFileList has an empty ctor and dtor (the empty static init pair and `global constructors
  keyed to MakeCol`), and the file-scope instance is the unreferenced 0x18 of .bss after
  term_read_req.
- CLOSED in the fourteenth pass (next item; was OPEN, ss_term eof): the original writes the vtables of cDbgButton
  and cDbgButtonBase (interface-unknown classes: only inline virtuals) and outputs `~cDbgButton`
  first among the eof functions, `~cDbgButtonBase` after `~Widget`; ours writes neither vtable
  (nothing references them: `AddButton` is never emitted) — .rodata is 0x30 short and the two
  dtors are missing. finish_vtable_vardecl writes an unknown-interface vtable only when its symbol
  was referenced, so the original had a reference our source does not create (or its later SN
  build writes every completed vtable in round 1, which would also explain the exact vtable order).
  `#pragma implementation "dbg_button.h"` makes ours write them but reorders the placed linkonce
  block; not pursued. Also open: terminalCameraInit's pool has four extra floats after the 1.333
  aspect (0.5, 3.1415927, 180, 240) that no instruction loads — mark_constant_pool drops
  unreferenced entries in our build, and `const f32` locals / `if (0)` code / unused inlines all
  emit nothing here (tested).
  Second pass (2026-09) findings: (a) `_._10cDbgButton` stores `_vt.14cDbgButtonBase` (0x3690,
  the base vtable before the inlined `delete name`), nothing in the REL references
  `_vt.10cDbgButton` (0x3630) — the referencing code was an emitted-then-dropped linkonce copy
  (the REL rule drops unreferenced first copies): the original build outputs a *used* but
  unreferenced comdat function (the implicit `cDbgButton::cDbgButton()` that the parsed
  `AddButton`'s `new cDbgButton` marks used, and likewise `Widget<SUB_SCREEN>::Widget(int)`), our
  cc1plus outputs comdat inlines only when `TREE_SYMBOL_REFERENCED` (decl2.c finish_file "stop
  lying" loop) — a compiler-build difference. A dropped stand-in that constructs a cDbgButton
  (`__attribute__((section(".gnu.linkonce.t.X")))` + modules.py LINKONCE_DROP) does write
  `_vt.10cDbgButton` in round 1, `~cDbgButton` right after the static-init function and
  `~cDbgButtonBase` in round 2 (tested), but the target's eof order also needs `~Widget` output
  in round 1 with `init`/`move` in round 2 (i.e. `_._t6Widget1Z10SUB_SCREEN` referenced by
  pre-finish_file code while `_vt.t6Widget` is not), which no source construct gives here
  (a direct dtor call inlines; an explicit ctor instantiation writes the vtable in round 1 and
  pulls init/move forward). Not applied. (b) The four dead floats are the pool of dead code
  (`tan(fovy * 0.5f * PI / 180.0f)`, `/ 240.0f` — the screenPos2puzzlePos formula) that the
  original kept: its varasm does not run mark_constant_pool (ours does, varasm.c
  output_constant_pool). The only zero-code reproduction is a file-scope
  `asm(".section .rodata; .long 0x3f000000,0x40490fdb,0x43340000,0x43700000; .text")` right after
  terminalCameraInit (compiler-build difference candidate #10; not applied since the unit cannot
  flip anyway). (c) partnerType's table had 12 leading zeros; the original has 14 (`{0 x14, 1 x5,
  2 x4, 3}`, .rodata 0x2a8..0x308) — fixed.
- Sscrn fourteenth pass (2026-09-11, ss_term flipped: `"Sscrn/ss_term.cpp": True` + STRIP_UNUSED in modules.py,
  `Sscrn.rel: OK` with all 11 units compiled, main.dol and 106 files OK -- the 5 wep00/wep34-37 RELs were a
  transient wep_mod.h `PSet` redefinition. Harness ssterm (deleted): the ssw14 scripts, `eof.py FILE.s`
  (definition order of a cc1plus .s with LO/VT tags), and an instrumented copy of tools/sn-gcc under the harness whose
  cc1plus prints with `FF_DUMP=1` every finish_file round: each saved_inlines decl with TREE_USED / DECL_EXTERNAL /
  DECL_NOT_REALLY_EXTERN / DECL_COMDAT / TREE_SYMBOL_REFERENCED, each vtable walk decision and each "stop lying" un-lie;
  the tree rebuilds byte-identical to the installed cc1plus in ~1 min, `make cc1plus` after a decl2.c edit in seconds):
  - **finish_file model, measured on ss_term.** A deferred inline (every in-class/template body: finish_function sets
    DECL_EXTERNAL=1 + NOT_REALLY_EXTERN and queues it) is output in round N only when that round's stop-lying loop
    finds its assembler name TREE_SYMBOL_REFERENCED -- set solely by varasm `assemble_name`, i.e. by asm text already
    written that mentions the name (a `bl`, a vtable word, a definition) -- or the decl is !COMDAT (interface-known
    class virtuals); wrapup_global_declarations then outputs in saved_inlines order (a do-while, so a decl referenced
    by output of the same wrapup follows in the next iteration). Vtables are written in the walk (reverse declaration
    order) when interface-known or referenced; mark_vtable_entries marks the entries used (template members are
    instantiated there). Our positions before the change: cDbgButtonBase members 508-523, cDbgWindowBase 524-530,
    cDbgButton 531-534 (dbg_button.h), cDbgWindow 824-832, `_._t6Widget1Z10SUB_SCREEN` 846 (instantiated by ss_main.h's
    ssWidgetDelete), ~SsTermInit 922, ~SsTermMain 938, cFileList 954-955, quit 957 / init 958 / transit 959
    (SsTermInit::move's inlined transit), move only in round 2. Read the order off the dump, not off theories: the
    second pass's "the original outputs every used comdat" is falsified here -- ~cDbgButtonBase is TREE_USED at parse
    (~cDbgButton's body) and still comes out in round 2 in the target.
  - **The three target differences and their source forms.** (1) `_vt.10cDbgButton` in round 1 and ~cDbgButton as the
    first eof function = a parse-time-emitted function that inlines cDbgButton's implicit constructor:
    `cDbgWindow::AddButton` written OUT OF LINE (db_toolbase.cpp style) between DbgDrawBoxFill and FindButton, so its
    "AddButton(): new failed." keeps its place after MakeCol's pool; never called, the original REL link dead-stripped
    the body (modules.py STRIP_UNUSED for the unit -- the REL link strips unreferenced globals too). The implicit ctor
    stores only the derived vptr, so `_vt.14cDbgButtonBase` / ~cDbgButtonBase stay in round 2, and the .rodata vtable
    order becomes round 1 [SsTermMain, SsTermInit, cDbgWindow, cDbgButton] + round 2 [cDbgWindowBase, cDbgButtonBase,
    Widget] = the target's. (2) ~Widget right after quit in round 1 needs BOTH a saved_inlines position after quit
    (so NOT instantiated by ss_main.h's ssWidgetDelete) AND `_._t6Widget1Z10SUB_SCREEN` referenced before round 1's
    stop-lying loop while `_vt.t6Widget` stays unreferenced until ~SsTermInit's round-1 output. No natural construct
    gives a non-inlined dtor call once the template has saved insns (mark_used instantiates an inline member at once
    inside a function; `delete`/`p->~T()` through a pointer dispatch virtually). Device, tagged `COMPILER-DIFF:
    candidate #8` (end-of-file order family): `template <> Widget<SUB_SCREEN>::~Widget();` declared before
    dbg_button.h (mark_used -> instantiate_decl returns the specialization, nothing is instantiated; the declaration
    also completes the class, replacing the old ssTermWidgetNum), a dead `static void ssTermWidgetKill(Widget<SUB_SCREEN>*
    w) { w->Widget<SUB_SCREEN>::~Widget(); }` compiled while the specialization has no body (no DECL_SAVED_INSNS ->
    real `bl _._t6Widget1Z10SUB_SCREEN`, assemble_name sets TREE_SYMBOL_REFERENCED; body dead-stripped), and the body
    `template <> inline Widget<SUB_SCREEN>::~Widget() { Mem_free(link); }` after SsTermInit::move (queued after quit;
    comdat like the instantiation, identical bytes, inlined into the synthesized ~SsTermInit/~SsTermMain in round 1).
    (3) The four dead floats are the pool of a dead-stripped function right after terminalCameraInit: `static void
    screenPos2terminalPos(Vec*, Vec*)` with ss_pzzl's screenPos2puzzlePos body (0.5, pi, 180, 240 in RTL order; a pool
    is output before its function, so it follows terminalCameraInit's 1.3333334). Candidate #10's asm was not needed.
  - **.bss order with a constructed file-scope object** (invisible to bcmp -- NOBITS -- but not to `make_rel --verify`):
    the target's `term_read_req` is the unit's first .bss word (ADDR16 fields 0x508), the cFileList instance follows.
    A constructed object's `.lcomm` is emitted while the static-init function is generated, a plain file-scope static
    only at the end of the file, so the read request is a function-local `static int` of SsTermInit::move (same code).
  - Pipeline facts: configure.py runs strip_unused BEFORE fold_linkonce for a module unit (verified identical REL in
    that order); when testing a pipeline by hand the object must be named `<unit>.o` -- ngccc.py derives the unit from
    the output stem and silently skips place_linkonce_module otherwise (63 linkonce sections left, wrong layout).
  - Flip bookkeeping: sync_rel_symbols renamed `cDbgButton_dt_cDbgButton` -> `_._10cDbgButton` and
    `cDbgButtonBase_dt_cDbgButtonBase` -> `_._14cDbgButtonBase` in the module symbols.txt / sym_map.tsv; scopes.py
    clean (term_ope_tbl global, the four Vec statics and the .bss local; the vtable WEAK lines are the known false
    positives); bcmp's 28-byte .text DIFF is the nameless cManager<cLight> block's linker-resolved `bl`s, as in every
    Sscrn unit.
- ss_model (src/Sscrn/ss_model.cpp, the character and weapon model builders; 40/47 byte-identical,
  .rodata and .data identical, .text +12): include order map_obj.h, light.h, widget.h, atari.h;
  `PL_ARC(n)` = `PL_ARC_PTR(pG->pPlArc, n)` re-read per call (pG reloaded); the model archive at
  SUB_SCREEN::x210 is an `SsArc`; `ssModelAdd(m, bin, tpl)` = `m->addModel(ssModInfoMgr.create(bin,
  tpl))` (cSsModInfoMgr got an asm-labelled `create__11cModInfoMgrPvT1`); the light set is one
  `static inline ssModelLight(m)` whose two `static const Vec` are the single .rodata copy after
  weaponFilename's strings; per-character `static Vec pos/rot; static f32 scale` locals land in
  .data at their function (`SS_MODEL_PLACE` stores pos, rot, then scale z, y, x); weapon hang =
  `wep->pParts->pParent = m->getPartsPtr(10)` + stores written per field (an inline taking f32
  parameters hoists the constants across the call); the `scale = 0.5` half-scale switch needs
  `case 0x13: case 0x16: case 0x17: break;` labels (they root the tree at 0x17 and let the compare
  be shared with the later switches through `mfcr`/`mtcrf`) and a `cModel* p = wep->pParts` local
  for the three stores; wep11 is two switches (`case 0: default:` / range pairs); wep01-04/06 are
  `if (type == 0) .. else if (type == 1)`, wep10 `if (0) .. ; if (1) .. else ..`, wep13's colour
  bytes are stored in index order and its `pParent` store is a `PSet` (the pG load must stay
  below it); the unit ends with an unreferenced `static int = 0` (.data 0xA40). playerModelInit
  passes the u8 weapon number/type through int-parameter aliases (COMPILER-DIFF 4). SOLVED
  (third pass, 2026-09-10, 46/47): the scale statics are one-element arrays `static f32 x_scale[1]
  = {1.0f}` read once into a block-local `f32 sc_ = (s)[0];` AFTER the pos/rot word copies
  (SS_MODEL_PLACE): the array element is an in-struct MEM, so sched1 keeps its `lfs` below the
  `m->rot` stores (fixed scalar vs varying struct would not alias) and the `lis` floats up into the
  callee-saved r28/r29, while the local holds the value for the three `m->scale` stores (a direct
  `x[0]` re-reads it after each store); the magazine flag is `cModel* one = (cModel*) 1;` declared
  before `wep->be_flag |= 2` (the constant's pseudo before the `ssWepModel2` high: `li r11,1; lis
  r9`). wep09Init SOLVED in the sixth pass (Matching): the three modelInit arms are a plain `if / else
  if / else if` chain (arms 0 and 1 both end in `b JOIN`, so jump2 cross-jumps arm 0 four insns deep
  into arm 1; the old `if/else if` + separate `if (type == 2)` let arm 1 fall through into the test,
  whose block-ending call got the flow.c `use` nop) - it was never compiler-build difference 6. Old residual text: the
  six character inits load the scale static's `lis` early into a callee-saved register (ours
  right before the `lfs`; chain / local / order variants tried) and wep09Init's two modelInit arms
  are cross-jumped in the original (compiler-build difference 6). Second pass (2026-09): the
  `lis` position is a consequence of the scale `lfs` staying BELOW the `m->rot = rot` word-copy
  stores in the original (the load-of-a-global-above-a-member-store rule, "Matching rules" above)
  while ours floats it above them (fixed scalar vs varying struct: no alias) so the `lis` stays
  adjacent; a `static f32 x[1]` array makes the load in-struct (ordered after the stores, `lis`
  hoisted into r29) but then every `m->scale.? = s` store re-reads it (3 `lfs`), and a `f32 sc =
  x[0]` local moves the load above the copies. No FSet-style lever exists for the block copy.

### Open

- t_emlist.cpp (0x6174 of code: a 0x3E0 work block behind a struct-member pointer reloaded after every
  store, 122 `const char*` name tables and a
  64-entry `{char name[16]; const char** flag, *type, *set, *x}` id table, TOOL_MENU-like char[]
  menus) has 36 of 53 functions matched (skeleton, data and menus done; the disp/camera/target functions
  are left); the stage rooms are split (config/G4BE08/modules.py); r10d, r10e, r11a (st1_2), r109, r107,
  r10a (st1_1) and r102 (st1_1 + st1_3) are Matching, r108 (st1_1/st1_3) is Matching since pass 4
  (2026-09-10); r11d, r113 (st1_3) and r203 (st2_0) are Matching since pass 5 (2026-09-10); r118 (st1_3), r101 and
  r105 (st1_1) are Matching since pass 6 (2026-09-10); r10f (st1_3, 11/14), r11e (st1_3, 16/18) and r119
  (st1_2, 26/27: only Init's table-address registers differ) have full sources (include/obj00.h,
  obj13.h, objGondola.h are their room-side views of the DOL objects); r10c (st1_2, 15/23,
  .rodata/.data equal) and r11b (st1_2, 8/14 + the nameless cLight block, .rodata equal) are written;
  st4_0 r410, r40b, r411 and st2_3 r22b, r229 are Matching, r40a (8/9) and r22a (6/7) written;
  r100 (st1_0) is Matching, r117 (st1_3, 20/21) and r11c (st1_3, 19/23 + reloc-name-only) are written
  (see "Stage rooms, st1 r100/r117/r11c pass"); the other st2/st4 rooms are unwritten (cSceObj.cpp,
  which r40c/r406/r40e/r220/r225 need, has no source yet);
  em_wrap.cpp matches in st1_0/st2_4 (Matching) and is one register-allocation diff away elsewhere.
- db_light.cpp (src/tools/db_light.cpp: the light editor, 0x12408 of code in t_camera/t_light/t_event;
  Tools = the same object with `SetToolLight` in front (`tools/db_light_tools.cpp`, DB_LIGHT_SET_TOOL_LIGHT),
  t_esp = Tools + `cLightTool::setLogMode` (`tools/db_light_esp.cpp`, DB_LIGHT_SET_LOG_MODE); every
  other function of those three objects is byte-identical to t_camera's) is fully written: 128 of
  134 functions byte-identical modulo relocs (Tools 129/135, t_esp 129/136; .rodata/.data/.bss
  byte-equal), see the db_light idioms at the end of this section. Remaining: printEditTable (-8:
  the flag-column `x*8` constants are cprop-folded in ours, the target keeps `x` a variable after
  the first `"P"/"-"` diamond and has two surviving `mr rX,y` giv copies), editColor (-0x10: the
  `tmp`/`c` 4-byte slot order — a by-value GXColor helper puts a 4-byte expansion-time temp first but
  one per inlined call), draw_light_graph (14 words: the `%1.6f` eprintf's `li r5,0` is scheduled
  2nd in the target and `col` gets r5 there), edit_cutsel (-4, the reverse #2/#4 `u8 line` case),
  edit_light_id_shadow (3 words, #2 mask one call later), edit_light_parent (r8/r10 for `n`, the
  case-2 `(id >> 16) + 101` temp untied). Not MATCHING anywhere yet.
  t_sce / t_movie (`tools/db_light_v2.cpp`, 0x1143C, unwritten): 120 of t_camera's functions are
  byte-identical there too, but the object has no cLightTool ctor/dtor/move/lightAnalysis/getCutNo,
  no cLitPathTool ctor/dtor/expand and no cVarRange/cVarLoop members except limitUpper/limitLower
  (the vtable is still there), starts with SetToolLight and its .bss ends with a 0x1C object at 0xA8
  instead of pLightEnv: an older build whose tool object lives elsewhere.
- The `.drs` archives are not rebuilt by `ninja` (`tools/drs.py rebuild` does one at a time); the
  sound bank's record types 1/2 and the p0/p1 parameters are not interpreted.
- Unit boundaries inside the big modules (Tools has ~39 source files) are not known; the `.gnu.linkonce`
  orphan sections of Sscrn/t_esp/t_event/t_id/t_movie/Tools/t_sce (their original ELFs had 12 extra
  sections between .text and .ctors, concatenated behind .text in the REL).

- em10 CLOSED (seventh pass 2026-09-10, 382/382, all 16 modules Matching; LostHead = a dead `if (a == 3)`
  in `case 0: default:` falling through into case 1 -> gcse PRE inserts the compare at the dispatch block and
  the else arm; setHand = a dead `if (w->x184 == 0) type = 0;` whose PRE copy pseudo holds r11 -- see the
  "Ganado shared library" seventh pass). The old note, for the record: `em10LostHead`
  (59: `a == 3` PRE'd into cr4 at the END of the last multi-predecessor block before the join on each path
  -- the switch dispatch block for cases 0/1/2-then and the case-2 else arm's first call block -- a
  placement no LCM/PRE gives for a single occurrence: our gcse leaves the compare at the join; `case 3:
  break;` gives 164 words, `if (a == 3) asm("")` in the else arm 72) and `setHand` (2: `no` r10 vs r11 with
  no r11 user anywhere -- global-alloc pass 0 has r11 free in ours; needs an r11 conflict that exists only
  in the original). `em10FindCk` is closed (the untagged bell override `r = 25000.0f;` after the switch, see the #13 sweep; the hard-register launder was not needed). Do not re-attempt
  LostHead without a mechanism that hoists a non-redundant compare (interblock motion, #5 family).

- Room helpers that exist under the same name in several rooms of one module (`em_reset`, `em_destroy`,
  `setTexRender`, `setLadderMotion`, `slide_move` in st4_0) MUST be `static`: r400/r405 (and flipped r406's
  `setTexRender`) still define them global and will clash in the -r link once a second such room flips.
  Make them static before flipping (sync then reports 0 renames; REL24 calls are binding-independent).
  Status 2026-09-10: r403's `reset_40..46` and r204's `setTexRender` made static; r405/r406 both still define
  a global `setTexRender` and link (ngcld -r keeps one symbol-table entry, the REL bytes are unaffected).
  A duplicated name stays a placeholder in symbols.txt (`setTexRender_A194`, `reset_40_7178`: the sync
  "keeps" it because the mangled name already exists at the first copy) — objdiff/unit_info then show 0%
  for a byte-identical function; compare with /tmp/rooms_a/mm.py (pairs leftovers by .text order).
- COMPILER-DIFF candidate #11 (temp slots): two freed 12-byte Vec slots of a block are merged by
  `combine_temp_slots` into one 24-byte slot; a sibling block's first Vec takes the whole slot (24-16 < 16
  forbids the split) so its second Vec gets a fresh slot (+0x10 frame); the original reuses both. 16-byte
  objects split fine. Explains frame diffs in r200/r20e/r221/r225/r402.
- `rank_for_schedule` in the SN source is stock: priority, then register weight, then class vs last
  scheduled, dependents, LUID; anti/output dependences cost 1.
- cse AROUND path: `beq` around a single-set block followed by a re-test of the same condition folds the
  second jump to `b`; the original keeps the conditional jump (r104 `execEvent00`) -- cse-pass difference.
- `IntSet(r->last, d->last)` on the LAST field of a struct copy keeps the following `lwz pG` below the copy.

## Don'ts

- Never change the semantics of a shared tool (strip_unused.py, fold_linkonce.py, sync_symbols.py,
  ngccc.py) to fit one unit. Make the new behaviour conditional on the evidence that distinguishes
  your case, and re-run the full `ninja` + DOL check before and after.

- Do not run `python3 configure.py` by hand before ninja: ninja runs it when a config file changed, and
  configure rewrites build.ninja/objdiff.json only when their content changed (restat rule). Concurrent
  ninja processes clobber `.ninja_deps`/`.ninja_log` ("premature end of file; recovering"), which makes
  `dtk split` re-run, rewrites config.json and re-triggers the manifest ("manifest still dirty after 100
  tries"): run one ninja at a time.

- Never conclude "compiler-side difference". Every such verdict in this project has been overturned
  (#1, #3, #5, #6, #7, #8, #9, #13, M1, M5, M6, the ss_term eof order, the view initPerspective PRE
  pattern "proven impossible" in pass 3 and reproduced in pure C++ in pass 5). When an RTL-level proof says
  the target is unreachable from the current source, the proof is right about the SOURCE SHAPE, not about
  the compiler: the original had a different structure — one shared pointer instead of two, an inline
  boundary elsewhere, an argument expression instead of a variable, a wrong constant or case value, a
  missing parameter, a different declaration order. Read the target for the structural signal (hoist
  order, which addresses recur, which values live across calls) and rewrite the shape. Tagged asm forms
  are a stopgap, to be removed when the structure is found.

- Never edit `build/`, `build.ninja`, `objdiff.json`, or `config/G4BE08/splits.txt` by hand. The same goes
  for `config/G4BE08/modules/<mod>/splits.txt`: change unit boundaries in `config/G4BE08/modules.py` and
  re-run `tools/gen_rel_config.py`.
- Do not run interactive `objdiff-cli diff`; use `tools/fdiff.py` (one-shot).

- Loop rotation is decided by stmt.c `expand_end_loop`'s scan for a jump to the loop end within the
  first ~30 insns: `while (1)` + a deep `break` stays un-rotated (test at top, `b top` at bottom);
  `if (!c) {...} else break;` stays un-rotated; `do { if (call()==1) break; SceSleep(1); } while (1);`
  gives the un-rotated poll; `for (;;) { if (c) break; }` and `while (c)` get rotated + duplicated test.
- Named struct arrays with non-constant initializers get a `memset` per row (C++ TYPE_FIELDS includes
  the class-name TYPE_DECL); the original used plain `void* tbl[N][M]`.
- A cEm local (0x3E0) against em.h's 0xDE0 cEm: `struct { u8 buf[0x3E0]; }` + `cEmConstruct asm("__3cEm")`
  + qualified `((cUnit*)&em)->cUnit::~cUnit()` reproduces frame, inlined dtor and the linkonce copies.
- After `sync_rel_symbols.py` the module split objects are not re-split by ninja; delete
  `build/G4BE08/config.json` to force it, otherwise unit_info/fdiff report stale names.
- COMPILER-DIFF candidate #7 -- CLOSED 2026-09-10 as a SOURCE FORM (not a compiler difference; see "#7/#9
  closed" in docs/research/compiler.md). The shape `A0; cmp0; b TEST; TOP: sleep; A; cmp; TEST: bcc TOP; store` is
  what our own jump.c produces from `for (;;) { A; if (c) { x = lim; break; } SceSleep(1); }`: the exit
  store on the break path sits between the copied exit jump and its `b END`, so jump1's jump-over-jump fold
  cannot fire, and jump2's fall-through cross-jump then merges `store; bcc TOP` (and any register-identical
  tail) of the copy into the loop's. r106 shakeClosetDoorR/L/Body, r11c closeGate (unit Matching), r103/r105
  execOpenCover and r202 throwRock are all 0 words from that spelling. Nothing installed.
- Room idioms found on r11d / r10f / r11e / r119 (src/st1/, 2026-09):
  - `RsfSet`/`RsfClear` store through a cast-then-deref word (flag_rsf.h `RsfFlagWord`, not
    `MEM_IN_STRUCT_P`): the rooms reload `pG` and their static work pointer *after* an RsfSet
    (`lwz r0,4(r3); oris; stw; lwz r9,pG`), which only a store that may alias fixed scalars gives. It
    does NOT explain the pool `lfs f1, 0.0` issued after the RsfSet store (r108/r118/r11d
    execShowView: `RTX_UNCHANGING_P` loads never depend on stores) - still OPEN.
  - Poll loops: `while (f() != 1) SceSleep(1);` = `b test; body; test: bl; cmpwi; bne` (r11d
    checkIronDoorKeyUse); the SceSleep-first door swing (`SndCall; L: SceSleep; rot -= spd; ...;
    if (!(rot < lim)) goto L`) is `goto open; wait: SceSleep(1); open: ...` like r113 (constants
    reloaded per iteration because a goto loop has no loop notes).
  - `int eff = EspPullCoreKind();` with `(u8) eff` at every use gives the `clrlwi r10,r23,24` at
    EstSet and one PRE'd `clrlwi r30,r23,24` before the three Effect*Delete calls (r10f DoorOpen); a
    `u8 eff` local masks nothing.
  - Template-copied locals (`int list[11] = {...}`, `Vec pos = {..}`) whose copies the target issues
    *after* a run of calls are declared mid-block after those calls (C++), and a `cEmWrap em;` whose
    ctor `bl` follows them is declared after them too (r11d execEmAppear_end, r10f DoorOpen).
  - COMPILER-DIFF #4 in the rooms: `setPtr(s16,..)`/`setEm(s16,..)` called with `int list[i]`
    elements get `lwz` straight into r4 in the original (ours `lhz; extsh`): int-parameter aliases
    `cEmWrapSetPtrI asm("setPtr__7cEmWrapsSci")`, `setEmI asm("setEm__FsSciii")` (r11d).
  - `for (i = 0; i < 11; i++) f(list[i])` over a local array: pointer compare `cmplw r31,r28; ble`
    needs a `u32 i` (`int i` gives `cmpw`); ours still initialises the loop pointer straight from the
    template-copy register where the original keeps an extra `mr r4,r8`/`mr r31,r4` copy (OPEN).
  - A work pointer whose element stores are followed by a reload of both the pointer and the element
    (`stw r3,0(r9); lwz r11,work; lwzx r3,r11,r29`) is the one-member-struct global (r10f
    `R10fWorkPtr r10f_work; r10f_work.p->gondola[i] = ...`); a typed `PSet(cObjGondola*&, ..)` gives
    `stwx r3,r29,r9` (index first) instead. Single pointer fields keep the typed `PSet` (r11d `mi`,
    r11e `rock[i]`, r119 `dog`).
  - Two accessor results stored through one local (`cLight* l; l = LightMgr.getWorkPtr(2); l->power
    = a; l = LightMgr.getWorkPtr(6); l->power = b;`) share one register (r10 twice); separate
    expressions get r10/r11 (r119 ThunderFlagOn/Off).
  - Repeated scroll-object blocks in the Evt_*_Func handlers (`w = SmdGetWorkPtr(id); if ((obj =
    SmdGetObjPtr(id)) && w) { setPos(&w->pos); setAng(&w->rot); }` x4) are written out with two
    function-scope locals (`cObj* obj` before `SmdWork* w`: obj r31, w r30); an inline helper with
    its own locals swaps the registers. The `SetMod(name, obj, 5, 0, 2, 0); setPos; setAng; be_flag
    |= 0x20; EspSetModelPtr` blocks are written out too: a helper taking the name string evaluates
    the string address before the `SmdGetObjPtr` call (r119 Evt_R119S00/S20_Func).
  - `pG->flags_60 >= 0` on the u32 field must be written `(int) pG->flags_60 >= 0` (`cmpwi; blt`);
    the unsigned form folds to true and the whole test disappears.
  - A call result tested and used in one block (`mr. r3,r3` after `SmdGetObjPtr`) is a block-local
    variable; a function-scope `cObj* obj` also used in other blocks gets `mr. r31,r3`.
  - `switch (e->funcMode)` with `cmpwi 1; beq; ble end; cmpwi 2; beq` has an empty `case 0: break;`.
  - `if (cnt > 899) for (o = ObjMgr.pAlive; o; o = o->next)` is the plain rotated loop; an explicit
    `&& ObjMgr.pAlive != 0` in the `if` adds a `mr r9,r0` copy of the head.
  - Routine bytes `xFC..xFF = 0` written in that source order are issued `ff, fc, fd, fe` (r11e
    funcAshley).
  - OPEN (r11d checkEmReset): `for (;;) { while (count > 10) SceSleep(1); setEm(tbl[i]); i++; if
    (i == 10) break; SceSleep(60); SceSleep(1); }` - the original keeps `cmpwi r31,9; li r3,0x3c;
    addi r31,1; bne` with SceSleep(60) laid out before the inner loop body; ours hoists the `i+1`
    into the outer loop header (interblock scheduling) and never cross-jumps the trailing
    `SceSleep(1)` into the inner loop body. goto / do-while / `i++ == 9` forms tried.
  - OPEN (r11e / r119 Init): the `lis` pseudos of the pos/rot table addresses passed to six
    `SatMgr/EatMgr.create` calls get callee-saved registers pair-wise (rot above pos in the original,
    pos above rot in ours for some pairs); arrays, eight separate statics, pointer locals and a dozen
    symbol names tried (not a name-hash effect).
  - OPEN (r10f GondolaGetOn/GetOff): `&posA[side]` is formed off the `mot` table's frame pseudo
    (`add r4,r26,r23; addi r4,r4,0x18`) and `mulli r26,r22,0xc` is issued before the SndStrReq call
    in the original; r11d appearLittleSister: a `void* zero` local's `li r31,0` survives next to the
    `andis.` result that cse merges it with in ours.
- Room idioms found on r10c / r11b / r229 / r22a (st1_2, st2_3) and r410 / r40b / r411 / r40a
  (st4_0; r410, r40b, r411, r22b, r229 Matching, 2026-09):
  - Work pointer reloaded after a store *through* it (`stw r3,0x74(r9); lwz r9,work; lwz 0x70(r9)`,
    `sth hp; lwz work` chains) = the one-member struct global (`R10cWorkPtr r10c_work; .p->`) as in
    r10f; stores into the work that are followed by a `pG`/`pPL`/`pSys` load in the original need a
    typed `PSet(cSat*&, ..)` / `U32Set(cnt, cnt + 1)` on top (a struct store lets ours hoist the
    fixed-scalar load above it). The calloc store whose `lis work@ha` sits in a callee-saved register
    *before* earlier calls is a reference variable `R11bWork*& wp = r11b_work.p;` declared at the
    top (`wp = MEM_CALLOC(..)`); ours then merges that `lis` with the PRE'd one of the later loads
    (r11b -4 bytes, OPEN), r229 (no later loads in Init) matches.
  - Two `EM_LIST(n)` byte stores with one `pG` load (`lbz flags; stb x3; ori; stb flags`) are written
    through a local `EmListData* l = EM_LIST(n);` (a QI store reloads `pG` otherwise), and an
    `EM_LIST(n)->x3 = 0` after an `EmSetFromList2(n, ..)` whose `pG + 0x5xx8` address is computed
    *before* the call is `EmListData* l = EM_LIST(n); em = EmSetFromList2(n, 1); l->x3 = 0;`.
  - `cPlayer* p = pPL; p->setPos(&v); p->setAng(&v)` when one `lwz pPL` feeds two calls (`mr r3,r30`
    twice); `pPLS->setPos(); pPLS->setAng()` (the struct view, twice, no local) when the original
    reloads pPL per call but keeps the first load below preceding Vec template stores (r10c ItemGet).
  - Frame-slot reuse decides block scoping: r10c/r22a's rope event has `{ Mtx m; ... FadeSetW(2,30);
    SceSleep(30); ObjMgr.destroy(obj); } { Vec pos2 = {..}; Vec ang2; f32 ry; Vec* pa = &ang2; ... }`
    in each arm — the else arm's FadeSetW colour pair lands at a fresh 0x68 slot because `m` is still
    live there, and `pos2` reuses 0x38 because `m` is dead; `Vec* pa = &ang2` declared at the block top
    puts the `addi r28,r1,0x48` into a callee-saved register before the FadeSet.
  - `if (RsfCheck(..) == 0) { SceSleep(1); } else break;` inside `for (;;)` is the un-rotated poll
    (`test; bne exit; sleep; b test`); `do { .. if (c) break; SceSleep(1); } while (1);` for the
    un-rotated mid-body break (r10c ItemGet), and `while (call() == 0) SceSleep(1)` for the
    `b test` form.
  - `dir ? (a < lim) : (a > lim)` as an `if` condition gives the `blt L; b L2; L34: ble L2` pair of
    arms (r10c SwitchExec); `if (n == 0) return; if (n == 1) return;` keeps two `beq`s where
    `n != 0 && n != 1` / a switch range-folds (r40a em_set).
  - `int skip = 1; if ((e->status & 0x40000000) == 0) skip = 0;` in a `static inline` (`li 1; andis.;
    bne; li 0; cmpwi`) — the `? 1 : 0` ternary on a single bit folds to `extrwi`; an inline helper
    with `void*& mod` keeps one GetMod slot for every event cut (r11b Evt_R11BS00_Func).
  - `cObj* obj = 0;` at the top *used* as an EstSet argument is the SI zero pseudo set in the first
    block (`li r27,0` before the first branch) that later byte stores (`l->x3 = 0`) and stack args
    share; unused it is deleted and the zero is created at the EstSet (r11b Init).
  - `if (spdY < -100.0f) spdY *= 0.8f; else spdY *= 0.935f;` gives the cross-jumped `fmuls` after the
    two `lfs f0` arms; `spdY *= k` with a `k` local fuses into the following `fmadds`. A member load
    reused across blocks (`dy = obj->pos.y - lim; ... obj->pos.y < lim`) must be written twice
    (gcse PRE copy `fmr f11,f0`), not cached in a local (r10c hako_down).
  - A block of `for (i = 0; i < 80; i++) { SceSleep(1); .. }` with a constant start has no entry
    test (`cmplwi 0x4f; ble` with `u32 i`); `f32 spd = 0.0f; f32 max = 100.0f;` declared mid-block
    (after the `BitOn(obj->be_flag, 0x20)`) keep their `lfs` below the preceding calls (r22a EleDown).
  - `FSet`/`FAdd`/`FSub` on `SmdGetObjPtr(id)->pos.y` where the original reloads `pPL` after the
    store; `pPL->pos.y = K` stores that reload pPL between each other are `FSetP(pPL->pos.y, K)`
    (r22a), `wheel->rotSpd.z` stores followed by a `pG` load are `FSet` too (r10c moveWheel).
  - A room whose `.rodata` carries `"event/evd/rNNNsXX.evd"` / `"evt_.._func"` strings with no code
    using them had a never-called static function (r229 `r229_evtSetup`): the original REL link did
    strip room objects at function level too; add the unit to modules.py `STRIP_UNUSED`.
  - Unit boundary: st2_3 r22b's group starts with the HALT string that r22a's pin had swallowed
    (0x2A88 -> 0x2A78); always check the last words of the previous room's `.rodata`.
  - TexRender rooms (r10c, r229, r11b): `u8* tbl = r10c_texTbl;` local for the blend table (`addi r25`
    kept, `stb 0xf7,4(r25)`), `tex->sy = tex->sx = 0x40` chain, per-object `x136 = 2; x137 = 0x12;
    x138 = 0xA0;` written in that order for every object (the scheduler emits 138,136,137 or
    138,137,136 per block by itself); `TexRenderModRes(cModel*)` reads a parts number from r4:
    `void TexRenderModResP(cModel*, int) asm("TexRenderModRes")`; `ModelInfoRefrectOn` is C++
    (model.h). `u8 GetEmIdFromList()` passed on unmasked: `int GetEmIdFromListI(u32) asm(..)`.
  - OPEN: independent `stfs` of two pool constants into a Vec (`v.x = 3145; v.z = 10394; v.y = 0`)
    come out x-first in the original and z-first in ours whatever the statement order (r10c
    EmEvent/EmEvent_exit, r40a first_init: the FPR pair f0/f13 swaps with them); the second word
    pair of a `Vec = {..}` template copy is loaded 8-then-4 in ours (r10c/r22a rope event; `static
    const Vec` sources fix the stores but not the loads); `-100.0f` in a nested `if` inside a large
    loop is hoisted by our second loop pass and not by the original (r10c hako_down);
    r10c SetEmHitAtari's 0.01/0.05/0.06 initialisers sit in the pool between the first if's
    Yarare constants and its else constants while their loads precede the first RsfCheck;
    r11b EmSetChange's `stb x3 = 0` is issued last by the original and second by ours.

- Locals whose frame slot sits inside freed inline-table slots must be declared after the getter
  calls: `assign_stack_temp` best-fits into the merged freed region; only fresh allocations extend the
  frame (trans `ShadowCastSetup`/`SelfShadowSetup`).
- A load the original does not hoist above scalar-global stores means the stores were not
  `MEM_SCALAR_P`: write the increments as `ISet(g, g + 1)` (an `INDIRECT_REF` of a plain pointer sets
  neither IN_STRUCT nor SCALAR, so `true_dependence` keeps the order).
- Branch arms ending in a call block cross-jumping of a shared tail (flow.c appends
  `(use (const_int 0))` after a block-ending CALL_INSN); duplicate the tail through a non-call
  statement into each arm to get the original's merged `li r7; bl`.

- A ctor that stores a base field before the vptr store has it in a base-class initializer
  (`Event::Event(u8) : cUnit(1)` -> `cUnit(u32 flag)` overload).
- A `u32` passed to a `u8` ctor parameter with no `clrlwi`: declare an int-parameter alias with
  `asm("__5EventUc")` and call it (GNU v2 ctors return `this`).
- `pWork[i].field` written at every use (no element pointer local) reloads pWork after each store.
- `if (ok) { body; return 1; } err; return 0;` places the err block at the end and cross-jumps it.
- `int n = 37; for (i = 0; i < n; ...)` gives `blt end`; a literal bound folds to `i <= 36`/`ble`.
- Prototyped `memset(p,0,12)` is a plain call; a zero aggregate initializer is the `crclr`+`memset`
  libcall -- both coexist in one TU.
- Address-taken scalars declared before a `char buf[]` get frame slots after the array unless the
  array is declared in an inner block after the first `&scalar` use.
- `Obj18Work* w = &obj->o18` produces `addi r11,r9,0x328; lwz 0x68(r11)` instead of a folded offset.
- A single-variable fade helper (`c = 0xFF; start = c; c = 0; end = c;`) delays the `li r0,0` to just
  before its `stw`.

- A `const T x[] = {..}` declared `extern` in a header is emitted at its definition; with internal
  linkage it is deferred behind the cManager template strings (cam_ctrl `smooth_ratio`).
- Empty in-class `C() {}`/`~C() {}` on a vtable-less class emits no body but makes its global object
  emit at the definition point (controls `.bss` order vs deferred plain arrays).
- `char st = member; switch (st) { ... member = st + 1; }` reuses the loaded byte for the increment.
- `(on & A) || (on & B)` on one lvalue folds to one mask; separate `andi.` tests need inline helpers.
- A do-while `{}` macro flips FPR assignment of two independent RMW chains vs a plain block.
- Constant folding needs the exact product literal (`0.8f * 1.2f`, `PI * 0.35f`, `1.33333333f`).
- A `lis rX,0x8023` with no reloc in the split object next to our `@ha` reloc is a dtk pairing miss.

- An expression computed identically at the end of both if/else arms is merged by jump2 into one insn
  placed before the join label, ahead of the join block's own loads.
- Frame size: total = ALIGN8(8 + ALIGN8(vars) + fpmem(8, +4 if `-(fp+gp)-8` not 8-aligned) + ALIGN8(fp+gp)).
- gcse PRE pseudo numbering can wrap (hash mod table size) and flip two giv registers; one extra
  pseudo before the copies (`int dead = 0;`, COMPILER-DIFF) fixes it. Register-priority ties are
  broken by qty/allocno number, not host qsort.
- A strength-reduced index passed to a call (`f(id - i)`) puts the giv init `li` after the hoisted
  invariants; a separate variable puts it before. A `do{}while(0)` macro around a loop inflates the
  loop-weighted refs of hoisted invariants.
- A float local assigned twice gets global alloc; one variable per value keeps all local-alloc'd
  (f31..f27 by declaration order). A loop counter shared by two loops is one low-priority pseudo.
- `no = g->x; switch (no) { case N: num = N; }` folds the case constant into the switch register.
- `bitTblChk((u32) used, i)` (integer table parameter of an inline) gives `lwzx` with the offset in a
  BASE_REGS register.
- Tools: fdiff.py writes a per-pid json, so concurrent runs do not clobber each other.

- db_light (tool module, 0x12408): every store through the tool pointer reloads it (a struct member,
  `LightToolPtr.p`); a group of u8 stores through it with *one* load is a chain
  (`p->a = p->b = p->c = 0`: all `p->` loads precede the stores, and the stores come out outer,
  innermost, ..., i.e. `x10 = x11 = x12 = x13 = 0` stores 10, 13, 12, 11). A `bool` is 4 bytes here.
- A pointer global whose load must stay *after* a store through an unrelated pointer type (pathSelect:
  `path[i] = NULL; createPath(pLitPath)`) is a struct member too (record alias set 0);
  a plain static is hoisted above the store.
- Menu loops: `eprintf(x, C + i * 14, ..., *name++)` (y a giv of i, the name a separate pointer biv)
  gives `li rY,C` last in the preheader and keeps `i` (`cmplwi i,N; ble`); a `y += 14` variable or
  `tbl[i]` indexing drops the counter for a pointer compare.
- `for (i = 0; p->data[i] <= 200; i++)` with an unsigned x step (`u32 xi += 4`) converts with the
  2^52 magic (no `xoris`); `(f32) (int) (u8) x` / `(f32) (s16) (u16) x` pick the signed conversion of
  a zero-extended byte / half (`lbz; xoris` / `lhz; sth; psq_l qr5`).
- `static int state = 0;` goes to .data (explicit zero init), `static f32 val;` to .bss; function-local
  statics are emitted in text order, file-scope statics after them in declaration order.
- `#line N "D:/Bio4/Prog/db_light.cpp"` before a `VECNormalize(...)` macro use reproduces its
  `__LINE__` (0xBA1, 0xC1B, 0xFF2).
- A `GXColor black = {0,0,0,0}` local is folded to `li 0`; the original loads the constant from .rodata
  once (r24) and copies it (`stw r24, 8(r1)`) into a 4-byte `GXColor tmp` local before each
  `DrawTile(&tmp)`: by-value GXColor helpers give 8-byte BLKmode temps instead.
- `GXSetChanAmbColor(0, whiteCol())` (inline returning the struct) plus `Mtx44 proj; Mtx mtx;`
  declared *after* the colour calls lays the frame out as arg temp 0x8, proj 0x10, mtx 0x50, colour
  0x80 (DrawTile).
- `if (a) { if (b == 0) return 0; return 1; } return 0;` puts `li r3,0` before each `beq` to the
  epilogue (SetToolLight); `if (!a) return 0; if (!b) return 0; return 1;` shares one `li r3,0` block.
- `cursor = (mode == 2)` is a setcc (`xori; subfic; adde`); `!= 2` (and every other spelling) branches.
- `pTool->cursor %= 0x100` on a u8 member gives `rlwinm r0,r0,0,23,23; subf` (the range-narrowed
  `% 256`).
- `u8 num = (a < 60) ? 60 : a;` keeps the `clrlwi` truncation at the join; `u8 num = a; if (num < 60)
  num = 60;` drops it.
- In a switch with `default:` written first, the default body precedes the case bodies and the case
  stores cross-jump into one `stb` (save's mode -> cursor).
- `sprintf(path, ...)` then `cDbLit lit;` (a mid-block declaration) constructs after the call.
- A switch on `pTool->cursor` inside `case 1:` whose cases all end in `pTool->x8 = 2/3/4; cursor = 0`
  is emitted as one shared tail (`stb r0,0x28; stb r11,0x6`) with `li` pairs per case (edit_tune).
- Things that did not move the needle (register / `lis @ha` pseudo choice): declaration order of
  locals, `int` vs `u32` counters, `xi = x` before the loop vs in the `for`, `by-value` vs `const&`
  GXColor helpers. The remaining db_light diffs are of that kind.
- db_light, second pass (121/134 in t_camera, 121/135 Tools, 122/136 t_esp; .rodata/.data now byte-equal):
  - Judge with a masked byte compare of the split vs compiled object (relocated fields masked on
    *both* sides, 16-bit relocs sit at word+2): objdiff's % hid the `xAxis` value (it is (0,1,0)), the
    editColor black template, `logX = 24.0f`, `sz = gamma2 = 0.25f`, `x1C = 3000.0f`, `* 0.1f` in move,
    string-space mistakes and the 0x400921FB60000000 double (`atan2(..) * 127.0 / 3.14159265f`, an f32
    PI widened).
  - "First-entry init" blocks (`if (pTool->x8 == 0) { x9 = cur->xD; x8 = 1; }`) have an else arm with
    a dead local set in both arms (`first = 1` / `first = 0`; one dead set is deleted by jump1, two
    survive to cse): the then arm ends in a jump, cse cannot skip it, and the join's `pTool` load
    re-uses the block's own `lis LightToolPtr@ha` instead of the hoisted one (select_type,
    shadow_select_type, edit_light_id, edit_light_select_sub, edit_light_parent).
  - Clamps are if/else with a store per arm (`if (cursor + 10 < 0xFF) cursor += 10; else cursor =
    0xFF;`): the stores are cross-jumped after reload and the join is a new cse ebb (fresh `lis`);
    `n = ..; if (n > 0xFE) n = 0xFF; store` is a skippable block that keeps the shared @ha register
    (edit_cutsel_main).
  - `cVarLoop::limitUpper` declares `range` before `v` (limitLower the other way round): the
    declaration order decides the load/compare schedule of the entry block.
  - A chain `l->x138 = pad[0] = pad[1] = pad[2] = 0` stores 138, 13b, 13a, 139 (initLightWork).
  - drawPath: the data pointer steps (`u8* d = p->data` declared with the locals, `*d`, `d++`) while
    `i` stays a counter; the `for` increments are written `xi += 4, i++, d++` (`addi xi` before
    `addi i`).
  - createLit: the same `cLightEnv* c` local in the table loop and the memcpy loop ties `c` to r4
    (the memcpy argument) in both.
  - cLitPathTool ctor: the else arm stores through a reference (`cLightPathHeader*& p = pLitPath; p =
    getPathHeader()`), which evaluates `lis LitPathPtr@ha` before the call into a callee-saved reg.
  - `static const` file-scope objects are output after the vtables in the original .rodata (finish_file
    order: vtables, then deferred namespace statics, in declaration order): the black GXColor template
    of editColor (0x1630) and the (0,1,0) axis (0x1634, an `f32[3]`: 4-aligned, a `Vec` would be 8).
    tools/ngccc.py now puts module `.gnu.linkonce.d.*` vtables in `.rodata` in place (like the DOL
    path) instead of fold_linkonce appending them; all 111 files still OK.
  - printEditTable's header table is a file-scope `static const char* table_head[]` defined *before*
    the inline row printer: an inline's string literals are emitted at parse time, a function-local
    static's initializer strings after the body.
  - editColor: `black` is a local copied from the deferred template (`GXColor black = blackTemplate;`
    right before the first use: one `lwz` into a callee-saved reg, `stw` into `tmp` per DrawTile); a
    `{0,0,0,0}` initializer folds to `li 0`. Open: the frame order tmp(0x8)/c(0xC) — both are
    ADDRESSOF pseudos and ours forces `c` first (`c.g = 0` is `(plus (addressof c) 1)`), the
    original forces `tmp` first while still storing c.r/c.g/c.b before the first DrawTile.
  - COMPILER-DIFF #2 in edit_light_id_shadow: the original zero-extends the u8 `col` once before the
    two uses of the else arm (`clrlwi r30,r30,24`, PRE-shared); reproduced with `int c = col; asm("" :
    "+r"(c)); (u8) c` except that our sched1 places the mask after the arm's first call (calls do
    not end sched blocks here). edit_cutsel's `line`: `u8 line = i + 6` gives the target's frame and
    size but a `clrlwi` where the original has a plain `mr` (the reverse #2/#4 case), so `int line`
    stays (-4 bytes).
  - Open register-only diffs: edit_light_type_spotlight/direct/parallel (the `spotRot`/`dirRot`
    `high` pseudo shares r30 with the `Vec* rot` pointer because the pointer's `addi` is scheduled
    after the `x = 0` store in the original and before it in ours: `lis r30; stfs @l(r30); addi
    r30,r30,@l`), drawLightInfo_SpotShadow (`mr r3/r4` of Draw_corn2 hoisted above the `len == 0`
    branch — COMPILER-DIFF #5), edit_light_parent (`n` r8 vs r10: the case-2 temp `(id >> 16) + 101`
    is not tied to `id >> 16`), lightAnalysis (+0x1C), move (+0x10), draw_light_graph (-0x34),
    printEditTable (-0xC, one more callee-saved register and a smaller frame in the original),
    edit_light_type_shadow_fit (-4).
- db_light, third pass (128/134 in t_camera; lightAnalysis, move, shadow_fit, drawLightInfo_SpotShadow,
  spotlight, direct, parallel now byte-identical):
  - `ObjMgrWork(i)` (obj.h) has the `no >= nArray` range check; lightAnalysis' scan loop has none:
    a local `objWorkNoChk(i)` (`pArray + size * no`). `if (objWorkNoChk(i)->isAlive()) { cObj* obj =
    objWorkNoChk(i); ...}` gives the target's `lwzx r0,r11,r9` (be_flag through the folded address)
    plus a separate `add r31,r11,r9` for the pointer; one `obj` local used for both gives `lwz 0(r31)`.
    move's bounding-box loop *keeps* the check (`blt L1; li r31,0; b L2`), written with a
    `cObjMgr* m = &ObjMgr` pointer inside the inline (`objWorkChkP`); the plain obj.h form lets
    thread_jumps + cse fold the check away (ours), the pointer form does not.
  - `addi r3,rObj,0x164; mr r29,r3` (an address computed into the argument register, then copied) is
    a gcse PRE copy: `obj->lightInfo.getLightNum()` called twice with an if/else *between* the two
    calls (the join label ends cse's ebb, so the second `&obj->lightInfo` is PRE'd: the precomputed
    argument pseudo dies at the `mr r3` and local-alloc gives it r3). A `cLightInfo* info` local
    gives `addi r30; mr r3,r30` — cse rewrites the later `&obj->lightInfo` into a copy of the arg
    pseudo even when it sits in a then-block (conditional jumps do not end cse's ebb; only labels do).
  - Two identical `0x150 + i*14`-style givs (`eprintf(x, 0x2A + i * 14, ...)` in *each* arm of an
    if/else inside the loop) are NOT combined by combine_givs when each is single-use (loop.c refuses
    to combine into a single-use DEST_REG giv): two `li rY,0x2a` in the preheader and two `addi 0xe`
    in the latch. A `y += 14` variable gives one.
  - A common address in both arms of a store diamond (`stb r3,3(r9)` / `stb r26,3(r9)` after ONE
    `lbz anaNum; lwz anaTbl; slwi; add`) is a pointer local computed before the `if`:
    `u8* p = (u8*) (anaNum * 4 + (u32) anaTbl);` (integer sum: index first in the `add`).
  - `int st = state; switch (st) { case 0: ...; case 1: color = st; }` reuses the switch register for
    the store (`stb r11,0x1e`); `color = state` reloads the member because the case label starts a new
    cse ebb. The `if (c) x = 0; else x = 0x14;` diamond order: `int c; if (modeSel == 0) c = 0; else
    c = 0x14; eprintf(.., c, ..)` gives the target's `li r5,0x14; beq; li r5,0` (jump.c's "x = b; if
    (...) x = a" hoists the *else* set when both arms are single constant sets and the if/else is a
    statement); the ternary `modeSel == 0 ? 0 : 0x14` as a call argument gives `li 0; bne; li 0x14`.
  - `gameCutNo = cutNo = getCutNo();` (dying-first rule: the store of the chain's last assignment is
    issued first, so the target's `stb 0x15; stb 0x14` order needs cutNo innermost).
  - `BitOff(pG->flags_170, ..)` (reference store) makes the following `pG->flags_60 &= ~..` reload
    `pG` from a PRE'd `high(pG)` register (`lis r28,pG@ha` inserted at every state-switch exit).
  - `pLog->x = (int) logX; pLog->y = (int) logY;` with one `lwz pLog` and no `lfs logY` reload: `int x
    = (int) logX; int y = (int) logY; cLog* l = pLog.p; l->x = x; l->y = y;` (the `sth` through
    pLog->p may alias `this->logY` and `pLog` itself).
  - draw_light_graph: the static graph parameters are `x0/y0/w0/h0/s0` — the static names decide
    gcse's `high(sym)` hash order and thus which of the two loop-hoisted address pseudos (gx/gy) gets
    r20/r21 (`gx/gy`, `x/y`(clash), `px..`, `graph_x..` all gave the swap). The distance and the
    1000-step marker share ONE variable `x` (`x = GetDistance3(..); if (x < l->x1C ..) { t = x / scale;
    ...}; func_attn(l, x); for (x = 1000.0f; ...)`) — that puts d in f30 and issues `fmr f1,f30` before
    `mr r3,r26` in the `%3.6f` call; `t = d / scale` is computed inside the then-arm only (the else arm
    stores the compare's 0.0 register as a.z/b.z). `col`: `v = func_attn(l, w0*scale); col = 0; if (v >
    0.04f) col = 6;` then `int c = col; asm("" : "+r"(c)); eprintf(.., (u8) c, ..)` (COMPILER-DIFF 2:
    the original masks the u8 argument; `col` must not cross the call so its `li` stays after the `bl`).
  - COMPILER-DIFF 2 with two uses (edit_light_type_shadow_fit): `int c; if (..) { eprintf(ON); c = 0;
    asm("" : "+r"(c)); } else { eprintf(OFF); c = 0x14; asm("" : "+r"(c)); }` then `(u8) c` at both
    eprintfs: the launder must sit in the ARMS — gcse's LCM only hoists/shares an expression whose
    operands are not set earlier in the same block as its first occurrence (`clrlwi r5,r5,24; mr r27,r5`
    at the join = the first extension into the arg register plus the PRE copy for the second use). With
    the asm in the join block (`int c = col; asm; (u8) c` twice) both uses extend separately.
  - COMPILER-DIFF 5 (drawLightInfo_SpotShadow): `Vec* pp = &pos; Vec* pn = &n; asm("" : "+r"(pp),
    "+r"(pn));` before the `if (len == 0.0f)` puts Draw_corn2's `mr r3,r25; mr r4,r26` above the
    branch (the laundered pseudos die at the call's arg copies and take r3/r4 in local-alloc).
  - `lis r30,spotRot@ha; stfs @l(r30); addi r30,r30,@l` (the `high` and the `Vec* rot` pointer share
    a register): compute `f32 sx = -(f32) pTool->joy.sx / 1000.0f;` BEFORE `Vec* rot = &spotRot;`
    (the psq_l/fneg/fdivs chain then follows the `x = 0` store and the `addi` is issued after it).
    spotlight's per-case `step` is ONE function-scope `f32 step` (cases 2 and 3 share f11); parallel's
    case 1 uses two variables (`c` for normal.x, `d` for normal.z) with the function-scope `const f32
    k` (one `f32 c` reused for both chains swaps f30/f31).
  - printEditTable: `y` is a giv (`0x150 + i * 14` written at each use: the `%02d` eprintf, the
    `printEditRow(l, 0x150 + i * 14, c)` argument, the EMPTY WORK eprintf) — its reduced register is
    `li r23,0x150` after the hoisted `lis` and the row printer's non-const `int y` parameter copy
    survives as `mr r29,r23` (the target has a second copy `mr r26,r23` used by the first "%s" column
    only — unexplained). `u8 c` (the colour goes through a temp: `li r5,..; mr r31,r5`), `col.a =
    l->color.r; col.r = col.b = col.g = l->color.r;` (two loads: a first, then g, b, r), and the swatch
    through a by-value helper (`drawColorTileC(int, int, int, int, GXColor c) { DrawTile(.., &c); }`: an
    address-taken by-value parameter gets a 4-byte SImode `assign_stack_temp` at the expansion, before
    the purge-time slot of the address-taken `col`, so the frame is temp 0x8 / col 0xC / fpmem 0x10).
    Each inlined call of such a helper allocates its OWN 4-byte temp (keep=1 slots are never freed), so
    editColor's nine DrawTiles cannot use it (frame +0x20).

