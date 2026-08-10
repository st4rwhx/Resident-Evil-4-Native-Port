### Inline-vs-macro sweep (integrate.c levers; model 93->94, cam_extra 32->33, r214 15->17, mercenaries 16->3 words, em35 10->4, Espgen42 122->72; 2026-09-10)

Generic mechanisms (all read off tools/sn-gcc/src/gcc/integrate.c / cse.c / gcse.c / haifa-sched.c and
confirmed on the units named):
- Inlined pool loads lose RTX_UNCHANGING_P (`copy_rtx_and_substitute`, MEM case, `!map->integrating`), so
  a float literal inside an inlined BODY sinks below every preceding pointer store of the block, while
  the same literal as a MACRO or written out floats above. Both directions are levers:
  - target pool `lfs` ABOVE a store that precedes it in source = macro / written out (em35CriticalTurn:
    `lfs f2, PI` above `stw timer`; the pl0f family);
  - target pool `lfs` BELOW stores that precede it in source = a real inline in the original: model
    `cModel::cModel` (`stfs 0.0 litArea.scale` after the three word stores -> `LightAreaInit(&litArea)`
    inline, 9 -> 0 words), mercenaries MercSysInitRoom (`lfs f1,0.0` right before `bl SndStrReq`, below
    the table stores -> `MercStrReq(no)` wrapper inline that owns the 0.0f literal, 16 -> 3).
