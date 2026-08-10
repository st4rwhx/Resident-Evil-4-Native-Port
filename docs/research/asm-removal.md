# Removing the asm-emitted instructions (2026-09-17)

After the public release, a reviewer pointed out that some `// COMPILER-DIFF` sites did not compile to
zero bytes: they were `asm` statements whose template contained a real instruction (`li`, `lis/addi`,
`mr`, `lfs`, `la`, `fdivs`, ...) placed by hand so that GCC's register allocator would see the same
value shapes as the vendor's build. This pass replaced them with C. The judge is
`tools/asmcheck.py` (GCC units: compiles the unit with every asm template marked and lists the
instructions that came from a template; `--all` for the census) and, for the MWCC units, a count of
`asm { }` blocks that contain a mnemonic other than a codeless pin.

Starting census: GCC game code 98 hand-placed instructions at 90 sites in 45 units; MWCC CRI
libraries 103 emitting `asm { }` blocks (register pins of the form `asm { mr r31, obj; mr sfd, r31 }`,
`asm { lwz r3, F(e); mr st, r3 }`, `asm { li r7, -1; mr m1, r7 }`, ...). Not counted, and kept:
hardware kernels that the vendor also had to write in asm (paired-single `psq_*`/`ps_*`, `frsqrte`,
GQR `mtspr`, cache ops, the `sndvd` exception handler, `mfspr`) — 18 sites / 231 instructions in the
GCC units, ~59 blocks in the CRI libraries. `LIMIT_ANGLE` (math_sub) was checked separately: the C
spelling reproduces the compare polarity but jump2 turns the first arm's `b end` into `blr` (jump.c
inserts a RETURN before the epilogue of a leaf routine once reload has run), so the target's `b` to a
lone `blr` was asm in the original too.

Method: one agent per unit group, `tools/research/kit/variant.sh` as the loop, RTL dumps and the
hooked compiler (`GDBG=1`/`LADBG=1`) or `ra.py`/`chaitin.py` to read the mechanism, then a second
and third pass on every site the first agent left open, seeded with its mechanism explanation.
Every unit stayed `bytecmp IDENTICAL`, `dtk shasum` 111 OK throughout.

## GCC 2.95: recipes by mechanism

Numbers in brackets are (unit, function). Each line: the asm removed -> the C that gives the same
bytes, and the pass that decides.

### local-alloc `update_equiv_regs` (the `#13` family: a single-set constant gets REG_EQUIV)

