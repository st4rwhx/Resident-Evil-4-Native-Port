### Stage rooms, third pass (cSceObj.cpp 17/18; r220, r40c, r20a Matching; r40f 8/9, r218 5/8, 2026-09)

- `src/st/cSceObj.cpp` + `include/cSceObj.h` (0xF8-byte mover, no vtable: `move` dispatches through a
  *local* `int (cSceObj::*tbl[3])()` copied from its `.rodata` template; a class without virtuals makes
  g++ 2.95 call the PMF without the index test). `accFrame/cstFrame/decFrame/cnt/frame` are `u32`
  (2^52 magic without `xoris`, `fixuns_truncsfsi2` with the 2^31 compare). Idioms found there:
  - `MTX_COPY` inside an inlined member: `MtxPtr d_ = (dst); MtxPtr s_ = (src);` (both initialised at
    the declaration) gives the target's `addi r7,s; addi r8,d` giv registers; sce_at.cpp's
    `d_ = (dst)` assignment form swaps them here.
  - Inline helpers whose frame is one `Vec d`: `&d` is the inline frame base (`(plus fp N)` at offset
    0), so `PSVECSubtract(target, .., &d)` gets `addi r5,r1,N; mr r27,r5` (arg first, copy after) while a
    hand-written `Vec d` local of the caller precomputes a pseudo (`addi r27; mr r5,r27`).
  - A second `if (obj)` block right after a first one is jump-threaded (`beq` straight to the end)
    unless something sits between the label and the compare: `cModel* o = obj; if (o) {...}` in the
    second block (cSceObj moveTo) keeps the target's `beq` to the second test AND stops gcse from
    PRE-copying `&d` (`mr r25,r27`) — the same frame slot `(plus r31,0x28)` is one gcse expression
    across `case 0`'s `Vec pos` and `case 6`'s inline `Vec d`.
  - `step = mode = 0;` (chain) then `frame = n;` gives `stb 0; stw 1c; stb 2` (setMove1_all); separate
    statements put `stb 2` before `stb 0`.
  - The in-class ctor zeroes 46 floats: source order = member order but `srcPos, dstPos, srcRot,
    dstRot` (src before dst); the target's `stfs` stream is RTL order except that the last (dying)
    store jumps forward to where haifa's pending-memory flush (32 refs) splits the block (18th in
    r220, 25th in r40c) — no source change needed, ours reproduces it. The `sub[i] = NULL` loop is
    the LAST statement (forward `bdnz` with `u32 i`, the `mtctr` set up among the stores).
  - OPEN: setMove1_all issues `lfs f0,0.01` before the two `Vec` struct copies (target after them,
    `lis` stays early); 20 source forms (chains, FSet, locals, do-while, copies first) tried.
- Rooms with a stack `cSceObj` (r220 moveElevator, r40c): `int evt = 1;` declared BEFORE the
  `cSceObj elv;` is compared after the ctor's loop label → survives as `li r0,1; cmpwi cr3,r0,1` (cse
  cannot see across the label, cprop cannot substitute into `cmpwi`); `int up = dir == 0;` there is
  `subfic; adde`. A loop counter reused by two loops (the `sub[]` search and the 90-frame loop) is
  globally allocated (r31): give the call-free search its own `u32 n` (r11).
- Range tests: `type >= 0x13 && type <= 0x14 && lid && box` folds to `subi/cmplwi 1`; `if (type <=
  0x14) if (type >= 0x13)` gets `cmpwi 0x12; ble` (fold's `X >= C` → `X > C-1`); the target's
  `cmpwi cr2,0x14; bgt; cmpwi 0x13; blt` is a `switch (type) { case 0x13: case 0x14: ... }` (cr2 kept
  for the second switch on the same value). Two zero-tests of one flag word (`(f & A) == 0 && (f & B)
  == 0`) fold to one mask: `static inline u32 flagBit(u32 f, u32 bit) { return f & bit; }` and
  `flagBit(f, A) == 0 && flagBit(f, B) == 0` keeps the two `andi./andis.` (r220 initElevator).
- `if (SceMesGetSelection() != 1) { Comeback; SceEventEnd; SceExit; } else {...}` lays the fail arm
  out first (r40f). `while (em->ckGoto()) SceSleep(1);` = `b test; L: bl SceSleep; test: ...; li r3,1;
  bne L`. Countdown `for (i = 0; i < 20; i++) SceSleep(1);` = `li 0x14; L: ..; subic.; bne`.
- `SceMesCamSndSet` is called by the st2/st4 rooms with a 4th argument (`li r6,4`) the DOL's
  3-parameter definition never reads: `SceMesCamSndSet4(...) asm("SceMesCamSndSet")` in st_room.h.
- TexRender object setup (r20a): `x136 = 2; x137 = 8; x138 = 0x20; alpha = 0.7f;` with alpha LAST
  gives the target's `stfs alpha; stb 136; stb 137; stb 138` (the dying last store goes first).
- `const f32 ry = PI;` in the mid-block declarations plus `f32 y = ry;` right before the call whose
  successor stores it: the `lis PI@ha` lands at the function top (r18) and `lfs f31` before the call
  (r20a CarryOnShoulder); `f32 ry = PI;` alone loads at its declaration, `const` alone folds to a
  literal at the use. `Vec* pa = &ang; pa->y = y;` with `ang.x/z` direct gives `stfs f31,4(r21)`.
- Template-copied Vecs and a `Vec rot = {0,0,0}; rot.y = -PI;` declared mid-block after the
  `setNoSuspend` calls (frame slots still in declaration order: the whole declaration group moves).
- cEmDoor `setCloseLock`: the room-side `cEmDoorSetCloseLock(cEm*) asm("setCloseLock__7cEmDoori")`
  alias (r101/r105/r411) is needed in r20a too. `getRoomEtcDoor(no, &work->door, 1) == 0` then
  `work->door = NULL` stores r3 (cse knows it is 0).
- Death bits of the loaded enemy list: `int list = pG->emlist_no; if (list >= 0) v = *(u32*)((list <<
  5) + (u32) pG + 0x501C) & (0x80000000 >> (no & 31)); else v = 0;` (r218, like r101's emDeadClear);
  `while (1) { if (a) { if (b) break; } SceSleep(1); }` stays un-rotated (the `while (!(a && b))`
  form is rotated).
- OPEN (r40f BombSet): the first of two `Vec p = {..}` template copies loads its words 0,4,8 in the
  target and 0,8,4 in ours (the r10c/r22a "second word pair" OPEN item; the second copy is 8-then-4
  in both). OPEN (r218, = r108 openCover tail): after each `do { pos.y += spd; if (..) break;
  SceSleep(1); } while (1)` the target re-materialises `lis work@ha` and the 2500.0 constant into
  fresh registers where ours reuses the loop's hoisted r28/f31; `cObj* o28/o29` get r31/r30 swapped.

### REL near-miss sweep (cSceObj x3, Tools/t_util Matching; r119 26/27, em3a 40/42, t_camera_data .rodata; 2026-09)

- A constant-pool `lfs` the original issues AFTER struct stores through `this` (cSceObj setMove1_all's
  0.01 after the two Vec copies): pool loads are `RTX_UNCHANGING_P` and never depend on stores, so the
  constant is a function-local `static const f32 rate = 0.01f;` (emitted at its declaration = exactly
  where the function's pool would be) read ONCE through a `const f32&` inline (`FCRef`) into a local:
  the reference read is a MEM with neither the struct nor the scalar flag and `true_dependence` keeps
  it below the stores; a direct use folds to the literal, two reference reads reload between the stores.
- Stores whose following `pG` load must stay below them (r119 `sat[i] = SatMgr.create(..)`): a typed
  `PSetSat(cSat*&, cSat*)` reference store with the call as the argument expression (the work pointer
  is still reloaded after the call, r3 stored directly). The pG load position moves the two `addi`
  table-address uses next to each other, which is what breaks the global-alloc live-length tie of the
  pos/rot `lis` pseudos (pairs 1/2 were tied at 146/162 insns and fell to pseudo order).
- Register-order lever: a loop counter and a strength-reduced giv with the same refs/lifetime are
  allocated in pseudo order (counter first -> higher register); one more reference to the counter
  (`if (best == -1) best = j; else if (..) best = j;` — two statements that jump2 cross-jumps back into
  one `mr`) makes it 5 refs vs 4 and wins the register (t_util TutilGet3DPosXZ j = r10, j*4 = r8).
- A `pG` read whose `lis pG@ha` the target issues after a preceding member store (em3aPatrolInit
  `w->pRoute = 0; if (pG->pRoomEmi == 0)`): read `pG` through a reference inline (`GRef(pG)->x`); the
  load then depends on the store and the store outranks the `lis` in sched1.
- `.rodata` string order with two literals in one function in reverse code order (t_camera_data
  "B404"/"EMPT"): `const char* const tag = "B404";` at the declarations parses the string first and
  folds to the literal at the use (a non-const `const char*` local is kept in a register).
- Sched1 register-weight rule, loads: `INSN_REG_WEIGHT` is +1 per SET (stores included) minus 1 per
  death, so in a 12-byte template copy the word-8 load (last use of the `addi` base) has weight 0 and
  is issued before the word-4 load (+1); sched2 (post-reload, no weights) only re-sorts through the
  hard-register anti-dependence chains. The original issues 0,4,8 in r22a RopeMove / r40f BombSet
  copy 1 and 0,8,4 in BombSet copy 2, so its tie-break is not this rule (OPEN, no source lever).
- Local-alloc FPR order of constant temps (r40a first_init, r119 third SetTree block): qty priority
  is refs*size/length with ties by qty number (assigned scanning the block BACKWARDS, so the later-
  dying temp wins ties); f0 goes to the first allocated. Our sched1 issues the loads in LUID order
  (x, y, addi, z), the original's allocation says z's range was not the shortest. `asm("" : "+f"(px))`
  (two deaths -> global alloc -> f0) reproduces r40a's bytes but is not a documented COMPILER-DIFF.
- cse1 path structure (r120 R120Event): our cse falls through into both `if (!(flags & 0x10))` event
  bodies (their `high(EvtMgr)`/string highs become r29/r30) and the tail after the outer `if` starts a
  new ebb whose PRE copy is rematerialised (`lis r9, pG@ha`); the original skipped both bodies
  (fresh `lis` in each) and carried `high(pG)` into the tail (r31). Nested and two-`if` forms compile
  identically (jump threading). OPEN.
- t_mv mvInit: the then arm's `cursor = 0` literal always finds the SImode `zero` pseudo (or the
  known-zero loaded byte) through cse `src_related`; the `asm("" : "+r"(c))` launder becomes
  `mr r10, r29` (cse substitutes r29 into the asm input). Not a launder case.
- Tool: `mcmp.py`-style masked compare (functions aligned by normalised name, reloc fields masked on
  both sides, same-section branches resolved by target) plus `perm.py`/`variants.py` (statement
  permutations / listed variants between two marker comments, ~0.35 s per variant) is the loop that
  found the r119/t_util/em3a/t_camera_data fixes; keep them in a private /tmp dir.

### Stage rooms, st2_2/st2_3 last-nine pass (r21d 31/33, r213 27/30, r223 28/29, r227 26/30, r214 15/25 written; sections identical; 2026-09)
- Units: src/st2/r21d.cpp, r213.cpp, r223.cpp, r227.cpp, r214.cpp (all with the `RxxxWorkPtr { p; }`
  struct-member work pointer). The `fn_<mod>_XXXX` 0x3B8 block mcmp reports MISSING is the cLight
  linkonce copy `place_linkonce_module` inserts (bytes present in the .o, nameless); ignore it. r212,
  r221, r226, r22c not started.
- `while (1)` vs `for (;;)` differ in GCC 2.95: a `for (;;) { ..; if (c) break; SceSleep(1); }` whose
  body starts with a conditional block gets rotated (`b body; L: SceSleep; body: ..; ble L`), `while
  (1)` keeps `..; bgt exit; SceSleep; b top` (r227 checkBox0Fall, setEmOnElv2 -- the latter also gained
  the target's `&local` copies `mr r23,r28` once written as `while (1)` with `continue`s).
- A flag variable cleared after a call (`SndStrReq(..); on = 0;` instead of before it) swaps which of
  two hoisted compares (`cmpwi cr7/cr6, on, 1/0`) gets cr7 (r223 StrCheck): the compare pseudo's
  post-sched live length decides.
- `if (RsfCheck(..) == 0) return 0; return 1;` gives `li r3,0; andis.; beq; li r3,1` (r223 isZouenGo2);
  `int v = 0; if (..) v = 1; return v;` keeps the value live across the call in r31.
- Store-order of `SetPos`/`SetAng` with literal coordinates where the object comes from a call: the
  target calls `SmdGetObjPtr` BEFORE the stores -- a block macro `{ cModel* m_ = (o); v.x = X; v.y = Y;
  v.z = Z; m_->setPos(&v); }` on the caller's Vec (r223 SET_POS_XYZ); an inline taking `Vec*` keeps
  the Vec address in a register instead (`addi r31,r1,N; mr r4,r31`).
- A loop test written `for (i = 0; (u32) i < 4; i++) { if (i == 3 || em[i].ckFindPL() == 1) break; ..}`
  keeps `cmplwi 3; cmpwi cr7 3; bgt; beq cr7` (r223 EmCheck); `(u32) i < 4 && i != 3` in the condition
  is range-folded to `cmplwi 2`.
- `u32 timer;` declared at the top but `timer = 0;` written right before the `for (;;)` puts the `li
  r30,0` after the init loop (r223 EmCheck); the r27 = timer+1 copy across the inner loop is automatic.
- Two `case` arms with identical bodies (`case 0: A; break; case 1: A; break; default: B;`) reproduce
  `cmpwi 0; beq A; cmpwi 1; bne B` when the bodies share one function-scope `void* mod` (identical
  stack slot -> cross-jumped); `case 0: case 1:` gives the range test `cmpwi 1; bgt; cmpwi 0; blt` (r227
  Evt_R227S02_Func). An extra `case 0: break;` in a funcMode switch adds the target's `ble end`.
- `R227Work*& wp = r227_work.p; wp = MEM_CALLOC(..)` keeps the following `lwz pG` below the work store
  (r223/r227 Init, the r21d idiom); `BitOn(obj->be_flag, 0x20)` before a `pG->room_id_prev` test keeps
  that pG load below the store (r227 initGondola).
- `cEmWrap e0; e1; ..` declared AFTER the leading `SceSleep`/`RsfSet`/`setEm` statements: the ctor calls
  follow them (C++ mid-block declarations, r227 setEm2/setEm3/setEm3_after/setEmOnElv1).
- Const-folded locals: `const f32 step = 10.0f; const f32 power = 20.0f;` declared where the pool wants
  them fold into their uses (fresh `lfs` per QuakeExec call, the step hoisted by loop.c) while fixing the
  pool order (r227 operateElv). A `static const Vec` INSIDE a function is output before that function's
  pool and copied at the use site (`v = gotoPos;`, r214 execCatapult); a file-scope `static const` table
  is output after the strings at the end (r214 r214_catTbl / r214_rockOfs).
- `int i` (signed) loop counters used only as array indices are reversed by loop.c into `subic.; bne`
  (r214 checkCatapult, throwRock `for (i = 10; i > 0; i--)`); `u32 i` is not (`cmplwi; ble`). A
  `do { .. d++; i++; } while (d <= &tbl[2]);` pointer walk keeps the pool `lfs` inside the loop and the
  `mr r29, r26` table copy (r214 initCatapult).
- Room-side `class cEmWrapD : public cEmWrap { ~cEmWrapD() {} };` for local `cEmWrap x[4]` arrays: the
  target has the ctor loop AND an empty dtor loop (`cmpw; beq; L: subi 0xc; cmpw; bne`, r214
  checkEmReset).
- Store order inside switch arms follows the "dying-first" rule the other way round from source order:
  `rockReady = 0; step = 2;` gives `stw step; stb rockReady` (r214 cCatapult214::move, three arms).
- COMPILER-DIFF 1 also shows in a *definition's* prologue (r223 reva_common_move: `fmr f28,f1; fmr f29,f2`
  before `mr r29,r5`); an asm-labelled definition cannot be assembled, so it stays a residual.
- Old prototypes the rooms were built against: `cEmRack::setBreak(Vec*)` (r227: `void
  cEmRackSetBreakV(cEmRack*, Vec*) asm("setBreak__7cEmRack")`), `SceAtDataSet_exec` 5th arg `(void*) 1`.
- Residual shapes left: fp-register order of 4 hoisted loop constants when pool order and priority order
  conflict (r227 checkBox0/1Fall, 14 words each); `&cMes.getWork()` pointer tied to r31 across the call
  (r227 operateElv, 2 words); `rot.x/y/z` literal store order z,y,x with the y literal first in the pool
  (r227 execGondola, 2 words); Init's inline `Vec*` param vs local Vec address (r213); FadeSetW register
  pair (r21d moveFence); PRE'd `&local` used through `mr r4,r31`/`4(r31)` stores in r214 execCatapult.
- `.bss` emission order: function-local statics (text order) -> objects needing construction (at
  finish_file) -> deferred plain file-scope statics/publics in definition order; `= 0` scalars go to `.data`.
- `global constructors keyed to X`: X is the first PUBLIC function emitted; a header prototype makes a
  `static`-defined function public -- remove it to move the key.
- Zero-aggregate initializer -> builtin memset libcall with `crclr cr1eq`; a prototyped `memset()` call
  never gets the builtin.
- `int off = !(flag & 1); if (off)` -> `xori; andi.; bne` (TRUTH_NOT as value).
- `(f32)(int)u8val` gives the lfd/0x4330 path instead of the `psq_l` fast-cast; `(u32)t` from float
  gives the `fcmpu 2^31 / bso / xoris` unsigned fixup.
- Post-increment in a loop condition (`i++ < n - 1`) yields `mr r0,rI; cmpw; addi rI` with `n-1`
  hoisted; `++i < n` yields the in-place biv compare.


### Stage rooms, last-four pass (r212 Matching; r221 31/38 written, sections identical; r22c .data identical, 9/48 written; r226 not started; 2026-09)
- r212 (src/st2/r212.cpp, st2_2, MATCHING): a room-local class driven by a member-function table
  (`static void (cR212Door::*tbl[3])() = {&wait, &open, &close}` in `.data`: `{0, 0xFFFF, ptr}` PMF
  entries, called `(this->*tbl[mode])()` — a class without virtuals gets no index test); its
  `cEmGanado::setDrill` pointer copied into a call-clobbered `mr r8,r3` = ONE function-scope
  `cEmGanado* g` assigned in both `if` blocks (a per-block local uses r3 directly).
- `PSetSat(work->p, EatMgr.create(...))` with the call INSIDE the reference setter's argument hoists
  the `lis work@ha` into a callee-saved register across the call; `cSat* e = create(); PSetSat(work->p, e)`
  keeps the target's fresh `lis r11` after the call (TrapInit eat0). The later stores of the same
  function want the call-inside form (sat[0..3], sat2, eat, eat2).
- A template-copied `Vec pos` at frame offset 0 passed to two `setPos` calls in later blocks: the
  target's `mr r28,r11` (copy of the block-move address pseudo) + `mr r4,r28` per call comes from a
  block-local `Vec* pp = &pos;` declared in EACH block (gcse PRE hoists the redundant `(plus fp 8)`
  into a copy after the template copy); one function-scope `Vec* pp` is merged by cse/cprop into the
  block-move pseudo and a bare `&pos` recomputes `addi r4,r1,8` per call (r212 EventTrap).
- A `u32 cnt` whose `li` the target issues after the template copy's `addi r9` but before its loads:
  declare `u32 cnt;` at the top and assign `cnt = 0;` right before the loop (RoofMove); an
  initialised declaration is scheduled among the copy's loads.
- `(f32)` GPR constant: `f32 ry = -2.68f;` declared mid-block (after the module swap) lands as
  `lwz r29, LC` before the first setPos and is stored with `stw` (SF constant in a GPR); declared at
  the top it becomes an FPR (`lfs f30`) at the prologue (EventTrap); an FPR the target keeps across a
  call (`lfs f30` before `setPos`, stored after) is `f32 ry = 3.09f;` declared right BEFORE the call
  and after the Vec stores that precede it (DrillAppearCheckEndProc; the pool order follows).
- `(u8) prm[i][k]` of an `int prm[4][3]` template array passed to an int parameter gives the target's
  `lbz +3; clrlwi 24` pair by itself in the first loop (giv pointer `&prm[i][k]+3`), plain `lbz 3(rP)`
  in the second; `u8` locals and asm launders both lose the mask (RoofTrapWatcher).
- `(no >> 5) << 2` with an `int no` parameter of an inline is `srawi; slwi`; the target's
  `rlwinm 29,3,29` needs `u32 no` (event flag / door flag helpers).
- Byte pair `mode`/`step` stores after a word store: the target issues `stb step; stb mode` from the
  source order `mode = ..; step = ..;` (last dying store first: setOpen/setClose/setOpened/setClosed).
- `EmSeCall` in the rooms passes the position in the SECOND slot: `EmSeCallP(int no, Vec* pos, int id,
  ...) asm("EmSeCall__FiiP3VeciiP5cUnit")` (the DOL definition forwards it there).
- r221 (src/st2/r221.cpp, st2_3, not flipped): stack `cSceObj` elevator like r220; `switch ((u32) dir)`
  with cases 0/1 for the `cmpwi 1; beq; cmplwi 1; bge` dir tree (OPEN: ours emits a linear
  `cmpwi 0/1` tree and folds the `(Vec*) down` argument to `li r5,0` where the target keeps `mr r5,r31`
  — its `cmpwi down,0` is hoisted to the top and spilled `mfcr`, so cse never sees the compare).
  `Vec a = {3277.0f, o->pos.y, 4196.0f}` (a MEM element) builds the constructor in a freed temp slot
  (memset + 3 `stfs`) and copies it into the variable; `{0.0f, dy, 0.0f}` with a REG `dy` builds in
  place — the target of initShutter has both the temp copy AND the difference computed once before
  the memset (OPEN, 6 forms tried). Float constants: `-407.00006f` (c3cb8002), `0.15280247f`,
  `5.471593f`, `3.1518683f`, `-1793.51f`, `-1410.2101f`, `-1350.1799f` (read pool words, do not
  round). `ScePrim* t = SceExec(...); PSetPrim(work->x, t);` = the eat0 idiom for task handles.
  `emDeadWords(list)[4]` (inline returning `(u32*)((list << 5) + (u32) pG + 0x501C)`) keeps the
  target's `addi 0x501c; lwz 0x10(r9)`; the folded `lwz 0x502c` comes from the flat expression.
  OPEN: checkElevatorArrive (-8: the `k+1`/`7200*k/7` giv shapes), checkBossAppear_end / throwBonbe
  (r21/r22 global-alloc of `eff2` vs `pG@ha`), initShutter, moveElevator.
- r22c (src/st2/r22c.cpp, st2_4, partial): `.data` is 263 objects generated from the split object —
  every target record is its own `static s32 name[]` array of varying length and the level tables
  are `static s32* lvl[] = {(s32*) count, (s32*) time, rec, ...}`; `.data` needs `.balign 8` at the
  end. The level names are a `static const char* [5]` in `.data` right after the master table (its
  strings follow the r22c.cpp filename in `.rodata`), the routine table `void (*[5])()` and a
  `const char* = "START"` variable come after itemSave (string order), then a record, `int = 4`, and
  the `char fname[] = "SS/___/id22c.dat"` the code patches. 9 of 48 functions identical; the shooting
  game (getBonus .. r22c_checkShootingScore, ResultScreen, Score*) is NOT written (static stubs
  keep the table relocations; replace them).
- GCC 2.95 memory disambiguation (confirmed): a struct-member store never blocks a scalar global load
  (`pG`); a store through a reference parameter (`PSet`/`BitOn16`/`U16Set`) or a scalar pointer deref
  blocks everything -- choose per site from whether the target reloads `pG` after the store.
- `*(volatile u16*) &at->flags |= m` keeps `addi rX,obj,0x2b4; lhz/sth 0x1a(rX)` AND keeps the following
  `pG` load below it (`AtariFlagsOr`).
- Among independent stores sharing one value register, the later use is issued early and the earlier
  use sinks before the call: to make a store land last, write it first.
- `int md = 1; obj->wep.mode = md;` gives an SImode pseudo cse reuses for a later `x = 1` store in the
  same ebb; a QImode constant is not reused for a later SImode store.
- Identical if/else arms calling the same function with constant args are not cross-jumped -- write
  them duplicated.
- HAZARD: two agents editing config/G4BE08/modules.py concurrently -- one read-modify-write dropped 14
  MATCHING entries (a446c30). Re-read the file immediately before editing and re-verify RELs after.


### Stage rooms, r22c shooting-game pass (st2_4/r22c 48/50 byte-identical, sections identical, not flipped; 2026-09-10)
- src/st2/r22c.cpp is complete (all 48 functions + the cManager<cObj>/<cEm> copies). The work pointer is
  the one-member struct `R22cWorkPtr r22c_work` (every `r22c_work.p->x = v` reloads the pointer, as the
  target does after each store); the previous plain `static R22cWork*` was why 9 "matched" functions
  and checkBottleCap/getBottleCap did not match. Work layout: step/resultStep u8 at 0/1, timer 4, time 8,
  hits C, score 10, total 14, ageSum 18, level 1C, state 20, combo 24, pause 28, ufoWait 2C, shotTotal/
  shotHit 30/34, eat 38, wepSel 3C, effFlags 40, wepNo/wepType/cnt46/cnt47 44..47, tbl 4C, itemSaveBuf 50,
  itemSel 54, cap[24] 58, capId B8, door[2] BC, strId C4, wepMan C8, effTimer CC, itemNum[24] D0,
  ResultScreen {state, data} 130, IDSystem 138, scoreTimer[8] 188 (0x1A8 total).
- Linkage: getBonus, r22c_checkGameLevel, r22c_checkGame, countMark, funcUfo (static), deleteAllMark,
  scoreRegist, setWepmanKilled, ScoreClear, ScoreMove are `extern "C"` (the .sym shows them without
  `()`); ResultScreen::{read,reloadtime,highscore(int),init,move(int),quit} are C++ members, renamed by
  the sync (`move__12ResultScreeni` etc.).
- `.rodata`/`.data` order: `static const char* r22c_levelName[5]` must be DEFINED AFTER R22cInit (its
  "-","A".. strings follow the r22c.cpp file-name string emitted by R22cInit's MEM_CALLOC); the routine
  table + `static const char* r22c_startMsg = "START"` sit between shootInit and shootReady (the START
  string follows shootInit's 0.0 pool); `static const int r22c_scoreTbl[7][5]` (file scope) lands after
  the cManager strings.
- Idioms found: `int id = tbl[k]` local before `ItemMgr.num((u16)(id + 0xDB))` keeps `lwzx` (the inline
  expression narrows the load to `lhz +2`); a `for (i = 0, n = 0; ...)` comma init gives `li i; li n`
  in that order (separate `n = 0;` before the loop puts it first); `pG->x8330` high scores are read
  through a `struct { u8 pad[0x8330]; s16 score[4]; }` view of pG (`lhax base,idx` with the record
  base first; the `(s16*)&pG->x8330` cast gives idx-first `add`); `return RsfCheck(..) == 0` is the
  `and.; mfcr; extrwi` store flag; an if/else on `(int) flags_174 < 0` whose else arm is one call that
  also ends a switch case is `goto` into that case body (COMPILER-DIFF 6 shape, r22c_talkWepMan);
  `switch (d->flag) { default: ... case 0xFE: ... case 0xFD: }` (default first) for the linear
  `cmpwi FD; beq; cmpwi FE; beq` chain with the default body laid out first (shootMain); the cap-total
  digit display accumulates into a separate `sum` and copies it (`mr r31, r30`) before the digit
  code (ResultScreen::move); ScoreSet's leading-zero count is a goto loop over `int* p = &d[3]` with
  `*--p` (`lwzu -4`) and `n = 1;` written before the pointer init; the digit array is walked through
  `int* d = digit` (`lwz 0xc(r7)` instead of the frame-relative form); loop-local `IdUnit* du` per
  display loop keeps the call result in r3 (a reused function-scope `u` gets a callee-saved copy);
  `(w->level == 2 || w->level == 3) && w->state == 2 || (w->level == 4 && w->state == 1) ||
  (w->level == 4 && w->state == 2)` as a MACRO (an inline returning it materialises `li/cmpwi`) gives
  the range fold on level and the two separate state compares (cse deletes the repeated level test);
  `flagBit(f, 1) && !(f & 2)` keeps two `andi.` where `(f & 1) && !(f & 2)` folds to `clrlwi 30; cmpwi 1`.
- Residuals: r22cGateCtrl (18 words: the target schedules `li open,0` before the first `init` call and
  the loop-invariant `lis` fillers one call earlier each, so `pG@ha` gets its own r24; every placement
  of `open = 0`, lim/spd/type forms tried); ResultScreen::highscore (10 words: `score` param r28 <->
  loop `IdSys@ha` r31 global-alloc order; `on` reusing `score` gives the target's r28 for the flag).

### Stage rooms, st1 r100/r117/r11c pass (r100 Matching; r117 20/21, r11c 21/23; r103/r104/r108/r118/r11d/r11e/r113/r120 residues and sections; 2026-09-10)

- r100 (st1_0, 37/37 byte-identical, sections identical, Matching; st1_0.rel byte-identical). r117 (st1_3,
  20/21, sections identical), r11c (st1_3, 21/23 real: closeGate is a #9 loop, EventBesiegedStart differs
  only in the `cManager<cEm>::create` reloc name and the two template instances are name-only) written.
- REL `.data`/`.rodata` alignment: the split object's section alignment must match ours or every later
  unit's data shifts (st1_0 r100: our 4-aligned `.data` moved the REL's .data start by 4, `st1_0.rel: FAILED`
  with every function identical). GNU as pads the section END to its alignment, so the alignment also
  shows as a size diff (r103 `.data` 0x24 vs 0x28, r104 `.rodata` 0x2a4 vs 0x2a8, r113/r11e/r11d `.data`).
  `asm(".section .data; .balign 8");` (or `.rodata`) before the first object of the section; check the
  target's `readelf -S` Al column (r100, r103, r104, r113, r11d, r11e now match). The reverse: r120's
  `.rodata .balign 8` was WRONG (the split object's align 8 is inferred from the address; st1.cpp's
  `.rodata` follows at 0x13b4, 4-aligned) — removed, `.rodata` 0x2c4 now equal. Check the next unit's
  offset in modules.py before trusting the split object's alignment.
- Unreferenced `.data` zero words: GCC 2.95 puts `static int x = 0;` in `.data` (r100's `r100_sndTimer`
  after `r100_sndPos`, 0x34 -> 0x38).
- Stack `cEm` in a room (r100 R100Init, r103 r103_setCorpse; em.h's cEm is the 0xDE0 EmMgr stride, the
  original's stack object is 0x3E0): `class RxxxEm : public cModel { u8 pad_320[0x378-0x320]; PlArc*
  subArc; u8 pad_37C[0x3E0-0x37C]; RxxxEm() asm("__3cEm"); };` with `RxxxEm em; RxxxEm* pe = &em;` and
  the store `*(PlArc**) ((u8*) pe + 0x378) = ...` — the original addresses subArc through the ctor's `this`
  pseudo (`addi r3,r1,8; mr r29,r3; ... 0x378(r29)`), a member access on `em` folds to the frame. The
  implicit `~cUnit` is emitted at scope end: do NOT add an explicit `((cUnit*) &em)->cUnit::~cUnit();`
  (double dtor). HEADER DEBT: plain `cEm em;` once em.h splits the work area off.
- Separate local per re-assigned object (r100 R100Init cop[0]/cop[1]): `o = SetObjSmd(); ...; o =
  SetObjSmd();` gives the first `addi r4, o, 0x1d8` an extra anti-dependence (the later re-assignment)
  -> one more dependent -> rank_for_schedule issues it before the argument `li`s (the original has it
  last). `cObj* o; cObj* o2;` — one local per object — drops the edge.
- `RsfSet(G_ROOM_ID, n); W->strId = SndStrReq(a, b, 0x80000003, 0, 0, 0.0f);` (execShowView in r104,
  r108, r118, r11d): the original loads the 0.0 AFTER the RsfSet store; a pool constant is
  RTX_UNCHANGING_P and floats above stores. `static inline f32 FCRef(const f32& v) { return v; }` +
  `static const f32 vol = 0.0f;` read as `FCRef(vol)` stays below (the cSceObj idiom); the `static const`
  takes the pool word's place in `.rodata` (sizes unchanged). Fixed 4 functions.
- `flag = RsfCheck(..); zero = 0; if (flag == 0)` (int locals) puts the else arm's `li 0` above the branch
  (#5 interblock shape, r100). Block-local `cEm* em = W->em;` for the xFC..xFF byte stores keeps the
  original's store order.
- r103 openShelf_main (4 words): two `lis` high halves get r9/r11 swapped. local-alloc orders qtys by
  n_refs/lifetime and, with sched2 on, extends each qty's life by one insn each side (`fake_birth/
  fake_death`), so hard regs of qtys dying just before/born just after are avoided — the two adjacent `lis`
  pseudos take the other's register when their births are one insn apart. Direct constants/FSet forms give
  the same result. OPEN.
- r103 checkCloseCover (4 words, constant-load pairing / `mr r30` position): all 24 declaration orders of
  `w, h, x, z` and the literal form tried; `wzxh` gives 2 words but changes the `.rodata` pool order. OPEN.
- r103 execOpenCover, r11c closeGate: #9 (our jump.c duplicates the rotated loop's exit test; `for`/`>=`
  forms give `cror`/`bns`, `while (!(x < c))` still duplicates, a goto form cross-jumps the peeled
  subtraction).
- r104 execEvent00 (1 word): `if (f & 0x40) skip = 1; if (!(f & 0x40)) {...}` — the target's second test
  is thread_jumped and its compare cse-deleted but the jump stays `bne` (cc0-equivalent compare); ours
  folds it to `b` (cse's record_jump_equiv/qty_comparison_code on the fallthrough path). `if (skip == 0)`
  (+16 bytes) and `== 0` forms tried. OPEN.
- r11d: the openDoor limit is -1.692f, not -1.69f (`.rodata` word 0x12c; check pool constants against
  rodump when a "pool order" diff is a single word). execHide_main (6 words): the target loads `spd = 0.0`
  between `li r4` and `li r6` and issues `li r3,6` last; init-after-call and a `Vec* pos` local do not
  move it. OPEN.
- r118 ThunderMove (2 words, `ori r30`/`li r28` order): declaring the zero inside the EstSet arm makes it
  worse (r31 stores, +0x194). OPEN.
- r117 EventChandelier (-4 bytes): after the first swing loop the target re-materialises `lis r29, pPL@ha`
  into a fresh callee-saved register, ours reuses the loop pseudo (gcse/cse path class, see the cse1 path
  items above). OPEN.
- Header additions: `include/sce_at.h` / `src/game/sce_at.cpp` `SceAtCreateExecAt(cModel* m, Vec* pos,
  int a, int b, int c, f32 h, int d, f32 ang, f32 range, int e, int prio, TaskFunc func, int arg, u8 flag)`
  (parameter order from r117/r11c call sites); `include/obj.h` `class cObjWep* allow;` at 0x200 (line 512).
- mcmp noise reminder: `MISSING cUnit_dt_cUnit/beginEvent/endEvent/op_delete` + `extra _._5cUnit ...` and a
  `.rodata reloc cUnit_dt_cUnit vs _._5cUnit` are naming-only; the REL shasum is the judge.

### Stage rooms, r226 pass (st2_3/r226 26/33 byte-identical, .rodata/.data/.bss identical, not flipped; r221 33/38; 2026-09-10)
- src/st2/r226.cpp (the statue chase) is complete. Work: `R226WorkPtr r226_work` (one-member struct), 0x170
  layout `x0, robo 4, cSat* sat[4] 8, eat[4] 0x18, x28, cEmWrap em[22] 0x2C, hitPoint/spdOld/spdNew/sub
  0x134.., moveTimer 0x144, Vec camPos 0x148, camAt 0x154, dieY 0x160, str 0x164, btnCnt 0x168, timer 0x16C`;
  `static Camera r226_cam` (0xF8) follows it in .bss. `.data`: two static `SceElevatorData` records (the struct
  is copied from r225.cpp; sce_com's `SceElevator` declared `extern "C"` locally), the GLOBAL `int R226EmNo[13]`
  / `R226EmIdx[14]` tables, then file-scope statics in definition order (`-2` ButtonCount adjust, two debug
  camera Vecs + 50.0f, 40, the camera init/speed/offset Vecs, 85/85/27/160). The 5 unreferenced pool words
  after R226EventRoboWalkBridgeStart's pool (`{10000, 0, -500}, 10, 0`) are a dead-stripped static debug
  camera helper (`r226_dbgCam`, uses the debug statics); the unit is in modules.py `STRIP_UNUSED`.
- Header additions: `include/objRobo.h` (room view `class cObjRobo : public cObj { WalkSequence(cObjRobo*, int); }`,
  `SetObjRobo`, and the asm-labelled `cObjRoboSetBeginEvent/SetEndEvent(cObjRobo*, int)` — the room passes
  `li r4, 0` to the parameterless members); `include/obj.h` RoboWork `int pillar` at 0x08 (was pad_2).
  sync renamed the DOL `SetObjRobo` to `SetObjRobo__FPvT0P3VecT2` (objRobo.cpp declares it C++; no other
  module imports it).
- Inline helpers with their own `Vec` (`setPosXYZ/setAngXYZ(cModel*, f32, f32, f32)`) are what the target's
  mixed store forms come from (integrate + cse): the inline's frame pseudo P has REG_EQUIV `fp+N`, so stores of
  LOADED values get `try_constants`-substituted to direct frame stores, stores of a CONSTANT argument keep P
  (recog rejects the constant store, the whole substitution group is undone) and then `find_best_addr`
  rewrites `(mem P)`/`(mem (plus P k))` through the cheapest *related* register: `stfs f12, 4(r11)` with `mr r4,
  r11` for the y constant, direct `0x30(r1)` for x/z; an inline temp at frame offset 0 is direct everywhere.
  The inline temps are `assign_stack_temp(keep=1)` slots popped at the end of each statement, so
  consecutive calls share one slot (0x30 in Init) UNLESS a function-level local keeps it busy: WalkDoorDie's
  target needs a caller `Vec pos` (0x10) + the inline for the angles (0x20); StartMain needs `Vec v = {..}`
  for setGoto declared at FUNCTION level mid-way (it reuses the freed inline slot 0x10 and stays live, pushing
  the loop's inline temps to 0x20); Init needs `zeroPos/zeroRot` at function level (0x10/0x20) and
  `pos/rot` block-scoped in the `if` (0x30/0x40, freed, reused by the later helper temps).
- `find_best_addr` (cse) prefers a `(plus reg const)` form over a bare register of equal address cost, and
  `use_related_value` binds a lo_sum CONST to the most recent register holding a related address: the camera
  distance `SQRTF(dx*dx + dy*dy + dz*dz)` written as an inline over `Vec* a, Vec* b` called with the GLOBALS
  `&r226_cam.param.pos, &r226_cam.param.at` gives the target's `addi r9, r30, 0xa4; addi r11, r9, 0xc; lfs
  0xa4(r30); lfs 4(r9); lfs 0xc(r9); lfs 4(r11)`; `&cam->param.at` through the `Camera* cam` local is
  precomputed into a pseudo shared with the PosToPos argument and hoisted a call earlier. The PosToPos
  arguments themselves must also be written on the global (`&r226_cam.param.at`, the emrock idiom) while
  `cam->param.fovy`/`cam->up`/`cam->dist`/`CameraSetOrientationUp(cam)` use the pointer; `GlobalWork* g =
  pG;` at the top keeps pG in a callee-saved register across the PSMTXMultVec calls (target `lwz r27, pG`
  first); `cModel* parts = pl->getPartsPtr(0);` before the PosToPos call (a nested call in the argument list
  precomputes `&g->Cam.param.at` into `r0; mr r3, r0`).
- `switch ((u32) x) { case 0: default: ..; case 1: ..; case 2: .. }` gives the `cmpwi 1; beq; cmplwi 1; blt
  default; cmpwi 2; beq` tree (playerRunMovePassage): the `case 0:` node is what adds the unsigned range test.
- Two Key tests ORed (`((trg & A) && (on & B)) || ((on & A) && (trg & B))`) keep `&Key` and the first `trg`
  words across the second test; the same as `if / else if` with two `hit = 1` bodies reloads `Key`.
- `cEmWrap::setEm(s16, ..)` fed from an `int` table: COMPILER-DIFF 4 alias `cEmWrapSetEmI(cEmWrap*, int, ...)
  asm("setEm__7cEmWrapsSciii")` (ours narrows the load to `lha +2`); `MotionMoveF(m, 0) asm("MotionMove")`
  for the two-argument player-routine calls; `GameSaveSave(&GameSave, pSaveData, -1)` (game.h).
- Loop-hoisted `lis pG@ha; lwz` per iteration with a `flags_178 |=` store inside = `BitOn(pG->flags_178, ..)`
  (a plain member store lets loop.c hoist the pG load); `IntSet(work->x, 0)` where the target loads
  `pG`/`pPL` after the zero stores; `AtariFlagsAndV(&pl->atari, mask)` (volatile) where the `sth` is followed
  by a pG/pPL reload, `pl->atari.throughOn()` / `setFlag100()` (member calls, `addi 0x2b4; lhz 0x1a`) where
  it is not; `U16Set(pG->pl_life, 0)` before a `MotionSetCore(.., ROOM_ARC_PTR(pG->pRoomArc..))` (pG reload).
- `(f32)(-i * 2000 / 60 + 1000)` = the falling switch (`subi giv, 0x7d0; mulhw 0x88888889; addi 0x3e8`),
  `(f32)(i * 2000 / 60 - 1000)` the rising one; `-4225.0f + (f32) i * 2290.0f / 60.0f` (`fmuls; fdivs; fadds`
  with the negative constant). `k = smdNo == 9;` (setcc `xori/subfic/adde`) followed by `if (smdNo == N) k =
  ..` chains; `(u8) hits[k]` of a local `int` table passed to EstSet = `lbz 3(rX); clrlwi 24`; `if (smdNo >=
  8 && smdNo <= 11)` = `subi 8; cmplwi 3`; a loop exit `if (i >= frames[k]) { ...; return; }` inside
  `while (1)` is moved by loop.c to after the nearest barrier (between the two MotionSetCore arms).
- `int n = 6; for (i = 0; i < n; i++) SmdSetTrans(smd[i], 0)` keeps `blt` against `&smd[6]` (a literal bound
  folds to `<= 5`/`ble`); the address of that block-scoped `smd[6]` is the earlier inline's frame-top pseudo
  (P = fp+0x20 for a Vec temp at 0x10), so the `addi r29, r1, 0x20` sits before the preceding loop.
- `ButtonCount`: `cPlayer* pl = pPL;` at the top (r29 across the calls) + `u32 max = *(u16*) m` (unsigned
  double trick) + `frame = (u32) ((f32) max * rate)` (2^31 fixup).
- Residuals: R226EventRoboWalkPassageStart/BridgeStart (`i++` hoisted into the compare block, #5, 2 words
  each); playerPillarDownCk (`fmr f31,f1` before `mr r28,r6` in the prologue, #1); R226EventPassageSwitchMain
  (COMPILER-DIFF 2 `extsb`/`clrlwi` of the s8/u8 locals reproduced with `int cutX = (s8) laundered` — the
  remaining 49 words are the callee-saved permutation the two extra pseudos cause plus the `cmpwi side,1;
  mfcr` one call later; assignment/declaration orders and launder placements do not move it); Init (4 words:
  the entry block's own `lis pG@ha`/`lwz` pair r9/r11 swapped against the PRE'd r19 copy — a local-alloc tie);
  R226EventRoboStartMain (8 words: the r108/r218 OPEN shape — after `do { setAng(rot.z + step); if (rot.z >=
  0.0f) break; SceSleep(1); } while (1)` the target reloads the 0.0 pool constant for the final `setAng(0.0f)`
  while our gcse PRE merges it with the loop compare's hoisted f31; the exit edge `bso` is preceded by the
  LOOP_END note so cse never follows it — the merge is gcse's, and no source form found that stops it).
- r221 setTexRender: `x136 = 2; x137 = 0x10; x138 = 0x30;` in that order (dying last store first). r221's
  throwBonbe `clrlwi r8, r16, 24` for `(u8) eff0` is COMPILER-DIFF 2 (an `asm` launder there costs a word and
  shuffles r16..r22); the remaining r221 diffs are the OPEN items of the last-four pass.

### Stage rooms, st1_1/st1_3/st2_0 bytes-first pass (r104 28/30, r208 38/41, r201 33/36, r207 18/21, r222 20/29, r11f 15/17, r106 13/18; none flipped; 2026-09-10)

- Syncs: st1_3/r103, st1_3/r11c and st2_0/r208 carried unmangled placeholders (0.00% rows). r208's static
  `funcAshley2(cEm*)` has the same mangled name as r210's: `sync_rel_symbols` leaves the placeholder alone
  ("already defined elsewhere") — rename it by hand in symbols.txt + sym_map.tsv; two LOCAL symbols with one
  name are fine (Tools has `OptionExec__Fv` twice). r208 and r222 both define `setResetNum/getResetNum/
  incResetNum` (r208's `extern "C"`, r222's C++): now `static` in both (`extern "C" { static ... }` in r208).
- flow's `(use (const_int 0))` nop: `find_basic_blocks` emits it after a CALL_INSN that ends a basic block
  (a call immediately followed by a loop label at gcse time — before loop.c places the hoisted invariants).
  It survives into sched1 as a ready insn on unit "none" and TAKES AN ISSUE SLOT (two-issue), splitting a pair
  of independent `li`s (r208 footingB_up `li r30,0; li r31,0` -> `li; lhz; li`) or pushing a low-priority `lis`
  past the next `bl` (r208 setEmGo). Fix = any statement between the call and the loop: `y = 2900.0f;` after
  the SceSetEventCancel (the `fmr` lands where the target has it), the go-flags zeroed right before `while (1)`
  (the `li`s hoist anyway). Check for it with `-fsched-verbose-6`: an insn with code -1 and unit `none`.
- `pGS->flag` (global.h struct view of pG) keeps the pG load below the preceding template-copy stores of a
  `Vec pos = {..}` (r11f EventS10EndProc) and below a `SmdGetObjPtr(n)->be_flag |= 2` store (r222
  first_cut_exit); `PSet(work->x, call())` for a work pointer assigned from a call right before an RsfCheck
  (r101 Init evt30, r202 Init sat: the target loads pG after the store). Target rule of thumb across this
  pass: the original never hoists a scalar-global load above a store through a pointer/reference and never
  hoists a store above a pseudo-based load (r208 SubUnderCrankExec loads all three template words before its
  first frame store, r222 R222Main loads the template after `seTimer = 30`); ours needs the reference/struct
  view per site, and an RTX_UNCHANGING pool/template load cannot be held back by any store form (IntSet,
  `const Vec&` reads change the copy shape instead).
- `static inline void AtariFlagsAnd(cAtariInfo* a, u16 m) { *(volatile u16*) &a->flags &= m; }` (r207
  EnemySet): the following `work->sub = pSUB; pSUB = NULL;` reloads pSUB after the `sth` and keeps pSUB@ha in a
  callee-saved register (31 -> 10 words). Residue: the zero of `pSUB = NULL` is issued after the `sth` in the
  target (`li r9,0` reusing the atari pointer's register) and before it in ours (do-while, volatile store,
  local copies tried).
- Item-event "done" callbacks with `__Fv` names (r104 openedBox/openedShelf): `static void f() { int no;
  g(no, 1); }` — the uninitialised local forwards r3 untouched (`li r4,1; bl` only), which is what the
  original's void-parameter callbacks did.
- r207 Init: `if (RsfCheck(G_ROOM_ID, 0) == 0)` (first visit sets flags 0/5/6) — the polarity was inverted
  (`blt` vs `bge` was the only diff).
- r201 moveAltarObj: both directions in ONE `for (;;) { if (open == 1) { if (!move()) {..; break;} } else
  {..} SceSleep(1); }` — the target shares one `li r3,1; bl SceSleep` block and re-tests the mfcr'd `open`
  compare after it (`b test; sleep: ..; test: mtcrf; bne`). Residue: the three inlined attachGem copies
  allocate `i`/`i*4`/work as r9/r10/r11 in the target and r10/r11/r9 in ours (int/u32, declaration order,
  pointer forms tried).
- r10b readEvent: `pLog->err(0, 0, "...[%d]>[%d]", size, max)` — the second `%d` argument IS `max` (the
  target's compare register r8 is the argument register, `mr r8` folded); the source lacked it (5 -> 3).
- r106 shake* halves: `x += k; if (!(x > lim)) { wait: SceSleep(1); x += k; if (!(x > lim)) goto wait; }
  x = lim;` (a goto loop INSIDE the `if`, label first): the peel keeps its own compare and the target
  cross-jumps its `ble` with the loop's (`fadds; fcmpu; stfs; b L`); the `goto open; open: if (..)` form
  jumped into the test and reloaded rot before the compare (19 -> 9 words per door; the rest is the loop's
  f0/f13 pair swapped — local-alloc priority of the two dying loads).
- r222 em_reset: `u32 idx = (u32) getResetNum() % 3; EM_LIST(0x19 + idx)` gives the remainder its own
  register (r9) instead of reusing the call result's r3.
- OPEN, arg-`li` family (r113 execHide / r11d execHide_main `li r3,6` of a u32-returning SndCall; r207
  EnemySetEndProc second setEm `li r4/r5` before a following setGoto's r4/r5; r201 setSwitchEnv `li r3,0`
  followed by `li r3,3`; r11f Evt_R11FS00_Func `li r5` vs `addi r4`): the target issues an argument `li`
  LAST among the call's `li`s when its register is set again later in the block (by the call's return value
  or the next call's argument); ours ranks it first (it has one more dependent through the output
  dependence, then LUID). All rank_for_schedule inputs checked (prio/weight/class/dependents equal);
  int-argument aliases, statement order, locals, void/u32 aliases do not move it. Mechanism unknown.
- OPEN, jump1 exit-test copy vs loop.c (r202 throwRock, COMPILER-DIFF #9 family): when the duplicated exit
  test contains a store + jump-to-exit, the copy's conditional jump lands on a label INSIDE the loop and
  loop.c rejects the loop ("ignored due to multiple entry points", `-dL`) — nothing is hoisted. The target
  has the same peel AND the hoisted store constant (`fmr f28,f30`) before the peel's compare, so its copy
  sat inside the loop notes. r106's close halves (store in the exit code, no peel in the target) are the
  known #9 shape; write those as goto loops.
- OPEN r201 checkSwitch: `while (on != 1) {..}` — loop.c hoists the invariant `cmpwi on,1` into a CC pseudo
  (`mfcr r30`/`mtcrf`) in ours, the target re-compares at the loop bottom while still hoisting the
  `lis disarmTrap@ha` (so the loop was valid); for/do-while/goto/volatile forms tried.
- r200 execTruckEvent_end is COMPILER-DIFF #11 exactly (two freed 12-byte Vec slots merge into a 24-byte
  slot, no split -> +0x10 frame); a prototyped `memset` or a struct copy loses the `crclr` libcall.
- Harness: /tmp/rooms_b (`mcmp.py MOD/UNIT [SYM]` with `OBJ=`, `tryv.py MOD/UNIT FUNC variants.py` resolving
  the room source through modules.py UNITS, `vapply.py`, `sbs.sh MOD/UNIT SYM [OBJ]`, `mdump.sh MOD/UNIT
  -dX` with `SRC_OVERRIDE`).

### Stage rooms, st4_0/st2_1 sync + polish pass (r206, r403, r40a, r20f Matching; r404 37/38, r402 18/19, r40e 12/14, r40f 8/9, r204 21/24, r209 54/61, r20d 19/32, r20e 25/31; 2026-09-10)

- The "0.00% on almost every function" state of r402/r403/r404/r204/r206/r209 was the unsynced
  symbols.txt (placeholder names); after `sync_rel_symbols.py` (no DOL rename in any of the six) the real
  counts were 15/19, 30/32, 34/38, 20/24, 28/29, 53/61. Names that a Matching room already carries
  (`reset_40__Fv` of r400, `setTexRender__Fv` of r20a/r406, `OpenBoxTreasure__Fi` of r40c (size 0x44 vs
  r402's 0x94), `em_destroy__Fv`, `setLadderMotion__Fi`) stay placeholders and keep showing 0% — see the
  helper note above; the module `.rel` shasum and a positional masked compare are the judges.
- Harness: /tmp/rooms_a (`mm.py MOD/UNIT [SYM]` = masked compare with target->ours name aliasing for the
  placeholder pairs; `tryv.py MOD/UNIT FUNC variants.py [--asm N] [--apply N]` compiles a room source from
  src/st4|st2 with the module flags; `sbs.sh MOD/UNIT SYM [OBJ]`; `mdump.sh MOD/UNIT -dX` with SRC_OVERRIDE).
- `cModel* m = pPL;` (or `= pSUB`) declared at the top of the block that does `v.x = K; v.y = K; v.z = K;
  m->setPos(&v);` fixes the FPR pair swap of two pool constants (`lfs f12/f13` x/y or x/z exchanged, the
  stores following): the pointer load moves out of the store block and local-alloc's qty order follows.
  Fixed r206_snipe, r403/r404 slide_move (both arms), r40a first_init (the r10c/r40a OPEN "x-first/z-first"
  item). A `Vec* pv` local or `f32 x/y/z` locals do not do it; the local declared AFTER the stores does not.
- `R402Work*& wp = r402_work.p; wp = MEM_CALLOC(..)` (the r21d/r11b reference-store idiom) also decides
  which of two hoisted `lis sym@ha` pseudos gets r30/r29 (R402Init: OpenBoxTreasure/OpenedBoxTreasure)
  and the whole PRE/copy shape of `Vec pos = {0,0,0}; Vec rot = {0,0,0}` memsets followed by create
  calls (R20fInit 35 -> 0 words: target `addi r3,r1,0x18; mr r31,r3` = the PRE copy taken right after the
  arg computation, ours had the address hoisted above the first memset into a callee-saved register).
- `pGS->pRoomArc` (struct view) for the `mot[9]` load between the GetEtcAddr calls of setLadderMotion
  (r400's fix, needed identically in r402/r403).
- `goto test; sleep: SceSleep(1); test: if (cond) goto sleep;` for `while (work->a - work->b <= K)
  SceSleep(1);` where the target reloads `lis work@ha` inside the test (no loop notes -> no loop.c hoist;
  r404_checkEmSetChainSaw 0x68 -> 0x48). The same goto shape for R402MoveDoor02's outer `for (t..40)` keeps
  `lfd 2^52`, `cmpwi cr4,dir,1` and `addi r23,r28,1` inside the outer body (41 -> 15 words).
- `init.m.x20 = 30000; init.m.mesStart = 1; init.m.mesA8 = 0xC; mesAC; x58; mes[0..9]` (mesStart second,
  a literal 1, no `int one` local) gives R404Init's `li`/`stw` order; `int one = 1` at function scope
  turned the 1 into an SI pseudo in r0 and shifted every constant register.
- COMPILER-DIFF #1 in a room: `SatMgr.create(&pos, &rot, poly, 0x40, 0x100, h)` needs the floats-first
  alias `cSat* SatMgrCreateF(cSatMgr*, Vec*, Vec*, Vec*, f32 h, int, int) asm("create__7cSatMgrP3VecN21iif")`
  (emobj.cpp's) so `fmr f1,f31` is issued before `li r7,0x40; li r8,0x100` (R402InitDoor02).
- `(c0 && AT(1)) || c0` with `c0` a variable: our cse deletes the dead AT(1) test (cse AROUND path); the
  target keeps it because the second operand was the macro re-expanded (`|| R209_SNIPE_AT(at, i, 0)` — a
  fresh load cse merges into c0's register while the compare survives). r209_BowgunActionSet4 83 -> 0.
- `const f32 lim = 11225.0f; const f32 limB = 8716.0f;` right before a `while (y < lim) { y += 50.0f; ..}`
  loop whose exit stores `y = lim; yB = limB;` puts 8716 before 50 in the pool (r209_SwitchAppearCheck,
  .rodata now equal); `const f32 lim = -2.83f; const f32 spd = -0.09f;` before the if/else of r204_TanaMove
  (pool -2.83, -0.09, 2.83) — the const-at-top pool lever, once more.
- `f32 ry = -1.388f;` declared before `pl->setPos(&p)` and stored after it (`stfs f31,0xc`), and the
  divisor/loop bound of `spd = (4000 - y) / 90; while ((f32) i < 90)` as ONE `const f32 n = 90.0f;` (both
  uses literal, `fmr f29,f13` copy of the constant register for the loop compare) — r20d_moveWall 29 -> 3.
- Include order is visible in `.rodata`: r204's target group is `[event.h][cFlag.set()][atari.h][map_obj.h]
  [light.h][widget.h][flag_rsf.h]`, i.e. event.h is included before atari.h and map_obj.h before light.h.
- r20d has a dead-stripped static helper after r20d_moveWall (pool 1.0, the signed int->float double, 120,
  PI/180, 2000, -1, PI/135; `r20d_dbgWall`, unit added to STRIP_UNUSED); r20d's `.rodata` still differs:
  execThrough's pool wants 10.0 first (ours 0.1, PI, 0.0, 10.0) and five unreferenced words {PI, PI/4, PI/2,
  7PI/8, PI/8} follow getTargetPos's PI at 0x2064 (a dead table or dead code of getTargetPos/throwLantern).
- r40e: the r104 `FCRef(vol)` static-const idiom for `SndStrReq(.., 0.0f)` after RsfSet (execShowView);
  `pG->flags_54 |= 0x04000000` (was 0x400) in gameResult.
- OPEN (this pass): r404_initEmSet (61: the target PRE-hoists `n*12` above the rot template-copy loop
  (`mulli r4` before, `add r4,r4,r6` after) and forms `&rot[n]` as `(n*12 + &pos) + 0x28` with a SECOND
  mulli — two cse blocks; every p/r placement, `int`/`u8` index and `&rot[0]+n` form tried, best 60);
  R402MoveDoor02 (15: `&id` PRE-hoisted above the goto loop in ours, in the outer body in the target;
  r22/r23/r24 permutation follows); r40e gameResult (8: `li r28,0xff` of FadeSetW's `black` issued after
  the two systemVISetBlack calls in the target, at the block top in ours; single-variable fade helper does
  not move it) and R40EExecEventS00 (2: the `addi r31,r9,cMes@l; addi r31,r31,4` two-step, same as r108/
  r117/r11c/r11d); r40f BombSet (the known "second word pair" template-copy OPEN); r209 R209Main (124:
  gcse PRE hoists `j+1` from the outer latch to the end of the pre-inner-loop block ("PRE/HOIST ... copying
  expression" in the gcse dump), so loop.c never sees `j` as a biv and the `j*4`/`j*8` givs are not
  reduced; a separate `u32 bit` counter gives the target's inner loop (pointer compare `cmplw r31,r25`,
  `k` biv) but the outer loop stays); r209 Switch/BridgeAppearCheck (7 each: `lis RO; lis work` order and
  the EstSet `li r8/li r10` order), 2ndBattleEmSet 29, BridgeAppearCheckEnd 28, OpenPicture 13;
  r204 EventChandelier1/2 (155/156) and nige_check (228): macro-generated, callee-saved set differs
  (stmw r16 vs r20); r20d throwLantern 84 (`switch (u->step)` tree `cmpwi 2; beq; cmplwi 2; bgt; cmpwi 0;
  beq; cmpwi 1; beq` and an `add r29,u,st; lwz 0x18(r29)` indexed motion pointer), operateCrank 68,
  execThrough 71, getTargetPos 51, setThrowLantern 46, execRoundSwitch 45; r20e checkPuzzle 366,
  initPuzzle 119, moveCrestDoor 33 (not looked at).

### Stage rooms, st4_0/st2_1 leftovers pass (r40e Matching; r209 57/61, r20d 26/32 with .rodata equal; r404 61, r402 15, r40f 5 unchanged; r204/r20e not iterated; 2026-09-10)

- Harness: /tmp/rooms_a2 (copies of /tmp/rooms_a's mm.py/tryv.py/sbs.sh/mdump.sh with the paths changed).
- **Loop-note barrier for a pinned constant** (r40e gameResult, 8 -> 0): `do { systemVISetBlack(1); ScreenReSize(0x280,
  0x1C0); systemVISetBlack(0); } while (0);` before `FadeSetW(2,0,0,0)`. The inline's `black = 0xFF` pseudo is live
  across later calls (cse feeds the second FadeSetW's `col.start = 0xFF` from it), so haifa's "REG_N_CALLS_CROSSED ==
  0 -> anti-dependence on the last call" rule does not pin it and sched1 hoists the `li r28,0xff` above the three
  calls; the target issues it after `systemVISetBlack(0)`. The do-while's loop notes end the scheduling region.
- **Two sets of one pointer variable** = the `addi r31,r9,cMes@l; addi r31,r31,4` two-step (r40e ExecEventS00, the
  r108/r117/r11c/r11d residue): `MesWork* w = (MesWork*) &cMes; w = (MesWork*) ((u8*) w + 4);` — the lo_sum and the
  `+4` set the SAME pseudo; `cMes.getWork()` / a `MessageControl* m` local / `(u32)&cMes + 4` give two pseudos
  (`addi r9,..; addi r31,r9,4`).
- **Unsigned switch index** (r209 OpenPicture 13 -> 0, r20d throwLantern): `switch ((u32) no)` with cases 0..2 gives
  `cmpwi 1; beq c1; cmplwi 1; blt c0; cmpwi 2; beq c2` (the left leaf 0 is bounded below by 0 for an unsigned index, so
  no `cmpwi 0` test; a signed int gives `bgt; cmpwi 0; beq`). The range test of a 4-case tree is `cmplwi 2; bgt` only
  for an unsigned index; a default-equal `case 4: break;` moves the root to 2.
- **Poll loop with the whole tail inside the hit arm** (r209 BridgeAppearCheckEnd 28 -> 0): `while (1) { if (SceAtHitCheck
  (0x25) != 0) { ...rest of the function...; break; } SceSleep(1); }` gives `L: bl; cmpwi; beq SLEEP; rest; b END; SLEEP:
  bl SceSleep; b L` with the invariants of the rest (`lis work@ha`, `&door`) hoisted before the loop into callee-saved
  registers; `while (cond == 0) SceSleep(1); rest;` is rotated (`b TEST`) and re-materialises them after the loop.
  `for (;;)` with the same body does NOT match (28) — `while (1)` does.
- **`const f32 lim = C;` declared before a call whose result is compared with C** (r20d check 13 -> 0, setThrowLantern
  46 -> 0 code, execThrough pool): the dead initialiser's `high` pseudo is cse-merged with the literal's, so the `lis`
  is issued before the call in a callee-saved register and the `lfs` after it (`lis r30,RO@ha; bl; lfs f0,RO@l(r30)`);
  declared at the function top it also puts C first in the pool. Place the declaration right before the call: at the
  very top it changes the surrounding register allocation (check: 17 words).
- Overload check: `SceKill((int) task)` selected `SceKill__Fi`; the target `bl`s `SceKill__FP7ScePrim` (r209).
- `if (t != 0) { *out = t->pos; return t; } { not-found body } return 0;` lays the found copy out LAST (r20d
  getTargetPos 51 -> 0); the `if (t == 0) {..; return 0;} copy; return t;` form lays it out first.
- A block-scoped `Vec d = {..}` declared AFTER `SceEventStart(0); pPL->setNoSuspend(1);` puts the template copy
  after the two calls (r20d execRoundSwitch 45 -> 20); `FSet(work->roundSwitch->rot.y, ang)` keeps the following
  `lwz pPL` (for `pl = pPL; pl->setPos(&pl->pos)`) below the store (-> 1).
- Index-first pointer form for a giv-plus-loaded-pointer `add rD, giv, ptr`: `(cLanternUnit*) (i * sizeof(cLanternUnit)
  + (u32) p->units)` (r20d checkLantern); store order `em = 0; active = 0;` (destroy: last RTL store issued first).
- `ObjPSet(work->crank[i], SetObjSmd(..))` (a `cObj*&` reference setter) keeps the next call's `lwz pG` below the
  store (r20d initCrank 12 -> 0); `pGS->pRoomArc` alone does not (4).
- throwLantern (84 -> 44): `setParent(pPLS, 0xA, 0)` (struct view, r20d defines `PlPtr`/`pPLS` locally) keeps the pPL
  load after the `em->pos/rot` template stores; `f32 a = LIMIT_ANGLE(..); Muku(&pPL->pos, &pos, a, PI)` (the nested
  call evaluated into a local, otherwise `&pPL->pos` is kept across it); `cnt = 0;` inside case 1; an unused
  `static const f32 angTbl[5] = {PI, PI/4, PI/2, 7PI/8, PI/8}` at the top of throwLantern is the five unreferenced
  pool words after getTargetPos's PI (emitted before the function's own pool; r20d `.rodata` now equal).
- OPEN, gcse PRE placement family (COMPILER-DIFF #3): R402MoveDoor02 (15) — `&id` = `(plus fp 0x28)` is computed in
  the inner loop body; our gcse (`pre_lcm`, block-based lcm.c) inserts it at the end of the block before `top` and the
  target has it in the outer body at the inner preheader (loop.c's position, `addi r24,r1,0x28` after `li r31,0`);
  R209Main (124) — the same PRE hoists the outer latch's `j+1` (`PRE: redundant insn .. reaching reg`, single
  occurrence!) to the end of the pre-inner-loop block, so loop.c loses `j` as a biv and the `j*4`/`j*8` givs (`li
  r22/r23,0; addi 4/8`) never form; a `u32 bit` counter, pointer loops and `u32* f = flags` do not change it. A
  proper edge-based LCM would leave both in place; the original's gcse evidently did. id inside the loop, while(1)/
  do-while, pointer local, one-member struct, inline accessor, cast-then-deref tried on MoveDoor02.
- OPEN r404 initEmSet (61): the target computes `&rot[n]` as `(n*12 + &pos) + 0x28` (the frame-slot distance) with a
  SECOND `mulli` and forms `&pos[n]` after the rot copy loop; our cse never relates `(plus fp 0x30)` to the `(plus fp
  8)` pseudo (fold_rtx skips PLUS qty_consts, get_related_value handles only CONST) and merges the two mults.
  `(Vec*)((u8*)&pos[n] + 0x28)` reproduces the +0x28 shape (15 words) but still one mult; both-after / inline /
  u8*-arithmetic / int index / u32 base forms: 84-98. Compiler-build candidate (cse frame-address relation).
- OPEN r209 Switch/BridgeAppearCheck (7 each): after `seId = RoomSeCall(..)` the target issues `lis RO(lim)` before
  `lis work` (so its `stw seId` carries no dependence to the following `obj->be_flag` loads); ours ranks the work
  chain higher. BitOn/U32Set/volatile/local copy/lim-before/`R209Work* wp` tried; the EstSet `li r8/li r10` order is
  a consequence (INSN_DEPEND count from the later `lwz r10`). 2ndBattleEmSet (28): `step = 4` is `li r25,4` before the
  `if (x4F88 > 7)` and `mr r29,r25` after the for loop in the target (a second pseudo for the constant, step's r29
  reused as the loop's `lis work` base); step-after / `next` local / declaration orders tried (`int arg = 0; u32 step
  = 0;` order applied, 29 -> 28).
- OPEN r20d: throwLantern (44) head — `int st = 0` stays a pseudo the original's cse does not fold: `stw r29,0xc(u)`
  then `add r29,u,r29; lwz r4,0x18(r29)` (unscaled, i.e. a byte offset) and the `step/state` stores before `lis pPL`
  (#12 family); operateCrank 68, execThrough 62 (loop/register structure), moveWall 3 (`li i,0` before `lwz pPL`),
  checkSwitch 1 (0.0 reloaded from the pool after the loop, r108 openCover family), execRoundSwitch 1 (`addi r4,r30`
  vs reload_cse's `addi r4,r3` after `mr r3,r30`).
- r204 EventChandelier1/2 (156/155, `stmw r16` vs `r20`: the target hoists every `lis` of the while(1) loops —
  pPL x2, work x2, pG x2, ActBtn, Key, two strings — into callee-saved registers at the top; FSet on the two pos
  stores made it worse) and nige_check (228), r20e (checkFinalPieceUse 2, move 10, moveCrestDoor 33, initPuzzle 119,
  checkPuzzle 366) not iterated this pass.

### Stage rooms, st2_2/st2_3/st1_2 bytes-first pass (r217 + r21d Matching; r214 15->22/25, r22c 49/50, r224 15/19, r11b 11/14, r227 26/30; 2026-09-10)

- Syncs: st2_3/r225 carried 9 unmangled placeholders (now synced; 2/13 -> 10/13 with no source change — the
  "0.00%" rows were placeholder names). `gnd_open` was defined global in BOTH r224.cpp and r225.cpp (now `static`
  in both; symbols.txt `gnd_open_13994 -> gnd_open__Fv scope:local`). r21d/r221's `setTexRender` and r226's
  `SceBgmCheck` placeholders renamed by hand (`setTexRender__Fv scope:local` twice is fine — locals). Rule: after a
  sync run `diff` against a saved copy of config/G4BE08/symbols.txt (no DOL rename happened this time).
- flow's `(use (const_int 0))` nop (call immediately followed by the `for (;;)` label) is what pushed r22cGateCtrl's
  `li open,0` and the loop-invariant `lis` fillers one call later each: `open = 0;` written right before the loop
  (18 -> 2 words); the else arm's literal `0.0f` in both the compare and the store (instead of a `lim` local set
  before the compare) puts the pool `lfs` after the member load (-> 0). r22c is 49/50: ResultScreen::highscore
  (10 words) is a global-alloc order tie (`score` r28 <-> loop `IdSys@ha` r31; with our priorities score (14 refs)
  can never rank below a 3-ref lis whose REG_EQUIV doubles its live length — not source-fixable).
- Inline `Vec` parameter vs frame-base local (r213 Init, 23 -> 14): a `Vec* pos` PARAMETER of a static inline
  always gets a pseudo copy (integrate.c process_reg_param copies a non-USERVAR arg such as the frame pointer),
  so the calls pass `mr r6,r31`; the target's fresh `addi r6,r1,8` per call is a caller-local `Vec pos` at frame
  offset 0 (the frame pointer itself, no pseudo). The target's `&rot` shape `addi r3,r1,24; li r4,0; mr r31,r3` is
  the gcse PRE copy of the memset argument pseudo (`(plus fp 16)` computed in the memset block, redundant in the
  create block after the zero loop) — a plain caller local, not an inline temp: write the setup flat in R213Init.
  Residue: ours issues `addi r30,r1,24` (T_0) and `addi r26,r1,40` (`&door0`) at the block top, the target after the
  memsets (frame-address pseudos are ready at t=0 for our sched1; the target's were not).
- Loop counter per loop (r217 2nd_set 37 -> 0): with one function-scope `u32 i` shared by five loops, `i` is one
  low-priority global pseudo and the `i*12` givs get r31; `for (u32 i = ...)` per loop gives every counter r31
  and the giv r30 as in the target. `pPLS->dmg.set()` (struct view) after a `pG->flags_5010 &= ~x` store keeps
  the pPL load below it. A `cEmWrap* e = &work.p->em[8];` local before the three `ang` stores puts the work load
  before them (`lwz r3; mr r4; stfs x`) — the bare member call issues `stfs x` first in one loop and not the
  other. `f32 y = 2.99f; ang.x = 0; pa->y = y; ang.z = 0;` = pool order y-first with stores x, y, z.
- `cObj* o = SmdGetObjPtr(tbl[i]);` inside a nested loop body (r217 Puzzle) swaps the two hoisted table-address
  pseudos (`objTbl` r27 / `savePos` r24) into the target's registers; a `const u8* tbl` local does the same.
- r214: `Vec p; Vec v = {0,0,0};` declaration order (p first) for the setRock frame; `c->em.getHp()` written on the
  member (no `cEmWrap* w` local) in the 50-iteration wait loop gives the target's `mr r29,r28` loop copy of `&c->em`;
  `R214Work*& wp` + `BitOn(pG->flags_51C0, ..)` in Init (32 -> 0, also `#line` moved to 98); Evt_R214S00_Func: the
  `work->bino = new (&work->binoObj) IdBinocular` / `work->focus = &work->focusObj` stores are PLAIN member stores
  (one work load, `addi r0,r9,1372; stw r11,1372(r9); stw r0,1360(r9)`), the following `bino->init(&pGS->Cam, ...)`
  needs the struct view so pG loads after them, and `IdBinocular::cutin()` is called with `li r4,0` (old prototype:
  `void IdBinocularCutinI(IdBinocular*, int) asm("cutin__11IdBinocular")`). initCatapult (37 -> 29): the target's
  `cmpw d, tbl+24` (SIGNED pointer compare) is loop.c's biv elimination of an `int i` counter, `for (int i = 0; i <
  3; i++) { R214CatapultData* d = &tbl[i]; ... }` — the `do { d++; i++; } while (d <= &tbl[2])` walk of the
  earlier note gives `cmplw`; residue = work@ha/i*68 giv r30/r31 swap. checkEmReset (86 -> 0): the two
  `if (barred[k]) setOpened()` blocks come BEFORE the locals (`int emNo[4] = {..}; int done[4]; u32 n = 0; for (i)
  done[i] = 0; cEmWrapD em[4];` mid-block, the `done` zeroing is an explicit loop = `mtctr 4; stw; bdnz`, not the
  `= {0,0,0,0}` memset). throwRock (68) is the #9 peel family (target peels the 2nd loop, ours the 3rd).
- r218 pos.z of bell 0 is 4367.0f (source typo 4359 fixed; .rodata now equal). The three remaining r218 functions
  (checkClawManDead_end 19, appearClawMan 25, checkClawManDead 27) are ONE mechanism, also behind r108 openCover,
  r226 RoboStartMain and r20d checkSwitch — the post-loop re-materialisation of `lis work@ha` / the pool constant
  after a `do { ..; if (c) break; SceSleep(1); } while (1)`: it is NOT gcse (our gcse dump: 0 substs) but cse1's
  AROUND path (`cse_end_of_basic_block`: the `bso/blt exit` jump's label is preceded by [SceSleep; b top; BARRIER;
  LOOP_END], the scan stops at the LOOP_END note, `skip_blocks` treats the jump as "around a block" and carries
  `high(work)` and the CONST_DOUBLE equivalence into the exit block). Experiment (/tmp/rooms_c/sngcc, cse.c patched
  to refuse the AROUND path when a BARRIER lies between the jump and its label): r218 5 -> 7/8, r226 26 -> 27,
  r108 10 -> 11, r20d +1, 0 changes over the 347 DOL units (7304 functions, argorder harness), but r20e execThrough
  (register names) and Sscrn ss_term OpeMesMove (an extra `mr`) regress, and r21d/r201/r224 loops whose exit block
  is entered by a FALL-THROUGH past LOOP_END ARE merged by the original — so the original's rule is narrower than
  "never across a loop end"; treat the shape as COMPILER-DIFF candidate #12 (loop-exit form). Nothing installed.
  Correction to the r226 pass note: "the exit edge bso is preceded by the LOOP_END note so cse never follows it —
  the merge is gcse's" is wrong; the merge is cse1's (`invalidate_skipped_block` only drops memory, not `high()`).
- r21d moveFence (4 -> 0, unit Matching): the two loop-hoisted FadeSetW constants: `u32 zero = 0; u32 black =
  0xFF;` declared/assigned in that order inside the inline (room-local `r21d_FadeSetW`, fade.h untouched — 39 users)
  gives 0 -> r27, 0xFF -> r28 and the `start, end` store order; the header's `black`-only form swaps the pair.
- r227 execGondola: `f32 ry = -1.57f; rot.x = 0; rot.z = 0; rot.y = ry;` (pool y-first, stores z, y, x). r224 em_set:
  a block-local `cPlayer* pl = pPLS;` AFTER the `work->plPos = pPLS->pos` copy makes the 3000.0 `lis` take r29
  (callee-saved: r3 is busy with the `this` load at local-alloc time), 12 -> 0. r11b EmSetChange (45 -> 0): the
  four EM_LIST groups through `pGS` (`#define EM_LIST_S(no) ((EmListData*) &pGS->emlist[(no) * 0x20])`, so the next
  group's pG load stays below the previous group's struct stores) and the store order `x3 = 0` FIRST, then pos[0..2]
  in groups 2-4 (group 1 keeps flags, flags4 |=, pos, x3); reference setters (U8Set/S16Set) fold the address into
  `sth rX, 0x5AF4(rPG)` instead. `sbs.sh` cannot show an `extern "C"` function of a variant object — use mcmp.
- Tried without effect (documented OPEN shapes confirmed): r216 close `stfs ang.y` before `addi r4,&ang` (open()
  has the reverse and matches; scope/order/pointer forms), r213 StatusSetChain FPR naming of four pool constants
  (0.0 has 3 refs and wins local-alloc here, loses in the target), r213 EventSwitchMain `lis` pair order
  (pG/CamCtrl PRE'd highs and the two loop constants), r227 operateElv `&cMes` r31 tie, r21a Init function-address
  `lis` pair, r119 Init (all 35 store permutations of the 2nd tree block), r11b EmEvent (36 Vec store orders: the
  f0/f13 pair), r224 reva_common_move (the loop's `0.0f` compare constant is a second pool load in the target,
  `fmr f26,f31` copy of `spd`'s initial 0.0 in ours), r224 Main (stack-arg zero `li` position).
- Harness: /tmp/rooms_c (mcmp/tryv/vapply/sbs/mdump copies with `fold_linkonce` added to tryv so the counts equal
  the ninja build's; `xcc.sh`/`xsweep2.sh` compile a unit with an alternative cc1plus dir and module CFLAGS —
  NEVER compare a recompiled object against the ninja .o of a unit with module CFLAGS, compile both ways).
  Variant names must not collide case-insensitively (wibo's cpp): `v_xyzXYZ` and `v_XYZxyz` clobber each other.

### Stage rooms, st1_1/st1_3/st2_0 pass 2 (r104, r117 Matching; r105 28/30, r201 34/36, r222 22/29; 2026-09-10)

- Harness: /tmp/rooms_b2 (copies of /tmp/rooms_b's `mcmp.py MOD/UNIT [SYM]`, `tryv.py MOD/UNIT FUNC variants.py`,
  `vapply.py`, `sbs.sh MOD/UNIT SYM [OBJ]`, `mdump.sh MOD/UNIT -dX` with the source resolved from `src/<stage>/`).
- **thread_jumps before cse1 is blocked by a user variable** (r104 execEvent00, the COMPILER-DIFF #12 "cse AROUND
  path" 1-word item, now 0): `if (f & 0x40) skip = 1; if (!(f & 0x40)) {..}` -- the target keeps `beq EV; li r26,1;
  bne SKIP` (second compare deleted, jump kept), ours folded the jump to `b`. Mechanism: toplev runs
  `thread_jumps (insns, max_reg, 1)` BEFORE cse1; it threads `beq L1` past `L1: cmp2; bne L2` when the two compare
  chains are equal, which deletes L1, so cse1 sees one block and folds `bne` from the fallthrough equivalence.
  `rtx_equal_for_thread_p` returns 0 for two different pseudos when either is `REG_USERVAR_P`: `u32 f = pG->flags_54;
  if (f & 0x40) skip = 1; if (!(pG->flags_54 & 0x40))` (the user variable on ONE side only) keeps L1 through cse1
  (which then deletes cmp2 as redundant but has no jump knowledge on the AROUND path), and the post-cse2
  `thread_jumps` threads the `beq` with nothing left to fold the `bne`. `if (f & 0x40) ... if (!(f & 0x40))` with
  the same variable on both sides threads again (same REGNO).
- **Global tables in a room** (r104 `.data` 0x28/0x98): the two `R104ResetData/R104PatrolData` tables are global in
  the REL (ADDR16 fields hold A only, `make_rel --verify` shows "ours 98 orig 00" at the `addi sym@l`); a `static`
  table gives S+A. Check `scope:` of the room's `.data` labels in `modules/<mod>/symbols.txt` before flipping.
- **`static const f32` + `FCRef` for BOTH uses of 0.0 in one function** (r105 StreanChk 5 -> 0): the two
  `SndStrReq(.., 0.0f)` calls sit in different arms; the else arm's 0.0 must load AFTER the `BitOff` store while the
  then arm's loads first anyway. Using `FCRef(vol)` for only the second call moves the pool word (13 words).
- **Work pointer declared before the template copies** (r105 markOpenCk 8 -> 0): `R105Mark* mk = &r105_work->mk;`
  BEFORE `Vec vx/vy/vz = {..}` puts the `lwz work` at an earlier LUID; in sched1 it ties with the last two
  template stores (prio 6, weight 0 each: every SET counts +1, the dying source -1) and LUID decides, so the target
  issues the load between `stw vz.x` and `stw vz.y` (register naming r9/r10/r11 follows). A `cObj* o` local (the
  `lwz 20(r9)` early too) is wrong (29 words).
- **Dead `do { } while (0);` at the top of the block after a `do {..} while (1)` poll loop** (r117 EventChandelier
  37 -> 13): with `-fcse-skip-blocks` cse1 follows the loop body's `bne SKIP` AROUND path into the exit block, merges
  the block's `high pPL` into the loop body's pseudo, gcse deletes that pseudo's occurrence (PRE copy of the bb-0
  reg) and the block ends up on the PRE reg (r26). The LOOP_END note ends cse1's path; the block keeps its own
  occurrence, PRE turns it into a copy `r = reaching_reg` (REG_EQUAL high pPL), and cse2 re-materialises a copy
  whose source is unknown at block entry as a fresh `lis` (the target's `lis r29,pPL@ha` between the loops).
  Verified with `-fno-cse-skip-blocks` on the old source (same fresh `lis`). Family: COMPILER-DIFF #12.
- **Non-const view of a `static const Vec` member read** (r117 `ry = ((Vec*) &r117_smdRot)->y;`, 13 -> 10): the
  target issues the `lfs` below the preceding `FSet(pPL->pos.z, ..)` store; a `const` read is RTX_UNCHANGING and
  floats above it.
- **Two more flow-nop sites** (`(use (const_int 0))` after a call immediately followed by a loop, r208 idiom): r117
  EventChandelier first loop (`cnt = 0;` moved from the declaration to just before `do {`, 10 -> 2) and second loop
  (`cnt = 0;` after the `RoomSeCall` that precedes it, 2 -> 0); r222 box_appear1/box_appear2 (`t = 0.0f;` moved
  between `SceSetEventCancel` and the `for`, 12 -> 0 each). Detect: sched1 `-fsched-verbose-6` ready list with an
  insn "on unit none" taking an issue slot at t=1..5 and a prio-1 `li` shifted by one slot in the target.
- **`asm("" : "+r"(on))` at the bottom of `while (on != 1) {.. SceSleep(1); }`** (r201 checkSwitch 13 -> 0,
  tagged `// COMPILER-DIFF: #13`): loop.c hoists the invariant compare into a CC pseudo (`mfcr r30`/`mtcrf`
  across the calls); the target re-compares `cmpwi r31,1` at the loop bottom (the REG_EQUIV compare is
  re-materialised at its use, the #13 shape for CC). `int o = on; while (o != 1) {..; o = on;}` gives the same.
- Analysed, still OPEN:
  - r108 str_check / r203 StreamCheck (18 each, one shape): the target has TWO `high EmMgr` chains -- chain A
    (`lis r11; addi r23,EmMgr@l`) hoisted out of the outer `for (;;)` for the inner loop's duplicated entry test
    `lwz r0,4(r23)`, chain B (`lis r24; addi r27`) in the outer loop body for the inner loop (`lwz r29,0(r24)`
    pArray, `lwz r0,8(r27)` size, the bottom test). Ours: gcse PRE (`PRE: redundant insn .. bb 1/2/9, reaching
    reg .. end of bb 0`) makes one pseudo. The target shape is what loop.c alone gives (inner loop hoists to its
    preheader, the outer loop skips a pseudo "made by loop-optimization for an inner loop"). An `extern cEmMgr
    EmMgr_2 asm("EmMgr")` alias for the body reproduces chain B (gcse's `expr_equiv_p` compares SYMBOL_REF XSTR
    pointers) but the plain-symbol bottom test then gets a third chain; alias everywhere merges again. Dead
    do-while at the outer/inner body top, `while` form, `cEmMgr* mgr` local: unchanged. COMPILER-DIFF #3 family.
  - r108 initChurchBell (5, the r103 checkCloseCover shape): `YarareInitCube(hit, x, x, z, w, h, w, 0, 1)` --
    target load order w, x, h, z with two `lis` pairs; all 24 declaration orders tried again (`wzxh` 2 words but
    the -3000/500 pool order flips). The single-use constants load in declaration order in the target (as if the
    loads stayed at the declarations), in argument order in ours (combine merges the load into the arg move).
  - r11f Evt_R11FS00_Func (2, `li r5,1` before `addi r4,r3,52`), r11d execHide_main (6), r201 setSwitchEnv (4),
    r207 EnemySetEndProc (10): the arg-`li` family. Checked in `-fsched-verbose-6`: the li's tie on priority,
    weight (+1 each), class and dependents (1: the call), LUID decides in ours. do-while wrap, `u32 se =` result
    variable, `(void)` cast, `int one = 1`/`u8 one` dying-copy locals (all folded to `li` by cse, weight unchanged),
    `pSUB` local: no change. In r201 the same constant `1` is issued FIRST for the three calls not followed by a
    `SceAtPtr(3)` and LAST for the two that are.
  - r118 ThunderMove (2): `li r28,0` (`void* zero`) vs the hoisted `ori r30,0x8889` -- both prio 1, weight +1
    (the ori's source is its own dest pseudo), class 3, LUID; `zero = 0` placed before the loop or after `cnt`
    unchanged (2).
  - r202 R202Init (2): `lwz pG` r10 vs r9 after `stw r3, r202_work@l(r9)` -- local-alloc adjacency of the two
    qtys in sched1 order. r208 operateCrank (4): `lwz W`/`lwz pPL` order before `FSet(pPL->rot.y, W->crank->rot.y -
    PI/2)` (statement swap / `ry` local / `crank` local: 12/8/6).
  - r105 execOpenCover, r103 execOpenCover, r11c closeGate: #7/#9 (peeled `fsubs; b TEST` loop with the caller-saved
    `lfs f13` limit reloaded per iteration). r200 execTruckEvent_end: #11. r222 R222Main 7 (template load after
    `seTimer = 30`, known).

### Stage rooms, st2_2/st2_3/st2_4/st1_0/st1_2 pass 2 (r227 Matching; r214 22->23/25, r218 5->6/8, r224 reva_common_move 52->32 words; 2026-09-10)

- Harness: /tmp/rooms_c2 (copies of /tmp/rooms_c with the paths rewritten; `gsize.sh MOD/UNIT SRC FUNC` prints a
  function's gcse expression-hash-table size from a -dG dump).
- **Loop-hoisted float constants get the target's FPR names only as `const f32` locals** (r227 checkBox0/1Fall
  17 -> 0 each, unit flipped): the four constants of the second wait loop (`rot.x > rotLim`, `+= -0.0349`,
  `spd += acc`, `dy > lim`) are allocated in the order of their preheader `lfs`s (shorter live range = higher
  priority = higher FPR); plain `f32 acc = 20.0f; f32 lim = 10000.0f; f32 rotLim = ...` declared before the loop
  are set in declaration order (`lfs` order acc, lim, rotLim, then the hoisted body literal), `const f32` locals
  fold into their uses so loop.c hoists all four in BODY order (rotLim, -0.0349, acc, lim) while the declarations
  still fix the pool order. Same lever as r20d `const f32 lim` / r227 operateElv `step`/`power`.
- The `&cMes` two-set pointer idiom (`MesWork* w = (MesWork*) &cMes; w = (MesWork*) ((u8*) w + 4);`, r40e)
  closes r227 operateElv's `addi r31,r9,cMes@l; addi r31,r31,4` (2 -> 0).
- **loop.c giv order = reverse discovery order** (r214 initCatapult 29 -> 0): `move_movables` emits the giv inits
  in reverse order of `record_giv` (the list is prepended), and the LAST init in the preheader has the shortest
  live range and wins the highest callee-saved register. `R214CatapultData* d = &tbl[i];` as the loop's first
  statement makes `&tbl[i]` the first giv and `i*68` (the work index) the last -> `li i68,0` before `mr d,tbl`
  and i68 in r30; the target's `mr r29,r26; li r31,0` needs the `tbl[i].field` reads written inline (the `&tbl[i]`
  giv is then discovered at the `SmdGetObjPtr(tbl[i].objId)` call, after the first `cat[i]` access).
- **cse folds every constant-pool load to its CONST_DOUBLE** (cse.c fold_rtx MEM case, `/u` or not), so a literal
  that the target RELOADS from the pool while a register already holds the value (r224 reva_common_move's loop
  compare `spd >= 0.0f` vs `f32 spd = 0.0f`, r218's post-loop `y0 + 2500.0f`, r226 RoboStartMain's final
  `setAng(0.0f)`) cannot be produced by any literal/inline/volatile/static-const form once cse's path knows the
  register: the inline-body-literal lever (RTX_UNCHANGING_P dropped) only stops gcse/loop.c, not cse's fold. In
  r224 the merge happens in cse2 within one block (init and hoisted loop constant); in r218 it is cse1's AROUND
  path. Still OPEN; r224's `spd = 0.0f` assigned after `SndCall`/`acc = reva_acc` gives the target's r26..r31
  set (52 -> 32 words, applied) but puts the init after the call (target: before).
- **`extern T sym_v asm("sym")` alias for the cse AROUND-path family** (COMPILER-DIFF candidate #12, the em2a/em21
  TrapCamMove lever, tagged): r218's three `do { ..; if (c) break; SceSleep(1); } while (1)` exit blocks
  re-materialise `lis work@ha` in the target; `r218_work_v.p->y0` in the exit statements gives cse a distinct
  SYMBOL_REF (appearClawMan 25 -> 0, checkClawManDead 27 -> 14 with `snd = 0;` moved to right before its store
  (the target keeps `li r29,0` in a callee-saved register and issues it after the wait loop), checkClawManDead_end
  19 -> 8). The alias to a `static` object is fine: one local symbol in the .o, 45 relocs against it. The residue
  in both is the 2500.0 pool reload (previous item). Do NOT alias `wp = r11b_work.p`-style references whose high
  the target keeps hoisted with a single use: with one use our update_equiv_regs moves the `lis` next to the
  store (R11bInit 42 -> 94); that shape (target `lis r30` before the first SceExec, one use `stw r3,0(r30)`, a
  second high r26 for the reads) is the #13 family from the other side (the original keeps a single-use
  REG_EQUIV `high` pseudo where it was set). OPEN.
- Global-alloc priority is `floor_log2(refs)*refs/live_length` (global.c allocno_compare) with REG_EQUIV pseudos'
  length doubled; r22c highscore's `score` (14 refs) can never rank below the loop's 3-ref `IdSys@ha` pseudo with
  our numbers (`on` flag split off, per-loop `int i`, `u32 i`, `IDSystem*` local, do-while, pointer walk: 10-35
  words). The target's order (lis r31 before score r28) is not reachable by source; OPEN (r22c stays 49/50).
- r216 close (2 words, `stfs ang.y` vs `addi r4,&ang`): sched2's ready list at the cycle after `fadds` ranks the
  `addi r4,r1,8` (class 3, independent of the last scheduled insn) above the store that depends on it with cost
  1 (also class 3) by LUID; our sched1 issued the addi at t=4 with the `lfs` on the other unit, the target's sched1
  order had the store first. Block-local/volatile/FSet/`Vec* pa` (before and after the stores)/store orders/`y`
  locals/do-while all 2+ words. OPEN (documented tie confirmed).
- r213 Init (14 words) is the block-based LCM of our gcse (lcm.c `pre_lcm` uses only antloc/transp, never `comp`):
  `(plus fp 24)` (&rot) is computed by the second memset's `force_operand` pseudo P in bb 0 AND inserted again at
  the end of bb 0 (`PRE/HOIST: end of bb 0, copying expression 3`), so the copy `mr r31,P` lands after the memset
  call and regmove cannot coalesce P into r3; the target's `addi r3,r1,24; li r4,0; mr r31,r3` is
  `pre_insert_copy_insn` right after the computation (an edge-based LCM that sees comp[bb0]) + regmove. Same
  family as R402MoveDoor02/R209Main (#3). A `do { } while (0)` after the Vec declarations moves `addi r26,r1,40`
  (&door0) to the target's place after the memsets but costs 6 words elsewhere (20).
- r213 EventSwitchMain (8 words): the two PRE'd highs (pG, CamCtrl) get their reaching pseudos in gcse
  hash-bucket order (`pre_delete` walks the table by bucket): bucket = raw hash % (n_insns/2 | 1); with our 117
  buckets pG is 105 and CamCtrl 80 (raw 22335 / 1384400942; the rtl-code constant offset was fitted from the
  dump), the target's order needs a table size in {105, 111, 113, 129, ...} = 8..11 fewer or 24+ more insns at
  gcse time. No source rewrite of the function changed n_insns (dead statements are gone by then). OPEN.
- r119 Init third SetTree block (7 words): after sched1 the `lwz r11,pG@l(r31)` sits between `stfs rot.z` and
  `stfs rot.x/rot.y` in BOTH compilers' RTL; ours then hoists it above all six stores in sched2 because the two
  later stores anti-depend on it (`anti_dependence` finds no alias-set exit for r31 = the hard frame pointer whose
  `reg_base_value` is wiped by the `lis r31,pG@ha` set) giving it +1 priority, while the earlier stores do not
  conflict (`true_dependence` exits on DIFFERENT_ALIAS_SETS_P). The target kept it in place, i.e. its load also
  depended on the earlier stores. Compiler-build difference candidate (alias.c true/anti asymmetry); OPEN.
- r223 .rodata is 4-aligned in ours, 8 in the split object (0x264 vs 0x268): r224's `.rodata` is 8-aligned in
  both, so the link pads identically; harmless for the REL. reva_common_move's prologue (`fmr f28,f1; fmr f29,f2`
  before `mr r29,r5`) is COMPILER-DIFF 1 in a definition; an inline body with the parameters reordered changes
  nothing (integrate substitutes the outer param pseudos directly).
- r224 R224Main (9 words, the stack-argument zero `li r0,0; stw r0,8(r1)` issued after `li r8,2; li r9,1` in the
  target): the `register int zero asm("r0")` #13 recipe gives the r0 but sched1 still issues the store early
  (weight -1); `int zero` locals unchanged. gnd_close (2 words: `li r3,5` before `li r4,0` after `stfs f31,160(r3)`)
  unchanged by `int off` locals / FSet / a shared `f32 z`.
- r10c EmEvent_exit / r11b EmEvent / r119 (f0/f13 pair of two Vec constant temps): `const f32` pool-order
  declarations + `f32 z = kz; f32 x = kx;` variables reorder the LOADS and the f0/f13 names to the target's but
  swap the r10/r11 of the two `lis` highs (still 4 words); the target's local-alloc had x's high shorter-lived.

### Stage rooms, never-iterated units pass (r119 Matching; r221 33->37/38, r226 26->28/33, r21a 11->15/17, r10c 15->19/23, r11b 11->12/14, r225 operateCrank 303->77 words; 2026-09-10)

- Harness: /tmp/rooms_c3 (copies of /tmp/rooms_c2 with the paths rewritten). Never edit a source with a Python
  `str.replace(a, b)` whose `a` came from a slice that may be empty: `s.replace("", block)` interleaves `block` between
  every character (r21a.cpp went to 500k lines; restored from the tryv variant copy in `out/v_<unit>_<name>.cpp`,
  which is always the full pre-edit source plus the variant).
- **Dead `do { } while (0);` right after a `do { ..; if (c) break; SceSleep(1); } while (1)` poll loop reloads the
  following pool constant** (r226 RoboStartMain 19 -> 0, tagged `// COMPILER-DIFF: #12`): cse1's AROUND path from the
  exit `bso` carries the loop compare's 0.0 register into the exit block and folds the `setAng(.., 0.0f)` pool load
  to it; the dead loop's LOOP_END note ends cse1's extended block before the fold (`cse_end_of_basic_block` stops at
  LOOP_END when `after_loop == 0`). `for (;;) { break; }` works too; placed AFTER the statement it does nothing. This is
  the r218 2500.0 / r108 openCover / r20d checkSwitch "pool constant reloaded after the poll loop" family.
- **`k * 7200 / 7` on a conditionally incremented counter is a strength-reduced giv** (r221 checkElevatorArrive 58 -> 0):
  loop.c verifies `k++` inside an `if` as a biv (`not_every_iteration`), reduces `k * 7200` to a giv incremented with
  `k++` and emits its init (`li r31,7200`) after the `7200 - i` giv's init in biv-list order (i first), i.e. LAST
  before LOOP_BEG; a hand-written running `sum += 7200` is a user pseudo whose `li` is issued at its source LUID
  (before the gcse `lis`es). Same function: `cEmWrap em;` declared INSIDE the loop body (ctor `bl` per iteration),
  a separate `u32 m` for the `for (m = n; m <= 7; m++)` tail loop (`mr r30,r28` copy hoisted above the calls),
  `PSetPrim/U32Set` reference stores for the three work fields whose following `pG` RMW reloads `pG`.
- **A template-less `Vec` built in a second slot and copied** (r221 initShutter 34 -> 0): `Vec d; f32 dy = 4386.0f -
  o->pos.y; Vec t = {0.0f, dy, 0.0f}; d = t;` -- `dy` computed first into a callee-saved FPR (`fsubs f31` before the
  memset), `t` at frame 0x18 (memset + `stfs f31,28(r1)`), the 3-word copy into `d` (frame 8), and the later
  `Vec v = {0,0,0}` at 0x28 because `t` is a named local, not a freed temp. `{0.0f, 4386.0f - o->pos.y, 0.0f}` directly
  builds in place; a `Vec* d` inline gives 46 words.
- **Unsigned 2-case switch tree** (r221 moveElevator): `switch ((u32) dir) { case 0: ..; case 1: ..; case 2: break; }`
  gives `cmpwi 1; beq c1; cmplwi 1; bge default` (the default-equal `case 2` makes 3 nodes -> balanced tree, root 1,
  left leaf 0 bounded by 0 -> no `cmpwi 0`; the `beq default` of node 2 is threaded away); two cases give the linear
  `cmpwi 0; beq; cmpwi 1; beq`. Read `mr r5,r31` after a `cmpwi cr4,r31,1` twice: r31 was reassigned
  (`addi r31,r3,148` = `&o->pos`), the SndCall argument is `&o->pos`, not the compared flag.
- **`R226Work*& wp = ..; wp = MEM_CALLOC(..); PSetRobo(wp->robo, NULL);`** (R226Init 9 -> 0): the reference store makes
  the following `lwz pG` (x4F9F test) depend on it, so sched1 issues the store's `lis`/`stw` and the three PRE'd
  `lis`es before the load; keep the `#line` directive immediately before the MEM_CALLOC line (the `wp` declaration
  above it), or `__LINE__` shifts. Same lever: R21aInit (function-address `lis` pair order, 10 -> 0), R11bInit
  (42 -> 18) with the `wp` declaration placed right BEFORE the `SceExec` call so the store-high's `lis` issues just
  before that `bl` (its LUID) and the read-high's `lis` after it.
- **Inline `setPosXYZ/setAngXYZ(cModel*, f32, f32, f32)` (Vec owned by the inline) for every `v.x = ..; v.y = ..;
  v.z = ..; m->setPos(&v)` block of a function** (r21a FallRoofStartEnd 12 -> 0, FallRoofStartMain 62 -> 0 together with
  per-loop counters, FallRoofDie 91 -> 66; r10c EmEvent_exit 4 -> 0, EmEvent 7 -> 0; r11b EmEvent 6 -> 0): all argument
  loads (`obj->pos.x/z`) precede the stores, consecutive calls share one frame slot (0x10 when the EstSet stack
  arguments own 8/0xC), the constant `y` goes through the inline frame pseudo (`stfs f13,4(r30)`, r226 idiom), and the
  x/z temporaries take the target's f0/f13. A `cModel* m = pPL;` at the top of the block does the same for a single
  block (r206 lever), a caller `Vec` with a mix of blocks does not (the inline temps then need a second slot).
- **Per-loop `int i` when a counted loop's counter is reused by a later loop** (r21a FallRoofEndMain 18 -> 0,
  StartMain): the reversed `for (i = 0; i < 4; i++)` loop gets a final-value insn `i = 4` after LOOP_END when `i` is
  live afterwards, which destroys cse2's "counter == 0 on the exit fall-through" knowledge; the target stores the
  dead counter register as the zero of the next EstSet's stack arguments (`stw r28,8(r1)`), so its counter was
  block-scoped. Also fixes the r30/r31 obj/counter swap.
- **`f32 ratio = pPL->frame / (f32) pPL->frameMax; frame = (u32) ((f32) max * ratio);`** (r225 operateCrank 303 -> 77):
  the division (with the `psq_l qr3` u16 fast-cast) is evaluated before the `(f32) max` double trick; the inline
  product converts `max` first. Residue (77): the 2^52 magic of `(f32) max` is hoisted into f31 by loop pass 2 in ours
  and reloaded in the arm (`lis r10; lfd f13`) by the target, and `gnd_open(); break;` is laid out between the arms of
  the final if/else in the target (a `goto open;` + `if (0) { open: gnd_open(); goto exit; }` in the else arm
  reproduces the layout but shrinks the loop so 800.0 gets hoisted too: 85). See the next item.
- **loop.c hoisting rule of record** (loop.c move_movables): a movable is hoisted when `threshold * savings * lifetime
  >= insn_count` (`* 2` if already moved once) with `threshold = (loop_has_call ? 1 : 2) * (1 + n_non_fixed_regs)`
  (~66 with a call) and **`threshold -= 3` after every moved movable**; `lifetime` is the LUID distance from the set
  to the last use (1-2 for a `lis`/`lfd` pair, whose `savings` is 2 because the load "forces" the high), `insn_count`
  the loop's real insns at THAT pass (pass 2 is smaller by everything pass 1 moved). A single-use pool constant in a
  conditional arm therefore flips between hoisted and not on a few insns of loop size and on how many movables
  precede it in RTL order: r225 operateCrank (262 insns, 66*2*2 = 264 >= 262 hoists the 2^52 magic; 1076 later in
  the list fails at 264 - 24), r10c hako_down (-100.0 hoisted at 260 insns, the target reloads it), r21a
  FallRoofMove. The target's loops had a few more pre-combine insns or a different movable order; adding dead
  `asm("" : : "r"(x))` statements (+6 insns) did not flip r225. No source lever found yet; treat with the
  COMPILER-DIFF #13 "REG_EQUIV constant re-materialised by reload" family when the target's constant load sits at
  its use with a spill-register high (r10 with pG in r9).
- **`pGS->pRoomArc` for the third of three identical `SetTree(.., &pos, &rot)` blocks** (R119Init 7 -> 0, unit
  Matching): the struct-view load depends on the six preceding `pos/rot` frame stores (a plain `pG` load is a fixed
  scalar that sched2 hoists above them by the anti-dependence priority, see the previous pass's analysis). The first
  two blocks match with plain `pG`.
- **FCRef `static const f32 vol = 0.0f` for a `SndStrReq(.., 0.0f)` after a `pG->flags_174 &= ~x` RMW** (r10c EmSet
  10 -> 0): the r104 execShowView idiom applies to any plain member store the target's 0.0 load follows; the pool
  word's place is unchanged when 0.0 is the function's only constant.
- **Declaration order of two pointer locals** decides a callee-saved pair (r10c moveWheel 4 -> 0: `R10cRotWork* wheelB;
  R10cRotWork* wheelA3 = ..; wheelB = ..;` -- the lower pseudo number wins the global-alloc tie).
- **`if (t2 != 0 && t2 == i)`** operand order (`cmpw t2,i`) and a per-loop `u32 j` for the 300-frame loop (r221
  throwBonbe 24 -> 20). Residue: `clrlwi r8,r16,24` for `(u8) eff0` at two EstSet calls -- eff0 is `int eff0 = 0` set
  to 0/2/4/0xB in the cases, so combine's nonzero_bits proves the mask redundant in ours (COMPILER-DIFF 2, the original
  masks); an uninitialised `eff0` removes the `li r16,0` the target has. Plus the eff2/pG-high r21/r22 tie
  (eff2 7 refs/920 insns vs the REG_EQUIV high 13 refs/870*2: ours ranks the high first; declaration orders,
  `k0/k1` as declarations, a `GlobalWork* g` local do not change it).
- Analysed, still OPEN:
  - r120 R120Event (114): after cse1 the tail already uses the x4F8E test's `high(pG)` pseudo (r84, AROUND path
    taken), but gcse then PREs `high(pG)` (the inner event bodies compute it again) to the END OF BB 0 = before the
    first `SceSleep` call (calls end basic blocks here), and the test's own occurrence becomes the fresh `lis r9`;
    the target has no PRE (one pseudo r31 from the first `lis` to the tail). A branch-free room-local FadeSetW
    changes nothing (the body has no labels either way). COMPILER-DIFF #3 family.
  - r225 SceElevator_r225 (301): sce_com's SceElevator without the flags_5014 bits plus the chapter-end arm; sce_com's
    own copy is 82%. Target keeps `cmpwi cr4,r26,0` (faded == 0) in cr4 across the whole up-loop (`mfcr r12` saved),
    PRE copies `mr r24,r30; mr r25,r28; mr r19,r29` of `&d->pos/&d->plPos/&d->plRot` before SceEventStart, and `Vec v`
    stores as `stfs f13,0x10(r1)` after `fmadds`. Not iterated (301 words, needs its own pass).
  - r21a FallRoofMove (13): PI/180 hoisted pair f27/f28 swapped (equal refs 5, lengths 734/730: ours allocates PI first
    although 180 is shorter -- global.c `find_reg` prefers registers already in `regs_used_so_far`/with fewer local
    refs, so the choice depends on local-alloc's use of f27/f28 elsewhere); `const f32`/inline/`f32 pi` forms: 13-56.
    FallRoofDie (66, frame +8): the loop's `0.0` for `SetAngXYZ(obj, ax, 0.0f, az)` lives in a GPR (`lwz r28; stw
    r28,4(r30)`) in the target and an FPR (f28, one more callee-saved FPR) in ours; the `&camAt`/`&pos` PRE copies
    (`mr r27,r29; mr r26,r30`) are issued after the first SndCall in the target, before the first setPos in ours.
  - r10c SetEmHitAtari (149, .rodata): the 0.01/0.05/0.06 `spd` constants enter the pool after the first arm's
    YarareInitCube constants but are loaded before the first RsfCheck; assignments in both arms of the first
    if/else (PRE into the pred block) give 116-119 words and the pool still differs. chkSwitchA (71), hako_down
    (37: the -100.0 hoist, previous item), TestPosMove (4: template word pair 4/8 order, the r40f/r22a family).
  - r11b R11bInit (18): `l->x3 = 1` in both arms shares one QI pseudo hoisted to the arm top (`li r5,1`); the target
    has `li r0,1` right before each `stb` (#13 shape). `do{}while(0)` before the store, `U8Set`, a `u8 one` local, the
    store first: 18-81. Plus the `lwz pG` r9/r11 naming after the first call.
  - r226 PassageStart/BridgeStart `i++` in the compare block (#5 interblock), playerPillarDownCk prologue (#1),
    PassageSwitchMain (49, #2). r22c highscore: `found` flag / `IDSystem* sys` per iteration / `int* d = digit`: 10-14
    (tie confirmed). r216 close, r22a RopeMove, r213 x3 not retried (documented ties).

### Stage rooms, st2_1 bytes-first pass 3 (r20e 25->28/31, r204 21/24 with EventChandelier 156->94, r209 57/61 with 2ndBattleEmSet 28->2, r20d throwLantern 44->31, r404 initEmSet 61->15; none flipped; 2026-09-10)

- Harness: /tmp/rooms_a3 (copies of /tmp/rooms_a2 with the paths rewritten; `tryv.py MOD/UNIT FUNC variants.py [--apply N]`
  builds ~8 variants/s, `sbs.sh MOD/UNIT SYM [OBJ]`, `mm.py`, `mdump.sh MOD/UNIT -dX` with `SRC_OVERRIDE`).
- **`while (1) { A; if (c) { S; break; } SceSleep(1); }` is jump1's own peel** (r20e moveCrestDoor 33 -> 0): the
  compiler duplicates A + the test in front of the loop (`sub; stfs; lfs spd=15; cmp; bge W; stfs; b END; W: sleep; ..`),
  which is the target's "first step written out" shape, and loop.c still hoists the highs whose body copies cse2 then
  turns back into `lis` (the per-iteration `lis r9,15@ha`/`lis r11,work@ha` are NOT a rejected loop). Two more levers
  were needed: (1) the pool-order `const f32 add = 15.0f;` must be declared AFTER the calls that precede the loop (at
  the block top the dead initialiser's `high` pseudo is cse-merged with the peel's 15.0 load and becomes live across
  `SceAtSetEnable`/`SndCall` -> `lis r30` above the calls, one more callee-saved register); (2) `FSub(obj->pos.y, spd)`
  (reference store) for the `-=`: a plain member store is a varying struct store that never conflicts with the fixed
  scalar `lwz work@l` (alias.c fixed_scalar_and_varying_struct_p), so ours hoists the work load above it; the target
  keeps `lwz work` below `stfs pos.y` in both the peel and the loop. `do {} while (1)` / `for (;;)` / goto forms: 24-45.
- **`while (1)` vs `do {} while (1)` for a sleep loop with a mid-body break** (r204 CHANDELIER first wait, 156 -> 116):
  with `while (1) { if (frame++ == 0x1D) ..; if (MotionGetState() & 4) break; SceSleep(1); }` our jump1 rotates the
  loop (`b TOP; SLEEP: bl SceSleep; TOP: ..; beq SLEEP`), loop.c then prints "Loop from N to M is phony" (the first insn
  after LOOP_BEG is a jump) and hoists NOTHING; every `lis` of the loop body stays at the loop and is re-materialised
  per block. `do { .. } while (1)` (the r117 form) keeps the natural layout, loop.c hoists the highs of all three
  loops and sched1 moves them above the entry calls (`stmw r16`, target). The `frame = 0;` right before the loop and
  the dead `do {} while (0);` after it are the r117 flow-nop / cse-path levers (needed here too).
- FSet(pPL->pos.x, ..) + `mdl = pPLS` (struct view) for the chandelier position writes: the target reloads pPL after
  each pos store and again for `setPos(&mdl->pos)` (three `lwz pPL@l`), the plain stores share one load (116 -> 94).
  `Vec* rot = (Vec*) &crot0;` as a pointer local (`mr r4, rot` at low_RotMatrix and setAng). Residue (94/96 words):
  one callee-saved register less (the target's `&crot0` lo_sum gets its own register with the high in another,
  ours ties them: `addi r30,r30,@l`), `mf -= 5` as `subi r0,mf,5; mr mf,r0; cmplwi r0` (a temp copied into mf;
  `mf2 = mf - 5; mf = mf2; if (mf2 > 0x41)` and `(mf -= 5) > 0x41` do not give it), pPL high naming.
- **gcse across a loop containing calls is where the original differs** (COMPILER-DIFF #3, mechanism candidate): in
  r20e initPuzzle the target hoists `y+1` (and `y<<4`) out of the outer latch into the pre-inner-loop block for the
  two setLayout loops (no call in the inner loop) exactly like ours, but NOT for the first loop whose inner body calls
  SmdGetObjPtr (there the target keeps `y` as a biv with reduced givs `y*4`, `y*16+0x178`); in r209 2ndBattleEmSet
  ours cprop's `next = 4` through the for loop with calls into `step = next` (`li step,4` after the loop) while the
  target keeps the pseudo (`li r25,4` before the `if`, `mr r29,r25` after the loop); R209Main and R402MoveDoor02
  (the known #3 items) also have calls in the crossed loop. Our lcm.c is the block-based one with `delayin` zero-
  initialised (an insertion can never be delayed through a loop header), so any anticipatable expression is hoisted
  to the inner loop's preheader regardless of calls. Workaround where no biv is involved: `asm volatile("" :
  "+r"(next)); // COMPILER-DIFF: #3` right after the constant's set (r209 2ndBattleEmSet 28 -> 2; a non-volatile asm
  or the asm after the loop swaps two registers). For a loop counter the launder kills the biv, so initPuzzle's first
  loop (103 words) and R209Main stay open.
- **Two-step `&p->cell[x][y]` addresses** (r20e checkPuzzle 366 -> 224): the slide code is a MACRO over
  `PUZZLE_CELL(p, x, y) = (x)*48 + (u32)(p) + (y)*16 + 0x178` cells (`(k*48 + p)` is the single loop.c giv, `+cy*16`
  added per access with 0x178/0x184/0x148 as displacements, `p->cy` reloaded for every cell address after the piece
  stores); the inline `r20e_slidePiece(p, R20eCell* from, R20eCell* to)` gave three givs (`k*48`, `k*48-48`,
  `k*48+p`) and one shared cy load. The frame block wants `p->cy*16 + (p->cx*48 + p) + 0x178` (cy term first). The
  piece pointer is `PUZZLE_PIECE(p, pc)` everywhere (`mulli; add p; addi 0x10`). `s8 v = *t; t += 3; if (c->piece != v)`
  in checkSolved (the tbl load and its increment before the compare). Residue: the target loads the first word of a
  `Vec pos = to->pos` copy through the copy's own address pseudo (`mr r9,r11; lwzu r8,0x148(r9)`) and keeps the piece
  pointer's `addi r11,r11,0x10` before `stw 0xc(r11)`; our cse (find_best_addr, "prefer the costlier equivalent
  address") rewrites both to `base+C` forms — also in the frame block (`lwzu r11,0x178(r9)`) and in initPuzzle's
  `obj->pos` copy the target rewrites like ours, so the rule is not simply "never rewrite".
- **`int hidden = n - 1; pc = PUZZLE_PIECE(p, hidden)`** (initPuzzle) keeps the target's `subi; mulli; add p; addi 0x10`
  (the array form folds to `n*0x28 - 0x18`). The `visible = 0` store there uses the reversed inner-loop counter
  register (cse2 knows it is 0 after `subic.; bne`) — the target has a fresh `li`; cse2 ignores LOOP_END notes
  ("after_loop"), so the dead do-while barrier does not work for a loop.c-created counter.
- **Weight lever for a call-result pair** (r20e moveArmorStatue 17 -> 0): `do { o23->..rot.y = PI; o24->..rot.y = PI; }
  while (0);` around the two final stores gives o23 the two refs that rank it above the parameter `noAnim` in global
  (o23 r31, noAnim r30, o24 r29, loop counter r28).
- **Function address evaluated before a store** (r20e checkFinalPieceUse 2 -> 0): `TaskFunc fn = (TaskFunc) end;
  work->snd = 0; SceSetEventCancel(1, fn, ..)` issues `addi r4,end@l` before `stw snd` (sched tie broken by LUID).
- **Frame-address relation via a `u8*` base declared after BOTH templates** (r404 initEmSet 61 -> 15): `u8* b = (u8*)
  &pos + n * 12; Vec* r = (Vec*) (b + 0x28);` written after the `Vec pos[3]`/`Vec rot[3]` copies (declared between
  them: 65). Residue = the target's second `mulli n,12` for the rot pointer (our gcse merges it across the rot copy
  loop; an `asm`-laundered index copy gives 64).
- r20d throwLantern 44 -> 31 (two tagged launders, COMPILER-DIFF #12 family): `register int st asm("r29"); st = 0;
  asm("" : "+r"(st));` + `IntSet(u->step, st); U32Set(u->state, 1)` + `*(void**) ((u8*) u->mot + st + 4)` gives the
  target's `stw r29,0xc(u); stw r0,8(u)` before `lis pPL@ha` and `add r29,u,r29; lwz r4,0x18(r29)`; a pseudo `st`
  with the asm gets split by the setter's parameter copy (`mr`), the reference setters are what order the stores
  before the pPL load, `*(int*)&u->step = st` does not.
- Negative results (do not retry): cFence20e::move (10 words) — the 2200 `lis` is issued one cycle late in the target
  (`lis; fadds; lfs f12` vs ours `lis; lfs; fadds`), consistent with the 2200 high being a reload-rematerialised
  pseudo (#13 shape for a compiler-generated high, the shadow make_comn_fit family); 16 forms (const placement,
  literal, `lim` local, do-while, register asm, static const) all 10-22. r209 Switch/BridgeAppearCheck (7 each) and
  R209Main (124) unchanged (while(1)+break, `asm` on j, pointer forms); r402 MoveDoor02 15 (`id` inside the loop
  block 66, two-set `idp` 15, asm 40); r40f BombSet 5 (`cEmWrap* b0` before the Vecs 9, top-level Vec decls 69, p1
  first 15, dead do-while 49); r20d moveWall 3 (`i = 0` after either call: 3), r204 nige_check 228 not iterated.

### Stage rooms, never-iterated units pass 2 (r226 Matching 33/33; r11b 12/14 Init 18->14, r10c SetEmHitAtari 149->95 + .rodata equal; r221/r21a/r225/r22c analysed; 2026-09-10)

- Harness: /tmp/rooms_c4 (copies of /tmp/rooms_c3 with the paths rewritten; `lr.sh VARIANT.cpp FUNC REGEX` prints the
  lreg `Register N used ..` lines of a variant). The -dl dump's `dump_flow_info` runs AFTER local_alloc, so its live
  lengths already include update_equiv_regs' REG_EQUIV doubling; the -dg dump's `;; N regs to allocate:` list IS the
  global-alloc priority order (allocno_compare = `floor_log2(refs)*refs/length`, descending).
- **`if (i++ == N)` for a counter whose increment the target issues between the compare and the branch** (r226
  RoboWalkPassageStart 9 -> 0, BridgeStart 12 -> 0): `if (i == N) {call} i++;` puts the `addi` after the call in ours
  (no interblock motion, #5 leaf rule); the post-increment in the test puts it in the compare block by source form.
- **Definition-side COMPILER-DIFF 1 (`fmr f31,f1` before `mr r28,r6` in a prologue)**: declare the definition with
  the f32 parameter BEFORE the trailing int one (same argument registers under the ABI: ints take r3.., floats f1..)
  as `extern "C" void name__F<orig mangled>(...)` and a macro with the original argument order for the callers
  (r226 playerPillarDownCk 2 -> 0). An `asm("name")` label on a DEFINITION does not work with this cc1plus: it emits
  `.l_f*name_s:` (the `*` verbatim prefix leaks into the local size label) and NgcAs rejects it.
- **Narrow-local masks that the original keeps although every set is a constant (`extsb`/`clrlwi` of s8/u8 locals
  set in both if/else arms, r226 PassageSwitchMain 49 -> 0; r221 throwBonbe `(u8) eff0` two sites)** = COMPILER-DIFF 2
  mechanism: ours deletes the extension in combine through `reg_nonzero_bits`/`reg_sign_bit_copies` (the global
  per-pseudo summary of ALL sets, `set_nonzero_bits_and_sign_copies`), the original does not use it (eff2's mask in
  the same function survives in ours only because gcse PRE'd it into a block where combine has no LOG_LINK to the
  use). Zero-cost reproduction: replace ONE arm's constant set by `asm("li %0,29" : "=r"(var))` -- an asm source makes
  reg_nonzero_bits full, the bytes are the same `li`, and refs/live length are unchanged (an `asm("" : "+r")`
  launder adds a set and a pseudo copy and shifts the callee-saved allocation). For a promoted `s8`/`u8` VARIABLE the
  conversion `(int) var` is a plain copy at expand time (SUBREG_PROMOTED_VAR_P), so the extension must be written on
  an `int` copy: `int c2 = cut2; cutX = (s8) c2;` (cse merges c2 into cut2 and combine then sees the opaque set).
  In r226 the extsb of `cut2` survives with a plain `cut2 = 8` in both arms once estNo has the asm set; per-loop
  `int i` for the three trailing loops fixed the r29/r30 counter/giv and the r31 10-loop counter. NOT applied in r221
  (the `li %0,2` form gives 20 -> 18 but the eff2/pG-high r21/r22 tie remains, see below).
- **Global-alloc order of a user variable against a REG_EQUIV `high`** (r221 throwBonbe eff2 r21/r22, OPEN): the -dg
  order shows the high (13 refs / 870 as printed by the post-local-alloc dump) at 3*13/870 = 0.0448 just
  above eff2 (6 refs / 282 = 0.0426); mot0/mot1 (7/292 = 0.048) sit right above. eff2 needs a priority in
  (0.0448, 0.0479]: 6 refs with a live length in [251, 267] (ours 282), or 7 refs with [293, 312]. Moving `eff2 = K`
  one statement later in the four arms shortens it by ~8.5 insns PER ARM (248 total, already above mot0/mot1 -> r24)
  and reorders the arm's `li`s (LUID tie among the free `li`s: the first in RTL takes the cycle-0 slot, the rest
  sink to the block end). A 7th ref through `asm("" : "=r"(e2x) : "0"(eff2), "r"(eff2))` (two inputs, no code)
  works arithmetically but the mask of e2x is no longer PRE'd into the join block (165 words); a goto loop for the
  300-frame wait (loop depth 1 -> the high loses 3 refs) drops the high below eff1 and re-materialises the loop's
  first pG load (34). No zero-code lever with the exact window found.
- **Single-use function-scope `int one = 1;` for the `li r0,1; stb` #13 shape** (r11b Init 18 -> 14): with the
  then-arm's `l->x3 = one` (the else arm keeps the literal) update_equiv_regs moves the `li` right before the
  `stb` after sched1, local-alloc gives it r0 and sched2 keeps it below the template copies' r0 temps. A `register
  int one asm("r0")` set in the arm is scheduled at the block top by sched1 (the copies' temps are still pseudos
  then) and pushes the temps to r9/r11 (17). Residue (14): sched2 of the same block -- ours hoists the second
  template's word-0 load `lwz r7,rot@l(r11)` to clock 3 (prio 29 through the store chain `stw r7,32(r1)` ->
  `stw 4(r30)` -> `stw 8(r30)`: alias.c cannot separate `[r1+0x20]` from `[r30+4]` because the `&rot` pseudo's
  `addi r30,r1,32` has no REG_EQUAL note, so the three stores chain) while the target issues it at clock 6 after the
  two `addi`s and loads word 1 before word 2 (the same tie decided the other way = the stores did not chain in the
  original); plus the `lwz pG` r9/r11 naming after the first call.
- **Pool order of constants assigned in both arms of an if/else** (r10c SetEmHitAtari, .rodata now equal, 149 -> 95):
  the target pool is [0.0][then-arm 750..500][0.01 0.05 0.06][else 93412..], i.e. the spd constants are created
  between the arms: `spdA = 0.01f; spdB = 0.05f; spdC = 0.06f;` as the LAST statements of the then arm and the FIRST
  of the else arm (angA/B/C = 0.0f right before the `if`). OPEN: the target loads all six values (one 0.0 `lfs` + two
  `fmr`, three spd `lfs`) in the block BEFORE the RsfCheck `bl` (the `lis` of 0.0 floats to the function top, the
  `lfs` cannot cross the nine SmdGetObjPtr calls -> RTL position after the ninth call); ours keeps a copy at the end
  of each arm (+0x18 .text). 2.95 gcse has no code hoisting and jump.c no common-prefix motion, so the placement
  is unexplained (assignments before the `if` give the pred-block loads but the pool order [0.0 0.01 0.05 0.06 750..],
  170 words).
- r21a FallRoofMove (13, OPEN): the -dg order allocates PI (5 refs / 734) before 180 (5 / 730) although the printed
  lengths give 180 the higher priority (10/1460 vs 10/1468 after doubling, both REG_EQUIV) -- the allocation order
  contradicts the dump numbers; not resolved. FallRoofDie (66): the loop's 0.0 pseudo is the inline's `y` parameter
  copy (`pref NON_SPECIAL_REGS`, 3 refs); the target gives it r28 in pass 0 (a GPR already used by the 60-loop
  counter `li r28,60`), ours has no free used-so-far GPR at its turn (the counter is r30, `&camAt`/`&v` copies in
  r26-r28) and takes f27 in pass 1 (+1 FPR save, frame +8). The address-pseudo permutation must be fixed first.
- r225 operateCrank (77) / SceElevator_r225 (301), r22c highscore (10), r120 R120Event (114), r216, r22a, r213 not
  iterated this pass.

### Stage rooms, st1_1/st1_3/st2_0 pass 3 (r200, r208, r11f Matching; r201 35/36, r207 19/21, r103 17/19, r108 13/16, r11d 16/21 with checkEmReset 22->7, r210 9/12; 2026-09-10)

- Harness: /tmp/rooms_b3 (copies of /tmp/rooms_b2 + /tmp/rooms_c4 with the paths rewritten: `mcmp.py MOD/UNIT [SYM]`,
  `tryv.py MOD/UNIT FUNC variants.py [--asm NAME]`, `vapply.py`, `sbs.sh MOD/UNIT SYM [OBJ]`, `mdump.sh MOD/UNIT -dX` with
  `SRC_OVERRIDE`). tryv's "N/M identical" count is not mcmp's (it lacks the placeholder pairing); judge with ninja + mcmp.
- **COMPILER-DIFF #11 lever (r200 execTruckEvent_end 73 -> 0, unit Matching)**: the merge of two freed 12-byte Vec slots
  (24 bytes, no split: `24 - 16 < 16`) is avoided by making ONE of the first block's Vecs a 16-byte object (`struct R200Vec4
  { Vec v; f32 pad; } ang;` with `Vec* pa = &ang.v` for the member stores): the merged slot is then 28 bytes and
  `assign_stack_temp` splits it, so the second block's `Vec pos/rot = {0,0,0}` reuse both slots (frame 0x50). A 16-byte
  `pos` with a template initialiser copies 16 bytes (wrong); keep the template Vec 12 bytes and widen the memberwise one.
  Tagged `// COMPILER-DIFF: 11`. Candidate for r20e/r221/r225/r402's frame diffs.
- **Arg-`li` family, three closed by source shape (mechanism still open for the `lfs` case)**:
  - r201 setSwitchEnv (4 -> 0): `SceAtPtr(3)->dstAngle = K; SceAtPtr(0x28)->dstAngle = K;` in each arm instead of `at =
    SceAtPtr(3); ang = K;` + one store after the join. Mechanism read off the sched dump: with three calls in the block,
    `li r4,1` (r4 never re-set) gets an anti-dependent at EVERY later call (the CALL's `reg_pending_clobbers` path adds
    `add_dependence (call, reg_last_sets[i], ANTI)` for call-used regs whose last set is still the `li`), while `li r3,0`'s
    chain is cut by the next `li r3,3`; more dependents -> issued first, so the `li` of a re-set register sinks. The
    target shows the same shape with ONE following call (r207 EnemySetEndProc second setEm, r11d appearLittleSister stack
    zeros before `li r8/r9`), i.e. the original ranks that case differently. Harness test (/tmp/sched5 h.py, whole tree):
    flipping the dependents tie-break sign = 11088 regressions / 0 fixes; counting only non-OUTPUT dependents = 7526 / 0.
    Neither is the original's rule. The r113/r11d execHide `li r3,6`-after-`lfs spd` case has equal dependents in every
    reading (the mode-1 arm without the `lfs` matches with `li r3` first); an `asm("li %0,6" : "=r"(grp) : "f"(spd))`
    dependence lands it one cycle early (3 words: asm consumers have cost 1) -- not applied.
  - r11f Evt_R11FS00_Func (2 -> 0, unit Matching): `EventMgr* em = &EvtMgr; em->EvtSndStrPlay(&em->x34, 1, 0x50, 1, 0.0f);`
    (the `addi r4,r3,52` key address before `li r5,1`).
  - r207 EnemySet (10 -> 0): `pSUB = NULL` after the volatile atari RMW is the #13 shape (`li r9,0` after the `sth`, reload's
    spill register): a single-use FUNCTION-scope `cSubChar* zero = NULL;` (update_equiv_regs moves the `li` next to the store,
    local-alloc gives it the freed r9). Tagged `// COMPILER-DIFF: #13`.
- **Struct views for load order (r208 Matching)**: `v.y = pPLS->pos.y` before `FSet(pPL->rot.y, W->crank->rot.y - PI/2)`
  issues `lwz W` before `lwz pPL` (operateCrank 4 -> 0); `pGS->flags_174 |= bit` right after a `Vec pos = {..}` template copy
  makes the three template loads precede the first frame store (SubUnderCrankExec 8 -> 0) -- the struct-view RMW conflicts
  with the frame stores, a plain `pG->` RMW does not (fixed scalar vs varying struct), and the store chain's priorities flip.
- **`const f32` declarations for single-use YarareInitCube constants (r103 checkCloseCover 5 -> 0, r108 initChurchBell
  5 -> 0)**: `const f32 w = 100.0f; const f32 h = 1200.0f; const f32 x = 0.0f; const f32 z = 50.0f;` loads w, x, h, z in the
  target's order with the pool unchanged; plain `f32` locals load h/z in argument order. (The "24 declaration orders" note
  of the earlier passes was about non-const locals.)
- **Goto into a do-while body + `i++` in the exit test + an array pointer local (r11d checkEmReset 22 -> 7)**:
  `SceSleep(1); i = 0; goto check; sleep: SceSleep(60); do { SceSleep(1); check:; } while (count > 10); setEm(t[i], ..);
  if (i++ != 9) goto sleep;` -- the 60-frame sleep falls into the wait loop's single `SceSleep(1)` body, the exit is the
  fall-through of `bne sleep` (no labelled empty exit block -> haifa forms one region for the whole loop and hoists setEm's
  `li r4..r7` above the count compare like the target; a `break`/`return` exit creates the empty leaf block and kills every
  region), the post-increment puts the `addi` between `cmpwi r31,9` and `bne`, `u8* t = tbl` keeps the base in the template
  copy's pseudo (no gcse PRE copy). Residue: `i`/`t` r30/r31 swapped (global order: t 4 refs/39 insns 0.205 vs i 5/76 0.13 --
  the -dl length of `t` is not doubled although set once).
- r108 checkEmReset (6 -> 0): int-view alias `cEm* setEmI(int, int, int, int, int) asm("setEm__FsSciii")` (COMPILER-DIFF 4: the
  original passes `int list[]` entries without the `lha` truncation) + `asm("" : "+r"(tbl))` on the array pointer INSIDE the
  `if (n <= 7)` block (COMPILER-DIFF 3: the end pointer `addi r29,r31,40` is formed from the array pseudo, ours folds it to
  the frame; the launder at function scope costs 12 words, inside the block 0).
- r11d appearLittleSister (8 -> 5): the `#12 (b)` asm zero also has to be live from the function entry to conflict with the
  work high (target zero r31 / work r30): `int zero; asm volatile("" : "=r"(zero));` at the top + `asm("li %0,0" :
  "+r"(zero));` at the original position (no code for the first). Residue 5 = the two EstSet stack stores issued after the
  `li`s (dependents: each `li rN` of the first EstSet has the second EstSet's `li rN` as an output dependent, the stores
  only the two calls -> `li`s first in ours, stores first in the target: the same ranking difference as above).
- r210 toroko_go (7 -> 0): the r104 `FCRef(static const f32 vol)` idiom for `SndStrReq(.., 0.0f)` after `obj->be_flag |= 0x20`.
  r101 Event20 (4 -> 2): `Vec* pp = &r->pos; Vec* pa = &r->rot;` before `r->setPos(pp); r->setAng(pa);` (the second call's
  `addi r4,r30,160` from the object register, not from the `this` copy). Residue: `addi r26,r1,24` (a PRE'd `&ang`) before
  `li r3,21` in the target, after in ours.
- Analysed, still OPEN (one try each with the new levers): r201 moveAltarObj (36: global order i/a+8/i*4/work = r9/r11/r10/r11
  in the target; ours ranks `work` (5 refs/10 insns) first and `i` (5/22) last -- asm-li init, `a->` for every access,
  `i` before `if (g)`, `sub` pointer, do-while: 36-65); r118 ThunderMove (2: `li r28,0` vs `ori 0x8889` LUID tie -- asm-li at
  the top / before the loop / after the first SceSleep all 2, `zero = 0` inside the loop 14-88); r207 EnemySetEndProc (10:
  duplicating `SceSleep(1); continue;` into both case arms gives the target's `li r4/r5`-last order but the sleep copies are
  not cross-jumped (10), `for(;;)` + `if (loop == 0) break;` rotates the loop (24)); r202 setRock (4) / R202Init (2): local-alloc
  fake-lifetime naming (the target's pParts r11 / -0.024 high r8 = exactly the birth/death adjacency rule applied to the
  same schedule, ours gives r9/r11 -- the qty order differs, not the schedule); r11d execEmAppear_end (11: `&list0` copied
  through r4 then r31 in the target = two frame-address pseudos, #3 family; `int* p0`/`cEmWrap* pe` locals 11-28);
  r210 funcAshley2 (9: #13 dying-store block -- the em2b `asm("" : "=r"(dmy) : "f"(one), "r"(zero))` launder gives the store
  order but its output consumer costs the `lis/li` positions, 9-11); r203 GanadoWandering (27: do-while/break, `return;`,
  `goto end` forms all 27 -- the region is formed either way, so the target's missing hoists are the 100-LUID limit);
  r108 switchSymbol 12, r108/r203 str_check/StreamCheck (#3), r103/r105 execOpenCover + r11c closeGate (#9), r202 throwRock
  (#9), r11e/r10f/r222/r106/r10b not iterated this pass.

### Stage rooms, never-iterated units pass 3 (r22c Matching 50/50; r221 throwBonbe 20 -> 18; r216/r22a/r11b/r21a/r224/r120 mechanisms read; 2026-09-10)

- Harness /tmp/rooms_c5 (rooms_c4 + deadtest copies with the paths rewritten; `tryv.py MOD/UNIT FUNC variants.py`,
  `vapply.py`, `lr.sh VARIANT.cpp FUNC REGEX` (r221 only), `mdump.sh MOD/UNIT -dX` needs `SRC_OVERRIDE=src/st2/rNNN.cpp`
  for the stage rooms (the unit source lives in src/st1|st2/, not src/<mod>/), `sbs.sh MOD/UNIT SYM [OBJ]`).
- mcmp's `fn_<mod>_XXXX: missing` row for a room that includes light.h is the nameless 0x3B8 cManager<cLight> block
  (r226, Matching, shows the same row): 36/38 in r221 and 12/14 in r11b are really 37/38 and 13/14.
- **COMPILER-DIFF candidate #17: global.c pass 0 `regs_used_so_far` (r22c ResultScreen::highscore 10 -> 0, unit
  Matching).** The target allocates the 14-ref `score` to r28 while r31 is free and the 3-ref loop high `IdSys@ha`
  gets r31: `find_reg` pass 0 only hands out registers already in `regs_used_so_far` (local-alloc's picks +
  `regs_ever_live`), pass 1 takes the first free one in REG_ALLOC_ORDER (r31 downwards). Local-alloc never uses r31
  (find_free_reg sets `used` for HARD_FRAME_POINTER_REGNUM), so with `this` (r29) and the bb0 high (r30) local, the
  first global pseudo gets r31 in ours; in the original r28 was ever-live before global-alloc. Zero-code
  reproduction: `register int pin asm("r28"); asm("" : "=r"(pin)); asm("" : : "r"(pin));` in the gap where `score`
  is dead (between the digit loop and `score = 0`): r28 is then used-so-far and conflict-free, `score` takes it in
  pass 0, `i`/the digit pointer take r29/r30 in pass 0 (their local predecessors are dead) and the loop high falls
  to r31 in pass 1. Placing the pair after `score = 0` (17 words) makes r28 conflict with `score`. No natural
  source form found (a separate `found` flag still sends the param pseudo to r31 in pass 1); a `found`-flag source
  with the ORIGINAL's local-alloc choosing r28 for something invisible is the likely truth. Also needed for the
  flip: `LINKONCE_DROP["st2_4/r22c.cpp"]` (light.h included, no cLight block in the original object; the .o was
  0x3B8 too big and the REL check failed until the drop; force the recompile -- the config is not a ninja dep).
- **Live lengths are POST-sched1 numbers** (r221 throwBonbe eff2 window): haifa `schedule_insns` rewrites
  REG_LIVE_LENGTH from the scheduled order (`sched_reg_live_length`), so moving `eff2 = K` among the arm's free `li`s
  changes nothing unless the schedule changes (mot0/mot1 before eff2 in all four arms: 282 in every variant). The
  7th-ref dead test `if (eff2 == 0) atNo = 0;` at the tail is PRE'd as a `compare` expression to the end of the join
  block (eff2 is never re-set) and sched1 issues it in the first free slot after `mr k1,r3` -> 290 whatever precedes
  it; a memory operand (`eff2 == (int) bonbe->be_flag`) pins it after the preceding call (after `SceAtSetEnable(atNo, 1)`: 308, in the window)
  but the adjacent jump splits the join block and the PRE'd `lis`/`clrlwi`/`cmpwi cr4` all move to the second
  sub-block (64-129 words), and a PRE'd compare costs the extra CR save word. Applied only the #2 opaque
  `asm("li %0,2" : "=r"(eff0))` (20 -> 18); the r21/r22 tie stays.
- **r216 cR216Pole::close (2, tie read off sched1)**: at t=4 the lsu takes the pool `lfs 0.0` (prio 5) so `stfs ang.y`
  (prio 3) waits while `addi r4,&ang` (iu2, prio 3) issues; sched2 then keeps that LUID order (both class 3, one
  dependent each). The stfs can only lead if the r4 setter is not ready before t=6 or the 0.0 load is elsewhere --
  no source form does either without changing the bytes (`open()` has the same shape and the target's order there).
- **r22a RopeMove (4)**: the word-8 template load leads in ours because it kills the `addi` base (weight 0 vs +1);
  every way of keeping the base alive (`asm("" :: "r"(&tbl))` after the copy / KeyStop / U32Set, a `const Vec*`
  local, a tied dummy) costs 15-109 words -- the asm is a barrier for the following free insns. Left.
- **r11b R11bInit (14)**: sched2 ranks `lwz r7,rot@l` at 29 through TWO alias chains -- `[r29+8]` (pos words) vs the
  rodata loads and `[r1+0x20]` vs `[r30+4]` -- because r29/r30 are hard registers set twice in the function
  (`lis r29,pPL@ha` / `addi r29,r1,16`; `addi r30,r1,32` / `addi r30,r1,48`): alias.c `record_set` drops a hard
  register's base on its second set, so every frame store through them conflicts with everything. The target's
  order (`addi r9; addi r8` at clock 3, `lwz r7` at clock 7, word 4 before word 8) is what our dump gives when both
  chains are absent -- the ORIGINAL's sched2 knew the bases although the same registers are set twice there too
  (same alias.c family as the r119 Init note). Not source-fixable.
- **r21a FallRoofDie (66)**: the target keeps `&obj->pos` in ONE pseudo (r28, `Vec* op = &obj->pos` shape) and PRE-copies
  `&camAt` (`mr r27,r29`) and the inline's `&v` (`mr r26,r30`) at the end of bb 1 (after SndCall); ours also inserts
  the three copies at the end of bb 1 (-dG: `PRE/HOIST: end of bb 1` for `(plus fp 32)`, `(plus fp 48)`, `(plus obj
  148)`) but the `&camAt` copy is scheduled up to the block move (`mr r28,r9`, its source dies) while the target's
  stays behind the call (its source r29 is the callee-saved block-move base). `Vec* op`/`Vec* pa = &camAt` locals:
  61/59/54 words (op_pa: r28 = &camAt directly, no copy, but the target HAS the copy). The 0.0 -> GPR (`lwz r28`) of
  the 10-loop follows only once r28 is a used-so-far GPR free across that loop (pass 0), i.e. after this copy
  structure matches. Left.
- **r224 reva_common_move (32)**: an opaque `asm("lfs %0,%1" : "=f"(zero) : "m"(0.0f))` does load the pool word (same
  .LC entry) but the "m" operand is printed as `0(r9)` after an `addi r9,r9,.LC@l` (no lo_sum form) and loop.c does
  not hoist an ASM_OPERANDS insn (`lis; addi; lfs` inside the loop, 32-38 words). The DOL-sweep recipe (a top-level
  asm `.rodata` word + `lis %0,SYM@ha` / `lfs %0,SYM@l(%1)` pairs) would need all three 0.0 loads rewritten. Left.
- **r120 R120Event (114)**: both `beq` skip jumps target the tail label (LABEL_NUSES 2), so cse1 can neither follow
  nor skip there in the original either (`cse_end_of_basic_block` requires `LABEL_NUSES == 1` for both paths); the
  tail's `lwz pG@l(r31)` is the PRE'd bb-0 high used directly (no cse2 re-materialisation = the #3/cse2 family), and
  the inner block's fresh `lis r4; addi r4` for the SAME string label (rodata_1224 twice) means the original did not
  PRE the string high while ours does (`addi r4,r29,308` twice). Restructuring with a goto cannot reduce the label's
  uses. Left.
- r225 operateCrank / SceElevator_r225, r213 (StatusSetChain/EventSwitchMain), r10c not iterated this pass.

### Stage rooms, st1_1/st1_3/st2_0 pass 4 (r201, r207, r108 Matching (r108 in both st1_1 and st1_3); r203 11->13/15, r11d 16->18/21; 2026-09-10)

- Harness: /tmp/rooms_b4 (rooms_b3 copies with the paths rewritten; `tryv.py MOD/UNIT FUNC variants.py [--asm NAME]`,
  `vapply.py`, `sbs.sh MOD/UNIT SYM [OBJ]`, `mdump.sh MOD/UNIT -dX` with `SRC_OVERRIDE`; new: `galloc.py DUMP.lreg FUNC
  [REGNOS]` = per-hard-reg local refs/length ratios + global allocno priorities `floor_log2(refs)*refs/len` from a -dl dump;
  `skel.py DUMP START END` = labels/jumps/calls skeleton of an RTL dump range, for reading jump2's cross-jump order).
- **Global-alloc order through the block-head loop depth, applied to a goto loop (r201 moveAltarObj 36 -> 0, unit
  Matching)**: `do { next: i++; if (i > 3) return; } while (0);` around ONLY the increment/compare block of the
  attachGem goto loop (the label INSIDE the notes) puts that block at depth 2: `i` gets 8 weighted refs (3*8/22 = 1.09 >
  work's 5/10 = 1.0) and is allocated first (r9); `a->sub + 8`, `i * 4` and work then take r11/r10/r11 as the original.
  Wrapping the whole loop raises `i*4`/`a+8` too (65 words). Same lever for r11d checkEmReset (7 -> 0): the do-while
  around `if (i++ != 9) goto sleep;` gives `i` 8 refs and r31 over the array pointer `t` (r30).
- **`galloc.py` numbers are the whole story for a callee-saved permutation**: r108 switchSymbol (12 -> 0) needed `dial`
  (5/88) above `n` (4/61) and the step constant (3/96, SF) above the 2^52 PRE copy (3/98 but DF = size 2, so 0.061 vs
  0.031). One dead test `if (ang + 0.10471976f == *(f32*) &r108_dial) rot = ang;` in the loop body (weighted refs +2
  for the step constant and +2 for the dial high) fixes both. Placement rule for a dead test inside a loop.c loop:
  BEFORE the loop's first conditional jump (here right after `next = ...`, before `turn:`), because a pool constant or
  high whose first use (= its set) is in a `maybe_never` block and that is used in another block is never hoisted
  ("used in basic blocks other than the one where it is set && maybe_never" -- the em_sub EmRackCk rule): the same test
  at `turn:` or after `SceSleep(2)` left the `lfs`/`lis` inside the loop (49-61 words). Comparing the constant against
  `ang` needs an FP compare (deleted at flow2 with the tidied jump); `*(f32*) &r108_dial` reads the pointer through the
  dial high without a new symbol.
- **The dying-store/arg-`li` shapes of r207 EnemySetEndProc (10 -> 0, unit Matching) are the block TAIL rule**
  (haifa `sched_analyze`'s "branches, calls, uses ... force them to remain in order at the end of the block": the
  trailing run of CALL/JUMP/USE insns is chained and EVERY earlier insn gets an ANTI dependence on the run's first insn,
  so a block ending `bl f; b L` gives every insn one uniform extra dependent on that call, and an arg `li` whose
  register is re-set by the next call has `call + output + uniform` = the same count as a never-re-set `li` with `call +
  anti(next call) + uniform`). When the block continues past the last call with a non-call insn, the uniform dependent
  disappears and the never-re-set `li r6..r8` (3 real dependents) outrank the re-set `li r4/r5` (2) -- the original's
  order. Code-less continuation used: `asm("" : "=r"(loop) : "0"(loop));` at the end of case 0 and `asm("" :
  "=r"(wave) : "0"(wave));` at the end of case 1 (tied non-volatile launders of live variables; a volatile or
  input-only asm is a scheduling barrier that adds ANTI dependents to every pseudo user, e.g. the `addi r3,work,N`
  arg, and moves it; two identical asms would be cross-jumped). The same insn also stops jump2 from merging arm 0's
  single `bl setGoto` into arm 1's fall-through copy (the fall-through `find_cross_jump` has minimum 1; the original
  never merges a one-insn tail = #6). Tagged `// COMPILER-DIFF: #6`.
- **jump2 cross-jump mechanics (read from jump.c, confirmed with `skel.py` before/after dumps)**: for a simple jump the
  FALL-THROUGH candidate (insns before the target label, minimum 1) is tried first, other jumps to the same label
  (minimum 2) only if it fails; on the label side `find_cross_jump` skips NOTEs and CODE_LABELs, on the jump side a
  CODE_LABEL stops the match; CALL_INSNs match unless `CALL_INSN_FUNCTION_USAGE` differs; the flow nop `(use
  (const_int 0))` (a call directly before a label at ANY find_basic_blocks scan -- gcse, flow1, flow2) is an INSN and
  kills the fall-through candidate (`GET_CODE` mismatch with a CALL_INSN), which is why arm 1's nop had protected the
  single-`bl` merge in the old source; after sched2 `delete_computation` is disabled (`reload_completed &&
  flag_schedule_insns_after_reload`), so a dead conditional jump surviving to jump2 leaves its compare and loads behind
  (3 words) -- a dead test must be tidied at flow2 (jump to the fall-through block with a SINGLE successor:
  `if (X) v = K; L:` falling into the same block; `beq L; L: b M` is 2 successors and survives).
- **Dead test to raise loop.c's pass-2 insn count (r108 str_check 18 -> 0, r203 StreamCheck 18 -> 0, the "#3 double
  EmMgr chain")**: the outer `for (;;)` hoist of the inner loop's `lis EmMgr@ha` happens only in the SECOND loop pass
  (pass 1's `move_insn` of `&EmMgr` re-materialises `high`+`lo_sum` with a fresh pseudo whose uid is above
  `max_uid_for_loop`, so pass 1 ignores it; pass 2 moves it: threshold 71 * savings 1 * life 1 >= insn_count 68). A dead
  `if (EmMgr.size == 0) found = 1; found = 2;` at the end of the outer body (both stores die in flow, the jump is tidied
  at flow2) raises pass 2's count above 71 while pass 1 still moves the entry-test chain (its movable matches the dead
  test's `lo_sum` -> savings 2), and cse2 then merges the two highs in the inner preheader = the original's chain B.
  `-fno-rerun-loop-opt` gives the same code; the rule "dead insns count for loop.c in both passes" is what makes the
  two passes differ.
- **Dead test to break haifa's region (r203 GanadoWandering 27 -> 0)**: the loop had exactly MAX_RGN_BLOCKS (10) blocks;
  one dead test at the body end (`if (r203_work.p->data == 0) em = 0;`, `em` the body-local pointer) makes 11 -> no
  interblock region -> nothing is hoisted above the branches, as the original. Check `;; rgn N nr_blocks` at the top of
  a `-dS -fsched-verbose-6` dump before counting LUIDs (the old note's "~24 LUIDs" was the block limit).
- **Source-logic check before any lever (r11d appearLittleSister 5 -> 0)**: the target's `bne` skipped only the first
  EstSet; our source had both inside the `if`, which also gave the first call's `li`s output dependents on the second's
  and sank its stack stores (the "stores after the li's" residue of pass 3). Moving `EstSet(..8..)` after the `if`
  fixed it; the #12 asm zero is still needed.
- Analysed, still OPEN (one try each): r103 openShelf_main (4: local-alloc span tie of the two constant highs -- the
  `lwz a->pParts` (prio 8) is issued between `lis A` and `lfs A` so A's span is 5 vs B's 3; 8 constant forms unchanged);
  r11d/r113 execHide `li r3,6` (the arg `li` with the lowest LUID and the same 1 dependent as the others is issued
  first in ours, last in the target; `spd` init placement/volatile forms 6-14); r118 ThunderMove (2: the `li zero,0` vs the
  hoisted `ori` -- both prio 1, weight +1, 0 dependents, LUID; the ori would win on weight only if its `lis` were a
  separate dying pseudo); r202 setRock (4: `lis -0.024` vs `lwz pParts` local qty order -- our sched1 issues the lis
  before `lfs 0.0`, the target's local-alloc needed pParts first; 6 constant forms); r210 dai_go (10: the three
  `v.x/v.z/v.y` store blocks -- the target stores x, z, y with loads x, z in every block, ours applies the
  last-dying-first rule in two of them; all 4 source orders tried) and funcAshley2 (#13), toroko_ret (38, not
  iterated); r101 Event20 (2: the PRE'd `&ang` addi before `li r3,21` = both would tie only if the block ended at the
  SearchEmModule call); r203 EventMeetAgain (26: `addi r11,r1,8; mr r27,r11` + `mr r30,r29; mr r3,r30` = two pseudos
  per frame address, #3 (c); pointer/copy forms 26-39); r11d execEmAppear_end (11, same family: `mr r4,r8; mr r31,r4`
  around the template-copy loop); r11e, r10f, r11c, r222, r105, r106, r10b not iterated this pass.

### Stage rooms, never-iterated units pass 4 (r216 Matching 30/30; r21a FallRoofMove 13 -> 0 (16/17); r224 15 -> 18/19 with reva_common_move 32 -> 12; r214 throwRock 68 -> 7; 2026-09-10)

- Harness /tmp/rooms_c6 (rooms_c5 copies with the paths rewritten; `tryv.py MOD/UNIT FUNC variants.py`, `sbs.sh MOD/UNIT SYM [OBJ]`,
  `mdump.sh MOD/UNIT -dX` with `SRC_OVERRIDE=src/st2/rNNN.cpp`, `fn.sh DUMP FUNC`).
- **global.c priority is `int(10000 * floor_log2(refs) * refs / live_length)` with `allocno_size` = 1 word** (dump_conflicts prints
  ` (N)` only for multi-word allocnos; none in these functions), so the "contradiction" recorded for r21a FallRoofMove's PI/180
  pair (5 refs / 734 vs 730) is a TIE at 136 broken by pseudo number (PI first). For r221 throwBonbe: pG high 448, eff2 425,
  mot0 479, mot1 482; eff2's window is 6 refs with length [251, 267] or 7 refs with [293, 311] (the earlier note's numbers hold).
  Whatever changes one real insn inside one pseudo's range but not the other's flips such a tie: r21a FallRoofMove 13 -> 0 by
  writing `cnt = 0;` BEFORE `step = 0; spd = 0.0f;` (its `li` then sits between the hoisted PI and 180 loads in sched1's order,
  PI gets the longer range and 180 wins f28/f27 as in the target). Read the -dg order and the -dl lengths together before
  trying pins.
- **The #17 pin cannot fix throwBonbe** (tested p1..p5, 108-207 words): with r22 pinned in an arm or bb 1, every pass-1
  allocno allocated before the high (bonbe 135, k0/k1 138/139, mot0/mot1 133/134, the RoomData high 96) takes r22 in pass 0
  unless it is live at the pin, and there is no gap where all of them are live while eff2 (or its PRE'd mask pseudo, born
  exactly where eff2 dies) is dead. `sbs.sh` on the variant objects showed the whole callee-saved set re-packed (`stmw r16`).
  The r21/r22 tie needs the priority route (a 7th eff2 ref with length 293-311; the join-block dead compare gives 290).
- **r216 cR216Pole::close (2 -> 0, unit flipped): a one-use `cEmWrap* e = em;` declared between `ang.y = em->getAngY() + spd`
  and the `ang.x/z = 0` stores, with `e->setAng(&ang)`.** The load `P = em` (pseudo, prio 5 through the coalesced `mr r3,P`)
  beats the `fadds` (prio 4) for the t=3 slot after the call, so `addi r4,&ang` (prio 3, weight +1) is no longer issued in the
  same cycle as the pool `lfs` while `stfs ang.y` waits; local-alloc ties P to r3 and the bytes are the target's
  `lwz r3; lfs; fadds; stfs y; addi r4`. open() keeps the direct `em->setAng` (its target has the other order). Same
  lever whenever a store that depends on a call result must precede an argument `addi` that a free `lwz r3,this` beats.
- **Memory-input asms as position anchors (r224 R224Main 9 -> 0, gnd_close 2 -> 0, both tagged COMPILER-DIFF: 13).** An asm
  with an input operand is scheduled after that operand's producer, and its output pseudo's live range starts there:
  - R224Main: `u32 v = pG->flags_174 & ~m; pG->flags_174 = v; asm("li %0,0" : "=r"(zero) : "r"(v));` for the stack-argument
    zero of `ActBtn.set(..., 0, zero)`. The zero is born after the RMW value, so local-alloc gives it r0 after the rlwinm temp
    dies (target `li r0,0; stw r0,8(r1)` after the store, `lwz r9,pG` instead of r11 because r9 is no longer taken by an early
    zero). An `"m"(pG->flags_174)` input orders it after the STORE instead and swaps `li r9,1`/`li r10,0` (2 words).
  - gnd_close: `cObj* o = SmdGetObjPtr(0x14); o->rot.x = 0.0f; asm("li %0,5" : "=r"(five) : "m"(o->rot.x));
    asm("li %0,0" : "=r"(zero) : "r"(five)); SceAtSetEnable(five, zero);` -- both argument `li`s wait for the `stfs` (the
    target's `stfs; li r3,5; li r4,0`); an `"r"(o)` input instead keeps `o` live across the asm and copies it (`mr r9,r3`).
    A bare `asm("li %0,5" : "=r"(x))` (no inputs) is a constant for gcse and is hoisted to bb 0 into a callee-saved register.
- **The pool-constant reload family, half-closed by a named `static const` + asm loads (r224 reva_common_move 32 -> 12, tagged
  COMPILER-DIFF: 12):** `static const f32 k0 = 0.0f;` inside the function takes the pool word's .rodata slot (emitted at its
  declaration, before the function's pool); `asm("lis %0,%1@ha" : "=b"(h) : "i"(&k0)); asm("lfs %0,%1@l(%2)" : "=f"(x) :
  "i"(&k0), "b"(h))` loads it opaquely (cse2 cannot fold the compare constant into spd's register), and a plain `FCRef(k0)`
  read (r10c's `static inline f32 FCRef(const f32&)`) inside a conditional arm gives the target's loop.c-hoisted `lis r28` with
  the `lfs` left in the arm. Facts learned: `FCRef`'s MEM is not `/u`, so loop.c never hoists the LOAD (the compare constant
  must be the asm, placed before the loop); a `const f32*` deref is not `/u` either; and haifa's "don't let a pseudo cross a
  call after scheduling if it doesn't already cross one" (sched_analyze_1/2, REG_N_CALLS_CROSSED == 0) is what keeps a
  loop.c-hoisted `lis` after the preceding call while an asm `lis` whose output is used in the loop floats to the function
  top -- write the asm `lis` where the register is consumed (the reload's high must stay compiler-generated). Residue 12:
  the preheader schedule (`lfs f30,acc` last in the target, our asm `lfs zero` last), f26/f27 and r28/r29 pairs -- with the
  reload's high now a single loop occurrence it is a loop.c movable (after pG's) instead of gcse's R insertion.
- **`duplicate_loop_exit_test` runs in TWO jump passes: jump1 (pre-cse, exit code counted on the raw expansion) and the
  `JUMP_AFTER_REGSCAN` pass right after loop.c (before cse2).** The 20-insn limit is applied to the RTL of that moment, and a
  jump1 peel makes loop.c IGNORE the loop ("multiple entry points": the copied `bge L_sleep` enters the loop body). r214
  throwRock 68 -> 7: loop 2 must NOT peel at jump1 (raw exit code > 20: `c->obj->pParts->rot.x += spd` expands to two address
  chains, the literal `spd += -0.0349f` adds a pool load -- 22) but MUST peel post-loop (hoisted constants, 12 insns), and the
  store constant must be a pseudo distinct from the compare's so jump2 cannot cross-jump the peel back: compare against
  `lim` (set before the loop), store the LITERAL `-0.24137f` (cse2 then turns the loop copy's hoisted pool pseudo into the
  target's `fmr f28,f30`). `acc` is kept only as a mid-block `const f32` for the pool order. Loop 3 is the target's
  sleep-first goto form (`goto body; sleep: SceSleep(1); body: ..; if (!(c)) goto sleep;`), no peel. Residue 7: `cmplwi`
  (u32 `i`; an `int` counter is reversed by loop.c: `li 3; addi -1; cmpwi 0`), the preheader `fmuls/addi` order and the
  r9/r11 naming of the loop-3 body loads.
- r22a RopeMove (4): sched2 ranks the word-8 load higher because `stw r9,8(r31)` anti-depends on the later `lwz r9,pG@l(r24)`
  (prio 47 vs 46); the DAG is identical in the target (same registers), so the original's sched2 input order differed
  upstream. Copy order / u32-view / pointer-copy forms: 15-107. r11b Init, r213 StatusSetChain (24 store permutations + an
  `fr11` pin: 7-12), r21a FallRoofDie (the PRE insertion block is fixed by the camAt template copy's antloc in bb 1: lcm.c
  `delayout = delayin & ~antloc`), r120, r225, r10c not moved.

### Stage rooms, st1_1/st1_3/st2_0 pass 5 (r113, r11d, r203 Matching; r118 2, r105/r103/r11c #9 mechanism read; 2026-09-10)

- Harness: /tmp/rooms_b5 (rooms_b4 copies with the paths rewritten; `out/` and `dump/` are symlinks into
  ~/.cache/rooms_b5 because /tmp (32G tmpfs) was full -- `export TMPDIR=~/.cache/tmp` before running
  ngccc.py/cc1plus when `df /tmp` shows 100%; the zsh "write failed: no space left on device" lines are harmless).
  ~/.cache/rooms_b5/sngcc is a cc1plus built from tools/sn-gcc with gcse.c `MAX_PASSES 2` (`xcc.sh` uses it):
  NEGATIVE, identical word counts on r105 execOpenCover and r11c closeGate in every source form -- a second PRE pass
  is not what the original did; nothing installed.
- **The arg-`li` family closed (r113 execHide 5 -> 0, r11d execHide_main 6 -> 0): the insn after a LOOP_BEG/LOOP_END
  note is a full sched1 barrier** (haifa `sched_analyze_insn`, `schedule_barrier_found`: it gets a TRUE dependence on
  `reg_last_sets[i]` of EVERY register). When a call is followed by a real loop entered by a jump (`call; LOOP_BEG; b
  START`), the entry jump depends on the call's `li r4..r8` (their registers' last sets) but NOT on `li r3` (r3's last
  set is the CALL itself, which re-sets it), so `li r3,K` has one dependent fewer and is issued last -- the target's
  "li r3 after lfs spd" order in the mode-0 arm (the mode-1 arm's goto loop has no notes and keeps LUID order, which is
  why the same call matched there). A goto loop has no notes -> LUID order (ours). So the original's open arm was a real
  loop: `SndCall(..); for (;;) { rot -= spd; asm("" : "+f"(spd)); spd += add; if (rot < lim) break; SceSleep(1); }`.
  The asm (tagged `// COMPILER-DIFF: candidate #9`) only stops jump1's `duplicate_loop_exit_test` (`asm_noperands > 0`
  in the exit code refuses the peel; the other refusals are a CALL_INSN, a CODE_LABEL, a LOOP_BEG/LOOP_CONT note or
  more than 20 insns between the test label and LOOP_END -- ours has 18 here; inline helpers with a return label
  refuse it too but leave `li r0`/`cmpwi` behind, `&&` adds a compare). The loop is then "phony" for loop.c (`-dL`:
  gcse PRE's `high(add)`/`high(lim)` insertions at the end of the preheader block land AFTER the LOOP_BEG note, before
  the `b START`, so `scan_loop`'s first insn is neither a label nor the entry jump) and nothing is hoisted -- the
  target's per-iteration `lis/lfs` reloads, previously read as "goto loop".
- Source-logic fixes found from 1-2 word diffs (check the branch targets with mcmp before any lever): r11d
  execHide_main's open arm ends with `rot.x = lim` (the target's exit jumps to the close arm's `stfs f12,0xa0(r9)` =
  cross-jumped final store); r11d execEmAppear_end declares `cEmWrap em;` INSIDE the 11-loop (the ctor `bl __7cEmWrap`
  is at the loop label) and destroys every enemy except the one being taken away: `if (!(em.isAlive() == 1 &&
  ckTakeAway() == 1)) em.destroy();` (was `if (alive) { if (takeAway) break; destroy; }`: a `bne`/`beq` pair 8 bytes
  off). Unit 18 -> 21/21, flipped.
- **r203 EventMeetAgain 26 -> 0 (unit Matching)**: (1) `Vec* pp = &pos;` declared at the USE (in the block after the
  `if (waitLoadOk) {..}` join, next to `pl->setPos(pp)`), not at the top: the join block's `pp = fp+8` is a fresh
  occurrence that gcse PREs (`mr r27,r11` = R copied from the template copy's pointer P right after `addi r11,r1,8`, `mr
  r4,r27` at the call); at the top, cse1 folds `pp` into P and cprop removes the copy (`addi r28,r1,8` used directly).
  A hard-register arg (`setPos(&pos)` -> `(set (reg 4) (plus fp 8))`) is never a gcse occurrence (pseudo dests only).
  (2) The callee-saved permutation (m/ry r29, &ang r31, pPL high r28, R r27, 0.0 f31) is #17 with TWO pins:
  `register int pin asm("r29"); register f32 fpin asm("fr31"); asm("" : "=r"(pin)); asm("" : "=f"(fpin)); asm("" : :
  "r"(pin), "f"(fpin));` placed AFTER the last call (the input-only asm is a sched barrier; at the top it reorders the
  template copy, 23 words). Why both: REG_ALLOC_ORDER lists the FPRs before the GPRs, so a NON_SPECIAL_REGS store-only
  constant (0.0, prio 0.263, first) takes a used-so-far GPR in pass 0 unless an FPR is used-so-far too; with f31 pinned
  0.0 -> f31, m and ry (non-overlapping) -> r29 in pass 0, &ang (conflicts with both) -> r31 in pass 1.
- **r118 ThunderMove (2, left)**: `li r28,0` (`void* zero`) vs loop.c's hoisted `0x88888889` pair (`lis; ori`, split
  from one `movsi` at sched1, same pseudo, no death -> weight +1 like the li): prio 1, weight 1, 0 dependents both; LUID
  decides and the hoisted pair sits at the preheader END, after every source insn. The target issues `ori` in the
  `bl SceSleep` cycle and `li r28` last, i.e. its zero came after the hoist (a pass-2 hoist or a later insertion). A
  block-local `void* zero = 0` in the EstSet arm folds into the `andi.` result (#12 (b)); at the body top it is hoisted
  before the constant (scan order). Not found.
- **#9 family read (r105/r103 execOpenCover 25/27, r11c closeGate 21; all left)**: the target is jump1's peel PLUS
  jump2's conditional cross-jump (jump.c `cross_jump && condjump_p`: `x = prev_real_insn (JUMP_LABEL (insn))` must be
  an opposing jump back to the label right after `insn` (`jump_back_p`), then `find_cross_jump (insn, x, 2)` compares
  the insns BEFORE both jumps): the peeled copy `lwz r9,pParts; lfs f0,rot; fcmpu f0,f13; blt END` before `TOP:` matches
  the loop's `lwz; lfs; fcmpu; bge TOP` (3 insns; the `stfs` bases differ), the copy is deleted and `blt END` becomes
  `b .L` with .L placed before the loop's `lwz` = the target's `sub; b test; L: sleep; sub; test: bge L`; the peel
  also deleted the original `b TEST` so body and test are ONE block (the body's `lis r9; lfs f13,lim` is the test's
  load scheduled up), and the copy's `lfs f13,lim` in the preheader is the dead remainder of the deleted compare. Ours
  peels the same way (`while (1) { sub; if (rot < lim) break; SceSleep(1); }`) but loop.c then hoists `lim` and
  `step` into f30/f31 (`fmr` copies after the peeled compare), so the loop's compare reads f31 and the copy's f13 -- no
  match, no cross-jump. What stopped the original's loop.c is not known (a phony loop as in execHide needs an
  insertion after LOOP_BEG, which the peel layout does not give; `for (init; cond;)`, `while (!(rot < lim))`,
  hand-peeled `if (c) do {} while (c)` forms: 21-54 words). The tagged asm form (no peel, phony loop) gives 15-16 words
  with `lim` reloaded in the test block instead of both predecessors -- not applied. r11c closeGate is the same peel +
  cross-jump on a compare of the pseudo `y = pos.y - spd` (`f32 y` variable, `y` stored and compared: the body reuses
  f0 for both); the pre keeps `fcmpu; stw; lfs; stfs; b .L` and `.L: bge TOP` alone. Note CCFP compares cannot be PRE'd
  by gcse in our build (`can_copy_p[CCFPmode] = 0`: rs6000.md's movcc is CCmode only), so the compare-in-both-preds
  shape is not PRE. `if (!(y < dst)) { top: SceSleep(1); y = ..; if (!(y < dst)) goto top; }` (peel_goto) gives the
  target modulo `blt END` vs `b .L` and the f30/f31 swap (10 words); with loop notes (`peel_do`) the pre matches and
  the body hoists `acc` (17).
- r101 Event20 (2, left): the four PRE insertions at the end of bb 6 (`&win`, `&pos`, `high(pPL)`, `&ang`, prio 2,
  1 uniform dependent each, LUID = expression index = first-occurrence order) fill the free slots in LUID order; the
  target's order (pPL high, &win, &pos, &ang) and its `addi r26,r1,24` BEFORE `li r3,21` (prio 4) need `&ang` ranked
  above an argument load -- a dead `do {} while (0)` or `Vec* pa = &ang` before SearchEmModule: 10/2 words.
- r103 openShelf_main (4, left): the two pool highs `ra`/`rb` r9/r11 swapped = local-alloc qty order under the same
  sched1 order (span A 3 vs B 2, QTY_CMP_PRI refs/span); statement swap, cModel* locals, FSet, const, literal,
  declaration order: 4-16 words.

### Stage rooms, never-iterated units pass 5 (r224 Matching 18/19 with reva_common_move 12 -> 0; r214 Matching 24/25 with throwRock 7 -> 0; r221 throwBonbe r21/r22 tie flipped in variants (18 -> 2) but not applied; 2026-09-10)

- Harness ~/.cache/rooms_c7 (rooms_c6 copies with the paths rewritten; `tryv.py`, new `tryv2.py MOD/UNIT FUNC
  variants.py BASE.cpp` = tryv applied on a variant file, `prio.sh VARIANT.cpp FUNC_RE [MINLEN]` = the global-alloc
  priorities `floor_log2(refs)*refs/len` of a variant's long-lived pseudos from its -dl dump, `liverange.py DUMP FUNC REGNO`
  = the post-sched1 positions (real-insn index) of a pseudo's sets/uses; `mdump.sh` dumps: sched2 is `-dR` (not a second
  -dS), the dependence/priority tables need `-fsched-verbose-N` with a DASH (`=N` is silently ignored: only ready lists)).
- **A `static const` one-member struct is a pool word cse cannot fold and a `/u` MEM sched1 does not order after stores**
  (r224 reva_common_move 12 -> 0, unit flipped; `static const struct { f32 v; } k0 = {0.0f};` at the function's top for the
  pool slot, read as `zero = k0.v`). The front end folds only `TREE_READONLY_DECL_P` scalars (`decl_constant_value` refuses
  CONSTRUCTOR initialisers), so the read stays a MEM of the named `.rodata` word; varasm gives a `const` decl's DECL_RTL
  `RTX_UNCHANGING_P` and `change_address` keeps it for the COMPONENT_REF, so alias.c treats the load like a pool load (no
  true dependence on a preceding store through an unknown pointer, unlike the r10c `FCRef(const f32&)` reference deref,
  which waited for `stw be_flag` and was issued after it), and cse does not fold it to its CONST_DOUBLE (only
  CONSTANT_POOL_ADDRESS_P MEMs are). What it does NOT give: a `REG_EQUAL (const_double)` note, so the pseudo has no
  REG_EQUIV and its live length is not doubled (a real pool constant's is) -- see the next item. The asm `lis/lfs` pair
  stays for spd's init (a plain `k0.v` there would let cse copy `zero` from `spd`: the `/u` entry survives the call).
- **`BitOn(obj->be_flag, 0x20)` where a following fixed-scalar load must wait for the store** (same function): the target's
  `lfs f30,acc` (a global scalar) is issued last in the preheader because it depends on the be_flag store; a struct-member
  store (`MEM_IN_STRUCT_P`, varying address) never aliases a fixed scalar (`fixed_scalar_and_varying_struct_p`), a
  reference store does. Same rule as the pG reloads of global.h.
- **Pool-constant vs named-word global-alloc tie: a dead FP test as the 4th ref** (same function, `if (spd == 1.85f) up = 0;`
  at the loop body's end, tagged COMPILER-DIFF: 12): the hoisted 1.85 pool pseudo (REG_EQUIV, length doubled) had 3 refs
  / 94 vs zero's 3 / 50, so zero took f27 first; the dead compare (loop-variant `spd`, so loop.c does not hoist it -- a
  dead `acc == 1.85f` compare IS invariant, is hoisted into the preheader and reorders the `lfs`es) gives 1.85 four refs
  (`2*4/len`) and the target's f27; `up` is re-set at the loop top before every read. Both loads keep their target order
  because zero's `/u` MEM has no store dependence and the pool load is a loop.c movable emitted after the preheader
  statements (LUID order zero, 1.85).
- **`for (i = 0; (int) i < 4; i++)` on a u32 counter = the target's unreversed signed `cmpwi r31,3; ble`** (r214 throwRock
  7 -> 0, unit flipped): a plain `int` counter is reversed by loop.c (`check_dbra_loop`: LT compare, constant bounds,
  `no_use_except_counting`), a u32 gives `cmplwi`; the cast keeps the biv unsigned for loop.c while the compare is signed.
  Same function: the inner wait loop as a plain `for (;;) { body; if (c) break; SceSleep(1); }` -- expand_end_loop rotates
  it into the target's sleep-first layout (`b body; sleep: bl; body: ..; bge sleep; bge cr7 sleep`) and, being a real
  loop.c loop inside the counted outer loop, its 0.0 compare constant is hoisted only to the outer body's head block
  (target `li r31,0; lfs f29; fmuls; fmuls; addi` = the head block starts at the `lfs`), which the hand-written goto form
  of the earlier pass could not give (the goto inner loop has no notes: both constants went to the outer preheader and the
  head schedule was `fmuls; addi; fmuls`). The body loads' r9/r11 naming followed from the same change.
- **r221 throwBonbe (18, NOT applied -- 2-word variants only).** The eff2 (6 refs / 282) vs pG-high (13 / 870 doubled) tie
  flips with `eff3 = 9; eff2 = 1;` in case 0 ONLY (eff2 len 254, prio 0.0472 in the (0.0448, 0.0479] window; the same swap
  in cases 1-3 shortens by 2 each and the four-arm swap by 28 + 2 + 2 + 2 overshoots to 248): arm 0's `li eff2` moves
  from sched1 position 61 (the slot sharing the RsfSet call's cycle) to 72 and the length drops 28. Residue 2 words: sched2
  then also emits `li r17,9` (eff3) in that slot and `li r22,1` later -- the free `li`s fill the slots before/after the
  `bl` in LUID order = sched1 order, and sched1 orders them by LUID too, so "eff2 late in sched1, early in sched2" needs a
  sched1-only tie-break: register weight. `asm("li %0,9" : "=r"(eff3) : "r"(no))` (no dies there: weight 0) makes sched1
  issue eff3 FIRST of all free `li`s (before atNo/eff1, 4 words), a dying input available only after the call does not
  exist in arm 0 (the RsfSet word pointer is inline-internal). Other negative results: a `"=m"` keep-alive
  `asm("" : "=m"(pos.x) : "r"(eff2))` after `SceAtSetEnable(atNo, 1)` gives eff2 the window (10 words: only the mask
  pseudo changes) but the PRE'd `clrlwi` mask is born at the join-block end while eff2 now dies at the asm, so the mask
  takes r14 (+frame); dead tests inside the 300-loop (`if (eff2 == 0) t1 = 0;` etc.) are PRE'd to the join as a compare
  kept in a callee-saved CR copy (30-63 words, frame +8); a dead pG test at the tail (`if ((int) pG->flags_174 < 0) i = 0;`)
  does not add a ref to the high (cse2 re-materialises a fresh `lis r9`, 18-33 words); `default: eff2 = 0;` and moving
  `eff2 = K` after `t2`/`t1`/`mot0`/`pos.x` in arm 0: 3-35.
- **Hard-register FPR pins for pool constants reorder sched1** (r213 StatusSetChain: `register f32 c50 asm("fr13")` etc.
  for the four constants, or fr11 for 0.0 alone: 12-25 words). The local-alloc order there is 0.0 (3 refs / 60) > 0.8 =
  0.1 (2 / 42) > 50.0 (2 / 44); the target's is 0.8 > 50.0 > 0.1 > 0.0, i.e. 0.0's span must exceed 1.5x the others'.
  Not resolved. r11b Init (14), r22a RopeMove (4), r21a FallRoofDie (66), r120 (114), r10c, r225 not moved this pass;
  for r21a the target's shape is now read exactly: `Vec* op = &obj->pos` (one pseudo r28 for both SndCalls), the camAt
  template copy's base IS the &camAt pseudo (r29) with the PRE copies `mr r27,r29; mr r26,r30` staying after SndCall,
  the 10-loop's 0.0 is `lwz r28,pool` (an SF pseudo in a GPR: pass 0 finds r28 = the dead &obj->pos register used so
  far) and `li r28,60` reuses it for the 60-loop.

### Stage rooms, st4_0/st2_1 pass 4 (r402 Matching 19/19; r20d execRoundSwitch 1 -> 0 (27/32); r404/r40f/r209/r204/r20e unchanged; 2026-09-10)

- Harness /tmp/rooms_a4 (rooms_a3 copies + /tmp/em3c29/lcm.py, rooms_b4 galloc.py; `tryv.py MOD/UNIT FUNC variants.py
  [--asm N] [--apply N]`, `mm.py`, `sbs.sh`, `mdump.sh MOD/UNIT -dX` with `SRC_OVERRIDE`).
- **Zero-code transparency kill for the #3 latch/preheader PRE (R402MoveDoor02 15 -> 0, unit flipped).** gcse's
  `compute_transp` uses `reg_set_in_block` for HARD registers too (`record_last_set_info` runs `note_stores` over every
  insn incl. CLOBBERs), and before reload the frame is `(reg 31)`. `asm("" : "=r"(obj) : "0"(obj) : "r31")` in a block
  B kills every `(plus fp N)` expression in B: with the kill in a block between the inner-loop head and the outer latch
  (here the eat arm, bb 9: `earlyout[9] = ~transp[9] = 1 -> earlyin[10] -> earlyin[2] = 1 -> delayin[2] = latein[2]`,
  redundant[2] = 0 in lcm.py's model) the `&id` occurrence stays in the inner body and loop.c pass 2 hoists it to the
  inner preheader with the LUID the target has (`addi r24,r1,0x28` between `li r31,0` and `cmpwi cr4`: pass-2 movables
  are emitted after pass 1's giv inits). Rules learned on the way: (a) the asm must be non-volatile and its output must
  be a pseudo with other refs (`obj` is dead there; jump1 deletes a set whose reg is referenced only by that insn,
  cse1's delete_trivially_dead_insns deletes it when `count_reg_usage` (which does not count a use of the SET_DEST
  inside SET_SRC) reaches 0, cse1 folds a constant/copy input into the asm operand first); flow1 deletes the dead asm
  before sched1/regalloc, so it leaves no trace; (b) placed at the TOP of the outer body it works for gcse but the
  clobber gives every fp-reading insn of the block a true dependence on the asm (priority 2, an issue slot at t=1 ->
  `&id` issued third: 10-13 words); (c) a volatile asm in the inner latch (+1 real insn) pushes loop.c pass 2 over the
  `threshold -= 3` step so the `cmpwi cr4,dir,1` movable stays in the loop (37 words) -- put the asm right after a call
  that precedes a label, where it REPLACES the flow nop `(use (const_int 0))` and the loop's real-insn count is unchanged
  (68 in both passes here); (d) the kill in bb 11 (after the inner loop) gives `earlyin[1]` and a PRE insertion at the
  END of bb 1 = the LUID before loop.c's inits (`&id` issued second). Tagged `// COMPILER-DIFF: 3`.
- **Loop notes as a register-weight lever (the same function's r22/r23/r24 permutation, 7 -> 0).** flow's
  `REG_N_REFS += loop_depth` counts LOOP_BEG/END notes only, so a goto loop's body weighs its refs at depth 1 and the
  target's allocation (`&id` 5 refs, `t+1` 4, `frames` 7 -> r24/r23/r22 by `floor_log2(refs)*refs/len`) needed depth 2:
  `top: do { body; if (t <= 40) goto top; } while (0);` -- the `do {} while (0)` around the whole goto-loop body is phony
  for loop.c (its label is unused, scan_start is not a CODE_LABEL) and still no loop.c hoist of the body's highs, but
  the notes double the body's ref weights. The goto must stay INSIDE the do-while (a `goto` from after `while (0)`
  jumps into the loop: 12 words; the label inside the do-while with the goto after it: 41). A `#17` r22 pin at the
  function end does not work here: pass 0 hands the used-so-far r22 to the first allocno without a conflict (`i*4`).
- **`asm("addi %0,%1,0xa0" : "=r"(rot) : "r"(pl) : "cc")` for the regmove operand pick** (r20d execRoundSwitch 1 -> 0,
  tagged `COMPILER-DIFF: 12 (regmove operand pick)`): the `addi r4,r3,0xa0` after `mr r3,r30` is NOT reload_cse but
  combine + regmove: combine merges `P = pl + 0xa0` into the arg move `r4 = P` (placed after `r3 = pl`), pl's death moves
  to it and regmove's `optimize_reg_copy_1` replaces pl by r3 there. A plain asm is combined into the move the same way
  (1 word) and a volatile one is a barrier (2); the "cc" clobber makes it a PARALLEL that combine leaves alone and gcse
  does not hash (`hash_scan_insn` records only SETs of single-set insns; PARALLEL members are scanned for calls only).
  The same PARALLEL trick keeps an asm out of gcse's PRE elsewhere (r404 below).
- **r404 initEmSet 15 (not applied, 9 with a tagged form).** Two `mulli n,12` exist in the target because the first is
  PRE-hoisted from bb 4 (after the rot template loop) to bb 2 (between the two block-copy loops -- both `Vec[3]`
  initialisers are real CFG loops, `subic. r8,r8,0x18; bne`) exactly like ours, and the second is a mult gcse did not
  see. `asm("mulli %0,%1,12" : "=r"(m2) : "r"(n), "m"(pPL))` for the rot chain reproduces both mults and the target's
  `lis r9,pPL@ha; mulli r30` order (the MEM operand gives the asm a dependence on the pPL high; an asm without it is
  PRE-hoisted like the real mult, a volatile one is a barrier, `"m"(pos[0])` waits for the frame stores); the add
  operand order `add r4,r4,r6` / `add r30,r30,r6` needs the integer sum written mult-first: `n * 12 + (u32) &pos` and
  `m2 + (u32) &pos + 0x28` (expand_binop swaps a non-REG op0 behind a REG op1; a pointer sum `(u8*) &pos + n*12`
  force_operands the address first and gives `add rD,r6,rM`). Residue 9 = bb 4's issue order (`lwz r29,pPL` before the
  asm mulli: the asm has cost 1 and weight +1, the real mulli would have cost 2). Zero-code forms tried: `u8` re-extension
  of `n` (an inline `u8` parameter, `u8 m = n`) is folded by cse1; `int`/`u32` casts, `Vec* p = &pos[n]` between the
  arrays (the mult then lives across the rot copy: 58-80); a launder of `n` (kills the PRE of the switch compare `cmpwi
  cr4,n,1` too: 21).
- **r209 R209Main 124 (not applied).** The em3c model's "extra edge" is confirmed as source-writable: `if (i != atNum)
  break;` after the inner loop (never true) gives the j loop an exit edge from the j body, `j+1` stays at the latch and
  loop.c forms the target's outer givs (`addi r25,4`, `addi r23,8`) -- 124 -> 69 -- but the test survives (loop.c
  computes `Final biv value for 82` yet does not insert `i = 8` after the loop because the biv is not eliminable: pass 2
  says `Cannot eliminate biv 82: biv used in insn 435` = the duplicated entry test `cmplwi i,8` that jump1's
  `duplicate_loop_exit_test` copies INSIDE the loop range for every `for`; `do {} while (i < atNum)` removes it but the
  giv `8 + j*8 + i` splits into `i + 8` (not worth reducing) and blocks `all_reduced`, so `i` is never eliminated and the
  inner compare stays `cmplwi r28,8` instead of the target's pointer compare `cmplw r31,r25`). The target therefore had
  (1) no entry-test copy or a foldable one, (2) one giv for the bit index, (3) the phantom exit edge. `u32 bit = 8 +
  j*8` / `i + (8 + j*8)` / do-while forms: 99-128.
- r209 Switch/BridgeAppearCheck (7 each, unchanged): the `stw seId` -> `lwz obj->be_flag` dependences are real
  (alias sets are 0 on every MEM: SN's `c_get_alias_set` returns `DECL_POINTER_ALIAS_SET(t)` for every decl -- the
  `//QAZ` block's `return` is outside its `if (DECL_NOALIAS(t))` -- so only `__attribute__((noalias))` decls get a set,
  and the C++ front end does not apply it to locals: `cObj* obj1 __attribute__((noalias))` leaves the sets 0); with
  them the work chain has sched1 priority 19 vs 5 for the `lis RO(lim)` chain, so `lis work` first is forced. `int
  seId` (alias-set idea) changes nothing. The target's order needs a shorter work chain or a longer RO chain.
- r40f BombSet 5 (unchanged): sched1 weight tie of the p0 template's word-1/word-2 loads (word 2 kills the lo_sum base
  -> weight 0 -> first in ours; the target has word 1 first for p0 and word 2 first for p1 with identical RTL shapes);
  in sched2 word 2 also outranks word 1 (its store has an anti-dependence on the following `lwz r3,work`). Pointer
  locals, `cEmWrap&` views, declaration orders, dead do-while, launders: 5-62.
- r20d checkSwitch 1 (unchanged): cse1 reaches the down-loop's tail through the AROUND path of `blt tail` (`skip_blocks`:
  `q` = the LOOP_END note is not a CODE_LABEL and there is no label between the jump and the target), so the tail's
  `move(0.0f)` becomes `fmr f1,f28` from the loop body's 0.0 pseudo; the target reloads the pool word, i.e. its tail
  label had a second use or a label in the skipped block. moveWall 3 (unchanged): `li r31,0` (i = 0, priority 1, no
  in-block dependents) is issued first in the target's post-call block and third in ours.
- r204 EventChandelier1/2 (94/96), nige_check (228), r20e initPuzzle (103), checkPuzzle (224), cFence20e::move (10),
  r20d throwLantern (31), operateCrank (68), execThrough (62) not iterated this pass.

### Stage rooms, st4_0/st2_1 pass 5 (r404 Matching 38/38 + st4_0 module flag; r209 R209Main 124 -> 9 (58/61); r40f BombSet 5, r209 Switch/BridgeAppearCheck 7+7, r20d checkSwitch 1 / moveWall 3 mechanisms read, unchanged; 2026-09-10)

- Harness ~/.cache/rooms_a5 (rooms_a4 copies with the paths rewritten, `lcm.py`; new `vsbs.sh MOD/UNIT FUNC
  VARIANTS.py NAME` = side-by-side target | variant asm with labels/pool offsets masked -- dtk's labels on an unlinked
  variant object are unreliable, judge layout by `OBJ=out/v_UNIT_NAME.o python3 mm.py MOD/UNIT FUNC`).
- **Two real `mulli`s from one index, zero code (r404 initEmSet 15 -> 0, unit flipped): re-set the holder of the first
  product before the second multiplication.** `u32 t = n * 12; t += (u32) &pos; Vec* b = (Vec*) t; t = n * 12; t += (u32)
  &pos; t += 0x28; Vec* r = (Vec*) t;` -- cse1 keeps the second `(mult n 12)` because its class has no valid register any
  more (`t` was re-set by the `+=`), gcse's PRE deletes only the block's antic occurrence (the first one) and inserts
  `R = n*12` between the two template-copy loops (the target's `mulli r4`), the second stays a real mult in bb 4. Every
  copy/launder form costs an `mr` (both values live), a hard-register destination is expanded through a pseudo copy, a
  `"cc"`-clobber asm has cost 1 (the real mult has 4) and issues in the wrong slot. Using ONE variable `t` for both
  chains adds the output/anti dependences that put `mr r3,pl` before `addi r30,r30,0x28` (two separate variables: 2 words);
  `cPlayer* pl = pPLS` (struct view) keeps the pPL load below the first template store (7 -> 2 words).
- **R209Main 124 -> 9, all zero-code:**
  (1) a `for` over a VARIABLE bound (`u32 atNum = 8` set in bb 0) keeps jump1's duplicated entry test `cmplw i,atNum`
  through cse1 (atNum is unknown in that ebb; gcse's cprop then substitutes 8 but does local propagation only from
  other blocks, so `i = 0` two insns earlier is not used and the jump survives until cse2) -> gcse sees a path around
  the loop, `high(pPL)` (used in the loop and once more after it) is not anticipatable at the earliest block and is
  only loop.c-hoisted into the loop's preheader; the target has it PRE'd before the FIRST loop. `i = 0; do {...; i++;}
  while (i < atNum);` (no entry test) gives the target's placement -- so where a matched function has a `lis` hoisted
  above an earlier loop, that loop had no foldable entry test. (`const u32 atNum` folds the test too but turns the
  bottom compare into `cmplwi 7; ble` and hoists the inner loop's invariants a level further: the inner loop's test
  must stay.)
  (2) The inner loop's bit index as its own counter (`u32 bit = 8 + i*8;` in the outer body, `for (j = 0; j < atNum;
  j++, bit++) ... R209_BIT_ON(flags, bit)`) leaves `j` with only the `atNo[j]` giv -> all givs reduced, `j` eliminated
  into the target's pointer compare `cmplw r31,r25`. Written as `8 + j*8 + i` inside the loop, fold reassociates to
  `(i + 8) + j*8`: the intermediate `i + 8` is a DEST_REG giv with benefit 2 (`benefit -= add_cost * biv_count` = 0,
  "not worth while") that blocks `all_reduced`, so `i` is never eliminated (loop.c reduces only givs with >= 2
  operations on the biv path). (3) The OUTER counter is `i`, the same variable as the other loops (pl0f rule
  confirmed: `i + 1` stays a latch biv and is not PRE'd across the inner loop, so the `j*4`/`j*8` givs form without
  any phantom exit edge), and `j` (dead in the other loops) shares the counter's register r27 like the target.
  (4) `u32* f = flags; f[2] |= X;` (the pointer variable becomes a fresh `(plus fp 8)` occurrence -> gcse copy of the
  reaching reg -> `lwz r0,8(r20)`; `flags[2]` expands to `(plus fp 16)` directly). (5) `(pG->flags_174 & A) &&
  (pGS->flags_174 & B)`: fold_truthop merges two bit tests of the SAME operand into one `rlwinm; cmpw`; the struct
  view keeps the target's two `andis.; beq` (cse merges the loads).
  Residue 9: `addi r29,r23,8` (bit init) is issued before the loop.c movables in ours (a source insn precedes
  LOOP_BEG) and after `lwzx` in the target, and the bit-set block's idx/shift-amount take r0/r9 the other way round
  (local-alloc: REG_N_REFS x3 at loop depth 3 makes floor_log2(9)*9/6 > floor_log2(6)*6/3; at depth 2 or 4 the shift
  amount wins as in the target and in loop 2). Both fit a target whose `bit` was a loop.c-REDUCED giv (init emitted
  after the movables, body at depth 3 with the same lens) with benefit >= 3 and no not-worth intermediate -- which our
  fold/loop.c cannot produce from any expression tried (`i + (8 + j*8)`, bit variable, `(j << 3)`, do-while: 42-128).
  `-freduce-all-givs` would reduce the intermediate too (then dead); not tested whole-tree.
- **r40f BombSet (5, unchanged; mechanism read).** The p0 word-1/word-2 order is decided in sched2, not sched1: anti and
  output dependences cost 1 in this haifa (`insn_cost`: `if (ncost <= 1) LINK_COST_FREE = ncost = 1` after
  rs6000_adjust_cost returns 0), so `stw r3,8(r11)` (word 2, temp r3) gets +1 from the anti-dependence on the following
  `lwz r3,work` and its load outranks word 1 (12 vs 11); sched1 does put word 2 first by weight (base dies there) and
  the target's register choice (w1 r7, w2 r3) shows its sched1 order was the same. The target's order needs the w1 chain
  >= the w2 chain in sched2 with these registers, which no LUID permutation of the same insns gives. Tried: static const
  templates, a two-set template pointer, `tbl[2]`, pointer/asm anchors on the template base, declaration and call
  placements (5-62).
- **r209 Switch/BridgeAppearCheck (7 each, unchanged; mechanism read).** The target's `lis RO; lis work; lfs f0; lwz
  r9,work; stw seId` order is register reuse: the RO high and the work POINTER are both r9, so sched2 chains `lis r9,RO
  -> lfs (anti) -> lwz r9,work (output) -> stw` and the RO chain inherits the work chain's priority. That needs
  local-alloc to allocate the RO high before the work high (equal QTY_CMP_PRI, 2 refs each, both born at the same
  sched1 cycle -> qty number = sched1 issue order), i.e. sched1 must issue `lis RO` first, but the `stw seId -> lwz
  be_flag`/`lfs pos` alias chain (work pointer base 0 from `find_base_value (MEM)`, objs base `(reg 3)`;
  `base_alias_check` returns 1 for a zero base) gives the work high priority 19 vs 5. No source form breaks that chain
  while keeping the store through the loaded pointer (a fixed-scalar store would need a global, `noalias` never reaches
  locals).
- r20d checkSwitch (1): cse1's skip_blocks path into the down-loop tail also requires `LABEL_NUSES == 1` on the tail
  label; a dead second `break` (`if (open != 0) break;`) is NOT folded before sched (gcse cprop keeps it as a cr4 compare:
  20-43 words), loop forms (`for/while/do` + break, goto loop, tail inside the arm, dead do-while around/before the
  tail) 1-109. moveWall (3): `li r31,0` (i = 0) first in the target's post-call block needs a dependent with the
  pPL-load chain's priority; an asm `li r4,1` anchored on `i` only reaches prio 3 (not tried in bytes).

### Stage rooms, st1_1/st1_3/st2_0 pass 6 (r118, r101, r105 Matching; r103 18/19 in both modules, r202 28/32; r11c closeGate and r106's doors classified #7 = CCFP PRE; 2026-09-10)

- Harness: ~/.cache/rooms_b6 (rooms_b5's `/tmp` copies were gone; rewritten from scratch: `cc.sh SRC MOD OUTDIR [cc1plus
  flags]` = SN cpp.exe + native cc1plus + NgcAs with the module's flags, dumps land in OUTDIR; `cmp.py OBJ MOD/UNIT [SYM|ALL]
  [--trunc]` = word-masked compare of our object against `build/G4BE08/<mod>/asm/<mod>/<unit>.s` (masks ADDR16/REL24
  fields, resolves NgcAs' REL14 fields; `--trunc` for a folded build object whose last function carries the linkonce
  block); `tryv.py SRC MOD/UNIT SYM variants.py [-k NAME] [--dump "-dX ..."]` (VARIANTS = {name: [(old, new), ..]});
  `sect.py MOD/UNIT` = .rodata/.data bytes + .bss size vs the split object; `fn.sh DUMP FUNC` = one function of an RTL dump.
  The SN gcc source is in ~/.cache/rooms_b6/sngcc/src/gcc (loop.c, jump.c, haifa-sched.c, gcse.c, local-alloc.c were
  read for the rules below).
- **r118 ThunderMove 2 -> 0 (unit Matching): loop.c emits its hoists in movables-list = scan order, so the order of two
  hoisted constants in the preheader is decided by where their FIRST occurrence sits in the loop body.** The target's
  `ori r30,0x8889` (the `% 30` magic) precedes the EstSet arm's zero `li r28,0`, but the arm comes first in RTL. A dead
  test at the top of the `cnt <= 0` body, `if (cnt == (int) 0x88888889) cnt = 0;` (cnt is re-set on every path of the
  body, so the store dies in flow and the compare goes at jump2), puts a `(const_int 0x88888889)` movable first in the
  list; `combine_movables` then folds the three `% 30` constants into it ("matches"), and the zero -- the `#12 (b)`
  `asm("li %0,0" : "=r"(zero))` in the then arm, hoisted as an invariant asm -- follows. Both tagged. Every source
  position of a plain `zero` variable fails: before the loop it is a preheader insn (LUID below every hoist), at the
  body top it is the first movable, in the arm cse folds it into the EffGetAreaState result, and a peel copy /
  gcse insertion cannot land after the hoists (both emit before LOOP_BEG; hoists are emitted right before the note,
  i.e. after everything the source or jump1 put there).
- **r101 (unit Matching, 25 -> 29/29):**
  - TitleCall 74 -> 0: **gcse PREs a loop-invariant `&scalar` argument (`addi rX,r1,8` + `mr r5,rX`, one more
    callee-saved reg) only when the loop has a preheader BLOCK.** `EventMgr* m = &EvtMgr;` before the `do {} while`
    made a block between the previous poll loop's exit test and the loop label; with `m = &EvtMgr` written INSIDE the
    loop body (loop.c hoists it later, the target's `addi r31,r9,EvtMgr@l` in the preheader) the poll loop's exit
    branch falls straight into the do-loop's label, the only predecessor block ends in a two-successor jump and the
    block-based LCM has no place to insert -> `addi r5,r1,8` at the call each iteration. (The `r101_getEvt(m, &evt)`
    inline did not help: an ADDRESSOF scalar's address is a pseudo after purge_addressof, i.e. a PRE occurrence.)
  - Event00 10 -> 0: `pSysS->region` (struct view of pSys, defined locally like em10.cpp's) keeps the pSys load
    below the `ang = pPL->pos` copy stores.
  - Event30 16 -> 0: the target frame has an unused 8-byte slot before `win`/`ladder` (0x30 vs 0x28); an unreferenced
    `u32 unused[2]` gets no slot, so a codeless `asm("" : "=m"(unused))` inside the `pLog->err` arm keeps the
    8-byte temp allocated (aggregate temps are allocated at expansion, address-taken scalars after them). Tagged
    COMPILER-DIFF (frame layout; the original's use is not in the bytes). At the function top the same asm takes an
    issue slot and shifts block 0's `lis/lwz` order (4 words).
  - Event20 2 -> 0: the four PRE insertions at the end of the SearchEmModule block (`&win`, `&pos`, high(pPL), `&ang`)
    are emitted in hash-table = first-occurrence order and fill sched1's free slots in that order; the LAST one shares
    the cycle with `li r3,0x15` and loses to it on priority, and sched2 keeps that pair's LUID order. The target's last
    filler is high(pPL), ours was `&ang`: `Vec* pa = &ang;` moved BEFORE `pl = pPLS;` (after `pos = template`) gives
    `&ang` the lower index, so `addi r26,r1,24` is issued a cycle earlier and precedes the `li` in sched2. Rule: a
    PRE filler's issue order is its expression index; move the first source occurrence to reorder the fillers.
- **r103/r105 execOpenCover 27/25 -> 0 (r105 Matching): the "#9 peel + conditional cross-jump" reading was wrong.** For
  CCFP compares `jump_back_p` can only pair a NORMAL conditional jump with an INVERTED one (`can_reverse_comparison_p`
  refuses FP reversal), and jump2 deletes the tail of the SCANNED jump, which then is always the loop's inverted `bge
  TOP`, never the peel copy's `blt END` -- the target's `sub; b TEST; TOP: sleep; sub; TEST: lwz; lfs; fcmpu; bge TOP`
  is simply a goto loop entered at its test with NO peel: `rot -= step; lim = K; goto test; wait: SceSleep(1); rot -=
  step; lim = K; test: if (!(rot < lim)) goto wait; rot = lim;` with `f32 step = K;` a variable (f31 across the call) and
  `f32 lim` assigned in BOTH predecessors of the test (f13 in each; the exit store reuses the register). r103 also
  needs a dead `do { } while (0);` between `rot = lim` and `SceAtSetEnable(c->at14, 1)`: its LOOP_END note makes the
  `lwz c->at14` a sched barrier (`reg_pending_sets_all`), so `li r4,1` gets an output dependence and follows the
  load (target `stfs; lwz r3; li r4`; without it `li r4` fills the store->load stall). Tagged.
- **COMPILER-DIFF #7 sharpened (r11c closeGate 19, r106 shakeClosetDoorR/L 27+27, all left): the original's gcse PREs
  CCFP compares.** Shape: `fcmpu` in BOTH predecessors of the loop test and the test block reduced to the bare
  `bge/ble TOP`, the pre-block ending `b TEST`. That is the block-based LCM's earliest placement of `(compare:CCFP y
  dst)` whose operands are set in every predecessor (INSERT = PPOUT & ~AVOUT & ~TRANSP), which our gcse skips because
  `can_copy_p[CCFPmode] = 0` (rs6000.md's `movcc` is CCmode only; `compute_can_copy` recog-tests a reg-reg move per
  MODE_CC mode). Integer compares ARE PRE'd by both (the r225 `faded == 0` note). No source form: C cannot branch on
  a CC computed in another block, and an asm branch would hide control flow from flow/regalloc. r103's loop is not
  #7 because its compared value is a MEM load made in the test block (the compare is not anticipatable there).
  - **#7 compiler-side research (2026-09-10, ~/.cache/ccfp7: h.py whole-tree harness on a fresh src/ snapshot, base
    18841/19929 identical; `mkmd.sh NAME md/FILE "-Ddefs"` builds a cc1plus from an alternative rs6000.md + `-D`
    hooks in gcse.c/jump.c/toplev.c; `one.py CFG UNIT`, `tryform.py FORM CFG` for r106; `reg_*.txt` = regression
    lists; `patches_ccfp7_*.diff` = the hooks). The "CCFP PRE" reading above is WRONG; the real mechanism is in
    jump.c and is reproduced by a `-D` variant (not installable as is). Nothing installed.**
    - SN-vs-stock facts: SN's gcse.c and lcm.c are stock 2.95.3 (SN header only); rs6000.md's `movcc`
      expander/insn is byte-identical to stock and CCmode-only; rs6000.h's `HARD_REGNO_MODE_OK` (CR regs take any
      MODE_CC mode) and `EXTRA_CC_MODES CCUNSmode, CCFPmode, CCEQmode` are stock; `AVOID_CCMODE_COPIES` is defined by
      neither (only mips.h). `compute_can_copy` recog-tests `(set (reg:M) (reg:M))` per MODE_CC mode; a `-DCCFP7_DEBUG`
      print confirms `can_copy_p[CC]=1, [CCUNS]=[CCFP]=[CCEQ]=0` in our build. No `-m` option, no `#define`, and no
      init-order change can make CCFP copyable: only an md move pattern can (`movccfp` expander + insn; `gen_move_insn`
      in `pre_delete`/`pre_insert_copies` needs the optab, so forcing the array alone would abort).
    - Variants and whole-tree results (regressions / newly identical vs base):
        md `movccfp` (+ insn with the movcc alternatives)      0 / 0  -- byte-identical tree; can_copy_p[CCFP] = 1
        md `movccfp`/`movccuns`/`movcceq`                      3 / 0  (t_scroll loadBinName, em32 setNext, r40e
                                                                       moveElevator: CCUNS/CCEQ compares PRE'd)
        md `movccfp` y-only (mcrf, no r/m alternatives)        ICE in r223 reva_common_move (caller-save needs the
                                                                       `mfcr` alternative: a hoisted CCFP compare
                                                                       lives across the loop's call in r30 in BOTH
                                                                       builds -- loop.c hoists CC compares without
                                                                       can_copy_p; caller-save.c saves the CR field
                                                                       in CCmode via movcc, so `mfcr r30 ... mtcrf`
                                                                       in reva_common_move (Matching) is stock)
      With CCFP copyable gcse does record `(compare:CCFP ..)` and does PRE it (r223 reva_common_move: the
      loop-invariant `f29 > f28` compare moves to the preheader in gcse instead of loop.c -- same final code).
      The r106/r11c shape is NOT produced: in the goto form (`y = m->rot.y + K; m->rot.y = y; goto test; wait: ..;
      test: if (!(y > lim)) goto wait;`) the compare `(compare:CCFP y lim)` has one occurrence at the join and no
      redundancy; 2.95.3's block LCM (`pre_lcm`: antin, earliest = ~transp | (earlyin & ~antin), delayin lfp,
      latein = delayin except in the last block, isolated; INSERT = optimal & ~redundant needs a `reaching_reg`
      that only `pre_delete` creates) gives optimal[test]=1, redundant[test]=0 -> nothing deleted, nothing inserted.
      Only a Morel-Renvoise placement (INSERT = PPOUT & ~AVOUT & (~PPIN | ~TRANSP), egcs-1.1 gcse) would put a
      single join compare into both predecessors, and the tree shows the original does not (0 whole-tree effect
      of CCFP copyability means no CCFP compare is ever partially redundant in our sources as written).
    - **What the original does: `duplicate_loop_exit_test`'s copied conditional jump is left UNFOLDED over the
      `b END` that follows it, and jump2's fall-through cross-jump (`find_cross_jump (b END, END, CJ_FALL_MIN=1)`)
      then matches only the two identical exit jumps (`stfs f0` vs `stfs f13` differ) -> `b END` is redirected to a
      new label before the loop's exit jump and the copy's jump deleted: `A0; cmp0; b TEST; TOP: sleep; A; cmp;
      TEST: bge TOP; END`.** In ours jump1 folds `bge TOP; b END; TOP:` into `blt END` at once ("conditional jump
      jumping over an unconditional jump", jump.c 1788ff) and jump2's condjump cross-jump (`jump_back_p`, minimum 2)
      finds no two matching insns (the pre's y0 and the loop's y1 are different pseudos, f0/f13), so the pre keeps
      `blt END`. Proof: `-DCCFP7_NOFOLD_DUP` (jump.c marks the JUMP_INSN copies made by duplicate_loop_exit_test by
      UID, the fold rule skips them; toplev.c's rest_of_compilation resets the marks per function) makes
      **r106 shakeClosetDoorR AND shakeClosetDoorL 0 words from the natural `m->rot.y += K; while (!(m->rot.y >
      lim)) { SceSleep(1); m->rot.y += K; }`** (also from `for (;;) { if (m->rot.y > lim) break; ... }`) and
      **r11c closeGate 0 words from the natural `while (1) { g->pos.y -= spd; spd += acc; if (g->pos.y < dst)
      break; SceSleep(1); }`** (the `#9` comment in r11c.cpp is confirmed: it is the rotated while(1)). The
      register difference f0/f13 is a consequence, not a cause: with no folded copy the pre's temp and the loop's
      temp are never cross-jumped, so regalloc names them independently.
    - But the blanket rule is not the original's either: whole tree `NOFOLD_DUP` = 1995 / 0 (every `for (i..)`
      copy stays unfolded), `NOFOLD_DUP + FP_ONLY` (only copies whose condition is on a CCFP reg) = 30 / 0
      (reg_nofoldfp.txt: cam_ctrl areaHit, snd sndVol/PitchCalcSub, e_rem_pio2/ef_rem_pio2, espgen02_Update,
      main_sub Bg_brightness_set, math_sub VecRadLimit, route_ck getNearPoint, esp08 move, esp47 Trans, t_camera
      tcEdit_area, r201 setBattleArea_sub, r207 WallMove, r209 LeaderEscapeToD/GatlingAppear, r20c KaigaMove, r211
      GrateOpen, r212 RoofMove, r214 BridgeRotate, r21b x5, r223 dai_down_end, r227 checkBox0/1Fall, r22a EleDown/Up).
      VecRadLimit's target (`lfsx; fcmpu; cror; bns END; lfs; lfs; TOP: ...; bso TOP`) and r207 WallMove's
      (`while (obj->pos.z < -9500.0f) { pos.z += 30; matUpdate(); SceSleep(1); }`) show the original DOES fold FP
      copies whose exit test is `load; compare` with the loop body ending in calls; r106/r11c (not folded) have the
      compared value STORED as the last statement before the test (`m->rot.y += K` / `g->pos.y -= spd; spd += acc`),
      so after cse the copy compares the pre's fresh temp and the loop's test compares the body's temp. The
      discriminating condition (a stock rule we have not identified, or a later-SN jump.c change) is the open
      question; candidates to test next in ~/.cache/ccfp7 with `tryform.py`: whether the copied exit code contains
      a store / sets a pseudo renamed through `reg_map` (`REG_LOOP_TEST_P`), whether LOOP_CONT sits inside the
      copied region, and the relative order of the fold and `duplicate_loop_exit_test` inside jump1's `while
      (changed)` loop (the copy is processed from `next = NEXT_INSN (temp)` in the same round). Both the `#7` shape
      and the r227/VecRadLimit shape must come out of one rule before anything is installed.
    - Workarounds this would retire (all tagged `#7`/`#9` loop spellings with gotos): r11c closeGate, r106
      shakeClosetDoorR/L (currently peeled `if (!(..)) { wait: ..; goto wait; }` = 9+9 words), r103/r105
      execOpenCover's goto form (Matching either way -- `nofoldfp` does not change them, so the goto form is
      also what a non-folding original produces). r202 throwRock's `fmr` copy is the loop.c "multiple entry"
      difference noted below, same family (the unfolded copy's `bge TOP` is a jump into the loop from outside;
      the original's loop.c evidently still treated the loop as valid -- with the copy inside the notes, or with
      the fold done later, that jump would not exist at loop time).
- **r202 throwRock 19 -> 9: the exit store's constant is a COPY of `lim` hoisted into the pre-block (`fmr f28,f30`).**
  Mechanism in the original: `for (;;) { sub; if (rot < lim) { rot = K; break; } SceSleep(1); }` is rotated by
  expand_end_loop (the `b END` exit jump is the "qualified conditional exit"), jump1 peels the exit region `sub; cmp;
  bge TOP; stfs; b END` before LOOP_BEG, and the original's loop.c still treated the loop as valid (hoisting the exit
  store's constant load to the preheader, cse2 -> `fmr` from `lim`); OUR loop.c invalidates it ("multiple entry
  points": the copy's `bge TOP` is a jump from outside the notes, `mark_loop_jump`), so nothing is hoisted and the
  store stays a fresh load -- a compiler-build difference in jump.c/loop.c (the peel copy sits outside the notes in
  2.95.3). Reproduction: hand-peeled `sub; lim2 = lim; asm("" : "+f"(lim)); if (rot < lim) rot = lim; else { do {
  sleep; sub; } while (!(rot < lim)); rot = lim2; }` -- the plain copy `lim2 = lim` survives gcse's copy propagation
  only because `lim` is laundered right after it (tagged #9); a launder on `lim2` itself (or `asm("fmr")`) changes
  lim2's ref count and swaps f28/f29. Left (9): the pre-block's GPR names (obj/pParts/lim-high r11/r10/r8 vs
  r10/r8/r11: local-alloc qty order under the same schedule) and `addi r31,r31,1` before the two `fmuls` in the bounce
  loop (gcse PREs the latch's `i + 1` to the end of the loop's top block in both builds; in ours `i` dies at that
  insn (weight 0) and it is issued first, in the target it ranked last).
- **r103 openShelf_main 4 (left): `lis A`/`lis B` r9/r11 swap = local-alloc's `QTY_CMP_PRI` (refs/span, then qty
  number).** sched1 issues `lis A; lwz a->pParts; lfs A; lis B; lfs B` (the lwz fills A's cycle), so A's high spans
  5 insn units vs B's 3 and B is allocated first (r9); A then conflicts with B through the fake-lifetime adjacency
  and takes r11. The target needs A allocated first, i.e. the `lwz` not between `lis A` and `lfs A` in SCHED1 (sched2
  may still put it there: r1-based `addi`s and loads after a call depend on the call in sched2 because r1 is in
  CALL_USED_REGISTERS). Launders/literals/const/FSet/pointer locals/dead do-while: 4-17 words; not found.
- Other reads: r10b readEvent 3 (`mr r7,r5` before `lis r6` = a sched2 tie broken by sched1's LUID order; in sched1
  the `max` copy has one dependent fewer than the string high); r222 R222Main 7 (the `Vec pos = {..}` template loads
  are `mem/u` and float above the `seTimer = 30` store; the target's did not -- a non-const view of a `static const
  Vec` puts the store first but changes the copy to `addi; lwz 0/8/4(rX)`); r202 initCatapult 106 (two `high(r202_work)`
  chains, #3 family); r10f R10fInit 6, r11e 8+11, r210 dai_go 10 / funcAshley2 9 / toroko_ret 323, r10b chkEmDie 162 /
  chkWater 16 / GakeEvent 8 / S10 10, r222 BoxMove 36 / dragon_down* 51+159+156 / Init 88 not iterated this pass.

### Stage rooms, st4_0/st2_1 pass 6 (r40f Matching 9/9 -> st4_0 fully linked; r209 60/61 with R209Main 9 -> 6; r20d 30/32 with moveWall, throwLantern, operateCrank 0 and execThrough 62 -> 4; 2026-09-10)

- Harness ~/.cache/rooms_a6 (rooms_a5 copies with the paths rewritten; `tryv.py MOD/UNIT FUNC variants.py
  [--asm N] [--apply N]`, `vsbs.sh MOD/UNIT FUNC variants.py NAME`, `mm.py`, `mdump.sh MOD/UNIT -dX` with `SRC_OVERRIDE`;
  the LA_DEBUG cc1plus of ~/.cache/em2b39_p7/sngcc prints local-alloc's qty order/priorities for any `dump/<unit>.i`).
- **r40f BombSet 5 -> 0, zero code (unit flipped, st4_0 now linked in full): struct view of the work pointer for the two
  `setGoto` calls** (`struct R40fWorkPtr { R40fWork* p; }; #define r40f_workS (((R40fWorkPtr*) &r40f_work)->p)`). The
  p0 word-1/word-2 issue order is a sched2 priority: the w2 store `stw r3,8(r11)` had +1 over the w1 store from its anti
  dependence on the following `lwz r3,work` (a `mem/f` fixed scalar never depends on the `mem/s` frame stores through
  r11), and with equal RTL no LUID permutation can flip it. As `mem/s` the work load conflicts with the frame stores
  (bases: r11 = frame vs the symbol -- known, so `fixed_scalar_and_varying_struct_p` was the only thing separating
  them), every frame store gains the load as a dependent, the chains tie and LUID/weight give the target's order in
  both sched passes. Every asm anchor (`"+m"(p0.y)` etc.) shifts the allocation instead (an asm's frame operand is
  `(plus r31 N)`, unrelatable to the PRE'd `(plus P 4)` store addresses, so it conflicts with both words).
- **r209 Switch/BridgeAppearCheck 7+7 -> 0, tagged `COMPILER-DIFF: candidate (sched1 order of the pool/work highs)`.**
  Mechanism confirmed with the LA trace: local-alloc allocates the three 2-ref qtys (lim high, work high, work ptr) in
  qty-number = sched1 birth order (equal QTY_CMP_PRI), and the ptr can share the lim high's r9 only if the high is
  allocated first (`post_mark_life` marks `[birth, death)`, the ptr's fake_birth = birth - 2 touches the lfs slot, so
  `lfs; lwz` adjacency is fine once the RO high owns r9). The codeless asm `asm("" : "=r"(seId), "=r"(wp) : "0"(seId),
  "1"(wp), "f"(lim))` after `int seId = RoomSeCall(..); const f32 lim = K; R209Work* wp = r209_work.p;` ties the store
  to the lim load (both highs then carry the store chain: equal priority, LUID puts `lis lim` first) without adding a
  ref to the pointer -- an asm on the pointer alone doubles its refs (`used 4 times`, pri 2.67 > the highs' 0.33) and it
  is allocated first (r9), pushing the lim high to r11; an asm on `seId` alone leaves the ptr's fake lifetime touching
  the RO high's last slot (r10). The `const f32 lim` must be declared AFTER the call (before it: hoisted `lis r30`
  callee-saved, 65 words). Alias facts read on the way: `find_base_value` returns a hard REG itself as the base (objs
  = `(reg 3)` copies), `(mem)` sources give 0, and base_alias_check treats two non-ADDRESS bases as never aliasing --
  the loaded work pointer (base 0) is what chains the seId store to the obj loads; noalias/restrict alias sets are
  unreachable from C++ (`c_get_alias_set`'s `//QAZ` return gives every COMPONENT_REF set 0; the restrict path needs a
  nonzero pointed-to set, which the same bug zeroes).
- **R209Main 9 -> 6 (bit index as a consec_sets giv).** `u32 base8 = i * 8;` in the outer body and, inside the inner
  loop, `u32 bit = j + base8; bit += 8;` (two consecutive sets of one variable = `consec_sets_giv`, one giv with
  benefit 2 adds, no not-worth intermediate): j keeps only reducible givs and is eliminated into the pointer compare,
  and the giv init `addi r29,r23,8` is emitted by loop.c after the movables (the target's LUID). Single-expression forms
  fail: `j + base8 + 8` / `(j + 8) + base8` create a `j + inv` intermediate DEST_REG giv (benefit 4 - add_cost 4 = 0,
  "not worth while", all_reduced = 0, `Cannot eliminate biv .. used in insn` = the duplicated entry compare in pass 2);
  `bit = j + K` with K hoisted is itself not worth. Residue 6 = the bit-set block's r0/r9 names: three tied qtys (idx:
  3 refs x depth, amt/shift and val/or 4 refs x depth each) sorted by the buggy 3-qty hand sort (`qty_compare (0, 1)`
  on qty NUMBERS, then swapped back): with idx born first the final order is [idx, amt, val] -> r0/r9/r11; the target's
  [amt r0, idx r9, val r11] needs PRI(idx) >= PRI(val), which no depth (2..4) or tie structure of these lens gives.
- **r20d moveWall 3 -> 0, zero code: the flow nop takes the issue slot.** `SndCall(6, 8, ..); do { .. }` puts a
  `(use (const_int 0))` after the call (call followed by the loop label); the nop has weight 0 and beats the free
  `li i,0` (weight +1) for the slot beside `bl SceEventStart`. `i = 0;` written right after the SndCall (declaration
  uninitialised) removes the nop and the `li` is issued with the first call as in the target.
- **r20d throwLantern 31 -> 0** (three zero-code levers + two tags): `pPLS` on BOTH pPL reads around the `sth atari.flags`
  store (`AtariFlagsOr(&pPLS->atari, ..); pPLS->dmg.set(..)`): cse1's `invalidate` runs true_dependence against the
  table's FIRST load, and a `mem/f` load survives a varying `mem/s` store through the fixed-scalar rule; only a `mem/s`
  first load is removed, so the second read reloads pPL (target: two `lwz pPL@l`). `*(void**) ((u8*) u + st + 0x18)`
  (u first) for `add r29,u,st`. `register f32 zero asm("fr30")` (tagged #2, value-carrying pin) for the 0.0/turn f30/f31
  tie. `asm("" : "+r"(u));` next to the `st` launder (tagged #12, companion): the `st` launder gave `li r29,0` one
  priority level over the parameter copy `mr r31,r3`; the same codeless copy on `u` restores the tie and the copy leads.
  `pPLS->rot.y` for the read after `u->step++` (the plain read floats above the struct store).
- **r20d operateCrank 68 -> 0, zero code:** loop-state zero-inits (`spd/lastMot/accel/seId = 0`) written after the
  switch (declared uninitialised: the `li`s belong to the block before the beginEvent calls, not block 0);
  `t = *(f32*) (idx * sizeof(cFence) + (u32) r20d_work.p + 0x2c)` (mult first: `add r11,idx36,work; lfs 0x2c(r11)`;
  the array form gives `addi work,0x2c; lfsx`); `if (t >= 1.0f)` for both exit tests -- the GE code on CCFPmode prints
  `cror un,eq,gt; bso/bns`, `!(t < 1.0f)` is TRUTH_NOT (fold does not invert FP compares) and prints a plain `bge/blt`.
- **r20d execThrough 62 -> 4:** `pPLS` on both sides of the atari `sth` (head and tail), `while (1)` for the final
  poll loop (SceSleep at the bottom, `bgt` exit; `for (;;)` lays the sleep out of line), the `a` Vec block-local in the
  loop AND in the block after it (same slot 0x18) with `f32 ry = p->rot.y` loaded before setPos and stored after
  (callee-saved f31), `FAdd(pPL->rot.y, da)` (reference store: the pPL reload and a separate rot.y load follow),
  `asm("" : "=r"(d) : "0"(d) : "r31")` after `setAng(&a)` in the loop body (tagged `COMPILER-DIFF: 3`: kills the PRE of
  `(plus fp 0x18)` into a callee-saved register -- placed in the after-block it is too late, cse2 still substitutes the
  hoisted pseudo) and `do { } while (0);` after the loop (tagged candidate #12: the after-block reloads 0.0 from the
  pool). Residue 4: `mr r3,r30` for setAng is issued first in the target's post-setPos block and fourth in ours (the
  frame stores have +1 via their true dependence on the call; a comma-expression argument order does not change it).
- **r20d checkSwitch 1 (unchanged, mechanism sharpened):** a second use of the tail label (`if (z) goto down_end;` with a
  block-local `int z = 0` in the sleep block, folded by cse1 later than the tail) does stop the skip_blocks fold and the
  tail gets its `lfs f1,0.0`, but the tail's `high(LC)` is then a fresh ebb occurrence: gcse PREs it into a second
  callee-saved register and cse2 re-materialises (`lis r30`, 20 words). The target's tail knew the high but not the 0.0
  register -- not reproducible by label uses; `FCRef(static const zero)` reads are not hoisted by loop.c here.
- **r20e cFence20e::move 10 (unchanged):** FPR/high pins (`register f32 up asm("fr12")`, `lim asm("fr0")`, both
  constants) leave 10-11; the target's `lis r9,2200` reusing the dead 50-high's r9 and the `lfs f12` one slot later are
  the reload-rematerialised shape the notes describe.
- Not iterated: r20e initPuzzle/checkPuzzle, r204 EventChandelier1/2 (frame 0xc0 vs 0xb8: ours has an unused 8-byte
  slot at 0x50 before the fpmem slot; `lis r30; addi r25,r30,crot0@l` two-register high) and nige_check.

### Stage rooms, tagged-forms pass (st1_0 fully linked: r120 R120Event 114 -> 0; st2_3 r221/r22a/r21a Matching; r213 28/30 with EventSwitchMain 8 -> 0; r10c 19 -> 21/23 with TestPosMove 4 -> 0, chkSwitchA 71 -> 0; r225/r11b analysed; 2026-09-10)

- Flipped: `st2_3/r221.cpp`, `st2_3/r22a.cpp`, `st2_3/r21a.cpp`, `st1_0/r120.cpp` (111 OK after each; st1_0 has no open
  unit left, st2_3 only r225). r221/r120 carry a nameless `cManager<cLight>` block (`fn_*` in the split, 37/38 and 7/8 in
  mcmp) -- compare the tail bytes with the REL24 fields masked (all nine differing words were `b`/`bl` relocs) before flipping.
- **r120 R120Event 114 -> 0 (zero code): two sequential `if`s, the first masking with a variable (`u32 mask = 0x10; if
  (!(pG->flags_51C0 & mask))`).** thread_jumps runs twice (toplev: before cse1 with flag_before_loop=1, and after loop before
  cse2). With a literal mask the two flag tests are pattern-equal in pass 1 -> the first `bne` is redirected past the second
  block before gcse, which then sees that block dominated and PREs its EvtMgr/string highs into r29/r30 (114). With `mask`
  the `and` operand is a REG_USERVAR_P pseudo, which rtx_equal_for_thread_p never pairs -> pass 1 fails; cse1 folds the
  constant in; pass 2 threads (`bne` -> past the s01 block, both `bne`s to one label as in the target) after gcse ran on the
  unthreaded CFG (s01 block undominated: fresh `lis r3/r4`). The threading also merges the second test into the call block,
  whose tail jump makes `add_branch_dependences` give `li r7/li r8` three dependents (call 1, the clobbering call 2, tail)
  against two for `addi r3,r30,EvtMgr@l`, so sched issues the `this` add last exactly like the target. A `u32 f =
  pG->flags` copy used in the first test blocks BOTH passes (4 words, wrong tail); `EventMgr* em = &EvtMgr` forms 6.
- **r10c TestPosMove 4 -> 0 and r22a RopeMove 4 -> 0 (zero code): `pGS->field` for a pG read declared next to two `Vec`
  template initializers.** The struct-view load is not a fixed scalar, so sched1 keeps it behind the template copies and
  their word 4/8 `lwz`/`stw` pairs come out 4-then-8 (the plain `pG` load is hoisted above them and the pair flips).
  Same lever as R119Init's third SetTree block; try it first on any "word 4/8 order" residue near a pG read.
- **r10c chkSwitchA 71 -> 0 (tagged `candidate #17`): `u32 z = 0;` between `SmdSetTrans(0x57, 0)` and `SmdSetTrans(0x48,
  0)`, passed as the last EstSet's two stack arguments (`z, (void*) z`).** The target's two `stw r29,8/0xc(r1)` zeros come
  from one callee-saved pseudo set ~15 calls earlier; ours materialised `li r0,0` at the stores (the arg pseudo was born at
  the stores, local-alloc gave it r0). The early single-set pseudo (REG_EQUIV 0, two uses, crosses calls) goes to global
  alloc and takes r29 once the first CamCtrl high dies; its presence also swaps the two CamCtrl highs (r29/r31) into the
  target's order. Declared at the block top (`z_top`) it is 175: the position of the set is the lever, not the variable.
- **r221 throwBonbe 18 -> 0 (tagged `candidate #17`): `int evNo;` set once in `case 0` (`evNo = 0` right after RsfSet)
  and read once by `SceEventStart(evNo)`.** update_equiv_regs folds the constant into the argument move after sched1 and
  deletes the set; its only effect is to occupy the post-RsfSet issue slot in sched1 so eff2's `li` is issued later there
  (live length below the pG high's priority -> r21/r22 order), while sched2 still puts `li eff2` right after the `bl`.
  `int evNo = 0` at the declaration or the set after `eff2 = 1` do not reach it.
- **r21a FallRoofDie 66 -> 0 (zero code): `for (;;)` with the exit `if (obj->pos.y <= -1500.0f) break;` before SceSleep,
  and `f32 spd = (f32) no + 20.0f` declared BEFORE the two Vec templates.** A real loop (rotated by expand_end_loop) puts
  the `mr r27,r29; mr r26,r30` PRE copies in the preheader after SndCall; the goto form hoisted them into the prologue.
  `spd` first decides local-alloc's r29/r28 order for `&camAt`/`&obj->pos`.
- **r213 EventSwitchMain 8 -> 0: literal constants in the rotation loop (`+ 0.06981317f`, `>= 1.5707964f`), `do { }
  while (0);` after it (tagged `#12`: the 1.5707964 store below reloads the pool, not the hoisted register) and a dead
  `if ((int) pG->flags_174 < 0) o41 = 0;` inside the `while (CamCtrl.IsMotionEnd() == 0)` poll (tagged `candidate #17`).**
  loop.c hoists the step pair and the limit's `lfs`; the limit's `lis` is gcse's PRE copy (its high also occurs after the
  loop), so the preheader is `lis lim; lis step; lfs step; lfs lim`. The dead test's in-loop pG read is a fourth,
  loop-weighted ref of the PRE'd pG high and breaks its priority tie with the RoomData high (r28/r27). gcse hash buckets:
  table size `(n_insns/2)|1`, symbols hashed by name (`h = h + (h<<7) + c`), bucket order = PRE pseudo numbering = the
  allocation tie-break (T=117, C=7933 for this function). StatusSetChain (7: local-alloc needs 0.8 > 50 > 0.1 > 0.0 with
  the 0.0 span > 1.5x the others; 120 store permutations >= 7) and Init (14: the `&rot` PRE copy must precede the memset
  call; regmove's optimize_reg_copy_1 stops at CALL_INSNs under flag_exceptions; no `__builtin_memset` in this g++) open.
- **r11b Init (14) open: `pos = static_const_vec;` (assignment) gives template loads WITHOUT /u; only `Vec pos =
  static_const;` (initialization) or a `(Vec)` cast (which adds a temp copy) gives `mem/s/u`.** Without /u the loads carry
  WAR dependences on the twice-set r29/r30 frame pointers (alias.c drops a hard reg's base on its second set).
- **r225 operateCrank 77 (open, two independent residues).** (a) Layout: `while (1) { ... if (!(SmdGetObjPtr(0x27)->pos.x
  < 800.0f)) { gnd_open(); break; } pos.x += ...; ...; SceSleep(1); }` reproduces the target's `bl gnd_open; b after`
  block between the arms of the final RsfCheck if/else (jump.c's "if (foo) bar; else break;" range swap needs the break
  block to end in a jump that is not to the next insn, which the do/while `else { gnd_open(); break; }` form loses in
  round 1). (b) The 2^52 unsigned-conversion magic: loop.c pass 2 hoists `lis/lfd` when `T*2*2 >= insn_count` with
  T = 1 + n_non_fixed_regs = 68..70 here (bounded from the dump: 260 insns move two pairs and not the third) and -3 per
  `move_insn` movable moved earlier in the same pass; ours is 262 (do/while) / 260 (the layout form) real insns in pass
  2, the target needs >= 281 or two movables ahead of it. Pass 1 shrinks the loop by 18: 3 movables + 15 gcse PRE copies
  `X = PREreg` (REG_EQUAL high) substituted away. Per-case `max = *(u16*) mot` duplication gets 274 but changes the
  tail (`lhzx`); duplicating the whole conversion per case (309) blocks the hoist but scrambles allocation. The
  `-dL` dump lists both passes ("Loop from A to B: N real insns", one listing per pass); use it before guessing.
  SceElevator_r225 (285) not iterated.
- **r10c SetEmHitAtari 95 (open): the target loads 0.01/0.05/0.06 (f26/f28/f27) in the block ending with the RsfCheck(14)
  call, yet their pool entries follow all six YarareInitCube constants of the then-arm** (pool order is creation order:
  spd sets before the `if` give `.rodata DIFFER`; after the if/else, too). 2.95 PRE is PAV-gated -- occurrences in sibling
  arms are never partially redundant, so neither the `lfs` nor the `high` is hoisted from two arm starts (ours keeps a
  copy per arm), and calls kill every MEM's transparency (`mem_set_in_block`, no RTX_UNCHANGING_P exception). Wrapping
  the ifs in an outer `for (;;)` (loop.c preheader) moves the highs to P but not the loads (120). Mechanism unknown;
  hako_down (37) is the operateCrank loop-count family (-100.0 hoisted at 260 insns).
- Harness gotcha (rooms_c8): `mdump.sh MOD/UNIT` compiles `src/MOD/UNIT.cpp`, which does not exist for rooms (sources
  live in src/st1, src/st2); without `SRC_OVERRIDE=src/st1/rNNN.cpp` it exits after cpp and the previous dump is read.

### Stage rooms, st4_0/st2_1 pass 7 (r209 Matching 61/61 -> st2_1 flag; r20d 31/32 with execThrough 4 -> 0; r20e 29/31 with cFence20e::move 10 -> 0 and initPuzzle 103 -> 49; r204 unchanged; 2026-09-10)

- Harness ~/.cache/rooms_a7 (rooms_a6 copies with the paths rewritten; `tryv.py MOD/UNIT FUNC variants.py
  [--asm N] [--apply N]`, `vsbs.sh`, `mm.py`, `mdump.sh MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE` (a relative path is
  taken from the re4 root and cpp fails silently, leaving the previous dumps in place); new `prio.py UNIT FUNCPAT` = the
  callee-saved/FPR global allocnos of dump/UNIT.i.{lreg,greg} for one function with refs/len/priority in allocation order).
- **R209Main 6 -> 0, zero code (unit flipped): an integer (u32) copy of the flags base for the bit-set block.** The block's
  three local qtys (byte index, shift amount/shifted bit, loaded word/or) go through local-alloc's 3-qty hand sort; with a
  pointer base the index pseudo prefers GENERAL_REGS (regclass: the pointer-flagged operand of `(plus idx base)` is the base,
  the other the index) and takes r0, so `lwzx` prints `r28,r0` (rs6000 prints the r0 operand second). `u32 fb = (u32) flags;
  u32 ofs = ((u32) bit >> 5) << 2; *(u32*) (ofs + fb) |= 0x80000000 >> (bit & 31);` makes both plus operands non-pointer
  -> half BASE_REGS preference -> idx r9, amt r0, `lwzx r11,r9,r28` like the target. `fb` must be read BEFORE the shift
  constant's first use: the loop.c-hoisted base copy `mr r28,r20` and `lis r24,0x8000` both have sched1 priority 1 in the
  preheader and the LUID (= RTL order of first occurrence in the loop body) decides the cycle-2 slot; with the cast written
  inside the statement (`ofs + (u32) flags`) the constant came first and `addi r11,r11,0x5dc` slipped between (2 words).
  A `register u32 ofs asm("r9")` pin also works (pin9c/pin9d) but is not needed.
- **Flip lesson (r209): local `.data` tables.** The REL shasum failed with ADDR16 fields `0` vs `S+A` for six r209 tables
  (`r209_leaderPoint`, `r209_snipeEmNo`, `r209_zeroVec`, `r209_bowgunStartPos/Pos/Pos2`): they are `static` in the original
  (the field holds S+A for a local symbol). `make_rel.py --verify orig/.../st2_1.rel` with the `--link` list from build.ninja
  names every differing field; mm.py cannot see it (it compares by section+offset). Make the tables static, re-sync, rebuild.
- **r20d execThrough 4 -> 0: the r31-clobber asm's OPERAND decides a sched2 dependent count.** The tagged #3 asm
  (`asm("" : "=r"(X) : "0"(X) : "r31")` in the loop body, after setAng) has anti-dependences on every r1/r31 reader of the
  block; with `X = d` it was a 4th dependent of `addi r4,r1,0x18` (mr r3,r30 had 3) and won the cycle beside `stfs f31`.
  With `X = p` (the block's `cPlayer* p`) the asm is instead an anti-dependent of `mr r3,p`: both copies have 4 dependents,
  equal priority, and sched2's LUID tie-break (sched1 had issued the dying `mr` first) puts `mr r3,r30` first like the target.
  Rule: a codeless asm's operand adds one INSN_DEPEND to the last setter/reader of that register -- pick the operand whose
  copy the target issues first.
- **r20d checkSwitch 1 (unchanged; mechanism now exact, no source form).** The target's tail `lfs f1,0.0@l(r29)` = cse1 knew
  the pool constant's HIGH in the tail but not its VALUE. In ours cse1 reaches the tail through the AROUND path of `blt tail`
  (q = LOOP_END; `invalidate_skipped_block` only removes `in_memory` MEMs (a `mem/u` pool load is not one unless its address
  is FIXED_BASE_PLUS_P), call-clobbered hard regs and the regs SET in the skipped block) and finds the compare's 0.0 pseudo Z.
  Every way of breaking the path also loses the high: a fresh tail ebb (second label use, label in the skipped block) leaves
  the tail's `high(LC)` to gcse, which deletes it as redundant with reaching reg R inserted at the END of the preheader
  block P (`R = high` after the SceAtSetEnable call); R is used in other blocks and `maybe_never` there, so loop.c's outer
  pass never hoists it (scan_loop's `! reg_in_basic_block_p && (maybe_never || ...)` -> not a movable) and it stays a second
  `lis r30` (20 words). Only invalidating Z on the AROUND path gives the target's shape, and Z is the anonymous compare
  constant: a `zero` variable compared instead and re-set by `asm("" : "+f"(zero))` in the tail DOES stop the fold
  (`lfs f1`) but with `zero` set in P the body has no `high(LC)` any more and the tail's high is fresh again (14 words); a
  variable set in the body has two sets and is not hoisted. `static const` replacements of the pool constant are folded by
  the front end (scalar) or lose the REG_EQUAL const_double notes cse2/loop.c use (struct member: 45 words). Tail inside
  the else arm (5: f28/f29 swap, the 0.0 pseudo gains a depth-3 ref), `while (1)`, `t = 0.0f; move(t)`: 1. Classified
  compiler-side (#12: the original's cse did not carry Z into the tail while carrying the high).
- **r20e cFence20e::move 10 -> 0, tagged `COMPILER-DIFF: #13` (asm-emitted pool constant).** The target's
  `lis r9,2200@ha` reuses the dead 50.0 high's r9 one insn after `lfs f13` and `lfs f12` follows a cycle later = a reload-
  rematerialised constant. As a pseudo the 2200 high is born (sched1 t=2, right after the 50.0 high's death) inside the
  fake lifetime local-alloc uses when sched2 runs (`fake_birth = birth - 2`: "avoid hard registers of qtys born immediately
  after this qty dies"), so it takes r11 and sched2 hoists the load. No priority change moves the `lis` two insns later (the
  block has no other IU insn), and `f32 up = 2200.0f` in an earlier block keeps `lis/lfs` there (update_equiv_regs moves an
  init only when the REG_EQUIV note equals the SET_SRC, i.e. the `high`, not a pool load). Recipe (DOL sweep 12's): the
  constant is the pool's FIRST entry, so `static const f32 k2200 = 2200.0f;` in the function takes its slot (.rodata equal)
  and `register u32 hi asm("r9"); asm("lis %0,%1@ha" : "=r"(hi) : "i"(&k2200)); asm("lfs %0,%1@l(%2)" : "=f"(up) :
  "i"(&k2200), "r"(hi));` after `obj->pos.y += 50.0f` gives the anti-dependence on `lfs f13,(r9)` and the target's order.
- **r20e initPuzzle 103 -> 49, zero code: the first layout pass shares the object loop's counters.** `r20e_setLayout` as an
  inline has its own `x`/`y`, so the object loop's `y + 1` was PRE'd across the SmdGetObjPtr inner loop (no biv, `slwi` per
  outer iteration). A macro `R20E_SET_LAYOUT(p, tbl)` over the FUNCTION's `x`/`y` for the first call (the else arm keeps the
  inline: the target's third nest has its own caller-saved r8 counter while nests 1 and 2 share r28 -- a counter that crosses
  the object loop's calls) leaves `y + 1` at the object loop's latch (pl0f BoatControl rule), forms the `y*4`/`y*16`
  givs and the `subic.` count-down and gives the target's r28 y / r27 p / r26 cnt order (y's refs and the shared life do
  it; the deadexit em3c form alone gave the loop shape with y ranked last, 70 words). The cell address in the macro must
  go through an inline `r20e_placePiece(p, &p->cell[x][y], pc)` that owns the `Vec pos` temp (integrate substitutes the
  frame address; a macro-local `Vec pos` gets its address PRE'd into r4 and the copy goes `stw r7,4(r4)`). Residue 49: the
  target stores `c->piece` through a giv COPY (`mr r7,r11`, stepped in parallel) and loads `c->pos` through the base giv
  in y,x,z order in both layout nests, ours has one pointer and x,y,z (c_store/expr_load forms 64-91: the source expression
  split does not decide it); `li r25,0x10` one slot later in the preheader.
- **r204 EventChandelier1/2 (94/96, unchanged; frame read):** the target frame is 8 smaller with ONE MORE callee-saved GPR
  (stmw r16, `lis r30; addi r25,r30,crot0@l` two-register high). Our extra 8 bytes are an unreferenced frame slot that
  appears only when the u64 `Key.trg & 0x80000` test and the f32->u32 `mf` conversion are both present (`(int)` conversion
  or a u32 Key test: fpmem at 0x50, frame 0xb0/0xb8; `(u32) Key.trg & ..` still has it); expand-time frame refs are only
  m/cpos, so it is a post-expand `assign_stack_local` (reload secondary memory for a DImode reload is the suspect). r208
  operateCrank (Matching) has the same two ingredients AND the slot in its target (vars 0x10..0x20, unexplained 0x20..0x28,
  fpmem 0x28), so the original produces it under some condition r204's source did not meet. Not found.

### Stage rooms, st1_1/st1_3/st2_0 pass 7 (r103 (both modules), r106, r10b, r11e Matching -> st1_1 fully linked; r10f 11/14, r202 30/32; 2026-09-11)

- Harness ~/.cache/rooms_b7 (rooms_b6 + fold7 copies: `cc.sh`, `cmp.py OBJ MOD/UNIT [SYM] [--trunc]`, `tryv.py SRC MOD/UNIT SYM
  variants.py [--dump "-dX"]`, `sect.py`, `rtl.py`, `fn.sh`; the LA_DEBUG cc1plus is ~/.cache/em2b39_p7/sngcc/cc1plus, run with
  `LA_DEBUG=1 CC1=... ./cc.sh ...` to print local-alloc's qty order/PRI per block). The SN gcc source for reading passes is
  tools/sn-gcc/src/gcc.
- **openShelf_main 4 -> 0 in r103 AND r106 (tagged `candidate #17 (local-alloc qty order of the two pool highs)`): a
  pseudo -> hard-register copy of the pParts pointer as a sched1-only insn.** Mechanism confirmed with LA_DEBUG: qty PRI =
  10000 * floor_log2(refs) * refs / (death - birth) with birth/death = 2 * sched1 insn index; the +1.92 high B (span 2)
  beat the -1.92 high A (span 4, `lwz pParts` between its `lis` and `lfs`) and took r9, A then conflicted through the
  fake-lifetime adjacency (A dies at `lfs A`, B born at the next insn) and got r11. Recipe: `cModel* pa = *(cModel*
  volatile*) &a->pParts; register cModel* pa2 asm("r10"); pa2 = pa; pa2->rot.y = ra; asm("" : "=m"(b->be_flag) :
  "r"(pa2)); b->pParts->rot.y = rb;`. Why each piece: the copy `(set r10 pa)` has prio 6 in sched1 (-> stfs a) and is
  ready at t3 (lwz latency 2), so it is issued right after `lis B` and before `lfs B` (prio 4), lengthening B's span to
  A's -> tie -> lower qty number (A) first -> r9/r11 as the target; local-alloc gives `pa` the r10 copy suggestion, the
  copy becomes `mr r10,r10` and reload_cse deletes it, so sched2 sees the target's insn set and reproduces its order.
  combine kills a plain copy in BOTH directions: `lwz pa -> mr r10,pa` merges into `lwz r10` unless the load is
  volatile (`can_combine_p` refuses a volatile MEM source), and `mr r10,pa -> stfs [r10]` merges into `stfs [pa]` unless
  r10 has a second use, which the codeless `"=m"` asm provides (its memory operand also keeps it a live store: pick a
  field nothing stores later; the `"=m"(a->be_flag)` form added a ref to `a` and swapped a/b's global-alloc order in
  r106, `"=m"(b->be_flag)` did not). A hard-register copy without the asm was also undone by regmove/combine; pins on
  pa/pb alone change nothing (the two highs' qtys are unaffected). Rejected zero-code ideas, with the reasons: a
  do-while(0) around the -1.92 statement for the loop-depth REG_N_REFS boost -- the insn after a LOOP_BEG/LOOP_END note
  is a full sched barrier in BOTH passes, and every layout puts B's `lis`/`lfs` on the wrong side of it; a codeless asm
  with a dead output is deleted at flow1 (before sched1), so it cannot be a sched1-only insn; `update_equiv_regs` moves
  a set before its use only for REG_BASIC_BLOCK < 0 pseudos.
- **r106 openShelf_main `li r30,0; li r29,0`: declare `b` before `a`** (the two zero inits tie at every sched rank and
  LUID = declaration order).
- **r10b readEvent 3 -> 0, zero code: `return 0;` in the err arms instead of `goto fail`.** With `goto fail` the block
  ends `bl err; b fail`; with `return 0` it continues `bl err; li r3,0; b END` at sched2 time (jump2 cross-jumps the tail
  afterwards, so the final layout is the same). The `li r3,0` gives `lwz r3,pLog` an output dependent and the block-end
  jump becomes a dependent of every insn, so `mr r7,size` (3 deps) and `lwz r3` (3) outrank the string `lis` (2) at
  their ties -- the pl14 cRoutine::set shape. Rule: when an error arm's argument copies/`this` load are issued before the
  string `lis` in the target and after it in ours, check whether the arm `return`s (block continues) or `goto`s.
- **r10b Evt_R10BS10_Func 10 -> 0, zero code: a user variable in the re-test.** jump.c `thread_jumps` (the jump pass
  after cse1) redirected our first `beq` past an identical second `if (pG->flags_60 & bit)` test; `rtx_equal_for_thread_p`
  returns 0 for `REG_USERVAR_P` pseudos, so `u32 f = pG->flags_60; if (f & bit)` in the second test keeps the target's
  re-read (the `li r9,1` #13-looking residue vanished with it). Not cse (the `-ds` dump only reflects the jump pass).
- **r10b chkWater 16 -> 0 / GakeEvent 8 -> 0, zero code: reference stores so a fixed-scalar load waits.** `BitOn(pG->
  flags_174, bit)` (chkWater) and `PSet(r10b_work->boat, EmSetFromList2(..))` (GakeEvent): a plain in-struct store never
  conflicts with the `mem/f` `lwz r10b_work`/`lwz pG` (fixed_scalar_and_varying_struct_p) and the load floats to the
  block top; the reference store is an unflagged MEM and the load depends on it (target: `stw; lwz`).
- **r10b chkEmDie 162 -> 0, zero code: `while (EvtMgr.IsAliveEvt(evtKey(&EvtMgr), 0, 0)) SceSleep(1);` in the inline
  wait loop, no `EventMgr* m = &EvtMgr` before it.** With `m` the lo_sum is set before the inner loop and gcse PREs one
  `high(EvtMgr)` for all six sites (one callee-saved reg); with `&EvtMgr` inside the loop test loop.c hoists the inner
  `addi` from its own high (the target's second `lis EvtMgr@ha`, r26, next to the SetEvt sites' r28).
- **r11e EmSet_exit 11 -> 0: `pe = &em;` AFTER `em.setEm(..)`** (the setEm `this` stays a fresh `addi r3,r1,8`; sched1
  hoists the pe addi to the top anyway since it crosses calls). **move_sasaeki1 8 -> 0: `const f32 w/h`** for the
  YarareInitCube constants (pool order w, h before 0.0; literals give the same code but move 0.0 first in the pool).
  **R11eInit 67 -> 0: `PSet(r11e_work->sat[i], SatMgr.create(..))`** for all eight create stores (the next create's `lwz
  pG` waits for the store; also fixed the koyaA/sakuA Pos/Rot high pairs' r27/r28, r23/r24 order).
- **r202 R202Init 2 -> 0, r10f R10fInit 6 -> 0 (tagged `candidate #17`, value-carrying pins): `register GlobalWork* g
  asm("r10"); g = pG;`** for the pG temp of a test right after `stw r3, work@l(r9)` (local-alloc adjacency of the work
  high and the pG load under the target's sched1 order) and, in R10fInit, for the setSubMotion block's pG temp (target
  pG r10 / work r11, ours reversed). A `pGS` struct view does not change it; pinning the work pointer to r11 works too.
- **r10f GondolaEmSet 71 -> 1: `s16 (*t)[3] = tbl; for (n = 0; n < 3 && t[k][n] != -1; n++)`** (no `p = tbl[k]` pointer:
  that made the `fp+24` PRE reg a second copy hoisted by loop.c, one more callee-saved register). Left 1 word: the
  peeled entry test is `lhax r0,t,k6` in the target and `lhax r0,k6,t` in ours -- expr.c `both_summands` puts a MULT
  term first in an EXPAND_SUM address (`(plus (mult k 6) t)`), cse/combine never reorder two REGs, and every spelling
  of the index (`t[k][n]`, `(*(t+k))[n]`, `*(t + k*3 + n)`, u8*/u32 casts) reaches the same MULT-first form while the
  loop's own giv init `add p,k6,t` matches; a byte-offset variable `ofs = k * 6` would give `(plus t ofs)` for the test
  but also for the giv add_val. Not found.
- Left: r10f GondolaGetOn 435 / GetOff 234 (frame layout + the `lwz 0xc/0x10/0x14/0x4/0x8` template-copy order of two
  Vec templates -- a different block-move shape; not iterated), r202 initCatapult 106 (#3) / setRock 4 (also a
  pre-existing pool-order diff at .rodata 0x1d8: -0.0349/-0.0698 swapped since the throwRock rewrite), r210 funcAshley2
  9 (the 1.0 `lfs`/`stfs` sit early in the target, after all `stb`s in ours; `"=m"` keep-alives, a hard-reg r0 zero,
  statement orders: 8-10), dai_go 10, toroko_ret 323, r222 (6 functions) not iterated.

### Stage rooms, st1_3/st2_0 pass 8 (r210 Matching 12/12 -> flipped; r222 22 -> 26/29, r202 30 -> 31/33, r10f 11 -> 12/15; 2026-09-11)

- Harness ~/.cache/rooms_b8 (rooms_a8 copies with the paths rewritten: `mcmp.py MOD/UNIT [SYM]` with `OBJ=`, `tryv.py
  MOD/UNIT SYM v/x.py` (variants dict, `(old, new, count)` triples for text that occurs in two functions), `mdump.sh
  MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE` (also for a tryv output under `out/`), `sbs.sh`, `fn.sh`, `rtl.py`,
  `prio.py`; `la_sim.py` = a local-alloc simulator (birth 2n / death 2m, fake lifetime, QTY_CMP_PRI, REG_ALLOC_ORDER
  r0,r9,r11,r10,r8..) fed with candidate sched1 orders — it reproduced ours for r202 setRock and listed the orders that
  give the target's names; deleted at the end).
- **r10f GondolaEmSet 1 -> 0, zero code: byte arithmetic `t + (k * 6 + n * 2)` with `u8* t = (u8*) tbl`.** expr.c
  both_summands swaps a MULT second operand to the front, so `t[k][n]` is `(plus (mult k 6) t)` everywhere; with the
  inner sum written first the address expands to `(plus t (plus (mult n 2) (mult k 6)))` (the outer plus keeps the
  base first because its second operand is a PLUS, not a MULT). The peeled entry test folds n = 0 to `lhax r0,t,k6`
  (target order) while loop.c's simplify_giv_expr re-associates `(plus t (plus n2 k6))` as `(plus n2 (plus k6 t))`
  (the USE operand is swapped behind the biv term), so the giv init stays `add p,k6,t`. Rule: to reorder a `(plus REG
  MULT)` address without changing the giv, write the index sum as its own parenthesised expression.
- **r202 setRock 4 -> 0 (tagged `candidate #17`, two value-carrying pins `register cObj* o asm("r10"); register cModel*
  pp asm("r11")` for the `pParts->rot.x = 0.0f` store).** Local-alloc simulation: the target's names (obj r10, pParts
  r11, -0.024 high r8) come out only when the -0.024 `lis` is issued in the same cycle as the obj load, i.e. before the
  0.0 `lis`; ours issues the 0.0 chain first (prio 4 vs 3: the -0.024 load has no consumer in the block) and no source
  form raises the second high's priority (step variable before/after, keep-alives, loop shapes: 4-8). Pinning
  pParts alone moves obj to r8 (the -0.024 high, span 4, is allocated before obj, span 8); pinning both leaves the high
  the only free qty -> r8.
- **r202 throwRock 6 -> 0: `const f32 step = -0.034906585f;` right before `v = -0.06981317f;`** (the const-at-top pool
  lever, mid-function: the entry is created at the declaration, the uses stay literals; the `.rodata 0x1d8` swap noted in
  pass 7 was this, not a pool-emission difference). .rodata of st2_0/r202 equal again.
- **r210 funcAshley2 9 -> 0, zero code: `FSet(p->motSpeedRate, 1.0f)`.** The block's `lwz pSUB` (fixed scalar, for
  `&pSUB->atari`) depends on a reference store but not on the in-struct `stfs`, so the 1.0 chain (lis, lfs, stfs) gets the
  `lwz pSUB -> lhz -> ori -> sth -> call` chain's priority and issues before the routine-byte stores like the target;
  without it the stfs is a leaf and sinks below all four `stb`s. Rule: when a constant store chain sits early in the
  target although nothing consumes it, look for a fixed-scalar load later in the block and store through a reference.
- **r210 dai_go 10 -> 0, zero code:** `cModel* m = pPL;` at the top of both `v.x = K; v.z = K; v.y = 0; m->setPos(&v)`
  blocks (the st4_0 slide_move lever: the pointer load leaves the store block and the two pool constants allocate x f0 /
  z f13 instead of z f0 / x f13) and the third block written `v.x = 0; v.z = K; v.y = 0` for the target's y, z, x store
  order (the shared 0.0 register's two stores bracket the z store; the pSUB block needed nothing).
- **r210 toroko_ret 38 -> 0, zero code: `static inline void r210_setAng(cModel* m, Vec* a) { m->setAng(a); }` for the
  three setAng calls.** The target recomputes `&ang` (`addi r4,r1,24`) at every setAng while `&pos` is one PRE'd pseudo
  (`mr r4,r26`); ours had merged the `&ang` uses with the template copy's destination pseudo (`mr r25,r30`, one more
  callee-saved register, frame naming shift). Through the inline the address is a hard-register argument set (never a
  gcse occurrence) — the FadeSet/penClothAtMake rule applied to a member call. Unit 12/12, `st2_0/r210.cpp` flipped,
  111 OK.
- **r222 BoxMove 13 -> 0 (tagged `#3` / `#13`, asm-emitted high + loads, named pool word).** The target shares ONE
  `lis 2.12@ha` (r10, set before the `bne`) between the then arm's store and the else arm's limit load, while the wait
  body reloads it; the block-based LCM never hoists an expression computed only in two sibling arms (isolated at the
  join), so the shape is #3. Recipe: top-level `asm(".section \".rodata\"\n\t.align 2\nr222_k212:\n\t.long 0x4007b8a5\n
  \t.section \".text\"")` right before the function (the word lands where the pool entry was; the pool keeps 0.05) +
  `extern const f32 r222_k212;` + `extern const f32 r222_k212_v asm("r222_k212");`; in the function `register u32 hi
  asm("r10"); asm("lis %0,%1@ha" : "=r"(hi) : "i"(&r222_k212));` before the `if`, `asm("lfs %0,%1@l(%2)" : "=f"(t) :
  "i"(&r222_k212), "r"(hi))` in the then arm and — chained on the loaded `rot.x` value with an extra `"f"(r)` input so
  it issues after `lfs f0,160(r11)` — in the else pre-block; the wait body reads `r222_k212_v` (fold-proof, `mem/u`,
  `lis r11; lfs f12,@l(r11)` like the target). A function-local `static const f32` + `FCRef` for the wait body puts the
  word in the same place but schedules the wait body's load last (8 words); the `"f"(r)` chain on the else-arm asm is
  needed too (2 words without it). `0x4007b8a5` = `2.1206448f` (compute the word with struct.pack; a wrong word only
  shows in the .rodata compare).
- **r222 dragon_down2/3 49+43 -> 0, zero code: `int hit = 0; u32 i = 0; int zero2 = 0;` declared AFTER the two EstSets
  (mid-block), only `int zero = 0` at the top.** The target has four zero registers (zero r30 local-alloc'd, hit r24,
  i r25, zero2 r23 global). cse canonicalises a register use to the class member with the latest REGNO_LAST_UID
  (`make_regs_eqv`): with all four set at the top, `zero`'s stack stores are rewritten to `hit`/`i` (whose last mentions
  are in the loop) and zero dies. Declared after zero's last use, the three inits join the class after its stores were
  processed; sched1 still hoists their `li`s above the calls to the block top (a `li` has no dependence on a preceding
  call), and `zero`, now block-local, takes r30 from local-alloc ahead of the globals. A tail mention of `zero`
  (`SceExec(.., zero, ..)`) keeps it the class head but survives as a `(use)`/copy and makes it global (r25): wrong.
- **r222 dragon_down 51 -> 0, zero code: `&obj->pos` written at both SndCall sites instead of a `Vec* pos` local.**
  gcse PREs the address into the CutCall(4) block's end = between LOOP_BEG and the entry jump of the following poll loop,
  which makes that loop phony for loop.c (the r113 execHide mechanism): its test reloads `lis CamCtrl@ha` per iteration
  and the exit code reuses that high for CutCall(5), exactly the target's shape; the pointer local sat above the
  EstSet and let loop.c hoist the high (`addi r3,r29`). goto forms of the poll loops: 20-191 words.
- **r222 R222Init 88 -> 72:** `R222Work*& wp = r222_work.p; wp = MEM_CALLOC(..)` hoists the calloc store's `lis
  r222_work@ha` into a callee-saved register before the call (the r11b/r402 idiom; a second high from the same symbol as
  the later loads' r31), `BitOn(SmdGetObjPtr(0x15)->be_flag, 0x20)` for the store before the first RsfCheck (`lwz pG`
  below it). Left (72): the four `r222_setHit` blocks — the target loads the first two YarareInitCube constants BEFORE
  the `hit[no]` store and reloads `r222_work.p` only after them (`lwz r9; li r4; lfs f1; li r5; stw r3,36(r9); lfs f4;
  fmr f3; lwz r9; lfs f2; fmr f6; lfs f5; lwz r3,36(r9)`); in ours every `lfs` of the block depends on the `stw
  r3,36(r9)` with cost 2 (`-fsched-verbose-6`: the store's dependents include the four pool loads although they are
  `mem/u` and the store is a plain in-struct MEM) and the work reload is issued first. PSet/RefHit/reference views do
  not remove that dependence; the mechanism (why a pool load waits for an in-struct store here) was not identified.
  Also the two PRE'd highs `lis pG`/`lis r222_work` in the prologue come out in the other order (gcse hash order).
- **r222 R222Main 7 (left, mechanism read):** target `li r0,30; lis r9,tpl@ha; stw r0,112(r11); addi r30,r1,8; lwz
  r11,tpl@l(r9); addi r9,r9,tpl@l; lwz r10,8(r9); lwz r0,4(r9)` = the template copy's word-0 temp took r11 (the work
  pointer's register, dead at the `stw`) and the addi-low is tied to the high, so sched2's anti-dependences give the
  order. That needs sched1 to issue `stw` before `lwz word0` and `lwz word0` before `addi-low`; ours issues the word-0
  load at t2 (mem/u, no dependence on the store) with the addi-low, the store at t3. A `static const Vec` + assignment
  (`pos = sePos`) makes the loads non-/u so they wait for the store, but with cost 2 the word-0 load lands at t4 and the
  addi-low at t2 (untied, `addi r11,r9`), 7-14 words in every spelling (init/assign/cast/memcpy/pointer/IntSet/dead
  do-while); no form found.
- **r202 initCatapult 67 (left, #3 read exactly):** gcse PREs `high(r202_work)` to the end of bb 0 (before the tbl
  template loop) in both builds (`-dG`: reaching reg inserted, copies in the for body and in the after-loop block, both
  carrying cse1's `REG_EQUAL (high ..)` note). loop1 re-materialises the body copy in the preheader (chain B, `lis r30`
  in both), cse2 re-materialises the after-loop copy in ours (cost 0 < REG) while the target uses the bb-0 register
  directly (`lwz r3,0(r27)` x8) and stores the loop's zero pseudo (`stw r28,180`) after the loop. The after-loop copy
  sits in the same block as its uses (calls do not end blocks here: 5 bbs), so cprop cannot propagate it; a REG_EQUAL
  copy is always re-materialised by cse2. `int zero` forms 67-93, `R202Work*&` view / dead do-while unchanged.
- **r10f GondolaGetOff 77 / GetOn 141 (left, read):** the target keeps `side` (`mr r23,r3`) and forms `&posA[side]` as
  `mulli r4,r23,12; add r4,r4,r31; addi r4,r4,24` off the mot table's frame pseudo (cse related value of `(plus fp 32)`
  through `(plus fp 8)`) with the posA copy's own address pseudo dying at the copy (r11); ours hoists `side*12`
  (`mulli r23,r3,12` in the prologue) and keeps `&posA` in a callee-saved register. The 24-byte two-Vec template
  copies load word 0 then 12,16,20 then 4,8 in the target and 0 then 20,4,8,12,16 in ours (the "second word pair"
  family); frame 0x14 bigger in the target.
- Fact confirmed: `expr.c both_summands` — "put a constant term last and put a multiplication first": `if (CONSTANT_P
  (op0) || GET_CODE (op1) == MULT) swap`; a PLUS second operand is never swapped. loop.c `simplify_giv_expr` PLUS: a USE
  (invariant) first operand is swapped behind a non-USE/non-CONST_INT second operand, two USEs keep their order.

### Stage rooms, st2_1 pass 8 (r20d Matching 32/32 -> flipped, st2_1.rel byte-identical; r204 EventChandelier1/2 94/96 -> 50/55 with the frame + save set fixed; r20e initPuzzle/checkPuzzle analysed, unchanged; 2026-09-11)

- Harness ~/.cache/rooms_a8 (rooms_c9 copies with the paths rewritten: `tryv.py MOD/UNIT FUNC variants.py`,
  `mcmp.py MOD/UNIT [SYM]`, `sbs.sh MOD/UNIT SYM [OBJ]`, `mdump.sh MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE`;
  `prio.py UNIT FUNCPAT` = global allocnos of dump/UNIT.i.{lreg,greg} with refs/len/priority in allocation order).
  Dump-flag reminder: `-dS` is sched1 (`.sched`), `-dR` sched2 (`.sched2`); `-fsched-verbose-9` (dash, not `=`) prints
  the dependence table with every insn's priority/cost/dependents and the per-cycle issue trace -- read it before
  theorising about a tie (below, every guess about weight/LUID was wrong until the table was read).
- **r20d checkSwitch 1 -> 0, unit + module flipped (all `COMPILER-DIFF: #12`, the AROUND-form tail reload).** The
  target's tail `move(0.0f)` reloads the pool 0.0 with the high shared with every other 0.0 load (`lfs f1,0.0@l(r29)`);
  our cse1 folds it to the compare's register on the AROUND path. No compiler-load form keeps both (a fresh tail ebb
  leaves the tail's high to gcse -> a second `lis` from P's reaching reg; a non-pool object loses the REG_EQUAL notes), so
  the 0.0 word is `static const f32 k0 = 0.0f;` in the pool's first slot (.rodata unchanged) and every 0.0 load is an
  asm `lfs %0,%1@l(%2)` through one asm `lis` (`u32 hi`, unpinned: it ranks below open/work/opened and takes r29 by
  itself; a `register .. asm("r29")` pin made `opened` take r29 = regs_used_so_far). Four consequences, each solved:
  (a) asm loads carry no REG_EQUAL note, so the multi-set `spd` (2 sets: update_equiv_regs doubles REG_LIVE_LENGTH on
  the FIRST noted set even when a later set clears the equivalence) and the compare copy `zero` lose their doubled
  length and outrank `t`/the pool constants: `spd` gets a keep-alive as an extra `"f"(spd)` input of the tail asm
  (+11 len -> 1.24 < t's 1.47), `zero` (function-scope) a `asm("" : : "f"(zero))` in the shared `sleep:` block (live
  everywhere: refs 7/len 121 = 0.116, below 0.005's 0.259 and above z0's 0.027 -> f28 after the constants, f27 for z0).
  (b) A `"m"(sw)` input (the address-taken `cEm* sw` slot) gives the spd asm its anti-dependence on the preceding
  SndCall (without it the asm is issued at t=4 beside the call) AND a second dependent (the call's memory flush), so
  it outranks the argument `li`s at t=5 like the target's `lfs` (prio 5 = theirs, depend count 2 vs 1).
  (c) An asm has latency 1, the pool `lfs` 2: the compiler copy `zero = spd` (fmr, priority 2 = fpu latency) became
  ready one cycle early and beat `li r30,0` (priority 1) for the t=6 slot; only readiness decides, no
  LUID/weight/class lever reaches it. Fix: the copy is an asm `fmr %0,%1` (priority 1) and `open = 0` an asm
  `li %0,0` with a FAKE `"=m"(sw)` output: the write after the load's read is an anti-dependence (ready at t=6, with
  `li r4,1`), the memory write makes the call depend on it (priority 5 = li r4's) and the 2 SETs give weight 2 > 1,
  so sched1 issues `li r4,1` first; sched2 then follows sched1's LUIDs. A `"+f"(zero)` chain, a `"cc"` clobber
  (REG_UNUSED cancels the weight), a junk second output (double-counts every input's REG_N_REFS: `hi` 6 -> 8 and it
  takes r31), a pinned-r3/r4 dependence (the arg `li` moves to t=5) and an `open` input (+2 refs -> open r31) all fail.
  (d) `SceAtSetEnable` must stay directly followed by the loop label (the flow nop `(use 0)` takes the t=4 slot beside
  the SndCall by weight 0; a copy written after the call removes it and `li r30,0` moves up).
- **r204 EventChandelier1/2 94/96 -> 50/55 (frame 184 and `stmw r16` now match; tagged `#13` + `12`).** The
  "unexplained 8-byte slot" of passes 3-7 is not a slot: with 15 saved GPRs (60 bytes) the save area rounds
  differently than with 16, so the target's ONE extra callee-saved register is the whole frame difference. That
  register is the untied `&crot0` pair `lis r30; addi r25,r30,crot0@l` (ours ties them `addi r30,r30`), with the high
  dying after the cpos setup (r30 is then reused for pPL). Form: `register u32 rh asm("r30"); asm("lis %0,%1@ha" :
  "=r"(rh) : "i"(&crot0)); asm("addi %0,%1,%2@l" : "=r"(rot) : "r"(rh), "i"(&crot0));` plus a codeless keep-alive
  `asm("" : "=m"(m) : "r"(rh))` after PSVECAdd (`m` is dead there); an unpinned `rh` or no keep-alive gives `rot` r30
  again (90-94). `mdl->setAng(&mdl->rot)` in the second block needs the r20d execRoundSwitch `asm("addi %0,%1,0xa0" :
  "=r"(rp) : "r"(mdl) : "cc")` (target `addi r4,r30,160; mr r3,r30`, ours `mr r3; addi r4,r3`). Left (50/55): the
  `mf -= 5` temp (`addi r0,r30,-5; mr r30,r0; cmplwi r0,65`: cse must NOT canonicalise the temp to `mf` in the
  compare, i.e. the temp's REGNO_LAST_UID must be later than mf's second compare in the Key block -- an asm `addi`
  into `mft` with `mf = mft; if (mft > 0x41)` is still folded to `addi r30,r30,-5` because cse rewrites the compare),
  the r25/r26/r27 naming of `rot` and the pG/work highs, and the chandOfs `addi r4` slot. Macro-line hazard: a `//`
  comment before the line-continuation backslash swallows it -- tags inside CHANDELIER are `/* COMPILER-DIFF: .. */`.
- **r204 nige_check 228 (not iterated, casetree read):** `python3 tools/research/casetree.py <19 cases> --target ..:r204_nige_check__Fv
  --ours st2_1/r204:r204_nige_check__Fv` shows the target's tree compares `cnt - 0x1E` (0x8, 0x13, 0x14, 0x1e, 0x28,
  0x32, 0x41, 0x46, 0x50, 0x55, 0x64, 0x78, 0xdb, 0xdc, 0xe6, 0xf0, 0xfa, 0x109 = our case values minus 30) with 18
  nodes: `case 0x8E` (em[8]) does not exist in the original switch. Rewrite the switch as `switch (cnt - 30)` (or the
  counter with a -30 bias) with 18 cases first; the rest of the 228 words was not looked at.
- **r20e initPuzzle 49 (unchanged; mechanism read).** In both layout nests the target has TWO stepping cell pointers
  with equal value (`add r11,..; mr r7,r11`, both `addi ,48`): the piece store through the copy (`stb r0,12(r7)`), the
  three `c->pos` loads through the base in y,x,z order. In ours the nest-2 (setLayout inline) ALSO has two pointers but
  split the other way ({x load} vs {store, y, z}): cse1's find_best_addr rewrites the bare-register address of the
  block copy's first word (`(mem (reg c))` -> the "costlier equivalent" `(plus 221 223)`) while `(plus c 4/8/12)`
  cannot be rewritten (a 3-term address is invalid), and after loop.c's move_movables left `223 = 312` the x-load's
  giv has add_val register 312, the others 223, so combine_givs (express_from needs structurally equal add_vals) keeps
  them apart; cse2 then makes the second init a copy (`mr`). The y,x,z load order is sched2's WAR on r0: the target's
  x word lands in r0 (pc's register, free after `mulli r9,r0,40`) and must follow the mulli. So the target's split
  needs the STORE's address in the other add_val family and ALL loads on the base: 25 forms tried (array/pointer
  splits for store vs loads, `(u8*)p + x*48 + (y*16 + 0x178)`, `&p->cell[x][0] + y` (41 words: the two-pointer shape
  but x-only split), pointer-biv loops `c += 3` with an explicit or inline-parameter copy `g = c` (44: cse merges the
  copy), byte-pointer and Vec-cast forms: 41-78). The single-use DEST_REG rule (`combine_givs` skips a DEST_REG giv
  used once as g1) means the store pointer must have >= 2 uses or be a non-replaceable giv; not found.
- **r20e checkPuzzle 224 (not iterated):** 158 differing lines in several independent regions (the pG-address form at
  the top, `lwzu` cell-address forms in the frame block, the slide blocks' store order and `li r31,1` placement) --
  a multi-pass job; casetree/xjump not applicable (no switch, no return tails).

### Stage rooms, tagged-forms pass 2 (st2_2 r213 30/30, st1_2 r11b 13/14 + cLight block, st1_2 r10c 23/23 flipped; st2_3 r225 10 -> 11/13: operateCrank 77 -> 0, SceElevator_r225 285 -> 157; 2026-09-11)

- Harness ~/.cache/rooms_c9 (rooms_c8 copy). Read the `-fsched-verbose-6` trace with `--> scheduling insn
  <<<N>>> on unit` lines before reasoning about slots; `sbs.sh` prints the TARGET on the left, ours on the right.
- **r213 StatusSetChain 2 -> 0, zero code (store order = local-alloc order of the pool constants).** Four pool
  constants (0.8 f0, 50 f13, 0.1 f12, 0.0 f11): sched1 sinks the non-dying `x48 = 0.0` store behind three zero
  stores (0.0's qty life 63 > 1.5x the others), and `x4C = 0.1f` written last puts its store after the dying x54
  (0.1's life equals 50's; the earlier qty wins). Only the statement order changes.
- **r213 Init 14 -> 0 (COMPILER-DIFF 3):** the second `memset(&rot)` argument is a hard-register variable
  (`register Vec* a3 asm("r3"); a3 = &rot; pr = a3; r213_memset(a3, 0, 12)` with the varargs `asm("memset")`
  alias for `crclr`), then `asm volatile("")` right after the call keeps the later gcse insertions behind it; `pr`
  replaces `&rot` in the four `SatMgr.create` calls.
- **r11b Init 14 -> 0:** copy-initialisation by placement new (`new (&pos) Vec(r11b_boatPos0)`) gives the `mem/s/u`
  template loads (assignment loses /u through the synthesized operator=); a codeless `asm("" : "=m"(rot2.x))`
  between the two `flags_51BC` RMWs is a sched1 issue-slot filler (tagged candidate; the original block had one more
  insn there). The nameless cLight block differs only in REL24 fields.
- **r10c hako_down 16 -> 0, zero code:** `const f32 lim = -16141.0f;` folds every use to the literal; the pool entry
  is created at the declaration, and a literal used twice in the loop becomes a loop.c pass-2 movable (262 insns,
  the extra threshold step keeps `-100.0` in the arm behind the 0.022 pair).
- **r10c SetEmHitAtari 5 -> 0 (COMPILER-DIFF 3):** the target's early `high(0.0)` is a dead pinned pool load
  `{ register f32 z asm("fr0"); z = 0.0f; }` at the top (cse forwards the constant, the hard reg is clobbered by
  the calls); `BitOn(SmdGetObjPtr(0x6C)->be_flag, 0x20)` as the reference store keeps the RsfCheck `lwz pG`/`lhz`
  below it; pool ORDER without code = a block of folded `const f32 k750 = 750.0f; ...` declarations; the three
  `spd = 0.01/0.05/0.06f` assignments hoisted before the `if` (both arms set them).
- **r225 operateCrank 77 -> 0, five independent pieces (three tagged candidates):**
  - Layout: `while (1) { ...; if (!(x < 800.0f)) { gnd_open(); break; } body...; SceSleep(1); } else break;` puts
    the gnd_open arm at the loop's end (zero code).
  - loop.c pass 2 (262 insns, T = 71): the 2^52 conversion magic is hoisted because `4*71 >= 262`; TWO earlier
    movables lower T to 65 first. A dead invariant product `ratio = pPL->pos.y * 3.7f;` before the real `ratio =`
    supplies `high(3.7)` + the forced pool load (both deleted at flow1 with the dead store; no callee-saved FPR
    because nothing survives to reload). Tagged `candidate (loop.c pass-2 threshold)`. Dead FP compares fail here
    (the hoisted constant's compare survives to flow2 and costs an `stfd`).
  - gcse PRE pseudo numbering: verified `hash = 119+6+(61<<7)+h(name)`, `h = h*129+c`, table = n_insns/2|1 buckets;
    the three loop highs (`CamCtrl`, `.LC27`, `r225_work`: 3 refs, lengths 688/682/696 -> all priority 43) are
    allocated in pseudo-number = bucket order. Ours (223 buckets) gives LC27 < work < CamCtrl; the target's
    r19/r18/r17 needs CamCtrl < LC27 < work = 219/229/233/235/281... buckets. Three dead big-constant tests
    `if (spd == 0x12345) lvl = 0;` (4 insns each, deleted at flow2) reach 229; other counts (1-5 simple, 1/2/4 big,
    mixes) miss either the bucket window or a global-alloc truncation window. Tagged `candidate (gcse table size)`.
    Compute candidates with the formula before scanning.
  - `pos.y = pPLS->pos.y;` (struct view, `struct PlPtr { cPlayer* p; }`): the scalar `mem/f` pPL load has no
    dependence on the in-struct template stores (`fixed_scalar_and_varying_struct_p`), so its chain
    `lwz pPL -> lfs pos.y -> stfs 20(r1) -> [unknown-base alias] lwz crank -> lfs -> fsubs -> stfs` outranks the
    `r225_work` load (21 vs 17) and sched2 issues it first; the in-struct load waits for the stores and the work
    load goes first as in the target. The `stfs 20(r1) -> lwz 0(r11)` dependence exists in both (loaded pointer =
    unknown base); const views (`/u` casts) do not remove it.
  - Block-0 issue slots: the target issues only `lis pPL@ha` in cycle 3 and only `bl` in cycle 8, so every free
    `li 0`/hoisted `lis` lands one slot later than ours; haifa's ready loop issues from the top of the sorted list
    and free insns fill every slot. A codeless asm on the first call's argument (`u32 n; asm("" : "=r"(n) :
    "0"(0x16)); SmdGetObjPtr(n)`) is ready one cycle after `li r3,22` with the argument's priority and takes the
    cycle-2 slot, which shifts the fillers exactly as the target. A `"=m"(local)` asm placed first also fits but
    wins cycle 1 over `li r3` (equal priority = `1 + prio(bl)`, lower LUID) and costs a frame slot; `"m"(global)`
    inputs and a bare `asm("")` (volatile barrier) wreck the block. Tagged `candidate (sched2 issue-slot filler)`.
- **r225 SceElevator_r225 285 -> 157 (all zero-code):** inline `SetPosXYZ(m, x, y, z)` for every Vec block with the
  PLAIN argument form in the shake loops (`SetPosXYZ(pPL, pPL->pos.x, fRand1_1() * step + pPL->pos.y, pPL->pos.z)`:
  argument MEMs are evaluated lazily -- pointer before the call, load after it, `lwz r30,pPL; bl; lfs 148(r30)`; a
  `y` local first gives the reload form); the FadeSet colours in an inline `FadeSetRGBA(mode, rgba0, rgba1)` whose
  locals are ONE 12-byte BLKmode struct (`GXColor c0, c1; u32 pad`): 4-byte GXColor locals are ADDRESSOF pseudos
  (SImode) that purge_addressof turns into permanent slots (24/28, 32/36); the inline frame temp is BLKmode and
  `assign_stack_temp` reuses the free 12-byte Vec slot only for an equal mode (8/12 as in the target). A
  `FadeWork* fade = &Fade[2];` before the goto up-loop is the target's hoisted `lis/addi Fade+0x48` with
  `lhz 0x18(fade)` in the arm. With the register pressure of these forms the `faded == 0` compare lands in cr4
  (`mfcr r12` prologue: TexRegist pass-0 rule, all used callee-saved GPRs conflict). Residue 157: ours PREs
  `high(pG)`/`high(RoomData)` to the pre-shake-loop block (bb 5 "PRE/HOIST", redundant RsfSet + quake-store
  occurrences); RoomData's pseudo is rematerialised from its REG_EQUIV but pG's gets r19 (14 GPRs, frame +8) while
  the target recomputes `lis pG@ha` at every use (13 GPRs); the target hoists `li 255` (FadeSet colour) and the
  pre-loop `done = 0` survives as the compare operand (`cmpwi cr4,r26` = a second zero register), ours folds a
  fresh `li 0`; FP callee-saved numbering (f26-f29 permuted) and the down-loop's `hSnd`/`done` block follow from
  those. sce_com's own SceElevator (DOL) is 82% with the same shapes.

### Stage rooms, st2_1 pass 9 (r204 21 -> 23/24: nige_check 228 -> 0 zero code, EventChandelier1 50 -> 0, EventChandelier2 55 -> 4; r20e initPuzzle 49 -> 37, checkPuzzle untouched; neither flipped; 2026-09-11)

- Harness ~/.cache/rooms_a9 (rooms_b9 copies with the paths rewritten; `order.py MOD/UNIT OBJ` now finds the module split
  object, `prio.py` parses the `(2)` size marks of `regs to allocate`; deleted at the end).
- **r204 nige_check 228 -> 0, zero code (four levers).** (1) The switch is `switch (r204_work.p->cnt - 30)` with 19 cases
  (pass 8's casetree read missed the ROOT: `cmpwi r9,0x70; beq` compares `cnt` itself because combine folds the first
  EQ compare of the `subi` temp back onto the source register -- LOG_LINK to the first use only, `added_sets_2`
  keeps the `subi` -- so the tree's root 0x52 = cnt 0x70 is the em[8] arm: our `case 0x8E` was the wrong VALUE, not an
  extra case; values `cnt - 30` = 8, 0x13, 0x14, 0x1e, 0x28, 0x32, 0x41, 0x46, 0x50, 0x52, 0x55, 0x64, 0x78, 0xdb,
  0xdc, 0xe6, 0xf0, 0xfa, 0x109, plain-mode balance root = 10th node). (2) `static inline int r204_isDead(cEm* em)
  { return (em->flags_324 & 0xFFFF0000) ? 1 : 0; }` for the `li r9,1; andis.; bne; li r9,0; cmpwi r9,0` chain (also
  fixed the r16/r17/r18 naming of the three Vec templates). (3) `cPlayer* pl = pPL;` assigned right before the
  `if (started == 0 && pl->checkEvent() == 1 ..)` and used again for `pl->pWep->pObj->setDisp(1, 1)` (target `lwz
  r28,pPL@l` above the `cmpwi r19,0`, `mr r3,r28`, `lwz r9,0x788(r28)`; every other pPL use reloads). (4) Loop
  counters: each `for` gets its own `u32 i` (the function-scope `i` shared by five loops was one pseudo with 10 sets
  ranking above every giv -> r30 in every loop; the target has i/giv = r29/r30, r30/r31, r30/r31, r31/r30 (far loop),
  r29/r30). The last loop's `i` then still took r31 (global pass 0: r29 not yet in `regs_used_so_far` when it was
  allocated, pass 1 gave the first free register) until the `ang` block got the r102 form `Vec* pa = &ang;` declared
  AFTER the setNoSuspend loop with `pa->y = K` through the pointer and `setAng(pa)` in both arms (x/z stores direct, y
  `stfs f0,4(r29)`, `mr r4,r29`): `pa`'s two extra refs (8 -> floor(log2 8)=3) rank it above the last loop's counter,
  it takes r29 in pass 1 and the counter finds r29 used-so-far. `pa` at the block top costs a `mr` copy (40 words),
  `setAng(&ang)` with `pa` for the store only gives the PRE'd second pseudo (207). (5) `FSub(SmdGetObjPtr(0x39)->pos.y,
  7.0666666f)` keeps `lwz pG` below the store (the r20e moveCrestDoor rule).
- **r204 EventChandelier1 50 -> 0 (two zero-code levers, the pass-8 keep-alive removed, two `candidate #17` pins).**
  - `if (mf - 5 > 0x41) OK else NO; mf -= 5;` (the decrement AFTER the if/else) is the target's `subi r0,r30,5; mr
    r30,r0; cmplwi r0,0x41`: gcse deletes the redundant `mf - 5` at the join and inserts the reaching copy `mf = T` at
    the END of the compare block, i.e. between the compare and its branch, where regmove's optimize_reg_copy_1 cannot
    see the compare any more (it scans forward from the copy and stops at the JUMP_INSN). Every "copy before compare"
    spelling (`t = mf - 5; mf = t; if (t > ..)`, `(mf -= 5) > ..`, int/u16 views, an r0-pinned temp, both arms
    decrementing) is undone by regmove (T dies at the compare -> T replaced by mf -> tie), 43-56 words.
  - `BitOn(pl->be_flag, 0x10)` in the tail keeps the endEvent `lis pPL@ha; lwz` below the flag store (8 -> 0 with the
    next item).
  - `register Vec* rot asm("r25")` and `register cModel* mdl asm("r30")` (tagged `candidate #17`), keep-alive asm
    dropped. Block 0's local-alloc gives `rot` (3 refs / span 54, PRI 3) a register before the pG and work highs (3 refs
    / spans 130 and 144, PRI 1) -> rot r27, pG r26, work r25; the target has pG r27, work r26, rot r25, which needs
    rot's PRI below both (span >= 98) or the highs at 4 refs -- neither has a source form. With rot pinned the pass-8
    `rh` keep-alive is unnecessary (rot can no longer take the dying high's r30) and its codeless insn was the one
    real insn that pushed the truncated global priorities (`int(30000/len)`) of the four loop-hoisted highs
    Key/ActBtn/"%d"/2^31 (len 434/440/438/436 -> 69/68/68/68, Key first, then allocno order) off the target's.
    `mdl` (2 sets, global rank 1 among the callee-saved pseudos) takes r28 in ours although r30 is free after the
    high's death (pass-0 exclusion not identified); pinned r30 as the target.
- **r204 EventChandelier2 55 -> 4 (left): the ActBtn / "%d" highs are r17/r18 swapped.** Same macro, but the second
  function has a different gcse table (213 buckets vs 207) and pool labels (.LC61 for its 2^31 double), so the
  priority tie among ActBtn/"%d"/2^31 (all 68) is broken by bucket order "%d"(5) < ActBtn(50) < 2^31(133); the target
  needs ActBtn < "%d" < 2^31, which no table size 151..321 gives with our `.LC` numbering (model: bucket = (7933 +
  h(name)) % size, h = h*129 + c over the SYMBOL_REF string, HIGH 119 + SImode 6 + SYMBOL_REF 61 << 7 -- verified
  against the dump's "hash value" column). Our TU carries 6 unemitted header-string constants (LC4, LC10-14) that
  the original's include set may not; an asm-emitted `lis r18,ActBtn@ha` pin (r3-pinned `addi` with a "cc" clobber
  so loop.c does not hoist the lo_sum) names the registers but lands the `lis` in the first free slot (4 words:
  the PRE insertions' LUIDs are later than any source asm). Not applied.
- **r20e initPuzzle 49 -> 37, zero code: two stepping cell pointers in both layout nests.** `R20eCell* c = &p->cell[0][y];
  R20eCell* cs = c;` before the x loop, `cs->piece = pc` through the copy, `pos = c->pos` through the original, `cs += 3;
  c += 3;` at the body end (the copy's increment FIRST: cse's `(set REG0 REG1)` swap makes the later-mentioned register
  the lo_sum's destination, so `c` must be mentioned last to stay the loads' pointer). Gives the target's `mr r7,r11`
  preheader copy, `stb r0,12(r7)`, loads through r11 and both `addi ,48` in the latch. Left 37: the three copy temps'
  names (target x r0, y r8, z r10 and the y,x,z load order; ours x r10, z r8, y r7 in x,y,z order): local-alloc's
  fake-lifetime rule (`fake_birth = birth - 2`) refuses r0 to a qty born right after `pc` dies at `mulli r9,r0,40`;
  sched1 issues `lwz x` in the mulli's cycle. Reading `cObj* o = q->obj` BEFORE the copy moves the stack stores of
  `pos` below `lwz o` (may-alias through the unknown-base pointer) and gives y r0 (29 words) -- structurally worse;
  not applied. `Vec* v = &c->pos`, memcpy, `Vec pos = c->pos`, memberwise y,x,z (67): no r0.
- r20e checkPuzzle 224 not iterated (pass 8's region list stands).

### Stage rooms, st1_3/st2_0 pass 9 (r222 Matching 29/29 -> `st2_0/r222.cpp` flipped; r202 initCatapult mechanism read (67 unchanged); r10f GondolaGetOff 77 -> 26, GetOn 141 -> 126 via the struct-view address form; 2026-09-11)

- Harness ~/.cache/rooms_b9 (rooms_a8 copies with the paths rewritten: `mcmp.py MOD/UNIT [SYM]` with `OBJ=`, `tryv.py
  MOD/UNIT SYM v/x.py`, `mdump.sh MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE`, `sbs.sh MOD/UNIT SYM [OBJ]`, `fn.sh`,
  `prio.py`, `order.py`; new: `tailcmp.py MOD/UNIT [OBJ]` = masked compare of the nameless `fn_*` linkonce block
  (`0 word diffs` = only reloc names differ), `flipchk.sh MOD UNIT...` = re-link the module with our objects for the
  named units (rsp built from build.ninja) + `make_rel.py --verify` against the original REL; deleted at the end).
- **r222 R222Main 7 -> 0, zero code: `int seReset = 30;` at the function top, `seTimer = seReset`.** The pseudo is set
  in bb 0 and used once in the store block, so at sched1 the `stw` has no `li` to wait for and is issued at t1 with
  the template's `lis` (t1: lis + stw; t2: `lwz word0` + addi-low, w0 first by weight 0 vs +1); update_equiv_regs
  then moves the `li` right before the store (after sched1: `li r0,30` is a 2-insn qty, r0) and local-alloc gives
  word 0 r11 (the work pointer died at the stw before its birth) and the addi-low r9 (born after the high's death
  at the lwz). Rule: when the target issues a constant store BEFORE `mem/u` loads that our sched1 puts after it,
  give the constant a pseudo set in another block (a literal is a `li` in the block that delays the store one cycle).
- **r222 R222Init 72 -> 0, zero code, two pieces.** (1) `r222_setHit` as a MACRO, not an inline: integrate.c copies
  the inline body's pool loads without RTX_UNCHANGING_P (the `mem/u` MEM becomes `(mem (reg))` through the forced
  address), so all four YarareInitCube constants of every block got a cost-2 true dependence on the `stw hit[no]`
  (`-fsched-verbose-6`: the store's dependents listed the `lfs`es) and issued after it; as pool MEMs of the function
  itself they are `mem/u` and never depend on a store (the em2dGravityMove lever). (2) `R222Work*& wp = r222_work.p;`
  declared AFTER `pG->flags_64 |= 0x20000`: the two PRE'd highs (`high(pG)`, `high(r222_work)`) are inserted at the
  end of bb 0 in expression-index = first-occurrence order and fill sched1's free slots in that order (target `lis
  r22 pG` before the stw, `lis r31 work` after); with the reference first, `high(r222_work)` was the earlier
  expression (its lo_sum is expanded at the declaration). Also `static Vec r222_zero` (make_rel --verify: ADDR16
  fields S+A, the split's `lbl_st2_0_data_300` is `scope:local`). Unit 29/29 (the 0x3B8 nameless cLight block
  differs only in REL24 names), `st2_0/r222.cpp` flipped, 111 OK.
- **r202 initCatapult 67 (unchanged; the mechanism is now exact, two pieces):**
  - The after-loop `stw r28,180(r9)` is the LOOP's hoisted zero: zero-code with `int zero;` declared before the loop,
    `zero = 0;` written INSIDE the body right before `cat[i].state = zero` (state/timer/thrown/nArea = zero, the two
    ScePrim* stores may stay literal 0: cse's class head), `cat[2].timer = zero` after the loop. loop.c hoists the
    user variable like the literal (same movables position: after `lis work` and `li 1`), and the after-block uses
    the same pseudo. Declared at the top or set before the loop, its `li` sits before the hoisted insns (LUID).
  - The after-loop `lwz r3,work@l(r27)` x9 = gcse's reaching reg R (`PRE/HOIST: end of bb 0, copying expression 13`)
    used directly: our copy `r_a = R` in the after-block carries cse1's `REG_EQUAL (high work)` and cse2
    re-materialises it (`high` cost 0 < REG 1); cprop cannot help (the copy is in the same block as every use --
    AVIN-based `find_avail_set`, and `insert_insn_end_bb` only reaches bb 0). Every sched/cse path was checked: cse's
    ebbs cannot carry bb 0 into the after-block (the template-copy label and the loop label end them; a backward
    `bne` is never followed: `no_labels_between_p` fails), `-fno-rerun-cse-after-loop` keeps the copy as `mr` but does
    not propagate it, a dead do-while around the loop hoists the body high to the top. TAGGED FORM TRIED (11 words,
    not applied): `register u32 hi asm("r27"); asm("lis %0,%1@ha" : "=r"(hi) : "i"(&r202_work))` at the top plus
    `({ asm("lwz %0,%1@l(%2)" : "=r"(t_) : "i"(&r202_work), "r"(hi), "m"(*(R202Work**) hi)); asm("" : "=r"(w_) :
    "0"(t_)); w_; })` for the nine reloads (the fake register-addressed "m" gives the store->load dependence
    without a second high; the codeless chain asm restores the load's 2-cycle latency so `li r4; li r5; addi r3`
    keep the target order) and `asm volatile("" : : "r"(hi))` at the end (the last reload otherwise reuses r27).
    It reproduces everything except bb 0: the asm `lis` has prio 1 / weight +1 like `li r10,48` and wins the t3 tie
    by LUID (any source insn precedes the block move's insns; the PRE insertion's LUID is after them), a "memory"
    clobber (weight +2) drops it behind the PRE `addi`s. So the `lis r27` in bb 0 can only be gcse's own R; the
    function needs R to survive (the #3 "target uses R directly" family: r10c r27, r120 r31, R213Init).
- **r10f GondolaGetOff 77 -> 26, GetOn 141 -> 126 (pure C++, no tag): `struct R10fGondolaTbl { void* mot[2][3];
  Vec posA[2]; Vec posB[2]; }; R10fGondolaTbl& t = *(R10fGondolaTbl*) mot;` and `&t.posA[side]`, `&t.posB[side]`.**
  The target's `mulli r4,side,12; add r4,r4,r31; addi r4,r4,24` (`addi 48` for posB) with `r31 = fp+8` (the mot
  copy's dest pseudo) is expr.c normal_inner_ref: base object `*t` at fp+8, variable offset `side*12` added first,
  the constant field offset (bitpos 24/48) last -- the bitpos-folding test (`(alignment * BITS_PER_UNIT) ==
  GET_MODE_ALIGNMENT (mode1)`) fails for a Vec (alignment 4, BLKmode), while for `mot[side][2]` (SImode, alignment 4)
  it folds the constant into the frame base first (`addi r9,r1,16; lwzx`), in both builds. A standalone
  `&posA[side]` forces `(plus fp 32)` into its own pseudo (r29) and cse merges it with the template copy's address.
  The plain struct initializer (`t = {{{..}},{..},{..}}`) is wrong (memset + element stores); the mot temp-copy and
  the two Vec templates need three separate array declarations. Left: GetOff's target recomputes `side*12` at each
  of the three uses (`mr r23,r3` kept, three `mulli`) while cse1 shares ours (bb-0 ebb through the pSUB fall-through
  and the AROUND path to the join) -- and GetOn's target DOES share it (`mulli r26,r22,12; mr r4,r26; add r4,r4,r23;
  addi r4,r4,24`: the copy `r4 = r26` and the `(r_m + r_8) + 24` association survive, ours combines to `addi
  r4,r26,24; add r4,r31,r4`); both read as the address chain computed with the hard argument register as its
  target (a hard-reg dest is not a gcse/cse candidate, an `(set r4 (plus r4 ..))` chain is not re-associated by
  combine) -- no source form found that expands a call argument that way (`static inline` on the functions,
  `&((Vec*) &mot[2])[side]`: unchanged). Also left in both: `li r4,0; li r3,9` before `stw r0,32(r1)`, the
  `lis r22`/`lis r21` order, GetOn's `mr r23,r8` (&mot copied early) and the 0x14 frame difference.
- Facts read this pass: integrate.c drops RTX_UNCHANGING_P from an inlined body's constant-pool loads (only
  MEM_IN_STRUCT_P / MEM_VOLATILE_P / alias set are copied); cse's `use_related_value` applies to CONST (symbol +
  offset) only -- frame addresses `(plus fp N)` have no related-value folding; `cse_end_of_basic_block` follows a
  conditional jump TAKEN only when the label has one use and is preceded by a BARRIER, AROUND only when the jump is
  forward with no label between (a loop's back edge is never followed; the fall-through continues the ebb); the
  `(use (const_int 0))` flow nop is emitted only after a CALL_INSN that ends a block; a codeless `asm("" : "=r"(w) :
  "0"(t))` chained on an asm load restores the 2-cycle latency the asm load lacks (asm producers cost 1).

### Stage rooms, st2_1 pass 10 (r204 Matching 24/24 -> flipped, st2_1.rel byte-identical: EventChandelier2 4 -> 0 with the `.LC` label lever; r20e initPuzzle 37 -> 0 zero code, 30/32; checkPuzzle 224 mechanisms read, unchanged; 2026-09-11)

- Harness ~/.cache/rooms_a10 (rooms_b9 copies with the paths rewritten, `bucket.py N NAME..` = the gcse bucket model,
  `fnr.sh MOD/UNIT SYM [OBJ]` = objdump -dr of one function with the reloc names inline into out/fnr_{t,o}.s, plus a
  `-D`-free debug build of cc1plus in `gcc/` with `getenv("GDBG")` prints in global.c's find_reg/prune_preferences --
  the fastest way to see WHICH allocno excludes a register: `cp -r tools/sn-gcc/{Makefile,src,obj}`, edit, `make cc1plus`
  (1 s), run it on the `.i`); deleted at the end.
- **r204 EventChandelier2 4 -> 0, `.LC` label lever, unit + module flipped (tagged `COMPILER-DIFF: candidate (gcse PRE
  pseudo numbering)`).** The four loop-hoisted highs Key/ActBtn/"%d"/2^31 tie at global priority 68 (Key 69) and are
  allocated in PRE-pseudo = gcse-bucket order, bucket = `(7933 + h(name)) % size`, `h = h*129 + c` (bucket.py reproduces
  every "hash value" of the -dG dump). "%d" is numbered ONCE (in EventChandelier1, shared), each function's 2^31 double is
  its own label: with our labels LC51/LC52/LC61 the order is right in 1 (207 buckets: ActBtn 20 < 2^31 118 < "%d" 119 ->
  r19/r18/r17) and wrong in 2 (213: "%d" 5 < ActBtn 50 < 2^31 133; the target has ActBtn r18, "%d" r17, 2^31 r16).
  Search over (label shift j from EventChandelier1 on, dead labels k in 2, table sizes): j = 21..27 satisfies BOTH
  functions at the current sizes (1: 20 < 170 < 171; 2: 50 < 51 < 179), j = 20 puts "%d" in ActBtn's bucket (wins only by
  insertion order); the alternative 5-6 dead sets (217 buckets) + 9 dead labels in 2 costs more statements. Applied: 21
  dead `f32 lcN = N.5f;` at the top of EventChandelier1 (deleted at cse1, pool entries never output, .rodata/code
  unchanged; the later functions' labels shift too and stayed identical). j = -12 also satisfies both, i.e. the original
  TU had ~12 fewer labels before these functions (our 6 unemitted header-string constants are not the whole story).
  Flip: `setTexRender` stays a placeholder in symbols.txt (r20a's global `setTexRender__Fv` exists in the module; ours is
  static, the -r link is fine) -- mcmp/unit_info then show 1 word (reloc name) in R204Init and a "missing" setTexRender;
  judge with `nm -n` (order + offsets identical) and `make_rel.py --verify` (OK).
- **r20e initPuzzle 37 -> 0, zero code (four levers, no tags).** Mechanism, read with the GDBG cc1plus: the x/y/z copy
  temps of `pos = c->pos` are GLOBAL allocnos (live from bb 13's loads into bb 14's `q->obj->pos` stores), not local
  qtys -- the pass-9 fake-lifetime story was wrong. x skipped r0 because `regs_someone_prefers[x]` had r0: the `mulli`
  temp (`pc*40`, local-alloc'd r9 but still an allocno) inherits `pc`'s r0 full preference through expand_preferences
  (pc dies at the mulli, no conflict), conflicts with x (x is born while it lives) and ranks BELOW x (2*6/7 = 1.71 vs
  3*9/11 = 2.45 at loop depth 3 = function + 2 loops; flow weights refs by depth) -> pass 0 excludes r0 for x, z, y.
  (1) Depth 4 flips the order (3*8/7 = 3.43 > 3*12/11 = 3.27): both layout nests are wrapped in `do { } while (0)`
  (depth 2 works too, 1.14 > 1.09, but needs a goto outer loop). The loop notes are a sched1 barrier, so what the
  target issues before `li y,0` must be computed before the do-while and the rest inside: nest 1 `{ u32 tbl = (u32)
  r20e_initLayout; do { y = 0; R20E_SET_LAYOUT(p, tbl); } while (0); }` (the table address is a plain statement
  before it -- a loop.c hoist would land after the barrier; a `u32`, not a pointer, keeps `add y,tbl` in the written
  order; loop.c does NOT hoist out of a do-while(0)), nest 2 `{ R20eWork* w = r20e_work; u32 tbl = ..; int x, y; do
  { y = 0; R20ePuzzle* q = &w->puzzle; R20E_SET_LAYOUT(q, tbl); } while (0); }` (`lwz r20e_work` before the barrier,
  `li r8,0` then `addi q,20` after; `y = 0` written BEFORE q so its LUID wins the tie; putting q outside gives `addi`
  before `li` and swaps the r10/r11 of w and the table high through the local-alloc qty lengths). Putting
  `last->piece = -1` inside the do-while gives `last` a weighted ref and swaps r18/r19 with the pieceObjId table
  address; wrapping the whole function shifts the object loop's r22-r24.
  (2) Latch order `addi c,48; addi cs,48` = source order `c += 3; cs += 3;`; cse's `(set REG0 REG1)` special case
  (which would swap the lo_sum's dest to the later-mentioned `cs`, pass 9) fires only when the copy DIRECTLY follows
  `c`'s set, so `x = 0;` sits between `R20eCell* c = ..;` and `R20eCell* cs = c;` (`for (; x < 3; x++)`).
  (3) Nest 2's `add r4,r8,r11` (y first) needs the table as a non-pointer: the inline `r20e_setLayout(const s8
  tbl[3][3])` made it pointer-first; nest 2 is the same macro with block-local counters. (4) The `((const s8*) (y +
  tbl))[x * 3]` index gives the same giv as `tbl[x][y]`.
- **r20e checkPuzzle 224 (unchanged; mechanisms of the first two regions read, four more listed).** (a) The loop
  arm's `SceMesSet(1, .., cMes.getWork()->..)`: the target keeps `addi r9,r20,cMes@l; addi r9,r9,4` in the arm with
  the high PRE'd to bb 0 (`lis r20` after SceEventStart, our gcse does the same: reaching reg at the end of bb 0, the
  pre-loop occurrence kept); ours then has loop.c move the lo_sum (`move_insn` via its REG_EQUAL symbol, savings 2,
  life 13 -> 71*2*13 >= 686 insns) and its `+4` to the preheader where combine folds them to `cMes+4@ha/@l`. The
  life 13 is the stale REGNO_LAST_UID of the inline's `this` (the second `getWork()`'s copy was cse'd and deleted, no
  reg_scan before loop 1); a `MesWork* w = cMes.getWork()` local gives life 10, still moved. The original's movable
  had life <= 4 or the loop >= 1847 insns -- not found. (b) Key: the body-top `high(Key)` (life 3, savings 2) is
  "not desirable" in loop pass 1 (639 < 686) but `combine_movables` matches the end-test's `high(Key)` into it
  (savings 3) and pass 2 (616 insns) moves it (639 >= 616) -> our `lis r23` in the preheader; the target recomputes
  both (`lis r9; addi r8,r9,Key@l` at the body top plus the gcse copy `lis r25` for puzzleMove's Key.trg test, and
  `lis r9; addi r9,r9` at the end test). Needs the two not to match (`!m1->global`, same src) or >= 640 insns at pass
  2. (c) The frame block: target `lwzu r11,376(r9); lwz r0,4(r9); lwz r10,8(r9)` = x loaded through `(mem (reg c))`
  (the pass-8 find_best_addr story), ours `addi r11,r9,376; lwz r10,376(r9)`. (d) The four slide loops: `mr r10,r9;
  mulli r9,r9,48` (the cx value copied before the multiply), `lbz r0,12(r9)` off `p + 376 + k*16` vs our stepping
  `lbz 0(r9); addi r9,48` giv, `li r3,1; li r4,-1` (state 1 / -1 constants in r3/r4, ours r31/r3), `lwzu r8,328(r9)`
  for the `to->pos` copy, and the target's temp order (x r8, z r0, y r10; `stw r0,16(r1)` after `addi r11,16`).
  Not iterated further.

### Stage rooms, st2_3 pass 3 (r225 Matching 13/13 -> st2_3 fully linked; SceElevator_r225 157 -> 0; 2026-09-11)

- Harness ~/.cache/rooms_c10 (rooms_b9 copies with the paths rewritten: `mcmp.py MOD/UNIT [SYM]`, `tryv.py
  MOD/UNIT FUNC variants.py`, `vapply.py`, `mdump.sh MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE`, `sbs.sh`, `prio.py`,
  `tailcmp.py` for the nameless cLight block). The 13th "function" `fn_st2_3_14FFC` is the linkonce block (0 word diffs,
  27 REL14/REL24 key diffs = naming), sections equal, order equal; `make_rel.py --verify` identical.
- **Read the loop shape from the layout, then reproduce the NOTES, not just the CFG.** Both elevator loops are laid out
  `b TOP; SLEEP: SceSleep(1)[; spd += accel]; TOP: body; ... -> SLEEP`. Three different sources give those bytes and
  they differ in what the compiler passes see:
  - `goto TOP; SLEEP: SceSleep(1); TOP: ...; goto SLEEP;` (goto loop): no notes at all.
  - `for (;;) { body; if (done) { tail...; break; } SceSleep(1); }`: expand_end_loop's rotation moves
    [start_label .. the FIRST jump to end_label] behind the rest (`LOOP_BEG; b start; newstart: SLEEP; start: body;
    beq newstart`), so a deep `break` DOES rotate as long as no NOTE_INSN_LOOP_BEG (an inner noted loop, e.g. the
    chapter-end `for (;;) SceSleep(1);`) precedes it in RTL. The gcse insertions of the pre-loop block go before the
    entry jump = AFTER LOOP_BEG: (a) loop.c finds a non-label scan_start and prints "Loop from A to B is phony" (nothing
    hoisted, the in-loop `lis pG@ha` stays a rematerialised copy), (b) sched1 makes the first inserted insn a loop-note
    barrier (`addi r29,r1,8` after `bl SndCall; mr r28,r3`, the `&obj->pos` copy after A's last use so local-alloc ties
    them: no `mr r28,r30`), (c) with the tail inside the `if (done)` arm LOOP_END precedes the next loop's preheader and
    anchors its hoisted `lis/li/lfs` behind the last call. This is the down loop.
  - `goto TOP; for (;;) { SceSleep(1); do { } while (0); spd += accel; TOP: body; ...; break; }`: notes present, loop.c
    prints "ignored due to multiple entry points" (no single-usage replacement of the PRE'd highs in the RsfCheck
    block), flow weights the in-loop refs x2 (accel's `spd += accel` puts accel above minSpd in the FPR order; the
    `faded == 0` CC pseudo gets 5 refs and is allocated before `done`, so pass 0 finds no used callee-saved GPR and it
    lands in cr4 with the `mfcr r12` prologue), update_equiv_regs leaves `white = 0xFF` set before the loop (depth 1 at
    the FadeSet store). This is the up loop; the `do { } while (0)` (tagged candidate) is the only way sched1 keeps
    `spd += accel` behind the SceSleep call (the note barrier; get_block_head_tail skips notes at a block HEAD, so a
    LOOP_BEG right after the block label is not one).
- **gcse cprop of a copy is per block start** (`oprs_not_set_p`): a `P = R` PRE copy is propagated only into LATER
  blocks. So in the up loop cse1 merging the RsfSet block's `high(pG)` into the RsfCheck block's pseudo (the #12
  fallthrough-arm carry) makes the RsfSet load use the reaching register R directly, while the RsfCheck block keeps
  `P = R` and cse2 re-materialises `lis` there; a single remaining use of R is moved next to it by update_equiv_regs
  (fresh `lis`, depth 0 only) -- two uses (bb 18 + the down loop's quake block through loop.c's single-usage
  replacement in the old do-while) were the 14th GPR r19. Fix (tagged `COMPILER-DIFF: candidate #12`): a bare
  `asm volatile("" : : "r"(d))` at the RsfSet arm's top flushes cse1 AND cse2 (a `do { } while (0)` only ends cse1's
  path: `cse_end_of_basic_block` stops at LOOP_END only when `after_loop == 0`).
- **cse2 keeps `P = R` (and canon-substitutes R into P's uses) only when R or its `high` is already in the table;
  otherwise the REG_EQUAL `high` (cost 0) wins and `lis` is re-materialised.** The target's `lwz r9,pPL@l(r23)` in the
  up loop's second SetPosXYZ = a dead `cPlayer* p = pPL;` at the loop top (tagged candidate #12 reverse): its high is
  cse1-merged with the SetPosXYZ load's, gcse deletes it as a copy in the earlier block and the final cprop puts R into
  the load; pPL's high then has 8 refs and takes r23 ahead of `&d->pos`/`&d->plPos`.
- **Zero-code forms found:** `spd >= maxSpd` / `obj->pos.y >= d->pos.y` / `<=` are the `cror un,eq,gt; bns` compares
  (`!(a < b)` is `blt`); `(fade->flags & 1) == 0` is the plain `andi.; bne`; the shake loops use the literal
  `fRand1_1() * 10.0f` (loop.c hoists the pool load and cse2 folds the up arm's to `fmr f31,minSpd` -- a `step = minSpd`
  copy is cprop'ed away, a literal survives because the copy is only created after gcse); separate counters `i`/`j`
  for the two shake loops keep `done` the canonical zero of the PRE'd `faded == 0` compare (`cmpwi cr4,r26` with a
  live top `li r26,0`), a shared `i` makes its `i = 0` the canonical register; a second step variable (`move`) in
  the down arm ends `step`'s life in the up loop so `spd` stays canonical in the `d->dir == 1` arm (`fneg f31,f30`,
  the target's `-spd`; one variable gives `-step`); `f32 y = obj->pos.y` for the fabs only with `obj->pos.y + step`
  re-read in SetPosXYZ is gcse's PRE of the load (`fmr f12,f13` + `fadds f13,f12,f31`), which needs `__builtin_fabsf`
  (the volatile-asm fabsf is a sched barrier that pins the copy behind the fsubs and local-alloc ties it away).
- objdiff/unit_info still show 98-99% for the byte-identical operateCrank/open_door/moveGrave/SceElevator (dtk `Sym+off`
  relocs such as `Fade+0x48`): judge with mcmp.py only.

### Stage rooms, st1_3/st2_0 pass 10 (r202 Matching 33/33 -> `st2_0/r202.cpp` flipped: initCatapult 67 -> 0; r10f Matching 15/15 -> `st1_3/r10f.cpp` flipped: GondolaGetOff 26 -> 0, GondolaGetOn 126 -> 0; 2026-09-11)

- Harness ~/.cache/rooms_b10 (rooms_a10 copies with the paths rewritten: `mcmp.py MOD/UNIT [SYM]` with `OBJ=`, `tryv.py
  MOD/UNIT SYM v/x.py`, `mdump.sh MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE`, `sbs.sh MOD/UNIT SYM [ABSOLUTE OBJ]`,
  `tailcmp.py`, `flipchk.sh MOD UNIT`, `order.py MOD/UNIT [OBJ]` fixed for module split objects; deleted at the end). Both
  flips verified with `make_rel.py --verify` (st2_0.rel / st1_3.rel OK) before the modules.py flag; 111 OK after each.
  Build hazard this pass: six agents' `ninja` runs re-generate build.ninja concurrently and `configure.py` reads
  objdiff.json / build/G4BE08/config.json while another instance writes them (`JSONDecodeError`, `premature end of file`) --
  retry `python3 configure.py && ninja ...` in a loop (29 tries once); compile the unit with `tools/ngccc.py` +
  `strip_unused.py --gcc --module` + `fold_linkonce.py --module` into the harness meanwhile and compare that object.
- **r202 initCatapult 67 -> 0, two pieces.** (1) Zero code: `int zero;` declared before the tbl loop, `zero = 0;` INSIDE the
  body right before `cat[i].state = zero` (state/timer/thrown/nArea = zero, the two ScePrim* stores stay literal 0) and
  `cat[2].timer = zero` after the loop: loop.c hoists the single-set user variable like the literal and the after-block
  stores the same pseudo (`stw r28,180`). (2) TAGGED `#3` (dead test as a gcse block boundary), the after-loop `lwz
  r3,work@l(r27)` x9 = gcse's reaching reg R used directly:
  ```
  R202Work** wp = &r202_work.p;
  if (i <= 3) { *wp = 0; }
  r202_work.p->cat[1].setNewArea(6, 7); ...
  ```
  Mechanism, all read off -dG/-dt: the after-loop block A now ends at the `if`; cse1's AROUND path (forward `bgt` over a
  label-free block, `LABEL_NUSES == 1`) rewrites the join block J's `high(r202_work)` uses to A's pseudo P (`make_regs_eqv`:
  a later pseudo leads only if it outlives the ebb); PRE turns A's `P = high` into `P = R`; cprop pass 2 propagates R into
  J's `(lo_sum P sym)` uses because the copy is AVIN at J (the in-block uses of pass 8/9 were never propagated: `cprop_insn`
  skips a reg already set in its block and `find_avail_set` is block-entry availability); the copy is dead and `wp`'s
  `(lo_sum P sym)` has only the dead store's use; cse2 folds `i <= 3` from the loop exit's `ble` (record_jump_equiv on the
  fall-through: `GTU i 3`, `comparison_dominates_p`; cse1 could not: its ebb stops at NOTE_INSN_LOOP_END and J's ebb starts
  after it), the dead block and the jump go, flow deletes the store's address chain and the re-materialised `lis P`. The
  test must be on a value cse1 does NOT know and cse2 DOES: the loop counter after the loop is the natural one (a `k = 0`
  set before the loop is unknown to both; cprop cannot fold PPC compares). Without the pointer (`if (i <= 3) r202_work.p =
  0;`) the high sits in the skipped block: 18 words; the zero piece alone 93 (the store's `li` is a `stw` slot).
  Rule (the #3 "target uses R directly" family: r10c r27, r120 r31, R213Init): the copy `P = R` has to land in a block
  that ENDS before every use of P and has no live use of P itself; a dead conditional right after the block's first high
  occurrence, on the exited loop's counter, is a zero-cost boundary that vanishes at cse2.
- **r10f GondolaGetOff 26 -> 0 and GondolaGetOn 126 -> 0, zero code: the setPos address through two inlines.**
  ```
  struct R10fGondolaTbl { void* mot[2][3]; Vec posA[2]; Vec posB[2]; };
  static inline void r10f_setPos(cModel* m, Vec* p) { m->setPos(p); }
  static inline void r10f_setPosA(cModel* m, R10fGondolaTbl* t, int side) { r10f_setPos(m, &t->posA[side]); }
  r10f_setPosA(pl, (R10fGondolaTbl*) mot, side);   // and r10f_setPosB(s, .., side) for posB
  ```
  Facts: the C++ FE rewrites `&x[y]` to `x + y` and `&s->f` to `s + off` (cp/typeck.c build_component_addr), and fold's
  split_tree associates `(t + 24) + side*12` into `t + (side*12 + 24)`, so a plain argument expands (EXPAND_NORMAL, binop)
  to `M = side*12; T = M + 24; A = t + T` = our old `addi r4,r23,24; add r4,r31,r4` and cse1 shares M across the sites.
  integrate.c expands an inline's argument with EXPAND_SUM (`both_summands`: MULT first, constant outermost ->
  `(plus (plus (mult side 12) t) 24)`) and, the formal not being `const`, `copy_to_mode_reg` -> `force_operand` computes
  the sum into the parameter copy pseudo as a CHAIN of sets of one pseudo: `mulli T; add T,T,t; addi T,T,24; mr r4,T`
  (local-alloc ties T to r4). A multi-set pseudo drops out of the mult's cse class, so `side*12` is recomputed per site
  (GetOff: three `mulli`, target) and is a gcse occurrence per site: in GetOn the posA site is in the block after the
  `sub` test, the posB and `mot[side][2]` sites are redundant -> `R = side*12` inserted (`mulli r26`), the copies' uses
  read R through cse2's canon_reg (`add r4,r26,r23`, `lwzx r4,r9,r26`) except the hard-reg chain (`mr r4,r26; add
  r4,r4,r23`: canon_reg never replaces a hard reg) -- exactly the target's asymmetric shapes. The table pointer argument
  `(R10fGondolaTbl*) mot` is `fp+8` copied into a fresh pseudo per site: cse1 merges it with the mot copy's destination
  in GetOff (bb 0 ebb), gcse PREs the two GetOn sites into a copy of it (`mr r23,r8`: the reaching reg of `(plus fp 8)`,
  cse2 `R = D`; a reference/pointer variable set once is cprop'd away, pass 9's `t` reference). `Vec* const p` gives the
  same code here.
- **r10f GondolaGetOn, the RsfCheck part (114 -> 0), zero code, four pieces:** (1) `int f` + `(u16) f` at the two
  setMoveMotion calls and `(u16) (i * 0x1C2)` in the else loop: the target masks (`clrlwi r5`) at each USE; a `u16 f`
  local is PROMOTE_MODE'd to SI and masks once at the assignment (hoisted above the `i == 0` test). (2) `p.x = 0.0f;
  p.y = +-K; p.z = 0.0f;` written in BOTH idx arms (x, y, z order): the 0.0 and K pool loads stay in the arms (`lis LC0`
  issued at t1 before `lis LCK` by LUID), jump2 cross-jumps the four stores into the join; the other five orders 23-33.
  (3) A for-scope counter per loop (`for (u32 i = 0; ..)` x3): one shared `i` aggregates the three loops' references
  (floor_log2(refs) * refs / live length) and outranks each loop's givs in global-alloc (i r29, givs r28); per-loop
  counters tie the giv on refs and lose on live length (`li i,0` issued first in the preheader) -> giv r29, i r28, loop
  2's own counter r29 like the target. (4) unchanged from pass 9: the struct-view forms above.
- Facts read this pass: gcse `cprop_insn` ignores uses of regs numbered >= max_gcse_regno (the reaching regs themselves
  are never propagated), skips a reg set earlier in its block (`oprs_not_set_p`), uses only sets in `cprop_avin` of the
  use's block, and `try_replace_reg` is a bare `validate_replace_src` (no REG_EQUAL fallback); pre_delete adds no note --
  the `REG_EQUAL (high sym)` on the copy is cse1's; cse1 (`cse_end_of_basic_block`, `!after_loop`) ends every ebb at
  NOTE_INSN_LOOP_END and a TAKEN path needs a BARRIER before the label (a loop's end label never qualifies), cse2 ignores
  the note; `record_jump_equiv (insn, 0)` runs on every fall-through of a conditional jump in the ebb; loop.c emits a
  biv's final value after the loop only when the biv is eliminated; `preserve_subexpressions_p` returns 1 under
  -fexpensive-optimizations so expand_call always copies a non-REG argument value costlier than 2 into a pseudo (the
  hard-register argument chains of the target come from integrate.c's force_operand, not from expand_call).

### Stage rooms, st2_1 pass 11 (r20e Matching 31/31 -> `st2_1/r20e.cpp` flipped, st2_1.rel byte-identical, st2_1 fully linked: checkPuzzle 224 -> 0, one tagged item; 2026-09-11)

- Harness ~/.cache/rooms_a11 (dol19a copies with the paths rewritten + module support: `mcmp.py`/`order.py`/`sbs.sh` find
  `build/G4BE08/<mod>/obj/<mod>/<unit>.o`; `tryv.py MOD/UNIT FUNC v.py` with `SRC`/`XFLAGS`/`FOLDMOD` env (`env.sh`); `dump.sh
  st2/r20e -dX` with `XCPP`/`XCC` (`-G0 -DREL_MODULE=..`); `cnt.sh x NAME..` = words + loop.c's per-pass outer-loop insn
  counts of a variant); deleted at the end. Judge: mcmp 31/32 (the nameless 0x3B8 cLight linkonce block shows "missing" in
  every room), a private `link_rel.py` + `make_rel.py --verify orig/.../st2_1.rel` -> `cmp` identical BEFORE flipping.
- **checkPuzzle 224 -> 0. Zero-code levers (all C++), read off the RTL passes (cse1/gcse/loop/combine/reload_cse):**
  - **`lwzu` copy source = a pointer PARAMETER of an inline (`r20e_framePos(Vec& pos, R20eCell* c)`, `r20e_cellPos(Vec&,
    R20eCell* to, ..)`)**: integrate expands inline arguments with EXPAND_SUM and `copy_to_mode_reg` -> `force_operand`
    computes the address sum INTO the parameter pseudo (`c = cy16 + cxp; c = c + 0x178`). cse1's `find_best_addr` cannot
    rewrite the block copy's first word `(mem c)` into the "costlier equivalent" `(plus X 0x178)` because that table entry
    mentions the re-set register (`exp_equiv_p` validate: REG_IN_TABLE != REG_TICK), so combine merges the `addi` into the
    load (`movsi_update`: `lwzu r11,0x178(r9)`), the later words stay `4(r9)/8(r9)`. Every non-parameter spelling (`Vec pos
    = c->pos`, `((R20eCell*)sum)->pos`, `*(Vec*)sum`, a pointer local) goes through fresh pseudos (memory_address ->
    force_operand(NULL)) and gets `addi r11,r9,376; lwz r10,376(r9)`. The r101 `BitOff(*(u32*)(sum), ..)` / event
    `BitOn(EvtDebug.pModel[no].flags, ..)` `lwzu`s are the same mechanism (a `u32&` parameter). The destination is a `Vec&`
    to the CALLER's block-local temp (all five copies share 8(r1); an inline-local Vec gets its own slot per copy).
  - **Statement order inside the inline decides the chain's survival**: `to`'s chain must be computed BEFORE `from`'s
    address (cse1 otherwise turns every chain step into a copy of `from`'s pseudos and rewrites `(mem to)`), and the piece
    byte must be loaded before the copy's stores (a later `p->cy` read reloads cy). `r20e_cellPos` does `pc = CELL(from)
    ->piece; pos = to->pos;` -- inline ARGUMENTS are not evaluated left to right (a MEM-reading or side-effecting argument
    was expanded before the pointer argument in every form tried), so the order has to come from statements.
  - **`mr r9,r11; lwzu r8,328(r9)` (row) vs `add r9,r9,r6; lwzu r8,0x168(r9)` (column) is reload_cse**: both `to` chains
    end in `T = a + r6; T = T + c`; when the add's operand order equals `from`'s `add` (`rtx_equal_p`), `reload_cse_regs`
    replaces the second add by a copy of the register holding the value (the `mr`), otherwise it stays. The orders come
    from expand's EXPAND_SUM association (`both_summands`: a sum with a constant is moved last, then "put a MULT first"),
    and the constants from fold's `(V+C)+A -> V+(A+C)`: `PUZZLE_CELL(p, x, y) = (x)*48 + 0x178 + (u32)p + (y)*16` folds
    to `x48 + (p + 0x178) + y16`; from = `(cy16 + (k48 + p)) + 0x184` / to `(cy16 + (k48 + p)) + 0x148` (row: equal
    orders -> `mr`), from = `(k16 + (cx48 + p))` / to `((cx48 + p) + k16) + 0x168` (column: `(plus k16 -16)` is a
    sum-with-constant, so the association puts it second -> separate add). `(k-1)*16` must reach expand as such
    (distributed to `k*16 - 16` under EXPAND_SUM); a `kk = k - 1` variable makes the to-address a second reduced giv
    (two bivs: DEST_REG givs combine only when identical, `combine_givs_p`).
  - **`addi r11,r11,0x10; stw 0xc(r11); stw 0(r11)` = the piece pointer as a chained parameter** (`r20e_setPiece(R20ePiece*
    q, const Vec&)`); a `q` local folds the 0x10 into the offsets (cse's associative `(plus (plus T 16) 12)` fold).
  - **`extsb r7,r0; stb r7` = a puzzleMove-scope `int pc` set in all four slide loops**: a global allocno (r7, after the
    locals took r0/r8..r11); a block-local `pc` is tied to its `lbz` byte by local-alloc (`combine_regs` ties operand 0
    with any dying operand) and the merged qty's refs put it first -> r0.
  - **The two cell scans are pointer BIVs** (`for (j = 0, c = PUZZLE_CELL(p, 0, cy); j < 3; j++, c += 3)`): `c->piece` is
    the address giv `c + 12` with benefit 0 ("not worth while"), so `lbz 12(c)` and `addi c,48` at the latch; a DEST_REG
    giv `c = CELL(j, cy)` (single use) is folded into the address (`lbz 0(g)`, +388 init, step after the load). Row init
    `p + (cy16 + 0x178)` needs `cy` invariant (`int cy = p->cy`) and the M4 macro's fold; column init `cx48 + (p + 0x178)`
    needs `p->cx` re-read: gcse PREs the load at the end of the test block and cse2 makes the recomputation the copy
    `mr r10,r9` (the "#3 (c)" shape; a `cx` local gives no copy).
  - **`beq` straight into the shared `li r0,0`**: the occupied-cell test is `if (piece != -1) { both scans }` around the
    scans; `if (piece == -1) return 0;` gets jump1's "hoist the single dead set above the conditional jump" (`li r0,0;
    beq`), so the tail never cross-jumps.
  - **cMes lo_sum kept in the arm (mechanism (a) of pass 10)**: `MesWork* w` assigned before BOTH SceMesSet calls. The
    arm's `w = getWork()` (`+4`) then has REGNO_FIRST_UID outside the loop -> `reg_in_basic_block_p` false + maybe_never ->
    not a movable, so it neither forces the lo_sum (savings 1, life 3: 62*3 < 691) nor moves itself. A single `w`, a
    `MessageControl* m`, or split loads still force (life 10-13, savings 2 -> moved). `force_movables` runs BEFORE
    `combine_movables`, and `m->savings`/`lifetime` are the SUMS over forced/matched movables ("savings 3" = 1 + 1 + 1).
  - **`fdivs f1,f31,f30` (one f31, frames f30) with pool order 10.0, 1.0, 4.0**: `int cnt = 4; f32 frames = cnt; f32 one
    = 1.0f;`. cse1 folds the int->float conversion to 4.0 as a REG_EQUAL note, loop.c's move_insn re-emits it as a pool
    load when hoisting (the 4.0 entry is created THEN, after 1.0's), while the loop-body order (frames first) is the
    movable/preheader order; the first-loaded constant has the longer life -> allocated second -> f30. Declaration-order
    swaps flip the pool instead.
- **One tagged item (`COMPILER-DIFF: candidate (loop.c pass-2 insn_count)`, two dead sets of a `KeyWork* key` in
  checkPuzzle's loop)**: the Key.rep block's `high(Key)` (savings 3 = itself + the cancel test's high + that high's
  lo_sum, life 3) is hoisted in loop pass 2 (71*9 = 639 >= 624 real insns); the target keeps it in the body. 20 in-loop
  nops (>= 640 insns) reproduce the target, so the original's pass-2 count was >= 640 (or it had the cancel test's
  lo_sum in a multi-set/other-block pseudo); no zero-code form gave either (kk/`return 0`/spellings measured with cnt.sh:
  622-625). The lever: `KeyWork* key = 0;` at the loop top and `key = (KeyWork*)(p + result);` after puzzleMove, `key =
  &Key; if (key->trg & ..)` for the cancel test -> `key` has three sets in three blocks (`may_not_optimize`), so the
  cancel test's lo_sum is not a movable, its high is not forced, and the Key.rep high drops to savings 2 x life 2 (284 <
  624). The second dead set's two insns also move gcse's expression table from 331 to 333 buckets (`max_cuid / 2`) --
  at 331 the reaching regs of `high(pG)`/`high(RoomData)` are numbered in the other order (bucket 275 > 34) and the arm's
  r25/r24 swap. Bucket = `(7933 + h(name)) % size` as in pass 10; pG < RoomData holds for sizes 329, 330, 332-340, not
  331. Both dead sets are deleted by flow (no code).
- Facts read this pass: `expand_expr` never uses subtargets at -O2 (`preserve_subexpressions_p` = flag_expensive_
  optimizations), so chained same-pseudo sums come only from `force_operand` (inline args, `copy_addr_to_reg` of an unforced
  sum); rs6000 `expand_block_move` copies both addresses with `copy_addr_to_reg` (the `S = c` copies in the .rtl dump);
  `combine_movables` refuses DEST_REG g1 with a single use and combines DEST_REG g2 only when identical (`tem ==
  g1->dest_reg`), DEST_ADDR g2 when `g1 + c` is a valid address; loop.c's `move_movables` threshold is `(1 + n_non_fixed_
  regs) * (has_call ? 1 : 2)` = 71 with calls, -3 per moved insn, and pass 2 (rerun-loop-opt) restarts at 71 with the
  post-cse2 count; `reg_in_basic_block_p` returns 0 as soon as REGNO_FIRST_UID is not the insn (a variable set before the
  loop disables the in-loop set as a movable); `do { } while (0)` loops are "phony" to loop.c (no movables); the `cmpwi
  cancel,0; mfcr r24 .. mtcrf; beq` shape is the CC pseudo of the duplicated exit test living across the loop.
