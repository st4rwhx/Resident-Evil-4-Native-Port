### Small tool RELs (t_light/t_light, t_id/tools+db_path, t_movie/t_movie+t_prim, t_event+t_sce/db_filelist matching; db_sctrl 15/22)

- The "linkonce orphan" tail of t_id/t_esp/Tools (ToolArrayPush/ToolWorkPop[/ToolEmArraySet], one
  cManager<cLight> block, cManager<T>::arrayPush/arrayPop x6) is tools.cpp itself built with
  `-DTOOLS_ARRAY` (modules.py CFLAGS; t_esp also `-DTOOLS_EM_ARRAY`): plain functions after
  `_unresolved`, then the deferred template bodies in instantiation order. `include/tools.h` declares
  them; `cManager<T>::arrayPush(int)/arrayPop()` live in cManager.h; the Esp/Espgen pool swaps are
  `extern "C" int` in esp.h/espgen.h, `ConsGetRoomValue` in cons.h. The bits of the flag word
  *exclude* a pool (`if (!(flags & 1))` — bit 0 is the `xori/andi.` form).
- `if (busy) return 0; body; return 1;` keeps `li r3,0` out of line after the body (`b end; li r3,0`);
  `if (free) { body; return 1; } return 0;` gets the `li r3,0` hoisted above the branch (4 bytes short).
- `t_id/linkonce.cpp`-style synthetic units are never right: a tail of named linkonce copies after the
  last object is the last object's own deferred output. Likewise a "unit" whose ctor key is not its
  first global function is two objects (t_event's db_filelist = Tools' db_toolbase.o + db_filelist.o).
- Unreferenced .bss of a unit whose own code never touches it (the global `cFileList DbgFileList`,
  driven by t_event.cpp) is attributed to the previous unit by the generator: pin it (`{".bss": 0xAC}`)
  and name the label by hand in symbols.txt/sym_map.tsv (data labels are not synced).
- A stray `DF magic + 1.0f` pool at the START of the next unit's .rodata (t_id/t_event db_sctrl at
  0xF0/0x1640) is that unit's dead-stripped leading function (never-called `static f32 f(int i)
  { return (f32) i + 1.0f; }` + STRIP_UNUSED), not a tail of the previous unit.
- `cString("...")` temporaries share a freed aggregate slot only because the class has a user copy
  constructor (BLKmode): cString.h declares `cString(const cString&)` (never defined; the DOL has none);
  with an SImode class the temp gets its own slot (t_movie movie_test_main frame 0xC0 vs 0xC8).
- `int col = (c) ? 6 : 0; f(..., (u8) col, ...)` gives the target's `clrlwi r7,r7,24` at the call; a
  `u8 col` local (either form) folds it away. A u8 colour passed as int in several arms with one
  PRE'd `clrlwi rX, rCol, 24` (db_sctrl sctrlMenu) is an `int col` set in two places (a dead `int col
  = 0;` before the block plus `col = 0;` in the loop: REG_N_SETS 2 stops gcse cprop) passed as `(u8) col`.
- Passing a struct by value to a varargs `%s` copies it to a temp and passes the address: movie_test_main's
  0x24-byte `MovieFile f = movie_file[i]` copy is the plain local form (block move loop of 0x18 + 0xC).
- `while (*p) { int c = *p; if (islower(c)) c -= 0x20; *p = c; p++; }` with `int c` keeps `subi` in
  place (`char c` re-extends with `extsb`); `islower` is newlib's `(_ctype_ + 1)[(int)(c)] & _L`.
- `pGS->debug_mode = ...` / `(*(u32*)((u8*)pGS + 0x68) & bit)` (the struct-view pG) keep the `lwz pG`
  below a preceding store through the work pointer where plain `pG`/`TOOL_FLAG` let ours hoist it.
- Clamps on s8 members: `w->cursor = w->cursor < 0 ? 0 : (w->cursor > 1 ? 1 : w->cursor);` gives the
  target's `extsb r9,r0 ... li r0,0/1 ... stb r0` with the raw byte kept for the store; `s8 c = ...` locals
  or if/else chains extend in place.
- `FuncPathWork`-sized locals accessed through a pointer (`PathParam buf; FuncPathWork* param =
  (FuncPathWork*) &buf;`) keep `&buf` in a callee-saved register (`lwz r0,4(r29)`); direct member reads
  address the frame (db_path pathGrabLine/pathDraw).
- `if (i > 0)` inside a loop body full of calls is the `cmpwi; mfcr rI; ... mtcrf 128, rI` CR spill
  when `i`'s register is dead after `i+1` was computed (pathDraw, ours reproduces it).
- A struct copy plus a byte store: `w->insertIdx = t + 1; w->insertPos = pt;` (byte store first in
  source) gives `stw x; stb; stw y; stw z` (the stb waits for its `psq_st/lbz` latency chain).
- Two consecutive stores of one value into two Vec locals: `g1.x = g0.x = v;` stores g1 first (chain
  order), `g0.x = g1.x = v` the other way.
- `int sx = (int)(v * 256.0f / 320.0f + 256.0f)` / `(int)(224.0f - y * 224.0f / 240.0f)` are the
  screen-position conversions of the tool cursors (drawCursor); drawAxis' label y is `224.0f - w1.y`.
- Rooms of unresolved diffs in db_sctrl: DbSctrl's `w->x/w->y/w->blink++` store order (target y, x, blink;
  no statement order gives it), SctrlAdjustAxisRange's two stepping pointers, drawAxis' early `lis`
  of the format string, drawScurve's `i`/`&wp` register swap (r26/r27), sctrlMenu's cross-jump of the
  case-1 cursor call.

### Small tool RELs, third pass (t_camera_draw Matching; t_camera_data 12/16, t_movie/t_se_at 11/19; 2026-09)

- t_camera (include/t_camera.h): `TcWork* pTc` is a plain global pointer (`.data` 0x55C -> the static
  `tcWork`, 0x644); `cam` (a `Camera`) sits at +0x10, the pad snapshot `JOY joy` at +0x10C. The tool
  reads the pad words through raw offsets (`TC_TRG = *(u32*)((u8*)pTc + 0x120)`): only that reloads
  `pTc->joy.trg` after a store to the blink counter (a non-struct load aliases the scalar store) while
  the pointer itself is kept, and it keeps the `lwz trg` below `stw blink` at the function end.
  Data pools are globals `tcTypeTbl[64][16]`, `tcAdat[0x60]`, `tcCdat[0x40]`, `tcLdat[0x40]` (records
  = cam_ctrl.h's file records with the arrays inline: TcCdat has `u16 frame[26]` at 0x10, a
  `union { f32 floor; Vec dir; }` at 0x44, `pos/at[26]`, `roll/fovy[26]`), plus `Camera tcGameCamera`
  in t_camera_data. Data labels were named by hand (`sync_rel_symbols` mis-assigns them when the
  relocation order differs) — check `symbols.txt`/`sym_map.tsv` after every sync.
- A single `int num = ngon->num;` local (instead of re-reading the field in the loop test) is what
  keeps `ngon` out of a callee-saved register and orders the three `addi rX, r3, 4/8/0xc` bases
  (tcDrawNgon); a struct local `p.x = p.y = p.z = 0.0f; for (j) p.x += b[j]*px[j] ...` IS turned into
  register accumulators by our compiler (the zero stores stay, `fmr f9,f31` copies in the preheader).
- A `CameraBSpline* bs = &CamBSpline;` local (like cam_ctrl.cpp) makes `bs->k` a `0(r30)` load; the
  bare global gives `lis/lwz CamBSpline@l` for the offset-0 member. Declare such externs in the tool
  header, not cam_ctrl.h (a header extern before cam_ctrl.cpp's own would reorder its .bss).
- Two passes of loop.c (`-frerun-loop-opt`): pass 1 eliminates a `for (i...) { p = &tbl[i]; }` biv
  through a giv whose add_val is a *pointer-flagged* register (`maybe_eliminate_biv_1`,
  `REGNO_POINTER_FLAG`), and pass 2 then fails BCT ("Initial value not constant") -> pointer compare
  instead of `mtctr`. The flag came from re-using one function-scope `TcCdat* c` (also assigned in a
  later loop); a block-local `TcCdat* cd = &tcCdat[i]` per loop keeps `bdnz`. Dump with
  `cc1plus ... -dL` (the `.loop` file lists "insert_bct" / "biv eliminated" per loop).
- gcse PRE splits a loop counter into `addi rT, rI, 1` in the arms + `mr rI, rT` at the end when the
  increment sits at the latch of a body with an if-skip; the original has the plain `addi rI,rI,1`
  early in the body (tcDataExport rec loop: write `i++;` as the first statement after the header
  stores). The frames loop keeps `mulli r9, r6, 0x394` (no giv reduction) only with the for-header
  `i++`; `&tcCdat[i++]` reduces the giv.
- `(u8*)p - buf` written as an expression inside the loop (not an `ofs` variable) is the giv the
  original strength-reduces (`subf r10, r29, r3` in the preheader, `addi r10, 2` per iteration);
  a separate `ofs += 2` variable gives the same code but PRE hoists the `subf` above the previous
  loop. Per-section typed pointers (`Vec* vp` adat, `Vec* pos` cdat, `u16* fp` frames, `size =
  (u8*)fp - buf` before the rec loop) reproduce the `mr r5,r8 / mr r3,r5` section copies; the cdat
  loop's `pos[j] = cd->pos[j]` (index form) + `*at++/*roll++/*fovy++` + `pos = (Vec*) fovy` gives
  the `mr r6, r5` giv copy for pos only (pos is live on the skip path, the others are block-local).
- Compare constants: the tree folder turns `ver < 2` into `ver <= 1` (`cmpwi 1; bgt`) and
  `ver >= 2` into `> 1`; the original's `cmpwi 2; bge` shared with a later `ver <= 2` (cr0 saved in
  `mfcr r25`) needs the constant to reach RTL unfolded — `int lo = 2; if (ver < lo)` (cse then
  propagates it and gcse merges the two compares). `ver == -1 || ver < -1` are two separate ifs
  (`cmpwi -1; beq; blt`).
- `ver <= 2` / `ver > 3` version arms: `if (ver <= 2) {A} else {B}` gives `bgt` on the saved cr0;
  `ver <= 3` a hoisted `cmpwi cr4, r3, 3`.
- File-record pointer walks the original hoists into locals: `Vec* pt = s->points; for (j) a->pt[j] =
  *pt++;` (a plain `s->points[j]` reloads the pointer after every store because the stores may alias).
- t_se_at (src/t_movie/t_se_at.cpp, SE attack editor; work `SeAtWork` 0x16D4 = Debug_alloc'd, `TSeAt`
  = snd.h's SeAt with `blk`/`se_no` as ints): the work pointer and the current-area pointer are
  one-member structs (`seAtWk.p`, `seAtCur.p`); `static int seAtSaveNum` (its low half is read with
  `lhz seAtSaveNum+2` for the u16 header count); `SetToolLight(int)` is this module's db_light_v2
  copy (scope global in symbols.txt although the .sym says local). Table definitions sit right before
  the first function using them (menu strings in .rodata parse order: block names, ToolSeAt,
  seAtInit, main menu, seAtAreaEdit, create+edit menus, AreaMove pool, input/rnd/flag names,
  DataInput, DataLoad, save menu, DataSave). Idioms: `BitSet(w->save, TOOL_FLAG(..))` for the two
  flag backups (pG reloaded after the store); `int zero = 0` at the top of seAtInit (`li r28,0`
  before the first call, reused for every zero store); `u8 valid = w->copyValid` loaded once for the
  four menu-enable stores (order edit[4], edit[3], create[2], create[1] -> issued create[1] first);
  `Vec axis = {0,1,0}` declared inside the `if` (16-byte template copy); the `+-1/+-10 with X` steps
  are a macro, the clamps `if (v >= 0) { n = v; if (n > M) n = M; } else n = 0;` with a second
  variable (`mr r10, r11`), the s8 cursor clamps in DataLoad the ternary form (raw byte kept); the pad
  masks are `RIGHT|0x20000` / `LEFT|0x10000` (joy.h's SLEFT/SRIGHT swapped); `s16 x = w->x + 0x60`
  (`lhz/addi/extsh`); menu-name loops `for (j = 0; j < num; j++)` with `int num = 6` (a literal
  bound folds to `ble tbl+0x14`, the original has `blt tbl+0x18`), a `u8 col` for the loop colour
  and `(u8) col2` casts on an `int col2` in the footer (per-call `clrlwi`); colour ternaries inline
  in the eprintf argument (`yesNo == 0 ? 6 : 7` -> `li 7; bne; li 6`; nested
  `sub == 1 ? (cursor == 0 ? 6 : 0) : 0` for two separate `li r5, 0`); `pGS->stage_no` in the save
  path (keeps `lwz pG` below `sth w->y`). Open: ToolSeAt `lis` placement, seAtInit's Snd save/zero
  store order and the work-pointer load position, EditMenu's r7/r8 menu pointers (mine `mr r7, r8`
  on the edit call), AreaMove's `&right` address pseudo (y/z stored via `addi r9, r1, 8`),
  DataInput `clrlwi r5, r29, 24` per iteration in the first name loop, DataLoad's `(u8)` cast of the
  first footer colour (folded into the arms by our front end), DataSave's `seAtSaveNum` reload after
  the record copy.
- A `u8` counter the target does not constant-fold (`mr r3,rN` at uses, `clrlwi` only at a u8-param
  call) is an `int` incremented through call arguments (`f(nGen++, ...)` twice).
- `lwz r0,g; mr r7,r0; cmplw r0` around an early-return test = gcse PRE's copy: keep a single early
  return and read the global again inside the later block.
- `lbz rV,0(p) ... or r0,rV,r0; extsh rV,r0` = `s16 v = p[0]; v |= p[1] << 8;`.
- `addi rX,r1,ofs` for a block-local object hoisted above an earlier call = `T* p = &obj;` declared
  before the `if`.
- 8-byte zero-initialised `static char name[8]` sits in `.sdata`; 10 bytes would move it to `.data`.
- gcse PRE hash: hash = 119+6+(61<<7)+h(name), h = h*129+c, table = n_insns/2|1 buckets; n_insns at
  gcse time excludes cse-deleted dead initialisers; `asm volatile("")` before a loop disables its
  invariant motion (exception `ErrorHandler`, t_bugcheck).

- Tools REL, fourth pass (t_motseq 15/21 written, src/Tools/t_motseq.cpp; t_rck 13/33 written, src/Tools/t_rck.cpp;
  neither Matching; include/db_mod.h grew `DbModSlot::seqFlag` (0x148) and the dbModSetViewFlag/UnsetViewFlag/
  MotionSetSeq(int, void*, u16, u16)/GetMotFilename declarations plus the `dbModMotionSetSeqI` u32-argument alias
  (COMPILER-DIFF 4), 2026-09):
  - t_motseq: work = `MsqWork` (0x1E10, `MsqSeq seq[1]` of 0x10C0 = `u16 num; u8 reverse; MotionSeqKey key[1024]` +
    editor bytes, then mode/sub1..3, a JOY copy at 0x10D8, the eprintf colour byte 0x15B8 with an UNALIGNED
    `GXColor bg` at 0x15B9 (`lwz 0x15b9` for the by-value GXSetCopyClear), seqNo/fileName at 0x1CA4/0x1CA8,
    start/end/add/max 0x1DAC..0x1DB8, seMode 0x1DBC) behind the one-member struct `msqWork.p` (.data, `= {0}`);
    routine table + `int msq_y_tbl[21]` follow it in .data. `msqSetMode(m)` = mode, sub1 = sub2 = sub3 = 0 as
    four plain statements (each reloads the pointer, so the order is the source order).
  - The Debug_alloc store with `lis rX,work@ha` before the call: `MsqWork*& wp = msqWork.p; wp = Debug_alloc(..)`.
    Pool order 0.0 before 1000/3000 while the first store is a 1000: `f32 zero; zero = 0.0f;` assigned right
    before the block and stored through the variable (the constant enters the pool at the assignment).
  - `strcpy(p, "0.seq")`-style byte stores written out (`p[0]='0'; ... p[5]=0;` natural order gives the target's
    `stb 5 first` schedule; the known-zero `sub1` register is stored for the terminator).
  - A case body that is a fall-through target of another case in the target (`case 5: bl msqSaveFile; b case7`) is
    TWO full copies in the source (case 5 with its own tail): the later copy keeps the known-zero register of the
    switch region; a source-level fallthrough label has two predecessors and gets fresh `li 0`s.
  - Default arm written first (`case 0: default:`) lays it out right after the compare tree; identical case bodies
    cross-jump into the LAST copy (4/6 shared tail sits after case 6, so source order 4, 5, 6, 7).
  - msqDisp: the flag/SE display block exists twice in the source (hand-inlined, strings once); its `int cx = 3`
    is declared right before the "--SEQUENCE INFO--" eprintf so cse folds `cx*8` to 24 for the lines before the
    Free loop and the SE line after the loop keeps `slwi r3,r17,3`; the loop y is the giv `238 + i * 14`.
    Palette `GXColor c1/c2/c3 = {..}` locals are 4-byte ADDRESSOF slots (`stw 0; stb` init), `col` is a fourth.
    `const f32` step constants (5, 4, 24, 25, 15) are declared right before the grid loop (after `x = 40; rc.w = 13`)
    to get the target's pool order with 369 last.
  - QuitCk: `cmpwi 0; beq L; cmpwi 1; L:` (dead second compare) is a switch whose case 1 shares the default body
    (`case 0: S; case 1: default: S;`); ours merges the bodies fully and drops both compares (open, -8 bytes).
    Sequence's lone dead `cmpwi r30,0` after the frame clamp is open too (no if/switch/dead-store form keeps it).
  - SeqResize (open): the target's delete loop is un-rotated (`b LOOP` back edge) with a PRE'd `num - 1`
    (`subi r0,r7,1` in both predecessors) and a real `lhz r7` reload after the `sth` — every while/for/do form
    tried rotates the loop or forwards the store to the load (`clrlwi`).
  - t_rck: `RckWork` (0x14AD4: mode/step/cursors/flags, `f32 curX, curY` used as a Vec, camMode, JOY copy at 0x28,
    screen centre floats 0x290.., catchTimer/cur/near/lineStart/savedRtp, `RckHeader hdr` (magic "2RTP", nPoint,
    nLine, nSq = nPoint², ofsLine, ofsNext), `RckPoint pt[128]` (Vec + lineOfs + nLine), `RckLine line[128][128]`
    (s16 to, u16 len), `s8 next[128][128]`) and the 0x14818 save buffer, both behind one-member .bss structs;
    rckSetNextPoint is Dijkstra over a 0x400-byte `RckNode[128]` frame array; the mode table is a local
    `void (*tbl[6])()` template copied to the stack in ToolRctRouteCheck; `.data` ends with `.balign 8`.
  - Screen-position conversions in the eprintf2 labels are `(u32)` (fcmpu 2^31/cror/bso pattern), the point
    height snap is `(f32)(s8)((y + 62.5f) / 500.0f) * 500.0f` (`psq_st qr4` + `extsb`).

### t_esp window system (db_window Matching, db_widget 94/113, include/db_widget.h; 2026-09)

- Unit pins of t_esp were one string too late: every header-string group of the module starts with
  atari.h's `cFlag.set()` message, so db_port/db_widget/db_window/t_esp start 0x24 earlier
  (0x23D0/0x2CA8/0x3410/0x35E0). The seven implicit destructors `~DB_WINDOW..~DB_SLIDEBAR` that sat at
  the start of "db_window" are db_widget.o's linkonce tail (after its cManager<cLight> block):
  db_window.o begins at `DB_MOUSE::DB_MOUSE`.
- Implicit (synthesized) destructors of a class whose vtable a unit emits: the original compiler
  synthesized them from the vtable entries alone; ours only synthesizes at a use inside a function.
  A never-called `static` function that `delete`s a pointer of each class (dead-stripped, listed in
  STRIP_UNUSED) makes ours emit the same seven bodies (no vptr store, size 0x20/0x44), in class
  declaration order, after the template block. In-class `virtual ~X() {}` bodies are wrong (they
  store the vptr, +0x10 each).
- GNU v2 vtable order = virtual declaration order with the dtor where declared; the key method is
  the first non-inline virtual in TYPE_METHODS order (ctor, dtor, then declaration order), so a
  class whose dtor is implicit gets its vtable where its first user-declared virtual is defined.
- Grouped `case A: case B:` labels with one body give a range-folded compare tree; the target's
  per-value `beq` nodes with cross-jumped bodies mean each case had its OWN identical body
  (`case S8: *(s8*)p = v; break; case U8: *(u8*)p = v; break;` compile identically and merge).
- A parameter list the .sym cannot show: `DB_WINDOW::CallActiveChangeCallback` takes
  `(DB_PRIMITIVE*, DB_KEYBORD*)` it never reads (the caller's `mr r5, r30` is the only evidence).
- `id = (counter += 0x10)` in C++ re-reads the lvalue: `stw r9,counter; stw r9,id` then later
  `lwz counter; stw id` (two stores of `id`).
- `size.y = h; rect = DB_RECT(base.x, base.y, w, size.y); size.x = w;` — the reload of the
  just-stored member is forwarded as `fmr f12,f2` while the untouched parameter is used directly;
  the temp of a 4-arg inline ctor is stored through its `this` pseudo (`0xc(r9)`) and the first
  member through the frame (`0x8(r1)`).
- `!(a & 1) && !(a & 0x10)` is folded to `(a & 0x11) == 0` by fold_truthop; nested `if`s (or
  `(a & bit) == 0` forms) keep two `andi.`. `int ok = (flags & 1) == 0; if (ok)` gives the
  `xori; andi.; beq` first test of DB_NUMERIC::Update.
- A `DB_PRIM_ARRAY::ChkMouseButton(DB_PRIMITIVE*, DB_MOUSE m, int)` by-value class parameter
  (class with a ctor) is copied by the caller in 0x18-byte `lwz/stw` chunks and passed by address.
- Store-order shapes seen here: `a = b = g = r = 1.0f` (chain) stores r, a, g, b; a DB_WINDOW
  `DB_WINDOW* w = 0;` dead initializer is the zero register of the two preceding member stores;
  `for (j = i; j != 0; j--) win[j] = win[j-1]` (u32) gives `mtctr i` after an `i != 0` test where a
  do/while or signed loop keeps `subic.`.
- Open (db_widget, sections byte-equal, 19 functions): DB_PRIMITIVE ctor's 60-store block order;
  the seven `SetNumPointer` min/max stores (pool order 127,-128 but stores `a0` then `a4` after
  the int stores — no statement/const-local order gives both); `&base`/`&size` kept in
  callee-saved registers in DB_STRING::Draw / DB_WINDOW_TITLE::Draw (base.y via `4(r30)`, base.x
  via `0x3c(r31)`: a PRE'd `(plus this 0x3c)`); DB_STRING ctor store schedule (vptr store between
  the colour chain and `str = len = 0`); DB_NUMERIC2::OnCalcMsg keeps MIN/MAX cross-jumped and
  DEFAULT separate (ours merges all three, COMPILER-DIFF #6 shape); AddPrimitive (-8),
  CallActiveChangeCallback (`mr r9,r3` copy of this), DB_WINDOW ctor temp loads (`0xc(r1)` vs
  `0xc(r9)`), DB_NUMERIC ctor (-4).

### Small tool RELs, fourth pass (t_light/t_scroll 37/42, t_movie/snd_test 58/77, t_movie/t_snd_vol 13/27 written; 2026-09)

- bcmp-style byte compares align functions by symbol name: an out-of-line header inline (varargs
  `cLog`-style body, no symbol in our object, `fn_<mod>_<addr>` in the split) shows as MISSING even when
  its bytes match — compare the `.text` tails by hand before counting it as a residue (t_scroll).
- Original-only shapes found in snd_test (none reproducible, all left as residues):
  - A loop-invariant `Snd_voice_work` address that our reload rematerialises with `lis/addi` is, in
    the original, spilled to a constant-pool word (`lis/addi/lwz 0(r)` of an anonymous `.rodata` word
    emitted after the function's last string, i.e. at pool position) and costs one more callee-saved
    register (disp_sequencer: one more stack slot for `ch_prio`). `static T* const p = &g` folds,
    `*(T* const*)&p` gives a *named* word at declaration position — neither is the pool shape.
  - Identical 1-char string literals (`">"`, `"<"`, `"D"`) are NOT merged across function groups in
    the original (3 copies of `">"`, 2 of `"<"`/`"D"` in one unit) while longer strings are. Ours merges
    them; distinct constants with identical bytes come from `">\0"` / `">\0\0"` literals
    (`// COMPILER-DIFF: candidate #10`). Within a function (or a group of functions that the original
    seems to share one literal between) keep the plain literal.
  - `w->cur += dir` on an s8/s16 member with an int `dir` gives `add r0,rDir,rCur` in the original; every
    source form (`dir + w->cur`, narrow locals, int temps, inline helpers) gives `add r0,rCur,rDir` here
    (test_tbl_now_check, test_req_no_select: 1 word each). Shortened narrow arithmetic swaps a
    `(subreg dir)` operand behind the loaded pseudo in our expand_binop.
- `switch (w->mode) { case 5: ..; case 6: case 7: ..; default: .. }` is the source of the target's
  `cmpwi 5; bne; ...; blt default; cmpwi 7; bgt default` chain (Snd_test_mode), not an if/else-if.
- A function whose every `return 0;` shows no `li r3,0` in the target is `void` (snd_test
  test_mode_menu/test_mode_move: the callers ignore the result).
- Calls that pass a stale `r3` (no `mr r3,r31` before the 2nd/3rd `bl`) come from a hard-register
  argument variable: `register SndTestWork* p asm("r3") = w;` then `f(p)` per call — GCC 2.95 never
  restores an explicit hard-register variable after a call (Snd_test_disp_basic).
- `lis/addi` + `stw r0,0xb0(r9)` for a single store to a global struct field (instead of the folded
  `stw r0,sym+0xb0@l(r9)`) means the field is `volatile` (SND_CTRL_WORK::dma_busy; the DMA wait loop
  `do {} while (ctrl->dma_busy != 0)` re-reads it for the same reason). Pointer-base form via a local
  `SND_CTRL_WORK* ctrl = &Snd_ctrl_work;` alone is folded back by cse.
- Zero-store order `stw 0x98; stw 0x90; stw 0x94` interleaved with an address computation is decided
  by source order through the scheduler: the target order was `dirNum(0x90), dirTop(0x94), dirCur(0x98)`
  in the source (directory_open) — try the permutations in a harness rather than reasoning.
- String helpers: `while (*s++ != '.') {}` is rotated with a duplicated head; `do { c = *s++; } while
  (c != '.');` gives the target's `bne <function start>` loop. `char c = *s; while (c) { ..; c = *++s; }`
  gives `lbz; extsb; cmpwi; beqlr` + `lbzu` (change_to_cap); `while ((c = *s++) != 0)` gives the
  `b test; body; test: addi; extsb; cmpwi` shape (get_dir_level).
- A `path[i + 1]` whose target `add` has the pointer first (`add r9,r3,r11`) is `*(path + i + 1)`; the
  plain subscript puts the index first in the sum here (dir_name_up).
- `if (a) return 0; ... return 1;` vs `if (!a) { ...; return 1; } return 0;` decide whether `li r3,0` is
  inline before the branch or out of line (blk_no_check inline, test_move_epara_* out of line) — check
  each leaf function's tail separately.
- A `s8 no = w->auxCur;` local set in every switch arm gives `extsb` at the load; the target's `lbz r11`
  in every arm (also in the default arm that has no use) plus `extsb` at each use is gcse PRE of a
  re-read `w->auxCur` used after the switch: drop the local and re-read the member.
- t_snd_vol: `GXColor`-looking colour parameters that arrive in a GPR (`stw r4,slot; addi r4,r1,slot`)
  are `u32` in the original (V4 passes aggregates by reference: our `GXColor col` param gives
  `lwz r0,0(r4)`); pass `u32` and cast `(GXColor*) &col` at the Tprim call. The tool's work pointer is a
  `Debug_alloc`'d struct reached through a `static` pointer that the original reloads after every
  store: same struct-member wrapper as t_scroll (`struct SndVolWorkPtr { SndVolWork* p; }`), initialised
  `= {NULL}` so it lands in `.data` next to the tables.

### Small tool RELs, fifth pass (t_esp/db_port 51/68 written; t_camera/t_camera 19/57 partial; 2026-09)

- db_port.cpp (src/t_esp/db_port.cpp, D:/Bio4/Prog/db_port.cpp; .data/.bss byte-equal, .rodata 0x10
  short): the game bridge of the effect tool. Include set from the header strings: db_widget.h
  (atari/light/map_obj/widget) then event.h. Everything except DB_DrawBox/DB_DrawBoxFill/DB_DrawString is
  `extern "C"`; `font_draw(u8*, f32 r,g,b,a, s16 y, s16 x, s16 z, s16 w, s16 h)` is this unit's own copy
  (not eprintf.cpp's) and `fontTMtx` is imported by name although static in the DOL (make_rel resolves
  DOL names from symbols.txt regardless of scope, no DOL edit needed). `SetToolLight__Fi` is global in
  the t_esp REL (LightToolStart calls it): tools/db_light_esp.cpp exports it like the Tools wrapper, the
  t_esp symbols.txt scope was flipped by hand. `DB_MODEL_FILES` (db_mod.h) is a 0x85C class with an
  inline ctor calling init(): EspToolInit's three `static DB_MODEL_FILES` locals produce the
  `if (!guard) { init(); guard = 1; }` pairs in .bss, followed by the nine `static u8 tbl[0x20]`
  texture-blend tables (function statics in text order), then the file-scope statics. The two
  `DB_MODEL_FILES::append` overloads (char* 0xE0 / void* 0x84) were named by hand in symbols.txt.
- Idioms found: `(f32) -(int) joy->trigL` = `lbz; neg; xoris` classic conversion, `(f32)(int)(u8) x`
  = `lbz; xoris` (DB_GetKeybordData); `int idx = Sp_char_ck(c) - 0x20; (idx & 0x1F) * 8;
  ((idx >> 5) & 7) * 16` = `clrlslwi 27,3` / `rlwinm 31,25,27` (font_draw); a `char name[5]` local is
  BLKmode and stored frame-relative while `char name[8]` (DImode) gets an `addi` address pseudo
  (LoadModEff); a `u8 zero = 0` declared mid-block AFTER the early-return compare gives the shared
  callee-saved zero of two later byte stores without hoisting its `li` to the prologue; `if (mode ==
  0) f = 0; else f = (u16)(8 << (mode - 1))` (SeqSet); `EstSet(m, -1, 0, 0, head, f | 1, 0, (u32) m,
  0xCF, 0)` is the C++ overload; `DB_GetCursorPos`: `if (parts == 0xFF || flag)` (not `flag == 0`);
  `if (ret == 0) { fail; return 0; } ok; return ret;` for the `beq fail` layout (LoadData); explicit
  `->pNext->pNext...` chains (a loop compiles to `mtctr`); `EspGetPathAddr(pe->id, pe->owner)` with the
  cEsp pulled into a plain local (`PullEsp(&esp, 6); pe = (DbPathEsp*) esp;` — the address-taken `esp`
  would be reloaded after every call); the sp_3dgrid sizes are `(f32) -anm->x4` / `(f32) anm->x6`
  (s16 fields) with `(f32)(int) anm->x0 * 0.5f` fallbacks and gen->x88/x8C read into locals right
  after DB_GetCursorPos; GXColor byte stores in a 4-byte ADDRESSOF local come out in pure source
  order (drawTexture2: g, a, b, r) and `Mtx m; Mtx44 proj;` declared after the colour calls give the
  temp-0x8/m-0x10/proj-0x40/col-0x80 frame.
- Residues (all register/schedule or compiler-build): GetActiveModel/DB_isGetComeEventTool/SeqSet keep
  `int on = 1; if (!(flags & bit)) on = 0;` (evtToolOn inline) but SeqSet's zero-argument register and
  the `r9/r11` naming differ; EspToolExit's `&CamDbg` stays `lis/addi` + `stb 0xf(r10)` in the target
  (ours folds to CamDbg+0xF; the target's shared `li r8,0` also feeds the GXColor word); DB_GetCursorPos
  moves `head` to r0 and loads `parts` into r3; DB_VecNullPartsPos/DB_VecMulEmPartsMat/sp_* differ in
  callee-saved assignment of the pointer params; EspToolInit (0x1FC8) is written but -0xCC: the target
  spills `&tbl1` into an anonymous `.rodata` word (the snd_test pool-spill shape, .rodata 0x28A8),
  hoists the `i + 1` increment into a stack slot and keeps `EvtDebug` in r28/r29 pairs. DB_ConfigLoad
  (-4) parses `[KEY] value` blocks into a 16 x 0x68 table (name, motNo, pos, ang, parOn/No/PtNo,
  parPos/parAng) — structure identical, register order open.
- t_camera.cpp (src/t_camera/t_camera.cpp) is PARTIAL: ToolCamera, tcDataInitialize, tcInit, tcMenu,
  tcSubMenu, tcEdit, the twelve tc*dat helpers, tcNext*Ptr, head/tail/next_suffix, tcQuit,
  tcToolCameraMove, tcPreviewOnOff, tcCurrentCameraNo are written (19 identical); tcEdit_select,
  tcEdit_area, tcArea*, tcDrawArea, tcEdit_camera*, fix_camera_dat, edit_rail_*, edit_frame_no,
  tcMoveOffsetPoint, tcCamera*Point, tcDrawOffset, tcDrawRail, tcLoad, tcSave are not. The unit defines
  the .bss pools (tcTypeTbl/tcAdat/tcCdat/tcLdat/tcWork) and .data (`tcTypeName[9]`, `tcOnOff[2]`,
  `tcMenuPos[12]`, pTc, the routine table, the main menu names). TcWork got the field names of the
  offsets this unit touches (include/t_camera.h); `TcAdat::poly` (TcPoly view at 0x58) is what
  tcAdatInit writes the corners through (`stfs 0x4(r27)` off `&a->num`). `tcNextCdatPtr(s8, int)`,
  `tcNextAdatPtr(s8, int, int)`, `head/tail_suffix(s8)`, `next_suffix(s8, int)` take s8 (no `extsb`
  at entry: `__FSc` mangling, symbols renamed by hand after the first sync). `CameraTargetDistance`
  (cam_sys.cpp) is imported by the REL: made non-static, declared in camera.h, DOL scope flipped.
- PITFALL (cost a REL mismatch): a partially written unit that declares the routine-table entries
  it does not define as plain (non-static) functions makes `sync_rel_symbols` flip their scope to
  global in the module symbols.txt; the split object's ADDR32 field then holds A instead of S+A and
  the REL's .data differs (t_camera .data+0x56C/0x570 = tcLoad/tcSave). Keep the scopes local by hand
  until the functions exist.

- Tools debug-tool editor template (include/dbg_tool.h, 2026-09): `cDbgFileSelectWindow` /
  `cDbgOkCancelWindow` (cDbgWindow-derived, 0x348 / 0x238), `cDbgButtonTemplate<T>` (cDbgButtonBase +
  `int (*pFunc)` and `void (*pUpdate)` `(int no, T*, cDbgButtonTemplate<T>*)` callbacks at 0x1C/0x20, 0x24), `cDbgEditWindow<T>`
  (cDbgWindowBase + pWork/numWork/rows/top/execMode/copyCursor/copyWinMode/copyBuf T/bufValid/
  pButton[128]/num/pCur/pTop/pBottom/the five work callbacks; virtual order dtor, GetCx, GetCy,
  SetCurrentBottomButton, ButtonAllUpdate, LocalUpdate, LocalDisp, SetCurrentTopButton,
  ButtonPushCheck), `cDbgToolMain<T>` (0x50: pMenu, pEdit, pSave, pLoad, pSaveOk, pLoadOk, pExitOk,
  mode, ..., the callback copies, vptr at 0x4C). Units: t_esp_area (36/38, ToolEspArea -36),
  t_lightarea (36/38, ToolLightAreaMain -36); .rodata/.data/.bss identical, function order identical,
  not flipped. Idioms found there:
  - A module's later copies of the header's linkonce functions are nameless (`fn_Tools_*`); pair them
    by the gap after the previous named function, and check the ORDER of the block: the FileSelect
    destructor is `virtual ~cDbgFileSelectWindow();` + `inline ... {}` defined AFTER LocalUpdate
    (deferred inline, emitted after Init/LocalUpdate), the OkCancel destructor stays implicit (synthesized
    when its vtable is written, emitted BEFORE its LocalUpdate).
  - The sizeof(T) gap under the tool's spill slots (ESP 0x98, LIGHT 0xD8) is the frame of a once-inlined
    helper with a T local (`T unused;` in CreateEditWindow): the inlined callee's frame is one stack temp,
    freed after the statement, and the loop body's `Vec pos, scr` (declared inside the `for (;;)`) reuse
    its start. A T local in LoadData would give two temps (LoadData is inlined again inside Update).
  - `for (i...; i++, w++) { AreaData* a = &w->area; if (IsWorkAlive(w)) {... w->flags ... w->areaNo}`:
    with `a` an unconditional giv, combine_givs makes the field loads a-relative (`lwz 0x30(rA)`,
    `lbz -2(rA)`) while `w` stays the stepped base; `a` inside the `if` keeps them w-relative.
  - Switch cases past cse's jump-following path length (PATHLENGTH 10 conditional jumps) lose the
    cse'd `&Joy`: the four-case AreaNoExec takes `JOY* joy = &Joy[0]` right before `rep = joy->rep`
    (so it does not live across the eprintf calls) and uses it in the cases; the return still reads
    `Joy[0].trg` directly (the pointer is dead at the join).
  - Consecutive `pG->flags |= a; pG->flags |= b;` fold into one `oris`; the target reloads pG between
    them: use BitOn/BitOff (global.h) for every flag update in the tool bodies, not only in Init/Exit.
  - The tool's first zero-initialised local is the zero register of the inlined cDbgToolMain
    constructor's 22 stores and gets the callee-saved register (`u8 wait` in ESP, `int preview` in
    LIGHT, declared before `tool`); the later `= 0` locals (u8 cnt, int cam, u8 wait, int plNoHit) take
    their zero from the most recent zero pseudo and spill in declaration order.
  - `if (m == 4) ... else if (m == 2) {} else if (m == 3) {} else wait = 1;` keeps the three compares
    (`!= 2 && != 3` folds to subi/cmplwi); empty `case 4: case 5: break;` in Disp shape its tree.
  - The window pointer of KeyCheck+LocalUpdate is a parameter (`WinUpdate(cDbgWindowBase*)`), the
    AddButton column loop reads `pEdit` once into a local, InitAllWork keeps `e = pEdit; w = e->pWork`
    locals: otherwise every store through the window reloads the tool member.
  - t_lightarea defines the module's `__builtin_new/__builtin_vec_new/__builtin_delete`
    (`Debug_alloc(size, 1)` / `Debug_free`) at the top of the unit; its works and the file image are
    Debug_alloc'd and `MakeSaveData` (SaveData without the file) feeds `LightAreaDataLoad` every frame;
    `((cUnitEventView*) pPL)->endEvent(0)` (st_mgr_event.h) is the `lha 0x18` virtual on the player.
  - OPEN: the edit window's `new` null test is compared BEFORE the constructor's AddButton loop
    (`cmpwi r31,0; mfcr r29` .. `mtcrf; bne` after `pEdit = e`; no branch around the body: not
    -fcheck-new), so the hoisted `preview == 0` compare takes cr7 (`mfcr; slwi 28`); the three
    OkCancel Init copies materialise `li r0,0` per window for pCur/pTop/pBottom while x1C/x20/num share
    the hoisted zero (one pseudo here, chains/NULL/order do not split it); cDbgWindow::Init (db_toolbase,
    19/20) and cDbgFileSelectWindow::Init schedule `li 1` before `li 0` and keep the LAST zero store
    last, ours issues `li 0` first and hoists the last zero store to the top (the FS path-pointer stores
    also move up); statement order, chains, in-class/out-of-class, an inlined base Init do not change it.
    ToolEspArea's r14/r15 (wait vs Joy@ha) and r22/r23 (`&tool`) swaps follow from those.
- cse and in-struct stores: the FIRST load of a global pointer decides whether a later in-struct store
  invalidates it; use the struct view (`pPLS`) for every read that must stay ordered, not just one.
- `static inline SetAngV(cModel* m, Vec* v) { m->setAng(v); }`: every call is a fresh `addi r4,r1,N`
  (integrate substitutes the caller's address into the hard-register arg set) -- no PRE copy.
- Peeled first iteration with a duplicated `y = lim` store = `step; if (y < lim) y = lim; else { wait:
  sleep; step; if (!(y < lim)) goto wait; y = lim; }`; `spd = 0.0f` after the preceding call.
- `(T*) ((i) * sizeof(T) + (u32) p + 0x10)` puts the array offset last and index-first.
- Two sibling `if` bodies needing the same Vec slots after a block that used one: declare the Vecs
  once mid-function (per-block declarations best-fit into the earlier block's slot).
- Three identical blocks with per-block cEmWrap member and task function: a macro, not an inline.
- `EM_LIST(0x19 + (u32) n % 3)` gives `mulhwu 0xaaaaaaab; srwi 1`; `(f32) n` with `u32 n` gives the
  2^52 magic without the 0x80000000 word.

- Vec temporaries loads-before-stores: `f32 x = a->pos.z; f32 y = ..; f32 z = ..; v.x = x; v.y = y;
  v.z = z;` (plain member assignments interleave load/store; `FSetP`/Vec* helpers get hoisted).
- `!(x & 1)` folds to `xori; andi.; beq`; the target's `andi.; bne` needs `(x & 1) == 0`.
- Un-rotated `for(;;)` with calls: keep every exit in an `else` arm (`if (c) { body } else break;`).
- `!(f & A) && !(f & B)` folds to one test; the target's two `andis./andi.` need nested ifs.
- `FAdd(SmdGetObjPtr(n)->pos.y, spd)` keeps a static-pointer load below the store.
- `u32 max = *(u16*) mot; (f32) max` gives `lhz` + `stw/lfd`; a `u16` local gives `sth/psq_l qr3`.
- `int i` over `work->em[i]` gives count-down `li 0x70; subic.`; `u32 i` keeps `cmplwi`.
- `p->total = p->cnt = call();` stores cnt then total from one work-pointer load.
- Second cEmWrap alias `cEmWrapSetPtrI(...) asm("setPtr__7cEmWrapsSci")` for an untruncated u32 (#4).


### Tools/t_sce t_sce_at.cpp (shared object, 58/59 named functions byte-identical, sections identical; 2026-09)

- src/tools/t_sce_at.cpp (D:/Bio4/Prog/t_sce_at.cpp, mem_alloc line 0xBE3): the scenario-area ("AEV") editor,
  the same object in Tools and t_sce (`bcmp` 58/60 in both; the 60th is the nameless cManager<cLight> block).
  Include order from the header strings: light.h, atari.h, dmg.h, map_obj.h, widget.h (explicit includes). Every
  function but `ToolSceAt()` is `extern "C"` (statics inside the block). Work = `Debug_alloc(0x19DB8)` behind the
  one-member struct `sceAtWk.p`, the edited area behind `sceAtCur.p` (.bss 8 = the two pointers): header 0xFC,
  `SceAtWork area[128]` 0x10C, load/save image 0x4F0C, copy buffer 0x9D1C, then two 512 x 64 message-name tables
  (loadMesName parses `#define MES_` of the room / common mes headers). `SceAtSys` (static in game/sce_at.cpp) is
  imported by name through a local `TSceAtSys` view; the DOL needs no edit (make_rel resolves DOL names by name).
  Not flipped: `tSceAtDataInput_basic_menu` is -0x24 (below). NOTE for the flip: t_sce has three `set_filename`
  (t_block, t_sce_at, t_sce_item, all `global` in the .sym); the compiled copy defines a global `set_filename`
  next to t_block's split one — check the -r link (or make ours static and re-sync) when flipping.
- Idioms that mattered here:
  - u8 wrap-around `field = n < 0 ? hi : (n > hi ? 0 : n)` must go through a second variable (`m = ...; field = m;`)
    or the store is duplicated into both arms (`stb` per arm); the 0..hi clamp is the if-form
    `if (n >= 0) { m = n; if (m > hi) m = hi; } else m = 0;` with `m` a SEPARATE variable (`CLAMP(n, n, ..)` ties
    the copy away: no `mr r0, n`). With a variable upper bound put `int max = num - 1;` first in the then-arm
    (`subi` scheduled before the `mr`).
  - `y = pW->y; y += 0x10;` (two statements) keeps `lha` (a second set stops combine from narrowing the load to
    `lhz` through the `extsh`); `y = pW->y + 0x10` gives `lhz`.
  - Three editors (ladder_main, pos_jump_main, cam_ctrl_main) address the payload as `pCur->ladder.pos.x` etc.
    (pCur reloaded per access); the others (mes, flg, shd, damage, scr_at, use, save, field) use a payload pointer
    `T* d = (T*) pCur->data` (r31 = pCur + 0x5C, loads `lwz 0x5c(pCur)` folded, stores `stw 0(r27)`).
  - `if (d->flags & 2) eprintf(...); fl = d->flags; if (fl & 2) { draw }`: the second test through a `u8` variable
    stops jump.c's thread_jumps (a REG_USERVAR on the Y side fails rtx_equal_for_thread_p) — the target keeps both
    tests; the plain second `if (d->flags & 2)` is threaded into the first `beq` (-4 bytes, branch to the end).
  - `u8 em = pCur->x39 & 9; if (em != 0)` gives the target's `andi. r0,r0,9; cmpwi r0,0` pair.
  - Case-local pointer copies where the target keeps pCur in one register across skipped blocks: case 2
    `SceAtWork* a = pCur;` (the store after six `if` blocks uses the case-top register; the cse path budget
    PATHLENGTH 10 is exhausted from the function entry), case 4 `u8 f = a->x38` reused for the RIGHT switch's
    `a->x38 = f | 0x80` while the LEFT switch and the final store re-read `pCur->x38`.
  - door_PosSet: the `dstPos == 0` test reads the constant through `static const f32 zero = 0.0f` + `FCRef`
    (the pool word sits before the function's pool, its `lfs` stays below the pPL struct copies), the pPL copies
    are `memcpy((u8*) pW + 0x14, &pPL->pos, 12)` (pPL reloaded between), the pG next-room stores are
    FSet/U16Set/U8Set (pG reloaded after each), the return copy `FSet(pCur->dstPos.x, pPL->pos.x)` (pPL reloaded),
    `Vec d = {0,0,0}` declared inside the loop body (memset per iteration), and `pCur->dstPos.x == (z =
    FCRef(zero))` inside the condition for the r3/r4 order of the two `lis`.
  - Menu enable chain `create[1] = create[2] = edit[3] = edit[4] = valid` (store order create[1], edit[4], edit[3],
    create[2]; createMenu's address pseudo first) — the t_se_at EditMenu r7/r8 open item is the same thing.
  - DataLoad: `int ret = 0` block-local inside `if (sel >= 0)` (li r3 after the blt), a separate `int cmp` for the
    strcmp result, error arms written first (`if (version != 0x104) {err} else { cmp = strcmp(); if (cmp != 0)
    {err} else {ok} }`), literal zeros in cases 8/9. DataSave: a typed `TSceAtFile* p` for the memcpy (crclr libcall).
    tSceAtExit: `if (sel >= 0) switch (sel)`. loadMesName: `u32 n` assigned after the early return (`cmplwi 0x1ff`,
    `li r27,0` after the call), `dst = (char*) (n * 64 + (u32) names)` (index first in the add).
- OPEN (tSceAtDataInput_basic_menu, -0x24): gcse PRE hoists `high(sceAtCur)` into r27 for every case body
  (`lwz r11, @l(r27)`), the original keeps a fresh `lis r9` per case start and per post-clamp store and uses r27
  only in the display tail. In our .gcse dump expression 0 (high(sceAtCur)) is redundant in 13 blocks with the copy
  inserted at the end of bb 0; the target's case-start occurrences were not replaced (rematerialised single-use
  pseudos). Case-local `SceAtWork* a` copies fix individual cases but not the pattern; not understood.
- db_mod.cpp (Tools 0x9978 / t_esp 0xA0EC): NOT written. Facts gathered: t_esp's build has `DB_MODEL_FILES::append
  (char*)`/`append(void*)`, `dbModMotionSet` (0x1AC) instead of `dbModMotionSetSeq`+`dbModGetMotFilename`, no
  `dbModGetViewFlag`/`dbModUnsetViewFlag`, and the extra dbModBinName..dbModelSetAng0 loader functions before
  dbModelSetCamera (a define like db_light's). State `pDbModState` = Debug_alloc(0x3460) (view flag at 0x345C,
  0x3449 = load failed, routine index s8 at 0x10, 0xD8 set no, 0x1C..0x22 per-type numbers, 0x141 set name),
  `dbModSlot[64]` (0x2754 each: cEm* at 4, sixteen file pointers at 0x48/0x88, 0xD0-byte name slots from 0x108,
  0x148 seqFlag, mem_alloc(0x98, line 0xEB) at 0x1D4), a second 64-slot table at .bss 0x9D5E8, the mot_tbl.txt
  image (`Room/Em/mot_tbl.txt`, .data 0x500) parsed by mottbl* into a 0x4C.. table at .bss 0x13AAEC; .data = the
  15-entry routine table + `TOOL_MENU`-like `{name, id, flag}` 16-entry table + the string tables and the option
  values (0x46, 0x12C, 8, 0xB, 0x32, 8).
- Cross-jump survivor for a shared `return 0`: `switch` with `case 0: default: return 0;` first and the
  last case's inline `if (A && B) return 0;` with NO trailing return (em10ClimbOverCk).
- Nested switches returning 0/1: outer `default: return 0;` FIRST; inner arms `case ..: break;
  default: return 0; } return 1;` -> per-inner `li r3,0; bnelr; li r3,1; blr` (em10ArmorCk).
- Reload hands scratch registers out round-robin over the spill set: identical arm bodies with one
  such insn alternate r9/r11 and cannot all cross-jump (three arms -> 1 and 3 merge).
- cse's `find_best_addr` rewrites `(mem (reg w))` to `(mem (plus em 0x3e0))` only for the zero-offset
  member, so `w->flags` and `em->x3E0` merge while `w->x16c` stays w-relative.

- A `Mtx` declared at function scope sits below block-scoped Vecs and can make gcse PRE insert a
  frame `addi` at a loop entry, which flags the loop "phony" and blocks all invariant hoisting --
  declare the matrix in the block that uses it (r224 `reva_move` 71% -> 98.7%).
- `pPLS->rot.y = ..; pPLS->setPos(&p);` reloads `pPL` between the store and the call when a template
  copy lies between them.
- A virtual call then a member store on the same global object keeps the pointer callee-saved only
  through a local (`cPlayer* pl = pPL`).
- Check identity with a label-normalised instruction diff: objdiff marks `bc` REL14 relocs as REPLACE
  with identical bytes.


### Tools REL flags-first sweep (t_flr_at Matching; t_motseq 17/21, t_vib 24/29; 2026-09)

- t_flr_at (26 -> 29/29, flipped): the tool loop's `Debug_alloc` store uses the loop-hoisted
  `lis flrAtWk@ha` only through the reference form (`FlrAtWork*& wp = flrAtWk.p; wp = Debug_alloc(..)`,
  t_motseq idiom); the title lookup is index-first (`*(const char**) (editType * 4 + (u32) tbl)` ->
  `lwzx r8, rIdx, rTbl`). flrAtDataSave: the save counter is read and stored through references
  around the 0x84-byte record copy (`ISet(n, IGet(n) + 1)`, `IGet` = `int&` getter) so both the load and
  the store stay below the copy's `stw`s, and the u16 header count is `*(u16*) ((u8*) &n + 2)` — the
  target's `lis/lhz` carry `sym+2@ha/@l` (a plain `(u16) n` narrows to `sym@ha` + `sym+2@l`, a different
  REL relocation addend although the linked bytes agree). flrAtDataLoad: COMPILER-DIFF 2 launder as an
  inline `static inline u8 ColU8(int c) { asm volatile("" : "+r"(c)); return c; }` used *in the argument
  list* (a statement-level launder before the call moves the x/y argument loads after it); the `volatile`
  matters: the plain `asm("")` form let sched1 issue the following ternary's `lis/lwz` before the `clrlwi`.
- t_motseq msqFrameSizeCk: `max = (int) x << 6` as ONE expression gives the shift result a global.c
  hard-reg *preference* for the fpmem-load temp (set_preference looks through unary/binary ops), so `max`
  inherits the temp's local-alloc register (r9) and pushes the loop giv to r11; two statements
  (`max = (int) x; max <<= 6;`) make `max` a two-set pseudo allocated by priority after the giv.
- t_vib: (a) a `u32 c = col;` copy whose address is passed to a Tprim call is not a separate local when
  the target stores the incoming argument register into the frame (`stw r5, slot`): use `&col` directly
  (tvibListLineDraw); when the copy's store must stay ahead of the following pointer load
  (tvibFrameMarkDraw: `stw r5` before `lwz tvib`, and `size`'s copy `mr r5,r6` allocated after r5 is dead)
  the local is a `GXColor c` filled with `*(u32*) &c = col` (a cast-then-deref store, no struct/scalar
  flag, so the in-struct `V->scroll` load depends on it). (b) A record pointer used in two loops
  (`TvibData* d` at function scope, tvibFileSave) picks up the hard-reg preference of the second loop's
  memcpy argument copy and is `add r4, ..` in BOTH loops; block-scoped `d`s give r9 in the first loop.
  A header `memcpy(p, f, size)` must pass a *variable* size (a `sizeof` literal is inlined as a block
  move); `hsize = sizeof; memcpy(p, f, hsize); p += hsize; size = hsize;` puts `li size` after the call
  and `addi p` before it. `memcpy(p, d, len); p += len; size += len;` (size last, carrying len's death)
  issues `add size` before the call and `add p` after. (c) Colour-word stores: `col = C;` written AFTER
  the S16Vec corner stores is the dying store issued 5th/last (tvibMainFrameDisp), written first it is
  hoisted to the block top; the second poly block has its own `col = 0` after its stores. (d) `x = list_x
  + i * 230 - 2` allocates the product to r0; `(list_x - 2) + i * 230` gives the target's `lhz r0 / mulli
  r9` pair. (e) `top + i` written twice (index and eprintf argument) instead of an `int n` local gives
  the `add r0; mr r10, r0` PRE copy; the loop test is `i < 16 && top + i < 64`.
- OPEN, t_sce_at tSceAtDataInput_basic_menu (-0x24), mechanism narrowed: ours merges the case-body and
  x38-block `high(sceAtCur)` computations into bb0's pseudo in **cse1** (the path from bb0 skips both
  `on = 1` diamonds and follows the dispatch `beq`s), then gcse PRE inserts the reaching reg at the end
  of bb0 (also in the target: `lis r27` in the prologue) and the tail joins use it; the target kept fresh
  `lis` in bb2 and every case, i.e. its cse1 did not carry bb0's pseudo into them. Not a global flag: a
  scratch cc1plus with cse1 or cse2 running without follow-jumps/skip-blocks (and the -fno-cse-* flags)
  regresses 13-35 other functions of the unit and leaves basic_menu unchanged. t_vib tvib_R0_VibLoopSet
  is the mirror image: PRE inserts `high(tvib)` at the end of bb0 AND of arm 1, the skipped arm 1 re-sets
  the reaching reg, and ours rematerialises arm 2's copy (`lis`) where the target keeps the hoisted r10.
- OPEN, unchanged (register/schedule ties, all forms in the sources tried): db_toolbase cDbgWindow::Init
  and the FileSelect/OkCancel Inits (dying zero store not hoisted by the original, `li 1` before `li 0`),
  t_atari plmove10 (copy `stw`s after the RMW `stfs`s), t_mv mvInit, t_dr ListDisp/Menu_main,
  t_esp_area/t_lightarea mains, t_motseq QuitCk/Sequence/SeqResize/msqDisp, t_vib tvibModeFrameDisp (the
  target does not cross-jump the two 12-store arms: different temp registers per arm, `mode` in r7),
  tvibFrameLineDraw (frame +0x10, one more callee-saved), tvibListVibDraw, tvibEditFrameDisp; t_rck and
  db_mod not iterated (db_mod dbmodGetFilenames: the target strength-reduces `type * 0x800 + 0x3a1`
  as an outer-loop giv `li r22,0x3a1 .. addi r22,0x800`, ours recomputes `slwi; addi` per iteration).

### t_esp/t_esp.cpp (the effect editor, 170/212 functions byte-identical, .rodata/.data/.bss identical, not Matching; 2026-09)

- Structure (src/t_esp/t_esp.cpp, `namespace t_esp_namespace`, include set = db_widget.h only: the .rodata header
  group is atari/light/map_obj/widget with no event.h string, so db_port's API is redeclared locally): 50 window
  structs `{DB_PRIM_ARRAY* pa; DB_WINDOW* win;}` built by IN-CLASS constructors that InitTool inlines
  (`g_pXWin = new X_WINDOW(g_pPrimArray)`), each widget in its own block `{ DB_POINT pos(x, y); int sx = N;
  pa->CreateButton(win, "..", &pos, cb, &sx, sy); }`. The `.rodata` proves the layout: every window's strings
  sit between its callbacks' strings/pools (parse order of the class body), InitTool's own pool (0x140 floats,
  first-use order = construction order) follows ClearSeqData's, so InitTool is one function calling the inlined
  ctors in .text order: Menu, Exit, Edit x4, Model, Load, LoadEm/Room/Sst/Event, Save, SaveEm/Room/Sst/Event,
  LoadCheck, SaveCheck, Option, DataSet, Time, ID, Path, Parent, Pos, Size, Speed, Color, Blend, Flag, Life, RT,
  AnmRate, Rotate, Vec0-2, Sub, Work0-6, WorkSp0-3, BasePos (the SUB_WINDOW class is DEFINED after WorkSp3
  although constructed before Work0; the LOAD_CHECK class right after LoadCheckClose_callback, before Save).
- Block-scoped `DB_POINT pos(x, y)` (db_widget.h gained `DB_POINT()` / `DB_POINT(f32, f32)`; db_widget/db_window/
  db_port unchanged) + `int sx` per widget are NOT slot-reused: the class-with-ctor temporaries get monotonic
  frame slots (16 bytes per widget, 0x2048 bytes of locals + one spilled `&local` pseudo each in InitTool's
  0x2930 frame). Same for inlined helpers holding such blocks. x stored via the frame, y via the ctor's `this`
  pseudo (`stfs f, 0x20(r1); stfs f, 4(rX)`).
- A public in-class inline ctor with a `static` local is never inlined (cp/decl.c "function with static variable
  cannot be inline"): `ID_WINDOW::ID_WINDOW` owns `static const char* kindName[] = {"Esp ", "Ctrl"}` (.data
  0x1524, defined right before the numeric that uses it, after the window's strings) and is therefore the only
  out-of-line ctor, emitted as a deferred inline AFTER the module's cManager<cLight> block (sym_map row renamed
  by hand to `__Q215t_esp_namespace9ID_WINDOWP13DB_PRIM_ARRAY`; sync cannot resolve linkonce copies).
- .bss order: EspToolTrans's unused `static DB_KEYBORD key;` (object + `_.tmp_0` guard) comes FIRST (function-local
  statics precede all file-scope ones), then the file-scope statics in declaration order (window pointers
  154D60..), the few globals (g_pMenuExitButton, the six pos/rpos DB_NUMERIC2*, g_immFlg[256]) interleaved at
  their declaration position. `.data` = 19 initialised scalars (`u8 = 0` stays in .data under -G 0), the name
  tables at the top (model 243 entries with two duplicated names, parent/parts 256, blend 17, filter 71, render
  9), then ID_WINDOW's table, ColorSimTypeUpdate's 16-entry table and three unreferenced statics
  (0, 0, 0x40000). `db_modelNo` (db_port.cpp .data+0x754, module label renamed by hand) is the BasePos "WorKNo".
- The record is the tool's own 0x12C struct (`TOOL_SEQ`: EspGenWork with array views x104[4]/x10C[4]/x110[4]/
  x124[4]/x128[4] and `Vec` fields); MakeImmSeq/AddSeq are 115-field macro expansions (`IMM(f)`: compare, flag
  byte, copy; `ADD(f)`: if/else store — the ternary form forwards the stored value and loses the clamp reload).
  s8 fields compare with plain `lbz/cmpw` (no extsb), s16 with `lha`. AddSeq's colour adds skip `no++` on the
  saturated paths (original bug, reproduced); the un-merged single `stb` of the 255 arm is COMPILER-DIFF #6.
- Idioms: `ISet(int&, int)` for `win->bring = 1` / `win->active = 0` in callbacks (the target reloads the next
  window pointer after the store); toggles are `if (v) v = 0; else v = 1;`; the dead `cmpwi 0; cmpwi 0xff` pair
  in the Save*FileNo callbacks is an `if (t < 0) .. else if (t > 0xFF)` on an unused s16 copy of the model type;
  `u32 i` edit-row loop (`(f32) i * 16.0f` is the unsigned double trick); PosStick scales 2000/-2000, 100/-100,
  40/-40, 1000/-1000, 1/-1 (fneg), 10/-10 with the y sign flipped for parts 0xF8..0xFD (`(u8)(parts + 8) <= 5`).
- Open: InitTool (-0x350: the 50 inlined ctors are written, register/`&local` spill order and `pa`/`p` choice per
  window not verified), ID_WINDOW ctor (254 words), Load/SaveEmTypeUpdate (group-skip loop shape), the
  0x12C struct-copy loops (DeleteSeq/InsertSeq/Copy/Paste/Make*SeqData: giv/base register choice), EspToolMain/
  ToolEspMain/DrawPosCursor (+-4..20), EditActiveChange (+4), the `bring`/`active` callbacks' zero register.

### Small tool RELs, flags-first sweep (t_scroll 38/42, snd_test 68/77, t_esp 186/212; none flipped; 2026-09-10)

- Judge with a masked compare whose `.rodata` reloc targets are compared by *content* (the referenced
  bytes), not by offset: snd_test's one missing pool word (the `Snd_voice_work` spill, original-only)
  shifted every later string and made 14 byte-identical functions look different. Placeholder-named
  target functions (`menu_13488`) pair with ours by `.text` order only when both objects list the same
  function set; sync the names first or the pairing lies (t_event today).
- `while ((s8) *p != '\n' && (s8) *p != '/') p++; if ((s8) *p != '\n')` — the second test in the SAME
  mode (`(s8)`) as the loop's exit compare lets jump.c thread the loop's `'\n'` exit past it
  (`beq -> loop end`); combine then narrows the single-use compare back to `lbz; cmpwi`. A `u8` view
  (`*p != '\n'`) keeps the re-test (t_scroll loadBinName).
- `lbzu rX,3(rP); cmpwi rX,'\n'; ... extsb` (no `clrlwi`, extsb only for the switch) is
  `p += 3; while (*p != '\n') { switch ((s8) *p) {..} p++; }` — the value re-read per iteration; a `c`
  variable (any type) that feeds both the compare and the switch gets an extra extension.
- `lbz r0; mr r31,r0; stb r0` (loaded byte copied into a long-lived variable, the store from the load
  register) is a *read-back of the just-stored member*: `obj->x = pWork->id; id = obj->x;` (cse forwards
  the store as a plain `mr`); `int id = pWork->id; obj->x = pWork->id;` gives `clrlwi` (t_scroll edit_ot).
- `lbz r0,40(r3); extsb r0; slwi r0; add r3,r0,r9` with the index temp in r0 in every arm of a
  function returning `idx * size + base`: ONE result variable assigned in each arm and a single
  `return p;` — the result pseudo is global, so local-alloc cannot tie the dying index to r3 (direct
  `return` expressions tie it: `lbz r3; ... add r3,r3,r9`) (snd_test test_get_tpara_adrs).
- `li r3,1` before the compare + `bne end` skipping a shared out-of-line `li r3,0` = `if (a != b)
  return 1; return 0;`; the `== b` form puts `li r3,0` inline and jumps to a trailing `li r3,1`
  (test_tbl_para_select). Same family: `beq body; li r3,1; b end; body..; li r3,1; b end; li r3,0` =
  an early `if (x != old) return 1;` plus a tail written `if (v == cur) return 0; store; call; return 1;`
  (the success block is the fallthrough, the fail `li r3,0` sits out of line at the end) (Snd_test_volume).
- A member RMW whose store is unconditional while only the update is guarded (`lhz` at the function
  top, `beq skip; xor; skip: sth`) is `v = *p; ...; if (c) v ^= bit; *p = v;` (move_type_flag).
- `lbz r3,3(r31); extsb r9,r3; lbzx ..,r9; ...; mr r3,r9` (raw byte in r3, sign-extended index in r9,
  copied to the call argument) = the s8 member read DIRECTLY at both uses (`w->efxState[w->aux]`,
  `Snd_efx_req(w->aux, ..)`): cse shares the QI load and the extension is a separate pseudo. A
  `s8 aux = w->aux` local ties the extension to r3 (test_efx_on_or_off).
- `lis/addi Snd_ctrl_work` hoisted into a callee-saved register before earlier calls, member stores
  `stb/sth off(rX)` = a `SND_CTRL_WORK* ctrl = &Snd_ctrl_work;` local declared before the calls
  (test_play_or_stop, Snd_test_disp_vol, Snd_test_disp_req_para). Snd_test_disp_req_para ignores its
  parameter: `SndTestWork* w = &Snd_test_work;` (the `lis/addi Snd_test_work` in r30).
- A function-level `int n = 0;` whose `li` the target issues AFTER a preceding loop is `int n; ...
  n = 0;` assigned after that loop; `int step = 1` likewise assigned after the early return / after
  the value it is compared against (test_tbl_aux_ck, Snd_test_volume, aram_dump_disp).
- `x` column as an explicit variable (`int x = 0x68; ... x += 0x18;` re-initialised per outer
  iteration) instead of the `0x68 + j * 0x18` giv: the counter keeps the higher register (target j r31,
  x r30) and the increments come out `j++, x += 24, n++` (aram_dump_disp).
- `v &= 0x7F; *p = v;` ties the mask to the variable's register (`clrlwi r9,r9,25`); `*p = v & 0x7F`
  gives a fresh `clrlwi r0,r9,25` (test_tbl_aux_ck).
- s8 fields the target `extsb`s in an eprintf `%d` argument while the header declares them u8:
  cast at the use (`(s8) sit->se_flag`), do not change the shared header (disp_sit_midi).
- A dead string in `.rodata` between two functions' string groups with no code using it: an `if (0)`
  call with that literal at the end of the preceding function reproduces it without STRIP_UNUSED
  (snd_test blk_file_disp "sbb").
- Pointer-global stores followed by a `->member` load of another global (t_esp callbacks
  `g_pLoadNow = NULL; g_pSaveNow = X; BRING(X)`): BOTH stores through a pointer-reference setter
  (`WSet(TOOL_WINDOW*&, ..)`, incl. the NULL one) keep the `lwz 4(rX)` below them AND in source
  order; with only the second one a reference the dependent store is issued first. `ISet` on an int
  global (`g_dataChanged = 1`, `g_modelLoad = 1`) before `BRING(g_pMenuWin)` likewise. `ISet(w->active,
  0)` on the parameter window keeps the `lwz g_pEditWin1` below it (EditActiveNext/PrevWindow).
- Nested call as an argument: the original re-reads a memory operand argument AFTER the inner call
  (`bl inner; mr r5,r3; lwz r4,0(r30)` with only the address `lis` callee-saved) — sequence the inner
  call into a local first (`int num = MakeSaveSeqData(..); SaveData(path, g_pSeqHead, num)`); ours
  precomputes the operand into a callee-saved pseudo before the inner call (SaveCheckOkCallback).
  OPEN variant (snd_test test_play_or_stop): the target has a DEAD `lhz r4,32(r31)` before the inner
  call and the reload after it — no source form gives the dead load (2 words, kept nested).
- Two identical if/else tails `r = g; b = a;` shared after an inner if/else (`b d8` into the sibling
  arm's copies): write the tail ONCE after the inner if/else, not per arm (SetEditTblColor; the
  remaining `lfs f28/fmr f30` vs `lfs f30/fmr f28` is cse's (set REG0 REG1) swap on `g = 1.0f; a = g;`
  — a's REGNO_LAST_UID is later than g's; no join order gives both the target's copy direction and
  copy order).
- `mulli r0,row,43; slwi r0,r0,2; add base` for a `[5][43]` table row loop = a FLAT index
  (`g_editNum[0][row * 43 + i]`); the 2-D `g_editNum[row][i]` folds to `mulli 172`.
- `sel->selX == 3 || sel->selX == 4` with two `cmpwi/beq` in the target: `SelXIs(sel, 3) ||
  SelXIs(sel, 4)` inline compares (the SysRegionIs idiom; the lvalue form is range-folded).
- `.bss` order of two pointer globals (`g_pLoadDirButton`/`g_pSaveDirButton` swapped): first
  declaration order, visible only through the reloc offsets of a function that reads both.
- Init store blocks derived in one shot with the refined store-order rule (ClearSeqData, 14 stores):
  target issue order = [the LAST dying store in RTL] + [the other dying stores in RTL order] + [the
  non-dying stores in RTL order]; a constant shared by several stores dies at its LAST store. Read
  the RTL order off the target: parts(0xFE), w, h, dplus, x30, anmRate, r, g, b, a, dr, dg, db, da
  with r/g/b/a and dr/dg/db/da as SEPARATE statements (a chain reverses them).
- OPEN, compiler-side (do not retry): edit_select_sub — gcse PRE turns the three post-loop
  `high(scrollWorkPtr)` into copies of a bb0 pseudo, the post-PRE cprop pass then substitutes that
  pseudo into the skipped-block join (`lo_sum 193`), cse2 restores the `lis` in every block and the
  join keeps its own pseudo (fresh `lis r9`); the target shares one `lis r10` between the `& 0x900`
  block and the `& 0x200` join, i.e. its cprop did not touch the join. DbSctrl `w->x/w->y` order:
  sched1 gives the target order (y, x), sched2 flips it through the anti-dependence of `stw r4` on
  the later `lwz r4,pG` (the clamped anti-dep cost rule). Snd_test_disp_voice / toolIdSpace:
  interblock speculative motion of the arm's arg moves / of the `&&` second compare into the test
  block — ours moves in disp_voice where the target does not, the target moves in toolIdSpace where
  ours does not (COMPILER-DIFF #5 family). Save*FileNoUpdateCallback (t_esp, 5 x 5 words): the dead
  `cmpwi 0; cmpwi 255` pair — ours keeps only the first compare of the dead `if (type < 0) ..
  else if (type > 0xFF)` (flow deletes the second store first, jump2's delete_computation removes its
  compare); switch/ternary/two-if/int forms all keep one compare or a branch.
- Not moved (register/schedule only, forms tried in the sources): t_scroll edit_litmask (`li 1`
  before `lwz x54`: both issue at the same cycle, unit choice), loadBinName tail `addi p` vs
  `cmplwi num`, printEditTable (112 words, allocation); snd_test test_blk_enable_ck (three
  callee-saved invariants vs r3/r0), dir_entry_read (gcse PRE of `&w->dir` into the loop's first
  block marks the loop phony: no invariant hoisting; the target hoists `cmpwi cr4,dirs` and the
  `blk_ext_name`/`dirName`/`dirIsDir` addresses — a `DVDDir* dir` local or moving `name[]` does
  not stop the PRE), disp_sit_normal/disp_sequencer/snd_test_disp_rit (original-only shapes, see
  the fourth-pass notes); db_sctrl and t_camera_data residues are the ones listed in their sections
  (tcDataImport: `extsb r7,r11` untied index + `lis pLog` not hoisted out of the record loop).

### Tool RELs, bytes-first pass (db_port 46->61/68, .rodata equal; t_esp_area/t_lightarea/t_id/t_camera/t_scroll synced; 2026-09-10)

- Counting: `unit_info.py` percentages of the tool units are dominated by placeholder names; use a masked
  compare (/tmp/tools_p/mcmp.py, copy of /tmp/plmod's with: weak `_vt.*` copies keyed by name so a
  by-name reference to the module's first vtable copy equals a local reference to the unit's own copy;
  `global_constructors_keyed_to_X`/`__static_initialization_and_destruction_0_XXXX` normalised;
  `set_filename_1E2FC`-style duplicate placeholders paired with the bare/`__Fv` name; nameless
  `fn_<mod>_XXXX` linkonce blocks paired with our bytes at the same distance from the previous named
  function with external `bl`s treated as equal; `GXWGFifo` relocs folded to the absolute word;
  `MCMP_CONTENT=1` keys `.rodata` references by the 12 bytes at the target (only for units whose `.rodata`
  still differs — for equal sections it produces false diffs where a reloc word falls inside the window).
- Sync hazard seen: `sync_rel_symbols.py build/G4BE08/src/t_id/t_id.o` renamed the DOL's `ScreenReSize`
  to `ScreenReSize__FUsUs` (main_sub.h declares it C++) and every other module's split object still
  imports the placeholder -> `make_rel: undefined symbol ScreenReSize`. Reverted by hand in symbols.txt
  + sym_map.tsv; t_id.cpp keeps the mangled reference (it is not linked). Namespace functions are not
  synced at all (the .sym demangles them as `ns::f`, sync matches by bare name): t_lightarea.cpp now
  lives in `namespace t_lightarea_namespace` like the original (`IsWorkAlive__21t_lightarea_namespaceP10LIGHT_AREA`
  etc., renamed by hand); the t_esp_area copies are plain `IsWorkAlive__FP8ESP_AREA`. Two units of one
  module with the same function name (`OptionExec__Fv` in t_esp_area vs the namespaced t_lightarea one)
  must not both get the same symbols.txt name. Hand-renamed data labels: Tools `_vt.10cDbgWindow`/
  `_vt.10cDbgButton`/`_vt.14cDbgWindowBase`/`_vt.14cDbgButtonBase` (db_toolbase's copies, 0x24B0..0x2560),
  `_vt.5cUnit`/`_vt.t8cVarLoop1ZUc` (db_light's, 0x1608/0x15E8), t_esp `dbModSlot` (.bss 0xE8), t_light's
  t_scroll `menu__Fv`..`printEditTable__Fv` (all `scope:local`, duplicates of t_light.cpp's).
- db_port.cpp (t_esp; 61/68, .rodata/.data/.bss equal, STRIP_UNUSED added for two never-called helpers
  whose pools survive: `DB_DrawPoint` [0.0001, 1.0] between MakeCol and DB_DrawBox, `DB_VecClear` [0.0f]
  after DB_DrawCross3D — the target's 8-aligned double magic needs exactly those words):
  - sp_PosRand_trans_1a: `Vec v = {0.0f, 0.01f, 0.0f}; Mtx inv;` declared MID-function (after the first
    PSMTXMultVec) = the 12-byte template copy at that point and frame order a, b, ofs, v, inv.
  - sp_ctrl01_trans: frame ry, rx, base, dir, p; NO pointer locals — `&gen->pos`, `&p`, `&dir` written at
    every use (gcse PRE copies them at the arm ends: `mr r27,r30` / `mr r27,r9`); `half`/`ay`/`ax` assigned
    as plain products first, then the three LIMIT_ANGLE calls; `(f32) i` on the `u32` counter (2^52 magic
    without 0x80000000); `dir.x = 0; dir.y = 0; dir.z = 1500` in natural order (the last-dying-store rule
    then gives y, z, x).
  - sp_path_trans / sp_path_trans2: the DbPathEsp payload is a sub-struct `DbPathWork w` at 0xF8 addressed
    through `pw = &pe->w` (`addi r31,r30,0xf8`, computed before the SetFreeWork virtual call); `esp` is
    read once into `pe` and every later use (incl. `PushEsp(&pe->esp)`) goes through it, so the pointer is
    never reloaded from its stack slot; the seed is an in-struct store (`struct { u32 v; } seed`) so the
    virtual call's vptr load waits for it (a plain scalar store is exempt from aliasing a varying in-struct
    load); `PathGetPosEmM(path, em, t, ..)` = the pl0e COMPILER-DIFF #1 alias (`mr r4` before `fmr f1`);
    trans2: `Vec rot; Mtx rm;` declared mid-body after the PathGetPos calls (seg's `&` slot precedes them),
    `EspgenWork* pw = &wk` used for memclr and SetFreeWork (the cse'd address survives in r28), `rot.x, rot.y,
    rot.z` in natural order.
  - sp_PosRand_trans: `Mtx m` declared FIRST — its address is the frame pointer itself, so each of the eight
    `PSMTXMultVec(m, ..)` gets a fresh `addi r3,r1,8` instead of one cse'd callee-saved pseudo.
  - DB_VecNullPartsPos / DB_VecMulEmPartsMat: the shared tails are source-level duplicates (`SET_MTX_POS`,
    `NONE_BODY` macros) that jump2 cross-jumps; `if (parts < em->nParts) {..} else {none}` lays the none body
    after the tail; in VecMul the two arm copies are inline bodies and the parts check is `goto none` with
    the label at the end (that is what gives `bne ok; b none` in the arms). The `cModel** tbl = EspEvModList`
    local puts the `lis/addi` before the compare.
  - DB_GetCursorPos passes `parts` (not `(u32) head`) to DB_VecMulEmPartsMat: `lbz r3,7(r4)` is the argument
    register, `mr r0,r3` the displaced head. DB_GetCamFrontPos takes `(f32 dist, f32* x, y, z)` (the fmr
    copy is first); t_esp.cpp's declaration/calls follow.
  - sp_tex_trans: `int tno = no; asm("" : "+r"(tno)); u32 n = (u8) tno;` (COMPILER-DIFF #2: one `clrlwi r28`
    at entry, reused for both calls), `GXTexObj* o = &obj; GXTlutObj* t = &tlut;` right after the tpl check
    (both addresses before TEXGet), `tex->textureHeader->..` re-read per field (no hdr/clut locals), the
    9th GXInitTexObjCI argument is 1, and `asm("" : "+r"(r)) // COMPILER-DIFF candidate #12` in the
    `owner == 1` arm so `b = 0.2f` reloads the pool instead of copying r. Residue 8 words: the entry-block
    `lis` order (the target issues high(0.8) first; ours has a gcse PRE insertion of high(0.8) for the
    tail's MakeCol alpha at the end of bb0).
  - drawTexture2 colour bytes `a, b, g, r` (source order = issue order for the 4-byte ADDRESSOF local);
    EspToolExit `volatile debugCamera* dbg = &CamDbg` + `*(u32*)&col = 0` written BEFORE `dbg->target_type = 0`
    (one shared zero, stb issued first); DB_ConfigLoad's table is `DbConfigModel tbl[10]` (memclr 0x410) and
    `num = 0` is assigned after the read check.
- Reload-cse copy shape (db_widget SetBase/SetSize, OPEN): `stfs f2,0x40(r3)` first, then `fmr f0,f2` for the
  re-read of the just-stored member = cse did NOT forward the store to the load (the load stayed a MEM
  through sched1, so the store outranks the size.x/size.y loads) and reload_cse_regs turned it into a copy.
  Ours forwards at cse1 (`stfs f2,4(r9)` directly); no member/pointer form found that keeps the load.
- IKreport (db_mod): `if (info & 0x30) { type = 0; if (info & 0x20) type = 1; if (info & 0x80) type = 2; }`
  (the 0x80 test is inside the 0x30 block); the target's `mr r11,r0` copy of `info` for the third test is a
  gcse PRE copy that no local/cast/macro form reproduces (combine removes the redundant extension).
- Do not flip a unit whose module symbols.txt got a `scope:` change: a global's ADDR32 field is A, a
  local's S+A, so changing a target symbol's scope changes the split object and the REL check.

### Tool RELs, bytes-first pass 2 (t_rck 13->28/33 + .rodata/.data equal; db_light 122->128/134 in every module; db_widget 95->102/113; t_snd_vol 13->19/27; t_event 56->61/69; t_id 48->52/69; 2026-09-10)

- Harness: /tmp/tools_p2 (copies of /tmp/tools_p's mcmp.py/tryv.py/vapply.py/rodiff.py/mdump.sh with the paths
  rewritten). Nothing flipped: no unit reached byte-identity.
- t_rck .rodata/.data (src/Tools/t_rck.cpp): the menu string arrays (`menu_str`, `save_str`, `clear_str`,
  `load_str`) are file-scope statics DEFINED right before the mode function that uses them (their strings are
  emitted at the definition, so they land between the previous function's pool and the next function's own
  strings), the six `GXColor` statics after `load_str`, and the `asm(".section .data; .balign 8")` pad after
  the LAST .data object (at the top it padded `.data` between `menu_mode` and `menu_str`). A `.data`
  "masked words equal" verdict says nothing about pointer-array positions (their words are all relocs).
- `(f32) (s8) x` is `psq_st qr4` + `stb/psq_l qr4`; the target's `psq_st qr4; lbz; extsb; xoris 0x8000;
  stw; lfd; fsub` (s8 quantisation, then the CLASSIC signed conversion) needs an `int` operand the front end
  cannot see through: `int t = (s8) (...); (f32) t * 500.0f` (`(f32)(int)(s8) x` is folded back). The
  `xoris r9, r0` (untied from the `extsb r0`) came from a multi-set function-scope `int t` shared by the two
  expansions (macro `RCK_GRID_Y`), not from the inline's own local (t_rck rckPointAdd).
- Two `RckLine*` pointer locals in one loop body (`a = &RCK->line[n][i]; b = &RCK->line[i][n]`, each pair
  reloading the struct-member work pointer) alternate r9/r11 per pair; ONE reused pointer local `l` gives the
  target's (r9, r11) for both (rckPointAdd). A `switch` over a u32 mode whose cases 1 and 2 share a body:
  `cmpwi 1; beq; cmplwi 1; blt; cmpwi 2; beq; cmpwi 3; beq` = two identical case bodies (separate tree
  nodes, cross-jumped), not `case 1: case 2:` (mode_main).
- Read the copy kind off the target: `lfsx/lfs; stfs` = memberwise `c.x = p->pos.x; c.y = ..; c.z = ..`,
  `lwz/stw` = a struct copy (`q = p->pos`). `lwzx rD, rW, rOfs` + `add rP, rW, rOfs; lwz 4(rP)` for
  `pt[RCK->cur].pos` is a pointer local `RckPoint* pt = &RCK->pt[RCK->cur]` (the plain array copy folds
  `0x2d4` into the displacements). Frame order follows declaration order (`Vec out; Vec c;`).
- `psq_st .., qr5` = an `(s16)` conversion (qr3 = u16). A u16 member stored from it must be stored
  directly (`ab->len = rckLineLen(pa, pb); ba->len = ab->len;`): a `u16 len` local is promoted and gets a
  `clrlwi 16` after the `lhz` (the fpmem load pattern is opaque to combine). The then-arm `ab->len = 0;
  ba->len = ab->len;` gives `li r0, 0` + the cross-jumped `sth; sth` tail (rckPointLineEnd).
- Reference-store idioms that mattered again: `RckWork*& wp = rckWork.p; wp = Debug_alloc(..)` (both
  `lis` hoisted before the first call: ToolRctRouteCheck, t_snd_vol init), `int zero = 0` at the function
  top + `pGS->pRoomRtp` for both pG reads after work stores (rckInit), `if (...) { .. } else { flags &= ~1; }`
  in BOTH arms so the two copies cross-jump and the entry arm reuses the already-loaded pointer
  (rckPointCatch), `int no = RCK->hdr.nPoint - 1; RCK->near = no;` for the compare on the stored value.
- rckMakeEditData: `RckLine l; *(u32*) &l = *(u32*) p;` (address-taken 8-byte local: `stw r0, 8(r1)`, `to`
  read back as `srwi 16` of the register, `len` as `lhz 2(r7)` from the frame) plus a `RckLine* d =
  &RCK->line[i][l.to]` pointer for the two stores. `dx*dx + dz*dz` = the target's standalone `fmuls dz`.
- t_snd_vol getInfoData (the 5 giv increments at the loop latch in the target, hoisted into the arms in
  ours): interblock scheduling did the hoisting (`-fno-schedule-insns` gives the target order), and it stops
  once the loop has ~6 more LUIDs. No natural form added them; six dead `int padN = i` initialisers tagged
  `// COMPILER-DIFF: 5` do. The sel copy is `memcpy(&work->sel[i], (u8*) hdr + ofs, 8)` (`lwzx` word 0,
  `lwz 4(add)` word 1), the curve copies compute `u32 size = t->num * 8 + 8` BEFORE the memcpy (size loaded
  before the work pointer).
- t_snd_vol: `s8 c = work->menuCur; switch (c) { .. case 5: case 6: work->mode = c; }` = one `extsb` and
  cross-jumped case bodies (a re-read `work->mode = work->menuCur` re-extends per arm); `*(u32*) &gray =
  0x80800080` (r, g, a = 0x80, b = 0 — read the constant off the bytes, the "gray" name lied);
  `(s16) (curDist * 2.0f)` (qr5) added to an s16 x; markDraw: `if (x < 0) return; if (x > 13) return;` (the
  `||` range-folds to `cmplwi 0xd`), `switch ((u32) kind)` for the `cmplwi 1; blt` node, `blink_r`/`blink_dir`
  statics read DIRECTLY (QI pseudos: `clrlwi 24` at the int uses, one `lbz` each, `extsb` only at the
  compare); editDataLineDraw: `(s16)` casts everywhere, `dv = ((f32) e[1].val - (f32) e[0].val) / (d1 - d0)`
  before `v = (f32) e[0].val` (e[1] converted first), declaration order `base, y0, y1`, `pt` stores in the
  order x0, x1, y0, y1, z0, z1 (brute-forced).
- Address-taken u32 parameter spilled at the function top (`stw r4, slot` right after the first `lis`,
  ours late): the target's `work->left` read is a REFERENCE read (`IRef(work->left)`, MEM with neither struct
  nor scalar flag) that may alias the frame store, so the store outranks it in sched1 (markDraw,
  editDataLineDraw — 11 and 26 words each).
- editScreenDisp: `S16Vec pt[3]` (the extra 8-byte frame slot: a copy-pasted over-sized array from
  mainFrameDisp; unused `GXColor`/`f32[2]` locals take no slot) and `(s16)` casts; OPEN: ours gcse-PREs
  `base - 4` above the two loops (reg 364 in the .gcse dump, "redundant insn 671 in bb 28") where the target
  computes it after them from a `mr r10, r23` copy of `base` (83 words, register naming follows).
- t_id (src/t_id/t_id.cpp): `SctrlInitAxisRange(w, xr * 1.2f, xr * -0.2f, yr * 1.2f, yr * -1.2f)` with
  `f32 xr = 90.0f; f32 yr = 2.0f;` LOCALS — the original computes the products at run time (`fmuls` from
  pool words 90, 2.0, 1.2, -0.2, -1.2); literal products fold (108.000008/-18/2.4/-2.4 in ours). The
  cursor-guide helper `idDrawGuide` is a MACRO (`ID_DRAW_GUIDE`): its `"(%3.0f, %3.0f)"` string sits after
  idEditPos's "Frame"/"Param" (macro text is expanded in place, an inline body is compiled at its definition
  and emits its strings there) and the pool loads keep RTX_UNCHANGING_P (pl0f lever): idEditPos 609 -> 364
  words, .rodata order fixed. `(s8) w->x17B != w->type` loads x17B first (toolIdFile). OPEN: idEditSize's
  `for (j = 0; j <= 2; j++) { if (j == 0) .. axisName[j] }` — our loop.c eliminates the biv `j` through the
  `&axisName[j]` giv (compares become `cmpw rP, &axisName + 4k` and `&axisName` is spilled to an anonymous
  .rodata word at 0x444, the only remaining .rodata diff); the target keeps `j` (`cmpwi r29, k`, `lwzx
  r8, r28, r23`). Pointer/u32-base forms of the access do not change it.
- t_event: the four `_._` destructors were naming only — db_toolbase's vtable copies at .rodata
  0x1958/0x19A8/0x19C0/0x1A08 are now `_vt.10cDbgWindow`/`_vt.10cDbgButton`/`_vt.14cDbgWindowBase`/
  `_vt.14cDbgButtonBase` in config/G4BE08/modules/t_event/{symbols.txt,sym_map.tsv} (like Tools). mcmp's
  `fn_t_event_1B528: 238 words` is a pairing artefact: the nameless cManager<cLight> block is paired at the
  distance from CallbackLoad, whose size differs by 0xC. RunStop: the `li r0, 0; sth` fresh zero (ours
  stores the `andi.` result cse knows to be 0) resisted the launder, `S16Set`, `do {} while (0)` and a
  local-copy form (21 words, OPEN).
- db_light (src/tools/db_light.cpp): `spotRot` is a FUNCTION STATIC of `edit_light_type_spotlight` (the
  unit's first function with a static), which is why it is the first .bss object; the .sym's "global" scope
  for a label at .bss+0 is meaningless (S+A = A when S = 0). A file-scope `Vec spotRot;` is deferred behind
  every function static (ours had it at 0x90); `Vec spotRot = {0,0,0}` goes to .data. Fixed six functions
  in all five copies at once (editColor 398 -> 120 words); touch db_light_tools.cpp/db_light_esp.cpp after
  editing the shared source (no dependency tracking for the wrapper units).
- db_widget `DB_NUMERIC::SetNumPointer` x7: `numType = ..; keta = ..; max = C; min = C; pNum = p;` — pNum
  LAST. Every store is a dying store, so the last one in RTL (`pNum`) is issued first and the rest follow
  RTL order (`stw pNum; stw numType; stw keta; stfs max; stfs min`); the `const f32 mx` pool trick is not
  needed once `max` is assigned before `min`.
- t_atari: naming artefact confirmed — `fn_Tools_1E028` (8) = the unit's `beginEvent__5cUnit` +
  `endEvent__5cUnit` linkonce copies, `fn_Tools_1E050` (0x4C) = `_._5cUnit` + `__dl__5cUnitPvUi`, and
  `__static_initialization_and_destruction_0_1DEF0` is the module-level duplicate of db_mod's name (a
  symbols.txt cannot hold two `__static_initialization_and_destruction_0`); the only real diff is plmove10
  (19 words, the OPEN copy-store order). t_camera_data / t_dr `.rodata` "differ" only by the trailing
  8-alignment pad word before the next unit.
- OPEN this pass (one try each unless noted): t_rck rckMakeSaveData (75: `p` and `buf` split into two pseudos
  because sched1 hoists `p += 0x18` above the header stores — `memclr_asm(p, size)`, memcpy/typed-pointer
  header copies and a chained `o` offset variable did not merge them), rckDrawPointLineNow (7: the
  `line[near][lineStart]` address is `w + (near<<9 + 0xad4)` then `lhax .., ls<<2` in the target, ours
  folds `0xad4` into the base; row-pointer, u32 and local-index forms tried), rckPointDelete/rckSetNextPoint/
  rckDrawPointLine untouched; t_snd_vol editDataDraw (2: `lha val` before `lfs dist` for markDraw's args,
  COMPILER-DIFF 1 family), edit_reverb_param/file_save/data_edit/combine_* untouched; t_se_at seAtInit (21:
  the `lwz pW` for the camera copies stays below the `Snd.se_at*` stores in the target although both are
  fixed-address in-struct MEMs; typed reference setters did not order them); t_id idEditRot/Size/Color
  (230-340 words each, structure aligned, not iterated); t_camera tcNextAdatPtr (2: `mr r4, r3` before the
  hoisted `cmpwi cr7`).

### Tool RELs, bytes-first pass 3 (db_toolbase t_event sizes fixed; t_sce_item 39->41/43, t_block 25->27/31, t_dr 14->15/16, db_light 128->129/134 x5, db_port 61->63/68 + .rodata re-equalised; nothing flipped; 2026-09-10)

- Harness /tmp/tools_p3 (tools_p2 copies with the paths rewritten; `ndump.sh MOD/UNIT -dFLAGS` compiles a unit with its
  build.ninja cflags and leaves the RTL dumps in /tmp/tools_p3/dump; `tryvh.py` = tryv with `HDRS="include/a.h include/b.h"`
  header variants, tuples `(hdr, old, new)`). tryv counts are lower than mcmp's (no fold_linkonce); judge per function.
- UNIT_CFLAGS is keyed per MODULE unit: `Tools/db_toolbase.cpp` had `-fimplement-inlines` but t_event's copy of the same
  source (`t_event/db_toolbase.cpp`) did not, so its object lacked `cDbgWindow::Init` (0x9f0 vs 0xa60). Every shared source
  needs its flag entry in every module that builds it.
- Clamp-with-reload idiom (t_sce_item / t_block ListDisp, t_dr ListDisp, three units): the target reloads the work pointer
  (`lwz r9, pW@l(r31)`) and `lha listTop` for the eprintf arguments after the `n = listTop + 7; end = n > 128 ? 128 : n`
  diamond, and ties `lha r9; addi r9,r9,7; mr r29,r9; cmpwi r9`. Our jump1 rewrites `if (c) end = 128; else end = n;` into
  `end = n; if (c) end = 128;` BEFORE cse1, so cse1 skips the block and carries the pointer over the join. Write the else
  arm as a RE-READ of the expression (`if (pW->listTop + 7 > 128) end = 128; else end = pW->listTop + 7;`): jump1 cannot
  hoist a load, cse1 follows the taken branch into the else arm (folds the re-read to n) and STOPS at the join label, the
  block after the join starts with an empty table (pointer reloaded), and the jump pass after cse hoists the copy. 0 words.
- dispItemSetList1: one `int col` set 0/6 in two places (two eprintf calls) is a global pseudo that outranks the element
  pointer `a` for r29; a second variable `col2` for the second block gives the target's `li r29,0/6; clrlwi r29,r29,24`
  (col2 dies at the mask and is tied to it) and puts high(pW) in r29, `a` in r27.
- db_light edit_light_id_shadow: the COMPILER-DIFF #2 launder must be `asm volatile("" : "+r"(c))` — the plain asm let
  sched1 issue the `clrlwi` one call later (the flrAtDataLoad rule holds for statement-level launders too). edit_cutsel:
  `u8 line = i + 6` reproduces the target's `addi r0,r28,6` + copy shape with `clrlwi` where the original has `mr r30,r0`
  (COMPILER-DIFF #4 in reverse: the original does not mask a narrow local store); `int line` folds the copy into the addi
  (6 words vs 1). No launder form (`asm` on the temp, `"=r"/"0"`) yields the plain copy.
- db_port: `symbolOnOff` must be a MACRO — as an inline its "ON"/"on" strings were emitted at its definition, before
  "ESPTOOL:open error!" (the .rodata had drifted since the pass-2 commit). SeqSet: the em27 `#13 (int shape)` recipe —
  a function-scope `int zero = 0` set in the first block and used only as the EstSet stack argument in the call block —
  gives the target's `li r0,0; stw r0,0xc(r1)` right before the store (10 -> 5 words; the rest is the `on`/andis r9/r11).
- db_widget DB_NUMERIC ctor: the target stores 2 (`DB_NUM_FLAG_NO_SELECT`) into numFlg before `SetNumFlg(0)`, after the
  nameTbl/nameNum zeros, and `type` before `flag |=` — block 1 exact (13 -> 7 words). Block 2 (9 constant stores after
  SetNumFlg) is the #13 dying-store shape: the target issues it in pure source order although `zero`/`1.0f` die at their
  last stores; ours hoists `edit = 0` and `step = 1.0f`. Neither the emrock `asm volatile("" :: "r"(zero))` nor keeping
  every constant (`3`, `255.0f`, `1.0f`, `0.0f`) live reproduces it: with all constants live ours issues the stores of
  freshly loaded values (`li r0,3`, `lfs f0/f13`) before the r29/f31 ones. Left at 7.
- cDbgWindow::Init (db_toolbase, 13 words, the same shape in FileSelect/OkCancel Init and t_esp_area
  `Init__20cDbgFileSelectWindow`), mechanism found but no admissible form: our first-issued store is the LAST RTL store
  because it carries the deaths of BOTH the zero pseudo and `this` (weight -1); the target's order (w, cyMax, pName, h,
  cxMax, then the six zeros in RTL order with pTop last) is reproduced EXACTLY by keeping both live past the block
  (`asm volatile("" : : "r"(this), "r"(zero))`), which then only leaves `li r0,1; li r9,0` (ours `li r0,0; li r9,1`: the
  zero has 7 dependents and is ranked first). The zero is REG_EQUIV 0 (`-dl`), the `1` REG_EQUIV 1, `this` has no note —
  why the original keeps `this` live is not understood; `register int z asm("r9")`, `int zero` at the top / after the
  call, `w = strlen(name)` last, and a use of `this` alone (10 words) do not reach it. Do not retry statement orders.
- Skeleton-compare rule refined (msq_R0_QuitCk, t_esp Save*FileNoUpdateCallback x5): the target keeps `cmpwi 0; beq L;
  cmpwi 1; L:` / `cmpwi 0; cmpwi 0xff` of arms that set the same constant. Ours: with both arms identical the whole switch
  (and the `lwz sub3`) is folded before flow; a `register int m asm("r0")` + one live set folds it too (only FindCk's
  distinct-value arms survive). The Save* pair dies in jump2: `delete_jump` of the jump-to-following `ble` deletes its
  compare through the cr0 REG_DEAD note (delete_computation) — the original's jump2 keeps the compare. Compiler-side.
- One try each, unchanged (documented ties): t_atari plmove10 (copy stores after the RMW stfs: `memcpy`, pointer, mid /
  after / declaration-initialiser copies, precomputed deltas — the copy `lwz`s outrank the RMW `lfs`s in ours because
  the copy stores carry output deps to the frame stores), t_mv mvInit (cse1 processes the fallthrough arm of a taken
  `bne` with the entry table on the re-walk — path re-scan with the last branch NOT_TAKEN — so the literal 0 becomes the
  `zero` register; the original's arm gets a fresh `li r10,0`: cse path difference, the same family as edit_select_sub /
  r104 execEvent00), t_dr Menu_main (high(DR) r30 vs `a` r28: priorities 0.146 vs 0.167 from `-dl` refs/lengths;
  a-first, `int no` local, `DrWork* d` forms unchanged), t_block tBlockAreaInfo_Menu case 0 (rep2 r9 / n r11 swap; rep
  local, m clamp, assignment forms), t_block dispAreaInfoList1 (`li r30,7` speculated into the test block, #5 family),
  t_se_at seAtInit (the `lwz pW` below the Snd stores: a `do {} while (0)` barrier keeps it below but also pins the
  `addi &g->Cam.param.pos` and `li r5,2` that the target computes inside the Snd block; an unknown-base `SndWork* s`
  launder makes the load wait but frees the save stores — the target needs a dependence of the pW load on the four Snd
  stores only; store order `Snd.se_at = 0; Snd.se_at_list = 0;` gives the target's 0x98-then-0x94), t_se_at ToolSeAt
  (`lis seAtWk@ha` before `mr r0,r3`), db_port DB_VecMulEmPartsMat (no/tbl/p r0/r11/r9 naming; if/else and ternary
  forms 19 words), db_light edit_light_parent (`n` r8/r10 and the case-2 `+101` temp untied: `u32 t`, `u16 hi`, `u32 hi`,
  `n + x` forms unchanged, a `t` shared by both arms 79), draw_light_graph (`col` in r5 = a hard-reg preference the
  original got from passing `col` unmasked in r5 somewhere; `col = 0; eprintf(.., col, ..)` before the second block and
  the ColU8 inline launder give 22-44 words, `asm("" : "+r"(col))` on the u8 drops the mask).

### Tool RELs, bytes-first pass 4 (db_toolbase Matching in Tools + t_event; t_dr Matching; t_block 27->28/31; 2026-09-10)

- Harness /tmp/tools_p4 (tools_p3 copies with the paths rewritten; `tryv.py` accepts an absolute `SRC=`, `mcmp.py`
  honours `OBJ=` for a variant object, `tryvh.py` puts the variant dir first on the include path -- shadow EVERY
  header on the include chain (`HDRS="include/dbg_tool.h include/db_toolbase.h"`) or the edited header is not seen,
  and pass the unit's UNIT_CFLAGS through `EXTRA=-fimplement-inlines`). `duplis.py` scans the target asm for two
  `lis` of one symbol in one block: 2299 sites, a normal PRE reaching-reg / loop.c shape ours reproduces (db_light
  `menu`), so a second `lis` in bb0 is not a difference by itself.
- **#13 mechanism, sharpened (read off the dumps of cDbgWindow::Init, DB_NUMERIC ctor, Menu_main, basic_menu)**:
  the original's movsi/movsf/movqi predicates accept `(set (mem) (const))`, so cse substitutes a known constant
  straight into every store (ours keeps the pseudo: `gpc_reg_operand` rejects it). Consequences seen in the tool
  targets: (a) a constant store carries NO register death, so an init block of constant stores is issued in pure
  RTL order while ours hoists the last store of each shared constant (cDbgWindow/FileSelect/OkCancel Init, the
  DB_NUMERIC ctor's 9 stores, emrock SetRock); (b) the pseudo's `li`/`lfs` is gone from sched1 and reload
  re-creates it before the first use, so the target's constant loads are in sched2/LUID order (`li r0,1` before
  `li r9,0` in cDbgWindow::Init; the ctor's `lis/lfs f13/li r0,3/lfs f0` hoisted to the block top), and reload's
  `find_equiv_reg` reuses a register already holding the value (the ctor's zero stores keep the allocated r29,
  its `min = 0.0f` keeps f31); (c) `(high sym)` IS CONSTANT_P in 2.95 (rtl.h), so a set-once `lis` pseudo gets
  `REG_EQUIV (high sym)` in `-dl` and update_equiv_regs DOUBLES its REG_LIVE_LENGTH (local-alloc.c 2.95.3: "*= 2",
  the global.c comment about negative lengths is from the older negating version), halving its global-alloc
  priority in ours; the targets allocate the high pseudo ahead of a 6-ref local (Menu_main r30 vs r28), i.e. the
  original's high pseudo did not carry the penalty (its cse turns the PRE copy sites into separate expressions).
- **Region-split recipe for the #13 dying-store shape** (`// COMPILER-DIFF: #13`, cDbgWindow::Init 13 -> 0, unit
  flipped in Tools and t_event): dead `do { } while (0);` statements split the sched1 region so that (1) the last
  store of the shared zero sits alone at the end (no hoist) and (2) the `li 1` outranks the `li 0` in the first
  region by dependents (3 vs 2: put the barrier after the second zero store). FileSelect Init (dbg_tool.h, 18 words
  in t_esp_area/t_lightarea/t_event) with barriers after `pCur = 0` and before `fileNo = 0` gets the exact store
  order but swaps the constant registers (target one=r9, zero=r0) -- not applied. DB_NUMERIC ctor: the barrier
  form would keep the `lfs/li` in the second region while the target has them at the block top -- not applicable.
- **Global-alloc order through REG_N_REFS** (t_dr Menu_main 12 -> 0, unit flipped): `a` (`&DR->area[no]`) had 6
  refs because the clamp stored `a->type` in BOTH arms (two `stb` insns until jump2 cross-jumps them); writing the
  clamp into an `int n` and storing once gives 5 refs (floor_log2(5)*5 = 10 vs 12) and the high(DR) pseudo is
  allocated first (r30, then `a` r28). Count RTL-level refs of the pre-jump2 code when a callee-saved pair is swapped.
- **jump1 single-insn hoist blocked by an intervening jump** (t_block dispAreaInfoList1 3 -> 0): `if (c) col = 7;
  else { col = 0; if (d) col = 6; }` lets jump1 move `li 7` above the branch (scan from the else label finds `col`
  set before any use); `if (c) col = 7; else if (d) col = 6; else col = 0;` (or a ternary) keeps `li r30,7` inside
  its arm: the scan hits the inner condjump first, and the inner if/else is the jump1 `x = 0; if (d) x = 6` form.
- basic_menu (t_sce_at, 215, OPEN): per-site asm aliases (`extern SceAtWorkPtr sceAtCur_N asm("sceAtCur")`, one per
  fresh-`lis` site, plain symbol at the bb0 load and the four `r27` sites) give 29 words -- proof that the
  target's fresh `lis r9` sites are occurrences neither cse1 nor PRE merged; the residue is the PRE-copy sites
  `P = r27` whose same-block use the target keeps on r27 while our cse2 rematerialises `lis P` (cse.c COST:
  pseudo 1 > HIGH 0 with the REG_EQUAL note pre_delete leaves on the copy). 22 tagged aliases for a non-flip: not
  applied; the r108/r203 alias lever and this one are the same family (#12/#13 PRE-copy handling).
- plmove10 (t_atari, 19, OPEN, mechanism): the `old = w->pos` copy loads tie with the RMW `lfs` at priority 9 and
  win by LUID; their priority comes from `old.x`'s frame-direct store (`stw r0,8(r1)`, cse's find_best_addr
  rewrite of the offset-0 word) having output dependences on the `4(r30)`/`8(r30)` stores because the block
  move's `addi r30,r1,8` has no REG_EQUAL note (cse adds it only when `(plus fp 8)` is already in its table), so
  alias.c cannot relate the two address forms. `asm("" :: "r"(&old))` / `Vec* po = &old` before the copy: 58-61.
- seAtInit (t_se_at, 21, OPEN): an unknown-base pointer (`SndWork* s = &Snd; asm("" : "+r"(s));`) with the
  seAtSaveHead/List stores written BEFORE the two NULL stores reproduces the target's `lwz pW` below the Snd
  stores (21 -> 9); the asm insn delays the two Snd loads by one slot and swaps the head/list registers. Volatile
  Snd views (5 forms) 21-22. The original's Snd base is unknown to alias.c by some other route.
- loadItemIdName (t_sce_item, 28): `p` r31 / `no` r30 allocation order; one-variable `e`/`no` forms (u32, pointer,
  in-place `+= len; -= 0x40000`) 28-29 -- the sys arm's `add r30,r30,r29; subis` is a reassigned variable but the
  `li r0,0` lands after the `subis` in ours. t_block tBlockAreaInfo_Menu case 0 (rep2/n r9-r11): rep-local first/
  after, clamp variable, if/else-if, u8 n -- 11-52. t_mv was flipped and t_event RunStop fixed by other agents.

### Tool RELs, bytes-first pass 5 (cDbgFileSelectWindow::Init closed in t_event/t_esp_area/t_lightarea: t_event 62->63/69, t_esp_area + t_lightarea 36->37/38; db_widget DB_NUMERIC ctor 7->2; 2026-09-10)

- Harness /tmp/tools_p5 (tools_p4 copies with the paths rewritten; `minivar.py BASE.cpp VARIANTS.py [START END]` compiles
  variants of a stand-alone mini source with the module flags and prints an objdump slice, `minidump.sh FILE.cpp -dX` = cc1plus
  dumps of a mini source; `tryvh.py` needs `EXTRA=-fno-implement-inlines` for t_event/Tools units).
- **cDbgFileSelectWindow::Init (dbg_tool.h, 18 -> 0 in all three users) = the cDbgWindow::Init region-split recipe with THREE
  dead `do { } while (0);` (before `pCur = 0`, before `pPath1 = path1`, before `fileNo = 0`)**, no register pins: the first region
  must leave the zero <= 3 dependents (`li r9,1` then ranks above `li r0,0`), the second keeps the dying pPath1/pPath2/pExt
  stores from passing `pCur`/`fileName[0]` (a byte store is NOT a barrier for same-base word stores: alias.c separates
  `this+0x248` from `this+0x23c`), the third keeps the last zero store last. Brute force over all 2- and 3-barrier placements
  (v_fs2.py, 55+165 variants, ~50 s): no 2-barrier placement reaches 0 (best 2 words); hard-register pins (`register int one
  asm("r9")`) were unnecessary once the regions were right. Shared header: verify every unit that constructs the window
  (t_event/t_event, Tools/t_esp_area, Tools/t_lightarea; db_toolbase unchanged).
- **#13 init-block launder without a later block (db_widget DB_NUMERIC ctor, 7 -> 2)**: when nothing separates the block from the
  next call and return, `asm("" : "=m"(field) : "r"(zero), "f"(one))` with a MEMORY output on a field the block does NOT store
  (`def`, stored in the first block) keeps the dying constants alive with no scheduling barrier and is never deleted by flow (a
  store); an output on a field stored in the block boosts that store's priority (`"=m"(edit)` hoisted the `edit = 0` store).
  The tied form `asm("" : "=r"(t) : "0"(this), ...)` + `t->SetDefault()` gets the same store order but delays `mr r3,r30`
  behind the asm (its `lfs` input) in sched2. `register f32 one asm("fr13"); one = 1.0f;` assigned AFTER `max = 255.0f` gives
  the target's pool order (255 before 1.0) and the reload spill register (1.0 -> f13, 255 -> f0). Residue 2 words: the target
  issues `fmr f1,f31` (the SetDefault argument) in the cycle of the FIRST store, ours one store later — in our sched2 the
  `fmr` ranks below every store of the block (ready list `... 181 205 169 165 161 154 150`), the target's ranks it second;
  not understood (`SetDefault(min)`/`(zf)` argument forms unchanged).
- **Save*FileNoUpdateCallback x5 (t_esp, 5 words each), mechanism corrected**: the second dead compare is NOT deleted by
  jump2's delete_computation (with `reload_completed && flag_schedule_insns_after_reload` it deletes only the jump — jump2
  runs after sched2 in this toplev.c) but by flow2: `find_basic_blocks` after flow1 deleted the dead `type = 0` store finds
  the `cmpwi 0xff; ble L2` block falling through into L2, `tidy_fallthru_edge` deletes the `ble`, and life_analysis deletes
  the dead cr0 set. The first compare survives because `b L2` (the then arm's jump) sits between `bge L1` and L1 (two
  successors), and jump2 later deletes both jumps as jumps-to-following. A third dead arm (`else step = 0;`) keeps the
  second block two-successor, but jump2's jump-around-jump inversion turns `bge L1; b L2; L1:` into `blt L2`, which is no
  longer to-following once `cmpwi 0xff` survives (`cmpwi; blt; cmpwi`, 3 words). So the original's flow2 kept the `ble` block
  (or its jump2 processed the inner `ble` before inverting the outer branch): compiler-side, one more form tried
  (`if (type < 0) type = 0xFF; if (type > 0xFF) type = 0;`, `||`, nested, int/s16, dead-variable arms: all keep 0 or 1 compare).
- **gcse PRE deletes a fully redundant occurrence only if its block is not "isolated"** (lcm.c: `isoin = latein | (isoout &
  ~antloc)`, `isoout = AND of successors' isoin`, `isoout[last] = 0`; `redundant = antloc & ~(latein | isoout)`): an occurrence
  with no LATER occurrence on any path to the exit is never deleted, whatever the availability (mini_pre.cpp: `a = U8(raw); ..;
  return U8(raw) & 0x80` -> "0 substs"). IKreport's target (`clrlwi r0; mr r11,r0` in the first block, `andi. r0,r11,0x80` in
  the join) is a PRE insertion+copy for the third test's recomputation, which our gcse never performs here; the third test also
  needs (a) the first computation as a single SET — `int info = (u8) raw` / an inline `IkU8(u16)` gives `zero_extendqisi2`,
  while `raw & 0xFF` is the `andsi3` PARALLEL with a CC clobber that gcse's hash_scan_set ignores — (b) SImode arithmetic
  (`(u8) raw & 0x80` is shortened to QImode by the front end; `(int) (u8) raw` / `IkU8(raw)` keep it) and (c) a dead
  `do { } while (0);` at the join-block top so cse1's AROUND path (block 2's `beq` over `li r8,1` into the join, label with one
  use) does not fold the recomputation into `info`. With all three the occurrence survives cse1 and PRE still reports 0 substs
  (isolated). Left at 10 words; the #12 taken-branch/AROUND family.
- **haifa tie at a call return (t_camera tcNextAdatPtr, 2 words)**: `mr r4,r3` (the result copy, no dependent inside its block:
  the compare that uses it is in the next block) and `cmpwi cr7,r29,0` (-> the block's `ble`) are both ready after the `bl`;
  ours ranks the compare first by priority (compare->branch cost), the target the copy — its original had equal priorities
  (compare->branch latency 1) or the copy's use in the same block. `int suffix;` assigned after the call, do-while, nested/two
  ifs: 2/2/9/12 words. Same family as the t_camera_data/t_id `lis`-before-`mr` ties (idEditMark 2, toolIdEditDisp 4).
- Negative results (one try each): t_se_at seAtInit volatile pW view (`SeAtWork* volatile* vp = &seAtWk.p`, 3 forms) does not
  keep the camPos `lwz pW` below the Snd zero stores (21); db_widget DB_STRING ctor: all 120 statement orders of
  max/colour-chain/type/str/len with and without a keep-alive asm stay >= 9 words — the target's `max, ca, type, cb, cg, cr,
  vptr, str, len` is neither source order nor the dying-first model (the vptr store, first in RTL, is issued 7th; the two
  other ctors issue it first); db_mod IKreport see above; t_atari plmove10, t_sce_at basic_menu, t_snd_vol editScreenDisp,
  t_rck, t_id idEdit*, db_light, db_port, t_camera_data not iterated this pass.

### Tool RELs, bytes-first pass 6 (t_id 52->53/69 with .rodata/.data now equal: idEditSize 330->22, idEditRot 235->18, idEditColor 340->201; t_camera_data 13->14/17: tcDataImport 15->0; nothing flipped; 2026-09-10)

- Harness /tmp/tools_p6 (tools_p5 copies with the paths rewritten; `tryv.py` needs `EXTRA=-DTOOLS_ARRAY` for t_id and
  `SRC=src/tools/db_light.cpp EXTRA="-fno-implement-inlines -DTOOLS_ARRAY"` for Tools/db_light). The SN loop.c source is
  readable at /tmp/st2rooms/loop.c (maybe_eliminate_biv_1 at 8540, update_giv_derive 5860, recombine_givs 7246).
- **Biv kept + `lwzx rD,rOfs,rTbl` table walk (t_id idEditSize/idEditRot inner loops)**: loop.c eliminates a counter through
  a compare-with-constant only via a giv whose add_val is a SYMBOL_REF/CONST or a POINTER-flagged register
  (`maybe_eliminate_biv_1`, and DEST_ADDR givs are always `always_computable`), so `axisName[j]` (giv `4j + &axisName`)
  eliminates `j` and spills `&axisName + 4k` compare constants into anonymous `.rodata` words (t_id's 0x444 diff). The
  target's shape (`cmpwi rJ,k` compares kept, `lwzx r8,r28,r23`, `addi r28,r28,4`) is an explicit byte-offset biv with a
  non-pointer base: function-scope `const char** tbl; u32 ofs;`, `tbl = axisName;` in each case arm (the multi-set pseudo
  is not hoisted out of the outer loop: fresh `lis/addi` per arm, one register for all three loops) and
  `for (j = 0, ofs = 0; j <= 2; j++, ofs += 4) .. *(const char**)(ofs + (u32) tbl)`. A giv form `ofs = j * 4` at the loop
  top derives the mem giv (`lwz 0(rG)`), mid-body forms leave `slwi` unreduced.
- **`add r4,rY,rI14` inside the inner loops with `mr r24,r11` after the outer `mulli`** = the outer `yy = y + i * 0xE` for
  the row eprintfs and `y + i * 0xE` written literally in the inner-loop eprintf: gcse PREs the second `mulli` into the
  copy, the `add` is a hard-register argument set loop.c never hoists.
- **Colour-select forms, read off the branch polarity**: `col = 7; if (f & 0x10) col = 0;` = `li 7; andi.; beq; li 0` (the
  ternary `(f & 0x10) ? 0 : 7` is fold-inverted -- then-value zero -> arms swapped, test inverted -> `li 0; bne; li 7`);
  `li 7; cmpw; beq; li 0` for `((j != 0) == bit) ? 7 : 0` is the if/else STATEMENT `if ((j != 0) != bit) col = 0; else
  col = 7;` (jump.c hoists the else set; the ternary is fold-inverted again); `sy + ((sy > 7) ? -0xE : 0)` is distributed
  by fold into `sy > 7 ? sy - 0xE : sy` (`mr r0,r4; ble; subi`) -- the target's `li r0,-0xe; ..; bgt; li r0,0; add` is a
  variable `dy = -0xE; if (sy <= 7) dy = 0;`.
- **s8 wrap clamp with a `case 0: break;` tree** (idEditSize subCur 2): `s8 cur = 0; s8 n; .. n = cur; n = cur - 1; n = n + 1;
  n = n < 0 ? 0 : (n > 2 ? 2 : n); if (n != cur) { ..; switch (n) { case 0: break; case 1: ..; case 2: .. } }` gives
  `mr r11,r10` (no extsb: s8 = s8), `subi; extsb`, `addi; extsb`, the `mr r9,r11; extsb r0,r9; cmpwi 2; ble; li r9,2;
  extsb r0,r9` clamp and `cmpwi 1; beq; ble default; cmpwi 2; beq` (the case-0 node). A u8 member clamp whose `< 0` compare
  survives (`lbz; cmpwi 0; blt`, idEditRot rotAxis) is `int n = d->rotAxis; d->rotAxis = n < 0 ? 0 : (n > 2 ? 2 : n)`.
- **Member re-reads, not locals** (idEditSize case 0): `if (d->sizeX > d->sizeY) ratio = d->sizeY / d->sizeX ..` gives the
  `lfs f13; lfs f0; fmr f11,f13; fmr f12,f0; fcmpu f13,f0` PRE copies and `fsubs f0,f11,f10; stfs` for `d->sizeX -= step`;
  `d->flags10A` read three times gives `lbz r0; mr r8,r0` + `andi. r0,r8,..`.
- **Function-scope float constants** (idEditRot): `f32 step = 1.0f; f32 v = 0.0f;` before the switch -> both `lfs` in the
  prologue (f30/f31), reused by case 1's `grid.x = grid.y = 1.0f` stores and the `SctrlInitCursor(.., 0.0f, 0.0f)` args;
  pool order 1.0, 0.0, 10.0; case 0's `step = (on & 0x100) ? 10.0f : 1.0f` still reloads 1.0 in the else arm (other ebb).
  idEditColor: `int v = 0; int step;` at function scope puts `li r10,0` before the Joy `addi`.
- **Dead test to stop loop.c pass 2** (idEditRot case-0 loop, 69 -> 18): the loop has 67 real insns in pass 2, so the `">"`
  string high (life 1) is hoisted (71 >= 67); the target keeps it in the if-arm. `if (d->x109 == 0) col = 7;` at the body
  end (+5 insns, col re-set at the loop top, x109 not otherwise read in the loop) -- tagged `COMPILER-DIFF: 3`.
- `switch (i) { case 2: ..; case 3: .. }` instead of `if (i == 2) ..; if (i == 3) ..` (the second `if` kept cr4 alive:
  `mfcr r12` prologue, `cmpwi cr4`). `*(u32*) &c0 = 0; c0.a = 0xFF;` per iteration = `stw r15,8(r1); stb r16,3(r31)` with
  `li r15,0; li r16,0xff` hoisted by loop.c (idEditColor; the `= {0,0,0,0xFF}` initializer or byte chains give `stb`s).
- **t_camera_data tcDataImport 15 -> 0**: `pLog.p->err(..)` inside the loop's error arm (the em2b rule: the `operator->`
  inline's BLOCK notes make loop.c hoist `lis pLog`), and the target's `extsb r7,r11` into r7 = the 4th argument register:
  the err call passed `s->camera_no` (the source had the `%02d` without its argument).
- Open, per function (one to three tries each): idEditSize/idEditRot preheader `li rOfs,0` issued LAST in the target
  (after the PRE `li i+1` and the `addi rX,x,0x40` giv init: an RTL position after loop.c's giv init that no for/while/
  do/ofs-first/tbl-in-loop form gives) and the tbl / `i*14`-copy r23/r24 pair; idEditColor `y` r22 vs r14 (lowest
  priority in ours) cascade; idEditPos not iterated (its 0x20a4 switch tree differs -- casetree); loadItemIdName `p` r31 /
  `e` r30: e (16 refs / 19 insns = 3.4) outranks p (33/76 = 2.2), the target needs e in (2.1, 2.17] (u32 `no`, in-place
  `e += len; e -= 0x40000`, `e = (char*) strtoul` forms 29); tcSetBesideCamera loop-2 `o` (146: 12 refs/16 = 2.25) vs the
  src giv (190: 17/30 = 2.27) razor-thin (if/else o, `Vec* dst`, `Vec* src`, byte-offset forms 9); t_atari plmove10
  (memcpy / word copies / `Vec old = w->pos` initializer / struct view / `Vec old[1]` / `asm("" :: "r"(&old))` / copy
  before the step select: 19-62); db_light printEditTable: `-fno-gcse` reproduces the target's `li r30,10; addi r30,r30,1;
  slwi r3,r30,3` x-variable shape exactly, i.e. the target's cprop did not fold `x = 10` into the `x++` after the `?:`
  join, and the row's `y` has TWO PRE copies (`mr r26,r23` for the "P" eprintf only, `mr r29,r23` for the rest) -- the
  inline/macro boundary of printEditRow is not understood; db_light editColor: `tmp` at 8 / `c` at 0xC (ADDRESSOF purge
  order: c forced first in the target; tmp/c declaration order, `GXColor tmpa[1]`, `u32 tmpw`, `GXColor* pc = &c` forms
  120-208); t_snd_vol editScreenDisp: `base + rows*0x14 + 4` and `base - 4` recomputed after the two loops from a
  `mr r10,r23` copy of base (#3 PRE family; `asm("" : "+r"(base))` 87, dead do-while 83); t_scroll loadBinName `addi p`
  before `cmplwi num` at the loop end (sched tie; if-break / for(;;) forms 52, `++p` / `< 0xF9` 2).

### Tool RELs, bytes-first pass 7 (t_sce_at Matching in Tools + t_sce: basic_menu 215 -> 0; t_sce_item 41 -> 42/43; db_light 130 -> 131/135 x5; t_scroll 38 -> 39/42; 2026-09-10)

- Harness /tmp/tools_p7 (tools_p6 copies with the paths rewritten; `sbs2.py T.s O.s [ctx]` = side-by-side asm diff of two
  dtk listings with labels/relocs masked, `odis.sh OBJ FUNC` = one function's dtk disassembly of any object, `fsect.sh DUMP
  FUNC` = one function's section of a -dX dump). /tmp is a 32 GB tmpfs that was FULL this pass (other harnesses: 2.2 GB
  /tmp/hostdep, 1.9 GB /tmp/c066-build3.log, 1.3 GB /tmp/re4disc ...): tryv/tryvh/mdump now write their objects and dumps
  under ~/.cache/tools_p7/{out,dump}; `rm` is a gio-trash alias here, use `/bin/rm`.
- **basic_menu (t_sce_at, 215 -> 0, unit flipped in Tools and t_sce) -- the "fresh `lis` vs r27" mechanism, read off -dG:**
  the target's four `lwz rX, sceAtCur@l(r27)` sites (x49 in the if-arm, x38&8, x39, x44 of the disp part) are NOT the
  sites gcse deletes directly. cse1 substitutes the block's own high pseudo Q into the NEXT site along its path (the
  fall-through of a `beq` into the arm, a single-use join label reached by the taken `beq`), the site becomes a copy
  `P = Q` and canon_reg makes the load use Q; gcse's PRE then turns Q's `lis` into `Q = R` (R = the bb-0 insertion, r27),
  cprop pass 2 copy-propagates R into Q's uses in OTHER blocks (`lwz (r27)` there), and Q's same-block uses keep Q whose
  copy cse2 re-materialises (`REG_EQUAL (high)` cost 0 < pseudo 1 in cse_insn's trial order) = the target's "fresh" `lis`
  in that block. So a fresh-`lis` block followed by an r27 block is ONE plain symbol with a cse1 path between them; the
  22-alias variant of pass 4 had broken that path at PC(19)..PC(22) (29 words), plain there gives the four r27 sites for
  free. Nine aliases stay (`extern SceAtWorkPtr sceAtCur_cN asm("sceAtCur")`, the menu[5] test and the FIRST pCur read of
  each of the eight cases; every other alias was removed one at a time at 0 words; each remaining one costs 36-383 words
  when plain): our cse1 reaches the case entries from bb 0 through the compare-tree dispatch (taken `beq`s to single-use
  labels + AROUND over the two `if (on) on = 1` blocks, path length 7 < PATHLENGTH), the original's did not. Not a flag
  (-fno-cse-follow-jumps 110, -fno-cse-skip-blocks 417, -fno-gcse 241 on the plain source).
- **The `do { } while (0);` before the second `on = pCur->x38 & 8` (the J1 block after `if (on) on = 1`)**: the target
  issues `stb; stb; lis; lwz` (the free `lis` after both menu stores), ours `stb; lis; stb; lwz` (lis at t=1 with the
  first store). The dead loop's LOOP_END note (a) ends cse1's path scan ("Don't cse out the end of a loop", cse.c) and
  (b) makes haifa give the first insn after it REG_DEP_ANTI links on every earlier insn of the block and `reg_pending_sets_all`
  (every later insn depends on it) -- the sched region split of the cDbgWindow::Init recipe, mechanism now exact. Caveat
  seen in t_sce_item's copy of the block: the lis->lwz link then becomes the 1-cycle anti link instead of the 2-cycle true
  one, so a compare that follows the lis in the target (`lis; cmpwi r3,1; lwz`) comes out `lis; lwz; cmpwi` (2 -> 3 words):
  the barrier is right only when nothing but the lis's own chain follows it. t_sce_item basic_menu (64 -> 2 with the four
  case aliases, tagged) keeps this residue; `asm volatile("")`/`asm("" ::: "memory")` barriers and the store-order variants
  are 5-8 words.
- **`u32 t = pCur->x38 & 0x7F` block-local for the `(u32) n <= 8 ? tbl[n] : "..."` select**: the function-scope `int n`
  (the switch cases' variable) is a global pseudo (r10 in ours); the target's `clrlwi r0,r0,25; cmplwi r0,8; slwi r0,r0,2`
  is a short-lived local (r0). Any block-local temp (u8/u32/int) gives it.
- **loadItemIdName (t_sce_item, 28 -> 0)**: (1) COMPILER-DIFF candidate #17 applied as `register int pin asm("r30"); asm("" :
  "=r"(pin)); asm("" : : "r"(pin));` at the function top -- r30 becomes used-so-far, `e`/`no` (allocated first, priority
  3.4) take it in pass 0 and `p` falls to r31 in pass 1 (target p r31 / e r30); the two asms emit nothing. (2) the sys
  arm's tail `add r30,r30,r29; li r0,0; subis r30,r30,4; stb r0,0(r30)` = in-place `e += len; e -= 0x40000; *e = zero;`
  with a BLOCK-LOCAL `int zero = 0;` declared before the `e += len` (a plain `*e = 0` puts the `li` after the `subis` in
  LUID order, and sched1's weight rule then keeps it there: `li` +1 vs `subis` 0 at equal priority; a function-scope zero
  is 84 words, an asm-li also 0 but tagged).
- **plmove10 (t_atari, 19, left; mechanism read off the sched2 dump)**: in sched2 the frame stores of the `old = w->pos`
  copy (`stw [r1+8]`, `stw [r30+4]`, `stw [r30+8]`) and the w-based RMW loads/stores all conflict pairwise, so the copy
  loads inherit the RMW stores' chain and outrank the RMW loads (46: prio 10, 65: 9; sched1 had them equal and LUID-ordered
  like the target). Cause: alias.c `find_base_term` for `(plus r31 N)` after reload -- r31 = `mr r31,r3` gets base
  `ADDRESS(VOIDmode, r3)` (copying-arguments), and the PLUS case returns 0 for a VOIDmode ADDRESS operand, so every
  `[r31+N]` has no base and `base_alias_check` returns 1 against the stack refs (in sched1 the pseudo has the pointer flag
  and the stack rule applies). The original's sched2 did not see these conflicts: the r119/r11b sched2-alias family, not
  source-fixable (memcpy / 12 word- and member-copy orders / `Vec* po` / precomputed RMW values: 19-62).
- **tcSetBesideCamera (t_camera_data, 9, left)**: `o` (146: 12 refs/16 insns, floor_log2 3 -> 2.25) vs the src giv (190:
  17/30, log2 4 -> 2.27) -- the global-alloc priority is `floor_log2(refs)*refs/length`, REG_ALLOC_ORDER is 0, 9, 11, 10,
  8, 7, ... so the FIRST allocated gets r8 (ours: src), the target allocated `o` first. #17 pins of r7/r8 in the outer
  loop change nothing (both are used-so-far already); a dead test at the body end (`if (c->flags == 7) o = 0;` /
  `if (i == 5) o = 0;`) flips the pair (9 -> 6/7) but its extra pseudo pushes the `i + 1` copy from r5 to r28. Left.
- **OkCancel Init x3 in ToolEspArea/ToolLightAreaMain (571/507, left)**: `asm("li %0,0" : "=r"(z) : "r"(len))` after the
  strlen for `pBottom = pTop = pCur = z` gives the target's separate `li r0,0` + pBottom/pCur/pTop store order, but the
  block's other order (all field stores before the AddButton `li r4..r10` argument moves, pName's dying store first) is
  the #13 constant-store family and the counts stay 564-593; not applied.
- **db_light**: edit_cutsel 1 -> 0 with `int t = i + 6; asm("mr %0,%1" : "=r"(line) : "r"(t));` (COMPILER-DIFF 4, the
  unmasked narrow store: applied, all five modules). printEditTable: `asm("li %0,10" : "=r"(x))` for `x = 10` with `7 * 8`
  and `10 * 8` written as literals reproduces the target's unfolded `li r30,0xa .. addi r30,r30,1; slwi r3,r30,3` chain
  exactly (gcse cprop pass 1 replaces reg x in `x + 1` with 10 and validate_replace_rtx folds it; the original's cprop did
  not), but the rest of the function (one more callee-saved register for the hoisted string highs r17/r18/r21, the two
  `y` copies, col at 0xc vs 8) is unchanged and the count rises 183 -> 219: not applied.
- **t_scroll loadBinName (2 -> 0)**: `p++; asm("" : : "r"(p));` at the loop end (COMPILER-DIFF #13 keep-alive) gives the
  `addi p` a same-block dependent, so it outranks the exit `cmplwi num` (an `asm("" : "+r"(p))` after the increment also
  0, before it 2). edit_litmask (2): `lbz id; lwz x54; li r9,1; slw` -- ours issues the `li 1` at t=1 with the lbz; the
  target's r9 anti-dependence on the lbz base is in ours too (same registers), the swap/temp/xor-eq forms 2-5, left.

### Tool RELs, bytes-first pass 8 (t_sce/t_sce_item Matching 43/43; t_camera_data 14->15/17; t_scroll 39->40/42; t_esp_area 571->432 + t_lightarea 507->389 words via dbg_tool.h; 2026-09-10)

- Harness ~/.cache/tools_p8 (tools_p7 copies with the paths rewritten; new: `vsbs.sh MOD/UNIT FUNC VARIANT|CUR
  [ctx]` = side-by-side diff of the target function against a tryv/tryvh variant object (or the current build) with a
  `DIFFLINES:` count -- the word count of a function whose SIZE differs is dominated by displacement, the line count is not;
  `sbs2.py` now masks `.rodata+0x..`/`_vt.`/`.text+0x..` too; `mdump.sh` takes `INC="-I<variant dir>"` for header variants;
  `galloc.py` copied from rooms_b5). /tmp was 42% full; everything went under ~/.cache/tools_p8.
- **basic_menu (t_sce_item, 2 -> 0, unit flipped): the do-while barrier of the t_sce_at copy is a dead test here.** The J1
  block's `stb; stb; lis pW; cmpwi r3,1; lwz` order = the two menu stores in one sched region and the pW `lis` + compare in
  the next: `if (menu == 0) n = 0;` between `menu[2].enable = on` and `x = pW->x + 0x80` (n is re-set before every read, menu
  is a live param -> no new pseudo/high) splits sched1's block; unlike the LOOP_END note it leaves the lis->lwz true dependence
  alone, so the compare stays between them (the do-while gave `lis; lwz; cmpwi`, 3 words). `if (sel == 0)` folds into the
  switch compare tree (12 words); `sel == 9` and `menu == 0` are equivalent. Tagged `#13 (region split, dead test)`.
- **cDbgOkCancelWindow::Init x3 (dbg_tool.h; ToolEspArea 571 -> 432, ToolLightAreaMain 507 -> 389, t_event SubToolMessInit
  313 -> 204; all three Init blocks byte-identical now)**: (1) `do {} while (0)` after `num = 0` = the region split whose
  second region is [pointer zeros + AddButton arg moves + bl]; (2) in the first two inlined copies the pointer zero is a fresh
  `li r0,0` issued after the six constant stores with the arg `li`s between pCur and pBottom/pTop: `cDbgButton* z;
  asm("li %0,0" : "=r"(z) : "m"(w)); pBottom = pTop = pCur = z;` -- the `"m"(w)` input orders the asm after the `stw w`
  without a register copy (`"r"(w)` copies the strlen result `mr r0,r3`, `"r"(name)`/`"r"(this)` tie the output to a dying
  callee-saved input and hoist it, an input-less asm is PRE'd to the function top); (3) the THIRD copy stores the pointers from
  the shared zero r31 (pBottom, the dying store, before the arg moves, pCur/pTop after): a second inline `InitLast` = plain
  `pBottom = pTop = pCur = 0` + the barrier, called for exitOk. With the third copy on the shared zero the zero pseudo has 13
  refs and outranks `one` (10) in global alloc -> `li r28,1; bl new; li r31,0` as the target (before that both had 10/218 and
  the tie went to the lower regno = `one`). Mechanism reading: #13(b) -- the original's chain zero is a REG_EQUIV constant
  reload re-creates per block (r0 + inheritance) except where the shared pseudo dies.
- **cDbgEditWindow<T> ctor (same header)**: `rows = nRows; pWork = work; numWork = n;` (the dying stores come out rows, pWork,
  numWork; the hoisted constant before strlen is the lower-LUID one -- NB the inline's PARAMETER pseudos (`n`, `nRows`) are set
  in parameter order before the body, so the statement order alone does not decide which `li` is hoisted); a `do {} while (0)`
  between `pCur = 0` and `pTop = 0` (the last six zero stores are their own region, pSetWorkNo -- the dying one -- first) and
  `const char* label = "00"; cb = NoButtonUpdate_callback;` locals BEFORE the loop for the target's `mr r6,r27; mr r10,r28`
  (literal arguments keep the `addi` in the body; block-locals inside the loop are folded back by cse1). Left in this
  function: the target's `cmpwi r31,0` + `mfcr r29` before the button loop / `mtcrf` after = the `pEdit == 0` compare PRE'd
  into the pre-loop block; ours keeps it after the loop because the pre-loop block SETS r31 (`mr r31,r3`), so ANTIN is 0 there
  and the block LCM's earliest block is the loop body -- the original had a block boundary between the `new` result copy and
  the ctor stores (a `new` null check `p ? ctor : 0`, cp/init.c `check_new = flag_check_new || nothrow` -- `operator new`
  declared `throw()` -- would give exactly that and be folded later?). Not tried this pass. Also left: the entry zero r14/r15,
  the tool pointer r23/r22 vs string high r22/r23, and the main-loop residues (diff lines 414 -> 286 in t_esp_area).
- **tcSetBesideCamera (t_camera_data, 9 -> 0)**: `if (c == 0) o = 0;` at the end of the second inner loop body (c = the TcCdat
  pointer r31, o the body-local pointer): `o` then beats the `&c->pos[n]` giv in global alloc (r8/r7) WITHOUT displacing the
  `i + 1` PRE copy from r5 -- the earlier `c->flags == 7` / `i == 5` tests added a pseudo or a ref to a live pseudo. Rule: pick
  the dead test's operand among registers that are live across the whole loop anyway (the object pointer), never a loaded field
  or the loop counter. Tagged `#13 (global-alloc order, dead test)`.
- **edit_select_sub (t_scroll, 5 -> 0)**: the last two `pWork->joy[0].rep` tests keep ONE high(scrollWorkPtr) register (r10)
  across the join; ours re-materialises `lis` after the label (cse2, REG_EQUAL (high) cost 0). Applied the DOL-sweep-10 asm-high
  recipe: `asm volatile("lis %0,scrollWorkPtr@ha" : "=r"(hi))` + `asm("lwz %0,scrollWorkPtr@l(%1)" : "=r"(w) : "r"(hi))` for
  each of the two loads (`// COMPILER-DIFF: 3`). The lis asm must be VOLATILE or have a register input that exists at that
  point: input-less non-volatile (`"i"(4)`, none, after a do-while) is a gcse expression and gets PRE'd to the top (46 words);
  a `"m"(pWork)` input materialises its own `lis r9` (4 words); `"r"(pWork->subCursor)` reloads (5).
- Negative results (one to six tries each, do not retry the same forms):
  - t_atari plmove10 (19): asm copy loads/stores (`lwz %0,4(%2)` pairs, `"m"` operands, one 6-insn asm, `"f"` inputs to delay
    the loads behind the RMW terms, volatile loads) 44-82 -- asm insns have no function unit and float to the block top or
    barrier everything; SN's `__attribute__((noalias))` on the `Vec old` local does NOT reach its MEMs (alias set stays 0 in
    the -dR dump; `c_get_alias_set`'s DECL_NOALIAS branch is hit only for a DECL node and the block move's MEMs are built
    from the ADDRESSOF/frame rtx). The sched2 alias story (find_base_term returns 0 for `(plus r31 N)`, `base_alias_check`
    returns 1 on a zero base, so every copy store conflicts with every RMW ref) is confirmed from the source; nothing
    source-side changes it.
  - tcSetBesideOffset (27): loop-1 givs `n*0xc+0x18c` (9 refs/84 insns) vs `n*4` (9/82) -- the shorter one is allocated
    first and takes r3, the target gives r3 to the 0xc giv; dead tests in the ctr loop body (`&c->pos[n]`, `&c->at[n]`,
    `c == 0`, `&c->roll[n]`: 55-76 -- they break the bdnz shape), in the outer body (`j = 0` targets: 31-60), roll/fovy
    before pos/at (47). tcDataExport (142): whole-function allocation from `buf` (r29 vs r31), not iterated.
  - t_scroll edit_litmask (2): `asm("li %0,1" : "=r"(one) : "r"(x54))` gives the target's `lbz; lwz; li; slw` order but the
    asm input keeps the loaded word in a user pseudo and rotates r9/r11/r0 (5 words); `"m"` inputs do not delay the li (2);
    `(1 << id) ^ x54` 3.
  - db_light editColor (120): `tmp = black; DrawTile(&tmp);` BEFORE the `c.r/c.g/c.b` stores gives the target's frame
    (tmp 0x8, c 0xC: purge_addressof puts a variable into the stack at its first unresolvable ADDRESSOF -- `c.g = 0` at offset 1
    is one, `tmp = black` (whole SImode store) is not, so c is forced first unless a `&tmp` precedes it) but the c stores can
    no longer move above the first call (153). The target's `addi r7,r1,8` per DrawTile (no `&tmp` pseudo) next to a hoisted
    `&c` pseudo (r28) is not explained; `mr r7,P` in ours = one purged `(plus fp N)` pseudo shared by cse.
  - t_snd_vol editScreenDisp (83): `s16 b = base` / `int b` / `base -= 4` / `(base + 4) + rows*0x14` around the two post-loop
    `pt` writes: 82-83 (the target computes `base - 4` and `base + rows*0x14 + 4` from a `mr r10,r23` copy after the loops;
    ours PREs `base - 4` above them).
  - t_id idEditRot (18): `ofs = j * 4` at the loop top (tbl set inside or outside the loop) 35, `asm("li %0,0" : "=r"(ofs) :
    "r"(tbl))` before the loop 31, a dead test at the body end 137. The target's `li rOfs,0` after the loop.c giv inits means
    the original's `ofs` init was emitted by loop.c (a giv), but every giv form folds tbl into the address (`lwz 0(rG)`).
  - t_sce_item still defines the globals `set_filename` / `angle_arrow_disp` that t_block / t_sce_at also define (ngcld keeps
    one symbol-table entry, the REL bytes are unaffected, 111 OK) -- make them static before another t_sce unit flips.

### Tool RELs, bytes-first pass 9 (t_scroll 40->41/42: edit_litmask 2->0, printEditTable 112->71; t_snd_vol editScreenDisp 82->11; t_sce_item duplicates made static; nothing flipped; 2026-09-10)

- Harness ~/.cache/tools_p9 (tools_p8 copies with the paths rewritten; `tryv.py` variant names must be at
  least two characters -- a one-letter name (`v_UNIT_A.o`) is silently not produced by ngccc/NgcAs; background
  `( ... &)` sweeps are killed with the tool call, run them in the foreground with a long block).
- **edit_litmask (t_scroll, 2 -> 0, tagged `#13` + `candidate #17`)**: `register u32 x54 asm("r11"); register u32 one
  asm("r9"); x54 = obj->lightInfo.x54; asm("li %0,1" : "=r"(one) : "r"(x54)); mask = x54 ^ (one << pWork->id);`. The
  target's `lbz id; lwz x54; li r9,1; slw` is the #13 reload-materialised constant (LUID right before the `slw`, after
  the lwz; sched2 ties on priority/dependents and LUID decides); the asm-li alone (pass 8: 5 words) leaves the x54
  pseudo and the `one` output to local-alloc, which rotates r9/r11/r0 -- the two value-carrying pins fix the names.
  Pinning `id` to r0 as well is worse (2): the lbz destination must stay a free temp.
- **printEditTable (t_scroll, 112 -> 71)**, three findings:
  (1) `x` (the column) is TWO variables in the original: `x = 4` / `x = 8` live in r3 (`li r3,4` AFTER the
  SmdGetGroupObjPtr call, `slwi r3,r3,3` at the eprintf, `slwi r24,r3,3` PRE'd across strncpy) and the tail base
  0x11 in a callee-saved register (r27, the dead `no`). One `int x` is one pseudo that crosses the tail's nine eprintf
  calls, so ours hoisted `li r26,4` above the call (REG_N_CALLS_CROSSED > 0 lets sched1 move the set) and kept it
  callee-saved everywhere. A second variable `x2` for the tail gives the r3 shape for free (a pseudo with
  N_CALLS_CROSSED == 0 gets the `sched_before_next_call`-style anti link that keeps its set after the call).
  (2) `flag = obj->be_flag` as a local is wrong: the target's `lwz r0,0(r28); andi. r9,r0,0x201; mr r11,r0; cmpwi
  r9,1` is gcse PRE of the LATER `obj->be_flag` reads (fully redundant after the isAlive load): the insertion `R =
  (mem be_flag)` at the end of the isAlive block becomes `mr r11,r0` through cse2. Re-read `obj->be_flag` at every
  use; the second block now has the copy (ours issues the `mr` before the `andi.`, a tie), the first block's copy is
  still cprop'd away by ours (it has one use).
  (3) the tail `(x2 + 3) * 8` is NOT folded in the target although both arms set 0x11 -- our gcse's
  `insert_set_in_table` merges two `(set x2 17)` insns into one entry (expr_equiv_p on the whole SET), so the
  constant is available at the join and cprop folds. The original's cprop does fold `x = 8` into the NO REGIST /
  UNKNOWN MODEL arms (`li r3,0x40`), so cross-block cprop exists there; what it did not do is merge the two arm
  sets. Applied `asm("li %0,0x11" : "=r"(x2))` in the else arm (tagged `candidate #12 (cprop)`, the UNKNOWN arm plain):
  111 -> 82. Note an input-less asm is PRE'd (gcse treats ASM_OPERANDS as an expression): the `li` moved to the
  else arm's preheader block with a copy `mr r31,r27` at the statement -- the target has the SAME early `li r27,0x11`
  without the copy, so the original's set was in the preheader block (before the name loop) in the source.
  (4) the three `pWork` sites: ours PREs high(scrollWorkPtr) of the nameTbl site above the j loop (single occurrence,
  block LCM never delays through a loop header) and then merges all three into one hoisted `lis r14`, which displaces
  the "SCL" string high (target r14) and adds a `lis r9` at the last eprintf. The edit_select_sub asm-high recipe at the
  nameTbl site alone (`asm volatile("lis")` + `asm("lwz")`, tagged 3) leaves the other two sites fresh like the target
  (82 -> 71). Left: i/y/no allocation (target y=r31, no=r27, i=r26; ours i first: 14 refs/226 vs y 13/228 -- the refs are
  1 + 2 x (4 PRE'd `+1` insertions + latch copy + compare), and the original has the same sets; declaration and
  for-init orders change nothing), the first block's `mr r11,r0`, and `x*8` PRE into r24 before the name loop.
- **editScreenDisp (t_snd_vol, 82 -> 11)**: (1) `x` is reused in loop 2 (`x = 0x40;` before the loop, `pt[0].x = x`):
  the target's `li r31,0x40` before loop 2 and `extsh r31,r9` in loop 1 are the same callee-saved pseudo, i.e. x is
  live across loop 2's calls (ours had loop 1's x block-local in r9 and pushed every callee-saved name by one);
  (2) `x = (u16) (s16) (...)` for the target's `psq_st qr5` + `lhz`; (3) loop 2's `pt[0].y`/`pt[1].y` through the
  existing `s16 v` (`extsh` then two `sth`; two direct member expressions store the SI sum without the extsh);
  (4) the post-loop `mr r10,r23` copy of `base` from which the target computes `base - 4` and `base + rows*0x14 + 4`:
  ours PREs the single `base - 4` above the two loops (the block-LCM loop-header rule again). Applied
  `asm("mr %0,%1" : "=r"(b) : "r"(base), "r"(i))` after the loops (tagged 3) -- the dummy `i` input keeps gcse from
  PREing the asm itself above the loops (with `base` alone it moved there, 94); store order in that block brute-forced
  (720 permutations, `x, z0, x1, y1, y0, z1` = 11). Left (11 words): three local-alloc temps where the target does not
  reuse a dying source (`extsh r10,r0` / `subi r0,r10,4` vs ours `extsh r0,r0` / `subi r11,r11`, and `lwz r10` vs
  `lwz r11` for the work pointer) -- the local-alloc fake-lifetime parity family; barriers, launders, fresh/int temps
  and all 726 loop-2 statement orders leave it.
- **t_sce_item**: `set_filename` / `angle_arrow_disp` are `static` now (t_block owns `set_filename`, t_sce_at
  `angle_arrow_disp` in the t_sce .sym). Safe: our objects carry no relocation against them (NgcAs resolves the
  same-section `bl`s; the split objects' REL24/REL14 entries are dtk artefacts), so the scope has no ADDR field to
  change; 43/43 and 111 OK after the change.
- **plmove10 (t_atari, 19, left) -- the mechanism is sched1-side, not sched2-side**: the target's `stw` copy stores
  come AFTER the three `stfs` RMW stores; after reload every `[r30+N]`/`[r1+8]` frame store conflicts pairwise with
  the `[r31+N]` stores (find_base_term returns 0 for `(plus r31 N)`, the same in both builds), so the sched2 output
  dependences follow the RTL order and the original's RTL at sched2 entry already had stfs before stw. Our sched1
  issues the copy stores first because their loads are ready early (`46 prio 9` ties the `lfs`, LUID wins). SN's
  toplev does NOT enable `-fstrict-aliasing` at -O2 (only the explicit flag; `get_alias_set` returns 0 otherwise), so
  the "u32 view / alias set != 0" idea is void: `-fstrict-aliasing` on the unit regresses two functions and leaves
  plmove10 at 19; `u32` word temporaries loaded before the RMW and stored after give the target's load/store
  structure but lose the `&w->pos`/`&old` pseudo addressing (`4(r9)`, `8(r30)`: 41-55 words).
- **tcSetBesideOffset (t_camera_data, 27, left)**: manual strength reduction (`u32 ofs = 0x18C; ofs += 0xC` etc., with
  and without `register ... asm("r3")` pins) 68-97; the giv list is prepended (record_giv), so the 0xc giv (found first)
  is initialised last in both builds and the 4-giv is the shorter allocno in ours; a different priority in the original
  is not explained by refs or lengths.
- **ToolSeAt (t_se_at, 2, left)**: `mr r0,r3` vs `lis r9` after the Debug_alloc call is a sched2 tie at equal priority
  (both class 3 against the call: the result copy's data link costs 1, the `lis` has a cost-0 ANTI link); the deciding
  dependents count includes the memclr argument copy `mr r3,r0` (insn 29), which ours still has at sched2 -- it is only
  deleted by jump2's noop-move `find_equiv_reg` rule -- and which the original evidently did not have there. Alloc-
  through-a-local, `PSet`-style and volatile store forms: 2-6 words.
- DB_NUMERIC ctor (db_widget, 2): `register f32 arg asm("fr1"); arg = 0.0f;` as an input of the keep-alive asm and the
  SetDefault argument moves the `fmr f1,f31` too early (before `lfs f0`, 5 words); the target issues it between the
  first and second zero store.

### Tool RELs, t_esp pass (db_port 63->67/68, db_widget 104->107/113, db_mod 59->60/75 (Tools 47->48/63), t_esp 188->193/212; db_light 132/136 untouched; nothing flipped; 2026-09-10)

- Harness ~/.cache/tesp (tools_p9 copies with the paths rewritten; `vsbs.sh MOD/UNIT FUNC CUR|VARIANT [ctx]`, `tryv.py`
  with `SRC=src/tools/db_mod.cpp` for the shared unit, `fsect.py DUMP FUNC [pattern before after]`). A wibo (CPP.exe)
  process hung for 9 minutes inside `ninja build/.../t_esp.o` while another agent's build ran; `kill` of the wibo pid let
  ninja finish with a good object (re-verified by deleting the .o and rebuilding).
- **Zero-code levers found (apply first, all verified byte-identical):**
  - `sp_tex_trans(int no)` (db_port): the `extern "C"` definition takes `int` while t_esp.cpp's declaration says `u8`;
    `n = (u8) no` is the entry `clrlwi r28,r3,24` with no launder (replaces the old #2 `asm("" : "+r")` form, 8 -> 0).
  - DB_ConfigLoad (db_port, 161 -> 0): `if (num++ != 0) cur++;` puts the increment between `cmpwi` and `beq`; the
    MODEL_NAME copy is `cur->name[i++] = *p; p++;` (byte read, then the byte store, then the `p++` re-reads p from its
    slot because the store may alias it) with the index being the function's `u32 i` (reused by the later load loop:
    one pseudo -> r30 in both loops -> the 10th callee-saved register the target saves and the odd-count fpmem slot at
    0x420). A block-local `int k` is a fresh pseudo allocated in pass 1 (r8) and shifts the whole frame by 8.
  - DB_WINDOW_TITLE::Draw / DB_STRING::Draw (db_widget, 27+36 -> 0): the `.y` reads of `base`/`size` go through a
    `DB_POINT*` accessor (`static inline f32 DB_PointY(DB_POINT* p) { return p->y; }`): gcse PREs `&base`/`&size` into
    callee-saved registers (`addi r30,r31,0x3c; lfs 4(r30)`) while the `.x` reads stay `this`-relative; the sum
    `drawPos.y + base.y` whose fadds result must be the `base.y` register is an inline `DB_AddY(f32 a, DB_POINT* p)
    { return a + p->y; }` (the parameter copy `a` is op0, local-alloc ties op1 = the load). The three colour variables
    survive as three pseudos only if the two chains have DIFFERENT copy sources at the join (`g = 0.7f; r = g; b = g;`
    / `b = 0.8f; g = b; r = b;`): with `r = g = b = K` in both arms gcse cprop merges them into one register.
  - init_dbEm (db_mod, 19 -> 0): `em->parent = 0` BEFORE `em->name[0] = 0` (the QI store then takes the SI zero's
    lowpart as `src_related`, cost 0 < the QI zero pseudo's 1; written first, the byte store reuses the loop-top QI zero);
    `em->motStat[i] = em->motFlag[i] = 0` (a chain: the outer destination's address is computed first, the inner store is
    issued first -- the two byte stores conflict in alias.c so RTL order is issue order); `&pGS->Cam` after the
    mem_alloc store (struct view keeps `lwz pG` below `stw cam`).
  - DeleteSeqData / InsertSeqData (t_esp, 33+45 -> 0): `TOOL_SEQ* s = &tbl[no + 1]; TOOL_SEQ* d = (TOOL_SEQ*) ((u8*) tbl
    + no * sizeof(TOOL_SEQ)); *d = *s;` -- the source address computed FIRST is `tbl + (ofs + 0x12c)` (`addi; add`),
    while `tbl[no] = tbl[no + 1]` derives it from the destination (`add; mr; addi`); the flag table pointer is read
    through a struct view (`((SeqFlgPtr*) &g_pSeqFlg)->p`, `struct SeqFlgPtr { u8* p; }`) so its load stays below the
    block copy, and the trailing `g_pSeqFlg[i] |= 1` must use the view too.
  - PasteSelectData (31 -> 0): `TOOL_SEQ* src = &g_pCopyBuf[g_copyNum - 1];` before the loop, `src--` in the `for`
    header (the target keeps `g_copyNum` re-read for the loop test and the pointer decremented). PartPasteSelectData
    (31 -> 0): `e = g_pEditTbl; src = &g_pCopyBuf[0]; bit = 1 << col;` locals before the loop, `&e[i]` in the call
    (declaration order e, src, bit: the other order swaps two callee-saved registers). CopySelectData (55 -> 0):
    stepping `TOOL_SEQ* e; TOOL_SEQ* c;` DECLARED AT THE TOP with `u32 i` (assigned after SelectCurrentIfNone: the
    PRE'd `i + 1`/`e + 0x12c` copies then take r7/r6 in the target's order; block-scoped pointers swap them),
    `*c++ = *e`, the count RMW through a reference (`{ int& n = g_copyNum; n = n + 1; }`: the load stays below the
    copy stores), the flag table through the struct view. MakeLoadSeqData (25 -> 11): `rec = head->rec` assigned
    AFTER `InitSeqTbl()` (caller-saved r5) and `&tbl[nSeq * i]` (`mullw nSeq, i`); left: `lhzx head, i2` operand
    order (ours `lhzx i2, head`: `((u16*) head)[i]`, byte/u32 sums and `(&head->num)[i]` all give idx first) and the
    prologue copy order.
- **Tagged forms applied (bytes identical):**
  - SeqSet (db_port, `#13`): `u32 fl = EvtDebug.flags; asm("li %0,1" : "=r"(on) : "r"(fl)); if ((fl & bit) == 0) on = 0;`
    -- the asm `li` is a consumer of the load so it is issued with `andis.` instead of at t=1 with the `lis`, and `on`
    can reuse the high's r9. An `"m"(EvtDebug.flags)` input does not delay it (no store to depend on).
  - DB_VecMulEmPartsMat (db_port, `#13`): the original never allocates the REG_EQUIV `high(EspEvModList)` pseudo, so
    `tbl = lo_sum(hi)` carries no r9 copy preference (global.c `set_preference` gives the dest of `(set X (op REG ..))` the
    REG's hard register when that REG was local-alloc'd) and `p` takes r9 in pass 0; ours excludes r9 for `p` through
    `regs_someone_prefers` and gives it r11. Reproduced with `asm("lis %0,EspEvModList@ha" : "=b"(hi));
    asm("addi %0,%1,EspEvModList@l" : "=r"(tbl) : "b"(hi));` (no preference), the `p = 0` as
    `asm("li %0,0" : "=r"(p) : "r"(tbl), "r"(no) x4)` in the THEN arm of `if (no > 0x7F) .. else asm volatile("slwi
    %0,%1,2\n\tlwzx %0,%2,%0" : "=&r"(p) : "r"(no), "b"(tbl));` -- jump.c's second transform (`if (c) { x = a; goto l; }
    x = b` -> `x = a; if (c) goto l; x = b`) hoists the single-insn then-arm to right AFTER the compare (LUID after
    `cmplwi`, the `bgt` inverted), the `"r"(tbl)` input makes it ready at t=3 with the compare (asm consumers wait 1
    cycle whatever the producer's latency), and the four dummy `no` inputs give `no` 7 refs / 7 insns = 2.0 = `p`'s
    4 / 4 so the lower regno (`no`) is allocated first and takes r0. The else arm must be volatile or jump.c's FIRST
    transform hoists the load instead (a non-volatile asm has no side effects for `side_effects_p`).
  - DB_WINDOW::CallActiveChangeCallback (db_widget, `candidate #17`): `register int ret asm("r3"); ret = 0;` -- the
    original allocates `ret` to the return register first and copies `this` to r9 (`mr r9,r3`); plain if/else, ternary,
    `ret += 1`, u8, local pointer forms all give `this` r3.
- **Mechanisms read, not closed:**
  - SetBase/SetSize (db_widget, 9/16): cse1 forwards the just-stored `base.y` into the DB_RECT temp (`stfs f2,4(r9)`);
    the target keeps the load through sched1 (`stfs f2,0x40(r3)` first, then `fmr f0,f2` from reload_cse). Nothing that
    stops cse1 keeps reload_cse's knowledge: `asm volatile("")` is an `asm_input` (does not flush cse), a `"memory"`
    clobber or a volatile ASM_OPERANDS also makes reload1.c forget every register value ("Forget all the register
    values at a volatile asm"), a volatile read is never simplified (`side_effects_p`), `-ffloat-store` regresses the
    unit (70/113). Named temp, reference, `FRef`, `do {} while (0)`: 9-14.
  - DB_WINDOW ctor (14): the DB_COLOR temp copy reads `0x8/0xc/0x10/0x14(r1)` in the target (all frame-relative) while
    cse rewrites ours to the ctor's `this` pseudo (`0xc(r9)`: find_best_addr replaces the base REG of `(plus REG N)` by
    an equivalent REG, and the ctor's `this` and the copy's address pseudo are both `fp+8`); a named `DB_COLOR c` is
    the same. The vptr store before/after `lfs 0.0` is the same block's schedule.
  - Save*FileNoUpdateCallback x5 (t_esp): unchanged (pass-5 mechanism); the target's `lha r11` reuses `step`'s register.
  - position_usage (db_mod, 34): `asm("li %0,43" : "=r"(x))` gives the target's unfolded `li r30,0x2b; slwi` but the join
    block's second `x * 8` stays a recomputation (`slwi r30,r28,3` after the mode diamond, x kept live in r28) where the
    target has the PRE copy `mr r29,r30` right after the first eprintf: gcse "0 substs" -- the join's occurrence is the
    last one on every path (the isolated-occurrence rule again; cse1 merged the "B" eprintf's `x * 8` into it); a
    `do {} while (0)` between the two last eprintfs does not un-merge them. 34 -> 44 with the asm, left plain.
  - EspToolInit (db_port, 1224 words, frame 0x1e0 vs 0x1c0: two spilled `&local` pseudos, `li r27,1` hoisted into a
    callee-saved register, BitOn/BitOff order, the evtToolOn fold) not iterated; db_light's four residues not iterated.

### Tool RELs, bytes-first pass 10 (t_light/t_scroll Matching 42/42; Tools/t_atari Matching 23/23; t_camera 51->52/57; db_sctrl 17->18/22 x2; t_snd_vol 19->20/27; 2026-09-10)

- Harness ~/.cache/tools_p10 (tools_p9 copies with the paths rewritten; new `lens.sh MOD/UNIT FUNC VARIANTS.py
  REGNO..` = tryv word counts plus the `-dl` refs/length line of the named pseudos for every variant -- the global-alloc
  order questions below are all "which of two allocnos with priority 0.59x is first").
- **printEditTable (t_scroll, 71 -> 0, unit flipped).** Five independent mechanisms, read off the dumps:
  (1) `switch (obj->x12E) { case 2: case 4: break; default: .. }` must be `if (obj->x12E != 2 && obj->x12E != 4) { .. }`
  with the member RE-READ in each test: the target's `beq` for `== 4` jumps PAST the col2 select's `clrlwi; cmpwi r0,4;
  bne` re-test straight to `li r30,6` (jump.c `thread_jumps`). thread_jumps walks the two compare chains backwards
  pairwise and needs `t2 == label` with no pending register pairs: with a switch the index is ONE promoted pseudo and the
  walk from the `== 4` compare hits the `== 2` JUMP_INSN at once (fail); with two `!=` re-reads every test is its own
  `load; zero_extend; cmp` triple, the pairs (P1~P2, T1~T2) are consumed by matching setters and the walk reaches the
  label (the `mem` operands compare equal by address). A `u8 type` local is wrong (cse merges the zero_extends, `cmpwi
  cr7` shape). Zero code.
  (2) The tail column base is set ONCE, at the tail top after the if/else, as the input-less `asm("li %0,0x11" :
  "=r"(x2))` (tagged `candidate #12 (cprop)`): gcse hashes the asm as an expression and PREs it into BOTH arms at the
  target's LUID (expr index = its first-occurrence position, between the "%8s" high and the tail's first string high;
  the arm insertion lands after the three `+1` insertions because those have weight 0), and because x2 has no other
  set, cse2's make_regs_eqv keeps the reaching reg as the class head, canon_reg rewrites the tail's uses to it and flow
  deletes the `x2 = R` copy -- no `mr`. A set in each arm (pass 9) keeps the copy (x2's FIRST_UID is outside the ebb);
  a plain `x2 = 0x11` or `x2 = x + 9` at the tail top is folded by cprop pass 1 (x = 8 is available) and never PRE'd.
  (3) Increment order `y++, no++, i++`: gcse inserts the PRE'd `+1`s in expression-index (= first-occurrence) order,
  sched1 keeps that LUID order (equal priority, weight 0), so i+1 is computed last in every block and is the shortest
  temp -> allocated first -> r26 shared with `i` (in place), no+1 r25, x*8 r24, y+1 r23; y (shorter than i) is allocated
  before i and takes r31 (shared with `&name`). The UNKNOWN-col block's `addi no+1` before the call is sched2: the tail
  base shares no's r27, so `li r27,0x11` gives no+1 an anti-dependent (+1 priority) there and in the else block.
  (4) The tail base (22 refs/120 insns, priority 0.73) is the first call-crossing allocno and takes r31 in pass 1; the
  target has it in r27 (r31 = y + &name). No natural form lowers it (its refs are the 9 tail uses + 2 insertions);
  applied `register int pin asm("r27"); asm("" : "=r"(pin)); asm("" : : "r"(pin));` at the function top (tagged
  `candidate #17`): r27 becomes used-so-far, the base takes it in pass 0, &name r31, col r30, y14 r29, obj r28, no r27
  (pass 0, dead in the tail) and the rest cascades. Also `register ScrollWork* wb asm("r9"); register u32 hib
  asm("r11")` on the pass-8 asm-high pair (tagged `#13`: as pseudos local-alloc ties the pointer to the dying high's
  r9; the original's REG_EQUIV high is reload-materialised into r11 after the pointer got r9).
  (5) `int cx = (x2 + 3) * 8; asm("" : "+r"(cx)); eprintf(cx, ..)` for the x54 eprintf only (tagged `candidate #1
  (arg copy)`): ours issues `lwz r8,x54` before `addi r3,x2,3` because the load has TWO dependents in the block (the
  call and the next eprintf's `lbz r8` = REG_DEP_OUTPUT: under 2.95.3 a call records call-used regs as CLOBBERS, so
  reg_last_sets[r8] still points at the load) while the addi has one (its shift); the launder keeps the argument copy
  `r3 = cx`, +1 priority for the addi chain. The x12F/x135 sites are `lbz` first in the target too (2 dependents).
  Zero-code alternatives (a `u32 v` local, the copy-free form) leave the tie.
- **plmove10 (t_atari, 19 -> 0, unit flipped, zero code): `GlobalWork*& gp = pG; Draw_local_pos(&w->pos, 1000,
  gp->Cam.viewMat)`.** The target's copy stores (`stw`) come after the three RMW `stfs` because its `lwz r5,pG` DEPENDS
  on the stfs stores (true dep, latency 2): the stores then have two dependents (call + load) and outrank the equal-
  priority copy stores in sched1. Ours exempted the pG load through alias.c's `fixed_scalar_and_varying_struct_p` (a
  plain `pG->` read is MEM_SCALAR_P at a fixed address, the stores MEM_IN_STRUCT_P at a varying one); a REFERENCE read
  is a MEM with neither flag (the pass-3 IRef finding) and gets the dependence. The "sched2-alias, not source-fixable"
  verdict of passes 7/9 was wrong: the sched1 order already differed. Rule: when a load of a global must follow
  struct-member stores through a pointer in the target, read the global through a reference.
- **SctrlAdjustAxisRange (db_sctrl, 20 -> 0)**: (a) the `.v` reads as `w->curve->key[i].v` while `.t` stays
  `c->key[i].t`: loop.c hoists the invariant `w->curve` load into its own pseudo, the two address givs then have
  different add_val registers and `combine_givs_p` cannot express one as the other + constant -> two stepped
  pointers (+0x14 and +0x18) as in the target (one giv with `-4(r9)` otherwise). cse2 later merges the hoisted load
  with the entry load, so no extra `lwz`. (b) `d = (fabsf(ymax) > fabsf(ymin) ? fabsf(ymax) : fabsf(ymin)) * 0.1f`
  instead of a named `m`: with the ternary temp the 0.1 pool load takes f13 and the select f0 (a named `m` is a
  multi-block pseudo global-alloc sees only after local-alloc gave the pool load f0).
- **drawScurve (db_sctrl, 55 -> 2)**: `k = &c->key[i]` inside the body instead of `k = c->key; .. k++`: the pointer is
  then a giv whose `addi rK,c,4` init is emitted in the loop PREHEADER (after the exit test) -- `k = c->key` before the
  loop puts it in the entry block, one insn earlier in every allocno's life, and i/k/&wp permute (r26/r27/r29/r30).
  Residue 2: `lis r19,pLog@ha` (VECNormalize's error arm) is hoisted in loop pass 1 in ours and in pass 2 in the target
  (the target's `addi r30,r28,4` giv init precedes it); its `savings 2 * life 6 * threshold 32 >= 204` in ours.
- **tcAdatInit (t_camera, 5 -> 0)**: `for (i = 0; i < 4;) { do { PSMTXMultVec(m, &a->pt[i], &a->pt[i]); i++; }
  while (0); }` (tagged `tie`): the phony do-while doubles the body's REG_N_REFS (the pt giv then takes r31 ahead of
  `a`/`poly`; a plain loop permutes r27/r28/r31) and with the increment INSIDE it the giv's `addi r31,r31,0xc` is no
  longer behind the LOOP_END anti-links and moves above the `bl` like the target's.
- **editDataDraw (t_snd_vol, 2 -> 0)**: the #1 floats-first alias `void markDrawF(f32 dist, s16 val, u32 col, int
  kind) asm("markDraw__FsUlif")`: the target loads `lha val` before `lfs dist`; the argument evaluation order follows
  the parameter order, same registers.
- Left, mechanisms read (do not retry the listed forms):
  - tcSetBesideOffset (t_camera_data, 27): loop-1 givs 207 (`n*12+0x18c`, 9 refs/84) vs 209 (`n*4`, 9/82): the target
    allocates 207 first (r3) although it is born one insn earlier (the giv-init order is loop.c's and identical);
    body permutations (at-first, interleaved, fovy-first, `k = n++`, pointer/temp forms, declaration orders, u32 n)
    27-89. A pin cannot separate them (both live through the whole loop). Loop 2's permutation cascades from it.
  - DbSctrl (2): sched2 issues `stw r4,8(x)` before `stw r5,0xc(y)` because the x store has an anti-dependent (`lwz
    r4,pG`) -> higher priority; the target has y first with the same insns; the statement orders change nothing.
  - drawAxis (22, two identical blocks): the two `fix_truncdfsi2` loadaddr pseudos (`unspec[0]`, "=b") get r3/r4 in
    ours and r9/r11 in the target (the second is `mr r11,r9` = `mr r4,r3` in ours, reload_cse's copy of the first); r9
    is free in the block, yet local-alloc skips it. `int sy` first, no locals, f32 temps: 22-47.
  - sctrlMenu (104): the log10 argument select: the target has both `fabs` arms directly in f1 (the DF call argument)
    and a/b in f11/f12; ours f0 + `fmr f1,f0` (set_preference strips SIGN/ZERO_EXTEND but not FLOAT_EXTEND, so the
    SF->DF extend gives no f1 preference). Double `fabs` asm forms put the arms in f1 but add `fmr` extends for the
    compare (`fabsd` 107, cmp-float/arms-double 99, `register f32 m asm("fr1")` 104). The string highs r20/r21/r22
    and the case-arm `li r6; extsb; addi r7` order are a second, untouched group.
  - tcCameraCopyPoint (13): the four `x629` test blocks are cross-jumped down to 2 in the target and 3 in ours (A1e
    into A2t fails); arm polarity swaps in the source are normalised by the front end (13 in all four).
  - tcNextAdatPtr (2), editScreenDisp (11: the `extsh`/`subi` operands are `(subreg:HI (reg:SI))` so combine_regs
    would refuse the tie -- ours still shares r0/r11, the target r10; `int` temps are folded back), tvib_R0_VibLoopSet
    (32: the cse2 fresh-`lis V@ha` family, 8 loads share one high in the target -- the asm-high pair x8 would do it).

### Tool RELs, bytes-first pass 11 (db_sctrl 18->21/22 x2: DbSctrl, drawScurve, drawAxis 0 words; t_camera tcNextAdatPtr 2->0 (52->53/57); t_snd_vol editScreenDisp 11->2; t_se_at ToolSeAt 2->0 (12->13/20); t_id idEditSize 22->12, idEditRot 18->12; nothing flipped; 2026-09-10)

- Harness ~/.cache/tools_p11 (tools_p10 copies with the paths rewritten; `tryv.py MOD/UNIT FUNC V.py` with
  `SRC=src/tools/db_sctrl.cpp` for the shared unit and `EXTRA=-DTOOLS_ARRAY` for t_id; `vsbs.sh MOD/UNIT FUNC CUR|VARIANT [ctx]`
  -- NB its left column is the TARGET, the right column ours: read `T`/`O` markers, not positions). em39.rel failed once
  at the end of the pass (another agent's edit 3 min old); every module of the units below was OK (110/111).
- **Zero-code levers (all verified byte-identical):**
  - DbSctrl (db_sctrl, 2 -> 0): `dbSctrlScreenOrientation(w, &pGS->Cam, pGS->Cam.param.fovy)` -- the struct-view pG read
    (global.h) makes the `lwz pG` depend on the three preceding member stores, so `stw r4,x` loses the anti-dependence
    bonus of the later `lwz r4,pG` and the stores come out in RTL order (y, x). The plmove10 reference-read rule again;
    `GlobalWork*& gp = pG` works too, `pGS` is the existing idiom.
  - drawScurve (2 -> 0): the two VECNormalize macro uses written out with `pLog.p->err(...)` (no `operator->` inline).
    The macro's `pLog->err` inline leaves BLOCK_BEG/END notes between the `lis pLog@ha` and its `lwz`; loop.c counts
    LUIDs over notes, the combined movable (two arms, `combine_movables`) had life 6 / savings 2 and pass 1 hoisted it
    (`32 * 2 * 6 >= 204` real insns). With the plain member read the life is 2, pass 1 fails (128 < 204) and pass 2 (168
    insns, threshold back to 71 - 3 per move) hoists it AFTER pass 1's giv init (`addi r30,r28,4`) and before the two
    string highs, as the original. Rule: a `lis sym@ha` that the target hoists in loop pass 2 while ours hoists it in pass 1
    -- check `-dL` for `move-insn savings S` and the life; every BLOCK note between the lis and its use adds one LUID.
    (tcDataImport needed the OPPOSITE form, `pLog.p->err` inside a record loop -- read the dump, not the rule.)
  - drawAxis (22 -> 0): `int sx; int sy;` at FUNCTION scope, assigned in both label blocks. A block-local `sx` is a
    local-alloc qty and the fix_trunc load `(set sx (unspec [fpmem P] 16))` ties the dying stack-slot address pseudo P
    ("b" input, `*fix_truncdfsi2_load`) to it through block_alloc's operand tie (combine_regs: both pseudos, P local,
    dies once), so P inherits sx's r3/r4 argument suggestion (`mr r4,r3` reload_cse copy). A multi-block sx has
    `reg_qty == -1` (no tie), global alloc still gives it r3 through the `subi r3,sx,0x48` PLUS preference, and the two
    P pseudos are allocated alone: r9, r11 (`mr r11,r9`). Rule for the fpmem loadaddr family: make the fix_trunc result
    a multi-block pseudo.
  - ToolSeAt (t_se_at, 2 -> 0): `SeAtWork*& wp = seAtWk.p; wp = Debug_alloc(..); memclr_asm(wp, size);` in a block.
    With `pW = alloc; memclr_asm(pW, ..)` (struct-member macro) cse1 forwards the re-read into `r3 = P` BEFORE the store
    in sched1's order, P conflicts with r3 (-> r0, `mr r0,r3`), and the copy `mr r3,r0` survives reload_cse (SN's
    reload_cse_record_set copies SREG's value list into DREG but never records DREG as a value of SREG, so `r3 = r0`
    after `r0 = r3` is not a noop for it) until jump2 -- one extra dependent for `mr r0,r3` in sched2, which then beats
    `lis seAtWk@ha` on the equal-priority tie. The reference form keeps the shape the original had (lis first).
  - idEditSize / idEditRot (t_id, 22 -> 12 / 18 -> 12): `for (j = 0; j <= 2; j++) { ofs = j * 4; ... eprintf(x + 0x40 +
    ofs * 8, .., *(const char**) (ofs + (u32) tbl)); }`. `ofs = j * 4` is a DEST_REG giv with TWO uses, so combine_givs
    is allowed to fold the name-address giv into it (`lwzx r8,rOfs,rTbl`, `addi rOfs,rOfs,4`) and its `li rOfs,0` is
    emitted by strength_reduce AFTER the hoisted invariants (`li i+1`, `addi rX,x,0x40`) -- the target's preheader order
    that no for-init form gives. A single-use `ofs = j * 4` (pass 8: 35 words) is never combined ("If a DEST_REG GIV
    is used only once, do not allow it to combine": the address giv then gets its own stepped pointer, `lwz 0(rG)`).
    Left in both: `tbl` (15 refs / 180 insns, 0.250) vs the PRE'd `i * 0xE` copy (11 / 133, 0.248) -- the target
    allocates the copy first (r24) and tbl r23; a 1-2 insn length change would flip it (no natural form found), and
    idEditRot's case-0 `addi r31,r31,1` before the last eprintf (the original had no block boundary between that call
    and the latch, i.e. its 5 extra loop insns are not our dead test; dead tests before the call: 65-73 words).
- **Tagged forms applied:**
  - sctrlMenu (db_sctrl, 104 -> 45): `register f32 m asm("fr1"); if (fabsf(a) > fabsf(b)) m = fabsf(a); else m =
    fabsf(b); e = log10(m) - 2.0;` (`COMPILER-DIFF: candidate #17 (FLOAT_EXTEND argument preference)`): both fabs arms
    go straight into f1 and a/b keep f11/f12 as the original; the ternary with the same pin was 104 (pass 10), `f64 md`
    with `asm("" : "=f"(md) : "0"(m))` ICEs (mode-changing tie), per-arm tied asms 52 (b tied into f1). Left (45): the
    `lwz joy->rep; andi.` pair issued after `frsp f31` in the target (ours right after the call: the load is ready at
    t+1 in both, the target's waited -- not understood), the three loop-invariant string/table highs allocated in the
    order ">" r22, col r21, "%s" r20, menu_name r19 (ours col r22, ">" r21, menu r20, "%s" r19: REG_EQUIV-doubled
    lengths 616/620/618 vs col 4 refs/98 -- undoubled highs (asm-high pairs, #13(c)) would give the target order but the
    volatile `lis` asm perturbs the whole block: 94), and the case-4 blink block's `li r5,0; extsb r4; li r6,0; add r4;
    addi r7` order (a 2-issue schedule in which only ONE free insn issues at t=2; ours fills both slots -- its two
    `li`s are in the same cycle; not modelled).
  - editScreenDisp (t_snd_vol, 11 -> 2, `COMPILER-DIFF: candidate (local-alloc qty order)` x4): loop 2 `register int
    v2 asm("r10"); register int t asm("r0"); t = base + i * 0x14; v2 = (s16) t;` (the sum in r0, the extsh into a fresh
    r10: the target does not reuse the dying r0), and the post-loop block `register int b asm("r10")` for the pass-9
    `asm("mr")` copy (shares the dead work-pointer register) with `register int t asm("r0"); t = b - 4;` (not in place).
    Pinning only v2 drags the sum into r10 (tie), only b leaves `subi r10,r10,4`. Left 2: the pinned `subi r0` is issued
    before `li r30,0; lfd f0` (equal priority 12, LUID order after sched1 put the hard-reg set first; an asm `subi` or
    a keep-alive of b: 2-33).
- **Read, not closed:**
  - tcSetBesideOffset (t_camera_data, 27): the two loop-1 givs are BOTH loop.c pass-2 reductions of the inner loop's
    pass-1 preheader inits (`450: n*4`, `459: n*12+0x18c`; the outer list is prepended, so the later insn 459 is reduced
    first -> pseudo 207, born first, longer by 2 -> allocated second -> r31). Swapping the inner reduction order (a body
    with `u32 k4 = n * 4` computed before the pos/at copies and `*(f32*) ((u32) rb + k4)` stores through `f32* rb =
    c->roll` locals) gives the target's allocation (r3 = n*12) but also swaps the inits/increments in the RTL (the
    target has n*12 first everywhere): the original allocated the earlier-born, longer giv first with identical RTL, i.e.
    its REG_N_REFS/REG_LIVE_LENGTH differed (both 9 refs, 84 vs 82 here). Dead tests after the inner loop
    (`&c->at[n] == 0`, `o = 0` target) restructure the loop (63). Loop 2's `addi r7,r7,0xc` position is a second,
    independent sched difference.
  - seAtInit (t_se_at, 21): the `lwz pW` below the four Snd/save stores -- reference views of Snd (`SndWork& s`,
    `SeAtHead*& hd`), a `SndWork* s = &Snd` pointer, `GlobalWork*&`/`SeAtWork*&` views of the copy and the store order
    all 21-28: alias.c separates the two symbol bases whatever the flags, so the target's dependence is not an alias
    verdict (the pass-4 laundered-pointer form is the only one that reproduces it).
  - tcCameraCopyPoint (t_camera, 13): the four `x629` test blocks (A1 `!=0`/A2 `==0` for the i-1 arm, B1 `==0`/B2
    `!=0` for the i+1 arm, each with its own `x627++` copy and `b END` at jump2 entry, fall-throughs A1 and B1 in both
    builds) -- tools/research/xjump.py on this layout reproduces OURS exactly (A1's `b END` is the first simplejump, its
    fall-through candidate B2's tail matches 7 insns, so A1 merges into B2 and A2/B1 stay apart); the target merged
    the OTHER pairs (A2 into B1, B2 into A1). Under the stock policy that needs a different jump2-entry layout (an
    A1 tail that does not fall/chain into B2 first); `goto inc`/`goto skip` shared-increment forms (26-27) and the
    `else if` form (13) do not give it. Not closed.
  - tcDataExport (142): whole-function allocation from `buf` r29 vs r31, not iterated.
- **tcNextAdatPtr (t_camera, 2 -> 0, tagged `COMPILER-DIFF: 5 (sched2 tie: call-result copy vs compare)`)**:
  `asm("" : "+r"(suffix));` right after the `next_suffixI` call. The result copy `mr r4,r3` and the next block's
  `cmpwi cr7,dir,0` are both ready after the call; ours ranks the compare first (its branch adds priority), the
  original the copy. The launder is a same-block consumer of the copy (priority +1) and the LUID order does the rest;
  a plain `asm("" :: "r"(suffix))` use (2) and the volatile use (2) do not.

### Tool RELs, t_esp pass 2 (db_port EspToolInit 1224 -> 160 words; db_widget 107 -> 109/113: SetBase/SetSize 0, DB_PRIMITIVE ctor 82 -> 50; db_mod/t_esp/db_light untouched; nothing flipped; 2026-09-10)

- Harness ~/.cache/tesp2 (tesp copies with the paths rewritten: `tryv.py MOD/UNIT FUNC v.py` (`SRC=src/tools/db_mod.cpp`
  for the shared unit), `vsbs.sh MOD/UNIT FUNC CUR|VARIANT [ctx]`, `mdump.sh MOD/UNIT -dX` (`SRC_OVERRIDE=` for a variant
  source; `-fsched-verbose-6` puts the ready lists + per-block visualization into `dump/<unit>.i.sched`), `minidump.sh
  /abs/path.cpp -dX` for stand-alone probes). Build 111 OK before and after every change.
- **EspToolInit (db_port, 1224 -> 160, all zero-code; the earlier "two spilled `&local` pseudos" reading was wrong):**
  - Frame 0x1e0 -> 0x1c0: `char name[0x30]` (not 0x50): the target's `Vec ofs` sits at 0x140 = right after `name`.
  - The `.sym`-less indices are readable off the pointer chains: `INFO7/INFO8` (not 8/9) in the `db_cutNo == 6/7/8`
    blocks and `INFO4` (not 5) for tbl3 -- count the `lwz 0x14` between `lwz 0x15c` and the RMW/`bl` at every site
    before believing an alignment-based sbs diff (a repeating `lwz 0x14` chain absorbs an off-by-one silently).
  - `int one = 1;` at the function top for `db_fog = one; db_emArray = one;` = the target's `li r27,1` hoisted into a
    callee-saved register (a pseudo set before the calls may cross them; a literal `1` is a block-local `li r10,1`).
  - `flagOn(u32 f, u32 bit) { int on = 1; if ((f & bit) == 0) on = 0; return on; }` (the evtToolOn shape) for every
    flag test of the function: `li r11,1; cmpwi/andis.; bcc; li r11,0; cmpwi r11,0; beq` x4 (EvtDebug.flags bit31,
    pModel[i].flags bits 29/31/30).
  - No `m` pointer: every field is `EvtDebug.pModel[i].field` (`#define M`); the target reloads `pModel` per use and keeps
    only the PRE'd `i*0x644` (`mr r26,r9` after `mulli`). `nBin = M.nBin` local -> the j loop is reversible
    (`subic. r27; bne`) with the bin/tpl offsets as givs (`li 0x30/0x330; addi 0x30`); a re-read bound keeps `cmpw`.
  - `nameIs4` must be a MACRO (an inline's `&&` chain ends in a setcc: `xori; subfic; adde; cmpwi`); `EVT_CUT_NO(buf3)`
    = a macro over the caller's buffer in the order `[0] = 0x49, [1] = 0x4A, [2] = 0`, used at the top too (the target's
    `lbz 0x4a; lbz 0x49; stb 9; stb 8` local-alloc order needs that statement order); `texBlendSet` a MACRO with a plain
    `{ }` body (the info chain is re-walked per call; a `do {} while (0)` body ends cse's path and splits `&tbl1` into two
    pseudos, which loses the loop.c hoist + the `.rodata` pool spill of `&tbl1`).
  - `StrCpy(char* d, const char* s) { strcpy(d, s); }` inline wrapper for the `name` copies only: integrate substitutes
    `&name` into the r3 arg set -> fresh `addi r3,r1,0x110` per call while `strcat(path, name)` keeps the PRE'd r18. The
    source operand goes through a `char* src = M.name;` local (`NAME_SET`): the direct expression gives
    `add r4,rOFS,rPM` (mult first), the local `add r4,rPM,rOFS` like the target (and the j-loop givs keep `0x30`
    folded into the init only with the local).
  - Parent arm: `idx = (s8) pad[0]; parent = (s8) pad[1]; Vec ofs = {0,0,0}; parentModel = (int) pModel[idx].pModel;
    dbModelParentChild(.., (s8) parentModel, ..)` -- both pad reads BEFORE the memset (the `ofs` address then lives < 25
    insns and is not loop.c-hoisted; the `parent` store after the second read reloads pModel), and the `(s8)` of an INT
    LOCAL gives `lwz 0x634; extsb` (combine cannot narrow `(sign_extend (subreg:QI (reg)))` through a load whose
    `extendqisi2` operand must be a register); `(s8) (int) EvtDebug.pModel[idx].pModel` narrows at expand to `lbz 0x637`.
  - `int* pParent = &parent;` as the FIRST declaration of the loop body (before the `static DB_MODEL_FILES` guards):
    loop.c hoists it (spilled to 0x170); declared after the guards its set is `maybe_never` (a `bne` precedes it) and
    used in another block -> not a movable.
  - Two `if (db_cutNo == 8)` in a row with the be_flag RMWs written `BitOn/BitOff(INFO7(em)->be_flag, 8)`: the reference
    store (MEM with neither flag) invalidates the `db_cutNo` load in cse, so the second test survives with its reload;
    plain `|=` (in-struct store) keeps the load and folds the second test even across the AROUND path
    (`invalidate_skipped_set` -> `invalidate_memory` does not save it: the fall-through equivalence is recorded anyway).
  - `BitOn(pG->flags_5014 ..) x2` BEFORE the EspEvModList zero loop (memory order), the loop through
    `list = EspEvModList; list[room] = 0` with a `u32 room` counter (count-up `mtctr`; the pointer local puts the
    `lis/addi` at the block top instead of loop.c's preheader position), `u32 nLit = LightMgr.nArray` + a block-local
    `int k` for the light loop (`li r10,0` right before the loop: a counter that crosses no call is not hoisted above
    the LightMgr calls; the function's `i` is). `p = dbModGetEmPtr(slot); list = EspEvModList; if ((u32) slot <= 0x7F)
    list[slot] = p;` (the call unconditional, the `lis/addi` before the compare). `info = em->pInfo; b = &info->bound;
    lit = M.x639;` locals before `size.x = b->size.x ..` (pInfo once, `&bound` pseudo, x639 read before PSVECSubtract).
  - Residue 160 words: (a) the init store block `db_fcvData/fog/emArray/cinesco/workPushed` -- the target issues it in
    source order with the `one`/zero pseudos NOT dying at their last stores (#13 dying-store family: SetRock); every
    keep-alive asm (`asm volatile("" :: "r")`, an output-less asm is volatile too) is a sched barrier that also delays the
    `li r4,1; li r3,1` DB_WorkPush args (the target issues them at t=3 in the `stb`'s stall slot); (b) the parent arm is a
    FRESH cse ebb in the target (`add r9,r26,r4` recomputed from the PRE'd product + the test's pModel register; ours
    reuses the sum); (c) the j-loop `&EvtDebug` is a fresh `lis/addi` in the target, a cse copy `mr r28,r30` in ours;
    (d) `li r14` = the zero (target) vs the 0xF7 (ours) of the nine texBlendTbl sites: both hoisted QI constants have
    19 refs, lengths 3378/3376 -> `760000/len` truncates to 224/225, so the shorter (0xF7) wins; the target's loop had a
    length making them tie (then the lower pseudo = the zero wins); (e) the `stw 4(r30)` template-copy order in the j
    loop and the callee-saved permutation that follows from (a)-(d).
- **SetBase/SetSize (db_widget, 9+16 -> 0, zero-code):** `DB_POINT* b = &base; b->y = y; base.x = x; rect =
  DB_RECT(base.x, base.y, size.x, size.y);` -- the `.y` store through a struct POINTER is `(mem (plus b 4))`, a different
  hash from the re-read `(mem (plus this 0x40))`, so cse1 keeps the load (sched1 then issues the store first: the
  load->temp-store->copy chain outranks the size loads) and reload_cse turns the load into `fmr f0,f2` after reload has
  made both addresses `0x40(r3)`; the `.x` store must precede the rect copy (its output dependence on the r11-based rect
  stores otherwise sinks it below them). An inline `FSet`/reference parameter is substituted by integrate and folds again.
- **DB_PRIMITIVE ctor (db_widget, 82 -> 50; frame and r29 now right):** `DB_RECT* r = &rect; r->x = fz; r->y = fz;
  r->w = fz; r->h = fz;` (rect.x via `this`, y/w/h via the pointer = the SetBase temp shape), `f32 fz = 0.0f; int iz =
  0;` locals for the first store block in the target's order (pos, drawPos, rect, base.y, flag, size, base.x, type..next,
  id), and `int* p = click; p[0..2] = 0;` inside the LAST click loop: `&click` is then PRE'd into the first block
  (`addi r29,r31,0x4c`, the extra callee-saved register) and copied into the loop (`mr r9,r29`). Left: the target's
  `stwx r0,r9,{r0,r10,r11}` (offsets 0/4/8 in registers -- loop-hoisted constants; no form found), base.x/id issued last
  in the first block although fz/iz die there (#13; `asm volatile` keep-alive costs the preheader interleave, and
  reusing fz/iz later loses the second pool load `lfs f31`), and the second block's temp-copy order.
- **Mechanisms read, not closed:** DB_WINDOW ctor (14): the DB_COLOR temp copy's source address is a pseudo copy of the
  frame register (`(set (reg 120) (reg 78))` in the .rtl, temp at frame offset 0) that cse rewrites to the ctor's `this`;
  `*(&color) =`, memcpy, a `DB_COLOR* pc`, a `DB_COLOR c; c = ..; color = c` all give the same 14 (or 42). Save*FileNo x5:
  the target's `cmpwi 0; cmpwi 0xff` need the first `bge` to reach jump2 as a jump-to-following or as
  `bcc X; b X`; every layout with a dead `b` between (`else if`, third arm, gotos) is inverted by the jump-around-jump
  rule into `blt L2` (3 words), and a `goto` to the next label is deleted by jump1 while the arm is still alive.
  position_usage: Back/B duplicated into both arms (jump2 cross-jumps them) gives 67 (worse than the 34/44 shapes);
  the `mr r29,r30` PRE copy needs the tail's `x*8` deleted by gcse, which the isolated-occurrence rule forbids.
  MakeLoadSeqData `lhzx head,i2`: `u16* cnt = (u16*) head` locals (top / after the call / in the loop), `*((u16*) head +
  i)`, `*(u16*) ((u32) head + i*2)`: 11 (the operand order stays idx-first; `d` in-loop local 16).

### Tool RELs, bytes-first pass 12 (t_id 53->57/69: idEditSize, idEditRot, idEditMark, toolIdSpace 0; db_sctrl sctrlMenu 45->32 x2; t_snd_vol editScreenDisp 2->0 (20->21/27); t_rck rckDrawPointLineNow 7->0 (28->29/33); t_vib tvib_R0_VibLoopSet 32->0 (24->25/29); nothing flipped; 2026-09-11)

- Harness ~/.cache/tools_p12 (tools_p11 copies with the paths rewritten). NB `-fsched-verbose=N` is an INVALID
  option for this cc1plus (it aborts silently after the cpp step and the previous dump is read): the spelling is
  `-fsched-verbose-N`; level 5 prints the `Region Dependences` tables (per-insn prio/dependents per region).
- **tools/research/xjump.py fixed: the jump_chain is searched only for labels that existed at jump2 entry** (jump.c: `if (INSN_UID
  (JUMP_LABEL (insn)) < max_uid)`); a label do_cross_jump creates (X<n>) is never a chain candidate. tcCameraCopyPoint's
  target layout = the i+1 arm with the `v.x >= 0` arm FIRST at jump2 entry (A2 -> B1 whole, then B2 -> A1 whole via the
  fall-through of the OLD label) -- but every source polarity (`if (v.x >= 0.0f) X else Y`) gives the same 13 words: the
  RTL has `!(>=)` first in all four (jump1's `bcc L1; b L2; L1:` inversion + arm order); the layout lever is unknown.
- **Zero-code levers (verified byte-identical):**
  - idEditSize / idEditRot (t_id, 12 -> 0 each): ONE table pointer per switch arm (`tbl`, `tbl3`, `tbl4`, each set in
    its own arm; a shared multi-set `tbl` had 15 refs/180 insns = 0.250 and beat the PRE'd `i * 0xE` copy 11/133 =
    0.248 for r24; three single-set pointers (5 refs) come after it and share r23). A single-set pointer set inside a
    case arm is NOT hoisted out of the i loop (pass-6 fear unfounded); the LAST arm's set belongs in the for-init
    (`for (j = 0, tbl4 = texFixName; ..)`) so `li j,0` precedes the `lis` in LUID order (the other arms want the set
    before the loop). Dead `= 0` initialisers of the split pointers cost 500 words (a second set = hoist/cse changes).
  - toolIdSpace (t_id, 5 -> 0): a SEPARATE counter for the second loop. Reusing `i` from the while loop makes loop.c's
    check_dbra_loop emit the reversed biv's final value (`i = 0xC0`, REGNO_FIRST_UID(i) != the for-init) after
    LOOP_END; flow deletes it but find_basic_blocks already made its block, whose only successor is EXIT -> the leaf
    rule kills every haifa region of the function (no `cmpw cr7` speculation in the while loop). With `int j` the
    regions form; `if (.. && p->no >= no) p->no += n;` (compare before the add in LUID) then lets the region
    scheduler speculate the `add` above `cmplw` into a fresh r11 (`int v = p->no + n` before the if: the cmp wins the
    sched1 weight tie and local-alloc ties v to the dying temp, `add r0,r0,r30` after the compare).
  - rckDrawPointLineNow (t_rck, 7 -> 0): `RckLine* row = w2->line[w2->near]; ((RckLine*) ((u32) row + (w2->lineStart
    << 2)))->to` = `w + (near << 9) + 0xad4` then `lhax r0,row,ls<<2` (the plain 2-D index folds 0xad4 into the base and
    sums the two index terms; `row[ls]`, `(u32) row + ls * 4`, `ls*4 + (u32) row` give the lhax operands swapped).
  - tvib_R0_VibLoopSet (t_vib, 32 -> 0): `TvibWork* v = V;` at the function top, used by the FIRST arm only (the other
    statements re-read the `V` macro): the target's `lwz r11,tvib@l(r10)` above the first `beq` is an unconditional
    read, not a speculated load (-fsched-spec-load is off), and with it the PRE'd high r10 stays the reaching register
    at every later site (the arm's own `lis` was the cse2 fresh-`lis` of pass 10).
  - editScreenDisp (t_snd_vol, 2 -> 0): the pinned `t = b - 4` moved BEFORE the `pt[1]` stores (after `pt[0].z = 0`):
    the hard-reg `subi r0` then follows `li r30,0; lfd f0` in sched1's LUID order.
- **Tagged forms applied:**
  - sctrlMenu (db_sctrl, 45 -> 32): (1) `.LC` label-hash lever (`candidate (gcse PRE pseudo numbering)`): 18 dead
    `f32 lcN = K;` locals move the "%s" string from .LC22 to .LC40 (bucket 45 < sctrl_menu_name's 201 in the 519-bucket
    table; .LC1x/.LC0x/.LC4x/.LC5x all qualify, 17 locals do not) so its PRE pseudo is numbered before menu_name's and
    wins the equal-priority global-alloc tie (3 refs / 618 vs 620 both truncate to 48) -> "%s" r20, menu r19. (2)
    `asm("" : : "r"(col))` after the loop (`#13`, REG_EQUIV live-length doubling): col 4 refs/98 insns (0.0816)
    outranked the ">" high 11/616 (0.0536, doubled from 308 by update_equiv_regs); the keep-alive stretches col to
    the loop exit (6 refs / ~300) and ">" takes r22, col r21. A hard-register `col` (r21) is wrong: the `(u8) col`
    temp then reuses r21 and everything cascades (190). (3) `do { } while (0);` after `e = log10(m) - 2.0;` (`#13
    region split`): the `lwz joy->rep; andi.` pair waits behind `frsp e` as in the original. Left (32): the case-4
    blink block `lbz; addi r3; li r5; extsb; li r6; add; addi r7` -- at t2 only `li r5` issues although `li r6`/`addi
    r7` are ready (the case-0 copy of the same call has `li r5, li r6` at t2 and `addi r7` only at t4 with the add):
    `(u8) col` / `col` / `int zero` / operand-order / `int s = w->sub` forms 32-40; with `w->sub + i` (add operands
    swapped) the cross-jump becomes the target's but the add is `add r4,r4,r26` (29). No X insn explains the empty
    slot; not modelled.
  - idEditRot case-0 loop (t_id): the dead test is replaced by `asm("" : : "r"(d));` before the eprintf + four dead
    `col = 7/6` sets after it (`#3 (loop.c pass-2 insn_count)`): dead SETS of a variable that is re-set before its
    next use survive delete_trivially_dead_insns (the variable has uses), are counted by loop.c and deleted by flow --
    the +5 real insns without a block boundary that the original loop had (its `i++` is scheduled before the eprintf
    call); the codeless use of `d` keeps its extra ref (d r26 ahead of `x + 0x18` r25). idEditMark (2 -> 0): the same
    five dead `col` sets at the loop-body end (pass-1 threshold 65 < 66 keeps the "%02X" high for pass 2, after the
    yy giv init). Both loops of the original had exactly 5 more insns than ours -- a common source shape not found.
- **Read, not closed (do not retry the listed forms):**
  - tcSetBesideOffset (t_camera_data, 27): the two loop-1 givs tie only by REG_LIVE_LENGTH 84/82 (27/84 = 3214 vs
    3292 after the 10000 scaling; equal lengths would give 207 first by allocno number). The preheader inits are
    adjacent (+1) and the other +1 is not in the RTL order; `i <= 3`, `j <= 2`, if/else `o`, `++i`, u32 n, s16 n
    (81), fovy-before-roll (31), `n++` in the for header (32), a dead `i == 7` test (95): 27 in all natural forms.
  - seAtInit (t_se_at, 21): a volatile `pW` load does not wait for the four Snd stores -- haifa's true_dependence
    orders a volatile read only after volatile STORES (`MEM_VOLATILE_P (x) && MEM_VOLATILE_P (mem)`), and the symbol
    bases differ otherwise; `asm("lwz %0,%1" : "=r"(w) : "m"(seAtWk.p))` 19, with `"m"(Snd.se_at)` 23; a reference
    view 21.
  - toolIdEditDisp (t_id, 4): the second `row = 0xC` must issue after `cmpwi r0,0` (t3, a dependence on the `lbz
    dispTop`), while the first `row = 2` at the function top issues with its lbz at t1 as ours; ternary / if-else
    row (jump1 hoists the set the same way), `r2`/`cx2`/`yy` locals for the CLIPBOARD eprintf 4-15.
  - idEditId (t_id, 12): the h * 480 / 448 + 0.5 chain: the target ties the fmuls result to h (f0) and the fadds
    result to the 0.5 register (f13); ours ties fmuls to the dying 480 (f13) -- the constant pseudos' creation order
    (`f32 k480 = 480.0f` before h) does not change the tie (15-22); `h + 0.5f` inside the cast 14.
  - t_block tBlockAreaInfo_Menu case 0 (11): rep2 r9 / n r11 local-alloc order unchanged (pass 3/4 forms).
  - tcDataExport (142) not iterated: buf r29 vs r31 -- two higher-priority allocnos took r31/r30 in the original.

### Tool RELs, t_esp pass 3 (db_port Matching 68/68 + `t_esp/db_port.cpp` flipped; db_widget 109 -> 110/113: DB_PRIMITIVE ctor 50 -> 0; t_esp 193 -> 194/212: SetEditTblColor 2 -> 0, MakeLoadSeqData 11 -> 9; db_mod/db_light untouched; 2026-09-11)

- Harness ~/.cache/tesp3 (tesp2 copies with the paths rewritten; `tryv.py MOD/UNIT FUNC v.py`, `vsbs.sh MOD/UNIT FUNC
  CUR|VARIANT [ctx]`, `mdump.sh MOD/UNIT -dX` with `SRC_OVERRIDE`, `-dS/-dR -fsched-verbose-6` for the sched1/sched2
  ready lists and the per-insn `prio` / dependents table). The compiler source is `tools/sn-gcc/src/gcc/` (the private
  copies under ~/.cache/em2b39_p8 are gone). Build 111 OK before and after every change.
- **EspToolInit (db_port, 160 -> 0; the unit is Matching). Six independent mechanisms:**
  - (a) Init store block, source order `fcvData, emArray, fog, cinesco, workPushed` (the `lis` order is statement order:
    fcv, emArray, fog, cinesco) with `asm("" : "=m"(*(u16*) &db_fcvData));` after the three stores (tagged `#13`): the
    input-less asm with a different-mode view of the fcv store is an output dependence that gives that store +1 priority,
    so it stays first although `one` dies at the fog store (weight -1 would lead in ours); fog then precedes emArray by
    weight and cinesco by LUID; the workPushed store is last in sched2 by dependents (2 vs 14). Rules read on the way:
    `INSN_REG_WEIGHT` is +1 per SET/CLOBBER (MEM dests included) and -1 per REG_DEAD/REG_UNUSED; every store gets an ANTI
    link to the next call (`flush_pending_lists`), so scalar-global stores tie on priority and only weight/dependents/LUID
    order them; the five stores wait for the last `pG` flag store because BitOn/BitOff MEMs carry no struct flag (13
    output/anti links each). An anchor on any store other than the first-issued one reorders the block (the anchored store
    gets the +1); an anchor with a `"r"(one)` input must be placed after the last `one` store (the death moves to the asm).
  - (b) The parent arm: `prod = i * sizeof(EvtDebugModel); asm("" : "=r"(rr), "=m"(buf3[2]) : "0"(prod));` at the body
    top (tagged `candidate #12 (AROUND form)`), the flags test reads `*(u32*) (prod + (u32) EvtDebug.pModel + 0x63C)`, and
    every other model field goes through `rr`: field reads `(*(EvtDebugModel*) (rr + (u32) EvtDebug.pModel)).f` (sum
    `add rX,r26,rP`), addresses passed to strcpy `((EvtDebugModel*) ((u32) EvtDebug.pModel + rr))->name` (`add r4,r4,r26`:
    the natural EXPAND_SUM order of `&pModel[i].name`; a MULT operand is moved first by expand, a REG operand keeps its
    place). The target's both arms recompute `pModel + i*0x644` from gcse's reaching-reg copy of the product although the
    arms are on cse's AROUND path (pModel itself IS folded: `r4` reused) -- with the flags test on the path, cse1 AND cse2
    fold the sum (cse2 canonicalises the reaching reg back through the copy), so no zero-code form exists: forced labels
    (`&&L`, static label tables) invalidate the enclosing loop in loop.c, a dead `do {} while (0)` ends only cse1's ebb, an
    `asm("" : "+r"(i))` in the skipped block kills the mult's availability for gcse (264). regmove splits the tied asm
    into `rr = prod` + the asm; the `"=m"(buf3[2])` output makes the flags load a dependent (asm prio 5, copy 6 > add 5),
    so sched1 issues the copy before the add and `prod` dies at the add, tied into the sum's r9 like the target (a tied
    asm alone: copy after the add, prod r0, 51 words; an asm `mr` is a cost-1 consumer issued a cycle too early: 16).
  - (c) `&EvtDebug` of the pScr test and of the j loop (#13, both tagged): the pScr block's is an asm-lis/addi pair
    (`ed0`; `lis r9; addi r9,r9; lwz 0xe0(r9)`: the target's lo_sum pseudo had two uses -- the j loop's cse copy, which
    reload re-materialised as `lis r9; addi r28,r9` -- so its offset is not folded into the load), the j loop's is a
    DISTINCT SYMBOL_REF `extern EventDebug EvtDebug_j asm("EvtDebug")` (the "*EvtDebug" string differs by strcmp, so
    cse/gcse/alias treat it as another object; an asm pointer has no alias base and its loads then wait for the buf3 frame
    stores). `nBin` is read through the pScr block's view (cse folds it to that block's sum, the target's `lwz 0x630(r9)`).
  - (d) `*pStage = (hi << 4) + c % 10` with a `u8 hi` set in BOTH blocks: a single-set quotient has nonzero_bits <= 0x1F and
    combine drops the target's `clrlslwi 24,4` mask. `l->be_flag &= 2` (sic, `rlwinm 0,30,30`), `s = M0->pScr` local.
  - (e) `la = (u32) EspEvModList; if (slot <= 0x7F) *(cModel**) (la + ((u32) slot << 2)) = p;` -- with a `cModel** list`
    the REGNO_POINTER_FLAG makes it the base and the index takes r0 (regclass `record_address_regs`: both operands
    unflagged -> both BASE_REGS -> index r9, the sum global r11 like the target); the shift keeps `la` first in the PLUS.
  - (f) The two "x:/soft/room/" template copies inside the j loop store word 1 LAST: read the following room id through
    the struct view (`G_ROOM_ID_S` = `*(u16*) &pGS->stage_no`): the pG load then depends on the copy's stores and the
    `stw r9,4(r30)` sits right before `lwz r9,pG`; with the fixed-scalar `pG` read the word-1 store's anti-dependence on
    the load ranks it first.
  - Read on the way: `base_alias_check` returns 0 for a stack ADDRESS (Pmode) base -- but after the prologue's `stwu`
    resets r1, `find_base_value` of a hard reg returns the REG itself, so frame stores DO conflict with argument-based
    (`this`) stores in sched2 (VOIDmode ADDRESS vs REG -> 1); and a hard register set more than once in the function has
    no base at all (the DB_PRIMITIVE temp pointer r7 vs the target's once-set r5).
- **DB_PRIMITIVE ctor (db_widget, 50 -> 0):** (1) block 1: `asm("" : "=m"(*(u32*) &pos.x) : "f"(fz), "r"(iz));` after
  `id = iz` (#13, anchored on the block's first store) keeps base.x/id last. (2) block 2 written `type; c += 0x10; id = c;
  parent..next = 0; id = c; rect = DB_RECT(..); flag = 0;` with `int& c = primIdCounter;` -- the reference MEMs carry
  neither flag, so the member stores order the counter load/store (the zero stores lead the block, the pool loads follow)
  and the SECOND `id = c` survives cse as the target's `lwz counter; stw id` re-read (a plain global read is forwarded
  from the store). The statement order matters: the re-read BEFORE the rect temp (JR: 2 words); PJRF/PRJF orders 34-42.
  (3) the last click loop: `int j0 = 0, j1 = 1, j2 = 2;` before the loop, `int z = j0 * 4; click[j0] = z; click[j1] = z;
  click[j2] = z;` inside -- the body is a fresh cse ebb, the `j*4` products survive to loop.c (hoisted), cse2 folds them
  to `li r10,4; li r11,8`, and the stored zero IS the index-0 product (one register, `stwx r0,r9,r0`). `p[j]` with `p`
  outside the loop puts the mult first in the PLUS and drops the `mr r9,r29`; a byte-offset form `(u8*) p + o1` hoists the
  address sum and folds it into a displacement.
- **Left in db_widget (mechanisms):** DB_WINDOW ctor (14): the DB_COLOR temp copy's loads are frame-relative in the target
  because its cse did not canonicalise the copy's base pseudo into the inline ctor's `this` (both `fp+8`; in DB_PRIMITIVE
  the target DID -- there the address is gcse's reaching reg r5 for both) -- `find_best_addr` prefers the higher-rtx-cost
  equivalent `(plus this 4)` over `(plus fp 12)`; named temps, do-while, memcpy, an r31-clobber asm (`mr r9,r11`, 33) do
  not stop it (cse2 redoes it). DB_STRING ctor (11) and DB_NUMERIC ctor (2) untouched.
- **t_esp:** SetEditTblColor `g = 1.0f; r = g; a = g; b = g;` (the copies' order decides which register takes the pool
  load: b must be the LAST copy). MakeLoadSeqData: `struct SeqCountView { u16 n[1]; }` and `((SeqCountView*) head)->n[i]`
  -- an ARRAY_REF keeps the base first (`lhzx r9,head,i2`), every pointer-arithmetic spelling is expanded with EXPAND_SUM
  (mult first); left (9): rec/t' allocation r5/r6 (pseudo 88 `head+0x30` 7 refs/28 vs the `t+1` giv 6/23) and the
  prologue copy order that follows from it. Save*FileNo x5 unchanged: the target keeps both `cmpwi` with the branches
  deleted by jump2 as jumps-to-following, which needs the dead arms non-empty at flow2 yet gone at jump2 -- nothing in the
  passes between (reload, reload_cse, sched2) deletes a register set; compiler-side as concluded in pass 5.
- Not iterated: db_mod (position_usage 34, IKreport 10), db_light (17/22/120/183), t_esp's larger residues.

### Tool RELs, bytes-first pass 13 (db_sctrl Matching in t_id + t_event: sctrlMenu 32 -> 0; t_camera tcCameraCopyPoint 13 -> 0 (53/57); t_rck 29 -> 32/33; t_se_at 12 -> 14/20: seAtInit 21 -> 0, AreaMove 22 -> 0; t_vib tvibFrameLineDraw 85 -> 67; 2026-09-11)

- Harness ~/.cache/tools_p13 (deleted at the end; scripts were dol15 copies plus module-aware `mtryv.py MOD/UNIT SYM V.py
  [--apply NAME]` (source, cflags and post-build read from build.ninja -- join the `$\n` continuations before matching
  `prodg_cc\s+(\S+)`), `msbs.sh MOD/UNIT SYM [ABS_OBJ]` (LEFT = target), `mdump.sh MOD/UNIT -dX` (`SRC_OVERRIDE` absolute),
  `mlens.py` (word count + the `-dl` refs/length line of every loop-carried giv), `layout.py DUMP FUNC` (compact label/jump
  layout of a -dR/-dJ dump for xjump work); `mcmp.py`/`order.py` take `MOD/UNIT` (split object under `build/G4BE08/<mod>/obj/`).
- **sctrlMenu (db_sctrl, 32 -> 0, tagged `#13 (free sched slot filler)`; unit flipped in t_id AND t_event, 111 OK).** The
  case-4 blink block's target order `lbz; addi r3; li r5; extsb; li r6; add; addi r7` is the 2-issue schedule with the t2
  second slot taken by a free insn that left no bytes (the Sscrn free-slot rule). Ours: t1 lbz+addi r3, t2 li r5+li r6, t3
  extsb+addi r7. A codeless NON-volatile asm `asm("" : "=m"(w->blink));` right before the `">"` eprintf is that insn in
  both passes: weight 0 (no register set), prio 3 (every insn of a block feeds the block's call: `sched_analyze` makes a
  CALL_INSN depend on all prior sets/uses of every register incl. pseudos), ready at t1, LUID before the arg moves -> it
  takes t1's iu slot, addi r3 slips to t2 with li r5, li r6 to t3 with extsb. Facts: an asm with NO outputs is implicitly
  volatile (stmt.c `if (noutputs == 0) vol = 1`) and a volatile asm is a full barrier in sched (`reg_pending_sets_all`)
  AND a gcse kill (the `i+1` PRE was re-inserted after it: +1 addi) -- always give a slot filler a `"=m"` output on a field
  the block re-reads later anyway (`w->blink`). The `#13` single-use-constant filler of the Sscrn pass does NOT work inside
  a loop: `recompute_reg_usage` weights REG_N_REFS by loop depth (set at depth 2 + use at depth 1 = 3 != 2), so
  update_equiv_regs neither substitutes nor moves it and the `li` stays (36-49 words). ASM insns cost 1 and consume an
  issue slot (`insn_cost`: INSN_CODE < 0 -> 1).
- **tcCameraCopyPoint (t_camera, 13 -> 0, zero code): the jump2-entry layout the target needs is a shared `p->x627++` behind
  `goto skip` with the `v.x >= 0.0f` arm FIRST in BOTH arms and a shared `TcWork* p` set in each test block** (`if (v.x >=
  0.0f) { p = PTC; if (p->x629 != 0) goto skip; } else { p = PTC; if (p->x629 == 0) goto skip; } p->x627++;` for the i-1
  arm, the x629 tests inverted for the i+1 arm, `skip:` before the shift loop). Why: at jump2 entry each arm then reads
  `bcc L1; T2: t; beq END; b Linc; L1: T1: t; bne END; Linc: inc` with Linc an OLD label, so (round 1) A1's inc merges into
  arm B's inc (`b Linc`), A2 merges wholesale into B1, and B2's `b Linc` finds A1's identical tail through Linc's jump_chain
  (redirect_jump prepends the redirected jump to the new label's chain) -> `bso A1`, B1 falls into inc: the target. The
  per-arm `p = PTC` load is what keeps the inc block's `lbz/addi/stb 0x627(r11)` on the tests' r11 (a plain `PTC->x627++`
  in the shared block reloads pTc into r9: 32 words); with the goto form in ONE arm only the other arm's copies still win
  (13-17); `if (!(v.x >= 0))` polarity in the goto form 17. Read with tools/research/xjump.py: its `bc` handling over-merges call
  tails (it turned a threaded `bge` into a 13-insn cross-jump the real jump2 never did) -- use the -dJ dump as the arbiter.
- **t_rck (29 -> 32/33, all zero code):** rckMakeSaveData 75 -> 0: (1) a running offset `u32 o = sizeof(RckHeader); hdr.hdrSize
  = o; o += nPoint*16; hdr.ofsLine = o; o += nLine*4; hdr.ofsNext = o;` -- one multi-set pseudo gives the target's r7 for
  the 24, the ofsLine and the ofsNext value (`addi r7,r9,24` is NOT tied to the dying `slwi r9` because the pseudo already
  has a quantity; a fresh `ofsLine` local is tied: `addi r9,r9,24`); (2) `p = (u8*) buf` assigned AFTER `memclr_asm(buf,
  size)` so the header copy stores through buf/p's own register and `p += 24` is `addi r31,r31,24` after them (the
  initialiser form let cse canonicalise the stores to buf and `addi r31,r28,24` ran early); (3) `RckLine* l = &RCK->line[i][j]`
  inside the j body for `l->to` and `*(u32*) l` (one giv, `lhax/lwzx r11,r10`; the plain 2-D index made two); (4) the total
  reuses `o` (`o = nPoint*16 + 24; o += nLine*4; o += nSq; total = o + 0x20; return total - (o & 0x1F);` -- fold turns
  `o + 0x20 - (o & 0x1F)` into `o - ((o & 31) - 32)`, the separate `total` statement keeps `addi r0,r7,32; subf`).
  rckPointDelete 121 -> 0: FUNCTION-scope `RckLine* l; RckPoint* pt;` reused by every loop (a multi-block pseudo is
  global-allocated and takes r11 after the local RCK load got r9; block-local pointers are local qtys ranked by refs/len and
  take r9), `l = &RCK->line[i][RCK->cur]` block BEFORE the `[cur][i]` block (the target's order), `int del; ... del = 0;`
  after the early `return` (the initialiser put `li r6,0` in block 0). rckSetNextPoint 184 -> 0: `best = start` BEFORE the k
  loop (the target never resets it per iteration -- a decomp error), pointer forms `RckNode* n`, `nb = &node[best]`,
  `RckLine* l`, the done test as `(n->done & 1) == 0` (`!(n->done & 1)` folds to `xori r0,r0,1; andi.; beq`), and in the
  relax loop `n = &node[i]` computed INSIDE the `if (l->to != -1)` block: computed at the body top, cse's find_best_addr
  rewrote `(mem n)` to `(mem (plus fp8 i8))` (ADDRESS_COST 0 everywhere, the tie-break takes the HIGHER rtx cost), gcse PRE'd
  `(plus fp 8)` (#3) into R with `r221 = R` copies, and cprop pass 2 propagated R only into the NEXT block's done load (no
  local cprop in 2.95.3) -> n's giv kept add_val r221, the done address got R, `combine_givs_p` refused (different add_val
  regs) -> a second index giv `lbzx r0,r6,r5`. In one block both stay r221 and combine into the stepping pointer.
  rckDrawPointLine 231 left (member-wise Vec copies via `lfs/stfs`, `w = RCK` after the `i == j` test, `w + (i*16 + 724)`
  hoisted offsets: 192 with the pointer forms; not finished).
- **seAtInit (t_se_at, 21 -> 0, tagged `#13 (memory anchor)` x3): one codeless `asm("" : "=m"(seAtWk.p) : "m"(loc))`
  right AFTER EACH store the `lwz pW` must wait for** (`seAtSaveHead = head; asm(.."m"(seAtSaveHead)); seAtSaveList = list;
  asm(.."m"(seAtSaveList)); asm(.. : "m"(Snd.se_at), "m"(Snd.se_at_list));`). The "m" input is a true dependence on that
  store, the "=m"(seAtWk.p) output an output dependence chain into every later pW load. ONE asm with all four "m" operands
  after the stores also orders the load (4 words) but extends the two save highs' lifetimes unequally (Head born t1, List t2,
  both dying at the asm -> List's qty ranks first and takes r10); per-store asms extend each high by one insn and the
  four qtys tie at refs/len = 1.0 -> pseudo-number order = the target's r0/r11/r10/r8. The pass-11 verdict "not source
  fixable" stands for zero-code forms; this is the r224 anchor idiom.
- **seAtAreaEdit_AreaMove (t_se_at, 22 -> 0, zero code): `Vec* rp = &right; rp->x = ..; rp->y = ..; rp->z = ..;`** for the
  one Vec the target stores through `addi r9,r1,8` (y/z via `4(r9)`/`8(r9)`, x frame-direct): cse's find_best_addr folds
  `(mem P)` to the frame address but leaves `(mem (plus P 4))` (the `(plus (plus fp 8) 4)` candidate is not a valid
  address); `up`/`dir` stay member stores. An inline `Vec*` setter gives 7 (integrate's parameter copy differs).
- **tvibFrameLineDraw (t_vib, 85 -> 67): the target's duplicated body head at the loop bottom (`i++; cmpw i,n; bge; fmr;
  cmpwi i,0; fadds; blt; lha frame_w; addi -1; cmpw; blt BODY`) is stmt.c expand_end_loop's exit-test rotation** (it moves
  the loop head up to the LAST conditional jump to `end_label` within 30 insns -- only a `break` jumps there, a `return`
  does not) followed by jump1's duplicate_loop_exit_test: source `for (i = start - scroll; i < n; i++) { cur = lv; lv +=
  step; if (i >= 0) { if (i >= frame_w - 1) break; ..} }` with `frame_w` re-read (the bottom copy reloads it) and `n = end -
  scroll` a local. Left 67: `c = col` must be the block's second insn (target `stw r4,24(r1)` before the V load; a
  `volatile u32 c` changes nothing, a `"=m"(c)` asm gives 58 with other shifts) and the x/x+cell_w u16 arithmetic.
- **Read, not closed:** tcSetBesideOffset (t_camera_data, 27): the two loop-1 givs' REG_LIVE_LENGTH 84/82 is haifa's
  post-sched recount (`find_pre_sched_live` + `find_post_sched_live` both add, so every insn counts twice); n*12 is
  born one insn before n*4 in the preheader and both live to the loop end, so the 2-insn gap is structural -- with
  identical RTL the original could not have had n*12 first (27/84 < 27/82); its REG_N_REFS must have differed (both 9 =
  init 1 + preheader copy 2 + increment 6). Pointer/`k4`/`k12`/post-increment forms give identical RTL (27); `"=m"`
  anchors on `c->at[n]` in the outer body break the giv combination (69-70). toolIdEditDisp (t_id, 4): `li r24,12` must
  issue at t3 with `cmpwi` after `lbz dispTop` -- a ready prio-1 insn cannot be held back two cycles by fillers (two
  slots per cycle), so the original's `row = 0xC` set was not ready before t3; unknown. tcDrawRail (t_camera, 39): the
  target has a dead `mr r3,r28` (&v) before each `bl tcGetFloor` and allocates `&v` before the pos giv -- an expansion
  difference (block-move argument setup?), not iterated. tvibModeFrameDisp/tvibListVibDraw/tvibEditFrameDisp,
  seAtAreaEdit_EditMenu (two zero pseudos r7/r8 swapped), tcToolCameraMove, tcEdit_select not iterated.

### Tool RELs, t_esp pass 4 (db_light 132 -> 134/136 in t_esp, 131 -> 133/135 in Tools, 130 -> 132/134 in t_camera/t_light/t_event: draw_light_graph 22 -> 0, editColor 120 -> 0; edit_light_parent 17 and printEditTable 183 left; db_mod/db_widget/t_esp unchanged; nothing flipped; 2026-09-11)

- Harness ~/.cache/tesp4 (tools_p13 copies with the paths rewritten: `mtryv.py MOD/UNIT FUNC v.py [--asm NAME]`, `msbs.sh`,
  `mdump.sh MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE`, `fsec.py`, `mcmp.py`, `order.py`, `prio.py`). Iterate db_light on
  `t_camera/db_light` (plain source, no wrapper) and re-check the five modules with the ninja objects: the harness build
  puts `blackTemplate` at .rodata 0x15E5 instead of 0x1630 (a `.rodata+N` reloc-target "2 words" on every local-symbol
  reference that the real build does not have). mcmp's `_._5cUnit / __t8cVarLoop1ZUcRCUcN21 / _._10cLightTool: 2 words` in
  t_camera/t_light/t_event/t_esp are the `.rodata+5640` vs `_vt.5cUnit` reloc-name pairing, not code. Build 111 OK after
  every edit.
- **draw_light_graph (22 -> 0, tagged `COMPILER-DIFF: 2 + #17`): `register int col5 asm("r5"); col5 = 0; if (v > 0.04f)
  col5 = 6; eprintf(.., (u8) col5, ..)`** replaces the `int c = col; asm("" : "+r"(c))` launder. The target's colour lives
  in r5 (`li r5,0 / li r5,6 / clrlwi r27,r5,24 / mr r5,r27`): in ours the zero-extension's dest (local qty, crosses the
  func_attn call -> r27) gives `col` a non-copy `hard_reg_preferences` bit for r27 (global.c `set_preference` strips the
  ZERO_EXTEND and records dest's hard reg for the src pseudo), and find_reg takes a free preferred register over the
  REG_ALLOC_ORDER scan, so col/c coalesce into r27. The pin gives r5 and the `(u8)` cast of the int pin the mask; the
  `%1.6f` block's `li r5,0` position follows. A plain `u8 col` argument (no launder) has no mask (12 words, #2 as ever).
- **editColor (120 -> 0): (1) by-value swatch argument = the target's reused 4-byte temp at frame offset 0 (tagged
  `COMPILER-DIFF: candidate #18 (by-value aggregate view)`).** `void DrawTileV(int, int, int, int, GXColor) asm("DrawTile__
  FiiiiP7GXColor")`: under the V4 ABI an aggregate is passed by invisible reference, so calls.c copies the argument into
  `assign_stack_temp (SImode, 4, keep=0)` (calls.c:1039) -- freed at the statement end and reused by every later call, and,
  being the first stack object, at `fp+0` = `addi r7,r1,8` recomputed per call (the frame-offset-0 rule) with `stw r24,8(r1)`
  / `lwz r0,12(r1); stw r0,8(r1)` copies; `c` is the ADDRESSOF pseudo purged into the next slot (12). The real DrawTile
  takes a pointer (mangled `P7GXColor`), so the copy in the original came from something else; every plain form fails:
  `GXColor tmp; tmp = c; DrawTile(&tmp)` purges `c` first (`c.g = 0` is `(mem (plus (addressof c) 1))`, forced at
  purge_addressof before the `&tmp` precompute copy; purge runs after cse1, before gcse) and PREs `&tmp` (r28 + `mr r7,r28`);
  the GXColorW BLKmode inline local shares one slot but is 8 bytes (`assign_stack_temp` rounds BLKmode to BIGGEST_ALIGNMENT:
  frame +8, c at 16); an inlined by-value parameter is `assign_stack_temp (mode, 4, keep=1)` per call (integrate.c:1514,
  never freed: +0x20); `{ }` blocks around the inlined calls do not free them either. **(2) `FSet(r, r + step * 10.0f)` (and
  g, b, a) in the JOY_RIGHT arms:** the target reloads `pTool->joy.rep` for the JOY_LEFT test after the arm, i.e. cse's AROUND
  path (`beq` over the arm) lost the load -- a store to the static `r` is a fixed scalar and does not invalidate the struct
  load, a reference store (neither flag) does. The `li r23,255` hoisted constant, the `y*14` temporaries and the callee-saved
  permutation all followed from (1).
- **edit_light_parent (17, unchanged; mechanism exact):** the target's `addi r10,r9,101` is the `(id >> 16) + 101` temp NOT
  tied to the dying shift result (local-alloc `combine_regs`), so the temp takes r10 by the fake-lifetime rule (born right
  after r9 dies), `n` (global, live across the arm) loses r10 to it and falls to r8 (REG_ALLOC_ORDER 0, 9, 11, 10, 8), and
  the remainder chain reuses r9. Ours ties (same RTL: `(set t (plus hi 101))`, hi dying, both block-local). Tried: `n + hi +
  1` (cse associates to 101 either way), function-scope `hi` set in both arms (global, no conflict with the tied temp ->
  identical bytes), function-scope `t` (79: t global takes r11, the case-1/case-2 `or; stw` cross-jump breaks), block-local
  `t`, `u32 id` local, `u16 lo/hi` locals (58), `volatile` read (23), `asm("addi")` (17), an asm keep-alive of hi (44), pins
  `t` r10 (52: hi takes r10 through the hard-reg suggestion) and hi r9 + t r10 (54: the remainder chain moves to r10).
- **printEditTable (183, unchanged; read):** `asm("li %0,10" : "=r"(x))` with literal `7 * 8` / `10 * 8` reproduces the
  x chain (the target's `x = 10` is opaque to gcse cprop: `x + 1` at the "P" join is not folded, `10 * 8` in the same
  block is), but the callee-saved shift stays (219): the target has a SECOND `y` copy (`mr r26,r23` for the "P" column, `mr
  r29,r23` for the rest: 18 callee-saved registers, ours 17) and two `high("%s")` pseudos (r17 PRE'd for P/S/parent, a fresh
  `lis r27` for E/O/E: the basic_menu fresh-lis/r27 cse1-path family). A `y0`/`y` split inside the row inline is merged by
  cse (219). The DrawTileV form in printEditRow gives 175 with the frame +16 (col forced at expand time here).
- **db_widget DB_NUMERIC ctor (2, exact):** `fmr f1,f31` (SetDefault's 0.0) one slot later than the target: sched2 ranks the
  `this`-based stores prio 5 (a TRUE dependence on the call's `(mem (symbol_ref SetDefault))` through the
  `ADDRESS (VOIDmode, r3)` base of `mr r30,r3` -- the plmove10 sched2-alias family) above the fmr's 4; the target's stores are
  prio 4 (anti only) and tie with the fmr on sched1's LUID. Compiler-side (alias.c after reload); the #13 asm stays.
- **db_widget DB_STRING ctor (11):** statement order of `max / colour chain / type / str / len` does not move a word
  (7 orders); the difference is the vptr store issued after the four `stfs` and the `type` store early (`li r9,4`,
  `li r0,0` swapped) -- a sched1 rank question not iterated further.
- **t_esp Save*FileNoUpdateCallback x5 (5 each, mechanism now exact, flow.c not jump.c):** flow1 deletes the dead `type =
  0xFF / 0` sets; flow2's `find_basic_blocks (.., do_cleanup=1)` -> `delete_unreachable_blocks` -> `tidy_fallthru_edge`
  deletes the `ble L2` whose only successor is the next block, and life_analysis then deletes `cmpwi 255` (cr0 dead); the
  first `bge L1; b L2; L1:` block keeps two successors, survives flow2 and jump2's jump-around-jump + jump-to-following
  deletion keeps its compare (`delete_computation` does not reach it), which is why ours keeps ONE compare and the original
  -- whose flow2 has no fallthru tidy -- keeps BOTH. Nothing survives flow2 in the arm yet vanishes at jump2 except a no-op
  move carrying a REG_EQUAL note (flow's `noop_move_p` skips those, jump2's `delete_noop_moves` does not), and cse creates
  `(set X X)` + REG_EQUAL only for a known constant -- no source form found. Tagged `register int type11 asm("r11"); type =
  g_modelType; type11 = type;` (regmove folds the copy into `lha r11`) gives the target's `lha r11; cmpwi r11,0` (5 -> 3
  words, not applied: not identical). `"cc"`-clobber / codeless-input asms in the arms keep the branches (7-8); a pinned
  `t = type` copy in the arms is hoisted by jump.c when both arms match and conflicts with `type` otherwise (9-15).
- Not iterated: db_mod (position_usage 34, IKreport 10, the rest), db_widget DB_WINDOW ctor (14), t_esp MakeLoadSeqData (9)
  and the larger residues.

### Tool RELs, bytes-first pass 14 (Tools/t_rck Matching 33/33 -> flipped; t_camera/t_camera Matching 57/57 -> flipped; t_vib 25 -> 27/29; t_block 27 -> 28/31; t_camera_data .rodata equal; 2026-09-11)

- Harness ~/.cache/tools_p14 (deleted at the end): the tesp4 module-aware scripts with the paths rewritten (`mtryv.py MOD/UNIT
  SYM V.py [--apply NAME]`, `msbs.sh MOD/UNIT SYM [OBJ]` = target LEFT, `mdump.sh MOD/UNIT -dX` with an ABSOLUTE `SRC_OVERRIDE`,
  `fsec.py DUMP FUNC`, `mcmp.py`, `order.py`, `prio.py PRE` on `<PRE>_lreg.txt`/`<PRE>_greg.txt` cut with fsec). 111 OK after
  both flips (`Tools/t_rck.cpp` and `t_camera/t_camera.cpp` in modules.py MATCHING; make_rel needed no scope fixes).
- **rckDrawPointLine (t_rck, 231 -> 0, zero code; five independent facts):** (1) `RckLine* lij = &w->line[i][j]; RckLine* l =
  &w->line[j][i]; RckPoint* p = &w->pt[i];` POINTER forms: an ADDR_EXPR of `x->arr[i]` builds `(plus w (plus (mult i 16) 724))`
  (fold groups the member offset with the index term), so `i*16+724` is ONE invariant pseudo hoisted by loop.c (`addi r23,r9,724`
  in the preheader), `p->pos.x` is rewritten by find_best_addr to `lfsx w,r23` and `.y/.z` read `4(p)`/`8(p)` off `add p,w,r23`;
  the direct `w->pt[i].pos.y` expands as `(plus (plus w 728) i16)` = `addi w,728; lfsx` per field. (2) ONE `RckPoint* p`
  reassigned (`p = &w->pt[j]` after the `a` stores): the second set carries an output dependence on the first and anti
  dependences on every `lfs 4(p)`, so sched1 issues it right after `a.z`'s load and local-alloc gives both the same r11; two
  pointers put the j add at t2 in its own register. (3) `if (dir.x == 0.0f && dir.z == 0.0f) continue;` between PSVECAdd and
  VECNormalize -- read off the target's `fcmpu cr7 x; bne; lfs z; fcmpu; beq CONT` (the macro's first test then reuses cr7 as a
  dead `bne cr7`); the two extra insns also push the inner loop over loop pass 2's `71 * 3 = 213` threshold so the `lis pLog@ha`
  (life 3) stays in the error block instead of the preheader (ours hoisted it at exactly 213 insns and it took r14, spilling the
  PRE'd `&colOne` pseudo -- reload then re-materialised `addi rX,r1,196` at both uses). (4) Member-wise Vec copies (`v[0].x =
  p->pos.x; ...`, `seg0.x = v[0].x; ...`, `v[0].x = mid.x`): `lfs/stfs` per field with the `p->` fields reloaded after the frame
  stores (cse invalidates the varying-address loads); `Vec seg0, seg1` are two locals (16-byte slots), not `Vec seg[2]`. (5)
  `u8 red = 0xFF; GXColor colBoth = {red, 0xFF, 0xFF, 0xFF}; colHit = {red, 0xFF, 0, 0xFF}` (the rckDrawPointLineNow idiom: the
  variable byte gives `stw 0; stb r8(-1)` where a constant first byte folds into the word store) and `RckWork* w = RCK` AFTER the
  `i == j` test.
- **tcDrawRail (t_camera, 39 -> 0, tagged `COMPILER-DIFF: candidate #18`):** the target has a dead `mr r3,&v` before each `bl
  tcGetFloor` (a `()` function, `tcGetFloor__Fv`) and allocates `&v` (r28) above the pos giv (r27); the r3 set is the shape of
  expand_call's `struct_value_rtx` move or an argument, neither of which the declaration allows. Every natural spelling was tried
  and rejected: a by-value inline (`Vec tcCdatPos(c, i)` -- integrate.c DROPS the callee's `(set r3 value_address)` because
  function.c marks it REG_FUNCTION_VALUE_P; with `v = f()` the C++ FE also goes through a temp slot, `Vec v = f()` writes v
  directly), `Vec* pv = &v` (fixes the allocation, 27), an inline wrapper taking `Vec*` (39), placement-new copy (39). Applied:
  `register Vec* a3 asm("r3"); a3 = &v; asm("" : "=m"(v.y) : "r"(a3));` after the copy -- the pin's extra refs also give `&v` the
  target's priority. No other `mr r3,rX; bl *__Fv` with a frame address exists in the whole tree (searched), so the mechanism is
  unknown; keep the tag.
- **tcToolCameraMove (87 -> 0, zero code): `Vec axis = {0,1,0}` declared INSIDE `if (PTC->joy.sx) {..}`** (the template copy
  lands in that block, its address pseudo is local and takes r4 for both arms' calls, frame 72 with r31 only). The last word was
  a linkage error: Bio4.sym names `CameraTargetDistance__FP6Cameraf` (C++) while camera.h declared it inside `extern "C"` --
  moved out of the block (cam_sys.cpp's definition now assembles under the mangled name; the DOL bytes are unchanged, the REL
  reloc resolves). A wrong-linkage declaration only shows in mcmp's reloc-name column and would have broken the flip.
- **tcEdit_select (159 -> 0; one tagged keep-alive):** (1) `PTC->adatTypeNum[n->area_no]++` (re-reading the just-stored member:
  cse forwards the `stb` as a copy `mr r0,r28` used as the index, and with that extra pseudo the 264-byte block-copy loop's
  temps fall into the target's r8/r10/r0/r11) -- the pass-12 "n r8" note was this. (2) `x += 0x15 + (cursor - 3) * 5;` for the
  default arm (fold keeps `(x + 21)` as a temp: `addi r10,r26,21; ...; add r27,r10,r0`; two statements write x twice, the single
  `x = x + 0x15 + e` form is re-associated to `x + (e + 21)`). (3) The temp form costs x two refs and x (25 refs / 192) then loses
  r27 to the cut pointer `c` (8 / 45 = 0.533 vs 0.521); `asm("" : : "r"(x));` right before `if (blink & 0x18)` (tagged `tie
  (global-alloc refs of x vs the cut pointer)`) adds the ref without extending the range (placed after the block it lengthens x
  to the end: 26-46 words). The natural extra ref was not found (`x++` in the blink block gives 27 refs but `addi r27,r27,1`).
- **tvibFrameLineDraw (t_vib, 67 -> 0, zero code):** `GXColor c; *(u32*) &c = col;` (tvibFrameMarkDraw's form: the cast store is
  an unflagged MEM, so the block's struct loads depend on it and sched1 issues it as the 2nd insn -- a `u32 c = col` store is
  `mem/f` and sinks); `int x` (no u16 truncation), `(u16) menu_x + cell_w * i` (lhz), `v[1].x = v[0].x + cell_w` (MEM + MEM keeps
  the written operand order -- see the next item), and the store statements in the order v0.x, v1.x, v0.y, v1.y, v0.z, v1.z
  (several orders give 0; the x's must precede the y's).
- **Narrow `+` operand order (tvibFrameLineDraw `x + cell_w`, snd_test `w->cur += dir` / `w->reqCur += step`): ours always puts
  the MEMORY operand first (`add r8,cw,x`, `add r0,cur,dir`) whatever the source order (`x + cw`, `cw + x`, casts, `dir + w->cur`
  all identical); the target puts the promoted variable first.** convert_to_integer shortens `(u8)(cur + dir)` to `cur + (u8)dir`
  in order, expand_binop's swap rule (`op1 REG && op0 != REG`) does not fire for a MEM + SUBREG pair, and widen_operand then
  force_regs both in operand order -- so the reversal is not in our expand path; with two MEMs (`v[0].x + cell_w`) the order is
  kept and matches. Treat as the same family as the pass-4 snd_test note; two words each in test_tbl_now_check /
  test_req_no_select.
- **tvibModeFrameDisp (80 -> 0, zero code): per-arm block-scoped `int x0, y0, x1, y1`** in each `case` of the mode switch. With
  function-scope locals set in both arms each is one global pseudo allocated to the same register in both arms, the two 12-store
  tails are identical RTL and jump2 cross-jumps them (ours 7 stores + `b`); the target's arms allocate differently (case 0's zero
  is the mode register r7, case 1's a fresh `li r0,0`) and stay separate.
- **tvibListVibDraw (90 -> 24):** `for (i = 0; i < 16; i++) { if (i >= 64) break; ...}` -- the target's bottom `cmpwi 15; bgt END;
  cmpwi 63; ble LOOP` is the for-test plus jump.c's duplicate of the break test (`i < 16 && i < 64` is range-folded to one
  compare); the extra exit block also removes the interblock hoist of the inner loop's `mr r5/r6` arg copies (leaf rule).
  `TvibData* d = &V->list[i + top]` and `if (i + top == V->listNo)` without an `n` local (gcse's reaching copy `mr r24,r9`). ONE
  `int xl = x + 49` variable reused for the frame-marker x (`xl = LIST_X(i) + 49` in the listNo block): a multi-set pseudo is not
  "birthing", so sched1 issues it by priority AFTER `x + 48` (a single-set `xl` live at block end gets adjust_priority's boost and
  takes the first slot). Left 24: the marker's `if (frame > 177) fx = xl + 178; else fx = xl + frame;` -- the target hoists the
  else set (`add r31,r31,r9` before `ble`), which jump.c does only when B does not mention X and X is not set between
  (`modified_between_p`), and with the hoisted layout cse's ebb follows the `ble JOIN` jump so `LIST_Y(i)` after the join reuses
  the `(i+7)>>3` quotient (`slwi r9,r10,3`; ours recomputes it as `clrrwi` -- combine folds `(ashift (ashiftrt t 3) 3)` when the
  quotient has one use). `int frame = V->frame` + a separate `fx` gets the hoist and the shared quotient (l11) but fx/xl then
  allocate apart (fx r5 = tied to the dying `frame`; the target's fx is xl's r31): the form where fx IS xl and B still hoists was
  not found (`xl += frame` fails modified_between_p).
- **tBlockAreaInfo_Menu (t_block, 11 -> 0, tagged `candidate #17`): `register int n asm("r11")`** for case 0's blockNo temp.
  n (10 refs / 17) outranks the rep2 load (3 / 9) in global-alloc and takes r9 in every plain form (n/m split, rep2 local, u8 n,
  direct RMW: 11-76); pinning rep2 to r9 instead pushes the `lis` high to r11 (a hard-register OUTPUT of the `lwz` conflicts with
  the address register dying in the same insn, so the high cannot share r9), pinning n leaves the high/rep2 tie intact.
- **tBlockSaveDataCreate (67 -> 38, zero code):** `int i = BLOCK_NUM - 1` at the declaration (`li r8,31` in block 0), `linkSize
  = nBlock * sizeof(BlockLink)` right after the memcpy, and `u32 areaSize = nArea * sizeof(TBlockArea) + sizeof(TBlockHeader);
  u32 ofsConnect = linkSize + areaSize;` locals (fold re-associates `linkSize + (n*56 + 24)` into `(linkSize + 24) + n*56` and cse
  then shares the `+24` with ofsArea; the target adds `24` to the product). Left: the target keeps `linkSize` as a copy `mr
  r26,r30` of the memcpy-size pseudo (both live: the pLink advance reads r30 after the copy) -- every statement order (before/
  after memcpy, advance through linkSize, `pLink += nBlock`) lets cse/regmove merge them (38-50).
- t_camera_data: the split object's `.rodata` ends with a 4-byte pad to 8 (`asm(".section .rodata; .balign 8; .text")` at the
  end of the file, the esp_app rule); tcSetBesideOffset (27) / tcDataExport (142) untouched.
- Facts read this pass: `rank_for_schedule` compares PRIORITY first and register weight second (the pass-13 "weight before
  priority" reading was wrong) -- a lower-priority insn can only win a slot through adjust_priority's birthing boost (dest set
  once and live at block end); `expand_binop` swaps commutative operands only for `(op1 == REG && op0 != REG)`, `target == op1`
  or a CONST_INT op0; jump.c's `x = b; if (..) x = a` hoist needs `x = a` not to reference x (post-cse fold of `xl + 178` to `L +
  227` satisfies it) AND b's registers unmodified between the hoist point and `x = b` (so `xl += frame` never hoists); global.c
  `set_preference` takes the FIRST operand of a PLUS as a copy-like preference source; integrate.c drops an inlined struct-return
  function's `(set r3 value_address)` (REG_FUNCTION_VALUE_P) so a by-value inline never leaves an r3 set; loop.c `move_movables`
  hoists when `71 * savings * lifetime >= insn_count` with lifetime counting notes (LUIDs), so a `lis` with two BLOCK_BEG notes
  between it and its load has life 3.

### Tool RELs, t_esp pass 5 (db_light Matching in all five modules -- t_camera/t_light/t_event 134/134, Tools 135/135, t_esp 136/136, flipped, 111 OK; t_esp 194 -> 195/212: MakeLoadSeqData 9 -> 0, MakeExecSeqData 42 -> 27; db_mod position_usage 34 -> 10 (t_esp 60/75, Tools 48/63); db_widget unchanged; 2026-09-11)

- Harness ~/.cache/tesp5 (tools_p14 copies with the paths rewritten; `mtryv.py MOD/UNIT FUNC v.py [--apply N] [--asm N]`
  now honours `CC1DIR=<dir with cc1plus>` for a hooked compiler; `msbs.sh`, `mdump.sh` (ABSOLUTE `SRC_OVERRIDE`, dumps named
  by the unit basename), `fsec.py`, `mcmp.py`, `order.py`, `prio.py PRE`). A hooked cc1plus (copy of tools/sn-gcc
  {Makefile,src,obj} into ~/.cache/tesp5/sngcc, `make OBJ=obj BIN=. ./cc1plus` after touching gcse.c, ~1 s) with env hooks
  `NO_CPROP=1` (skip one_cprop_pass) / `NO_CPROP_REGS=a,b,c` (skip cprop into uses of those regnos) was used to test the
  printEditTable hypothesis; nothing installed, deleted with the harness. Build 111 OK before and after every edit.
- **edit_light_parent (db_light, 17 -> 0, zero code): one function-scope `u32 no;` reused by both arms of case 2 (`no =
  cur->parentId >> 16; no = (no + 101) % n;` / `(no + 99) % n`, then `(no << 16)` in the store).** The multi-set,
  multi-death pseudo is not a local-alloc qty, so the `+101` temp is not tied to it (`addi r10,r9,101` by the fake-lifetime
  rule) and the remainder goes back into `no` (r9) untied from the dying temp -- exactly the target's r9/r10/r9 chain; `n`
  then falls to r8. Every fresh-pseudo spelling ties (combine_regs needs `reg_qty[sreg] == -2`, i.e. a single-death pseudo
  not yet seen); the three-pin form (hi r9, t r10, rem r9) gives 52.
- **printEditTable (db_light, 183 -> 0; tagged `candidate #12 (gcse cprop)` x2 + `candidate #18`): the row printer is a MACRO
  over printEditTable's own `x`/`y`/`c`, not an inline with its own locals** (the target's row `x` and the outer `x = 4`
  are ONE pseudo, r30). Pieces, each read off the target: (1) the "%02d"/"P" columns read `0x150 + i * 14` (the outer
  expression pseudo, cse-canonical in the row's ebb: `mr r4,r23` for the first row call, its loop.c giv copy `mr r26,r23`
  for the "P" column after the diamond) while the E.. columns read a SECOND variable `y = (i + 24) * 14;` set right after
  the outer "%02d" eprintf -- a spelling cse cannot fold into the first pseudo but loop.c combines (mult 14, add 336 ->
  the second giv copy `mr r29,r23`); (2) `asm("li %0,10" : "=r"(x))` and `int t80; asm("li %0,80" : "=r"(t80))` for the
  "P" call's `x*8`: the target keeps `li r30,10` and `li r3,80` as pseudo sets of the pre-diamond block (`x + 1` after
  the join not folded, the r3 arg move not folded, T not moved by update_equiv_regs), ours cprops all three into the "P"
  join block -- the hooked compiler shows disabling cprop for exactly those pseudos gives the target's chain, so this is
  the #12 "block entered with less knowledge" family on gcse's side (stock hash_scan_set/cprop_insn record and propagate
  them; the cause is unknown); (3) `DrawTileV(x * 8 + 1, y + 1, 0x16, 0xC, col)` (the editColor by-value view) for the
  swatch's 4-byte temp at frame 8 / `col` at 12; (4) the type-4 colour as a FOUR-member chain `col.r = col.g = col.b =
  col.a = l->color.r;`: the C++ front end re-evaluates the rhs for the value of an assignment to a struct member (the
  three-member chain `col.r = col.b = col.g = X` loads X for g AND for b: `lbz` x3), so the innermost `col.a = X` owns
  load 1 and the re-evaluated rhs of `col.b = ...` is the ONE load shared by b, g, r (stores a, g, b, r in that order,
  `lbz r11; addi r9,r1,12; stb 3(r9); lbz r0; stb 1; stb 2; stb r0,12(r1)`). A `u8/int v` local instead hoists `&col`
  out of the loop: the `(plus fp 12)` pseudo's loop.c life goes 6 -> 8 (`threshold * savings * life >= insn_count` flips
  in pass 2), and the extra `lbz` of the three-member chain also decided the "%s"/"E" PRE-high tie (r17/r18: both 7 refs,
  lengths 706/704 -> 198 vs 198 truncated -> lower pseudo first; one insn fewer in both ranges made it 199 vs 198).
- **db_light flip: the compiled unit must define the module's 0x34-byte COMMON block** (`asm(".comm common_" STR(REL_MODULE)
  ",52,4")`, the em10.cpp/st_room.h idiom) or make_rel fails with "COMMON symbols take 0x0 bytes, the original had 0x34";
  the flag went into modules.py for t_camera, t_light, t_event, Tools and t_esp at once (db_light_tools.cpp /
  db_light_esp.cpp wrappers: `touch` them after every db_light.cpp edit). The `_._5cUnit / __t8cVarLoop.. / _._10cLightTool:
  2 words` lines of mcmp in t_camera/t_light/t_event/t_esp are the `.rodata+5640` vs `_vt.5cUnit` reloc-name pairing.
- **MakeLoadSeqData (t_esp, 9 -> 0, zero code): declare the inner-loop pointer `TOOL_SEQ* t;` at function scope BEFORE
  `TOOL_SEQ* rec;`.** loop.c reduces the givs in pseudo order, so `t + 300` gets the lower new_reg and is allocated before
  `rec + 300` (equal priority 6 refs / 23: r6 / r5 instead of r5 / r6), and the four prologue parameter copies reorder
  with it. Statement forms of the copy loop (`*t = *rec; t++; rec++;` etc.) do not move it.
- **MakeExecSeqData (t_esp, 42 -> 27):** `rec = (TOOL_SEQ*) head->rec` assigned AFTER the count-clearing loop (the target's
  `addi rec,head,48` sits in the second loop's preheader), the flag table read per iteration through the struct view
  `((SeqFlgPtr*) &g_pSeqFlg)->p[j]` and `(.. & 1) == 0` (`!(x & 1)` folds to `xori; bne`), a separate `u32 j` for the
  second loop. Left: caller-saved naming (zero r10/i r11 vs r0/r10, rec r10 vs r8, the three highs) -- 12 forms tried.
  MakeSaveSeqData (58): the same `rec` placement helps structurally but `size` must take r3 and `head` move to r5 (the
  target allocates `size`, 5 weighted refs, before `head`, 9) -- not found, left as is.
- **position_usage (db_mod, 34 -> 10, zero code): `x = 43;` at the END of both arms of the mode if/else, the else written
  `else { if (mode == 1) {..} x = 43; }` so x is defined on every path.** Three effects: cse cannot fold the join's
  `x * 8` (`li r30,43; slwi r30,r30,3` once, `mr r3,r30` per call and the tail's PRE copy `mr r29,r30` -- the pass-1
  asm-li form left the tail recomputing), and with x live across no call (REG_N_CALLS_CROSSED 0: the uninitialised
  `mode != 0, 1` path of the else-if form made x live from the entry across every call) sched1 anchors each arm's `li`
  behind the arm's last call (`add_dependence (insn, last_function_call)`), so jump2 cross-jumps arm A's `li` into arm B's.
  Left (10): the target's `li r30,43` is issued after the join's `mulli r4,r31,14` (one block, not the cross-jump label
  block) and the tail's `y++; (y - 1) * 14` is not combined (`addi; addi -1; mulli`, i.e. y live after the last call).
- **Save*FileNoUpdateCallback x5 (t_esp, 5 each, still compiler-side): the no-op-move construction cannot work in this
  pass order.** toplev.c rest_of_compilation: reload (598) -> `reload_cse_regs` (607) -> flow2 (660) -> sched2 (689) ->
  jump2 with JUMP_NOOP_MOVES (713). `reload_cse_noop_set_p` deletes `(set r r)` (dreg == sreg) and every copy whose dest
  already holds the source's value BEFORE flow2, so the arm is empty at flow2 exactly as now; jump2's `delete_noop_moves`
  (same-rtx set, sreg == dreg, a copy redundant per `find_equiv_reg`, a constant already in a register) sees nothing
  reload_cse did not, because `find_equiv_reg` stops at the same CODE_LABEL / volatile asm and reload_cse forgets there
  too; a bare USE/CLOBBER or an asm keeps the arm alive through jump2 (the branch stays). Pins `register int t asm("r11")`
  in the arms (6), `asm("" :: "r"(t))` (8), `volatile int t = type` (14).
- **db_widget DB_NUMERIC ctor (2, unchanged; the sched2 table read):** after `bl DB_STRING` the block's nine stores split
  5 : 4 -- `minus, pNum, min, ketaFloat, edit` (the source's first five statements) have prio 5 with a TRUE dependence on
  the SetDefault call, `max, keta, unit, step` prio 4 (anti only), and the `fmr f1,f31` is prio 4 -- so the split is by
  RTL position, not by base or offset; `DB_NUMERIC* self` / pinned / laundered `this` views, `SetDefault(zero)`, a
  `k255` local, moving `max = 255.0f` first and a `"m"` keep-alive asm change nothing (4-31 words). Not resolved.
- Not iterated: db_widget DB_STRING (11) / DB_WINDOW (14), db_mod IKreport (10) and the larger residues, t_esp's larger
  residues.

### Tool RELs, bytes-first pass 15 (Tools/t_vib Matching 29/29 -> flipped; t_movie/t_se_at 14 -> 18/20; t_movie/snd_test 51 -> 53/77; t_sce/t_block tBlockArea_disp 105 -> 2; t_camera_data / t_esp_area / t_lightarea read; 2026-09-11)

- Harness ~/.cache/tools_p15 (deleted at the end): dol18a copies with the paths rewritten plus `mtryv.py MOD/UNIT SYM V.py
  [--apply NAME] [--src ABS]` (source, cflags and post_build parsed from build.ninja after joining the `$\n` continuations),
  `mdump.py MOD/UNIT -dX...` (runs CPP.exe + cc1plus itself with ngccc.py's define lists, dumps land in `dump/<stem>.i.<pass>`
  -- ngccc.py writes the `.i` into a temp dir, so `-d` flags through ngccc give nothing), `msbs.sh`, `mcmp.py`/`order.py`
  resolving `MOD/UNIT` to `build/G4BE08/<mod>/obj/`. NB while several agents run `ninja` at once, the `build build.ninja
  objdiff.json: configure` regeneration reads a half-written objdiff.json ("Expecting ',' delimiter: line 4045") on almost
  every attempt; `cp build.ninja /tmp/x.ninja && ninja -f /tmp/x.ninja <targets>` builds the requested objects without the
  regeneration (the copy's regen rule outputs `build.ninja`, not the loaded manifest).
- **tvibListVibDraw (24 -> 0, zero code): the marker `if (V->frame > 177) xl += 178; else xl += V->frame;` written with the
  ARMS SWAPPED, `if (V->frame <= 177) xl += V->frame; else xl += 178;`.** jump.c has two hoists: form 1 (`if (c) x = a; else
  x = b;` -> `x = b; if (c) x = a`) requires B's registers unmodified between the hoist point and `x = b`, which `x = a` breaks
  when a also sets x; form 2 (`if (c) { x = a; goto l; } x = b;` -> `x = a; if (c) goto l; x = b;`, jump.c ~l.650) requires only
  that B (the FALL-THROUGH arm, `x + 178` cse-folded to `L + 227`) does not reference x -- A may. So the hoisted `add r31,r31,r9`
  before the `ble` is the THEN arm of a `<=` test, not the else arm of a `>` test; cse then follows the `ble` and LIST_Y(i) reuses
  the `(i+7)>>3` quotient as the pass-14 note predicted.
- **tvibEditFrameDisp (92 -> 0, zero code, four facts):**
  (1) `(i + V->scroll) % 10` -- source operand order IS kept for an int + int-member add (`add r11,r30,r11`); the pass-14
  "memory operand always first" is the narrow-store family below, not a general rule (probe: `s->scroll + i` / `i + s->scroll`
  give `add r0,r0,r4` / `add r4,r4,r0`).
  (2) fold's association moves a constant OUT of the left group and INTO the right one: `menu_y + (i * 16 - 8)` becomes
  `(menu_y - 8) + i*16` (split_tree on arg1), while `menu_y - 8 + i * 16` becomes `menu_y + (i*16 + -8)` (split_tree on arg0:
  `VAR +- (ARG1 +- CON)`). The target's `slwi; addi -8; add menu_y,t` = the constant written next to the FIRST operand. Same for
  `y + 8 + cell_h * 8` (target `slwi; addi 8; add y,t`) and for MULT: `cell_w * 29 * V->frame / 2000` gives `mulli frame,29;
  mullw cell_w,t` where `cell_w * (V->frame * 29)` is re-associated to `(cell_w*29) * frame`. Rule: when the target keeps
  `t = b OP K` as a unit and adds/multiplies it into `a`, write `a OP K OP b` (K attached to a), never `a OP (b OP K)`.
  (3) The `col = 0x0000FFFF` / `col = 0xFFFF00FF` constants whose `stw col,48(r1)` the target issues AFTER the `sth` block and
  `mr r4,r24` (ours first): the assignment is written AFTER the six vertex stores (`v[..] = ..; col = K; TprimDrawFrameFn(..)`),
  which also puts `li r5,2` first (ours last); earlier positions give 108-109 words.
  (4) The loop-hoisted highs then named themselves (r18 menu_y / r20 cell_h had been swapped by the association above).
- **Narrow `+` operand order (snd_test test_tbl_now_check 1 -> 0, test_req_no_select 1 -> 0, zero code): `int c = (u8) w->cur;
  w->cur = dir + c;` (`int c = (u16) w->reqCur; w->reqCur = step + c;`).** Mechanism, exact: convert_to_integer shortens EVERY
  narrow-store `+`/`-`/bit-op to the narrow mode on PPC (`TRULY_NOOP_TRUNCATION`, convert.c l.311), so `w->cur += dir` is a
  QImode add; expand_binop widens it with `widen_operand (.., no_extend=1)` = paradoxical `(subreg:SI (reg:QI load))` operands
  (force_reg of the MEM, force_reg of `(subreg:QI dir)`); cse then folds the dir side back to `(reg dir)` and its commutative
  canonicalisation (cse.c ~l.4700: swap when op0 is class 'o' and op1 is not, or op0 is a SUBREG of an 'o' and op1 is not) puts
  the SUBREG-of-load FIRST whatever the source order -- `cur + dir`, `dir + cur`, `(u8)`, `(s8)` casts all give `add cur,dir`.
  An `int` local holding the byte is a REG ('o'): both operands are objects, no swap, source order kept; the `(u8)` cast makes
  the load a plain `lbz` (`int c = w->cur` keeps an `extsb`; a `u8 c` local is a promoted SUBREG and swaps again). tvibFrameLineDraw's
  MEM + MEM `v[0].x + cell_w` matched for the same reason (two SUBREGs, no swap).
- **t_se_at seAtAreaEdit_EditMenu (38 -> 0, zero code):** `TOOL_MENU* c = seAtCreateMenu; TOOL_MENU* e = seAtEditMenu;` pointer
  locals declared in that order (c first): the address pseudo created first takes r8, the four `enable` stores come out
  last-statement-first (`c[1], e[4], e[3], c[2]` from the source order e4, e3, c2, c1) and the then-arm call finds its menu
  already in r7.
- **seAtDataSave (41 -> 0, zero code): `{ int& n = seAtSaveNum; n++; }` after the work copy and `{ int& n = seAtSaveNum;
  pW->fileHead.num = n; }`** -- reference views of the static counter: the increment's `lwz` stays below the block copy's
  stores and the `lhz seAtSaveNum+2` below the `sth version` store (a MEM with neither the struct nor the scalar flag; the
  plain static is a fixed scalar that sched1 hoists above in-struct stores). An inline `IInc(int&)` helper differs by 2 words
  (integrate's copy); a reference for the index read too (`pW->file[n]`) costs 44.
- **seAtAreaEdit (63 -> 0, zero code, three pieces):** `Vec v[2]` instead of `Vec a, b` (two aggregate locals are 8-aligned
  slots at 8 and 24; the target's pair at 8 and 20 is ONE 24-byte object -- read the frame: 12-byte spacing = array/struct,
  16-byte = separate Vecs); `u8 no = pW->areaNo;` read at the function top and used by the FIRST arm only (`pW->areaNo = no +
  1`, the other arms re-read the member: the target's `lbz r10,35(r11)` above the `sub == 0` test is an unconditional read,
  the tvib_R0_VibLoopSet rule); `Camera* cam = &pG->Cam;` declared INSIDE each arm -- a per-arm cam is a block-local qty that
  local-alloc gives r30 (callee-saved, crosses calls), which makes r30 used-so-far, so `&v[1]`/`&v[0]` take r29/r28 and the
  PRE'd `high(seAtWk)` is the only pass-1 allocno and gets r31; the function-scope `cam` set in both arms is a global pseudo
  (priority 0.5) that took r31 first and pushed the high to r28.
- **seAtDataLoad (87 -> 0, one tagged item): (1) the REP_UP switch has `case 0: pW->server ^= 1; break;` like the DOWN one** --
  the target's `ble` from the UP tree lands INSIDE the DOWN tree at its `cmpwi 0; beq server^=1` (jump2 cross-jumped the two
  identical case-0 paths); a `case 0: break;` alone gives the tree but a `ble` to the chain end (1 word). Rule: a compare-tree
  branch into the middle of ANOTHER switch's tree = identical case bodies in both switches. (2) `if (yesNo == 0) { sub = 3;
  step = 0; step2 = 0; } else { sub = 1; step = 0; step2 = 0; }` (the zero stores repeated in both arms: separate `stb` arms
  instead of jump.c's else-set hoist, tails cross-jumped with the JOY_B arm's). (3) `register int col5 asm("r5")` for the
  `(u8) (sub == 1 ? (loadCursor == 0 ? 6 : 0) : 0)` eprintf colour with `int x = pW->x; int y = pW->y + 0x20;` read before it
  (tagged `COMPILER-DIFF: 2`, the draw_light_graph form): the `(u8)` cast of a constant ternary is folded by nonzero_bits, the
  `asm("" : "+r")` launder keeps the mask but costs the issue slot (3 words), the pin masks in place. Left: seAtAreaEdit_DataInput
  (170; a `clrlwi r5,r29,24` #2 mask, `lis .rodata` highs hoisted differently, register names).
- **t_block tBlockArea_disp (105 -> 2, zero code):** (1) `BlockLink* l = &pW->link[i];` BEFORE `cBlockUnit* u = Block.getUnitPtr(..)`
  (pW loaded before the call, `l` from the kept register); (2) the goto search loop is `for (int j = 0; j < 8; j++) { col =
  0x00808080; if (l->link[j] == pW->blockNo) { col = 0x0080FF80; break; } if (pW->link[pW->blockNo].link[j] == i) { col =
  0x0080FF80; break; } }` -- two separate break arms (cross-jumped into one hit block), a block-local `j` (a function-scope j shared
  with loop 2 is one global pseudo in r31; the target's loop-1 j is r8), the `col` re-set at the body top; (3) loop 2 reads
  `l->link[j]` DIRECTLY at every use (`if (l->link[j] == -1) continue; if ((pW->link[l->link[j]].flags & 1) == 0) continue; ...`):
  the byte stays one QI pseudo with `extsb r0,r8` at each int use and a re-read `lbzx r4` for the Draw_line3d argument after the
  join; the `s8 n = l->link[j]` local is promoted and extended once; (4) `case 1: col = 0x00808080; break;` written LAST in the
  mode switch (after case 3): case 2 and case 3 then both end in `bl dispBlockArea; b end` and jump2 merges the calls (with case 1
  first, case 3's call ends the switch and flow's `(use (const_int 0))` after it blocks the merge: 16 words). Left (2): the area
  loop's preheader `li r30,1504` (giv init) before `lis r29,-32768` (BIT_ON's mask, loop.c move_movables) -- ours emits the
  movable first (pass 1: movables, then strength_reduce's inits); the target's order needs the constant hoisted in loop pass 2
  (68 insns in pass 1 vs the `71 * savings * lifetime` limit: the original's loop had >= 72 insns at pass 1), not found.
- **Read, not closed:**
  - t_camera_data tcSetBesideOffset (27): the two loop-1 givs (207 = n*12+396, 209 = n*4; 9 refs / 84 vs 82) are BOTH pass-1
    allocations (REG_ALLOC_ORDER runs 0, 9, 11, 10, 8, 7, 6, 5, 4, 3 BEFORE 31..13: the first allocated gets r3, the second r31),
    so the target simply allocated n*12 first. Recording order in the outer loop's pass 2 (givs 450 n*4, 457 n*12, 459 n*12+396;
    reduction = reverse = 459, 457, 450) fixes the init LUIDs (n*12 first). Statement orders (`roll[n]` first flips the registers
    but also the body schedule: 47), `for (j..; j++, n++)`, `u32 n`, pointer/reference forms of `o`, `f32* roll` locals: 27-78.
    Loop 2's register cascade (`ready+i*132` r3 vs r12, `i+1` r31 vs r3, pos ptr r6/r7) follows from `regs_someone_prefers`
    (c = `mr r28,r3` prefers r3, so r3 is skipped in pass 0 by everything that conflicts with c).
  - t_block tBlockSaveDataCreate (38): gcse PREs the tail's `linkSize + 24` (expression 55) into the END of the area loop's test
    block and then-arm (`PRE/HOIST: end of bb 6/7`), the target computes it in the tail; and the target's `mr r26,r30` copy of the
    memcpy-size pseudo (both live: the pLink advance reads r30 after it) is undone in ours by gcse's COPY-PROP ("Replacing reg 86 in
    insn 462 with reg 119") -- `linkSize = size` with `size` used later, `size = 0` after the advance, `linkSize + 24 + n*56`,
    advance-before-assignment: 38-44 words each. The copy survives only when the copy's src or dest is set again in the same
    block (hash_scan_set's `oprs_available_p`) or 119 is mentioned later than 86 (cse's make_regs_eqv canonical rule) -- no
    natural spelling found.
  - t_camera_data tcDataExport (142), t_esp_area ToolEspArea (442), t_lightarea ToolLightAreaMain (395): the edit-window
    constructor block's `cmpwi r31,0` (the `new` result) is computed BEFORE the AddButton loop and kept in `mfcr r29` across it
    in the target, i.e. gcse PRE'd the `if (pEdit == 0)` compare across the loop; ours never does (block LCM's `delayin` leaves a
    single occurrence after a loop at its original block -- the same LCM that DOES insert `linkSize + 24` into t_block's loop:
    both directions exist, the difference between the two functions is not understood). `li r29,5`/`li r11,32` (the rows/num
    constants: r29 callee-saved in the target) untouched.

### Tool RELs, t_esp pass 6 (db_widget 110 -> 111/113: DB_NUMERIC ctor 2 -> 0, DB_WINDOW ctor 14 -> 3; t_esp 195/212: MakeExecSeqData 27 -> 2; db_mod untouched; nothing flipped; 2026-09-11)

- Harness ~/.cache/tesp6 (tools_p15 copies with the paths rewritten; `mtryv.py MOD/UNIT FUNC v.py [--src PATH]`, `msbs.sh`
  (accepts a relative object path now), `mdump.sh MOD/UNIT -dX` with an absolute `SRC_OVERRIDE`, `fsec.py`, `mcmp.py`,
  `order.py`, plus `mkobj.py MOD/UNIT` = compile the unit's ninja object in place with the build's cflags AND post_build
  (strip_unused + fold_linkonce) and `mkrel.py MOD` = relink `<mod>.elf`/`.rel` from build.ninja's rules and check its
  sha1 -- both without ninja). Two harness bugs fixed on the way: (1) tools_p15's `mtryv.py` chained the post_build with
  `&&` behind a `grep -v warning` that exits 1 when the compiler prints nothing, so strip_unused/fold_linkonce NEVER ran
  for variant objects (the "93/113" baselines of the earlier t_esp passes were unstripped objects; the counts were only
  comparable relative to each other); (2) variant names differing only by case (`MCTz`/`MCTZ`) compile to ONE object
  through wibo's case-insensitive path mapping. Build: `ninja` re-runs `dtk split` + configure.py on every invocation
  today (a concurrent ninja rewrites `.ninja_deps`), and configure.py's `json.load(objdiff.json)` races with the other
  agents' configure runs (the file is truncated while being rewritten) -- every ninja run of this pass died there with
  `JSONDecodeError`, the shasum check stayed 111 OK; judge with mkobj/mkrel + `dtk shasum -c` when that happens.
- **DB_NUMERIC ctor (2 -> 0, the #13 keep-alive asm MOVED behind the call): the block's second issue slot.** sched2 issue
  rate is 2; the `fmr f1,f31` argument copy (prio 4: TRUE dep on the SetDefault call, cost 1) ties with the codeless
  `asm("" : "=m"(def) : "r"(zero), "f"(one))` (prio 4: anti to the call) and loses the LUID tie-break, so at the cycle of
  the first store the asm takes the free slot and the fmr slips one store later. Any input that makes the asm ready
  later either raises a store's priority (`"m"(max)`: the store becomes the asm's dependent, 10) or changes sched1's
  dying-store order (`"f"(k255)` with a two-use 255.0: `stw r0,168`/`stfs f0,160` swap). Placing the asm AFTER
  `SetDefault(0.0f)` keeps zero/one alive past the block without an insn in it; `step = one; unit = one;` is the order
  that survives the move (unit/step swapped otherwise). sched2 facts read on the way: `sched_analyze` handles a
  CALL_INSN's call-used-register uses (ANTI) BEFORE `sched_analyze_insn`, and the MEM pass skips pending stores that
  already have a link ("If a dependency already exists"), so a store from a call-used register is ANTI-only (prio +1)
  while a store from a callee-saved register gets the TRUE dependence (prio +2) -- the "5 : 4 split" of pass 5 is by
  source REGISTER CLASS, not by RTL position. A hard-register SET clears `reg_last_uses`; a call does not, so uses of a
  call-used register keep collecting ANTI links from every later call until the register is set again (that is why a
  `stfs f0` store has 5 dependents and a `stw r11` store 4 in DB_STRING: sched2's depend_count tie-break = how many
  later calls/sets touch the stored register).
- **DB_STRING ctor (11, mechanism exact, not closed): the whole diff is local-alloc's naming of the four constant
  pseudos.** Target zero r0 (`str = len = 0`), type r9 (`li r9,4`), `_vt` lo_sum r11 (`lis r11; addi r11`), pool high
  r9; ours type r0, zero r11, vt r9, LC r11. With the target's names sched2's depend counts give the target's store
  order by themselves: colours/type 5 dependents (three calls + the later `lfs f0`/`lis r9`), vptr 4 (r11 never set
  again), str/len 3 (`mr r0,r3` right after `new[]` clears r0's uses) -> ca, type, cr, cg, cb, vptr, str, len; ours with
  vt=r9 (5, via the later `lis r9`) and type=r0 (3) puts the vptr store first and type last. Local-alloc order (qty pri =
  log2(refs)*refs/life): vt (4 refs / 6 insns = 1.33) before LC (2/2 = 1.0) -> r9/r11; type (2/4) before zero (3/11) ->
  r0/r11. The target needs LC allocated before vt (LC r9, vt r11) and zero before type (r0, then r9): a longer vt life
  (the vptr store 8+ insns after its `lis`) or an adjacent `lis LC; lfs`, and a zero qty ranked above the type qty --
  no statement order (all 48 permutations of max/colours/type/zeros: 11 or 9), `int zero`, `asm("li")`, extra zero
  uses, or `register .. asm("r0"/"r9")` pins (the pinned stores collect extra ANTI links from the later calls and move
  to the block top: 9-12) gives it; `len = 0; str = 0;` is the right source order (str must be the dying zero store,
  LUID str < len). The same three constants in reload's spill order r0/r9/r11 is the #13 shape again.
- **DB_WINDOW ctor (14 -> 3, zero code + one header ctor).** (1) The colour temp copy's `lwz 8..20(r1)` are frame-relative
  because the temp is built inside an inline taking the destination by pointer (`static inline DB_ColorSet(DB_COLOR* c,
  r, g, b, a) { *c = DB_COLOR(r, g, b, a); }`, `DB_ColorSet(&color, ..)`): written as a member assignment cse rewrites
  the copy's `(plus fp 12)` into the inlined ctor's `(plus this 4)`. (2) `DB_WINDOW() : color(1.0f)` with a new
  one-argument `DB_COLOR(f32 v) { a = b = g = r = v; }` (db_widget.h, unused elsewhere: emits nothing): the 1.0f
  expanded in the caller is an unchanging pool MEM whose `lfs` issues before the vptr store; the inlined default ctor's
  own 1.0f is copied by integrate.c WITHOUT RTX_UNCHANGING_P ("this MEM might not be const in the function it is being
  inlined into"), true_dependence then makes the pool load wait for the `this`-based vptr store (base ADDRESS(r3) vs
  SYMBOL -> may alias). Rule: a pool constant inside an inlined body is a normal memory read after inlining; when the
  target loads it above a preceding store, the constant was an argument of the inline. Left (3 words): the temp's
  a/b/g store order -- ours g, a, b because the 0.1 pseudo and the temp's `this` die at the g store (weight -1); the
  target's a, b, g is the RTL order as if neither died (the #13 family: the temp's `this` = `(plus fp 8)` REG_EQUIV
  pseudo unallocated and rebuilt by reload's address reload `addi r9,r1,8` with inheritance across the three stores
  reproduces exactly `stfs 8(r1)` + `k(r9)` stores + `k(r1)` loads; named temps, keep-alive asms on `&t`/the constants:
  5-14).
- **MakeExecSeqData (t_esp, 27 -> 2, zero code): `g_seqFlgNum[((PageView*) &g_page)->v]` -- g_page read through a struct
  view.** As a fixed scalar the `g_page` load (and `&g_seqFlgNum[g_page]`, `slwi`) is loop-invariant and hoisted above
  the loop, and high(g_page) then lives in a callee-saved register; the target reloads g_page and re-indexes per
  iteration: an in-struct load conflicts with the loop's block-copy/`head->num++` stores (ADDRESS-based `rec`/`head`
  vs SYMBOL -> may alias), so loop.c leaves it in the body. That alone puts the hoisted `lis`/`addi` into the target's
  r9/r31/r12/r5 (r12 = the last call-used register in REG_ALLOC_ORDER, taken in global.c's pass 0 before any
  callee-saved one; r31 = pass 1) and j/rec into r11/r10 (`u32 j` separate or shared `i`: same). Left (2): the clearing
  loop's HImode zero is r0 in ours and r10 (rec's later register) in the target -- global.c gives it r0 unless r0
  conflicts/is preferred; a `TOOL_SEQ* rec = 0` dead initializer (cse lowpart reuse) does not merge them (4).
- **IKreport (db_mod, 10, read): the target's `mr r11,r0` after `clrlwi r0,r0,24` is a gcse reaching-register copy for
  the THIRD test (`andi. r0,r11,128` in the join block after the `& 0x20` diamond) while tests 1/2 use r0.** The
  expression must be recomputed in the join: `(partsInfo[i] & 0xFF)` written per test folds to `& 0x30/0x20/0x80` at
  tree level (12, no clrlwi), `u8 info` narrows the load to `lbz 1(r9)` (8), the current `int info = .. & 0xFF` local has
  no copy (10). Not closed.
- **Save*FileNoUpdateCallback x5 (5 each) and position_usage (10): unchanged**; the assignment's no-op-move idea fails as
  pass 5 said (reload_cse deletes `(set r r)` before flow2; an asm with tied in/out registers survives to final and keeps
  the `ble`). position_usage: the target's `li r30,43` sits INSIDE the join block after `mulli r4,r31,14` (not a
  cross-jumped tail), and its first `y++` is issued above the first tail eprintf while ours stays behind the call
  (the later `y++`s hoist in both).

### Tool RELs, db_light_v2 pass 1 (t_sce/db_light + t_movie/db_light Matching 121/121 -> flipped, both RELs identical, 111 OK; t_movie edit_menu placeholder renamed; 2026-09-11)

- Harness ~/.cache/dbl_v2 (dol21a copies with module-aware paths: `mcmp.py MOD/UNIT` and `order.py MOD/UNIT` pick
  `build/G4BE08/<mod>/obj/<mod>/<file>.o` as the target, `mtryv.py MOD/UNIT FUNC v.py [--apply N]` compiles a variant of
  src/tools/db_light_v2.cpp as a module unit (`-G 0 -DREL_MODULE=<mod>`, strip_unused + fold_linkonce `--module`),
  `msbs.sh MOD/UNIT SYM`, `verify.sh MOD` = private `ngcld -r` link of the module's build.ninja object list with OUR
  db_light.o in place of the split object + `make_rel.py --verify` + `cmp` against orig/G4BE08/files/Rel/<mod>.rel);
  deleted at the end. `pninja.sh` (private manifest) was needed again: the shared `ninja` never settled ("manifest
  'build.ninja' still dirty after 100 tries") with the other agents running.
- **The unit was already byte-identical; the "114-115/121" and the objdiff percentages (updateLit 98.6%, lightCutWork
  94.3%, edit_light_select 99.4%, ...) are reloc spellings, not code.** `mcmp.py` gives 120/121 in both modules with the
  only row `_._5cUnit: 2 words` = the `.rodata+0x1608` (target `lbl_<mod>_rodata_1608`) vs `_vt.5cUnit` reloc-name
  pairing of the same address (the t_esp pass 4/5 note), and .rodata/.data/.bss equal, order OK. The percentages are the
  target's `lbl_<mod>_bss_A0` / `lbl_<mod>_rodata_608` data labels against our `LightToolPtr` / `.rodata+0x608`
  (objdiff compares reloc NAMES; `sync_rel_symbols.py` renames only the placeholders our object references as
  UNDEFINED names, never a unit's own local statics) -- the Matching t_camera/db_light shows the identical list of
  percentages. Judge module units with a masked-word compare (mcmp) and `make_rel.py --verify`, never with unit_info's %.
  No source change was needed: src/tools/db_light_v2.cpp stays the 9-line `#define DB_LIGHT_SET_TOOL_LIGHT` +
  `#include "db_light.cpp"` wrapper (STRIP_UNUSED build of Tools' object), zero code, no tags of its own.
- **t_movie `edit_menu` 0% = a duplicate-name placeholder, not a missing or different function.** t_movie has TWO static
  `edit_menu(void)`: db_light.cpp's (0x424, 0x188) and t_snd_vol.cpp's (0x1A3BC, 0x3A4). The sync gave `edit_menu__Fv`
  to the t_snd_vol copy first and then refused the db_light one ("keeping (edit_menu__Fv already defined elsewhere)"),
  so the db_light row kept the generated `edit_menu` and objdiff found no such symbol in our object. dtk accepts duplicate
  LOCAL names in a module symbols.txt (t_light carries eight: edit__Fv, edit_scale__Fv, load__Fv, save__Fv, ...; st2_0
  funcAshley2__FP3cEm, st2_3 setTexRender__Fv x3), so the row was renamed BY HAND in
  config/G4BE08/modules/t_movie/symbols.txt AND sym_map.tsv (column 6; the tool's `rename` will not do it); the next
  split picked it up (the concurrent agents' regeneration loop re-splits every few seconds; otherwise delete
  build/G4BE08/config.json). `git diff config/G4BE08/symbols.txt` clean (no DOL renames).
- Flip: `verify.sh t_sce` / `verify.sh t_movie` -> `make_rel --verify` OK and `cmp` identical BEFORE touching modules.py
  (the REL is the only judge of ADDR16 scopes: `LightToolPtr` LOCAL in ours = S+A like the target's scope:local label;
  the module COMMON block comes from db_light.cpp's `.comm common_<REL_MODULE>` asm). `"t_sce/db_light.cpp": True` and
  `"t_movie/db_light.cpp": True` in MATCHING; `pninja.sh -k 0` links `build/G4BE08/src/{t_sce,t_movie}/db_light.o`,
  t_sce.rel 32ad15d7.. / t_movie.rel 63a9e15a.. as build.sha1, 111 OK. t_movie's undefined `__pp__t9cVarRange1ZUci`,
  `__opi__t9cVarRange1ZUc`, `tcCurrentCameraNo__Fv` (stripped in this build, no `--link` list) resolve to the DOL like
  the original's imports -- the sync's "unresolved (no matching relocation in the split object)" lines are expected.

### Tool RELs, t_esp pass 7 (db_widget 111 -> 112/113: DB_WINDOW ctor 3 -> 0 zero code; t_esp 195 -> 196/212: MakeExecSeqData 2 -> 0 zero code; DB_STRING ctor 11 -> 8 with a pin (not applied), Save*FileNo x5 and db_mod position_usage / IKreport mechanisms sharpened; nothing flipped; 2026-09-11)

- Harness ~/.cache/tesp7 (dol20a copies with the paths rewritten; module-aware `mcmp.py MOD/UNIT [SYM]` (module vtable
  relocs `.rodata+N` vs `_vt.X` counted equal), `order.py MOD/UNIT`, `mtryv.py MOD/UNIT FUNC v.py [--apply N] [--asm N]`
  whose variants may also edit a header copy (`{'src': [...], 'hdr': {'db_widget.h': [...]}}`, the variant dir goes first
  on the include path), `msbs.sh`, `mdump.sh MOD/UNIT -dX` (absolute `SRC_OVERRIDE`, `-G0` like the module build), `fsec.py`,
  `prio.py`); build.ninja copied to `x.ninja` for `ninja -f` object builds without the manifest regeneration. Deleted at
  the end. 111 OK before and after every edit.
- **DB_WINDOW ctor (db_widget, 3 -> 0, zero code): the four-argument `DB_COLOR(r, g, b, a)` ctor stores `r, g, b, a` in
  that order (db_widget.h; the only caller is DB_ColorSet).** The temp's store order is sched1's register-pressure rank of
  the ctor's RTL order: with `r, g, b, a` the g store is +1, b 0 (the 0.1 pseudo dies), a -1 (the 0.3 pseudo and the temp's
  `this` die) -> a, b, g = the target; the old `r, a, b, g` order made g the -1 store. Rule: a temp copied through a
  pointer inline is ordered by the dying operands of the CTOR BODY's statement order, so read the target's store order
  as "dying stores first, then LUID" and permute the ctor body, not the call site.
- **MakeExecSeqData (t_esp, 2 -> 0, zero code): `rec = 0;` inside the count-clearing loop (`for (i..) { rec = 0; num[i] =
  0; }`).** The target's HImode zero is r10 = `rec`'s later register because the zero IS rec: loop.c hoists the
  invariant `rec = 0` into the preheader as an SImode zero, the `num[i] = 0` stores take its lowpart (cse's
  `notreg_cost` subreg rule), and `rec` (two sets: the zero and `head->rec`) is a global pseudo with BASE_REGS class
  (`addi`/`stw` through it), so global.c cannot give it r0 and takes r10 after i (r11) and num (r9). A `TOOL_SEQ* rec =
  0` at the declaration, `u16 z = 0` locals (top, before the loop, per iteration), `u32 z`, `*num++ = 0` all stay r0 (4-5).
  Rule: a constant that shares a register with a later POINTER value is the pointer variable's dead initialiser; the
  set must sit where loop.c hoists it (inside the loop) so cse's lowpart reuse sees it in the preheader.
- **DB_STRING ctor (db_widget, 11, mechanism exact, no source form applied).** (1) The target's four constant names
  (LC high r9, `_vt` r11, zero r0, type r9) are reload's spill round-robin over {r0, r9, r11} (`lis` needs BASE_REGS -> r9,
  then r11, r0, r9) on REG_EQUIV pseudos that local-alloc did not allocate = the #13 tie again; our local-alloc gives vt
  (high+lo TIED by block_alloc's "tie with any dying operand" rule on the elf_low insn: 4 refs / life 12 = 6666) r9 before
  LC (2 refs / life 4 = 5000) r11, type (2500) r0 before zero (1363) r11. (2) `register int zero asm("r0"); zero = 0; str =
  (char*) zero; len = zero;` gives ALL FOUR names (11 -> 8): the pinned `li r0,0` is issued FIRST at t=4 (sched1 boosts a
  SET of a register in `bb_live_regs` to max_priority -- `birthing_insn_p`; call-used hard regs are in bb_live_regs because
  every call marks them live and nothing kills them), so `lis LC; lfs` become adjacent (LC life 2 -> 10000 >= vt's 8/8)
  and LC takes r9, vt r11, type r9 (r0 busy). Residue: the r0 stores are issued early because in sched1 every later call
  (strcpy, strlen) collects an ANTI link on them through `reg_last_uses[r0]` -- the new[] result copy `mr r0,r3` is a PSEUDO
  set in sched1 and does not clear the list -- and **LOG_LINKS are never cleared between sched1 and sched2** (no pass
  frees them; flow2 prepends its def-use links; `add_dependence` only upgrades kinds), so sched2's depend counts include
  sched1's pseudo-based links: 5 dependents vs the target's 3 (`77 79 127`). Pinning the new[] result to r0 as well
  (`register char* p asm("r0")`) clears the uses but cse/reload_cse then forward r3 into the store (`stw r3,116`, the `mr
  r0,r3` vanishes: 6). Combine merges a single-use hard-register copy `(set r0 P)` into its use, so a second r0 SET
  needs two uses to survive to sched1. Not applied (tagged pins that do not close the function).
- **Save*FileNoUpdateCallback x5 (t_esp, 5 each, mechanism exact from the bytes, no pure-C form).** The target has BOTH
  compares and NO branch: `lha r11; cmpwi r11,0; cmpwi r11,255`. Reading the passes: jump1 keeps both branches (the dead
  `type = 0xFF/0` sets are not trivially dead -- the pseudo has uses in the compares); flow1 deletes the two sets; the
  branches then survive to jump2 only if their arm BLOCKS are non-empty at flow2's `find_basic_blocks (.., 1)` (its
  fallthru tidy deletes a conditional jump whose block has a single successor, and life_analysis then deletes the
  compare); at jump2 the arms must contain only USE/CLOBBER insns (`prev_active_insn` skips them) so both jumps are
  "jump to following insn" and `delete_jump` leaves the compares. A bare `(use hard)`/`(clobber hard)` insn is what
  satisfies both (flow never deletes a hard-register CLOBBER or a USE; final emits nothing), but at -O2 no C++ construct
  emits one: `use_variable` runs only under `obey_regdecls` (-O0), store_constructor's union CLOBBER targets a pseudo (a
  `register union .. asm("r11")` local is still expanded into a pseudo, 6 words), asm statements are active insns, and a
  no-op or redundant move is deleted by reload_cse before flow2. Leave the five at 5 words (`lha r0` + one compare).
- **IKreport (db_mod, 10, mechanism exact).** Ours: `int info = mw->partsInfo[i] & 0xFF` expands as `(and:HI load 255)`
  + `(zero_extend:SI ..)` (the FE narrows the u16 bit-op), which is why cse does NOT fold the tests into the load (its
  `lookup_as_function (reg, AND)` shortcut needs an AND-of-constant equivalence): `int v = load; int info = v & 0xFF`
  or `(u8) v` forms lose the `clrlwi` (tests folded onto the load: 22). The target's `mr r11,r0` is gcse's recomputation
  `R = E` of the byte expression inserted at the end of the first block (cse2 then folds it to `R = info`) for a second
  occurrence of E in the third test's block -- which requires that block to be OUTSIDE cse1's path from the first
  (every plain spelling puts it on the AROUND path: the `if (info & 0x20) type = 1;` diamond is a skippable
  straight-line block with a single-use join label, so cse1 folds the second occurrence to `info` and no copy exists).
  A `do {} while (0)` boundary plus a re-derived byte gives a fresh ebb but the re-derivation either folds (int/u16
  locals) or hashes differently (a second load, `(and info 255)`); no natural form found.
- **position_usage (db_mod, 10, read).** The target issues every `y++` (`addi r31,r31,1`) ABOVE the eprintf call whose y
  argument used the old y and keeps the LAST increment (`addi r31,1; addi r4,r31,-1; mulli` for "B"): ours folds the last
  `y++; (y - 1) * 14` by combine because y dies there (same block), i.e. in the original y was live after the last call
  or the increment and its use were in different blocks; `y++` inside the argument list (`y++ * 14`) creates a temp copy
  that cse folds the next `(y - 1)` into. Not closed.
- Facts read this pass: local-alloc `block_alloc` ties operand 0 with ANY dying register operand when no constraint
  requires a match (`lis`+`addi` pairs are one qty); `QTY_CMP_PRI = floor(log2 refs) * refs * size / (death - birth)`
  in half-insn units, ties by qty number = birth (SET) order; sched1's `sched_before_next_call` gives every insn USING a
  non-call-crossing pseudo an ANTI link to the next call, so a store from such a pseudo has prio 12 (anti cost 1) while
  a store from a call-crossing one gets the TRUE memory dependence (prio 13) and is issued first; ANTI/OUTPUT cost is 1
  on rs6000 (`rs6000_adjust_cost` returns 0, `insn_cost` clamps to 1); `insert_insn_end_bb` puts gcse's recomputation
  before the block-ending jump, after the compare.

### Tool RELs, bytes-first pass 16 (t_sce/t_block Matching 31/31 -> flipped, t_sce.rel byte-identical; t_movie/t_se_at seAtAreaEdit_DataInput 170 -> 58; t_esp_area/t_lightarea/t_camera_data/snd_test read; 2026-09-11)

- Harness ~/.cache/tools_p16 (deleted at the end): tesp6's module-aware scripts with the paths rewritten (`mtryv.py MOD/UNIT SYM
  V.py [--apply N] [--src ABS]`, `mdump.py MOD/UNIT -dX [--src ABS]` -- give `--src` an ABSOLUTE path, `msbs.sh`, `mcmp.py`,
  `order.py`, `prio.py`), `lcount.sh` (loop.c insn counts of every mtryv variant), `mkrel.py MOD [split.o=ours.o] --verify`
  (relinks the module with one object substituted into ~/.cache/tools_p16/rel/ and runs make_rel.py --verify against the
  original REL -- the flip check without touching build/), and rtl2.py fixed to list `insn/i` (inlined) insns. While other
  agents run `ninja`, build single objects with `cp build.ninja /tmp/x.ninja && ninja -f /tmp/x.ninja <obj>` (the pass-15 recipe).
- **tBlockSaveDataCreate (t_block, 38 -> 0, zero code): `u32 linkSize = nBlock * sizeof(BlockLink);` declared and computed AFTER
  the area loop (with the header writes), not before the memcpy.** Both pass-15 residues fall out of it: (1) the tail's
  `nBlock * 12` is fully redundant with the memcpy-size pseudo, so gcse deletes it and re-inserts `R = nBlock*12` at the END of
  the pre-loop block; cse2 turns that into the copy `mr r26,r30` (both live: the pLink advance still reads r30) and sched1
  issues the copy right after `bl memcpy` (its LUID precedes loop.c's later preheader inits, equal priority); (2) `linkSize + 24`
  now has its operand SET in the tail block, so it is not locally anticipatable and the block-based LCM cannot insert it into
  the loop. Read the block-based PRE (lcm.c pre_lcm) before guessing: `latein[bb] = delayin[bb]` for every block but the last
  (the `& ~delayin(succ)` term is applied to the last block only), `delayin` is an intersection over predecessors that starts
  at 0, so a block with a back-edge predecessor (an INNER loop header, t_block's 24-byte copy loop bb 8) never becomes
  delayed and breaks the delay chain: everything upstream on the chain is "latest" and `optimal = latein & ~isoout` then
  selects the blocks whose successors lost isolatedness -- that is how a single post-loop occurrence gets recomputed at the
  end of two loop blocks (t_block bb 6/7). Conversely a post-loop expression whose operand is set in the block right before a
  single-block loop stays put (t_esp_area, below).
- **tBlockArea_disp (t_block, 2 -> 0, 3 tagged dead sets): the area loop needs EXACTLY 5 more real insns at loop.c time** (pass
  1: 72 vs 67, so `71 * savings * lifetime >= insn_count` fails and BIT_ON's `lis 0x80000000` stays in the loop; pass 2: 71,
  moved, hence AFTER the pass-1 giv init `li r30,1504` in LUID). E must be 5: pass 1 needs >= 72 and pass 2 (65 + E + 1) <= 71.
  The same "+5" appears in both t_id loops of pass 12 -- a common construct, still unidentified. Written as case 3 in the
  case-2 style (`col = 0x00808080; if (..) { col = 0x00FF8080; disp(i, col); }` = 2 dead sets after cse folds the argument)
  plus three dead `col = 7/6/7;` after the switch, tagged `COMPILER-DIFF: 3 (loop.c pass-1 insn_count, dead sets)`. Facts:
  cse1 DELETES a set of a register to the value it already holds on the path (`col = K` at the body top then `col = K` in a
  case arm: the arm's set vanishes and the code changes), so dead-set fillers must differ from the reaching value; a
  `default:` arm with N sets costs N + 1 insns (its own jump to the end); a repeated `if (c)` test on the same operands is
  folded by cse1's `qty_comparison_code` (only +2 from its sets); a `for (;;)`-style nested repeat test is not a lever. A
  signed `n / 32` in bitOn (int parameter) gives exactly +5 and the target's preheader but leaves a `mr` + 4-bit rlwinm from
  the incomplete `(n + (n >> 31 >>> 27))` folding (24 words): the division sequence is the right SIZE, not the right code.
- **seAtAreaEdit_DataInput (t_se_at, 170 -> 58, zero code): (1) per-arm `int v, n;` in cases 1, 2a, 2b, 3, 4 (block-local
  qtys: local-alloc ties n to the dying v -> `cmpw r11,r0; li r11,0; ori r11,r11,0x8000` without the `mr`, the pass-15
  seAtAreaEdit lesson), while case 0 keeps the FUNCTION-scope pair (its `mr r0,r11` copy is a global pseudo); (2) ONE `u8 col`
  for both halves (the pass-15 `int col2` was wrong: the tail's colour is in the same callee-saved r29 as the first half's
  because the pseudo crosses the name loop's calls, and its zero feeds the three `stb r29` SUB_RESET stores); (3) a separate
  `u32 k` counter for the trailing flags loop (i shared by the case-5 loop and the tail put i above y in global-alloc
  priority: with two counters y takes r31 and i r28 like the target). Left 58: col r28/oy r29 vs the target's r29/r31 (col
  must be allocated before oy: equal `floor(log2 refs) * refs / len` -> allocno order 83 < 90; ours has col len 76 vs oy 68),
  the #2 masks `clrlwi r5,r29,24` in the name loop and the PRE-shared `clrlwi r5,r29,24; mr r29,r5` extension in the tail
  (`int c = col; asm("" : "+r"(c)); (u8) c` keeps the loop mask, `asm("" : "+r"(col))` on the u8 itself does not, neither
  fixes the allocation order), and the `lis seAtWk` / `li col,0` LUID swap at 0x9c.
- **ToolEspArea / ToolLightAreaMain (442 / 395, read; `CreateEditWindow` rewritten with a `cDbgEditWindow<T>* edit` local like
  the other Create* helpers -- same bytes).** The target's `cmpwi r31,0` (edit == 0) sits among the inlined ctor's last zero
  stores BEFORE the AddButton loop with the CR kept in `mfcr r29`/`mtcrf 128,r29` across it. By pre_lcm this needs the block
  ending at the loop head to be TRANSPARENT for the compare, i.e. a basic-block boundary between `mr r31,r3` (the `new`
  result) and the ctor stores; ours has `new`, `mr`, strlen, the stores and the loop entry test in ONE block (bb 13) so the
  compare stays in the tail (T is its own optimal point). `-fcheck-new` gives the boundary but two compares and a `beq`;
  `-fexceptions` (SN's cc1plus defaults flag_exceptions to 0 in cp/lex.c lang_init_options -- the pass-2 "-fno-exceptions
  changes nothing" probe proved nothing) gives exactly the `mfcr` shape because calls inside an EH region end blocks, but also
  `__get_eh_context` and `__builtin_delete` landing pads the target does not have. The construct that leaves a used label or
  jump between the `new` and the ctor body with no bytes was not found. The `_._`/AddButton 2-word diffs of both units are
  `_vt.` reloc naming only.
- **tcSetBesideOffset (t_camera_data, 27, read):** pointer locals for `&c->roll[n]`/`&c->fovy[n]` re-base the n*4 giv on
  812 (55), `f32 r/f` value temps 42, `int k = n * 12` byte offsets 27 (identical RTL). The n*12 giv (regno 207) must beat the
  n*4 giv (209) in global-alloc priority; equal REG_LIVE_LENGTH would do it (allocno order), but the preheader `li 396` /
  `li 0` order is fixed by the giv reduction order AND by sched1's LUID tie, so n*12 is always born one insn earlier (84 vs
  82). Not closed.
- **snd_test disp_sequencer (64, read): the 4-byte `.rodata` word `.4byte Snd_voice_work` between "%3d" and "- WTREGION - "
  is the unit's .rodata size difference (0x1004/0x1000) and shifts every later string (the 2-3 word "diffs" of cursor_disp,
  load_select, Snd_test_disp_aux, ...).** It is loaded as `lis; addi; lwz r9,0(r11)` in the seq_note_count loop preheader
  (a hoisted invariant load, not a `lwz @l`). `static T* const p = Snd_voice_work` (local or file scope), a 1-element const
  pointer array and a const struct are all folded away or (volatile) put in .data by our compiler; reload's
  `reg_equiv_memory_loc = force_const_mem` path applies only to non-LEGITIMATE constants (CONST_DOUBLE), not symbols. Source
  of the pooled address not found. test_play_or_stop (2): the target re-reads `w->reqCur` (`lhz r4,32(r31)`) after the inner
  `Snd_test_get_str_name` call instead of keeping it in r30 (a REG_EQUIV mem re-read across a call, or a MEM argument left
  unpromoted until load_register_parameters); not iterated.
- t_movie/t_snd_vol (21/27): the six residues are 104-812 words with .text size differences up to 0x5d0 (edit_reverb_param,
  file_save, combine_tbl_edit) -- structural rewrites, not tie-breaks; not started this pass.

### Tool RELs, bytes-first pass 17a (t_movie/t_se_at seAtAreaEdit_DataInput 58 -> 17, one tagged item; t_esp_area/t_lightarea CreateEditWindow mechanism read to the LCM equations; snd_test pooled address forms tried; 2026-09-11)

- Harness ~/.cache/tools_p17a (deleted at the end): dol23a copies + `mtryv.py MOD/UNIT FUNC V.py [--apply N] [--src ABS] [--all]`
  (cflags/post_build parsed from build.ninja after joining `$\n`, judged with `OBJ=... bytecmp.py --residue`; env `INC=dir`
  prepends an include dir for header-copy experiments), `mdump.sh MOD/UNIT -dX` (module defines, `-G0`, `-fno-implement-inlines`
  for Tools), `msbs.sh`, `prio.py` on fsec-cut `<pre>_lreg/_greg.txt`. 111 OK before and after; nothing flipped.
- **seAtAreaEdit_DataInput (t_se_at, 58 -> 17, 19/20 functions, .text size equal):**
  (1) zero code: the two name loops are `eprintf(pW->x, pW->y + j * 16, ...)` / `eprintf(0x38, 0xF0 + j * 16, ...)` -- the `oy`
  variable was wrong: the target's `li r31,0` / `li r31,240` are loop.c giv inits, issued LAST in the preheader (they are created
  after gcse's `lis` insertions, so their LUIDs are the highest), and with two per-loop giv pseudos the first-half register
  clique allocates oy r31 / loop ptr r30 / col r29 / "%s" high r28 / end ptr r27 like the target (one function-scope `oy` was
  14 refs / 70 insns = priority 0.6, above y (0.508), so it took r29 by pass-0 reuse and y the fresh r31).
  (2) zero code: `int col` with `(u8) col` at every eprintf -- the tail's `clrlwi r5,r29,24; mr r29,r5` is the `(u8)` extension
  computed once at the join and PRE-copied for the eight later uses (a promoted `u8 col` passes `mr r5,col`, no mask; `int col`
  without the casts has no mask at all). The extension pseudo takes col's r29 because col dies at the mask.
  (3) TAGGED `asm("" : "+r"(col)); // COMPILER-DIFF: 3` right before the first name loop: our gcse PREs the loop body's
  `(zero_extend (subreg:QI col))` into the preheader (`clrlwi r28,r30,24` hoisted, 145 words), the target keeps `clrlwi
  r5,r29,24` in the body. Read off lcm.c (block-based): the body occurrence stays iff the preheader is NOT transparent (col set
  there: then earlyout[pre] = 1, delayin/latein[body] = 1, isoout[body] = isoin[body] & isoin[after] = 1 -> optimal[body] = 0 and
  redundant[body] = 0) or the body is not antloc (col set in the body before the extension). Tried and rejected as zero-code
  killers: `col = col` (jump1 deletes the noop before gcse), `col = (u8) col` (computes the extension into col: the body's copy
  becomes fully redundant), `col &= 0xFF` / `|= 0` / `*= 1` / `+= num - 6` (folded), a `u8 c = col` in the body (E into c, hoisted),
  `register int col asm("r29")` (138-142), `s8`/`u8` col with a `u8`-parameter eprintf redeclaration (80-186). The asm costs a
  sched1 issue slot in the preheader (`lis r28 "%s"` / `addi r30` order, 4 of the 17 words).
  Left 17: the then-arm `li r29,0` is issued BEFORE `lis r9,seAtWk@ha` in the target = haifa's birthing boost (adjust_priority:
  dest set ONCE in the function and live at block end -> priority = max, tie with the `lis` by LUID) -- so the target's
  first-half zero is a single-set pseudo although the else arm's `li r29,7` is a low-priority insn of the same r29 (two
  pseudos in one register with no copy, or a set flow did not count: not understood; `int col = 0` at the top, `col = 0` before
  the clamp, `{ int z = 0; col = z; }`, a clamp rewritten through an `n` local all 17-155); case 0's `v` r11 vs r9 (global pass 0
  skips r9 for a reason not visible in the dumps -- the second `and.` result is a reload SCRATCH, so not the cause; per-arm
  `int v, n` in case 0 changes nothing); case 5's `mr r29,r27` one slot later.
- **ToolEspArea / ToolLightAreaMain CreateEditWindow (read to the equations, not closed).** The target's `cmpwi r31,0` before the
  inlined ctor's AddButton loop with `mfcr r29` / `mtcrf 128,r29` is gcse PRE of `(compare edit 0)` from the post-loop block into
  the END of the last pre-loop block, and the CC pseudo living across the loop's calls is allocated to a callee-saved GPR (rs6000
  HARD_REGNO_MODE_OK is 1 for any mode in an INT reg; reload then does the CR0 output reload `mfcr` after the compare and the
  input reload `mtcrf` before the `bne`). With lcm.c's block formulation this insertion happens iff there is a basic-block
  boundary anywhere between the `mr r31,r3` (the set of `edit`) and the loop head: then earlyout[the block with the set] = 1,
  earlyin/earlyout of the transparent pre-loop block = 1/0, earlyin[body] = 0, delayin[body] = 0 (the back-edge predecessor
  keeps the intersection at 0), latein[after] = 0, isoin[body] = 0 -> optimal[pre-loop] = 1, redundant[after] = 1. With the set
  and the stores in ONE block (ours) earlyin[body] = 1 -> latein[body] = 1 -> isoout[pre] = 1 -> optimal[pre] = 0 and the
  compare stays after the loop (its own optimal point). What creates that boundary with no bytes was not found: `-fcheck-new`'s
  `p != 0 ? (ctor, p) : p` leaves its `beq` (thread_jumps would only redirect it, the target has no branch there); a jump cse1
  folds (constant `num`/`rows` entry tests, `if (name)`) is gone before gcse (the second jump pass deletes the unused label); a
  CALL ends a block only inside an EH region or with nonlocal labels; loop notes and inline boundaries do not split blocks. A
  jump whose condition only cse2 can fold (a constant propagated by gcse's cprop from another block) would leave the boundary
  at gcse time and no code -- no natural CreateEditWindow statement of that kind was found. The .rodata "vtable slot" rows of
  both units (`_._t14cDbgEditWindow..., 0` vs `0xc`) are the 12-byte .text size shift of this one function (cmpwi/mfcr/mtcrf),
  not a vtable order difference; the `fn_Tools_2683C/2F2D8` 24-word rows are the nameless linkonce block pairing.
- **snd_test disp_sequencer `.4byte Snd_voice_work` (read, not closed):** the target reads the array base as `lis; addi; lwz
  r9,0(r11)` in the 64-voice loop preheader (a full address then a zero-offset load = `(mem (reg))`, not a pool `lwz @l`).
  A function-local `static SND_VOICE_WORK* const p = Snd_voice_work`, `static ... const tbl[1] = {..}; tbl[0]`, a `static const
  struct {..} tbl; tbl.p`, `static const u32 addr = (u32) Snd_voice_work` and a `static ... const& p` all fold (expand_expr's
  readonly ARRAY_REF/COMPONENT_REF constructor lookup and decl_constant_value; the reference costs 42 words). The same
  preheader also shows `li r9,84; addi r27,r9,84` for the `0x54 + 0x54` y (the first 0x54 in a register = a REG_EQUIV constant
  variable, #13 family) and `addi r10,r30,1; mr r26,r10` for `ch + 1` computed early: disp_sequencer has more than the pool
  word; not iterated further.
- t_camera_data tcSetBesideOffset (27) / tcDataExport (142), t_lightarea fn_Tools_30410 (59) and the 23 other snd_test functions
  were not iterated this pass.

### Tool RELs, t_esp pass 8 (t_esp 196 -> 200/212: Save*FileNoUpdateCallback x5 5 -> 0 zero code; Load/SaveEmTypeUpdateCallback 87/96 -> 2/2, ID_WINDOW ctor 254 -> 157, ToolEspMain 72 -> 38, InitTool 12049 -> 11628; db_widget DB_STRING ctor 11 unchanged; nothing flipped; 2026-09-11)

- Harness ~/.cache/tesp8 (dol21a copies made module-aware: `mcmp.py MOD/UNIT [SYM]` reads `build/G4BE08/<mod>/obj/<unit>.o`
  and counts `_vt.X` vs `.rodata+N` reloc pairs equal, `tryv.py MOD/UNIT FUNC v.py` compiles with the module cflags + strip_unused/
  fold_linkonce `--module`, `dump.sh MOD/UNIT -dX` with `-G0 -DREL_MODULE`, `-dw` = the flow2 dump, `ins.py DUMP FUNC [re]` = one
  line per insn incl. the `insn/i` inlined ones (rtl.py misses those), `qty.py LREG FUNC` = local-alloc births/deaths/priorities
  in half-insn units). Deleted at the end. 111 OK before and after every edit.
- **Save*FileNoUpdateCallback x5 (t_esp, 5 -> 0 each, zero code): the dead `cmpwi 0; cmpwi 255` pair is a wrap of the model type
  whose CLAMPED copy is written back through a THIRD test.** `step = (s16) g_modelType; type = step; if (step < 0) type = 0xFF; if
  (step > 0xFF) type = 0; if (type != step) step = type;` -- `step` reused for the type (its r11), two independent `if`s (an
  `else if` leaves a `b` that jump2 inverts into `blt`), and a third test on the clamped copy whose arm is a dead set. Mechanism,
  read pass by pass: the arms `type = 0xFF/0` are LIVE at flow1 (test 3 reads `type`), so both arm blocks are non-empty at flow2's
  `find_basic_blocks(cleanup)` and their `bge/ble` survive; test 3's own arm is dead at flow1 and empty at the cleanup, so
  `tidy_fallthru_edge` deletes its jump, life_analysis deletes its compare, then `type` is dead and the two arm sets go; jump2
  finds `bge L1; L1:` / `ble L2; L2:` = jumps to the following insn and `delete_computation` deletes only the jump after reload
  (`reload_completed && flag_schedule_insns_after_reload` short-circuit), so both compares stay. Every dead-set / noop-move / USE /
  CLOBBER idea is closed by the source: reload deletes ALL bare CLOBBER insns at its end (reload1.c 1179), reload_cse deletes
  `(set r r)` before flow2 (only a REG_FUNCTION_VALUE_P dest survives as a `(use)`, i.e. non-void functions only), a pseudo live
  at flow1 cannot die by flow2 unless its use is in a jump that the cleanup removes. Rule: "both compares, no branch" = a
  test whose result feeds ONE more conditional whose arm is dead.
- **Load/SaveEmTypeUpdateCallback (87/96 -> 2/2): the group-skip loops read off the target.** (1) `ModelTypeStep`/`GroupSkip` read
  `g_pKey->` directly (no `k` parameter: a non-const pointer parameter of an inline is copied by integrate.c and the copy `mr r7,r10`
  then names the rep[RIGHT]/trg[] loads in ours the wrong way; the target's `mr r7,r10` is cse turning the second `g_pKey` load
  into a copy of the first). (2) the direction keys are `trg[KEY_R]` / `trg[KEY_Z]` (0x7c/0x80), not `rep[KEY_L]/[KEY_R]`.
  (3) the wrap constant is `MODEL_NAME_NUM` (243) in the tail too (`addi r0,r7,-242` = t + 1 - 243), not 242. (4) both loops
  keep the last `u16 t = g_modelType` and compute `g_modelType = t + dir` from it (`add r9,r7,r5` with no reload), compare the
  name bytes through `a0`/`a1` locals (the second loop's `d0` IS `a0`, r6), the second loop is a `do { step; t = load; n = tbl[t];
  if (a0 != n[0]) break; } while (c1 == n[1])`, `c0 = name[0]; c1 = name[1];` in that order. Left (2): loop 2's fresh
  `high(g_modelType)` (loop.c-hoisted `lis r12`) belongs to the last `lhz t` in the target and to the loop-top store in ours --
  cse1's path into the loop top would have to know the outer high (a `b`-entered top / follow_jumps), no plain form found.
- **ID_WINDOW ctor (254 -> 157): two structural facts.** (a) `DB_NUMERIC2* n` is ONE function-scope variable assigned in four
  widget blocks: a 4-set/4-death pseudo is not a local-alloc qty, global.c gives it r31 -- local-alloc never hands out r31
  (`find_free_reg` marks every ELIMINABLE_REGS `from`, i.e. the frame pointer), so a single-block ctor whose target uses r31 has a
  multi-block/multi-death pseudo; with block-local `n`s ours used r22..r30 and shifted every callee-saved name by one. (b) the
  `CreateString` widgets are `pa->CreateString(win, "..", &DB_POINT(x, y));` -- the address of a TEMPORARY (cc1plus warns "taking
  address of temporary", accepts it): the pos stores are then expanded inside the argument evaluation, after the `win`/`pa` loads
  and the string `lis`, which is the target's `lwz r4; lwz r3; lis r5; stfs f31; addi r5; mr r6; stfs` order in all 13 blocks
  (a block-local `DB_POINT pos(x, y)` puts both stores first). Applied to all 117 CreateString blocks of the file: InitTool
  12049 -> 11628 (size 0x9ebc -> 0xa0a4 of 0xa20c), frame unchanged. The numeric blocks (`int sx` + `&pos`) are NOT that form:
  the target issues their pos stores early and the `win`/`pa` loads before the `sx` frame store; with the temp form the loads
  stay last (the sx store blocks them), with the local form the loads are late too -- open (157).
- **ToolEspMain (72 -> 38): `sel = &WIN_SEL(g_pEditActive); if (g_pEditActive == g_pEditWin3 && sel->selX == ..)` for the first
  two selX tests** (the target's `lwz r11,4(r9); addi r11,r11,148; lwz r0,20(r11)` = a pointer local, a direct expression folds
  to `lwz r0,168(r9)`); the later PosRand pair keeps ONE shared `sel` across the call (r30) as before. Left: the entry's
  `g_work/eventNo/eventSNo` store block (the zero `li r11,0` issued before the `stw g_pKey`, the `g_work` high not tied into the
  `addi r3` argument) and two hoisted-`lis` slots around the `new` calls.
- **DB_STRING ctor (db_widget, 11, unchanged; the local-alloc arithmetic).** The whole ctor is one block, so all four constants
  are local qtys and reload never sees them; their names are the qty order `QTY_CMP_PRI = floor(log2 refs)*refs/(death-birth)`
  (half-insn units, births/deaths in the POST-sched1 order): vt-hi+lo tied 8/12 = 6666 beats LC 2/4 = 5000 and type 2/8 = 2500
  beats zero 3/22 = 1363; the target needs LC >= vt (LC r9 first, vt r11) and zero >= type (r0, then r9 freed by LC's death).
  Levers computed: LC life 2 is impossible (issue rate 2 puts `lis vt` in the second slot of the same cycle), vt life >= 16 needs
  the vptr store >= 8 insns after `lis vt`, zero needs a 4th ref or life <= 12, type needs `li 4` issued at t<=5 and stored last.
  32 forms tried (member-initializer lists, `int t = 4`/`int z = 0` at the top, chains, orders, `new char[max]`): every one 11
  (the store block re-sorts to the same schedule). The "unallocated REG_EQUIV" reading of pass 7 is equivalent in effect but has
  no source lever either. Not closed.
- Not iterated: AddSeq 197, EspToolMain 40, PartPasteSeqData 46, EditActiveChange 50, MakeSaveSeqData 58, PosActiveChange 62.

### Tool RELs, db_mod pass 1 (t_esp 57 -> 59/75, Tools 45 -> 47/63, 2246 -> 1850 words, .text gap 0xF0 -> 0x2C; dbmodInfoDisp 107 -> 0 and drawOrientation 115 -> 0 zero code; dbmod_motion 212 -> 56, dbmodDispModelName 227 -> 209 (+68 -> +24 bytes); nothing flipped; 2026-09-11)

- Harness ~/.cache/dbmod (deleted): `mbuild.sh MOD SRC OUT` compiles db_mod for either module with the module's own
  defines (`-DREL_MODULE=t_esp -DTOOLS_ARRAY -DTOOLS_EM_ARRAY` / `-DREL_MODULE=Tools -fno-implement-inlines -DTOOLS_ARRAY`);
  the OUTPUT MUST BE NAMED `db_mod.o` in its own directory -- ngccc.py derives the module unit for `place_linkonce_module`
  from `-DREL_MODULE` + the output stem, any other stem loses the 0x3B8 nameless cManager<cEm> block and the ctor order.
  `mtryv.py FUNC v.py [--apply N] [--mod Tools]` (variants slice the function body out of the file and rewrite it),
  `msbs.sh MOD SYM [OBJ]`, `mdump.sh MOD -dX`, `mflags.sh MOD FUNC "flags"` (also `CC1DIR=` for a hooked cc1plus,
  `SRC_OVERRIDE=`), `tree.py build LABEL ENV=1 | cmp LABEL | diff A B` = the WHOLE-TREE harness: every prodg_cc edge of
  build.ninja compiled with a hooked cc1plus (`~/.cache/dbmod/sngcc`, copy of tools/sn-gcc {Makefile,src,obj}, env-var
  hooks in gcse.c) in 7 s on 24 threads, bytecmp per unit in 3 s, regressions/fixes as function lists. 111 OK before
  and after.
- **dbmodInfoDisp (107 -> 0, zero code): the coordinates are `300 + x * 7` (the 7-pixel eprintf2 font: `x = 0` for the
  label column, `x = 7` = seven characters over for the values) and `y * 10` with `y` a loop biv (`y = 3; ... y++` at the
  end of each body), not `300 + x` / `y + i * 10`.** Reading: (1) the target's `li r29,0 .. addi r5,r29,300` and
  `li r31,49 .. addi r5,r31,300` are NOT unpropagated constants (a `li rA,K; addi rB,rA,C` pair with a single-set `int
  x = K` is always cprop-folded by both builds: whole-tree `NO_CONSTPROP` regresses 87 functions, so the original's
  gcse does constant-propagate; `GCSE_SINGLESET` -- skip multi-set pseudos -- regresses 106; `GCSE_NOMERGE` -- one hash
  entry per set insn -- changes nothing anywhere): they are the loop-invariant `x * 7` hoisted by loop.c into the
  preheader and folded there by cse2 (`(mult x 7)` is not a form cprop can fold -- validate_replace_rtx_1 only
  simplifies PLUS/MINUS/extensions -- so the mult survives into the loop, loop.c hoists it, cse2 sees `x = 0` in the
  preheader ebb and makes it `li`), one pseudo per loop (r29/r31 = two different products, not one variable).
  Rule: `li rA,K; addi rB,rA,C` where K is a product of a small constant = a hoisted `var * c` with `+ C` left in the
  loop; look for the font width. (2) the eleven `li rN,30 .. addi rN,rN,10` registers are eleven `y * 10` givs of a
  SECOND biv `y` (each call site its own single-use giv, loop.c never combines single-use DEST_REG givs), and `y` is
  eliminated after reduction; the switch is on `i`, so cse's jump-follow knowledge of `i == k` in the case bodies never
  meets the row expression (a `y + i * 10` spelling is folded to `li r6,30/40/50` in cases 0-2 by cse1's TAKEN paths --
  our cse1 does follow the `beq` into every case body with a one-use label preceded by a barrier, the original too:
  Sscrn SsMapInit::move's 5-case switch stores the switch register). The t_camera author's `y++` idiom, same tool team.
- **drawOrientation (115 -> 0, zero code):** `Vec v[2]; Vec w[2];` (the frame has a/b at 0x8/0x14 and wa/wb at 0x20/0x2c
  = 12-byte spacing: BLKmode locals get align -1 = 8 bytes and size rounded to 8, so two separate `Vec a, b` cost 16
  each; an array of two is ONE 24-byte object), `Vec zero = {0.0f, 0.0f, 0.0f};` at the declaration (the memset
  libcall with `crclr cr1eq` BEFORE the `p == 0` test; the old prototyped `memset(&zero, 0, 12)` had no crclr), and the
  row-pointer matrix copy `f32 (*d)[4] = m; f32 (*s)[4] = p->mat; i = 3; while (i--) { dp = *d; sp = *s; for (j < 4)
  *dp++ = *sp++; s++; d++; }` with `d` declared BEFORE `s` (loop.c reduces the `+16` givs in pseudo order: d's step
  gets r11, s's r10). `v[0] = v[1] = zero` chains stay.
- **dbmodDispModelName (227 -> 209, 0x604 -> 0x630 of 0x648) and dbmod_motion (212 -> 56, size exact):** (1) the row is
  the literal `(i + 4) * 14` at every call, no `int row` local: the target's `addi r0,r26,4; mulli r31,r0,14; mr
  r20,r0` is the header's temp plus gcse's PRE copy of `i + 4` for the k-loops, which recompute `mulli r4,r20,14`
  (with a `row` variable the mult is computed once and the copy disappears). (2) the `switch (motType) { case 0: case 1:
  case -1: break; }` the target keeps as `cmpwi 0; beq J; bgt J; cmpwi -1` (no branch after the last compare) is a
  switch with DEAD sets in two arms and an EMPTY case 1: `case 0: color = 0; break; case 1: break; case -1: color = 2;
  break;` -- `color` has other uses so the sets survive cse1's delete_trivially_dead_insns, flow1 kills them, jump2
  deletes the `beq J` of case -1 as a jump-to-next and leaves its compare (the pass-7 Save*FileNo mechanism); the empty
  case 1 folds `cmpwi 1; beq J; b J` into the `bgt J`. Three plain empty cases delete the whole tree. (3) motion:
  `if (motNo[k] != -1) { ... }` around the middle of case 1 with ONE `dbmodGetFilenames(); break;` after it (the
  early-exit copy's `bl; b` tail is never cross-jumped: a call ends the block); the `^` line re-reads
  `pDbModState.p->sub` at every use (no `k` local: `lbz 6(rN); extsb` after each call) and is spelled `(hs - 1 + (digits
  - digit) + 25) * 8` (fold makes `add; addi 24; slwi` -- the `25 + hs - 1 + ..` order gives `addi 24` first); `x = 6;`
  assigned right before the display loop and `nx = 16;` INSIDE the loop body (top): with the sets at the function top
  gcse PRE hoists `x * 8`/`nx * 8` out of the 2-iteration loop (`slwi r17,r0,3` in the preheader, `mr r3,r17` per use),
  the target keeps `li r15,6`/`li r19,16` and a `slwi r3,rX,3` at every use; the in-loop `nx = 16` is loop.c-hoisted
  (not used before set) behind the `i = 0` init, which is the target's `li r27,0 .. li r19,16` order. Left: motion is
  pure register naming (i r27 vs r28), DispModelName's remaining 24 bytes are the "%s" string high (five uses, each its
  own `lis` in the target, ours PREs two) and the `[%6s] "null"` arm whose `li r27,0` (the folded `i - 1`) the target
  schedules before the call so the arm is not cross-jumped with the `--------.---` arm.
- Not moved (read only): position_usage 10 (the last `y++` kept live: `return y`, trailing `y++`, dead do-while, Back
  duplicated into the arms -- 10/12/29 words), IKreport 10, GetFilenames 222 (the target's name-table offset is a biv
  `li r22,929; addi r22,r22,2048` per type copied `mr r27,r22` into the inner loop, `int t = 0` kept live and no `dir =
  0`: `char* dir;` alone -4 bytes; an explicit `ofs += FILE_NUM` counter 226), locate 475, blend 185, MotionMove 194,
  p_info 159 (its `x = 6` has the same hoisted `slwi r17` shape as motion had), option 88, light 81, trans 71, scale 64.
- Compiler-side hypotheses tested whole-tree and REJECTED this pass (harness numbers = regressions / fixes vs the
  installed compiler; the 2-3 "fixes" in every run are stale ninja objects of units other agents were editing):
  cprop skipping multi-set pseudos 106 / 0; one SET hash entry per insn (no merging of identical `(set x K)` from
  different blocks) 0 / 0 -- the em2b/t_scroll "two arms set 0x11" note is not explained by merging; no constant
  propagation at all 87 / 5 (the five t_esp Save*FileNoUpdateCallback become identical -- their 5 words are cprop's
  alter_jumps folding of a constant into a conditional jump, a lead for that family); no cprop at all 1617.
  `-fno-cse-follow-jumps` reproduces InfoDisp's eleven givs but not its x and regresses 8 functions of this unit.

### Tool RELs, bytes-first pass 17b (Tools/t_motseq 353 -> 202 words: msqDisp 147 -> 4, msq_R0_QuitCk 7 -> 0; t_event/t_event 1095 -> 900: MainPreview 195 -> 0; t_id / t_snd_vol not started; nothing flipped; 2026-09-11)

- Harness ~/.cache/tools_p17b (deleted at the end): tesp8's module-aware scripts with the paths rewritten (`tryv.py MOD/UNIT SYM V.py`
  with the `headers = {variant: {"hdr.h": [(old, new)]}}` dict for header variants, `sbs.sh`, `mcmp.py`, `order.py`, `dump.sh MOD/UNIT -dX`
  with an absolute `SRC_OVERRIDE`). Build single objects with `flock build/.ninja.lock ninja <obj>` (the pass-15 unlocked copy-manifest
  recipe clobbers `.ninja_deps` -- "premature end of file; recovering").
- **The kept dead `cmpwi` = a conditional jump that jump2 deletes AFTER flow2.** SN's toplev.c runs `find_basic_blocks + life_analysis`
  (flow2, `-dw` dump) between reload_cse and sched2, and jump2 after sched2; `delete_computation` with `reload_completed &&
  flag_schedule_insns_after_reload` deletes only the jump. So: a body deleted before flow2 (trivially dead set, flow1-dead set, cse
  fold, `update_equiv_regs` move, reload_cse noop move) leaves a jump-to-next that flow2's `tidy_fallthru_edge` removes and whose
  compare life_analysis then deletes (ours); a body that survives flow2 and is removed by jump2 itself (cross-jump into an identical
  arm, jump threading, `delete_noop_moves`) leaves the compare. msq_R0_QuitCk (7 -> 0, zero code): `switch (w->sub3) { default:
  msqSetMode(3); break; case 0: msqSetMode(3); break; case 1: msqSetMode(3); break; }` -- default written FIRST with three identical
  arms: case 1's arm cross-jumps wholesale into the default body, its `beq` becomes jump-to-next, jump2 deletes it and `cmpwi r0,1`
  stays. The two-arm `case 1: default:` grouping emits no compare on 1 at all; `default` written last or an if/else chain lay the
  bodies out so that the arms do not merge (9-21 words). The same construction is still open for msq_R0_Sequence's `cmpwi r30,0`
  after the `if (cur2 > 0) .. else ..` (the then-arm jumps to the compare, so it is a statement after the inner if/else inside `if
  (cur != cur2)`): dead sets of cur/cur2/m/w, `(s16)` self-casts, `cur2 = cur2 + 0`, re-loads of m/MSQ all vanish before flow2 or
  keep code (96 words; not closed).
- **msqDisp (147 -> 4, zero code, six facts):** (1) `int cx;` set to 3 (sequence half) and 48 (copy half) with `cx * 8` at every x
  argument: a two-set variable is not cprop'd by gcse (single-set regs only), so the pre-loop eprintfs fold in cse1's ebb (`li r3,24`
  / `li r3,384`), the loop's `cx << 3` is hoisted by loop.c and folded by cse2 in the preheader (`li r23,24`), and the post-loop
  if/else arms compute `slwi r3,r17,3` (new ebb; in the copy half the arms' occurrence is gcse-PRE'd into a second preheader
  register `li r25,384` next to loop.c's `li r26,384`). (2) The row y of `eprintf(cx * 8, y, .., i, flag[i] ? "ON" : "--")` is
  `238 + j++ * 14` with a SEPARATE post-incremented counter `j` (i is the for counter used by `%1d`/`flag[i]`): the ternary argument
  makes expand_call precompute the arguments, so the giv copy `mr r4,r30` and, after the biv increment of j in the SAME (argument)
  block, loop.c's giv step `g += 14` sit before the branch, and regmove's optimize_reg_copy_1 rewrites the step as `addi r30,r4,14`
  off the argument copy. `y += 14` after the call, `for (..; i++, y += 14)`, `int y = yy; yy = y + 14;` and `(yy += 14) - 14` give
  54-95 words; `flag[i++]` in the argument moves `i++` into the argument block too and passes i+1 (74). (3) `GXColor c1 = {k80, 0x80,
  0x80, 0x40}` with `u8 k80 = 0x80; u8 k40 = 0x40;` variable FIRST bytes (the rckDrawPointLineNow idiom: `stw 0; stb r0(-128)`
  instead of the folded `lis 0x8000; stw`); a 3-element initializer + `.a =` store still folds the first byte. (4) `f32 tx = 248.0f`
  for the two cursor tiles is a SEPARATE single-set variable from the loop's `x = 40.0f` (both end in f31): a single-set pseudo live
  at the block end gets haifa's birthing boost and its `lfs` issues before the shared 17.0 word `lwz r31`; one `x` set twice loses
  the slot (10 words). (5) `f32 rowY = 369.0f;` declared INSIDE the outer loop body (block-local, `rc.y = rowY`): loop.c hoists the
  set into f29 (callee-saved) and the pool order stays `5, 4, 24, 25, 15, 369` -- the same variable declared before the loop puts
  369.0 before the loop constants in `.rodata` (pool entries are created after loop.c, not at expand), and the literal `rc.y = 369.0f`
  (load adjacent to its store) is never hoisted. (6) `if (y0 < 0 || y0 >= seqMax) col = c3; else if (..) {..} else col = c2;` (the
  c3 arm first) and the cursor tile's `rc.y = ..` written BEFORE `rc.w = 17.0f` (the `lbz cursor` then waits only for the x store).
  Left 4: the preheader order of the two hoisted FPR constants (target `lfs f30 (5.0)` before `lfs f29 (369.0)`; ours the reverse
  because rowY's set precedes the inner loop's preheader in the outer body) -- no placement of rowY gives both the pool order and
  the hoist order.
- **msq_R0_Sequence (97 -> 96): `int c = (u8) w->..x2; w->..x2 = step + c;`** (the pass-15 narrow `+` int-local form: `add r0,r11,r0`).
- **msq_R0_SeqResize (78, read, not closed):** the target never forwards `w->seq[0].num` through the preceding `sth`: loop 1 is `BFC:
  sth t; k--; lhz n; i--; blt; lhz k->frame; cmp; ble; t = n - 1; b BFC` with the first `t = n - 1` (`subi r0,r7,1`, NOT merged with
  `i = num - 1` although both are `(plus A -1)` in one cse path) and the giv init `mr r11,r10` before the label, and the exit
  `clrlwi r31,r7,16` truncates the int n to the u16 num; loop 2 likewise reloads n (`lhz r9; subi r0,r9,1; mr r7,r9`) and indexes
  `key[(u16)(n + 1) - 1]` via `clrlslwi r9,r0,16,2`. cse.c facts read for it: a plain `(set tmp:HI (mem:HI))` load IS forwarded
  through a same-address store (the mem's class holds the stored subreg -> `clrlwi`), while a single-insn `(set n (zero_extend (mem:HI)))`
  is not (any store removes every non-MEM `in_memory` element, and the bare `(mem:HI)` class does not contain the zero_extend);
  this compiler expands `int n = w->num` as the two-insn form (COMPONENT_REF into an HI temp), so every u16/int local, `MSQ->`
  re-read, `--i`/`num--`/while/for/for-step form tried forwards (74-79 words). The form that produces the single-insn load, or a
  label between the store and the reload at cse1 time, was not found.
- **t_event MainPreview (195 -> 0, zero code, six facts):** (1) `EvtHdrCopy* h = (EvtHdrCopy*) t->pEvd;` ONE source pointer for both
  48-byte copies (`t->hdr = *h; d->hdr = *h;`: the target keeps r8 across the first copy, ours re-read `t->pEvd` after its stores)
  and (2) `EvtDebugView* d = EVTDBG;` a POINTER local for the global destination (`addi r9,r9,EvtDebug@l; addi r9,r9,32` -- the
  member offset added to the pointer, not folded into the reloc; 100 -> 39 words: the block-copy loop's registers followed).
  (3) `SubToolCameraMove`/`SubToolLightMove` are NON-static members whose bodies use `this` and ignore the `ToolEvt* t` parameter:
  the caller's `t->SubToolLightMove(t)` passes t in r3 AND r4 (`mr r3,r31; mr r4,r31`) while the bodies stay byte-identical to the
  static form (this = r3 = the old t). Rule: a `mr r4,r31` duplicate of `this` at a call whose callee never reads r4 = a non-static
  member with an unused pointer parameter. (4) `FlagBit(t->flags, A) || FlagBit(t->flags, B)` inline for the two `||` pairs on one
  lvalue (`andis. 8192; bne; andis. 4096` instead of the folded `andis. 12288`; the `!(f & A) && !(f & B)` pair likewise). (5) The
  four `mode/step/x04/x06` stores in the order `mode = 0; step = 0; x04 = 0; x06 = 0;` (the ctor's order; x06 first gave x4 first
  after cross-jumping). (6) `EventMgr* m = &EvtMgr; u32* pp = &m->x34;` declared AFTER the `t->EvtTaskSuspend(0)` call in both the
  case-3 then-arm and case 4 (`lis r30; addi r30; addi r29,r30,52; mr r3,r30; mr r4,r29` shared by the two EvtSndStrStop calls;
  the plain `EvtMgr.EvtSndStrStop(&EvtMgr.x34, ..)` re-materialises the symbol address per call). (7) `u32* fp = &t->flags; *fp &=
  ~0x00400000;` and `u32* sp = &ev->status; *sp &= ~..; sp = &ev->status; *sp &= ~..;` for the stores the `lwz pG` must not be
  hoisted above: `*(u32*) &t->flags` is folded back to the COMPONENT_REF and `p[i]` (TE_FLG_OFF) is an ARRAY_REF -- both set
  MEM_IN_STRUCT_P and `fixed_scalar_and_varying_struct_p` lets the fixed-scalar pG load pass; only a pointer VARIABLE deref is a
  plain INDIRECT_REF MEM (the RsfFlagWord rule, re-read: the pointer must be a named local).
- t_event SubToolMessInit (212, 0x10a0/0x1074): the missing 11 words are the inlined `cDbgToolMain<T>` constructor's loops (`li r9,99;
  addi r28,r9,-1; li r29,10; loop: bl memset(p, 0, 16); ...; cmpwi r9,-1`) -- the shared header's ctor (the ToolEspArea/ToolLightAreaMain
  block of pass 16, owned by tools pass 17a), not this unit. t_id/t_id and t_movie/t_snd_vol were not started this pass.

### Tool RELs, t_esp pass 9 (t_esp 200 -> 201/212: ID_WINDOW ctor 157 -> 0 zero code; InitTool 11629 -> 9684 words with the frame now EXACT (0x2930) and .text 0x9ebc -> 0xa124 of 0xa20c; Load/SaveEmType 2/2 loop body reproduced, entry left; db_widget DB_STRING ctor 11 untouched; nothing flipped; 2026-09-11)

- Harness ~/.cache/tesp9 (deleted): `mk.sh MOD/UNIT SRC OUTDIR` = compile with the module cflags + strip_unused/fold_linkonce
  into `OUTDIR/<stem>.o` (the stem must be the unit's, ngccc.py derives the linkonce unit from it), `tryv.py MOD/UNIT FUNC v.py
  [--apply NAME]` (variants may carry a header copy: `{'src': [...], 'hdr': {...}}`), `dump.sh MOD/UNIT -dX`, `ins.py DUMP FUNC
  [re]` (one line per insn), `mini.sh FILE.cpp -dX` (standalone snippets against the real headers, the fastest way to read a
  frontend/expand shape), `olist.sh OBJ FUNC OUT` (objdump with the relocation symbols folded into the insn lines) +
  `segcmp.py T.s O.s [N] [norm]` (InitTool split at every `bl __builtin_new` = one segment per window ctor; per-segment line
  counts and skeleton diffs; N prints one segment's diff), `xform.py IN OUT A B` (the widget-block rewrite below). 111 OK
  before and after every edit; judged with tools/bytecmp.py.
- **Numeric/button widget blocks (ID_WINDOW ctor 157 -> 0, zero code; applied to all 223 blocks of the file): the block reads
  `pa`/`win` into locals BEFORE the `pos`/`sx` locals**: `{ DB_PRIM_ARRAY* pa_ = pa; DB_WINDOW* win_ = win; DB_POINT pos(x, y);
  int sx = N; pa_->CreateNumeric2(win_, ..., &pos, &sx, sy, flg); }`. Mechanism: the target issues `lwz r4,4(r30); addi r9,r1,N;
  lwz r3,0(r30); mr r7,r9; stfs pos.x; ...; stw sx` -- the two member loads first, the pos stores after them, `sx` last. The
  loads are first only when they are PSEUDO loads with an `mr r4,P` arg copy (prio call+3 = lwz 2 + mr 1) instead of direct
  `(set r4 (mem))` arg loads (prio call+2, and `addi &pos` with three dependents wins the tie). `expand_call` never
  precomputes them (`preserve_subexpressions_p` copies only MEMs with rtx_cost > 2, and `(mem r82)` costs 2); it is COMBINE that
  merges `P = mem; r4 = P` into the direct load, and combine refuses when a memory STORE lies between the load and the copy.
  So: the CreateString form `&DB_POINT(x, y)` (pass 8) works because the temp's stores sit between the loads and the copies
  in RTL; for the numerics the pos/sx locals must be initialised AFTER the pa/win reads -> locals for pa/win declared first
  (the pos stores keep their early LUID and are issued right after the loads, the way the target has them; the temp form
  `&DB_POINT(..)` puts them last: 165). `int sx` may also be written `&(sx = 0)` inside the argument list (146 on one block)
  but the locals form is the natural one. The rule in one line: when a call's argument loads are issued first in the
  target while ours issues the address/store chain first, the loads were pseudos -- put a memory store between the load
  and the call (declare the loaded values in locals before the stored ones).
- **InitTool frame 0x2C08 -> 0x2930 = target (zero code): the EDIT_WINDOW ctor must not carry a dead page body.** The
  ctor was `EDIT_WINDOW(p, no) { pa = p; win = NULL; if (no == 0) { ...page-1 widgets... } page = no; }` inlined four
  times: integrate.c allocates the inlined function's WHOLE frame (`DECL_FRAME_SIZE`, here 0x100) in the caller as soon as
  the copied body mentions the virtual frame pointer -- a `for` loop body's address-taken `pos`/`sx` locals are real slots
  at save-for-inline time (block-scoped locals outside a loop are `addressof` pseudos and cost nothing) -- and the dead
  `if (1 == 0)` body still gets the 0x100 block, three times (the 256-byte jumps in ours' pos-slot sequence at every
  EDIT2/3/4 window). The target also stores NO `page` member anywhere in InitTool (the object is 0xC bytes, `li r3,0xc`
  before `__builtin_new`, but offset 8 is never written). Form: `EDIT_WINDOW(DB_PRIM_ARRAY* p) { pa = p; win = NULL; }` plus
  `static inline void CreateEditWindow1..4(EDIT_WINDOW* e, DB_PRIM_ARRAY* p)` written in MEMBER style: `e->win =
  p->CreateNormalWindow(..)` (`mr r0,r3; stw r0,4(r29)`), `e->win->SetCloseCallback(..)` (forwarded, r3 direct),
  `e->win->SetActiveChangeCallback(..)` (reloaded after the call, `lwz r3,4(r29)`), and every widget block `pa_ = e->pa;
  win_ = e->win;` (the target reloads `lwz r3,0(r29)`/`lwz r4,4(r29)` per block; a local `DB_WINDOW* win` gives a register).
  After this the pos/sx slot sequence of all 566 widgets is identical to the target's (only store ORDER differs inside a
  few window blocks) and .text is 0xe4 short (57 insns: the target has 4 more `mr`, 18 fewer spill reloads).
- **InitTool residue read (segcmp): every window segment still differs by schedule/allocation, not structure.** The window
  block `{ DB_POINT pos(x, y); f32 w; f32 h; u32 flg; win = pa->CreateNormalWindow(..) }` is issued by the target as `lwz p =
  g_pPrimArray (first, before mr r30,r3); stw win = NULL; li flg; stw pa = p; lis str; stfs pos.x; addi; lwz &pos(spill);
  mr r3,p; lwz &flg; lwz &h; stfs pos.y; lwz &w; lwz &pos; stfs w; stfs h; stw flg` (member stores at the top, frame stores
  at the bottom) while ours has the member stores at the bottom and pos.x third; a `pa_ = pa` local in the window block
  moves the `lwz g_pPrimArray` first in a mini test but the real order depends on the spilled `&pos` reloads -- not
  closed. The entry block is ~850 `addi rX,r1,N; stw rX,0x20xx(r1)` spill pairs (846 target / 857 ours) interleaved with
  the flag stores; its order follows the spill-slot assignment of the whole function and cannot be read until the windows
  align. Applied anyway (pure C, the widget-block style): `DB_PRIM_ARRAY* pa_ = pa;` as the first statement of all 41
  window blocks -- cse folds it to the parameter (no RTL change in the block itself) but the 41 extra uids shift gcse's
  hash-table size (`max_uid/4|1`) and with it the PRE `lis` placements and spill slots: 11465 -> 9684 words, .text -4.
  Read this as "the original InitTool has MORE RTL than ours" (the spill-slot-rotation rule), not as a form.
- **Load/SaveEmTypeUpdateCallback (2/2, mechanism closed to the loop ENTRY).** Writing loop 2 as a rotated while
  (`while (a0 == (n = tbl[t = load])[0] && c1 == n[1]) { step; }` after a peeled first `step`) reproduces the loop body
  EXACTLY -- `lis r12` hoisted into the preheader for the load, `sth r9,0(r4)` through the outer high at the top: cse1
  follows the bottom `beq TOP` into the TOP block (preceded by a BARRIER, NUSES 1) knowing the TEST block's high, gcse's
  redundant-copy then sits in the load's block (kept: same-block copies are never cprop'd; `find_avail_set` is avin-based)
  and loop.c hoists it -- but the target enters the loop at TOP by fall-through with no `b TEST`. A peeled `step` before
  the loop is what a while form needs and jump2 would cross-jump it into TOP (the tails are register-identical up to the
  TOP label), except that cse1 KNOWS `dir == -1` there (`record_jump_equiv` of the `bne cr7` guard, whose compare is only
  hoisted by gcse later) and folds the peeled `t + dir` into `addi r0,r7,-1` (25 words). The do-while / for(;;) / goto /
  `while (dir == -1)` / `do {} while (a && b)` / duplicated-exit-test (`while (a0 == n[0] && c1 == n[1]) { step; load }`,
  whose dup test folds away) forms all give the pass-8 shape (2). What is still missing: a form where TOP is preceded by a
  BARRIER at cse1 time without a `dir`-known peel (or where the peel is computed in an ebb that does not know `dir`).
  Facts read: `duplicate_loop_exit_test` copies the exit code (< 20 insns, no calls/labels) with its `beq NEW` intact and a
  `b END; barrier` after it; cse's "keep on going past the label" (cse_basic_block 9028) needs the label's remaining uses
  to be exactly the jump it just made unconditional; scan_loop marks a loop "phony" (no hoisting at all) when the first
  insn after LOOP_BEG is a non-jump insn -- gcse's PRE of the `cmpwi cr7,r5,-1` guard before the loop-1 entry `b` is what
  makes loop 1 phony in both builds (its load's `lis r9` stays inside).
- Not iterated: ToolEspMain 38, EspToolMain 40, PartPasteSeqData 46, EditActiveChange 50, MakeSaveSeqData 58,
  PosActiveChange 62, AddSeq 197; db_widget DB_STRING ctor 11 (pass 8's arithmetic stands).

### Tool RELs, db_mod pass 2 (t_esp 59 -> 61/75, Tools 47 -> 49/63, 1850 -> 929 words in both, .text gap 0x2C -> 0x3C ours shorter; dbmod_light 81 -> 0 and dbmod_option 88 -> 0 zero code; dbmod_scale 64 -> 2, dbmodGetFilenames 222 -> 17 (size exact), dbmodDispModelName 209 -> 100 (size exact), dbmod_locate 475 -> 103; nothing flipped; 2026-09-11)

- Harness ~/.cache/dbmod2 (deleted): `mbuild.sh MOD SRC OUTDIR` (module cflags + strip_unused/fold_linkonce `--module`, output
  `OUTDIR/db_mod.o` as pass 1 requires), `mtryv.py FUNC v.py [--mod MOD] [--apply NAME]` (variants = substring edits of
  src/tools/db_mod.cpp, judged with `OBJ= tools/bytecmp.py MOD/db_mod`), `mrel.py MOD SYM [OBJ]` = side-by-side disassembly
  with FUNCTION-RELATIVE branch targets and reloc names on both sides (the target's REL14 fields are resolved from the
  `SYM+0x..` reloc line, ours from the `.text+0x..` one), `mdump.sh MOD -dX` (`-G0`, module defines). 111 OK before and after.
  Every fix below is zero-code (no tag, no asm); the unit is still not identical, both module flags stay False.
- **Highlight colour: the selected row's `eprintf` colour is 4 in every page function, not 8** (`rlwinm r5,r5,0,29,29` =
  bit 4 vs our `28,28` = 8: the `(i == sub) ? 8 : 0` labels of DispModelName/motion/locate/blend/scale/light/option/p_info
  and motion's hash-digit colour were all wrong by one bit; -1..-2 words each, read the rlwinm mask before anything else).
- **`x = 6; y = 4;` assigned right AFTER the title eprintf (before the display loop), `int x; int y;` uninitialised at the
  top: dbmod_light 81 -> 0, dbmod_option 88 -> 0, dbmod_scale 64 -> 2.** Pass 1's motion lever generalised: with the sets at
  the declarations gcse PREs `x * 8` out of the loop (`li r0,6; slwi r27,r0,3` in the preheader, `mr r3,r27` per use);
  set two statements later the target's `li r21,6; li r22,4` land after the title call and every use is `slwi r3,r21,3` /
  `mulli r4,r22,14`. Does NOT help blend (185 -> 291) or p_info (159 -> 173): their x/y are used before the title.
  scale's last 2 words: the preheader order `li r30,56 (giv init); li r24,5 (x-1)` vs ours `li 5; li 56` on the
  single-iteration `for (i = 0; i <= 0; i++)` -- declaration/assignment order variants do not move it.
- **dbmodDispModelName 209 -> 100 (0x630 -> 0x648, exact):**
  (1) the `^` cursor test is `k == hs + (digits[0] - digit - 1)`: fold's `associate` turns `hs - 1 + (a - b)` into `hs +
  ((a - b) - 1)` (split_tree of MINUS(hs,1) gives var hs / con -1, "VAR +- (ARG1 +- CON)"), but `hs + ((a - b) - 1)`
  splits arg1 (var a-b, con -1) and returns `PLUS(fold(hs + -1), a - b)` WITHOUT re-folding -- so `hs - 1` is a separate
  invariant, loop.c hoists it (`addi r22,r28,-1` in the k preheader) and the loop does `subf; add r0,r22,r0; cmpw`.
  That extra callee-saved pseudo (19 candidates for 18 regs) is what spills the `"%s"` high: the target's five per-use
  `lis rX,"%s"@ha` (all the same .LC, verified on the relocs) are reload's REG_EQUIV rematerialisation of the PRE'd
  high(LC) pseudo that lost global-alloc, not five expressions. Rule: N per-use `lis` of ONE .LC in a function that uses
  all of r14-r31 = the high was spilled; find the missing callee-saved value instead of splitting the string.
  (2) case 2's colour is the statement form `if (k == type) color = 0; else color = 7;` through the function's `color`
  (`li r30,7; bne; li r30,0`); the ternary as an argument gives `li r5,0; beq; li r5,7`. (3) the m_stat switch is
  written `case 3: color = 0; case 2: color = 2; case 1: color = 7;` -- with case 3 FIRST its body follows the compare
  tree, jump1's jump-around-jump makes `cmpwi 3; bne end; L3:` and jump2 cross-jumps the body into the else arm's
  `li r30,0`, leaving `bne end; b Lelse`; our 1,2,3 order gives `beq Lelse; b end`. (4) `name = dbmodSkipPath(...)` in
  the bin/tex loops through the SAME `char* name` used in the hash loop: a 3-set pseudo is global-alloc'd (`mr r29,r3;
  ... mr r8,r29`), the direct argument was tied to r8 (`mr r8,r3`), +8 bytes. Left: pure register naming (i r26/r24,
  k r31/r27, the "%6s" spill register r11 vs r9 = reload's round-robin after x's `lwz r9,8(r1)`).
- **dbmodGetFilenames 222 -> 17 (0x778 -> 0x78c, exact):**
  (1) the bin/tex name table is `char name[2][FILE_NUM][NAME_LEN]` (0x3A1; `name[0]` = bin, `name[1]` = tex; the same for
  `locName[2][..]` at 0x1C47) indexed `name[i][k]`: `(p + 929) + i*2048` is folded by `associate` into `p + (i*2048 +
  929)`, then `+ k*128` stays outside, so the RTL is `t = i*2048 + 929` (one insn chain, a giv of the outer biv reduced
  to `li r22,929; addi r22,r22,2048` with the `mr r27,r22` copy in the inner preheader) and `add p,t; add ,k*128`; our
  `binName[type * FILE_NUM + k]` made `k*128 + (i*2048 + 929)` one giv. (2) The outer counter is `i` and the inner `k` in
  ALL loops of the function (the two bin/tex loops, the FILE_NUM loop with its digit loop, the locate loops, the final
  copy loop): `i + 1` is then NOT PRE'd across the inner loops (the pl0f/R209Main rule) -- the mechanism is lcm's
  isolatedness: `isoin[K] = latein[K] | (isoout[K] - antloc[K])` ignores transparency, so the next loop's `i = 0` kill
  block passes the isolation of that loop's header (earliest + anticipated = latein) back to the previous latch, and
  `redundant = antloc - (latein | isoout)` keeps the increment in place (a biv again). With a private `type` counter the
  latch was the only occurrence, delayin stopped at the inner header, and `type + 1` was inserted before the inner
  loop (`addi r27,r29,1 .. mr r29,r27`, no biv, no giv). Rule: a function whose LAST outer loop has its increment
  PRE'd (locate) but earlier ones keep the biv = one counter variable shared by all the outer loops.
  (3) `pDbModState.p->motFileNum = i = 0;` (the store's zero IS the counter register `stb r26,320`). (4) `no =
  mottblUnitNum(unit[t], name[t]); q = mottblUnitPtr(unit[t], no);` as two statements with `s16 no` (the nested call
  precomputed the outer `unit[t]` load before the inner call and cse shared it; the target reloads it after the call).
  (5) the locate header walks `q`, the locate loop `p` outer / `q` inner (mirrors the first section); the two strchr
  results go through a `char* c` that is never live across a call (`mr. r3,r3`: global.c's hard-reg preference for a
  pseudo copied from r3 with no conflicting life). Left: {pNo,dir} vs {biv,pMotTbl-high} callee-saved permutation
  (all four `log2(7)*7/148-150` in allocno order; the target ranks the temporaries first, no source lever found among
  declaration orders/zero inits).
- **dbmod_locate 475 -> 103 (0x11fc -> 0x11cc, now 16 bytes SHORT):**
  (1) `Vec ab[2]` for the two axis-line Vecs (12-byte spacing, pass-1 drawOrientation rule; frame 256 -> 248).
  (2) **`pax = &ax` is assigned INSIDE the loop's `if (no == em->no)` block right after RotMatrix, not before the loop:**
  before the loop it is anticipated on every path, gcse PREs `(plus fp 24)` with the step-1/sub-2 arm's `&ax` and
  inserts `addi r26,r1,24` at 20 arm exits (80 bytes; the "#3" frame-address family); inside the conditional block it
  is not anticipated at the arms, loop.c hoists the single set to the preheader (the target's `addi r10,r1,24` there),
  and the pseudo -- no REG_EQUAL note on a `(set p (plus fp N))` insn, so no REG_EQUIV -- is spilled to a stack slot
  (`stw r10,148(r1)`, `lwz r3,148(r1)` at the three call uses) instead of being rematerialised. The column stores are
  `ax.x = m[0][0]` etc. directly (24/28/32(r1)), not through pax. (3) `Vec* pos = &em->pos;` as case 0's first
  statement (`addi r31,r24,8`; `pos->x` becomes `8(r24)` by find_best_addr, y/z stay `4/8(r31)`, `PSVECAdd(pos, &v,
  pos)`), stores written `z, y, x`. (4) case 4 is `if (pEm) { .. } else { parentParts = 0; }` (the zero-store arm
  after the body). (5) the row is a second counter `for (i = 0, y = 4; i <= 4; i++, y++)` with the literal `y * 14`
  at every use: the first eprintf computes it into the argument (`mulli r4,r30,14`) and gcse's PRE copy `mr r31,r4`
  serves all later uses; `int row = y * 14` gives `mulli r31; mr r4,r31` and the literal `(i + 4) * 14` recomputes
  `mulli` in the k loops (two-level expression, the copy is of `i + 4` only). Left (103): `li r11,6; slwi r3,r11,3`
  / `li r11,16` / `li r10,25` per-use x constants that ours folds (`x = 6` in the body is cse-folded, before the loop
  PRE'd -- open), register names, and the case-1 `deg` schedule.
- **position_usage 10 (mechanism confirmed, not closed): `int position_usage(..) { ..; return y; }` gives the target's
  `addi r31,r31,1 .. addi r4,r31,-1; mulli` for the last pair (8 words, +4 bytes for the `mr r3,r31`)** -- combine folds
  `y++; (y - 1) * 14` only when y dies at the use; the original had y live past the last eprintf without returning it in
  r3 (the callers pass one argument). `for (;;) {..; break;}`, `if (mode >= 0)` around the last call: 10 / 26.
- Not iterated: IKreport 10 (pass 7 mechanism), dbmod_motion 54, dbmod_trans 71 (target saves r22-r31, ours r24-r31; the
  case-0 transMode derivation `andi. 1; beq; andi. 16` differs), dbmod_blend 184, dbmod_p_info 158, dbModMotionMove 194.
  `create__t8cManager1Z3cEmi 1` / `loadModel 1` / `fn_*_1BF24 24` are layout-pairing artefacts that vanish when the
  .text size matches (they disappeared while DispModelName/GetFilenames were exact and locate still +32).

### Tool RELs, bytes-first pass 18a (t_movie/t_se_at Matching 20/20 -> flipped, t_movie.rel byte-identical, 111 OK; t_movie/snd_test 53 -> 70/77 with .rodata/.data byte-identical; t_esp_area/t_lightarea CreateEditWindow boundary reproduced by a dead test, not applied; 2026-09-11)

- Harness ~/.cache/tools_p18a (deleted at the end): tools_p17b copies with `mtryv.py MOD/UNIT FUNC V.py [--apply NAME] [--src ABS]
  [--all]` (cflags/post_build parsed from build.ninja -- the build edge's first line can be `prodg_cc $`-continued, and post_build
  uses `$python`; `grep -v warning` exits 1 when nothing matched, so chain the post_build with `; true &&`, not `&&`), env
  `INC=dir` for header-copy experiments, and `dump.sh MOD/UNIT -dX` reading the module defines/-G0/-fno-implement-inlines from
  build.ninja (`INC=dir` prepends an include dir there too).
- **seAtAreaEdit_DataInput (t_se_at, 17 -> 0, zero code, three pieces; the pass-17a `asm("" : "+r"(col))` tag is gone):**
  (1) `num = 6;` at the FUNCTION TOP, not at the loop. cse1's ebb from the first-half join label does not know it (the ebb from
  block 0 dies at the inputCursor clamp's 2-use label), so the name loop's rotated entry test `b TEST; ...; TEST: cmplw i,num;
  blt` survives to gcse; with a real exit edge before the body the `(zero_extend (subreg:QI col))` of the body is NOT
  anticipated at the loop entry (antin[P] = 0 -> earlyin[body] = 1 -> latein[body] = 1: the body is its own optimal point) and
  the mask stays `clrlwi r5,r29,24` in the body; gcse's cprop then puts the 6 into the compare (`num = 6` is the single
  reaching set) and cse2 folds `0 < 6` away (i = 0 is in its ebb), so no entry test remains. Rule: a loop-body extension the
  target keeps in the body = the loop's trip count came from a variable defined in another ebb (gcse cprop, not cse1, folds it).
  (2) cursor clamps through a pointer local with ONE store at the join (`SeAtWork* w = pW; int c = w->inputCursor; if (c >= 0) {
  if (c > 5) c = 5; } else c = 0; w->inputCursor = c;`, same for rndCursor/flagCursor): the store then shares the sched2 block
  with `col = 0` and the following `lis pW@ha` -- the store's anti-dependence on the pointer's r9 ranks it (8) above the `lis`
  (7), and `li col,0` (2) fills the second issue slot of the same cycle: `stb r0,34(r9); li r29,0; lis r9`. The two-store form
  (each arm stores) cross-jumps the stores behind a label, so `col = 0` starts the next block and sched2 puts the `lis` first
  (pass 17a's "birthing boost" reading was wrong: adjust_priority runs only for insns that become ready through a dependence).
  The same form moves case 5's `mr r29,r27` before its `stw` (the flagCursor clamp).
  (3) case 0's `v` in r11: `v` is ONE function-scope variable for all six value clamps (`n` per-arm as in pass 16). The shared
  pseudo conflicts with the kept `addi r9,r9,Joy@l` pointer of cases 1-4, so global-alloc skips r9 in case 0 too (with a per-case
  `v`, case 0's v takes the dying pCur's r9). Per-arm `n` keeps the tie (`cmpw r11,r0; li r11,0; ori`) in cases 1-4 and the
  `mr r0,r11` copy in case 0 / rnd_range.
- **snd_test disp_sequencer's `.4byte Snd_voice_work` pool word (.rodata 0x1000 -> 0x1004 and the 23 shifted .data pointers are
  ONE fix; 17 functions flipped to identical): `SND_VOICE_WORK* const voices = Snd_voice_work;` (a const local in the loop body)
  passed to `seq_note_count(int ch, SND_VOICE_WORK* const& work)`.** Mechanism: the const scalar is folded everywhere and never
  gets a DECL_RTL; the reference binding takes its address, and expr.c's ADDR_EXPR case with a CONSTANT_P operand calls
  `force_const_mem` -- a per-function POOL entry (output after the function's strings, hence after "%3d"), read back through the
  reference as `(mem (reg))` = `lis; addi; lwz r9,0(r11)` in the loop preheader. Rejected (17a + this pass): `static T* const`
  (folded), `static const` arrays/structs (folded, or emitted at the inline's definition = .rodata 0x610, too early), a
  reference bound directly to the array (`seq_note_count(ch, Snd_voice_work)`: a TARGET_EXPR temp, stack slot, folded),
  `&(T* const&) Snd_voice_work` (folded), `static T* const p; T* const* pp = &p; *pp` (the word appears but as a static at the
  inline's position, 125 words). disp_sequencer itself stays 64 (frame-spilled `addi rX,r28,K` argument pointers, `0x54+0x54`,
  `ch+1` -- unchanged). test_play_or_stop's `lhz r4,32(r31)` re-read after `Snd_test_get_str_name`: a `name` local for the
  inner call gives 21 (worse); not the form.
- **ToolEspArea / ToolLightAreaMain CreateEditWindow (438/393, read; mechanism reproduced, not applied).** The boundary sits
  INSIDE the inlined cDbgEditWindow ctor between `pCur = 0` and `pTop = 0` (t_event's SubToolMessInit shows the same split:
  `stw r0,0x264 (pCur); lis; lis; cmpwi r31,0; stw 0x27c (pSetWorkNo); stw 0x268 (pTop) ...`), i.e. the construct is in the
  header, and it is the same spot pass 16's `do {} while (0)` sched-region split emulates (and cDbgWindow::Init's second one
  in db_toolbase.h). A dead test there in a header copy (`if (pTop) i = 0;` / `if (pButton[0]) i = 0;`, INC= experiment)
  gives EXACTLY the target's control structure: `cmpwi r31,0` PRE'd to the end of the pTop..pSetWorkNo block, `mfcr r29` /
  `mtcrf 128,r29; bne` after the AddButton loop, the stores 764,744,748,752,756,760 interleaved with the loop's `lis`/`addi`/
  `li i,0`, jump2 deleting the load+compare+jump (438 -> 427). What is left is register naming: the zero pseudo now spans two
  blocks (cse1 skips the dead-test block, so both halves canonicalise to one zero), becomes a global allocno and lands in r8
  after local-alloc gives the `1` constant r0; the target has zero r0 / one r9 / rows-5 r29 (hoisted above strlen) / n-32 r11
  (after strlen) = a BLOCK-LOCAL zero, i.e. in the original the boundary was gone before local-alloc but present at gcse and
  at sched1 (loop notes give the sched barrier; the CFG boundary must fold by the post-cse2 jump pass). No cse1-unfoldable,
  cse2-foldable test exists in that ebb (all ctor operands are constants known to cse1; `w` (strlen) and this-memory are
  unknown to both), so the construct is still unidentified: candidates are a loop whose exit test gcse's cprop makes constant
  (the t_se_at `num` mechanism inside the ctor), which needs a value defined in another ebb -- none is reachable from a
  header ctor whose actuals are literals in t_esp_area/t_lightarea and variables in t_event. Do not retry: `if (this == 0)
  <dead>` (E's occurrence lands in the pre-boundary block: `stw 740; cmpwi | lis; lis`), `if (w == 0) <dead>` (w's r3 stays
  live: 431), dead loops `for (i = 0; i < num; i++) pButton[i] = 0` (num just stored: folded by cse1, no boundary; num stored
  before strlen: real loop, 533), -fcheck-new (a real `beq`).
- t_camera/t_camera_data (tcSetBesideOffset 27, tcDataExport 142) and t_lightarea fn_Tools_30410 (59) not iterated this pass.

### Tool RELs, bytes-first pass 18b (Tools/t_motseq 202 -> 117: msqDisp 4 -> 0, msq_R0_Sequence 96 -> 15; t_movie/t_snd_vol 1635 -> 940: edit_reverb_param 812 -> 200 (0x1018 -> 0x15d8 of 0x15e8), file_save 330 -> 276; t_id/t_id 1206 -> 1176: idEditPos 364 -> 334; t_event untouched; nothing flipped; 2026-09-11)

- Harness ~/.cache/tools_p18b (tools_p18a copies + dol25b order.py/prio.py, deleted at the end). `mtryv.py`/`dump.sh` build-edge regex
  fixed to `prodg_cc\s+(\S+)` (edges whose `$` continuation leaves several spaces before the source, t_movie/t_snd_vol). 111 OK
  before and after every edit.
- **msqDisp (4 -> 0, zero code): the preheader order of two loop.c-hoisted FPR constants is the RTL order of their SETS at
  loop time.** `move_movables` emits every hoist with `emit_insn_before (.., loop_start)` in movables-list order, and the list is
  the insn order of the loop body; an inner-loop constant hoisted in the inner pass sits at the inner LOOP_BEG, i.e. AFTER every
  outer-body statement before that inner loop. So `f32 rowY = 369.0f` (outer body, block-local) was hoisted before the inner
  loop's 5.0 whatever its placement. The target has 5.0 first because 5.0 was a plain VARIABLE set BEFORE the loop (`f32 rowStep
  = 5.0f;` instead of `const f32 rowStep`): its set precedes LOOP_BEG in RTL, every loop.c insertion lands after it, and its
  pool entry is still created at the declaration (pool order 5, 4, 24, 25, 15, 369 unchanged). Rule: an FPR constant loaded in
  the preheader BEFORE all loop.c hoists = a non-const variable initialised before the loop; hoisted ones follow body order.
- **msq_R0_Sequence's dead `cmpwi r30,0` (96 -> 15, zero code): the pass-8 third-test form.** `int t = cur2; if (cur != 0) t =
  cur; if (t != cur2) cur = t;` right after the inner if/else inside `if (cur != cur2)` -- `cur` is dead afterwards, so test 3's
  arm dies at flow1, flow2 removes test 3 and then `t`, jump2 deletes only test 1's branch after reload: `cmpwi cur,0` stays
  with no branch (the `dead = 1` arm of the old source was trivially dead and took the compare with it before flow2). Left 15:
  the flagDisp store block's r0/r9 alternation -- the eight `lbz 7(r11); rlwinm; stb` chains are serialised by the may-alias
  store->load dependence (cost 2), so consecutive chains conflict through local-alloc's birth-2/death+1 extension and alternate
  r0/r9; the target has 4262 and 4263 both in r0, i.e. one filler insn between `stb 4262` and `lbz 4263` in ITS sched1 output
  (t=16 in ours is idle). The only free insns are `lbz cursor`/`extsb`/`addi` (sub2 = cursor + 2) and `lis r28`; every
  placement/local/shared-temp form of the sub2 statement tried (13 variants, 15-70). Not closed.
- **msq_R0_SeqResize (78, read further, not closed).** (1) The 2-insn `(set tmp:HI (mem)); (set n (zero_extend tmp))` load form is
  NOT a C-level choice: the SN rs6000.md `zero_extendhisi2` define_expand takes `gpc_reg_operand`, so `emit_unop_insn` copies
  every MEM into a HI pseudo (the pass-17b "single-insn zero_extend(mem)" form does not exist before combine). (2) A promoted
  `u16 num = w->num` is the 3-insn form (`tmpHI; tmp2 = zext; num = tmp2` from store_expr's SUBREG_PROMOTED branch), an `int n =
  w->num` the 2-insn form straight into n; cse canonicalises `num - 1` to `tmp2 - 1`, so the target's `lhz r0; mr r7,r0; addic.
  r8,r0,-1` is a promoted u16 (r7) whose copy survived because tmp2 is live at a later use (`addi r0,r7,-1` uses r7 = the
  variable, i.e. the value was NOT known equal to tmp2 there: a different cse ebb). (3) The loop-1 shape `addi t; mr k; L: sth t;
  k--; lhz n; i--; blt; lhz frame; cmp; ble; addi t; b L` is gcse PRE of a loop-head `n - 1` (recomputed at the end of the
  preheader and the latch, head occurrence deleted) -- reproduced structurally by a goto loop with the store before the label
  and at the bottom (`w->num = n - 1; again: k--; n = w->num; i--; if (i < 0) goto out; if (k->frame <= max) goto out; w->num = n
  - 1; goto again;`, variants G/G3/H: 70-84) except that ours folds the preheader `n - 1` into `i` (`sth r10`) so jump2 cannot
  cross-jump the bottom `sth r0` into it. (4) The non-forwarded reload: a label between store and load is excluded (the loop
  label is at the `sth`), `volatile` reads reproduce the block shape but break msqSeqDelete/msqSeqAdd/msqFrameSizeCk (the field
  is not volatile), and cse records the store's `(subreg:HI t)` source normally (src_elt != 0), so the mechanism that keeps
  `lhz r7,0(r4)` after `sth r0,0(r4)` in one block is still unknown. Also observed: loop 2's `lhz r9; addi r0,r9,-1; mr r7,r9`
  is the same promoted-u16 copy surviving because the `f <= max` jump sits between the copy and tmp2's death (regmove's
  optimize_reg_copy_1 stops at a JUMP_INSN) -- our source order `num = w->num` before the while test gives it too.
- **t_snd_vol edit_reverb_param 812 -> 200, size 0x1018 -> 0x15d8 (target 0x15e8): four structural findings.**
  (1) The 0x5D0 gap was the step switch written ONCE with `fstep = big ? 0.1f : 0.01f`: the target has `if (Joy[0].on & 0x400)
  { switch (work->efxCur[sel]) {.. -= 0.1f / -= 10 ..} } else { switch (..) {.. -= 0.01f / -= 1 ..} }` per direction and per
  set = 8 switches (`fsubs` with +0.1/+0.01 constants for the `-` direction, `fadds` for `+`; the switch operand
  `work->efxCur[sel]` is re-read INSIDE each arm of the `& 0x400` test, not passed in). Written as macros `EFX_SW_DPL2/EFX_SW_ST(op,
  fs, is)` inside one `static inline efx_param_move(p, sel, dir)` whose constant `sel`/`dir` fold at inline time.
  (2) `SndVolWork::x29` is `s8` (every `lbz 41; extsb` index/compare in the target; combine_tbl_edit 115 -> 86, combine_tbl_disp
  104 -> 103 for free) and the `cur` local does not exist (`work->efxCur[work->x29]--` re-reads `work`, a struct-member pointer).
  (3) **An inlined function's constant-pool loads lose RTX_UNCHANGING_P** (integrate.c copies a MEM whose address is already
  `lo_sum` through the generic path and clears `/u` when `map->integrating`; only bare `(mem (symbol_ref LC))` is re-created via
  force_const_mem), so in an inlined clamp `lfs 0.1` true-depends on the preceding `stfs` (base unknown vs symbol) and the store
  cannot sink below the next field's loads; in the target it does. Clamps written in place (macros `EFX_CLAMP_COMMON/AUX`) keep
  `mem/u`. Rule: a pool load the target issues BEFORE a store that ours issues after = the code was not inlined from a function.
  (4) Clamp shapes: float `p->f = p->f < lo ? lo : p->f > hi ? hi : p->f;` (ternary: one store after the join, value in a temp,
  `fmr f12,f0` copy of the loaded value); int `int v = p->f; int e = (s16) v; int r; if (e >= 0) { r = v; if (e > 0x7F) r = 0x7F; }
  else r = 0; p->f = r;` -- the ternary result must be a REGISTER target that the condition does not mention (`e`, not `(s16)
  v`: `safe_from_p` refuses the target otherwise and a promoted `u16 v` goes through the SUBREG_PROMOTED branch with target 0 ->
  fresh temp + `mr`), `r = v` first so jump.c's else-hoist gives `mr r0,r11` before `cmpwi 127`, and the `>= 0` outer test lays
  the `li 0` arm last. Left 200: `work`/`p` in r11/r10 (ours r10/r11) through the move region, `lfs f0,8(r10)` before `lfs
  f13,0(r8)` in the `+` big-step arms, the two `y` chains (target `li r31,128` before the TprimDrawFrameFn call and `mr r4,r31;
  addi r0,r31,16; extsh r31,r0` per row: `y` is NOT constant-folded although ours folds it through gcse cprop even with `y =
  0x80` moved before the colour diamond -- the target's `y` set was not available at the first use; mechanism not found), and
  the stereo panel's `li r7,-1; stw` / `sth` order. `init 1` in the residue line is the `bl` into the next unit's linkonce copy
  resolved by offset (our .text is 0x10 shorter); it disappears with the size.
- **t_snd_vol file_save (330 -> 276): `SndRoomHdr* hdr = (SndRoomHdr*) work->fileBuf` declared inside `case 2:` (a function-top
  initialiser hoists `lwz work; addis; addi` above the six eprintfs).** Left: the target keeps one zero pseudo in r27 (`li
  r27,0` after the first eprintf) for the `sub/step/x6 = 0` stores of every arm, ours `li r0,0` per arm.
- **t_id idEditPos (364 -> 334): `case 1:` `case 2:` `case 5:` of the subCur switch each carry their own `if (joy->trg & 0x100) {
  subStep = 0; editStep++; }` body** (the target tree tests 2, 0, 1 as separate nodes; the grouped label gave a range). Left
  (.text still 0x28 short): the `*pos = d->vtx[k]` copies store the first word through `d` (`stw r10,280(r26)`) and the rest
  through `pos`, the `editStep--`/`++` tails cross-jump into different arms (#6 family), toolIdOption (0x594/0x574: the target
  saves r16..r31 with frame 88 = no locals; ours r17..r31 + an 8-byte spill slot), idEditColor 0x1C, toolIdInit 0xC.
- t_event/t_event not started: its .text gap (0x7320/0x72f0) is SubToolMessInit's shared `cDbgToolMain<T>` ctor loops (pass 18a)
  plus CallbackLoad 0xC.

### Tool RELs, db_mod pass 3 (t_esp 61 -> 62/75, Tools 49 -> 50/63, 929 -> 600 words in both, .text gap 0x3C -> 0x1C ours shorter; dbmod_trans 71 -> 0 zero code; dbmod_blend 184 -> 25, dbmod_p_info 158 -> 117 (0x62c -> 0x630 of 0x638), dbModMotionMove 194 -> 163, dbmod_motion 54 -> 27; nothing flipped; 2026-09-11)

- Harness ~/.cache/dbmod3 (deleted): `mbuild.sh MOD SRC OUTDIR` (module cflags from build.ninja + strip_unused/fold_linkonce
  `--module`, output `OUTDIR/db_mod.o`), `mtryv.py FUNC v.py [--mod MOD] [--apply NAME]` (substring variants, judged with
  `OBJ= tools/bytecmp.py MOD/db_mod FUNC`, 0.5 s per variant), `mrel.py MOD SYM [OBJ] [--all|--ours|--target]` (side-by-side
  disassembly with function-relative branch targets: the target's REL14/REL24 are resolved from the `SYM+0x..` reloc line, ours from
  the `.text+0x..` one -- a first version looped forever on the reloc lines, increment the index before every `continue`),
  `mdump.sh MOD -dX` (`-G0`, module defines, `SRC_OVERRIDE=`), `mcheck.sh` (both modules). 111 OK before and after; every change is
  pure C++ (no tag, no asm); `python3 tools/bytecmp.py` DIFF in both modules, both modules.py flags stay False.
- **dbmod_trans 71 -> 0 (zero code, three pieces):** (1) case 0 is `if (flags & 1) { if (flags & 0x10) transMode = 0; else
  transMode = 2; } else transMode = 1;` -- the `!(flags & 1)`-first spelling makes fold turn `(f & 1) == 0` into `(f ^ 1) & 1`
  (`xori; andi.`) and the `transMode = 0` store then reuses that zero instead of the switch register `step` (the target's `stb r9`
  stores the `lbz step` register cse knows to be 0 in case 0). (2) `y = 4;` right after the title eprintf (`int y;` at the top)
  and the literal `y * 14` at BOTH calls: the `mulli r31,r23,14` stays inside the loop (the target's `li r23,4` in the preheader
  and the per-iteration mult) -- with `int row = 4 * 14` or `y` set at the declaration the row folds to `li r4,56`. (3) the colour
  is the statement form `if (i == transMode) color = 0; else color = 7;` (jump.c hoists the else set: `li r5,7; bne; li r5,0`);
  the ternary as an argument gives `li 0; beq; li 7`. Same size, 10 callee-saved registers (r22-r31) reproduced.
- **dbmod_blend 184 -> 25 (size +4 left):** (1) `x = 6; cx = 5; nx = 16; ny = 6;` assigned right AFTER the title eprintf
  (`int x; int cx; int nx; int ny;` uninitialised at the top): pass 2's rule for the pages whose x/y are only used in the display
  loop -- the target's `li r19,6 / li r24,5 / li r28,16 / li r27,6` land in the preheader after the title and every use is
  `slwi r3,rX,3` / `mulli r4,r24,14` / `mulli r4,r27,14` (pass 2 measured 185 -> 291 for this because (2) and (3) were missing,
  not because x/y are used before the title -- they are not). (2) the sub switch has an EMPTY `case 0: break;` before `case 1:`
  (CLAMP(sub, 1, 2) makes it dead): the target's tree is `cmpwi 1; beq C1; ble END; cmpwi 2; beq C2; b END`, the `ble END` is
  the empty case below 1. (3) **MotionSetCore's second argument is `&em->pEm->mot` (the model's own MotionWork, pEm + 472), not
  `&em->mot[1]` (em + 472; the same offset by coincidence)** -- the target's `lwz r3,4(r30); addi r4,r3,472` computes it from the
  loaded pEm and stores em + 472 separately for `motBlend`; our `addi r30,r31,472` shared one pseudo for both. Left (25): case 0's
  eprintf passes the `color` register set at the switch head (`li r5,0` before the tree, no set in case 0) while ours folds it to
  a fresh `li r5,0` in case 0 (cse1's NOT_TAKEN re-walk / gcse cprop knows color == 0 there; `int color = 0` at the declaration,
  block-scoped colours and per-arm forms not tried), and the cx/pDbModState-high naming (r24/r23 vs r23/r22 = one callee-saved
  register more in the target).
- **dbmod_p_info 158 -> 117 (0x62c -> 0x630 of 0x638):** (1) case 0's classification is `if (info & 0x30) { type = 0; if (info &
  0x20) type = 1; if (info & 0x80) type = 2; }` (the 0x80 test INSIDE the 0x30 block, IKreport's structure) with `int info =
  mw->partsInfo[k] & 0xFF` (`lhzx; clrlwi`, not the `u8` local's `lbz`), and its loop counter is a THIRD variable `k` (target
  r6, caller-saved; the display loop's `i` is r29): a shared `i` gives r29 in both loops. Left there: the gcse recomputation copy
  `mr r9,r0` for the third test (IKreport's exact mechanism, pass 7). (2) the j-loop row is `(i + j + 1) * H`: a giv of biv j
  with add_val `i + 1` (`mr r29,r28` from the PRE'd `i + 1` copy, `addi r29,r29,1` in the latch, `mullw r6,r29,r4`); `(i + 1 + j)`
  is folded to `i + (j + 1)` and cse merges the `j + 1` with the increment (`addi r30,r11,1; add r6,r28,r30`, no giv). (3) `v = 0;
  x = 6;` (that order) after the title eprintf, BEFORE the `for`: the target's `li r26,0; li r15,6` sit between `li r29,0` (i)
  and the entry compare, i.e. straight-line statements (a loop movable would land after the `bge` exit test); `x = 6` at the
  declaration puts `li r17,6` in the prologue and lets cprop fold `x + 7`/`x + 13` early (hoisted `li r14,13`). Left (117):
  `x + 13 -> 19` still hoisted to the preheader in pass 2 (`li r14,19`; the target keeps `li r0,19` / `li r0,13` next to their
  `slwi` in the loop -- the life of the folded constant at loop pass 2 decides, ours is 4-5 after scan_loop's single-use
  replacement moves the `ashift` next to the `r3 =` arg move), the zero pseudo `li r25,0` + `mr r7,r25` for the 5th eprintf2
  argument of both j-loop calls (a variable holding 0 that cse cannot fold in the j-loop ebb; `int c = 0` in the else arm / at the
  j-body top and `color` tried: 126-131), the `mr r18,r27` label-pointer copy of the j == 0 arm, and the register names that follow
  (pinfoNum high r14/r15, x r15/r16).
- **dbModMotionMove 194 -> 163 (size now +4):** `DB_EM::xE28`, `xE29` and `motStat[FILE_NUM]` are `s8` (`lbz; extsb; cmpwi 1`,
  `addi; extsb; stb`, `lbzx; extsb; clrlwi 16` for the `(u16) motStat[xE29]` argument) -- DB_EM is defined in db_mod.cpp, no
  header change; every other function of the unit unchanged (62/75 stays). Left (163): the `order[]` init loop -- the target has
  TWO decrementing registers plus `bdnz` (`li r24,63; addi r9,r1,71; stb r24,0(r9); addi r9,-1; addi r24,-1`): the address giv
  `&order[i]` was strength-reduced from the frame pointer directly (init fp + 71) while `&order` (fp + 8) is still PRE'd into
  r23 for the later `lbzx`; ours keeps `stbx rI,rBase,rI` because the address is `(plus (reg fp+8) i)` (gcse replaced the
  frame address everywhere, giv benefit 0 -> "not worth while"). `s8`/`u8` counters, `(u8) i` index, `i - 1` / `i--` spellings,
  a separate `n` counter and a stepping pointer (119, unnatural) do not give it; the ORIGINAL's address must have been a
  three-operand `fp + i + 8` at loop time (an expression whose `fp + 8` sub-term gcse could not match). Also left: the swap loop's
  `i + 1` (the target computes `addi r7,r7,1` in the no-swap arm and `mr r7,r5` on the swap path = gcse inserted `i + 1` at the
  end of the ELSE block, not in the compare block; `continue`, `while`, `++j` forms unchanged), and the register naming after it.
- **dbmod_motion 54 -> 27:** the `^` cursor of the digit editor accumulates into `hs`: `hs = nlen - len; hs += digits[sub] - digit;
  eprintf((hs - 1 + 25) * 8, ...)` -- the target's `subf r30; add r30,r30,r0; addi r30,r30,24; slwi r3,r30,3` is one user
  variable (global pseudo, callee-saved) through the whole chain; the single expression `(hs - 1 + (d - digit) + 25) * 8`
  computes `add r3,r3,r27` into the argument register. Left (27): register naming (k r29/r31, hs r31/r29), `li r19,16` (nx) in
  the preheader where ours has the pDbModState high, and the `s8 no` at +0x168 in r8 vs our r4.
- **position_usage 10 (read, not applied):** `int position_usage(int mode) { ...; return y; }` reproduces the tail exactly
  (`addi r31,r31,1` above the Back call, `addi r4,r31,-1; mulli` for B) at the cost of a `mr r3,r31` the target does not have
  (8 words, size 0x2f4 vs 0x2f0); a static store, a dead `if (mode == 2)` call and a trailing dead loop give 9-19. The other 4
  words are the join block's `li r30,43` AFTER `mulli r4,r31,14` (the x = 43 is evaluated after the Reset row in the target)
  and the first `y++` above the Reset call. Not closed.
- **dbmod_scale 2 (read, not closed):** loop pass 1 moves our folded `x - 1 = 5` (regno 267, life 1, savings 1, insn_count 64)
  while the same-shaped `high("%f")`/`16` movables are "not desirable" in the same loop, so the target's `li r30,56 (giv init);
  li r24,5` order = `x - 1` moved in pass 2 (after the giv init) as in dbmod_light (124 insns, life 1 -> life 2 in pass 2). The
  criterion is `threshold * savings * lifetime >= insn_count` (loop.c 1861) with threshold = (1 + n_non_fixed_regs) ~ 53..63
  here; why 267 passes it (already_moved / forces are not printed) was not resolved; `i < 1`, x in the loop, a second counter,
  do-while, `(x - 1) << 3`, `8 * (x - 1)`, statement order, `f32 sc` local, `switch (i)`, a label pointer: 2-57, none 0.
- Not iterated: IKreport 10 (pass 7), dbmodGetFilenames 17 (pass 2 permutation), dbmodDispModelName 100, dbmod_locate 103.

### Tool RELs, t_esp pass 10 (t_esp 201 -> 208/212: ToolEspMain 38 -> 0, EspToolMain 40 -> 0, PartPasteSeqData 46 -> 0, EditActiveChange_callback 50 -> 0, MakeSaveSeqData 58 -> 0, PosActiveChange_callback 62 -> 0, AddSeq 197 -> 0, all zero code; Load/SaveEmType 2/2 and InitTool 9684 unchanged; db_widget DB_STRING ctor 11 untouched; nothing flipped; 2026-09-11)

- Harness ~/.cache/tesp10 (deleted): `mk.sh MOD/UNIT SRC OUTDIR` (module cflags parsed from build.ninja + fold_linkonce
  `--module`, output `OUTDIR/<unit>.o`), `mtryv.py MOD/UNIT FUNC v.py [--apply NAME]` (variants = exact-substring edits of the
  unit source, optional `{'src': [...], 'hdr': {...}}` header copies via `INC=`; judged with `OBJ= tools/bytecmp.py`),
  `mdump.sh MOD/UNIT -dX` (module defines/-G0 from build.ninja, `SRC_OVERRIDE=`), `msbs.sh MOD/UNIT SYM [ABS_OBJ]` (the OBJ
  path must be absolute: the script cds into the repo). 111 OK before and after every edit; judged with tools/bytecmp.py.
- **Body census for RELs: NOT AVAILABLE.** The module `.sym` lists `.text` functions only; every data symbol is a `lbl_<mod>_<sec>_<off>`
  label, so the target has no `name.N` local-static numbers to compare `var_labelno` against (ours: `key.1039`/`_.tmp_0.1040` =
  346 bodies parsed before EspToolTrans, `kindName.690` = 230 before the ID_WINDOW ctor; `nb.sh db_widget.h` = 74 bodies /
  7 labels from our header set). The DOL-unit census (sweep 25a) cannot be repeated for t_esp/db_widget; the "original has more
  RTL" signal for InitTool stays the pass-9 uid-count reading. Use the header-string groups of `.rodata` (identical already) as
  the only include-set evidence.
- **ToolEspMain (38 -> 0, zero code, four independent pieces):** (1) `sel` is a BLOCK-LOCAL pointer in each of the sp_* test
  blocks (`{ DB_ACTIVE_SELECT* s = &WIN_SEL(g_pEditActive); if (.. && s->selX == 0) sp_sphere(..); }`; the target's `addi
  r11,r11,148` / `addi r9,r11,148` are block-local qtys); only the PosRand/0x1A pair shares one `sel` across the call (r30).
  A function-scope `sel` assigned in five blocks is a multi-set global pseudo and takes r30 everywhere (the pass-8 form).
  (2) `sp_ctrl01_trans(g_pEditSeq)`: db_port.cpp defines `sp_ctrl01_trans(EspGenWork*)`; t_esp.cpp had redeclared it with no
  parameter, so the test block's `g_pEditSeq` load died at the `lbz` (r9) instead of living into the call as r3 (`lwz
  r3,0(r28); lbz r0,265(r3); ..; lbz r0,1(r3); ..; bl`). Rule: a test block whose pointer load sits in r3/r4 with no visible
  argument move = the following call takes that pointer; check the callee's real prototype before reading registers.
  (3) constant-store blocks are read back as "dying store first (sched1 weight -1), then source order (LUID)": entry `g_eventNo
  = 0; g_eventSNo = 0; g_work = 1;` gives the target's `stb SNo; addi r3; stw work; addi r4; stb eNo` (the zero's LAST use is
  eventSNo, `one` dies at work); exit `g_initDone = 0; g_lightTool = 0; g_modelLoad = 0;` gives `stw modelLoad; stw initDone;
  stw lightTool` (modelLoad = the zero's last use, its `lis` issued last). (4) The loop-invariant `lis` order at the end of bb 0
  (`lis r14 lightTool` before `lis r27 camMode`) is gcse's bitmap_index = FIRST-OCCURRENCE order of the expressions in the RTL
  scan: `g_lightTool = 0; g_exitReq = 0; g_camMode = 0;` puts high(lightTool) first (the store block itself is issued in the
  same order either way). Rule: PRE insertions at one block end are ordered by the first occurrence of each expression in
  source order, so a store-block permutation is a lever on the `lis` filler order between calls.
- **EspToolMain (40 -> 0, zero code):** (1) `DB_MOUSE mouse = *g_pMouse; DB_KEYBORD key = *g_pKey;` declared mid-block as
  COPY-INITIALISED locals: no default-ctor calls (the target has no `bl DB_MOUSE::DB_MOUSE`), the bitwise copy loops stay, and
  the frame layout (mouse 8, key 104) is unchanged. Rule: a class local with a user ctor whose ctor call is absent in the target
  = a copy-initialised declaration. (2) `memclr_asm(((SeqPtrView*) &g_pEditSeq2)->p, ..)`: the pointer load waits below the
  preceding 300-byte record copy (struct view = may alias the copy's stores; a fixed-scalar load is hoisted above them).
  (3) store orders by the dying-store rule: `g_initDone = 1; g_lightTool = 0; g_modelLoad = 0; g_fovy = 45.0f;` (target
  initDone, modelLoad, fovy, lightTool) and `g_dataChanged = 1; g_fileMenu = 3; g_motionCam = 1;` (target fileMenu, motionCam,
  dataChanged with the `lis` order dC, fM, mC).
- **PartPasteSeqData (46 -> 0, zero code): `union { struct { s8 x10C, x10D, x10E, x10F; }; s8 inter[4]; };` in TOOL_SEQ and
  `dst->inter[i] = src->inter[i]` in the 4-iteration copy loop.** `(&dst->x10C)[i]` is pointer arithmetic: loop.c strength-reduces
  both addresses into pointer bivs (`lbz r9,0(r6); addi r6,1; stb; addi r5,1`), while an ARRAY_REF member keeps `base + i`
  (`lbzx r9,r26,r7; stbx r9,r25,r7` with the two bases hoisted) like the neighbouring `path[i]`/`x124[i]`/`x128[i]`; the
  u16 `x110[i]` is a separate `i*2` giv (r6) in both. Anonymous structs inside a union compile in this cc1plus.
- **EditActiveChange_callback (50 -> 0, zero code):** (1) `if (sel->selX == 0) { if (w != g_pEditWin1->win) prevWin = 1; }
  else { sel->SetSelX(0); p = ..; }` (the target lays the flag arm out first, `bne` to the call arm; the `else if` form puts the
  calls first). (2) `(g_pSeqFlg[n] & 1) == 0` (the `!` form folds to `xori; andi.; beq`). (3) `static u32 g_editTop;`:
  `g_editTop + 5 <= SEQ_TBL_LAST` is `cmplwi` in the target (no other user of the variable changes). (4) `u32 ofs = n *
  sizeof(TOOL_SEQ); ((TOOL_SEQ*) ((u32) g_pEditTbl + ofs))->stat`: a REG offset keeps the table base first in the PLUS (`lbzx
  r0,r10,r9`), while `g_pEditTbl[n].stat` is expanded mult-first (`lbzx r0,r9,r10`; PartPasteSelectData's target IS mult-first,
  so the two functions were spelled differently). A `SeqTblView { TOOL_SEQ e[1]; }` view gives base-first too but shifts the
  register allocation of `n`/`dir` (19 words); the local offset does not.
- **MakeSaveSeqData (58 -> 0, zero code): `size += sizeof(TOOL_SEQ)` LAST in the inner body (after the copy and the count
  increment).** With it first, `head` (10 loop-weighted refs / 70 insns) outranks `size` (8 / 110) in global.c and takes r3 (its
  parameter preference); with the increment last `size` wins r3 and `head` moves to r5 (`mr r5,r3` at the top). Also: `TOOL_SEQ*
  t;` declared BEFORE `u32 i, j` (loop.c reduces `t + 300` ahead of `j + 1`: r31 / r4), `size = 0x30;` before `rec = head->rec`
  (`li r3,48; addi r10,r5,48`), `rec` assigned after the clearing loop, `&tbl[nSeq * i]`, and the count through the
  `SeqCountView` array member (`lhzx r9,r5,r8`, base first). Rule: when the return value's register is r3 but a pointer
  parameter was copied out of r3 at the top, the accumulator outranked the parameter in global.c -- move the accumulator's
  update to the end of the loop body (its live length shrinks, its priority rises).
- **PosActiveChange_callback (62 -> 0, zero code):** (1) `w->sel.SetActivePrimitive(p);` (the `this` address goes straight
  into r3 as a pseudo that dies at the copy -> `addi r3,r29,148`), `sel = &w->sel;` AFTER the pos block, and the arms call
  `w->sel.SetActiveDown()` etc. DIRECTLY (not `sel->`): each direct member call is a fresh `(plus w 148)` pseudo occurrence in
  its own cse ebb, gcse PREs them (redundant from the join's `sel = E`), inserts `R = E` at the END of the join block ("also in
  blocks that already compute it"), cse2 turns it into `R = sel` and `sel` dies there: `addi r9,w,148; lwz r0,24(r9); mr r29,r9`
  with r29 = R used by every later `mr r3,r29`. With `sel->` everywhere the join's occurrence is isolated (no later occurrence)
  and no copy exists (`addi r29,r29,148`, 46 words). Rule: `addi rT,rB,N; lwz ..(rT); mr rG,rT` at a join = a pointer local
  whose LATER uses were spelled as fresh member-address expressions (PRE copies), not through the local. (2) The pos stores
  through the struct view `((SeqPtrView*) &g_pEditSeq)->p->pos.x = 256.0f` etc.: the target reloads g_pEditSeq after each store
  (`stfs f0,12(r4); lwz r9; stfs f13,16(r9); lwz r11; stfs f12,20(r11)`) while the else arm's three addresses share the first
  load -- a plain `TOOL_SEQ* seq` local shares r4 for the stores too.
- **AddSeq (197 -> 0, zero code): the saturating colour macro reads the flag once (`u8 f = g_immFlg[no]`) and tests it in
  EVERY arm of the clamp chain, with the imm/add choice as a nested if in the final else and ONE `no++`:**
  `f32 v; if (f == 0) v = ..; if (f == 0 && v > 255.0f) tbl->f = 255; else if (f == 0 && v < 0.0f) tbl->f = 0; else { if (f)
  tbl->f = imm->f; else tbl->f = tbl->f + delta->f; no++; }`. Mechanism: jump1's thread_jumps threads the first `f != 0`
  branch through the two `f == 0` re-tests straight to the imm store (`bne Limm`), cse1 folds the fall-through tests (f == 0
  known; the 0 arm stores the flag register, `stb r6`), the final else's `beq Ladd` folds to a fall-through, and jump2
  cross-jumps the imm arm's `stb` into the add arm's (`Limm: lbz; b Lst; Ladd: add; Lst: stb; addi`) -- the `no++` sits alone
  in the Lend block (a label at sched2 time), so the tail is `stb; addi`. With `no++` inside each arm (the old form) sched2
  hoists the `addi` into the store's load shadow in both arms (`addi; stb`), the cross-jump then also swallows the 255 arm's
  `stb` (the tail ends in `stb`), and every branch offset of the four colour blocks shifts (197). Rule: a cross-jumped tail
  `stb; addi` where our sched2 gives `addi; stb` = the increment was in a block of its own (a shared statement after the
  if/else), i.e. the arms were structured with a common continuation, not duplicated.
- **Load/SaveEmTypeUpdateCallback (2/2, unchanged; the mechanism sharpened one step).** cse1 can never know the outer
  `high(g_modelType)` at the loop-2 TOP block: `cse_end_of_basic_block` stops at EVERY code label, and follows a conditional
  jump into a 1-use label only when the label is preceded by a BARRIER (cse.c 8602-8640) -- the target's TOP is entered by
  fall-through from the `lbz r0,1(r8); lis r12; extsb r10,r0` preheader. Our -dG dump: TOP's `H = high` (insn 232) is the
  only in-loop occurrence PRE finds ("PRE: redundant insn 232 in bb 25, reaching reg 316"); it becomes the same-block copy
  `H = 316` (never cprop'd: `find_avail_set` is avin-based) with a `REG_EQUAL (high)` note, loop.c hoists the copy to the
  preheader and cse2/reload rematerialise it as `lis r12` (HIGH cost 0 < REG 1) -- that is our fresh r12 at the TOP store. The
  `lhz r9` (bb 27) and `lhz t` (bb 29) blocks are NOT occurrences at gcse time: they already use the outer pseudo 316/96, i.e.
  cse1 folded them (their labels are 1-use and the `beq`/`ble` into them skip label-free blocks = the `-fcse-skip-blocks`
  AROUND path from TOP's ebb, which knew the outer high through ... the preheader? open). In the target the roles are swapped:
  TOP folded, the last `lhz t` block a PRE copy rematerialised as `lis r12`. So the lever is which of TOP / the last block
  cse1 reaches with the outer high on its path (label uses, barriers, skip-blocks), and the do-while / rotated-while / peeled
  forms of pass 9 all make TOP the fresh one. Not closed.
- **InitTool (9684, unchanged; segment census):** with `bl` as the segment separator, 675 of 806 segments differ, but every
  one of the 40 inspected differs only by (a) the spill-slot offsets of the `&pos` reloads (`lwz r0,8624(r1)` vs 8688: our slot
  numbering is 16 slots behind by the 5th widget and the difference varies along the function -- reload assigns slots in
  pseudo-number order, so the set/order of spilled `&pos` pseudos differs), (b) the callee-saved register of the window
  object (`e` r28/r30 vs r30/r29) and of the `pa_`/`win_` reloads, (c) reload order inside a block. No segment differs in
  instruction set or count except the entry spill block (the target's zero `li r29,0` for the `sx = 0` stores is callee-saved
  in both). The pass-9 reading stands: schedule/allocation only; the structural work (the 566 widget blocks, the four
  CreateEditWindow helpers, the frame) is done.
- Not iterated: db_widget DB_STRING ctor 11 (pass 8's local-alloc arithmetic stands: LC/vt and zero/type qty order), db_mod
  (owned by the db_mod pass).

### Tool RELs, bytes-first pass 19a (t_movie/snd_test 70 -> 73/77, 322 -> 206 words: Snd_test_disp_voice 28 -> 0, test_blk_enable_ck 31 -> 0, dir_entry_read 37 -> 0 zero code; snd_test_disp_rit 37 -> 28, disp_sit_normal 123 -> 112 structural; t_esp_area/t_lightarea/t_camera_data not moved; nothing flipped; 2026-09-11)

- Harness ~/.cache/tools_p19a (deleted at the end): dol26a scripts + `mtryv.py MOD/UNIT FUNC V.py [--apply N] [--src ABS] [--all]`
  (build.ninja edge parsed after joining `$\n`, judged with `OBJ=... bytecmp.py MOD/UNIT FUNC`; env `XFLAGS=` prepends cflags such as
  `-fno-sched-interblock` for diagnosis, `INC=`), `mdump.sh MOD/UNIT -dX` (module defines/-G0 read from build.ninja, `SRC_OVERRIDE=`),
  `msbs.sh`, `prio_all.py` (prio.py for every hard reg). 111 OK before and after every edit.
- **Snd_test_disp_voice (28 -> 0, zero code): four facts about a `for (i) for (j) { if (...) eprintf(a) else eprintf(b) }` grid.**
  (1) Ours had interblock scheduling (arm 1's arg moves hoisted above the `beq`, `/b3` in the -dS ready list) because the inner
  loop formed a 4-block region; in the target the region did not form. The region disappeared by itself once the loop body had
  the target's RTL (below) -- do not chase find_rgns: `is_cfg_nonregular` (REG_LABEL notes = jump tables / `&&label`), the
  leaf rule (a block whose only successor is EXIT, incl. a lone trailing CODE_LABEL) and `too_large` (10 blocks / 100 LUIDs,
  LUIDs count NOTEs inside blocks) are the only killers, and none of them was the cause here.
  (2) `int n = i * 8 + j;` BEFORE the if (the target's `add r8,r27,r28` in the loop header; an expression in both arms stays in
  the arms because `j` is set in the latch, so the block-based PRE has no back-edge availability for it), (3) `int y = 0x142 +
  j * 0xE;` INSIDE the inner body (ONE occurrence -> loop.c reduces it: init `li r29,322` emitted by strength_reduce AFTER
  gcse's preheader insertions and the latch step `addi r29,14`; a `y` variable set before the loop has the wrong LUID and
  loses the r28/r29 allocation order to `j`; two occurrences (per arm) give two givs), (4) `Snd_voice_work[n].status` instead
  of a `vw++` pointer: the address is a DEST_ADDR giv whose increment loop.c places right AFTER the load (`auto_inc_opt`,
  rs6000 has HAVE_PRE_INCREMENT: `lhz r0,0(r31); addi r31,r31,32` in the header, not at the latch) and whose init
  (`slwi r0,r11,8; add r31,r0,r24`, combine merges `i<<3` PRE'd reg with the giv's `<<5`) is emitted by loop.c after the gcse
  insertions -- so `i` dies THERE and the PRE'd `addi r26,r11,1` keeps INSN_REG_WEIGHT +1 (rank_for_schedule prefers weight 0
  = an insn where an operand dies) and issues by LUID after `li j,0` and `slwi i8`. Rule: when the target's preheader issues a
  gcse-PRE'd insn late although it is independent, an operand of that insn dies LATER in the target's block -- look for a
  loop.c-emitted reader (giv init, hoisted invariant), which always follows the gcse insertions in RTL order.
- **test_blk_enable_ck (31 -> 0, zero code): `if (tbl == 0) { if (A[no].num == 0) continue; } else { if (B[no].num == 0)
  continue; } store; return 0;`** instead of `num = A/B; if (num)`. jump2 cross-jumps the identical `cmpwi r9,0; beq` tails
  (`lwzx r9; b L; ...; lwzx r9; L: cmpwi`), and each `num` is a BLOCK-LOCAL qty that local-alloc puts in r9 next to the dying
  r0 index temp (birth-2/death+1 fake lifetime); the one shared `num` was a global allocno that took r0 (global.c processes
  REG_DEAD before mark_reg_store, so the dying r0 temp does not conflict), which shifted tbl/w/max/i/max-1/iss by one register
  each. Rule: a shared compare on a value loaded in two arms where the target's registers look "shifted by one" = the compare
  was written per arm.
- **dir_entry_read (37 -> 0, zero code): `for (;;) { if (w->dirNum > 0x7F) return; if (DVDReadDir(..) == 0) break; ... }
  DVDCloseDir(..);`** -- the original returns WITHOUT closing when the list is full (`bgt` to the epilogue). Read off the
  loop dump: with `while (A && B())` our gcse PRE'd `&w->dir` (used by OpenDir, ReadDir and CloseDir) into the LOOP_BEG block
  (before the entry jump: `insert_insn_end_bb` puts it before a block-ending jump), and loop.c then reports "Loop ... is phony"
  (scan_loop: the first non-note after LOOP_BEG must be the entry JUMP or a label) and hoists NOTHING -- the target's
  `cmpwi cr4,r30,0` (the invariant `dirs` compare kept in a callee-saved CR field with `mfcr/mtcrf`), `lis/addi blk_ext_name`,
  `addi r29,r31,668`, `addi r30,r31,1180` are loop.c move_movables hoists, not PRE. With the return the exit edge kills
  anticipatability at LOOP_BEG and the post-loop `&w->dir` is PRE'd into the CALL block instead (`addi r0,r31,120` after
  `bl DVDReadDir`, `mr r3,r0`). Rule: a loop whose invariants the target hoists but ours recomputes in the body, with a
  gcse-inserted insn sitting between LOOP_BEG and the entry jump in the -dL dump ("phony") = an expression PRE'd along the
  loop's exit path that the original did not have on that path (different exit destination / return).
- **Loop-invariant compares hoisted with `mfcr rN` / `mtcrf 128,rN` around a loop are loop.c movables when the compare is inside
  the loop** (dir_entry_read `cmpwi cr4,r30,0`). The t_esp_area/t_lightarea `cmpwi r31,0` is after the AddButton loop, so that
  one is still the PRE placement question of passes 16-18a; not moved this pass.
- **Pointer advanced in place vs computed: `rit = blk->rit; rit += w->reqCur;` (snd_test_disp_rit 37 -> 28) and `rgn = (WTREGION*)
  (wt + hdr->rgn_ofs); rgn += inst->keyRegion[..]; art += rgn->articulationIndex;` (disp_sit_normal 123 -> 112).** When the
  final pointer is the SAME variable as its base, cse's find_best_addr cannot rewrite the first `rit->` load's address into
  `(plus base off)` (the base register was overwritten, exp_equiv_p fails), the load keeps `mem(rit)`, and combine forms the
  update load `lhzux r0,r30,r11` (movhi_update, "0" constraint = same reg) leaving the sign extension as a separate `extsh`
  (no lhaux pattern); with `rit = &blk->rit[cur]` cse prefers the more complex equal-cost address (`(p->cost+1)>>1 >
  best_rtx_cost` picks the PLUS over the REG) and combine gives `lhax`. For the three wavetable pointers the in-place form is
  also what puts the bases (`add r26,r10,r6`, `add r24,r10,r9`) before the index adds and keeps the base register for the
  result (`add r26,r26,r0`).
- **Read, not closed (snd_test):** disp_sit_normal 112: the target keeps the gcse PRE copies `mr r21,r29` / `mr r22,r30` of the
  FREE4/FREE5 row y (`y+0x46`, `y+0x54`, shared with the VOL(DLS)/VOL(SYN) rows) and reuses r29/r30 for dlsVol/synVol; ours
  merges the two occurrences in cse1 (the .cse dump has one `(plus y 70)` for columns 2+3) so no copy exists -- the target's
  cse path from the LINK join did not reach the third column (PATHLENGTH 10 / skip_blocks through the six on_off_name
  ternaries and the two `< 0` diamonds is exactly at the limit in ours); the block that lengthens the original's path was not
  found (a `t = y; if (t != y)` dead test changed nothing). snd_test_disp_rit 28 / disp_sequencer 64: the `0x54 + 0x54` y is
  `li rX,84; addi rY,rX,84` in the target (a REG_EQUIV constant pseudo re-materialised right before the add, #13 family; in
  disp_rit it sits after the STR_TYPE diamond so cse1 could not fold it, in disp_sequencer it is loop.c-hoisted into the
  preheader) -- no C form gives an unfoldable, unallocated 84; plus block-0 local ties (r10/r8/r11/r9). test_play_or_stop 2:
  the target re-reads `w->reqCur` (`lhz r4`) after the nested `Snd_test_get_str_name(blk)` call and has a DEAD `lhz r4` before
  it; `precompute_register_parameters` converts the u16 MEM into a pseudo before the nested call in ours (`lhz r30; mr r4,r30`);
  RTX_UNCHANGING_P is only set for TREE_STATIC readonly objects (expr.c), so a `const` view is not the lever.
- Tools/t_esp_area, Tools/t_lightarea, t_camera/t_camera_data: unchanged (438/393+59, 27+142).

### Tool RELs, t_snd_vol/t_motseq closer (Tools/t_motseq Matching 21/21 -> flipped, Tools.rel byte-identical, 111 OK; t_movie/t_snd_vol 940 -> 357 words: combine_tbl_edit 86 -> 0, file_save 276 -> 37, file_load 122 -> 9, data_edit 152 -> 8; combine_tbl_disp 103 and edit_reverb_param 200 untouched; 2026-09-11)

- Harness ~/.cache/tools_sv (tools_p19a copies: `mtryv.py MOD/UNIT SYM V.py [--apply]`, `mdump.sh MOD/UNIT -dX`, `msbs.sh`; plus
  `fn.py DUMP 'void f()' [regex]` = one function's section of a multi-function RTL dump), deleted at the end. Every edit
  built under the ninja lock; `ninja -k 0` + shasum = 111 OK before and after the flip.
- **msq_R0_SeqResize (78 -> 0, zero code) -- how 2.95.3 PRE places a re-loaded field, read off lcm.c.** The target's `mr
  r7,r0` / `lhz r7,0(r4)` after each `sth` / `clrlwi r31,r7,16` is the gcse reaching register (HImode!) of the load
  `(mem:HI w)`: `num` is NOT a local (a promoted u16/int local is SImode and `clrlwi` never appears; a HImode multi-set
  pseudo can only be gcse's). Every read is the field `w->seq[0].num`, `w->seq[0].num--` gives the paradoxical-subreg
  `(plus (subreg:SI R:HI) -1)` (HI arithmetic via convert.c narrowing + widen_operand), which cse cannot fold into
  `i = zext(tmp) - 1` (the `n - 1`-folds-into-`i` blocker of pass 18b was the promoted local). PRE rules that decide the
  shape (lcm.c block-based: latein = delayin for every block but the last; optimal = latein - isoout; redundant = antloc -
  (latein | isoout); insertion at the END of an optimal block, before its jump): a load at the head of a block that also
  stores the field is "isolated" (both successors are latest) and is never deleted -- so the store must be the LAST insn of
  the preheader and of the loop body and the loop head block must be store- and load-free: `w->seq[0].num--; for (;;) {
  i--; if (i < 0) goto done; k = &w->seq[0].key[i]; if (k->frame <= max) goto done; w->seq[0].num--; }` -- `goto` exits
  (a `break` within 30 insns makes stmt.c rotate the loop and puts the label before the store), `for(;;)` (loop notes: the
  `k` giv init `mr r11,r10` comes from loop.c; a goto loop has none). PRE then inserts `R = mem` at the end of the head
  block (the target's `lhz r7` right after the `sth`), cse2 turns the bottom `num--` load into `R` and the two `sth` tails
  cross-jump (the label lands on the `sth`). Pass 18b's "cse does not forward the store" mystery was this insertion order.
- **SeqResize, the rest (all zero code):** (1) `k = &w->seq[0].key[i]` must be computed AFTER the `i >= 0` test (a
  different block): in the same block combine folds `(ashift i 2) + 4` with `i = tmp - 1` into `tmp << 2` through a
  split PARALLEL; the ADDR_EXPR form gives `w + (i*4 + 4)` (`slwi; addi 4; lhzx r0,r4,r9`) while the direct
  `key[i].frame` gives `(w+4) + i*4` (expr.c's BLKmode/alignment special block). (2) `k`'s bb-3 set survives to cse2 only if
  the pseudo has other uses: `k` is a function-scope pointer also assigned in loop 2 and the `num == 0` block, otherwise
  cse1's find_best_addr rewrites `(mem k)` to `(mem (plus w 120))` and delete_trivially_dead_insns removes `k` before
  loop.c's giv init can be cse'd to `mr r11,r10`. (3) loop 2 entry: `k = &key[num - 1]; f = k->frame;` BEFORE `if (step >
  0)`, `f += step` inside (the `lhzx r8` is issued before the `subf.`). (4) loop 2 as a goto loop with a `u8 z = 0` in the
  then-block: with loop notes the two byte-store zero has REG_N_REFS 5 (loop depth 2) and beats `max` (7 refs / 54 insns)
  in global.c's priority (floor_log2(refs)*refs/len), taking r6; without notes it has 3 refs and follows `max` (r5/r6 as
  the target). (5) `max = m->mot.maxFrame; max <<= 6;` as two statements ties the fix-conversion load to max's register
  (`lwz r6; slwi r6,r6,6`). Also: `sth r31,4(r4)` for `key[0].frame = 0` is cse's jump equivalence (r31 == 0 after the
  `bne`), free.
- **msq_R0_Sequence 15 -> 0, tagged `#13 (free sched slot filler)`:** `asm("" : "=m"(fd[2]) : "m"(fd[2]))` between the
  flagDisp[2] and flagDisp[3] chains. The `"m"` input makes it a true dependent of `stb flagDisp[2]` (ready one cycle after
  it), the `"=m"` output a store the next `lbz 7(r11)` must follow, so it is the one insn between `stb 4262` and `lbz 4263`
  in sched1's order and local-alloc's birth-2/death+1 rule no longer makes the chains conflict (r0/r0 like the target).
  Variants: `"=m"(fd[2])` alone lands too early (37), `"=m"(fd[3])` 3. In ours `lis r28` fills the t=2 slot and nothing is
  ready at t=16; what the original had there is unknown (13 sub2 placements were tried in pass 18b).
- **t_snd_vol combine_tbl_edit 86 -> 0 (zero code):** no `s8* p`: every case is `sel->vol[work->x29]--; sel->vol[work->x29]
  = sel->vol[work->x29] < -1 ? -1 : sel->vol[work->x29] > 31 ? 31 : sel->vol[work->x29];` (per-field tails, so jump2
  cross-jumps only the identical `--`/`++` clamp tails of the same field, and the store is `stb 2(r10)` off `sel + x29`),
  and the final efxCur clamp written on the field (`stbx`), not through a pointer.
- **file_save 276 -> 37:** (1) memcpy sizes through a variable (`n = sizeof(SndRoomHdr); memcpy(p, hdr, n); p += n; size
  = n;`, `n = sizeof(CombSel)` in the sel loop): SN's movstrsi inlines every constant size (a 24-byte/iteration loop for
  0x240, `lwz/stw` pairs for 8), the target's `bl memcpy` + `crclr` is the libcall for a non-constant length; `li r5,8`
  is cse afterwards. (2) `int size = 0;` at the top: the pseudo is live through the whole function (r27), and cse uses it
  as the zero of every `sub/step/x6 = 0` store on the paths it reaches (`sth r27`) -- the one-shared-zero of pass 18b. (3)
  `p = work->fileBuf; hdr = (SndRoomHdr*) p;` in that order (all section pointers `addi rX,r28,N` come off p). (4) The
  offset and copy loops go through block-local pointers `CombSel* s = &work->sel[i]` / `EditTbl* t = &work->vol[i]`
  (`lwz work; add r4,r0,r10; lbz 6(r4)`; the second `t->num` read after the `hdr->vol_ofs[i]` store reuses the pointer
  instead of reloading `work`; `t` is the memcpy source register). Left 37 = register order only (p r29/r28, the
  hoisted `hdr+0x140/0x1c0/0x240` pointers r4,r3,r31 vs r3,r31,r30, the loop counter r28/r29, the sel-loop `s` r9 vs r4:
  the target has r9 occupied across the offset loops).
- **file_load 122 -> 9:** the first loadCur switch also has `case 0: work->dest ^= 1;` (its `cmpwi r0,0; beq; b end`
  cross-jumps into the second switch's dispatch = the target's `ble +0x180`), the `yesno` branch of case 2 carries the full
  `sub/step/x6` tail in both arms (jump2 merges them with the `& 0x200` arm), the three colour arguments are NESTED
  ternaries `work->sub == 1 ? (work->loadCur == k ? 6 : 0) : 0` (inner `temp = 0; if (b) temp = 6` + outer else `temp = 0`
  = the target's two `li r5,0`), and `work->room = work->room < 0 ? 0 : work->room` (the `beq` skips only the `li`). Left
  9: the target's `clrlwi r5,r5,24` for the `(u8)` cast of the first colour -- ours drops it (combine's nonzero_bits of the
  constant-set temp); a `u8` local, u8-typed arms and `(u8)` on the whole expression all fold it. Not found.
- **data_edit 152 -> 8 (zero code):** `step = 1` AFTER the two eprintfs (r6, not a callee-saved: frame 0x30/stmw r27), `TblEnt*
  ne = &tbl->e[tbl->num]` for the new-point block (`ne[-1].dist`, `ne->val`, `stfsx f0,r31,r11`), an explicit `case 100000:
  tbl->scale = 1000.0f; break;` beside the identical default (the dead `cmpw r9,0x186a0` survives), and every clamp as a
  ternary (`e->val = e->val < lo ? lo : e->val > hi ? hi : e->val;`, the four dist clamps likewise). Left 8: local-alloc
  gives the editMode temp r8/ne r10 where the target has r7/r8 (the num temp's death and ne's birth are within the
  birth-2/death+1 window in the target's sched1 order).
- **combine_tbl_disp 103 (analysed, not applied):** the target's `li r24,0x113; li r25,0x181` / `extsh r3,r24` (ListDraw's
  s16 x) and `mr r3,r18; mr r4,r21` (eprintf) are `int` variables holding the x/y constants set right before each loop
  (loop.c-hoisted from the body or declared per branch), 0xA5 stays literal; the y of loop 1 is `int yb = 0x6A + i*0x48`
  (giv init 0x6a) plus `(i/2)*0x14` in a second statement. Those forms reproduce the visible insns but leave 103 (frame
  address `mr r15,r22` giv copies for `col[]`/`c[]` = the #3 family, `lis pG@ha` kept inside the i loop, cursorCol
  address register). edit_reverb_param 200 not started this pass.

### Tool RELs, db_mod pass 4 (t_esp 62 -> 67/75, Tools 50 -> 55/63, 600 -> 409 words in both, .text 0x1C short -> 0xC short; dbmod_scale 2 -> 0 (tagged), position_usage 10 -> 0, IKreport 10 -> 0, dbmodGetFilenames 17 -> 0, dbmod_blend 25 -> 0 (tagged); dbmod_motion 27 -> 11, dbModMotionMove 163 -> 98, dbmod_p_info 117 -> 71; DispModelName 100 / locate 103 untouched; nothing flipped; 2026-09-11)

- Harness ~/.cache/dbmod4 (deleted): `mbuild.sh MOD SRC OUTDIR` (module cflags, strip_unused/fold_linkonce, output `OUTDIR/db_mod.o`),
  `mtryv.py FUNC v.py [--mod MOD] [--only A,B] [--apply NAME] [--keep]` (v.py = `VARIANTS = {name: [(old, new), ..]}` substring
  edits, judged with `OBJ= tools/bytecmp.py MOD/db_mod FUNC`, 0.3 s per variant), `mrel.py MOD SYM [OBJ] [--all|--target|--ours]`
  (side-by-side objdump with function-relative branch labels and the reloc-name/offset noise removed from the diff key),
  `mdump.sh MOD -dX [-fsched-verbose-9]` (`CC1DIR=` for a hooked cc1plus, `SRC_OVERRIDE=`), `tree.py build LABEL [CC1DIR] [ENV=v]` /
  `tree.py cmp BASE NEW` (every prodg_cc edge of build.ninja in 9 s on 24 threads + bytecmp per unit, regressions/fixes per
  function). Hooked private cc1plus (copy of tools/sn-gcc): loop.c prints `[thr N ic N am N mo N]` per movable and reads
  `LOOP_THR_ADJ`/`SR_THR_ADJ`; global.c prints `;; ORDER i: reg R refs N len L -> hard` per allocno under `GDBG=1`; haifa-sched.c
  prints `;; LL reg R block B seg N` = the post-sched1 live-length segments under `GDBG=1`. 111 OK before and after.
- **dbmod_scale 2 -> 0 (TAGGED, `// COMPILER-DIFF`): two `asm("" : : "r"(i))` at the loop-body top.** The mechanism, read with the
  hooked loop.c: move_movables' threshold starts at `1 + n_non_fixed_regs` = 71 and is lowered by 3 after every moved movable
  (71, 68, 65, 62 ...); the loop has 64 real insns at pass 1, so the third movable (the cprop-folded `x - 1` = `li r24,5`) still
  passes (65 >= 64) and the fourth ("%f" high, 62) does not; the target moved only two in pass 1 and `x - 1` in pass 2 (after the
  giv init `li r30,56`), i.e. its loop had >= 66 real insns at loop pass 1 that were gone by final (or one more pass-1 movable).
  Ten source spellings of the body change nothing; the two codeless volatile asms (real insns, no code, no operands to allocate)
  reproduce it. Whole-tree `LOOP_THR_ADJ=-1/-2/-3` regress 81/97/181 matched functions (a lower threshold is NOT the original's
  compiler), so the extra insns are a source-shape fact still to be found.
- **position_usage 10 -> 0 (zero code): `int x = 43;` at the declaration (no `x = 43` in the arms) and the increments inside the
  argument list, `eprintf(x * 8, y++ * 14, ..)` followed by `eprintf(x * 8, (y - 1) * 14, ..)`.** (1) `y++` as a post-increment
  argument is queued by expand and emitted at `emit_queue` BEFORE the call (`addi r31,r31,1` above every `bl`), and combine refuses
  to fold the following `(y - 1) * 14` because `INSN_CUID (insn) < last_call_cuid` ("don't combine across a CALL_INSN") -- that is
  the whole "y live past the last call" story of passes 2/3. (2) a single `x = 43` at the top with all uses in later ebbs: cse1
  never sees the constant at `x * 8`, the pseudo has a REG_EQUIV constant, loses global-alloc (the function uses r28-r31 for y and
  the tail copies) and reload REMATERIALISES it at the use -- the target's `li r30,43; slwi r30,r30,3` inside the join block after
  `mulli r4,r31,14` is that rematerialisation, not a cross-jumped arm tail. Rule: a `li rX,K; slwi rX,rX,n` pair mid-block with K a
  declaration-time constant = spilled REG_EQUIV pseudo, declare the variable initialised.
- **IKreport 10 -> 0 (zero code): a second variable holding the byte, `int info = .. & 0xFF; int flag = info;`, tests 1-2 on
  `info`, test 3 (`& 0x80`, the join block) on `flag`.** cse1 canonicalises `flag` to the older pseudo inside its ebb (tests 1-2
  use r0) and the copy `mr r11,r0` survives for the test outside the ebb. The same copy closed dbmod_p_info's case-0 classifier
  (117 -> 71, size 0x62c -> 0x634 of 0x638). Not gcse (pass 7's reading): a plain second variable.
- **dbmodGetFilenames 17 -> 0 (zero code): the case bodies are `t = 1; pNo = &..binNo; pNum = &..binNum; dir = ..binDir;` (dir
  LAST, pNo/pNum/dir in that order).** The pseudo copied from the loaded `pDbModState.p` dies at the last statement; sched1 ranks the
  three `addi`s by register weight (the one carrying the REG_DEAD is issued first, then LUID order) and the post-sched1 live
  lengths of pNo/dir change with it, which is what flips the {pNo, dir} vs {name-table biv, pMotTbl high} callee-saved
  permutation of passes 2/3 (global.c priority `floor_log2(refs) * refs * 4 / live_length * 10000`, ties by allocno number; the
  four were 7 refs / 148, 148, 150, 148 insns in ours). Rule: for a callee-saved permutation among equal-ref pseudos, look at
  which statement of the block carries the death of their common source -- the statement ORDER inside the arms, not the
  declarations, moves the lengths.
- **dbmod_blend 25 -> 0 (one TAGGED item): `x = 6; y = 4;` before the loop and the expressions `(x - 1) * 8` (cursor), `(x + 10) * 8`
  (values), `(x - 1) * 14` (case-1 row), `(y + 2) * 14` (case-2 rows) instead of the cx/nx/ny variables** -- cse1 does not know x/y
  in the loop, cprop folds each `x + k` to a constant pseudo inside the loop, loop.c hoists them in insn order (pDbModState high,
  5, 16, "%.2f" high, 6: the target's preheader), cse2 merges the duplicates; `y = 4` itself dies after cprop (no `li r,4`, the
  target has none). The colour of case 0 stays tagged: `asm("" : "+r"(color))` after `color = 0` keeps `(set r5 color)` in case 0
  (the target's `li r5,0` before the tree serves case 0; ours folds the argument to a fresh `li r5,0`). Setting `color` at the
  body top / mid / as `int color = 0` in the loop: 61 words each.
- **dbmod_motion 27 -> 11:** (1) the motSub `old` is a block-local variable (`int oldSub = ..` inside `if (joy->on & 0x800)`): the
  shared `old` pseudo spans blocks and goes to global alloc (r4), the local one to local-alloc's first free register (r8);
  (2) `nx = 16` after the first eprintf of the body (pass-1 rule sharpened: after, not before); (3) the digit loop uses a THIRD
  counter `j` (not the `k` of case 1): k's 31 refs over 151 insns outrank hs/nlen and take r31; with `j` the loop counters are
  r29/r30/r31 as in the target. Left (11): `lhax r9` vs `r0` for the motNum test (a `no` variable, int or s16, does not change it)
  and the tail's pDbModState high r29/r30 + `len`/`hs` r30/r31 with `addi r30,r30,24` written into the variable's register
  (`hs = hs - 1 + 25; eprintf(hs * 8, ..)` and the len-only forms give 18-42: the tail's chain is a different variable from the
  loop's hs/len, and its identity was not found).
- **dbModMotionMove 163 -> 98 (size 0xa58 -> 0xa5c of 0xa54):** (1) the sub-slot loop normalises the z column through a FOURTH
  Vec (`Vec ax, ay, az, az2;`, frame 192 -> 208 = the target's; the first loop's az stays at 104(r1), the second uses 120(r1));
  (2) the `order[]` init is `s8* p = &order[SLOT_NUM - 1]; for (n = SLOT_NUM - 1; n >= 0; n--) *p-- = n;` with `n` = the main
  loop's callee-saved counter (the value register is `li r24,63`, callee-saved, later `li r24,0` for the main loop; with `i` it is
  r7): `addi r9,r1,71; stb; addi -1; addi -1; bdnz` reproduced. Left (98): &order r23/r24 and n r24/r26 naming, the swap loop's
  `i + 1` (target: `addi r7,r7,1` in the no-mismatch arm and `mr r7,r5` after the search = PRE insertion at the END of the else
  block; `continue`/`while` forms unchanged), `!(em->xE38 & 1)` as `andi.; bne` (ours `xori; andi.; beq`; `== 0`, nested-if and a
  u32 local: 458/458/142), and `move`: the target evaluates `((flags & 1) && (flags & 0x10)) ? state & 3 : ~flags & 1` in the
  arms with the CR set there and one shared `beq` (`andi. r9,r0,3; b L; L508: not r0,r9; andi. r11,r0,1; L: beq`), plus a
  `mr r9,r0` copy of the u16 flags used by tests 2-3 -- the ternary inside the `if` gives the `not`/`andi.` arm (140) but not the
  shared beq nor the copy.
- **dbmod_p_info 117 -> 71 (size now 0x634, +4 short):** the `flag` copy above. Left: the zero pseudo (`li r25,0` in the j-loop
  PREHEADER, `mr r7,r25` as the colour of both eprintf2 calls) and, as its consequence, `li r0,19` (x + 13) rematerialised at the
  use instead of hoisted to r14: with one more callee-saved pseudo live across the j loop the '19' pseudo is the one that loses
  global alloc (r14 goes to the pinfoNum high, r15 to x, ... the target's names). The zero must be a loop MOVABLE (set inside the
  j body, one set, not live at loop entry) that cse1 cannot fold at the two calls: `int color = 0` at the top (38, hoisting order
  wrong), `color = 0` before the loop / at the i-body top (71), at the j-body bottom (33: live around the loop, not moved),
  `y - 4` / `x - 6` (71). Also left: `mr r18,r27` (the label base copy) and the register names after it.
- **dbmodDispModelName 100 (read, unchanged):** `register asm` pins on i/k destroy the giv machinery (231-254 words, -0x60 bytes);
  cx/nx inside the loop (bottom / case 2) 175-244. The residue is the callee-saved permutation i r26/r24, k r31/r27, len/name/hs
  and the givs; global.c's order for ours: color (40 refs/139) r30, the k-loop givs (11/42, 11/46, 18/102) r31/r29/r28, k (63/462)
  r27, ... i (31/288) r24 -- the target hands r31 to k and r26 to i, so its k-loop temporaries rank BELOW k (fewer refs or longer
  lives) -- the same kind of lever as GetFilenames' statement order, not found this pass.
- Compiler-side facts (whole-tree, nothing installed): `LOOP_THR_ADJ` -1/-2/-3 = 81/97/181 regressions, 1/3/3 fixes. The
  `fn_*_1BF24 24`, `create__t8cManager1Z3cEmi 1`, `loadModel 1` rows are still the .text-size pairing artefact.

### Tool RELs, t_esp pass 11 (t_esp 208/212: InitTool 9684 -> 11530 words with the 882 `&pos` spill SLOTS now in the target's order and the entry block's 25 constant registers exact, .text 0xa124 -> 0xa050 of 0xa20c; Load/SaveEmType 2/2 untouched; db_widget DB_STRING ctor 11 -> 7 with a pin; nothing flipped; 111 OK; 2026-09-11)

- Harness ~/.cache/tesp11 (deleted): `mk.sh SRC OUTDIR [-dX..]` (module cflags, `fold_linkonce --module`; with dump flags it runs cpp +
  cc1plus by hand so the dumps land in OUTDIR), `tryv.py BASE.cpp V.py [names]` (exact-substring variants, `old*` = replace-all; prints
  InitTool words/size + the entry-block skeleton), `slots.py LISTING dtk|obj v` (the `addi rX,r1,C; stw rX,S(r1)` pairs of the entry
  block = spill slot S of every PRE'd `&pos` C), `fitn.py SLOTS [K]` (brute-forces the gcse table size N whose bucket order
  `(K + C) % N` reproduces the slot order), `sbs.py`/`segcmp.py`/`seglen.py` (side-by-side and per-`bl` segment diffs of the dtk target
  vs our objdump), `skel.py`, `storder.py OBJ` (final order of the entry block's symbol stores), `ins.py DUMP FUNC` (one line per insn
  of a 2.95 dump), `deadset.py`, `rostr.py OFF..` (the string at a module .rodata offset: unit base 0x35E0), `mkw.sh`/`tryw.py` (the same
  for db_widget). 111 OK before and after every edit.
- **InitTool spill-slot order = gcse bucket order, and the table size is a COMPUTATION (done, exact).** Facts: `alloc_expr_hash_table
  (max_cuid)` with `expr_hash_table_size = (n_insns / 2) | 1` (min 11), `max_cuid` = the count of 'i'-class insns (INSN/JUMP/CALL) when
  gcse runs; `pre_delete` walks `expr_hash_table[0..N-1]` and each chain in insertion (first-occurrence) order and creates the reaching
  regs in that order; reload's `alter_reg` assigns fresh slots in ascending pseudo order; `hash((plus:SI (reg 31) (const_int C))) =
  13289 + C` (13258 + REGNO 31, verified: N = 5195 reproduces our own 883 slots with 0 mismatches). The target's 882 `&pos` slots
  (0x1a80..0x2844, read with slots.py) are reproduced with 0 mismatches by N = 5233 or 5235 ONLY (2617 also fits but is half; N = 5237
  already permutes 48). Ours had 5195 (max_cuid 10388..10391); +76 real insns give 5233 and +80 give 5235: 76 dead `i = K;` sets (distinct
  constants; equal ones are deleted by cse1 as noop sets, `i = K` survives cse1 because `i` has real uses, flow1 deletes them after gcse)
  right after the entry statement block, tagged `// COMPILER-DIFF: candidate (gcse table size)`. After the rest of this pass's edits the
  table is 5235 buckets (2353 entries) and the slot ORDER is exact; the two N are equivalent for the slots. Rule for any function with
  hundreds of PRE'd addresses: read the target's `addi;stw` pairs, fit N with fitn.py (the wrap points of the C sequence give N directly),
  then move the insn count -- the "original has more RTL" reading of pass 9 was exactly 76-80 insns.
- **Slot SET (which `&pos` are spilled) is the last residue of the entry: ours spills `&fp+0x60` and keeps `&fp+0x150`, the target the
  reverse; the low spill region (0x1a54..0x1a7c, copies `T = R` of `&pos` values made before/after the first `__builtin_new`) has 11 slots
  in the target and 9 in ours (10 before the CreateEditWindow change), so every slot offset is 8 bytes low and each `lwz/stw slot(r1)`
  differs -- that is the whole 11530-word count (the previous 9684 had the same offset problem plus a wrong order).** Which callee-saved
  regs the first &pos pseudos get (target r31,r21,r20,r19,r18,r17,r28,r16,r15,r14 = &8,&20,&30,&40,&50,&60,&110,&120,&130,&140; ours
  now the same set) and which highs of g_pEditSeq/g_pEditSeq2 get hard regs per window (the target has ONE high pseudo per inlined
  window ctor -- `expr_equiv_p` compares SYMBOL_REF strings by pointer, so the inlined copies are different gcse expressions -- and
  reload's `find_equiv_reg` reuses a callee-saved register still holding the constant across later windows: r17/r23/r16 regions, fresh
  `lis r6` where a call-clobbered one held it) are global.c priority effects downstream of that.
- **Entry statement block (zero code): the order `g_filter, g_render, g_roomCam, g_bgR, g_bgG, g_bgB, g_grid, g_workEm, g_modSk, g_fog,
  g_evCam, g_cinesco, g_pTexRender, g_pEditSeq, g_pEditSeq2` gives every constant/high the target's register (QI zero r0, SI zero r29
  callee-saved, one r4, fifty r5, wk2 r10, wk r11, pTexRender r8, pEditSeq r3, pEditSeq2 r30, the 7AC..7CC highs r28/r7/r27/r6/r21/r20/
  r9/r26/r25/r24/r23/r22) and the target's final store order except `stw pTexRender` (target first, ours after cinesco).** Mechanisms
  read: (1) the three byte zeros must PRECEDE the first word zero in RTL, otherwise cse1's wider-mode rule (`src_related =
  gen_lowpart_if_possible` of an SImode register already holding the constant) makes the QI stores `subreg`s of the SI zero and one
  pseudo serves all six (ours before: `li r0,0` for everything); (2) the SI zero then gets r29 = the first callee-saved in
  REG_ALLOC_ORDER after r31 (never handed out by local-alloc) and r30 (pEditSeq2's high), i.e. it is the block's lowest-priority qty
  with the longest life; (3) `g_editRowNo[i] = i;` in the row loop (target `stbx r11,r9,r11` stores the counter, ours stored a zero);
  (4) the final (sched2) order of block 0 is decided by the hard-register reuse chains reload creates (spill temps r0/r8/r10 through 880
  pairs), not by sched1 priorities -- `li r29,0; lis r8; stw r29,pTexRender` are first in the target because the pTexRender high sits at
  the head of the r8 chain, i.e. its store precedes the first r8 spill pair in the post-reload order; ours has it after ~20 pairs.
- **CreateEditWindow1..4 take the slot by reference and allocate inside (zero code):** the target computes `lis r9,g_pEditWin1@ha; addi
  r24,r9,@l` BEFORE `__builtin_new`, keeps r24 through the whole widget body and stores `stw r28,0(r24)` at its END; only an address
  argument evaluated before the inline body gives a `lo_sum` in a register (a plain `g_pEditWin1 = new ..` embeds the address in the
  MEM and stores right after `new`). Form: `static inline void CreateEditWindowN(TOOL_WINDOW*& slot, DB_PRIM_ARRAY* p) { EDIT_WINDOW* e =
  new EDIT_WINDOW(p); ...; slot = e; }`, call `CreateEditWindowN(g_pEditWinN, g_pPrimArray)`. Reproduced: `addi r25,r9,@l` before
  the new, `stw r30,0(r25)` at the end. Costs ~2900 words in the count because it removes one low spill (see above), kept anyway.
- **The 19 CreateString blocks still written as `{ DB_POINT pos(x, y); pa->CreateString(win, s, &pos); }` (the `->SetUpdateCallback`
  chains, the X:/Y:/Z: and Work-window macros) are the temporary form `&DB_POINT(x, y)` like the other 117 (zero code).** With the
  local form ours issued the pos.x fp-store FIRST and the pos.y store through the argument register (`stfs 4(r6)`, regmove folds the
  dying arg copy into the copy's def); the target's block is `lwz r6,slot; lis; lwz r4; addi r5; lwz r3; [mr r9,r6;] stfs x,C(r1); stfs
  y,4(r9)` -- the `mr` is reload_cse turning the second reload of the spilled `&pos` R (the pos.y base) into a copy of the argument's
  reload, present or absent per block depending on whether reload_cse_simplify_operands substitutes the argument register into the
  store. All 149 CreateString calls are now the temporary form; the 8-line CreateString segments are the target's length.
- Not closed: InitTool 11530 (the slot SET and the low spill count above; every per-`bl` segment differs only by spill offsets /
  callee-saved names / reload order; 157 segments differ in length by +-1..9 = the `mr` copies and rematerialised highs);
  Load/SaveEmTypeUpdateCallback 2/2 (pass 10 mechanism stands: which of loop-2 TOP / the last `lhz t` block cse1 reaches with the outer
  high); DB_STRING ctor 7 (`register u32 zero asm("r0") = 0` in a block after the float/type stores, tagged `candidate (local-alloc qty
  order)`: all four constants now in the target's registers; the str/len zero stores are still issued third/sixth where the target has
  them last after the vptr store -- the zero's stores follow its `li` by haifa's last-scheduled-insn class rule; a live-out zero
  (asm keep-alive after the `new`) gives 14). fn_t_esp_3DE4C 24 (the shared cDbgToolMain ctor, another owner).

### Tool RELs closer: snd_test/t_camera_data/t_esp_area/t_lightarea (t_movie/snd_test 73 -> 75/77, 206 -> 140 words: test_play_or_stop 2 -> 0, snd_test_disp_rit 28 -> 0, disp_sequencer 64 -> 28; t_camera_data / t_esp_area / t_lightarea unchanged, dbg_tool.h untouched; nothing flipped; 111 OK; 2026-09-11)

- Harness ~/.cache/tools_fin (deleted): `mtry.py MOD/UNIT [FUNC] [--src ABS] [--dump '-dX'] ` (build.ninja edge parsed after joining `$\n`,
  judged with `OBJ=... bytecmp.py`; `INC=dir` prepends an include dir for header-copy experiments; `--dump` runs ngccc.py through a wrapper
  that keeps the temp dir, so the RTL dumps land in `dump/`), `fd.sh MOD/UNIT SYM` (side-by-side objdump of the split object vs the harness
  object; fdiff.py ignores `OBJ=`), `hvar.py NAME OLD NEW` (dbg_tool.h variants for `INC=`).
- **test_play_or_stop 2 -> 0 (tagged `COMPILER-DIFF: asm-emitted dead load`).** `asm volatile("lhz 4,%0" : : "m"(w->reqCur) : "r4");`
  before the inner call written into a local (`char* name = Snd_test_get_str_name(blk); Snd_str_prepare(blk, w->reqCur, name, -1)`). The
  local alone is 21 words (the arg moves after the call are fine, the dead `lhz r4` before it is what the 2 words were); no C form gives a
  load into a hard register that a call then kills -- `precompute_register_parameters` converts a promoted MEM arg into a pseudo before
  the nested call, and a `(mem:HI)` arg would only stay a MEM for a HImode (struct-typed) parameter, which gives ONE load after the call.
- **snd_test_disp_rit 28 -> 0: the `li r4,84; addi r4,r4,84` MONOPOLY y (tagged `#13 (rematerialised REG_EQUIV constant)`) + a
  block-0 tie fixed by ONE `blk->shd` load.** (1) `int half = 42; int k = half + half;` at the function top and
  `asm("addi %0,%0,84" : "=r"(y) : "0"(k))` at the row. Mechanism (local-alloc.c update_equiv_regs): cse folds `half + half` and leaves a
  REG_EQUAL 84 note (a literal `k = 84` has NONE, so it never becomes REG_EQUIV); the note becomes REG_EQUIV, REG_N_REFS == 2 and
  REG_BASIC_BLOCK < 0 set `reg_equiv_replace`; at the use `validate_replace_rtx (k -> 84)` FAILS only for an asm whose constraint rejects a
  constant (a plain `(plus k 84)` is folded to 168 by plus_constant), so the init is MOVED in front of the use (`depth == 0` branch) and
  local-alloc ties the "0"-constrained input to the output: `li r4,84; addi r4,r4,84`. gcse's cprop does not fold it for the same
  constraint reason. (2) The 15 remaining block-0 words (Snd_str_blk high r8/r10, &blkNo r9/r8, reqCur r10/r11, blk r11/r9 rotated)
  came from `blk->shd + ((u32*) blk->shd)[..]`: two loads of the same field that cse merges but whose qty birth differs; `u8* base =
  blk->shd; shd = base + ((u32*) base)[rit->shd_no]` gives the target's ties (0 words). Rule: a whole block of caller-saved temporaries
  "rotated by one" with identical code = one field read twice in the source where the original read it once.
- **disp_sequencer 64 -> 28 (one structural item + tagged `#13 (asm-emitted constant + keep-alive)`).** (1) STRUCTURAL: the channel label
  is `eprintf2(6, 13, x, 0x54, 0, 1, "%03d", ch + 1)` -- the target passes r10 = `addi r10,r30,1` and keeps it as the loop's next ch
  (`mr r26,r10` .. `mr r30,r26`); with `ch` the increment is a plain `addi r27,r30,1`. (2) The PAN y `li r9,84; addi r27,r9,84` sits at
  the TOP of the join block after the `D` diamond (a loop-body computation, not hoisted): `asm("li %0,84" : "=r"(k) : "r"(ch)); y = k +
  0x54; asm("" : : "r"(y));` right after the `if`. The "r"(ch) input keeps loop.c from hoisting the asm (an input-free asm is invariant
  and lands in the outer preheader as `li r9,84` after the header eprintfs; a "m" input pins it behind the previous call so it lands at
  its use); the keep-alive gives `addi y` an in-block dependent, otherwise sched1 issues `li k` after the voices `lwz` and k/voices
  cannot share r9 (28 -> 39). Rejected: k set in both arms of the diamond (a global r6, the arms' `li`s are not cross-jumped because
  sched1 hoists the free `li` above the call), the REG_EQUIV moved-init recipe (the use is hoisted into block 0 by loop.c -> same block
  as the set, no move), "=m" keep-alives (48-120). Left (28): the two spilled row pointers (`addi r9,r28,0x31aa; stw r9,8(r1)` and
  0x31ba) are issued one header eprintf too early in ours (sched1 slot choice among 14 free `addi`s), and `addi r27,r9,84` is one slot
  early (the keep-alive is a barrier: target `li r9; lis r11; li r0,64; addi r27`).
- **tcSetBesideOffset 27 (t_camera_data) read, not closed: it is NOT "the n*12 giv first" but "157 (loop 2's i+1) must take r31".**
  Ours: 209 (n*4 giv) 9 refs/82 insns beats 207 (n*12+0x18c giv) 9/84 (same refs, 207's init is emitted first -- the outer-loop
  re-reduction of the inner loop's preheader givs is in list order, last-discovered first), 209 takes r3 in find_reg pass 1 (pass 0
  excludes r3 because c (84) has a copy preference for it from `mr r28,r3`, and r31 is never in regs_used_so_far), 207 takes r31. The
  target's whole permutation (loop 1 r3/r31 swapped, loop 2 shifted by one) follows from ONE fact: 157 = `i+1` of loop 2 (allocated
  before 209/207) got r31, which puts r31 into regs_used_so_far so 209 takes it in pass 0 and 207 gets r3 in pass 1. 157 takes r31 only
  if r3 is unavailable to it in pass 1 = a hard-reg conflict. Tried: `asm("" : : : "r31")` in block 0 (no effect: flow never marks the
  frame-pointer regno ever-live before reload, so r31 cannot enter regs_used_so_far), swapping the pos/at and roll/fovy statements
  (right allocation, wrong `li` order and body order: the init order and the schedule order move together), `register QfpsOfs* rr
  asm("r3") = ready[i]` (r3 conflicts everywhere in loop 2: 32), `asm("" : : : "r3")` at the loop-2 body bottom (157 -> r31 and the
  ready row -> r3 as the target, but the clobber also kills c's r3 copy preference, so loop 1's j*44 giv takes r3 in pass 0: 27).
  Open: a codeless r3 conflict for 157 that leaves c's preference intact (r3 live where c is not: c is live everywhere in loop 2).
- **ToolEspArea / ToolLightAreaMain CreateEditWindow boundary (438/393, not applied).** The `cmpwi r31,0` placement is lcm.c's
  block-based PRE: `earlyout[S] = ~transp[S] | (earlyin - antin)` makes the compare earliest at the exit of the block that sets
  `edit`; with the loop header as S's direct successor `delayin[header]` is 1 (antin & earlyin) and the delay runs to the exit block
  (no insertion, ours); ANY block boundary between the `mr r31,r3` and the loop header gives `latein` at that block (its successor,
  the loop header, has `delayin` 0 through the zero-initialised back edge) -> insertion at its end = the target. The dead test `if
  (pTop) i = 0;` in place of the `do {} while (0)` reproduces the structure (t_esp_area 438 -> 427, size 0x1c4c vs 0x1c50 still one
  word short; t_lightarea 393 -> 488 words but .text size EXACT, so its six small-function "diffs" (cutBuffer, LocalUpdate,
  execCopyWindow, fn_2F2D8, fn_30410 24/59) are address artefacts of ToolLightAreaMain's size and vanish; t_event SubToolMessInit 145
  -> 157). Remaining in the ctor: (a) `rows`(5)/`n`(32): the target hoists `li r29,5` above strlen and keeps `li r11,32` after, ours
  the reverse -- the two `li`s are CreateEditWindow's inlined PARAMETER setup (cse folds the ctor's copies into them), sched1 issues the
  lower LUID (n, the 5th parameter) into the free slot before the call; swapping the ctor's parameter order changes nothing, swapping
  CreateEditWindow's (`u32 nRows, u32 n`, call sites `(.., 5, ESP_AREA_MAX)`) fixes it (424) but needs t_event's call site too (not
  ours). (b) The zero: target `li r0,0` once after strlen for BOTH halves (so the zero pseudo does span the boundary and is a global
  allocno that got r0), `one` r9, `n` r11; ours zero r8 because local-alloc gives `one` r0 first. `register u32 z asm("r0")` in the
  ctor is IGNORED (the RTL shows a pseudo: the asm register name of a local is dropped when the TEMPLATE member is instantiated), an
  asm-emitted zero (`asm("li %0,0" : "=r"(z) : "m"(x))`) is the same global pseudo (r8). Do not retry: dead tests `i = rows/n/wx/7/1`
  (identical to `i = 0`), `x = 0`/`num = 1` (468), `if (pTop == (T*) nRows)` (427).
- Not iterated: disp_sit_normal 112, tcDataExport 142, fn_t_camera_1B8C4 24, fn_Tools_30410 59.

### Tool RELs, t_snd_vol pass 2 / t_id (t_movie/t_snd_vol 357 -> 135 words, 26/27: data_edit 8 -> 0 (pin), file_load 9 -> 0 (launder), file_save 37 -> 0 zero code, combine_tbl_disp 103 -> 0 zero code, edit_reverb_param 200 -> 135 (launder); t_id/t_id 1176 -> 1155, .text gap 0x78 -> 0x64: toolIdInit 53 -> 16 (size closed), ToolInterfaceDesign size closed; nothing flipped; 2026-09-11)

- Harness ~/.cache/tools_sv2 (deleted): `mtryv.py MOD/UNIT FUNC v.py [--apply NAME] [--keep]` (substring variants of src/MOD/UNIT.cpp,
  judged with OBJ= bytecmp), `mbuild.sh`, `mdump.sh MOD/UNIT -dX` (SRC_OVERRIDE), `sbs.py MOD/UNIT SYM [OBJ] [--all]` (objdump side by
  side with branch targets and reloc immediates masked -- fdiff.py runs ninja UNLOCKED and truncates at 80 rows, avoid it), `dump/fn.py
  DUMP 'void f()' [regex]`. Every edit built under the ninja lock; `ninja -k 0` + shasum = 111 OK before and after.
- **file_save 37 -> 0, all zero code, three readings of the target's registers.** (1) The offset loop's `s` is untied from its `work`
  load (`lwz r0,work; add r4,r0,r10`) and sits in r4 = the memcpy source register: ONE function-scope `CombSel* s` assigned in both the
  offset loop and the copy loop (REG_BASIC_BLOCK global -> local-alloc cannot tie it, global.c takes the `mr r4,s` copy preference).
  (2) `memcpy(p, t, n); p += n; size += n;` in that order: sched1's tie-break for the two adds after the call is INSN_REG_WEIGHT
  (sets minus deaths), and `n` dies in whichever add comes LAST in the source -- the target issues `size += n` before the call and
  `p += n` after, i.e. `n` died in `size += n`. (3) `n = HDWrite_only(...)` reuses the `n` variable (crosses the memcpy calls ->
  callee-saved r30 = `mr. r30,r3`); a fresh `int ret` gets r3, `i` gets the loop counter's register. Rule: a call result that lands in a
  callee-saved register with no later call is an existing call-crossing variable being reused.
- **data_edit 8 -> 0, tagged `register TblEnt* ne asm("r8")`.** local-alloc gives `ne` r10 (num is a GLOBAL pseudo -- live from bb 21 --
  so its r10 is unknown to local-alloc and does not block ne), then global.c gives editMode r8; the target has ne r8 / editMode r7, i.e.
  r10 was blocked for ne in ITS local-alloc. Zero-code forms tried (all 8): `int num` local, `tbl->e + tbl->num`, `pe = &e[num-1]; ne =
  pe + 1`, function-scope ne, `val` store first.
- **file_load 9 -> 0, tagged launder in the ELSE arm.** `int col; if (sub == 1) col = loadCur == 0 ? 6 : 0; else { col = 0; asm("" :
  "+r"(col)); }` keeps the `clrlwi r5,r5,24` of `(u8) col` (combine's nonzero_bits of a reg whose every set is a constant folds it; the
  asm set is an unknown). The launder AFTER the join lands as an insn in the join block and pushes the clrlwi one slot later (2 words);
  in an arm it is free. `u8 col` variable, uninitialised-path `u8`, `(u8)` on the inner ternary: 9.
- **combine_tbl_disp 103 -> 0, zero code -- the whole residue was source shape:**
  - `int y` (not s16) block-local in each loop body, declared at its point of use: the `(s16)` conversion is made ONCE at the first
    ListDraw arg (cse shares the `sign_extend` for the next two -> `extsh rN,rN; mr r4,rN` x3), and local-alloc ties the sum, the extsh
    result and the mulli/add operand into one call-crossing qty (`mulli r30; addi r30,r30,186; extsh r30,r30`; r31 is the frame
    pointer before reload so the first callee-saved a LOCAL qty can take is r30). A function-scope y (s16 or int) is REG_BLOCK_GLOBAL:
    no tie, the sum goes to r0.
  - `li r18,275 / li r19,385 / li r21,168` + `mr r3,r18` / `extsh r3,r18` are NOT int variables holding constants (gcse cprop folds
    `mr r3,x1` whenever the constant set reaches on all paths, even with two identical sets: identical constant sets share one
    set-table entry): they are `0xA5 + w`, `0xA5 + w * 2`, `ybase - 0x12` with `int w = 0x6E` / `ybase = 0xBA` variables -- cprop
    cannot substitute a constant into `(plus w 165)` (2.95.3 try_replace_reg has no simplify step), loop.c hoists the invariant
    plus to the preheader in body order, cse2 folds it there to `li`, and the body uses stay `mr`/`extsh`. Preheader order = first
    use order in the body (fmt lis, 168, 275, 385).
  - `CombSel* s = i <= 1 ? &work->sel[work->copySrc] : &work->sel[work->copyDst];` -- each arm computes the whole address (its own
    `work` load, kept live to the `add`), jump2 cross-jumps the identical `extsb; slwi; addi 108; add` tails (target's `b` into the
    other arm after the `lbz`). `int n = ..; s = &work->sel[n]` reloads `work` in the join: the three `(mem work)` have distinct
    `high` pseudos at gcse time, so PRE cannot delete the join load.
  - Loop 1: `int yb = 0x6A + i * 0x48; int y = yb + (i / 2) * 0x14;` as two statements -- fold reassociates `(0x6A + i*0x48) +
    (i/2)*0x14` to `(i*0x48 + (i/2)*0x14) + 0x6A` and the giv then starts at 0 (`li r28,0`) instead of 106.
  - Loop 2's `y` statement sits AFTER the second eprintf (right before the first ListDraw): the sum's pseudo crosses no call, so
    haifa's "don't let a non-call-crossing pseudo cross a call" anti-dependence pins the addi/extsh after that call.
  - Once the x/y variables occupy r18/r19/r21, the `&col`/`&c` second pseudos and `high(pG)` are spilled and REMATERIALISED by reload
    (`addi r9,r1,8` / `lis r9,pG@ha` inside the loop): the "#3 frame-address giv copies" of the previous pass were a register-pressure
    effect, not PRE.
- **edit_reverb_param 200 -> 135, tagged launder `asm("" : "+r"(y))` after both `y = 0x80`.** cse1 folds `y += 0x10` to `li r31,144`
  and cprop folds the first `mr r4,y`; the target keeps the chain. Zero-code forms tried: `y = 0x80` before the col/active diamond
  (200), two-set y with equal constants (folds: shared set entry). Left 135: (a) `work`/`p`/`Joy` = r11/r10/r10 vs ours r10/r11/r11:
  global.c order is p (33 refs/113, prio 1.46) > work (16/64, 1.0) > Joy (7/80, 0.17) in ours; the target allocates work before p (work
  r11 first, p r10, Joy r10) -- p needs <= 31 weighted refs or work >= 32; not found. (b) stereo panel: the target keeps the second
  `col` value in r7 and stores it (`stw r7,32(r1)`) after the pt[] `sth`s; ours stores at the join. Dropping the duplicated stereo
  `if` shrinks .text by 0x58 (the target has the duplicate); `u32 c2` register temp / col assigned after pt: 508/545.
- **t_id toolIdInit 53 -> 16, .text gap closed.** (1) `case 1: w->lang = one; break; case 2: w->lang = one; break;` with `u8 one = 1`
  declared at the top (tagged candidate): the target has `li r30,1` in the prologue block, a call-crossing pseudo used only by the
  cross-jumped `stb r30,0x178(r31)` arm; the tree tests 1 and 2 as separate nodes (`beq; bgt` to the same label). (2) The four
  `lis r30..r27,pIdBufN@ha` before the Debug_alloc calls come from an INDIRECT_REF store whose address is evaluated before the call:
  `static inline void IdBufAlloc(void*& p, u32 size) { p = Debug_alloc(size, 1); }` -- the reference argument `&pIdBufN` = `lo_sum(high)`
  is computed as a call argument, cse folds the lo_sum back into the store (`stw r3,sym@l(rN)`) and the `high` crosses the call.
  `*(void**) &pIdBuf0 = ...` folds back to the plain store (50). Left 16: the field-store block's constant qtys (100/0/1/255 in
  r8/r7/r11/r10 vs r7/r8/r9/r11) and `w->level = 0` stored from the SImode zero (`stb r7,30` = the `stw r7,36` pseudo) where ours
  uses the QImode zero: level is assigned an int-typed zero in the target (e.g. an int variable/expression), not the literal.
- **t_id ToolInterfaceDesign, size closed (25 -> 41 words, 0x130 -> 0x138):** the second joypad test is `Joy[0].trg != 0` (fresh
  `addi r9,r25,Joy@l` off a hoisted high) while the first stays `joy->trg & 0x400` through the `JOY* joy` pointer (r24). Left 41: pure
  interblock scheduling -- ours hoists `addi r3,r31,toolIdSys@l`, `li r11,0`, `li r3,1` (TaskSleep arg), `li r11,16` above the
  branches (haifa forms one 9-block region over the `while (1)` body, `-fsched-interblock` speculative motion); the target has every
  constant in its own arm, i.e. its loop body was NOT a single region: `too_large` (> 10 blocks or > 100 insns) or a REG_LABEL /
  computed jump in the function. A source form with two more blocks or > 100 insns in the body would split it -- not tried.
- Not iterated in t_id: toolIdDrawSafeZone 31 (0x8 short), toolIdPaste 45 (0x4 long), toolIdOption 146 (0x20 short), idEditTrans 176
  (0x4 long), idEditColor 201 (0x1C short), idEditPos 334 (0x28 short), idEditUnit 79, toolIdEdit 58, idEditId 12, toolIdEditDisp 4,
  the three dtors/create/static_init (reloc-by-address artefacts of the .text size gap; they vanish when the sizes match).

### Tool RELs, t_event closer (t_event/t_event 900 -> 362 words in-tree, .text 0x72f0 -> 0x7324 of 0x7320: CallbackLoad 136 -> 1, CallbackSave 174 -> 12, SubToolMessInit 212 -> 42 (0x1074 -> 0x1098 of 0x10a0), SubToolMessMove 346 -> 274 (106 with the dbg_tool.h Update() case order below); nothing flipped; 2026-09-11)

- Harness ~/.cache/tools_tev (deleted): `mbuild.sh SRC OUTDIR` (module cflags + fold_linkonce, absolute OUTDIR), `mtryv.py FUNC v.py
  [--apply N] [--keep]` (variants may carry `{'src': [...], 'hdr': {'include/x.h': [(old, new)]}}` = a copied include dir), `msbs.sh
  SYM [OBJ]` (ours truncated to the target's length: the unnamed template tail follows every function in our object), `mdump.sh -dX`
  with `SRC_OVERRIDE`, `lcount.sh SRC FUNC` (the `Loop from .. real insns` / `savings` lines of one function). All builds under the lock.
- **Both xml callbacks are three inline levels, the arrays owned by the middle one, and the tool's `EventMessageData* m = t->pMess`
  loaded at the callback's top.** `CallbackLoad(arg) { t; m = t->pMess; char path[0x100]; sprintf; EvtMessRead(m, path); }` (m in a
  callee-saved reg BEFORE sprintf: the target never keeps `t`), `EvtMessRead(m, path) { XmlNodeData d; char tmp[0x80]; char
  buf[XML_BUF_SIZE]; XmlNodeDataClear(&d); m->num = 0; memset(m); if (!EvtReadXml(path, &d, tmp, buf)) err; m->num = d.num; loop }`,
  `EvtReadXml(name, d, tmp, buf) { XmlSimple xml; char* cur; size = HDRead(name, buf); buf[size] = 0; if (size > XML_BUF_SIZE - 1)
  ..; cur = buf; xml.GetXmlStart(&cur, cur, "Node"); d->num = 0; loop; }` -- frame path 8, d 264, tmp 17872, buf 18000, xml/cur at
  fp+0x35390/94 in both callbacks; the save side mirrors it (`EvtMessWrite(m, path)` / `EvtWriteXml(name, d, tmp, buf)` with
  `cur = buf; SetXmlStart(&cur, buf); .. HDWrite(name, buf, cur - buf)`). SubToolMessInit has the same `m = t->pMess` at function
  scope (loaded before the `sw == 1` test) and calls `EvtMessRead(m, path)` with its block-local `char path[0x80]`.
- **The size limit is a literal, not a parameter** (`if (size > XML_BUF_SIZE - 1)`): a `u32 max` parameter of the inline is a pseudo
  live across HDRead (`lis r30,3; ori` before the call, callee-saved); the literal is materialised after the call into r0 (18 -> 1).
- **`xml.GetXmlStart(&cur, cur, "Node")` with `cur = buf` just before** = `mr r5,r27`: cse forwards the store to the load and the
  argument is the buf pseudo; `GetXmlStart(&cur, buf, ..)` also gives `mr r5,r27` in ours but its 4 words vanished with `cur`
  (CallbackLoad 5 -> 1) -- the source reads from `cur`, like `GetXmlNext(&cur, cur, ..)`.
- **XmlNodeDataClear is two `while (i--)` loops over a stepped pointer** (`XmlNode* n = d->node; i = 100; while (i--) { char* p =
  (char*) n; j = 11; while (j--) { memset(p, 0, 16); p += 16; } n++; }`): both loops are check_dbra_loop's GE/nonneg reversal (start
  99 / 10, `cmpwi rJ,0; subi rJ,1; bne` inner = combine's `(ge (plus j -1) 0)` -> `(ne j 0)`, outer `subi r28,r9,1 .. mr r9,r28;
  cmpwi r9,-1; bne` with the decrement and `n + 176` PRE'd to the outer body top). A `for (j = 0; j < 11; j++)` gets the NE form
  (`li 11; addic.; bne`) because its duplicated exit test sets `loop_info->vtop`; `for (i = 99; i >= 0; i--)` / do-while / `for (i =
  100; i > 0; i--)` are 33-99 words. The memset address must not be a giv of the counter (a call in the loop allows the reversal only
  with `no_use_except_counting`), hence the separate pointers. Left in both callbacks: the two PRE'd registers swap (`i-1` r28 /
  `n+176` r29 in CallbackLoad, the reverse in CallbackSave; ours the opposite of the target in each) -- 4 words each, not found
  (declaration orders, u32, s[0] / `char (*s)[16]`, `while (i-- != 0)` all identical).
- **`XmlStrToBool` is `if (strcmp(s, "true") == 0 || strcmp(s, "True") == 0) return 1; return strcmp(s, "TRUE") == 0;`** (int; the
  last term as a value = `subfic r0,r3,0; adde r3,r0,r3; cmpwi r3,1` with the two `||` terms jumping to `li r3,1`); the single `||`
  chain (bool or int, inline or written out) is jumps only (78), `(a || b) ? 1 : c` 43.
- **The fill loop indexes `d.node[i].s[k]` / `m->elem[i].x` directly (no `n`/`e` pointer locals) and the SAVE loop has `e = &m->elem[i]`
  INSIDE the body.** Two effects: (1) with the pointer locals the loop is 70 real insns at loop pass 2 and the "true" `lis` (savings 1,
  life 1) is hoisted (`threshold * savings * life >= insn_count`, threshold = (1 + n_non_fixed_regs) = 72 with a call in the loop);
  the direct forms re-derive every address (88 insns) and the `lis` stays in the loop like the target (16 words). (2) giv increments
  are emitted in the giv list order = reverse discovery, so the LAST address expression discovered (the `e` giv of the save loop's
  `&m->elem[i]` written after the strcmp/`d.node[i]` uses) gets the FIRST increment (`addi r30,r30,24` right after `addi r23,r23,1`);
  `for (..; i++, e++)` / `e++` in the body give e's increment the right slot but put its init before the entry test (target: giv init in
  the preheader). Save loop: `for (i = 0; i < 100; i++) { e = &m->elem[i]; if (e->flag & 1) {..} }` -- the biv i is eliminated
  (`cmpw r30,r28` SIGNED compare against `m + 2376`); a pointer loop `for (e = m->elem; e <= &m->elem[99]; e++)` compares `cmplw` with an
  entry test (62), `for (i..) m->elem[i].x` keeps i*24 as a second giv (64).
- **HDRead's buffer argument is a fresh `addi r4,r1,18000` in the target while r27 holds the same address (1 word, CallbackLoad and
  SubToolMessInit; the reverse in CallbackSave: `mr r4,r14; subf r5,r4,r5` where ours re-materialises).** Mechanism read in
  integrate.c: an inline argument that is `(plus fp N)` is copied to a `reg/v` pseudo by process_reg_param with a CONST_AGE_PARM
  equivalence, and `subst_constants` rewrites every VALID use to the address (`(set r4 (plus fp N))` for call arguments; stores /
  addresses keep the pseudo). Our `(set r4 (plus fp 18000))` is then folded by cse1 into the pseudo when the parameter copy is in the
  same ebb (EvtReadXml entered after the clear loop) or left alone when it is not (CallbackSave's HDWrite after the write loop). The
  target does the OPPOSITE in both places, so its parameter copy of `buf` is in another ebb for the read side and in the same ebb for
  the write side -- consistent with EvtReadXml's `buf` being bound at the function top (an inline entered before the clear loop that
  also owns `HDRead`) but every such structure moved the `d` copies / `name` substitution (58-65). Not closed; `char* const` params
  change nothing (TREE_READONLY is what integrate sets itself; only a SET of the parm reg clears it).
- **SubToolMessMove: `if (pEdit->GetCx() == 3)`, not GetCy** (vtable slot 16 = GetCx: column 3 is the MessNo column), `EventMessageData*
  m = t->pMess` local (`m->elem[no]`, `m->elem[j]`: the target loads pMess once), the back-search as `for (j = no - 1; j >= 0 && (p =
  &m->elem[j])->messNo == -1; j--) cnt++;` (the exit test duplicated at the entry = the peeled `mulli; add; lwz 16; cmpwi -1` copy;
  ours still eliminates j into a pointer compare `cmpw r9,r8` where the target keeps `addic. r11,-1` -- open), `EVT_MES_Y` as `336 -
  cMes.getWork()->lineSpace - cMes.getWork()->fontH - 1` (the `addi rX,cMes,4` base; our `fontH - lineSpace` order came out swapped),
  and a frame-only `EventMessageData::MessElem unused;` first local (tagged COMPILER-DIFF: 24 bytes below the fast-cast temps, frame
  136 -> 160 = the target; the CreateEditWindow `T unused` precedent). The mesCnt block is open: the target addresses `mesCnt[no+1]`
  as a register (&EvtDebug+196, `4(r28)` for [2], `0(r28)` for [1]) and `mesCnt[no]` through `lwzu r8,192(r30)`, then stores `stwx
  r27,r30,r27` / `stwx r0,r26,r27` / `stw r31,4(r26)` (no = 0 folded only in the third); `mesCnt[no+k]` in the eprintfs, `mesCnt[2] =
  i` literal, `s32* mc` bases: 105-107, never the mixed form.
- **`pMessTool` is a one-member struct global** (`static struct { cDbgToolMain<..>* p; } MessTool; MessTool.p->SetIsWorkAliveFunc(..)`):
  the target reloads the tool pointer after every Set*Func / callback-pointer store (`lwz r9,pMessTool; lwz r11,4(r9); stw; stw;
  lwz r9,pMessTool ..`), which only a non-scalar load (struct member) gives against struct-member stores (98 -> 86). SubToolMessMove
  reads the global per use (`MessTool.p->Update()`, `MessTool.p->Disp()`, `MessTool.p->pEdit->GetCx()` -- a local copy moves the
  tool pointer off r31 and keeps it live: 263). Two more SubToolMessInit facts: `CreateEditWindow(.., m->elem, 100, 5)` takes the
  function-level `m` (the target stores `stw r15,40(r31)` = pWork straight from m's register; `t->pMess->elem` reloads it and swaps
  the m/path registers, 86 -> 71), and the four callback-pointer stores are TWO helper calls, `MessSetSaveFunc(MessTool.p,
  CallbackSave, t)` / `MessSetLoadFunc(MessTool.p, CallbackLoad, t)` (inline `tool->pSaveFunc = f; tool->saveArg = arg;`): one tool
  read per pair (`lwz pMessTool; stw 64; stw 32; lwz pMessTool; stw 68; stw 36`), where the member-by-member stores reload the struct
  global after every store (71 -> 42; this also settled the clear loop's PRE-register pair in this function).
- **dbg_tool.h (owned by the tools closer, NOT edited): three findings from the t_event target, all confirmed against
  t_lightarea's ToolLightAreaMain too.** (1) `cDbgToolMain<T>::Update()` lays its arms out 0, 1, 2, 6, 3, 7, 4, 5?, 8: case 6
  (LoadData) directly after case 2 and case 7 (SaveData) after case 3 (t_lightarea target: `lwz 0x44(r22); li 6; Debug_alloc/HDRead;
  lwz 0x40; li 7; Debug_alloc/HDWrite; lwz 0x48; li 5`); with the header copy reordered SubToolMessMove drops 346 -> 106 (in-tree 274).
  (2) case 4's pad test reads a u32 at offset 0 of a symbol (`lwz r0,0(r9); andi. 512`), not `Joy[0].on` (+16). (3) `ret = 0` (case 5,
  `li r20,0`) is laid out at the very end, after case 8. The 11-word `cDbgToolMain<T>` ctor loop gap of SubToolMessInit and the
  template members (fn_1B528 24, execCopyWindow 3, LocalUpdate 2, cutBuffer 1, fn_1D370 2) are the same header items as in
  Tools/t_lightarea. Remaining in SubToolMessInit besides the header (the ctor's extra `stw r0,616(r31)` and the CreateEditWindow
  `mfcr r29 .. mtcrf` CR-kept null test that does not come out for MessElem): the HDRead buffer word and a `li r27,5 / addis /
  lwz t` issue-order triple before the CreateEditWindow ctor.
- .rodata reloc "diffs" (`_._t18cDbgButtonTemplate.. +0` vs `NoButtonUpdate_callback+0xc0`) and `__7ToolEvt`'s 1 word are the
  nameless template tail shifting with our .text size (+0x10); they go with the sizes.

### Tool RELs, db_mod pass 5 (t_esp 67 -> 68/75, Tools 55 -> 56/63, 409 -> 240 words in both; dbmod_p_info 71 -> 0 (three tags), dbmod_motion 11 -> 2 (zero code), dbmodDispModelName 100 -> 19 (one tag), dbModMotionMove 98 -> 90 (one tag); dbmod_locate 103 untouched; nothing flipped; 2026-09-11)

- Harness ~/.cache/dbmod5 (deleted): `mbuild.sh MOD SRC OUTDIR` (module cflags + strip_unused/fold_linkonce, `OUTDIR/db_mod.o`,
  `CC1DIR=` for a hooked cc1plus, 0.3 s), `mtryv.py FUNC v.py [--only A,B] [--apply N] [--keep]` (substring variants judged with
  `OBJ= tools/bytecmp.py MOD/db_mod`), `mrel.py MOD SYM [OBJ] [--all|--target|--ours]` (side-by-side objdump, function-relative
  labels, reloc immediates masked as `@R`), `mdump.sh MOD SRC OUTDIR -dX..` (cpp via wibo, then cc1plus with dump flags, dumps
  next to `OUTDIR/db_mod.i`), `sngcc/` = copy of tools/sn-gcc with a `GDBG=1` hook in global.c printing, per allocno in
  allocation order, `GORDER <fn> i: reg R refs N len L calls C pri P -> hard  used14-31 <regs_used_so_far> conf
  <hard_reg_conflicts> smpref <regs_someone_prefers>` BEFORE find_reg (the two bit strings are r14..r31). 111 not re-checked
  (nothing flipped; both db_mod objects rebuilt through the locked ninja).
- **Register-allocation facts read from global.c/local-alloc.c this pass (they decide every remaining permutation):**
  (1) `find_reg` pass 0 never allocates a hard reg for the first time: candidates are `regs_used_so_far` (call-used regs,
  `regs_ever_live`, and the regs local-alloc handed out) minus conflicts; only pass 1 takes a virgin callee-saved reg, in
  `reg_alloc_order` (31, 30, ...). So the first callee-saved global allocno does NOT get r31 when r30/r29 were used by some
  local qty somewhere in the function and do not conflict with it. (2) local-alloc never uses r31 (`find_free_reg` sets
  FRAME_POINTER_REGNUM = 31 in `used`), so a callee-saved value in r31 is always a GLOBAL pseudo and a chain like `subf r30,r11,r3;
  subf r30,r30,r3; add r30,r30,r0; addi r30,r30,24; slwi r3,r30,3` (all one register, then the shift into r3) is a LOCAL qty:
  `combine_regs` ties the output to a dying pseudo input only if that input is local (`reg_qty >= 0`) and the output has no qty
  yet; a hard-reg input (the call result r3) gives the output a qty with an r3 suggestion first, which blocks the tie
  (`addi r3,rX,24` = ours). (3) `update_equiv_regs` DOUBLES `REG_LIVE_LENGTH` of a pseudo set once to a constant/invariant
  (REG_EQUIV), halving its global priority; an asm-produced value has no REG_EQUIV and keeps its short length. (4) global
  priority = floor(log2(refs))*refs/len: one weighted ref more across a log2 boundary (63 -> 64, 31 -> 32) or a length change of a
  few insns flips neighbours; the `GORDER` hook shows the numbers. (5) local-alloc allocates with a lifetime extended by one insn
  on each side (`fake_birth/fake_death`, INSN_SCHEDULING) and only retries with the real lifetime if that fails, so two local
  qtys where one dies at the insn that births the other never share a register unless the whole class is exhausted.
- **dbmod_p_info 71 -> 0 (three `// COMPILER-DIFF` tags):** (a) the zero colour of both eprintf2 calls is ONE asm-produced value,
  `({ int z; asm("li %0,0" : "=r"(z)); z; })` as the 5th argument of both calls with `#line 2960` before each call (gcse hashes
  ASM_OPERANDS with their line number; only equal expressions are PRE'd together). Mechanism: gcse PRE inserts partially
  redundant expressions at the END of the preheader block in bitmap-index order = order of FIRST OCCURRENCE in the insn
  stream, and the 5th argument is evaluated after the four dbmodPinfo* highs (arguments 1-4), which is why `li r25,0` follows the
  four `lis`. An inlined `dbmodZero()` call as the argument is precomputed FIRST (`precompute_arguments` evaluates arguments
  containing a CALL_EXPR before the others when a stack argument exists), landing before the highs; a statement-expression
  without `#line` gives two different expressions (no PRE). cprop never folds the PRE'd reaching register (`regno >=
  max_gcse_regno`), so the target's zero was most likely `(set R E)` with E folded to 0 by cse2 (REG_EQUIV, doubled length ->
  lowest priority -> r25); no C spelling of E was found (`x - 6`, `y - 4`, `i - i` are folded by cprop/cse1/fold before PRE).
  (b) `register Vec* v asm("r26")` and (c) `asm("" : : "m"(dbmodPinfoLabel[j]))` at the else-arm top: with the asm zero's
  undoubled length its priority (3636) beats the label giv (2746) and v (2211); the pin puts v where the target has it and the
  `"m"` use adds one ref to the label giv (13 -> 16 weighted, 4507) so it is allocated before the zero (r27 above r25). A
  `"r"(&dbmodPinfoLabel[j])` use creates a second giv; the `"m"` form shares the DEST_ADDR giv; placed in the j == 0 arm it
  changes that arm's `lwzx r10,r18,r30` into `lwz 0(r27)`.
- **dbmod_motion 11 -> 2 (zero code):** the loop's `len` is the variable reused: `hs = strlen(motName[i]) - hashOfs[i]; name =
  ..; nlen = strlen(name); hs = nlen - hs; he = ..` (len/hs in ONE register r31: a global pseudo allocated late, when every
  used callee-saved reg conflicted, hence the virgin r31; with a separate `len` it is a local qty in r29 that conflicts with
  `j` and pushes j to r31), and the tail's chain is a fourth variable used ONLY there and reused through the chain: `len =
  strlen(..) - hashOfs[sub]; len = strlen(dbmodSkipPath(..)) - len; len += digits[sub] - digit; eprintf((len - 1 + 25) * 8, ..)`
  (a block-local `int l` is the same thing) -- a local qty, so local-alloc ties every step into r30 and the pDbModState high
  gets r29. Left (2): `lhax r9,r9,r11; cmpwi r9,-1` for the motNum test (ours r0): the loaded value's preferred class is
  BASE_REGS in the target (r0 excluded) and it shares the address temp's r9, which with (5) above means it is not a plain local
  GENERAL qty; `asm("" : "+b"(no))` on an `int no` gives r9 but moves the address temp to r11 (9 words), eleven spellings of
  the test (`+ 1 == 0`, `(s16) -1`, `< 0`, `~x == 0`, pointer forms) change nothing. Same residue in DispModelName (there the
  `+b` launder is exact, applied).
- **dbmodDispModelName 100 -> 19 (one tag):** (1) the bin/tex loops count with `i`, not `k` (the target's tail loop counter is
  r26 = the display loop's i; k stays r31 for the hash and type loops -- with k in all four loops k has 63 weighted refs and
  ranks below the k-loop givs). (2) `hs = strlen(motName[0]) - hashOfs[0]; ..; hs = nlen - hs;` (len/hs one register, as in
  motion). (3) no `cx`/`nx` variables: the cursor column is `(x - 1) * 8`, case 0/1's column `(x + 8) * 8`, case 1's row
  `(y + 1) * 14`, with `int x = 6; int y = 4;` at the declarations (x, y are multi-set -- reassigned for the BIN/TEX part -- so
  they live in stack slots and the loop-invariant `x - 1`, `x + 8`, `y + 1` are loop.c movables folded by cse2 in the preheader
  ebb to `li r14,5` / `li r15,14`, which is why those two `li` follow the hoisted highs; `int cx = 5` at the declaration puts
  them first, `cx = 5` anywhere in the loop-top block folds the cursor's `cx * 8`). (4) `int no = motNum[0]; asm("" : "+b"(no))`
  (COMPILER-DIFF) for `lha r9,98(r8)`. Left (19): `lis r11; addi r7,r11,"%s"@l` per use (ours r9 and the `lis` scheduled later),
  and nlen r25 / `li r24,184` (the `(23 + k) * 8` giv) swapped because the target hoists the "^" high in loop PASS 2 (after the
  giv init): its hash loop had >= 63 real insns at pass 1 (ours 58; four movables lower the threshold 71 -> 62 for the fourth,
  `62 * 1 * 1 < 63`). Five codeless asms at the hash-loop top reproduce the giv order but lengthen the pDbModState high's life
  (20 refs/720 -> 730) below the "^" high's (5/90 -> 98) -- the target has both at exactly 1111 (20/720 vs 5/90, tie broken by
  allocno number), so its extra pass-1 insns were gone by flow time (merged by cse2/combine); not applied.
- **dbModMotionMove 98 -> 90 (one tag):** `move` is not a variable: each arm tests and copies itself (`if ((flags & 1) &&
  (f2 & 0x10)) { if (model->mot.state & 3) { pos/rot copy } } else { if (~f2 & 1) { pos/rot copy } }`; jump2 cross-jumps the
  two copies and the two `beq` into the target's `andi. r9,r0,3; b L; L508: not r0,r9; andi. r11,r0,1; L: beq`), and tests 2-3
  read a copy of the u16 flags: `u16 f2 = flags; asm("" : "+r"(f2))` (COMPILER-DIFF: a plain `f2 = flags` is copy-propagated
  away by gcse, unlike IKreport's `flag = info`). Left (90): the copy's direction (`lhz r9; mr r0,r9` vs the target's `lhz r0;
  mr r9,r0` -- the loaded value is the short-lived one in the target), the `order[]` init (`addi r23,r1,8` first, n r24) and
  the swap loop's `i + 1` (target: computed separately in each arm, `mr r7,r5` after the search; ours PRE'd above the test),
  `!(em->xE38 & 1)` (`xori; andi.; beq` -- the empty-then nested if `if (xE38 & 1) {} else {..}` is 149), the dbModSlot end
  pointer (`addis r11,r9,10; addi r11,r11,-21076` = `&dbModSlot[64]` computed from the loaded pointer) and the trailing loop's
  register names.
- Not iterated: dbmod_locate 103 (pass 2's list still applies), the 24/1/1-word `fn_*`/`create`/`loadModel` rows (the .text
  size pairing artefact, gone when the sizes agree).

### Tool RELs, t_esp pass 12 (t_esp 208/212: InitTool 11530 -> 11057 words with the EDIT windows' `g_pPrimArray` load now after the `new` (zero code); the remaining residue = the window objects' alias status: the target compiles as if the `new` results carried no REG_NOALIAS -- diagnosed with a hooked cc1plus (8389 words, the entry's 10 callee-saved `&pos` and the 0x2930 frame exact), not a compiler difference (whole tree: 11 matched functions need it); Load/SaveEmType 2/2 untouched; db_widget DB_STRING ctor 7 unchanged, its flow.c/haifa mechanics pinned down; nothing flipped; 111 OK; 2026-09-11)

- Harness ~/.cache/tesp12 (deleted): `mk.sh UNIT SRC OUTDIR [-dX..]` (module cflags; `NATIVE_DIR=` picks another cc1plus dir; with dump
  flags it runs cpp + cc1plus by hand into OUTDIR), `tryv.py UNIT BASE V.py` / `tryt.py BASE V.py` (exact-substring variants; tryt prints
  InitTool words, frame, the entry block's callee-saved `addi rN,r1,C` set and the low-slot count), `lst.py target|OBJ [FUNC]` (plain
  listing, decimal offsets hexed), `ins.py DUMP FUNC`, `pseudo.py DUMP REG..` (set/use insns of a pseudo with its stream index), `occ.py
  OUTDIR LO HI` (callee-saved occupants of InitTool over a stream range, from -dl/-dg), `regstat.py OUTDIR` (global-alloc order and
  priorities of the `&fp+C` reaching regs; the -dg dump's order is the `;; N regs to allocate:` line and the hard regs are the
  `Register dispositions:` block), `sngcc/` (copy of tools/sn-gcc with an env-guarded `LADBG` fprintf after local-alloc's allocation
  loop and an env-guarded `NOMALLOC` in calls.c's special_function_p), `tree.py build LABEL [ENV=..] | cmp A B` (every prodg_cc unit of
  build.ninja with sngcc's cc1plus, bytecmp per unit, 18 s with 12 workers; 29 units whose build line wraps before `prodg_cc` are
  skipped in both labels).
- **InitTool: the EDIT windows read `g_pPrimArray` inside the helper (11530 -> 11057, zero code).** `CreateEditWindowN(TOOL_WINDOW*&
  slot, DB_PRIM_ARRAY* p)` evaluated the argument before the inlined body, so the `lwz g_pPrimArray` preceded `bl __builtin_new`,
  crossed the call and took a callee-saved register (`lwz r28,0(r27)` before the new, `mr r3,r28`); the target loads it after the
  `bl` like every other window's inlined ctor (`lwz r27,g_pPrimArray@l(r27)`, the dying high's register). Form: `CreateEditWindowN
  (TOOL_WINDOW*& slot) { EDIT_WINDOW* e = new EDIT_WINDOW(g_pPrimArray); ... DB_PRIM_ARRAY* pa_ = e->pa; ...}`.
- **InitTool: the spill SET / callee-saved `&pos` set is decided by whether the window object pointers are alias-exempt, and the
  target's are NOT.** Facts (o_t0 = the pass-11 source): global.c allocates the `&fp+C` reaching regs in priority order `&8 (r31),
  &20, &30, &40, &50, &60, &70, ..., &110, ...` (pri = floor(log2 refs)*refs/len ~ 26-28, ties by regno) but `find_reg` gives a
  register only if none of the already-placed allocnos overlaps: `&60` [61,1180] overlaps the second window's hoisted `T = R`
  copies (`reg/v 832.. = 12152..`, the loop-invariant `&pos` addresses of CreateEditWindow1's `for (i < 5)` body, hoisted by loop.c to
  the preheader and spread by sched1 as fillers through the MENU/EXIT/EDIT1 block; refs 5, len ~600, pri ~165 -> placed first in
  r14-r19/r31) which are born at #1115-#1205, so `&60`/`&70`/`&80` get nothing while `&110`-`&150` (dying at #1059-#1089, before those
  births) get r18-r14. In the target `&60` is r17, `&110` r28, `&150` spilled, and the MENU window's `p = g_pPrimArray` is r28
  (callee-saved although it crosses no call) with the ctor's `pa = p; win = NULL` stores issued FIRST after the `new`; ours had p in
  r0 and those stores last. Mechanism: `__builtin_new` is `is_malloc` in calls.c (name check), the result copy gets a REG_NOALIAS note,
  alias.c gives the pseudo a unique base, so the inlined ctor's `mem(e)` stores have no output dependence on the later fp-relative
  `pos.x/pos.y` (MEM_IN_STRUCT_P, so not exempt as fixed scalars) stores: sched1 priority 108 (call + 1) for `e->pa = p`, 110 for the
  p load (below the pool `lfs` at 111/112 whose `high` dies -> weight 0), p issued 5th after the call and after `e = r3`, life
  [88,116] -> r0 free -> r0; with the base unknown the stores chain into the pos stores (prio 111/112), p's load becomes prio 113
  = first after the call, born before `e = r3` (r3 busy), dying at `mr r3,p` after all of r4-r11/r0 are taken by the CNW arguments
  and pool highs -> local-alloc's first free is r28 (r31 never, r30 = e, r29 = const 0); the const 1 then takes r26, const 4 r25,
  high(pMenuWin) r24, const -1 r23, high(pExitWin) r22 (ours: 28/26/25/24/23 = one register higher each), and global.c then finds r21-r17
  free for `&20`-`&60`, r28 for `&110` (p-MENU's r28 is dead by then), r16-r14 for `&120`-`&140`, nothing for `&150`. Proof by the
  hooked compiler (`NOMALLOC=1`, is_malloc forced 0 for `__builtin_new`/`__builtin_vec_new`): InitTool 11057 -> 8389 words, frame
  0x2930 exact, the entry's callee-saved set `r31=&8 r21=&20 r20=&30 r19=&40 r18=&50 r17=&60 r28=&110 r16=&120 r15=&130 r14=&140`
  exact, the MENU window segment exact, the 882 `&pos` slot pairs the same C set (357 slots still +-4: one low copy fewer), the
  `Load/SaveEmType` first diff unchanged. BUT the whole tree with NOMALLOC=1 loses 11 identical functions (Sscrn ss_cap init,
  ss_file, ss_item, ss_main, ss_map, ss_pzzl, ss_shop, game/player, t_event, t_esp/db_widget AddPrimitive x2) and gains none: the
  original compiler DOES emit REG_NOALIAS for `new`, so t_esp's ORIGINAL SOURCE constructs its 40 windows in a way whose object
  pointer has no known base. Not found: (a) `void* operator new(unsigned)` declared inside `namespace t_esp_namespace` (DECL_CONTEXT
  != NULL would kill is_malloc) ICEs (378); declared globally it changes nothing; (b) a shared `TOOL_WINDOW* w` reassigned per
  window with `InitX(w, p)` helpers (multi-set pseudo -> base 0) makes `w` a whole-function global allocno and reshuffles everything
  (11201); (c) an asm launder of `this` in the ctors (`asm("" : "=r"(t) : "0"(this))`) gives the early stores but leaves `mr r9,r30`
  and `lwz r3,0(r30)` reloads (10674). Candidates left: the object pointer passing through a non-inlined boundary or a memory
  round trip whose load cse does not fold (a call between the store and the reload), a derived-to-base conversion with a non-zero
  offset (a second base class or a vptr class would change the layout -- the objects are 8/12 bytes, so no), or the `new` expression
  written so that the ctor runs on a copy (`TOOL_WINDOW* w = new TOOL_WINDOW; *w = TOOL_WINDOW(p)`-like temporaries). Whatever it
  is, it must (1) leave `bl __builtin_new; mr r30,r3` and the inlined ctor bodies as they are, (2) make every `mem(e)` store depend on
  the following fp-relative struct stores, (3) not add insns. Test recipe: compare the entry's callee-saved set printed by tryt.py
  against the target line above; NOMALLOC=1 with sngcc is the oracle for what the rest of the function looks like once the alias
  status is right (8389 words left there: spill offsets +-4 and the per-segment residues).
- **DB_STRING ctor (db_widget, 7 with the r0 pin, not closed; the two remaining mechanisms are exact):** (1) the pinned `li r0,0`
  is boosted to max priority in sched1 (birthing_insn_p: a SET of a call-used hard reg with REG_N_SETS == 1 -- flow counts hard-reg
  sets too), which puts `lis LC; lfs` adjacent and gives the four constants the target's names; (2) the same pin makes every later
  CALL_INSN add an anti link from `reg_last_uses[r0]` (sched_analyze's call loop adds the links but never clears reg_last_uses; only a
  SET of r0 does), and LOG_LINKS survive into sched2, so `stw r0,str/len` have 5 dependents (129, strlen, strcpy, `mr r0,r3`, new)
  where the target has 3 and are sorted (equal priority 12, class 3, then depend count, then LUID) before the colour/type stores
  (5) and the vptr store (4) instead of last. Every way to clear the list needs a second r0 SET before strcpy in sched1, which
  costs the boost (REG_N_SETS 2) and the names: `asm("" : "=r"(zero))` + a non-volatile use (`asm("" : "=m"(max) : "r"(zero))`)
  14-15, the `new[]` result pinned to r0 (cse forwards r3, the `mr r0,r3` vanishes: 6 words, size -4), `asm("" : "=r"(p) : "0"(q))`
  keeps the `mr r0,r3` but loses the boost (11), a clobber does not clear uses (14-16). Anchors are out: an asm with `"=m"(str),
  "=m"(len)` outputs is deleted by flow (its outputs are in `mem_set_list` because the identical zero stores follow), a `"=m"(max)`
  output survives but raises the vptr store's priority through the true dependence (12-15), and an asm without outputs is
  volatile (`asm_operands/v`, a full barrier: 18). Unpinned, local-alloc's qty order (LADBG) is vt-hi+lo (tied on the elf_low
  insn, 8 refs / life 12 = 6666) before LC (2/4 = 5000) and type (2/4) before zero (3/24 = 1250) -> vt r9, LC r11, type r0, zero
  r11; every statement order (10 more tried, including the DB_PRIMITIVE-ctor style `int iz = 0` local, `len = 0; str = 0;` and a
  two-set `char* p = 0; ...; p = new char[max]`) leaves that arithmetic (11). The sched1 stream order that the target's sched2 needs
  is ca, type, cr, cg, cb, [vptr], str, len among the stores, i.e. colours before type in the source and `len = 0; str = 0;` (str
  the dying store) -- with the pin the r0 stores go first anyway (4 sched1 dependents vs 2). A fourth zero ref through a codeless
  asm does not change the qty order either. Left as the pin (7).
- Not iterated: Load/SaveEmTypeUpdateCallback 2/2 (pass-10 mechanism stands: which of loop-2 TOP / the last `lhz t` block cse1
  reaches with the outer high), fn_t_esp_3DE4C 24 (another owner).

### Tool RELs, t_snd_vol final / t_id pass 2 (t_movie/t_snd_vol 26/27 -> IDENTICAL, edit_reverb_param 135 -> 0 (one pin), flipped, t_movie.rel verify OK; t_id/t_id 52/69 -> 55/69, 1155 -> 763 words, .text gap 0x64 -> 0x28 (only idEditPos left size-different): toolIdDrawSafeZone 31 / toolIdPaste 45 / idEditTrans 176 -> 0, toolIdOption 146 -> 75, idEditColor 201 -> 132, both size-exact; 111 OK; 2026-09-11)

**Harness.** `~/.cache/tools_sv3/h.py` (deleted): `build MOD/UNIT SRC OUT.o [-dX]` (cpp + cc1plus by hand, dumps beside OUT), `try MOD/UNIT FUNC VFILE [--apply NAME] [--keep]` (VFILE = `V = [(name, [(old, new), ...])]`, unique-substring edits, prints the function's bytecmp word count and .text size per variant), `sbs MOD/UNIT SYM [OBJ] [--all]` (objdump side by side, branch targets and relocated immediates masked). Private objects must be named `<unit>.o` (ngccc derives the unit from the stem). Ten variants compile and judge in ~2 s, so the ~20 min zero-code search is 50-100 variants.

**t_snd_vol edit_reverb_param (135 -> 0).** Two mechanisms.
- 123 words: the pin. `efx_param_move` (the +=/-= switch helper) received `p = &work->efx[0|1]` as a parameter; the target allocates `work` before `p` (`work` r11, `p` r10, then `Joy` reuses r10 after `p` dies). `register SndEfxParam* p asm("r10")` at the four call sites (`// COMPILER-DIFF: pin`). Pinning `p` in the clamp blocks (the other `SndEfxParam* p` locals) does nothing — only the helper's parameter matters.
- 12 words, zero code: the helper had to become a **macro**, not an inline function. integrate.c copies an inlined function's pool loads (`lfs f0, 0.01f`) without RTX_UNCHANGING_P; `anti_dependence` then no longer returns 0 for them against the arm's `stfs`, the depend count of the pool load rises, and sched2's depend-count tie-break puts the pool load after the field load in the cross-jumped `+= 0.01f` arms (target: pool load first). Same statements written as a macro keep the flag (rule: when a cross-jumped arm's pool load and field load come out in the wrong order and the code came through an inline function, try a macro). `asm volatile("")` or an `asm("" : "+f")` launder in the arms breaks jump2's cross-jumping (247/332 words) — do not.
- The stereo panel's `col = work->x29 == 1 ? cursorCol[...] : 0xFFFFFFFF` form: a **register temp** (`u32 c; if (...) c = ...; else c = 0xFFFFFFFF; col = c;`) gives the target's li/mr chain + single store; storing into the field in each arm gives two stores. Same rule as the DOL clamp temps.

**t_id pass 2 (per function).**
- toolIdDrawSafeZone 31 -> 0: `if (w->drawSafe == 0) return;` before the `Vec a[4]/b[4]` initialisers (the target tests before the array copies; with the test after, the copies are scheduled above it).
- toolIdPaste 45 -> 0: declaration order `int cnt; int level0;`, loop `for (lv = 0; lv <= 7; lv++) { c = idClip; cnt = 0; for (i ...; i++, c++) { if (c->be_flag != 0xFF && lv == c->level) ...`, and the overflow message through `pLog.p->err(...)` (the inline `operator->` form keeps `pLog.p` live across the call and changes the allocation).
- idEditTrans 176 -> 0: clamp as a temp (`v = v < 0 ? 4 : (v > 4 ? 0 : v); d->transType = v;`), the ON/OFF display as **two eprintf calls in if/else** (jump2 cross-jumps the shared tail; a `cond ? "ON-/---" : "---/OFF"` argument gives one call with a selected pointer), `int c = 7; if (j == d->transType) c = 0;` before the loop eprintf, and one pin: `register int step asm("r11") = (joy->on & 0x100) ? 10 : 1; // COMPILER-DIFF: pin` in cases 2 and 4 (the target allocates `step` before `joy`; pinning `joy` to r10 instead disturbs local-alloc, 20 words).
- toolIdOption 146 -> 75 (size exact): function-level assignment order `cx = 0x23; r0 = 0xB; eprintf(...); sx = 0x2E; r1 = 0xC; r2 = 0xD; mx = 0x22; vx = 0x2F;`, case 1/2 as two-call if/else, and the language list with `(u8) col` at function scope (block-local `int mx/vx/sx` inside the loop body get cse-folded to `li r3,272`, worse). Remaining 75 = global-alloc order of `w/i/col` (target r31/r30/r29, ours r29/r31/r30) plus the preheader constant order: the target's `sx/r1/r2` appear as loop pass-1 hoists after `i = 0` and the PRE'd `lis` highs, `mx/vx` after the giv init like pass-2 hoists.
- idEditColor 201 -> 132 (size exact): `Draw_tileI(..., (int) (*pc / 255.0f * 100.0f), ...)` with the **implicit** u8 -> f32 conversion (explicit `(f32)` of a u8 load gives psq_l under -mfast-cast; the implicit one gives the classic xoris/stw/lfd/fsub double trick, which is what the target has), and the case-1 loop as `if (i == 0) col = 5; else if (w->subCur + 1 == i) col = 4; else col = 0;` with the inner `col = 7; if ((j != 0) != ((d->x109 >> 2) & 1)) col = 0;`. Remaining 132 = `onOffName3`'s address hoisted to the outer preheader in ours (target keeps `lis r9; addi r25` inside `if (i == 2)`), biv `j` eliminated in ours (target keeps the `j` counter r29 and a `j*4` giv r27), and register renumbering downstream.
- ToolInterfaceDesign 41 (unchanged): the target has **no interblock motion** in the `while (1)` body (its `addi r3,toolIdSys`, `li r0,0`, `li r0,16`, `li r3,1` stay in their blocks); ours hoists them to the block after the `blrl`. A variant with two extra blocks in the body (the pause toggle as if/else) reproduces the target's schedule from `bne` on but costs 4 bytes -> the original's loop had >= 11 blocks or a nonregular CFG (haifa `too_large` MAX_RGN_BLOCKS 10, or `unreachable`/`is_cfg_nonregular` -> single-block regions). `asm volatile("")` at the loop top, `if (0) break;`, `return;`/`TaskExit();` after the loop and do/while all compile to the same 41. Exceptions are off (no call-split blocks). Not found.
- toolIdInit 16, toolIdEdit 58, idEditUnit 79, idEditId 12 untouched.
- idEditPos 334 (size 0x10d4/0x10ac): mixed store bases for `*pos = d->vtx[N]` (`stw r10,280(r26)` via `d`, the others via `pos`), the clamp temp form for `a`, per-branch `lis r18/r17` hoists, register naming. Too large for this pass.

**Flags.** `"t_movie/t_snd_vol.cpp": True` added to MATCHING. t_id is not Matching (55/69).

### Tool RELs, cDbgToolMain ctor boundary (Tools/t_esp_area ToolEspArea 438 -> 399 (size 0x1c4c vs 0x1c50), Tools/t_lightarea ToolLightAreaMain 393 -> 462 words but .text size now EXACT (37/38, fn_Tools_30410 59 -> name-only), t_event SubToolMessInit 145 -> 16; one header change (include/dbg_tool.h), db_toolbase stays IDENTICAL; nothing flipped; 2026-09-11)

Harness ~/.cache/tools_ctor (deleted): `htry.py v.py` built the three units against a copied include/ with header/source
edits and judged each with `OBJ=.. bytecmp.py` (3 s per variant); `sbs.py MOD/UNIT SYM OBJ` normalised side-by-side;
`hdump.sh MOD/UNIT INCDIR -dX ...` with `SRC_OVERRIDE=` for the swapped-signature sources (`-fsched-verbose-5` prints the
per-insn prio/cost/deps table and the ready lists). All three units share the inlined `cDbgEditWindow<T>` ctor, so every
item below is in include/dbg_tool.h and was verified on all four includers after each change (`rg -l dbg_tool.h src include`:
t_event.cpp, t_lightarea.cpp, t_esp_area.cpp, db_toolbase.cpp; db_toolbase.h only mentions it).

- **ninja does not track include/dbg_tool.h** (`ninja -t deps` is truncated, "premature end of file"); after a header edit
  remove the four objects (`/bin/rm build/G4BE08/src/{Tools/t_esp_area,Tools/t_lightarea,t_event/t_event,Tools/db_toolbase}.o`)
  before the flock'd ninja, otherwise "no work to do" judges the old objects.
- **Target ctor shape (all three units):** block A after `bl strlen`: `li r0,0; li r9,1; li r11,n` then the 17 constant stores;
  block B: `lis r9,"00"@ha; lis r11,NoButtonUpdate@ha; cmpwi edit,0; stw r0,pSetWorkNo; stw pTop; addi; stw; addi; stw;
  li i,0; stw; mfcr r29; stw; addi rX,r1,184; lis Joy`; loop `mr r5,i; mr r8,i; mr r3,edit; li r4,0; mr r6,label; li r7,0;
  li r9,0; mr r10,cb; bl AddButton`; after it `stw edit,4(tool); mtcrf 128,r29; bne err`. `cmpwi edit,0` is gcse block-PRE
  of `(compare edit 0)` (the `edit == 0` test after the loop) inserted at the end of the pre-loop block; it needs a basic-block
  boundary between the `new` result copy and the loop head at gcse time, and that boundary must be gone by flow1 (the zero is
  block-local -> r0 from local-alloc; the `1` r9, `n` r11 follow in REG_ALLOC_ORDER).
- **The boundary construct (`if (n > 128) n = 128;` after the `do {} while (0);`, tagged dead test):** cse.c
  `cse_end_of_basic_block` stops the cse1 EBB at a NOTE_INSN_LOOP_END not followed by a label (`! after_loop`), so cse1 never
  sees the constant `n` for a test placed after the do-while and the jump reaches gcse; cse2 (after_loop) / gcse cprop fold
  it, the never-taken jump is deleted, the post-cse2 jump pass drops the label, flow1 sees one block. The tested parameter must
  be WRITTEN in the ctor: integrate.c substitutes the constant actual for an unmodified formal and the test folds at expand
  (`if (n > 128) return;`, `&&` forms, tests on `rows`/`num` memory: no boundary or real code). `if (nRows > 128) nRows = 128;`
  also works but then `n` (not rows) is the constant hoisted above strlen. A test placed BEFORE the do-while is folded by cse1
  (435/389/38 words: no boundary).
- **CreateEditWindow parameter order `(int wx, int wy, T* work, const char* name, u32 nRows, u32 n)` (call sites reordered in
  t_esp_area.cpp:237, t_lightarea.cpp:382, t_event.cpp:1186):** the inline entry copies the actuals into pseudos in formal
  order and sched1 breaks priority ties by LUID: `lis work` issues before `lis name` (target `lis r28; lis r30; addi r30;
  addi r28`), and the rows constant (lower LUID than n) is the one issued above `bl strlen` (`li r29,5` between `stw pName`
  and `stw wy`). The ctor's own parameter order is irrelevant (its formals reuse the caller's pseudos).
- **`asm("")` before the test (tagged codeless sched barrier): block B's issue order.** Ours had `stw r0,pSetWorkNo` FIRST in
  block B, target `lis; lis; cmpwi; stw pSetWorkNo; stw pTop; addi; ...`. Read off -dS with `-fsched-verbose-5`: haifa attaches
  LOOP_BEG/END notes to the next insn as a full barrier (both sched passes); after reload the first insn after the notes was
  the pSetWorkNo store (sched1 puts the dying-zero store first by INSN_REG_WEIGHT; the PRE copy `high(esp_area_work)` that
  preceded it is a REG_EQUIV init deleted by reload), so sched2 issued it alone in cycle 1. The target's order is exactly the
  issue-rate-2 (750) schedule of the region with NO real insn forced first: cycle {lis, lis}, {cmpwi, stw764}, {stw744, addi},
  {stw, addi}, {stw, li}, {stw, mfcr}, {stw, addi r1,184}, {lis Joy}. A volatile asm (`asm("")`, no outputs => volatile) is a
  sched barrier both ways (haifa `ASM_OPERANDS && MEM_VOLATILE_P`: all-regs deps + `reg_pending_sets_all`) and emits nothing,
  so it takes the notes' slot at sched2. It must sit BEFORE the dead test: gcse's `insert_insn_end_bb` puts the block-A
  insertions (`cmpwi edit,0`, `addi r1,184`, `lis Joy/pPL/LC`) right before the jump, i.e. after the asm; with the asm after
  the test they land before the barrier and `cmpwi`/`mfcr` move into block A (447 words). Results: t_esp_area 407 -> 399,
  t_lightarea 474 -> 462, t_event SubToolMessInit 27 -> 16 (its ctor region is now identical; the 16 are before `new`).
- **Remaining ctor residue: `wx`/`rows` r28 vs target r29 and `work` r29 vs r28 (2 x 5 words per unit).** local-alloc
  (`QTY_CMP_PRI = floor_log2(refs)*refs*size*10000/(death-birth)`, birth/death = 2*insn_number in the sched1 output, notes not
  counted, lo_sum output combined with its dying high input: work qty = 4 refs) gives name 9523 (r30), work 320000/48 = 6666,
  wx 80000/12 = 6666 -- an exact tie broken by qty number (work born first) -> work r29, wx r28, rows reuses r28. The target
  needs wx above work: one insn less between `li wx` (issued at t=3 with `li r3,772`) and `stw wx`, or one more between
  `lis work` and `stw pWork`, or a third wx ref. Not found: swapping x/y store order, wy/wx or work/name-first parameter
  orders in either signature (401-456), `asm("" : "+r"(wx))` launders (wx becomes post-call r0; 406-413), `+r`(work) (409),
  update_equiv_regs does not move a `REG_EQUIV const` init inside a block (`REG_BASIC_BLOCK >= 0`), REG_N_REFS is loop-depth
  weighted per BLOCK HEAD only (flow.c `calculate_loop_depth`), so the do-while notes inside the block do not weight the refs.
- **Not the mechanism (each built and read):** memory-based tests (`rows`, `num`: real code, 445), two-condition register tests
  with both params modified (block B zero separate but the loop entry test `0 < rows` no longer folds, 447), the six stores
  inside a `do {} while (0)` (406-445), a `z` pointer local for the zeros (unchanged), chained `pTop = pBottom = .. = 0`
  (type error across the function-pointer members), label/cb declared before the stores (unchanged / 476).
- **Tool-body residues left (t_esp_area 399 = 10 ctor + ~389 body; t_lightarea 462; same causes):** `tool` pointer r23 vs r22
  with the `addi r1,184` local taking the other (global.c allocno order; tool 27 refs / 4326 insns), `lis r19/r20`, the
  main-loop `mr r8,r30` + `mr r3,r8` (target keeps the edit pointer in r8 across the switch; ours reloads `mr r3,r30`), `lwz
  r11,4(tool)` vs ours `lwz r11,12(r1)` (target re-reads pEdit from the tool struct where ours has a spill slot), and the
  cDbgToolMain::Update case-3 `pSaveFunc` block placed ~0x90 earlier in ours (jump1 block layout) -- all downstream of the
  tool body, not of the ctor. fn_Tools_30410 (0x7cc, after LocalDisp<LIGHT_AREA>, calls strlen/FindButton/Joy) is
  cDbgFileSelectWindow::LocalUpdate (nameless in our object; fn_Tools_2F2D8 0x3b8 / fn_Tools_31174 0x98 its Init/dtor): its
  59 words were the 12-byte .text shift, gone now that ToolLightAreaMain is size-exact.

### Tool RELs, t_esp pass 13 (t_esp 208/212: InitTool 11057 -> 8389 words = the pass-12 NOMALLOC oracle exactly, frame 0x2930 and the entry's callee-saved set `r31=&8 r21=&20 r20=&30 r19=&40 r18=&50 r17=&60 r28=&110 r16=&120 r15=&130 r14=&140` exact, all 50 inlined windows in the target's `mr rX,r3; stw rZ,4(rX); stw rP,0(rX)` shape; the alias base is removed by a tagged class-scope `operator new` bound to the `__builtin_new` symbol, no natural source form found; Load/SaveEmType 2/2 and fn_t_esp_3DE4C 24 untouched; nothing flipped; 2026-09-11)

- Harness ~/.cache/tesp13 (deleted): `mk.sh SRC OUTDIR [-dX..]` (module cflags, wibo cpp + native cc1plus + NgcAs; `CC1=` picks
  the hooked sngcc), `gen.py [names]` (probe variants of a MENU/EXIT two-window InitTool from tmpl.h macros, judged by
  `depchk.py SCHED_DUMP FUNC`: for every `(set (mem (reg X)) ..)` store whether the defining r3 copy carries REG_NOALIAS and
  whether REG_DEP_OUTPUT links to the following fp-relative stores exist = the target's behaviour), `rtlseq.py DUMP N` (the N
  insns after each `__builtin_new` call, per pass), `tryt.py SRC LABEL` (full-unit variant: bytecmp InitTool with `OBJ=`, frame,
  the entry block's `addi rN,r1,C` callee-saved set against the oracle line, the early-store shape over the 51 `bl
  __builtin_new`).
- **Applied (tagged, `// COMPILER-DIFF` on TOOL_WINDOW): `static void* operator new(unsigned n) asm("__builtin_new");` in the
  common base.** calls.c special_function_p (546-623) sets is_malloc for `__nw`/`__builtin_new` only when `DECL_CONTEXT
  (fndecl) == NULL_TREE && TREE_PUBLIC (fndecl) && IDENTIFIER_LENGTH <= 17`; a class-scope allocator has DECL_CONTEXT = the
  class, so expand_call (2349-2375) emits no REG_NOALIAS on the result copy, alias.c record_set never gives the pseudo a unique
  ADDRESS, init_alias_analysis leaves its base 0 (`find_base_value (reg 3)` -> reg_base_value[3], wiped at the first r3 set),
  and every `mem(this)` store gets an output dependence on the later `pos` stores. The asm label keeps the call `bl
  __builtin_new` (the symbol is printed from the `*`-prefixed assembler name; the module's own `__builtin_new` definition binds
  it). Every other insn is the same as before: build_op_new_call -> build_method_call of the static member, args `(size)`,
  `copy_to_reg (valreg)` path instead of the is_malloc temp, the ctor's `this` chain collapses in cse the same way. Result =
  the oracle to the word (8389, size 0xa148 vs 0xa20c: the residue is the pass-11 spill-slot order/offsets of the 880 hoisted
  `&pos` address pseudos -- 4570 offset mismatches and ~1900 unaligned `stw rX,slot(r1)`/`addi rX,r1,C` pairs, the entry block
  cycling r8/r10/r0 where ours cycles r0/r10 -- plus the per-segment residues; nothing window-shaped remains).
- **Why no natural form: every way a pseudo loses its REG_NOALIAS base was read and probed (stock cc1plus, gen.py):**
  - the base is lost only by (a) `record_set` seeing a second set of the same pseudo in stream order (`reg_seen`, any
    position, or a CLOBBER after the noted set), (b) the noted insn not being a plain SET with the note (combine merged it:
    distribute_notes 11468-11483 drops REG_NOALIAS unless from_insn == i3), (c) the store base being a pseudo whose set has a
    base-less source (MEM, hard reg without note, ASM_OPERANDS). Forms tested and their route: base / `::new` / `new X[1]`
    (`bl __builtin_vec_new`, same name list, plus an expand_vec_init loop with labels: free and wrong shape) / `(u32)`,`(void*)` casts (find_base_value passes through NOP moves: no
    insn at all on 32-bit) / `MENU_WINDOW* m = new ..; m->Init(p)` (integrate.c 1540-1561: `this` is TREE_READONLY so no arg
    copy, process_reg_param maps `this` straight onto a REG_USERVAR_P pseudo -- the stores DO use the user variable, but it has
    one set: cse's prev-rewrite (cse.c 7965-8010, needs the r3 copy immediately before and the dest canonical) turns `temp =
    r3 [NOALIAS]; m = temp` into `m = r3 [NOALIAS(temp)]`, note kept) / slot through the global (`g_pX = new X; g_pX->Init(p)`:
    cse forwards the store to the load, no MEM base) / inlined helper parameter / reference-slot helper / derived-pointer
    helper / placement new on a `::operator new` result / a base-class or member inline `operator new` wrapper calling
    `::operator new` (the inner call is the builtin decl: is_malloc again): all keep the base (depchk `NOALIAS free`).
  - forms that lose it and why they are not the target: `if (m) m->Init(p)` (combine folds the r3 copy into the compare ->
    `mr.` PARALLEL, note dropped -- adds a branch); a ctor with a branch inside (the copy survives cse because `this` is
    canonical, but combine's two-SET parallel split copy-propagates the temp into every use and gcse cprop does the same
    across the label: base kept, so `free`); a shared `TOOL_WINDOW* w` reassigned per window (two sets -> base 0 only when
    `w` is canonical, i.e. used across a label: then it is a whole-function allocno, pass 12's 11201; in one block cse
    substitutes the temps and the base is kept); `X* m = NULL; ..; m = new X; m->Init(p)` loses the base only when the zero
    set stays live because cse reuses `m` as the block's canonical zero register (prevar probe: second window only) -- in
    InitTool the SI zero is born at the top (`li r29,0`) and the QI zero for the `stb`s is a different pseudo, so a
    pre-initialised window pointer's zero set is dead and flow deletes it.
  - declarations that cannot change is_malloc: a namespace-scope `operator new` ICEs (cp/decl.c 4158, assert 378 in pushdecl:
    IDENTIFIER_GLOBAL_VALUE (`__nw`) is the builtin); a global `static void* operator new(unsigned)` is emitted `.globl`
    (TREE_PUBLIC stays 1, is_malloc 1); a user global `operator new(size_t)` mangles to `__builtin_new` (cp/method.c
    1633-1646, one-parameter non-method special case) and its DECL_NAME is `__nw`, in the list; `__attribute__((const))` on
    it takes the `is_const` branch first (2349, libcall block, REG_EQUAL, and cse would merge the equal-size calls); an
    indirect call through a function pointer gives fndecl 0 but `bctrl`.
- The entry block still differs in schedule (target: `li r29,0; stw r29,g_pTexRender`, `g_pEditSeq2 = &g_editSeqWk2` first,
  then the `addi/stw` spill pairs cycling r8/r10/r0, then `li r0,0` + the three `stb`; ours: the `stb` zeros first, the SI
  zero at the end of the pairs) -- part of the pass-11 spill-slot residue, not touched.

### Tool RELs, t_id pass 3 (t_id/t_id 55/69 -> 57/69, 763 -> 615 words, .text gap 0x28 unchanged: idEditId 12 -> 0 (one anchor + dead sets), toolIdEdit 58 -> 0 zero code, idEditUnit 79 -> 15 (dead sets + one pin), toolIdOption 75 -> 65, toolIdInit 16 -> 12; not flipped; 2026-09-11)

Harness `~/.cache/tid3/h.py` (deleted): `build SRC OUT`, `dump SRC OUT -dX` (cpp via wibo + cc1plus by hand), `try FUNC VFILE
[--loop] [--keep] [--apply]` (VFILE defines `V = {name: [(old, new), ...]}` substring edits of src/t_id/t_id.cpp; `--loop` also
prints the function's `Loop from ..: N real insns` counts of both loop passes), `sbs SYM [OBJ] [--all]` (objdump side by side,
relocated immediates masked). `BASE=<variant.cpp>` stacks variants.

**Mechanisms read this pass (GCC 2.95, all confirmed on the dumps).**
- **local-alloc tie order / FP chains** (idEditId): `(set D (plus C K))` ties D with operand 1 (C) first, then the constant.
  The target's `fmuls f0; fdivs f0; fadds f13,f0,f13` (result tied to the 0.5 constant, chain in f0) needs C NOT to be a local
  qty: ONE `f32 s` variable for both dimensions (`s = (f32) size->w; d->sizeX = s; s = (f32) (int) size->h; s = s * 480.0f;
  s = s / 448.0f; d->sizeY = (f32) (int) (s + 0.5f);`) has two deaths -> global pseudo (f0, also the psq_l result), the
  `* 480` and `/ 448` are sets of s (a `s * 480.0f / 448.0f` temp ties to the 480 constant instead), the `+ 0.5f` temp ties to
  the constant and the DF float_extend of the fctiwz input joins that qty (a qty that already contains a DF reg cannot take
  the SF->DF extend: `usize < qty_size` -> the `fmr f13,f0` of the one-variable forms).
- **cse's copy swap** (idEditId, idEditUnit): `y += 0xE; yy = y;` -> cse.c "Special handling for (set REG0 REG1) where REG0 is
  the cheapest" rewrites it into `yy = y + 14; y = yy` when yy's REGNO_LAST_UID is beyond the block and later than y's
  (make_regs_eqv). The target keeps `addi r25,r25,14` in place and inits yy with a `mr r31,r25` placed AFTER the pass-1
  hoisted highs: yy is a **giv** (`yy = y + i * 0xE` in the body), its init is emitted by strength_reduce after
  move_movables pass 1. A dead `y = yy;` after the loop also prevents the swap but leaves the init early.
- **loop.c move_movables threshold is 71** (`1 * (1 + n_non_fixed_regs)` with calls), minus 3 per moved insn; a `high` used
  once (savings 1, lifetime 1) is hoisted in pass 1 only while the running threshold >= insn_count. idEditId: ours 61 insns,
  the target keeps the third high ("%02X") for pass 2 -> insn_count >= 65 at loop time; idEditUnit: pass 2 at 67 insns still
  hoists ">"/"YES/---" (threshold 71 -> 68 -> 65), the target hoists neither -> >= 72. Both need exactly **+5 insns** at
  loop.c time, like idEditMark (`col`/`d` dead sets, tagged `COMPILER-DIFF: 3`). Zero-code forms that do NOT add insns:
  `switch (i)` for the value arm, `if/else` col forms (jump1 normalises them before cse), `y + i * 0xE` in the arm (cse folds
  it with the jump equivalence), dead statements (cse1's delete_dead_from_cse), `yy` dead sets (they make yy a multi-set biv
  and break the giv). The original construct is still unknown.
- **global.c priority tie w vs i** (idEditId): w 19 refs / 175 insns = 4342 vs i 15 / 104 = 4326 (`floor_log2(refs) * refs *
  10000 / live_length`); the target has i (r30) before w (r29). One extra insn in w's live range flips it: `asm("")` at the end
  of `case 1` (`COMPILER-DIFF: anchor`); the same asm in the preheader or the case-1 top changes the schedule (5-8 words).
  `register int i asm("r30")` is worse (54: a hard-reg biv is not reduced). Dead statements do not count (deleted before
  flow counts).
- **sched1 store order = REG_WEIGHT** (toolIdEdit, toolIdInit): stores whose source register dies (weight 0) go first in LUID
  order, then the rest. toolIdEdit's `editMode = 2; editStep = 0; subCur = 0` arm comes out `stb 2; stb 3; stb 11` only from
  the source order `editMode = 2; subCur = 0; editStep = 0;` (the shared QI zero dies at editStep). toolIdInit's 11-store
  block cannot be produced by this model with five shared constants (the first six target stores hold four QI-zero stores):
  the constants must be live after the block there (tested: with the five constants kept live the order becomes source
  order) -- the later use is not found (call args are hard-reg sets; `IdBufAlloc`'s 1 is SImode).
- **toolIdEdit 58 -> 0, zero code, four readings**: (1) `else if (w->editSel != 0) { ... } else { idEditNo(...); }` (the
  idEditNo call is out of line: the then-part falls through); (2) the `trg & 0x100` arm has NO `subCur = 0` (its `b` goes to
  the function end, not into the 0x800 arm's tail); (3) `editSel` 2 is **idEditSize** and 3 is **idEditPos** (editDispFunc
  order; our source had them swapped), arms written `case 3` then `case 2`; (4) each of the six arms repeats `r = f(...);
  if (r == 0) w->editMode = r; w->focus = 0; break;` -- jump2 cross-jumps the identical `mr. r3,r3; bne; stb r3,2; li 0;
  stb 94` tails, while a shared `goto sub` block gets `cmpwi r3,0` (the copy `r = r3` sits in the arm, combine can only fuse
  copy + compare in one block). Case 1 is `r = idEditId(...); if (r == 0) w->editMode = r;` (tail shared with case 2's
  `stb r3,2`), case 2 stores `editMode` before `editStep`.
- **toolIdInit 16 -> 12**: `w->level = w->x24 = 0;` (level's zero is the SImode one). Left 12: the store order (above) and
  the 1/lang registers.
- **idEditUnit 79 -> 15**: giv form + 5 dead `d` sets + `register JOY* joy asm("r11")` (`COMPILER-DIFF: pin`; unpinned joy
  r10 / editStep value r8, target r11 / r10). Left 15: the `w->editStep` value r10 vs r9 and its `extsb` temp r0 vs r10,
  three `lbz/stb` pairs r9/r0 swapped.
- **toolIdOption 75 -> 65**: `sx = 0x2E; r1 = 0xC; r2 = 0xD;` moved INTO the loop body (loop.c hoists the three `li` after
  `i = 0` and the PRE'd high, as the target). mx/vx must land after r1/r2 (pass-2 position): assigning them in the body top
  (103), inside their arms (96, cse folds `mx << 3`), or leaving only them outside (65) -- not found. Left also the w/i/col
  callee-saved permutation (r31/r30/r29 vs r29/r31/r30).
- ToolInterfaceDesign 41, idEditColor 132, idEditPos 334 untouched (see pass 2 for their mechanisms).

**Flags.** Nothing flipped; t_id/t_id stays False. `ninja -k 0` + shasum = 111 OK after the pass.

### Tool RELs, db_mod pass 6 (t_esp 68 -> 74/75, Tools 56 -> 62/63, 240 -> 19 words in both; dbmod_locate 103 -> 0 (zero code), dbModMotionMove 90 -> 0 (zero code, pass-5 tag removed), dbmod_motion 2 -> 0 (one tag, asm-emitted `lhax`); dbmodDispModelName 19 untouched (two residues, both explained, not reproduced); nothing flipped; 2026-09-11)

- Harness ~/.cache/dbmod6 (deleted): pass-5 layout (`mbuild.sh`, `mtryv.py FUNC v.py [--only] [--apply] [--keep]`, `mrel.py MOD
  SYM [OBJ] [--all]`, `mdump.sh MOD SRC OUTDIR -dX..` -- run both .sh with `sh`, pass ABSOLUTE paths (mdump cd's into OUTDIR),
  `fn.py DUMP FUNC` cuts one function out of a dump, `sngcc/cc1plus` with the GORDER hook). `mrel.py` wants mangled names
  (`dbmod_motion__Fv`). A second `VARIANTS = {...}` in a v.py silently replaces the first (use `VARIANTS.update`).
- **dbmod_locate 103 -> 0, zero code**: (1) the label loop is `pv = 0; x = 6; y = 4; for (i = 0; i <= 4; y++, i++)` with
  every column written through x: `eprintf(x * 8, ...)`, `(x + 10) * 8`, `(x + 11 + k * 8) * 8` (x/y are i-loop invariants
  that loop.c hoists and cse2 folds, the `y++, i++` order fixes the biv order); (2) case 3's colour is the statement form
  `if (dbModSlot[k].alive) { color = 0; } else { color = 7; }` (the ternary gives the store-flag form; note the target's
  polarity is alive -> 0); (3) the angle arm calls `VecRadLimit((Vec*) axis)` (the axis pointer, not `&em->rot`).
- **dbModMotionMove 90 -> 0, zero code, six readings** (pass-5 `f2` asm tag removed): (1) init loop `for (n = 0; n <
  SLOT_NUM; n++) order[n] = n;` -- check_dbra_loop reverses it into the `bdnz` down-counter with a pointer giv; its giv
  inits (`li r24,63; addi r9,r1,71`) are emitted by loop.c and therefore land AFTER the PRE-inserted highs; (2) the fix-up
  pass is a `while (i <= SLOT_NUM - 1)` whose match arm is `i++; continue;` with a second `i++` at the bottom (PRE leaves
  `addi r7,r7,1; b` in the match block and cse turns the bottom increment into `mr r7,r5` from `j = i + 1`); the swap is
  `u8 tmp = order[j]; order[j] = order[i]; order[i] = tmp;` (raw byte, target store order); (3) `(em->xE38 & 1) == 0`
  gives `andi.; bne` (`!(x & 1)` in a && chain gives the `xori; andi.` store-flag form); (4) the flags block re-reads
  `em->mot[0].flags` in each of the three tests -- cse1's ebb stops at the 2-use `beq` label, so the re-reads are not
  merged and PRE inserts the `mr r9,r0` copy the target has; removing the asm launder also shortened the pLog/fmt/file
  highs' live lengths by 2, which is what orders their priorities (163/163/162) as the target's r21/r20/r19; (5) the
  allDone loop is the index form `for (n = 0; n <= SLOT_NUM - 1; n++) { em = &dbModSlot[n]; ... }` (biv-eliminated end
  pointer `addis r11,r9,10; addi r11,r11,-21076`, signed `cmpw`, no entry test; the plain `dbModSlot[n]` form gets
  `mtctr/bdnz`).
- **dbmod_motion 2 -> 0, one tag**: the target loads `motNum[i]` into the address temp's register (`lhax r9,r9,r11`, r9
  reused); every C spelling (14 variables, `+b`/`b` launders, pointer forms) allocates the value to r0, because
  local-alloc's `block_alloc` ties operand 0 of an insn only with a dying REG operand, never with a register inside a MEM
  address -- and an asm's input operands ARE plain REG operands, so `asm("lhax %0,%1,%2" : "=r"(no) : "b"(pDbModState.p->
  motNum), "r"(i * 2)); if (no == -1)` gets the tie (`COMPILER-DIFF`, one asm-emitted instruction; the operands are the
  cse'd address and index). `asm("" : "+b"(x))` on the address moves the temp to r11 instead: local-alloc's first try uses
  the extended (fake_birth/fake_death +-1 insn) lifetime when sched2 is on and avoids the neighbour's register.
- **dbmodDispModelName 19 untouched -- the two residues and what they are:**
  (a) `lis "%s"@ha` at the first two of five uses: r11 in the target (scheduled 3 insns earlier), r9 in ours (kept below
  `slwi r3,r9,3` by the anti-dependence). The high is PRE pseudo 440 (`(high "%s")`, 12 refs across 716 insns, crosses 20
  calls, spilled, REG_EQUIV) rematerialised by reload before each use; `find_reload_regs` orders potential regs per insn
  (unused regs first in REG_ALLOC_ORDER r0, r9, r11 -- r0 not BASE), so ours takes r9 whenever r9 is free at the insn;
  the target's r11 means r9 was busy (live pseudo or another reload) at those two insns in its pre-sched2 order. The other
  three uses match. No source form found.
  (b) nlen/giv registers (target nlen r25, giv `li 184` r24; ours swapped): global priority `floor_log2(refs)*refs/len*10000`
  with REG_N_REFS weighted by flow's loop_depth (1 outside loops, 2 in the i-loop, 3 in the hash loop): nlen 9/57 = 4736,
  giv 469 14/88 = 4772 -> giv first -> r25. The target's `li 184` precedes `lis "^"` in the preheader (the "^" high is
  hoisted in loop pass 2, after the giv init), making the giv 14/89 = 4719 < 4736. In pass 1 the "^" high is the 4th
  movable (`"%c"` high 510, `hs - 1` 529, `i + 5` 562, `"^"` 573; move iff `threshold*savings*lifetime >= insn_count`,
  threshold 71 then -3 per moved movable, so 62*1*1 >= 58 moves it). The target needs insn_count >= 63 in pass 1 or two
  more movables ahead of it (56 < 58) -- the extra insns must be gone by flow's final pass without touching live lengths
  (the pDbModState high and the "^" high tie at exactly 1111 = 4*20/720 = 2*5/90 and any extra insn in the range of only
  one of them flips their r18/r19). Insns that vanish this way: gcse-inserted PRE copies (`(set 214 (reg 442))`) and giv
  computations (deleted by cse2 after strength reduction), flow's `(use (const_int 0))` nop after a call that ends a
  block, and dead sets (flow deletes them before its live-length update: `propagate_block` jumps to `flushed`). Tried
  and rejected: dead `color = 0` at the body top (cse1's ebb continues through the 2-use label and deletes the then-arm's
  redundant `(set color 0)` instead -- count stays 58), nested ternary, ternary as the eprintf argument, nested ifs,
  `continue` form, `he - digit` for the caret test, `x = 23 + k` / `y = i + 4` variables, `k * 8 + 184` for the caret
  column (two givs that combine_givs does not merge, 136 words), `char c = name[k]` (load hoisted, 45), nlen asms in
  the loop (`"+r"`/`"+b"`/volatile: 63-75, they flip r18/r19 and i's register).
- **Compiler facts read this pass:** lcm.c PRE is block-based (`earlyin = OR preds earlyout`, `earlyout = ~transp |
  (earlyin & ~antin)`, `delayin = (antin & earlyin) | AND preds delayout`, `latein = delayin & (antloc | ~AND succs
  delayin)`, `isoin = latein | (isoout & ~antloc)`, `optimal = latein & ~isoout`, `redundant = antloc & ~optimal`) and
  inserts at the END of a block (`insert_insn_end_bb`), never on an edge -- a source-level block is needed on the path to
  get an insertion there. loop.c: `insert_bct` only for a compile-time iteration count (`loop_iterations` bails on
  multiple back edges or a non-constant initial/final value) with no call/tablejump in the loop; `scan_loop` counts
  insn_count (`count_loop_regs_set`, every class-'i' insn) BEFORE it substitutes single-use invariants into their use
  (`loop_has_call && reg_single_usage && no_labels_between_p && validate_replace_rtx` -> the set is deleted, e.g. the PRE
  copy feeding `(mult 222 14)`); movables are moved in insn order, giv inits after them, pass-2 movables after the giv
  inits. flow's REG_N_REFS weight is loop_depth (starts at 1). reload: REG_EQUIV `(high sym)` pseudos are
  rematerialised per use with the reload reg chosen by `find_reload_regs`' per-insn potential order, then
  `allocate_reload_reg`'s round robin over the function's spill_regs.

**Flags.** Nothing flipped; t_esp/db_mod and Tools/db_mod stay False (19 words in dbmodDispModelName in both). Both
objects rebuilt through the locked ninja; 111 not re-checked (nothing flipped).

### Tool RELs, dbg_tool.h Update layout (one header edit: `cDbgToolMain<T>::Update()` arms in source order 0,1,2,6,3,7,4,5,8; t_event SubToolMessMove 274 -> 106, Tools/t_esp_area ToolEspArea 399 -> 287, Tools/t_lightarea ToolLightAreaMain 462 -> 121; db_toolbase IDENTICAL, 111 OK; nothing flipped; 2026-09-11)

Harness /tmp/dbgv (deleted): a copy of include/dbg_tool.h plus each includer's .cpp with `#include "dbg_tool.h"` rewritten to the
absolute path of the copy, judged with `tools/research/kit/variant.sh <unit> /tmp/dbgv/<file>.cpp [FUNC] --no-diff` (t_event.cpp,
t_esp_area.cpp, t_lightarea.cpp and db_toolbase.cpp all include dbg_tool.h directly, so the absolute-path include is enough; no
build/ objects touched until the final apply). `reorder.py` cut the top-level `case N:` blocks of the `switch (mode)` and re-emitted
them in a given order.

- **Applied (zero code): the `case` labels of `switch (mode)` in `cDbgToolMain<T>::Update()` now stand in source order
  0, 1, 2, 6, 3, 7, 4, 5, 8** (case 6 LoadData right after case 2, case 7 SaveData right after case 3; bodies unchanged). GCC 2.95
  lays the arms out in source order, so this is exactly the t_event closer's "header copy reordered" form. Per includer:
  t_event/t_event SubToolMessMove 274 -> 106 words (unit 336 -> 168, 59/69), Tools/t_esp_area ToolEspArea 399 -> 287 (unit 431 ->
  319, 32/38), Tools/t_lightarea ToolLightAreaMain 462 -> 121 (37/38), Tools/db_toolbase stays IDENTICAL, t_id/t_id (does not include
  dbg_tool.h) unchanged at 574 words 58/69. Objects: `/bin/rm build/G4BE08/src/Tools/*.o build/G4BE08/src/t_*/*.o` then the
  flock'd `ninja -k 0`; `dtk shasum -c` 111 OK.
- **Rejected: source order 0,1,2,6,3,7,4,8,5 (case 5 `ret = 0` last in the source): SubToolMessMove 175 words** (worse than 106) and
  the register allocation shifts from the prologue on. Keep 5 before 8.
- **The t_event closer's finding (2) ("case 4's pad test reads offset 0 of a symbol, not `Joy[0].on`") is a misreading of the dtk
  listing:** the target is `lis r9, Joy+0x10@ha; lwz r0, Joy+0x10@l(r9); andi. r9,r0,0x200` (dtk shows the reloc'd `lwz` as
  `lwz r0, 0x0(r9)`), i.e. plain `Joy[0].on & 0x200` with the address folded into the lo_sum; ours already emits the identical
  three instructions at SubToolMessMove+0x818. The t_lightarea target spells the same test `lis Joy@ha; addi Joy@l; lwz 0x10`
  (its Joy address is shared with the neighbouring trg tests). No header change for (2).
- **Finding (3) remains a residue, not chased:** the target's case-5 `li r20,0` sits at the very end of the switch region
  (SubToolMessMove+0x8c0, after case 8's `stw r30,0x1c(r31); b end`, falling through into `cmpwi r20,0`), ours emits
  `li r20,0; b end` in source position after case 4 (+0x830, the 2-word insert). Putting case 5 last in the source does not give
  that shape (see the rejected order); the remaining 106 words of SubToolMessMove are that insert plus register/issue-order
  differences in the inlined WinUpdate/copy-loop regions (+0x698..0x7d0, +0xd50..0xec8).

### Tool RELs, t_esp pass 14 (t_esp 208/212: InitTool 8389 -> 2916 words, the 886 spill slots and the whole entry block exact (seg 0: 1850 insns, 3 differ); slot ORDER = 118 dead sets (N 5233), slot SET = the EDIT windows' `pa` register form, the spill-pair cycle r8/r10/r0 and every `lis r6/r8` pool high = reload's spill SET {r0,r6,r8,r9,r10,r11} forced with a tagged asm; IN PROGRESS 2026-09-11)

- Harness ~/.cache/tesp14 (pass-13 style; deleted at the end): `slots.py LISTING [-v]` (`addi rX,r1,C; stw rX,S(r1)` pairs
  sorted by S, `-v` also the other `stw rX,S(r1)`), `fitn.py SLOTS` (odd N whose bucket order `(13289 + C) % N` is monotone
  along the slot sequence), `setdead.py FILE N` (rewrites the dead-set block to N sets), `sbs.py T.s O.dis A B` / `segcmp.py
  T.s O.dis` (per-`bl` segment side-by-side / diff summary), `ins.py DUMP 'InitTool()'` (one line per insn of a dump). Variants
  through `tools/research/kit/variant.sh t_esp/t_esp V.cpp --no-diff` (1 s) and `tools/research/kit/rtl.sh ... -dG`/`-dg` for the table size
  (`Expression hash table (N buckets, ...)` under `;; Function void t_esp_namespace::InitTool()`) and the reload picks
  (`Spilling reg R.` lines: `rg 'Spilling reg' | sort | uniq -c` = the function's spill_regs).
- **Slot order (N arithmetic).** After pass 13 the plain tree had `Expression hash table (5231 buckets, 2354 entries)` and its
  slot sequence fits N = 5231 (fitn: 1 descent, the wrap), the target's fits 5233/5235 only. N = (n_insns / 2) | 1, so +1 bucket
  = +2 real insns at gcse: 78 or 80 dead sets -> 5233 (verified in the -dG dump), 82 -> 5235; with the right N the C sequence of
  the 882 `&pos` slots is exact but every S is 4 bytes low (10433 words: worse than 8389, because every `lwz/stw slot(r1)`
  moves) -- the slot SET below. The `pa` form removes 36-40 insns (47 `lwz r3,0(r29)` reloads of `e->pa`), so 76 sets -> 5213,
  80 -> 5215 and **118 sets -> 5233** (v4 of the previous agent had found the same count): 886/886 slots exact in S and C.
  Rule: every change to InitTool's insn count re-fits N; read it from the dump, never guess (2 insns per bucket).
- **Slot SET (the 4-byte shift): the EDIT windows keep the ctor argument in a callee-saved register.** Target window 1 (after
  the `stw r29,g_pEditWin1` `__builtin_new`): `lwz r0,0x237c(r1); lis r9; lwz r28,g_pPrimArray@l(r9); mr r29,r3; stw
  r0,0x1a7c(r1); li r6,0; stw r6,4(r29); stw r28,0(r29); ... mr r3,r28` for CreateNormalWindow and all 10 CreateNumeric of the
  row loop, but `lwz r3,0(r29)` (= `e->pa`) for the 5 CreateButton. Ours reloaded `e->pa` everywhere, so the copy pseudo of the
  0x237c `&pos` took r14 instead of being spilled to 0x1a7c (the target's 11th low-region slot), shifting r14..r20's reloads by
  one and every later slot by 4. Form (zero code, no tag): `DB_PRIM_ARRAY* pa = e->pa;` right after the `new` (cse forwards the
  ctor store, so `pa` = the argument pseudo) and `DB_PRIM_ARRAY* pa_ = pa;` in the CreateNormalWindow/CreateNumeric blocks, `pa_
  = e->pa` kept in the CreateButton blocks (target evidence per call site). 8389 -> 6627 with N = 5233.
- **Entry block + the per-segment r6/r9 residue = reload's spill SET (reload1.c).** Facts: `order_regs_for_reload` lists per insn
  the hard regs not in `live_before`/`live_after` (CLOBBERs count: build_insn_chain's live_after includes everything the insn
  sets) in REG_ALLOC_ORDER r0, r9, r11, r10, r8, r7, r6, r5, r4, r3; `find_reload_regs` takes the first for each reload need and
  ORs it into the function-wide `used_spill_regs`; `finish_spills` then gives EVERY insn all spill regs not held by live pseudos
  (`chain->used_spill_regs`), and `allocate_reload_reg` hands them out round-robin from `last_spill_reg` over `spill_regs` in
  ascending regno order. Ours had spill_regs {r0,r9,r10,r11} (`Spilling reg 9/0/10/11`), the target {r0,r6,r8,r9,r10,r11} (77
  `lis r6,rodata`, 57 `lis r8,rodata` pool highs are rematerialised REG_EQUIV highs = reloads; ours had 497 `lis r9`). With
  r8 in the set the entry block's pairs cycle r8/r10/r0 (r9/r11 hold the g_editRowNo/g_editRowWk addresses, r6/r7 the data
  highs) and the pTexRender store comes first (its high is in r8, so it precedes the first r8 pair); with r6 too, the CreateString
  segments' `lis r6,H; lfs f0,L(r6)` follow. Verified by forcing: two whole-function REG_EQUIV constants (`u32 k8 = 0x1234; u32
  k6 = 0x5678;` at the top, 3 refs each so update_equiv_regs neither substitutes nor moves them, no callee-saved reg free ->
  rematerialised) read by two asms at the END of InitTool with r0,r7,r9,r10,r11 clobbered -> the reloads pick r8 then r6
  (`Spilling reg 8` x4, `reg 6` x4): 6627 -> 2916, seg 0 1850 insns with 3 differing, tagged `// COMPILER-DIFF: candidate
  (reload spill set)`. Failed forcing forms: `"r"(0x1234)` (block-local constant, local-alloc gives it r8: no reload), `"m"(global)`
  (the high pseudo is block-local and allocated). Natural source: some insn of the original needs a reload while r0/r9/r10/r11
  (and r8/r7) hold live pseudos -- most likely in the EDIT row loop (7-argument CreateNumeric/CreateButton with the double
  conversion constants live); not found in the box.
- Residue 2916: per-window segments (+-1..8 insns each: `lis rN,H` register names, `li rX,K` positions, some `lwz rN,slot`
  callee-saved copies) -- the per-window mechanisms, not touched yet in this pass.

### Tool RELs, db_mod pass 7 (dbmodDispModelName 19 -> 10 words: residue (1) nlen/giv closed with two dead sets tagged `candidate (loop.c insn_count)`; residue (2) is reload's spill-reg round robin; in progress; 2026-09-11)

- Harness /tmp/dbmod7 (deleted at the end): `try.py LABEL OLD NEW ..` (exact-once edits of base.cpp -> tools/research/kit/rtl.sh
  + bytecmp + the `Loop from`/`Insn N: regno` lines of the function), `fn.sh DUMP FUNC`.
- **Kit extended (persistent, tools/research/sngdbg reload1.c, env-gated, byte-identical without the vars):** `RLDSPILL=11,10`
  adds hard regs to the function-wide spill set at `finish_spills` (oracle for "the target's reload had one more spill
  reg"); `RLDDBG=1` prints one line per reload register handed out by `allocate_reload_reg` (`insn uid, reload index,
  hard reg, round-robin index / n_spills, class`).
- **Residue (1) closed, insn arithmetic (-dL, k-loop `Loop from 440`):** pass 1 had 58 real insns, movables in insn order
  `"%c"` high (thr 71), `hs - 1` (68, life 6), `i + 5` (65), `"^"` high (62): 62*1*1 >= 58 moved the `"^"` in pass 1
  (before the giv init `li 184`), so the giv was 14/88 > nlen 9/57 and took r25. Constraint: the `"^"` must fail
  (thr < count) while `i + 5` still passes (its thr >= count). Two dead sets at the body top: `f = 0;` (constant, set once
  in the loop, `f` has refs after the loops -> an invariant movable, `global move-insn`, moved FIRST: -3 for every later
  movable) and `no = k;` (non-invariant, stays, +1). Count 58 + 2 = 60; thresholds `f=0` 71, `"%c"` 68, `hs-1` 65,
  `i+5` 62 >= 60 moved, `"^"` 59 < 60 not moved; pass 2 (53 real, thr 71) moves the `"^"` after the giv init. Both dead
  sets are deleted by flow before the live-length update (no REG_LIVE_LENGTH effect: pDbModState/"^" stay tied at 1111,
  r18/r19 unchanged; nlen r25 / giv r24 now match). Two movable dead sets (`no = 0; f = 0;`) give 60 with `i + 5` at 59
  < 60 (12 words); dead TESTS are wrong here because their compare survives to jump2 inside the loop and lengthens
  nlen and the giv equally (needs len_giv/len_nlen > 42/27; 88/57 -> 89/58 goes the wrong way).
- **Residue (2) read off reload1.c (SN 2.95.3, insn_chain reload):** the `high` pseudos of `"%s"`/`"[%6s]"` are
  REG_EQUIV-spilled and rematerialised per use by `allocate_reload_reg`, which walks `spill_regs[]` (the function-wide
  union `used_spill_regs`, ascending hard-reg order) ROUND ROBIN from `last_spill_reg + 1`, skipping regs live at the
  insn and regs outside the reload class (r0 is not BASE); `last_spill_reg` persists across insns and every CALL_INSN
  takes an LR (r65) scratch reload, resetting the robin to the LR index. Ours: spill set {r0, r9, LR}; the target's
  had r11 too. `RLDSPILL=11` reproduces site 1 exactly (insn 99: previous reload = x's `lwz r9,8(r1)` at index 1 ->
  r11) and leaves sites 3-5 at r9 (previous reload = a call's LR -> r0 -> r9), but site 2 (insn 318, `"[%6s]"` after
  the `no == -1` bne) still gets r9: its previous reload is the null arm's call (LR). The target therefore had r9
  busy at 318 (a pseudo in r9 live across the lo_sum: `no` in r9 is the candidate) or one more r9 reload between the
  null-arm call and 318; r9 busy at a BASE reload is also what puts r11 into the spill set. `char* d = motDir; asm("" :
  "+r"(d) : "r"(no))` before the eprintf does not do it (the asm is scheduled before the lo_sum, `no` dies early).
- Stopped by the user at 16:34 with the tree at InitTool 2916 words (built, `ninja` clean for t_esp.o, bytecmp 208/212,
  nothing flipped, no `ninja -k 0`/shasum re-run after the edit). Edits in src/t_esp/t_esp.cpp only: the 4 CreateEditWindowN
  helpers (`pa` form), the dead-set block 76 -> 118, `k8`/`k6` + the two end-of-function asms (tagged). First residues after
  the entry block: seg 3 (`stw r16,0x1a58` vs `stw r11,4(r7)` order, a sched2 tie), seg 25/37/67/100/129 (the EDIT row-loop
  preheaders: the three pool/bss highs' reload registers rotated, target r8,r6,r10 vs ours r6,r10,r8 = the round-robin phase,
  i.e. one earlier reload or one live pseudo differs there), seg 162 (the LOAD window: the target hoists `lis r25 g_pPrimArray`,
  `lis r19/r21/r16/r28 rodata` and `lis r27 bss` into callee-saved registers BEFORE its `__builtin_new`, ours has 6 fewer insns
  there and the FP/GPR callee-saved names differ from seg 163 on: a global-alloc difference that starts at that window).
  Verified: the N/slot arithmetic and the spill-set mechanism (dump evidence + word counts); unverified: the natural source
  form for r6/r8 in spill_regs and the per-window residues.

### Tool RELs, t_id pass 4 (STOPPED by the user mid-pass; ToolInterfaceDesign 41 -> 0 via the dead-test lever; nothing else applied; not flipped; 2026-09-11)

- **ToolInterfaceDesign 41 -> 0**: the `while (1)` body had exactly 9 blocks (`;; rgn 0 nr_blocks 9` in `-dS -fsched-verbose-6`), so sched1 formed an interblock region and hoisted `addi r3,toolIdSys`/`li 0`/`li 16`/`li 3,1` above the branches. A dead test with two compares at the body end (`{ IdTool* dead; if (pIdTool->cnt == 0 && pIdTool->mode == 0) dead = 0; }`, tagged `COMPILER-DIFF: dead test`) adds 2 blocks -> 11 > MAX_RGN_BLOCKS -> no region; the set is deleted by flow, both compares by jump2. Verified with variant.sh (0 words, size 0x138); the locked ninja rebuild was NOT run.
- **toolIdInit 12 (unchanged)**: read off the `-fsched-verbose-9` dump: the 11 stores are one lsu op per cycle from t=4; rank = weight (a store whose source reg dies = 0, else +1; lower first), then LUID. Target order (lang2, cnt, drawSafe, type, x17B, pause, menuY, level, parentNo, menuX, x24) is pure source order, i.e. NO constant dies at its last store; a `"=m"` keep-alive asm after the stores reproduces the sched1 order (source order) but perturbs sched2/allocation (19-20 words); `volatile` view 21, `int zi` for level/x24 17 (cse merges QI/SI zero). Also the target's level store (`stb r7`, the SI zero) precedes the x24 store (`stw r7`), which `w->level = w->x24 = 0` cannot give. Original construct still unknown.
- **idEditPos 334 (unchanged; structure read, nothing applied)**: (1) subCur cases are in NUMERIC order 0,1,2,3,4,5 (cross-jump leaves `andi.; b` stubs at 1 and 2 and the full body at 5; our 0,1,2,5,3,4 puts the body before case 3); (2) `*pos = d->vtx[N]`'s x word is stored via `d` (`stw r10,0x118(r26)`) and y/z via `pos` -- `d->pos = d->vtx[N]` gives d-based addresses everywhere including the ID_DRAW_GUIDE tail (target keeps `pos` r20 there): try `d->pos.x = d->vtx[N].x; pos->y = ..` or a mixed view; (3) `pos` has a SECOND pseudo in case 1/subCur 0 (`addi r31,r26,0x118` before `bl toolIdCalcVertex`): a block-local `Vec* pos = &d->pos` there; (4) the inner menu loops pass `y + i * 0xE` (target `add r4,r24,r25` with `mr r25,r11` = i*0xE kept), `yy` only for the two outer eprintfs; (5) `y += 0xE` sits after the toolIdCalcVertex call in the target's case 0. Cumulative variant caseorder+d->pos+y+i*0xE: size 0x10c4 (from 0x10ac, target 0x10d4). Tool: a masked mnemonic-level difflib compare of dtk disassemblies (registers/labels/symbols normalised) is far more useful than fdiff for 300-word functions -- worth adding to the kit.
- idEditColor, toolIdOption, idEditUnit not started.

### Tool RELs: snd_test/t_camera_data closer 2 (t_movie/snd_test 76 -> 77/77 IDENTICAL, disp_sequencer 28 -> 0 pure C, asm tag removed; NOT flipped — stopped by the user before the modules.py edit; t_camera_data untouched; 2026-09-11)

- **disp_sequencer 28 -> 0, zero code, one structural item replaces the `#13` asm pair:** `int y0; ... y0 = 0x54;` right before the
  `for (ch ...)` and `y = y0 + 0x54;` after the D diamond (the `asm("li %0,84")`, its keep-alive and `int k` removed). Read off the
  dumps (`tools/research/kit/rtl.sh`, `GDBG=1`): the 14 row pointers `seq + 0x31aa..0x327a` and the "%03d" high are gcse PRE/HOIST insertions
  at the end of the preheader bb in body order (`PRE/HOIST: end of bb 2, insn 789/792/...`), the "%3d" high and `Snd_voice_work` are
  re-emitted by loop.c AFTER them (the body copy's REG_EQUAL symbol_ref becomes `emit_move_insn` -> fresh high/lo_sum, the PRE high
  dies). sched1 issues these free fillers ONE per header-eprintf slot in LUID order (`ch = 0`, "%03d", flag, prio, ...); the two
  spilled row pointers (ch_flag/ch_prio -> `stw 8/0xc(r1)`) are pinned to their sched1 slot by reload's r9 pair, everything else
  floats in sched2. The target has them one slot later = one more free filler with a LUID below `ch = 0`: the `li y0,84` (single-set
  constant, REG_EQUIV, deleted by reload as the equiv init and rematerialised as `li r9,84` right before `addi r27,r9,84`). The same
  pseudo explains the body `addi` (cse cannot fold across the join label; cprop has no simplify step for `(plus reg const)`). Rule: a
  spilled pair issued N slots off in a call-sequence preheader = N missing/extra free fillers with lower LUIDs, look for a
  REG_EQUIV constant whose init was deleted.
- Verified: `python3 tools/bytecmp.py t_movie/snd_test` = IDENTICAL after a locked ninja of snd_test.o. Unverified/not done: the flip
  (`config/G4BE08/modules.py` MATCHING entry for `t_movie/snd_test.cpp`, `tools/make_rel.py --verify`, `ninja -k 0`, shasum 111 OK).
- t_camera_data (tcDataExport 142w size 0x538/0x534, tcSetBesideOffset 27w, fn_t_camera_1B8C4 24w) not started.

### Tool RELs, lightarea/esp_area/t_event closer (paused after 10 min; Tools/t_lightarea ToolLightAreaMain 121 -> 80 words in a VARIANT only, nothing applied; 2026-09-11)

Harness ~/.cache/tools_la (deleted): `tools/research/kit/variant.sh Tools/t_lightarea <copy> --no-diff` on a copy of src/Tools/t_lightarea.cpp; no tree, build/ or config edit was made.
- **Verified (variant, not applied): ToolLightAreaMain 121 -> 80 by deleting the display loop's `AreaData* a = &w->area;` local and writing `&w->area` / `w->area.u.xz4.h` at each of its four uses** (src/Tools/t_lightarea.cpp:441-451). The `a` variable is a second DEST_REG giv: loop.c combines it with the address-giv base (`addi r30,w,4`) as `mr r29,r30`, live across the loop's calls, costing one callee-saved register (i/mfcr/&pos/lis shifted r27..r24 -> r26..r24 and r29 for w). The target's shape is the base giv r30 for `AreaGetCenterPos(&pos, &w->area)`/the `lfs 8(r30)`/`lbz -2(r30)` refs plus a block-local `mr r10,r30` after `bl GetScreenPos` feeding both `AreaDataDisp(&w->area, ..)` arms (PRE of the arms' common `(plus w 4)` at the join, combined by loop.c into `r10 = r30 + 0`). Also the whole 0x1920-0x1adc register naming.
- **Remaining 80 (read, not tried):** (a) 4 words are the vtable relocs `_vt.20cDbgFileSelectWindow`/`_vt.18cDbgOkCancelWindow` (+0x28c/+0x294/+0x4cc/+0x4dc): the target references t_esp_area's linkonce copy (Tools .rodata 0x39d8/0x3988), ours WEAK and unresolved in Tools.elf falls back to our own copy (0x45d0/0x4580) -- a module-link artefact, resolved only once Tools/t_esp_area (the first definer) is Matching; **t_lightarea cannot flip alone**, make_rel --verify would see the reference to the second copy. (b) `lwz r11,4(r22)` / `lwz r0,0x1c(r22)` (target reads `tool.pEdit`/`tool.mode` through the `&tool` pseudo r22 = r1+8) vs ours `lwz 0xc(r1)`/`0x24(r1)` at the body's direct `tool.pEdit->GetCurrentNo()` (:447) and `tool.mode == 4` (:469); t_esp_area's ToolEspArea has the identical residue at its `tool.mode == 4` (target `lwz r0,0x1c(r23)`, ours `0x24(r1)`) while the following `tool.mode == 2/3` chain (`lwz r0,0x1c(r22)`) and the inlined Update() accesses already go through the pseudo -- so the original's body accesses were inside inlined cDbgToolMain members (`this`), e.g. a mode getter / an edit-current-no getter; header-side, not tried. (c) HEADER: `MakeSaveData` first loop: target `li cnt,0; mr w,work; li i,0; cmplw i,num` = a stepped `T* w = work` assigned BEFORE the loop entry test (ours `&work[i]` is an address giv whose init `mr r31,r24` lands in the preheader after the hoisted `lis`es), in both the inlined case-7 SaveData (+0x1640) and the body's `tool.MakeSaveData` call (+0x1c7c); and the second loop's `src = work` issued before `dst = mem + 1` (target `mr r31,r24; mr r29,r23`, ours reversed: sched1 LUID tie -> put `src = work` textually before `dst = (T*)(mem + 1)`). Not verified (dbg_tool.h not editable in this pass). (d) HEADER: case-5 `li r18,0` laid out last after case 8's `stw r31,0x1c(r22); b end` (the t_event closer's finding (3)); in t_lightarea `ret`'s 1 is also the constant of every KeyCheck `stw r18` store. Not re-tried.
- t_esp_area (32/38, ToolEspArea 287 + the five template members owned by the header) and t_event (59/69) not started.

### Tool RELs, esp_area/lightarea closer 2 (Tools/t_esp_area ToolEspArea 287 -> 74 applied (37/38, size EXACT), -> 7 with a VERIFIED dbg_tool.h change; Tools/t_lightarea ToolLightAreaMain 121 -> 78 applied, -> 4 (the vtable reloc words only) with the same header; t_event SubToolMessMove 106 -> 84 and db_toolbase IDENTICAL under it; header NOT applied (out of scope), nothing flipped; 2026-09-12)

Harness ~/.cache/tools_ea (kept for the header pass; delete when dbg_tool.h is applied): `hv.sh` judges both units with
`tools/research/kit/variant.sh` against `inc/dbg_tool.h` (a COPY of include/dbg_tool.h; `h_<unit>.cpp` = the tree source with
`#include "dbg_tool.h"` rewritten to the copy's absolute path, also for t_event and db_toolbase); `inc/dbg_tool.orig.h`,
`inc/dbg_tool.v4.h` (the verified header), `inc/dbg_tool.v5.h` (v4 + member getters). Mechanisms read with `GDBG=1`/`LADBG=1
CC1DIR=tools/research/sngdbg` and `tools/research/kit/rtl.sh` (rtl4/, rtl5/). include/dbg_tool.h itself was not edited.

**Applied in the tree (src/Tools/t_esp_area.cpp, src/Tools/t_lightarea.cpp only):**
- **(1) The display loop's `AreaData* a = &w->area;` local removed; `&w->area` / `w->area.u.xz4.h` at its four uses (both
  units).** t_lightarea 121 -> 80 as the previous closer measured; t_esp_area 287 -> 169 AND its five "template member" diffs
  (`cutBuffer`/`fn_Tools_28718`/`LocalUpdate`/`execCopyWindow`/`fn_Tools_2683C`, 24/3/2/2/1 words) vanished: they were never
  header-owned, only the 4-byte size gap of ToolEspArea (0x1c50 vs 0x1c4c) shifting their relocation targets. The `a` pseudo was a
  second DEST_REG giv that loop.c combined with the address giv (`mr r29,r30`) across the loop's calls (one callee-saved register).
- **(2) `tool.pEdit->GetCurrentNo()` and the three `tool.mode == 4/2/3` reads go through file-local inline getters
  `ToolEdit(&tool)` / `ToolMode(&tool)` (`static inline T f(cDbgToolMain<X>* t) { return t->m; }`, both units).** integrate.c
  copies the `&tool` actual (`(plus fp 8)`) into a pseudo and gcse merges it with the inlined members' `this` pseudo, so the body's
  reads become `lwz 0x1c(rT)`/`lwz 4(rT)` as in the target (ours read `0x24(r1)`/`0xc(r1)`). ToolEspArea 169 -> 74 (this also
  fixed the r22<->r23 swap of the `&tool` pseudo against the hoisted `lis "  ..."@ha`: the pseudo's refs), ToolLightAreaMain 80 ->
  78. All three mode reads must go through the getter (with only `== 4` the `== 2/3` chain reloads `0x24(r1)`, +1 word). A
  function-scope `cDbgToolMain<X>& t = tool;` reference view is WRONG (579 words: `&tool` live from the prologue in r16, frame +8).
  VERIFIED equivalent (v5 header): class members `int GetMode() { return mode; }` / `cDbgEditWindow<T>* GetEdit() { return
  pEdit; }` used as `tool.GetMode()` / `tool.GetEdit()` give the same bytes -- that is the original's form; when dbg_tool.h is
  edited, add the two getters and delete the file-local stand-ins (they carry a comment saying so).

**VERIFIED header changes (inc/dbg_tool.v4.h; `diff -u include/dbg_tool.h ~/.cache/tools_ea/inc/dbg_tool.v4.h`), judged on all
four includers -- t_esp_area 74 -> 7, t_lightarea 78 -> 4, t_event/t_event 168 -> 146 words (SubToolMessMove 106 -> 84, CallbackSave
12 unchanged), Tools/db_toolbase IDENTICAL:**
- **(H1) `cDbgToolMain<T>::MakeSaveData` count loop as a stepped pointer with its OWN counter, the pointer assigned before `cnt =
  0`:** `{ T* w = work; u32 j; cnt = 0; for (j = 0; j < num; j++, w++) if (IsWorkAlive(w)) cnt++; }`. Three separate facts, each
  worth words: (a) the stepped pointer (target `mr w,work` in the preheader; ours `&work[i]` was an address giv initialised after
  the hoisted `lis`es): t_esp_area 74 -> 52, t_lightarea 78 -> 70. (b) `T* w = work;` BEFORE `cnt = 0;` (sched1 LUID tie between
  the two preheader sets: target `li cnt,0; mr w,work; li i,0`): -> 51/69. (c) the count loop's counter is NOT the `i` shared with
  the copy loop: cse.c make_regs_eqv puts the NEW register first in the `0` class only when its last use is later than the class
  head's (`uid_cuid[REGNO_LAST_UID (new)] > uid_cuid[REGNO_LAST_UID (firstr)]`); with a shared `i` (last use in the copy loop, after
  `mem->num = cnt`) the entry test canon_regs to `cmplw i,num`, with a loop-local counter it stays `cmplw cnt,num` -- the target's
  entry test reads the cnt register (`cmplw r27,r25`) and the bottom test the counter (`cmplw r28,r25`). -> 48/49. This also fixed
  the i/cnt register pair (i had refs 26 len 118 over both loops, pri 8813; cnt refs 10 len 66).
- **(H2) copy loop: `T* src = work;` declared before `dst = (T*) (mem + 1);`** (both inside the block; target `mr r31,r24; mr
  r29,r22`, ours reversed: sched1 LUID tie): t_esp_area 48 -> 46, t_lightarea 49 -> 43 (the body-inlined `tool.MakeSaveData` call at
  +0x27fc..+0x28d0 also resolved: `addi r29,r28,0x10` position and the r24..r28 naming).
- **(H3) `Update()`: `case 5: ret = 0; break;` moved AFTER case 8 (source order 0,1,2,6,3,7,4,8,5):** t_esp_area 46 -> 7,
  t_lightarea 43 -> 4, t_event SubToolMessMove 95 -> 84. GCC lays the arms out in source order and the last arm's `li r18,0` falls
  through into the `cmpwi r18,0` of the caller (`if (tool.Update() == 0) break;`), exactly the target's `stw r31,0x1c(rT); b end;
  li r18,0; end:`. The "dbg_tool.h Update layout" section rejected this order (SubToolMessMove 175 vs 106) -- that measurement was
  taken WITHOUT H1/H2; with them it is 84 vs 95. Re-check t_event with H1-H3 together before applying, never H3 alone.
- Consequence, not a separate change: the r19<->r20 pair in ToolEspArea (`lis 0x4330` reg refs 99 len 2840 pri 2091 vs the
  case-7 `lis "HALT %s(%d)\n"@ha` refs 5 len 48 pri 2083) flipped with H1 (the HALT high's life shortened by one insn -> 2127).

**Remaining after v4 (read, boxed out):**
- **ToolEspArea 7 words = r28<->r29 in the inlined CreateEditWindow ctor block** (`lis/addi esp_area_work` target r28, the `li 4`
  (wx) and `li 5` (nRows) constants target r29). local-alloc block 13 (LADBG): `q0 reg299 work refs 4 birth 4 death 52 pri 1666`
  vs `q2 reg297 li 4 refs 2 birth 14 death 26 pri 1666` -- an exact priority tie broken by qty number (`qty_compare_1` returns
  `q1 - q2`); work is q0 because sched1 issues its lis->addi->stw chain first (longer critical path than `li 4`->stw). The target
  needs work's life >= 49 suids (pri <= 1632) or refs 3, or the constant born first at sched1; the final instruction order is
  identical in both, so whatever differed was gone by sched2 (an RA-time insn inside the ctor's block, REG_N_REFS weighting from
  the `do {} while (0)`, or a REG_EQUIV'd constant). Only t_esp_area shows it: its `work` actual is a bss ARRAY (`lis/addi`,
  refs 4 with the combined high); t_lightarea/t_event pass loaded pointers (`lwz`). Tried and rejected: a codeless
  `asm("" : : "r"(esp_area_work))` after the ctor (226 words: the address becomes a global allocno); header copies with `pWork =
  work` before `rows = nRows` (7, byte-identical to v4 -- the store order there is not LUID-driven), `y = wy; x = wx;` (14),
  `if (nRows > 128) nRows = 128;` (9). Not tried: `register`/asm pins are impossible from the body (integrate copies a hard-reg
  actual), and the ctor is shared with t_event/t_lightarea whose ctor blocks are already identical.
- **ToolLightAreaMain 4 words = the `_vt.20cDbgFileSelectWindow`/`_vt.18cDbgOkCancelWindow` relocs** (+0x28c/+0x294/+0x4cc/
  +0x4dc): the target resolves to t_esp_area's linkonce copy; a module-link artefact that disappears when t_esp_area is Matching
  and linked first. Flip order therefore stays: t_esp_area first (IDENTICAL needed), then re-run bytecmp on t_lightarea.
- No body-side residue is left in either unit; both wait on the dbg_tool.h edit (H1-H3 + the getters) and the ctor qty tie.

### Tool RELs, dbg_tool.h H1-H3 (header APPLIED: MakeSaveData count/copy loops + Update() case 5 last + GetMode()/GetEdit(); Tools/t_esp_area 74 -> 7, Tools/t_lightarea 78 -> 4, t_event/t_event 163 -> 141, db_toolbase IDENTICAL in Tools and t_event; 111 OK; nothing flipped yet; 2026-09-12)

include/dbg_tool.h now carries the "esp_area/lightarea closer 2" verified changes (v5 of ~/.cache/tools_ea, harness deleted):
- H1 `cDbgToolMain<T>::MakeSaveData` count loop = `{ T* w = work; u32 j; cnt = 0; for (j = 0; j < num; j++, w++) if (IsWorkAlive(w)) cnt++; }`.
- H2 copy loop: `T* src = work;` declared, then `dst = (T*) (mem + 1);` inside the same block.
- H3 `Update()` arms in source order 0,1,2,6,3,7,4,8,5 (case 5 last: its `li ret,0` falls through into the caller's `cmpwi`).
- `int GetMode()` / `cDbgEditWindow<T>* GetEdit()` members; the file-local `ToolMode(&tool)`/`ToolEdit(&tool)` stand-ins in
  src/Tools/t_esp_area.cpp and src/Tools/t_lightarea.cpp are gone, the bodies use `tool.GetMode()`/`tool.GetEdit()` (byte-equal).
- ninja does not track the header: after a dbg_tool.h edit `/bin/rm build/G4BE08/src/Tools/*.o build/G4BE08/src/t_*/*.o` first.
  Includers are exactly Tools/{t_esp_area,t_lightarea,db_toolbase} and t_event/{t_event,db_toolbase}.
- Pre-checked with variant.sh against the CURRENT t_event source (the t_event agent had moved on since closer 2: 163 in-tree,
  141 under the header) before the tree edit; the tree build reproduced every variant number.

### Tool RELs, t_camera_data/db_widget closer (t_camera_data 14 -> 15/17: tcDataExport 142 -> 193 words but size 0x538 EXACT and the nameless cManager<cLight> block identical; tcSetBesideOffset 27 / db_widget DB_STRING ctor 7 unchanged; nothing flipped; 111 OK; 2026-09-12)

Harness ~/.cache/tcam (deleted): `mk.py BASE OUT OLD NEW..` exact-substring variants + `ins.py DUMP FUNC [regex]` (one line per insn of an
`rtl.sh` dump) on top of the kit. Only src/t_camera/t_camera_data.cpp edited (tcDataExport); no config edit.
- **tcDataExport: the 4 missing bytes are ONE reused pointer variable.** The target's `addi r31,r29,0x10` before memclr, `mr r25,r31`
  in the block after strncpy (r25 = rec for the area computation and the last loop) and loop 2's fovy pointer in the same r31
  (`add r31,r5,r0; stfs f0,0(r31); addi r31,r31,4; mr r5,r31`) are one multi-set pseudo: set at the top from `buf + 0x10`,
  copied into `rec` (the copy is kept because the pseudo is born in block 0 and used in loop 2, so it is a global allocno and
  local-alloc cannot tie it), then reused as the fovy writer. Its refs 17 / len 52 give pri 13076 (GDBG), above buf's 3105, and
  it crosses the two calls, so it is the FIRST pass-1 allocno (r31) -- the whole-function permutation of the earlier notes
  ("buf r29 vs r31") starts there. Form applied: `f32* fovy = (f32*) (buf + 0x10);` at the top, `rec = (CameraAreaRec*) fovy;`
  after the header stores, the block-local `f32* fovy` of loop 2 removed (variant.sh: size 0x534 -> 0x538 exact, .text 0x1600,
  the nameless 0x3B8 block's 24 branch-reloc words become identical because its address is right again). A plain `u8* p` + `rec = p`
  without the reuse folds back into one pseudo (142, size short): the reuse is the fact, the type is not.
- **tcDataExport remaining 193 words, all allocation (read off GDBG / the listings, not closed):**
  (a) callee-saved order: target pTc-high r30 BEFORE buf r29; ours buf r30 (pass 1 after r31) then the block-0 pTc high r29. The
  block-0 pTc high (refs 4 len 54 calls 1, pri 1481) can only precede buf if buf's priority is below 1481, i.e. buf with <= 15
  weighted refs (ours 25: 3 hdr stores, 2 strncpy args, memclr, the 4 loop-2 `- buf`, loop-4/5 preheader subfs, `d - buf`,
  the depth-2 `cc - buf`, size). Offsets through `(u8*) hdr` do not help (cse merges hdr into buf, buf is the class head). Not found:
  which ten buf refs the original did not have (a helper computing the offsets? a second base pointer that stays a separate
  pseudo because it dies later than buf?).
  (b) loop 2 `mr r6,r5` (pp = pos) kept in the target, tied in ours (pos takes pp's r6 by copy preference; r6 is free in the
  loop header/latch where pos lives). Placing `pp = pos` before the `switch` keeps a copy (cse cannot cross the join label) but
  the target's copy sits AFTER the switch next to its uses (174 words, size +4). `d->pos`/`at` written from `pos` instead of `pp`
  change nothing (cse canonicalises to pos, the older head). So in the target r6 was excluded for pos: a conflicting allocno in r6
  during the header/latch, or `regs_someone_prefers`; not identified.
  (c) target cd (the tcCdat row pointer) r3 with the `+0x32c` giv base r4 and roll r5; ours cd r29 (pass 1: every caller-saved
  reg is busy in the body), giv base r3, roll r4, fovy r5. Follows from (a)/(b) and from loop 4's `i`: ours splits `i+1` into a
  new pseudo (`addi r6,r5,1` at the body top + `mr r5,r6` at the latch, loop.c makes three copies `352 = 95 + 1`, one per path
  into the increment), the target increments in place (`addi r6,r6,1`). `i++` as the last body statement instead of the for
  header turns the row address into a giv (`subi/addi` pair, 192 words, size +8): wrong. Loop 5's `i++` inside the body is in
  place in both. Why loop 4's for-header increment is split in ours and not in the target is the next thing to read (jump/loop
  dumps: uids 1189/1192/1198).
- **tcSetBesideOffset 27 (read only, unchanged):** GDBG order is 157 (`i+1` of loop 2, refs 4 len 24, pri 3333) -> pass 1 r3 (pass 0
  skips r3 through `regs_someone_prefers`: c = `mr r28,r3` prefers r3 and ranks below at 2891; pass 1 ignores smpref and r3 is the
  first non-conflicting register in REG_ALLOC_ORDER 0,9,11,10,8,7,6,5,4,3,31..), then 209 (n*4, 3292) -> pass 1 r3 and
  207 (n*12+0x18c, 3214) -> pass 1 r31; 157 and 209 share r3 (different loops). Target: 207 r3, 209 r31, 157 r31. The order that
  gives it with no other change is 207 > 209 > 157 (207 pass 1 r3, 209 pass 1 r31, 157 pass 0 r31), i.e. 157's priority below
  3214 (len >= 25) AND 207 before 209 (the 84/82 live-length tie of the earlier notes). Alternatively 157 first with a hard r3
  conflict, but every r3 clobber inside loop 2 also hits c (live everywhere) and removes c's preference. No pin possible: 207/209
  are loop.c givs, 157 is loop.c's increment copy. Left.
- **db_widget DB_STRING ctor 7 (unchanged, pin kept):** member-initialiser forms (`: str(0), len(0)`, `.., max(max_)`, all seven
  members), `str = NULL; len = 0;` as statements, `asm("li %0,0" : "=r"(zero))` with either store order (11 = the unpinned
  local-alloc naming of pass 12), the same with an `"r"(max_)` input (13). `ninja -t targets | rg db_widget` lists only
  build/G4BE08/src/t_esp/db_widget.o: src/t_esp/db_widget.cpp is built once, no second copy to keep identical. `fn_t_esp_2671C`
  = name-only (target name vs our nameless 0x3B8 block at .text+0x3628), not a code difference.
- fn_t_camera_1B8C4 is the nameless cManager<cLight> linkonce block (light.h); its 24 "words" were the branch relocs naming
  `<nameless@0x1244>` because tcDataExport was 4 bytes short. Gone with the size.

### Tool RELs, t_esp pass 15 (t_esp 208/212: InitTool 2916 -> 2880 words, two zero-code items: the EDIT row-loop preheaders (segs 37/67/100, `num[3]` nameTbl + the `tbl` table pointer) exact, dead sets 118 -> 116 (N 5235); the LOAD/MODEL window region read (sched1 call-crossing + 3 more callee-saved highs/FP constants in the target), not closed; nothing flipped; Load/SaveEmType 2/2 and fn_t_esp_3DE4C 24 untouched; 2026-09-12)

- Harness ~/.cache/tesp15 (kept for the next pass, delete when InitTool is closed): `seg.py T.s O.s [summary|A [B]]` = per-`bl`
  segment compare of two dtk listings (symbols/labels/`bl` targets normalised; `summary` = one line per differing segment with
  insn counts and a difflib distance, `A B` = side-by-side), `cmpv.sh V.cpp [N]` = variant.sh --no-diff + dtk disasm + seg summary
  in ~/.cache/tesp15/o_<V>/ (3.5 s). T.s = the target's InitTool from build/G4BE08/t_esp/asm/t_esp/t_esp.s. Read segment
  counts and the summary head, never whole listings: the per-segment view is what made the two items below obvious.
- **Window-1 row loop `num[3]->nameNum/nameTbl` (seg 37, 2916 -> 2903).** Target `lwz r9,0xc(r30)` once for both stores, ours reloaded
  `num[3]` after the `nameNum` store (a MEM_IN_STRUCT store may alias the `num[3]` pointer slot). Form: `DB_NUMERIC* n = num[3];
  n->nameTbl = ..; n->nameNum = 256;` (the same `n` form every other window already used); the store order in the target is nameNum
  (0xc4) then nameTbl (0xc0) = sched1 LUID tie, which the source order nameTbl-then-nameNum gives (the reverse gave the reverse).
- **EDIT windows 2-4 row-loop preheaders (segs 67/100/129, 2903 -> 2880 with the dead-set refit).** Target `lis r9,g_editNum@ha;
  addi r9,r9,g_editNum@l; addi r30,r9,0x30` = the `num` giv initialised as `P + 0x30` where P is a pseudo holding `&g_editNum`,
  REG_EQUIV sym, spilled and rematerialised by reload into r9. Ours `lis r11; addi r30,r11,g_editNum+0x30@l`: `&g_editNum[i][12]`
  goes through get_inner_reference (bitpos 384, offset `i*172`) and `plus_constant((plus sym mult), 48)` folds the constant into
  the symbol (expr.c); `g_editNum[i] + 12` folds the same way (the tree fold reassociates `(A + &g) + 48` via split_tree, the
  ADDR_EXPR is TREE_CONSTANT). Unfolded forms that FAIL in InitTool: `num = g_editNum[i]; num += 12` (2-set `num` is no giv:
  `add r30,r11,r30` per preheader, 8165w), `row = g_editNum[i]; num = row + 12` (row a giv, num derived from it, 8444w), absolute
  indices `num = g_editNum[i]; num[12..21]` (the loop-body `lo_sum g_editNum` has life 1 -> loop.c "move-insn savings 1 not
  desirable" against 302 insns, so `num` is not a giv; in window 1 the same lo_sum has life 38 because cse's find_best_addr
  rewrote the offset-0 `num[0]` address to `(plus mult lo_sum)`, so it IS hoisted there; 3114w). Form that works (zero code, all
  four windows): `DB_NUMERIC* (*tbl)[43] = g_editNum;` right before the `for (i..)` and `num = &tbl[i][12]` (`tbl[i]` for window
  1): `tbl` is a pseudo set in the preheader block, cse does not fold it into the loop body's `(plus (plus tbl mult) 48)`, loop.c
  makes `num` a giv with add_val `tbl + 48`, update_equiv_regs gives `tbl` REG_EQUIV sym -> rematerialised at the giv init.
  Segments 67 and 100 are exact, 129 has one FPR name left (f16/f18). The 4 extra `tbl` sets moved N 5235 -> 5237 (one slot 4
  bytes low from 0x2054 on, 7969w); 116 dead sets = 5235 again (the tree's base N was 5235, not the 5233 written in pass 14: both
  fit the slot order). Rule confirmed: after any InitTool insn-count change, read N from `-dG` and refit with the dead sets.
- **Seg 25 (window 1 preheader, 22 lines) = the reload round-robin phase, not closed.** Target picks r8 (lfd `LC1533` high), r6
  (`&g_editRowNo` high), r10 (lfs high); ours r6, r8, r10. RLDDBG prints only the lfd's address reload (insn 1355 -> r6, rr 1/7,
  previous reload = a call's LR r65, r0 busy with 0x4330); the two `high` operand reloads of the REG_EQUIV `(high sym)` pseudos
  (insns 1380/1360) are NOT printed by the hook, i.e. they get their register outside allocate_reload_reg's success path
  (choose_reload_regs inheritance/equiv or find_reloads' dummy-reload path); extend the RLDDBG hook to those paths before reading
  the phase again. The target order needs the lfs (1360) before the addi (1380) in the RTL order or r10 busy at 1380.
- **Segs 162-163 (MODEL window `new`, "LOAD window region"), read, not changed.** The target issues 6 callee-saved highs (`lis
  r25 g_pPrimArray, r19, r21, r16, r28, r27`) between `li r3,8` and the `bl __builtin_new`; ours' sched1 order already has 2 of
  them before the call (`r26 g_pPrimArray`, `r17 LC713`, greg dump insns 4642/4726 before call 4637) and sched2 moves them after
  it (a callee-saved hard reg has no dependence on the previous call in sched_analyze_1: only call_used regs get the
  last_function_call anti-dep; pseudos need REG_N_CALLS_CROSSED > 0, haifa-sched.c). The scratch highs (`lis r11/r10/r9/r4`) carry
  `REG_DEP_ANTI` on the call in both. The target also has 3 more callee-saved highs (11 vs our 8 around this window) and copies
  3 pool constants into f21/f20/f19 (`fmr`) = 3 FP values live across the following calls that ours reloads; the register names
  from seg 163 on follow from those extra live values (global.c order), so find the 3 highs / 3 FP constants first (candidates:
  the DB_POINT constants shared by the MODEL/LOAD/SAVE windows, e.g. 8.0f/72.0f/4.0f, and the highs of `g_pLoadWin`-family
  symbols or the CreateString label strings), then the sched2 position of the pre-call `lis`es.
- Not touched: the `k8/k6` spill-set asm (tag stays), Load/SaveEmType, fn_t_esp_3DE4C, dbg_tool.h/db_widget. Tree edits in
  src/t_esp/t_esp.cpp only (window-1 `n` form, 4 `tbl` locals, dead sets 116). Verified: locked ninja of t_esp.o, bytecmp 208/212,
  InitTool 2880. Not run: `ninja -k 0`, make_rel --verify, shasum (nothing flipped).

### Tool RELs, t_event closer 2 (t_event/t_event 168 -> 129 words, 59 -> 60/69: CallbackLoad 1 -> 0, CallbackSave 12 -> 9, SubToolMessInit 16 -> 15 (6 real + 9 tail-shift artefacts), SubToolMessMove 106 -> 72; nothing flipped; 2026-09-12)
Harness ~/.cache/tev2 (deleted): variants judged with `tools/research/kit/variant.sh t_event/t_event <v.cpp> FUNC`, dumps with
`tools/research/kit/rtl.sh ... -dj -ds -dL`, allocation order with `GDBG=1 CC1DIR=tools/research/sngdbg`. Every finding below was verified on a
variant before the tree edit; the tree object rebuilt under the lock; modules.py untouched (unit not identical).

- **HDRead's buffer word (CallbackLoad 1 -> 0, SubToolMessInit 16 -> 15): the read is in the ARRAY OWNER, not in the parser
  inline.** `EvtMessRead(m, path) { d; tmp; buf; u32 size; XmlNodeDataClear(&d); m->num = 0; memset(m); size = HDRead(path, buf);
  if (EvtReadXml(path, &d, tmp, buf, size) == 0) ..}` with `EvtReadXml(name, d, tmp, buf, size) { buf[size] = 0; if (size >
  XML_BUF_SIZE - 1) ..; if (size == 0) ..; cur = buf; ..}`. Mechanism (read in integrate.c/cse.c): a `buf` PARAMETER of an inline
  gets a CONST_AGE_PARM equivalence to `(plus vsv N)` and `subst_constants` rewrites the call-argument set to `(set r4 (plus fp N))`,
  which cse1 folds back to the parameter pseudo when the copy is in the same ebb (`mr r4,r27`); the owner's own `HDRead(path, buf)`
  emits `(set r4 (plus fp N))` with NO equivalent pseudo yet (the `buf` pseudo r27 is created later, by the parser inline's parameter
  copy, and PRE'd to bb 0 in both) -> `addi r4,r1,18000` = the target. `size` as an extra inline parameter changes nothing else
  (the limit literal still materialises after the call).
- **HDWrite's buffer word (CallbackSave 12 -> 9, the reverse symptom): the write is in the array owner too, with `cur - buf` computed
  THERE.** `EvtWriteXml(d, tmp, buf)` returns `cur`; `EvtMessWrite: cur = EvtWriteXml(&d, tmp, buf); HDWrite(path, buf, cur - buf);`.
  `cur - buf` forces `(plus fp N)` into a pseudo right before the argument loads (expand_binop operand), cse1 rewrites the r4 set as
  a copy of it (`mr r4,r14`), and reload_cse then swaps the lower-numbered r4 into the subtraction (`subf r5,r4,r5`); the inline
  parameter form keeps `(minus cur (reg buf))` (substitution invalid there) and a fresh `addi r4` for the argument.
- **Back-search loop of SubToolMessMove (106 -> 72, TAGGED `candidate (loop.c biv elimination)`):** the target keeps `j` as the counter
  (`subic. r11,r31,1; blt` entry, `subic. r11,r11,1; blt` in the loop) while ours eliminated the biv into a pointer compare (`-dL`:
  "Biv 1667 initialized at insn 2769: initial value is complex ... biv 1667 was eliminated"). `asm("" : : "r"(j))` after the loop
  keeps it. Still open in that loop (~15 words): the target's giv inits are `subi r3,r29,0x18` (e - 24) and `mr r9,r29; subi r9,r9,8`
  (e - 8) and its `mulli r27,r31,0x18` (no*24) lives in a callee-saved register across IsWorkAlive, i.e. loop.c emitted the giv
  initial values as `no*24 + m - 24` (expand_mult_add distributing a `(plus no -1)` initial value) and cse2 folded `no*24 + m` into
  `e`. Ours emits them from the peeled copy (`mr r11,r10`). valid_initial_value_p accepts only REG/CONSTANT, so the biv's init
  must have been a register whose product cse2 did NOT share with the peeled `mulli`; `for (j = no; j > 0 && (p = &m->elem[j -
  1])->messNo == -1; j--)` (75 words) reproduces the callee-saved `mulli` and the `e - 24` peeled address but tests `mr.; ble` on j
  instead of `subic.; blt` on j-1 (the giv `j - 1` is folded into `j*24 - 24` by cse1 before loop.c sees it, so it cannot replace
  the biv in the compare). `while` / body-assignment spellings: 84. Not closed.
- **CallbackSave's five `add r3,r3,r27` (target) vs `add r3,r27,r3` (ours) for `d.node[d.num].s[X]`:** the RTL at expansion is
  `(plus (reg 91) (reg mult))` (reg 91 = the inline's frame-base pseudo, `d` at offset 0 of EvtMessWrite's frame), the target has the
  mult first. Tried without effect: `(d.node + d.num)->s[X]`, `d.num[d.node]`, `&..[0]`, `(u32) d.num`, a `XmlNode* n = d.node`
  base (42, worse). expand_binop swaps only for `op1 REG && op0 !REG` or `target == op1`; combine's 3-insn split would reorder but
  fails with the `lwz` of d.num as i1. Not found (5 words). The remaining 4 words are the clear loop's PRE pair (`i-1` / `n+176`,
  pseudos 275/279 at global priority 6666, allocated by allocno order = gcse bucket order; CallbackLoad has the target's order,
  CallbackSave and SubToolMessInit the reverse) -- the `.LC`/pseudo-count lever of the catalogue row 2, not applied.
- **mesCnt block of SubToolMessMove (still open, ~20 words), mechanism read this pass:** gcse cprop scans only SET_SRC
  (`find_used_regs`: `case SET: x = SET_SRC (x)`), never a store's address, and `(set (mem) (const_int 0))` is invalid, so a
  `no = 0` pseudo used in store ADDRESSES inside the loop survives cprop, its `(ashift no 2)` becomes `(set T 0)` (hoisted), and cse2
  in the preheader makes T and the stored zero copies of `no` -> `stwx r27,r30,r27` (`no` as index AND value). The loads
  `mesCnt[no+1]`/`mesCnt[no]` in the eprintfs fold their index (same ebb as `no = 0`) but keep the PRE'd `&mesCnt[1]` / `&mesCnt[0]`
  pseudos as bases (`lwz r8,0(r28)`, `lwzu r8,0xc0(r30)`). What is NOT explained: the third store `stw r31,4(r26)` (mesCnt[no+2]
  rebased on `&mesCnt[1]` with the index folded) while the first two keep the index register; ours folds all three.
- **Header-side (include/dbg_tool.h, not edited):** with `case 5: ret = 0;` last in `cDbgToolMain<T>::Update()` (current header) our
  build still emits the arm as `li r20,0; b end` after case 4 (+0x830) where the target has `li r20,0` at the region end (+0x8c0)
  falling into the caller's `cmpwi r20,0`; jump optimisation moves the one-insn arm. Needs a header experiment (`default:` arm, or the
  arm ending differently); it costs SubToolMessMove 2 words + ~8 branch-target words. The inlined WinUpdate/copy-loop region
  (+0x654..0x7d0: `li rX,0` / `mr r28,r23` order, r25-r29 permutation) is header-owned too.
- **Applied after the pool fix (each step verified with variant.sh, then the locked ninja; all pure C):**
  * own locals declared `mode, adxibuf_p, adxwk_p, nfrm, width, height` (colours mode r21 > adxibuf_p r20 > adxwk_p r19 > nfrm
    r18 > height r17, width still the pass-1 spill) — 247 -> 240w;
  * the frame-table block as `static Sint32 mwsfcre_MallocFrmTbl(mwply, cprm, frmtbl)` returning `ret`: the helper's `ret` is the
    temp numbered between the cwk2 and picusr_p Malloc results -> frmret r26;
  * picusr_p/fname_p through `static void *mwsfcre_MallocX(mwply, size) { return MWSFD_Malloc(mwply, size); }` (folds the constant
    like the direct call, but makes the result a TWO-level temp like the MallocWk ones): inlined helpers are cloned breadth-first, so
    a level-2 result is numbered after every level-1 result; with two direct calls and two wrappers the order was picusr, fname,
    hnwork, buf700 — all four through wrappers gives the target's call order picusr r25 > hnwork r24 > buf700 r23 > fname r22 (182w);
  * `Sint32 mode = cprm->mode;` at the declaration (the macro no longer assigns it): the target loads mode BEFORE nfrm/width/height;
  * MWSFCRE_CALC_BUFSIZ arms with the computed sizes (`sjb = ..; vib = ..;`) FIRST and the constant stores after: the backend-merged
    `li 0` then has a higher temp id than the bps chain and is coloured first (`li r3, 0`, chain r4/r5..) — 178 -> 146w;
  * mpvpara block: `Sint32 h; Sint32 w;` (h declared first -> h r0, w r3) and `cwidth`/`cheight` stored before `width`/`height`
    (the `&mwsfd_mpvpara` address temp then colours r6 after adxtpara r4 and the zero r5) — 146 -> 126w.
- **Negative results (do not retry):** a top-level `Sint32 hnwksiz = 0x4000;` local, a reused multi-def `size = 0x4000;`, and
  `crepara.hnwksiz = 0x4000; MWSFD_Malloc(mwply, crepara.hnwksiz)` (the struct-member form keeps `li r3, 0x4000; cmpwi` but adds a
  real `stw` to the frame); MallocWk made extern (numbering unchanged: FIFO cloning does not depend on linkage); IsUseAdxt spellings
  for the target's second dead `b` (`case 4:` first/last, `mode = 4;` body, `goto ok`, `return TRUE` per case, a `Bool ret` local
  — the frontend forwards `case 4` to the default label, or emits a second `li r0, 1`); declaration order / parameter form of the
  helper's `nfrm` (block-2 colours follow the PCode first-appearance order of the temps, not the declaration order: `nfrm =` after
  the width/height loads gives nfrm r19 / height r20 but moves the `lwz nfrm` below the buffmt-error block, 177w).
- **Residue 126w (13 lines):** (1) block 2 of the inlined MallocFrmTbl: target nfrm r19 / height r20 / frmtbl ptr r20, ours r20 /
  r19 / r19 — the target's nfrm load is the first instruction of the block yet coloured before height (a temp with a higher id whose
  load the scheduler still hoists over the width/height loads: not a helper local, not a parameter; open); (2) the target's second
  `b end` in both inlined IsUseAdxt switches (`b end; b end; li r0,0`): the frontend forwards an empty `case 4:` to the DEFAULT label
  (AST `CASE 0x4: L@773 = DEFAULT`), so a separately laid-out `b` block needs a case body the frontend keeps and the backend deletes
  — not found (pass 16a's `ret = FALSE` dead-store shape gives a different tree here). No tag applied: 2 words of dead code shift
  every later branch offset, so this is the only blocker left besides (1).
- mwPlyCalcWorkSfd 4w unchanged: `total = sibsiz + size; return total;`, a `SumWk(size, sib)` helper with a `ret` local, `size2 =`
  all re-rank the CWS_BUFSIZ zero temps (13w) or bounce through `mr`. Flags: `lib/mwsfdcre.c` stays False (8/10); objects.py
  untouched; the tree object was rebuilt through the locked ninja after every applied step (bytecmp 8/10, 126w + 4w).

### Tool RELs, t_id pass 5

- idEditPos 334w (size 0x10ac vs 0x10d4) -> IDENTICAL (one #17 pin). Structural reading in order: (1) the ID_DRAW_GUIDE
  eprintf's `sx + (sx > 0x198 ? -0x68 : 8)` computed as `int dx/dy` statements before the call (loop.c scan_loop
  lines 904-1000: a single-use invariant pseudo in a loop with calls gets its set deleted and the source substituted
  into the use when `no_labels_between_p`; a `?:` in the argument list puts the join label between the plus and the
  arg set, so the `add r4,r24,r25` stays inside the loop only when the col/ofs is a statement); `p = *pos` not
  `p = d->pos`. (2) subCur-0 arm: `PSVECAdd(&d->vtx[i], &d->pos, &d->vtx[i])`, the `a = vt & 0xF` read as a block
  `{ int t = d->vtxType; a = t & 0xF; } vt = d->vtxType;` (two loads, the target's order), case 0 =
  `PSVECAdd(&vtx[0], &vtx[2], &pos); PSVECScale(&pos, &pos, 0.5f)`, cases 1-4 `d->pos = d->vtx[N]` (struct copy:
  rs6000 expand_block_move copy_addr_to_reg pseudos -> "first word via base+const, y/z via pseudo": cse.c
  find_best_addr with ADDRESS_COST 0 rewrites `(mem S)` to `(mem (plus base const))` for the first word only).
  (3) subCur case 5 (subStep=0; editStep++) placed after case 4. (4) menu loop `for (i = 0; i <= 5; i++)` with
  `y + i * 0xE` inside the inner eprintfs and `y += 0xE` after the POS-MENU eprintf (gcse PRE puts `addi r24,r25,0xe`
  at the arm tails; `y + 0xE + i*0xE` folds to addi-after-add = worse). Inner loops: `int col = 7; if (j == ..) col = 0;`
  (if-form puts `li 7` before the compare, ternary after), the onOffName loop as `u32 ofs = j * 4; ...
  *(const char**)(ofs + (u32) onOffName)` (address kept as a giv), IPOW loop `(u8) col`. (5) editStep 2 / subCur 1
  / subStep 0: `Vec t; Vec* pt = &t; pt->x = mat[0][3]; pt->y = mat[1][3]; pt->z = mat[2][3];` then `ofs.x = t.x;
  ofs.y = t.y; ofs.z = 0.0f` - the store order x,z,y comes from haifa rank_for_schedule INSN_REG_WEIGHT (sets minus
  dead notes; the pseudo `pt` dies at the z store), not from source order; frame-based `(plus fp N)` and pseudo-based
  `(plus S N)` stores conflict in alias.c (reg_known_value only from REG_EQUAL/EQUIV), which serialises them.
- Last 3w (IPOW loop `li r5,7 / li r5,0 / clrlwi r30,r5,24`, ours r30 for col): global.c set_preference strips ONE
  operator level (`GET_RTX_FORMAT(code)[0] == 'e'` -> src = XEXP(src,0)), so `(set ext (zero_extend (subreg col)))`
  with ext already local-alloc'd to r30 gives col a hard_reg_preference for r30 (GDBG `pref 000000000000000010`), and
  find_reg honours a preference after the pass-0 scan whenever the reg is in regs_used_so_far and free of conflicts
  (r30 is in used_so_far from local-alloc's own assignments). The target's col has no such preference, so its ext
  pseudo was not local-alloc'd (multi-block or two deaths) - shape not found in 20 min; `u8 col` / `s8 col` /
  `u8 c = col` / shared `int col` / `col & 0xFF` all reproduce ours (u8 var: promoted, no clrlwi at all). Applied
  `register int col asm("r5"); // COMPILER-DIFF: #17`.
- mdiff.py (scratch, masked structural diff of dtk target vs ours with `--regs`): the `b .L` vs `b idEditRot` and
  `bl .L` lines are dtk relocation artefacts, ignore. The 1-4w diffs in `create__t8cManager1Z6cLighti` vanished with
  the size fix; `_._6cCoord/_._7ID_DATA/_._5cUnit` 2w and `__static_initialization_and_destruction_0` 4w remain.

### Tool RELs, t_esp pass 16 (t_esp 208/212: InitTool 2880 -> 2061 words: segs 25/0/3 statement order, 12 CreateButton ids (real value bugs, 0 -> 1/2), and the cse1 1001-insn hash-flush grid: the target has ~4 more cse1-time insns per window ctor, reproduced with a tagged 4-dead-set pad in all 48 ctors (dead sets 116 -> 76, N 5235); segs 129/132/135 and most of 176..340 exact; not closed, nothing flipped; 2026-09-12)

- InitTool 2880 -> 2861w (diffsegs 485 -> 482, total_d 4109 -> 4103), all three steps plain C statement order, no insn-count
  change (segment sizes identical, so the 116 dead sets / N 5235 stand). Segments 0..122 are now exact; 123 is the dtk label
  artefact. Flag stays False (208/212). Working tree was reset by a history rewrite at 01:07 (nothing of this pass was in
  the tree yet; `/tmp/t16/base.cpp` = pre-reset source); edits re-applied and re-verified through the locked ninja.
- seg 25 (window-1 row 0, reload rr r6/r8/r10 vs r9/r10/r11): the pass-15 note that the RLDDBG hook misses the `(high sym)`
  operand reloads was wrong for the current source (all three print: insn 1349 -> r6 rr1/8, 1374 -> r8, 1354 -> r10); the
  reload order follows the sched1 stream order, and at cycle 2 the second issue slot is a priority-156 tie between `li r26,0`
  (`int sx = 0`) and `r565 = lo_sum(high g_editRowNo)` (`&g_editRowNo[i]`), both weight +1 (the PRE'd `high g_editRowNo`
  pseudo has 3 uses and never dies), decided by LUID. Fix: `u8* no = &g_editRowNo[i];` declared before `int sx = 0;` and
  passed to CreateNumeric. Windows 2/3 were already exact because their `sx = -1` is a shared callee-saved constant (no `li`).
- seg 0 (`lis r20,g_cinesco` before `lis r9,g_evCam`): sched1 tie of the two `high` insns, LUID -> source order `g_cinesco = 0;`
  before `g_evCam = 1;` (the stores themselves keep their order).
- seg 3 (`stw r26,g_dirLocal` before `stw r11,4(g_dir)`): sched1 tie at priority 128 between the strcpy's second word store
  (weight -1, the loaded word dies) and the g_dirLocal store (weight -1, its `high` pseudo dies; the constant 1 in r26 is shared
  with seg 18 and does not die), LUID -> `g_dirLocal = 1;` before `strcpy(g_dir, "X:/Soft/");`. In sched2 the frame spill
  stores (no MEM flags, base r1) and the `mem/s` strcpy stores through r7 (unknown base) form an output-dep chain in RTL order,
  so the sched1 order decides the whole 6-store tail.
- segs 162-163 (MODEL window, 6 callee-saved `lis` before `bl __builtin_new`, `lwz g_pEditWin1` late): NOT a callee-saved
  register/lifetime question. In sched2 a `lis rN` of a callee-saved reg has no dependence on a call either way, and the six
  `lis` (g_pPrimArray, 4 CreateString labels, g_pModelWin) have low priority; in ours every issue slot before the call is taken
  by the `lwz e->win / stw active / stw *slot / lwz g_pEditWin1 / stw g_pEditActive / li r3,8` chain. In the target the
  `lwz r0,g_pEditWin1` is TRUE-dependent on `stw r29,0(r6)` (`*slot = e`): it issues two cycles after that store, the call
  slips 3 cycles, and the six `lis` fill the 6 free slots (target order c1 lwz e->win+lis r6, c2 addi r6+lis r11, c3 stw
  active+lis r10, c4 stw slot+li r3, c5 lis r25+lis r19, c6 lwz+lis r21, c7 lis r16+lis r28, c8 stw+lis r27, c9 bl). Ours has
  no such dependence: `slot` (reg/v, `TOOL_WINDOW*&` parameter) is set once to `lo_sum(high g_pEditWin4)`, so alias.c gives
  the store base = symbol g_pEditWin4 and base_alias_check says "differing symbols never alias" (also via canon_rtx of the
  REG_EQUIV). The target's slot pointer therefore has NO known base and no REG_EQUIV/REG_EQUAL known value (multi-set pseudo,
  or set once from an opaque source), yet is materialised as `lis r6; addi r6,r6,@l` right before the store. Confirmed by
  laundering the address: `asm("lis %0,%2@ha" : "=r"(hi) : "r"(e), "i"(&slot)); asm("addi %0,%1,%2@l" : "=r"(ps) :
  "r"(hi), "i"(&slot)); *ps = e;` reproduces the target's 19-insn pre-call shape exactly (segment 162 19/19) but with
  local-alloc names (r9 for the address, r11 for e->win vs target r6/r9) and it shifts N (seg 0 d12) and window-4's
  allocation (segs 123-128) because `e` gains a use; total_d 4159 > 4103, so NOT applied. `asm("":"+r"(ps))` on `&slot`
  puts the address in the constant pool (the in/out asm operand reload forces the symbol to memory) - do not use that form.
  Tried and rejected natural forms: `TOOL_WINDOW** slot` + `&g_pEditWinN` (identical RTL to the reference), one InitTool-scope
  `TOOL_WINDOW** pp` assigned before each call (pp lives across the windows -> callee-saved, 8071w). Not viable in theory:
  `&g_pEditWin1 + 3` / array forms (base equal but memrefs_conflict_p separates the offsets), `*pp++` (PLUS on dest keeps the
  base), volatile (both MEMs must be volatile). Open: find the source form whose slot pointer is a block-local non-REG_EQUIV
  pseudo (base 0) that local-alloc puts in r6 with e->win in r9, or a REG_EQUIV pseudo whose REG_EQUIV value has no base
  (a MEM equiv would be rematerialised as a load, so no). The three `fmr f19/f20/f21` pool constants and segs 129/132/135
  FPR names (f16/f18/f17 vs f18/f21/f19) were not reached; they still look downstream of the MODEL window's FP constants.
- Kit note: `~/.cache/tesp15/` (seg.py, cmpv.sh, T.s) kept; `/tmp/t16/sbs.py T.s O_init.s a [b] [ctx]` prints segments side
  by side with real symbol names (dtk labels on the target side): `python3 /tmp/t16/sbs.py ~/.cache/tesp15/T.s
  ~/.cache/tesp15/o_<V>/O_init.s 162 162 30`.
- **12 CreateButton ids were wrong in the source (2861 -> 2851w, +1 insn, N still 5235).** A scan of every segment's `li`
  immediates (`li r9,N` = the 7th CreateButton argument) showed the target passing sequential widget ids where ours passed 0:
  MODEL `[LOAD]` 2, LOAD_EM `[LOAD]` 2, LOAD_ROOM/LOAD_SST `[Load]` 1, LOAD_EVENT `[Load]` 2, SAVE_EM `"  "` 1 + `[SAVE]` 2,
  SAVE_ROOM/SAVE_SST `[SAVE]` 1, SAVE_EVENT `"  "` 1 + `[SAVE]` 2, DATASET `[Load]` 1 (rule: ids count the window's
  selectable widgets in creation order). Segs 215/247 exact, 218 7/7. Check the `li` multiset per segment before chasing
  scheduling: the remaining whole-function difference is `li 1` 143 vs 127 (the target rematerialises 1 where ours shares r26)
  and one `li 2`/`li 3` pair (segs 378/549, constant sharing across a flush, see below).
- **LOAD_EVENT..SAVE FP constants, segs 129/132/135 FPR names, seg 163's `fmr f21,f25; fmr f20,f22; fmr f19,f24` = cse1's
  hash flush position, MECHANISM FOUND, natural form not found.** cse.c cse_basic_block flushes the table every 1001 non-NOTE
  insns (`num_insns++ > 1000`, count reset); the path after the window-4 row loop is one block (`;; Processing block from 4610
  to 0, 13528 sets.`, no label after 4609 in InitTool). Ours: LOAD_EVENT's 164.0f load is insn 989 of that path, the flush
  hits at 1002, 12 insns later, so LOAD_EVENT's pos/h fold into MODEL's f24/f22/f23 and SAVE_* reload from the pool. Target:
  the flush precedes LOAD_EVENT's loads, they become fresh pseudos (f21/f20/f19), SAVE_* fold into THOSE, cse2 turns the three
  loads into copies of MODEL's regs and sched1 hoists the `fmr` up to seg 163; 192.0f then takes the dead f25. Verified with a
  scratch `int d; d = 1; .. d = D;` block (deleted at cse1's end, so it counts for the flush but not for gcse's N) placed after
  `g_pEditActive = g_pEditWin1;` plus 4 fewer `i = k` dead sets (the three unfolded loads + 1 are real insns at gcse): D >= 13
  gives seg 163 the `fmr` copies, seg 217 `lfs 192.0` into f25, segs 129/132/135 exact, segs 220/238 same size, region
  162-340 total_d 637 -> 526..475; but every later flush shifts by the same D, region 341-660 goes 2245 -> 2360..2390 and
  661+ 1186 -> 1330 (words 2851 -> 3150 at D 13-16; D >= 18 loses one entry-block `&pos` slot (seg 0 d52), D >= 30 breaks N).
  Using the loop variable `i` for the block-local sets instead changes window-4's allocation (seg 138) - use a fresh local.
  So the target has 13..183 more cse1-time insns between label 4609 and LOAD_EVENT's `lfs 164.0` than we do, gone by the
  final code, and probably a different insn profile after it (the later regions want a different shift). Candidates not yet
  tested: per-window insns cse1 itself deletes (folded loads/copies: 5 windows x 3), the target's seg 162 `lwz g_pEditWin1`
  form. Nothing applied; `/tmp/t16/gen2.py D 112 OUT` regenerates the probe from the tree source.
- **The flush grid is per window: `TOOL_WINDOW_CSE_PAD()` applied (2851 -> 2068w, diffsegs 481 -> 375, total_d 4082 -> 3129).**
  A flat shift D of the first flush (scan D 13..57 with N refit, `/tmp/t16/scan.sh`) never fixes the later regions (best D 23:
  2593w, regions [14, 558, 2232, 1033] for segs 0-161/162-340/341-660/661+ vs cur [14, 637, 2245, 1186]); K dead sets of a
  fresh local at the top of EVERY window ctor (`{ int d_; d_ = 1; .. d_ = K; }`, 34 ctors) shift each later flush by K per
  window: K=3 2801w [64, 446, 2309, 1192], **K=4 2068w [2, 292, 1848, 987]** (seg 0 exact, region 0-161 = the dtk artefact
  only), K=5 2284w [74, 567, 2001, 873], K=6 2646w. So the target's ctors carry ~4 cse1-time insns each that we lack; they
  survive to gcse (N rises 8 buckets per K, so the InitTool dead-set block is refit: 80 sets with K=4 = 5235). Applied as a
  tagged macro (`#define TOOL_WINDOW_CSE_PAD()` above `g_pMenuWin`, called first in each `XXX_WINDOW(DB_PRIM_ARRAY* p)` ctor).
  The natural form is unknown: 4 register-only insns per ctor that flow deletes (dead sets) or that cse2/combine fold; the
  ctors' `pa = p; win = NULL;` and the DB_POINT/w/h/flg locals are all real stores in both binaries. With the pad, seg 163 has
  the target's FP names (f25/f24/f23 loads, f21/f20/f19 copies) but the three `fmr` and `li r29,0` sit after the
  CreateNormalWindow call (seg 164) instead of before it, and the 6-`lis` shape of seg 162 is unchanged - those remain the
  seg 162-163 scheduling item above. Per-window K may differ between ctors (only uniform K was scanned):
  `/tmp/t16/gen3.py K SETS OUT` and `/tmp/t16/scanw.sh K...` regenerate/scan from `/tmp/t16/cur.cpp` (pre-pad source).
  The 14 macro-defined ctors (VEC_WINDOW_CLASS, WORK_WINDOW_CLASS, WORK_WINDOW_CLASS_U: `cls(DB_PRIM_ARRAY* p) {`) were missed
  by the first regex; padding them too gives 2061w [2, 292, 1848, 971] with 76 sets (N 5237 -> 5235). Padding at the ctor END
  (after `win->active = 0`) is worse (K=4 2636w, seg 0 d40); per-window K overrides for MODEL/LOAD/LOAD_EM (3 or 5, others 4)
  are all worse than uniform 4 (`/tmp/t16/gen5.py SETS OUT NAME=K`, `/tmp/t16/scan5.sh NAME=K`). Tail windows 698..761 (the
  WORK*/VEC* windows): the target materialises `flg = 4` freshly in each (`li r10,4`) while ours shares one pseudo (`li r31,4`
  hoisted to seg 217) - a constant-sharing difference of the same family (whole-function `li` multiset now differs only in
  0x0 179/188 and 0x4 64/55); not read.

### Tool RELs, t_camera_data closer 2 (tcDataExport 193 / tcSetBesideOffset 27 / DB_STRING ctor 7 unchanged; mechanisms pinned down, no source change adopted; 2026-09-12)
- **tcDataExport buf/pTc-high order (item a) is NOT reachable through buf's refs.** Weighted refs of buf (reg 82) at lreg
  time are exactly the 25 in the final code (`python3 ~/.cache/tcam2/refs.py <lreg-dump> 82`: 9 at depth 1 incl. the
  memclr arg copy, loop-2 4x`- buf` at depth 2, loop-5 `d-buf` 2 and `cc-buf` 3, three hoisted giv inits at depth 1).
  pTc-high (reg 360) is the gcse-PRE pseudo of `(high pTc)`: cse2 gives its copy insn a REG_EQUAL `(high pTc)` note, so
  local-alloc's update_equiv_regs doubles its REG_LIVE_LENGTH (27 -> 54, pri 1481) - the target has the same doubling.
  buf would need <= 15 weighted refs; no source shape gives that. The swap must come from another allocno taking r30 in
  pass 1 before buf: loop-4 `d+1` (reg 356, pri 4000, allocated before buf) does exactly that in the target (target
  loop 4 has all of r0,r3-r12 busy so it falls to pass 1 -> r30); in ours it finds r7 free.
- **Every `X+1` copy in this function is gcse-PRE (lcm.c block LCM), not loop.c.** `compute_latein` sets
  `latein = delayin` for every non-last block, so an expression anticipatable at a loop header is inserted at the end of
  EVERY block where it can be delayed and that reaches the deleted occurrence. Loop-4 `i = i + 1` at the latch (BB 29)
  gets `352 = i + 1` inserted in BB 24 (body top), 25, 26 (uids 1193/1196/1202); cse2 deletes 25/26 as redundant, so
  the final code has one `addi rA, rI, 1` at the body top and `mr rI, rA` at the latch. The target's in-place
  `addi r6, r6, 1` right after the `mulli` is the SAME insertion with `352` allocated to i's register (i dead there):
  i must sit in a register that is free across BBs 25-28. Ours: i (reg 96, shared with loops 1-5, refs 18 len 170) ->
  r5; 352 -> r6 (first free in REG_ALLOC_ORDER). Do not try `i++` in the body (V1: i becomes a biv, givs reduced,
  `subi/addi` pair, size +8). The d+1 copies (loop 2 uid 1190 -> reg 361, loop 4 uids 1199/1205 -> reg 356, loop 5)
  are PRE too and identical in the target (r30 there because of the pass-1 fall-through above).
- **j vs loop-2 giv base is a priority knife edge.** j (reg 97, shared, refs 50 len 242 pri 10330) is allocated at
  order 118 -> r4; the loop-2 giv base `cd+0x32c` (reg 395, refs 38 len 184 pri 10326) at 119 -> r3. Target has giv
  base r4, j r12 (r3 excluded by regs_someone_prefers: buf's r3 param preference), cd r3, i.e. giv base BEFORE j. Any
  shape that lowers j's pri below 10326 or raises the base's above 10330 flips it. Tried: loop-1 counter split off
  (`k`): j becomes 37/160 = 11562, worse (V6, 193 unchanged). Per-loop counters for loops 4/5 only (V4) -> 177 words,
  size exact, giv base r4 but j4/j5 land in r11/r7 (target r12 everywhere = one shared j); all-loops-separate (V5) ->
  333 words size +0x18. Not adopted: no evidence for the V4 shape.
- **`mr r6, r5` (pp = pos, item b)** survives cse only if pp is the cse class head at the copy: cse.c make_regs_eqv makes
  NEW canonical iff `REGNO_LAST_UID(new) > REGNO_LAST_UID(old)` (and new lives past the ebb). pos is mentioned after
  loop 2 (`fp = pos`), pp only inside loop 2 -> pos is the head in ours and the copy dies. The target's minus uses pp, so
  in the target pp's last uid is later than pos's (pp mentioned again later in the function, or pos's last mention is
  inside loop 2). Not resolved.
- **Pins fail here**: `register int j asm("r12")` -> 217 words size +0x14 (frame changes); plus `register u8* buf
  asm("r29")` copy of the parameter -> 223 words size -0x18. Do not use pins in tcDataExport.
- Tools: `~/.cache/tcam2/refs.py <dump> <regno>` = weighted REG_N_REFS per insn from an lreg dump (joins multi-line
  insns, tracks LOOP_BEG/END depth, ignores notes). `~/.cache/tcam2/rtl_base/` has all dumps of the current source.

### Tool RELs, t_event closer 3 (t_event/t_event 129 -> 55 words, 60 -> 66/69, .text size now equal; header MakeSaveData shares ONE stepped pointer; mesCnt `no` moved into the loop's if; Tools/t_esp_area 7, Tools/t_lightarea 4, db_toolbase IDENTICAL x2; 111 OK; not flipped; 2026-09-12)
Scratch ~/.cache/tev3/: `hv.sh <dbg_tool.h variant> [t_event src]` judges a HEADER variant on all five includer objects
(sed's the `#include "dbg_tool.h"` to the variant's absolute path, variant.sh --no-diff each; baseline reproduces the tree numbers),
`ins.py <rtl dump> <pseudo,list>` (one line per insn touching the pseudos + the live-in block headers), `sbs.py`. Kit extended:
`KIT_NLINES=N` env raises variant.py's 400-row side-by-side cap (README updated).
- **Item 1 (Update() `ret = 0` arm) was already closed by H3** — the 0x3b40 `li r20,0` falls into `cmpwi r20,0` in both. No header work.
- **Item 4 closed (inlined MakeSaveData register permutation, 26w): dbg_tool.h now uses ONE `T* src` for the count loop and the copy
  loop** (function-scope, `src = work` twice). Mechanism (GDBG): src (reg 478 refs 19 len 40 pri 19000) was allocated before dst (479
  refs 18 len 41 pri 17560) and took r29 in pass 0; the target has dst r29 / src r28 in the copy loop AND w r28 / const r29 in the count
  loop, i.e. one multi-set pointer pseudo whose priority (refs 26 / len 69 -> 15072) drops below dst's, so dst is allocated first. Both
  loops then use r28 for the pointer, and the count loop's 0x11111111 constant (reg 474, pri 4166) gets r29 because dst is not live there.
  t_lightarea's own MakeSaveData call has the same shape and stayed at 4w ONLY with `cnt = 0;` BETWEEN the `work` load and `src = work`:
  cse.c's "(set REG0 REG1) where REG0 is cheapest" rule (cse_insn, the block after the `Special handling for (set REG0 REG1)` comment)
  fires when the previous insn is the SET of REG1 and REG0 (the multi-set src) lives past REG1's last use — it rewrites `lwz work; mr
  src,work` into `lwz src; mr work,src` (t_lightarea 4 -> 6w). With `cnt = 0` as the previous insn the rule does not fire.
  `make_regs_eqv`: a pseudo whose REGNO_LAST_UID is beyond the ebb and later than the class head's becomes the class head.
- **Item 2b mostly closed (mesCnt block, 46 -> 40w, size equal):** `int no = 0;` declared INSIDE the loop's `if` body (same ebb as the
  stores) and stored as the value (`EvtDebug.mesCnt[no] = no;`), third store `mesCnt[2] = i`. cse1 then knows no = 0 in that ebb: `(ashift
  no 2)` folds, the index and the stored value are canonicalised to the same class head `no` -> `stwx rN,rBase,rN` with one hoisted
  `li rN,0`. With `no` at the block top (previous form) gcse cprop cannot replace `no` inside `(ashift no 2)` (validate_replace_rtx_1
  only simplifies PLUS/MINUS/extend, so `(set T (ashift (const_int 0) 2))` fails recog) while it DOES fold `(plus no 1)`/`(plus no 2)`
  -> three hoisted constants 0/4/8 in separate registers (`li r24,0; li r25,0; li r26,4; li r27,8`). Open (~10w): the target forms
  `&EvtDebug.mesCnt[1]` as a pseudo at the block TOP (`addi r28,r30,0xc4` before the first eprintf), reads mesCnt[2] as `4(r28)` and
  mesCnt[1] as `0(r28)`, copies it for the loop (`mr r26,r28`) and stores `mesCnt[no+1]` as `stwx r0,r26,rN` and the third as `stw
  r31,4(r26)`; mesCnt[0] is `lwzu r8,0xc0(r30)` (the loop's `&mesCnt[0]` base formed by combine from `lwz (mem B0)` + `set B0 (plus E
  0xc0)`, so that base was a pseudo BEFORE the last eprintf too). fold does NOT distribute `(no + 1) * 4` (the ARRAY_REF index is
  converted first), so `mesCnt[no + 1]` cannot produce a `&mesCnt[1] + no*4` address; a struct-array view `((MesRec*) mesCnt)[no].no`
  gives the right base but loses the shared zero register (43w). Not found: what makes both bases pseudos at the top of the block.
- **Item 2a open (back-search giv inits, ~15w).** Read off the target: `mulli r27,r31,0x18` (A = no*24) is callee-saved because the
  lwzu base giv is initialised as `A + m` AFTER the IsWorkAlive call and reload_cse_regs then rewrites that `add` to `mr r9,r29` (r29
  = e = m + A already computed, same value) — hence the odd `mr r9,r29; subi r9,r9,8` pair; the p giv is `subi r3,r29,0x18` (combine
  folded its `mr; subi`). So in the original cse2 did NOT fold `(plus A m)` into e for the address giv, and both giv inits are in
  terms of `no` (biv initial value must be a REG or constant for loop.c, valid_initial_value_p), i.e. the biv is `j = no` and the
  index is `elem[j - 1]`, with the compare eliminated into the `j - 1` giv (`subic. r11,r31,1; blt`). Ours with exactly that source
  (`for (j = no; j > 0 && (p = &m->elem[j - 1])->messNo == -1; j--)`) gives 49w: `ble` instead of `blt`, A callee-saved as in the
  target (!), but the address giv comes out `(A - 8) + m` (`subi r0,r29,8; add r9,r0,r28`) and p reuses the peeled `m + (no-1)*24`.
  `while` and `for`+`break` spellings identical (49). The current `j = no - 1; elem[j]` + `asm("" : : "r"(j))` form (46w) stays.
- Remaining 55w: SubToolMessMove 40 (2a 15 + 2b 10 + register names downstream), CallbackSave 9 (`add r3,r3,r27` operand order 5 +
  clear-loop PRE pair 4), SubToolMessInit 6 (clear-loop PRE pair).

### Tool RELs, t_id pass 5 (continued: idEditColor, toolIdOption, idEditUnit, toolIdInit, toolIdEditDisp)

- idEditColor 132 -> 0, pure C. Mechanisms: (a) `const char** tbl3 = onOffName3` as a VARIABLE base for the inner
  `for (j = 0; j <= 1; j++)` keeps the biv j alive (`cmpwi j,0`, `lwzx`): loop.c `maybe_eliminate_biv` needs a giv with
  CONSTANT add_val to replace a compare-vs-constant; a symbol base gets the biv eliminated. (b) `u32 ofs = j * 4;` used
  twice (`*(const char**)(ofs + (u32) tbl3)` and `x + 0x40 + ofs * 8`) gives one giv with two uses -> `lwzx rOfs,rTbl` +
  `addi rOfs,rOfs,4`. (c) `int x2;` declared at the outer scope (with `step`) gets the earlier pseudo regno -> the earlier
  reload spill slot; `x1`/`y2` declared in the inner block. (d) `(int) ((f32) yy + 2.8f)` inline instead of `fy = (f32) yy`
  so `x + 0x40` is hoisted before the 0x4330 int->float constant materialisation (LUID order of the loop invariants).
  Inner loops `int col = 7; if (..) col = 0;` (not ternary) and `(j != 0) != ((d->x109 >> 2) & 1)` for the on/off compare.
- toolIdOption 65 -> 12, pure C. Mechanisms: (a) loop.c move_movables threshold `thr * sav * life >= ic` (LOOPDBG prints
  `[thr sav life ic]`); thr drops by 3 per moved insn, so short-lived single-use invariants stay in pass 1 and move in
  pass 2 (`-frerun-loop-opt`, smaller ic) -> they land AFTER the giv inits in the preheader. `mx = cx - 1` / `vx = cx +
  0xC` (cx set before the loop) right before their uses reproduces the target's late `addi` hoists; pre-loop `mx = 0x22;
  vx = 0x2F` hoisted too early. (b) cse1 ebb rules: a set placed after the if-join label is folded into the switch cases
  (cse follows a conditional jump when the target label has one use and is preceded by a BARRIER, i.e. case blocks after
  `b end`), so `sx/r1/r2` go right after the first eprintf, before the `if`. `mx = sx - 0xC` at body top folds to
  `li 0x110` because sx is known in the same ebb; compute from cx instead. (c) `i == optCur` operand order (not
  `optCur == i`). (d) `for (i = 0, sx = 0x2E; ...)` for the second loop. Residues (12w): `lbz lang2`/`lbz lang` load
  order (ours' lang load has an extra dependent, the `stb` in the if-body, so haifa's "more dependents first" schedules
  it earlier); `optMenuName` giv-init `lis` early (~8w): `simplify_giv_expr` REG case substitutes an invariant movable
  only when its single_set src is PLUS/MULT/ASHIFT/CONST_INT/SYMBOL_REF or a consec group with REG_EQUAL; the `(lo_sum)`
  movable is rejected -> add_val = reg -> `mr` merged by combine -> early `lis`. Writing `w->lang != w->lang2` (swapped)
  adds an `extsb` (shorten_compare asymmetry, `w->lang2 != w->lang` emits none): not a lever.
- idEditUnit 15w (pre-existing `register JOY* joy asm("r11")` pin kept). Target `stb r10,0x5d` stores the QI editStep
  LOAD (cse knows load == 2 via record_jump_cond on the paradoxical-subreg compare) while ours stores the SI zero-extended
  pseudo, which keeps the ext live to the end and shifts r0/r9/r10/r11. `w->grpSw = w->editStep;` reloads editStep
  (`lbz r0,0x3`) -> 33w, worse. Shape not found.
- toolIdInit 12w: `lbz r9/li r11,1` register swap and the 11-store block order after `w->lang2 = w->lang` (target: 0x1c,
  0x5f, 0x17a, 0x17b, 0x5c, 0x18, 0x1e, 0x28, 0x14, 0x24 = source order with `level` before `x24`; ours puts the dying-
  register stores 0x5f/0x5c first). Ours' .sched (sched1) order is 0x179, 0x5f, 0x5c, 0x1e, 0x28, 0x14, 0x1c, 0x17a,
  0x17b, 0x18, 0x24: the stores whose source pseudo dies (REG_DEAD -> INSN_REG_WEIGHT -1) go first, the shared QI zero
  (pseudo 167, four uses) and the shared 100 (172) go last. The target is pure source order, i.e. its sched1 saw equal
  weights for all eleven stores (or a different region); the `1` store (0x5f) is third in the target even though `li
  r11,1` dies there. No source form tried.
- toolIdEditDisp 4w: `li r24,0xc` before/after `cmpwi r0,0` and `addi r7,SYM@l` before/after `li r3,0x128` (sched2 ties).
  `row = (w->dispTop == 0) ? 0x13 : 0xC;` produces the same code as the if-form here (no lever).
- mdiff.py note: ours' listings show `beq .text+0x...` for static-function branch targets and `bl .L` for unresolved
  relocs; mask both before diffing.

### Tool RELs, t_event closer 3, part 2 (t_event/t_event 55 -> 42 words, 66 -> 67/69; CallbackSave IDENTICAL; SubToolMessInit 6 -> 2; includers unchanged; 111 OK; not flipped; 2026-09-12 02:05)
- Item 3a SOLVED, `add r3, r3, r27` (5w): the record address `d.node[d.num].s[X]` (ARRAY_REF) expands `(plus base T)`;
  the target's `(plus T base)` is the integer-arithmetic order (AGENTS "add operand order" rule): `((XmlNode*) (d.num *
  sizeof(XmlNode) + (u32) d.node))->s[X]` (local `CUR_NODE` macro around the five sprintfs) gives `mulli; add rMul, rBase;
  addi off`. Pointer forms `(d.node + d.num)->s[X]`, `(d.num + d.node)->`, `(&d)->node[..]` all stay base-first (the C++
  front end/expand canonicalise pointer sums pointer-first; expand's both_summands "MULT first" rule only fires with
  EXPAND_SUM and a sum-with-constant base, which a call argument never is). Combine never merges the mult into the plus
  here (the split would give `(plus T base)` too) — not investigated further.
- Item 3b SOLVED (CallbackSave 4w) / mostly (SubToolMessInit 6 -> 2w): the clear loop's two PRE pseudos `i - 1` (`subi`)
  and `n + 176` (`addi 0xb0`) tie on priority and are allocated in gcse hash-bucket order. Dump (`-dG`, "Expression
  hash table (S buckets)") gives the buckets directly: CallbackSave S=161, h(i-1)=150, h(n+1)=2; SubToolMessInit S=609,
  576 / 141. The hash is linear in regno, so d dead pseudos declared before the inlined call shift both by d and the
  order flips when h(i-1) wraps: predicted d=11/33, measured d=13/35 (the flip window is [13,16+] / [35,44+]; 1-12 and
  30-34 do nothing). Form: one `int dead0, ..., deadN;` line (uninitialised, never emitted) before `EvtMessWrite`'s
  caller body / before `EvtMessRead(m, path)` in SubToolMessInit, tagged `COMPILER-DIFF: candidate (gcse PRE pseudo
  numbering)`. CallbackLoad (same inline, no lever) already had the target order.
- Remaining 42w: SubToolMessMove 40 (2a back-search giv inits ~15, 2b `&mesCnt[1]` base pseudo ~10, rest register
  permutation downstream), SubToolMessInit 2 (`stw r11, 0x44(r9)` one slot earlier in ours at +0xc6c, sched2).
- Harness additions in ~/.cache/tev3: `mk.py K1 K2 OUT` (dead-pseudo variants), vT/vU (integer address), vX/vY/vZ scans.

### Tool RELs, t_camera_data closer 3 (DB_STRING ctor 7 -> 2 words with a memory-input launder; tcDataExport / tcSetBesideOffset see below; 2026-09-12)
Scratch ~/.cache/tcam3/ (variants `dbw_<X>.cpp`, `rtl_<X>/` dumps, LADBG logs; deleted at the end).
- **DB_STRING ctor 7 -> 2 (applied): `u32 zero = 0; asm("" : "+r"(zero) : "m"(ca)); len = zero; str = (char*) zero;`** (the
  pinned-r0 block is gone). Mechanism read off LADBG + the sched dumps (one 603-style model: 2 issues/cycle, ONE lsu, so stores
  issue one per cycle and the 2-per-cycle iu slots decide the qty lifetimes): local-alloc allocates LC-high (2 refs, len 4, 5000)
  BEFORE vt (lis+addi+store, 4 refs) only if vt's length is >= 16 (pri <= 5000, tie -> lower qty number = LC): the vptr store
  (insn 20, prio 12, lowest LUID) must have 7 insns between `lis vt` (c4 slot 2) and itself, i.e. it must lose c8 to a prio-13
  insn. Plain `str = 0; len = 0` gives 6 (vt 6666 -> r9, LC r11); the pinned r0 zero gives the stores anti links to every later
  call (`sched2` dependents 5 vs the target's 3) so they issue first in both passes (7 words). The launder with `"m"(ca)`: in sched1
  the ca store (56) gains a dependent and beats the vptr store at c7, the launder (prio 13) is ready at c8 and issues before the
  vptr store -> vt len 16 -> LC r9, vt r11; zero has 5 refs (li + launder in/out + 2 stores, floor_log2 = 2 -> 3846) > type's 2500
  -> zero r0, type r9; the stores rank last (3 dependents, late LUIDs) = target order. The residue: any "m" input into an asm
  costs 1 into the asm and the asm costs 1 to its dependents, so the store the launder reads gets sched2 prio 14 and takes c7's
  lsu slot from `stw max` (prio 13): target `stw max; mr r3; stfs ca`, ours `stfs ca; mr r3; stw max`. Every other memory
  operand is worse (`"m"(max)` -> 14 -> hoisted to c4 because it is ready from c3 and out-ranks `lis vt` by dependents; cr/cb/cg
  3/5 words), `"=m"` outputs behave the same (cost into an asm is always 1, LINK_COST_FREE), a hard-register input (`r3`) is an
  anti dependence in the wrong direction (the launder precedes `mr r3,r29` in the stream), a second launder gives the li prio
  15 and hoists it into c4. What is missing is a dependence that exists in sched1 only; none of the operand kinds gives one.
  Numbers: LADBG for `str = 0; len = 0`: vt q4 refs 4 birth 14 death 26 pri 6666 -> r9, LC q3 12-16 5000 -> r11, type 24-28
  5000 -> r0, zero 20-42 1363 -> r11; with the launder: vt 14-30 5000 -> r11, LC 5000 -> r9, zero 2142 -> r0, type 2000 -> r9.
- **The sink is generic:** every `addi rX,rX,K` IV update with no later read in the block goes to the block end, not only the exit
  counter (n3 probe: a second IV `n += 3` in the 1.164 loop is sunk too, block 92 insns; MakeCnvZTbl B43 sinks two; B32 sinks a
  `stw`), and a pointer IV already last stays (B29 `addi r60,0xa0`). So the vendor's `addi r3,r3,8` at slot 27 was skipped for a
  reason inside its block: the only skip condition consistent with a generic sink is a LATER READ of r3 in the vendor's pre-RA B8
  that vanished afterwards — a compiler copy (`mr rX, r3` / range-split web copy of `i`, e.g. a separate web for the remainder loop
  or a latch copy) coalesced by the RA. Such a copy is raw-last, so the pre-RA scheduler parks it at the block end (no slot shift),
  the RA deletes it, the block keeps 0x8 and is never rescheduled = the target. No C form found that makes the frontend emit it: the
  frontend unrolls (AST: loop 2 runs on web @352 `= 0x10; ... += 8`, remainder on the same web, loop 3 on @351, loop 1 on `i`);
  an inlined `i = sfxzmv_inc(i)`, a `?:` increment, a `volatile` read of i after `i++`, a `k++` counter (pass 20) all stop the
  unroll (145-148w). `#pragma scheduling off` (both schedulers, 247w) is not a fallback: the pre-RA schedule IS the target.
- Tests asked for: a call in the block is impossible (any call stops the 8x unroll); a `volatile` byte store `vy[i] = ..` compiles
  to the identical object (74w: the scheduler ignores volatile on the destination); `#pragma scheduling 750` identical, `603` 178w.
- Next step for CCIR: a frontend form that creates a coalescable copy of `i` at the latch/exit of the unrolled loop (a second web for
  the remainder: try `i` declared in an inner block, a `Sint32 j` remainder loop written by hand after an 8-step main loop with the
  same body — pass 20's hand unroll used eight locals, not the compiler's `i - 16..i - 9` shape — or `#pragma opt_unroll_loops`
  interplay). Read `flags.py`-style: the win condition is B8 `sunk_by_peep=False` with the addi still at slot 27.
- (02:25) SubToolMessInit's last 2w (`stw r11,0x44(r9)` one slot early): the inlined MessSetLoadFunc stores
  `pLoadFunc = CallbackLoad` / `loadArg = t`; `t` is a spilled param (reload `lwz 0x532c(sp+0x30000)` inserted right
  before its store), the load cannot pass the pointer store (alias: unknown base vs frame), so ours is `stw 0x44; lwz t;
  stw 0x24`. The target has `lwz t` first: its post-reload order was `lwz t; stw 0x24; stw 0x44` and sched2 issued the
  0x44 store while the 0x24 one waited on the load. Ours puts the 0x44 store first even with `loadArg = arg;` written
  first (sched1's register-pressure tie-break: the CallbackLoad-address pseudo dies in its store, `t` does not).
  Probes: store order swapped (same), `(void*) t` (same), `void* larg = t; asm("" : "+r"(larg))` (82w). Open.
- SubToolMessMove 2a (back-search, 15w): the target's giv inits are `no*24` (callee-saved r27, shared with `e =
  &m->elem[no]`), `mr r9,e; subi r9,r9,8` (= &elem[no-1].messNo) and `e - 0x18 - 0x18`, i.e. loop.c saw biv init `no`
  with add_val `m - 8`, while the pre-test still indexes `elem[no-1]` from `j = no - 1` (`subic.; mulli; add`). Ours
  computes every giv from `j` (the duplicated exit test sets j before the loop, so the biv's start value is j itself).
  `j = no; while (--j >= 0 ...)`, `for (j = no; --j >= 0 ...)`, explicit `while (--j >= 0) { if (...) break; }` all
  compile to the current 40w; pointer-stepped forms (`p = e; (p - 1)->messNo`, `(--p)->`, `p = e - 1`) drop the
  pre-test `mulli` (59-60w, size -0x10). Open: a shape whose peeled test does not redefine the biv.
- SubToolMessMove 2b (`&mesCnt[1]` base, ~10w): target forms `B = r30 + 0xc4` before the eprintfs (reads `4(B)`,
  `0(B)`), copies it for the loop (`stwx messNo, B, no`; `stw i, 4(B)`), and forms `A = r30 + 0xc0` via `lwzu` for
  `stwx no, A, no`. cp/typeck's pointer_int_sum distributes `(no + 1) * 4` only for pointer sums, and the front end
  reassociates `(mesCnt + 1) + no` back to `mesCnt + (1 + no)`; `(&mesCnt[1])[no]`, `*(mesCnt + (no + 1))` = 39w
  (`stw 0xc4(rB)`, folded), `s32* mc = &EvtDebug.mesCnt[1]` at the block top = 51-52w (mc becomes the class head:
  `addi mc, hi, EvtDebug+0xc4@l; subi r30, mc, 0xc4`), `u32`/`s32`/`unsigned long no` = 40w. Open.

### Tool RELs, t_id pass 6 (toolIdInit 12 -> 0 pure C, toolIdEditDisp 4 -> 0 two tagged asm forms, idEditUnit 15 -> 9 one tagged codeless qty; 61 -> 63/69, 53 -> 31 words; not flipped; 2026-09-12)
Harness ~/.cache/tid6/ (deleted at the end): try.py NAME FUNC 'old=>new'.. (variant.sh wrapper on a copy of the tree file),
model.py (the sched1/local-alloc/sched2 store-order model below, brute-forced over the merges of the target's two store groups).

- **toolIdInit 12 -> 0, pure C: the eleven-store block after `w->lang2 = w->lang` is now `type, x17B, pause, cnt, menuX, menuY,
  level = x24 = 0, drawSafe, parentNo`.** Three mechanisms, all read off the dumps (`rtl.sh` + `LADBG=1`):
  1. sched1 ranks the stores (all prio 29, all ready at t=4 after `lbz` + five `li` fill t=1..3 at issue rate 2) by INSN_REG_WEIGHT:
     every SET is +1, each REG_DEAD -1, so a store whose source pseudo dies there (weight 0) goes before the others (+1), ties by
     LUID = source order. The QI zero (4 stores), the 100 (2) and the SI zero (2) die at their LAST store in source order.
  2. sched2 then re-sorts the same-priority stores by "more dependents first": a store whose source HARD register is rewritten
     later in the block (here r0/r9/r11 by the post-call code: `lwz r9/r11`, `li r0`) has one extra anti-dependence (10 vs 9
     dependents), so the r0/r9/r11 stores come first, each group keeping its sched1 order. The r0/r9/r11 set = the values
     local-alloc puts there = lang, the QI zero, the `1`, decided by local-alloc's qty priority `refs/(death-birth)` on the sched1
     positions (`li 1` at position 5 dying at the third store ties lang's 2/12 -> lang q0 first -> r9, then the `1` -> r11: the
     register swap of pass 5 was only the store order).
  3. cse: a QI `= 0` store AFTER the chained `w->level = w->x24 = 0` takes the SI zero's subreg (r7), not the QI zero (r0); the four
     QI zero stores must precede the chain (`e4` variant: `stw r0,0x24 / stb r11,0x17b`).
  model.py enumerates the 462 merges of the target's two sched2 groups, keeps those that are "dying first, then LUID" for some
  source order and whose local-alloc replay gives the target's registers: 420 source orders, all with type/x17B first, cnt the last
  QI zero, menuX before menuY, drawSafe and parentNo last; the first one tried compiled to 0 words. Negative: `level = 0` as a
  separate statement or through an `int z` (16/13 words: the SI zero's death moves).
- **toolIdEditDisp 4 -> 0, two tagged asm forms** (both `// COMPILER-DIFF: candidate`):
  1. `li r24,0xc` between `cmpwi r0,0` and `bne`: the target's li was not ready before t=3 in BOTH schedulers (block = lbz, cmp, li,
     bne; the li is independent, so any C form is hoisted to t=1 next to the lbz). `if (top == 0) row = 0x13; else row = 0xC;` gives
     the target's LUID order (jump.c "if (..) x = a; else x = b -> x = b; if (..) x = a" emits `x = b` right before the condjump, after
     the compare; in THIS toplev jump2 runs AFTER sched2, so a jump2-time transform would leave the li there, but the transform
     fires in jump1 for every spelling tried: if/else, ?:, switch, chained if, dead statements in the arms). Applied: `int top =
     w->dispTop; int t2; asm("" : "=r"(t2) : "r"(top)); if (top == 0) row = 0x13; else asm("li %0,12" : "=r"(row) : "r"(t2));` —
     an asm consumer always costs 1 (LINK_COST_FREE for unrecognizable insns), so the codeless asm sits at t=2 and the li at t=3
     after the cmp (LUID: the else arm is moved behind the compare). One asm with `"r"(top)` is at t=2 = before the cmp (d4).
  2. `li r3,0x128` before `addi r7,fmt@l` (t=2 slot 2 of sched2): a sched1 tie — the addi (its high dies: weight 0) beats the
     constant arg set (+1) at t=3, so the addi has the lower sched2 LUID. Fix = give the li weight 0: `int yy = (ternary) * 0xE;
     register int x128 asm("r3"); asm("li %0,0x128" : "=r"(x128) : "r"(i)); eprintf(x128, yy, ..)` — `i` (the loop counter, dead
     since the loop) dies at the asm. The yy statement is needed so the asm lands in the call's block, after the ternary's blocks
     (without it the li goes to the arm's first block); `y`/`row`/`n1`/`n2`/`cx` as the dying input each perturb global's order
     (5-17 words), `i` is free. A shared `int sx = 0x128` for both eprintfs is folded by gcse cprop (same code as the constant);
     hard-register anti inputs (`r7`, `r8`, `r9`) all raise the asm to prio 5 -> t=1 (the asm's LUID is before the argument
     insns, so a hard arg register gives an anti-dependence, never the true dependence wanted).

### Tool RELs, t_esp pass 17 (t_esp 208/212: InitTool segs 162-163 SHAPE exact with a pure-C form (`DEACTIVATE(e)`, the `int&` store), words 2061 -> 2182 because the callee-saved names downstream of the MODEL window permute; the cse1 flush grid read with a new `CSEDBG=1` hook: the target's 2nd flush sits 4-6 cse1-insns later than ours (between SAVE_EVENT's `w` and `h` loads), a uniform -5 shift fixes seg 271 but not the later regions; `flg = 4` tail sharing is register pressure, not a spelling; nothing flipped; 2026-09-12)

- **Segs 162-163 (MODEL window `new`, `lwz g_pEditWin1` late, six callee-saved `lis` + three `fmr` before the call): pure C,
  applied.** Pass 16 attributed the target's `lwz g_pEditWin1` delay to a true dependence on `stw *slot` and looked for a slot
  pointer without base/REG_EQUIV. That was the wrong store: the dependence is on the PRECEDING store `e->win->active = 0`
  (`stw r31,0x58(r9)`). Written as `DEACTIVATE(e)` (= `ISet((e)->win->active, 0)`, the file's own macro; `ISet(int& d, int v)`
  stores through a reference, so the MEM has no MEM_IN_STRUCT_P) the store may alias the fixed scalar `g_pEditWin1`
  (alias.c: only struct/scalar-flagged MEMs are disjoint from fixed scalars; the base of `e->win` is a MEM load = 0), sched1 keeps
  the load below it, the `bl __builtin_new` slips 3 cycles and the six `lis` + `fmr f21,f25; fmr f20,f22; fmr f19,f24; li r29,0`
  fill the slots exactly (seg 162 19/19, seg 163 34/34 with only callee-saved names differing). Applied to all four
  `CreateEditWindowN` tails (windows 1-3 unchanged, segs 0-161 still exact). The slot-pointer question is CLOSED: the store
  `*slot = e` keeps its base (integrate.c `process_reg_param` copies a constant argument into a `reg/v` temp via `high`+`lo_sum`;
  record_set keeps the base for `lo_sum` with dest as operand; `update_equiv_regs` gives REG_EQUIV to any multi-set pseudo whose
  sets all carry the same invariant REG_EQUAL, so even a 2-set pointer to the same symbol stays known). Also read: a plain local
  `TOOL_WINDOW** pp = &g_pEditWin4; *pp = e` set in the same block is folded by combine into `stw @l(rhi)` (LOG_LINKS are
  per-block, which is why the entry-set reference temp is NOT folded and reload rematerialises it as `lis r6; addi r6`).
  Words 2061 -> 2182: the six `lis` pseudos now live across the call as in the target, and global.c hands the CreateString
  label highs different callee-saved names (ours r18/r23/r19 vs target r19/r21/r16) — the target has ~3 more callee-saved
  highs in this region (e.g. `high g_pPrimArray` kept in r18 across SAVE..SAVE_EVENT, segs 230/271/284; ours `lis r6` fresh) and
  those extra live values come from the flush grid below, so the name permutation is downstream, not a reason to revert.
- **Kit: `CSEDBG=1` hook in tools/research/sngdbg (cse.c, patch + README updated).** Prints every 1001-insn hash flush of cse1/cse2
  with the insn UID (`CSEDBG <fn> flush at insn U (block from F)`; the flush precedes U). `~/.cache/tesp15/flushmap.py
  <dump.jump> <tail-start-uid> <uids,...>` names the window (k-th `__builtin_new` after the tail label) and the offset in
  non-note insns. Ours (tree source, tail block from 4539): LOAD_EVENT+4, SAVE_EVENT+21, OPTION+534, PATH+535, SIZE+180,
  SPEED+686, COLOR+861, LIFE+2, ROTATE+665, WORK0+12, WORK6+8, BASEPOS+117 (cse1); cse2 has its own 8-flush grid from 21668.
  Also in ~/.cache/tesp15: `genK.py OUT NAME=K..` (per-ctor pad override), `refit.py IN OUT +D` (InitTool dead-set block),
  `getN.sh V.cpp` (gcse N from `-dG`, `;; Function InitTool` header), `runv.sh V.cpp` (words/diffsegs/region totals in one line).
- **Where the target's flushes are, method: a constant loaded FRESH although an equal constant was loaded a few insns earlier
  marks a flush between the two loads.** Seg 271 (SAVE_EVENT CreateNormalWindow): target pos.x f21 / pos.y f20 / w f25 all
  SHARED with LOAD_EVENT, h = fresh `lfs f21` -> the target's flush 2 is between SAVE_EVENT's `w = 192.0f` load and its `h =
  128.0f` load. Ours flushes at UID 7400 = between the pos stores and the `w` load (w and h both fresh), i.e. 4-6 cse1-insns
  EARLIER. A -5 shift (K=3 instead of 4 in the MODEL/LOAD/LOAD_EM/LOAD_ROOM/LOAD_SST pads, `genK.py`) makes seg 271 21/21
  (FP names only) and region 162-340 373 -> 325, but seg 0 breaks (d80: the LOAD_EVENT/SAVE `&pos` spill slots 0x66c/0x6b0/
  0x6bc/0x6c0/0x6d0/0x6d8 reorder although N stays 5235 after `refit.py +5` — the fresh/shared change alters which `high LC`
  expressions sit in the PRE table around those slots, so the slot order is not a pure-N fit any more) and regions 341+ get
  worse (all later flushes move -5 too; the target's later flushes are not uniformly shifted). 2316w, NOT applied. Next: fit
  each flush separately with the fresh-constant method (flush k's interval from the first window after it whose constants are
  fresh vs shared), then set the per-ctor pads to hit the 12 intervals; the CSEDBG line gives ours instantly.
- **Tail windows 698-761 `flg = 4` (`li r10,4` per window in the target, `stw r31` from one `li r31,4` at seg 217 in ours): not
  a spelling.** The target has a fresh scratch `li rN,4` in EVERY CreateNormalWindow segment (64 sites), ours in all but the ten
  VEC0..WORK6 windows, which share one pseudo that global.c put in r31 (its `li` is sched1-hoisted to seg 217: the tail is one
  basic block). cse shares equal constants within a window in both compilers, so the target's per-window `li` are reload
  rematerialisations of spilled REG_EQUIV pseudos (or single-use pseudos): the difference is that r31 is FREE in ours over
  698-761 and taken in the target = the same missing callee-saved values as above (more highs/FP constants live across the
  tail). A per-window literal or block-local `flg` cannot change this (the constant is CSE'd either way); do not spend on it.
- Not touched: Load/SaveEmType 2/2, fn_t_esp_3DE4C 24, dbg_tool.h/db_widget, the `k8/k6` asm, the CSE pads (76 dead sets, N 5235).
  Tree edit: the four `DEACTIVATE(e)` tails in src/t_esp/t_esp.cpp only. Verified: locked ninja of t_esp.o, bytecmp 208/212,
  InitTool 2182 words. Not run: `ninja -k 0`, make_rel --verify, shasum (nothing flipped). Scratch /tmp/t17 (mk.py variant
  generator, v*.py, scanD.sh) is disposable; the kept harness is ~/.cache/tesp15 + the kit.

### Tool RELs, t_event closer 4 (t_event/t_event 42 -> 17 words, 67 -> 68/69: SubToolMessInit IDENTICAL (tagged anchor), SubToolMessMove 40 -> 17 (hand-peeled back-search + asm `mr` + r11/r30 pins, `EventDebug* d` base for mesCnt); dbg_tool.h untouched, includers unchanged, 111 OK; not flipped; 2026-09-12)
Scratch ~/.cache/tev4/ (`mk.py OUT [--base=B] OLD NEW..` exact-substring variants, `try.sh NAME [FUNC]` = variant.sh word lines; v*.cpp
variants, rtl_base/ rtl_v4a/ rtl_v6a/ dumps; delete at the end of the family). Only src/t_event/t_event.cpp edited; objects.py/modules.py untouched.
- **SubToolMessInit 2 -> 0 (tag #5, sched1 tie).** The inlined MessSetLoadFunc stores 1618 `[P+0x44]=F` (F = CallbackLoad address) and
  1619 `[P+0x24]=t` tie in sched1 exactly: prio 13/13, INSN_REG_WEIGHT -1/-1 (F dies at 1618, P = the MessTool.p load dies at 1619),
  class 3/3, dependents 3/3 (`1707 1642 1628`) -> LUID -> 1618 first (ours). The Save pair (1603/1604) has the same tie and the target
  keeps LUID order there, so the target's Load pair had a weight difference: F not dying at its store. `asm("" : "=m"(path[0]) :
  "r"(CallbackLoad));` right after the MessSetLoadFunc call (cse1 folds the fresh lo_sum into F, path is dead) gives 1618 weight 0 ->
  1619 first -> reload puts `lwz t` before both stores, sched2 issues `stw 0x44` while `stw 0x24` waits = target, no code. Zero-code
  forms not found: `(void*) t`, `this`, store order are all weight-neutral (a REG_USERVAR actual is never copied by integrate).
- **SubToolMessMove 2a (back-search, 40 -> 23 with 2b's share): read fully, closed with an asm `mr` + two pins.** Target preheader
  `mr r9,e; subi r9,r9,8` (giv1 = elem[no-1].messNo), `subi r3,e,0x18` (p giv), loop `subi p,p,0x18 | addi cnt | subic. k,k,1; blt |
  lwzu v,-0x18(g1) | mr p,pg | cmpwi v,-1; beq top`. The `mr` is reload_cse's rewrite of `add T,m,A` (A = no*24, callee-saved r27 because
  it is used again after IsWorkAlive) = `emit_iv_add_mult(initial_value = REG no, 24, (plus m -8))`, i.e. loop.c saw biv init `no`
  (`valid_initial_value_p`: REG/CONST only; the backward scan stops at the first CODE_LABEL, `record_initial` takes the last set) and a
  giv `k - 1` reduced to the counter (its init `(plus no -1)` cse'd with the peeled `subic. r11,r31,1`). fold-const associate rule
  (`fold(X + (VAR + CON))` -> `(X + CON) + VAR`, split_tree) puts loop.c's constant INSIDE: every loop.c spelling gives `(A - 8) + m`
  (`subi; add`), never the target's `(A + m) - 8`; and cse2 merges `(plus A m)` with e (exp_equiv_p checks both orders) unless e's class
  is gone. Spellings measured (words for the function): `j = no; j - 1 >= 0 && elem[j-1]; j--` (for/while) 59: cse rewrites the
  step `j = j - 1` into `j = t` (t = the compare temp, same ebb) -> no biv, no strength reduction; `no` as the biv 61; hand-peeled
  `k = no; do { k--; cnt++; } while (k - 1 >= 0 && elem[k-1])` 63/60 (biv init REG no, inits `(A-24)+m`/`(A-8)+m`, the `k-1` giv
  "not worth while 0 vs 11": mult-1/const-add givs get `benefit -= add_cost*biv_count` = 0, so the biv is never eliminated and the
  counter stays `mr k,no; subi k,k,1; subic. r0,k,1`). Hand-written induction variables reproduce the loop bytes exactly:
  `q = e - 1; ofs = no*24; asm("mr %0,%1" : "=r"(base) : "r"(e), "r"(ofs)); mp = (s32*)(base - 8); do { q--; cnt++; if (--k < 0)
  break; mp -= 6; p = q; } while (*mp == -1);` (38w, size exact; the `"r"(ofs)` input keeps A live across the call -> r27; `no*24 +
  (u32) m - 8` as C is cse'd to `subi r9,e,8`, 54w). Remaining permutation: k took a virgin callee-saved r30 in global.c pass 1 (GDBG
  prints no GORDER lines for SubToolMessInit/SubToolMessMove - hook gap, not investigated) -> `register int k asm("r11")` 38 -> 30;
  m/e order (target m r30 before e r29: with the asm, e has 5 refs vs m 3; the target's `add T,m,A` gave m 4 / e 4) -> `register
  EventMessageData* m asm("r30")` 30 -> 23. Back-search block now byte-identical.
- **SubToolMessMove 2b (`&mesCnt[1]` base, 23 -> 17):** the target's `addi r28,r30,0xc4` is `(plus D 0xc4)` with D = the `&EvtDebug`
  pseudo already in cse's table: `EventDebug* d = &EvtDebug; s32* mc = &d->mesCnt[1];` at the block top, reads `mc[1]`/`mc[0]`, loop
  stores `mc[no] = e->messNo; mc[1] = i;` (`EvtDebug.mesCnt[no] = no` and the [0] read unchanged). A bare `s32* mc = &EvtDebug.mesCnt[1]`
  makes the `EvtDebug+0xc4` constant the class head and derives `&EvtDebug` from it (`subi r29,mc,0xc4`, 30w). Left 17w: `lwz 0(rB)`
  for mc[0] (ours folds to `0xc4(rD)`), the loop's `lwzu` A formation at the [0] read + `mr r26,rB` copy (ours hoists `addi A,D,0xc0`
  before the eprintfs), `mc[no]` as `stwx` (the target's `no` is NOT a known constant in the loop ebb: `int no = 0` before the loop
  gives `stwx` but three zero registers, 27w - `no` gets local-alloc's REG_EQUIV and is rematerialised; the original's `no` must be
  multi-set or otherwise not REG_EQUIV'd), and the downstream e/cMes r29<->r30 pair of the last loop.
- Not started: Tools/t_esp_area's 7-word ctor tie (header change) - no time left in the box; flip order unchanged (t_esp_area first).

### Tool RELs, t_camera_data closer 4 (DB_STRING ctor 2 / tcSetBesideOffset 27 / tcDataExport 62 unchanged; the sched1-only mechanisms found are the store->call dependence kind, INSN_REG_WEIGHT and flow2's dead-anchor deletion; no source change adopted; 2026-09-12)
Scratch ~/.cache/tcam4/ (deleted): `prio.sh <variant.cpp>` = rtl.sh + bytecmp + the sched1/sched2 dependence tables,
`sched.py <dumpdir>` = the ctor's sched1/sched2 issue order with labels `name[uid]prio/dependents`, `ins.py DUMP FUNC [re]` = one line per insn.
- **DB_STRING ctor (2w, not closed). Three haifa facts read off the source (`tools/research/sngdbg/src/gcc/haifa-sched.c`, `flow.c`) that the
  earlier passes modelled wrongly:**
  (1) **A store before a call gets a TRUE dependence on the call (cost 2 -> prio 13) iff every pseudo it reads has REG_N_CALLS_CROSSED > 0;
  otherwise the read puts the insn into `sched_before_next_call` (sched_analyze_2 REG case), the call adds that link as REG_DEP_ANTI first,
  and the MEM loop's true_dependence hit is skipped ("If a dependency already exists") -> prio 12.** That is why `stw max` (reads
  `this`/`max_`, both live across the base-ctor call) is 13 and every other store (vt, type, 0.0f, zero pseudos born after the call) is 12,
  in sched1. In sched2 the same split comes from the call's pre-loop over call-used hard regs (`reg_last_uses`): a store reading only
  callee-saved registers (r29/r30) gets the true dep (13), stores reading r0/r9/r11/f0 get anti (12). A launder-copy of `max_`
  (`asm("" : "=r"(m) : "0"(max_))`, m dies before `new`) turns `stw max` into a 12 in sched1 and leaves 13 in sched2.
  (2) **sched1's rank_for_schedule has INSN_REG_WEIGHT right after priority** (before class/dependents/LUID; sched2 skips it): a store
  whose source register dies there weighs -1, an insn setting a pseudo +1, `"+r"` launders +1, `"=m"` anchors with a dying input -1.
  So among prio-12 stores the LAST use of a pseudo issues first (the chain `cb = cg = cr = z; ca = z;` issues ca before cr/cg/cb with no
  dependence at all; `stw vt` and `stw type` are -1 too, `stw max` 0), and the base's `"m"(ca)` launder is only needed for the count of
  prio>=13 insns between `lis vt` and `stw vt` (7: lfs, li 0, addi vt, stw max, stfs ca(14), li 4, launder), not for the ca-first order.
  (3) **A codeless `asm("" : "=m"(field) : "r"(x))` whose field is overwritten later in the block with no intervening load/call/volatile
  asm survives flow1 and sched1 but is deleted by flow2 (`insn_dead_p` walks `mem_set_list`; the sched1 output order decides)** -> it is
  a sched1-only insn: prio 13 (output dependence into the real store, cost 1), no register output, gone before sched2. Variant Vb
  (`asm("" : "=m"(str) : "r"(max_))` before `max = max_`, zero launder without a memory input, `cb = cg = cr = z; ca = z; type = ..;`)
  reproduces the base's sched1 shape (c6: addi vt + launder, c7: anchor + stw max, c8: li 4 + stw vt, vt len 16, zero 5 refs) and the
  target's store order, but sched2 then puts the zero launder (13, ready c6, 5 dependents) into c7 with `stw max`, pushing `mr r3,r29` to
  c8 where `stfs ca` (5 dependents: the f0 anti links to lfs 1.0 and the three calls) beats it: residue `stw max; stfs ca; mr r3` = 2w, the
  mirror of the base's `stfs ca; mr r3; stw max`. The base residue is `stfs ca` at 14 (launder dependent) > `stw max` 13 at c7.
- **The closed set (do not retry):** the launder must exist in sched2 (its output feeds the two zero stores; it also keeps `li 0` at 2
  dependents so `lis vt` wins the c4 tie by LUID - an anchor instead of the launder gives `li 0` 3-4 dependents -> c4), so in sched2 it
  is prio 13 and ready at c6; every input that delays it to c8 raises its producer: `"m"(max)` -> `stw max` 14 (c4 in sched1, c6 in
  sched2), `"m"(ca)` -> `stfs ca` 14 (the base), `"r"`(type const) -> `li 4` 14 (c4), `"f"`(0.0f) -> ready c6 (asm links cost 1), a
  max_ launder in front (`"+r"(max_)`, prio 15) reproduces sched1 exactly (D1) but displaces `addi vt` at c6 in sched2. `stw max` cannot
  be held to c6 in sched1 without a pseudo that 79 (`mr r3,max_`) also reads. Alias-based pass-selective deps run the wrong way
  (unknown pseudo base -> unknown hard-reg base). What is left: a dependence that exists in sched2 only and delays the launder to c8
  (an anti/output link through a hard register written at c7, i.e. r29/r3 after `mr r3,r29` - nothing in the function writes them), or
  an insn in sched2 that outranks `stfs ca` at c8 on the lsu.
- **tcSetBesideOffset 27 unchanged.** Probes for "one more weighted ref on the n*12 giv" (loop 2 body): `Vec* p = &c->pos[n]; *p = ..`
  27 (folded back), `.x/.y/.z` member stores 58 (size +8), laundered `p` 44, anchors `asm("" : "=m"(c->num) : "r"(&c->pos[n]))` 37 (size
  -4) / with `&c->at[n]` too 37 (size +0x10) - the anchor address becomes its own giv as in closer 3. An output-less `asm("" : : "r"(&c->pos[n]))`
  (a barrier) gives 24 words, size exact, IV updates moved after the stores, r3/r31 still swapped: not adopted.
- **tcDataExport 62 unchanged** (no probes this pass; closer 3's allocation facts stand: raise loop-4 `tcCdat` base / `pTc`-high above pri
  4000 or give loop-4 `d+1` a hard conflict with r4-r7).
- Flags: t_camera/t_camera_data and t_esp/db_widget stay False; no tree edit; nothing rebuilt under the lock (all variants via the kit).

### Tool RELs, t_id pass 7 (idEditUnit 9 -> 0 pure C + pin removed (`grpSw = 0`), toolIdOption 12 -> 8 (`s8 lang2`, alias name); the 2/2/2/4-word rows are a bytecmp artefact — REL bytes identical; `common_t_id` + `ScreenReSize` alias fixed so the module links; 63 -> 64/69, 31 -> 18 words (8 real); not flipped; 2026-09-12)
Harness ~/.cache/tid7/ (deleted at the end): try.py NAME FUNC 'old=>new'.. (variant.sh on a copy of the tree file), relcheck.sh <t_id.o>
(links the module with our object into the scratch dir with tools/link_rel.py and runs make_rel --verify; the tree's t_id.elf still holds the split object).
- **Reloc-row verdict: `_._6cCoord`/`_._7ID_DATA`/`_._5cUnit` 2w and `__static_initialization_and_destruction_0` 4w are bytecmp
  artefacts, not source.** Pass 6 mis-read the fdiff (`lbl_t_id_bss_14DAC4` is the SECOND diff pair of a longer listing). The words are the
  `lis/addi _vt.5cUnit` pairs: our object DEFINES `_vt.5cUnit` WEAK in its own .rodata (0xaf8 = module .rodata 0xE20, the target's anonymous
  `lbl_t_id_rodata_E20`), but bytecmp resolves every weak name through the linked ELFs (module ELF, then the DOL), and the module ELF in build/
  is linked from the SPLIT object (no `_vt.5cUnit` there) -> the DOL's copy 0x8021d6a8 -> `(t_id, .rodata, 0xe20)` vs `(addr, 0x8021d6a8)`.
  Proof: module linked with our t_id.o -> `make_rel --verify` lists 33 differing bytes, all inside idEditUnit (9) and toolIdOption (20 + the
  3 reloc-table entries of its `lis optMenuName/langName2` order); the four small functions are byte-identical in the REL. Noted in
  tools/bytecmp.py's docstring (no logic change). After the flip the rows vanish by themselves.
- **Two link blockers fixed in src/t_id/t_id.cpp (needed for the flip, no code change):** (1) `asm(".comm common_" STR(REL_MODULE) ",52,4")`
  — t_id.cpp is the module's COMMON-block owner (gen_rel_config: the unit with template instantiations), make_rel died with
  `COMMON symbols take 0x0 bytes, the original had 0x34` once our object replaced the skeleton; (2) `ScreenReSizeI(int, u32) asm("ScreenReSize")`
  — the alias named the mangled `ScreenReSize__FUsUs`, undefined at link time (bytecmp only listed it as a note); the DOL function is C
  linkage. The rename also moved toolIdOption 12 -> 10w (gcse hash-bucket order of the label names, catalogue row 2).
- toolIdOption `lbz lang2`/`lbz lang` (2w, still open). Read off the dumps: the expansion is `load lang2 (QI); zext; load lang (QI); sext; cmp`
  (rs6000 `zero_extendqisi2` expander wants a register, so every promoted byte field is a QI load + a separate extension); cse1 shares the
  lang load with the store `w->lang2 = w->lang` (2 uses -> the load stays put), combine merges the single-use lang2 load INTO its extension
  and then the 3-way (i1 = the zext-load, i2 = `sext lang`, i3 = cmp) is split with the zext(mem lang2) placed at i2's position = AFTER the lang
  load, and the compare becomes `cmp(zext lang2, subreg:SI lang)`. The target has lang2 first, so its zext-load never moved: its lang2 load
  was multi-use or the sext was combined 2-way. Spellings that do not change it: `(u8) w->lang` (shorten_compare then loads both bytes
  first, the merged zext still lands after), `!(==)`, `x17D |= 2` first, `u8 l2 = w->lang2` local, `w->x17D = w->x17D | 2`; `s8 l = w->lang`
  local: +4 bytes (extsb); `(u8) w->lang != w->lang2`: 11w. A launder/anchor asm on `int l2` gives the order but shifts `w` to r30 (92w).
- **idEditUnit 9 -> 0, pure C, the r11 pin REMOVED: the case-2 arm is `w->grpSw = 0`, not `= 2`.** The target's `stb r10,0x5d` stores the
  `trg & 0x100` pseudo (`andi. r10,r0,0x100; beq else`), which cse knows to be ZERO on the followed `beq` (cse_basic_block calls
  `record_jump_equiv (insn, 1)` only for TAKEN path entries — a one-use label preceded by a barrier; AROUND entries, the skip-blocks
  jump over `stb r25`, record nothing), and the QI constant store takes the newest known-zero SI register (`insert` puts a new
  equal-cost element BEFORE the older ones -> `trg & 0x100`, not `trg & 0x200`). Our `= 2` matched only because the editStep value
  happened to be 2 there; with `= 0` the sign-extended index dies at the case tree, the QI load takes r10, joy's high r9/r11 unpinned.
  The pass-6 codeless 4th-qty asm after `toolIdDataInit(d)` is still needed (6w without it). Lesson: a `stb rX` of a "constant"
  from a register that a preceding `beq` proved zero is a `= 0` store — check the semantics before hunting register order.
- **toolIdOption `lbz lang2`/`lbz lang` 2w -> 0: `lang2` is `s8` (include/t_id.h had `u8`).** With equal signedness shorten_compare
  emits `load lang2; load lang; sext; sext; cmp`, combine folds both extensions into `cmpw (subreg lang2) (subreg lang)` 3-way WITHOUT
  a split (same-kind extensions compare in the narrow mode; LOAD_EXTEND_OP makes the paradoxical subregs valid), so both loads stay in
  expansion order. Verified as a header change (only toolIdOption moves, 64/69, 18w). toolIdOption 8w left = the optMenuName preheader.
- **toolIdOption 8w left = the first loop's preheader order (`lis optMenuName` at t5 next to the pool `lis`, target at t7 next to
  `lis Screen`, its `addi r26` giv init unchanged).** Read off the dumps (rtl.sh -dG -dL, sched verbose): sched1 block 56 ranks the
  hoisted highs prio 2 (pool `lis` 3), ties by LUID, and `high(optMenuName)` has the LOWEST LUID of the highs because it is hoisted by
  **gcse PRE** (expression 81, `PRE/HOIST: end of bb 56 .. reg 398`: the menu-name load is on every path of the body, langName2/Screen
  are case arms and stay loop.c movables) — loop.c then re-emits it as movable `405 = high` right after `high optCur`, before the sx/r1/r2
  constants and the langName2/Screen/pool pairs; the giv init `mr` merges with the lo_sum (`addi r26`) at the preheader end. The target's
  final order (pool-high, langName2-high | lfs, langName2-low | Screen-high, optMenu-high | Screen-low, optMenu-low | li, li) is what sched2
  gives a sched1 stream whose high/low pairs are ADJACENT with the highs alternating r9/r11 (Screen-high waits for lfs's r9, optMenu-high
  for addi r17's r11): its `lis optMenuName` had a LUID after Screen's pair, i.e. it was not PRE'd into bb 56 ahead of loop.c's movables.
  Spellings that do not change it (all 8-10w): `*(optMenuName + i)`, `&optMenuName[i]`, a `const char** pn` local (before the loop, in the
  body, or `pn++` pointer biv: 120w), `(u8)`/`(int)` views. Not tried: a body shape where the menu-name load is not on every path at
  gcse time (so PRE leaves it to loop.c), and an explicit `#pragma`-free `-fno-gcse` check of the LUID hypothesis with `tools/research/kit/rtl.sh`.
  Tagged lever not applied (an asm-emitted `lis/addi` pair before the loop lands at t5 as well; the giv init `mr` cannot merge into an asm).
- Not flipped: toolIdOption 8w. Tree edits this pass: src/t_id/t_id.cpp (`common_t_id` .comm, `ScreenReSize` alias name, idEditUnit
  `grpSw = 0` + pin removed), include/t_id.h (`s8 lang2`), tools/bytecmp.py docstring. Verified: locked ninja of t_id.o, bytecmp 64/69
  18w (8 real), module linked with our object -> make_rel --verify = 18 differing REL bytes, all toolIdOption+0x354.. and its 3 reloc
  entries. Not run: `ninja -k 0` (nothing flipped). Harness ~/.cache/tid7 deleted.

### Tool RELs, t_esp pass 18 (t_esp 208/212: InitTool flush-grid fit — the grid is TWO grids (cse1 12 flushes + cse2 8 flushes on the same tail block), the pads move them at different rates, pass 17's direction was inverted (a pad set REMOVED before a flush moves that flush LATER in the code); pad-type table (cse1, cse2) insns measured; cse2's first flush fitted (2182 -> 2137w, /tmp/t18/x8.cpp); nothing flipped; 2026-09-12)

- **Direction.** cse_basic_block flushes before the 1002nd non-note insn of the block; removing K pad insns before flush k lands
  flush k (and every later one) K insns LATER in the code, adding moves them earlier. CSEDBG proof: MODEL pad K=3 -> cse1
  flushes at LOAD_EVENT+5 / SAVE_EVENT+22 / OPTION+535 (base +4/+21/+534); pass 17's "K=3 in 5 windows = -5 shift" moved
  ours +5 later (SAVE_EVENT+26, between `w` and `h` as the target wants), which is why it fixed seg 271.
- **Two grids.** cse2 runs on the same block with its own 1001 counter over the post-gcse stream (base: LOAD_EVENT+61,
  OPTION+77, PATH+354, SPEED+133, COLOR+575, ROTATE+26, SUB+98, WORKSP2+13 in the `.loop`-dump insn numbering) and shares
  `high`/constants the same way. Seg 219 (LOAD_EVENT `" Name :"`: target fresh `lis r8`, ours `addi r5,r17` shared) is a cse2
  share: the `high .LC713` is cse2-insn +55 of LOAD_EVENT, our cse2 flush at +61 folds it into MODEL's, the target's cse2 flush
  precedes it. `/tmp/t18/flushes.sh V.cpp` prints both grids (CSEDBG + flushmap on `.jump` for cse1, on `.loop` for cse2);
  `/tmp/t18/wincount.py DUMP [DUMP2]` = per-window insn counts of any dump (the cse1 stream is `.jump`, the cse2 stream `.loop`).
- **Why the K=4 pads survive to gcse/cse2 (and the pad-type table).** `d_ = 4` is the class head of const 4 when the window is
  the FIRST after a cse1 flush (table empty), so the ctor's `flg = 4` store is rewritten to `stw d_`; `d_` is then referenced and
  delete_dead_from_cse keeps all its sets until flow1 (the flg `li` is deleted instead). In every other window the head is the
  earlier window's `d_`/flg pseudo and the pad is deleted at cse1's end. Measured on MODEL (first window; relative to no pad):
  `1..4` = (4 cse1, 3 cse2); `1..5` = (5, 0); `1:2:3:5:4` = (5, 4); `1:2` = (2, 0); `2:4` = (2, 1); K=0/1 = (0, 0) (a single
  set is deleted by jump1). Rule: n sets ending in 4 = (n, n-1) in a first-after-flush window, n sets otherwise = (n, 0).
  Hence: cse1-only shift = a not-ending-in-4 pad anywhere; cse2-only shift = lengthen the first-after-flush window's
  ending-in-4 pad by x and shorten x cse1 insns in the following windows (`/tmp/t18/mk.py OUT NAME=1:2:3:5:...:4 NAME2=2 ..`,
  per-instance pads for the macro classes via `cls##_CSE_PAD()`).
- **N.** gcse's bucket count moves with the grid (a shared `high` deletes its `lo_sum`/copy: MODEL K=3 alone = -2 buckets), so
  every grid change is followed by `/tmp/t18/autoN.py V.cpp` (in-place refit of the `i = k` block to N 5233/5235, ~2 sets per
  bucket). Seg 0's `&pos` slot order still permutes at N 5233/5235 when the grid is wrong (the PRE'd `high LC` set changes); it
  is a whole-grid consequence, judge it last.
- **Metric.** `/tmp/t18/fl.sh V.cpp`: words, raw regions, `shape` (registers + spill slots normalised), `mset` (spill/reload
  insns dropped, per-segment instruction multiset) — mset isolates fresh-vs-shared from allocation noise; regions are bounded at
  the 12 cse1 flush windows (0-161 | 162-215 | 216 | 270 | 297 | 353 | 416 | 445 | 495 | 605 | 620 | 697 | 739 | 776+).
- **cse2 flush 1 fitted:** MODEL pad `1:2:3:5:6:7:8:9:10:11:12:4` + LOAD/LOAD_EM/LOAD_ROOM/LOAD_SST `1:2` (cse1 grid unchanged,
  cse2 grid -8: LOAD_EVENT+53), N refit 76 -> 68 sets: 2182 -> 2137w, mset regions [2 8 4 9 4 ..] vs base [2 11 5 10 4 ..],
  segs 209/210/217/219/220 mset-exact. Variant /tmp/t18/x8.cpp (not in the tree yet).
- **APPLIED (src/lib/mpv_mcy.c `MPVMC16_OneRef4p_TuneC`, 136 -> 122w, size equal):** pixel/sum locals declared before `i/stride/s0/s1/d`
  (loop variables declared without initialisers, assigned `stride, s0, s1, d` in that order so the `d` load stays last and takes r3),
  `(Uint32)` casts on the sum operands, pass 8's a1 body (pixel 9 + `p8` before `d[0]`, second half `p1..p7` with `p8` as the d[16]
  pack base). Prologue now identical but `stmw r21/-0x40` (ours r23/-0x30); block 1 lives in r7-r12 + r25-r28 (target + r21-r24, r31);
  first block-1 divergence at instruction 11 (target `addi`, ours `add`), load order 18/20. Not flipped (H2/V2 225w); objects.py untouched.
- **Residue, exact class:** the target's block-1 interference graph is NOT ours: with the loop variables fixed, the 8 pixel/sum orders x
  8 declaration-group orders never exceed 24/54 target colours in the offline model, and the pre-RA schedule is identical across all
  the spellings tried (masked/unmasked pixels, casts, `inter=1`, `ba=1`, 8 load/sum orders, p8 placement) — so the vendor's pre-RA
  schedule differs, i.e. their raw order or DAG differs in a way not yet found (the list-scheduler model in `lsched.py`, height
  priority + raw-order ties, 1-4 wide, 288 latency combinations, reproduces at most 16/79 of OUR pre-RA order: the compiler picks
  `lbz b0, a1` (p0's first-add operands, height 13) over `lbz b2` (p1's, height 14) at position 3 — the pre-RA scheduler is not a
  plain height list scheduler; it walks the sums' first-add operand pairs p1, p0, p2, p5, p3, p4, p6, p7, p8 — read the algorithm
  before more permutations). Own-local pixels (needed for the colours) and the masked-load split (needed for the block boundary)
  exclude each other in every spelling tried (E in [28,35] needs one deleted instruction per LOAD that does not turn the local into
  the load temp's copy); the vendor may have had a different per-load spelling or a different first-half statement set.
- **Transfer:** mpv_mcy H2 225 -> 222w with `Uint32 w0..x3` declared before `i/s/stride/d` (fixes `li r7,0x10` but moves `i` to r30, the
  size gap 0x470/0x468 = the case-0 `mr` copies stays), V2 225 -> 224w: not applied (no read mechanism, 1-3 words). mpv_mc 8x8 4p
  (72w) read with chaitin: ours L2 = loop vars + own-local pixels a2..a7/b5 (unmasked pixels ARE own locals there), loop vars pop
  first (stride r0, s0 r4, s1 r5, d r3); the target has `li r0,8` + d r3, stride r4, s0 r5, s1 r6 and `lbz r0 (b0)` / `add r0 (a1+b0)`
  in the body, so ONE body node popped before its loop variables and took r0 (a same-level node with a higher vid: a pixel declared
  before the loop variables or a sum temp at level 2) and `d` popped before `stride` (d declared before stride) — the 8x8 needs
  exactly one level-2 body node above the loop variables, ours has seven pixels; not closed. `MPVMC08_OneRef1p_TuneC` stays asm.
- Harnesses ~/.cache/cri_swar9 and ~/.cache/cri_swar10 deleted at the end of this pass (15 min to rebuild from this text: gen10.py's
  option list above, rr.sh = ra.py + cnt.py + blk.py + sum.py + osearch.py). Kit untouched.
- **Loop-B nx8 placement (42: all 9w; 45: the same + the downstream tail = 42w), read further, NOT closed; no pin/anchor lever
  reproduces it (none applied).** New facts (rs6000 `REG_ALLOC_ORDER` for the GPRs is r0, r9, r11, r10, r8, r7, r6, r5, r4, r3, r31..:
  earlier notes assumed r0, r11, r10, r9):
  1. The target Z block needs exactly two local-alloc ties to FAIL that ours make: `nx -> nx+1` (`lhz r11; addi r9,r11,1`) and
     `ix32 -> prod` (`mullw r9,r10,r9` = `(mult ix32 nx8)`, prod tied to the SECOND operand: block_alloc tries operand 1 first and
     stops at the first win, so ix32 was not tie-able). `combine_regs` refuses only for: `reg_qty[ureg] < 0` (ureg not block-local
     or `REG_N_DEATHS != 1`), `reg_qty[sreg] == -1`, a hard register, class disjointness (never for two GPR classes:
     `reg_meets_class_p` is contains-or-contained), or call-crossing mismatch. So both `p->nx` and `(t_i>>2)<<5` were not
     local-alloc candidates in the original, and both are computed and used inside Z. Mechanism not identified.
  2. With the ties broken, the rest of the target's registers follow from the order r0, r9, r11, r10 and local-alloc's +-1 fake
     overlap (a value dying at insn N conflicts with one born at N unless tied): chain {nx1, nx8, prod, sums} is BASE class
     (stbx index, r0 excluded) -> r9; jx32 -> r0 (jx and t_j then prefer r0 through set_preference/expand_preferences: `pref`
     comes from the LOCAL register of the consumer `slwi jx32`; ours e1 gives jx32 r6 and so t_j/jx r6); nx (BASE, overlaps the
     chain at the addi) -> r11; ix32 -> r10 needs r0 taken by jx32 (slwi before mullw in the sched1 order) and r9/r11 by the chain/nx;
     t_i (global, pri 33750) -> r0 excluded by `smpref` (jx prefers r0), r9 free (chain born after t_i's death) -> r9.
  3. The `mr r10/r7/r11/r6(/r5),r8` copies at the body end are the `-mfast-cast` `floatsidf2_loadaddr` pseudos of the three
     float conversions (reload turns the unspec into a copy of r8 = hA); sched1 hoists them to the top of Z as soon as the integer
     chain stalls (in e1 they take r8/r10/r7 and push jx32 to r6). In the target they do not overlap ix32 (r10), so its sched1
     order had them after the mullw.
  Tried and failed (42 words in brackets, tree = 9): `((i / 4) << 5) * ((p->nx + 1) >> 3)` operand swap [49: lhz in Z, t_i r0, jx
  r6, ix32 tied]; function-level `nxb`/`nx1` with a second set in loop A or the loop-B outer head [47-71]; pins `nxb r11` [58],
  `+ ti r9` [45], `+ ix32 r10` [44: the pinned ix32 gives prod the r10 suggestion], each with an `asm("" : "=m"(tmp) : "r"(x))`
  life-extender [38-61]; codeless anchors with 2-4 inputs after the mullw to move the deaths [46-180: the anchor store shifts
  the global order]. The only lever that changes the tie pattern to the target's (`mullw r9,r11,r9`) is the 4-input anchor and it
  costs 170 words elsewhere. Next: the two non-candidates must come from the ORIGINAL's expression shape (a second death or a
  second block for both `p->nx` and `(i/4)<<5`); read `REG_N_DEATHS`/`REG_BASIC_BLOCK` of 528/527 in a `-dl` dump of any form
  that reproduces it, do not permute pins further.
- Flags untouched (no IDENTICAL). Tree edits: `src/game/Espgen42.cpp`, `src/game/espgen45.cpp` (the `k4` statement + two-set `c`,
  comments). 111 not re-run (no flag change, both objects compile). Harness ~/.cache/dol_espg9 deleted.
- **cse1 flush 2 fitted (on top of the cse2-flush-1 fit): SAVE `{ }` (K=0) + SAVE_EM `1:2`** = 6 cse1 insns fewer between F1 and
  F2, F2 SAVE_EVENT+21 -> +27 = precedes `h = 128.0f`'s `high` (cse1 stream: 23-25 `w` high/lo_sum/mem, 26 `stfs w`, 27-29 `h`),
  so `w` is shared with LOAD_EVENT and `h` fresh as in the target's seg 271. Variant /tmp/t18/y6.cpp: grids cse1 LOAD_EVENT+4
  SAVE_EVENT+27 OPTION+540 PATH+541 SIZE+186 SPEED+692 COLOR+867 LIFE+8 ROTATE+671 WORK0+18 WORK6+14 BASEPOS+123, cse2
  LOAD_EVENT+53 OPTION+71 PATH+349 SPEED+129 COLOR+571 ROTATE+22 SUB+96 WORKSP2+12; N 5235 (76 sets); 2119w, diffsegs 360,
  mset regions [2 8 5 3 5 19 6 9 18 5 56 6 28 9] (base [2 11 5 10 4 14 6 9 19 5 56 15 46 11]). SAVE K=0 alone (F2 +25 = before
  `w`'s load) is 2111w but region 3 mset 6: fewer words is not the better grid, judge by the fresh/shared reading.
- **cse1 flush 3 (OPTION+540) scan -4..+12 on top of y6 (`/tmp/t18/z*.cpp`): region 4 (297-352) mset is 5 at every shift** —
  OPTION's 40 segments have no constant whose fresh/shared state changes in that range, so F3's interval cannot be read from
  OPTION; it must be read from the first constants of DATASET/TIME/ID (segs 339-352 are exact already) — i.e. F3 is
  unconstrained within +-12 and F4 (PATH+541) is the next one to read: PATH region mset 19 (segs 354 378 380 381 391 392 402).
  Later regions move with every shift (+1 later: region 11 mset 6 -> 0, region 12 28 -> 21; -4: 15/46), so the tail flushes
  F9..F12 want ~+1..+2 relative to y6 and F5..F8 something else: fit them in order with the (n,0) pads of the windows between.
- **Seg 0's `&pos` slot offsets are NOT a PRE-order/N question once N is 5233/5235: reload numbers the spill slots sequentially
  over the spilled pseudos in regno order, so one more or one fewer spilled pseudo anywhere shifts every later `&pos` offset by
  4 (the "permutation" of pass 17 = a different SPILL SET = global pressure).** Every grid variant here shows it (y6 seg 0
  d58: 0x9a0 -> 0x9a8, 0x1a9c -> 0x282c...). It is a whole-function allocation consequence and can only be judged when the
  grids and the live callee-saved set match; it is not a reason to reject a grid fit, but it blocks applying one to the tree
  under the "seg 0 exact" rule, so NOTHING WAS APPLIED (tree src/t_esp/t_esp.cpp unchanged, base 2182w).
- **Remat vs fresh ambiguity in the mset metric.** Segs 230/238/271 in y6: ours `lis r,g_pPrimArray@ha; lwz` where the target
  has `lwz r0,@l(r18)`: the high IS shared at cse1 in both (SAVE's high is the head after F1), ours is a spilled REG_EQUIV
  pseudo rematerialised by reload, the target's got r18. That is allocation (more live callee-saved values in ours over
  SAVE..SAVE_EVENT), not the grid; the CreateString-label highs (`r17` of seg 219, cse2) are the grid signal. Read `lis` of
  fixed globals as remat candidates, `lis` of `.LC` labels as fresh/shared evidence.
- Kept: /tmp/t18 (mk.py per-window pads incl. `NAME=a:b:c` literal pads and the macro classes, scan.py, fl.sh, shape.py,
  flushes.sh, wincount.py, autoN.py, y.sh; base.cpp = tree source). ~/.cache/tesp15 untouched. Not run: ninja, make_rel,
  shasum (nothing flipped). Next: (1) fit F4..F12 (cse1) one by one on top of y6 with the pads of the windows between, reading
  each interval from the first `.LC` constant of the windows after it (fresh `lis`+`lfs` vs a callee-saved reuse), (2) fit
  cse2's flushes 2-8 the same way (their knob is the ending-in-4 pad of the first-after-cse1-flush window: SAVE_EVENT,
  DATASET, PARENT, SPEED?, COLOR?, BLEND, RELEASE, VEC0, WORK1, WORKSP0, and a not-ending-in-4 pad in the windows after it to
  hold cse1), (3) only then the callee-saved names and seg 0.

### Tool RELs, t_event closer 5 (t_event/t_event 17 words and Tools/t_esp_area 7 words READ, nothing applied: SubToolMessMove 17 unchanged, ToolEspArea 7 unchanged; dbg_tool.h and t_event.cpp untouched; not flipped; 2026-09-12)
Scratch ~/.cache/tev5/ (`hv.sh HDR [units..]` judges a dbg_tool.h variant on all five includers through `variant.sh` (rewrites
`#include "dbg_tool.h"` to the variant's absolute path; 1.2 s for the five), `mk.py OUT [--base=B] OLD NEW..` exact-substring variants,
`ins.py DUMP FUNC [regex]` one line per insn of an `rtl.sh` dump with block/loop notes, `try.sh V.cpp` = SubToolMessMove word lines;
h*.h header variants, v*.cpp t_event variants; delete at the end of the family). No tree file edited by this pass.
- **ToolEspArea 7w = the block-13 local-alloc tie, numbers confirmed (LADBG, `variant.sh` baseline reproduces 7/4/IDENT/17/IDENT):**
  `q0 reg299 work refs 4 birth 4 death 52 pri 1666 -> 29`, `q1 reg301 name refs 5 birth 6 death 48 pri 2380 -> 30` (placed first),
  `q2 reg297 li 4 refs 2 birth 14 death 26 pri 1666 -> 28`, `q3 reg298 li 25 [18,32) 1428 -> r0`, `q5 reg303 li 5 [34,50) 1250 -> 28`,
  `q6 reg309 li 32 [38,54) 1250 -> 11`. pri = floor(log2 refs)*refs*10000/life; qty numbers = birth order, tie -> lower qty (work).
  The suid map (sched1 output = lreg dump order): 912 lis work 4 | 914 lis name 6 | 915 addi name 8 | 913 addi work 10 | 922 li r3,0x304
  12 | 909 li 4 14 | 923 bl new 16 | 911 li 25 18 | 924 mr r31 20 | 939/940 vt 22/24 | 943 stw x 26 | 941 stw vt 28 | 945 mr r3 30 |
  944 stw y 32 | 917 li 5 34 | 946 bl strlen 36 | 928 li 32 38 | 956 li 0 40 | 949 li 1 42 | 948 stw w 44 | 954 stw cyMax 46 | 955 stw
  pName 48 | 960 stw rows 50 | 961 stw pWork 52 | 962 stw numWork 54 | ... Sched1 facts: 912 and 914 both prio 12 (the `addi` of each
  must precede `bl new`: their highs have REG_N_CALLS_CROSSED 0 -> `sched_before_next_call` anti-deps), tie -> LUID -> lis work first;
  909 (prio 8) takes the second slot of t=3 (after 922, prio 11) ahead of 911 (prio 8, higher LUID); 917 (prio 5) takes the slot before
  `bl strlen` at t=8; the post-strlen stores go in LUID order among the weight -1 (dying-source) ones: 948 954 955 960 961 962, then the
  weight-0 ones. In t_lightarea the same block has work = `lwz` (q2 reg313 refs 2 [10,52) 476 -> 28, the `lis` high q0 [4,10) 3333 ->
  r9) and the same slots, so the constant is first there by priority; only the combined `lis/addi` qty ties.
- **What the target needs (unchanged): work pri < 1666 or wx pri > 1666.** Routes and why each fails from the shared header:
  (a) work life 50 = `stw pWork` at suid 54: needs an RA-time insn between 960 and 961 in sched1's output that leaves no code; the store
  order itself is LUID-driven and the target has pWork before numWork. (b) work refs 3: impossible while the high is combined (2+2). (c)
  wx refs 3 with life <= 16, or wx born after `bl new` (life 8) with the `li 4` still hoisted above the call by sched2 (legal: r29 is
  not call-used, `li r0,25` is). (d) loop-depth doubling of REG_N_REFS (work 8 -> 5000 vs wx 4 -> 6666 flips it) but the LOOP notes
  change sched1/loop.c: `do {} while (0)` around x..x20 47/51/60, around `x = wx` 19/22/25, around `x = wx; y = wy` 19/22/25.
  Tried and rejected (t_esp_area/t_lightarea/t_event words): `T* dead; asm("" : "=r"(dead) : "r"(work))` at the ctor top 7/4/17
  (deleted before RA: flow removes a non-volatile asm whose only output is dead; LADBG numbers unchanged), the same with `"r"(wx)`
  7/4/17, `register int xw asm("r29"); xw = wx; x = xw;` 18/21/37 (the copy takes a sched1 slot), `register T* pw asm("r28"); pw = work;
  pWork = pw;` 7/4/17 (cse canonicalises the store back to the pseudo, the pin copy dies), `asm("" : "=m"(x) : "r"(wx))` before `x = wx`
  7/4/25 (refs still 2 per LADBG: the anchor is gone before RA) and after it 14/12/29. Not tried: an anchor carrying `wx` that survives to
  RA without a memory operand (none found: an asm output must be a MEM or a used register), caller-side forms in t_esp_area.cpp (not
  owned by this pass; the previous pass measured the post-ctor `asm("" : : "r"(esp_area_work))` at 226).
- **SubToolMessMove 17w, the mesCnt block read to the cse mechanism (cse.c find_best_addr, rs6000 `ADDRESS_COST(X) 0`):** at equal
  address cost cse replaces a MEM address by the class member with the HIGHER rtx cost (`(p->cost + 1) >> 1 > best_rtx_cost`), so a
  `(mem rB)` with rB = `(plus D 0xc4)` in its class becomes `0xc4(D)` -- that is ours' `lwz r8,0xc4(r30)` for `mc[0]`; the `4(rB)` read
  survives because `(plus (plus D 0xc4) 4)` is not a valid address. Members are skipped when stale (`exp_equiv_p` with validate: a REG
  whose tick changed). The target's `lwz r8,0(r28)` therefore had D invalidated between rB's set and the [0] read, or rB not in D's
  class. Dest addresses get only `canon_reg`, never fold: `EvtDebug.mesCnt[no] = no` expands with the sum INSIDE the MEM (ARRAY_REF via
  get_inner_reference + LEGITIMIZE_ADDRESS `(plus reg reg)`), so `(ashift no 2)` is canonicalised to `no` (its class {0, no, ..} has `no`
  as first reg) and stays `stwx no,A,no`; `mc[no]` with mc a POINTER expands `(set T (plus mc (ashift no 2)))` + `(mem T)` (INDIRECT_REF of
  PLUS_EXPR goes through force_operand), the SET's source folds to `mc` (no = 0 known in the arm) and the store becomes `0(mc)`. The
  target's `stwx r0,r26,r27` therefore came from an ARRAY_REF whose base pseudo is `D + 0xc4` (fresh per expansion, hence the loop-hoisted
  copy `mr r26,r28` = reload_cse/cse2 rewriting `addi mc2,D,0xc4` to a copy of rB), and its `lwzu r8,0xc0(r30)` is combine merging
  `(set A (plus D 0xc0))` into a `(mem A)` read whose address cse did NOT rewrite (same staleness condition), A then feeding the loop's
  `stwx no,A,no` by PRE. Variants measured (SubToolMessMove words): `d = &EvtDebug;` re-set after the 1st eprintf 17 (the no-op set does
  not bump D's tick), `asm("" : "+r"(d))` there 28 (`lwz r8,0(r27)` appears -- the staleness mechanism confirmed -- but D is
  rematerialised for the loop and the size changes), `s32 (&mc)[2] = *(s32 (*)[2]) &d->mesCnt[1]` 17 (the frontend still emits pointer
  arithmetic), `EvtDebug.mesCnt[no + 1]/[no + 2]` stores 27 (constant offsets fold). Open: a C form that (1) invalidates D between the rB
  set and the [1]/[0] reads without a second `addi EvtDebug`, (2) writes the loop's `[1]`/`[2]` stores as an ARRAY_REF with a `D + 0xc4`
  base (a struct member array at 0xc4 in the original EventDebug -- `s32 mesCnt; s32 mesNo[2]`? -- would give exactly `(mem (plus base no))`
  for `mesNo[no]` and `4(base)` for `mesNo[1]`; check game/event.cpp's other mesCnt uses before changing include/event.h), and (3) leaves
  e/cMes as r29/r28 (the downstream pair follows once rB dies at the [0] read).
- Flip order unchanged: t_esp_area needs IDENTICAL first (its 7 words), then t_lightarea's 4 vtable-reloc words, then the Tools REL.

### Tool RELs, t_id pass 8 (toolIdOption 8 -> 0 pure C; t_id/t_id IDENTICAL and FLIPPED, make_rel --verify OK, 111 OK; the 2/2/2/4 artefact rows vanished with the flip as predicted; 2026-09-12)
Harness ~/.cache/tid8/ (deleted at the end): variant copies v1-v4, rtl.sh dumps of the base, `gdbg.log` (GDBG=1 LOOPDBG lines).
- **The pass-7 hypothesis (gcse PRE gives the optMenuName high the lowest LUID) was WRONG in the mechanism but right about the
  symptom.** `-fno-gcse` via rtl.sh keeps the same preheader order (`lis pool; lis optMenuName; lfs; lis langName2; lis Screen`, 70w
  elsewhere), so PRE is not what puts it first. Read off `GDBG=1` `LOOPDBG` (production flags through variant.sh; a bare
  `cc1plus -O2 -mfast-cast` re-run of the .i has DIFFERENT insn/reg numbers, use the kit): loop pass 1 (ic 131) movables in body
  order are `290 = high optCur` [thr 71 sav 1 life 25 -> move], **`293 = lo_sum optMenuName` [thr 68 sav 1 life 2 = 136 >= 131 ->
  move]**, sx/r1/r2, langName2 [56*2*5], Screen [50*2*2], pool [44*2*2]; the lo_sum's REG_EQUAL `symbol_ref` makes it a `move_insn`
  movable, re-emitted by `gen_move_insn` as a fresh `high` + `lo_sum` pair (pseudo 405) at its LIST position = second, hence the
  early `lis optMenuName` (sched1 prio-2 tie by LUID). The PRE'd high 398 only turns the body's high into a copy (`294 = 398`) that dies.
- **Target = the lo_sum missed pass 1 and hoisted in pass 2.** loop_optimize runs twice (`flag_rerun_loop_opt`, toplev.c 4199/4212
  with `delete_trivially_dead_insns` between); pass-2 movables and giv inits are emitted before `loop_start` = AFTER every pass-1
  hoist, so a pass-2 `high/lo_sum` has a LUID after langName2/Screen/pool and sched1 issues it last among the prio-2 highs
  (target: `lis pool; lis langName2; lfs; addi r17; lis Screen; lis optMenuName; addi r18; addi r26`). Lever: one more pass-1
  movable BEFORE the lo_sum in body order costs it 3 of threshold: `sx = 0x2E;` ahead of the menu-name eprintf -> thr 65*1*2 = 130
  < 131 -> stay; pass 2 (ic 104, thr 71: 142 >= 104) moves it first, then mx/vx, then the giv init `412 = 293` that combine merges
  into `addi r26`. 8 -> 2w with `sx` alone (its `li r27,0x2e` then precedes `lis optCur`: body order sx < optCur); **`col = (i ==
  optCur) ? 4 : 0; sx = 0x2E; eprintf(.., col, .., optMenuName[i])` -> 0w** (optCur's high is scanned before sx again; `int c =`
  or `int sel = (i == optCur)` locals give 0w too; `col` chosen — it is the body's colour variable). The comment in the source
  records the arithmetic.
- General rule (new for the catalogue row 1): **the same movable can hoist in loop pass 1 or pass 2, and the pass decides its
  preheader LUID** — pass-2 hoists come after all pass-1 hoists and giv inits. When a hoisted `lis/addi` pair sits too EARLY in
  ours, do not look for a later body position: push its `thr*sav*life` under pass-1's `insn_count` (an extra pass-1 movable ahead of
  it = -3 thr, or +ic) and let pass 2 place it. `GDBG=1` prints both passes' decisions (second block = ic of pass 2).
- Flip: `"t_id/t_id.cpp": True` in config/G4BE08/modules.py; locked `ninja build/G4BE08/t_id/t_id.rel`; `make_rel.py --verify
  orig/G4BE08/files/Rel/t_id.rel --out /tmp/x.rel` -> `OK (94652 bytes)`, `cmp` identical; bytecmp t_id/t_id IDENTICAL (the
  `_._6cCoord`/`_._7ID_DATA`/`_._5cUnit` 2w and `__static_initialization_and_destruction_0` 4w rows disappeared once the module ELF
  was linked from our object, confirming pass 7's weak-`_vt.5cUnit` reading); `flock ... ninja -k 0` + `dtk shasum -c` = 111 OK;
  `git diff config/G4BE08/symbols.txt` empty. Tree edits: src/t_id/t_id.cpp (toolIdOption body top), config/G4BE08/modules.py.
  No tagged forms added; t_id/t_id has none. The t_id module is now fully Matching (tools, db_path, db_sctrl, t_util, t_id).

### Tool RELs, t_esp pass 19 (t_esp 208/212: InitTool 2182 -> 2119w in the tree = pass 18's y6 applied (seg 0 d56 = the spill-set symptom, recorded); cse1 F4 and F7 read and fitted, POS_MINMAX is a reference store (`FSet`), best variant /tmp/t19/p20b.cpp 2030w not applied (seg 0 d94 > y6's); the per-constant timeline tool; nothing flipped; 2026-09-12)

- **Tree:** src/t_esp/t_esp.cpp = /tmp/t18/y6.cpp (MODEL `1:2:3:5..12:4`, LOAD/LOAD_EM/LOAD_ROOM/LOAD_SST `1:2`, SAVE `{ }`, SAVE_EM `1:2`,
  the macro classes `cls##_CSE_PAD()` all `1:2:3:4`); locked ninja + bytecmp: InitTool 2119w, 208/212, .rodata/.data/.bss unchanged.
  Seg 0 d56 = `&pos` slot / 0x28xx spill-slot permutation only (size 1850/1850): the spill-set symptom, judged last (pass 18).
- **Reading tool (kept in /tmp/t19, disposable but 10 min to rebuild): `tl.py O_init.s [LABEL..]`** = per pool constant the timeline of
  segments where the target / ours load it FRESH (`F`) or issue an `fmr` copy right after the fresh load (`C`); target labels
  `lbl_t_esp_rodata_X` = ours `.rodata+0x(X-0x35E0)`. `lc.py O_init.s` = the .LC-only region metric (mset restricted to rodata-label
  insns: the grid signal without allocation noise); `ss.py DUMP WINDOW` = `high .LC` / call offsets per final segment (its seg column
  is 1 low); `win.py DUMP WINDOW a b` = the raw stream; `v.sh NAME PAD..` = mk.py on y6's pad set + autoN + both grids + fl.sh;
  `scan.sh PREFIX "BASE" "L:pads".." runs variants in parallel (7 in 45 s).
- **Reading rules confirmed.** (1) A constant is shared in the final code if cse1 shares it (no flush between the two loads) OR cse2
  rescues it (the earlier load must be a cse2-time LOAD insn, i.e. itself cse1-fresh, inside the same cse2 window); a rescue is an
  `fmr` copy that survives only when the source stays live (sched1 hoists the copy above the store), so `C` is weak evidence and `F`
  is the strong one. (2) The pass-18 "(n,0) pad" table holds for long pads (`1..20`). (3) A cse1 flush between the two `addressof`
  sets of a `DB_POINT pos` (the ctor stores and the `&pos` argument) gives `addi rX,r1,pos` for the argument + both stores through
  the pointer; outside the pair the first store goes direct (`stfs f,pos(r1)`) and the argument reuses the pointer. The target has
  that `addi` form ONLY at seg 698 (WORK0, 0x1630) = y6's F10 (WORK0+18) is inside the right pair; but target seg 271 (F2 between
  `w` and `h`, inside SAVE_EVENT's pair) shows the pointer form, so the `addi` signal is not reliable on its own — read the `.LC`s.
- **F4 (PATH) read and fitted:** target seg 377 shares 284.0/16.0 (.LC1581/.LC1514) and its second `addressof`, seg 378 loads 284.0
  and 32.0 fresh with the pointer shared -> F4 in y6-stream [PATH+525, PATH+537]; ours +541 -> PATH pad `1..8` (+4 insns, F4 at
  537) .. `1..16`. With `1..12` (d=8): seg 378 mset-exact, PATH region mset 19 -> 14; the rest of the PATH region (380/391 remat
  `lis g_pPrimArray`, 381/392/402/412/414 `li 1` for keyMode fresh in the target vs shared) is allocation/const-1 sharing.
- **POS_MINMAX (segs 406-414, pure C):** the target reloads `g_pPosNumX` after the `max` store (`lwz; stfs 0xa0; lis; lwz; stfs 0xa4`):
  `#define POS_MINMAX(n) FSet((n)->max, 327670.0f); FSet((n)->min, -327680.0f);` (reference store = may alias the fixed scalar,
  lever-catalogue alias.c row). PATH region mset 14 -> 3 on top of the F4 fit. In /tmp/t19/base.cpp only, NOT in the tree (see below).
- **F5 (SIZE), F6 (SPEED) unreadable:** every constant between cse2 F3' (PATH+349) and F4' (SPEED+122) is rescued by cse2, `lc.py`
  shows no .LC evidence in SIZE/SPEED for shifts -12..+16 (`/tmp/t19/f5_*`); left where the F4 fit puts them.
- **F7 (COLOR) read and fitted:** 50DC (16.0, .LC1519) is SHARED at seg 543 (COLOR+857..859) and FRESH at seg 561 (FLAG+55..58) in
  the target; ours (after the F4 fit) flushed at COLOR+847 (543 fresh, 561 shared). F7 target in [COLOR+860, FLAG+58]; fitted by
  removing pads: PATH `1..8` + POS/SIZE/SPEED/COLOR `{ }` (PARENT keeps `1:2:3:4`: it is first-after-F4, its `d_ = 4` is cse2's) =
  F7 COLOR+863, and F8..F12 land back on y6's positions (LIFE+8 ROTATE+671 WORK0+18 WORK6+14 BASEPOS+123: the F10 addressof form
  at 698 returns). Variant **/tmp/t19/p20b.cpp: 2030w**, lc segs 17 -> 10 (543/561/681/698/699 fixed), mset [2 8 5 3 5 8 5 9 18 5
  56 6 28 9] vs y6 [2 8 5 3 5 19 6 9 18 5 56 6 28 9], N 5235. NOT applied: seg 0 d94 (y6 d56; same permutation class, sizes equal)
  and the task's "seg 0 not worse than y6" guard. The F7 `1:2` pad forms (q_A..q_D) do not bring the `li 1` keyMode form back.
- **What the remaining .LC evidence says (p20b): all cse2.** 50EC (.LC1523): target fresh at 619 (ANMRATE+76) although loaded at 571
  (FLAG+208) with no cse1 flush between (F8 is FLAG+7xx in both) -> the target's cse2 window boundary (F5'/F6') lies between
  FLAG+208 and ANMRATE+76; ours F6' = ROTATE+17 (.loop). The same boundary explains 621 (50D4 fresh at ROTATE+15, loaded at 579),
  and 663/690 (ours fresh: ours F9 ROTATE+671 sits between 619/621 and 663/690 and cse2 cannot rescue because 619/621's loads
  precede F6' = ROTATE+17; the target's F6' before 619's load rescues both). So cse2 F6' must move ~40-80 .loop insns EARLIER
  (ending-in-4 pads lengthened in the first-after-cse1-flush windows before it, cse1 held with `{ }` pads after) — F8, F9 stay.
  623C/624C/625C (target copies) are the weak signal (VEC-window uses rescued, copy hoisted above the store).
- **F1 (LOAD_EVENT+4)**: LOAD_EVENT's own pad (+6..+9) sits AFTER F1, so LOAD_EVENT is the "first window after F1" for the cse2 knob,
  not SAVE. F2's interval is one insn wide (SAVE_EVENT+26/27), so any cse2 pad growth before it must be matched exactly by `{ }`/
  `1:2` pads in SAVE_EM/SAVE_ROOM/SAVE_SST (10 insns available).
- Not run: ninja -k 0, make_rel --verify, shasum (nothing flipped). Kept: /tmp/t18 (pass 18), /tmp/t19 (this pass; p20b.cpp, g8.cpp
  = F4 fit + FSet 2058w, base.cpp = tree + FSet). Next: (1) decide on p20b (2030w) vs the seg-0 guard, or first move cse2 F6' as
  above and re-measure seg 0; (2) cse2 F5'/F6' fit with tl.py on 619/621/663/690; (3) callee-saved names, seg 0 last.

### Tool RELs, t_event closer 6 (t_event/t_event 17 -> 0 words, IDENTICAL and FLIPPED: SubToolMessMove's mesCnt block read to the expand/cse/combine mechanism, no header change; make_rel --verify OK after `MessTool` made non-static; 111 OK; 2026-09-12)
Scratch ~/.cache/tev6/ (hv.sh = event.h variant on game/event + t_event through variant.sh with the absolute-include swap, blk.py = the
mesCnt block of every RTL dump; deleted at the end with ~/.cache/tev5/). Tree: src/t_event/t_event.cpp (the block and `MessTool`),
config/G4BE08/modules.py (`"t_event/t_event.cpp": True`). include/event.h UNTOUCHED (the header hypothesis is not needed, see below).
- **The 17 words were three address forms, all decided before RA.** Target: `addi rB,rD,0xc4`; `lwz 4(rB)` [2]; `lwz 0(rB)` [1];
  `lwzu r8,0xc0(rD)` [0] (rD becomes rA = &mesCnt[0], used by the loop's `stwx no,rA,no`); loop `stwx messNo,rP,no` / `stw i,4(rP)` with
  rP = `mr r26,r28` (a loop-hoisted `(plus D 0xc4)` that cse2 rewrote to a copy of rB); then `lwz r0,0x10(e)` BEFORE the three stores.
  (a) `0(rB)` is NOT rewritten by cse when the address is `(plus rB idx)` with idx a REGISTER holding 0: find_best_addr only improves a
  bare REG or `(plus REG CONST_INT)` (its fold branch needs a HIGHER rtx cost, so `(plus rB 0)`->rB is rejected), canon_reg keeps the
  index reg, combine later folds the index's `(set idx 0)` (same block, LOG_LINK) into `(mem rB)`, and rB's set cannot merge (used in
  between). The index register must be set in the SAME basic block before the reads: `i = 0;` before the eprintfs (the for's own `i = 0`
  is then a deleted no-op; `li r31,0` appears once). A block-top `no` reused by the loop fails (multi-set, no hoist: 12w).
  (b) The address stays inside the MEM only for an ARRAY_REF whose base is the POINTER VARIABLE: `p->v[i]` with `struct MesCntView { s32
  v[3]; }* p` (COMPONENT_REF base, cp build_array_ref). `s32* mc; mc[i]` computes `(set T (plus (ashift i 2) rB))` + `(mem T)` -> cse
  folds T to rB and rewrites `0xc4(D)` (17w, ours). A pointer/reference to array is pointer arithmetic (`TREE_CODE (array) ==
  INDIRECT_REF` -> `*(a + i)`; closer 5). A struct member array off `d` (`d->mesCnt[i+1]`) gets a FRESH single-use base that combine
  merges into `0xc4(D)`. Array bound matters: with `v[2]` the frontend computes the address into a pseudo again (`(set T (plus rB
  ashift))` + `(mem T)`, 36w); `v[3]` keeps it in the MEM.
  (c) `lwzu` = combine's `*movsi_update1` (i2 `(set A (plus D 0xc0))` + i3 `(mem A)`, A live into the loop -> PARALLEL, reload ties
  A to D). Combine never crosses a CALL with a non-constant source (can_combine_p `last_call_cuid`), so A's set must sit AFTER the fourth
  eprintf: `x = (MesCntView*) &d->mesCnt[0];` between eprintf 0x140 and 0x190, and the loop's `x->v[no] = no` must use the SAME pointer
  (a fresh loop base would be a `mr` copy: cse2 rewrites it to A, local-alloc cannot tie a global pseudo).
  (d) The loop's [1]/[2] stores through a loop-fresh `MesCntView* y = (MesCntView*) &d->mesCnt[1]` (`y->v[no]`, `y->v[1]`): hoisted,
  cse2 turns `(plus D 0xc4)` into `(set P rB)` = `mr r26,r28` (P global, rB dead -> no tie).
  (e) Store order: `lwz r0,0x10(e)` ahead of `stwx no`: the messNo re-load conflicts with the `x->v[no]` store (e's base is unknown --
  loaded, multi-set; -fstrict-aliasing is OFF in this SN build: toplev.c `flag_strict_aliasing = 0`, so the s32/int alias sets do not
  separate them), so in the target's RTL the load PRECEDED store a: `mes = e->messNo;` after the MesSet call, then `x->v[no] = no;
  y->v[no] = mes; y->v[1] = i;` (LUID order). The six statement orders without the local: 4/4/2/2/3/3 words.
- **Header variants (judged first, none needed).** `s32 mesCnt; s32 mesNo[2]` is ruled out by game/event MesClear (`lwzx r11,r9,r0` with
  r0 = no: an ARRAY at 0xc0); `s32 mesCnt[1]; s32 mesNo[2]` or a nested struct changes nothing for t_event: the shape needs pointer
  variables to structs with a leading array (b) whatever the member split, and constant-index member accesses off `d` fold to `K(D)`.
  event.h stays; game/event stays IDENTICAL by construction (no includer rebuilt).
- **REL flip detail:** with the unit linked, `make_rel --verify` had 20 bytes `ours c4 orig 00` = every `MessTool@l` field: the field holds
  S+A for a LOCAL symbol and A only for a global (make_rel header rule; module symbols.txt has `lbl_t_event_bss_C4 ... scope:global`), so
  `static struct { ... } MessTool` -> `struct MessToolWork { ... } MessTool` (non-static). Then OK (208220 bytes). sync_rel_symbols: 0
  changes, DOL symbols.txt unchanged. Full `ninja -k 0`, 111 OK. t_event is now 7/7 units Matching (t_esp_area/t_lightarea words are the
  Tools REL's, unchanged: 7/4).
- Mechanism rows for the catalogue: "cse rewrites `0(rB)` to `K(D)`" -> keep a zero-valued REGISTER index inside the MEM via a struct-pointer
  ARRAY_REF (`p->v[i]`, bound >= 3), index set in the same block; "`lwzu`/update form missing" -> the base's set after the last call before
  the read and the same pointer used later; "load after the first store, target before" -> cache the value in a local after the call.

### Tool RELs, t_esp pass 20 (t_esp 208/212: InitTool 2119 -> 2030w in the tree = pass 19's p20b applied (F4/F7 fits + POS_MINMAX `FSet`); the fresh-vs-callee-saved `lis`/`li` pattern of the tail read to reload's spill-reg rotation + find_equiv_reg inheritance; cse2 F6' knob measured; IN PROGRESS 2026-09-12)

- **Tree:** src/t_esp/t_esp.cpp = /tmp/t19/p20b.cpp (y6 + PATH `1..8`, POS/SIZE/SPEED/COLOR `{ }`, `FSet` POS_MINMAX); locked ninja +
  bytecmp: InitTool 2030w, 208/212, .rodata/.data/.bss OK, size 0xa20c/0xa0f4. Seg 0 d94 (slot permutation, sizes equal) accepted per
  the pass-20 decision (spill-set symptom, judged last).
- **Offsets are per-variant.** `flushes.sh` prints `WINDOW+k` counted in THAT variant's stream: a pad of +p insns at the window top
  moves every later same-window `+k` by p in code terms (p20b's PATH+541 = y6's PATH+537). Compare flush positions only after
  subtracting the pad delta (`wincount.py A.loop B.loop` gives the per-window deltas of both streams).
- **The `li 1` keyMode item (2) read: not a grid effect.** In p20b the PARENT `keyMode = 1` remat (pseudo r5407, single set, REG_EQUIV,
  not allocated) got the callee-saved SPILL register r14 from reload's spill-reg rotation (`li r14,1`, seg 381); `reload_cse_regs` then
  serves the 13 later stores of 1 (4 keyMode + 9 `int sx = 1` frame stores, segs 381-560) from r14 (`stw r14`). In g8 and the target the
  same remat took r8 (volatile: forgotten at the next call) and every store of 1 is a fresh `li r9/r10/r0/r11`. r14-r17 are reload
  registers in all three (4 `lwz rX,off(r1)` slot reloads each). The pick is the rotation state (`last_spill_reg`) = the number of
  reloads issued before seg 381, changed by the F4 shift (g8 `1..12` vs p20b `1..8`); no source lever, judge with the spill set.
- **Same mechanism for the `g_pEditSeq2` high (`/tmp/t20/hl.py O_init.s lbl_t_esp_bss_13C87C _15t_esp_namespace.g_pEditSeq2`
  prints the per-seg register of a symbol's high, T vs O).** Target: 350-377 r15, 378-443 r22, 461-651 r14 (ONE `lis r14`, 40 uses =
  exactly ours' four cse1-window heads 6131/6811/7532/8253 of the .loop dump), 656/659 `lis r11` fresh, 668-688 `lis r9` fresh per use
  (9), 702-751 r14 again, 757-775 `lis r9` fresh. Every head is a single-set `(set rH (high sym))` with REG_EQUIV (ours too: lreg
  "Register 6811 ... set 1 time", REG_EQUIV note) -> rematerialised by reload; a remat landing in a callee-saved spill register is
  inherited across calls by `find_equiv_reg`/`reload_cse_regs` until that register is reused (a `lwz r14,slot(r1)` reload), a remat in
  r9/r11 is fresh per use. So the 461-651 merge is NOT a cse2 window (2400 .loop insns): it is the r14 pick at 461. Ours (p20b) 350-377
  r16, 378-443 r21, 461-775 r15 (g8: r14 / r18 / r14). Use-count groups 10 / 16 / 40 match ours exactly -> the cse1 grid F3..F8 is
  consistent with the target; the boundaries the high shows (377|378 = F4, 443|461 = F4' or F5, 651|656, 688|702, 751|757) are the
  grid signal, the register names are the rotation.
- **cse2 F6' knob measured (scan /tmp/t20/s1_*, v.sh = p20b pads + overrides):** LIFE pad `1:2:3:5..n:4` (LIFE straddles F8 = LIFE+8:
  sets 1,2 before the flush, the rest after; `d_ = 4` is the head) adds n-4 cse2 insns AFTER F8 = F6' moves n-4 earlier with F5' fixed;
  BLEND's pad adds only ~4 cse2 insns (a `(const_int 4)` argument set in COLOR's tail after F7 makes BLEND's `d_ = 4` not the head);
  COLOR's pad moves F5' AND F7 (F7 must stay >= COLOR+860). Removing a (4,0) pad in FLAG/ROTATE ADDS 4 .loop insns to that window
  (RELEASE -1, ANMRATE 0): the (n,0) table is cse1-exact only. Results (words / lc regions): p20b 2030 [0 0 0 0 0 0 0 0 2 1 6 0 0 1];
  LIFE n=48 + RELEASE/ANMRATE/ROTATE `{ }` 2003 [0 0 1 0 0 0 0 0 1 1 3 2 0 2] (F6' ANMRATE+47: 2 short of 619's load at ANMRATE+45);
  LIFE n=60 + the three `{ }` 1933 [0 0 1 0 0 0 0 0 1 0 1 2 0 1] (F6' ANMRATE+35, F7' SUB+31, F8' WORKSP1+59, F9 ROTATE+623, F10
  SUB+162 = the WORK0 addressof pair lost); the region-8 residue (seg 545, 5104 fresh) needs F5' outside (COLOR+557, +582] or F7 >
  COLOR+880 (then 545 is cse1-shared): both blocked (PARENT is the only removable cse1 pad between F4 and F7, F4 sits at its late edge).

### Tool RELs, t_esp pass 21 (t_esp 208/212: InitTool 2030 -> 1811w in the tree: the F4..F7 "surplus" was the F7 reading, not a construct; cse2 F5' fitted (PARENT 16-set pad + `FSTORE_AT` POS_MINMAX, 2030 -> 1919w) and F6'/F7' fitted to one insn (LIFE 36-set pad, eight tail pads removed, 1919 -> 1811w); lc segs 10 -> 5; nothing flipped; 2026-09-12)

- **Tree:** src/t_esp/t_esp.cpp = p20b + (a) POS_MINMAX as `FSTORE_AT(n, 0xA0/0xA4, v)` = `*(f32*) ((u8*) (n) + off) = v`,
  (b) PARENT pad `1:2:3:5..16:4`, (c) LIFE pad `1:2:3:5..36:4`, (d) RELEASE/ANMRATE/ROTATE/SUB `{ }` and the macro pads
  VEC0/VEC1/VEC2/WORK0 `{ }`, (e) dead-set block 76 -> 32 (N 5235). Locked ninja + bytecmp: InitTool 1811w (size 0xa20c/0xa0fc),
  208/212, .rodata/.data/.bss OK, full `ninja -k 0` 111 OK. Flags untouched (no IDENTICAL, no make_rel).
- **Item 1, the recount (CSEDBG grids + `wincount.py` on p20b's `.jump`):** ours between F4 (PATH+541 of 579) and F7 (COLOR+863
  of 1020): PARENT 212, POS 578, SIZE 492, SPEED 823 = 3003 + 3 (flushmap offsets). The target's implied count depends on the
  F7 reading: (A) F7 in [COLOR+856, +880] (after 543's 16.0 load, before 545's 5104 load) with 545/547 rescued by cse2 from
  543's cse1-fresh load, which needs F5' outside (COLOR+557, +613] .loop -- ours' 863 already satisfies it, so the target
  has the SAME cse1 count as ours between F4 and F7 (0..24 fewer); (B) F7 > COLOR+916 (cse1-shared) needs >= 54 fewer. The
  per-window insn-kind tallies (`/tmp/t21/kinds.py`: argsets 736, loads 319, stores 273, addressof 260, LC high/lo_sum/mem
  251/251/243, sym high/lo_sum 198/125, plus 96, set-const 74) show no construct that could lose 54 cse1-time insns with the
  final code unchanged, and the region COLOR+863..WORK0+18 needs +35..+59 (the 44 tail pads) although ROTATE has the same
  statement mix as SPEED/COLOR -- so reading (B)'s deficit does not exist; (A) is the target. Pass 20's "≥18/≥53 fewer" is
  withdrawn.
- **The only cse1-only construct found: FSet's reference `addi`.** `.jump` -> `.cse` diff per window (`/tmp/t21/diffuid.py`):
  cse1 deletes `(set rA (plus rP 160))` of `FSet((n)->max, v)` by folding the address into the store (`find_best_addr`), so
  FSet costs (1 cse1, 0 cse2) per store -- the same class as the pad dead sets. `*(f32*) ((u8*) n + 0xA0) = v` gives the
  1-insn store `(mem (plus rP 160))` that is still NOT `MEM_IN_STRUCT_P` (expr.c INDIRECT_REF sets the flag only for a
  PLUS_EXPR / aggregate operand; the cast is a NOP_EXPR), so both alias effects stay: the pointer is reloaded for `->min`
  (fixed_scalar_and_varying_struct_p needs IN_STRUCT on the varying side) AND the next block's fixed-scalar `g_pEditSeq`
  loads stay below the min store in sched1 (segs 406-414 exact, PATH region mset 8 as before). The struct-VIEW form
  (`((NumPtrView*) &g)->p->max = v`, IN_STRUCT load + IN_STRUCT store) reloads the pointer too but frees the following
  loads from the store: sched1 hoists the next block's `lwz g_pEditSeq2` above it, r6 is clobbered, the second `lis` is
  rematerialised into r10 (segs 408/410, 21 insns vs 20) -- rejected. `offsetof` is not available in this TU.
- **cse2 F5' fit (2030 -> 1919w, /tmp/t21/c_c12.cpp).** Pads in a first-after-flush window are (n, n-1) = n cse2 insns minus
  the window's displaced `li flg,4`; there is no cse2-only knob, so F5' (COLOR+567, inside (557, 613]) can only move with a
  matching cse1 change: PARENT `1:2:3:5..16:4` = +12 cse1 / +12 cse2 (F5' -> COLOR+555, F4' SPEED+122 -> +110, no .LC change
  in SIZE/SPEED) and the FSTORE_AT form = -12 cse1 in POS (F5 SIZE+174 -> +186 alone, back to +174 together): the cse1 grid is
  p20b's to the insn, cse2 F6'/F7'/F8' move -12 (ROTATE+9, SUB+83, WORKSP1+114). lc segs 10 -> 8 (545 and 663 fixed), mset
  [2 8 4 3 5 8 5 9 18 5 54 0 18 7] vs p20b [2 8 5 3 5 8 5 9 18 5 56 6 28 9]. D = 10 leaves 663 (F6' -10 only), D = 13/14 lose
  681/698 (F7' too early). The COLOR-pad route (n, n-1) fails: it moves F7 below +856 (543's 16.0 fresh).
- **cse2 F6'/F7' (the LIFE knob, item 3; 1919 -> 1811w, /tmp/t21/M_l36.cpp).** With F5' at COLOR+555 the F6' target
  (<= ANMRATE+46: 619's load; l37 with F6' = +46 fixes 619, l36 with +47 does not) is 33 .loop insns away, not 46: LIFE
  `1:2:3:5..n:4` with RELEASE/ANMRATE/ROTATE/VEC0/VEC1/VEC2/SUB/WORK0 `{ }` (-32 cse1, VEC0's -3 cse2) keeps F10 at WORK0+18
  (pad-4 terms; F9 ROTATE+671 -> +651, F11/F12 unchanged) for n <= 36. Edges read off n = 34..44: 619 shared iff F6' <= ANMRATE+46
  (n >= 37); 681/698/699 shared iff F7' >= SUB+49 (n = 36 gives +51 OK, n = 37 gives +48 bad; p20b's +95 and c12's +83 were OK,
  s1_b's +31 bad, so the rule is "F7' after 681's load", not "not between"). F7' - F6' = 1001 puts the two edges ONE cse2 insn
  apart: the target has 1..3 fewer cse2-time insns than ours between ANMRATE+46 and SUB+49 (ROTATE/VEC0-2/SUB real code; the
  pads there are cse1-only), so both cannot hold with pads. Applied n = 36 (F6' ANMRATE+47, F7' SUB+51, F8' WORKSP1+82): lc
  segs 8 -> 5 = 221 619 623 690 782, mset [2 9 4 3 5 8 5 9 17 5 40 0 18 7], shape 1109 -> 1096. Seg 221 (LOAD_EVENT " Name :")
  is allocation: the string's `high` pseudo got callee-saved r18 in this variant (target fresh `lis r8`), a spill-set effect
  of the changed tail, not the grid (F1' unchanged). 623 = the weak `fmr` copy signal, 690 = F6' one short, 782 = BASEPOS.
- **Coordinates:** `flushes.sh` prints `WINDOW+k` in the variant's own stream; a pad of p sets at that window's top shifts every
  same-window offset by p (c8/c12 looked like "F7 unchanged" and were -8/-12 in code terms; FLAG `1..28` looked like F8 = FLAG+728
  twice). Adding insns before a flush moves it EARLIER; to move F8 later after a cse1 change before it, REMOVE insns between.
- **Item 5 (seg 0 / spill rotation): not touched.** Seg 0 d136 (p20b d94, c12 d128), same permutation class from line 698
  (`&pos` slot offsets / 0x28xx spill slots), size 1850/1850; POS segs 406-414 and the `li 1` keyMode pattern unchanged.
- Harness /tmp/t21 (kept small: v.sh/vv.sh/scan.sh = the t20 scripts with `BASE=` for the pad base, `kinds.py DUMP WINS` insn-kind
  tally, `diffuid.py A B WIN [v]` = insns present in dump A and gone in B, `wc2.py` multi-dump wincount, cbase.cpp = pass-19 base +
  FSTORE_AT, rtl_base/ = p20b's jump/cse/gcse/loop/cse2/flow dumps). /tmp/t18, /tmp/t19, /tmp/t20, ~/.cache/tesp15, the kit untouched.
- Next: (1) the 1-3 cse2-time insns of ROTATE..SUB (a real insn surviving cse1 that flow/combine delete: the `(set r r3)` copies of the
  `->SetKeta()` chains, the `keyMode = 1` / `flg = 4` constant sets in VEC0 (first after F9), the PRE-rewritten addressof copies) --
  one removed (1,1) insn plus a `1:2` (2,0) pad lets n = 37 hold both edges; (2) then the callee-saved/spill picture (seg 221, 380/391
  remat `lis g_pPrimArray`, the `li 1` pattern) and seg 0 last.
- Last probe (not applied): l37 + one `1:2:3:4` (4,0) pad in VEC1 / VEC2 / SUB / ROTATE gives the SAME grid in all four (cse1
  F10 WORK0+13 -> +9 in pad-0 terms = +13 pad-4, below the (+14, +33] pair; cse2 F7' SUB+48 -> +49) and 681/698/699 stay fresh,
  1873w: the +1 cse2 shift is N/PRE-side (the autoN refit changes which `lo_sum`s gcse deletes in the tail), not the pad's
  window, and F7' = +49 with F10 at +13 does not share 681/698 -- so either the 681 edge is >= +50/+51 or 698's pair is the
  binding one; 1-3 real cse2-time insns of ROTATE..SUB remain the missing knob.

### Tool RELs, t_esp pass 22 (t_esp 208/212: InitTool 1811 -> 1788w in the tree: cse2 F6' fitted (LIFE 37-set pad + ROTATE's shared `sx2`, lc segs 5 -> 3 = 221 623 782); the F9/F10 edges read exactly off the RTL (F10 = WORK0+14 is pinned to ONE insn, the "F7' >= 49/51" edge of pass 21 was the F10 pin + allocation, not cse2); nothing flipped; 2026-09-12)

- **Tree:** src/t_esp/t_esp.cpp = pass 21 + LIFE pad `1:2:3:5..37:4` (37 sets), ROTATE_WINDOW `int sx2;` at ctor scope with the
  RND_ROT rows 1/2 written `sx2 = 2; int sx = sx2;` / `int sx = sx2;` (row 3 keeps `int sx = 2`), dead-set block 32 -> 28 (N 5235).
  Locked ninja + bytecmp: InitTool 1788w (size 0xa20c/0xa0fc), 208/212, .rodata/.data/.bss OK. Grids: cse1 LOAD_EVENT+4 SAVE_EVENT+27
  OPTION+540 PATH+541 SIZE+174 SPEED+684 COLOR+863 LIFE+8 ROTATE+647 WORK0+14 WORK6+14 BASEPOS+123 (base coordinates: the LIFE +1 and
  the ROTATE -1 cancel), cse2 LOAD_EVENT+53 OPTION+71 PATH+349 SPEED+110 COLOR+555 ANMRATE+46 SUB+50 WORKSP1+81. lc [0 0 1 0 0 0 0 0 0 0
  1 0 0 1] segs 221 623 782 (pass 21: 5), mset [2 9 4 3 5 8 5 9 17 3 38 0 18 7] (pass 21: [.. 5 40 ..]). Flags untouched, no make_rel.
- **The pass-21 "F7' edge" does not exist.** 681/698/699 are not cse2 evidence: 698's pos.x (344.0f, .LC1613) is shared with VEC2's 681
  load by CSE1 (both inside the F9..F10 window; a cse2 rescue is impossible, F7' lies in SUB between them), and 681/699's `fmr` is the
  344.0 pseudo living r28 (GPR, `-mfast-cast`) when its range spans 681..698 -- allocation, gone whenever 698 stops sharing. Read off
  the WORK0 `.jump` stream (offsets from WORK0's `__builtin_new`, pad-0 coordinates = the tree): 10 `(set this (addressof pos))`, 11-13
  the 344.0 high/lo_sum/load, 14-16 the 280.0 high/lo_sum/load, 17 store x, 18 store y. Target 698 = x via the pointer (`stfs f24,0(r8)`)
  + `addi r5,r1,0x1630` for the argument + 344.0 SHARED + 280.0 FRESH (`lfs f26,51C0` although f30 holds it since 663: no cse2 rescue
  across F7'). So F10 > 10 (x store via pointer needs the flush between the this-addressof and the x store), F10 >= 14 (344.0 load
  before the flush), F10 <= 14 (280.0 load after the flush): **F10 = WORK0+14 exactly** (pass 19's "(WORK0+14, WORK0+33]" and pass
  20's "(+9, +33]" are wrong; the tree already had +14/+18-pad-4). l37 (F10 +13) split the 344.0 load -> 698 fresh; b1 (+19) shared 280.0
  -> 698/706 differ. Consequence: every cse1 change between F8 and F10 must net to ZERO insns.
- **F9's position decides ROTATE's cse2 count by one insn (the `DB_POINT pos` addressof pair, block 11 = `pos(274, 16)`):** `.jump`
  ROTATE 640 `(set r8893 (addressof pos))` [the `&pos` argument; the ctor's `this` r8873 holds the same addressof from the pos ctor],
  641 `(set r8894 (addressof sx))`, 646 `(set r7 r8893)`, 647 `(set r8 r8894)`. cse1 rewrites 640 to `(set r8893 r8873)` and 646 to
  `(set r7 r8873)`, so r8893 dies and cse1 deletes it -- unless the flush falls in [641, 646] (before the r7 set): then the copy
  survives to cse2 (+1 .loop insn in ROTATE, no final-code change: cse2 canonicalises it). F9 = 647 (tree) or 648..654: base count;
  641..646: +1; 655..660 would leave block 11's 360.0f/-360.0f high/lo_sum/load cse1-fresh (+6); <= 640 puts the argument in the
  `addi rX,r1,pos` form (final code). The sx addressof (SI) never makes a copy pair (its store goes direct to the frame). With F10
  pinned, F9 = 647 is pinned too, so the LIFE knob's +1 cse1 insn had to be paid back by exactly -1 cse1-only insn between F8 and F9.
- **cse1-only levers found (0 cse2, same post-cse1 RTL):** (a) a duplicate FP literal within one cse1 window is 3 cse1-only insns
  (high, lo_sum, load; cse1 folds the address and shares the loaded pseudo): `f32 rmax, rmin;` at ctor scope, `rmax = 360.0f; n->max
  = rmax;` in row 1 and `n->max = rmax;` in a later row = -3 per constant per row (/tmp/t22/b1.cpp: ROT_MINMAX of row 2 = -6 cse1,
  F9 647 -> 652 in base coordinates, F10 -> +19 = the 698/706 symptom above); (b) a duplicate int literal `int sx = 2` is 1 cse1-only
  insn (`(set r 2)` deleted at cse1's end): `int sx2; ... sx2 = 2; int sx = sx2; ... int sx = sx2;` = -1 (applied: c1). `int sx = 1`
  rows (c2, the head is keyMode's pseudo) give the same -1/1788w; the `sx = 3` rows (c3) are +1 in .loop (block 11's set is the head
  of const 3 for block 12 after F9) -- keep the shared variable in rows BEFORE F9 whose constant has an earlier head.
- **Scan results (/tmp/t22, `ct.sh NAME [noN]` = autoN + per-window .jump/.loop deltas vs base + both grids + fl/lc, `scan.sh L:PADS|L:-`
  parallel, `pad.py OUT BASE WIN=a:b:5-37:4` pads on the tree form, `setn.py FILE N` dead-set count):** l37 (LIFE 37 alone) 1870w F6' 46
  F7' 48 F10 13 [lc 221 623 681 698 782]; a2..a5 (l37 + RELEASE `1:2`..`1:2:3:5:6`) 1838-1874w F9 644..641 (all +1 copy) F10 11..8;
  b1/b2/b8 1892/1927/1977w; c1/c2 1788w; c3 1788w with F7' 49. The dead-set count (24/32/36) does not change the .loop tail streams
  (only N), so the "autoN shifts F7' by +-1" of pass 21 was the LIFE pad's own +1 through the F9 copy, not the refit.
- **Remaining lc: 221** (LOAD_EVENT `" Name :"` high in callee-saved r18 vs the target's fresh `lis r8`; allocation, unchanged since
  pass 21), **623** (ROTATE CreateString: target `fmr` copy = weak), **782** (BASEPOS: target loads a constant fresh that ours shares =
  F12 (BASEPOS+123) or F8' (WORKSP1+81) reading, not started). mset regions [2 9 4 3 5 8 5 9 17 3 38 0 18 7]: the allocation items
  (`lis g_pEditSeq/g_pEditSeq2` remat in the target vs r18/r15 in ours at every CreateNumeric2 of ROTATE/VEC (segs 635-679: T 18 vs O
  17 per block), 380/391 `lis g_pPrimArray`, the `li 1` keyMode pattern, seg 0 d136) are item 2/3, not touched: with the grid at 3 lc
  segments they are next.
- Not run: make_rel --verify, ninja -k 0, shasum (nothing flipped; flags untouched). Kept: /tmp/t22 (base.cpp = pass-21 tree, c1.cpp =
  the applied variant, ct.sh/scan.sh/pad.py/setn.py, rtl_base/rtlG_base dumps), /tmp/t18..t21, ~/.cache/tesp15, the kit.
- **THE SHAPE (applied to both units): the pixel words are THREE rotating pairs, `Uint32 a0, b0, a1, b1, a2, b2` with pixel k in pair
  k % 3, each pair loaded right before the sum that needs it (`a1 = s0[1]; b1 = s1[1]; p0 = (Uint32)a0 + (Uint32)a1 + (Uint32)b0 +
  (Uint32)b1 + 2; a2 = s0[2]; b2 = s1[2]; p1 = (Uint32)a1 + (Uint32)a2 + ...; a0 = s0[3]; ...`), the sums `p0..p7` reused for the second
  half (the 9th sum is `p0 = ...` again, computed AFTER `d[1]` like a copy-pasted second half; `d[16] = AVG4(p0, p1, p2, p3)`), the
  `(Uint32)` casts kept, no masks.** Why it is the vendor's: (1) the first webs of the three pairs (pixels 0, 1, 2) are own locals and every
  later web (pixels 3..16) a range-split frontend temporary, the sums' first webs own locals and their second webs @temps -- with the
  pixels declared before the sums the scan-1 survivors are exactly the target's L2 {a6, a7, a2, a8, p2, p3, p4} (+ p8 = p0's second web),
  the block-1 volatile colours are 29/30 and 45/49 in all (w3i probe: 30w); (2) the second half's sums stay variables except p6' (q/p in
  one register for p9, p10, p11, p12, p13, p15 and a separate temp for p14 = exactly the target's block-2 web structure, which no other
  naming gave); (3) mpv_mc 8x8 4p (no second half) is BYTE-IDENTICAL with the same three pairs, the prefetch after the first pair
  (`a0 = s0[0]; b0 = s1[0]; __dcbt(s1, stride);` -- the 8x8 target has two lbz before the dcbt, the 16x16 target none) and the loop
  variables declared BEFORE the pixel words and the sums (declared after them: 52w with d/stride/s0/s1 in r9/r6/r7/r8; the 16x16 form
  does not care: 30w either way). The pass-11 question "why `add (a1+b0)` before `lbz a2`" was the raw order of the loads (pair k+1
  right before p_k) plus the dcbt position; the pass-10 rule "pixels before the loop variables" holds only for the 16x16 kernel.
  Negative (16x16, all worse than 30w): 2 or 4 rotating pairs (119/114), the 9th sum before `d[0]` (`p8`, 77w) or between the stores
  (73w), `Uint8`/`Uint16` pairs (106/136w), the sums declared before the pairs (77w), masks at the loads (136w: the webs become the load
  temps again), the sliding window with forward copies `a0 = a1; b0 = b1;` (122w: the frontend folds the copies), no casts (115w),
  `Uint16` sums (178w: the truncations stay), the pack macro with `>> 2 & 0xFF << n` / `& 0x3FC << n` / mask variables (136w: the
  chain's rlwinm base changes), `(Uint16)`/`& 0xFF`/`(Uint32)(Uint8)` at every use (113w: the extra deleted instructions land before
  `d[0]`).
- **The 30-word residue = the first block's cut.** The codegen starts a new block at the first statement end where the block's INITIAL
  instruction count exceeds 100 (tree/m1: d[1] ends at 105 after d[0]'s 97 -> cut after d[1]; d1 probe: cut at 101 = d[1]'s stw; w3i: cut
  at 101 after `b2 = s1[11]`, the boundary at 100 (`a2 = s0[11]`) not taken). The target's block 1 ends after the 9th sum (its 67 final
  instructions = dcbt + 20 lbz + 36 add/addi + 8 rlwinm/rlwimi + 2 stw), so the vendor's count was <= 100 at d[1]'s end and >= 101 at
  the 9th sum's end; ours is 83 and 91 (dcbt 1 + 18 lbz + 16 cast copies `mr @t, a` of the two-use pixels + 8 x 4 adds + 2 x 8 pack, then
  2 lbz + 2 mr + 4 adds): the original had 10..17 more later-deleted initial instructions by the 9th sum with at most 17 by d[1] -- e.g. 2
  per sum (16/18), or 5..8 per store, or 1 per sum + 1..4 per store; NOT 1 per load (18/20: the cut then falls after d[1], as it does for
  masks / `(Uint8)` at the load / per-use narrowing casts, all +18..+26). Every construct tried either adds nothing (the frontend folds
  copies, `p += 2`, `2 + ...`, casts on the store arguments, mask variables, `*(s0 + k)`, `(Uint32)s0[k]`, integer-cast pointers) or too
  much before d[0]. Candidates not yet tried: a helper/macro that yields exactly one folded instruction per sum (an inlined `avg4()`
  with 4 parameters gives 4 argument copies = too many), a `Uint8` view of the sums inside the pack that the peephole folds, an
  `addi`-generating address form for the stores only. When found, check the second half too: its cut (block 2 = 69 final instructions)
  follows from the same count.
- **Tree state:** src/lib/mpv_mc.c `MPVMC08_OneRef4p_TuneC` = the three-pair form, byte-identical (mpv_mc 3/5: 4p, 1p asm + one; V2 73w,
  H2 436w untouched); src/lib/mpv_mcy.c `MPVMC16_OneRef4p_TuneC` = the three-pair form with the reused sums, 30w (mpv_mcy 2/5; H2/V2
  225w untouched -- the average kernels have no pixel sums, the transfer is the loop-variable/word-class reading, not tried this pass).
  Nothing flipped, objects.py untouched by this pass, no `ninja -k 0` needed. Pass-11 harness deleted; the pass-12 harness
  ~/.cache/cri_swar12/ is KEPT for the next pass (bodies/w3i.c = the applied 16x16 form, bodies/e3o6.c = the 8x8 form,
  `ev.sh NAME` = words + block sizes + block-1 colour score + L2 set + order scores in one line, `gen3.py NAME npairs= pix= cast=
  pdecl= sums=reuse|p8|mid avg= mask=`); delete it when the cut is found.
- **Item 2 (callee-saved/spill rotation), read only, 20-minute box:** callee-saved reloads `lwz r14-r17,slot(r1)` exist only in seg 0
  (16 in both); in the tail r14-r18 are REMAT homes handed out by reload's spill-register rotation. First-window picks (seg 163/164 =
  LOAD's SetCloseCallback/CreateString): target `lis r14 LoadNowClose_callback`, `lis r18 g_pPrimArray`, `lis r17 g_pEditSeq`, `lis r15
  g_pEditSeq2`, the keyMode `1` remat in a volatile register (every later `stw` of 1 is a fresh `li`); ours `lis r15 LoadNowClose`, `lis
  r17`, `lis r16`, **`li r14,1`** (reload_cse then serves 10 later stores of 1 from r14: `li 1` T 9/13 vs O 4/8 in regions 381-416/416-495),
  then 218 `lis r15 g_pEditSeq2` (target: r14, re-used at 218 = the r14 that 461-651 inherits; `hl.py` timeline T 350 r15 / 378 r22 / 461 r14
  / 656-688 fresh r11,r9 / 702-751 r14 / 757+ r9 vs O 350 r16 / 378 r22 / 461-775 r15). The tail sequences are the same list of picks
  shifted by ONE rotation step (ours one ahead at seg 163), so the shift is made before the tail: seg 0's reload count / spill set (d136
  there) -- item 3, not a tail source lever. The seg 380/391 `lis g_pPrimArray` remats and seg 221's r18 high are the same shift.

### Tool RELs, t_esp pass 23 (t_esp 208/212: InitTool 1788 -> 1771 -> 1570w in the tree, seg 0 EXACT (d136 -> 0) and the pass-14 `k8/k6` spill-set asm REMOVED: the seg-0 "rotation phase" was two DB_POINT `this` copies surviving cse2 (cse2 flushes F2'/F6' inside a copy -> pos.y-store interval), read off RLDDBG + the entry addi classes; cse1 F1 moved LOAD_EVENT+4 -> +6 (the g_pPrimArray high of SAVE..PARENT = one r18 pseudo + reload inheritance); the tail's r14-r18 "remat homes" are local-alloc block-13 picks, not reload; nothing flipped; 2026-09-12)

- **Tree:** src/t_esp/t_esp.cpp = pass 22 + (a) SAVE_EVENT in-block pad `{ int d_; d_ = 1; d_ = 4; }` after `f32 h` (after
  cse1 F2 = SAVE_EVENT+27, so `d_ = 4` heads const 4 and the pad is (2 cse1, 1 cse2)), (b) SAVE_CHECK pad 4 -> 2 sets (-2 cse1),
  (c) LIFE pad 37 -> 39 sets, (d) ROTATE `int sx1;` shared by the ACCELE rows 4-6 (`sx1 = 1; int sx = sx1;` / `int sx = sx1;`
  x2 = -2 cse1-only), (e) `u32 k8/k6` + the two end-of-function asms deleted; dead-set block 28 (N 5233). Locked ninja + bytecmp:
  InitTool 1771w (size 0xa20c/0xa0e8), 208/212. Grids: cse1 unchanged (LOAD_EVENT+4 SAVE_EVENT+27 OPTION+540 PATH+541 SIZE+174
  SPEED+684 COLOR+863 LIFE+8 ROTATE+644(=647 in pass-22 coordinates: 3 ROTATE insns removed before it) WORK0+14 WORK6+14
  BASEPOS+123), cse2 LOAD_EVENT+53 OPTION+70 PATH+348 SPEED+109 COLOR+554 ANMRATE+43 SUB+47 WORKSP1+78. lc segs 221 782 (623
  gone), mset [2 9 4 3 4 8 5 9 15 3 38 0 18 5], seg summary 289 differing segments / total_d 2487 (pass 22: 298 / 2664).
- **Seg 0 d136 read exactly (RLDDBG + `/tmp/t23/rldmap.py GREG RLD` = every reload pick with its post-reload insn):** the 892
  entry `addi rX,r1,C; stw rX,S(r1)` pairs are output reloads of the PRE'd `&pos` pseudos; `allocate_reload_reg` walks
  spill_regs {r0,r6,r8,r9,r10,r11,f0,LR} (n_spills 8; f0 and LR ARE in the set) round robin from `last_spill_reg` (persists
  across insns; every CALL_INSN takes an LR reload = the robin restarts at r0 after each call, so the phase never carries across a
  `bl`). r6/r9/r11 hold live pseudos in the entry, so BASE-class picks alternate r8/r10 and only a GENERAL-class pick can take
  r0. Ours differed from the target at exactly two pseudos (C = 0x9a0 = OPTION "FOG   :" CreateString, C = 0x1360 = ANMRATE
  CreateNumeric2): ours picked r0 (class GENERAL), the target r8 (class BASE), and each skip inverts the r8/r10 alternation until
  the next GENERAL pick (33 addis, d136). A `&pos` pseudo is GENERAL-class iff its only uses are copies: in ours those two had
  `(set (reg/v this) P)` copies SURVIVING cse2 (lreg: `used 2 times`, `pref GENERAL_REGS`), every other one has the pos.y store
  `(mem (plus P 4))` directly (3 uses, `pref BASE_REGS`).
- **Why the `this` copies survive: cse2 flushes, not cse1.** cse1 never folds `(mem (plus this 4))` -> `(plus (addressof) 4)`
  (find_best_addr's validate_change fails: not a legitimate address) and never deletes the `this` copy; gcse PRE turns the
  addressof into `(set this P)`; **cse2's canon_reg** replaces `this` by P in the pos.y store and the argument copy, and then
  the copy is dead. A cse2 flush between `(set this P)` and the pos.y store leaves `this` with a base use = a real pseudo
  (`lwz r9,slot; mr r6,r9; stfs 4(r9)` in ours vs `lwz r6,slot; stfs 4(r6)` in the target at seg 303; the seg-619 target shape
  `lwz r7,slot; mr r9,r7; stfs 4(r9)` is NO copy: `(set r7 P)` first, then the store's BASE reload gets `reload_override_in` = r7
  from find_equiv_reg and the rotation's r9). A flush between the pos.y store and the argument copy is invisible (the copy is
  tied to the argument register). Rule: **every cse2 flush must avoid (this-copy, pos.y store] of every DB_POINT block** (2 of
  ~11 cse2 insns per CreateString block, 4 of ~20 per CreateNumeric2 block); the seg-0 rotation is the oracle for the two
  visible ones, `/tmp/t23/surv.py DUMP.cse2` lists the survivors (`*` = base use kept = visible).
- **Fits:** F6' ANMRATE+46 -> +44 (allowed [36,44] with 619's 80.0f still fresh): LIFE 39 sets (+2 cse2 after F8; F7' SUB+48,
  F8' WORKSP1+79 move too, both harmless: F7' stays outside SUB's interval, F8' still inside WORKSP1's pos.y-store -> argument
  interval = invisible), cse1 +2 paid back by `sx1` (F9/F10 exact). LIFE 41 + `sx0` sharing (F6' +42) is 1775w, worse. F2'
  OPTION+71 -> +70 (allowed [62,70] or [73,81]): no pad between F2 and F2' has cse2 weight (LOAD_CHECK's `1:2:3:5:4` = (5,0):
  SAVE_EVENT's own `flg = 4` after F2 heads const 4, so the (n, n-1) rule needs the pad AFTER F2 in SAVE_EVENT itself, before
  its `flg`), hence the in-block pad `1:4` = (2,1) + SAVE_CHECK `1:2`. seg 0 d136 -> d58 (F6') -> d2 (F2') -> exact.
- **`k8/k6` removed (item 2):** with the grids fitted the plain source already has spill_regs {r0,r6,r8,r9,r10,r11,f0,LR}
  (`Spilling reg 6` 14 / `reg 8` 6 lines in .greg, 284 r6 / 533 r8 picks in RLDDBG), i.e. the pass-14 forcing is unnecessary
  now (it was needed at 6627w when the tail's pressure differed). Removing it: segs 172/193/203/213/331 (LOAD/OPTION
  CreateNumeric `li r8,1` position) and 802 (epilogue) closer, 516/517 one insn shorter; words 1734 -> 1771 is difflib
  alignment across the 4-insn size change (seg metric 2527 -> 2487), applied.
- **Item 3 (slot permutation) needs nothing:** the C -> S slot map was already identical in both (only the registers of 33
  addis differed); N 5233 after the removal, dead-set block unchanged.
- **The tail's `lis r14 LoadNowClose` / `li r14,1` / `lis r14 g_pEditSeq2` names are LOCAL-ALLOC picks (block 13, the whole
  tail is one basic block), not reload picks and not global.c:** RLDDBG shows no r14-r18 reload in InitTool, GDBG has no GORDER
  line for them, LADBG `b13` lists them (`q144 reg5406 refs 14 ... -> 14`); `.greg` "Register dispositions" lists 32 tail pseudos
  in r14-r18. Pass 20/22's "spill-register rotation / remat home" reading of them is withdrawn; the target leaves the keyMode `1`
  pseudo UNALLOCATED (fresh `li` per store) and gives r14 to `LoadNowClose_callback`'s high (order below).
- Harness /tmp/t23 (kept): base.cpp (pass-22 tree), D1.cpp (first step), E1.cpp (= the tree), ct.sh/scan.sh (t22's + surv.py + runv), pad.py (also
  rewrites `TOOL_WINDOW_CSE_PAD();` pads), mkrot.py (ROTATE sx sharing), rldmap.py, surv.py, rld_base.log, greg/lreg/cse/cse2
  extracts. /tmp/t18..t22, ~/.cache/tesp15, the kit untouched.
- **APPLIED, mpv_mcy `MPVMC16_OneRefH2_TuneC` 225 -> 197w, .text size now exact (0x470; the unit's `.text` no longer "sizes differ"):** (1) case 0:
  `a3 = s[16]; a3 = (w3 << 8) | a3;` instead of `w4 = s[16]; a3 = (w3 << 8) | w4;` (s4: 201w, size exact) — the byte-load web is a3's FIRST
  web and the or->rlwimi fusion's `mr` into a3's second web is kept, giving the target's `lbz r20; mr r8,r20; rlwimi r8,w3,8,0,23` and, with
  it, the kept `srwi; mr; rlwimi` copies of a0/a1/a2 (the tree had 2 of 4; the `a_k = w_k << 8; a_k |= ..` split spelling (m16a) changes
  nothing: the frontend forwards the single-use first def; `a_k = w >> 24; a_k |= w << 8` (s2) 206w); (2) case 0 through `MPVMC16_AVG2V(w, a,
  x, m1, m2)` with `Uint32 m1 = 0xFEFEFEFE; Uint32 m2 = 0x01010101;` function locals = the target's association `(w & a) + (sh + (x & m2))`
  (the literal-mask macro rebuilds `sh + ((w & a) + (x & m2))`); (3) case 1: the byte shift FIRST and the roles swapped — `a0 = (w0 << 8) |
  (w1 >> 24); w0 = (w0 << 16) | (w1 >> 16); x0 = a0 ^ w0; d[0] = AVG2V(a0, w0, x0, m1, m2)` (the target computes `srwi w>>24` before `srwi
  w>>16` for every word and xors byte ^ half; u1/u2 197w, `x0 = w0 ^ a0` order does not matter). In-place accumulation (s6, `w0 &= a0; a0 = x0
  & m1; ..`) is WRONG for the 16x16 (230w, size -4: the target's `and r27,w0,a0` is a fresh register). Raw-order probes (lbz after a2, loads
  w1,w2,w0,w3) change nothing (t1/t2 202w). Residue 197w = colours (target stmw r20 = 12 callee-saved, ours r21/r22) + the post-RA order that
  follows them (target `srwi w3>>24; mr a1; lbz` where ours issues the lbz first: sched.py on ours picks LBZ h=10 / MR h=12 in one cycle).
- **APPLIED, mpv_mc `MPVMC08_OneRefH2_TuneC` -> the target's structure (436 -> 494w but size 0x324 -> 0x40c vs 0x3ec; the old form was 200
  bytes short):** `for (i = 0; i < 4; i++) { ROW(0, 1) ROW(2, 3) d += 4; }` per case (two rows per iteration, ctr 4) with the row's average
  accumulated IN PLACE (`x0 = w0 ^ a0; w0 &= a0; x1 = w1 ^ a1; a0 = x0 & m1; x0 &= m2; w1 &= a1; a1 = x1 & m1; x1 &= m2; a0 >>= 1; w0 += x0; a1 >>=
  1; w0 += a0; w1 += x1; d[o0] = w0; w1 += a1; d[o1] = w1;` = the target's `and r4,r4,r5 / and r8,r8,r9` in the word registers and `and r10,r10,
  r12` in x0's), the tree's per-case a0/a1/w0/w1 expressions kept (case 1 `__rlwimi/__rlwinm`, case 3 `__lwbrx`), mask variables m1/m2 (literal
  masks re-materialise `lis/subi` per case: h8e 506w). Every row is the target's 26 opcodes; the residue is a 2-register frame (ours peaks at
  12 live: the pre-RA scheduler issues `and (x0 & m1)` in the cycle of `and w0,a0` because it "frees" its srwi, where the target's post-RA
  order has `and w0,a0; xor x1; and a0=x0&m1` — its `x0 & m1` took a0's register r5, i.e. was coloured after a0 died = emitted after `and w0,
  a0` within that cycle) -> m2 lands in r31, `stwu/stw r30/r31`, a shared `b epilogue` instead of the target's per-case `blr`/`bgelr`. Next: the
  in-cycle emission order (pass-11 "sched.py --verbose": frees > height > class > input) — find the DAG change that lets `and w0,a0` go
  first (e.g. `w0 &= a0` freeing a successor: the `x0 & m2` and issued before it), or make `x0 & m1` share a0's register by construction.
- **16x16 V2 (225w, colours only):** declaration-order probes v1-v4 (natural, reversed, target-colour order `x1, a3, w3, a2, w2, x0, x2, x3, a0,
  w0`) 221-230w; nothing applied. The target hands r29 x1, r24 a3, r23 w3, r22 a2, r21 w2, r18 a0, r17 w0 with backend temps in r25-r28 BETWEEN
  x1 and a3 — not a plain "own locals in declaration order" level; read it with chaitin.py next.
- Tree: src/lib/mpv_mcy.c (4p 30w unchanged, H2 197w, V2 225w; 2/5), src/lib/mpv_mc.c (H2 two-row form 494w/0x40c, V2 73w; 3/5); locked ninja
  of both objects OK; objects.py untouched, nothing flipped, no `ninja -k 0`. Pass-12 harness ~/.cache/cri_swar12 DELETED; the
  pass-13 harness ~/.cache/cri_swar13 (gen.py/p.sh/cnt.py/sum.sh/h8gen.py, bodies/, ra_*/out_* dumps) is kept for the next pass —
  delete it when the 4p cut and the H2 frames are closed.
- **Second tree step (same pass): InitTool 1771 -> 1570w = cse1 F1 moved LOAD_EVENT+4 -> +6** (LOAD_SST pad `1:2` -> `{ }`
  = -2 cse1 before F1, paid back by SAVE_ROOM `1..6` (+2 cse1-only) so F2 = SAVE_EVENT+27 and every later flush stay; N 5233,
  28 sets; locked ninja + bytecmp 1570w size 0xa20c/0xa0d4, 208/212; lc segs 623 782; mset [2 8 0 0 0 0 0 7 15 3 38 0 18 5]
  (LOAD_EVENT/SAVE_EVENT/OPTION/PATH/SIZE regions now 0); seg summary 264 / total_d 2124; seg 0 still exact). Why: the pass-18
  F1 = +4 sat between the LOAD_EVENT ctor argument's `(set H (high g_pPrimArray))` (+3) and its `lo_sum` (+4), making that H the
  F1..F2 cse1 head with its set BEFORE cse2's F1' (LOAD_EVENT+53); cse2 then could not merge LOAD_CHECK/SAVE_CHECK/OPTION's highs
  (their sets are after F1') into it, so ours had four g_pPrimArray highs for segs 230-446 (refs 6/4/5/5, local-alloc pri 53/30/
  27/17, none allocated -> `lis r6` remat per `new`). With F1 after the lo_sum the F1..F2 head is SAVE's high (set at seg 230,
  inside cse2's F1'..F2' window), cse2 folds LOAD_CHECK/SAVE_CHECK/OPTION's into it (9 refs -> the first block-13 local-alloc
  pick -> r18, its `lis` sched1-hoisted to seg 163 exactly like the target's `lis r18`), and the DATASET..PARENT highs (segs
  340-446, unallocated) are served by **reload's find_equiv_reg inheritance** (`lwz r0,@l(r18)` with no `lis`: r18 still holds the
  value, dead pseudo or not, until r18 is overwritten). The same inheritance explains lc 221: the LOAD_EVENT `" Name :"` high
  (post-F1 head, unallocated) inherited r18 from the pre-F1 `" Name :"` high in ours, while the target's r16 (its `" Name :"`)
  had been reused at seg 210 by a single-use g_pEditSeq high -> fresh `lis r8`. Rule for the tail: **an unallocated REG_EQUIV
  high/constant is `lis`-free wherever the hard register of an EARLIER pseudo with the same value has not been overwritten;
  read the callee-saved names as local-alloc's block-13 pick ORDER (LADBG `b13 ... -> rN`, free regs handed out r18, r17, r16,
  r15, r14 in priority order), not as reload picks.**
- **Block-13 local-alloc order (LADBG on the tree before the F1 step; the remaining LOAD-region item):** ours 2555 `" Name :"`
  high (refs 5 len 1186 pri 84) r18, 4919 g_pEditSeq (11/4204, 78) r17, 4923 g_pEditSeq2 (11/4200, 78) r16, 2807 LoadNowClose
  (5/1312, 76) r15, 5406 keyMode `1` (14/5658, 74) r14. Target (read off the listing): r18 SAVE's g_pPrimArray high, r17
  g_pEditSeq, r16 `" Name :"`, r15 g_pEditSeq2, r14 LoadNowClose, keyMode `1` UNALLOCATED (fresh `li` per store = the `li 1`
  pattern of passes 20/22), i.e. the order PA > g_pEditSeq > `" Name :"` > g_pEditSeq2 > LoadNowClose > `1`: `" Name :"` must
  rank between the two g_pEditSeq highs (ours ties them at 78), e.g. a ~50-insn longer `" Name :"` life or one more insn between
  the g_pEditSeq2 high's sched1 slot and its death. Re-read LADBG after the F1 step before touching it. lc 623 (ROTATE `fmr`) is
  this allocation class too; 782 (BASEPOS) not started.

### Tool RELs, t_esp pass 24 (t_esp 208/212: InitTool 1570 -> 1415 -> 1409w in the tree, all pure C: (1) the CreateNumeric2 rows compute their two edit pointers BEFORE the `DB_POINT pos` (54 rows, ID_WINDOW excepted) -- read off seg 378, where the target shares g_pEditSeq's high but loads 284.0/32.0 and g_pEditSeq2's high fresh; (2) `g_pLoadDirButton` is `DB_STRING*` (a NOP_EXPR RHS makes expand_assignment legitimise the LHS address before the CreateButton call -> the high crosses the call -> sched1-hoistable -> the target's callee-saved r24; segs 163/184 exact); (3) cse1 F12 BASEPOS+123 -> +107 (WORKSP2 pad +20, seg 782's 8.0 fresh); LOAD..PATH mset 0, lc 782 gone, seg 0 exact; the block-13 A/Name order read to a 84/84 priority tie; nothing flipped; 2026-09-12)

- **Tree:** src/t_esp/t_esp.cpp = pass 23 + (a) every non-ID `pa_->CreateNumeric2(win_, &A, &B, &pos, &sx, ..)` row written
  `T* n1 = &A; T* n2 = &B; DB_POINT pos(..); int sx = ..; pa_->CreateNumeric2(win_, n1, n2, &pos, &sx, ..);` (54 rows incl. the
  VEC/WORK macro classes and the `->SetKeta()`/`->SetUpdateCallback()` chained rows; the 10 ID_WINDOW rows keep the in-call form),
  (b) `static DB_STRING* g_pLoadDirButton;` (was DB_BUTTON*; g_pSaveDirButton left DB_BUTTON*, see below), (c) PATH pad `1..8` ->
  `1:2:3:4` and POS `{ }` -> `1:2:3:5` (F4 +4 cse1 insns later = after row 378's n1; every other flush unchanged), (d) WORKSP2 pad
  `1:2:3:4` -> `1:2:3:4:5..20` (+20 cse1-only, F12 -16). Locked ninja + bytecmp: 1409w, size 0xa20c/0xa0e0, 208/212, N 5233 (28 sets,
  autoN: no refit at either step). Grids: cse1 LOAD_EVENT+6 SAVE_EVENT+27 OPTION+540 PATH+541 (own coords; 545 in pass-23 coords)
  SIZE+174 SPEED+684 COLOR+863 LIFE+8 ROTATE+644 WORK0+14 WORK6+14 BASEPOS+107; cse2 unchanged (LOAD_EVENT+53 OPTION+70 PATH+348
  SPEED+109 COLOR+554 ANMRATE+43 SUB+47 WORKSP1+78). mset [2 0 0 0 0 0 0 7 15 3 38 0 18 2] (pass 23 [2 8 0 0 0 0 0 7 15 3 38 0 18 5]),
  lc segs 623 only (782 fixed), seg summary 290 / total_d 1925 (pass 23: 264 / 2124; the segment count rose because the LOAD/PATH
  register names r16/r17, r21/r22 now show in ~25 segments that were d0 with the old, wrong allocation). Steps: 1570 -> 1564 (row 377
  n1 only, probe) -> 1617 (row 378 n1 + F4 +4: A gets its 12th ref, B/B2 permute) -> 1604 (g_pLoadDirButton DB_STRING*: segs 163/184
  exact) -> 1415 (all 54 rows) -> 1409 (F12).
- **Seg 378 (PATH row `x128[3]`, `pos(284.0f, 32.0f)`) is the whole item-1 reading.** Target: `lis r9; lfs f0` 284.0 FRESH, `lis r11;
  lfs f27` 32.0 fresh, `lwz r5, g_pEditSeq@l(r17)` = A (the first block-13 head, 350-377 in both) SHARED, `lwz r6, g_pEditSeq2@l(r22)`
  = B2 fresh. With the row order [pos consts][sx][g_pEditSeq high][g_pEditSeq2 high] this is unreachable: one cse1 flush before the
  consts makes both highs fresh, one between the highs shares the consts too; cse2 cannot help (F3' must lie in [PATH+314, +356] so
  that 378's 284.0 is not rescued from 377's load at +313, and A's set (seg 350) is before F3' in any case). So the row computes
  `&g_pEditSeq->x128[3]` first: [n1][n2][pos][sx] with F4 in (n1, n2] gives exactly T's four states. Row 377 alone (`P1`) changed
  nothing (F4 at +541 is after row 377); row 378 alone with F4 at +541 (`P3`) changed nothing either (F4 falls before the n1 that now
  sits at +541..544) -> F4 had to move to +545 (PATH `1..8` -> `1..4`, POS `{ }` -> `1:2:3:5` (4,0) pays it back before F5).
  Applying the order to ALL rows (`/tmp/t24/n12t.py`, typed locals; `__typeof__` probe `n12.py` gave the byte-identical object) was
  the big win (1604 -> 1415, regions 350-399 122 -> 36 and 700-749 156 -> 32 with the SAME grids: `d.jump: PATH-4 POS+4`, `.loop`
  PATH-1 PARENT+1 only). Chained rows (`->SetKeta`) reverted = 1462 (worse), so they are pointers-first too. ID_WINDOW's rows are
  NOT: its ctor is out of line (`stw r3, g_pIdWin@l` at seg 353 = a call result) and the standalone `__Q215t_esp_namespace9ID_WINDOW..`
  has the target's [pos stores][g_pEditSeq load] order (it went 0 -> 14w with the n1 form; reverted, 0 again). Reading: the vendor's
  plain rows went through an inline helper whose pointer parameters are copied before its `DB_POINT pos` body; ID_WINDOW (`n = ...`
  rows with nameTbl) was written out by hand. Not tried: the real inline helper (its f32/int parameter copies would change every cse1
  count and the whole grid); the typed-locals form is count-neutral.
- **Item 2, `g_pLoadDirButton`: seg 183 target `stw r3, g_pLoadDirButton@l(r24)` with `lis r24` at the TOP of seg 163 (hoisted 20
  segments), ours `lis r9; stw` fresh.** expr.c expand_assignment: when the RHS is a bare CALL_EXPR the call is expanded FIRST and
  `to_rtx` (the LHS `(mem sym)`, legitimised into high/lo_sum by expand_expr's VAR_DECL `change_address`) only after it -> the high
  pseudo is set after the call, REG_N_CALLS_CROSSED 0 -> haifa `sched_analyze_1` gives it REG_DEP_ANTI on `last_function_call` ("don't
  let it cross a call after scheduling if it doesn't already cross one") -> never hoisted -> local scratch `lis r9`. Any non-CALL_EXPR
  RHS (the `new` form of `g_pXxxWin = new XXX_WINDOW`, a NOP_EXPR from a pointer conversion) goes through store_expr with `to_rtx`
  legitimised BEFORE the RHS -> the high is set before the call(s) -> hoistable filler -> callee-saved. `static DB_STRING*
  g_pLoadDirButton` (DB_BUTTON -> DB_STRING base conversion = NOP_EXPR; SetString/SetColor are DB_STRING methods, SetDirName unchanged)
  reproduces seg 163 exactly and seg 184's `lis r23 g_pEditSeq` (LADBG: q20 reg2756 g_pLoadDirButton-high -> r24, q21 g_pLoadWin-high
  -> r23, then B2 (17 refs) -> r23 after it dies). **g_pSaveDirButton is the same** (target seg 237 `lis r6 ..DirButton; lis r8 g_pSaveWin`
  = reload's rotation for a hoisted-then-spilled pseudo; DB_STRING* reproduces both registers, `Q1`/`N6`) but its extra filler
  (first use 237, priority above the WORK0 head 9682's) pushes 9682's sched1 slot from block insn 718 to 758 = past the LOAD_EVENT
  `" Name :"` use at seg 221, so 221 inherits Name's r17 (lc 221 back) and seg 210 loses its `lis` -- 1418w vs 1415w: NOT applied,
  the tree keeps DB_BUTTON* for g_pSaveDirButton. Rule: the target has one fewer filler above 9682 than N6 (or one more slot).
  Array/struct forms for the two buttons (`g_pDirButton[2]`) break SetDirCallback and seg 163 -- rejected.
- **Item 3, F12 (BASEPOS, `/tmp/t19/win.py DUMP BASEPOS` + the .LC values):** rows 779-782 are (8.0, 0/16/32/48), 783 (95.0, 0.0);
  target seg 782 loads 8.0 (LC1564) fresh, ours shared f25: T's F12 lies between row 781's 8.0 use (+95) and row 782's 8.0 high
  (+112); ours +123. WORKSP2 `1:2:3:4:5..20` (not first-after-flush = (20,0), cse2 untouched) -> BASEPOS+107; +16/+20/+24 all give
  1409w / 782 d6, +28 (F12 <= +95) 1419w. Residue at 782: `lfs f0` vs `lis r11` order and the BASEPOS `this` name r29/r30.
- **Block-13 local-alloc order, read (LADBG on the tree, `/tmp/t24/la_tree1.log`):** q135 reg5364 (19 refs, cls 2) 134 -> r24, B2
  (W2 g_pEditSeq2 head, 17 refs 656-6274) 121 -> r23, q213 const-0 (25 refs, from seg 187) 116 -> r22, B (W2 g_pEditSeq head, 16 refs
  708-6270) 115 -> r21, q54 reg3270 98 -> r20, PA (9 refs 256-3048) 96 -> r18, **Name (5 refs 22-1208) 84 -> r17, A (12 refs 636-4880)
  84 -> r16**, A2 (11, 640-4836) 78 -> r15, LoadNowClose (5, 84-1392) 76 -> r14, then 9682 (WORK0 g_pEditSeq head, 21 refs, 1436-)
  72 -> r17 and 9686 -> r14 after Name/LoadNowClose die, label high q6 (3 refs 20-692) 44 -> r22, g_pLoadDirButton high 39 -> r24,
  g_pLoadWin high 38 -> r23. Target: r18 PA, r17 A, r16 Name, r15 A2, r14 LoadNowClose, r23 B, r22 B2, r21 label, r24
  g_pLoadDirButton (then a const 0), r23 g_pLoadWin. So two inversions remain: A vs Name (ours a 84/84 TIE broken by qty number,
  q7 < q128) and B vs B2 (115 vs 121). Arithmetic: A = floor(log2 12)*12*10000/(death-birth) = 360000/4244; Name = 100000/1186. k
  extra sched1-stream insns between Name's set (block insn 11) and A's set (318) give Name 100000/(1186+2k) and A 360000/(4244-2k):
  k = 3 flips it (Name 83, A 84); so does A's set 5 stream insns later or Name's last use (seg 210) 10 insns later. The fillers
  (dep-free `lis`/`lfs`/`li` heads) are issued by sched1 in priority then LUID order into the free slots of the call chain (`-dS
  -fsched-verbose-2`: A = uid 9228 at t=159 right after the g_pDataSetWin high, then A2, ..., B2 at t=164, five PARENT fillers, B
  at t=177); the FINAL `lis` positions are sched2's WAR-limited hoists (T's `lis r23 B` at 184 = right after g_pLoadWin's last r23
  use; ours identical now), so the final code does not show the sched1 slots -- read them from LADBG birth or the .sched dump. Which
  three fillers T has above A (or which slot structure) is not found; the visible hoisted set of segs 162-215 is identical in T and
  ours. B vs B2 needs B's length < 16/17 of B2's (B's set >= 136 stream insns later, or B2's earlier).
- **Alias status of the dir-button stores (segs 183/237 d6, open):** target order `stw r3,@l(r24); li r3,8; li r24,0; lwz r9,4(r30);
  stw r31,0x58(r9)`, ours issues the DEACTIVATE `lwz r9,4(r30)` first: in the target the `e->win` load waits for the button store
  (the store is not `MEM_SCALAR_P`-disjoint from the IN_STRUCT load), i.e. the vendor's store was not a plain scalar global (a struct
  member?). The array form was rejected (above); a struct form was not tried.
- Harness /tmp/t24 (kept): base.cpp (pass-23 tree), tree1.cpp (1415w), tree2.cpp (= the tree, 1409w), P1..P4/Q1/Q2/N1..N7/W16..W28
  variants, ct.sh/scan.sh/pad.py/surv.py/rldmap.py (t23's on /tmp/t24 paths), highs.py (InitTool's `(set r (high sym))` insns in
  stream order with use counts), n12.py/n12t.py (row rewriter, probe/typed), unn.py (row reverter: chained | line ranges), la_*.log
  (LADBG), gdbg_Q2.log, rtl_base/rtl_Q2/rtl_tree1 (-dj -dL -dl -dS sched dumps; `sched_*.txt` = InitTool's sched1 issue log). Not
  run: make_rel --verify, ninja -k 0, shasum (nothing flipped). /tmp/t23, ~/.cache/tesp15, the kit untouched.
- Next: (1) the A/Name tie: find the 3 stream insns of segs 162-210 (a hoistable high in T that ours pins after a call -- audit every
  `g_x = f()` store and every `->` store in MODEL..LOAD_SST with the CALL_EXPR rule above; the g_pSaveDirButton typing must come with
  it, see the 9682 slot); (2) B vs B2 (same family); (3) 489-688 in T are fresh `lis r6` per row for g_pEditSeq while r16 holds the
  value -- find_equiv_reg is unbounded (reload.c 6216), so something between 484 and 489 must set r16 in T's pre-sched2 order or the
  461-484 uses are the W10 head's inheritance and the W5..W9 heads are excluded for another reason; (4) lc 623, the 38-bucket.

### Tool RELs, t_esp pass 25 (t_esp 208/212: InitTool 1409 -> 1397w in the tree, pure C: the two dir buttons are one-member structs stored through a pointer (`DirButtonSlot* slot = &g_pX; slot->p = CreateButton(..)`) = hoisted high + MEM_IN_STRUCT_P store -> segs 183/237 EXACT, the block-13 A/Name tie flipped to the target's (A r17, Name r16, 9682 r16), seg 210/221 exact; new residue PA/reg9001 96/96 tie (one insn), B/B2 read to "B's sched1 issue ~170 insns later in T"; nothing flipped; 2026-09-12)

- **Item 1 (segs 183/237 + A/Name): the vendor's dir-button stores were struct-member stores through a pointer.** Read: (a) T's
  seg 237 `lis r6 ..DirButton; lis r8 g_pSaveWin` = two hoisted highs (pass 24's N6 `DB_STRING*` reproduces the registers) and
  (b) T's `lwz r9,4(r30); stw r31,0x58(r9)` (`win->active = 0`) WAITS for the button store in both 183 and 237, which a fixed-address
  MEM_SCALAR_P store can never impose on a varying IN_STRUCT load (alias.c fixed_scalar_and_varying_struct_p) -> the store is
  IN_STRUCT. Forms measured on the pass-24 tree (`~/.cache/tesp15/runv.sh`): plain one-member struct `g_pX.p = f()` (S1) 1412w:
  the store is IN_STRUCT (237's load order right) but the address is legitimised only in store_field, i.e. after the call (RTL:
  `high`/`lo_sum` insns 6359/6360 after call 6357), so the high is pinned again (`lis r9`) and 163 loses `lis r24` (d16) -- yet the
  extra stall at 183/237 alone shifted every later filler ~10-20 insns earlier (A born 636 -> 598, B 708 -> 668, reg9001 1324 -> 1256)
  and flipped A/Name. `DirButtonSlot* slot = &g_pX; slot->p = f()` (S4) 1397w: the address pseudo (`lo_sum(high, sym)`) is computed
  before the call and combine folds it back into the store -> `stw r3, sym@l(r24)` hoisted AND IN_STRUCT: 163/183/237/210/221 exact,
  segs 350-377 (A) d2 -> 0, 167/188/200 (Name) -> 0, 702-751 (9682 r16) -> 0. Placement of the `slot` line inside the row block
  (first / after win_ / after pos / after sx, 16 combos) changes nothing. Load-only (S6) 1399, Save-only (S5) 1410 -> both.
  LADBG (S4): A q126 birth 604 death 4880 pri 84, Name q7 22-1216 pri 83 (base 636/1208 = 84/84 tie), A2 78 r15, LNC 75 r14, 9682 -> r16,
  9686 -> r14 = the target's r14-r18 except PA.
- **New residue from S4: PA vs reg9001 (the 25-ref `li 0`, cls 2, born 1292 now vs 1324): 1000000/(11602-1292) = 96.99 -> 96 ties PA (96)
  and loses on qty number -> PA r19, reg9001 r18 (T: PA r18, reg9001 r19); one insn later (birth >= 1294) restores 97.** Shows as
  d2 in 163/164/165/186/199/209/230/238/251/261/284/291/298/340/347/352/354/380/391 and +2 in 662-698 (`stw r19,0x58(r9)` = win->active).
  B/B2 unchanged (B2 121 r23, const0 115 r22, B 114 r21; T B r23, B2 r22, const0 r21).
- **Sched1 facts read this pass (`/tmp/t25/vis.py SCHED FUNCLINE 13 [lo hi]` = the `-fsched-verbose-9` block-13 issue table as one line
  per cycle; the rtl.sh dumps have no priority table).** Issue rate 2; block 13's chain (the call sequence + its argument/store insns)
  fills most slots and the dep-free fillers (`lis` highs, `li`, `lfs` highs) go into the free slots in priority order = by their FIRST
  consumer's call position (ties: dependents count, then LUID). Free slots are dense inside each window ctor's `new` + CreateNormalWindow
  stretch (one per cycle) and sparse (one per ~5 cycles) inside the CreateString/CreateNumeric chains, so a filler's local-alloc birth
  moves by 1 insn per extra higher-priority filler in a dense stretch and by ~10 in a sparse one (pass 24's N6 numbers). The top fillers of
  block 13: r2469/r2467/r2477 highs (c1-c3), LC713 " Name :" q4 (c4), LC715 q6 (c5), LC717 "  No  :" = Name q7 (c6, insn 11), LC719 q8,
  LC722 q10; A (uid 9228) at c151 (S4; base c159), A2 next, B2 (10068) c164, B (10244) c168, reg7595 const0 c269, reg9001 const0 c323,
  9682/9686 c355 (both slots). Name's last use (`%5=r2555+low(LC717)`, seg 210) at c302-304; LOAD_EVENT's LC717 is a fresh 2-ref pseudo
  r3256 whose reload inherits Name's r16/r17 at seg 221 iff 9682's set comes AFTER that use in sched1 order (N6's lc 221; S4 keeps 9682
  before it). reg9001's first consumer is `[r9000+0x58]=r9001` (ROTATE's `win->active = 0`, c3183) and r8413's (the next filler, issued
  one slot after it) is `[r8413+low(g_pRotateWin)]=this` at c3184: equal priority, reg9001 first on dependents (25 vs 1); swapping them
  is the +1 insn reg9001 needs, i.e. T's ROTATE tail (seg 662: T stores unit/max/min = source order 4613, ours min/unit/max) is the place
  to look, together with the ROTATE `fmr` (lc 623).
- **B vs B2 (still open; the read):** T's r23 = B (`lis r23` at 184 = sched2's WAR hoist, uses 384-443), r22 = B2 (378-443), r21 = label
  then const0 (`li r21,0` at 187). B2 cannot be denied r23 by g_pLoadWin's high (it dies at the `g_pLoadWin = new` store, c150, before
  any PATH/PARENT filler can issue: A's consumer at 350 outranks B2's at 378), and `lis r23 B` hoisted to 184 means B is ONE pseudo
  spanning 384-443 (a separate 384-388 pseudo in r23 would put the `lis` after 388). So local-alloc allocated B before B2: pri(B) >=
  122 = 640000/len -> B's set >= insn 512 (c~256, ~90 cycles after ours at c168), i.e. in T B's first consumer is far later than seg 384
  or B has 18+ refs (4*18*10000/5598 = 128; ours 16 = 15 uses + set, T shows 15 uses). Not found; the PARENT row 388 is the in-call
  `n = CreateNumeric2(win_, &g_pEditSeq->parts, ..)` form (pass 24 kept the chained/`n =` rows in-call).
- Negative/neutral this pass: N6 alone on the pass-24 tree 1412w (210 d3, 221 d10, 237 d6 — as pass 24); S1/S2/S3 (plain struct member,
  DB_BUTTON/DB_STRING typing) 1412w; S5 (Save slot only) 1410w, S6 (Load slot only) 1399w; the 16 `slot` placements 1397w each; the
  ROTATE last row's `unit/max/min` statement order (all 5 other permutations, R1-R5) 1397w each with seg 662 unchanged — the three
  `stfs` are ordered by sched2 (register deaths), not by LUID, so seg 662 / reg9001's slot need another lever.
- Tree: src/t_esp/t_esp.cpp = pass 24 + `struct DirButtonSlot { DB_BUTTON* p; }` for both dir buttons, `.p->` in SetDirCallback,
  `DirButtonSlot* slot = &g_pX; slot->p = pa_->CreateButton(..)` in the LOAD/SAVE ctors (g_pLoadDirButton's pass-24 `DB_STRING*` typing
  superseded). Locked ninja + bytecmp: InitTool 1397w (fdiff 80 `*` lines), size 0xa20c/0xa0e0, 208/212, sections identical. Nothing
  flipped: no make_rel/`ninja -k 0`/shasum. Harness /tmp/t25 (kept): base.cpp (pass-24 tree), tree1.cpp (= the tree), N6/S1-S6/T_*
  variants, la_*.log (LADBG), rtl_base/rtl_S1/rtl_tree1 (-dS -dl -dj), vis.py, dsum.py (per-segment d deltas between two tesp15 o_*
  dirs), regmap.py (callee-saved register definitions per segment), ct.sh/scan.sh/pad.py/unn.py (t24's on /tmp/t25 paths).
  ~/.cache/tesp15, the kit, /tmp/t24 untouched.
- Next: (1) reg9001 +1 insn: read the ROTATE tail (seg 662 store order, lc 623) — a chain change there swaps the reg9001/r8413 filler
  order or shifts the slot; check with LADBG that A stays >= 84 (birth <= 5 insns earlier than 604 breaks it) and Name <= 83 (death >=
  1213); (2) B/B2 as above (find what delays B's sched1 issue by ~170 insns: its first consumer, or a dependence); (3) segs 461-484 (d4)
  and 668-698 (the 9682/9686 rows: `lis r6` fresh per row in T = mechanism 2), (4) the 38-bucket.

### t_camera_data pass 5 (tcSetBesideOffset 27 -> 0w pure C: one `QfpsOfs* o` for both loops; tcDataExport 62 -> 27w size exact: shared `CameraCut* dc` (loops 2/4) + `CameraAreaInfo* da` (loops 1/5), `r, da, size` statement order, asm-emitted tcCdat base in loop 4; kit gained GFORCE/GFORCEFN/NOEQV oracles; not flipped; IN PROGRESS 2026-09-12)
Scratch /tmp/tcam/ (`ins.py DUMP FUNC [re]` one line per insn; v*.cpp variants; rtl_*/ dumps). Tree edits: src/t_camera/t_camera_data.cpp only
(tcSetBesideOffset, tcDataExport). Nothing built under the lock except the unit itself.
- **New kit oracles (`tools/research/sngdbg`, env-gated, CHANGE CODEGEN, never on by default):** `GFORCE="<pseudo>:<hardreg>[,..]"` hands the
  listed pseudo the listed hard register in find_reg (after the scan and the preference override; conflicts are NOT checked, so a forced
  register already taken by an earlier-allocated conflicting allocno garbles the code — force both allocnos, or read GORDER first);
  `GFORCEFN=<substring of current_function_name>` scopes it (pseudo numbers repeat per function). `NOEQV="<pseudo>[,..]"` makes
  update_equiv_regs ignore that pseudo's REG_EQUAL note (no REG_EQUIV, no live-length doubling). Together they answer "what if allocno X
  had taken rN" in one 0.7 s variant.sh run: `GFORCEFN=tcDataExport GFORCE=354:30,359:30 GDBG=1 CC1DIR=tools/research/sngdbg variant.sh ...`.
- **tcSetBesideOffset 27 -> 0 (pure C, applied).** GFORCE showed the whole residue is ONE allocno: `o` of loop 2 (reg 131, refs 12 len 5,
  pri 72000, allocno 0) takes r11 in ours (conf r1,r9 only: `lwz r11,0xc(rO)` is born as `o` dies) and r8 in the target; everything else
  (162 r8->r7, 199 r7->r6, 161/160 r6/r5->r5/r4, 138 r4->r12, 134 r12->r3 in pass 1, `i+1` 157 r3->r31, then the loop-1 givs 209 r31 /
  207 r3) cascades from it, and the loop-2 body's `addi` order follows from the r11 anti-dependence. The target's `o` was live across
  r9/r10/r11 temporaries: **the vendor declared ONE `QfpsOfs* o` at function scope and assigned it in both loops** (loop 1's `o` is live
  across its r9/r10/r11 loads -> conf r0,r1,r9,r10,r11 -> r8). Form: `QfpsOfs* o;` with `n, i, j`, `o = ..` in both bodies. 0 words.
- **tcDataExport 62 -> 27 (size 0x538 exact).** Three facts, each read with GFORCE before the source form was found:
  (1) buf r29 / block-0 pTc-high r30 / both `d+1` copies r30 / cd r3: the loop-2 `d+1` (reg 359, R4 L43 -> pass-1 r3) and the loop-4 `d+1`
  (reg 354, R4 L20 -> pass-0 r7) are ONE gcse-PRE pseudo when loops 2 and 4 share one `CameraCut* d`: merged R8 L63 pri 3809, conflicts
  = union (every volatile reg + r31 through fovy) -> pass 1 r30 BEFORE buf (3105) -> buf r29, then block-0 pTc-high r30, cd r3 (r3 free
  in loop 2 now). Likewise one `CameraAreaInfo*` for loops 1/5 merges their `d+1` (R8 L80, 3000 -> r4 = the target's `addi r4,r7,0x30`).
  Loops 0-3 identical after this (62 -> 48).
  (2) loop-5 preheader `mr r10,r25; mr r7,r28; subf r26,r29,r3`: LUID order = statement order `r = rec; da = area; size = fp - buf;`
  (ours had `size` before the block). 48 -> 45 with the two above; the loop-4 word below made it size +4 until (3).
  (3) loop 4 `addi r6,r6,1` in place vs ours `addi r7,r6,1 .. mr r6,r7`: the PRE'd `i+1` (reg 348, R4 L28, pri 2857) and the hoisted
  `tcCdat` lo_sum base (reg 267, R5, haifa segments 35) are a 2857/2857 TIE broken by allocno number (267 wins -> r7, 262 pTc-high -> r5,
  348 -> r6 = i's register) **only if 267's live length is not doubled**: local-alloc update_equiv_regs doubles REG_LIVE_LENGTH of a
  once-set pseudo carrying a function-invariant REG_EQUAL (`;; LL` segs 35 -> GORDER len 70, pri 1428). cse1 puts REG_EQUAL(sym) on
  every `lo_sum(high sym, sym)` in the same ebb, loop.c re-emits the movable via emit_move_insn(sym) with the note, so every C spelling
  (`&tcCdat[i]`, `tcCdat + i`, `(u8*)tcCdat + i*sizeof`, `if (tcCdat[i].enable)` first, `while` form) doubles it. `NOEQV=267` = 27 words
  size exact = the proof. No C form found that gives the base no REG_EQUAL (a `+r` launder in the body is hoisted too but its lo_sum set is
  processed first and still doubled; a 2-set base with equal values keeps the equivalence; a base variable before the `for` lands before
  the entry test). Applied tagged: `asm("lis %0,tcCdat@ha" : "=b"(hi)); asm("addi %0,%1,tcCdat@l" : "=r"(cb) : "b"(hi)); c = (TcCdat*)
  (i * sizeof(TcCdat) + (u32) cb);` inside the body (both asms are invariant and hoisted; `"=b"` keeps the high off r0; the plus must be
  written mult-first or the `lbzx/add` operands swap). What the vendor wrote there is open.
- **tcDataExport remaining 27 words = loop 5, three items, read not closed:** (a) `r` (reg 85, R19 L62 12258) r10 and `cc` (294, R16 L25
  25600) r8 in the target, ours the reverse: at cc's turn r10/r11 must be excluded (r11 is `no`'s preference through its `lbz` temp,
  r10 is `r`; both are lower-priority allocnos), i.e. the target allocated `no` and `r` before `cc` or cc's len was >= 53. (b) `r+1`
  (349, R4 L37 2162) r30 / loop-5 pTc-high (333, R8 L112 2142) r31 in the target, ours r31/r30: 333 must precede 349 (349 len >= 38 or
  333 len <= 110). GFORCE 349:30,333:31 (+348:6) = 24 words. (c) the `found` pin (`register int found asm("r5")`) is wrong in kind: the
  target's `cmpw r5,r0` for the inner loop's entry test is cse substituting `found` (the OLDER zero) for `j`, which cse never does for a
  hard register (make_regs_eqv prefers pseudos as the canonical member) -> the vendor's `found` was a pseudo that landed in r5; unpinned,
  ours cascades from j (R50 L242 10330) beating the loop-2 giv base (10326 -> j r4, everything after shifts): 4 priority points. The
  pin adds 2 refs / 12 len to j (52/254 = 10236). Statement order inside the loop-5 body (no/r->area/found/i++/cc permutations) changes
  nothing with the pin and nothing unpinned.

### Tool RELs, t_esp pass 26 (t_esp 208/212: InitTool 1397w unchanged in the tree, ROTATE row 12 stores now `max/min/unit` = seg 662's `stfs` order is the target's; the segs 486-688 "fresh `lis r6` per row" residue READ EXACTLY: reload's find_equiv_reg poisoning by ONE address reload per symbol, which is a cse1 flush landing between a row's `(set X (lo_sum P sym))` and its `(mem X)` load -> T's cse1 F6 = SPEED raccel.x row's g_pEditSeq load (+17 cse1-insns from ours), F9 = ROTATE rrotSpd.x row's g_pEditSeq2 load (+35); a probe with F9 there reproduces the addi form, the 653-688 poison AND the PA/reg9001 flip (162-446 all -2) but costs the LIFE pad (1481w); new `SCHDBG=1` hook; nothing flipped; 2026-09-12)

- **Tree:** src/t_esp/t_esp.cpp = pass 25 + ROTATE's last row (rrotSpd.z, line ~4613) written `n->max = 10.0f; n->min = -10.0f; n->unit = 0.1f;`
  (was unit/max/min). Locked ninja + bytecmp: 1397w (unchanged), size 0xa20c/0xa0e0, 208/212, sections identical. Seg 662's three `stfs`
  now come in T's order (unit 0xb8, max 0xa0, min 0xa4); the seg's d stays 12 because the r29/r30 `this`/`n` names and r18/r19 still differ.
  Why pass 25's "all 6 permutations neutral" was wrong: the sched1 order of the three stores is NOT LUID — the store that is the LAST
  source statement carries `n`'s death (INSN_REG_WEIGHT -1 vs 0 for the other two) and rank_for_schedule prefers the smaller weight, so
  the source-last store issues FIRST and the other two follow in source order; T = unit, max, min <=> source max, min, unit (= row 10's
  order in the source already). The words did not move, so pass 25 saw no change.
- **New hook `SCHDBG=1` (tools/research/sngdbg, haifa-sched.c schedule_block; README updated, patch regenerated, binary re-verified
  byte-identical with the hook on and off):** one line per issued insn, sched1 and sched2, `clk uid pri w dep luid` = the rank_for_schedule
  keys. Read with it: reg9001 (`li 0`, uid 16944) pri 787 issues at c323; the g_pRotateWin high (15882) pri 786 at c324; the win->active
  store 16945 pri 786, the g_pRotateWin store 16954 pri 785 (its only successor is the `new` call at 784: the win->active store also feeds
  the g_pRotateWin store with an output dependence of cost 1), so **reg9001 vs r8413 is a priority difference of 1, not a tie**: no
  ROTATE-tail spelling swaps them (swapping the stores' order would need the g_pRotateWin store before the `win->active` store, which is
  not T's final order either). The three `stfs` 16941/16933/16937 all pri 790 with w -1/0/0 (see above).
- **Mechanism 3 read exactly (T's fresh `lis r6, g_pEditSeq@ha` in every row 486-688 while r16 holds the high; g_pEditSeq2 r14-inherited
  486-651, fresh 653-688; both fresh 757-759; ours inherits everywhere):** reload1.c choose_reload_regs calls `find_equiv_reg (search_equiv
  = (high sym), insn, class, -1, NULL_PTR, ...)` for an unallocated REG_EQUIV high, and with `reload_reg_p == NULL_PTR` the backward scan does
  NOT skip reload-inserted insns (`INSN_UID (p) < reload_first_uid` is only required when reload_reg_p is a real array). The scan stops at
  the FIRST insn whose SET_SRC equals `(high sym)` and whose destination is a hard reg (true_regnum; unallocated pseudos are skipped) and
  then only VALIDATES that register: if it is call-used and a CALL lies between, `return 0` — it never looks further back. Hence one reload
  insn `lis r6, sym@ha` anywhere after the allocated head's `lis r16` poisons every later unallocated high of that symbol until the next
  ALLOCATED head (9682/9686's own uses 702-751 are fine, W11 757-775 is poisoned again). Ours never emits such a reload (every row inherits
  r16/r14, an inherited reload emits no insn), T does exactly twice: the module's only two `addi rX, rX, sym@l` (seg 486 `lis r6; addi r6;
  lwz r5, 0(r6)` for g_pEditSeq; seg 653 `lis r11; addi r11; lwz r6, 0(r11)` for g_pEditSeq2). That "addi form" = `(mem X)` with X an
  unallocated pseudo whose reg_equiv_constant is the bare `sym` (find_reloads_address: `strict_memory_address_p (sym)` fails -> push_reload
  of X into BASE_REG_CLASS -> `lis; addi` + `lwz 0(r)`). Expand emits every `g_pEditSeq->m` reference as `(set P (high sym)); (set X (lo_sum
  P sym)) [REG_EQUAL sym]; (set t (mem X))` (.jump dump); cse1 normally rewrites the load's address to `(mem (lo_sum P sym))` (find_best_addr)
  and X dies. **If a cse1 flush falls exactly between X's set and the load, the load keeps `(mem X)`** (X is unknown to the new table),
  X keeps its REG_EQUAL sym -> REG_EQUIV -> unallocated -> the addi form, and the poison follows. Verified with the probe P2 (below):
  moving F9 to ROTATE row 10's g_pEditSeq2 load gives `lis r11, g_pEditSeq2@ha; addi r11, r11, @l; lwz r6, 0(r11)` at 653 and fresh
  `lis r9/r11, g_pEditSeq2@ha` at 656, 659, 668-688, 765-775 (T: 656/659 r11, 668-688 r9, 757-759).
- **The two pins this gives on T's cse1 grid (Ra/tree uid coordinates, /tmp/t26/rtl_Ra):** F6 = "flush at insn 12729" = the SPEED
  raccel.x row (`n = CreateNumeric2(win_, &g_pEditSeq->raccel.x, ..)`, the in-call row after the 3 rspeed typed-local rows): X set 12728,
  load 12729; ours F6 = 12702 -> **+17 cse1-time insns** (count of .jump insns in [12702,12729)). F9 = "flush at insn 16788" = ROTATE
  rrotSpd.x row's g_pEditSeq2 load (X set 16787); ours 16743 -> **+35**. Both are one-insn windows. F6 also explains T's 486 shape
  exactly (g_pEditSeq addi, g_pEditSeq2 `@l(r14)` inherited: the flush is before g_pEditSeq2's X/load, whose head P2 then inherits r14).
  Consistency check with the 0.0 copy at lc 623: T has THREE `fmr` copies at ROTATE's head (623 0.0->f22, 624 16.0->f21, 625 32.0->f23),
  ours two (16.0, 32.0): the 0.0 copy C is row 10's pos.y pseudo (`(set C H)` made before F9, its store after F9), i.e. T's F9 lies after row
  10's `(set r8908 (mem LC 0.0))` (16769) too — consistent with 16788 (pointers are computed AFTER pos in the in-call rows). T's VEC rows use
  C (f22) for 0.0, not H (f28): not yet explained (cse2 canon should prefer H; read it when the grid is refitted).
- **Probe P2 (/tmp/t26/P2.cpp = tree + LIFE pad 39 -> 4 sets + VEC0 pad 35 sets + autoN; NOT applied): 1481w, regions [74 62 1447 501]
  vs the tree's [2 88 1344 477].** F9 lands on the pin (P2 uid 16753 = 16788 here), F10 +2. Gains: segs 162-446 every d2 of the PA/reg9001
  list goes to 0 (LADBG P2: reg9001 death 11602 -> 11598, pri 96 -> 97 -> allocated before PA -> reg9001 r19, PA r18 = T; i.e. mechanism 1
  is TWO stream insns between reg9001's birth and its last use, not the ROTATE tail), 651/656/659/662/670-672/679-680/688-689 -1..-2 (the
  poison). Losses: 545-625 +2..+12 (the LIFE pad also sets cse2's F6'/F7'/F8' — pass 21-23 — so ANMRATE/ROTATE-head constants re-shift),
  667-669/676-678/685-687 +1/+2 (reload's r9/r11 rotation for the fresh highs), 706-735 +2/+4, seg 0 d74 (spill-slot order; autoN kept N
  5235 but the pad block moved). So the F9 pin needs -35 cse1-only insns in (F8, F9] with the cse2 count kept, or a joint refit of F8..F10
  with cse2 F6'..F8'; the F6 pin needs -17 in (F5, F6] (no pad exists there: SPEED's top pad is `{ }`, SIZE's 4-set pad is before F5) and
  +17 in (F6, F7]. Pad weights (pass 18/23): a ctor-top pad of n sets = n cse1-time insns, 0 cse2 unless a flush splits it (LIFE: n-1 cse2).
- **B vs B2 not touched** (needs B's set >= insn 512 or 18+ refs; with T's F6 at 12729 the SPEED..COLOR heads change, re-read after the
  refit). The 38-segment bucket (r29/r30 `this`/`n` names in 545-662) not started.
- Harness /tmp/t26 (kept): base.cpp (pass-25 tree), Ra.cpp (= the tree), P1/P2 (probes), rtl_Ra (-dj -ds -dL -dl dumps + csedbg_Ra.log =
  the 12 cse1 + 8 cse2 flush uids), rtl_P1, sch_base/sch_Ra.log (SCHDBG), la_Ra/la_P2.log (LADBG), tseg.py (print T/O segments), s1b13_Ra.txt.
  /tmp/t25, ~/.cache/tesp15, the kit untouched (o_base/o_Ra/o_P1/o_P2 under ~/.cache/tesp15 are this pass's cmpv outputs).
- Next: (1) refit cse1 F6 -> 12729 (+17) and F9 -> 16788 (+35) with the cse2 grid held (the pass-18 pad-type table; candidates: real-code
  cse1-only savings in SIZE/SPEED (`int sx = sxK` sharing = -1 cse1 each, typed-local vs in-call rows) and in LIFE..ROTATE, paying back after
  F6/F9 with VEC0/COLOR pads) — this closes mechanisms 1 and 3 together and puts the module's two `addi` forms in; (2) then re-read B/B2 and
  the r29/r30 bucket with SCHDBG/LADBG.

### t_camera_data pass 5b (closing facts for tcDataExport's last 27 words: unpinned `found` -> r5 reproduces 25 words, `j = 0` as a statement before the body fixes the j/giv-base tie; nothing more applied; t_camera_data 16/17, tcSetBesideOffset 0w, tcDataExport 27w, size exact, 111 OK; 2026-09-12)
- **The `found` pin can be removed once two facts hold; both read, neither closed.** (1) j vs the loop-2 giv base (nopin j R50 L242 10330
  beats reg 390 R38 L184 10326 -> j r4 and 146-188 words): writing `j = 0;` as a statement anywhere in the loop-5 body BEFORE `i++`/`cc =
  cut` (`for (; j < ..; j++, cc++)`) gives `li r12,0` an earlier LUID -> sched1 issues it earlier -> j L248 (10080) -> giv base first -> j
  r12, loops 0-3 identical again (v15a-c; `j = 0` after `cc = cut` is too late). Keep `found = 0` BEFORE `j = 0`: the inner loop's entry
  test `cmpw r5,r0` is cse canonicalising j to the older zero (found); with `j = 0` first the compare would read j. (2) found (reg 290,
  R15 L76, 5921) is allocated before i (98, R18 L170, 4235) and takes r6, pushing i to r5 (and loop-2 `pos` 93 R6 L72 off r5 -> `mr r6,r8`
  + a deleted `mr r6,r5`); the target has i r6 / found r5. `GFORCE=290:5` on v15a = 25 words (the pin's `li r12,0`/`li r5,0` order and
  `cmpw r5` become right); `+351:30,335:31` (r+1 / loop-5 pTc-high) = 22. Not found: what puts i before found (found refs <= 10 with L76,
  or i len <= 121) — found's 15 weighted refs are init 2, cse'd entry compare 2, two `found = 1` copies (the inner loop's first iteration
  is peeled by jump/cse before cross-jumping merges the tails) 2x?, `cmpwi` 2, the two zero stores 2+2. Permuting the body statements
  changes nothing.
- **Remaining after (1)+(2): `r` (85) r10 / `cc` (294) r8 (ours reverse; cc's smpref has r11 through `no`'s `lbz r11` temp, the target's
  temp is r9 = `lbz r9,1(r7); extsb r11,r9` with the extsb issued before `lwz r9,pTc@l` — a sched1 order difference in the body-top
  block that also frees r9 for the pTc load) and r+1 (351, R4 L37 2162) vs loop-5 pTc-high (335, R8 L112 2142): 20 priority points.**
- Kit: `tools/research/sngdbg/patches/dbg-hooks.patch` regenerated from the current src (seven files, includes another agent's SCHDBG/reload1
  hooks and this pass's GFORCE/GFORCEFN/NOEQV); README documents both. `tools/research/sngdbg/cc1plus` sha1 27268379524a...
- Flags: t_camera/t_camera_data stays False (tcDataExport 27w). Scratch /tmp/tcam/ left (ins.py, variants v*.cpp, final.cpp = the tree).

### Tool RELs, t_esp pass 27 (t_esp 208/212: InitTool 1397 -> 465w in the harness (C2, not yet in the tree): cse1 F6/F9 refitted with a NEW cse1-only lever (an explicit `f32 cK = K;` after a constant's first use = -3 cse1-time insns per later use, 0 cse2), the two flush rows are pointers-first (`T* n1 = &A; T* n2 = &B;` before `pos`: the target's 486 has 0.0 FRESH and one `&pos` pseudo, so the pin is +6/+24 insns, not pass 26's +17/+35), and the r29/r30 `this`/`n` bucket is ONE ctor-scope `DB_NUMERIC2* n` per window (13 sets -> not a local-alloc qty -> global.c -> r29/r28 after `this` r30); IN PROGRESS 2026-09-12)
- **Lever (measured, `d.jump SPEED-6 / ROTATE-24`, `d.loop` 0 from it):** a pool constant's 2nd..k-th use in one cse1 window costs 3 pre-cse1
  insns each (high, lo_sum, load) that cse1 deletes (the load becomes a copy of the class head, canon_reg moves the uses, the copy is dead).
  `f32 c16 = 16.0f;` written AFTER the first use and `c16` in the later uses: the definition is itself deleted the same way (+3 -3), every
  later use costs 0 -> (3 - 3k, 0). Same for `int sxK` (pass 22/23's `sx1`): (1 - k, 0). Must not cross a cse1 flush (a use after the flush
  keeps the copy alive). Pads only ADD; this is the only (-n, 0) knob besides the sx sharing.
- **Pins re-read (T segs 486 and 653):** T's 486 loads 0.0 (50DC) FRESH into f28, `li r0,3` fresh, and has ONE `&pos` pseudo (`lwz r7,slot;
  mr r11,r7`); with the pos-first in-call row (pass 26's +17 pin, probe A1) ours shares 0.0 (f23) and splits `&pos` (`addi r7,r1,K` for the
  argument: cse1 cannot merge the argument addressof across the flush, gcse leaves a block-local second occurrence, cse2 merges it only
  through a REG_EQUAL -> REG_EQUIV -> rematerialised). So the row is pointers-first (`n1; n2; pos; sx; call`): row = pa, win, high1, lo_sum1
  | F6 | load1 .. -> F6 = ours+6 (.jump insns 12702,12703,pa,win,high,lo_sum). F9 (653, g_pEditSeq2 = the 2nd pointer): 0.0 SHARED there
  (f22) because cse2 rescues it (the (F8,F9] 0.0 head is ROTATE's first "X:" row, set after F6' = ANMRATE+43; the (F5,F6] head is a SIZE
  row set before F4' = SPEED+109, so 486's is not rescued); F9 = ours+24 (16 insns 16743..16758 + pa, win, high1, lo_sum1, load1, plus1,
  high2, lo_sum2). Making all 18 `n =` rows of SPEED/ROTATE pointers-first is count-neutral and 477 -> 465w (C2).
- **Fit (C2 = /tmp/t27/C2.cpp):** SPEED `f32 c16` after "Y:"(5,16) used by the 95/175/255 "Y:" strings (-6); COLOR pad `1:2:3:5:6:7` (+6, first
  after F6, not ending in 4 = (6,0)); ROTATE `f32 c16` (6 uses: 95/175/255 "Y:" + rot.y/rotSpd.y/rrot.y = -15) + `f32 c32` (95/175/255 "Z:" +
  rot.z = -9); SUB pad 24 sets `1:2:3:5..25` (+24). Grids: cse1 unchanged in window offsets (= F6 +6 / F9 +24 in code), cse2 SPEED+108
  COLOR+552 ANMRATE+40 SUB+47 WORKSP1+78: the -1/-2/-3 are the INHERENT cse2-stream changes of the pinned cse1 grid (a surviving
  `(set X (lo_sum P sym))` head in SIZE and LIFE, the fresh post-flush highs, the rrot.z 360/-360 now shared) — the target has them too.
  seg 0 exact, surv 3 copies none visible, mset [2 0 0 0 0 0 0 0 0 0 2 0 4 1], lc 623 only. Gains: 162-446 all d2 -> 0 (PA r18 / reg9001 r19 =
  T), 486 `lis r6; addi r6; lwz r5,0(r6)` + 653 `lis r11; addi r11; lwz r6,0(r11)` = the module's two addi forms, the 489-688 `lis r6` poison,
  656-688 `lis r9/r11` poison, 9682/9686 r16/r14.
- **`this` r30 / `n` r29 (T) vs ours r29/r30 = local-alloc order:** LADBG: every `n` (refs 4, life 12, pri 6666) is allocated before its
  window's `this` (refs 65/902, pri 4323) and takes the first free callee-saved r30; `this` gets r29. T's `n` is r29 in SIZE/COLOR/ROTATE/BASEPOS
  and r28 in SPEED (r29 there = the hoisted `lis r29; addi r29, g_pSpeedWin@l` address, 445-520) = allocated AFTER `this` and after that address
  pseudo: it is not a local qty at all -> ONE variable `DB_NUMERIC2* n;` at ctor scope assigned in every row (REG_N_DEATHS 13 -> reg_qty -1 ->
  global.c, whose find_reg pass 0 takes the first non-conflicting `regs_used_so_far` register: r29, or r28 in SPEED). ID_WINDOW already had
  that form (pass 24). Applied in the harness to SIZE/SPEED/COLOR/ROTATE (`DB_NUMERIC2* n`) and BASEPOS (`DB_NUMERIC* n`): 1003 -> 477w
  (C1), on the pass-26 tree alone 1397 -> 844w (C0).
- Harness /tmp/t27: base.cpp (= the tree), mkA.py (probe A1: pos-first +17 fit, 1080w, wrong pin), mkB.py (the refit variants; keys in the
  docstring), mkC.py (ctor-scope `n`), wdiff.py (masked one-line diff of a window's insns between two dumps), ct.sh/scan.sh/pad.py/surv.py/
  dsum.py/regmap.py (t25's), rtl_*/, gdbg_*.log, la_B1.log.

### t_camera_data pass 6 (tcDataExport 27 -> 0w pure C, both tags removed; t_camera_data 17/17, flipped, REL verify OK, 111 OK; 2026-09-12)
Scratch /tmp/tcam6/ (v*.cpp variants, run.sh = variant + GORDER summary, gsum.py/dbl.py/llsum.py GDBG readers). Tree edits:
src/t_camera/t_camera_data.cpp (tcDataExport only) + the `# t_camera_data pass 6` MATCHING block in config/G4BE08/modules.py.
Words: 27 -> 22 (loop-5 `i++` in the for header) -> 12 (raw-word `r->area` store) -> 2 (loop-5 cut walker = the shared `dc`)
-> 0 (`static const char tag[] = "B404"`); then `for (j = 0; ..)` back in the header and the loop-4 asm tcCdat base removed: still 0.
- **New fact 1 (local-alloc, generalises pass 5's REG_EQUIV doubling): a MULTI-set pseudo is doubled too when its FIRST set in chain
  order carries an invariant REG_EQUAL.** update_equiv_regs walks the insns forward; at the first set `REG_N_SETS != 1` is tolerated when the
  note is invariant and no earlier replacement exists -> REG_EQUIV + `REG_LIVE_LENGTH *= 2`; a later set with another note (or none) calls
  no_equiv, which removes the notes but never undoes the doubling. cse puts REG_EQUAL (const) on every `li`, so every counter whose first
  surviving set is `x = 0` is doubled (found 38 -> 76, i 85 -> 170, j 124 -> 248 in the pass-5b form; `r`/`cc`/`no`, first set a copy/load,
  are not). Read with `dbl.py` (GORDER len vs the `;; LL` sum; `NOEQV=<reg>` confirms). `NOEQV=98` (i) on the pass-5b form = 25 words = the
  GFORCE=290:5 result, so "i before found" only needed i's priority above 5921 = i's live length down.
- **Fact 2 (the `found` pin, closed): `for (i = 0; i < pTc->adatNum; i++)` with the increment in the HEADER.** gcse PRE hoists `i + 1` to the
  body top (`addi r6,r6,1` at 528 in the target IS the PRE copy, allocated to i's register), so i dies at the block top and is reborn at the
  copy-back: i L170 -> L92, pri 4235 -> 7826 > found 5921 -> i r6, found r5, `cmpw r5,r0` = cse's older zero. The pass-5b statement form
  `i++;` in the body kept i live through the whole body. Side effect: the PRE'd `i + 1` pseudo is shared with loop 4's (one gcse expression
  -> one reaching_reg, REG_N_SETS 2), R8 L70 3428 > the tcCdat base 2857, so the base's doubling no longer matters: **the loop-4 asm base is
  gone** (`c = &tcCdat[i]`, plain C). Also closes (3): r+1 (351) and the loop-5 pTc-high (335) both 2105, allocno order 335 first -> r31/r30.
- **Fact 3 (`r` r10 / `cc` r8 and `no`'s r9 load temp): the loop-5 body-top sched1 order.** Target issue order 927 lbz no-tmp, 939 subf | 941
  stw r->area, 919 li found | 929 extsb no, 1218 addi i+1 | 1117 lwz pTc, 935 mr cc | 923 li j, 1215 addi da+1 | 1118 lbz cdatNum, 1221 addi
  r+1 | extsb | cmpw | bge. Ours issued 1117 at t=3 beside 929 (929 is "birthing": single-set pseudo live at block end -> adjust_priority
  raises it to max_priority 8; 1117 pri 7 was ready at t=1). In the target 1117 is not ready before t=4 => a TRUE dependence on the store
  941 (t=2, store cost 2). alias.c true_dependence: `r->area = ..` as a COMPONENT_REF store is MEM_IN_STRUCT_P with a varying address and the
  `pTc` load is MEM_SCALAR_P at a fixed lo_sum -> fixed_scalar_and_varying_struct_p says disjoint. An INDIRECT_REF store whose address is
  not a PLUS/aggregate (expr.c 6323) is neither IN_STRUCT nor SCALAR -> dependence. Form: `*(u32*) &r->area = (u8*) da - buf;` (a reference
  `CameraAreaInfo*& ra = r->area; ra = ..` works too; `*(p + k)`/`p[k]` do NOT: address by addition = in-struct). With the load at t=4 the no
  temp (r9) is dead before the pTc temp is born -> both r9 (local-alloc), `no` loses its r11 preference, cc's smpref loses r11.
- **Fact 4 (`cc` r8): the loop-5 cut walker is the same variable as loops 2/4's `dc`** (r8 in all three loops in the target = one pseudo:
  R40 L90 22222 + loop 5 -> R56 L114 24561, allocated 60th -> r8; `no` 12000 -> r11, `r` 12258 -> r10 because r11 is now taken). A separate
  `cc` (R16 L24 26666) is allocated before `no`/`r` and takes r10/r11 first; no refs/length spelling of a fresh local reaches 12000.
- **Fact 5 (the 2 words at +0x5c: `mr r3,buf` before `addi r4,B404@l`): `static const char tag[] = "B404"`.** A `const char* const tag`
  local computes `(set P (high LC))` at the declaration (block 0); cse (follow_jumps into block 2) reuses P for the strncpy address, so the
  lo_sum insn kills P (INSN_REG_WEIGHT 0) and sched1 issues it before `mr r3` (weight 1). The static array is a .rodata object emitted at the
  declaration (keeps the B404-before-EMPT order the pass-5 comment needed) whose address high is computed in block 2 -> the lo_sum waits a
  cycle, `mr r3` goes first, sched2's LUID tie then keeps it. A bare literal at the call flips the .rodata order (EMPT first, 4 words).
- Statement order in the loop-5 body top: any order of `no`/`dc = cut`/`found = 0` with the store after them is identical; `dc = cut` after
  `no` with a separate `j = 0` statement between is 2 words (v22). `for (j = 0; ..)` in the header is fine now (pass 5b's `j = 0` statement
  was compensating the pinned found's LUID).
- Flip: bytecmp IDENTICAL, make_rel --verify OK (206300 bytes; the `--link` list comes from `ninja -t commands build/G4BE08/t_camera/t_camera.rel`),
  `flock .. ninja -k 0` clean, `dtk shasum -c` 111 OK, symbols.txt unchanged. Catalogue rows touched: GCC row 7 (global.c: add "the first set
  of a multi-set counter is `li` -> doubled; move the increment into the for header to shorten a counter"), row 4 (alias: `*(u32*) &s->f = v`
  raw-word store as the dependence lever), row 6 (sched1: birthing insns jump to max_priority; a store->load true dependence costs 2).

### Tool RELs, t_esp pass 27, part 2 (continuation of "t_esp pass 27" above — another agent's t_camera section landed between; InitTool 1397 -> 465w IN THE TREE (C2 applied: the F6/F9 refit, 18 pointers-first `n =` rows, one ctor-scope `n` per window), size 0xa20c/0xa1f8, 208/212, seg 0 exact, surv none visible; cse2 F7' re-read to <= SUB+37 (probe D11: ROTATE 276 -> 185, lc 623 and seg 758 fixed, but the W heads' r16/r14 swap costs +120w in 461-751 -> 476w, not applied); nothing flipped; 2026-09-12)
- **Tree:** src/t_esp/t_esp.cpp = pass 26 + (a) SPEED `f32 c16 = 16.0f;` after "Y:"(5,16), used by the 95/175/255 "Y:" strings (-6 cse1);
  (b) COLOR pad `1:2:3:5:6:7` (+6); (c) ROTATE `f32 c16` (95/175/255 "Y:" + rot.y/rotSpd.y/rrot.y) + `f32 c32` (95/175/255 "Z:" + rot.z) =
  -24; (d) SUB pad `1:2:3:5..25` (+24); (e) all 18 `DB_NUMERIC2* n = pa_->CreateNumeric2(win_, &g_pEditSeq->F, &g_pEditSeq2->F, ..)` rows of
  SPEED/ROTATE written pointers-first (`f32* n1 = &..; f32* n2 = &..; DB_POINT pos; int sx; n = pa_->CreateNumeric2(win_, n1, n2, ..)`);
  (f) one `DB_NUMERIC2* n;` (BASEPOS: `DB_NUMERIC* n;`) declared after the ctor-top pad of SIZE/SPEED/COLOR/ROTATE/BASEPOS, `n = ..` per
  row. Locked ninja + bytecmp: 1397 -> 465w (fdiff 80 `*` lines), size 0xa20c/0xa1f8 (5 insns short: 467/692 +1, 623 -1 (the 0.0 `fmr`),
  757/758 -2 each, 802 -1 = T's epilogue `li r3,1`), 208/212. Grids: cse1 LOAD_EVENT+6 SAVE_EVENT+27 OPTION+540 PATH+541 SIZE+174 SPEED+684
  COLOR+869 LIFE+8 ROTATE+644 WORK0+14 WORK6+14 BASEPOS+107 (offsets; F6/F9 are +6/+24 code insns later than pass 26 = T's pins), cse2
  LOAD_EVENT+53 OPTION+70 PATH+348 SPEED+108 COLOR+552 ANMRATE+40 SUB+47 WORKSP1+78; mset [2 0 0 0 0 0 0 0 0 0 2 0 4 1]; regions
  [2 0 0 6 40 8 48 114 70 4 276 0 32 27]; seg 0 exact (d2 = seg 123 as before); PA r18 / reg9001 r19 / 9682 r16 / 9686 r14 = T.
- **cse2 F7' is mis-fitted by >= 10 (read off the 0.0 copy):** cse2 rewrites the F9-row's post-flush 0.0 load into `(set C0 H0)` [REG_EQUAL
  0.0] (H0 = ROTATE's "X:" head, uid 15963; likewise C16/C32 for rrotSpd.y/z) and canon_reg moves every later 0.0 use back to H0 — C0
  survives only for a use AFTER the next cse2 flush. T's 692 (SUB row 1, pos.y 0.0) stores f22 = C0 and 623 has `fmr f22,f28`; ours (cse2
  dump) has C0's SUB use (uid 17921 = SUB+41 in the .loop stream) canon'd to H0 and F7' at SUB+47, while C16's SUB use (17956 = SUB+58) is
  after F7' and survives (`fmr f22,f28` at 624 = C16, T's f21). So T's F7' <= SUB+37 (17911 = SUB row 1's `this` copy; (37,41] is the
  copy's forbidden interval), >= SUB+12 (17877's LC1523 head must stay rescued). Knob: VEC0 is first-after-F9 -> its pad `1:2:3:5..N:4` =
  (N, N-1); D11 = tree + VEC0 11 sets + SUB pad 24 -> 13: F7' SUB+37, lc 0 (623 `fmr f22,f28` for 0.0, 624/625 f21/f23 = T), ROTATE
  region 276 -> 185, seg 758 d16 -> 0 (T's 757-775 poison = a cse2-fresh head after F8'), but F8' WORKSP1+78 -> +68 drops one WORKSP1 pointer
  use from the W heads 9633/9637 (refs 21 -> 20, deaths 13056 -> 13026) and with births 1424/1444 the priorities are 68 vs 69 -> g_pEditSeq2
  takes r16 (T: g_pEditSeq r16) -> 461-751 +4 each = 476w. D12/D13 (F7' +36/+35) identical. In the pass-26 tree the two `lis` fillers were
  born 1418/1420 (one cycle, tie -> qty order -> T's r16/r14); the refit's filler set (fresh post-flush highs, shared 360s) moved them to
  1424/1444 (a sparse filler stretch), so the tie now depends on the lengths. T has refs 20/20 too (its 757 is poisoned = F8' before that
  row), so T's `lis` pair sits in one cycle: the next lever is the filler count above them (one more/less dep-free `lis`/`lfs` head with a
  consumer before WORK0), not F8'.
- **Negative this pass:** WORK1 pad `1:2:3:5..14:4` has NO cse2 weight (WORK0's `flg = 4` is after F10 = WORK0+14, so WORK1 is not the
  const-4 head); an in-block pad after `f32 h` in the WORK macro (`cls##_CSE_PAD2()`, WORK0 11 sets ending in 4) IS (11,10) but moves F8'
  the wrong way (earlier); `DB_POINT pos; pos.x = ..; pos.y = ..;` in a WORK row is (0, +1) — not a negative cse2 knob (485w). No negative
  cse2-only knob exists in (F7', F8'] yet.
- **Residue at 465w (regions):** SPEED 114 / COLOR 70 / SIZE 48: spill-register rotation phase (r9/r10/r11 in `li`/`mr` picks, e.g. 448
  `lwz r6,slot` late + `mr r8,r6` in T vs `mr r9,r6` early) and the sched2 order of the ROTATE rows 637-659 (T issues `lis r6 g_pEditSeq;
  lwz r5` after the ROT_MINMAX stores, ours first); OPTION 40 (segs 323/332/335, pre-existing); WORK6 32 (757/758: T `stw r31` const-0 vs
  ours r25 + the W11 poison, fixed by D11's F8'); BASEPOS 27 (791 d22, 802 `li r3,1`); FP names f26/f28 for 0.0/16.0 heads (D11 fixes).
- Harness /tmp/t27 (kept): tree.cpp (= the tree), mkB.py/mkC.py (the applied edits from base.cpp), mkD.py (VEC0/SUB/WORKn pad variants),
  mkE.py (WORK PAD2 hook), wdiff.py, D11-13/E1-3/F0 variants + logs, la_C2/la_D11.log, rtl_tree (-dl -dg -dt), rtl_tree2 (-dc -dN -df).
  Nothing flipped: no make_rel/`ninja -k 0`/shasum. ~/.cache/tesp15, the kit, /tmp/t23-t26 untouched.
- Next: (1) D11 + bring the 9633/9637 `lis` pair back into one sched1 cycle (SCHDBG=1 on C2 vs D11: their issue cycles and the fillers
  between; a filler-count change above them), then re-read F8' (T's 757 says F8' < the 757 row's high); (2) the spill rotation in SIZE/SPEED/
  COLOR (RLDDBG: the first differing pick per window); (3) the ROTATE row order 637-659 (SCHDBG s2 priorities of `lis r6`/`lwz r7`).
- **Addendum (the W-head `lis` pair in D11, .lreg = sched1 order):** 9634's `lis` (uid 18202) is issued in the free slot right after a
  LOAD_EVENT-region `SetCloseCallback` call (5852), 9638's (18206) only after the following CreateString row (5853-5879: no free slot inside
  it) — 20 insns later; in the pass-26 tree both sat in one cycle (births 1418/1420). One filler more or less above them (a dep-free
  `lis`/`lfs` head whose consumer precedes WORK0) shifts 9634 into the pre-call slot (18195's) and 9638 into the post-call one (2 apart,
  g_pEditSeq shorter by 2 -> r16). Candidates: the F7'/F8' positions decide which SUB/WORKSP1 highs are fresh heads (= fillers).

### Tool RELs, t_esp pass 28 (t_esp 209/212: InitTool 465 -> 431w IN THE TREE (387w while the size was one insn short; `int InitTool()` + `return 1` = the target's epilogue `li r3,1`, size now exact 0xa20c/0xa20c), seg-aligned total_d 627 -> 490, regions [2 46 416 26]; the W heads' r16/r14 swap of D11 READ EXACTLY: the target's WORK heads have 19 refs (WORKSP1 rows 757-759 are ALL fresh `lis`, so F8' precedes WORKSP1 row 1's g_pEditSeq load, not row 2's) = a 66/66 local-alloc tie that qty order resolves to g_pEditSeq r16; seg 692 EXACT with cse2 F7' at SUB+41 (between SUB row 1's DB_POINT `this` copy and its pos.y store: the surviving copy is the target's single `lwz r6,slot`); new cse2 knob: WORK0 in-block pad after `f32 h` (24 sets ending in 4) paid by the WORK1-6 ctor-top pads; nothing flipped; 2026-09-12)
- **The premise of pass 27 part 2 ("T has refs 20/20 too") was wrong.** T's segs 749-751 (WORKSP0) use r16/r14, 757/758/759 (WORKSP1 rows 1-3)
  are all `lis r6,g_pEditSeq@ha; lis r9,g_pEditSeq2@ha` fresh (raw T.s: `lbl_t_esp_bss_13C74C`/`13C87C`), so T's cse2 F8' is before WORKSP1 row 1's
  g_pEditSeq load (WORKSP1+52 in the .loop stream) and after WORKSP0 row 3's g_pEditSeq2 load: the WORK heads have 18 loads = refs 19, deaths at
  WORKSP0 row 3 (suids 12900/12904), births 1424/1444 (the sched1 slots of D11/C2, unchanged) -> 4*19*10000/11476 = 66.2 and /11460 = 66.3 -> 66/66
  tie -> qty order (lower qty = earlier birth) -> g_pEditSeq r16, g_pEditSeq2 r14 = T. No filler count change is needed; the sched1 filler list
  in the band reg9001..W heads is identical in C2 and D11 (fill.py) and all of its heads but K0 are spilled (rematerialised = invisible).
  Consequence: the W heads' allocation depends only on F8' <= WORKSP1+52 (F5 with F8' = +46 broke 3 segs: keep F8' in [+49, +52]).
- **cse2 F8' = F7' + 1001 cse2-stream insns.** Moving F8' alone needs cse2 weight in (F7', F8']: the WORK0 in-block pad after `f32 h`
  (`cls##_CSE_PAD2()` hook in WORK_WINDOW_CLASS, pass 27's mkE.py) = (N, N-1) because it heads the constant-4 class after F10 = WORK0+14;
  its +N cse1 is paid back by removing the WORK1-6 ctor-top `1:2:3:4` pads (-24, all in (F10, F11]) so F11/F12 keep their content positions
  (F11 reads WORK6+10 now = the old +14 minus WORK6's 4-set pad; F12 BASEPOS+107 unchanged). PAD2 20 with the pads removed moved F11/F12 by
  +4 and broke segs 730/735/740 (f30/f27 w/h names, a split `&pos`): the pads must be paid exactly.
- **Seg 692 (SUB row 1) = a surviving DB_POINT `this` copy.** Ours had `lwz r6,slot` for the pos.y store's reload AND a second `lwz r6,slot`
  for the `&pos` argument (the move `(set r6 P)` with P spilled becomes a load; reload_cse cannot delete it because the `stfs 4(r6)` store
  invalidates the slot value: hard-reg base = may-alias). T has one load: with F7' in (37, 41] (= between the copy `(set this P)` at SUB+37 and
  the pos.y store at +41) the copy survives, the store's base and the argument both read `this`, which local-alloc gives r6 -> `lwz r6,slot`
  (the copy) + `stfs f22,4(r6)` + r6 passed. surv.py marks it `*` (visible): that is T's form here, not a defect. The 0.0 stays C0 (f22) because
  the store is after F7' too. F7' at SUB+21 (probe F3: VEC0 27-set pad + SUB 6 + `f32 c4` in SUB) fixed the W heads but left 692 d3.
- **Applied (G1 = /tmp/t28/G1.cpp; tree = G1 + comments):** VEC0 pad `1:2:3:5:6:7:4` (7 sets: +7 cse1, +6 cse2 -> F7' SUB+37 -> +41), SUB pad
  24 -> 17 (cse1 window (F9,F10] count held: F10 WORK0+14), WORK0 PAD2 24 sets ending in 4 (+23 cse2 -> F8' WORKSP1+72 -> +49), WORK1-6
  `_CSE_PAD` -> `{ }` (-24 cse1), the `i = k` gcse-N block -> 0 sets (N 5235, autoN). Grids: cse1 LOAD_EVENT+6 SAVE_EVENT+27 OPTION+540 PATH+541
  SIZE+174 SPEED+684 COLOR+869 LIFE+8 ROTATE+644 WORK0+14 WORK6+10 BASEPOS+107; cse2 LOAD_EVENT+53 OPTION+70 PATH+348 SPEED+108 COLOR+552
  ANMRATE+40 SUB+41 WORKSP1+49 (was SUB+47 WORKSP1+78). Segments: 619-635 (ROTATE tail, 0.0/16.0/32.0 `fmr` names), 663-693 (VEC/SUB), 692,
  702-751 (W heads), 757/758 all exact; 637-656 -2 each; nothing regressed (dsum C2 -> G1: only decreases). Residue 387w: SIZE/SPEED/COLOR spill
  rotation (404-556, 48+114+70), ROTATE rows 637-659 (18/12/14/../24/20 = spill picks + `lis r6; lwz` order), OPTION 271/323/332/335 (pre-existing,
  the &pos split of a cse1 flush), BASEPOS 791 d22, 802 `li r3,1` (T 44 / O 43).
- **`f32 cK` lever accounting:** the saving is 3 - 3k with k = LATER uses (SUB's 4.0 with 4 later uses = -9, not -12; a constant with one later
  use saves 0). Not applied in the end (G1 needs no cK).
- Harness /tmp/t28 (kept): base.cpp (= pass-27 tree), D11.cpp (t27's), F1-F5/G1/G2 variants + logs, tree28.cpp (= the tree), mkF.py (vec0=N sub=K
  c4/c24/c5 levers), apply_g1.py (the WORK edits), mk.py (exact-string edits), ct.sh NAME (autoN + both grids + words + the W heads' LADBG lines
  + their `lis` names), fill.py SCHLOG LREG [lo hi] (sched1 fillers of block 13 with bodies, marks the W heads), cyc.py (issue table with bodies),
  heads.py LREG GREG LO HI (every high/const head whose first use is in a stream range, refs/calls/disposition), uses.py LREG REG.. (all insns of
  a pseudo), off.py DUMP WINDOW REGEX (cse2-stream offsets inside a window; 6 `new`s precede the tail block), rtl_*/sch_*.log dumps.
  ~/.cache/tesp15, the kit, /tmp/t23-t27 untouched. Nothing flipped: no make_rel/`ninja -k 0`/shasum.
- **802 `li r3,1` = a return value:** `int InitTool()` with `return 1;` at the end (declaration at line ~434 changed too; the caller ignores
  it; the mangled name `InitTool__15t_esp_namespacev` carries no return type). Probe R1 = tree + this: 802 T44/O44, d5 -> d4 (the r8/r9 spill
  pick of the four global zero stores remains), size exact. APPLIED.
- **Spill rotation read (seg 404 = SIZE row 2, same shape in 428/434/...):** T `li r10,0` (flags) is the row's 2nd insn and `li r9,1` (index)
  its last; ours the reverse (`li r9,1` 2nd, `li r10,0` last), so the DB_POINT `this` copy (`mr rX,r7` .. `stfs 4(rX)`) takes r9 in T and r10
  in ours. Both `li` are hard-reg sets with equal priority/weight/dependents in ours -> LUID = argument order (r9 first). T issues the r10 zero
  first: its `(set r10 ..)` must rank higher (a lower INSN_REG_WEIGHT, i.e. a source pseudo dying there, or a higher priority) — candidates: the
  flags 0 as a use of a per-row/short-lived zero pseudo (rematerialised `li r10,0` by reload), not a literal. Not probed (time); the same read
  applies to SPEED/COLOR rows (SIZE 48 / SPEED 114 / COLOR 70 residue words) and to ROTATE 637-659 (`lis r6; lwz r5` after the ROT_MINMAX
  stores in T: the g_pEditSeq load's sched2 rank).
- Remaining after this pass (431w): SIZE/SPEED/COLOR spill-register phase (404-556), ROTATE rows 637-659, OPTION 271/323/332/335 (the `&pos`
  split at a cse1 flush, pre-existing), BASEPOS 791 d22, 802 d4 (r8/r9 pick).
- **adx_dcd5 `ADX_DecodeSte4AsSte` 25w: the "helper LOCALS as kept copies" lever is negative for PARAMETER sources.** Requirement 1 (two coalesced
  copies live across the loop) needs a compiler copy whose source the frontend cannot substitute; for a call result that is the callee (sfd_mps above),
  for a parameter nothing stops it: `hl = histl; hr = histr;` as inlined-helper locals (single-def, or 2-def by a second `hl = histl` before/after the loop,
  or `hl = histl .. hl = histr` webs) and as own locals (1-def, 2-def, two disjoint webs) are all propagated away (`0/0`, ghost list unchanged, 25w pinned /
  115w unpinned; the 2-def own pair costs +8). Requirement 3 (`mr qtbl, addi-temp` kept): an identity `asm { mr qtbl, qtbl }` after the def / after the
  frame loop / at the loop end does not make qtbl multi-def for backend-02 copy propagation (115w/115w/116w unpinned, 25/25/27w pinned); a non-`register`
  qtbl still gets `lis r75; addi r76; mr r50,r76` folded at backend-02. Model: the tree's pinned dump is order/colour IDENTICAL under `--k 28` (10 cost-only
  divergences: the compiler's costs are lower for the loop values). Left 25w/143w; the three pass-62 requirements stand.
- **Catalogue notes from this pass (MWCC rows 2/9):** (a) a codeless coalesced copy of a CALL RESULT = an inlined `static` helper's local (`obj = f(); g(obj)`
  inside the helper) — the call cannot be moved, the local is an `@N` web, both `mr` coalesce (sfd_mps 5 -> 0w); a parameter copy in the same position is
  substituted whatever the web shape. (b) `asm { mr x, x }` on a `register` local is a frontend-visible READ that leaves no pcode after backend-02 and does not
  dirty the block (mwsfdcre 4 -> 0w) — use it where the frontend pulls a single-read web into its use and every C second read is folded or emits code; it is
  NOT a second def for backend copy propagation (adx_dcd5 qtbl). (c) The post-RA peephole runs on clean blocks too (sfh_main B37 `000c` before pass 15 and
  still folded), so a block-dirtying difference cannot explain the vendor's unfolded byte-swap chain.
- Tree at the end of the pass: src/lib/sfd_mps.c (helper + `?:` n + pin removed), src/lib/mwsfdcre.c (mwPlyCalcWorkSfd), config/G4BE08/objects.py
  (`# CRI pass 65`: `lib/sfd_mps.c` True); locked `ninja -k 0` clean, `dtk shasum -c` 111 OK. Harness ~/.cache/cri65 deleted. Note: every locked ninja run this
  pass printed `ninja: warning: premature end of file; recovering` and re-ran the split (another process wrote .ninja_log/.ninja_deps concurrently).

### Tool RELs, t_esp pass 29 (t_esp 209 -> 210/212: InitTool 431 -> 0w IDENTICAL IN THE TREE, pure C (pointers-first rows, FSTORE_AT `n->` stores, store orders, .bss order, tail order, cse2 F4' refit, the SAVE_EVENT `ppos`): the SIZE/SPEED/COLOR/ROTATE "spill-register phase" was NOT a reload phase — it is (a) the row form (pointers-first) and (b) every `n->min/max/unit` store spelled `FSTORE_AT` so the NEXT row's g_pEditSeq loads depend on it; BASEPOS 791/802, OPTION 323-335, the .bss order of the two dir-button slots, and cse2 F4' (SPEED+108 -> +88, T-read off the SPEED string rows) all closed; nothing flipped; 2026-09-12)
- **Read (seg 404 = POS row 1, SCHDBG s1 + LOG_LINKS):** the `this` copy's register (`mr r9,r7` T / `mr r10,r7` ours) is reload's pick for the
  pos.y store's address (11540 spilled, inherited from the `(set r7 11540)` load), chosen among the spill regs NOT live at the store: ours
  issues `li r9,1` at sched1 one cycle BEFORE the pos.y store (the store lost the LSU to the two g_pEditSeq/g_pEditSeq2 loads, pri 2071 > 2070,
  and `li r9` (w 1, dep 4, LUID 8207) took the IU slot before `li r10` (LUID 8208)); T has `li r9` AFTER the store. Pointers-first
  (`f32* n1 = &..; f32* n2 = &..;` before `DB_POINT pos`) gives the loads LUIDs below the pos stores -> at the 2071 LSU tie the loads issue first,
  the `addi`s (w 0) take the IU slots, both `li` land after the store -> r9 free -> `mr r9,r7` = T (seg 404 d8 -> 0, no other seg moved).
  Same for SIZE rows 1 and 3 (segs 428/434 -> 0; 427's `addi/stfs` order too). The `u32 flg = 0` / `int sy = 1` pseudo forms change nothing
  (cse folds the constant back; INSN_REG_WEIGHT cannot be reached that way). Anti/output dependences cost 1 cycle here (rs6000_adjust_cost
  returns 0, insn_cost clamps to 1), a store->load true dependence 2.
- **Read (segs 437/441 and all of SPEED/COLOR/ROTATE 471-659):** T's g_pEditSeq loads of a row sit AFTER the previous row's `n->unit`/`min`/`max`
  stores (final order `stfs f30,0xb8(r29)` .. `stfs pos.x` .. `lwz r5,g_pEditSeq@l`), i.e. the loads are true-dependent on those stores; ours
  issued them right after the call (a COMPONENT_REF store `n->unit = v` is MEM_IN_STRUCT_P and disjoint from the fixed-scalar `mem/f` load).
  `FSTORE_AT(n, 0xB8|0xA4|0xA0, v)` (the pass-19 byte-offset store: not in-struct, not scalar, same insn count) on ALL 33 `n->` stores of
  SIZE/SPEED/COLOR/ROTATE + ROT_MINMAX: 417 -> 265 (SIZE/SPEED/COLOR) -> 106w (ROTATE 637-659 all exact but 659). ROTATE row 11's three
  stores then in `max, min, unit` source order like rows 10/12 (659 -> 0, 103w). SetDefault segments 440/522: T issues `mr r3` and the
  pool `lfs` BEFORE the min store = the min store is anti-dependent on the max value's pool load (the pool `mem` is not `/u` here), so the
  source order is `max` then `min` (SIZE row 4, COLOR row 1 swapped: 95w).
- Harness /tmp/t29: base.cpp (= the tree), P3/P5/P6 (pointers-first rows), P9/P10/P11 (FSTORE_AT), Q1 (ROTATE order), R1 (min/max swap),
  p.sh NAME [seg..] (runv + per-seg d), lipat.py T.s O.s (per-row `li r9`/`li r10`/pos.y-store/copy order T vs O), calls.py LREG IDX..
  (segment index -> call uid), cycu.py SCHLOG LREG s1|s2 UID [before after] (issue table around a uid with bodies), rtl_*/dbg_*.log.

### Tool RELs, t_esp pass 29, part 2 (continuation of "t_esp pass 29" above — other agents' sections landed between; InitTool 431 -> 40w in the tree, then the cse2 F4' read; 2026-09-12)
- **IN THE TREE (12:14): InitTool 431 -> 40w, size exact, 209/212 (Load/SaveEmType 2/2 untouched).** Edits, all pure C: POS row 1 /
  SIZE rows 1, 3 / COLOR row 1 pointers-first; the 33 `n->max/min/unit` stores of SIZE/SPEED/COLOR/ROTATE (+ROT_MINMAX) as FSTORE_AT;
  ROTATE row 11 `max,min,unit`; SIZE row 4 / COLOR row 1 `max` before `min`; BASEPOS row 2's `nameNum`/`nameTbl` as byte-offset
  stores (`*(u32*)((u8*)n + 0xC4) = 256; *(const char***)((u8*)n + 0xC0) = tbl;` -> seg 791 d22 -> 0: row 3's g_pSeqHead load waits for
  them in T); the tail `g_page = 0; g_editTop = 0; g_editCursor = 0; g_fileMenu = 0;` (seg 802 -> 0: g_page's store is first at sched1
  because its shared high dies there, so at reload r8 (g_fileMenu's allocated high) is live and the g_page remat takes r9; then
  fileMenu r10 / editTop r8 / editCursor r7 = the local-alloc order of the three short highs, 24 orders tried, only this one is 0);
  `.bss`: `g_pSaveDirButton` declared BEFORE `g_pLoadDirButton` (T's 154E3C/154E40) and SetDirCallback's SetString/SetColor pairs
  save-first (SetDirCallback stays identical; the button names are ours); OPTION rows 5/10/12: `n->max = K; n->nameTbl = tbl;
  n->nameNum = k;` (max first: the pool load of K is issued before the nameNum store in T = the nameNum store is after the load in
  LUID order; segs 323/332/335 -> 0). Grids unchanged throughout (cse1 LOAD_EVENT+6 .. BASEPOS+107, cse2 LOAD_EVENT+53 .. WORKSP1+49,
  N 5235).
- **Remaining 40w = two cse2-grid items, both read, neither fitted:**
  (a) **SPEED strings 448/449/452/453 (d10/10/10/12): T's cse2 F4' lies in (SPEED+73, SPEED+91] of the .loop stream, ours +108.** Read
  off `lipat.py`: T issues `lwz r6,slot` (the DB_POINT copy) AFTER the string's `addi r5` in 448/449/450 and BEFORE it in 452+; ours
  after only in 450. The copy `(set r6 C)` is w 0 and the string lo_sum `(set r5 (lo_sum P LC))` is w 0 only when P dies there, so
  T's "X:"/"Y:"/"Z:" heads of 448-450 die there (= POS's heads, shared across F3', last use) and T's 452 "X:" is a head that lives
  on (P1 from cse1, shared with 455/458, rematerialised `lis r8`); ours shares 448 AND 452 with POS's head 5574 because F4' (+108) is
  after 452's high (+90) — 450's Z: head dies at 450 in ours too (matches). F4' was never T-read before (F5'..F8' were). Cost of the
  move: +17..35 cse2 insns in (F3', F4'] and the same number removed in (F4', F5'] with cse1 held — and gcse's N (5235 = insn count at
  gcse /2|1, the `i = k` block is EMPTY so nothing can be subtracted): every cse2-weighted pad set is a gcse insn, so a symmetric
  +k/-k needs a real-code -k somewhere or the k inside existing cse2 pads (PARENT 16 / VEC0 7 / WORK0 24 / LIFE 39: moving weight
  between them moves F5'..F8', all T-pinned). Not attempted this pass.
  (b) **SAVE_EVENT 271 d6 = the `&pos` split (F2 = SAVE_EVENT+27 between the DB_POINT copy at +14 and the argument's addressof at
  +35).** `DB_POINT* ppos = &pos;` right after `pos` and `ppos` as the argument: cse1 folds the ppos set into the copy before F2,
  combine folds the copy into the argument -> one pseudo, `lwz r5,slot` = T (X3b: 271 d6 -> d2, 323-335 unchanged). It adds one
  cse2-stream insn (the copy) -> F2' OPTION+70 -> +69 and everything after -1; removing the SAVE_EVENT `{1,4}` in-block pad (-1
  cse2, -2 cse1) + SAVE_CHECK pad `1:2` -> `1:2:3:5` (+2 cse1) restores BOTH grids and N exactly (X6) — but the post-sched1 stream
  is 2 insns shorter from SAVE_EVENT on (the split's `(set A (plus fp K))` + move are gone), and the block-13 local-alloc tie of the
  two entry highs flips: g_pEditSeq (reg 5438, refs 17, [672,6274]) 121 vs g_pEditSeq2 (5357, [656,6278]) 120 -> r23/r22 = T;
  in X6 deaths 6270/6274 -> 121/121 -> qty order -> g_pEditSeq2 r23 (segs 404/428/434/437/441 +4 each, 106w). Needs g_pEditSeq2's
  length +1 insn (death >= 6276) or g_pEditSeq's -1 with the grids held; not found this pass. `pos` declared after `w`/`h` (X1)
  moves the frame slots and the whole entry (2003w) — not the form.
- **`f32 cK` lever, corrected:** the saving is -3 per later use ONLY while the constant's uses stay in one cse1 window AND the k-th
  use is not the window's... (OPTION `c16` over 5 string rows measured -12, over 6 rows -15: one of the uses saved nothing);
  measure with off.py, do not assume 3k. Pads with values that occur as constants in the code (8, 9, 10, 12, 16, 0x34 ...) become
  class heads and REWRITE later `li` into copies (Y15: 6502w); use values like 7001+ for long pads.
- Scratch /tmp/t29 (kept): base/P*/Q*/R*/S1/T*/U*/V*/W*/X*/Y* variants + o_* under ~/.cache/tesp15, rtl_base/rtl_cur2 (-dj -dL
  -dl) + dbg_*.log (SCHDBG/LADBG/RLDDBG), gdbg_cur/la_cur logs, flushes.sh / flushmap.py / off.py / heads.py / uses.py / wincount.py
  (the t18/t28 tools patched for `int InitTool`), p.sh, lipat.py, calls.py, cycu.py. Nothing flipped (t_esp 209/212).

### Tool RELs, t_esp pass 29, part 3 (continuation — InitTool 40 -> 8 -> 0w IDENTICAL in the tree: cse2 F4' refit and the SAVE_EVENT `ppos`; t_esp 210/212, Load/SaveEmType 2/2 left; nothing flipped; 2026-09-12)
- **IN THE TREE (12:21): InitTool 40 -> 8w — cse2 F4' refitted to T's read (Z3).** PARENT pad 16 -> 36 sets (`1:2:3:5..16:7001..7020:4`,
  (+20 cse1, +20 cse2)), LIFE pad 39 -> 19 sets (-20 cse1, -20 cse2), POS `f32 c5/c112/c24/c128/c16` after each constant's first
  use (-18 cse1-only: the lever saves 3 per later use BEYOND the first, i.e. 3(k-1) for k later uses — c5/c112/c24/c128 with 2 later
  uses = -3 each, c16 with 3 = -6; the pass-27 "3-3k" was measured on 3-use constants), POS pad `1:2:3:5` -> `1:2` (-2), LIFE second
  block `{ int e_; e_ = 7101..7120; }` after the d_ pad (+20 cse1-only in (F8, F9]). Grids: cse1 unchanged (F5 SIZE+174 .. F12
  BASEPOS+107), cse2 LOAD_EVENT+53 OPTION+70 PATH+348 **SPEED+88** (was +108) **COLOR+532** (was +552) ANMRATE+40 SUB+41 WORKSP1+49,
  N 5235. Every COLOR/BLEND/FLAG segment stays exact with F5' at COLOR+532, so the pass-21 word-fit "+552" was never T's position
  either; segs 448/449/452/453 -> 0 (the SPEED "X:"/"Y:" heads now die at 448/449, 452/453 use the cse1 head P1 like T).
- **Remaining 8w = seg 271 (SAVE_EVENT CreateNormalWindow, the `&pos` split at cse1 F2 = SAVE_EVENT+27).** The merged form is
  `DB_POINT* ppos = &pos;` between `pos` and `w` with `ppos` as the argument (Z11: 271 d6 -> d2 = `lwz r5,slot` as T); it is 0 cse1,
  +1 cse2 (F2' 70 -> 69 and every later flush -1, all harmless) and -2 post-sched1 insns, and it permutes the FP constant registers of
  the SAVE..OPTION region (segs 217/220/233/271/274-329: f23/f24/f25/f27/f28 names = block-13 local-alloc qty order; the 192.0 head
  reg3194 gains 2 refs, the 9-ref head born at suid 1468 disappears in favour of an 8-ref head born 1770) — 42w. With the SAVE_EVENT
  `{1,4}` pad removed + SAVE_CHECK `1:2:3:5` (Z4: F2' back to 70, grids and N exact) the same FP permutation remains, so it is not
  F2'. `ppos` after `flg` (after F2) = no change at all (Z12 = Z3). `pos` after `w`/`h` moves the frame slots (X1, 2003w). Next: read
  the FP qty list (la_Z3.log vs la_Z4/Z11: `rg -- '-> (55|56|57|59|60)$'`, births < 3000) to find which 128/144/164/192 head changes
  class with the ppos copy in the cse2 stream, then place a (0 cse1, +1 cse2) real insn elsewhere in (F1', F2'] or re-pin F2' with it.
- Harness additions: Z1-Z12 variants (Z3 = the tree), la_Z3/la_Z4/la_Z11.log (LADBG block 13), gdbg_cur.log; ~/.cache/tesp15 o_* for each.
  Nothing flipped (t_esp 209/212: Load/SaveEmType 2/2, InitTool 8).
- **IN THE TREE (12:33): InitTool IDENTICAL (431 -> 0w), t_esp 210/212 (Load/SaveEmTypeUpdateCallback 2/2 left).** Seg 271 closed by
  Z16/Z17: `DB_POINT* ppos = &pos;` right after `pos` in SAVE_EVENT's CreateNormalWindow row, `ppos` as the argument, SAVE_SST pad
  `1:2:3:4` -> `1:2:3` and SAVE_CHECK `1:2` -> `1:2:3`. Read: the ppos set is folded into the DB_POINT copy C by cse1 (same window,
  before F2) and the argument's separate `(set A (addressof pos))` disappears, so the cse1 stream is one insn SHORTER from +35 on and
  F2 (= the 1002nd insn) lands one insn earlier — before the `w` STORE instead of before the `h` high. With the `w` store after the
  flush, cse1 leaves `(set 3897 3194)` (the w value as a copy of LOAD_EVENT's 192.0 head) alive with its REG_EQUAL 192.0, cse2 then
  learns 3194 = 192.0 inside (F1', F2'] and merges LOAD_CHECK/SAVE_CHECK's 192.0 loads into it (reg3194 3 -> 5 refs, the 4099 head
  gone) = the f23/f24/f25/f27/f28 permutation of segs 217-329 (Z4/Z11). SAVE_SST -1 puts F2 back after the store (F2 = SAVE_EVENT+28
  in the new numbering = the same content), SAVE_CHECK +1 restores F3..F12. Grids: cse1 LOAD_EVENT+6 SAVE_EVENT+28 OPTION+540 PATH+541
  SIZE+174 SPEED+684 COLOR+869 LIFE+8 ROTATE+644 WORK0+14 WORK6+10 BASEPOS+107; cse2 LOAD_EVENT+53 OPTION+70 PATH+348 SPEED+88
  COLOR+532 ANMRATE+40 SUB+41 WORKSP1+49; N 5235. fdiff still prints 80 `*` REPLACE lines with identical text (relocation-name
  artefacts); bytecmp is the judge. Not flipped: the module needs Load/SaveEmTypeUpdateCallback (2/2 words, open since pass 8).
- Catalogue rows touched (GCC): row 4 (alias): `n->f = v` through a call result is MEM_IN_STRUCT_P and lets the next row's fixed-scalar
  global loads (`g_pEditSeq`) issue before it; `*(f32*)((u8*)n + off) = v` (FSTORE_AT) is neither in-struct nor scalar and orders them
  after (T's rows do). Row 6 (sched1 tie): the argument `li rN,K` sets are LUID-ordered among equals, but whether an address copy
  reload lands in r9 or r10 is decided by which `li` precedes the pos.y store in the sched1 stream — pointers-first rows move the
  g_pEditSeq loads' LUIDs below the stores. Row "cse2 flush grid": a source change that deletes a cse1-stream insn BEFORE a flush
  moves that flush one insn earlier in content and can split a load from its store, leaving a copy with a REG_EQUAL constant alive
  into cse2 (a (0 cse1, +1 cse2) knob with side effects on constant sharing).
- Load/SaveEmTypeUpdateCallback 2/2 (out of this pass's scope, one probe): the do-loop of ModelTypeGroupSkip has one loop-body high of
  g_modelType (`lis r12`, hoisted to the preheader) and the entry high r4; T uses the body high for the LAST ref of the body (`t =
  g_modelType`, `lhz r7`) and r4 for the first (`g_modelType = t + dir`, `sth r9`), ours the reverse. Rotating the loop (one step before
  a `for (;;)` with the step last) is 47/48w and changes the size — not the form; the ebb/PRE question of which ref keeps the body head is
  open (read the .cse/.gcse dumps of the do-body).

### Tools/t_esp_area pass 1 (the r28/r29 ctor tie FLIPS with one codeless anchor in the dbg_tool.h ctor, but every added sched1 insn in block 13 breaks a second tie, the path1/path2 PRE copies r18/r17; IN PROGRESS 2026-09-12)
Scratch /tmp/tea/ (`hv.sh HDR` = the four includers through variant.sh; h0.h = the tree header; a1-a4, b1-b4, c1-c9, d1-d2 = header variants;
rtl0/ = rtl.sh dumps of the tree; dbg0.log = SCHDBG+LADBG of the same compile (the function name prints EMPTY for ToolEspArea: grep
`^LADBG  b13`/`SCHDBG  s1 b13`); `sch.py LOG ' ' s1|s2 DUMP` = the issue table; g0.log/g8.log = GDBG of the tree / variant c8).
- **Volatile asms are out:** an output-less `asm("" : : "r"(work))` anywhere in the ctor body (a1-a4) or as the operands of the existing
  `asm("")` barrier (b1-b4) is a volatile ASM_OPERANDS = `flush_hash_table` in cse (cse.c 7719; the bare `asm("")` is an ASM_INPUT and
  does not flush): 236-247 words in t_esp_area (the loop's `i < rows` pre-test stops folding, `lwz r10,0x30(r31)` + `cmplw` + `bge`).
- **The tie flips with ONE extra RA-time insn between suid 26 (`stw x`) and 52 (`stw pWork`)**: `asm("" : "=m"(x) : "r"(name))` right
  after `w = strlen(name)` (d1; `"=m"(y)` d2 the same): pri 1, ready at c3 but every slot up to c10 goes to a higher-priority insn, so it
  issues at c11 next to `stw w` -> `stw pWork` at suid 54 -> work 2*4/50 = 1600 < `li 4` 1666 -> li4 r29, work r28, li5 r29: the ctor
  block e08-e98 is IDENTICAL. The x store before strlen is not made dead by the `"=m"(x)` (the call clears mem_set_list between them at
  flow1/flow2), and at sched2 the anchor takes a free IU slot (c12) with no visible change. Anchors reading `work` (+1 ref -> 2000) or
  `n` (`li 32` gets 2 dependents and wins the c8 slot from `li 5`: c5-c7) or `len` = the strlen result before `w = len` (c8/c9: fine for
  the ctor, `stw w` stays the dying store; after `w = len` (c1/c2) `stw w` sinks) all fail elsewhere; `"=m"(h)` (c3) is deleted by flow1.
- **What the anchor costs: the r17/r18 global pair (6 words, `mr r18,r28; mr r17,r29` at +0x218, `mr r7,r18; mr r8,r17`, `mr r5,r18;
  mr r6,r17`) = the gcse PRE copies of `&path1` (reg 2268, REG_EQUIV fp+0x58) and `&path2` (2272, fp+0x98), refs 3 each, live from
  block 3 through block 27 (every block of the tool loop), REG_EQUIV-doubled lengths 1034 / 1022 -> pri floor(30000/len) = 29 / 29 ->
  allocno order -> 2268 r18 (= the target). haifa counts every 'i'-class insn of a block once per live pseudo (haifa-sched.c 5498), so
  ONE more sched1 insn in block 13 gives 1036/1024 -> 28/29 -> 2272 first -> r17/r18 swapped. 1034 is the exact 29 boundary; the tie
  survives only with 0 or 7..18 extra insns in blocks 3-27 (both at 28). t_event's SubToolMessInit has the same shape (8 words = two
  such pairs) and t_lightarea's ctor block is unchanged (its 4 words stay the t_esp_area vtable relocs).
- Read (haifa/local-alloc source): suid = 2*insn_number, insn_number counts every non-NOTE insn (labels, USE/CLOBBER, asms); the sched1
  weight puts a store in the dying group by the ORIGINAL order's REG_DEAD (cyMax's `1` store issues before h/cxMax because `cyMax = 1`
  is the last `1` in source); `update_equiv_regs` moves a 2-ref constant init before its use only for multi-block pseudos
  (REG_BASIC_BLOCK < 0) - an RA-time insn haifa never counted - but the only candidate here (`wy`: a caller variable set in block 0)
  loses the sched2 c6 tie against `addi r9,vt` by LUID (the moved `li r0,25` sits before `stw y`), and its removal from block 13's
  sched1 hands the c4 slot to `li 5` (crosses the `new` call). `T* w2 = work; pWork = w2;`-style copies are folded by cse (canon_reg,
  the pseudo is the qty head) or become noops only via regmove optimize_reg_copy_2 (`B = A; ...; A = B;` with B's second def
  unknown to cse) which then still cost a sched1 insn.
- Next: find a way to add the anchor's RA-time insn WITHOUT a block-13 sched1 insn, or to re-tie the path1/path2 pair (2268 needs
  refs 4 or 2272 refs 2 / len +9 or 2268 len -4, e.g. `&path2`'s PRE copy born 7 sched1 insns later than `&path1`'s in block 3).

### Tools/t_esp_area pass 2 (the remaining constraints read and measured: pre-strlen issue-slot budget, the live-out priority boost, the moved-init LUID; nothing applied, nothing flipped; 2026-09-12)
Scratch /tmp/tea/ kept (e1/f1/f2 header variants, vwy*.cpp = caller with `int wyv = 0x19`, vp1-3 = caller anchors before the first sprintf).
- **The ctor block has a fixed sched1 slot budget before strlen.** c1-c8 = 15 issue slots (2 per cycle, one beside `bl __builtin_new`) for
  exactly 15 insns (12 IU: lis/lis/addi/addi, `li r3`, li4, li25, lis/addi vt, `mr r31,r3`, `mr r3,name`, li5; 3 LSU: x, vt, y). Every
  scheduled insn consumes a slot (haifa 7064, USE/CLOBBER included), so ANY extra pre-strlen insn - launder-copies (`asm("" : "=r"(a) :
  "0"(b))`, e1/f1/f2), copies, anchors with a pre-strlen store as output - evicts the lowest-priority one, `li 5` (pri 5), to the
  strlen cycle: it then no longer crosses the call (`li r11,5`, or `li r27,5` when it lands before the call), and `li 32` moves up.
  Post-strlen slots are free (LSU-bound), which is why the d1 anchor is invisible there.
- **`adjust_priority` (haifa 4346): a ready insn setting a single-set pseudo that is LIVE AT THE BLOCK END is raised to
  max_priority - sched1 only (reload_completed skips it).** That is why the gcse PRE copies `mr r18,r28`/`mr r17,r29` (2268/2272) issue
  right after their `addi` in block 3 (p 30/25 in SCHDBG vs prio 2 in the region table) and why `mr r31,r3` prints p 10 at sched1 and
  8 at sched2. Block-local pseudos (li4, work, li5) are never boosted. Not usable here: the boosted insn must be live-out (a global
  allocno).
- **`update_equiv_regs`' moved init (the only RA-time insn haifa does not count) verified on `wy`:** caller `int wyv = 0x19;` + 
  `CreateEditWindow(4, wyv, ..)` (vwy.cpp) makes wy a 2-ref multi-block pseudo; its `li 25` leaves block 13's sched1 stream (the
  r17/r18 pair stays 29/29) and is re-inserted before `stw y` at RA. Two failures, both as predicted: (a) the freed c4 slot goes to
  `li 5` -> crosses `new` -> `li r27,5` (+`li r28,0x20`); (b) at sched2 the moved `li r0,25` has the LUID of `stw y` (c8) and loses the
  c6 tie (pri 8 = 8, dependents 4 = 4) to `addi r9,r9,vt@l`: `addi r9; li r0,0x19` instead of the target's `li r0; addi r9`. A c4
  filler must be a non-MEM codeless insn (MEM insns after the call carry the flush anti, cost 1) ready at c4 with pri >= 5 and
  weight <= 0, AND lose its sched2 slot to `li 5`; every launder-copy fails one side: name-launder before `pName` (pri 5) wins c8 at
  sched2 by LUID and evicts `li 5`; name-launder before strlen (pri 8) takes c3 at sched2 and puts `li r29,4` after `bl`; wx-launder
  (f1) ties into li4's qty (refs 2+2 -> 6666 > name's 2380 -> li4 takes r30); wy-launder in the ctor puts the moved init before `stw x`
  (li4 life 14 -> 1428 < work); in the caller (LUID < li4's, li4 to c4: life 8 -> 2500 > name).
- **Block-3 anchors to re-time the pair's births (vp1-3: `asm("" : "=m"(pG->room_id) : "r"(path2))` before the first sprintf, so
  `addi r29,r1,0x98` gets pri >= 30) reorder sched2 visibly (36-210 words).** The pair needs 2268 len -4 or 2272 len +9 or 7-18 extra
  sched1 insns in blocks 3-27; none of the refs levers exists (Init#1 uses the block-local `&path1` r28, the later two sites the PRE copy).
- Conclusion of the two passes: the target's ctor block is reproduced exactly by one codeless post-strlen anchor (d1), and the vendor's
  build must have had either the same extra insn plus different global lengths, or an RA-time-only insn between `stw x` and `stw pWork`
  that we cannot construct without moving `li 25` (sched2 LUID) or `li 5` (call crossing). Flags unchanged: Tools/t_esp_area 7w (+4
  reloc), Tools/t_lightarea 4w, t_event/t_event IDENTICAL, db_toolbase IDENTICAL; include/dbg_tool.h, src/Tools/t_esp_area.cpp untouched.
- Next: (1) find a natural source of one FEWER sched1 insn in ToolEspArea's blocks 3-27 (or 7 more) that is byte-neutral - then d1
  closes t_esp_area (t_event's SubToolMessInit needs the same accounting for its two pairs); (2) alternatively a sched2-only dependent
  for a moved `li r0,25` (a fifth dependent of r0 in the region, or one fewer for `addi r9`).

### Tool RELs, t_esp pass 30 (t_esp 210/212 unchanged: Load/SaveEmTypeUpdateCallback 2/2 — the loop-2 `lis r12` mechanism READ EXACTLY and reproduced up to a 9-word entry residue (pass-9's rotated while, variant C) / a 1-word `mr` (variant I); nothing in the tree, nothing flipped; new `CSEDBG=2` hook; 2026-09-12)
- Scope: the last 2 words of each callback = the do-loop of `ModelTypeGroupSkip` (loop 2, the `dir == -1` back-skip). T: TOP's
  `sth r9,@l(r4)` through the ENTRY high R (r4), the body's last ref `lhz r7,@l(r12)` through a fresh high hoisted into the
  preheader (`lbz r0,1(r8); lis r12; extsb r10,r0; TOP:`); ours the reverse. Harness /tmp/t30 (ins.py = one line per insn of a
  dump, mk.py/try.sh = loop-2 body variants through tools/research/kit/variant.sh, rtl_base/rC/rD/rE/rF = rtl.sh dumps incl. `-dt` cse2).
- **Mechanism (gcse + cprop, read off the dumps):** every `g_modelType` ref expands to its own `(set H (high)); (mem (lo_sum H sym))`;
  cse1 merges them per ebb; gcse PRE turns each in-loop `(set H (high))` occurrence into `H = R` (R = the bb-0 insertion, `lis r4`)
  and gcse's cprop pass 2 then replaces H by R in every block where that copy is AVAILABLE AT BLOCK ENTRY (`find_avail_set` is
  avin-based) — never in the copy's own block. loop.c hoists the surviving `H = R` (REG_EQUAL high -> move-insn `lis`). So the block
  that keeps the fresh `lis` is the block holding the PRE copy, and TOP can use R only if its store's high pseudo was set in a block
  whose copy reaches TOP's entry. In a fall-through-entered `do {} while` that is impossible (cse_end_of_basic_block stops at TOP's
  label; a `do {} while (0)` split, variant B, changes nothing: the wrap's join label is merged after the inner LOOP_END and the AROUND
  path jumps over the note). T's distribution = a ROTATED WHILE at cse1: `jump TEST; TOP: step; wrap; TEST: t = load; n = tbl[t];
  tests; beq TOP` — cse1 follows the back edge into TOP (barrier-preceded, 1 use) with TEST's high H_t, PRE puts the copy in TEST,
  cprop gives TOP (whose only predecessor is TEST) R, loop.c hoists H_t -> `lis r12`. Pass 9's form.
- **The entry `b TEST` is removed by `duplicate_loop_exit_test` in the jump pass BEFORE cse2** (toplev: `jump_optimize (.., JUMP_AFTER_REGSCAN)`
  right before cse2; jump.c 2577): it copies the exit code (TEST .. LOOP_END; refused if > 20 insns, or a CODE_LABEL/CALL/LOOP_CONT
  inside) before LOOP_BEG with `beq TOP` intact + `jump END; barrier`, and deletes `jump TEST`. At jump1 the exit code is > 20 insns
  (fresh high/lo_sum pairs), after cse1/gcse/loop it is <= 20 -> duplicated pre-cse2 (variant C: `.loop` dump still has `jump 272`,
  `.sched` has NOTE_INSN_LOOP_VTOP). cse2 then folds the copy in the ebb that reaches from loop-1's TEST via the AROUND path (`bne 222`
  skips the label-free `beq TOP1` block) and knows t, n, a0, c1; a folded `beq TOP` becomes `jump TOP` + barrier, the post-cse2 jump
  pass deletes the unreachable `jump END` and the jump-to-next -> fall-through into TOP, exactly T's `bne cr7; lbz; lis r12; extsb; TOP:`.
  Variant C (`while (a0 == (n = tbl[(s16)(t = g_modelType)])[0] && c1 == n[1]) { step; wrap }`): body EXACT, entry residue = the
  UNFOLDED part of the copy (`extsh; slwi; lwzx; lbz; extsb; cmpw r6; b` = 9w; jump2 cross-jumps its tail into the loop's test).
- **Why the copy does not fold (cse.c, read with the new hook):** `exp_equiv_p` validates the regs of the LOOKED-UP expression
  (`REG_IN_TABLE (r) == REG_TICK (r)`), and `reg_in_table` is set only by `mention_regs` = when an expression containing r is
  INSERTED; a `(set r ..)` dest is left at -1 (cse.c 7809). `canon_reg` rewrites regs to their class head, and `make_regs_eqv`
  (cse.c 984) makes the NEW reg the head when it lives beyond the block and its `REGNO_LAST_UID` cuid is later than the head's.
  So `X2 = X1` (loop-2 pseudo := loop-1 pseudo) with X2 living longer makes X2 the head, and the next lookup of an expression
  containing X2 fails although the same expression with X1 is in the table. In the copy: (a) same `t` (C): `t = load` folds to the
  self-set `(set t t)` (kept: "it is good to have SET_SRC == SET_DEST") -> `invalidate (t)` -> reg_tick bump -> loop-1's index entry
  `(sign_extend (subreg:HI t))` is stale -> `n = tbl[..]` recomputed, then `n` self-set -> `(mem n) == a0` lost; (b) own `t2` (D/E):
  `t2 = t`, t2 head (used in loop 2 + tail; a tail `t = t2` does not help: gcse cprop rewrites the tail's t to t2 first, F likewise for
  a `t = t2` inside the test) -> the index lookup fails with `[r226 qty 344 first 226 tick 1 intable -1]`; (c) loop-2's table lo_sum
  pseudo P2, hoisted by loop.c and rewritten by cse2 to `P2 = P1` (P1 = loop-1 TEST's lo_sum), becomes head over P1 -> `(mem (plus P2
  idx))` never hits loop-1's `(mem (plus P1 idx)) == n`. Fix for (c): a user pointer assigned in loop-1's test and indexed in loop-2's
  (`tbl = g_modelNameTbl; n = tbl[(s16) t]` / `n2 = tbl[..]`): same code (`lis r11; addi r11` stay inside TEST1), no P2. Fix for
  (a)/(b) found only with a copy: `u16 tmp; while ((n2 = tbl[(s16)(tmp = g_modelType)], t = tmp, a0 == n2[0]) && c1 == n2[1]) { tmp = 0;
  step; wrap } tmp = 0; tail(t)` (variant I: the dead `tmp = 0` kills stop gcse cprop from propagating tmp into t's uses, so tmp is
  the loop-test-local pseudo and t stays head) -> the copy folds COMPLETELY (entry exact), but the `t = tmp` copy is not coalesced
  (`mr r7,r0` in TEST2, +1 word) and the hoisted high lands in r8 (17w); G/H (`(s16) g_modelType` index, `(u16)` conversion copy) give
  `lha`/`clrlwi`. The index must be `(s16) t` from the SI variable (T: `lhz r7; extsh r0,r7`) and `t` must be the load's own dest.
- Facts for the next pass: (1) jump2's cross_jump cannot merge a peeled step into TOP (labels in the first sequence stop
  `find_cross_jump`; two jumps to different labels are unequal), so pass 9's "peel + cross-jump" idea is dead; (2) the tail can be
  spelled `g_modelType += 1; if ((s16) g_modelType > 242) g_modelType -= 243;` — the tail is in TEST2's cse1 ebb (AROUND from the a0
  exit over LOOP_END) so `(mem g_modelType)` folds to t and the code is identical, removing every post-loop reference to t; (3) what
  is still needed is a spelling where loop-2's load goes straight into a variable that canonicalises, in the pre-cse2 copy, to a reg
  already mentioned in cse2's table (loop-1's `t`) without a self-set: candidates = make loop-1's `t` referenced later in the stream
  than loop-2's (REGNO_LAST_UID rule) in a way gcse cprop cannot rewrite, or record loop-1's index under a reg that is not re-set
  (a second same-value variable in TEST1 that outlives t); (4) SaveEmTypeUpdateCallback is the same inline, so one fix closes both.
- Kit: `tools/research/sngdbg` gained the `CSEDBG=2` hook (cse_insn per-SET trace: found/not found + qty/head/tick/intable of the regs; README
  section, patches/dbg-hooks.patch refreshed; binary still cmp-equal to the tree object with no env var). /tmp/t30 is scratch.

### Tools/t_esp_area pass 3 (Tools/t_esp_area 37 -> 38/38 and Tools/t_lightarea 37 -> 38/38 FLIPPED, Tools.rel + t_event.rel verify OK, 111 OK: the ctor anchor d1 applied in dbg_tool.h, the r18/r17 pair re-tied by a 4th codeless ref in ToolEspArea, t_event's d1 cost read as a gcse table-size wrap and closed with a dead-test size lever; 2026-09-12)
Scratch /tmp/tea3/ (`hv3.sh HDR [TEA_SRC] [TEV_SRC]` = the four includers with caller variants; `try.sh NAME LINE 'TEXT'` = one
ToolEspArea insertion + GDBG pair lengths; `try_tev.sh NAME SRC` = t_event words + SubToolMessInit's gcse table sizes + the tie pairs'
buckets read off the -dG dump; `gsplit.py LOG [--fn N --regs ..]` splits a GDBG log into functions (the fn name print is garbage for
ToolEspArea/SubToolMessInit) and sums the `;; LL` segments per pseudo).
- **ToolEspArea (d1 header): the pair's lengths are 2268 1036 / 2272 1024 (raw 518/512: b3 93/86, b27 4/5, every other block equal), so
  the "-1 insn" search was replaced by a refs change: `asm("" : "=m"(buf[0]) : "r"(path1));` between `SetInitWorkFunc` and
  `InitAllWork()` (block 21, before the InitAllWork loop) makes &path1's PRE copy refs 4 -> pri floor(log2 4 * 4 * 10000 / 1038) = 77
  (vs 29): allocated long before the pair's old slot, still `[scan pass0 r18]`; &path2 stays 29 -> r17. ToolEspArea's 7 code words -> 0
  (the 4 vtable-reloc words are the unflipped-link artefact). Same anchor at other statements costs a sched2 slot (after
  CreateEditWindow 2w `lwz 0x30(r29)` moved; after AddEditColumn#2 25w; before sprintf 16w); before InitAllWork the slot is free.
- **t_event's 8 words under d1 are NOT a length effect: the two swapped pairs are gcse-PRE-numbering ties** (refs 4 len 12 pri 6666:
  the inlined EvtMessRead's `i-1` vs `p+0xb0`; refs 2 len 23 pri 869: `t->room`/`t->no` = `(plus t 0x1114/0x1124)`), and the d1 asm is
  +1 insn at gcse entry in SubToolMessInit: n 1219 -> 1220, expression table `(n/2)|1` 609 -> 611 buckets. Raw hashes (fitted from two
  sizes): room 17713 / no 17729, i-1 13400 / p+0xb0 13574; bucket = hash % S; the room/no pair wraps at S=611 (605 vs 10) and only
  there (k=29), the i-1 pair wraps for S in 610..617 and 639..646. Good S: <= 609 or 619..637 (odd, != 633). `t` is a parameter
  pseudo (83), so the dead-pseudo lever cannot move that pair; only n can.
- Lever applied in SubToolMessInit: `{ int z = 0; do { } while (0); if (z > 128) { z = z*77+1; .. 7 statements .. } }` before
  `EvtMessRead(m, path)`: cse1 is blind past the LOOP_END note, gcse counts set+cmp+jump+14 = 17 insns (n 1237, S 619), cse2 folds the
  test, jump opt deletes the arm, flow1 the set: 0 code, 0 sched1 insns; t_event IDENTICAL with d1 (k=5/6 -> S 617 4w, k=7/8 -> 619 0w).
  Precedent: st2_3/r225 `candidate (gcse table size)`. Ordinary dead tests fail: a constant from another block is never folded on PPC
  (cprop cannot rewrite a `cmpwi` operand to a constant, the jump tests a CC pseudo; V7 110w).
- gcse hash-table sizes are `(n/2)|1` and `(n/4)|1` (odd), n = 'i'-class insns at gcse entry = cse1 dump count + purge_addressof's
  insertions (SubToolMessInit: 1151 + 68). Read S straight off the -dG dump ("Expression hash table (S buckets").
- **Applied (tree):** include/dbg_tool.h `asm("" : "=m"(x) : "r"(name));` right after `w = strlen(name)` in the cDbgEditWindow ctor
  (tag `candidate (local-alloc qty order)`); src/Tools/t_esp_area.cpp `asm("" : "=m"(buf[0]) : "r"(path1));` before `tool.InitAllWork()`
  (tag #17); src/t_event/t_event.cpp the 17-insn dead test before `EvtMessRead(m, path)` (tag `candidate (gcse table size)`). Locked
  `ninja -k 0` after deleting the five includer objects: t_esp_area 4 words + t_lightarea 4 words = the `_vt.20cDbgFileSelectWindow` /
  `_vt.18cDbgOkCancelWindow` linkonce relocs resolved to t_event's copies while Tools' own objects were unlinked; t_event, Tools/db_toolbase,
  t_event/db_toolbase IDENTICAL. Flipped both in config/G4BE08/modules.py (`# Tools/t_esp_area pass 3` block) -> both IDENTICAL,
  `make_rel.py --verify` Tools.rel OK (512356 bytes, cmp-equal to orig) and t_event.rel OK (208220), `dtk shasum -c` 111 OK, symbols.txt
  unchanged. REL report after the flip: 358/359 module files linked (was 356).
- Catalogue rows touched: GCC row 2 (gcse PRE pseudo numbering: the LOOP_END-blinded dead test as the table-size lever, +3 + 2 per arm
  statement, 0 code, 0 sched1 insns; the header-insn-count hazard: one more insn in a shared inline shifts every includer's S) and row 7
  (global.c: a codeless `asm("" : "=m"(m) : "r"(p))` ref is the refs 3 -> 4 log2 step, 29 -> 77, when the lengths cannot be re-tied).
- Not found: a byte-neutral -1 sched1 insn in ToolEspArea's blocks 3-27 (the 7-insn birth gap is the sprintf#1 argument chain between the
  two boosted PRE copies; every caller-side insertion is +1) - the refs lever replaced it. Scratch /tmp/tea3 kept small (scripts, logs;
  rtl dumps deleted).
### Tool RELs, t_esp pass 31 (t_esp 210 -> 212/212, MATCHING, t_esp.rel verify OK + cmp-equal, 111 OK: Load/SaveEmTypeUpdateCallback 2/2 -> 0/0 pure C, no pins, no dead statements; 2026-09-12)
- Result: `ModelTypeGroupSkip` (the inline both callbacks close with) rewritten; `t_esp/t_esp.cpp` flipped in config/G4BE08/modules.py
  (`# t_esp pass 31` block); `make_rel.py --verify orig/G4BE08/files/Rel/t_esp.rel` OK (441468 bytes, cmp-equal), `flock ninja -k 0`,
  `dtk shasum -c` 111 OK, `git diff config/G4BE08/symbols.txt` empty. Tree edits: src/t_esp/t_esp.cpp (ModelTypeGroupSkip only),
  config/G4BE08/modules.py, this section. Scratch /tmp/t31 (mk.py/try.sh/cs.sh = variant + CSEDBG=2 harness, fA..fH variants, dumps).
- **The form (variant G/H):** no loop-carried `t` at all. Both loops step the GLOBAL (`ModelTypeWrap(dir)` = `g_modelType += dir` + the two
  wraps, already in the file), each loop's test reads it into a BLOCK-LOCAL `u16` used only for the table index (`u16 t = g_modelType;
  n = g_modelNameTbl[(s16) t];` in loop 1's body; `while (a0 == (n2 = g_modelNameTbl[(s16) (t2 = g_modelType)])[0] && c1 == n2[1])` with
  `u16 t2` declared inside the `dir == -1` block), tail `g_modelType += 1; if ((s16) g_modelType > 242) g_modelType -= 243;`. No `tbl`
  pointer (adding pass 30's `tbl = g_modelNameTbl` to this form REGRESSES: a `lhz r0` reload appears in TOP2, variant F).
- **Mechanism, read off the `.cse2` dump + CSEDBG=2 (variant G):** every `g_modelType` read is two insns, `(set h (mem:HI))` +
  `(set t (zero_extend h))`. With the global-step spelling, cse1's back-edge follow makes TOP2's `+= dir` a use of the HI pseudo `h262`
  (`(plus (subreg:SI h262) dir)`), the tail likewise (`(plus (subreg:SI h262) 1)` / `-242`), so `h262` is the loop-carried value and the
  `u16 t2` (`r265`) is set in TEST2 and used ONLY there (its single use is the index `(sign_extend (subreg:HI r265))`; combine later merges
  the two into `extsh r0,r7` on the `lhz r7` of `h262` — exactly T's `lhz r7; extsh r0,r7`). `duplicate_loop_exit_test` (jump pass right
  before cse2) gives a fresh pseudo to every reg whose FIRST and LAST uid lie inside the exit code (jump.c: `REGNO_FIRST_UID (regno) ==
  INSN_UID (insn)`), so in the copy `t2` -> `r338`, the index/n/a temps likewise, while `h262` (mentioned in TOP2) keeps its number.
  cse2, in loop-1 TEST's ebb (AROUND over the `beq TOP1` block, through `c1 = n[1]`): `(set h262 (mem:HI))` -> `(set h262 h204)` (h204 =
  TEST1's HI pseudo; h262 becomes the class head — lives beyond, later LAST_UID — which is HARMLESS here); `(set r338 (zero_extend h262))`:
  canon_hash hashes a bare REG by its QTY (`hash += REG << 7 + REG_QTY`), so `(zero_extend h262)` finds TEST1's `(zero_extend h204)` class
  -> `(set r338 r205)` (r205 = loop 1's u16 temp), r338 is block-local so r205 STAYS head; the index `(sign_extend (subreg:HI r338))` is
  canon_reg'd to `(subreg:HI r205)` — a SUBREG hashes by REGNO (cse.c 7791 comment), which is why pass 30's `t2 = t` (t2 head) never hit —
  and finds insn 206 -> `r339 = r209`; then the shift, `(mem (plus idx P))` (bare regs, qty hash: loop 2's hoisted lo_sum `r259 = r208`
  is found although r259 is head — pass 30's "(c)" was the same SUBREG symptom, not a separate blocker), `(mem n)`, `(mem (plus n 1))`
  all hit -> `(compare a0 a0)`, `(compare c1 c1)` -> both jumps deleted (NOTE_INSN_DELETED 476/480), `jump END` unreachable -> the entry
  falls into TOP2 = T's `bne cr7; lbz; lis r12; extsb r10; TOP:`. The only real copy left, `mr h262,h204`, is two global pseudos with
  abutting lives -> global gives both r7 -> `mr r7,r7` deleted by jump2 noop_moves. The fresh-pseudo copies die at flow.
- **Why the pass-30 forms failed, now exact:** (a) same `t`: the copy's `(set t (zero_extend h'))` folds to the self-set `(set t t)`
  (t IS the class head so the cse.c 7289 "replace SET_SRC with the head" rule does not fire), `invalidate (t)` + `remove_invalid_refs`
  delete the index entry; (b) own loop-carried `t2`: `(set t2 t1)` with `uid_cuid[REGNO_LAST_UID (t2)] > ..(t1)` makes t2 head
  (make_regs_eqv 984), and the SUBREG index then hashes under t2's regno. Confirmed by the head rule's converse: variant D/E = pass-30's
  D + a dead `t = 0;` AFTER loop 2 (LAST_UID(t1) later than every t2 mention, flow deletes it) -> IDENTICAL too, `mr t2,t1` coalesced.
  So the catalogue-grade fact: **a cse2 fold across a `(subreg:HI reg)` (any `(s16)`/`(u8)` view of a promoted local) needs the reg to be
  the class HEAD; a bare reg in any other expression hashes by qty and folds whatever the head is.** Levers: a block-local temp for the
  narrow view (fresh pseudo in the duplicated test), or make the older variable live longer (`REGNO_LAST_UID` = last PATTERN mention;
  REG_NOTES only set REGNO_LAST_NOTE_UID); a tagged dead set after the loop is the fallback.
- Hypothesis verdicts: H2 (`tbl[(s16) g_modelType]` index) is dead as written (`lha`; the `lhz+extsh` needs a u16 pseudo) but its core
  — loop 2 with no loop-carried variable — is the answer once the u16 is a test-local index temp; H1's head question is real (D/E) but
  needs the dead set; H3 (function-level `tbl`) regresses (F); H4 not needed (no copy survives). Both callbacks closed by the one inline.