- A constant kept in a callee-saved register across a loop: make the pseudo two-set. `list = no*sz + 0x30;`
  then `list = (u32) EspEvModList;` in the arm (espgen10 EspgenDataSet): `no_equiv`, global gives it r11,
  the compiler's own `high` temp has no r11 suggestion and takes r9. Same for `u32 la = (u32) EspEvModList;
  p = *(cModel**)(la + (no << 2))` (db_port DB_VecMulEmPartsMat) — the unflagged sum makes both address
  operands BASE_REGS, the index is local-alloc'd r9, `p` takes r9 in pass 0.
- A constant that must be materialised at its use, not hoisted: set it inside the loop body and use it
  after the loop (`sz = 8 * sizeof(u32);` in the `for`, item cItemMgr::init): the set does not dominate the
  call block, cprop cannot fold it, sched1 ranks the arg copy at weight 0, update_equiv_regs substitutes.
- `update_equiv_regs` moves a REG_EQUIV init to its single use only when `REG_N_REFS == 2`: a third use in
  another block keeps `lis` in block 0 (merchant buyupPrice: three 0.5 arms share one high via cse1
  path-following; t_scroll edit_litmask: `u32 one = 1;` before the `if`).
- A `li` that must sit AFTER a load in the same block: the init moved by update_equiv_regs lands just
  before its use (t_scroll edit_litmask `one << id`).
- REG_EQUIV doubling a live length: kill it with a fold that only combine can do — `j = ((u32) e >> 16) &
  0xFFFF0000;` is 0 to combine (nonzero bits) but not to cse, so no REG_EQUAL note (sce_com SceEventEnd,
  title titleWait, t_event `t->stopWait = ((u32) t & 0x8) >> 4`, t_id `t2 = ((u32) w & 8) >> 4; row = t2 + 12`).
- `validate_replace_rtx` quirk: `int y0 = 0x54; eprintf(.., y0 + 0x54, ..)` — the constant-last swap is a
  no-op because both constants are equal (`rtx_equal_p`), the PLUS is never folded, the init is moved
  before the use -> `li r4,84; addi r4,r4,84` (snd_test).

### cse (path following, `find_best_addr`, canon_reg)

- A literal 0 canonicalised onto another zero in the ebb: `do { } while (0)` before it (LOOP_END ends the
  cse path) — r118, t_mv; or a `volatile`-free launder `int z = 0; asm("" : "+r"(z) : "m"(*num)); *num = z;`
  when the site is also a cross-jump candidate (em_sub).
- cse never canonicalises a HARD register: `register int res asm("r0") = ..; sel = res;` keeps `mr r29,r0;
  cmpwi r29,1` with `cmpwi r0,2` in the other arm (sce_at x2, model, motion). With a codeless later use of
  the r0 value (`asm("" : "=m"(vx) : "r"(t))`, or a read in the error arm) combine cannot fold the copy
  into its `addi` (the combined pattern becomes an unrecognised PARALLEL) and regmove skips the hard-reg
  source.
- `find_best_addr` prefers the `lo_sum` form of `(mem (reg tbl))` at equal ADDRESS_COST: `{ u32 z = 0;
  if (((PieceInfo*)(z + (u32) tbl))->id == 0xFFFF) return; }` keeps the register form (cse does not fold
  `(plus z tbl)`, combine folds `z = 0` after cse2); a dead `tbl = 0` kills gcse copy-propagation of
  `base = tbl` (ss_pzzl pieceTblInit).
- A shared high across three arms: write the arm per test group so every arm is a cse path continuation
  (fall-through arms, plus jump1's `if (foo) bar; else break` swap for the last), then jump2 cross-jumps
  them back into one (merchant buyupPrice).
- The AROUND path carrying an address pseudo into the join: an explicit `else pm = m;` arm ends the
  extended block, `&p0` stays a hard-reg argument set (at_mod ObaLineHitChk).
- `-fno-gcse`-shaped site: a dead `{ const f32* pk = &r222_k212; }` before the `if` puts `high(sym)` in
  the pre-branch block; cse1 (else path) and gcse (then arm) rewrite both loads to it (r222 BoxMove).

### gcse PRE / cprop

- One high for several pool loads: put every occurrence of the constant INSIDE the loop (`f32 z0 = 0.0f;`
  after the loop top): PRE inserts a single `high` at the preheader end and every load is a copy of it
  (r20d).
- A constant needed in two arms without a shared pseudo: `int t80 = x * 8; asm("" : "+r"(t80));
  asm("" : "+r"(x));` — the launders are the last sets in the ebb, cprop has no available set to propagate
  into the join (db_light).
- `(u32) em - em2` with `em2 = (u32) em` at the top: cprop makes it `em - em`, PRE hoists it, cse2 folds
  to 0 with REG_EQUIV -> lowest priority -> the target's r25 (db_mod).
- The string-high numbering (expression-hash bucket order `(7933 + h) % T`): shift the `.LC` numbering with
  dead `f32 lcN` labels and the table size with a codeless `+m` anchor; the hash model of the lever
  table row predicts the order (t_bugcheck menuLife: 13 labels, T = 183).
- A `(plus fp N)` argument address PRE'd into a copy: pass `&local` through a `static inline` wrapper
  (integrate substitutes into hard-register argument sets, which PRE never touches) — cam_ctrl
  r0_RailBehind, three `la` at once.

### haifa scheduler (sched1 / sched2)

- A `li` that must issue after a store in the same block: an anti-dependence via the value the store
  reads — reuse the stored variable (`pG->flags = v; v = 0; ActBtn.set(.., v)`, r224 R224Main), or a
  codeless `asm volatile("")` after the store when the constants are hard-register argument sets that no
  C dependence can reach (r224 gnd_close, pad `dead = 10`).
- A sched1 issue slot that must be filled so a later `lis` slides one insn (local-alloc fake-lifetime
  rule: a qty born <= 1 insn after another's death cannot take its register): `asm("" : "=m"(spd))`
  between a load and its store (r20e cFence20e::move: both pool highs then get r9).
- A hard-reg load that must follow a copy: `register pzlPlayer* pl asm("r3"); pl = wk->x2B0;` (the load
  gets an anti-dependence on the r3 copy) — ss_pzzl pzzlCursorDisp.
- A `mr` copy that must follow the argument moves: `pv = pa; asm("" : "+r"(pv) : "r"(pm), "r"(pw));`
  (pendulum).
- sched2 dependents tie-break: a codeless `asm("" : "+r"(w) : "r"(one))` after the call gives the `li`
  its sixth dependent without taking an issue slot in the contested cycles (em32 R0_Init).
- Two divides in one block: `do { du = a / b; dv = c / d; } while (0);` — loop notes are barriers and end
  the cse ebb (esp08).

### local-alloc block_alloc (qty order `refs*log2(refs)/life`)

- Raise a qty's refs without code: an asm that mentions the pseudo TWICE as inputs counts 2 refs
  (`asm("" : "=m"(esp) : "r"(e1), "r"(e1))`), and its placement stretches the competing high's life
  (esp_app EspDrawLaserLine: reload 1 6666 > high 5000, r9/r11 walk restored).
- Remove a qty from the competition: pin the word to the register the target uses (`register u32 c
  asm("r11")` for the .z word of a struct copy written as three `u32` loads/stores — `u32*` loads are not
  MEM_IN_STRUCT_P, so the sda21 store gates the first load in both schedulers) — Espgen43 AddSandPower.
- A `fabs` whose result must not be tied to its dying input: set the variable in two blocks (`ay =
  fabsf(lp.y)` in both arms) so it is global-allocated (em18).
- The lo_sum output tied to a dying high: keep the high alive past a call — `low_RotMatrix(m, &crot0);
  rot = &crot0;` assigned AFTER the call, so P's last use is the hard-reg argument (reload_cse rewrites
  it to `mr r4,r25`) and P gets r30 ahead of the other high because the pinned `mdl` r30 is live in
  block 0 (r204 EventChandelier).

### combine / regmove / jump

- Narrow extension the target keeps: read the incoming register directly, `register int rb asm("r9");
  int mode = (u8) rb;` (no LOG_LINK, `zero_extendqisi2` survives; pl_class) — or pin the variable
  (`register int eff0 asm("r16")`: `set_nonzero_bits_and_sign_copies` requires a pseudo; r221).
- An unmasked `mr` copy of an `int` derived from a `u8`: `register int t asm("r0"); t = i + 6;
  asm("" : "+r"(t)); line = t;` (db_light).
- `fadds len,fx,len` operand order: `f32 l2 = len; len = fx + l2;` (expand_binop swaps when op1 is the
  target rtx; cse then deletes the copy) — pl_debug DrawGage.
- `mr` kept for a `(plus m A)` value: `s32 nofs = -(s32) ofs; base = (u8*) m - nofs;` — a different
  expression for cse, simplified by combine, rewritten by reload_cse to the target's `mr` (t_event).
- jump2's `x = b; if (c) x = a` hoist: it needs a one-insn else arm at jump2 time — `int t2 = ((u32) w &
  8) >> 4; row = t2 + 12;` blocks the earlier passes, combine folds it, jump2 fires (t_id); blocking it
  instead: a dead `types = 0;` before each arm's constant with the variable pinned (ss_item).
- Cross-jump / arm layout: `asm("" : "=&r"(z) : "0"(&zero), "r"(&zero), "m"(zero))` — early clobber stops
  the local-alloc tie, the duplicated input stops regmove, reload emits the `mr` for the "0" match
  (ss_map).
- `edit_select_sub` (t_scroll): see the open list.

### `.rodata` pool words

A named `static const f32 k` used as an asm operand had stood in for the compiler's pool constant. In
every closed site the literal (`0.5f`, `2200.0f`, `0.0f`, `0.8f`) is back and the pool word lands at the
same position (`.rodata OK`). Exception: r224/r222 keep a top-level `asm(".section .rodata ...")`
word plus an `extern const f32 x asm("name")` alias, because the REL needs a distinct SYMBOL_REF for
cse to keep two highs apart and a public C definition leaves the REL's `@l` fields 0. Those directives
emit data, not instructions.

## MWCC 2.4.7 (CRI libraries): recipes

The compiler colours by virtual-register id in Chaitin scans (docs/matching.md, MWCC table). The
emitting pins were replaced by:

- A kept parameter copy: `void *obj` parameter + typed copy `MWPLY mp = obj;` (mfci, mpv_frm, mwsfdfrm
  mwPlyGetCurFrm, adx_sjd, mwsfdcre) — the copy survives because it feeds an argument move or because
  a codeless pin references it.
- Helper split: move the body into `static Sint32 sub(SFD sfd)` so the value is an inlined-helper web
  (ranked above own locals and nested inline temps): sfd_adxt Create, sfd_see ExecEstimate, sfd_pts
  ReadPtsQue (search loop merged in, locals declared in colour order).
- Declaration order and `id << 2` vs `id * 4` (arithmetic-defined local vs hoisted temp): sfd_set.
- `#pragma opt_lifetimes off` around a function so `o = X + o` stays one web (adx_bsc), or `p = buf + i;
  p = (Char8 *)((Uint32 *)p + 1);` under it (mpv_hdec).
- The word-pointer step form for a temp created after a load: `q = (Uint8 *)(ptr - 2) + ((bitpos + 7) >>
  3)` (mpv_dec).
- Frontend CSE: `data + ofs` as one @temp shared by byte 0 and the `+4` read, the memcpy argument
  coalesced (adx_dcd).
- `fb + (sl - 1)` vs `(sl + fb - 1)` picks `add; subi` vs `subi; add` (gcci).
- Byte swap: `SWAP32(*(Uint32 *)(hdr + ofs))` with the M4 dead-conditional block boundary (sfh_main).
- `#pragma fp_contract on` for a fused `fnmsubs` in a unit built without `-fp_contract`; `(f32)(f64)x` for
  `frsp` under `#pragma scheduling off` (vec, mtx).