- Inline f32 ARGUMENTS are expanded at the call head, before the body and before any call inside the
  body: a pool constant loaded before a `bl` and first used AFTER it (e.g. `lfs f30,-0.05` before
  `bl setPos`, used by the following `setAng`) is an argument of an inline that contains both calls.
  With cse merging the arg pseudos of later calls (a `mem/u` is not `in_memory`, so `invalidate_memory`
  on the call does not kill it), the constant is loaded once and kept callee-saved (r214
  `r214_emPosAng(w, x, y, z, rx, ry, rz)`: exec3rdEmSet 27 -> 0). A literal that is RELOADED in the
  target (`lfs f0, 0.0` after a loop whose compare already holds 0.0 in f31) was a body literal (no
  /u -> gcse cannot merge it across the loop's stores), e.g. a `setAngXYZero` variant of the helper.
  Scanner: /tmp/inl_sweep/scan_argload.py lists such loads in the target `.s` files.
- Inline-owned `Vec` temp: `assign_stack_temp(BLKmode, frame, keep=1)` is freed at the end of the
  statement, so consecutive inline calls REUSE one 16-byte slot (r214 exec3rdEmSet: eleven position /
  angle blocks share frame 8; a caller-level `Vec v` would take that slot and push the temps to a second
  one, cf. pl0f). The inline frame is a pseudo with `REG_EQUIV (plus fp N)`: the argument sets get the
  constant address (`addi r4,r1,8` recomputed per call), the stores are frame-direct.
- `Vec*` parameter on a caller local that is NOT at frame offset 0: the parameter pseudo stays
  (`addi r31,r1,0x28; mr r4,r31`), field stores go through it (`stfs f13,4(r31)`) and cse folds only the
  zero-offset `.x` store to the frame (`stfs f0,0x28(r1)`); the pseudo is callee-saved when the Vec is
  reused after the call (r214 execCatapult 40 -> 0 with `r214_emPosAngY(w, &v, x, y, z, ry)` whose rx/rz
  zeros are body literals: the 0.0 is loaded after the setPos call, only the 0.44 argument before it).
- BY-VALUE `Vec` parameter of an inline (`AddWaterPowerCore(EspgenWork* w, Vec v)` called with a global
  `Chk_pos`): integrate copies the argument into a stack temp through an address pseudo
  (`addi r11,r1,8; lwz ..; stw r10,8(r1); stw r0,4(r11); stw r8,8(r11)`, the .x word direct) and passes
  that pseudo to the body's calls (`mr r4,r11; mr r5,r4`), and the pseudo dies at the first call; an
  inline-local `Vec v = Chk_pos` gives frame-direct stores and `addi r4,r1,8` (Espgen42 AddWaterPowerSub
  122 -> 72; the rest is k/idx register naming and `lfs 0(rSum)` vs `lfsx`).
- Value-returning clamp inline (`f32 scopeClamp01(f32 v) { if (v < 0) return 0; if (v > 1) return 1;
  return v; }`) for the target shape `lfs f13,0.0; fcmpu; ..; fmr f13,f0; lfs f12,1.0; fcmpu; ble;
  fmr f13,f12; stfs f13` (one store after the join, the 0.0 register doubling as the result):
  cam_extra CameraScope::move 259 -> 91. Its remaining diff is a 0x10 bigger target frame plus one more
  callee-saved GPR (an inline with a Vec temp somewhere; `scopeYure(this, &yure2)` with a `Vec*` out
  parameter reproduces the `addi r11,r1,0x58; stw 8(r11); stw 4(r11)` copy but costs words in
  combination — not applied).
- `FRef(f32&)` on `static f32` range constants (IdScope::move, now 0 words): the loads stay below the
  stores through the `IdSys.unitPtr()` results (a plain static read is hoisted above stores through a
  call-result pointer); `b->rot.y = 0; b->rot.x = 0` in that order.
- haifa: EVERY memory read after a CALL_INSN gets an anti-dependence on the call (`last_pending_memory_flush`,
  no RTX_UNCHANGING_P exemption), so a pool load can never be scheduled above a call: a constant in the
  target before a `bl` was there in RTL order (argument evaluation, a declaration initialiser, or gcse PRE
  into the block that ends with the call — `insert_insn_end_bb` puts the copy before the call).
- Negative results of this sweep (do not retry the same forms): motion `nearOne` as a macro (+56 words),
  cam_ctrl r0_RailBehind `VecSet/VecZero(&ang)` (no change), emwep emWepEscapeCamMove (the six camera
  literals as an inline helper: a `Vec*` parameter gets a callee-saved pseudo, inline-local Vecs get a
  freed temp slot instead of the four permanent ones; the target's `lfs` below `stfs fovy` is still
  unexplained), shadow MakeSoftShadow as a `SoftShadowPass` inline (+100), em_set EmSetWork (the target's
  kx/kp/kr load interleave is not a plain macro/inline switch; EmSetFromList is Matching with the inline),
  r226 R226EventRoboStartMain (`setAngXYZero` / caller-Vec pointer forms 19 -> 23), mercenaries
  `wk->stage = 0` (`li r11,0` after `lwz pG`: inline/int-local forms unchanged, 3 words left).
- Detector caveat: the mcmp per-function keys use absolute `.rodata` offsets, so a pool change in one
  function makes every later function "differ" until the section matches again; judge by the unit total.

### COMPILER-DIFF #13 sweep (em21, em27, em28, em3d + wep07/08/33 pl_shotgun Matching; em3c 46/47, em22 70/72, emrock SetRock, mercenaries MercSysInitRoom; 2026-09-10)

- Harness /tmp/cd13 (copies of /tmp/em2f_p `tryv.py MOD/UNIT FUNC variants.py [--asm NAME]`, `mcmp.py MOD
  [SYM]`, `mdump.sh MOD/UNIT -dX`; /tmp/dolties3 `dtryv.py UNIT FUNC variants.py`, `dmcmp.py UNIT [SYM]`,
  `dsbs.sh UNIT SYM [OBJ]`, `ddump.sh UNIT -dX` for DOL units; /tmp/wep_last `tryv.py wep07/pl_shotgun ..`
  with `SRC=src/wep/pl_shotgun.cpp` for the shared weapon sources). REG_EQUIV of the pseudo was checked in
  the `-dl` dump before every launder (`Register N ... set 3 times; user var; FLOAT_REGS` with
  `REG_EQUIV (const_double ...)` on each arm set for the bell `r`; `REG_EQUIV (const_int 30)` on the
  single QI set of em21DmCk).
- Results (words before -> after): em21 WakeCk 25 -> 0 (untagged override form), em21DmCk 4 -> 0 (`register
  int c asm("r9")`), em3c R1_Die_Normal 3 -> 0, em27DmCk 3 -> 0 (both: function-scope `int zero = 0` used
  once as the EstSet stack argument, tagged #13), em28 EscapeCk 23 -> 0 (override form), em3d ChainGunMove
  15 -> 0 (not #13: the GUN_AIM macro temporaries as FUNCTION variables, see below), em10 FindCk 0 -> 0
  with the tag removed (override form), em22 R1_Wait 16 -> 0 (block-local `int zero = 0` before the pG
  test), emrock SetRock 24 -> 0 (`asm volatile("" : : "r"(zero))` after the store block, #13), mercenaries
  MercSysInitRoom 3 -> 0 (`register int z asm("r11")`, #13), pl_shotgun wep07_r3_fire00 2 -> 0 (`register
  f32 zero asm("fr0")`, #13; wep07/08/33 flipped). Not #13 / left: cam_qfps init 12 (below), em_sub
  EmRackCk 49 (below), sce_at sceAtFunc_pos_jump 5 (below), shadow make_comn_fit/parallel_light 2+2 (the
  1.0 pool HIGH is r11 in the target = a reload spill register; a compiler-generated high cannot be named),
  em3a/em38/em23/em2a/em3b/em35/em36 residues (register ties, branch polarity, store order: none has a
  constant loaded late or a REG_EQUIV pseudo in the diff), Sscrn PieceCombine::move (61 words of register
  allocation around the `curX = 0` cse-path residue; the unit cannot flip), em10 setHand `no` (r10/r11, no
  REG_EQUIV pseudo involved).
- The int shape has two zero-cost forms besides the hard register: (a) EstSet-style stack constant after
  a compare (em3c R1_Die_Normal then-arm, em27DmCk): a FUNCTION-scope `int zero = 0;` used exactly once in
  another block. sched1 then sees `stw zero, 0xc(r1)` as a leaf (issued after `mr r3`, before `li r4`,
  like a substituted constant store) and update_equiv_regs moves the `li` right before it, where it takes
  r0 (the shortest qty) — `li r0,0; mr r3,r31; stw r0,0xc(r1); li r4,-1; stw r31,8(r1)` is what the
  original's reload rematerialisation + sched2 give. cse must not know a register equal to 0 there (em3c
  keeps its dead `do {} while (0)` before `case 0:`; em27's `zero = 0` is assigned after `em->dmHit = 0`
  so the QI zero store does not merge into it). (b) A zero whose `li` the target issues at the top of the
  test block before the pG load (em22 R1_Wait's third EmRoutineSet): a block-local `int zero = 0;` declared
  before the `if` (two uses -> not moved by update_equiv_regs, scheduled as a free insn at the block top).
- Dying-store shape of #13 (emrock SetRock, 24 -> 0): the original's zero pseudo (REG_EQUIV 0, never
  allocated, reloaded per label region: `li r30,0` again after the join) does NOT die at `seAlways[2] = 0`,
  so the 30-store block is issued in pure source order; ours would hoist that dying store to the block
  top. `int zero; zero = 0;` before the block + `asm volatile("" : : "r"(zero)); // COMPILER-DIFF: #13`
  right after the last store of the block keeps our pseudo live past it; the second block's zeros stay
  literals (the original's fresh `li r30,0`/`li r0,0` after the label). Caveat: the volatile asm
  invalidates reload_cse (a later `mr r3,rX` copy the original's reload_cse deleted reappears when the
  asm sits between the copy's source and a call: cam_qfps init, +1 word) and a non-volatile `asm("" : :
  "r"(x))` does the same, so the launder only works when no call argument copy follows in the block.
- cam_qfps init (12, OPEN): the same dying-store family — eleven reference stores in pure source order in
  the target (0.8/0.0 both in f0, 0/1/2 in r7/r10/r8 = reload spill registers with inheritance) — the
  order is reproduced by the asm launder (all five constants + the flags RMW temp live past the block) but
  the names are not (r9/r7/r10 rotate: allocated pseudos, not spill registers) and the `mr r3,r31` of the
  following call reappears; `register int` variables for the constants get copied into pseudos by the
  `u8&`/`s16&` setters (worse). Left.
- em3d ChainGunMove (15 -> 0, NOT #13): the two EM3D_GUN_AIM copies share the four clamp constants (cse
  merges the second copy into the first's pseudos, all hoisted); the target's f29 for `angY` and f28 for
  limX come from ONE multi-set `angY` (both copies write the function's variable) that conflicts with every
  constant and is allocated after limY (f30); macro-local `angY`s are two short block-local pseudos and
  the first takes f30. Rule: when a macro is expanded twice and the target's register order puts a
  per-copy temporary below the shared constants, the temporary was a function-scope variable.
- em_sub EmRackCk (49, OPEN, NOT #13): the target hoists the 400.0 clamp constant in loop.c's SECOND pass
  (last `lfs` of the preheader, highest allocation priority -> f27) although its set is inside the
  maybe_never region and used in six blocks — our loop.c (scan_loop: "used in basic blocks other than the
  one where it is set ... && maybe_never" -> unsafe; the comment says the old behaviour that allowed it for
  non-user temporaries "was removed") never hoists it. Setting `xmax` at the loop top hoists it in pass 1
  (longest life -> f23, 44 words with `const f32` pool-order declarations); adding an `asm("" :: "f"(xmax))`
  use gives the target's registers (14 words) but the preheader `lfs` order (400 first instead of last)
  cannot follow: not applied.
- sce_at sceAtFunc_pos_jump (5, OPEN, #13-consistent): the 0.0 (two stores) is f13 and `dstAngle` f0 in
  the target = the only local-alloc'd FPR pseudo took f0 and reload's 0.0 the next free register; ours
  gives the 3-ref 0.0 f0. `register f32 z asm("fr13")` fixes the registers but the hard-register set is
  scheduled unlike a reload insn (issued first, so the `rot.z` store overtakes `rot.y`: 2 words in all six
  store orders); pseudo forms (`f32 z = 0.0f` first, laundered `a`) keep 5.
- Bell-radius rule of record (supersedes the hard-register recipe for this shape): keep the three
  identical `switch (pG->bell_stat)` arms and write `r = K;` once after the switch; no tag needed. em3c
  FindCk, em10 FindCk, em21 WakeCk, em28 EscapeCk, em2d FindCk (recipe recorded for its owner) all match.

#### #13 compiler-side research (2026-09-10, /tmp/equiv13; NEGATIVE: no configuration or one-line change reproduces it; nothing installed)
Harness: /tmp/equiv13 = the /tmp/sched5 h.py harness (tree snapshot refreshed from src/ at 15:10; base =
installed cc1plus, 18591/19929 identical) + `tree_plain/` (hardlinked copy with the 15 launder-dependent
functions reverted to their plain source: em10/em21/em28/em3c/em2d bell blocks without the override,
em21DmCk `em->dmType = 0x1E`, MercSysInitRoom `wk->stage = 0`, em27DmCk/em3c R1_Die_Normal/db_port SeqSet
EstSet literal 0, em22 R1_Wait literals, em2a `EmRoutineSet(em,1,2,0,dead)`, SetRock/em2b R0_Init/em39 ctor/
cam_extra cutin without the asm launders, wep07 fire00 `m3r[2] == 0.0f`, em32 R0_Init literals, r11b `l->x3 = 1`,
item `s[i].id = 0xFFFF`, esp16 `t = 0.0f`, r201 without the `on` launder; `bp.sh CFG` builds them, `t13.py CFG..`
prints the test-set table + full-tree regressions split into launder functions / others; `dump.py CFG UNIT
[--plain] -dl -dg` keeps the lreg/greg dumps in d/; `mk.sh NAME "-Ddefs"` builds a cc1plus variant from the
hooked copy of tools/sn-gcc in gcc/ -- the hooks are `-D` only, base rebuilt with no defs is byte-identical in
output to the installed compiler; NOTE mk.sh must rm the insn-*.o objects or a previous variant's defs leak).
- **Source diff (SN v1.79 vs stock, stock 2.95.2 + 2.95.3 fetched to /tmp/equiv13/stock):** global.c,
  local-alloc.c, regclass.c are stock apart from a `reg_alloc_count` debug counter, xmalloc instead of alloca
  and PROTO removal. `update_equiv_regs`/`no_equiv`/`validate_equiv_mem` are byte-for-byte stock 2.95.3
  (multi-set "always the same value" rule present; the REG_N_REFS==2 substitution/move only for
  REG_BASIC_BLOCK<0). global.c's only exclusions are REG_LIVE_LENGTH == -1 (setjmp) and the `>= 0` test in the
  allocation loop (-2 is never set anywhere in 2.95; the "parameters marked -2" comment is a GCC 1.x leftover).
  reload1.c is based on **2.95.2** (2.95.3's reload_reg_unavailable / free_for_value_p / regno_clobbered_p mode
  changes are absent) + SN: TARGET_PS_CALLSAVE caller-save, PS subreg in gen_reload, reload_cse skips PSmode
  insns, `reload_cse_regno_equal_p` returns 0 for a pseudo sreg, move2add only for equal mode sizes ("Jeppe
  $4 bug"). reg_equiv_constant/memory_loc/init (INSN_LIST), alter_reg, order_regs_for_reload (uses==0 regs
  in REG_ALLOC_ORDER first), allocate_reload_reg (round-robin from last_spill_reg over the function's
  spill_regs), choose_reload_regs inheritance: stock 2.95.2. reload.c: header only. rs6000.md movqi/movhi/
  movsi/movsf insn conditions (`gpc_reg_operand (op0) || gpc_reg_operand (op1)`) and the expanders (force_reg
  of a constant when the dest is a MEM) are stock; expr.c/stmt.c SN changes are PS subregs, `__sync`,
  "SNAPS migrated back from GCC3.0.4" builtins and the switch-table threshold -- constant-store expansion
  is stock. So (a)/(b)/(c) of the assignment: no SN change touches the equivalence path; whatever the
  original does is not in the v1.79 drop.
- **Full-tree results** (regressions = identical with base that stop being identical; launder = the 15
  functions whose match depends on a `#13` launder (32 per-unit entries, em10FindCk is in 16 REL units),
  i.e. expected casualties of a real fix):

  | configuration (what is not allocated / changed) | regressions all / launder / other | newly identical | plain test set (15 confirmed) |
  |---|---|---|---|
  | every REG_EQUIV pseudo (const, MEM, frame) never gets a hard reg | 15205 / 32 / 15173 | 9 (cam_extra dtor rodata noise) | all worse (em21DmCk 4->123) |
  | REG_EQUIV constants, single-block | 13279 / 32 / 13247 | 0 | all worse |
  | REG_EQUIV constants, multi-block | 8409 / 30 / 8379 | 0 | all worse |
  | REG_EQUIV constants with 2 refs, single-block | 12717 / 32 / 12685 | 0 | all worse |
  | multi-set REG_EQUIV pseudos (the bell `r`) | 5488 / 25 / 5463 | 0 | bell cases WORSE (em10 20->105): reload rematerialises a pool MEM as `lis; addi; lfs 0(r)`, never the target's `lis; lfs @l` |
  | 2-ref constant whose only use is a plain store | 5882 / 8 / 5874 | 1 (r224 R224Main 9->0) | em21DmCk block 8 right, block 11 wrong (42) |
  | constants skipped by local-alloc only (global allocates) | 10170 / 32 / 10138 | 2 (event DelEvt, sce_at pos_jump) | worse |
  | 2-ref constants skipped by local-alloc only | 9353 / 32 / 9321 | 1 (DelEvt) | worse |
  | GCC 3.0 update_equiv_regs: substitute the equivalence within one block too | 13 / 0 / 13 | 0 | unchanged |
  | + GCC 3.0 move-init-before-use within one block | 9462 / 31 / 9431 | 1 | worse |
  | movqi/movhi/movsi insns accept `(set (mem) (const))` (cse/combine/update_equiv fold the constant into the store, reload rematerialises it) | 7987 / 12 / 7975 | 3 (Espgen42 AddWaterPower 26->0, r222 R222Main 7->0, r224 R224Main 9->0) | em21DmCk block 8 / em27DmCk right, every other block with several constants wrong |
  | + the 3.0 substitution | 7989 / 12 / 7977 | 3 | same |
  | local-alloc 3-qty hand sort replaced by a real sort | 3105 / 3 / 3102 | 0 | unchanged (the buggy `qty_compare (0, 1)` on qty numbers is in the original too; GCC 3.0.4 still has it) |

  Nothing comes near zero non-launder regressions; **the original allocates REG_EQUIV constant pseudos
  exactly like stock 2.95 in >12000 identical functions**, so "#13 = REG_EQUIV constants are never
  allocated" is false as a compiler rule. Recommended action: none on the compiler; keep the per-site
  recipes (bell override, `int zero` single-use, hard-register/asm launders) and the tags.
- **What the residues really are** (sharpened #13, read off the lreg dumps of em21DmCk / MercSysInitRoom /
  em27DmCk in d/base@plain and the target asm):
  1. Bell family (em10/em21/em28/em3c/em2d FindCk...): NOT an allocation difference. In the original the
     pool load sits once in the join block as an ordinary block-local pseudo (that is what the override
     form reproduces byte for byte, registers included); a pseudo really left to reload would come out as
     `lis rX,LC@ha; addi rX,rX,LC@l; lfs fY,0(rX)` (find_reloads_address on the raw pool MEM), a shape the
     target never has. Why the original's RTL had one load in the join block (source form vs. an arm merge
     before sched1 -- 2.95 cross-jumps only in jump2) is still open; the recipe is exact.
  2. Constant-store shape in a block of two local qtys (em21DmCk block 8: `lbz mode; li 30; stb; cmpwi
     mode` -> target `lbz r0; li r9,30; stb r9`, ours `lbz r9; li r0,30`; MercSysInitRoom `lwz r9,pG;
     li r11,0; stw r11; lhz r0` with r0 free): under stock local-alloc the 2-ref constant (life 2) outranks
     the load (life 6) and takes r0; the target's assignment is what "constant not a local qty" gives
     (global pass 0 / reload round-robin pick r9, r11). But in the SAME function em21DmCk block 11 has
     three identical-shape 2-ref QI constants (`4`, `60` -> the same `em->dmType`, `7`) that ARE allocated
     and hoisted exactly like ours (r8/r7/r6 by the fake-lifetime walk); em27DmCk's `lbz wep; li r0,0;
     stb r0` gives the constant r0 in the target because `wep` is a global pseudo there. So the property
     that excludes the constant in block 8 is not its shape, refs, mode, block-locality, equivalence
     kind, base register or adjacency (all identical to the allocated ones) -- it is a tie decided
     differently in the original, cause unknown; every candidate rule breaks thousands of functions.
  3. Dying-store / issue-order shapes (SetRock, em2b R0_Init, em39 ctor, cam_qfps init, em32 R0_Init):
     sched1 store order with constants that do not die at their last store; consistent with (2) but not
     separable from source form (the asm launders stay).
  4. The four sweep-7 classifications (drawPoint, EspDrawLaserLine, em_set, espgen10) rest on the same
     "would be a reload register" reading; drawPoint's diff is only which of two `esp` stack reloads gets
     r11 (reload's round-robin state before the block), not the zero.
  Recipes that still work and why: `int zero = 0` used once in another block = update_equiv_regs's stock
  multi-block move (the pseudo becomes a 2-insn qty, r0); the hard-register `register T x asm("rN")`
  forms remove the qty; the bell override changes the RTL shape, not the allocation.

### COMPILER-DIFF #12 sweep (r218 + Tools/t_mv Matching; r108 openCover, em22 R1_Threat, t_event RunStop, r10b Evt_R10BS00 0 words; 2026-09-10)

- Harness /tmp/cd12 (rooms_c2 copies with the paths rewritten; `tryv.py`/`vapply.py` also accept single-unit modules
  `em22/em22`; `fsect.sh DUMP FUNC` prints one function's section of a -dX dump; `flag12.py`/`flag12b.py SWEEP` list the
  target-only fresh `lis`/`li 0`/pool `lfs` lines of every non-identical function, optionally only those within a few
  lines after a loop back-edge; `ctx.sh MOD/UNIT SYM` = sbs diff lines with context — remember zsh does not word-split
  `$f`, use a bash script for `MOD/UNIT SYM` pairs).
- **#12 is one mechanism with four source-visible forms.** Our cse1 (and cse2) carries its hash table into a block the
  original's cse entered with an empty table. Confirm in the `-ds` dump: the block's expression already reads the pseudo of an
  EARLIER block (`(mem (lo_sum (reg N) sym))` with N set before a branch; a store whose source is the `andi.`/`zero`
  pseudo instead of a fresh `(const_int 0)`), and `-dG` shows no PRE for it (gcse only ever *copies* a reaching reg,
  cse is what *substitutes*). Forms and recipes, all confirmed this pass:
  - **(a) loop-exit / AROUND form** — a `do { ..; if (c) break; SceSleep(1); } while (1)` (or any poll loop) whose exit
    jump lands right after `b top; LOOP_END`: `cse_end_of_basic_block`'s backward scan stops at the LOOP_END note,
    `skip_blocks` treats the exit branch as "around a block" and the exit block inherits `high(work)`, hoisted pool
    constants and jump equivalences. ZERO-CODE RECIPE: a dead `do { } while (0);` as the FIRST statement after the
    loop (its LOOP_BEG/LOOP_END notes end the path; the r117 lever). r218 checkClawManDead/_end/appearClawMan: 14+8 ->
    0 with four of them, and the earlier `extern R218WorkPtr r218_work_v asm("r218_work")` aliases became unnecessary
    (removed); r108 openCover 19 -> 0 (`lis coverL/coverR@ha` + the 220.0 reload) with one, unit 10 -> 11/16. Use it
    before any alias: it also fixes the pool-constant reload that no alias can reach (cse folds every pool load to its
    CONST_DOUBLE). Fallback when the do-while is refused by the shape: a `.rodata` object emitted by a top-level asm right
    before the function (`asm(".section \".rodata\"\n\t.align 2\nr218_k2500:\n\t.long 0x451c4000\n\t.section
    \".text\"")` + `extern const f32 r218_k2500;` reads everywhere + `extern const f32 r218_k2500_v asm("r218_k2500")` for
    the exit statement) — same word, same position as the pool entry (the pool is emitted right before the function, so
    every constant of that function's pool must become an object, in target order, for the .rodata to stay equal), local
    symbol, `mem/u` so loop.c still hoists it, fold-proof because `fold_rtx` only folds CONSTANT_POOL_ADDRESS_P MEMs; but
    the hoisted pseudo then has no `REG_EQUIV` (update_equiv_regs makes MEM equivalences only for single-block pseudos;
    the CONST_DOUBLE REG_EQUAL of a real pool load is what doubles the live length in global-alloc), so FPR naming can
    drift (r108: 220 took f30 over x0/x1). Both r218 functions matched with it too; the do-while is preferred.
  - **(b) fallthrough-arm form** — the arm entered by falling through a conditional jump (then arm of `bne`, the
    `if (!(x & bit))` body) stores a fresh `li rX,0` in the original while ours stores a register cse knows to be 0:
    the `andi./andis.` result (record_jump_equiv on the not-taken edge) or a `zero` variable set before the branch. Our
    cse1 re-walks the path with the last branch NOT_TAKEN and the entry block's table (cse.c
    `cse_end_of_basic_block`, "If the last branch was previously TAKEN, mark it NOT_TAKEN"); the original's arm started
    with an empty table. Plain launders fail (`asm("" : "+r"(c))` on `u8 c = 0` becomes `mr rX,r29`: cse substitutes the
    known register into the asm INPUT), `int zero = 0` locals are folded the same way, dead do-while does not help (no
    loop end on this path). Proof that it is cse: cse.c flushes its table at a volatile ASM_OPERANDS insn, but only when the
    PATTERN is the bare `(asm_operands)` — `asm volatile("")` is an ASM_INPUT, `"=r"` outputs wrap it in a SET, a "memory"
    clobber in a PARALLEL, none flush (7/4/7 words); `asm volatile("" : : "r"(pMv))` at the arm top flushes and the arm
    gets its fresh `li r10,0`, but the flush also drops `high(pMv)` (a reload `lis r8; lwz` in the arm, 6 words) — too
    broad. TAGGED RECIPE: let the asm produce the constant,
    `int c; asm("li %0,0" : "=r"(c)); p->cursor = c;` (`// COMPILER-DIFF: candidate #12 (fallthrough-arm form)`), so cse
    never sees a `(const_int 0)` source; a non-volatile asm with a register output is an ordinary insn for sched (no
    barrier) and local-alloc names it like the original's `li`. Tools/t_mv mvInit 7 -> 0 (module Matching), t_event
    RunStop 21 -> 0 (`sth stopWait` zero, module 56 -> 57/69), r11d appearLittleSister 17 -> 8 (the callee-saved `li r31,0`
    feeding two EstSet stack zeros; residue = global-alloc order zero/work r31/r30 and the stack-store slots).
  - **(c) chain form (`lis` glued to its use)** — a `high(sym)` set in an early block is substituted by cse1 into a block
    reached through a chain of else-if tests (single-use labels, path length < PATHLENGTH), gcse then PREs it and
    update_equiv_regs drags the single-use copy next to the load AFTER sched1, so sched2 cannot lift it (anti-dependence
    on the reused r9) and the block's temporaries get other names; the original's block kept its own `lis` pseudo through
    sched1 (issued at slot 4, r7). Recipe: a distinct SYMBOL_REF for that block's read (`extern cPlayer* pPL_v asm("pPL")`,
    the em2a/em21 TrapCamMove lever) or the struct view (`pPLS->pos` = `((PlayerPtr*) &pPL)->p`) — in em22 R1_Threat the
    struct view alone gave 0 once the control flow was right: the source had `if (fabsf(Muku) < 1.047) { if (timer)
    timer--; else RS; } else if (routeAngAbs > 1.57) ..` but the target's `timer--` arm jumps INTO the else-if chain
    (`b .L_27C0`), i.e. `if (..) { if (timer) timer--; else { RS; break; } } if (routeAngAbs > 1.57) ..` — a `b` to
    the wrong label in a 1-word diff is a source-logic bug, not a compiler difference. em22 is 71/72 (R1_Jump: `fl`
    prefers f1 by copy preference, target f12 with `fmr f12,f1` and the compare on the copy; ternary `fl = (t == K) ?
    pos.y : t` reproduces the copy but compares `t`; hard-reg/keep-alive forms documented earlier — still OPEN).
  - **(d) thread_jumps form** (r104 execEvent00, pass 2): `u32 f = pG->flags; if (f & bit) skip = 1; if (!(pG->flags &
    bit))` — the user variable on one side keeps the second compare's label through cse1.
- Not #12 although flagged by the fresh-`lis`/`li 0` heuristic (checked in the sbs diff, leave them): r103/r106
  openShelf_main (two pool `lfs` vs `lwz pParts` issue order, sched tie), r113 execHide / r11d execHide_main (arg-`li`
  family), r11c closeGate / r103 / r105 execOpenCover (#7/#9 peeled loop), r10b chkWater (loop-invariant `lis` hoist
  order), r10b Evt_R10BS00 (18 -> 0: the r214 `IdBinocularCutinI(bino, 0)` int-argument alias, #4 family; unit 11 ->
  12/18), r222 BoxMove (26 -> 13 with `f32 lim; lim = 2.12f` assigned in BOTH predecessors of the goto-loop test —
  the target compares/stores that register; the rest is the target's gcse hoisting `high(2.12)` above the `if (opened)`
  for the then arm and the else arm, which keeps our else arm from being cross-jumped into the wait body: #3 family),
  t_movie/t_se_at ToolSeAt (`addi r7,sym+80` vs a separate label), t_esp/db_widget AddPrimitive (#6 return-0 tail),
  t_camera_data tcDataImport (loop-hoisted `high` r23 vs the target's in-loop `lis`, equiv/loop family), em35
  R1_Critical (#5), r223 reva_common_move (#1 prologue order), r224 R224Main (#13 stack zero), tvib_R0_VibLoopSet (the
  reverse direction: the target KEEPS the PRE'd high, ours re-materialises).
- r108 str_check / r203 StreamCheck untouched (#3 double EmMgr chain); r203/r20d/r226 belong to other agents.

### Dead-test lever sweep (pl0f R1_Drop 42 -> 0, pl0f 94 -> 95/104; r225 SceElevator_r225 301 -> 285; r10c/r120/r221/r22c/r204/r209 analysed; 2026-09-10)

- Harness /tmp/deadtest (rooms_c4 copies with the paths rewritten; `tryv.py`/`vapply.py` also take single-unit
  modules such as `pl0f/pl0f`; `fn.sh DUMP FUNC`; the SN gcse.c/lcm.c/cse.c/local-alloc.c source under `sngcc/`).
- **Dead-test lever, rules of record** (a dead `if (X) local = K;` = the LostHead/setHand mechanism, generalised):
  - Lifetimes: the store goes in flow (after loop.c, before combine/sched1/regalloc), the compare and its operand
    load in jump2's `delete_computation` (after sched2/reload). So the test's insns COUNT for loop.c's
    `insn_count` (both passes), gcse (CFG edges; its load/compare are hash-table occurrences), cse1's path
    structure (its labels), global-alloc (a ref of the compared pseudo, live range extended to the compare) and
    sched1's block boundaries; they never count for the final bytes.
  - The store target must be a pseudo with OTHER refs in the function (a never-read variable is trivially dead:
    deleted before cse1 and the empty `if` folded -> no effect at all, R120Event `done = 1`), and dead by
    liveness (redefined before every read: a block-local copy at the loop top `f32 s = spd; ... s;` re-set at the
    body end, a body-local pointer `u = 0;` before its real `u = f();`, `paras = 0` before the case-1 body).
    Memory targets (Vec/array locals) are never deleted.
  - Side effects that ruin the shape: (a) the test's operand creates a new occurrence of its `high`/address
    expression -> PRE and a hoisted pointer (r22c `IdSys.active`: `&IdSys` becomes a callee-saved pseudo and the
    loop's `addi r3,rH,IdSys@l` a `mr`; r10c `r10c_work.p->cnt`: high(r10c_work) PRE'd to the top) -- test a
    symbol/field the function does not reference otherwise, or a local; (b) any computed temp in the test
    (`eff2 * 100 + 5`) is a single occurrence our block LCM hoists across the following loop -> a ghost
    callee-saved register (r221: +r14, frame +8); (c) a test inside a straight-line prologue splits sched1's
    block, so the `lis`es hoisted to the function top lose their slots (r204 EventChandelier 94 -> 120..240,
    r10c 95 -> 107..174); (d) a dead `if/else if/else` chain leaves the branch skeleton (the skip jumps do not
    become jumps to the next insn, r10c dt2: 75 with `lbz; cmpwi; beq` residue) -- one test, one store,
    falling into the next statement.
  - Where it works: **inside a loop body to raise loop.c's pass-2 `insn_count`** (pl0f R1_Drop 42 -> 0: the
    VECNormalize string `lis` needed insn_count > 71 = threshold*savings*life; the dead `if (w->timer == 0)
    s = 0.0f;` at the body end adds 4). Check the arithmetic in the -dL dump first: `Loop from .. : N real
    insns` (pass 2 is the second listing) and each movable's `savings`/`life`; the rule is
    `threshold(71 with a call) * savings * life >= insn_count`, threshold -= 3 after every moved movable. A dead
    test gives +3..+5: r225 operateCrank (262, the 2^52 `lfd` needs >= 285) and r10c hako_down (260; 0.0 moves at
    284, -100.0 at 272 after the first move) are out of reach; their original loops had 13-25 more real insns.
  - Register-priority use (r221 throwBonbe eff2 vs the pG high): a dead read of eff2 after its last use adds the
    7th ref and extends the range to the compare; `if (eff2 == 0) atNo = 0;` after the last call gives 290
    (window [293, 312]: eff2 then ranks above mot0/mot1, 23 words), the `* 100 + 5` form 296 (20 -> 12 words)
    but with the ghost register above. Not applied.
- **gcse basic blocks span calls** in these dumps (bb 0 of SetEmHitAtari holds 20 calls up to the first
  `bne`): "PRE/HOIST: end of bb 0" inserts before that jump and sched1 then floats the free `lis` to the
  function top (callee-saved). `insert_insn_end_bb` puts an insertion before the first parameter load only when
  the block really ends in a CALL_INSN.
- **cse2 re-materialises PRE'd `high` copies** (r10c SetEmHitAtari `lis r11` at P, R120Event `lis r9` for the
  x4F8E test, R213Init): gcse turns the redundant occurrence into `(set P (reg R))` with the old
  `REG_EQUAL (high sym)` note; cse2's `fold_rtx` returns R's known `(high sym)` (HIGH is CONSTANT_P), SN's
  `CONST_COSTS` gives HIGH cost 0 = REG, and `cse_insn` prefers src_folded on ties -> `lis` again. The target
  uses R directly (one `lis r31`/`lis r27` at the top, no second `lis`): the #3 family seen from cse2's side. A
  dead test only changes WHICH blocks are redundant, never this fold, so it cannot produce the target's
  one-`lis` shape (r10c `dtx_*`, r120 `else_*` variants: 41-107 words).
- **cse's AROUND path needs no label between the jump and its target** (`cse_end_of_basic_block`:
  `no_labels_between_p (p, q)` for the skip_blocks case; the TAKEN case needs a BARRIER before the label). An
  inner `if` inside an outer `if` body therefore makes the code after the outer `if` a fresh ebb (R120Event's
  tail: high(pG) recomputed -> PRE at bb 0's end). A dead `else { x = K; }` on the outer `if` gives the tail the
  body's table instead (the arm ends in `b tail` = BARRIER), but then our cse2 fold above re-materialises the
  highs per use (43-47 words).
- r10c SetEmHitAtari (95, unchanged): the six pre-block loads come from `spdA = 0.01f; spdB = 0.05f; spdC =
  0.06f;` written BEFORE the `if` with the then-arm's constants entered into the pool first by a dead
  `{ const f32 k750 = 750.0f; ... k500 = 500.0f; }` block after `angC = 0.0f` (.rodata equal, structure equal;
  residue: the 0.0 high hoisted to the top in r27 = a high pseudo that crossed calls in the original's RTL, and
  the spdB/spdC f27/f28 order). A `f32 angA = 0.0f` declaration initialiser gives 59 words (cse merges the
  later assignment, arms use f29). Not applied (same count).
- r225 SceElevator_r225 (301 -> 285, applied): the target's up-loop is `b TOP; SLEEP: SceSleep(1); spd += accel;
  cmpwi cr4,faded,0; TOP: ...` = a goto loop (`goto up_top; up_sleep: ...; up_top: {...; goto up_sleep;}`), no
  loop notes, so none of its highs is hoisted (ours hoisted pG/RoomData/Fade into r18/r19/r24/r30). `while (1)`
  and `do {} while (1)` compile like `for (;;)` here (no jump1 rotation: the body's first `if` is a clamp, not
  a break). The `faded == 0` compare is PRE'd by both (`compare` expression, inserted at the dispatch block and
  the latch); cr4 vs `mfcr r28` is the TexRegist pass-0 rule (a used callee-saved GPR is free in ours because
  the loop's hoisted highs took different registers). Residue 285: SetPosXYZ-style inline blocks (`lfs y; lfs x;
  lfs z; fadds; stfs x,y,z` with one shared slot at 8, the FadeSet colours reusing it), the shake loops'
  `fRand1_1()` evaluated before `pos.x` (y-first), the down-loop -- needs its own pass.
- Not dead-test shapes (analysed, unchanged): R209Main `j+1` / R402MoveDoor02 `&id` / R213Init `(plus fp 24)`
  -- the block LCM's `delayin` is zero-initialised so it never passes a loop header and hoists a SINGLE occurrence
  from the outer latch to the pre-inner-loop block (latein[B] & ~isoout[B] with isoout[B] = isoin[I] = 0); a kill
  of `j` inside the inner loop would stop it but is never dead. r22c highscore (the loop's IdSys high must beat
  `score` (14 refs) in priority: not by refs). r204 EventChandelier: the extra callee-saved register is the
  crot0 high H kept apart from its lo_sum X (`lis r30; addi r25,r30`) = H had a second, deleted use in a
  branch-free prologue (not a test).

### Switch tree model (tools/research/casetree.py, stmt.c + jump.c; validated on 836 switches of the Matching modules, 2026-09-10)

Read tools/research/casetree.py's docstring for the API; `python3 tools/research/casetree.py <cases> --layout A,B,D --target
build/G4BE08/<mod>/asm/<mod>/<unit>.s:<func>[:rN[:.L_start]] [--ours <mod>/<unit>:<func>]` prints the model's final
branch sequence next to the target's/ours and MATCH/DIFF (harness /tmp/casetree2: validate.py parses every switch of a
source function and checks it against the target runs; validate_all.py over all Matching modules).  Rules, all read off
stmt.c/jump.c and confirmed with cc1plus probes:
- **Expansion:** SN's stmt.c never emits a jump table (`if (1)` at the CASE_VALUES_THRESHOLD test); the rest of the
  case-tree code is stock 2.95.3.  Nodes = sorted case values after `group_case_nodes` (adjacent values whose labels
  have the same first real insn merge: two `break`-only arms merge with each other and with `default: break;`; an arm
  with a real body never merges with its neighbour even when the bodies are identical).  Default-grouped and
  break-arm values are real nodes that shape the balance; their compares fold away later (see jump1), so the target
  tree is reproduced by LISTING them (`case 5: case 6: .. default:` / `case 0x17: case 0x2A: break;`), searched with
  casetree.search.
- **Balance (plain mode):** root = the node where a countdown from `(n + ranges + 1) / 2` (2 per range, 1 per single)
  reaches <= 0; exactly 3 nodes -> middle; 1-2 nodes -> linear right chain.  A list of >= 4 nodes never makes its
  FIRST node the root.
- **Cost mode** (`use_cost_table`): on when the index is not an enum (the un-promoted type is tested; a u8 member is
  int, an `enum` switch is OFF) and every case value is in -1..127 with no control char other than \0 \b \t \n \v \f
  (0x1..0x7, 0xE..0x1F, 0x7F disable it -> every damage switch is plain).  Then the split bisects the cost_table
  weights (alnum 16, punct/space 8, \0 \t 4, \n 2, \b \v \f 1, -1 -> 0), and a list whose first node reaches half the
  cost is left LOPSIDED: root = first node, right chain LINEAR (`case 'A'..'D': / 'a' / 'x'` -> `cmpwi 65 blt; cmpwi 68
  ble; cmpwi 97 beq; cmpwi 120 beq`).  A game switch is in cost mode when all its values are 0, 8..0xC or 0x20..0x7E.
- **Index type:** u8/u16 members and enums are int (`cmpwi`, INT bounds).  `s8`/`s16`/`char` (char is signed) keep
  their type: a leaf whose high is 127 (low -128) gets no bound test.  `(u32)` index: ordered compares `cmplwi`, EQ
  compares still `cmpwi` (SELECT_CC_MODE), the two are not cse-merged (`cmpwi 5; beq; cmplwi 5; ble`), and a leaf with
  low 0 has a low bound (no `cmplwi 0; blt`).
- **Bound pruning** (`node_has_low/high_bound`): a leaf whose low-1 / high+1 is a parent's high / low emits no test
  on that side; both sides bounded -> plain `b label` (`[D]` between `[0-C]` and `[E-11]` in em31DmCk).  A right-only
  range node emits `[cmpwi low; blt D]; cmpwi high; ble label` (the LT only without a low bound), a left-only one
  `[cmpwi high; bgt D]; cmpwi low; bge label`, a both-children node `cmpwi high; bgt T (or the right child's label if
  that child is bounded); cmpwi low; bge label; <left>; b D; T: <right>`; a single node `cmpwi v; beq label` then the
  same dispatch on its children (`bgt/blt` straight to a bounded child; a single leaf child is handled as `cmpwi
  child; beq` with no range test).
- **jump1** (before flow; a deleted conditional jump loses its compare): `beq D; b D` (a default/break leaf's EQ
  followed by the default jump) -> both gone; `b L; L:` -> gone; `bgt L1; b L2; L1:` -> `ble L2`; jumps to
  `default: break;` / `case X: break;` labels thread to the switch exit; from the second round on, the range swap
  `if (foo) bar; else break;` (`bcc L1; R1; b L2; L1: R2; b X; L2:`, L1 used once) -> `b!cc L1; R2; b X; L1: R1; b
  L2`: fires when the left subtree's final `b A` targets the arm laid out right after the tree (A written FIRST),
  giving the "right child before left" order (em29DmCk 0x29, em31DmCk high half) and also on a case label whose
  arm follows the tree (`beq A; ..; b B; A: body; b END; B:` -> `bne A'; body; b END; A': ..`).  A `default: goto
  normal;` written FIRST puts the default label right behind the tree: the last node's `bgt default; b big` inverts
  to `ble big` and falls into `default: b normal` (em36BloodSet, `--goto D=N`).
- **jump2** (after reload, with sched2 on): `delete_computation` deletes only the jump, so a conditional jump made
  redundant by jump2's cross-jumping keeps its compare: `cmpwi 0; b D` (em28DmCk, three identical arms), `lbz dmWep;
  cmpwi 0x21` with no branch (em30DmCk `if ((em->dmWep ^ 0x21) == 0) return em->dmWep;`, em29DmCk).  Cross-jumping
  also merges the last branch of two nodes when the arm body is not adjacent (`cmpwi 1; b L; T: cmpwi 7; L: beq A`,
  em24DmCk's hitCheck switch whose arm ends in `return`): model it with 'EXIT' in the layout before the far arm.
  Identical arms: `merge={'A1': 'A0'}` (survivor normally the LAST identical arm; `'A3+'` when the copies jump past a
  private first insn of the survivor, e.g. its own dead `fcmpu`).
- **em31DmCk (11 words, compiler-side):** the target's `> 0x17` half is `[18-28] -> [2b-2c]{[29-2a],[2d]}` -- a
  right-only range root with a BALANCED right subtree.  balance_case_nodes never picks the first node of a >= 4 list
  in plain mode, and cost mode (off here: 0..C, 0xE..0x11, 0x18..0x1F are control chars) leaves such a root with a
  LINEAR chain.  The left half additionally needs `[12-13]` to have a bounded right child (`cmpwi 0x13; ble X; b
  EXIT` = an explicit `case 0x14..0x16: default:`), which makes the left list cost 9 and would move the root off 0x17
  unless the right list costs 9-10 -- no (n, r) rule and no ctype cost table satisfies both halves; brute force over
  explicit default/break labels for 0x14-0x16, 0x18-0x28 (any split), 0x2B-0x2C, 0x2E-0x30 x 9 layouts x 3 empty sets
  (15876 configurations, /tmp/casetree2/em31s2.py) finds no match.  Nested `if`/switch forms cannot bound `[29-2a]`
  from below (an inner switch's `[29-2a]` has no parent with high 0x28: `cmpwi 0x29; blt` appears).  Left as a
  compiler-side difference; do not retry source forms.
- **em29DmCk hp>0 arm (`lbz dmWep; cmpwi 0x21` dead, then the kind switch):** the mechanism is `if (em->dmWep == 0x21)
  em29DmRoutineSet(em, kind); else em29DmRoutineSet(em, kind);` -- jump2 cross-jumps the THEN copy into the ELSE copy,
  `bne ELSE; b ELSE` loses its branch and the compare survives.  With our jump.c the THEN copy only collapses
  partially: its kind-0 body's `b END` is first tried against the code before END (the ELSE kind-2 body's `stb r0,fe`,
  a 1-insn match, `find_cross_jump(insn, END, 1)`), so it becomes `b NEW` and never reaches the chain lookup that would
  match the ELSE kind-0 body whole (+14 insns, 74 diff lines vs 50 without the if).  The original merged the whole
  body (longest/other-candidate match first) -- COMPILER-DIFF 6 (cross-jump policy).  if/switch, `!=`, default-first
  and `return` forms all give the same partial merge; not reproducible from source.

### COMPILER-DIFF #6 resolved: the jump2 cross-jump policy is stock; the survivor is decided by the RTL at jump2 entry (db_widget AddPrimitive 92 -> 100%, DB_NUMERIC2::OnCalcMsg 94 -> 100%; tools/research/xjump.py; 2026-09-10)

- Harness /tmp/cd6/h (copy of the /tmp/sched5 h.py whole-tree harness: `python3 h.py build CFG` compiles all 755 ProDG units
  with cc/CFG/cc1plus in 11 s and compares every function with the split object, `h.py cmp base CFG` lists
  regressions/fixes; `mk.sh NAME "-DDEFS"` builds a cc1plus from gcc/ = the repo's tools/sn-gcc source with `-D` hooks in
  jump.c; `CJ_LOG=file` makes the hooked jump.c log every cross-jump it performs: function, candidate kind fall/chain/
  return/cond, minimum, matched insns, what precedes the scanned tail). Base: 18695/19929 functions identical (the 5 wep*
  errors are another agent's transient PSet redefinition). Every policy variant regresses matched functions and fixes NONE:
    fall-through candidate minimum 2 (`find_cross_jump (insn, JUMP_LABEL, 2)`)   2147 regressions / 0 fixes
    no fall-through candidate at all                                              3767 / 0
    no `--minimum` when the backward scan hits a CODE_LABEL                         428 / 0
    jump_chain walked oldest-first instead of latest-first                          467 / 0
    no jump-around-jump `--minimum`                                                 516 / 0
    no USE-before-jump move (jump1's `use r3; b END` -> `b END'; END': use r3`)   1732 / 0
    USE move followed by `next = insn` (immediate re-examination)                    840 / 0
    range swap refused when range2's jump targets label2                            833 / 0
    range swap allowed in the first round / no range swap                      955 / 0, 1012 / 0
  So the original's jump.c is ours on every decision that picks a cross-jump survivor; the residues come from the RTL
  that reaches jump2 (expansion/jump1 layout, sched2 order, insn identity), i.e. they have source levers. tools/research/xjump.py
  models the jump2 loop on a hand-written insn list (fall-through / chain candidates, minimum rules, label creation,
  re-examination, jump-over-jump inversion, threading) and prints the merges and the final layout; validated against
  `-dR`/`-dJ` dumps of AddPrimitive (both forms), OnCalcMsg, isKamae, item use.
- **The rule (jump.c `jump_optimize_1`, cross_jump pass = jump2, after sched2).** Jumps are scanned in insn order,
  repeatedly until nothing changes; a successful cross-jump deletes the SCANNED jump's tail and redirects that jump to a
  label put before the CANDIDATE's tail (an existing label is reused), so the candidate is the survivor. A copy survives
  iff its own scan fails every time it is examined. For a simplejump `b L`:
  (1) candidate 1 = the code falling into L, `find_cross_jump (insn, L, minimum = 1)`: ONE matching insn suffices, but the
      first compared pair is the last insn before L vs the last insn before the jump -- a mismatch there ends it. The
      fall-through arm's last insn is what protects it: flow.c's `(use (const_int 0))` nop after a CALL_INSN that ends a
      basic block (count_basic_blocks inserts it whenever a call is followed by a label), the `(use r3)` that a
      value-select return leaves (`int r; if (c) {..; r = 1;} else r = 0; return r;` -- reload turns the dead `mr r3,r3`
      into a USE, and it sits between the else arm's `li r3,0` and the return label), a compare/branch, or simply a
      different insn (`li r3,0` before END vs the `li r3,1` tails).
  (2) candidates 2..n = the other simplejumps to L in jump_chain order = the LATEST in insn order first (mark_all_labels
      prepends; a jump redirected by an earlier merge is moved to the new label's chain and no longer competes),
      `minimum = 2`: two matching insns, or one insn plus a bonus. Bonuses: a CODE_LABEL directly before the scanned
      tail (`--minimum`, then stop -- this includes the label that an EARLIER merge created before a survivor's tail, so a
      survivor that collects copies can itself merge into a later/earlier copy with one insn); or, at the first
      mismatching pair, i1 (scanned side) is a conditional jump whose label is right after the scanned jump
      (`bcc skip; li r3,K; b END; skip:`) -- but `GET_CODE (i1) != GET_CODE (i2)` breaks BEFORE that test, so the bonus
      only applies when the candidate's preceding insn is also a JUMP_INSN (`bcc skip; li; b END` vs `stb; li; b END`:
      no bonus, no merge; vs `bne X; li; b END`: merge). USE/CLOBBER insns must match but do not count.
  (3) RETURN insns (leaf functions, `b END` -> `blr` conversion in the same pass) cross-jump among themselves from
      jump_chain[0], latest first, minimum 2; conditional jumps only when the target follows an opposing jump back.
  Consequences: with N identical `li r3,K; b END` copies and no fall-through match, copy 1 survives iff its scan fails
  (no label before its `li`, its preceding insn not a condjump-around whose counterpart is a jump); copies 2..N-1 with a
  label/bonus merge into copy N; copy N -- now preceded by the label the merges created -- merges into copy 1 with one
  insn: copy 1 is the survivor and everything else reads `b/bcc L1` (pl_class isKamae, em2bAtkRtnCk's Debug copy DB50,
  item use's E3F4). If copy 1 has a label before its `li` (an `a || b` join), copy 1 merges into copy N first and copy N
  survives -- the "ours keeps the last copy" observation. Whether a `return K` copy has a 1-insn `li r3,K` tail at all is
  sched2's decision (em2bAtkRtnCk: the target's `li r3,1` sits among the RS stores in 6 blocks and last in 9).
- **Recipes (all verified with our cc1plus):**
  - Keep an early `X: li r3,K; b END` copy separate from a final `return K` that falls into END: write the final
    if/else as a value select `int ret; if (c) {..; ret = 1;} else {ret = 0;} return ret;` (db_widget AddPrimitive 100%:
    `beq DT; cmpwi; bne L2; DT: li r3,0; b END; ..; bne Lz; THEN; li r3,1; ..; b END; Lz: li r3,0; END:` -- the same source
    with two `return` statements gives the jump1 range swap (THEN last) and the DT copy merged through the chain).
    `if (c) {..; return 1;} return 0;` / `if (!c) return 0; ..; return 1;` / `goto ng` / for-break / do-while forms all
    merge the copy (12 forms tried, /tmp/cd6/p3.cpp).
  - Keep the fall-through arm of a switch/if-chain out of the merges while the other identical arms merge into each other
    (DB_NUMERIC2::OnCalcMsg 100%: MIN's `lfs f31; mr r3; bl SetNumFloat` tail jumps into MAX's, DEFAULT keeps its own):
    end every such arm in the CALL -- write the arm's other statements BEFORE the call (`v = 0.0f; SetNumFloat(min);`
    instead of `SetNumFloat(min); v = 0.0f;`), so the fall-through arm ends `bl; (use 0)` and cannot be candidate 1; the
    arms then merge in chain order (latest first) -- MIN into MAX because MAX is the last `b JOIN`. With the statement
    after the call (`lis; lfs f31` behind the `bl`, r9 is call-clobbered so sched2 cannot hoist it) all three arms merge
    into DEFAULT's copy. Same family as the emBarred "flow-nop rule" above.
  - Make an early copy the survivor of later `return K` copies (isKamae, IsExePacket): the early copy must have no label
    before its `li` and must not be `bcc skip; li; b END` when some later copy is `bcc X; li; b END` -- e.g. `if (a || b)
    goto ng;` with `ng: return 0;` at the very end, or a store before the `li` (`if (c) { p->x = 1; return 1; }`).
  - A `return 0` copy that must NOT be merged into an identical later one: give it a different tail (a store or a
    `(use)`: `asm volatile("")` between the compare and the return -- tagged COMPILER-DIFF earlier, now just a layout
    lever) or make its preceding insn a non-jump.
  - Identical multi-insn arms that the target keeps apart although both are `b JOIN` with 2+ identical insns
    (em39ArmControl 0xC/0xE `add; bl MotionSetCore; lbz; li r0,1; stb; b`, em2bAtkRtnCk's r11-zero RS(1,0x11) copies A/D/
    F/H) cannot be produced by this policy from identical RTL: in the original the insns differed in a way the asm does
    not show (mode/form of the constant or zero register: a QImode `li r0,1` for `x8BC = 1` vs an SImode one, a `(reg)` vs
    `(subreg)` zero substituted by cse's AROUND-path knowledge). Check ours with `-dR`: if the two arms' RTL is
    identical in ours, the lever is upstream (give one arm a distinct constant/register form: the "register-distinct
    duplicate bodies survive jump2" lever), not jump2.
- **Per-residue recommendations for the owners** (read-only analysis, target vs our -dR dump):
  - em2b em2bAtkRtnCk (195): the Debug survivor follows from the rule: its `beq DB58; li r3,1; b END` tail finds no
    chain candidate whose insn before `li r3,1` is a JUMP_INSN (all later copies precede `li r3,1` with a `stb`), so
    it survives, and the 9 copies whose sched2 order ends `stb rX,0xfe; li r3,1` merge into it one by one (1 insn each:
    they are scanned later, the chain leads them to each other first and finally, via the created label, into DB50).
    Two things ours must reproduce first: (a) sched2's `li r3,1` position -- ours leaves `li r3,1` LAST in every
    return-1 block, the target has it among the stores in the 6 blocks that still `b END` (DD08 `stb; stb; li r3,1; stb;
    stb`, DD20, DE0C, E0B0, E290, E2CC) -- those are the blocks with a fresh `li rX,0` zero (`li r0,0`/`li r9,0`) or an
    extra `stw` (timer stores), so the zero/constant pseudos' lifetimes decide the tie; (b) the un-merged identical
    r11-group copies (see above: RTL identity). The `goto` into the Debug `if` (124 words) is the wrong lever: the
    survivor is free once (a) holds.
  - em39 em39ArmControl (56): the 0xC/0xE tails are identical in our RTL and merge (5 insns, chain); the target keeps them
    -> give the two arms different RTL for the `x8BC = 1` store (`li r0,1` QI vs SI: e.g. one arm `em->x8BC = 1`, the other
    through an `int` variable / `k` value, or a different zero/one pseudo), then jump2 leaves both; the third arm's
    `stb r28` (the SI `1` shared with `flag = 1`) already differs.
  - item get / use / bulletNum (game, DOL owner): use -- the 11 `li r3,N; bl healing; cmpwi; bne EXIT; li r3,0; b END`
    arms are un-merged in the target from `bl` on, their `li r3,0` copies all merged into E3F4 (the ITEM_TYPE `return 0`,
    a `bne E3FC; li r3,0; b END` jump-around copy that survives). tools/research/xjump.py on our RTL reproduces OURS exactly
    (everything collapses: the ITEM_TYPE copy merges into a later `li r3,0` because its jump-around bonus applies against
    a JUMP_INSN counterpart, the healing tails merge 3 insns deep into the last arm). Under the stock policy the target
    needs, at each healing copy's scan, every later healing copy to mismatch within the first two insns (`li r3,0` then
    `bne EXIT`): i.e. the `bne`s did not all target the same label at jump2 time, or the `li r3,0` was not directly
    before the `b` (a `(use)`/store between). Not reproduced; levers to search with the DOL harness: arm order in the
    switch, a per-arm `break` target, an `int` result variable, `asm volatile("")` after the call (tagged, 1 arm). get: separate `li r3,1; b
    end` copies = the "copy 1 survives" rule (no label before its `li`, no jump-around counterpart that is a jump).
    bulletNum: the inner default's `li r3,0; b end` is not merged into the outer default's `li r3,0` fall-through -> the
    outer default's block must not end in `li r3,0` (value-select `n = 0` join with `return n`, i.e. a `(use r3)` before
    END, as in AddPrimitive).
  - t_esp AddSeq (t_* owner): the un-merged `li r0,0xff; stb r0,0x9c(r3); b JOIN` 255 arm is a layout question: in the
    target the IMM arm (`lbz r0,0x9c(r5); b STORE`) sits between the 0 arm and the ELSE arm, and ELSE (`add; STORE: stb;
    addi r8`) falls into JOIN ending in `addi` -- no 1-insn `stb` can match it. Ours lays IMM last (`lbz; addi; stb` falls
    into JOIN ending in the `stb`) and the 255 arm's stb merges into it. Reorder the arms so the `+= delta; no++` arm is
    last (e.g. `if (imm) {...} else if (v > 255) .. else if (v < 0) .. else {...}` with `no++` after the store) --
    `no++` before/after the store and `no = no + 1` do not change ours' block order (3 variants).
  - em29DmCk hp>0 arm, em2d CamouflageMove, em2c DmCk `type = 0`, ss_pzzl quit, event IsExePacket, cam_ctrl roomInit:
    already consistent with the rule (their notes describe the fall-through/label/chain mechanics correctly); the
    em29DmCk "1-insn match first" partial merge is the fall-through candidate 1 (minimum 1) winning over a longer chain
    match -- the policy is the original's, so the original's THEN copy did not see `stb r0,fe` before END: its ELSE kind-2
    body did not fall into END (ended in a jump or a use), which is a layout question for the owner.

### COMPILER-DIFF #1 / #8 research: argument-move order and FPR argument deaths are not a rank_for_schedule difference (whole-tree harness, 30 compiler variants, nothing installed; 2026-09-10)

- Harness /tmp/rank18/h (a symlink: /tmp was a full 32 GB tmpfs, the tree lives in /var/tmp/rank18/h; `TMPDIR` for
  ngccc's scratch dirs is set in h.py). Copy of the /tmp/cd6/h whole-tree harness on a fresh src/ snapshot:
  `python3 h.py build CFG` = all 755 ProDG units with cc/CFG/cc1plus (+ cc/CFG/flags) in 11 s, per-function masked
  compare against the split objects (base 18726/19929 identical); `h.py cmp base CFG` regressions/fixes; `mk.sh NAME
  "-DDEFS"` builds a cc1plus from gcc/ = tools/sn-gcc with `-D` hooks (RK_* in haifa-sched.c, CA_* in calls.c, RM_* in
  regmove.c; the Makefile needed `$(SCHED5)` added to CFLAGS); `mkmd.sh NAME md/X.md` builds with an alternative
  rs6000.md (the insn-*.c files are make intermediates: `rm cc1plus; touch the md` or nothing regenerates);
  `bp.sh CFG` builds the 83 plain-form test units from tree_plain/ (every `// COMPILER-DIFF: #1` alias reverted to the
  real call: atari_init.h `AtariInit` calling `at->init(parts, flags, cnt, x..h)`, SetObaModel, de_Boor_Cox,
  PathGetPosEm, cSatMgr::create, SoftShadowGXDraw, R213ChainAngSet, r226 playerPillarDownCk in the mangled parameter
  order, emrock's #13 `asm volatile` removed); `t18.py CFG...` = the 31-function test table + whole-tree counts;
  `fd.sh CFG UNIT SYM [all]` side-by-side disassembly; `dump.py CFG UNIT [--plain] -dS -fsched-verbose-7` (DUMP_ROOT
  selects a tree); `prolog.py CFG` lists every function whose prologue parameter-copy set matches ours but in another
  order (6 in the whole tree); `prolog2.py` classifies the copied parameters by use.
- **Source facts (SN v1.79 vs stock 2.95.3, /tmp/equiv13/stock).** haifa-sched.c, regmove.c: byte-identical apart from
  the SN header / PROTO removal. calls.c is stock 2.95.2 (only the `HAVE_call_pop` test differs); on rs6000 there is no
  PUSH_ROUNDING, so PUSH_ARGS_REVERSED is undefined: `initialize_argument_information`, `precompute_arguments` and
  `load_register_parameters` all walk the arguments first-to-last (LOAD_ARGS_REVERSED is not defined either), i.e. the
  register moves are emitted in declaration order and that is the LUID order sched1 sees. `precompute_register_parameters`
  copies a value into a pseudo only if it is not a REG, `rtx_cost (value, SET) > 2` and `preserve_subexpressions_p ()`
  (inside a loop; SMALL_REGISTER_CLASSES is 0 on rs6000): CONST_INT and CONST_DOUBLE cost 0 (`CONST_COSTS`), so
  constants are never precomputed; an FP constant argument is `fN = mem(LC)` straight from `emit_move_insn`/
  `force_const_mem`, and a second use of the same constant at the call is folded by cse into the hard-reg copy
  `fM = fN` (class {mem, fN}: the hard reg costs 2, the MEM more). rs6000.h/rs6000.c: `ADJUST_COST` = rs6000_adjust_cost
  (anti/output links cost 0 -> clamped to 1 by insn_cost; a data link into a `jmpreg` insn 4) and `ADJUST_PRIORITY` =
  rs6000_adjust_priority whose body is `#if 0` (a no-op) exist in stock 2.95.3 too; no MD_SCHED_INIT/REORDER/
  VARIABLE_ISSUE hooks, ISSUE_RATE = 2 for ppc750. rs6000.md carries SN-Phil's Gekko timings (store/fpstore
  ready-delay 2 instead of 1, compare 1 instead of 3, fpcompare 1 instead of 5, mtjmpr 3 instead of 4, fp 1 instead
  of 3, dmul 2 instead of 4, a new sdiv 17/17); reverting any one of them regresses 3011-3994 matched functions and all
  of them 7590, fixing none -- the original was built with v1.79's timings.
- **rank_for_schedule (haifa-sched.c 4158) and INSN_REG_WEIGHT as they are.** Order of tests: INSN_PRIORITY (longest
  latency path to the block end; every insn of a block ending in a branch has an anti link to the branch, so the
  minimum is 2 there; the priority of an arg move is 1 + priority(call), of an `lfs` 2 + ...); then, in sched1 only,
  INSN_REG_WEIGHT (smaller first) = +1 per SET or CLOBBER in the pattern (a `(set (mem) ..)` store counts +1 like a
  register set) minus 1 per REG_DEAD or REG_UNUSED note whose operand is a REG (hard registers included, so `mr r31,r4`
  and `fmr f29,f1` parameter copies are 0, `li r4,K` is +1, a store of a dying pseudo 0, a compare setting a CC pseudo
  +1 unless an operand dies); then interblock preferences; then the class relative to the last scheduled insn (data
  dependent with cost > 1 = worst; on rs6000 only loads and the few multi-cycle ops give that, every anti/output link
  is cost 1 = class 3); then the number of dependents; then INSN_LUID. `adjust_priority` (called only for insns released
  by schedule_insn, never for the initially ready ones): its death part is dead code (REG_DEAD notes were removed), the
  `birthing_insn_p` part is live (a single-set pseudo live at the block end gets INSN_PRIORITY := max_priority when it
  becomes ready); sched2 has no weights at all (find_pre_sched_live runs only before reload) and inherits sched1's
  order as its LUIDs. `insn_cost` for a `fmr` is 1 (type fp, SN timing), for `li`/`mr`/`addi` 1, `lfs`/`lwz` 2.
- **Whole-tree table** (regressions = functions identical with the installed compiler that stop being identical /
  newly identical; lists in /tmp/rank18/h/logs/<cfg>.cmp; the test-set columns in `python3 t18.py <cfgs>`):
    RK_NOWEIGHT   weight test skipped                             11138 / 2 (cam_qfps init 12 -> 0, t_snd_vol editDataDraw)
    RK_NODEATH    no -1 for deaths (weight = number of sets)      11187 / 2 (same two)
    RK_DEATH_GPR_ONLY  FP-mode deaths not counted                  3891 / 0
    RK_DEATH_FPR_ONLY  only FP-mode deaths counted                10888 / 1
    RK_NOHARDDEATH     hard-register deaths not counted             897 / 0
    RK_NOFPHARDDEATH   FP hard-register deaths not counted          344 / 2 (emwep setThrow, emshield setFall)
    RK_NREFS2  hard-reg death in a copy into a single-use pseudo
               not counted (REG_N_REFS <= 2)                        288 / 2 (same two; r226 idx 2 -> 4)
    RK_PARMCOPY  hard-reg death in any copy into a pseudo ignored   316 / 0
    RK_NOUNUSED  REG_UNUSED not counted                               4 / 0
    RK_STORE_NOSET  a MEM store does not count +1                  2800 / 0
    RK_DEPS_BEFORE_WEIGHT  dependents compared before weight       2535 / 0
    RK_WEIGHT_AFTER_CLASS  weight compared after the class test      12 / 0
    RK_NOCLASS   last-scheduled-insn class test skipped              13 / 0
    RK_NODEPCOUNT  dependents test skipped                         9852 / 0
    RK_REVLUID   LUID tie reversed                                13558 / 0
    RK_NOBIRTH   birthing priority boost off                       3770 / 2 (merchant sellPrice, t_camera tcNextAdatPtr)
    CA_FPFIRST   FP arg moves emitted before int arg moves          418 / 1 (379 of them not alias sites; fixes em2b
                                                                  Die_Event, r402 6 -> 4, em38 7 -> 4 in the plain set)
    CA_INTFIRST  int arg moves before FP arg moves                  159 / 0
    CA_LOADREV   moves emitted last-to-first                      11835 / 1 (pl0e pl0ePathMove plain 2 -> 0)
    CA_PUSHREV   PUSH_ARGS_REVERSED (evaluation last-to-first)    11876 / 1
    CA_NOPRECOMP precompute copy never                              599 / 0
    CA_PRECOMP_ALL  precompute copy of every non-REG value with
                    rtx_cost > 2 (constants still cost 0)           126 / 0
    RM_NOHARDDEST regmove never substitutes a hard destination      997 / 1 (r20d execRoundSwitch)
    RM_NOHARD     regmove leaves every hard-reg copy alone          998 / 1
    md fp 3 / fp 2 / store 1 / compare 3 / all stock timings   3312 / 3011 / 3912 / 3994 / 7590, 0 fixes
  Flags on the test units (installed compiler): -fno-regmove alone changes nothing at -O2 (toplev runs regmove under
  -fexpensive-optimizations too); -fno-gcse / -fno-strength-reduce / -fno-cse-skip-blocks only make the test functions
  worse. No variant fixes more than 2 functions; the original's haifa, calls.c and regmove are ours.
- **What the target's order says about the original's RTL at sched1 entry (all six prologue cases simulated by hand
  against the .sched dumps).** The only prologues in the whole tree whose parameter-copy order differs from ours are
  emshield `setFall(f32 gravity, Vec* spd)` (target `mr r31,r4; stw pMotion; fmr f29,f1`), emwep `setThrow(Vec*, f32
  grav, EmAtkInfo*)` (`mr r26,r5; addi; fmr f30,f1`), r223 `reva_common_move(cObj*, int axis, int mode, f32 lo, f32 hi)`
  (`mr r31,r3; fmr f28,f1; fmr f29,f2; mr r29,r5`), pl_wep `PlWepAutoTrack(cModel*, int mode, f32 rate)` (`fmr f31,f1`
  before `mr r29,r4`), em_sub `EmYarareContactCk(cEm*, Vec*, Vec* out, f32 r)` (`fmr f1` before `mr r5`) and r226
  `playerPillarDownCk(.., f32 dist, int idx)` (`fmr f31,f1; mr r28,r6`, the alias reorders the parameters). In every one
  of them the stock ranking with the priorities of our dump reproduces the target exactly if ONE copy has weight +1,
  i.e. the incoming hard register did not carry a REG_DEAD note on that copy in the original: f1 in setFall and
  setThrow, r5 (`mode`) in r223, r4 (`mode`) in PlWepAutoTrack, r5 (`out`) in EmYarareContactCk, r6 (`idx`) in r226;
  every other copy of those functions dies as in ours. The affected parameters are used once in a store (gravity,
  grav, idx) or only in compares (mode, `if (out)`), but obj16 `SetObj16`'s `partsNo` (one `stw`) and obj20
  `SetObaModel`'s `type`/`partsNo` copies die normally, so the property is not "single use" (RK_NREFS2 fixes
  setFall/setThrow and breaks 288 others), not "FP hard reg" (RK_NOFPHARDDEATH breaks the `stfs f1` parameter stores and
  SetObaModel's `fmr f31,f1`), not the parameter order (the six cases move FP copies both earlier and later than
  declaration order), not regmove (stock, and our RTL keeps the copies). In ours a register parameter's copy always
  carries the REG_DEAD of the incoming register (nothing in C++ can add a later use of `f1`; `register f32 asm("fr1")`
  keep-alives are deleted, an asm input materialises a second copy), so the #8 prologue family has no source lever:
  keep the r226-style reorder alias (declaration under the mangled name with the parameters in the target's copy
  order) where a module needs it, and leave the two DOL cases (emshield setFall, emwep setThrow, 2 words each).
- **Arg-move ties (#1).** In the plain forms the FP and int arg moves of one call have equal priority (1 + the call's)
  and, whenever the FP move is a hard-reg copy (`fmr f2,f1`, cse's fold of a repeated constant) or its source pseudo
  lives on, equal weight +1 and one dependent, so ours issues them in LUID = declaration order (em2b Die_Event's third
  `SetObaModel(em, 0xD, &v, 1000.0f, 0, 1000.0f)`: sched2 t=3 `li r6,0; fmr f2,f1`, target `fmr f2,f1; li r6,0`; em10
  R0_Init's init: ours `li r4; stb; li r5; stfs; li r6; stfs; fmr f2,f1; stfs; fmr f3,f1; fmr f6,f5`, target `fmr f2,f1;
  stb; fmr f3,f1; stfs; fmr f6,f5; stfs; li r4; stfs; li r5; li r6`; emobj create: target issues `lfs f1,h` before the
  `lwz attr`/`lwz flag` loads of equal priority 5 and LUID before it). Under the stock ranking the target needs either
  LUID(FP move) < LUID(li) -- CA_FPFIRST, which fixes em2b/em38/r402 and breaks 379 sites where the target keeps
  `mr r3,x; mr r4,y; fmr f1` / `mr r3,r30; lfs f4` in declaration order -- or weight 0 for the FP move = its source
  pseudo dying there: `f1 = P; f2 = P` with P the 1000.0 pseudo dying at the third call reproduces all three Die_Event
  calls (at calls 1/2 P lives on, +1, and the target indeed has `li r6,0; fmr f2,f1` there), and separate dying zero
  pseudos for x/y/z plus `f6 = P250` as P250's death reproduce em10. That is the #13 world: constants held in pseudos
  with a REG_EQUIV that reload rematerialises at each use (`lfs f1,LC(r30)` before every call, `fmr f2,f1` after the
  pseudo was tied to f1); ours re-loads the constant per use at expansion (no pseudo, so no death) and a hand-made
  pseudo (`f32 r = 1000.0f;`, with or without `asm("" : "+f"(r))`) is folded back to `f2 = f1` by regmove's
  optimize_reg_copy_1 (`f1 = P` followed by P's death at `f2 = P`) -- and with RM_NOHARDDEST + CA_PRECOMP_ALL the fold is
  gone but the tree loses 1123 matches. So #1 is the same compiler-build difference as #13 (how constant pseudos are
  allocated/rematerialised), not a scheduler policy. Levers that DO work in ours (already used, restated): (a) the
  floats-first asm alias = the LUID lever; (b) an inline wrapper whose float parameters are the arguments
  (atari_init.h `AtariInit`) turns each constant into a pseudo that dies at its arg move (weight 0) -- it reproduces the
  interleave only when the pseudo is not shared with another use (a constant shared with a store, e.g. the 0.0 of
  `pos = 0`, keeps +1 and sorts with the `li`s); (c) an FP argument that is the last use of a variable (`fmr f1,P`, P
  dying) always precedes the `li`s -- passing a value through a variable used once at the call is the source form of a
  dying FP move.
- **Other test-set residues, classified by the variants:** cam_qfps init (12) is fixed only by switching the weight rule
  off (RK_NOWEIGHT/NODEATH); em2b Hook/UpperCut/DashAtk (15/21/6: `li r9,0; stw r9` vs ours `stw r0`) and em32 R0_Init
  (2) do not move under any scheduler variant -- they are the zero-register choice (#13), not an ordering; r11d
  execHide_main / r113 execHide (`li r3,6` after `li r4,20; li r6,0` in the target) and em2d JumpAtk (`cmpwi` before
  `stw`) move under no variant either (JumpAtk even gets worse without weights), so they are not rank ties of our RTL;
  t_option tp_pl_flag (5, the `addi r3,r30,ItemMgr@l` "no dying-source bonus") is untouched by every death variant
  because its PRE'd high pseudo's death is real in ours -- again an RTL difference (the #12/#3 families), not the bonus.
- Do not re-run the rank_for_schedule/weight/calls.c/md experiments; nothing in /tmp/rank18 is to be installed.

### COMPILER-DIFF #8 closed: a different-mode read of the incoming hard register removes the parameter copy's REG_DEAD (emshield + st2_3/r223 Matching; emwep setThrow 2 -> 0; pl_wep PlWepAutoTrack 50 -> 48; em_sub EmYarareContactCk 109 -> 103; 2026-09-10)

- Harness ~/.cache/cd8/ (`cc.sh SRC OUTDIR [cc1plus flags]` = SN cpp.exe + native cc1plus with the unit's
  flags, dumps kept in OUTDIR; `try.sh SRC.cpp UNIT SYM OUTDIR` = compile a source copy, NgcAs it, word-masked compare of
  SYM (or `ALL`) against `build/G4BE08/asm/UNIT.s` via `cmp.py`; the `src/es_*.cpp` etc. copies are the variants below).
- **Mechanism (regmove.c optimize_reg_copy_1, stock).** A parameter copy `(set (reg/v:SF 83) (reg:SF 33 f1))` comes out
  of assign_parms WITHOUT a death note; flow puts `REG_DEAD f1` on the last user of f1 in the block. If that user is a
  later insn P of block 0, regmove (`-fexpensive-optimizations`, pass 0) runs optimize_reg_copy_1 on the copy: it scans
  forward to P, substitutes the pseudo for f1 in every insn up to P (`validate_replace_rtx`), and MOVES the death note
  to the copy. That is why every same-mode keep-alive fails: `asm("" : "=m"(x) : "f"(hf))` with `register f32 hf
  asm("fr1")` shows `(reg/v:SF 83)` as the asm input in the `-dN` dump (the `-dc` combine dump still has f1 and the death
  on the asm), the death sits on the copy again and the asm has become a dependent of the copy (priority +1, worse).
  cse is NOT the culprit: `canon_reg` never replaces a hard register. The scan has three exits that leave f1 live:
  (1) a `(use (reg f1))` insn ("Don't change a USE of a register") -- only expand_value_return emits one, for the
  return register; (2) a basic-block boundary (jump/label/loop note) -- a use in another block works but keeps f1 live
  through every call-free path there (setFall: f1 live around the loop, the third `fmuls f1` of the fRand arm moved to
  f0 and cross-jumped, 85 words); (3) **the death test `find_regno_note (p, REG_DEAD, sregno) && GET_MODE (note) ==
  GET_MODE (src)`: a death in another mode is not accepted, the `else if (sregno < FIRST_PSEUDO_REGISTER &&
  dead_or_set_p (p, src)) break;` clause stops the scan, and the copy keeps f1 alive** -- INSN_REG_WEIGHT of the copy
  becomes +1 (one SET, no death) and it sorts with the `li`/`lis`/`addi` group instead of the dying copies.
- **Recipe (tag every line `// COMPILER-DIFF: #8`).** In the function body, block 0 (before the first branch/call):
  ```
  register f64 hd asm("fr1");              // f32 parameter in f1: read it in DFmode
  asm("" : "=m"(hp) : "f"(hd));            // codeless; hp = a `this`/first-parameter field block 0 neither loads nor stores
  register s16 hm asm("r5");               // int parameter in r5: read it in HImode (one register; DF in a GPR covers two)
  asm("" : "=m"(obj->be_flag) : "r"(hm));
  ```
  Evidence: `-dN` insn 7 `(set (reg/v:SF 83) (reg:SF 33 f1)) ... (nil)` (no note; the asm carries `REG_DEAD (reg/v:DF 33
  f1)`), `-dS -fsched-verbose-9` setFall block 0 t=4 ready `513 495 33 7 9 26 507` -> picks 507, 9 (`mr r31,r4`), t=5 26
  (`stw`), 7 (`fmr f29,f1`) = the target's `mr r31,r4; stw r0,0x1d8(r29); fmr f29,f1`. Output-operand rules, all
  measured on setFall: (a) the "=m" location must NOT be stored later in the same block by an identical MEM -- flow's
  `mem_set_list` deletes the asm as a dead store (asm before `pMotion = 0` with `"=m"(pMotion)`: asm gone, 2 words
  back); (b) it must not be the location of a block-0 store either (`"=m"(pMotion)` after `pMotion = 0`: output
  dependence, the `stw` gains a dependent and sched2 issues it three slots early, 3 words); (c) not a field through a
  derived pointer (`"=m"(w->gravity)`, w = this+0x3e0: the asm's address depends on the `addi`, whose priority rises,
  4 words) and not a frame slot (`"=m"(v.x)`: no dependence in sched1, but after reload `(mem (r1+56))` vs
  `(mem (r29+472))` is an output dependence -- sched2's alias.c has no base for r1 -- 3 words); (d) a field of the
  same base register as the block's stores with a different offset (`hp`, `xFC`) gives NO dependence in either pass
  (`memrefs_conflict_p` separates them by offset), the asm is ready at t=2 with priority 1 and weight 0 (one SET, the
  DF death) and is issued last in every dump; one output suffices (three "=m" outputs, weight +2, identical bytes).
  (e) Inside the block the asm may sit anywhere; put it right after the parameter-copy-independent first statement.
- **Where the death alone is not enough: r223 reva_common_move** (`mr r31,r3; fmr f28,f1; fmr f29,f2; mr r29,r5; lfs
  f30`). With only `mode`'s copy at +1 the ranking gives `fmr; fmr; lfs f30; mr r29,r5` (the `lfs` of `spd = 0.0f` is
  weight 0 because its `high` pseudo dies there; the target's `lfs` ranked as +1 = the #13 family, its high pseudo
  lived on to the loop preheader where reload rematerialised `lis r9`). The r226 reorder alias reproduces it with
  every copy at weight 0: `extern "C" void reva_common_move__FP4cObjiiff(cObj*, int axis, f32 lo, f32 hi, int mode)`
  + `#define reva_common_move(obj, axis, mode, lo, hi) ...(obj, axis, lo, hi, mode)` (same argument registers; the
  callers' arg-move order is unchanged in bytes). So: try the reorder alias first when the target order is simply
  "FP copies before a trailing int copy" among weight-0 copies (r223, r226); use the different-mode read when an FP
  copy must sink below GPR copies/stores that follow it in LUID order (setFall, setThrow) or an int copy must sink
  below an FP copy that follows it (PlWepAutoTrack `mode` r4, EmYarareContactCk `out` r5).
- Results (words before -> after): emshield setFall 2 -> 0 (unit 14/14, flipped), emwep setThrow 2 -> 0 (unit still
  41/62), st2_3 r223 reva_common_move 4 -> 0 (unit 29/29, module flag set, st2_3.rel OK), pl_wep PlWepAutoTrack 50 ->
  48 (prologue fixed; the 48 left are the m3r tail: ours keeps a second `lis r8,m3r@ha` for the `stfs f1` where the
  target stores through the first high `stfs f1,m3r@l(r11)` -- #3/#12 high family), em_sub EmYarareContactCk 109 ->
  103 (prologue fixed; frame 0xd0 vs 0xc0 with one more GPR saved, allocation residue). The top-of-file #1 note and
  the "#1 / #8 research" section's "no source lever" sentence are superseded by this section.

### COMPILER-DIFF tag audit (2026-09-10)

Harness ~/.cache/tagaudit (`inventory.py` -> `tags.tsv` = file, line, item, kind, enclosing function,
unit(s), Matching, age; `h.py base|check|accept|revert SRC` = rebuild the source's units and compare each .o with the
saved baseline, and when an .o differs, relink the module .rel / main.dol and compare its sha1 with build.sha1 (an
alias declaration removal changes the symbol table but not the sections, so the object compare alone gives false
DIFFs); `t.py SRC LABEL OLD NEW..` = apply exact replacements, check, accept or revert; `atari1.py`, `alias4.py` =
the #1 atari-init and #4 int-view alias families; `results.tsv` = every trial). Scope: units flagged Matching in
config/G4BE08/objects.py / modules.py whose source was untouched for 30 minutes (77 of the 116 tagged files; the 39
non-Matching files and the 6 headers were not touched). Every unit stayed byte-identical: `python3 configure.py &&
ninja -k 0 && dtk shasum -c` = 111 OK after each accepted change.

- **Inventory (301 tag lines, 161 of them comment-only; code lines in brackets):** #13 61 [38], #4 45 [4], #12 31
  [24], #1 30 [18], candidate #12 24 [13], #2 22 [14], #3 12 [7], #6 11 [1], #8 11 [4], candidate #17 9 [7], #5 8
  [1], #17 7 [1], candidate #9 4 [2], #9 2, candidate #10 2, candidate #8 2 [1], #11 1, candidate #3 1, candidate
  #14/#15/#16 1 [1] each, M1 1 (CRI, sfd_lib), unnumbered 14 ("tie" 4, "candidate (..)" 5, none 5). Per kind (code
  lines): register-asm 30, launder 29, opaque-asm 19, alias-use 18 (the atariInitF/AtariInit calls), alias 16,
  dead-test 13, keep-alive 9, dead-init 3, other 3.
- **Necessity check: 167 workarounds tested in Matching units (one trial per workaround; an alias is tested with
  all its call sites in plain form at once, each register pin / launder / dead test separately), 15 unnecessary
  (removed, plain form kept), 152 needed (restored exactly).** Removed:
  - #1 floats-first `atariInitF`/`AtariInit` -> `x.init(parts, flags, cnt, ...)`: em21 R0_Init, em22 R0_Init, em23
    R0_Init, em27 R0_Init, em2a R0_Init, em36 R0_Init, em3d R0_Init, wep14/objMine cObjMine::init (8 of 26 atari-init
    sites; the other 18 -- em24/26/28/29/2e/2f/31/34 x3/35/38/3a/3b x2, pl14 -- still need the alias, as do the
    setThrow/PathGetPosEm/SatMgrCreateF/playerPillarDownCk order aliases).
  - #2: r226 R226Init `asm("" : "+r"(id))` on the GetEmIdFromList result (plain `(u8) id` argument identical), r226
    R226EventPassageSwitchMain `asm("li %0,29" : "=r"(estNo))` -> `estNo = 29`, r20f R20fInit `asm("" : "+r"(id))`,
    em2c em2cBlendMotSet2 `asm("" : "=r"(dd) : "0"((int) d))` -> `dd = (int) d` (the em2cBlendMotSet copy and the
    two "order only" launders of BlendMotSet2 are still needed).
  - #4: r411 `GetEmIdFromListI` int view (plain `GetEmIdFromList` identical in both callers).
  - #5: r100 R100Init `zero = 0` local for the else arm's two byte stores (literal 0 identical).
  - candidate #14: em2d em2dInitRtnSet `register f32 fz asm("fr31")` (a plain `f32 fz` gets f31 anyway).
  Everything else tested stays: all #13 hard-register/asm-emitted constants (em32 R0_Init x6, em21DmCk, em27DmCk,
  em2a two, r207 zero, r224 x3, r201 `on` launder, pl_shotgun fr0, espgen10 x3, pl0f `one`, t_sce_item dead test),
  all #12 / candidate #12 forms (t_sce_at 9 aliases + do-while, t_sce_item 4 aliases, r218 4 do-whiles, r108, r226,
  em32 W_FRESH/W_SET x4, ss_item asm li x2, em21/em2a cam aliases, em2d fmr, t_mv, r11d, em25), the #4 int-view
  aliases in em10 (16 units), em32, em2c, ss_item x3, ss_file, ss_model x5, r104 x2, r108, r11d x2, r203, r205, r214
  x4, r216, r217, r21b, r223, r226, r20b, r20c, r40e, r400, r403, the #2 launders (em32 x2, em2c x4, em36 r30,
  em10 Ctrl12SetS, pl_class clrlwi, t_flr_at, r226 int copies), #3 (r108 tbl, merchant 0.5 high), #5 (em18 fabsfE,
  r20c hp, ss_map zero), #6 (r207 x2, EtcModel, sndvd), #8 (emshield, r223), #11 (r200), candidate #8/#9/#15/#16/#17
  (em2d, r113, r11d, em2c x2, t_sce_item, ss_item x3, r22c, r203 x2, t_flag x2, objWep), the unnumbered ties
  (vfprintf, lightPath, room_jmp, emBar, pl14, em21 neck do-while, texture pins, pl_debug fadds, merchant dead
  tests).
- **Tag hygiene:** added the missing tag to em2c em2cBlendMotSet2's third launder (`bb`, #2 order-only) and to r223's
  `cEmWrapSetEmI` alias (#4); retagged em25 em25DmCk's "cse AROUND path" launder as `candidate #12 (AROUND form)`
  (the #12 sweep's form (a)); reworded the r226 #2 comment that described the removed asm set; removed r411's
  orphaned comment. Left as is and listed: the unnumbered tags "tie" (vfprintf fftoa, lightPath movePath, room_jmp
  getRoomInfo, emBar emBarHitCk; all loop-note register-weight levers), "register tie" (pl14 cSubLuis scan),
  "candidate (local-alloc qty order)" (texture DataLoad), "candidate (operand order)" (pl_debug DrawGage),
  "candidate (loop.c pass-2 insn_count)" (merchant buyupPrice), "gcse PRE pseudo numbering" (sce_at, not Matching),
  "candidate (gcse cprop)" (em2b, not Matching), r204/r118 "dead test" (not Matching / owned), and em21's
  "COMPILER-DIFF-tagged tie lever" comment (a mention, not a tag). The #5 and #6 tags on needed workarounds (em18,
  r20c, ss_map; r207, EtcModel, sndvd, r22c, item, ss_pzzl, event, em35) were not retagged: both items are resolved
  as stock-compiler, source-form differences, so those workarounds will not be removable when the original build
  turns up; a mechanical sweep should treat #5/#6 tags as permanent source levers.
- **Skipped (not tested):** the 39 non-Matching tagged files (em2b, em39, em3c, cam_ctrl, cam_extra, emrock, em_sub,
  emwep, esp16, event, item, mercenaries, pl_wep, puzzle, sce_at, shadow, title, adx_bau, ss_pzzl, ss_shop, r103,
  r11b, r11c, r202, r204, r209, r20d, r213, r221, r404, t_camera, t_camera_data, db_port, db_widget, t_event, t_id,
  t_scroll, snd_test, t_snd_vol, db_light); Matching files edited by another agent within the 30-minute window
  (st4/r402 R402MoveDoor02 `asm("" : "=r"(obj) : "0"(obj) : "r31")` #3 -- its #1 SatMgrCreateF alias was tested
  before the edit; espgen02 #17, filter06 #17, sfd_lib M1, r118 dead test / candidate #12); the 6 headers
  (dbg_tool.h #13 x6, db_toolbase.h #13, math_sub.h #1 de_Boor_CoxF, id_sys.h #4, item.h #4 x2, db_mod.h #4:
  their users include non-Matching units, so a plain form cannot be verified without changing those). No tagged
  site was skipped for an unclear plain form.
- **Per-item status after the audit:** #1 (arg-move order) compiler-side tie (rank18 research negative); 8 of 26
  atari-init aliases were unnecessary, so check the plain member call before adding one. #2 (narrow extension)
  compiler-side (combine's promoted-parameter knowledge); 4 of 18 launders unnecessary. #3 (frame-address / high
  PRE) compiler-side, unknown mechanism (gcse3/highcost negative). #4 (narrow truncation) compiler-side; 1 of 33
  int-view aliases unnecessary. #5 RESOLVED stock haifa: the remaining tags are source-form levers (shared local /
  early-clobber fabs / asm-anchored copy). #6 RESOLVED stock jump2: the remaining tags are RTL-at-jump2-entry levers
  (empty asm, tied launder). #7 candidate (no tagged site). #8 CLOSED with the different-mode hard-register read
  (emshield, r223; em2d's candidate #8 launder is a sched1 tie, still needed). #9 candidate (rotated loop entry: r113,
  r11d launders needed). #10 candidate (rodata copies; no code). #11 candidate (temp-slot merge: r200 needed). #12
  compiler-side (cse hash table carried into a block the original entered empty); every tested form is needed. #13
  compiler-side (REG_EQUIV constants never allocated; equiv13 research negative); all 24 tested sites needed. #14
  candidate: em10 setHand asm needed, em2d fr31 pin unnecessary. #15, #16 candidates: em2c asms needed. #17
  candidate (global.c pass 0 regs_used_so_far): all 9 tested pins needed.

### COMPILER-DIFF #7/#9 closed: peeled FP loop exit tests are a source form (r106, r11c Matching, r202 throwRock 0, r103/r105 natural, puzzle selPiece 61% -> 99.5%; 2026-09-10)

- Harness ~/.cache/fold7 (copy of ccfp7 with paths rewritten; `dj.sh CFG SRC MOD OUTDIR -dX..` = cpp + cc/CFG/cc1plus
  + NgcAs with the dumps kept, `rtl.py DUMP FUNC [-a]` one line per insn, `exitcode.py DUMP FUNC` = the copied
  exit code (VTOP..LOOP_END) and the insns before each LOOP_BEG, `duppass.py CFG UNIT FUNC` = in which jump pass
  duplicate_loop_exit_test fired, `scan8.py` = target-asm scan for the `b L; ..; L: bcc` two-compare shape,
  `mk.sh NAME "-Ddefs"` builds a hooked cc1plus; jump.c hooks FOLD7_NODUP1/2, FOLD7_DUPLIMIT=N, FOLD7_NOFOLD_FWD).
  `stock/jump.c` = stock 2.95.3 from gcc-mirror: SN's jump.c is stock apart from the header and PROTO removal.
- **Mechanism (jump.c read, -dj/-dJ dumps on the natural forms).** `expand_end_loop` rotates any loop whose first
  jump to the end label is within 30 insns (`for (;;) { A; if (c) { S; break; } sleep; }` -> `LOOP_BEG; b START;
  TOP: sleep; START: A; cmp; bcc TOP; S; b END`), and jump1's `duplicate_loop_exit_test` copies START..LOOP_END
  (<= 20 INSN/JUMP_INSN, no call/label/asm/inner LOOP_BEG) in front of LOOP_BEG, rescanning the copy at once.
  The copy's `bcc TOP` is folded over the following `b END` ("conditional jump jumping over an unconditional
  jump", `prev_active_insn (b END) == insn`) whenever the two are adjacent; for FP compares the fold goes through
  invert_exp's arm SWAP (`can_reverse_comparison_p` refuses MODE_FLOAT reversal and the swapped `(pc) (label_ref)`
  branch pattern exists), which is why VecRadLimit's peel prints `cror; bns END` (swapped GE) and not `blt END`.
  The parent hypothesis "fold iff can_reverse_comparison_p" is therefore wrong: the fold never depends on it.
  What the original did in r106/r11c is simply the case where an INSN sits between the copied `bcc TOP` and
  `b END` -- the exit store `x = lim` on the break path -- so no fold is possible in any jump pass; at jump2
  the fall-through cross-jump of the copy's `b END` (`find_cross_jump (b END, END, 1)`) matches backwards
  `store == store`, `bcc TOP == bcc TOP` (same CR reg and label after reload) and stops at the first
  register difference (the compares: the copy's operand is cse1's forwarded pre-loop temp f0, the loop's is its
  own temp f13), deleting the copy's tail and redirecting `b END` to a new label before the loop's `bcc`:
  `A0; cmp0; b TEST; TOP: sleep; A; cmp; TEST: bcc TOP; stfs lim; END`. With `while (!(c)) {..}` or `if (c)
  break;` there is nothing between the copied jump and `b END`, the fold fires and jump2 has nothing to pair.
- **Why the 30 "fold" functions fold**: their break path is empty (`while (p[i] >= PI) p[i] -= PI2;`, r207
  `while (pos.z < K) {...}`), so the copy folds; r227 checkBox0Fall's `while (1) { A; if (c) break; sleep; }`
  has a >20-insn exit code at jump1, is peeled by the pre-cse2 jump pass and folded there, then jump2's
  condjump cross-jump (`jump_back_p`) deletes the LOOP's register-identical exit code (`TOP: A; cmp; blt END;
  sleep; b TOP`). The whole-tree `-D` variants (FP-only no-fold = 30 regressions; "no fold when the compared
  MEM is stored earlier in the block" = 7 / FP-only 1 regression, esp08 move) are all superseded: nothing is
  installed, the base compiler reproduces every case from the right source.
- **Source recipes (all verified byte-identical with the installed compiler):**
  - Peeled test, compare in both predecessors, `b TEST` entry, store after the loop's test (r106 doors/body,
    r11c closeGate, r103/r105 execOpenCover):
    `for (;;) { m->rot.y += K; if (m->rot.y > lim) { m->rot.y = lim; break; } SceSleep(1); }`
    (`for (;;)`, NOT `while (1)`: the constant-true test changes the rotation). The exit store must be on the
    break path. A second such loop right after the first (r106's close half) shows no peel at all: its copy
    sits after the first loop's END label, cse1 cannot forward the `rot.y = lim` store into it, the copy is
    register-identical to the loop's exit code and jump2 deletes all of it (`stfs lim; b TEST2; wait: sleep;
    TEST2: A; cmp; stfs; bge wait; stfs 0`).
  - `step` as a variable (f31 across SceSleep), constants for the limit written inline in BOTH the compare and
    the exit store (r103/r105: `if (rot.z < -(73.0f * 0.01f)) { rot.z = -(73.0f * 0.01f); break; }`; a `lim`
    variable gives 54 words). r103's `do { } while (0);` sched barrier is not needed any more (the for-loop's
    LOOP_END note is the barrier).
  - Peel made AFTER loop.c (`fmr f28,f30` = the exit store's constant hoisted and cse2'd into `lim`, the copy's
    store `stfs f30`, the loop's `stfs f28`; r202 throwRock): make the exit code > 20 insns at jump1 so jump1
    refuses (ours was exactly 19 + `b END`): `v += -0.034906585f;` inline (3 insns for the constant) instead
    of `v += a;`, exit store `rot.x = -0.24137f;` as a constant (cse1 folds the copy's load into `lim`).
    The third (bounce) loop is the natural nested `for (i..) { lim *= 0.5f; v *= -0.5f; for (;;) { rot.x += v;
    v += K; if (rot.x < lim && v < 0.0f) { rot.x = lim; break; } SceSleep(1); } }` (never peeled: > 20 insns
    at jump1, gcse insertions between LOOP_BEG and the entry jump at the pre-cse2 pass). throwRock 9 -> 0.
  - Never-peeled integer loop with a store on the break path (puzzle selPiece): the clamp macro goes INSIDE the
    `if (p == 0) { SEL_CHECK(..); break; }` (exit code > 20 insns), not after the loop. 115 -> 17 words, the
    17 left are the r27/r29 (`ret`/`d`) allocation swap.
- Sanity limits of duplicate_loop_exit_test that decide "peeled or not" (count on the raw expansion, rescanned
  each `while (changed)` round without the deleted `b END`): 20 INSN+JUMP_INSN, no CALL_INSN, no CODE_LABEL, no
  ASM_OPERANDS, no LOOP_BEG/LOOP_CONT note inside START..LOOP_END. A second chance is the pre-cse2 after_regscan
  jump pass, which only fires when LOOP_BEG is still directly followed by the entry jump (gcse PRE insertions
  and loop.c hoists land between them whenever the loop has a preheader block, r106 under FOLD7_NODUP1).
- Results: r106 shakeClosetDoorR/L 9+9 -> 0, shakeClosetBody 8 -> 0 (unit 16/17, openShelf_main r29/r30 +
  r9/r11 swaps left); r11c closeGate 21 -> 0, unit Matching (st1_3.rel OK); r202 throwRock 9 -> 0 (unit 27/30);
  r103/r105 execOpenCover rewritten to the natural form, bytes identical (both RELs OK); puzzle selPiece
  61.9% -> 99.5%. r214 not touched (another agent).


### COMPILER-DIFF tag audit 2 (2026-09-11)

Harness ~/.cache/tagaudit2 (copies of ~/.cache/dol18a plus `inventory.py` -> `tags.tsv`/`scope.txt`; `h.py units|base|try|check|diff
SRC` = rebuild every unit of a source privately with the real ninja cflags/post-build and compare every non-symtab section
byte-for-byte (bytes + relocs) against a saved base .o; `necessity.py [FILTER]` = derive the PLAIN form of every code-bearing
tag line mechanically (asm launder/keep-alive -> deleted, `asm(""... "=r"(x):"0"(y))` -> `x = y`, `asm("li %0,K")` ->
`x = K`, `register T x asm("rN")` -> `T x`, `do{}while(0)` -> deleted) and rebuild). Scope: 363 tag lines in Matching DOL
units (objects.py) and Matching REL units (modules.py), EXCLUDING lib/CRI, tools/*, t_*/Tools, and the units other agents own
(st2/r202,r204,r20e,r225, st1/r10f, and the unmatched-unit list). Every accepted change kept the unit byte-identical (private
compile + `ninja <unit>.o` + `cmp` with the pre-edit copy).

- **Mechanical necessity sweep: 113 code-bearing tag lines reduced to plain C and rebuilt; 5 were unnecessary (removed,
  byte-identical), 108 needed (restored).** This is the whole-scope confirmation of audit-1's per-item findings: every
  asm-emitting form (`lis`/`li`/`lfs`/`addi`/`mr`/`fmr` asms, `.rodata` pins), every value pin/keep-alive, every launder and
  dead statement stays needed EXCEPT:

  | tag group | site (before -> after) | structural form that replaced it |
  |---|---|---|
  | #2 order-only launder | em2c em2cBlendMotSet2 (2 launders -> 0) | pass the promoted `m1`/`b` arguments straight into `MotionSetCore` (the `int mm1/bb` opaque copies were unnecessary; the prologue copy order is unchanged) |
  | #13 value pin | game/cam_qfps init `one` r10 (pin -> plain `int one`) | a plain `int one` gets r10 anyway; only `two` r8 and `zero` r7 still need the pin |
  | #13 asm-block reg pin | game/esp_app EspDrawLaserLine `hi` r11 (pin -> plain `u32 hi`) | plain `u32 hi` takes r11; the asm `lis`/`li`/`lfs` and `c5` r8 pin still needed |
  | candidate #12 address form | Sscrn/ss_pzzl pieceTblInit `asm("" : "+r"(tix))` (launder -> deleted) | the block-local `u32 tix` alone keeps the +20 out of the load displacement |
  | #12 AROUND form | st2 r20d_checkSwitch `asm("fmr")` copy + `do{}while(0)` marker (2 -> 0) | `zero = spd;` plain assignment; the loop-exit `do{}while(0)` was redundant with the k0-asm block already present |

- **Groups confirmed fully needed (representative sites tried with the view/ss_term structural lens, all DIFF when plainified):**
  - #1 atari-init floats-first alias (`atariInitF`/`AtariInit`, 15 sites in em24/26/28/29/2e/2f/31/34/35/38/3a/3b/3c/39): all still need the alias (the 8 audit-1 removals were the member-call sites; these are the interleaved ones). Compiler-side (rank18 negative).
  - #1 arg-move order comments (em10, emBarred, pl0e, pl14, r213, r226, r402, t_option): compiler-side, no code to remove.
  - #4 int-view/narrow aliases (em10 x3, em2b, em2c, em32, em39, r104/r108/r11d/r203/r205/r209/r216/r217/r21b/r223/r226/r400/r403/r404/r40e, ss_file/ss_item/ss_model/ss_pzzl/ss_shop): all needed. Compiler-side; see the PROMOTE_PROTOTYPES probe below.
  - #2 narrow extension (em2b, em2c x4, em32, em36 `register u8 kind`, em39, item, pl_class, r20d, r221, r226): all needed. See the PROMOTE_PROTOTYPES probe.
  - #13 asm-emitted constants / hard-register pins / keep-alives (em21DmCk r9, em2a `int two`, em2b x63c + the x63C/x640/x4/rope keep-alives, em32 R0_Init lw/bn/hp/six/one + `li flip`, em39 zero/w r29, esp04 `s` fr0 + keep-alive, esp12/esp16 `z` fr12 + magic, esp_app `lis`/`li`/`lfs`, espgen10 hi/list/idx, event `zero`, item `sz`/`m1`, pad `dead` r16, pl0f, pl_shotgun `zero` fr0, r11b `one`, r201, r207, r224 x2, ss_pzzl `lis`/`addi`/`mr`, ss_shop `zero`/`cur`): all needed. Compiler-side (equiv13 negative: the original allocates REG_EQUIV constant pseudos like stock 2.95 in >12000 identical functions).
  - #12 cse-path forms (em2b re-walk x2 + gcse-cprop `one`, em25 AROUND, em32 W_FRESH/W_SET + path knowledge, r20d k0-asm block + `st` r29 + `addi rot` regmove pick, r213/r224/r226 AROUND, ss_item `li` x2): all needed. Compiler-side (cse hash table carried into a block the original entered empty).
  - #3 (em3c, item `wm` copy in model, merchant, r108, r10c, r213, r222, r20d, ss_shop `scr` launders, r402): all needed. Compiler-side (gcse3/highcost negative).
  - #5 (em18 fabsf, ss_map/ss_pzzl sched ties, r20c): RESOLVED stock-haifa source levers, not removable.
  - #6 (item `use` 11 `asm volatile("")` layout barriers, em35, EtcModel, event, r207 x2, r22c, ss_pzzl x2): RESOLVED stock-jump2 RTL-at-entry levers. item `use` tried in 13 structural spellings (break/goto-ng/return-0/inline-dec/goto-declabel polarity, big-arm variants) -> min 22 words: the ten identical `bl healing; cmpwi; bne KEEP; b NG` tails are register-identical, so stock jump2 merges them without the barrier; needed.
  - #8 (emshield `fr1`, em2d `f`, r223): RESOLVED, the different-mode hard-register read; needed.
  - #17 candidates (esp04 `s`, espgen02 colR, event `pin` r27, filter06, item `arm` r0 x2, objWep, obj00, pad, ss_item pin/hi/lo, ss_pzzl base/mp, ss_shop val, r103/r106/r10c/r203/r22c pins, r221 evNo, t_flag, em39 pep/x8B6): all needed (global.c pass-0 regs_used_so_far / local-alloc fake-lifetime).
  - candidate "polymorphic copy vptr temp" (esp0a x4, esp0e): NOT removable. `*p = *esp` on a class with a vptr expands to a block move of the WHOLE object (the vptr word included, `mem/f` at +0xF4 with a plain SImode set) -- so ours floats the vptr reload above the block stores (fixed scalar vs varying struct). The manual `memcpy` + `volatile u32 vt` save/restore is the source form that reproduces the target's ordered reload; `*p = *esp` / a plain (non-volatile) temp both DIFF (28-33 words). Confirmed: the synthesized `operator=` (cp/method.c do_build_assign_ref) is bitwise for these POD-with-vptr classes, so there is no member-wise assignment to lean on.

- **Compiler-config probe (NEGATIVE, nothing installed): `#define PROMOTE_PROTOTYPES` in rs6000.h.** #2/#4 are the narrow
  argument extension/truncation family. cc1plus's rs6000.h defines PROMOTE_FUNCTION_ARGS but NOT PROMOTE_PROTOTYPES, so cp/decl.c
  keeps `DECL_ARG_TYPE = TREE_TYPE` for a prototyped narrow parameter and cp/call.c/typeck.c pass the value unpromoted; the callee
  then has no `clrlwi`/`extsh` to fold and combine's setup_incoming_promotions drops the extension at the use. Built a
  PROMOTE_PROTOTYPES cc1plus (~/.cache/tagaudit2/sngcc-pp) and tried it on the #2 sites: it makes em2c em2cBlendMotSet/Set2 and
  pl_class cMot3::set MASK correctly (`clrlwi` at the calls) with the plain source -- BUT it regresses 5 of 63 Matching game units
  whole-source (t_log, db_log, atari, flr_at, quake) and leaves em2c 3 words off (a prologue-copy order shift), so it is NOT the
  shipped compiler's config either. The lever is real for the #2/#4 mechanism (narrow prototyped args) but not installable; the
  aliases/launders stay. (Recommendation for a future compiler hunt: a later SN build may enable PROMOTE_PROTOTYPES selectively;
  smoke-test the 5 regressors before adopting.)

- **Tag table after audit 2** (per group: sites in Matching scope -> removed / needed):
  #1 23 -> 0/23; #2 14 -> 2/12; #3 16 -> 0/16; #4 27 -> 0/27; #5 3 -> 0/3; #6 20 -> 0/20; #8 6 -> 0/6; #9 4 -> 0/4;
  #11 1 -> 0/1; #12 (all forms incl candidate) 40 -> 3/37; #13 (all forms) 69 -> 2/67; #17 (incl candidate) 45 -> 0/45;
  cand "polymorphic copy vptr temp" 31 -> 0/31; misc ties (loop-note weight, sched slot, global-alloc) 30 -> 0/30.
  Total 363 code+comment lines, 113 code-bearing tested, 5 sites (7 tag lines) removed. Every remaining tag is a confirmed
  source lever for a stock-compiler difference (#1/#2/#3/#4/#12/#13/#17) or a resolved source-form lever (#5/#6/#8/#9) -- none is
  removable until a later SN compiler build turns up; treat them as permanent per the audit-1 note.
  UPDATE 2026-09-11: cand "polymorphic copy vptr temp" 31 -> 31/0 -- removed with the installed
  shipped-build-temp-flags compiler patch (see "Compiler"; esp0a/esp0e plain `*p = *q`, byte-identical). Every other group
  unchanged: the patch touches only assign_temp/assign_parms slots, and the five representative removals of the
  "fixed scalar" comment family (snd RefU16, objRobo GRef/FRef, esp_app filler, trans ISet, shadow FRef) all still DIFF.
- **DOL/REL state:** the 111-check was 111 OK at the start of this pass; at the end main.dol was FAILED because emwep.cpp
  (SetWeapon 99.80%) and id_sys.cpp (IDSystem::move 98.70%) -- both in the skip list, both Matching=True, both edited by other
  agents within the last 20 min -- were mid-edit and not byte-identical. That regression is not this pass's: all 5 edited units
  (em2c, cam_qfps, esp_app, ss_pzzl, r20d) are byte-identical to their pre-edit .o (verified by private compile + `cmp`).
- Files changed: src/em2c/em2c.cpp, src/game/cam_qfps.cpp, src/game/esp_app.cpp, src/Sscrn/ss_pzzl.cpp, src/st2/r20d.cpp.

### Structural tag hunt (2026-09-11)

Harness ~/.cache/taghunt (dol19a copies with the paths rewritten; `h.py NCCDIR LABEL` = rebuild every `prodg_cc*` unit of
build.ninja with the cc1plus/cc1 in NCCDIR into `tree/LABEL/` (the real cflags and post_build, `$python` expanded, ~10 s for
734 units) and compare base vs variant per function (mcmp masked compare) against the split objects -> `reg_LABEL.txt` /
`fix_LABEL.txt`; `sngcc*/` = copies of tools/sn-gcc with one-line function.c edits, `make ./cc1plus` in the copy; deleted at
the end). No source file was changed in this pass; 111 files OK before and after (the `ninja` "FAILED build.ninja
objdiff.json" seen all morning is other agents' configure.py runs racing on objdiff.json, not a build failure).

- **Family A, "polymorphic copy vptr temp" (esp0a x4, esp0e; 31 tag lines): the structure IS the plain `*p = *esp`, and the
  one thing that differs is a MEM flag the original compiler does not set.** Read off the tree with gdb on cc1plus
  (`break emit_block_move` / `save_noncopied_parts`, `debug_tree` of the expand_expr_stmt argument): the statement is a bare
  `MODIFY_EXPR` (cp/call.c build_over_call's trivial-assign shortcut; TYPE_HAS_COMPLEX_ASSIGN_REF is not set for `has_virtual`,
  so `operator=` is bitwise), and expr.c's `case MODIFY_EXPR` runs `save_noncopied_parts` because cp/class.c gives every class
  with a vfield `TYPE_NONCOPIED_PARTS = (vtable, vfield)`: `assign_temp (vptr type, keep 0, memory_required 1)` (the slot at
  frame offset 0 = `8(r1)`, freed after the statement and reused by the next copy -- the `stw r0,8(r1)` of every copy in
  the target), `store_expr` of `p->vptr` into it, the block move (SN's 24-byte `expand_block_move` loop, ONE temp pseudo),
  then `expand_assignment (p->vptr, RTL_EXPR temp)` = the `lwz r0,8(r1); stw r0,0xf4(p)` restore. Stock GCC 2.95, nothing
  SN-specific, no source construct involved. The reload floats above the tail copy stores in ours only because stock
  `assign_temp` does `MEM_SET_IN_STRUCT_P (tmp, AGGREGATE_TYPE_P (type))`, which for the pointer-typed temp SETS
  `MEM_SCALAR_P` (`mem/f`), and alias.c `fixed_scalar_and_varying_struct_p` then exempts the fixed scalar read from the
  `mem/s` stores through the stepping pointer. (Side fact that misled the audit: `assign_stack_temp_for_type` reuses the
  slot's rtx object, so a later `volatile u32 vt` declared in the same function retroactively turns the earlier compiler
  temp into `mem/v/f` -- the `.rtl` dump flags of a compiler temp are not its own.)
  - Source forms tried with the installed compiler, all DIFF (Esp0a_Trans words): `*base = *esp` 18, `base[0] = esp[0]` 18,
    `*base = *(const cEsp*) esp` 18, `cEsp& d = *base; d = *esp` 30, `cEsp* d = base; *d = *esp` 30, `*(cEsp0a*) base = *esp`
    31, `static inline EspCopy(d, s) { *d = *s; }` 63, volatile-qualified source/dest = compile errors (the implicit
    `operator=` is not volatile). There is no C++ spelling: the dest MEM of an aggregate INDIRECT_REF is always
    MEM_IN_STRUCT_P (expr.c 6323) and the temp is always the compiler's. The tagged form stays the only pure-C emulation
    with the installed compiler.
  - **Compiler-build finding, verified whole-tree (nothing installed, decision for the owner).** function.c edited so that
    non-aggregate stack temps and parameter slots get NEITHER flag (`if (AGGREGATE_TYPE_P (type)) MEM_SET_IN_STRUCT_P (tmp,
    1);` in `assign_temp`, and `if (aggregate) MEM_SET_IN_STRUCT_P (stack_parm / DECL_RTL (parm), 1);` at the five
    `assign_parms` sites, function.c lines 1172, 4546, 4688, 4745, 4802, 5062 of the v1.79 drop; 6 one-line changes, build
    ~1 min): **0 regressions over the 18239 functions identical today, 7 newly identical with the sources as they are**:
    objRobo TaskSwitchFront / TaskSwitchBack / R0WalkBridge (the sweep-17 "REG_EQUIV x4 / lis combine" residues), sce_com
    SceChapterEnd (17 words in sweep 18b), t_se_at seAtAreaEdit / seAtAreaEdit_EditMenu / seAtDataSave. With it, esp0a with all
    four copies written `*base = *esp; *e.p = *base; *base = *this; *p = *base;` (memcpy declaration gone) is 6/6 identical
    and esp0e with `PSMTXIdentity(m); *p = *esp;` is 8/8 identical, i.e. the whole family's 31 tag lines become removable.
    The `assign_temp`-only edit alone is 0 regressions / 6 fixes (esp0a/esp0e plain both identical too); adding the parm
    slots gives SceChapterEnd; widening further to `put_reg_into_stack` (1740), `expand_decl`'s 3023, 4907, 5104, 6189
    regresses item `combine` (a #17-pinned function) for no extra fix -- so the shipped compiler's function.c differs from
    v1.79 exactly at assign_temp + assign_parms, consistent with the snd note "address-taken parm slots without
    MEM_SCALAR_P" (SndBgmTblSet, RefU16 lever) and the "scalar frame MEM" issue-slot filler of esp_app EffAreaUpdate. If
    adopted (tools/sn-gcc/patches, rebuild, re-verify 111): remove the esp0a/esp0e volatile-memcpy blocks, then re-test
    every `RefU16`/parm-slot lever and the objRobo/sce_com/t_se_at residues' tags. Not adopted here: a compiler change is
    a project decision and other agents were building concurrently.
- **Family C, #17 `register int pin asm("rN")` used-so-far pins (event GetMod r27; also pad, ss_item, r203, r22c,
  t_sce_item): no structure found.** GetMod without the pins: nm r31, type r30, wkNo r29, this r28, mod r27 by the stock
  priority order (this 3 refs / 27 insns = 0.111, mod 3 / 49 = 0.061); the target has this r27 / mod r28, which pass 0 gives
  only when r27 is `regs_used_so_far` and free during `this`'s range. Nothing in the target's code occupies r27 in the err
  block or the tail, so it is not a live value; `regs_ever_live` from a hard-register mention or a local-alloc'd
  call-crossing qty are the remaining candidates, neither visible in the bytes. Natural spellings tried (`ret =` in both
  arms, `this->datTbl`, a `DatTbl* tbl` local (22), `mod[0] = 0`, `return ret`) all leave 6 words. The pin stays.
- **Family B (#13 keep-alive after a call / dying-store order) and F (dead tests as loop.c insn counts): read, not
  solved.** esp04 move10's y-loop (`register f32 s asm("fr0")` + `"=m"` keep-alive of `y`): the target hoists BOTH the
  step (`fmr f0,f11`, a copy of the `-sizeY` compare's load) and the bound `lim - sizeX` (`fmr f11,f13`, the if's
  own `fsubs`) and enters the loop without its own test (cse folded `v = y` into the if's jump equivalence), while the
  x-loop above it keeps `fsubs` inside the loop and duplicates the entry test -- the two halves were not written the same
  way in the original, and no call or debug statement follows that could be the keep-alive. cam_qfps init (r7/r8/r10
  constants = "reload spill registers", eleven stores in source order) and esp_app EspDrawLaserLine (`lis r11` for the
  0.8 pool, `stb r29` = the `andi.` zero substituted on the fall-through path) are register-allocation shapes, not
  structure; esp0e/esp45 HideCheck's "+3 insns" dead test has no debug-statement candidate (the loop body has no
  call, `p` is a memory struct so a dead `p.z` store would survive). All tags left in place.
- Method note: the gdb route (`gdb -batch` on the static i386 cc1plus, `break emit_block_move`, `frame N; info frame`
  for the CFA, `call (void)debug_tree(*(int*)CFA)`, `watch *(unsigned char*)(rtx+3)` for MEM flag bits) locates the
  expander of a puzzling RTL shape in minutes; the flags byte of an rtx is at offset 3 (code:16, mode:8, then
  jump/call/unchanging/volatil/in_struct/used/integrated/frame_related from bit 0).

### Compiler patch adopted: shipped-build-temp-flags (esp0a/esp0e pure C; 2026-09-11)

- Project decision (orchestrator): the tag-hunt finding is installed as `tools/sn-gcc/patches/shipped-build-temp-flags.patch`
  (header = the evidence), applied by build.sh after linux-host.patch and to the live tools/sn-gcc/src; `make` in tools/sn-gcc
  rebuilt cc1plus/cc1 (byte-identical to the harness pair built in ~/.cache/ccpatch, deleted at the end); installed atomically
  (tmp file + `mv`) into build/compilers/ProDG/3.9.3-v1.79/. sha1 before/after: cc1plus 95ebed45..d8 -> 457337da..c8, cc1
  65dae09b..19 -> f1cd0748..be (full hashes in "Compiler"). The patch applies cleanly to the drop's function.c
  (`patch --dry-run` on the CRLF->LF NGC_GNU_SRC copy; the patched result equals the harness copy).
- Whole-tree comparison (763 prodg_cc units, 19119 functions identical with the old compiler): 0 regressions, 0 changed
  objects. Sensitivity check of the harness: the wider put_reg_into_stack edit shows the known item `combine` regression
  (3 changed objects, 1 regression), as in the tag hunt.
- The tag hunt's "+7 newly identical" (objRobo TaskSwitchFront/Back/R0WalkBridge, sce_com SceChapterEnd, t_se_at x3) does not
  reproduce: HEAD objRobo is 16/19 (9/9/21 words) with both compilers, the working tree's pinned objRobo is 19/19 with both,
  the pins removed 17/19 (12/12) with both, and all 39 objRobo variants left in ~/.cache/dol19a/out are byte-identical between
  the two compilers. Those seven were the owning agents' source edits landing between the tag hunt's base and variant builds.
  The whole-tree "0 changed objects" also says why: every previously affected site already carried a source workaround.
- Rebuild: `ninja` regenerating build.ninja raced other agents' objdiff.json writes as usual; `cp build.ninja X && ninja -f X
  -k 0` rebuilt the 763 NGCCC units (the prodg_cc rule depends on the cc1plus/cc1 paths, so the mtime change alone triggers
  the rebuild); `dtk shasum -c` 111 files OK.
- esp0a.cpp / esp0e.cpp: the memcpy-through-u8*-locals + `volatile u32 vt` recipe and its 31 `// COMPILER-DIFF: candidate
  (polymorphic copy vptr temp)` lines replaced by the plain assignments; `extern "C" memcpy` declarations gone; both units 6/6
  and 8/8 byte-identical (mcmp) and Matching; objects.py comments updated (surgical, own lines only). objRobo was already
  19/19 and flagged by its owner (pins, #17); sce_com 30/33 and t_se_at 18/20 unchanged (not flipped).
- Representative workaround removals with the new compiler (all still needed, none removed): snd SndCall RefU16(blk)/(no)
  0 -> 60 words (RefU32 alone 0 -> 7); objRobo GRef(pG)/FRef 19/19 -> 17/19; esp_app EffAreaUpdate `"=m"` filler 0 -> 2;
  trans SelfShadowSetup ISet x3 0 -> 12; shadow make_comn_parallel_light FRef 2 -> 10. Their MEMs come from make_decl_rtl
  (globals/statics), expand_decl (user locals) and put_reg_into_stack (address-taken register parms), none of which the patch
  touches. Only the polymorphic-copy family was an assign_temp effect.
- Files changed: tools/sn-gcc/patches/shipped-build-temp-flags.patch (new), tools/sn-gcc/build.sh, src/game/esp0a.cpp,
  src/game/esp0e.cpp, config/G4BE08/objects.py (two comments), README.md (one sentence), AGENTS.md; untracked:
  tools/sn-gcc/src/gcc/function.c, tools/sn-gcc/cc1plus, tools/sn-gcc/cc1, build/compilers/ProDG/3.9.3-v1.79/cc1plus, cc1.

### Compiler research: inlined-MEM RTX_UNCHANGING_P (2026-09-11; nothing installed)

Question: which rule does the shipped SN build use for `RTX_UNCHANGING_P` on MEMs copied into an inlined body, such that
the em_set "#13 pool-high" cases come out right AND the constant-store ctor cases keep their RAW dependence. Harness
~/.cache/ccunch (deleted at the end): `sngcc/` = copy of tools/sn-gcc with `getenv`-switched hooks (`HK_INTU=n` in
integrate.c `copy_rtx_and_substitute`'s MEM case, `HK_TD=n` in alias.c `true_dependence`), `h.py build LABEL [VAR=VAL..]`
= every prodg_cc unit of build.ninja compiled with the private cc1plus/cc1 (16 threads, ~12 s) into tree/LABEL/ with the
real cflags/post_build, then a per-function masked compare (mcmp) against the DOL/REL split objects -> tree/LABEL.json;
`h.py cmp A B` = regressions / newly identical / changed objects. 684 units with split objects, 17029 of 17686 functions
identical with the installed compiler. Harness pitfall: mangled constructor names start with `__`, so metadata keys in
the per-unit dict must not use that prefix (my first runs silently skipped every ctor/dtor -- the numbers below are
from the fixed harness, all variants rebuilt back-to-back with the reference to exclude concurrent source edits).
Plain em_set = the tagged `EM_SET_WORK_K` call sites replaced by the `EmSetWork` inline (SRC override, not committed).

- **The target's behaviour is asymmetric per SITE, not per direction.** Read off the target bytes with the 2-cycle
  store->load latency of rs6000.md (a load issued the cycle after a store cannot be RAW-dependent on it) and the
  `li 0,0; lis; stw` issue order (a store with no dependents is never issued before ready `lis` of higher priority):
  - inlined pool loads that behave as UNCHANGING (no WAR on the body's later stores, no RAW on the caller's earlier
    stores): EmSetWork's `f32 kx/kr/kp = const` loads (body top, user variables), EmSetDist's `em->x374 = 1e16f` load
    (store source; the caller's `stb d->flags = 7` (QI, through the arg `d`) one cycle before it, then the `lwz pPL`
    that IS dependent on that stb two cycles later);
  - inlined pool loads that behave as CHANGING (RAW dependence on preceding stores): setAbility's `pitch *
    0.017453292f` (member inline; RAW on the caller's `stw bow.allow` -- reproduced to 0 words by adding the RAW deps
    back, see `HK_TD=3`), SubEyeDir::mix's `1.0f - z` (RAW on the caller's `stfs parts->x`), the cSceObj/cCoord ctor
    constants (RAW on the caller's `stw obj->be_flag` / `stw pParent`), and model.cpp's FREE `LightAreaInit(&litArea)`
    0.0 (RAW on its own body's third `stw la->lightNo`: target `stw;stw;stw;li;addi;li;lfs`, ours with /u `lis;stw;lfs`).
  So the shipped build does not have one `/u` flag state for "inlined pool MEMs"; and it is not a `true_dependence`
  change either: removing the `RTX_UNCHANGING_P (x) && !RTX_UNCHANGING_P (mem)` early-out (globally `HK_TD=1`, or
  only for the scheduler `HK_TD=2`) regresses **2787-2795** matched functions -- native pool loads float above stores
  everywhere in the target. `HK_TD=3` (keep /u on inlined pool MEMs, mark them RTX_INTEGRATED_P, and let the
  scheduler's true_dependence ignore /u for those) gives 0 regressions on the tree and fixes setAbility/r220/moveFace,
  but breaks plain em_set (EmSetEvent 1 -> 24, EmSetFromList2 3 -> 16: the 1e16 load must NOT depend on the QI stb).
- **Whole-tree table** (regressions / newly identical vs the installed compiler; plain em_set EmSetEvent, EmSetFromList2
  words in brackets, 74/73 with the installed compiler):
    HK_INTU=1  keep /u on every copied MEM                                  29 / 0   [1, 3]
    HK_INTU=2  keep /u only on MEMs whose address pseudo is set from a       23 / 0   [1, 3]  (the 23: cModel/cParts
               constant-pool ADDRESS (`hk_scan_pool_regs`)                                 ctors, 15 weapon `init`
                                                                                          (setAbility), r21d/r220/r221/
                                                                                          r225 (cSceObj ctor), pl_npc
                                                                                          moveFace (SubEyeDir::mix))
    HK_INTU=7  =2 but only member-function inlinees                          23 / 0   [74, 73] (exactly the 23 above)
    HK_INTU=6  =2 but only non-member inlinees                                1 / 0   [1, 3]   (model __6cModel via
                                                                                          LightAreaInit)
    HK_INTU=11 =2 but only MEMs copied before the body's first store         17 / 0   [1, 3]   (setAbility x15,
                                                                                          moveFace; the ctors are
                                                                                          covered by their vptr store)
    HK_INTU=4  =2 but only loads into a REG_USERVAR_P pseudo                  0 / 0   [12, 3]  (misses EmSetDist's
                                                                                          store-source 1e16)
    HK_INTU=10 =2 but only non-member inlinees AND before the body's first    0 / 0   [1, 3]
               store
    HK_INTU=2 + HK_TD=3 (see above)                                            0 / 0   [24, 16]
  Remaining plain em_set words under 10: EmSetEvent `li r28,0xff` vs ours `li -1` (the `u8 no` parameter; `const u8`
  does not change it), EmSetFromList2 the 1e16 load/`fmuls` order + f10/f12 in the EmSetDist block. Both harness-only
  (em_set is committed in its tagged form; with HK_INTU=10 the tagged form goes 18 -> 65, i.e. the tag would have to
  be removed together with the compiler change).
- **Decision: nothing installed.** No variant reaches 0 regressions AND ≥ 1 newly identical function with today's
  sources (every affected site already carries a workaround), and the 0-regression rule (10) is a two-condition
  empirical partition ("inlinee is not a class member" AND "no store of the body copied before the MEM") with no
  mechanism in the 2.95.3 source behind either condition: `this` is TREE_READONLY like every unmodified parameter
  (`initialize_for_inline` sets TREE_READONLY on all unassigned parms, so "readonly formal" does not separate members),
  member inlines are saved with the same `save_for_inline_nocopy`/DECL_DEFER_OUTPUT path, and `cse_not_expected` is
  per-function. If a plain-source site is found where 10 (or 4) makes a function identical, the edit is: in
  integrate.c `expand_inline_function`, before the copy loop, scan the inline body's insns for single sets of a pseudo
  whose SET_SRC or REG_EQUAL note contains an `(address ...)` (the saved pool reference), and in
  `copy_rtx_and_substitute`'s MEM case copy `RTX_UNCHANGING_P` when `XEXP (orig, 0)` is such a pseudo and the extra
  condition holds; install only with the whole-tree proof (tools/sn-gcc/patches, build.sh, atomic mv, 111 OK).
- Side facts for the #13 pool family: the caller-emitted parameter-constant loads of an inline call
  (`setAbility(5.73f, ..)`: expand_inline_function's parameter setup emits `(set (reg/v) (mem/u LC))` for a CONST_DOUBLE
  actual through `emit_move_insn` -> movsf -> `force_const_mem`, in the caller, not through copy_rtx_and_substitute)
  are `/u` in both compilers and never the cause of a diff; a
  dependence table with insn-level deps is `-dS -fsched-verbose-5` (`;; --- Region Dependences ---`, the `dep` column =
  number of predecessors, the list = forward dependents); the sched dumps' order is what the target follows for the
  in-block diffs of this family (INTU=2 + TD=3 reproduces cObjBow::init to the byte although only the LC17 load
  gained the RAW dependence -- the parameter-constant loads were waiting on the lsu anyway).

### GCC sweep pass 1 (parked GCC residues re-read with the full catalogue: Tools/t_esp_area 7w, t_esp/db_widget 2w, game/db_cam menu 2w, game/Espgen42/45 9w/42w, t_camera/t_camera_data 27w/62w — all unchanged, nothing applied, nothing flipped; the t_esp_area tie and the db_widget residue read to one sched1 number each; 2026-09-12)
Scratch /tmp/gsweep/ (`hv.sh HDR [units..]` = a dbg_tool.h variant judged on the five includers through variant.sh; h*.h header
variants; dbw_*.cpp db_widget variants; rtl0/ dbw0/ = rtl.sh dumps). No tree file edited unless a line below says APPLIED.
- **bytecmp artefact (not code): Tools/t_esp_area now prints 11 words, 4 of them (+0x240/248/480/490) are `lis/addi` relocs whose
  target symbol resolves to `(t_event, .rodata, 0x2408/0x2458)` in ours vs `(Tools, .rodata, 0x3988/0x39d8)` — the linkonce
  0x50/0xB0 rodata objects shared with t_event (Matching since closer 6); same bytes 3d200000/3be90000. The code residue is still the
  7-word r28/r29 ctor tie.**
- **ToolEspArea 7w, the tie read off the sched1 dump (rtl.sh, block 13 = the edit-window ctor):** two-wide issue (iu2 + lsu); the
  `li 4` (909, prio 8) fills the iu2 slot of cycle 3 next to `li r3,0x304` (922, prio 11) — pushing it after `bl new` (h1: a
  `"=m"(unused)` anchor before the `new` in CreateEditWindow takes that slot) makes it a call-free qty (`li r11,4` after the call,
  28/20/12 words) — so the target's `li r29,4` before the call means life >= [14, stw x). Its store 943 (prio 7, lsu) is issued in
  cycle 6 AFTER `addi vt` 940 (prio 8, iu2) because in-cycle order = ready-list order = priority; the only way to shorten `li 4`'s
  life to 10 (pri 8000 > work's 6666) is 943 prio >= 8, i.e. a TRUE (cost 2) store->call link to strlen instead of the ANTI (cost 1)
  that every `this`-based store has here. The `new` result's REG_NOALIAS base denies it: `NOMALLOC=1` (49 words: the other window
  ctors lose their late-store shape) and the class-scope `operator new` bound to `__builtin_new` on cDbgEditWindow (h4: 19/14/8 words;
  the asm label is ignored on a template member, the call becomes `__nw__t14cDbgEditWindow...`, and the stores `stw x; stw y; stw vt`
  move before `mr r3,name`) both show the target's edit-window `new` DID have the alias base. Left: 940 prio <= 7 needs the vt store
  941 without a link to the strlen call (impossible: sched_before_next_call on the call-free vt pseudo), or 943 >= 8 without NOALIAS
  removal. h2 (anchor after the ctor) 19/6/10, h3 (`"r"(wx)` input) 32/26/16. Flip order unchanged (t_esp_area first, then
  t_lightarea's 4 vtable-reloc words).
- **db_widget DB_STRING ctor 2w (`stfs ca` before `stw max; mr r3` in ours): the residue is the pass-12 launder's own sched2 cost.**
  `asm("" : "+r"(zero) : "m"(ca))` holds the zero's re-definition after the ca store 56 at sched1 (t=8, the local-alloc naming),
  but at sched2 the same true dependence lifts 56 to prio 14 over `stw max` 33 (13): t=7 lsu goes to 56 instead of 33. Any
  memory input raises its store the same way (`"m"(max)` 11 — 33 jumps to t=4, `"m"(type)` 8, `"m"(cr)` 3, `"m"(cg)` 4, `"m"(cb)` 5,
  ca+max 12), any register input raises its producer (`"r"(t)` with `u32 t = DB_PRIM_STRING; type = t` 10 — the `li 4` goes to
  t=4; `"f"(z)` 12; `"r"(this)` 9; no input 9 = the unheld naming). The asm's priority is >= 13 whenever its output feeds a
  pre-call store and >= 12 for any memory output before the `bl __builtin_vec_new` (flush link), so 56 >= 13 = 33's tie with
  the worse depend count. Moving the asm after the zero stores with a late-store output (`asm("" : "=m"(cr) : "r"(zero), "m"(ca))`
  6, cb 8, cg 7, ca 12) keeps the raise through the call link. Needed: a sched1-only hold on 56 (a dependence reload removes),
  none found in C/asm. 2w form kept.
- **game/db_cam menu 2w (closer 7 stands):** the y/z `lwz` tie at sched2 t=8 (prio 29/29, 13/13 dependents, LUID) needs a sched2-only
  dependent on y with priority >= 29 or one fewer on z; the Espgen43 pass-selective lever (`"r"(anti)` pinned to a register a LATER insn
  writes) only RAISES the asm that carries it (anti link asm -> writer), it cannot add a dependent to y without a read of r0 (live from
  entry: every earlier r0 temp moves) or a def of r0 between y and its store (excludes r0 from y). Not re-probed.
- **game/Espgen42/45 (9w/42w, pass 13 stands):** the missing construct is two sched1 insns of priority >= 80 ready at t7/t8 that emit
  nothing. The only sched1-only codeless insn known (t_camera_data closer 4 fact 3: a `"=m"(field)` anchor whose field is overwritten
  later in the block with no intervening aliasing load/call/volatile asm is deleted by flow2) needs its output-dependent store to be on
  the >= 79 chain; in Z every later store (`nrm[k].x +=`, `nk->z +=`, `nk->y *=`) is preceded by its own load of the same MEM and the
  bump `stbx`/fpmem stores are not nameable, so the anchor would carry the tail stores' priority (2-4). Not probed.
- **t_camera_data tcSetBesideOffset 27w:** GDBG order this tree: allocno 37 reg 153 (`i+1`, refs 4 len 24, 3333) -> pass1 r3; 38 reg
  207 (n*4 giv, refs 9 len 82, 3292) -> pass1 r3; 39 reg 205 (n*12 giv, refs 9 len 84, 3214) -> pass1 r31. The target has `i+1` and n*4
  in r31 and n*12 in r3, i.e. r3 was NOT free for reg 153 there: its loop-2 `ready + i*0x84` pointer is `add r3,r23,r9` (ours reg 132
  refs 5 len 18 pri 5555 -> pass0 r12, allocno 32, before the givs). So the question is not the giv pair's 2-insn length gap (closers
  2-4) but why reg 132 took r3 in the target's pass 0 (a `regs_used_so_far` register that does not conflict with 132 over loop 2) and
  r12 in ours; r3's only earlier use is the `tcCdatPtr` call/result (`mr r28,r3`). Not probed further (box spent on the read).
- Flags unchanged: Tools/t_esp_area, Tools/t_lightarea, t_esp/db_widget, game/db_cam, game/Espgen42, game/espgen45, t_camera/t_camera_data
  all stay False; no tree file edited; nothing built under the lock (kit only). Flip order for the Tools REL unchanged.

### GCC sweep pass 2 (the parked GCC residues re-read with the pass-6 doubling fact and the NOEQV oracle; IN PROGRESS 2026-09-12)
Scratch /tmp/gsweep2/ (`hv.sh HDR` = a dbg_tool.h variant judged on the four includers through variant.sh; h*.h header variants;
tea_*.log = LADBG/SCHDBG dumps of ToolEspArea). No tree file edited unless a line below says APPLIED.
- **Tools/t_esp_area ToolEspArea 7w (+4 reloc-name words), the r28/r29 ctor tie: NOT a doubling question.** `NOEQV=297` (the `li 4`
  pseudo) and `NOEQV=297,299` (+ the `work` lo_sum) = 11 words unchanged. Reason (local-alloc.c 926-931): update_equiv_regs doubles
  REG_LIVE_LENGTH with the comment "does not affect the priority in local-alloc"; block_alloc's QTY_CMP_PRI uses its own birth/death
  suids, and block_alloc never reads reg_equiv_*. The doubling fact only reaches global.c allocnos; this tie (q0 work refs 4 [4,52)
  1666 vs q2 `li 4` refs 2 [14,26) 1666, tie -> lower qty number) is a local qty pair. Sched1/sched2 tables re-read with SCHDBG
  (block 13): 940 `addi vt` pri 8 dep 3, 943 `stw x` pri 7 dep 2 (same cycle 6, priority order), every post-strlen store pri 4 w 0
  dep 1 -> LUID order in BOTH passes. New probe (h1: `numWork = n; pWork = work;` in the header ctor) flips the tie to the target's
  registers (work death 54 -> life 50 -> 1600 < 1666 -> `li 4` r29, work r28: e08-e64 all clean) but the final store order follows
  (sched2 LUID = the sched1 order): 6 words in t_esp_area (`stw r11,0x2c; stw r28,0x28` swapped + the 4 relocs... = 2 code words),
  t_lightarea 4 -> 6, t_event 0 -> 2. So the target's ctor needs pWork's store AFTER numWork's at sched1 and BEFORE it at sched2, or
  `stw x` before `addi vt` at sched1 (pass 1's priority read stands). Codeless anchors extending `work` (`asm("" : "=m"(rows|pWork|
  numWork) : "r"(work))` after the stores, h5-h8) are kept insns at sched2 too: 23-240 words in every includer. Not closed; nothing applied.
- **t_esp/db_widget DB_STRING ctor 2w (launder kept): not a doubling question either.** The ctor is ONE basic block, so every pseudo
  (this/max_/s included: LADBG q0 reg82 refs 18 -> r30, q2 reg83 -> r29, q1 reg84 -> r28) is a local qty; global.c allocates nothing,
  REG_LIVE_LENGTH is never read. `NOEQV=93` (zero), `=87` (type), `=85,86,90,93,87` (all four constants) on the plain form
  (/tmp/gsweep2/dbw_plain.cpp: no launder, `str = 0; len = 0;`) = 11 words each = the plain form's 11. Pass 8's arithmetic re-read
  off SCHDBG+LADBG (/tmp/gsweep2/sch.py LOG FUNC s1|s2 DUMP = issue table with patterns): sched1 issues the pri-12 stores dying-source
  first (printed `w` = INSN_REG_WEIGHT+1: vt l12, type l25, ca l32, len l37 at w 0) then the rest in LUID order; LC [12,16) 5000, vt
  tied hi+lo [14,26) 6666, type [24,28) 5000, zero [20,42) 1363. The target needs `stw vt` >= 2 insns later at sched1 (life 16 = 5000,
  LC wins the tie on qty number) AND zero above type (zero life <= 14 or type born <= suid 12 / stored last). No pass-6 lever reaches a
  one-block function (no loop header, no block-0 high, no fixed scalar load: the pool `lfs` is `mem/u`). Launder kept, 2w.
- **game/db_cam menu 2w (closer 7 stands, no pass-6 lever applies):** the y/z campos loads are block-local 2-ref qtys (LADBG z [30,40)
  2000 -> r8, y [34,42) 2500 -> r0 = the target's names), the tie is sched2's (prio 29/29, 13/13 dependents, LUID = the z-first
  sched1 stream). The doubling fact cannot touch a local qty; the raw-word (`*(u32*)&s->f`) store kind changes alias classes for
  BOTH loads alike (their earlier fixed-scalar neighbour `stw r0,ProjType` already gates y through the r0 anti at t=8, and every later
  store is already a dependent of both), so it cannot add a y-only dependent; a y-only dependent must be a `4(r9)` reference after the
  loads, which is closer 7's "fourth use of the lo_sum base -> y-first sched1 -> z takes r0". Not re-probed.
- **game/Espgen42 / espgen45 (9w / 42w unchanged; the xoris slot is not a pass-6 question, but the loop-B `nk` r30 PIN is a
  `floor(log2 refs)` step that the doubling fact points at):** the Z-block residue (pass 13: two codeless >= 80 insns at t7/t8) is a
  sched1 DAG question; none of the pass-6 facts adds sched1 insns (the raw-word store kind only changes alias classes of existing
  MEMs, the counters are already header-incremented, the block is not block 0). Re-read with GDBG on the UNPINNED tree
  (/tmp/gsweep2/e42_nopin.cpp, 28w): k = reg 91 refs 32 len 217 pri 7373 -> r30, the `&nrm[k]` pointer reg 518 refs 18 len 110
  6545 -> r28 (the target: pointer r30, k r28). k's 32 weighted refs (flow: 1 per ref at depth 0, 2 at the outer bodies, 3 inside;
  listed per insn by a 20-line script over the .sched dump: poke set+`k*4` 2, loop-A `k = i*(nx+1)` 2 + `k++` 4 + three giv inits 6,
  loop-B set 2 + two giv inits 4 + `k-nx`/`k+nx` 6 + `k++` 6) sit EXACTLY on the 2^5 step: one weighted ref fewer gives
  floor(log2)=4 -> 4*31/217 = 5714 < 6545 -> the pointer is allocated first. Probe p3 (the poke index as a separate local `kc`
  instead of k: refs 30, 5581) gives k r28 / pointer r30 WITHOUT the pin and the loop-B words = the tree's 9 (17w total: the poke
  block then differs — the target's poke index `lwz r28,0x64(r1)` IS k's register, so the vendor's poke used k). p1 (`k = i*(nx+1)+1`
  one statement, refs 28) drops k to r27 (29w). `int k = 0;` at the declaration is deleted as dead (no doubling reachable: k's first
  set is the fpmem load, no invariant note). So the pin's mechanism is one weighted k-ref (or len >= 245, or 3 more pointer refs
  / len <= 97) — the vendor's loop-A/B body has one k reference fewer than ours at the same code; candidates: the `k++` of loop A
  (`x2` = a set+use pair counted 4) written so that the increment reads a giv, or a `k*4`/`k*12` giv init that the vendor's loop.c
  did not create (a giv shared between `c += k` and `hA[k]`). Not closed; nothing applied. espgen45 carries the same pin (same fix).
- **Scope of the doubling fact, stated once (from local-alloc.c 926-950 + the four oracles above):** REG_LIVE_LENGTH doubling reaches
  ONLY global.c allocnos (multi-block pseudos or pseudos with REG_N_DEATHS != 1). A block-local qty's priority is block_alloc's own
  `refs*size/(death-birth)` over post-sched1 suids; nothing in block_alloc reads reg_equiv_*. So `NOEQV` is the right first test for a
  residue ONLY when GORDER lists the pseudo; for LADBG-listed ties (t_esp_area's work/`li 4`, db_widget's four constants, db_cam's
  y/z loads) it is a no-op by construction. Catalogue row 7 (global.c) keeps the pass-6 addendum; rows 6/8 (sched1 tie, local-alloc
  qty order) should say "REG_EQUIV does not enter".
- Flags unchanged: Tools/t_esp_area (7w+4 reloc), Tools/t_lightarea (4w, its vtable relocs into t_esp_area; unchanged since t_esp_area
  did not move), t_esp/db_widget (2w), game/db_cam (2w), game/Espgen42 (9w), game/espgen45 (42w) all False. No tree file edited (the
  `M config/G4BE08/objects.py` in the tree is another agent's CRI pass 65 block). Nothing built under the lock; every variant through
  the kit. Scratch /tmp/gsweep2 kept: hv.sh, sch.py, h1-h8.h, tea_*.log, dbw_plain.cpp + rtl_dbw_plain/, e42_nopin/p1-p5.cpp + logs.
- Next (not this pass): (1) Espgen42/45 — find the ONE weighted k-ref the vendor's body lacks (listing above); with it the r30 `nk`
  pin goes (17w unpinned includes the loop-A poke's 8 words that come from the `kc` stand-in, so the ref must be elsewhere) — read the
  loop-A `k++`/giv-init triple with `-dl`; (2) t_esp_area — the h1 swap shows the register question is exactly "pWork's store 2 suids
  later at sched1", so look for a sched1-only insn between the two dying stores that reload deletes (a same-hard-reg pseudo copy of
  `work` whose destination ties: `T* w2 = work; pWork = w2;` is folded by cse — try a copy that survives cse, e.g. through the `n`
  parameter path), keeping t_lightarea/t_event IDENTICAL via hv.sh.

### 2-word ties pass 1 (t_esp/db_widget Matching 112 -> 113/113 FLIPPED, t_esp.rel verify OK, 111 OK: DB_STRING ctor 2 -> 0w with four flow2-deleted `"=m"` anchors; game/db_cam menu 2w and lib/adx_tsvr nlp_trap_entry 2w below; 2026-09-12)
Scratch /tmp/ties/ (dbw/run.sh = variant + SCHDBG/LADBG summary, dbw/anch.sh = which asm anchors survive flow2 via `rtl.sh .. -dg -dw`, sch.py = the gsweep2 issue-table reader).
- **db_widget `DB_STRING::DB_STRING` 2 -> 0w (tagged, flipped).** Form (src/t_esp/db_widget.cpp): `u32 zero = 0; max = max_;` then four anchors
  `asm("" : "=m"(cr) : "r"(zero)); asm("" : "=m"(cg) : "r"(zero)); asm("" : "=m"(cb)); asm("" : "=m"(ca));` ABOVE `ca = cb = cg = cr = 0.0f; type = DB_PRIM_STRING;
  len = zero; str = (char*) zero;` (str the dying zero store). The pass-12 launder (`"+r"(zero) : "m"(ca)`) is gone. Also `primIdCounter` is now a
  non-static global: the original's `.data+0x780` object is scope global, and a `static` puts the section offset 0x0780 into the three `@l` fields of
  DB_PRIMITIVE's ctor where the REL has 0 (bytecmp resolves relocs by address and cannot see it; only `make_rel --verify` does: 6 bytes).
- **The lever = a sched1-only insn, read off flow.c:** a `"=m"(F)` anchor whose F is overwritten later in the block is dead for `insn_dead_p`
  (mem_set_list, rtx_equal_p on the MEM) unless something between it and the store (backward scan order) invalidates the entry. flow1 sees the
  SOURCE order: the pool `lfs` of the colour chain sits between the anchors and the colour stores, and a `mem/u (lo_sum ..)` read is NOT the
  `CONSTANT_POOL_ADDRESS_P (XEXP (x, 0))` case of mark_used_regs (that needs a bare SYMBOL_REF), so anti_dependence (param `this` ADDRESS vs pool
  symbol = may conflict) splices the entries out and the anchors survive flow1. sched1 issues the `lfs` (prio 14-16) at c5, ABOVE the prio-13
  anchors, so in flow2's scan (post-sched1 order) nothing sits between anchor and store: all four become NOTE_INSN_DELETED, sched2 never sees them.
  Tried and refuted on the way: (a) input-less anchors + a third `"=m"(id) : "r"(x)` asm placed after the zero stores (f8/g1-g4/h3) = the third asm
  survives (never overwritten) AND blocks the deletion of the first two (h1: the same third asm WITHOUT an input does not block; the flow.c reason
  was not found, recorded as an observation); (b) anchors reading `zero` alone (f1, 11w): `li zero` becomes prio 14 (anchor 13 + 1), beats
  `lis vt` at c4 (dep count 5 vs 2) and vt's life stays 12; the two extra input-less anchors (m3/m4/f10s/f11s, all IDENTICAL) pay that back.
- **Why it matches (SCHDBG/LADBG):** (A) LC-before-vt needs the vt store >= 15th issued (life 16 = 5000 = LC's, LC wins on qty number): the
  four prio-13 anchors (output-dep, cost 1, to their prio-12 colour stores) take issue slots before the prio-12 vptr store: vt [18,34). (B)
  zero-above-type: 2 anchor reads -> zero 5 refs, floor_log2 2 -> 2777 > type 2500 (type `li` 15th, store 20th) -> zero r0, type r9. (C) sched2
  order needs the sched1 (LUID) order ca < type < cr < cg < cb (the dep-count-5 group), str < len (dep-count-3 group): the chain's dying `ca`
  store, `type =` after the chain, `len = zero; str = zero` (str dying) give it. New fact for row 6: a store whose insn touches NO call-free pseudo
  (`stw max` = reg83 crosses the `new` call) is linked to the call by the call-address MEM scan as a TRUE dependence (prio 13), every store
  reading a call-free pseudo is in `sched_before_next_call` first and only gets the ANTI (prio 12) — that is why `stw max` leads the group.
- Zero-code shapes from today's rows, tested once each on the plain form (all 11w = the plain form; none reaches a one-block, `this`-relative
  store group): raw-word `*(u32*)&str = 0; *(u32*)&len = 0` (z1: no fixed scalar load exists, the pool load is mem/u and never depends on a
  store); `f32 c0 = 0.0f` after the first colour store (z2: cse1-only, sched1 stream identical); struct-member store through a pointer / `for`
  header / `static const char tag[]` / `(u8)` constants: no global, no loop, no string, no narrow mode in the ctor — not applicable.
### 2-word ties pass 2 (game/db_cam menu 2w: the `"r"(&campos)` codeless-ref position sweep is negative at every boundary, mechanism read; lib/adx_tsvr nlp_trap_entry 2w below; 2026-09-12)
Scratch /tmp/ties2/ (dbc/run.sh = variant + SCHDBG s1/s2 issue tables of the campos block, dbc/dep.py = y/z load uids and their sched2 dependents from the LOG_LINKS of the `.sched2` dump).
- **db_cam `debugCamera::menu` 2w (unchanged; the mandated position sweep `asm("" : "=m"(X) : "r"(&campos))`, X overwritten later):** before the
  campos memcpy (p0, X=roll) 43w; after it, inside the d0 block (p1) 39w; before memcpy(d1) (p3) 39w / X=`((u32*)d1)[1]` (q3) 37w / X=`pG->Cam.param.at.y`
  (q1) 39w; after memcpy(d1) (p4) 34w; before memcpy(d2) (p5) 34w / X=`((u32*)d2)[2]` (q5) 30w; after memcpy(d2) before FSet (p6, X=roll) 24w;
  after FSet (p6b) 24w; after the call, X=`pG->flags_60` (q7) 24w. flow2 (`rtl.sh -dw -dg`, asm_operands count greg vs flow2) deletes ONLY p6/p6b:
  the anchor's pG load is cse-shared with FSet's, so the two MEMs are rtx_equal; at every other position the later store's pG pseudo is a different
  reload (the u8* memcpy stores alias pG) or the memcpy MEM is `(plus d 4)` in another pseudo, or the call clears mem_set_list (q7) — the anchor
  survives into sched2 and costs a slot. Uniform mechanism, position-independent (LADBG b32 at p1/p4/p6): the base reg209's death moves from the z
  load to the anchor (refs 6 -> 7, death 34 -> 48/64/80 = the anchor's sched1 slot, it sinks to its output chain), so the z load loses its
  weight-0 (`INSN_REG_WEIGHT`: z kills the base, y does not) and sched1's y/z tie falls through to LUID = y first; then y [30,42) 1666 vs z [34,40)
  3333 -> z r0, y r8/r10 (the closer-7 world). **Correction to closer 7/GCC sweep 2: the current z-first sched1 order is the reg-weight rule
  (`rank_for_schedule`: smaller INSN_REG_WEIGHT first, before dependents/LUID), not LUID;** so the target's sched1 order is z-first iff the base
  dies at z, and every `"r"(base)` / `"m"(campos.*)` operand after the loads breaks it — the lever of catalogue row 7 (codeless ref = refs step)
  is not available on a block-local memcpy base whose death is the tie-breaker. sched2 dependents (dep.py on the base): y `470(A) 471(T) 472(A)
  481(A) 497(O) 499-501(A) 528-530(A) 557(A)`, z the same with 472(T)/471(A)/491(O) — 12 named each (SCHDBG 13/13), symmetric; the target-copy /
  up-copy stores are dependents of BOTH loads (different base register + const -> `memrefs_conflict_p` returns 1 for REG vs REG), so no C store
  can be a y-only dependent; the only y-only sched2 dependent would be a `(plus r9 4)`-addressed write = a `"+m"(campos.y)` asm, which is a base
  use in sched1 (above). Not closed; nothing applied. Left 2w.

### 2-word ties pass 3 (game/db_cam Matching 12 -> 13/13 FLIPPED, 111 OK: debugCamera::menu 2 -> 0w — the campos copy as three named words + a codeless reader of the y word ready one cycle behind the roll constant's `lfs`, kept alive by a second codeless anchor after FSet; the sched2 y/z tie broken by DEPENDENTS (14/13), the z-first sched1 stream and every register unchanged; 2026-09-12)
Scratch /tmp/dbcam3/ (run.sh NAME [LINE 'TEXT'..] = variant + SCHDBG/LADBG; sch.py LOG s1|s2 = compact issue table (uid/pri/w/dep/luid per cycle, negative weights included) + the block's LADBG in allocation order; vis.py DUMP = the sched dump's block visualization one line per cycle; rtl_*/ dumps; a1-a4, u1-u3, v1-v16 variants; kept).
- **Applied (src/game/db_cam.cpp menu; objects.py `# 2-word ties pass 3`):** `static union { Vec v; u32 w[3]; } campos`; the pos copy `u32 wx = campos.w[0]; wy = ..[1]; wz = ..[2]; *(u32*) d0 = wx; *(u32*) ((u32) d0 + 4) = wy; *(u32*) ((u32) d0 + 8) = wz;` (u3: RTL identical to the memcpy expansion, 2w), then tagged `asm("" : "=&r"(t) : "r"(wy), "f"(0.0f));` (u32 t at the else-block top) and after `FSet(roll, 0.0f)` tagged `asm("" : "=m"(ProjType) : "r"(t));`. Two tags, no pins. Negatives on the final shape: without the second anchor 2w (the first is flow-dead), `"=r"` instead of `"=&r"` 8w (local-alloc ties t to wy: the y word's life runs to the second anchor), without the `"f"(0.0f)` input 8w (the anchor lands at t13 ahead of the `lfs`), second anchor before FSet 7w (the up copy's pG reload/addi swap r9/r11), `register u32 t asm("r12")` pin instead of the pseudo = IDENTICAL too (not needed), `"f"(zf)` with `f32 zf = 0.0f` written at the copy = IDENTICAL too.
- **The word-copy equivalence (u1-u3), catalogue-grade:** `campos.w[k]` on a union static = `(mem/s:SI (plus base k) 0)` = move_by_pieces's source loads exactly (ARRAY_REF of an aggregate -> in_struct, alias set 0). Stores: `*(u32*) ((u32) d0 + k)` = a FLAGLESS `(mem:SI (plus r207 k) 0)` (expand_expr INDIRECT_REF sets in_struct only when the address tree is a PLUS_EXPR / the pointee is an aggregate; behind a NOP_EXPR cast the address is not a PLUS_EXPR) — alias-equivalent to memcpy's `mem/f` scalar stores because `fixed_scalar_and_varying_struct_p` only ever exempts a fixed SCALAR against a varying IN_STRUCT MEM: flagless and scalar both conflict with `[pG]` (the reload stays) and with the `mem/s` loads. `*(u32*) (d0 + k)` and `((u32*) d0)[k]` (k > 0) are `mem/s` (u1: pG not reloaded after them, 32w); `u32* p1 = (u32*) (d0 + 4); *p1 = wy` folds to `(plus r208 0x11c)` (cse's qty_const address replacement + fold) and loses the `addi r10,r7,0x118` base (u2: 84w). With this spelling a memcpy'd word becomes a C value that codeless asms can read — the missing lever of ties passes 1/2 ("such reads must name the y WORD of the memcpy expansion, which C cannot").
- **The mechanism, read off SCHDBG (v13):** sched1 unchanged (z t8 w0, y t9 w1, stores x/z/y at t10-12, the anchor at t14 slot 2 behind `lwz target.x`, LC50 high t9 s2 / `lfs` t13 as before); sched2 t8: y 465 pri 29 dep 15 LUID 251 issued before z 471 pri 29 dep 14 LUID 249 — rank_for_schedule's dependents key beats LUID; the target's `lwz r0,4(r9); lis r3; lwz r8,8(r9); addi r4` falls out with y r0 / z r8 (LADBG: y [34,48) 3 refs 2142 after LC50 high 2500 -> r11 and tied with d0 base 2142 (lower qty, r10) -> r0; z [30,40) 2000 -> r8). Every other slot of the block is the base's.
- **haifa facts fixed this pass (for the catalogue, rows 6/7):**
  * `insn_cost`: a consumer with INSN_CODE < 0 (any asm) sets LINK_COST_FREE on every incoming link -> cost 1 whatever the producer's latency; an asm as producer costs 1. So a codeless anchor is ready exactly one cycle after its LAST predecessor's issue, and "delay a load by making it depend on an anchor" only works if the anchor itself issues late. Store -> load true dependence costs 2 (`lwz target.x` queued 2 cycles behind `stw y`), anti/output cost 0 -> LINK_COST_FREE -> 1; the lsu issues one load/store per cycle (the LC50 `lfs` ready at t10 waits behind the three stores until t13).
  * An anchor's priority is 1 + its highest dependent, and every insn it depends on inherits priority >= that: an anchor with `"r"(d0)` before the loads pulls `addi d0` from t7 to t4 and the pG load to t2 (a2/a3 62w); `"r"(this)` lands at t5 (a1 41w); a destination-field anchor `"=m"(((Vec*) d0)->z)` is unknown-base and gates ALL three campos loads (a4 32w, y t10). The brief's offset-disjoint pre-load anchors therefore cannot delay only z: they issue as soon as ready and z was never readiness-bound (it issues at t8 by ranking). Read the s1 table before placing a filler: fillers work when their input is produced late by an insn that is itself pinned by dependences (Espgen 16's jx), not when the input is an early address.
  * `sched_analyze_1` analyses a SET's destination BEFORE its source and `sched_analyze_2` skips a MEM read's true dependence when a link to that insn already exists (`find_insn_list`): a `"=m"(X)` anchor whose X conflicts with the pending stores records OUTPUT links (cost 1) and its `"m"` inputs add nothing (v3/v4). A PARALLEL is analysed last-element-first, so with two outputs the LAST output's SET is seen first (v6: `"=m", "=r"` order gives true links, `"=r", "=m"` does not) — still cost 1 for an asm consumer, so no delay either way.
  * local-alloc block_alloc ties an asm's output (pseudo or hard reg) to its first dying REG input unless the output constraint is `=&` (recog_constraints[0][1] != '&'): `"=r"(t) : "r"(wy)` gives wy the `sugg r12` (v8 54w, y -> r12) or one qty with t (v14 8w); `"=&r"` is the codeless spelling of "an output that must not share the input's register".
  * Fake-death overlap (the "+-1 insn" quirk, measured): the up copy's `addi r11 = pG + 0x138` [70,76) took r11 in the base ONLY because the FSet pG reload [78,80) already held r9 and the qty death is extended one insn; a codeless insn inserted between the last up store and that reload (v9's second anchor before FSet, 7w) un-overlaps them and the addi takes r9. Put late anchors after the insn whose qty they would otherwise separate.
  * `f32` constant as an asm `"f"` input shares the pool load with the later `d = 0.0f` store (one `lfs`, r236 refs 2 -> 3, f0 unchanged); its priority rises by the anchor's chain (15 -> 16 here) — check the high's slot (must stay below the next iu competitor, 514 p20 at t8 s2) before using a pool load as the readiness gate.
- Catalogue row 6 (sched1 tie) addendum to write: when the two tied insns are LOADS whose registers are already right and only the sched2 order is wrong, add a sched2 dependent to the one the target issues first — a codeless `"=&r"(t) : "r"(word), "f"(late-value)` reader of that value, its readiness gated by an input produced in the cycle before the first free slot, kept alive by a second `"=m"(fixed scalar) : "r"(t)` anchor placed after the next block-local qty pair; needs the value nameable in C (the word-copy spelling above for memcpy'd structs).
- Flip: `"game/db_cam.cpp": True` (objects.py `# 2-word ties pass 3`), locked `ninja -k 0`, `build/G4BE08/main.dol` built, `dtk shasum -c` 111 OK, `git diff config/G4BE08/symbols.txt` empty. Tree edits: src/game/db_cam.cpp (menu: the campos union, the copy block, two tagged asms), config/G4BE08/objects.py, this section. Kit untouched; /tmp/dbcam3 kept.