- **Codeless K-pin** `asm { mr rV, x; mr x, rV }` (`x` a `register` local or parameter, rV a volatile the
  function never uses): both `mr` are deleted; the mechanism is NOT ghost neighbours — rV leaves the colour
  set, K 29 -> 28 per distinct rV (`chaitin.py --k 28` reproduces every such site exactly). A node with
  exactly 28 neighbours at its scan then survives one level. sfd_hds (typed `res` copy kept alive by it),
  sfd_adxt ExcludeHdr/ExecServerSub, sfd_mpv Destroy, mwsfdfrm 296, mwsfdcre (four registers = K 25).
  Proof of codelessness: swap rV, bytes identical.
- **Dead-conditional arm as neighbour source**: `z = 0; if (z != 0) len = len + a->f1 + a->f2 + a->f3;`
  — the arm's loads are live with `len` at RA time and coloured before it (+1 neighbour per load: r3, r4,
  r5), the arm is deleted post-RA (`li; cmpi; bt` and the loads). Arms with a call or a store are not
  deleted (adx_sjd 449/538).
- **Self copy as opaque second def** `asm { mr v, v }`: stops constant substitution, range splitting and
  add-chain folding of `v`; deleted before scheduling (rna_res `half`, sfx_alp `k`, sfd_pts found-arm).
  `tbl = (T *)((Uint32)tbl + 0)` is the pure-C form when the value is unknown (sfd_lib).
- sfx_alp SFXA_Create: the five constants derived from one two-def variable `k` (`k = 100; .. k += 0xFF -
  100;` with self copies) so `li 100`/`li 0xFF` share a vreg and the second gets a WAR edge on the store —
  the one scheduler slot no other C shape moved.

- sfd_mpv Create: `void *obj` + own-local `SFD sfd` declared before `mpv`, a dead arm before the return
  reading `mpv->picstat + sfd->prm.x38` (lifts both to L2, sfd outranks mpv by vid, the pool base stays
  L1), and `z = 0; asm { mr z, z }` in the entry block as a dead `li` that fills the cycle-1 scheduler
  slot (class 4, input-order tie) so the pool `addi` issues before `li r4,5`. GoDdelim: `asm { mr t, t }`
  keeps `n = t` a user copy.
- mwsfdfrm AnalySofdecHeader: `sfh` declared last + a dead arm `sfh = (SFH)((Uint32)sfh + p->sfh_cnt)`
  right before `ccs = 0` (+2 neighbours on sfh, +0 on ccs: sfh L2 -> r31). mwsfdsfx CnvFrmInfToSfx: the
  three `mwply->x94/x98/x9c` copies and plane 1's `cb`/`cbw` as own locals with a `(T)((Uint32)v + 0)`
  second def — each takes one backend temp out of the pool base's scan-1 degree (31 -> 28 = L1).
- adx_dcd5 Ste4AsSte: `x = (Sint32)(Uint32)t;` — the frontend substitutes a plain copy but not one
  whose source carries a type conversion; `t` has three reaching defs so the backend keeps the `mr`.

### Codeless pins that remain in the CRI units (emit nothing)

- `asm { mr rV, x; mr x, rV }` K-pins: sfd_hds, sfd_adxt x2, sfd_mpv Destroy, mwsfdfrm AnalyTotalFrmNum,
  mwsfdcre (4 registers), cri_cvfs.
- `asm { mr v, v }` self copies: sfd_pts x2 (found arm), rna_res, sfx_alp x2, sfd_mpv x2, mwsfdcre x2.
- `asm { mr r6, v }` one-way writes in adx_dcd5 Ste4AsSte/Ste4AsMono (5 blocks, 10 writes): the target
  functions contain no `mr r6,` instruction at all — the writes are deleted by the allocator; their
  effect is the K rule with a never-removed ghost aliased to r6 (`chaitin.py --k 29 ghost=6`: 0
  divergences on both functions). A pure-C carrier exists for the ghost (a post-loop dead arm `histl =
  histr;` keeps the parameter copy coalesced) and for the c1/c2 copies (`if (z != 0) c1 = c2;`), but the
  arm's `li; cmpwi` must sit in the merged ENTRY block (before the hoisted `extsh`) and the post-RA
  peephole never sweeps dead defs out of entry/exit blocks: +4/+8 bytes. Kept.

## Open sites (kept as asm, with the mechanism)

GCC game code: none except the sites below in `t_light/t_scroll.cpp` if the last pass leaves them
(see the tail of this file).

MWCC: `lib/dct_ac.c` `dctac_Init` — `asm { lis hi, dctac_i_const@ha; addi bss, hi, ..@l }`, `asm { addi
ip, bss, __ArenaHi@l }`, `asm { addi fp, bss, 0x200 }` (4 instructions). With `#pragma pool_data on`
plain C reproduces the target's `.bss` pool exactly (`addi r31; stw 0x400(r31); addi r28,r31,0` with the
relocation, `addi r27,r31,0x200`), but our MWCC then also pools the function's two 8-byte `.rodata`
constants (a `.rodata` pool forms at >= 3 poolable objects of <= 8 bytes, literal or static, wherever
defined), while the vendor's build addressed them `lis; lfd sym@l`. Escaping the pool needs a 16-byte
table, and MWCC never folds a non-zero offset into an `@l` relocation (`addi r6,r6,tbl@l; lfd 8(r6)`,
+4 bytes). This is the "MWCC compiler-build differences" item M2 of docs/matching.md; the asm stays.
